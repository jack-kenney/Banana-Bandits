#include "util.h"
#include "player.h"
#include "weapon.h"
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <libdragon.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Pump a small number of audio buffers. This helps prevent underruns when a
// frame takes longer (gameplay) without stalling too long in one spot.
void audio_pump(int max_buffers)
{
    for (int i = 0; i < max_buffers && audio_can_write(); i++)
    {
        mixer_try_play();
    }
}

uint64_t ticks_to_us(uint64_t ticks)
{
    return (ticks * 1000000ULL) / (uint64_t)TICKS_PER_SECOND;
}

uint64_t rcp_ticks_to_us(uint64_t ticks)
{
    return (ticks * 1000000ULL) / (uint64_t)RCP_FREQUENCY;
}

float s16_to_f32(int16_t v)
{
    return (float)v;
}

static void debug_draw_box_wireframe(surface_t *surface, T3DViewport *viewport, const T3DVec3 cornersWorld[8], uint32_t color)
{
    T3DVec3 screen[8];
    for (int i = 0; i < 8; i++)
    {
        t3d_viewport_calc_viewspace_pos(viewport, &screen[i], &cornersWorld[i]);
    }

    static const uint8_t edges[12][2] = {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},
        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4},
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7},
    };

    for (int e = 0; e < 12; e++)
    {
        int a = edges[e][0];
        int b = edges[e][1];
        int x0 = (int)screen[a].v[0];
        int y0 = (int)screen[a].v[1];
        int x1 = (int)screen[b].v[0];
        int y1 = (int)screen[b].v[1];
        graphics_draw_line(surface, x0, y0, x1, y1, color);
    }
}

void debug_draw_aabbf(surface_t *surface, T3DViewport *viewport, const AabbF *aabb, uint32_t color)
{
    const T3DVec3 cornersWorld[8] = {
        (T3DVec3){{aabb->min.v[0], aabb->min.v[1], aabb->min.v[2]}},
        (T3DVec3){{aabb->max.v[0], aabb->min.v[1], aabb->min.v[2]}},
        (T3DVec3){{aabb->max.v[0], aabb->min.v[1], aabb->max.v[2]}},
        (T3DVec3){{aabb->min.v[0], aabb->min.v[1], aabb->max.v[2]}},
        (T3DVec3){{aabb->min.v[0], aabb->max.v[1], aabb->min.v[2]}},
        (T3DVec3){{aabb->max.v[0], aabb->max.v[1], aabb->min.v[2]}},
        (T3DVec3){{aabb->max.v[0], aabb->max.v[1], aabb->max.v[2]}},
        (T3DVec3){{aabb->min.v[0], aabb->max.v[1], aabb->max.v[2]}},
    };

    debug_draw_box_wireframe(surface, viewport, cornersWorld, color);
}

void debug_draw_object_aabb_mat4(surface_t *surface, T3DViewport *viewport, const T3DObject *obj, const T3DMat4 *modelMat, uint32_t color)
{
    if (!surface || !viewport || !obj || !modelMat)
        return;

    const float minX = s16_to_f32(obj->aabbMin[0]);
    const float minY = s16_to_f32(obj->aabbMin[1]);
    const float minZ = s16_to_f32(obj->aabbMin[2]);
    const float maxX = s16_to_f32(obj->aabbMax[0]);
    const float maxY = s16_to_f32(obj->aabbMax[1]);
    const float maxZ = s16_to_f32(obj->aabbMax[2]);

    const T3DVec3 localCorners[8] = {
        (T3DVec3){{minX, minY, minZ}},
        (T3DVec3){{maxX, minY, minZ}},
        (T3DVec3){{maxX, minY, maxZ}},
        (T3DVec3){{minX, minY, maxZ}},
        (T3DVec3){{minX, maxY, minZ}},
        (T3DVec3){{maxX, maxY, minZ}},
        (T3DVec3){{maxX, maxY, maxZ}},
        (T3DVec3){{minX, maxY, maxZ}},
    };

    T3DVec3 cornersWorld[8];
    for (int i = 0; i < 8; i++)
    {
        T3DVec4 out;
        t3d_mat4_mul_vec3(&out, modelMat, &localCorners[i]);
        cornersWorld[i] = (T3DVec3){{out.v[0], out.v[1], out.v[2]}};
    }

    debug_draw_box_wireframe(surface, viewport, cornersWorld, color);
}

float get_time_s(void)
{
    return (float)((double)get_ticks_us() / 1000000.0);
}

void game_reset(T3DVec3 spawnPositions[4], Entity * entities[], int numPlayers)
{
    for (int i = 0; i < 4; i++)
    {
        ((Player *)entities[i])->hitpoints = 100.0f;
        ((Player *)entities[i])->alive = true;
        ((Player *)entities[i])->e.pos = spawnPositions[i];
        ((Player *)entities[i])->currSpeed = 0.0f;
        ((Player *)entities[i])->moveDir = (T3DVec3){{0, 0, 0}};
        ((Player *)entities[i])->rotY = 0.0f;
        if (((Player *)entities[i])->weapon)
        {
            ((Player *)entities[i])->weapon->equipped = false;
            ((Player *)entities[i])->weapon->attachedPlayer = NULL;
        }
        ((Player *)entities[i])->weapon = NULL;
    }

    for (int i = numPlayers; i < numPlayers + 2; i++)
    {
        ((Weapon *)entities[i])->equipped = false;
        ((Weapon *)entities[i])->attachedPlayer = NULL;
        ((Weapon *)entities[i])->isAttack = false;
        ((Weapon *)entities[i])->attackFrame = 0;
    }
}

void did_i_win(int *winner, Entity *entities[], int numPlayers)
{
    int aliveCount = 0;
    int lastAliveIdx = -1;
    for (int i = 0; i < numPlayers; i++)
    {
        if (((Player *)entities[i])->alive)
        {
            aliveCount++;
            lastAliveIdx = i;
        }
    }
    // debugf("Alive players: %d\n", aliveCount);
    // debugf("Current winner: %d\n", *winner);
    if (aliveCount == 1)
    {
        *winner = lastAliveIdx;
        debugf("Player %d wins the game!\n", *winner + 1);
    }
}

bool starts_with_ci(const char *str, const char *prefix)
{
    if (!str || !prefix)
        return false;

    for (; *prefix != '\0'; str++, prefix++)
    {
        if (*str == '\0')
            return false;
        if (tolower((unsigned char)*str) != tolower((unsigned char)*prefix))
            return false;
    }

    return true;
}

static int parse_spawn_index(const char *name, int maxSpawns)
{
    if (!name || maxSpawns <= 0)
        return -1;

    if (!starts_with_ci(name, "spawn"))
        return -1;

    const char *p = name + 5;
    if (*p == '_' || *p == '-' || *p == '.')
        p++;

    if (starts_with_ci(p, "player"))
    {
        p += 6;
        if (*p == '_' || *p == '-' || *p == '.')
            p++;
    }

    if (*p == 'p' || *p == 'P')
        p++;

    char *end = NULL;
    long idx = strtol(p, &end, 10);
    if (end == p)
        return -1;

    if (idx < 1 || idx > maxSpawns)
        return -1;

    return (int)(idx - 1);
}

T3DVec3 object_aabb_center(const T3DObject *obj)
{
    const float minX = s16_to_f32(obj->aabbMin[0]);
    const float minY = s16_to_f32(obj->aabbMin[1]);
    const float minZ = s16_to_f32(obj->aabbMin[2]);
    const float maxX = s16_to_f32(obj->aabbMax[0]);
    const float maxY = s16_to_f32(obj->aabbMax[1]);
    const float maxZ = s16_to_f32(obj->aabbMax[2]);

    return (T3DVec3){{
        (minX + maxX) * 0.5f,
        (minY + maxY) * 0.5f,
        (minZ + maxZ) * 0.5f,
    }};
}

int enumerate_map_objects(const T3DModel *model, T3DVec3 *spawnPositions, int maxSpawns)
{
    if (!model)
        return 0;

    if (maxSpawns > 4)
        maxSpawns = 4;
    if (!spawnPositions || maxSpawns <= 0)
        return 0;

    bool used[4] = {false, false, false, false};
    int found = 0;

    T3DModelIter it = t3d_model_iter_create(model, T3D_CHUNK_TYPE_OBJECT);
    while(t3d_model_iter_next(&it))
    {
        const T3DObject *obj = it.object;
        debugf("Map Object: %s, Parts: %d, Triangles: %d\n", obj->name ? obj->name : "(null)", obj->numParts, obj->triCount);

        if (!obj->name)
            continue;

        int idx = parse_spawn_index(obj->name, maxSpawns);
        if (idx < 0 && starts_with_ci(obj->name, "spawn"))
        {
            for (int i = 0; i < maxSpawns; i++)
            {
                if (!used[i])
                {
                    idx = i;
                    break;
                }
            }
        }

        if (idx >= 0 && idx < maxSpawns && !used[idx])
        {
            spawnPositions[idx] = object_aabb_center(obj);
            used[idx] = true;
            found++;
            debugf("Spawn %d from %s => (%f, %f, %f)\n", idx + 1, obj->name,
                   spawnPositions[idx].v[0], spawnPositions[idx].v[1], spawnPositions[idx].v[2]);
        }
    }

    return found;
}