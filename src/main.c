#include <libdragon.h>
#include <t3d/t3d.h>
#include <rspq_profile.h>
#include "battle.h"
#include "util.h"

static xm64player_t musicPlayer;
wav64_t dominating, smack1, smack2, smack3, smack4;

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
    // console_init();
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS);
    t3d_init((T3DInitParams){});
    wav64_open(&dominating, "rom:/dominating.wav64");
    wav64_open(&smack1, "rom:/smack1.wav64");
    wav64_open(&smack2, "rom:/smack2.wav64");
    wav64_open(&smack3, "rom:/smack3.wav64");
    wav64_open(&smack4, "rom:/smack4.wav64");
    mixer_ch_set_vol(SFX_CH, 0.75f, 0.75f);
    // wav64_play(&dominating, SFX_CH);

    // Prime a few buffers so playback starts immediately.
    audio_pump(2);
    xm64player_open(&musicPlayer, "rom:/AQUA.xm64");
    xm64player_play(&musicPlayer, 0);
    rdpq_font_t *fnt = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_font_style(fnt, STYLE_TITLE, &(rdpq_fontstyle_t){RGBA32(0xAA, 0xAA, 0xFF, 0xFF)});
    rdpq_font_style(fnt, STYLE_GREY, &(rdpq_fontstyle_t){RGBA32(0x66, 0x66, 0x66, 0xFF)});
    rdpq_font_style(fnt, STYLE_GREEN, &(rdpq_fontstyle_t){RGBA32(0x39, 0xBF, 0x1F, 0xFF)});
    rdpq_text_register_font(FONT_BUILTIN_DEBUG_MONO, fnt);

    // Map model and drawlist are initialized per battle.
}

static void main_menu_loop(void)
{
    int menuSelection = 0;

    while (1)
    {
        joypad_poll();
        joypad_inputs_t joypad1 = joypad_get_inputs(JOYPAD_PORT_1);
        joypad_buttons_t joypad1_btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);

        if (joypad1.stick_y > 24)
        {
            menuSelection--;
            if (menuSelection < 0)
                menuSelection = 0;
        }
        if (joypad1.stick_y < -24)
        {
            menuSelection++;
            if (menuSelection > 1)
                menuSelection = 1;
        }

        if (joypad1_btn.a)
        {
            if (menuSelection == 0)
            {
                battle_mode_loop();
            }
        }
        surface_t *surface = display_get();
        rdpq_attach(surface, display_get_zbuf());
        rdpq_set_mode_standard();
        rdpq_set_mode_fill(RGBA32(20, 20, 30, 0xFF));
        rdpq_set_scissor(0, 0, display_get_width(), display_get_height());
        rdpq_fill_rectangle(0, 0, display_get_width(), display_get_height());

        float posX = 127;
        float posY = 40;
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

        rdpq_sync_full(NULL, NULL);
        rdpq_detach_show();
    }
}

int main(void)
{
    // console_init();
    game_init();
    main_menu_loop();
    return 0;
}