#include "player.h"
#include "weapon.h"
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <stdlib.h>
#include <string.h>
#include <t3d/t3dskeleton.h>
#include "collision.h"
#include "battle.h"
#include "util.h"

#define FB_COUNT 3
#define PLAYER_AABB_HEIGHT 90.0f
#define PLAYER_AABB_WIDTH 30.0f
#define PLAYER_GRAVITY -900.0f
#define PLAYER_JUMP_VEL 260.0f

//Player players[4];

static inline void player_refresh_aabb(Player *player)
{
    player->e.aabb.min.v[0] = player->e.pos.v[0] - PLAYER_AABB_WIDTH / 2.0f;
    player->e.aabb.max.v[0] = player->e.pos.v[0] + PLAYER_AABB_WIDTH / 2.0f;
    player->e.aabb.min.v[2] = player->e.pos.v[2] - PLAYER_AABB_WIDTH / 2.0f;
    player->e.aabb.max.v[2] = player->e.pos.v[2] + PLAYER_AABB_WIDTH / 2.0f;
    player->e.aabb.min.v[1] = player->e.pos.v[1];
    player->e.aabb.max.v[1] = player->e.pos.v[1] + PLAYER_AABB_HEIGHT;
}

void player_init(Entity *e, T3DVec3 position, T3DModel *model)
{
    Player *player = (Player *)e;
    player->e.type = E_PLAYER;
    player->e.visible = true;
    player->e.modelMatFP = malloc_uncached(sizeof(T3DMat4FP) * FB_COUNT);
    player->moveDir = (T3DVec3){{0, 0, 0}};
    player->e.pos = position;
    player->currSpeed = 0.0f;
    player->rotY = 0.0f;
    player->velY = 0.0f;
    player->onGround = true;
    player->alive = true;
    player->isHittable = 0;
    player->hitpoints = 100.0f;
    player->weapon = NULL;
    player->state.s = STATE_IDLE;
    player->state.frame = 0;
    player_refresh_aabb(player);
    player->skel = malloc_uncached(sizeof(*player->skel));
    *player->skel = t3d_skeleton_create_buffered(model, FB_COUNT);
    player->skelBlend = malloc_uncached(sizeof(*player->skel));
    *player->skelBlend = t3d_skeleton_clone(player->skel, false); // optimized for blending, has no matrices
    player->handBoneIdx = t3d_skeleton_find_bone(player->skel, "Bone.011");

    player->animIdle = t3d_anim_create(model, "bananaJump");
    t3d_anim_set_looping(&player->animIdle, true);
    t3d_anim_set_playing(&player->animIdle, true);
    t3d_anim_set_speed(&player->animIdle, 1.0f);
    t3d_anim_attach(&player->animIdle, player->skel);

    player->animPunch = t3d_anim_create(model, "bananaPunch4");
    t3d_anim_set_looping(&player->animPunch, false);
    t3d_anim_set_playing(&player->animPunch, false);
    t3d_anim_set_speed(&player->animPunch, 2.0f);
    t3d_anim_attach(&player->animPunch, player->skelBlend);

    player->animDodge = t3d_anim_create(model, "bananaDodge");
    t3d_anim_set_looping(&player->animDodge, false);
    t3d_anim_set_playing(&player->animDodge, false);
    t3d_anim_set_speed(&player->animDodge, 1.0f);
    t3d_anim_attach(&player->animDodge, player->skel);

    player->animPunch2 = t3d_anim_create(model, "bananaPunch5");
    t3d_anim_set_looping(&player->animPunch2, false);
    t3d_anim_set_playing(&player->animPunch2, false);
    t3d_anim_set_speed(&player->animPunch2, 1.5f);
    t3d_anim_attach(&player->animPunch2, player->skel);

    // model = t3d_model_load("rom:/banana.t3dm");
    for (int i = 0; i < FB_COUNT; i++)
    {
        t3d_mat4fp_from_srt_euler(&player->e.modelMatFP[i],
                                  (float[3]){0.125f, 0.125f, 0.125f},
                                  (float[3]){0.0f, -player->rotY, 0},
                                  player->e.pos.v);
        rspq_block_begin();
        t3d_matrix_push(&player->e.modelMatFP[i]);
        rdpq_set_prim_color(RGBA32(255, 255, 255, 255));
        t3d_model_draw_skinned(model, player->skel);
        t3d_matrix_pop(1);
        player->e.dplEntity[i] = rspq_block_end();
    }
}

void set_player_state(Player *player, PlayerState newState)
{
    // Avoid redundant state changes
    if (player->state.s == newState.s)
        return;
    debugf("Player state changed from %d to %d\n", player->state.s, newState.s);
    player->prevState = player->state;
    player->state = newState;
    //player->state.frame = 0;
}

// Cleanup player resources
void player_cleanup(Player *player)
{
    for (int i = 0; i < FB_COUNT; i++)
    {
        if (player->e.dplEntity[i])
            rspq_block_free(player->e.dplEntity[i]);
        player->e.dplEntity[i] = NULL;
    }

    t3d_anim_destroy(&player->animIdle);
    t3d_anim_destroy(&player->animPunch);
    t3d_anim_destroy(&player->animDodge);
    t3d_anim_destroy(&player->animPunch2);

    if (player->skel)
        t3d_skeleton_destroy(player->skel);
    if (player->skelBlend)
        t3d_skeleton_destroy(player->skelBlend);

    if (player->e.modelMatFP)
        free_uncached(player->e.modelMatFP);
    if (player->skel)
        free_uncached(player->skel);
    if (player->skelBlend)
        free_uncached(player->skelBlend);

    player->e.modelMatFP = NULL;
    player->skel = NULL;
    player->skelBlend = NULL;
}

void player_entity_cleanup(Entity *e)
{
    if (!e)
        return;
    player_cleanup((Player *)e);
}

void player_entity_update(Entity *e, const EntityUpdateContext *ctx)
{
    if (!e || !ctx)
        return;
    Player *player = (Player *)e;
    player_update(player, &ctx->joypadInputs[player->playerIndex], &ctx->joypadPressed[player->playerIndex], ctx->camPos, ctx->frameIdx, ctx->deltaTime, ctx->entities, ctx->numPlayers, ctx->state);
    player->e.visible = player->alive && (player->isHittable % 2 == 0);
}

void drop_weapon(Player *player)
{
    Weapon *dropped = player->weapon;

    dropped->equipped = false;
    dropped->attachedPlayer = NULL;
    dropped->isAttack = false;
    dropped->attackFrame = 0;

    // Drop it a bit in front of the player so it's out of pickup overlap.
    T3DVec3 dropDir = player->moveDir;
    dropDir.v[1] = 0.0f;
    float len2 = t3d_vec3_len2(&dropDir);
    if (len2 < 0.0001f)
    {
        // If not moving, use facing direction from rotY.
        dropDir.v[0] = sinf(player->rotY);
        dropDir.v[1] = 0.0f;
        dropDir.v[2] = cosf(player->rotY);
    }
    else
    {
        float invLen = 1.0f / sqrtf(len2);
        t3d_vec3_scale(&dropDir, &dropDir, invLen);
    }

    const float dropDistance = 70.0f;
    dropped->e.pos.v[0] = player->e.pos.v[0] + dropDir.v[0] * dropDistance;
    dropped->e.pos.v[2] = player->e.pos.v[2] + dropDir.v[2] * dropDistance;
    dropped->e.pos.v[1] = 0.15f;

    // Keep within arena bounds.
    const float BOX_SIZE = 240.0f;
    if (dropped->e.pos.v[0] < -BOX_SIZE)
        dropped->e.pos.v[0] = -BOX_SIZE;
    if (dropped->e.pos.v[0] > BOX_SIZE)
        dropped->e.pos.v[0] = BOX_SIZE;
    if (dropped->e.pos.v[2] < -BOX_SIZE)
        dropped->e.pos.v[2] = -BOX_SIZE;
    if (dropped->e.pos.v[2] > BOX_SIZE)
        dropped->e.pos.v[2] = BOX_SIZE;

    if (dropped->hit)
        *dropped->hit = dropped->e.pos;
    weapon_refresh_aabb(dropped);

    player->weapon = NULL;
}
static bool resolve_player_map_collision(Player *player, T3DObject *mapColObj)
{
    // Simple Y-axis collision resolution with map collision object.
    // Assumes mapColObj is axis-aligned and flat on Y axis.
    int16_t objMinX = mapColObj->aabbMin[0];
    int16_t objMaxX = mapColObj->aabbMax[0];
    int16_t objMinZ = mapColObj->aabbMin[2];
    int16_t objMaxZ = mapColObj->aabbMax[2];
    int16_t objMinY = mapColObj->aabbMin[1];
    int16_t objMaxY = mapColObj->aabbMax[1];
    int16_t objY = objMaxY; // use top Y for ground

    // Check horizontal overlap.
    if (player->e.aabb.max.v[0] < objMinX || player->e.aabb.min.v[0] > objMaxX)
        return false;
    if (player->e.aabb.max.v[2] < objMinZ || player->e.aabb.min.v[2] > objMaxZ)
        return false;

    // Require vertical overlap with the object bounds.
    const float minY = player->e.aabb.min.v[1];
    const float maxY = player->e.aabb.max.v[1];
    if (maxY < objMinY || minY > objMaxY)
        return false;

    // Snap to top only when feet are near top.
    const float eps = 0.5f;
    if (minY >= objY - eps && minY <= objY + eps)
    {
        // Push player up to the top of the object.
        player->e.pos.v[1] = objY;
        player_refresh_aabb(player);
        return true;
    }
    // Otherwise resolve in XZ to keep player outside the object.
    const float aMinX = player->e.aabb.min.v[0];
    const float aMaxX = player->e.aabb.max.v[0];
    const float aMinZ = player->e.aabb.min.v[2];
    const float aMaxZ = player->e.aabb.max.v[2];

    float penX = fminf(aMaxX - objMinX, objMaxX - aMinX);
    float penZ = fminf(aMaxZ - objMinZ, objMaxZ - aMinZ);
    if (penX <= 0.0f || penZ <= 0.0f)
        return false;

    const float sepBias = 0.05f;
    float pushX = 0.0f;
    float pushZ = 0.0f;
    if (penX < penZ)
    {
        float aCenterX = 0.5f * (aMinX + aMaxX);
        float bCenterX = 0.5f * (objMinX + objMaxX);
        float s = (aCenterX < bCenterX) ? -1.0f : 1.0f;
        pushX = s * (penX + sepBias);
    }
    else
    {
        float aCenterZ = 0.5f * (aMinZ + aMaxZ);
        float bCenterZ = 0.5f * (objMinZ + objMaxZ);
        float s = (aCenterZ < bCenterZ) ? -1.0f : 1.0f;
        pushZ = s * (penZ + sepBias);
    }

    player->e.pos.v[0] += pushX;
    player->e.pos.v[2] += pushZ;
    player_refresh_aabb(player);
    return false;
}
// internal helper: collision detection
static void collision_detect(Player *player, Entity *entities[], int numPlayers, BattleState *state)
{
    for (int i = numPlayers; i < numPlayers + 2; i++)
    {
        if (player == ((Weapon *)entities[i])->attachedPlayer)
            continue;
        if (player->weapon)
            continue;
        if (((Weapon *)entities[i])->equipped)
            continue;

        // Use AABB overlap for pickup (player world AABB vs weapon pickup AABB).
        if (!aabbf_overlaps(&player->e.aabb, &((Weapon *)entities[i])->e.aabb))
            continue;

        player->weapon = (Weapon *)entities[i];
        ((Weapon *)entities[i])->equipped = true;
        ((Weapon *)entities[i])->attachedPlayer = player;
    }

    if (player->isHittable > 0)
    {
        player->isHittable -= 1;
    }

    for (int i = 0; i < 4; i++)
    {
        // Resolve each pair only once (collision_detect runs per-player).
        if (((Player *)entities[i]) <= player)
            continue;
        if (!((Player *)entities[i])->alive)
            continue;

        if (!aabbf_overlaps(&player->e.aabb, &((Player *)entities[i])->e.aabb))
            continue;

        // Minimum-translation-vector (MTV) resolution in XZ.
        const float aMinX = player->e.aabb.min.v[0];
        const float aMaxX = player->e.aabb.max.v[0];
        const float aMinZ = player->e.aabb.min.v[2];
        const float aMaxZ = player->e.aabb.max.v[2];

        const float bMinX = ((Player *)entities[i])->e.aabb.min.v[0];
        const float bMaxX = ((Player *)entities[i])->e.aabb.max.v[0];
        const float bMinZ = ((Player *)entities[i])->e.aabb.min.v[2];
        const float bMaxZ = ((Player *)entities[i])->e.aabb.max.v[2];

        // Positive penetration depth along each axis (if overlapping).
        float penX = fminf(aMaxX - bMinX, bMaxX - aMinX);
        float penZ = fminf(aMaxZ - bMinZ, bMaxZ - aMinZ);

        if (penX <= 0.0f || penZ <= 0.0f)
            continue;

        // Add a tiny bias so we don't end up exactly touching and re-trigger due to float jitter.
        const float sepBias = 0.05f;

        // Choose the axis of least penetration.
        float pushX = 0.0f;
        float pushZ = 0.0f;
        if (penX < penZ)
        {
            float aCenterX = 0.5f * (aMinX + aMaxX);
            float bCenterX = 0.5f * (bMinX + bMaxX);
            float s = (aCenterX < bCenterX) ? -1.0f : 1.0f;
            pushX = s * (penX + sepBias);
        }
        else
        {
            float aCenterZ = 0.5f * (aMinZ + aMaxZ);
            float bCenterZ = 0.5f * (bMinZ + bMaxZ);
            float s = (aCenterZ < bCenterZ) ? -1.0f : 1.0f;
            pushZ = s * (penZ + sepBias);
        }

        // Split the correction between both players.
        player->e.pos.v[0] += 0.5f * pushX;
        player->e.pos.v[2] += 0.5f * pushZ;
        ((Player *)entities[i])->e.pos.v[0] -= 0.5f * pushX;
        ((Player *)entities[i])->e.pos.v[2] -= 0.5f * pushZ;

        player_refresh_aabb(player);
        player_refresh_aabb((Player *)entities[i]);
    }
    bool onTop = false;
    if (state && state->modelMap)
        collision_resolve_entity_vs_map(&player->e, state->modelMap, &onTop);
    if (!onTop && player->e.pos.v[1] < 1.0f)
    {
        player->e.pos.v[1] = 0.0f;
        player->velY = 0.0f;
        onTop = true;
    }
    player->onGround = onTop;
    if (player->onGround && player->velY < 0.0f)
        player->velY = 0.0f;
    player_refresh_aabb(player);
}


void player_update(Player *player, const joypad_inputs_t *joypad, const joypad_buttons_t *joybtns, T3DVec3 *camPos, int frameIdx, float deltaTime, Entity *entities[], int numPlayers, BattleState *state)
{
    
    if(player->state.s == STATE_HITLAG)
    {
        //player->isHittable += 1;
        player->state.frame += 1;
        if (player->state.frame >= 10)
        {
            player->state.frame = 0;
            set_player_state(player, player->prevState);
        }
        return;
    }
    
    else if (player->state.s == STATE_DODGE)
    {
        player->state.frame += 1;
        player->e.pos.v[0] -= player->moveDir.v[0] * 8.0f;
        player->e.pos.v[2] -= player->moveDir.v[2] * 8.0f;
        if (player->state.frame >= 10)
        {
            player->state.frame = 0;
            set_player_state(player, (PlayerState){.s = STATE_IDLE, .frame = 0});
        }
        //return;
    }
    float speed = 0.0f;
    T3DVec3 newDir = {0};
    newDir.v[0] = (float)joypad->stick_x * 0.05f;
    newDir.v[2] = -(float)joypad->stick_y * 0.05f;
    speed = sqrtf(t3d_vec3_len2(&newDir));
    if (speed > 0.15f)
    {
        newDir.v[0] /= speed;
        newDir.v[2] /= speed;
        player->moveDir = newDir;

        float newAngle = atan2f(player->moveDir.v[0], player->moveDir.v[2]);
        player->rotY = t3d_lerp_angle(player->rotY, newAngle, 0.5f);
        player->currSpeed = t3d_lerp(player->currSpeed, speed * 0.3f, 0.15f);
    }
    else
    {
        player->currSpeed *= 0.64f;
    }
    if (joypad->btn.z)
        player->currSpeed = 5.0f;

    if(player->state.s != STATE_DODGE)
    {
        player->e.pos.v[0] += player->moveDir.v[0] * player->currSpeed;
        player->e.pos.v[2] += player->moveDir.v[2] * player->currSpeed;
    }
   
    const float BOX_SIZE = 240.0f;
    if (player->e.pos.v[0] < -BOX_SIZE)
        player->e.pos.v[0] = -BOX_SIZE;
    if (player->e.pos.v[0] > BOX_SIZE)
        player->e.pos.v[0] = BOX_SIZE;
    if (player->e.pos.v[2] < -BOX_SIZE)
        player->e.pos.v[2] = -BOX_SIZE;
    if (player->e.pos.v[2] > BOX_SIZE)
        player->e.pos.v[2] = BOX_SIZE;

    if (joybtns->b && !(player->state.s == STATE_ATTACK))
    {
        // State refactor, delete these later
        // player->attacking = true;
        // player->attackFrame = 0;
        set_player_state(player, (PlayerState){.s = STATE_ATTACK, .frame = 0});
        if (player->weapon)
            player->weapon->isAttack = true;
    }

    if(joybtns->b && (player->state.s == STATE_ATTACK) && (player->state.frame > 20 && player->state.frame < 30))
    {
        // State refactor, delete these later
        // player->attacking = true;
        // player->attackFrame = 0;
        set_player_state(player, (PlayerState){.s = STATE_ATTACK2, .frame = 0});
        if (player->weapon)
            player->weapon->isAttack = true;
    }

    if (joybtns->r && !(player->state.s == STATE_DODGE))
    {
        set_player_state(player, (PlayerState){.s = STATE_DODGE, .frame = 0});
    }
    /*
    if (player->state.s == STATE_ATTACK)
    {
        player->state.frame += 1;
        if (player->state.frame >= ATK_LENGTH * 2)
        {
            player->state.frame = 0;
            set_player_state(player, (PlayerState){.s = STATE_IDLE, .frame = 0});
            if (player->weapon)
                player->weapon->attackFrame = 0;
        }
    }
    */

    if (joybtns->a && player->onGround)
    {
        player->velY = PLAYER_JUMP_VEL;
        player->onGround = false;
    }

    player->velY += PLAYER_GRAVITY * deltaTime;
    player->e.pos.v[1] += player->velY * deltaTime;

    // Keep the gameplay/debug AABB tied to the final position (including vertical movement).
    player_refresh_aabb(player);

    if (joypad->btn.c_right)
    {
        camPos->v[1] += 2.0f;
    }

    if (joypad->btn.c_left)
    {
        camPos->v[1] -= 2.0f;
    }

    if (joybtns->c_up)
    {
        if (player->weapon)
        {
            drop_weapon(player);
        }
    }

    t3d_anim_update(&player->animIdle, deltaTime);

    if (player->state.s == STATE_DODGE)
    {
        if (!player->animDodge.isPlaying)
        {
            t3d_anim_set_playing(&player->animDodge, true);
            t3d_anim_set_time(&player->animDodge, 0.0f);
        }
        t3d_anim_update(&player->animDodge, deltaTime);

        // If the non-looping animation finished, drop back to idle
        if (!player->animDodge.isPlaying)
        {
            set_player_state(player, (PlayerState){.s = STATE_IDLE, .frame = 0});
            t3d_skeleton_reset(player->skel);
            t3d_anim_update(&player->animIdle, 0.0f);
        }
    }
    // Attack overrides base while active
    else if (player->state.s == STATE_ATTACK)
    {
        player->state.frame += 1;
        if (!player->animPunch.isPlaying)
        {
            t3d_anim_set_playing(&player->animPunch, true);
            t3d_anim_set_time(&player->animPunch, 0.0f);
        }
        t3d_anim_update(&player->animPunch, deltaTime);

        // If the non-looping animation finished, drop back to idle
        if (!player->animPunch.isPlaying)
        {
            set_player_state(player, (PlayerState){.s = STATE_IDLE, .frame = 0});
            t3d_skeleton_reset(player->skel);
            t3d_anim_update(&player->animIdle, 0.0f);
            if (player->weapon)
            {
                player->weapon->attackFrame = 0;
                player->weapon->isAttack = false;
            }
        }
    }
    else if (player->state.s == STATE_ATTACK2)
    {
        player->state.frame += 1;
        if (!player->animPunch2.isPlaying)
        {
            t3d_anim_set_playing(&player->animPunch2, true);
            t3d_anim_set_time(&player->animPunch2, 0.0f);
        }
        t3d_anim_update(&player->animPunch2, deltaTime);

        // If the non-looping animation finished, drop back to idle
        if (!player->animPunch2.isPlaying)
        {
            set_player_state(player, (PlayerState){.s = STATE_IDLE, .frame = 0});
            t3d_skeleton_reset(player->skel);
            t3d_anim_update(&player->animIdle, 0.0f);
            if (player->weapon)
            {
                player->weapon->attackFrame = 0;
                player->weapon->isAttack = false;
            }
        }
    }
    else
    {
        // Ensure next attack starts from the beginning
        if (player->animPunch.isPlaying)
        {
            t3d_anim_set_playing(&player->animPunch, false);
        }
        t3d_anim_set_time(&player->animDodge, 0.0f);
        t3d_anim_set_time(&player->animPunch, 0.0f);
    }
    
    float animBlend = 1.0f;
    if (player->state.frame > 0)
    {
        animBlend = 1.0f / (player->state.frame / 18.0f);
        if (animBlend > 1.0f)
            animBlend = 1.0f;
    }
    // Update base pose
    
        // NOTE: Buffered skeleton matrices can switch to a new matrix buffer when any bone changes.
    // Forcing the root bone as dirty ensures the full hierarchy gets valid matrices every frame.

    player->skel->bones[0].hasChanged = true;
    player->skelBlend->bones[0].hasChanged = true;
    // We now blend the walk animation with the idle/attack one
    if(player->state.s == STATE_ATTACK)
        t3d_skeleton_blend(player->skel, player->skel, player->skelBlend, animBlend);
    t3d_skeleton_update(player->skel);


    // Run collision after AABB refresh so overlap tests use current frame positions.
    collision_detect(player, entities, numPlayers, state);
    t3d_mat4fp_from_srt_euler(&player->e.modelMatFP[frameIdx],
                              (float[3]){0.125f, 0.125f, 0.125f},
                              (float[3]){0.0f, -player->rotY, 0},
                              player->e.pos.v);
}
