#include <stdio.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>
#include <t3d/t3danim.h>
#include <t3d/t3ddebug.h>
#include <libdragon.h>

#define JUMP_HEIGHT 12.0f;

surface_t *depthBuffer;
T3DViewport viewport;
rdpq_font_t *font;
rdpq_font_t *fontBillboard;
T3DMat4FP *mapMatFP;
rspq_block_t *dplMap;
T3DModel *model;
T3DModel *modelShadow;
T3DModel *modelMap;
T3DModel *modelCrystal;
T3DVec3 camPos;
T3DVec3 camTarget;
T3DVec3 lightDirVec;


typedef struct {
    T3DVec3 moveDir;
    T3DVec3 playerPos;
    float currSpeed;
    float rotY;
    int jumpFrame;
    bool asc;
    rspq_block_t *dplPlayer;
    T3DMat4FP *modelMatFP;
} Player;

Player player1;

rspq_syncpoint_t syncPoint;

void player_init(Player *player)
{
    player->modelMatFP = malloc_uncached(sizeof(T3DMat4FP));
    player->moveDir = (T3DVec3){{0,0,0}};
    player->playerPos = (T3DVec3){{0,0,0}};
    player->currSpeed = 0.0f;
    player->rotY = 0.0f;
    player->jumpFrame = 0;
    rspq_block_begin();
        t3d_matrix_push(player->modelMatFP);
        t3d_model_draw(modelCrystal); // as in the last example, draw skinned with the main skeleton
        t3d_matrix_pop(1);
    player->dplPlayer = rspq_block_end();
}

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
    modelCrystal = t3d_model_load("rom:/cube.t3dm");
    rspq_block_begin();
    //t3d_model_draw(modelShadow);
    //t3d_model_draw(modelCrystal);
    t3d_model_draw(modelMap);
    dplMap = rspq_block_end();
}


void player_movement(Player *player, joypad_port_t port) 
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
    // Move player
    if (joypad.btn.z) player->currSpeed = 5.0f;
    player->playerPos.v[0] += player->moveDir.v[0] * player->currSpeed;
    player->playerPos.v[2] += player->moveDir.v[2] * player->currSpeed;
    // ...and limit position inside the box
    const float BOX_SIZE = 140.0f;
    if(player->playerPos.v[0] < -BOX_SIZE)player->playerPos.v[0] = -BOX_SIZE;
    if(player->playerPos.v[0] >  BOX_SIZE)player->playerPos.v[0] =  BOX_SIZE;
    if(player->playerPos.v[2] < -BOX_SIZE)player->playerPos.v[2] = -BOX_SIZE;
    if(player->playerPos.v[2] >  BOX_SIZE)player->playerPos.v[2] =  BOX_SIZE;

      // Update player matrix
    t3d_mat4fp_from_srt_euler(player->modelMatFP,
    (float[3]){0.125f, 0.125f, 0.125f},
    (float[3]){0.0f, -player->rotY, 0},
    player->playerPos.v);

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
    player_init(&player1);
    uint8_t colorAmbient[4] = {0xAA, 0xAA, 0xAA, 0xFF};
    uint8_t colorDir[4]     = {0xFF, 0xAA, 0xAA, 0xFF};
    while(1) {
        joypad_poll();
        player_movement(&player1, JOYPAD_PORT_1);

        t3d_viewport_set_projection(&viewport, T3D_DEG_TO_RAD(90.0f), 20.0f, 160.0f);
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

        rspq_block_run(player1.dplPlayer);  
        rspq_block_run(dplMap);
        syncPoint = rspq_syncpoint_new();
        rdpq_sync_tile();
        rdpq_sync_pipe(); // Hardware crashes otherwise
        rdpq_detach_show();
    }  
}