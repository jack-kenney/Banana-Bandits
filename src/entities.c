#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include "util.h"
#include "entities.h"
#include "player.h"



void entity_init(Entity *e, T3DVec3 position, T3DModel *model, EntityType type)
{
    if (!e)
        return;

    e->type = type;
    e->visible = true;
    e->modelMatFP = malloc_uncached(sizeof(T3DMat4FP) * FB_COUNT);
    e->pos = position;
    for (int i = 0; i < FB_COUNT; i++)
    {
        t3d_mat4fp_from_srt_euler(&e->modelMatFP[i],
                                  (float[3]){1.0f, 1.0f, 1.0f},
                                  (float[3]){0.0f, 0.0f, 0.0f},
                                  e->pos.v);
        rspq_block_begin();
        t3d_matrix_push(&e->modelMatFP[i]);
        t3d_model_draw(model);
        t3d_matrix_pop(1);
        e->dplEntity[i] = rspq_block_end();
    }
}

void entity_update(Entity *e, const EntityUpdateContext *ctx)
{
    if (!e || !ctx || !e->modelMatFP)
        return;

    // Default behavior: keep model matrix in sync with current position.
    t3d_mat4fp_from_srt_euler(&e->modelMatFP[ctx->frameIdx],
                              (float[3]){1.0f, 1.0f, 1.0f},
                              (float[3]){0.0f, 0.0f, 0.0f},
                              e->pos.v);
}

void entity_draw(Entity *e, int frameIdx)
{
    if (!e || !e->visible)
        return;
    if (frameIdx < 0 || frameIdx >= FB_COUNT)
        return;
    if (!e->dplEntity[frameIdx])
        return;

    if (e->type == E_PLAYER)
    {
        Player *player = (Player *)e;
        if (player->skel)
            t3d_skeleton_use(player->skel);
    }

    rspq_block_run(e->dplEntity[frameIdx]);
}

void entity_cleanup(Entity *e)
{
    if (!e)
        return;

    for (int i = 0; i < FB_COUNT; i++)
    {
        if (e->dplEntity[i])
            rspq_block_free(e->dplEntity[i]);
        e->dplEntity[i] = NULL;
    }

    if (e->modelMatFP)
        free_uncached(e->modelMatFP);

    e->modelMatFP = NULL;
}