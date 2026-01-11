#include "entities.h"
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <stdlib.h>
#include <string.h>
#include <t3d/t3dskeleton.h>

#define FB_COUNT 3

Player players[4];

// internal helper: collision detection
static void collision_detect(Player *player)
{
    for(int i = 0; i < 2; i++)
    {

        if(player != pipes[i].attachedPlayer)
        {
            float dist = t3d_vec3_distance(&player->playerPos, &pipes[i].wepPos);
            if(dist < 15.0f && !player->hasWeapon)
            {
                if(!pipes[i].equipped)
                {
                    player->hasWeapon = true;
                    player->weapon = &pipes[i];
                    pipes[i].equipped = true;
                    pipes[i].attachedPlayer = player;
                }
            }
        }
    }

    //debugf("model vertex chunk  : %c\n", model->chunkOffsets[model->chunkIdxVertices].type);

    if(player->isHittable > 0)
    {
        player->isHittable -= 1;
    }
    for(int i = 0; i < 4; i++)
    {
        if(player != &players[i])
        {
            for(int j = 0; j < 2; j++)
            {
                float diff = t3d_vec3_distance(pipes[j].hit, &players[i].playerPos);
                //debugf("Distance from attack hit to Player %d: %f\n", i, diff); 
                if(diff < 50.0f)
                {
                    if(player->attacking && players[i].isHittable == 0 && players[i].alive && &pipes[j] == player->weapon)
                    {
                        players[i].isHittable = 15;
                        players[i].hitpoints -= player->weapon->damage;
                        if(players[i].hitpoints <= 0.0f && players[i].alive)
                        {
                            players[i].alive = false;
                        }
                        debugf("Player %p hit Player %p with damage %f\n", (void*)player, (void*)&players[i], player->weapon->damage);
                        debugf("Player %d hitpoints remaining: %f\n", i, players[i].hitpoints);
                        //player->attacking = false; // prevent multiple hits per attack
                    }
                }
            }
        }
    }
}

void player_init(Player *player, T3DVec3 position, T3DModel *model)
{
    player->modelMatFP = malloc_uncached(sizeof(T3DMat4FP) * FB_COUNT);
    player->moveDir = (T3DVec3){{0,0,0}};
    player->playerPos = position;
    player->currSpeed = 0.0f;
    player->rotY = 0.0f;
    player->jumpFrame = 0;
    player->alive = true;
    player->isHittable = 0;
    player->hitpoints = 100.0f;
    player->attacking = false;
    player->attackFrame = 0;
    player->weapon = malloc_uncached(sizeof(Weapon));
    player->skel = malloc_uncached(sizeof(*player->skel));
    *player->skel = t3d_skeleton_create_buffered(model, FB_COUNT);
    //model = t3d_model_load("rom:/banana.t3dm");
    rspq_block_begin();     
        rdpq_set_prim_color(RGBA32(255, 255, 255, 255));
        t3d_model_draw_skinned(model, player->skel);
    player->dplPlayer = rspq_block_end();
}

void player_update(Player *player, joypad_port_t port, T3DVec3 *camPos, int frameIdx)
{
    float speed = 0.0f;
    T3DVec3 newDir = {0};
    joypad_inputs_t joypad = joypad_get_inputs(port);

    newDir.v[0] = (float)joypad.stick_x * 0.05f;
    newDir.v[2] = -(float)joypad.stick_y * 0.05f;
    speed = sqrtf(t3d_vec3_len2(&newDir));
    if(speed > 0.15f)
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

    if (joypad.btn.z) player->currSpeed = 5.0f;
    player->playerPos.v[0] += player->moveDir.v[0] * player->currSpeed;
    player->playerPos.v[2] += player->moveDir.v[2] * player->currSpeed;
 
    const float BOX_SIZE = 140.0f;
    if(player->playerPos.v[0] < -BOX_SIZE) player->playerPos.v[0] = -BOX_SIZE;
    if(player->playerPos.v[0] >  BOX_SIZE) player->playerPos.v[0] =  BOX_SIZE;
    if(player->playerPos.v[2] < -BOX_SIZE) player->playerPos.v[2] = -BOX_SIZE;
    if(player->playerPos.v[2] >  BOX_SIZE) player->playerPos.v[2] =  BOX_SIZE;

    collision_detect(player);

    if(joypad.btn.b && !player->attacking) {
        player->attacking = true;
        player->attackFrame = 0;
    }
 
    if(player->attacking)
    {
        player->attackFrame += 1;
        if(player->attackFrame >= ATK_LENGTH * 2)
        {
            player->attackFrame = 0;
            player->attacking = false;
            player->weapon->attackFrame = 0;
        }
    }

    if(joypad.btn.a && !player->asc && player->jumpFrame == 0) player->asc = true;

    if (player->asc) // && !joypad.btn.a)
    {
        debugf("Jump frame: %d\n", player->jumpFrame);
        if(player->jumpFrame < 5)
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
        if(joypad.btn.c_right)
    {
        camPos->v[1] += 2.0f;
    }

    if(joypad.btn.c_left)
    {
        camPos->v[1] -= 2.0f;
    }
    
    if(joypad.btn.c_up){
        //debugf("player-> hasWeapon %i\n", player->hasWeapon);
        //debugf("pipe 1 coordinates: %f, %f, %f\n", pipes[0].wepPos.v[0], pipes[0].wepPos.v[1], pipes[0].wepPos.v[2]);
        //debugf("pipe 2 coordinates: %f, %f, %f\n", pipes[1].wepPos.v[0], pipes[1].wepPos.v[1], pipes[1].wepPos.v[2]);
        if(player->hasWeapon){
            player->weapon->equipped = false;
            player->weapon->attachedPlayer = NULL;
            player->hasWeapon = false;
            //player->weapon = NULL;
        }
    }

    t3d_mat4fp_from_srt_euler(&player->modelMatFP[frameIdx],
        (float[3]){0.125f, 0.125f, 0.125f},
        (float[3]){0.0f, -player->rotY, 0},
        player->playerPos.v);
}
