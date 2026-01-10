#include "entities.h"
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern Weapon pipes[];

void weapon_init(Weapon *weapon, T3DVec3 position, T3DModel *model)
{
    weapon->modelMatFP = malloc_uncached(sizeof(T3DMat4FP));
    weapon->wepPos = position;
    weapon->equipped = false;
    weapon->attachedPlayer = malloc_uncached(sizeof(Player));
     memset(weapon->attachedPlayer, 0, sizeof(Player))  ;
    weapon->damage = 10.0f;
    weapon->isAttack = false;
    weapon->attackFrame = 0;
    weapon->bobFrame = 0;
    weapon->hit = malloc_uncached(sizeof(T3DVec3));
    rspq_block_begin();
        t3d_matrix_push(weapon->modelMatFP);
        t3d_model_draw(model); // as in the last example, draw skinned with the main skeleton
        t3d_matrix_pop(1);
    weapon->dplIdle = rspq_block_end();
        rspq_block_begin();
        t3d_matrix_push(weapon->attachedPlayer->modelMatFP);
        t3d_matrix_push(weapon->modelMatFP);
        t3d_model_draw(model); // as in the last example, draw skinned with the main skeleton
        t3d_matrix_pop(1);
        t3d_matrix_pop(1);
    weapon->dplCarry = rspq_block_end();
    weapon->dplWeapon = weapon->dplIdle;
    //debugf("Weapon initialized and DPLs created.\n");
    //debugf("Weapon initialized at position: (%f, %f, %f)\n", position.v[0], position.v[1], position.v[2]);
}

void pipe_movement(Weapon *pipe, float globalYrot)
{
    pipe->bobFrame += 1;
    if(pipe->bobFrame >= 30) pipe->bobFrame = 0;
    float attackRotation = 0.0f;
    float pitch = 0.0f;
    //debugf("equipped: %d, attachedPlayer: %p\n", pipe->equipped, (void*)pipe->attachedPlayer);
    if (pipe->equipped && pipe->attachedPlayer)
    {
        //debugf("pipe->rotY: %f\n", pipe->rotY);
        //pipe->dplWeapon = pipe->dplCarry;
        /*pipe->wepPos.v[0] = pipe->attachedPlayer->playerPos.v[0] + 5.0f;
        pipe->wepPos.v[1] = pipe->attachedPlayer->playerPos.v[1] + 10.0f;
        pipe->wepPos.v[2] = pipe->attachedPlayer->playerPos.v[2] + 5.0f;
        debugf("Pipe is at position: (%f, %f, %f)\n", pipe->wepPos.v[0], pipe->wepPos.v[1], pipe->wepPos.v[2]);*/
        Player *p = pipe->attachedPlayer;
        float angle = p->rotY;
        // right vector based on angle
        float right_x = cosf(angle);
        float right_z = -sinf(angle);

        // offsets
        const float lateral_offset = -10.0f;
        const float forward_offset = 8.0f;
        float vertical_offset;
        if(pipe->bobFrame >= 15 ) vertical_offset = pipe->bobFrame * 0.3f;
        else (vertical_offset = (30 - pipe->bobFrame) * 0.3f);

        // forward vector (player facing)
        float forward_x = sinf(angle);
        float forward_z = cosf(angle);

        // compute world position: player + right*lateral + forward*forward + up*vertical
        pipe->wepPos.v[0] = p->playerPos.v[0] + right_x * lateral_offset + forward_x * forward_offset;
        pipe->wepPos.v[1] = p->playerPos.v[1] + vertical_offset;
        pipe->wepPos.v[2] = p->playerPos.v[2] + right_z * lateral_offset + forward_z * forward_offset;

        pipe->rotY = p->rotY;

        if(p->attacking)
        {
            //debugf("Weapon position during attack: (%f, %f, %f)\n", pipe->wepPos.v[0], pipe->wepPos.v[1], pipe->wepPos.v[2]);
            //debugf("Player attack frame: %i\n", p->attackFrame);
            //debugf("Pipe attack frame:   %i\n", pipe->attackFrame);
            pipe->isAttack = true;
            T3DVec3 attackDir;
            t3d_vec3_scale(&attackDir, &p->moveDir, 50.0f);
            //debugf("Scaled attack direction: (%f, %f, %f)\n", attackDir.v[0], attackDir.v[1], attackDir.v[2]);
            t3d_vec3_add(pipe->hit, &pipe->wepPos, &attackDir); // Example scaling, adjust as needed
            //debugf("Attack hit position: (%f, %f, %f)\n", pipe->hit->v[0], pipe->hit->v[1], pipe->hit->v[2]);
            if(pipe->attackFrame >= ATK_LENGTH * 2)
            {
                pipe->attackFrame = 0;
                pipe->isAttack = false;
            }
            if(pipe->attackFrame < ATK_LENGTH && pipe->isAttack)
            {
                pipe->attackFrame += 1;
                attackRotation = ((T3D_PI / 2) / ATK_LENGTH) * pipe->attackFrame;
            }
            else if (pipe->attackFrame >= ATK_LENGTH && pipe->isAttack)
            {
                pipe->attackFrame += 1;
                attackRotation = (T3D_PI / 2) - (((T3D_PI / 2) / ATK_LENGTH) * (pipe->attackFrame - ATK_LENGTH));
            }
            // pitch forwards during the attack
            pitch = attackRotation;
        }
        else
        {
            pitch = 0;
        }

        /*
        // Build pitched forward direction by rotating the forward vector around the right axis
        T3DVec3 f = (T3DVec3){{forward_x, 0.0f, forward_z}};
        T3DVec3 r = (T3DVec3){{right_x, 0.0f, right_z}};
        t3d_vec3_norm(&f);
        t3d_vec3_norm(&r);

        T3DVec3 cross_rf;
        t3d_vec3_cross(&cross_rf, &r, &f); // cross(r, f)

        float c = cosf(pitch);
        float s = sinf(pitch);

        T3DVec3 dir_rot = {
            .v = {
                f.v[0] * c + cross_rf.v[0] * s,
                f.v[1] * c + cross_rf.v[1] * s,
                f.v[2] * c + cross_rf.v[2] * s
            }
        };
        t3d_vec3_norm(&dir_rot);

        // Create rotation matrix that looks along dir_rot, with world-up (0,1,0)
        T3DMat4 mat;
        t3d_mat4_rot_from_dir(&mat, &dir_rot, &(T3DVec3){{0,1,0}});

        // apply scale (0.5) to the rotation basis (rows 0..2) and set translation
        float sx = 0.5f, sy = 0.5f, sz = 0.5f;
        mat.m[0][0] *= sx; mat.m[0][1] *= sx; mat.m[0][2] *= sx;
        mat.m[1][0] *= sy; mat.m[1][1] *= sy; mat.m[1][2] *= sy;
        mat.m[2][0] *= sz; mat.m[2][1] *= sz; mat.m[2][2] *= sz;

        mat.m[3][0] = pipe->wepPos.v[0];
        mat.m[3][1] = pipe->wepPos.v[1];
        mat.m[3][2] = pipe->wepPos.v[2];
        mat.m[3][3] = 1.0f;

        t3d_mat4_to_fixed_3x4(pipe->modelMatFP, &mat);*/
        
        //debugf("rotY: %f\n", pipe->rotY );
     
        t3d_mat4fp_from_srt_euler(pipe->modelMatFP,
            (float[3]){0.5f, 0.5f, 0.5f},
            (float[3]){0, -pipe->rotY + (T3D_PI / 2), pitch},
            pipe->wepPos.v);
        }
    else
    {
        // keep model matrix in sync with world position when not equipped
        t3d_mat4fp_from_srt_euler(pipe->modelMatFP,
            (float[3]){0.5f, 0.5f, 0.5f},
            (float[3]){0.0f, globalYrot, 0.0f},
            pipe->wepPos.v);
    }
}