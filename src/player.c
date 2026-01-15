#include "entities.h"
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <stdlib.h>
#include <string.h>
#include <t3d/t3dskeleton.h>

#define FB_COUNT 3

Player players[4];

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
    player->weapon = NULL;
    player->hasWeapon = false;
    player->skel = malloc_uncached(sizeof(*player->skel));
    *player->skel = t3d_skeleton_create_buffered(model, FB_COUNT);
    player->skelBlend = malloc_uncached(sizeof(*player->skel));
    *player->skelBlend = t3d_skeleton_clone(player->skel, false); // optimized for blending, has no matrices    
    player->handBoneIdx = t3d_skeleton_find_bone(player->skel, "Bone.011");

    //model = t3d_model_load("rom:/banana.t3dm");
    rspq_block_begin();     
        rdpq_set_prim_color(RGBA32(255, 255, 255, 255));
        t3d_model_draw_skinned(model, player->skel);
    player->dplPlayer = rspq_block_end();
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

        if (player != pipes[i].attachedPlayer)
        {
            float dist = t3d_vec3_distance(&player->playerPos, &pipes[i].wepPos);
            if (dist < 15.0f && !player->hasWeapon)
            {
                if (!pipes[i].equipped)
                {
                    player->hasWeapon = true;
                    player->weapon = &pipes[i];
                    pipes[i].equipped = true;
                    pipes[i].attachedPlayer = player;
                }
            }
        }
    }

    if (player->isHittable > 0)
    {
        player->isHittable -= 1;
    }

    if (!player->attacking || !player->hasWeapon || !player->weapon)
        return;

    for (int i = 0; i < 4; i++)
    {
        if (&players[i] == player)
            continue;
        if (!players[i].alive)
            continue;

        for (int j = 0; j < 2; j++)
        {
            if (&pipes[j] != player->weapon)
                continue;
            if (!pipes[j].hit)
                continue;

            float diff = t3d_vec3_distance(pipes[j].hit, &players[i].playerPos);
            if (diff < 50.0f && players[i].isHittable == 0)
            {
                players[i].isHittable = 15;
                players[i].hitpoints -= player->weapon->damage;
                if (players[i].hitpoints <= 0.0f)
                    players[i].alive = false;
            }
        }
    }
}

void player_update(Player *player, joypad_port_t port, T3DVec3 *camPos, int frameIdx)
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
    collision_detect(player);
    if (joybtns.b && !player->attacking)
    {
        player->attacking = true;
        player->attackFrame = 0;
    }

    if (player->attacking)
    {
        player->attackFrame += 1;
        if (player->attackFrame >= ATK_LENGTH * 2)
        {
            player->attackFrame = 0;
            player->attacking = false;
            if (player->weapon)
                player->weapon->attackFrame = 0;
        }
    }

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
            player->weapon->equipped = false;
            player->weapon->attachedPlayer = NULL;
            player->hasWeapon = false;
            player->weapon = NULL;
        }
    }
    t3d_mat4fp_from_srt_euler(&player->modelMatFP[frameIdx],
                              (float[3]){0.125f, 0.125f, 0.125f},
                              (float[3]){0.0f, -player->rotY, 0},
                              player->playerPos.v);
}
