#include <stdio.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>
#include <t3d/t3danim.h>
#include <t3d/t3ddebug.h>
#include <libdragon.h>

surface_t *depthBuffer;
T3DViewport viewport;
rdpq_font_t *font;
rdpq_font_t *fontBillboard;
T3DMat4FP* mapMatFP;
rspq_block_t *dplMap;
T3DModel *model;
T3DModel *modelShadow;
T3DModel *modelMap;
T3DModel *modelCrystal;
T3DVec3 camPos;
T3DVec3 camTarget;
T3DVec3 lightDirVec;

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
    modelShadow = t3d_model_load("rom:/shadow.t3dm");
    modelCrystal = t3d_model_load("rom:/cube.t3dm");
    rspq_block_begin();
    t3d_model_draw(modelShadow);
    t3d_model_draw(modelCrystal);
    t3d_model_draw(modelMap);
    dplMap = rspq_block_end();
}

int main(void)
{
    //console_init();

    //debug_init_usblog();
    //console_set_debug(true);

    //printf("\n\n\n\n\n                 Lick my balls!\n");
    //printf("\n\n\n\n\n                 Thank you for reading this story.\n");
    asset_init_compression(2);
    asset_init_compression(3);
    //int res = dfs_init(DFS_DEFAULT_LOCATION);
    //if (res < 0) return 0;
    debug_init_usblog();
    debug_init_isviewer();
    console_set_debug(true);
    //debugf("DFS init result: %d\n", res);
    joypad_init();
    timer_init();
    rdpq_init();
    audio_init(32000, 3);
    mixer_init(32);
    game_init();

    uint8_t colorAmbient[4] = {0xAA, 0xAA, 0xAA, 0xFF};
    uint8_t colorDir[4]     = {0xFF, 0xAA, 0xAA, 0xFF};
    joypad_inputs_t inputs;
    while(1) {
        joypad_poll();
        inputs = joypad_get_inputs(JOYPAD_PORT_1);
        if (inputs.stick_x > 15){
            camTarget.x += 5.0f;
            if(inputs.btn.z) camPos.x += 5.0f;
        }
        if (inputs.stick_x < -15){
            camTarget.x -= 5.0f;
            if(inputs.btn.z) camPos.x -= 5.0f;
        }
        if(inputs.stick_y < -15){
            camTarget.z += 5.0f;
            camPos.z += 5.0f;
        }
        if(inputs.stick_y > 15){
            camTarget.z -= 5.0f;
            camPos.z -= 5.0f;
        }
        if(inputs.btn.c_right){
            camPos.y += 5.0f;
            camTarget.y += 5.0f;
        }
        if(inputs.btn.c_left){
            camPos.y -= 5.0f;
            camTarget.y -= 5.0f;
        }
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

        rspq_block_run(dplMap);
        syncPoint = rspq_syncpoint_new();
        rdpq_sync_tile();
        rdpq_sync_pipe(); // Hardware crashes otherwise
        rdpq_detach_show();
    }  
}