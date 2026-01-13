#include <stdio.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>
#include <t3d/t3danim.h>
#include <t3d/t3ddebug.h>
#include <libdragon.h>
#include "entities.h"

#define FB_COUNT 3

T3DVec3 spawnPositions[] = {
    (T3DVec3){{-100, 0.15f, 0}},
    (T3DVec3){{0, 0.15f, -100}},
    (T3DVec3){{100, 0.15f, 0}},
    (T3DVec3){{0, 0.15f, 100}},
};

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
T3DModel *modelBanana;
T3DModel *modelHitbubble;
T3DVec3 camPos;
T3DVec3 camTarget;
T3DVec3 lightDirVec;
float globalYrot;
int gameMode;

Weapon pipes[2];

rspq_syncpoint_t syncPoint;

float get_time_s()
{
    return (float)((double)get_ticks_us() / 1000000.0);
}

// Function to initialize some console and t3d stuff, load models, premake RSP blocks.
void game_init()
{
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
    dfs_init(DFS_DEFAULT_LOCATION);
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS);
    depthBuffer = display_get_zbuf();
    t3d_init((T3DInitParams){});
    viewport = t3d_viewport_create_buffered(FB_COUNT);
    camPos = (T3DVec3){{0, 150.0f, 200.0f}};
    camTarget = (T3DVec3){{0, 0, 40}};
    lightDirVec = (T3DVec3){{1.0f, 1.0f, 1.0f}};
    t3d_vec3_norm(&lightDirVec);
    gameMode = 0;

    modelMap = t3d_model_load("rom:/map1.t3dm");
    modelWeapon = t3d_model_load("rom:/pipe.t3dm");
    modelBanana = t3d_model_load("rom:/banana_arm1_b4.t3dm");
    modelHitbubble = t3d_model_load("rom:/hitbubble.t3dm");
    hitbubbleFP = malloc_uncached(sizeof(T3DMat4FP));

    rspq_block_begin();
    t3d_matrix_push(hitbubbleFP);
    t3d_model_draw(modelHitbubble);
    t3d_matrix_pop(1);
    dplHitbubble = rspq_block_end();

    rspq_block_begin();
    // t3d_model_draw(modelShadow);
    // t3d_model_draw(modelCrystal);
    t3d_model_draw(modelMap);
    dplMap = rspq_block_end();
}

int main(void)
{
    // console_init();
    game_init();

    // Intialize weapons
    weapon_init(&pipes[0], (T3DVec3){{0.0f, 0.0f, 0.0f}}, modelWeapon);
    weapon_init(&pipes[1], (T3DVec3){{50.0f, 0.0f, 50.0f}}, modelWeapon);

    // Animation variables for each character
    T3DAnim animPunch[4], animIdle[4];

    // Timing variables
    float lastTime = get_time_s() - (1.0f / 60.0f);
    int frameIdx = 0;

    // Per-player initialization tasks happen in this loop
    for (int i = 0; i < 4; i++)
    {
        player_init(&players[i], spawnPositions[i], modelBanana);
        // Base animation (always running)
        animIdle[i] = t3d_anim_create(modelBanana, "bananaJump");
        t3d_anim_set_looping(&animIdle[i], true);
        t3d_anim_set_playing(&animIdle[i], true);
        t3d_anim_set_speed(&animIdle[i], 1.0f);
        t3d_anim_attach(&animIdle[i], players[i].skel);

        // Attack animation (overrides base while playing)
        animPunch[i] = t3d_anim_create(modelBanana, "bananaWorm");
        t3d_anim_set_looping(&animPunch[i], false);
        t3d_anim_set_playing(&animPunch[i], false);
        t3d_anim_set_speed(&animPunch[i], 2.0f);
        t3d_anim_attach(&animPunch[i], players[i].skel);
    }

    // These are used for drawing players' HP bars
    float HP0 = players[0].hitpoints;
    float HP1 = players[1].hitpoints;
    float HP2 = players[2].hitpoints;
    float HP3 = players[3].hitpoints;

    // Lighting colors and screen size, these should probably be moved
    uint8_t colorAmbient[4] = {0xAA, 0xAA, 0xAA, 0xFF};
    uint8_t colorDir[4] = {0xFF, 0xAA, 0xAA, 0xFF};
    int sizeX = display_get_width();
    int sizeY = display_get_height();

    // Oh right, this isn't used LMAO
    sprite_t *spriteBanana = sprite_load("rom:/hpbar.sprite");

    // Main gameplay loop.
    // TODO: figure out how to limit framerate.
    while (1)
    {
        // Keep track of frame index for animation matrices(buffered)
        frameIdx = (frameIdx + 1) % FB_COUNT;
        float newTime = get_time_s();
        float deltaTime = newTime - lastTime;
        lastTime = newTime;

        // Update global Y rotation for weapons
        globalYrot += ((2 * T3D_PI) / 60.0f); // rotate 360 degrees every 60 frames
        globalYrot = fmodf(globalYrot, (2 * T3D_PI));


        // Set viewport
        t3d_viewport_set_projection(&viewport, T3D_DEG_TO_RAD(90.0f), 20.0f, 320.0f);
        t3d_viewport_look_at(&viewport, &camPos, &camTarget, &(T3DVec3){{0, 1, 0}});

        // ======== Draw (3D) ======== //
        rdpq_attach(display_get(), depthBuffer);
        t3d_frame_start();
        t3d_viewport_attach(&viewport);

        t3d_screen_clear_color(RGBA32(224, 180, 96, 0xFF));
        t3d_screen_clear_depth();

        t3d_light_set_ambient(colorAmbient);
        t3d_light_set_directional(0, colorDir, &lightDirVec);
        t3d_light_set_count(1);

        // Poll the joypads
        joypad_poll();

        joypad_inputs_t joypad1 = joypad_get_inputs(JOYPAD_PORT_1);
        if (joypad1.btn.start)  
        {
            gameMode = (gameMode + 1) % 2; // toggle between 0 and 1
        }

        // Draw map first (background)
        rspq_block_run(dplMap);

        switch (gameMode){
            case 0:
            {
                // Update players //
                for (int i = 0; i < 4; i++)
                {

                    // Only do this for alive players
                    if (!players[i].alive)
                        continue;

                    // Actually do the update
                    player_update(&players[i], JOYPAD_PORT_1 + i, &camPos, frameIdx);

                    // Update base pose
                    t3d_anim_update(&animIdle[i], deltaTime);

                    // Attack overrides base while active
                    if (players[i].attacking)
                    {
                        if (!animPunch[i].isPlaying)
                        {
                            t3d_anim_set_playing(&animPunch[i], true);
                            t3d_anim_set_time(&animPunch[i], 0.0f);
                        }
                        t3d_anim_update(&animPunch[i], deltaTime);

                        // If the non-looping animation finished, drop back to idle
                        if (!animPunch[i].isPlaying)
                        {
                            players[i].attacking = false;
                            players[i].attackFrame = 0;
                        }
                    }
                    else
                    {
                        // Ensure next attack starts from the beginning
                        if (animPunch[i].isPlaying)
                        {
                            t3d_anim_set_playing(&animPunch[i], false);
                        }
                        t3d_anim_set_time(&animPunch[i], 0.0f);
                    }

                    // NOTE: Buffered skeleton matrices can switch to a new matrix buffer when any bone changes.
                    // Forcing the root bone as dirty ensures the full hierarchy gets valid matrices every frame.
                    players[i].skel->bones[0].hasChanged = true;
                    t3d_skeleton_update(players[i].skel);
                }

                // Update weapons
                pipe_movement(&pipes[0], globalYrot, frameIdx);
                pipe_movement(&pipes[1], globalYrot, frameIdx);
                for (int i = 0; i < 2; i++)
                {
                    t3d_mat4fp_from_srt_euler(hitbubbleFP,
                                            (float[3]){0.1f, 0.1f, 0.1f},
                                            (float[3]){0.0f, 0.0f, 0.0f},
                                            pipes[i].hit->v);
                }
                rspq_block_run(dplHitbubble);

                // Draw players
                for (int i = 0; i < 4; i++)
                {
                    if (players[i].alive && (players[i].isHittable % 2 == 0))
                    {
                        // Buffered skeletons require selecting the active matrices before drawing.
                        // Also, the model matrix is buffered per-frame to avoid RSP/CPU races.
                        t3d_skeleton_use(players[i].skel);
                        t3d_matrix_push(&players[i].modelMatFP[frameIdx]);
                        rspq_block_run(players[i].dplPlayer);
                        t3d_matrix_pop(1);
                    }
                }

                // Draw weapons
                for (int i = 0; i < 2; i++)
                {
                    t3d_matrix_push(&pipes[i].modelMatFP[frameIdx]);
                    rspq_block_run(pipes[i].dplWeapon);
                    t3d_matrix_pop(1);
                }

                // ======== Draw (2D) ======== //
                rdpq_sync_pipe();
                rdpq_set_scissor(0, 0, sizeX, sizeY);
                rdpq_set_mode_standard();

                // Get all players HP and linearly interpolate for smooth bar movement
                HP0 = t3d_lerp(HP0, players[0].hitpoints, 0.5f);
                HP1 = t3d_lerp(HP1, players[1].hitpoints, 0.5f);
                HP2 = t3d_lerp(HP2, players[2].hitpoints, 0.5f);
                HP3 = t3d_lerp(HP3, players[3].hitpoints, 0.5f);

                // Draw green HP bars first
                rdpq_set_mode_fill(RGBA32(0, 0xCC, 0, 0xFF));
                rdpq_fill_rectangle(20, 25, HP0 + 20, 30);
                rdpq_fill_rectangle(200, 20, HP1 + 200, 25);
                rdpq_fill_rectangle(20, 200, HP2 + 20, 205);
                rdpq_fill_rectangle(200, 200, HP3 + 200, 205);

                // Then draw red HP bar for missing HP
                rdpq_set_mode_fill(RGBA32(0xCC, 0, 0, 0xFF));
                rdpq_fill_rectangle(HP0 + 20, 25, 120, 30);
                rdpq_fill_rectangle(HP1 + 200, 20, 300, 25);
                rdpq_fill_rectangle(HP2 + 20, 200, 120, 205);
                rdpq_fill_rectangle(HP3 + 200, 200, 300, 205);

            }
            break;
            case 1:
            {
                rspq_block_run(dplMap);
            }
            break;
        }

        // Draw shared 2D overlay (if desired across all modes)
        rdpq_sync_pipe();
        rdpq_set_mode_standard();
        rdpq_mode_alphacompare(128);
        rdpq_sprite_blit(spriteBanana, 10, 20, NULL);
        syncPoint = rspq_syncpoint_new();
        rdpq_sync_tile();
        rdpq_sync_pipe(); // Hardware crashes otherwise
        rdpq_detach_show();
    }
}