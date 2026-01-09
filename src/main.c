#include <stdio.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>
#include <t3d/t3danim.h>
#include <t3d/t3ddebug.h>
#include <libdragon.h>
#include "entities.h"

#define JUMP_HEIGHT 12.0f
#define ATK_LENGTH 5.0f

surface_t *depthBuffer;
T3DViewport viewport;
rdpq_font_t *font;
rdpq_font_t *fontBillboard;
T3DMat4FP *mapMatFP;
T3DMat4FP *hitbubbleFP;
rspq_block_t *dplMap;
rspq_block_t *dplHitbubble;
T3DModel *model;
T3DModel *modelShadow;
T3DModel *modelMap;
T3DModel *modelWeapon;
T3DModel *modelCrystal;
T3DModel *modelHitbubble;
T3DVec3 camPos;
T3DVec3 camTarget;
T3DVec3 lightDirVec;
float globalYrot;

Weapon pipe;

rspq_syncpoint_t syncPoint;

void game_init()
{
    dfs_init(DFS_DEFAULT_LOCATION);
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS);
    depthBuffer = display_get_zbuf();
    t3d_init((T3DInitParams){});

    viewport = t3d_viewport_create();
    camPos = (T3DVec3){{0, 150.0f, 200.0f}};
    camTarget = (T3DVec3){{0, 0, 40}};

    lightDirVec = (T3DVec3){{1.0f, 1.0f, 1.0f}};
    t3d_vec3_norm(&lightDirVec);

    modelMap = t3d_model_load("rom:/map1.t3dm");
    //modelShadow = t3d_model_load("rom:/shadow.t3dm");
    modelWeapon = t3d_model_load("rom:/pipe.t3dm");
    modelCrystal = t3d_model_load("rom:/banana.t3dm");
    modelHitbubble = t3d_model_load("rom:/hitbubble.t3dm");
    hitbubbleFP = malloc_uncached(sizeof(T3DMat4FP));

    rspq_block_begin();
    t3d_matrix_push(hitbubbleFP);
    t3d_model_draw(modelHitbubble);
    t3d_matrix_pop(1);
    dplHitbubble = rspq_block_end();

    rspq_block_begin();
    //t3d_model_draw(modelShadow);
    //t3d_model_draw(modelCrystal);
    t3d_model_draw(modelMap);
    dplMap = rspq_block_end();
}

int main(void)
{
    //console_init();

    //debug_init_usblog();
    //console_set_debug(true);


    asset_init_compression(2);
    asset_init_compression(3);

    debug_init_usblog();
    debug_init_isviewer();
    console_set_debug(true);
    joypad_init();
    timer_init();
    rdpq_init();
    audio_init(32000, 3);
    mixer_init(32);
    game_init();
    player_init(&players[0], (T3DVec3){{-100,0.15f,0}}, modelCrystal);
    player_init(&players[1], (T3DVec3){{0,0.15f,-100}}, modelCrystal);
    player_init(&players[2], (T3DVec3){{100,0.15f,0}}, modelCrystal);
    player_init(&players[3], (T3DVec3){{0,0.15f,100}}, modelCrystal);
    weapon_init(&pipe, (T3DVec3){{0.0f,0.0f,0.0f}}, modelWeapon);
    uint8_t colorAmbient[4] = {0xAA, 0xAA, 0xAA, 0xFF};
    uint8_t colorDir[4]     = {0xFF, 0xAA, 0xAA, 0xFF};
    while(1) {
        globalYrot += (2 * T3D_PI) / 60.0f;
        joypad_poll();
        player_update(&players[0], JOYPAD_PORT_1, &camPos);
        player_update(&players[1], JOYPAD_PORT_2, &camPos);
        player_update(&players[2], JOYPAD_PORT_3, &camPos);
        player_update(&players[3], JOYPAD_PORT_4, &camPos);
        pipe_movement(&pipe, globalYrot);
        t3d_mat4fp_from_srt_euler(hitbubbleFP,
            (float[3]){0.1f, 0.1f, 0.1f},
            (float[3]){0.0f, 0.0f, 0.0f},
            pipe.hit->v);
        t3d_viewport_set_projection(&viewport, T3D_DEG_TO_RAD(90.0f), 20.0f, 320.0f);
        t3d_viewport_look_at(&viewport, &camPos, &camTarget, &(T3DVec3){{0,1,0}});
        // ======== Draw (3D) ======== //
        rdpq_attach(display_get(), depthBuffer);
        t3d_frame_start();
        t3d_viewport_attach(&viewport);

        t3d_screen_clear_color(RGBA32(224, 180, 96, 0xFF));
        t3d_screen_clear_depth();

        t3d_light_set_ambient(colorAmbient);
        t3d_light_set_directional(0, colorDir, &lightDirVec);
        t3d_light_set_count(1);
        rspq_block_run(dplMap);
        rspq_block_run(dplHitbubble);
        for(int i = 0; i < 4; i++)
        {
            if(players[i].alive)
            {
                rspq_block_run(players[i].dplPlayer);
            }
        }

        rspq_block_run(pipe.dplWeapon);
 
        syncPoint = rspq_syncpoint_new();
        rdpq_sync_tile();
        rdpq_sync_pipe(); // Hardware crashes otherwise
        rdpq_detach_show();
    }  
}