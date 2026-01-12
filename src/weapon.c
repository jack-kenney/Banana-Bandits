#include "entities.h"
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <t3d/t3dskeleton.h>

extern Weapon pipes[];

#define FB_COUNT 3

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

        // Prefer attaching to the animated hand bone.
        // `t3d_skeleton_get_bone_pos_model_space` returns a position in *model space*,
        // so we must transform it into *world space* using the same SRT matrix as the player render.
        if (p->handBoneIdx >= 0)
        {
            pipe->boneIndexWeapon = p->handBoneIdx;
            T3DVec3 bonePos = t3d_skeleton_get_bone_pos_model_space(p->skel, p->handBoneIdx);

            T3DMat4 playerMat;
            t3d_mat4_from_srt_euler(&playerMat,
                                    (float[3]){0.125f, 0.125f, 0.125f},
                                    (float[3]){0.0f, -p->rotY, 0.0f},
                                    p->playerPos.v);

            T3DVec4 boneWorld;
            t3d_mat4_mul_vec3(&boneWorld, &playerMat, &bonePos);
            pipe->wepPos = (T3DVec3){{boneWorld.v[0], boneWorld.v[1], boneWorld.v[2]}};
            //debugf("Weapon attached to bone index %d at world position (%f, %f, %f)\n",
                   //p->handBoneIdx, pipe->wepPos.v[0], pipe->wepPos.v[1], pipe->wepPos.v[2]);
            //debugf("Bone model space position: (%f, %f, %f)\n",
                   //bonePos.v[0], bonePos.v[1], bonePos.v[2]);
        }
        else
        {
            // Fallback if we couldn't resolve a hand bone
            pipe->wepPos = p->playerPos;
        }

        // Adjust weapon attachment height
        pipe->wepPos.v[1] -= 35.0f;

        if (p->attacking)
        {
            pipe->isAttack = true;
            T3DVec3 attackDir;
            t3d_vec3_scale(&attackDir, &p->moveDir, 50.0f);
            t3d_vec3_add(pipe->hit, &pipe->wepPos, &attackDir);
            debugf("pipe attackframe: %d\n", pipe->attackFrame);

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
            debugf("pipe attackframe: %d\n", pipe->attackFrame);
            pipe->attackFrame = 0;
            pitch = 0;
        }

        // Update the weapon's render matrix for this frame.
        t3d_mat4fp_from_srt_euler(&pipe->modelMatFP[frameIdx],
                                  (float[3]){0.5f, 0.5f, 0.5f},
                                  (float[3]){0, -pipe->rotY + (T3D_PI / 2), pitch},
                                  pipe->wepPos.v);
    }
    else
    {
        // keep model matrix in sync with world position when not equipped
        t3d_mat4fp_from_srt_euler(&pipe->modelMatFP[frameIdx],
                                  (float[3]){0.5f, 0.5f, 0.5f},
                                  (float[3]){0.0f, globalYrot, 0.0f},
                                  pipe->wepPos.v);
    }
}
