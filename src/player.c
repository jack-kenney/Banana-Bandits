#include "entities.h"
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <stdlib.h>
#include <string.h>

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
                    player->weapon->wepPos = pipes[i].wepPos;
                    player->weapon->damage = pipes[i].damage;
                    player->hasWeapon = true;
                    player->weapon = &pipes[i];
                    pipes[i].equipped = true;
                    pipes[i].attachedPlayer = player;
                }
            }
        }
    }

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
                    if(player->attacking && players[i].isHittable == 0 && players[i].alive)
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
    player->modelMatFP = malloc_uncached(sizeof(T3DMat4FP));
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
    t3d_model_load("rom:/banana.t3dm");
    rspq_block_begin();
        t3d_matrix_push(player->modelMatFP);
        t3d_model_draw(model); // requires modelCrystal from main/global scope
        t3d_matrix_pop(1);
    player->dplPlayer = rspq_block_end();
}

void player_update(Player *player, joypad_port_t port, T3DVec3 *camPos)
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

    if(joypad.btn.a) player->asc = true;

    if (player->asc && !joypad.btn.a)
    {
        if(player->jumpFrame < 5)
        {
            player->playerPos.v[1] += JUMP_HEIGHT;
            player->jumpFrame++;
        }
        else
        {
            player->asc = false;
        }
    }
    else if (player->jumpFrame > 0)
    {
        player->playerPos.v[1] -= JUMP_HEIGHT;
        player->jumpFrame--;
    }
        if(joypad.btn.c_right)
    {
        camPos->v[1] += 2.0f;
    }

    if(joypad.btn.c_left)
    {
        camPos->v[1] -= 2.0f;
    }

    t3d_mat4fp_from_srt_euler(player->modelMatFP,
        (float[3]){0.125f, 0.125f, 0.125f},
        (float[3]){0.0f, -player->rotY, 0},
        player->playerPos.v);
}
