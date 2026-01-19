#include "entities.h"
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <stdlib.h>
#include <string.h>
#include <t3d/t3dskeleton.h>
#include "collision.h"

#define FB_COUNT 3
#define PLAYER_AABB_HEIGHT 90.0f
#define PLAYER_AABB_WIDTH 30.0f

Player players[4];

static inline void player_refresh_aabb(Player *player)
{
    player->aabb.min.v[0] = player->playerPos.v[0] - PLAYER_AABB_WIDTH / 2.0f;
    player->aabb.max.v[0] = player->playerPos.v[0] + PLAYER_AABB_WIDTH / 2.0f;
    player->aabb.min.v[2] = player->playerPos.v[2] - PLAYER_AABB_WIDTH / 2.0f;
    player->aabb.max.v[2] = player->playerPos.v[2] + PLAYER_AABB_WIDTH / 2.0f;
    player->aabb.min.v[1] = player->playerPos.v[1];
    player->aabb.max.v[1] = player->playerPos.v[1] + PLAYER_AABB_HEIGHT;
}

void player_init(Player *player, T3DVec3 position, T3DModel *model)
{
    player->modelMatFP = malloc_uncached(sizeof(T3DMat4FP) * FB_COUNT);
    player->moveDir = (T3DVec3){{0, 0, 0}};
    player->playerPos = position;
    player->currSpeed = 0.0f;
    player->rotY = 0.0f;
    player->jumpFrame = 0;
    player->asc = false;
    player->alive = true;
    player->isHittable = 0;
    player->hitpoints = 100.0f;
    player->weapon = NULL;
    player->hasWeapon = false;
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

    player->animPunch = t3d_anim_create(model, "bananaPunch3");
    t3d_anim_set_looping(&player->animPunch, false);
    t3d_anim_set_playing(&player->animPunch, false);
    t3d_anim_set_speed(&player->animPunch, 2.0f);
    t3d_anim_attach(&player->animPunch, player->skel);

    // model = t3d_model_load("rom:/banana.t3dm");
    rspq_block_begin();
    rdpq_set_prim_color(RGBA32(255, 255, 255, 255));
    t3d_model_draw_skinned(model, player->skel);
    player->dplPlayer = rspq_block_end();
}

void set_player_state(Player *player, PlayerState newState)
{
    player->state = newState;
    //player->state.frame = 0;
}

// Cleanup player resources
void player_cleanup(Player *player)
{
    rspq_block_free(player->dplPlayer);

    t3d_skeleton_destroy(player->skel);
    t3d_skeleton_destroy(player->skelBlend);

    free_uncached(player->modelMatFP);
}

// internal helper: collision detection
static void collision_detect(Player *player)
{
    for (int i = 0; i < 2; i++)
    {
        if (player == pipes[i].attachedPlayer)
            continue;
        if (player->hasWeapon)
            continue;
        if (pipes[i].equipped)
            continue;

        // Use AABB overlap for pickup (player world AABB vs weapon pickup AABB).
        if (!aabbf_overlaps(&player->aabb, &pipes[i].aabb))
            continue;

        player->hasWeapon = true;
        player->weapon = &pipes[i];
        pipes[i].equipped = true;
        pipes[i].attachedPlayer = player;
    }

    if (player->isHittable > 0)
    {
        player->isHittable -= 1;
    }

    for (int i = 0; i < 4; i++)
    {
        // Resolve each pair only once (collision_detect runs per-player).
        if (&players[i] <= player)
            continue;
        if (!players[i].alive)
            continue;

        if (!aabbf_overlaps(&player->aabb, &players[i].aabb))
            continue;

        // Minimum-translation-vector (MTV) resolution in XZ.
        const float aMinX = player->aabb.min.v[0];
        const float aMaxX = player->aabb.max.v[0];
        const float aMinZ = player->aabb.min.v[2];
        const float aMaxZ = player->aabb.max.v[2];

        const float bMinX = players[i].aabb.min.v[0];
        const float bMaxX = players[i].aabb.max.v[0];
        const float bMinZ = players[i].aabb.min.v[2];
        const float bMaxZ = players[i].aabb.max.v[2];

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
        player->playerPos.v[0] += 0.5f * pushX;
        player->playerPos.v[2] += 0.5f * pushZ;
        players[i].playerPos.v[0] -= 0.5f * pushX;
        players[i].playerPos.v[2] -= 0.5f * pushZ;

        player_refresh_aabb(player);
        player_refresh_aabb(&players[i]);
    }
}

void player_update(Player *player, joypad_port_t port, T3DVec3 *camPos, int frameIdx, float deltaTime)
{
    float speed = 0.0f;
    T3DVec3 newDir = {0};
    joypad_inputs_t joypad = joypad_get_inputs(port);
    joypad_buttons_t joybtns = joypad_get_buttons_pressed(port);
    newDir.v[0] = (float)joypad.stick_x * 0.05f;
    newDir.v[2] = -(float)joypad.stick_y * 0.05f;
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
    if (joypad.btn.z)
        player->currSpeed = 5.0f;
    player->playerPos.v[0] += player->moveDir.v[0] * player->currSpeed;
    player->playerPos.v[2] += player->moveDir.v[2] * player->currSpeed;

    const float BOX_SIZE = 240.0f;
    if (player->playerPos.v[0] < -BOX_SIZE)
        player->playerPos.v[0] = -BOX_SIZE;
    if (player->playerPos.v[0] > BOX_SIZE)
        player->playerPos.v[0] = BOX_SIZE;
    if (player->playerPos.v[2] < -BOX_SIZE)
        player->playerPos.v[2] = -BOX_SIZE;
    if (player->playerPos.v[2] > BOX_SIZE)
        player->playerPos.v[2] = BOX_SIZE;

    if (joybtns.b && !(player->state.s == STATE_ATTACK))
    {
        // State refactor, delete these later
        // player->attacking = true;
        // player->attackFrame = 0;
        set_player_state(player, (PlayerState){.s = STATE_ATTACK, .frame = 0});
        if (player->weapon)
            player->weapon->isAttack = true;
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

    if (joybtns.a && !player->asc && player->jumpFrame == 0)
        player->asc = true;

    if (player->asc) // && !joypad.btn.a)
    {
        if (player->jumpFrame < 5)
        {
            player->playerPos.v[1] += JUMP_HEIGHT;
            player->jumpFrame++;
        }
        else if (player->jumpFrame >= 5 && player->jumpFrame < 10)
        {
            player->playerPos.v[1] -= JUMP_HEIGHT;
            player->jumpFrame++;
        }
        else
        {
            player->jumpFrame = 0;
            player->asc = false;
        }
    }

    // Keep the gameplay/debug AABB tied to the final position (including vertical movement).
    player_refresh_aabb(player);

    if (joypad.btn.c_right)
    {
        camPos->v[1] += 2.0f;
    }

    if (joypad.btn.c_left)
    {
        camPos->v[1] -= 2.0f;
    }

    if (joybtns.c_up)
    {
        if (player->hasWeapon)
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
            dropped->wepPos.v[0] = player->playerPos.v[0] + dropDir.v[0] * dropDistance;
            dropped->wepPos.v[2] = player->playerPos.v[2] + dropDir.v[2] * dropDistance;
            dropped->wepPos.v[1] = 0.15f;

            // Keep within arena bounds.
            const float BOX_SIZE = 240.0f;
            if (dropped->wepPos.v[0] < -BOX_SIZE)
                dropped->wepPos.v[0] = -BOX_SIZE;
            if (dropped->wepPos.v[0] > BOX_SIZE)
                dropped->wepPos.v[0] = BOX_SIZE;
            if (dropped->wepPos.v[2] < -BOX_SIZE)
                dropped->wepPos.v[2] = -BOX_SIZE;
            if (dropped->wepPos.v[2] > BOX_SIZE)
                dropped->wepPos.v[2] = BOX_SIZE;

            if (dropped->hit)
                *dropped->hit = dropped->wepPos;
            weapon_refresh_aabb(dropped);

            player->hasWeapon = false;
            player->weapon = NULL;
        }
    }
    // Update base pose
    t3d_anim_update(&player->animIdle, deltaTime);

    // Attack overrides base while active
    if (player->state.s == STATE_ATTACK)
    {
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
    else
    {
        // Ensure next attack starts from the beginning
        if (player->animPunch.isPlaying)
        {
            t3d_anim_set_playing(&player->animPunch, false);
        }
        t3d_anim_set_time(&player->animPunch, 0.0f);
    }
    // NOTE: Buffered skeleton matrices can switch to a new matrix buffer when any bone changes.
    // Forcing the root bone as dirty ensures the full hierarchy gets valid matrices every frame.
    player->skel->bones[0].hasChanged = true;
    t3d_skeleton_update(player->skel);
    // Run collision after AABB refresh so overlap tests use current frame positions.
    collision_detect(player);
    t3d_mat4fp_from_srt_euler(&player->modelMatFP[frameIdx],
                              (float[3]){0.125f, 0.125f, 0.125f},
                              (float[3]){0.0f, -player->rotY, 0},
                              player->playerPos.v);
}
