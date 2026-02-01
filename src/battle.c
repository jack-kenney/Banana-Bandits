#include "battle.h"

#include <math.h>
#include <string.h>

#include <libdragon.h>
#include <rspq_profile.h>
#include <t3d/t3d.h>
#include <t3d/t3danim.h>
#include <t3d/t3ddebug.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>
#include "battle.h"
#include "collision.h"
#include "entities.h"
#include "player.h"
#include "util.h"
#include "weapon.h"

perf_stats_t empty_perf_stats = {0};

static const T3DVec3 defaultSpawnPositions[] = {
    (T3DVec3){{-100, 0.15f, 0}},
    (T3DVec3){{0, 0.15f, -100}},
    (T3DVec3){{100, 0.15f, 0}},
    (T3DVec3){{0, 0.15f, 100}},
};

static bool map_draw_filter(void *userData, const T3DObject *obj)
{
    (void)userData;
    if (!obj || !obj->name)
        return true;

    return !starts_with_ci(obj->name, "spawn");
}

enum GameMode
{
    GAME_MODE_PLAY,
    GAME_MODE_MENU,
    GAME_MODE_PAUSE,
    GAME_MODE_RESET,
    GAME_MODE_END
};

// Mixer exposes a cumulative counter of CPU ticks spent waiting for the RSP
// during mixing. Use it as a lightweight RSP contention indicator.
extern int64_t __mixer_profile_rsp;

// rspq_profile uses deferred callbacks to accumulate data. Those callbacks are
// normally processed during rspq_wait/syncpoint waits; for an in-game HUD we
// poll them opportunistically while the debug overlay is enabled.
extern bool __rspq_deferred_poll(void);

extern wav64_t dominating, smack1, smack2, smack3, smack4;

// Cleanup per-match resources (entities, match-specific models/effects).
static void battle_cleanup_match(BattleState *state)
{
    for (int i = 0; i < state->numEntities; i++)
    {
        if (!state->entities[i])
            continue;

        if (state->entities[i]->cleanup)
            state->entities[i]->cleanup(state->entities[i]);

        free(state->entities[i]);
        state->entities[i] = NULL;
    }
    state->numEntities = 0;

    if (state->dplHitbubble)
        rspq_block_free(state->dplHitbubble);
    state->dplHitbubble = NULL;

    if (state->modelWeapon)
        t3d_model_free(state->modelWeapon);
    if (state->modelBanana)
        t3d_model_free(state->modelBanana);
    if (state->modelHitbubble)
        t3d_model_free(state->modelHitbubble);

    state->modelWeapon = NULL;
    state->modelBanana = NULL;
    state->modelHitbubble = NULL;

    if (state->hitbubbleFP)
        free_uncached(state->hitbubbleFP);
    state->hitbubbleFP = NULL;

    if (state->spriteBanana)
        sprite_free(state->spriteBanana);
    state->spriteBanana = NULL;
}

// Set up the players & game, called when a new match is started.
static void battle_start_match(BattleState *state)
{
    heap_stats_t stats;
    sys_get_heap_stats(&stats);
    debugf("Heap stats before game start: total=%u used=%u\n",
           stats.total, stats.used);
    // If we somehow start twice without cleaning up (eg: starting from menu
    // after already initializing at boot), clean up first.
    if (state->modelBanana || state->modelWeapon || state->modelHitbubble || state->winner || state->hitbubbleFP || state->dplHitbubble)
        battle_cleanup_match(state);

    state->winner = -1;
    state->globalYrot = 0.0f;
    for (int i = 0; i < MAX_ENTITIES; i++)
        state->entities[i] = NULL;
    state->modelWeapon = t3d_model_load("rom:/pipe2.t3dm");
    state->modelBanana = t3d_model_load("rom:/banana_arm1_b4_new_low_poly.t3dm");
    state->modelHitbubble = t3d_model_load("rom:/hitbubble.t3dm");
    state->hitbubbleFP = malloc_uncached(sizeof(T3DMat4FP));
    debugf("Banana model AABB: Min(%d, %d, %d) Max(%d, %d, %d)\n",
           state->modelBanana->aabbMin[0], state->modelBanana->aabbMin[1], state->modelBanana->aabbMin[2],
           state->modelBanana->aabbMax[0], state->modelBanana->aabbMax[1], state->modelBanana->aabbMax[2]);
    rspq_block_begin();
    t3d_matrix_push(state->hitbubbleFP);
    t3d_model_draw(state->modelHitbubble);
    t3d_matrix_pop(1);
    state->dplHitbubble = rspq_block_end();

    for (int i = 0; i < state->numPlayers; i++)
    {
        debugf("Initializing player %d\n", i + 1);
        Player *p = malloc(sizeof(Player));
        state->entities[i] = (Entity *)p;
        p->e.init = (EntityInitFunc)player_init;
        p->e.init((Entity *)p, state->spawnPositions[i], state->modelBanana);
        p->e.update = player_entity_update;
        p->e.cleanup = player_entity_cleanup;
        p->playerIndex = i;
        state->HP[i] = p->hitpoints;
    }

    // Intialize weapons
    for (int i = 0; i < 2; i++)
    {
        debugf("Initializing weapon %d\n", i + 1);
        Weapon *p = malloc(sizeof(Weapon));
        state->entities[state->numPlayers + i] = (Entity *)p;
        p->e.cleanup = weapon_entity_cleanup;
        p->e.update = weapon_entity_update;
    }
    weapon_init(state->entities[state->numPlayers], (T3DVec3){{0.0f, 0.0f, 0.0f}}, state->modelWeapon);
    weapon_init(state->entities[state->numPlayers + 1], (T3DVec3){{50.0f, 0.0f, 50.0f}}, state->modelWeapon);

    state->numEntities = state->numPlayers + 2;

    // Timing variables
    state->lastTime = get_time_s() - (1.0f / 60.0f);
    state->frameIdx = 0;
    // Per-player initialization tasks happen in this loop

    // Load p1 HP bar sprite
    state->spriteBanana = sprite_load("rom:/hpbar.sprite");
}

static void battle_init(BattleState *state)
{
    memset(state, 0, sizeof(*state));
    state->numPlayers = 4;
    state->depthBuffer = display_get_zbuf();
    state->viewport = t3d_viewport_create_buffered(FB_COUNT);
    state->camPos = (T3DVec3){{0, 150.0f, 200.0f}};
    state->camTarget = (T3DVec3){{0, 0, 40}};
    state->lightDirVec = (T3DVec3){{1.0f, 1.0f, 1.0f}};
    t3d_vec3_norm(&state->lightDirVec);

    memcpy(state->spawnPositions, defaultSpawnPositions, sizeof(defaultSpawnPositions));

    state->modelMap = t3d_model_load("rom:/map.t3dm");
    int mapSpawns = enumerate_map_objects(state->modelMap, state->spawnPositions, 4);
    debugf("Map spawns found: %d\n", mapSpawns);
    rspq_block_begin();
    T3DModelDrawConf drawConf = {0};
    drawConf.filterCb = map_draw_filter;
    t3d_model_draw_custom(state->modelMap, drawConf);
    state->dplMap = rspq_block_end();
}

static void battle_shutdown(BattleState *state)
{
    battle_cleanup_match(state);
    if (state->dplMap)
        rspq_block_free(state->dplMap);
    state->dplMap = NULL;

    if (state->modelMap)
        t3d_model_free(state->modelMap);
    state->modelMap = NULL;

    t3d_viewport_destroy(&state->viewport);
}

void battle_mode_loop(void)
{
    BattleState state;
    battle_init(&state);
    battle_start_match(&state);

    state.gameMode = GAME_MODE_PLAY;
    bool exit_to_menu = false;

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
            if (state.debugDrawMapCandidates)
            {
                // Process a few deferred callbacks without blocking.
                for (int i = 0; i < 4; i++)
                {
                    if (!__rspq_deferred_poll())
                        break;
                }
            }

            uint64_t mix_ticks = (uint64_t)__mixer_profile_rsp;
            state.perf_last.cpu_mixer_wait_us = ticks_to_us(mix_ticks - state.perf_prev_mixer_ticks);
            state.perf_prev_mixer_ticks = mix_ticks;

            rspq_profile_data_t cur = {0};
            rspq_profile_get_data(&cur);
            // Only compute a delta if we have a new profiled frame.
            if (cur.frame_count != 0 && cur.frame_count != state.perf_prev_rspq.frame_count)
            {
                uint64_t dt_total = cur.total_ticks - state.perf_prev_rspq.total_ticks;
                uint64_t dt_busy = cur.rdp_busy_ticks - state.perf_prev_rspq.rdp_busy_ticks;
                state.perf_last.rspq_ok = dt_total > 0;
                state.perf_last.rsp_frame_us = rcp_ticks_to_us(dt_total);
                state.perf_last.rdp_busy_us = rcp_ticks_to_us(dt_busy);

                // Find top two overlays/slots for this frame.
                uint64_t top1 = 0, top2 = 0;
                const char *top1n = NULL, *top2n = NULL;
                for (int i = 0; i < RSPQ_PROFILE_SLOT_COUNT; i++)
                {
                    const char *name = cur.slots[i].name;
                    if (!name)
                        continue;
                    uint64_t dt = cur.slots[i].total_ticks - state.perf_prev_rspq.slots[i].total_ticks;
                    if (dt > top1)
                    {
                        top2 = top1;
                        top2n = top1n;
                        top1 = dt;
                        top1n = name;
                    }
                    else if (dt > top2)
                    {
                        top2 = dt;
                        top2n = name;
                    }
                }

                memset(state.perf_last.top1_name, 0, sizeof(state.perf_last.top1_name));
                memset(state.perf_last.top2_name, 0, sizeof(state.perf_last.top2_name));
                if (top1n)
                    strncpy(state.perf_last.top1_name, top1n, sizeof(state.perf_last.top1_name) - 1);
                if (top2n)
                    strncpy(state.perf_last.top2_name, top2n, sizeof(state.perf_last.top2_name) - 1);
                state.perf_last.top1_us = rcp_ticks_to_us(top1);
                state.perf_last.top2_us = rcp_ticks_to_us(top2);

                state.perf_prev_rspq = cur;
            }
            else
            {
                state.perf_last.rspq_ok = false;
                state.perf_last.rsp_frame_us = 0;
                state.perf_last.rdp_busy_us = 0;
                state.perf_last.top1_name[0] = '\0';
                state.perf_last.top2_name[0] = '\0';
                state.perf_last.top1_us = 0;
                state.perf_last.top2_us = 0;
            }
        }

        // Pump audio early in the frame (before RSP-heavy work).
        // Keep it small to avoid fighting with the renderer.
        audio_pump(1);

        // Keep track of frame index for animation matrices(buffered)
        state.frameIdx = (state.frameIdx + 1) % FB_COUNT;
        float newTime = get_time_s();
        float deltaTime = newTime - state.lastTime;
        state.lastTime = newTime;
        // debugf("Frame Time: %.4f s (%.2f FPS)\n", deltaTime, 1.0f / deltaTime);

        // Update global Y rotation for weapons
        state.globalYrot += ((2 * T3D_PI) / 60.0f); // rotate 360 degrees every 60 frames
        state.globalYrot = fmodf(state.globalYrot, (2 * T3D_PI));

        // Poll the joypads
        joypad_poll();

        joypad_buttons_t joypad1_btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);

        joypad_inputs_t joypad1 = joypad_get_inputs(JOYPAD_PORT_1);
        if (joypad1_btn.start && state.gameMode == GAME_MODE_PLAY)
        {
            state.gameMode = GAME_MODE_PAUSE;
        }
        else if (joypad1_btn.start && state.gameMode == GAME_MODE_PAUSE)
        {
            menuSelection = 0;
            state.gameMode = GAME_MODE_PLAY; // toggle between 0 and 1
        }

        if (joypad1_btn.c_down)
        {
            wav64_play(&dominating, SFX_CH);
            state.debugDrawMapCandidates = !state.debugDrawMapCandidates;
        }

        // Set viewport
        t3d_viewport_set_projection(&state.viewport, T3D_DEG_TO_RAD(90.0f), 20.0f, 320.0f);
        t3d_viewport_look_at(&state.viewport, &state.camPos, &state.camTarget, &(T3DVec3){{0, 1, 0}});

        // ======== Draw (3D) ======== //
        surface_t *surface = display_get();
        rdpq_attach(surface, state.depthBuffer);
        t3d_frame_start();
        t3d_viewport_attach(&state.viewport);

        t3d_screen_clear_color(RGBA32(224, 180, 96, 0xFF));
        t3d_screen_clear_depth();

        t3d_light_set_ambient(colorAmbient);
        t3d_light_set_directional(0, colorDir, &state.lightDirVec);
        t3d_light_set_count(1);

        // Draw map first (background)
        rspq_block_run(state.dplMap);

        switch (state.gameMode)
        {
        case GAME_MODE_PLAY:
        {
            // Update players //
            did_i_win(&state.winner, state.entities, state.numPlayers);

            if (state.winner != -1)
            {
                state.gameMode = GAME_MODE_END;
                break;
            }
            EntityUpdateContext updateCtx = {
                .deltaTime = deltaTime,
                .frameIdx = state.frameIdx,
                .camPos = &state.camPos,
                .globalYrot = state.globalYrot,
                .entities = state.entities,
                .numPlayers = state.numPlayers,
                .state = &state,
            };

            for (int i = 0; i < state.numEntities; i++)
            {
                if (state.entities[i] && state.entities[i]->update)
                    state.entities[i]->update(state.entities[i], &updateCtx);
            }
            for (int i = 0; i < 2; i++)
            {
                weapon_draw_hitbubble((Weapon *)state.entities[state.numPlayers + i], state.hitbubbleFP, state.dplHitbubble);
            }

            // Draw all entities generically
            for (int i = 0; i < state.numEntities; i++)
            {
                entity_draw(state.entities[i], state.frameIdx);
            }

            // ======== Draw (2D) ======== //
            rdpq_sync_pipe();
            rdpq_set_scissor(0, 0, sizeX, sizeY);
            rdpq_set_mode_standard();

            // Get all players HP and linearly interpolate for smooth bar movement
            for (int i = 0; i < 4; i++)
            {
                state.HP[i] = t3d_lerp(state.HP[i], ((Player *)state.entities[i])->hitpoints, 0.5f);
            }

            // Draw green HP bars first
            rdpq_set_mode_fill(RGBA32(0, 0xCC, 0, 0xFF));
            rdpq_fill_rectangle(20, 25, state.HP[0] + 20, 30);
            rdpq_fill_rectangle(200, 20, state.HP[1] + 200, 25);
            rdpq_fill_rectangle(20, 200, state.HP[2] + 20, 205);
            rdpq_fill_rectangle(200, 200, state.HP[3] + 200, 205);

            // Then draw red HP bar for missing HP
            rdpq_set_mode_fill(RGBA32(0xCC, 0, 0, 0xFF));
            rdpq_fill_rectangle(state.HP[0] + 20, 25, 120, 30);
            rdpq_fill_rectangle(state.HP[1] + 200, 20, 300, 25);
            rdpq_fill_rectangle(state.HP[2] + 20, 200, 120, 205);
            rdpq_fill_rectangle(state.HP[3] + 200, 200, 300, 205);
            // Draw shared 2D overlay (if desired across all modes)
            rdpq_set_mode_standard();
            rdpq_mode_alphacompare(128);
            rdpq_sprite_blit(state.spriteBanana, 10, 20, NULL);
        }
        break;
        case GAME_MODE_PAUSE:
        {
            float posX = 127;
            float posY = 40;
            if (menuSelection > 2)
                menuSelection = 2;
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

            if (joypad1.stick_y > 24)
            {
                menuSelection--;
                if (menuSelection < 0)
                    menuSelection = 0;
            }
            if (joypad1.stick_y < -24)
            {
                menuSelection++;
                if (menuSelection > 2)
                    menuSelection = 2;
            }
            if (joypad1_btn.a)
            {
                switch (menuSelection)
                {
                case 0:
                    menuSelection = 0;
                    state.gameMode = GAME_MODE_PLAY;
                    break;
                case 1:
                    battle_cleanup_match(&state);
                    battle_start_match(&state);
                    state.gameMode = GAME_MODE_PLAY;
                    break;
                case 2:
                    menuSelection = 0;
                    state.winner = -1;
                    battle_cleanup_match(&state);
                    exit_to_menu = true;
                    break;
                }
            }
        }
        break;
        case (GAME_MODE_END):
        {
            state.debugDrawMapCandidates = false;
            float posX = 127;
            float posY = 100;
            rdpq_sync_pipe();
            rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_TITLE) "Game Over!");
            posY += 20;
            rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_GREEN) "Player %d Wins!", state.winner + 1);
            posY += 20;
            rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, posX, posY, STYLE(STYLE_GREY) "Press A to return to Menu");

            if (joypad1_btn.a)
            {
                heap_stats_t stats;
                sys_get_heap_stats(&stats);
                debugf("Heap stats before cleanup: total=%u used=%u\n",
                       stats.total, stats.used);
                battle_cleanup_match(&state);
                exit_to_menu = true;
                menuSelection = 0;
            }
        }
        }

        if (exit_to_menu)
        {
            rdpq_detach_show();
            break;
        }

        // Debug overlay: CPU/RSP performance summary.
        if (state.debugDrawMapCandidates)
        {
            // Ensure the RDP is in a safe state before switching to 2D/text.
            // This matters especially on frames where we break out of gameplay
            // early (eg: winner detected) and skip the usual 2D sync.
            rdpq_sync_pipe();
            const float cpu_ms = (float)state.perf_last.cpu_frame_us / 1000.0f;
            const float mix_ms = (float)state.perf_last.cpu_mixer_wait_us / 1000.0f;

            rdpq_set_mode_standard();
            float y = 10;
            rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 10, y, STYLE(STYLE_GREY) "CPU frame: %5.2f ms  (mixer wait: %5.2f ms)", cpu_ms, mix_ms);
            y += 10;

            if (state.perf_last.rspq_ok)
            {
                float rsp_ms = (float)state.perf_last.rsp_frame_us / 1000.0f;
                float rdp_ms = (float)state.perf_last.rdp_busy_us / 1000.0f;
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 10, y, STYLE(STYLE_GREY) "RSP frame: %5.2f ms  (RDP busy: %5.2f ms)", rsp_ms, rdp_ms);
                y += 10;

                if (state.perf_last.top1_name[0])
                {
                    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 10, y, STYLE(STYLE_GREY) "Top: %-24s %5.2f ms", state.perf_last.top1_name, (float)state.perf_last.top1_us / 1000.0f);
                    y += 10;
                }
                if (state.perf_last.top2_name[0])
                {
                    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 10, y, STYLE(STYLE_GREY) "     %-24s %5.2f ms", state.perf_last.top2_name, (float)state.perf_last.top2_us / 1000.0f);
                    y += 10;
                }
            }
            else
            {
                rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 10, y, STYLE(STYLE_GREY) "RSPQ profiler: disabled/unavailable");
            }
        }

        rspq_syncpoint_new();
        rdpq_sync_tile();
        rdpq_sync_pipe(); // Hardware crashes otherwise

        // Optional CPU debug overlay: draw bounds/colliders (wireframe).
        // We must wait for the RDP to finish before touching the framebuffer.
        if (state.debugDrawMapCandidates && state.gameMode == GAME_MODE_PLAY && state.modelMap)
        {
            rdpq_detach_wait();

            // Broadphase: capsule-derived AABB vs map object AABBs (draw candidate objects).
            if (state.debugDrawMapCandidates && state.modelMap)
            {
                T3DMat4 mapIdentity = {0};
                mapIdentity.m[0][0] = 1.0f;
                mapIdentity.m[1][1] = 1.0f;
                mapIdentity.m[2][2] = 1.0f;
                mapIdentity.m[3][3] = 1.0f;

                uint32_t queryColor = graphics_make_color(0x00, 0xFF, 0xFF, 0xFF);
                uint32_t candColor = graphics_make_color(0x80, 0xFF, 0xFF, 0xFF);

                static float lastPrint = 0.0f;
                int totalCandidates = 0;

                for (int p = 0; p < 4; p++)
                {
                    if (!((Player *)state.entities[p])->alive)
                        continue;

                    // Use the gameplay AABB tied to the player (world-space).
                    AabbF query = ((Player *)state.entities[p])->e.aabb;
                    // Draw the query AABB.
                    debug_draw_aabbf(surface, &state.viewport, &query, queryColor);

                    // Iterate all map objects and draw those whose AABB overlaps query.
                    int playerCandidates = 0;
                    T3DModelIter it = t3d_model_iter_create(state.modelMap, T3D_CHUNK_TYPE_OBJECT);
                    while (t3d_model_iter_next(&it))
                    {
                        const T3DObject *obj = it.object;
                        AabbF objAabb = {
                            .min = (T3DVec3){{s16_to_f32(obj->aabbMin[0]), s16_to_f32(obj->aabbMin[1]), s16_to_f32(obj->aabbMin[2])}},
                            .max = (T3DVec3){{s16_to_f32(obj->aabbMax[0]), s16_to_f32(obj->aabbMax[1]), s16_to_f32(obj->aabbMax[2])}},
                        };

                        if (!aabbf_overlaps(&query, &objAabb))
                            continue;

                        // Cap draws to avoid turning the frame into a line soup.
                        if (playerCandidates < 64)
                        {
                            debug_draw_object_aabb_mat4(surface, &state.viewport, obj, &mapIdentity, candColor);
                        }
                        playerCandidates++;
                    }

                    totalCandidates += playerCandidates;
                }

                float now = get_time_s();
                if (now - lastPrint > 0.5f)
                {
                    debugf("Map broadphase candidates (sum over players): %d\n", totalCandidates);
                    lastPrint = now;
                }

                // Also draw weapon AABBs while in C-down mode.
                uint32_t weaponColors[2];
                weaponColors[0] = graphics_make_color(0xFF, 0x00, 0xFF, 0xFF);
                weaponColors[1] = graphics_make_color(0xFF, 0x80, 0xFF, 0xFF);
                for (int w = 0; w < 2; w++)
                {
                    debug_draw_aabbf(surface, &state.viewport, &((Weapon *)state.entities[state.numPlayers + w])->e.aabb, weaponColors[w]);
                }
            }

            uint32_t colors[4];
            colors[0] = graphics_make_color(0xFF, 0x40, 0x40, 0xFF);
            colors[1] = graphics_make_color(0x40, 0xFF, 0x40, 0xFF);
            colors[2] = graphics_make_color(0x40, 0xA0, 0xFF, 0xFF);
            colors[3] = graphics_make_color(0xFF, 0xFF, 0x40, 0xFF);

            for (int i = 0; i < 4; i++)
            {
                if (!((Player *)state.entities[i])->alive)
                    continue;
                debug_draw_aabbf(surface, &state.viewport, &((Player *)state.entities[i])->e.aabb, colors[i]);
            }

            display_show(surface);
        }
        else
        {
            rdpq_sync_full(NULL, NULL);
            rdpq_detach_show();
        }

        // Pump audio after graphics submission / swap, when the RSP is more
        // likely to be idle.
        audio_pump(2);

        // Tell the RSPQ profiler that a frame has completed. Do this only
        // when the debug overlay is active to avoid overhead.
        if (state.debugDrawMapCandidates)
        {
            rspq_profile_next_frame();
        }

        state.perf_last.cpu_frame_us = ticks_to_us((uint32_t)(TICKS_READ() - cpu_frame_start));
    }

    battle_shutdown(&state);
}
