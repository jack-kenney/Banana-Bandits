#include "entities.h"
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <t3d/t3dskeleton.h>
#include "collision.h"

extern Weapon pipes[];
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
    const T3DVec3 *center = useHitbox ? weapon->hit : &weapon->wepPos;

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

void weapon_init(Weapon *weapon, T3DVec3 position, T3DModel *model)
{
    weapon->modelMatFP = malloc_uncached(sizeof(T3DMat4FP) * FB_COUNT);
    weapon->wepPos = position;
    weapon->equipped = false;
    weapon->attachedPlayer = NULL;
    weapon->damage = 10.0f;
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
    weapon->dplWeapon = rspq_block_end();
    weapon->dplIdle = weapon->dplWeapon;
    weapon->dplCarry = weapon->dplWeapon;
    // debugf("Weapon initialized and DPLs created.\n");
    // debugf("Weapon initialized at position: (%f, %f, %f)\n", position.v[0], position.v[1], position.v[2]);
}

// Cleanup weapon resources
void weapon_cleanup(Weapon *weapon)
{
    if (weapon->dplWeapon)
        rspq_block_free(weapon->dplWeapon);
    if (weapon->modelMatFP)
        free_uncached(weapon->modelMatFP);
    if (weapon->hit)
        free_uncached(weapon->hit);

    weapon->dplWeapon = NULL;
    weapon->dplIdle = NULL;
    weapon->dplCarry = NULL;
    weapon->modelMatFP = NULL;
    weapon->hit = NULL;
    weapon->attachedPlayer = NULL;
    weapon->equipped = false;
}
void pipe_movement(Weapon *pipe, float globalYrot, int frameIdx)
{
    pipe->bobFrame += 1;
    if (pipe->bobFrame >= 30)
        pipe->bobFrame = 0;
    float attackRotation = 0.0f;
    float pitch = 0.0f;
    // debugf("equipped: %d, attachedPlayer: %p\n", pipe->equipped, (void*)pipe->attachedPlayer);
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
                                    p->playerPos.v);

            // Bone matrix is in model space. Compose with player transform to get world space.
            t3d_mat4_mul(&worldBoneMat, &playerMat, boneMat);

            // Apply grip offset to align weapon axes with the hand bone axes.
            T3DMat4 gripMat;
            t3d_mat4_from_srt_euler(&gripMat, WEAPON_GRIP_SCALE, WEAPON_GRIP_ROT, WEAPON_GRIP_OFFSET);
            t3d_mat4_mul(&weaponBaseMat, &worldBoneMat, &gripMat);
            pipe->wepPos = (T3DVec3){{weaponBaseMat.m[3][0], weaponBaseMat.m[3][1], weaponBaseMat.m[3][2]}};
            hasBoneMatrix = true;
            // debugf("Weapon attached to bone index %d at world position (%f, %f, %f)\n",
            // p->handBoneIdx, pipe->wepPos.v[0], pipe->wepPos.v[1], pipe->wepPos.v[2]);
        }
        else
        {
            // Fallback if we couldn't resolve a hand bone
            pipe->wepPos = p->playerPos;
        }

        // Adjust weapon attachment height
        //pipe->wepPos.v[1] -= 35.0f;
        /*
        if (p->state.s == STATE_ATTACK)
        {
            pipe->isAttack = true;
            T3DVec3 attackDir;
            t3d_vec3_scale(&attackDir, &p->moveDir, 50.0f);
            t3d_vec3_add(pipe->hit, &pipe->wepPos, &attackDir);
            if (pipe->attackFrame >= ATK_LENGTH * 2)
            {
                pipe->attackFrame = 0;
                pipe->isAttack = false;
            }
            if (pipe->attackFrame < ATK_LENGTH && pipe->isAttack)
            {
                pipe->attackFrame += 2;
                attackRotation = ((T3D_PI / 2) / ATK_LENGTH) * pipe->attackFrame;
            }
            else if (pipe->attackFrame >= ATK_LENGTH && pipe->isAttack)
            {
                pipe->attackFrame += 2;
                attackRotation = (T3D_PI / 2) - (((T3D_PI / 2) / ATK_LENGTH) * (pipe->attackFrame - ATK_LENGTH));
            }
            pitch = attackRotation;
        }
        else
        {
            pipe->attackFrame = 0;
            pipe->isAttack = false;
            pitch = 0;
            if (pipe->hit)
                *pipe->hit = pipe->wepPos;
        }
        */
        // Keep weapon AABB in sync; during attacks it's centered at the hit point.
        weapon_refresh_aabb(pipe);
        T3DVec3 attackDir;
        t3d_vec3_scale(&attackDir, &p->moveDir, 1.0f);
        t3d_vec3_add(pipe->hit, &pipe->wepPos, &attackDir);
        // Apply weapon hits via AABB overlap against player AABBs.
        if (pipe->isAttack && p->state.frame >= 5 && p->state.frame <= 15 && p->state.s == STATE_ATTACK)
        {

            for (int i = 0; i < 4; i++)
            {
                Player *target = &players[i];
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
                        wav64_play(&smack1, 28);
                        break;
                    case 1:
                        wav64_play(&smack2, 28);
                        break;
                    case 2:
                        wav64_play(&smack3, 28);
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
            t3d_mat4_to_fixed_3x4(&pipe->modelMatFP[frameIdx], &weaponMat);
        }
        else
        {
            t3d_mat4fp_from_srt_euler(&pipe->modelMatFP[frameIdx],
                                      (float[3]){0.5f, 0.5f, 0.5f},
                                      (float[3]){0, -pipe->rotY + (T3D_PI / 2), pitch},
                                      pipe->wepPos.v);
        }

    }
    else
    {
        if (pipe->hit)
            *pipe->hit = pipe->wepPos;

        weapon_refresh_aabb(pipe);

        // keep model matrix in sync with world position when not equipped
        t3d_mat4fp_from_srt_euler(&pipe->modelMatFP[frameIdx],
                                  (float[3]){0.5f, 0.5f, 0.5f},
                                  (float[3]){0.0f, globalYrot, 0.0f},
                                  pipe->wepPos.v);
    }
}
