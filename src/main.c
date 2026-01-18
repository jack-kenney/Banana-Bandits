#include <stdio.h>
#include <math.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>
#include <t3d/t3danim.h>
#include <t3d/t3ddebug.h>
#include <libdragon.h>
#include <rspq_profile.h>
#include "entities.h"
#include "util.h" 
#include "collision.h"
#define FB_COUNT 3

// Reserve a safe mixer channel for SFX. Stereo waveforms need ch+1, so avoid
// using the very last channel.
#define SFX_CH 30

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
int gameMode, frameIdx;
int *winner;
float HP0, HP1, HP2, HP3, lastTime;
sprite_t *spriteBanana;
bool gameCleanedUp = false;
xm64player_t musicPlayer;
wav64_t dominating;
T3DVec3 spawnPositions[] = {
    (T3DVec3){{-100, 0.15f, 0}},
    (T3DVec3){{0, 0.15f, -100}},
    (T3DVec3){{100, 0.15f, 0}},
    (T3DVec3){{0, 0.15f, 100}},
};

T3DAnim animPunch[4], animIdle[4];
enum GameMode {
    GAME_MODE_PLAY,
    GAME_MODE_MENU,
    GAME_MODE_PAUSE,
    GAME_MODE_RESET,
    GAME_MODE_END
};



#define STRINGIFY(x) #x
#define STYLE(id) "^0" STRINGIFY(id)
#define STYLE_TITLE 1
#define STYLE_GREY 2
#define STYLE_GREEN 3

Weapon pipes[2];

rspq_syncpoint_t syncPoint;

// Debug rendering (CPU) of simple wireframe bounds.

static bool debugDrawMapCandidates = false;

// Mixer exposes a cumulative counter of CPU ticks spent waiting for the RSP
// during mixing. Use it as a lightweight RSP contention indicator.
extern int64_t __mixer_profile_rsp;

// rspq_profile uses deferred callbacks to accumulate data. Those callbacks are
// normally processed during rspq_wait/syncpoint waits; for an in-game HUD we
// poll them opportunistically while the debug overlay is enabled.
extern bool __rspq_deferred_poll(void);

static perf_stats_t perf_last = {0};
static uint64_t perf_prev_mixer_ticks = 0;
static rspq_profile_data_t perf_prev_rspq = {0};

// Function to cleanup game resources
void game_cleanup()
{
    for (int i = 0; i < 4; i++)
    {
        if (players[i].modelMatFP)
            player_cleanup(&players[i]);
        players[i].modelMatFP = NULL;
        players[i].skel = NULL;
        players[i].skelBlend = NULL;
        players[i].weapon = NULL;
        players[i].hasWeapon = false;
        players[i].dplPlayer = NULL;
    }
    if (pipes[0].modelMatFP || pipes[0].hit)
        weapon_cleanup(&pipes[0]);
    if (pipes[1].modelMatFP || pipes[1].hit)
        weapon_cleanup(&pipes[1]);


    //rspq_block_free(dplMap);
    if (dplHitbubble)
        rspq_block_free(dplHitbubble);
    dplHitbubble = NULL;

    if (modelWeapon)
        t3d_model_free(modelWeapon);
    if (modelBanana)
        t3d_model_free(modelBanana);
    if (modelHitbubble)
        t3d_model_free(modelHitbubble);

    modelWeapon = NULL;
    modelBanana = NULL;
    modelHitbubble = NULL;

    if (hitbubbleFP)
        free_uncached(hitbubbleFP);
    if (winner)
        free(winner);
    hitbubbleFP = NULL;
    winner = NULL;

    if (spriteBanana)
        sprite_free(spriteBanana);
    spriteBanana = NULL;

    for (int i = 0; i < 4; i++)
    {
        t3d_anim_destroy(&animIdle[i]);
        t3d_anim_destroy(&animPunch[i]);
    }

    gameCleanedUp = true;
}

// Function to set up the players & game, called when a new game is started
void game_start()
{
    // If we somehow start twice without cleaning up (eg: starting from menu
    // after already initializing at boot), clean up first.
    if (modelBanana || modelWeapon || modelHitbubble || winner || hitbubbleFP || dplHitbubble)
        game_cleanup();

    winner = malloc(sizeof(int));
    *winner = -1;
    modelWeapon = t3d_model_load("rom:/pipe2.t3dm");
    modelBanana = t3d_model_load("rom:/banana_arm1_b4.t3dm");
    modelHitbubble = t3d_model_load("rom:/hitbubble.t3dm");
    hitbubbleFP = malloc_uncached(sizeof(T3DMat4FP));
    debugf("Banana model AABB: Min(%d, %d, %d) Max(%d, %d, %d)\n",
           modelBanana->aabbMin[0], modelBanana->aabbMin[1], modelBanana->aabbMin[2],
           modelBanana->aabbMax[0], modelBanana->aabbMax[1], modelBanana->aabbMax[2]);
    rspq_block_begin();
    t3d_matrix_push(hitbubbleFP);
    t3d_model_draw(modelHitbubble);
    t3d_matrix_pop(1);
    dplHitbubble = rspq_block_end();

    // Intialize weapons
    weapon_init(&pipes[0], (T3DVec3){{0.0f, 0.0f, 0.0f}}, modelWeapon);
    weapon_init(&pipes[1], (T3DVec3){{50.0f, 0.0f, 50.0f}}, modelWeapon);

    // Timing variables
    lastTime = get_time_s() - (1.0f / 60.0f);
    frameIdx = 0;
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
    HP0 = players[0].hitpoints;
    HP1 = players[1].hitpoints;
    HP2 = players[2].hitpoints;
    HP3 = players[3].hitpoints;

        // Load p1 HP bar sprite
    spriteBanana = sprite_load("rom:/hpbar.sprite");

    gameCleanedUp = false;


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
    // More buffers = less chance of underruns during heavy frames (at the
    // cost of a bit more latency).
    audio_init(44100, 4);
    mixer_init(32);
    mixer_set_vol(1.0f);
    debugf("Audio: freq=%dHz buf=%d samples can_write=%d\n",
        audio_get_frequency(), audio_get_buffer_length(), (int)audio_can_write());
    dfs_init(DFS_DEFAULT_LOCATION);

    // Start RSPQ profiler if available in the linked libdragon build.
    // If libdragon was built without RSPQ_PROFILE, calls will be no-ops.
    rspq_profile_start();
    //console_init();
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS);
    depthBuffer = display_get_zbuf();
    t3d_init((T3DInitParams){});
    viewport = t3d_viewport_create_buffered(FB_COUNT);
    camPos = (T3DVec3){{0, 150.0f, 200.0f}};
    camTarget = (T3DVec3){{0, 0, 40}};
    lightDirVec = (T3DVec3){{1.0f, 1.0f, 1.0f}};
    t3d_vec3_norm(&lightDirVec);
    gameMode = GAME_MODE_MENU;
    wav64_open(&dominating, "rom:/dominating.wav64");
    mixer_ch_set_vol(SFX_CH, 0.75f, 0.75f);
    //wav64_play(&dominating, SFX_CH);

    // Prime a few buffers so playback starts immediately.
    audio_pump(2);
    modelMap = t3d_model_load("rom:/map1.t3dm");
    rdpq_font_t* fnt = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_font_style(fnt, STYLE_TITLE, &(rdpq_fontstyle_t){RGBA32(0xAA, 0xAA, 0xFF, 0xFF)});
    rdpq_font_style(fnt, STYLE_GREY,  &(rdpq_fontstyle_t){RGBA32(0x66, 0x66, 0x66, 0xFF)});
    rdpq_font_style(fnt, STYLE_GREEN, &(rdpq_fontstyle_t){RGBA32(0x39, 0xBF, 0x1F, 0xFF)});
    rdpq_text_register_font(FONT_BUILTIN_DEBUG_MONO, fnt);
 
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
    // Stay in menu until the user starts a game.

    // Lighting colors and screen size, these should probably be moved
    uint8_t colorAmbient[4] = {0xAA, 0xAA, 0xAA, 0xFF};
    uint8_t colorDir[4] = {0xFF, 0xAA, 0xAA, 0xFF};
    int sizeX = display_get_width();
    int sizeY = display_get_height();

    int menuSelection = 0;

    // Main gameplay loop.
    // TODO: figure out how to limit framerate.
    while (1)
    {
        uint32_t cpu_frame_start = TICKS_READ();

        // Update perf stats from the previous frame.
        {
            if (debugDrawMapCandidates) {
                // Process a few deferred callbacks without blocking.
                for (int i = 0; i < 4; i++) {
                    if (!__rspq_deferred_poll()) break;
                }
            }

            uint64_t mix_ticks = (uint64_t)__mixer_profile_rsp;
            perf_last.cpu_mixer_wait_us = ticks_to_us(mix_ticks - perf_prev_mixer_ticks);
            perf_prev_mixer_ticks = mix_ticks;

            rspq_profile_data_t cur = {0};
            rspq_profile_get_data(&cur);
            // Only compute a delta if we have a new profiled frame.
            if (cur.frame_count != 0 && cur.frame_count != perf_prev_rspq.frame_count) {
                uint64_t dt_total = cur.total_ticks - perf_prev_rspq.total_ticks;
                uint64_t dt_busy  = cur.rdp_busy_ticks - perf_prev_rspq.rdp_busy_ticks;
                perf_last.rspq_ok = dt_total > 0;
                perf_last.rsp_frame_us = rcp_ticks_to_us(dt_total);
                perf_last.rdp_busy_us  = rcp_ticks_to_us(dt_busy);

                // Find top two overlays/slots for this frame.
                uint64_t top1 = 0, top2 = 0;
                const char *top1n = NULL, *top2n = NULL;
                for (int i = 0; i < RSPQ_PROFILE_SLOT_COUNT; i++) {
                    const char *name = cur.slots[i].name;
                    if (!name) continue;
                    uint64_t dt = cur.slots[i].total_ticks - perf_prev_rspq.slots[i].total_ticks;
                    if (dt > top1) {
                        top2 = top1; top2n = top1n;
                        top1 = dt;   top1n = name;
                    } else if (dt > top2) {
                        top2 = dt;   top2n = name;
                    }
                }

                memset(perf_last.top1_name, 0, sizeof(perf_last.top1_name));
                memset(perf_last.top2_name, 0, sizeof(perf_last.top2_name));
                if (top1n) strncpy(perf_last.top1_name, top1n, sizeof(perf_last.top1_name) - 1);
                if (top2n) strncpy(perf_last.top2_name, top2n, sizeof(perf_last.top2_name) - 1);
                perf_last.top1_us = rcp_ticks_to_us(top1);
                perf_last.top2_us = rcp_ticks_to_us(top2);

                perf_prev_rspq = cur;
            } else {
                perf_last.rspq_ok = false;
                perf_last.rsp_frame_us = 0;
                perf_last.rdp_busy_us = 0;
                perf_last.top1_name[0] = '\0';
                perf_last.top2_name[0] = '\0';
                perf_last.top1_us = 0;
                perf_last.top2_us = 0;
            }
        }

        // Pump audio early in the frame (before RSP-heavy work).
        // Keep it small to avoid fighting with the renderer.
        audio_pump(1);

        // Keep track of frame index for animation matrices(buffered)
        frameIdx = (frameIdx + 1) % FB_COUNT;
        float newTime = get_time_s();
        float deltaTime = newTime - lastTime;
        lastTime = newTime;
        //debugf("Frame Time: %.4f s (%.2f FPS)\n", deltaTime, 1.0f / deltaTime);

        // Update global Y rotation for weapons
        globalYrot += ((2 * T3D_PI) / 60.0f); // rotate 360 degrees every 60 frames
        globalYrot = fmodf(globalYrot, (2 * T3D_PI));

        // Poll the joypads
        joypad_poll();

        joypad_buttons_t joypad1_btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);

        joypad_inputs_t joypad1 = joypad_get_inputs(JOYPAD_PORT_1);
        if (joypad1_btn.start && gameMode == GAME_MODE_PLAY)
        {
            gameMode = GAME_MODE_PAUSE;
        }
        else if (joypad1_btn.start && gameMode == GAME_MODE_PAUSE)
        {
            menuSelection = 0;
            gameMode = GAME_MODE_PLAY; // toggle between 0 and 1
        }

        if (joypad1_btn.c_down) {
            wav64_play(&dominating, SFX_CH);
            debugDrawMapCandidates = !debugDrawMapCandidates;
        }


        // Set viewport
        t3d_viewport_set_projection(&viewport, T3D_DEG_TO_RAD(90.0f), 20.0f, 320.0f);
        t3d_viewport_look_at(&viewport, &camPos, &camTarget, &(T3DVec3){{0, 1, 0}});

        // ======== Draw (3D) ======== //
        surface_t *surface = display_get();
        rdpq_attach(surface, depthBuffer);
        t3d_frame_start();
        t3d_viewport_attach(&viewport);

        t3d_screen_clear_color(RGBA32(224, 180, 96, 0xFF));
        t3d_screen_clear_depth();

        t3d_light_set_ambient(colorAmbient);
        t3d_light_set_directional(0, colorDir, &lightDirVec);
        t3d_light_set_count(1);
        
        // Draw map first (background)
        rspq_block_run(dplMap);

        switch (gameMode){
            case GAME_MODE_PLAY:
            {
                // Update players //
                did_i_win(winner);

                if(*winner != -1) {
                    gameMode = GAME_MODE_END;
                    break;
                }
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
                    if (players[i].state.s == STATE_ATTACK)
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
                            players[i].state.s = STATE_IDLE;
                            players[i].state.frame = 0;
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
                        // Draw shared 2D overlay (if desired across all modes)
                rdpq_sync_pipe();
                rdpq_set_mode_standard();
                rdpq_mode_alphacompare(128);
                rdpq_sprite_blit(spriteBanana, 10, 20, NULL);

            }
            break;
            case GAME_MODE_PAUSE:
            {
                rspq_block_run(dplMap);
                float posX = 127;
                float posY = 40;
                if(menuSelection > 2) menuSelection = 2;
                float cursorX = posX - 10;
                float cursorY = 60 + (10 * menuSelection);
                rdpq_sync_pipe();
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_TITLE) "GAME PAUSED");
                posY += 20;
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_GREY) "Resume Game");
                posY += 10;
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_GREY) "Reset Game");
                posY += 10;
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_GREY) "Exit to Menu");
                posY += 10;
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, cursorX, cursorY, STYLE(STYLE_GREY) ">");

                if(joypad1.stick_y > 24) {
                    menuSelection--;
                    if(menuSelection < 0) menuSelection = 0;
                }
                if(joypad1.stick_y < -24) {
                    menuSelection ++;
                    if(menuSelection > 2) menuSelection = 2;
                }
                if(joypad1_btn.a) {
                    switch(menuSelection) {
                        case 0:
                            menuSelection = 0;  
                            gameMode = GAME_MODE_PLAY;
                            break;
                        case 1:
                            gameMode = GAME_MODE_RESET;
                            break;
                        case 2:
                            menuSelection = 0;
                            *winner = -1;
                             game_cleanup();
                            gameMode = GAME_MODE_MENU;
                            break;
                    }
                }
                rdpq_sync_pipe();
            }
            break;
            case GAME_MODE_MENU:
            {
                game_reset(spawnPositions);
                //draw menu here
                    // ======== Draw (UI) ======== //
                rspq_block_run(dplMap);
                float posX = 127;
                float posY = 40;
                if(menuSelection > 1) menuSelection = 1;
                float cursorX = posX - 10;
                float cursorY = 60 + (10 * menuSelection);
                rdpq_sync_pipe();
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_TITLE) "Banana Bandits");
                posY += 20;
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_GREY) "Start Game");
                posY += 10;
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_GREY) "Options");
                posY += 10;
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, cursorX, cursorY, STYLE(STYLE_GREY) ">");

                if(joypad1.stick_y > 24) {
                    menuSelection--;
                    if(menuSelection < 0) menuSelection = 0;
                }
                if(joypad1.stick_y < -24) {
                    menuSelection++;
                    if(menuSelection > 1) menuSelection = 1;
                }
                if(joypad1_btn.a) {
                    switch(menuSelection) {
                        case 0:
                            gameMode = GAME_MODE_PLAY;
                            game_start();
                            break;
                        case 1:
                            //options
                            break;
                    }
                }
                rdpq_sync_pipe();
            }
            break;
            case (GAME_MODE_RESET): {
                game_cleanup();
                game_start();
                gameMode = GAME_MODE_PLAY;
                menuSelection = 0;
            }
            break;
            case (GAME_MODE_END): {
                rspq_block_run(dplMap);
                debugDrawMapCandidates = false;
                float posX = 127;
                float posY = 100;
                rdpq_sync_pipe();
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_TITLE) "Game Over!");
                posY += 20;
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_GREEN) "Player %d Wins!", *winner + 1);
                posY += 20;
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_GREY) "Press A to return to Menu");

                if(joypad1_btn.a) {
                    game_cleanup();
                    gameMode = GAME_MODE_MENU;
                    menuSelection = 0;
                }
                rdpq_sync_pipe();
            }

        }

        // Debug overlay: CPU/RSP performance summary.
        if (debugDrawMapCandidates) {
            // Ensure the RDP is in a safe state before switching to 2D/text.
            // This matters especially on frames where we break out of gameplay
            // early (eg: winner detected) and skip the usual 2D sync.
            rdpq_sync_pipe();
            const float cpu_ms = (float)perf_last.cpu_frame_us / 1000.0f;
            const float mix_ms = (float)perf_last.cpu_mixer_wait_us / 1000.0f;

            rdpq_set_mode_standard();
            float y = 10;
            rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 10, y, STYLE(STYLE_GREY)
                "CPU frame: %5.2f ms  (mixer wait: %5.2f ms)", cpu_ms, mix_ms);
            y += 10;

            if (perf_last.rspq_ok) {
                float rsp_ms = (float)perf_last.rsp_frame_us / 1000.0f;
                float rdp_ms = (float)perf_last.rdp_busy_us / 1000.0f;
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 10, y, STYLE(STYLE_GREY)
                    "RSP frame: %5.2f ms  (RDP busy: %5.2f ms)", rsp_ms, rdp_ms);
                y += 10;

                if (perf_last.top1_name[0]) {
                    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 10, y, STYLE(STYLE_GREY)
                        "Top: %-24s %5.2f ms", perf_last.top1_name, (float)perf_last.top1_us / 1000.0f);
                    y += 10;
                }
                if (perf_last.top2_name[0]) {
                    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 10, y, STYLE(STYLE_GREY)
                        "     %-24s %5.2f ms", perf_last.top2_name, (float)perf_last.top2_us / 1000.0f);
                    y += 10;
                }
            } else {
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 10, y, STYLE(STYLE_GREY)
                    "RSPQ profiler: disabled/unavailable");
            }
        }


        syncPoint = rspq_syncpoint_new();
        rdpq_sync_tile();
        rdpq_sync_pipe(); // Hardware crashes otherwise

        // Optional CPU debug overlay: draw bounds/colliders (wireframe).
        // We must wait for the RDP to finish before touching the framebuffer.
        if (debugDrawMapCandidates && gameMode == GAME_MODE_PLAY && modelMap) {
            rdpq_detach_wait();

            // Broadphase: capsule-derived AABB vs map object AABBs (draw candidate objects).
            if (debugDrawMapCandidates && modelMap) {
                T3DMat4 mapIdentity = {0};
                mapIdentity.m[0][0] = 1.0f;
                mapIdentity.m[1][1] = 1.0f;
                mapIdentity.m[2][2] = 1.0f;
                mapIdentity.m[3][3] = 1.0f;

                uint32_t queryColor = graphics_make_color(0x00, 0xFF, 0xFF, 0xFF);
                uint32_t candColor  = graphics_make_color(0x80, 0xFF, 0xFF, 0xFF);

                static float lastPrint = 0.0f;
                int totalCandidates = 0;

                for (int p = 0; p < 4; p++) {
                    if (!players[p].alive) continue;

                    // Use the gameplay AABB tied to the player (world-space).
                    AabbF query = players[p].aabb;

                    // Draw the query AABB.
                    debug_draw_aabbf(surface, &viewport, &query, queryColor);

                    // Iterate all map objects and draw those whose AABB overlaps query.
                    int playerCandidates = 0;
                    T3DModelIter it = t3d_model_iter_create(modelMap, T3D_CHUNK_TYPE_OBJECT);
                    while (t3d_model_iter_next(&it)) {
                        const T3DObject *obj = it.object;
                        AabbF objAabb = {
                            .min = (T3DVec3){{s16_to_f32(obj->aabbMin[0]), s16_to_f32(obj->aabbMin[1]), s16_to_f32(obj->aabbMin[2])}},
                            .max = (T3DVec3){{s16_to_f32(obj->aabbMax[0]), s16_to_f32(obj->aabbMax[1]), s16_to_f32(obj->aabbMax[2])}},
                        };

                        if (!aabbf_overlaps(&query, &objAabb)) continue;

                        // Cap draws to avoid turning the frame into a line soup.
                        if (playerCandidates < 64) {
                            debug_draw_object_aabb_mat4(surface, &viewport, obj, &mapIdentity, candColor);
                        }
                        playerCandidates++;
                    }

                    totalCandidates += playerCandidates;
                }

                float now = get_time_s();
                if (now - lastPrint > 0.5f) {
                    debugf("Map broadphase candidates (sum over players): %d\n", totalCandidates);
                    lastPrint = now;
                }

                // Also draw weapon AABBs while in C-down mode.
                uint32_t weaponColors[2];
                weaponColors[0] = graphics_make_color(0xFF, 0x00, 0xFF, 0xFF);
                weaponColors[1] = graphics_make_color(0xFF, 0x80, 0xFF, 0xFF);
                for (int w = 0; w < 2; w++) {
                    debug_draw_aabbf(surface, &viewport, &pipes[w].aabb, weaponColors[w]);
                }
            }

            uint32_t colors[4];
            colors[0] = graphics_make_color(0xFF, 0x40, 0x40, 0xFF);
            colors[1] = graphics_make_color(0x40, 0xFF, 0x40, 0xFF);
            colors[2] = graphics_make_color(0x40, 0xA0, 0xFF, 0xFF);
            colors[3] = graphics_make_color(0xFF, 0xFF, 0x40, 0xFF);

            for (int i = 0; i < 4; i++) {
                if (!players[i].alive) continue;
                debug_draw_aabbf(surface, &viewport, &players[i].aabb, colors[i]);
            }

            display_show(surface);
        } else {
            rdpq_detach_show();
        }

        // Pump audio after graphics submission / swap, when the RSP is more
        // likely to be idle.
        audio_pump(2);

        // Tell the RSPQ profiler that a frame has completed. Do this only
        // when the debug overlay is active to avoid overhead.
        if (debugDrawMapCandidates) {
            rspq_profile_next_frame();
        }

        perf_last.cpu_frame_us = ticks_to_us((uint32_t)(TICKS_READ() - cpu_frame_start));
    }
}