#ifndef UTIL_H
#define UTIL_H

#include "entities.h"
#include <t3d/t3dmath.h>

typedef struct {
    uint64_t cpu_frame_us;
    uint64_t cpu_mixer_wait_us;

    bool rspq_ok;
    uint64_t rsp_frame_us;
    uint64_t rdp_busy_us;

    char top1_name[26];
    uint64_t top1_us;
    char top2_name[26];
    uint64_t top2_us;
} perf_stats_t;


void did_i_win(int *winner);

void audio_pump(int max_buffers);
uint64_t ticks_to_us(uint64_t ticks);
uint64_t rcp_ticks_to_us(uint64_t ticks);
float s16_to_f32(int16_t v);
float get_time_s(void);

void debug_draw_aabbf(surface_t *surface, T3DViewport *viewport, const AabbF *aabb, uint32_t color);
void debug_draw_object_aabb_mat4(surface_t *surface, T3DViewport *viewport, const T3DObject *obj, const T3DMat4 *modelMat, uint32_t color);

#endif
