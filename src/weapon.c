#include "weapon.h"
#include "player.h"
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <t3d/t3dskeleton.h>
#include "collision.h"

extern wav64_t dominating, smack1, smack2, smack3, smack4;

#define FB_COUNT 3

// Weapon AABB dimensions (world units).
// - When attacking: represents the active hitbox (centered at `weapon->hit`).
// - Otherwise: represents the pickup volume (centered at `weapon->wepPos`).
static const float WEAPON_HIT_AABB_HALF_EXTENT_XZ = 10.0f;
static const float WEAPON_HIT_AABB_HALF_EXTENT_Y = 30.0f;
static const float WEAPON_PICKUP_AABB_HALF_EXTENT_XZ = 15.0f;
static const float WEAPON_PICKUP_AABB_HALF_EXTENT_Y = 45.0f;
static const float WEAPON_AABB_Y_OFFSET = 50.0f;
// Weapon grip offset relative to the hand bone.
// Tune these to align the weapon model's axes with the bone's axes.
// Compensate for player scale (0.125) so weapon stays at its intended size (0.5 world scale).
static const float WEAPON_GRIP_SCALE[3] = {4.0f, 4.0f, 4.0f};
static const float WEAPON_GRIP_ROT[3] = {0.0f, T3D_PI / 2.0f, 0.0f};
static const float WEAPON_GRIP_OFFSET[3] = {0.0f, 0.0f, 0.0f};

void weapon_refresh_aabb(Weapon *weapon)
{
    const bool useHitbox = (weapon->isAttack && weapon->hit);
    const T3DVec3 *center = useHitbox ? weapon->hit : &weapon->e.pos;

    const float hx = useHitbox ? WEAPON_HIT_AABB_HALF_EXTENT_XZ : WEAPON_PICKUP_AABB_HALF_EXTENT_XZ;
    const float hy = useHitbox ? WEAPON_HIT_AABB_HALF_EXTENT_Y : WEAPON_PICKUP_AABB_HALF_EXTENT_Y;

    weapon->aabb.min.v[0] = center->v[0] - hx;
    weapon->aabb.max.v[0] = center->v[0] + hx;
    weapon->aabb.min.v[2] = center->v[2] - hx;
    weapon->aabb.max.v[2] = center->v[2] + hx;
    const float cy = center->v[1] + WEAPON_AABB_Y_OFFSET;
    weapon->aabb.min.v[1] = cy - hy;
    weapon->aabb.max.v[1] = cy + hy;
}

void weapon_init(Entity *e, T3DVec3 position, T3DModel *model)
{
    Weapon *weapon = (Weapon *)e;
    weapon->e.type = E_WEAPON;
    weapon->e.modelMatFP = malloc_uncached(sizeof(T3DMat4FP) * FB_COUNT);
    weapon->e.pos = position;
    weapon->equipped = false;
    weapon->attachedPlayer = NULL;
    weapon->damage = 100.0f;
    weapon->isAttack = false;
    weapon->attackFrame = 0;
    weapon->bobFrame = 0;
    weapon->boneIndexWeapon = -1;
    weapon->hit = malloc_uncached(sizeof(T3DVec3));
    if (weapon->hit)
        *weapon->hit = position;

    weapon_refresh_aabb(weapon);

    // Record a matrix-free display list; caller pushes the correct matrix per frame.
    rspq_block_begin();
    rdpq_set_prim_color(RGBA32(255, 255, 255, 255));
    t3d_model_draw(model);
    weapon->e.dplEntity = rspq_block_end();

    // debugf("Weapon initialized and DPLs created.\n");
    // debugf("Weapon initialized at position: (%f, %f, %f)\n", position.v[0], position.v[1], position.v[2]);
}

// Cleanup weapon resources
void weapon_cleanup(Weapon *weapon)
{
    if (weapon->e.dplEntity)
        rspq_block_free(weapon->e.dplEntity);
    if (weapon->e.modelMatFP)
        free_uncached(weapon->e.modelMatFP);
    if (weapon->hit)
        free_uncached(weapon->hit);

    weapon->e.dplEntity = NULL;
    weapon->e.modelMatFP = NULL;
    weapon->hit = NULL;
    weapon->attachedPlayer = NULL;
    weapon->equipped = false;
}

void weapon_entity_cleanup(Entity *e)
{
    if (!e)
        return;
    weapon_cleanup((Weapon *)e);
}

void weapon_entity_update(Entity *e, const EntityUpdateContext *ctx)
{
    if (!e || !ctx)
        return;
    pipe_movement((Weapon *)e, ctx->globalYrot, ctx->frameIdx, ctx->entities, ctx->numPlayers);
}

void weapon_draw_hitbubble(Weapon *weapon, T3DMat4FP *hitbubbleFP, rspq_block_t *dplHitbubble)
{
    if (!weapon || !hitbubbleFP || !dplHitbubble || !weapon->hit)
        return;

    t3d_mat4fp_from_srt_euler(hitbubbleFP,
                              (float[3]){0.1f, 0.1f, 0.1f},
                              (float[3]){0.0f, 0.0f, 0.0f},
                              weapon->hit->v);
    rspq_block_run(dplHitbubble);
}
void pipe_movement(Weapon *pipe, float globalYrot, int frameIdx, Entity * entities[], int numPlayers)
{
    pipe->bobFrame += 1;
    if (pipe->bobFrame >= 30)
        pipe->bobFrame = 0;
    float pitch = 0.0f;
    if (pipe->equipped && pipe->attachedPlayer)
    {
        Player *p = pipe->attachedPlayer;
        pipe->rotY = p->rotY;
        bool hasBoneMatrix = false;
        T3DMat4 worldBoneMat;
        T3DMat4 weaponBaseMat;

        if (p->handBoneIdx >= 0)
        {
            pipe->boneIndexWeapon = p->handBoneIdx;
            T3DMat4 *boneMat = &p->skel->bones[p->handBoneIdx].matrix;
            T3DMat4 playerMat;
            t3d_mat4_from_srt_euler(&playerMat,
                                    (float[3]){0.125f, 0.125f, 0.125f},
                                    (float[3]){0.0f, -p->rotY, 0.0f},
                                    p->e.pos.v);

            // Bone matrix is in model space. Compose with player transform to get world space.
            t3d_mat4_mul(&worldBoneMat, &playerMat, boneMat);

            // Apply grip offset to align weapon axes with the hand bone axes.
            T3DMat4 gripMat;
            t3d_mat4_from_srt_euler(&gripMat, WEAPON_GRIP_SCALE, WEAPON_GRIP_ROT, WEAPON_GRIP_OFFSET);
            t3d_mat4_mul(&weaponBaseMat, &worldBoneMat, &gripMat);
            pipe->e.pos = (T3DVec3){{weaponBaseMat.m[3][0], weaponBaseMat.m[3][1], weaponBaseMat.m[3][2]}};
            hasBoneMatrix = true;
        }
        else
        {
            // Fallback if we couldn't resolve a hand bone
            pipe->e.pos = p->e.pos;
        }

        // Keep weapon AABB in sync; during attacks it's centered at the hit point.
        weapon_refresh_aabb(pipe);
        T3DVec3 attackDir;
        t3d_vec3_scale(&attackDir, &p->moveDir, 1.0f);
        t3d_vec3_add(pipe->hit, &pipe->e.pos, &attackDir);
        // Apply weapon hits via AABB overlap against player AABBs.
        if (pipe->isAttack && p->state.frame >= 5 && p->state.frame <= 15 && (p->state.s == STATE_ATTACK || p->state.s == STATE_ATTACK2))
        {

            for (int i = 0; i < numPlayers; i++)
            {
                Player *target = (Player *)entities[i];
                if (!target->alive)
                    continue;
                if (target == p)
                    continue;
                if (target->isHittable != 0)
                    continue;

                if (!aabbf_overlaps(&pipe->aabb, &target->aabb))
                    continue;

                set_player_state(target, (PlayerState){.s = STATE_HITLAG, .frame = 0});
                set_player_state(p, (PlayerState){.s = STATE_HITLAG, .frame = p->state.frame});
                target->isHittable = 16;
                target->hitpoints -= pipe->damage;
                int playIdx = rand() % 4;
                switch (playIdx)
                {
                    case 0:
                        wav64_play(&smack1, 22);
                        break;
                    case 1:
                        wav64_play(&smack2, 24);
                        break;
                    case 2:
                        wav64_play(&smack3, 26);
                        break;
                    case 3:
                        wav64_play(&smack4, 28);
                        break;
                }
                if (target->hitpoints <= 0.0f)
                {
                    wav64_play(&dominating, 30);
                    target->alive = false;
                    if(target->weapon)
                    {
                        drop_weapon(target);
                    }
                }
            }
        }

        // Update the weapon's render matrix for this frame.
        if (hasBoneMatrix)
        {
            T3DMat4 attackMat;
            t3d_mat4_from_srt_euler(&attackMat,
                                    (float[3]){1.0f, 1.0f, 1.0f},
                                    (float[3]){0.0f, 0.0f, pitch},
                                    (float[3]){0.0f, 0.0f, 0.0f});

            T3DMat4 weaponMat;
            t3d_mat4_mul(&weaponMat, &weaponBaseMat, &attackMat);
            t3d_mat4_to_fixed_3x4(&pipe->e.modelMatFP[frameIdx], &weaponMat);
        }
        else
        {
            t3d_mat4fp_from_srt_euler(&pipe->e.modelMatFP[frameIdx],
                                      (float[3]){0.5f, 0.5f, 0.5f},
                                      (float[3]){0, -pipe->rotY + (T3D_PI / 2), pitch},
                                      pipe->e.pos.v);
        }

    }
    else
    {
        if (pipe->hit)
            *pipe->hit = pipe->e.pos;

        weapon_refresh_aabb(pipe);

        // keep model matrix in sync with world position when not equipped
        t3d_mat4fp_from_srt_euler(&pipe->e.modelMatFP[frameIdx],
                                  (float[3]){0.5f, 0.5f, 0.5f},
                                  (float[3]){0.0f, globalYrot, 0.0f},
                                  pipe->e.pos.v);
    }
}
