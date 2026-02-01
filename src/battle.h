#ifndef BATTLE_H
#define BATTLE_H

#include <ctype.h>
#include <stdio.h>
#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>
#include <t3d/t3danim.h>
#include <rspq_profile.h>
#include "entities.h"
#include "util.h"
// Reserve a safe mixer channel for SFX. Stereo waveforms need ch+1, so avoid
// using the very last channel.
#define SFX_CH 30

#define STRINGIFY(x) #x
#define STYLE(id) "^0" STRINGIFY(id)
#define STYLE_TITLE 1
#define STYLE_GREY 2
#define STYLE_GREEN 3

#define MAX_ENTITIES 16

typedef struct BattleState
{
    surface_t *depthBuffer;
    T3DViewport viewport;
    T3DMat4FP *hitbubbleFP;
    rspq_block_t *dplMap;
    rspq_block_t *dplHitbubble;
    T3DModel *modelMap;
    T3DModel *modelWeapon;
    T3DModel *modelBanana;
    T3DModel *modelHitbubble;
    T3DVec3 camPos;
    T3DVec3 camTarget;
    T3DVec3 lightDirVec;
    T3DVec3 spawnPositions[4];
    float globalYrot;
    int gameMode;
    int frameIdx;
    int winner;
    float HP[4];
    float lastTime;
    sprite_t *spriteBanana;
    Entity *entities[MAX_ENTITIES];
    int numEntities;
    int numPlayers;
    bool debugDrawMapCandidates;
    perf_stats_t perf_last;
    uint64_t perf_prev_mixer_ticks;
    rspq_profile_data_t perf_prev_rspq;
} BattleState;

void battle_mode_loop(void);

#endif // BATTLE_H
