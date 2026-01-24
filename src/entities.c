#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include "util.h"
#include "entities.h"



void entity_init(Entity *e, T3DVec3 position, T3DModel *model, EntityType type)
{
    if (!e)
        return;

    e->type = type;
    e->modelMatFP = malloc_uncached(sizeof(T3DMat4FP) * FB_COUNT);
    e->pos = position;
    t3d_mat4fp_from_srt_euler(&e->modelMatFP[0],
                              (float[3]){1.0f, 1.0f, 1.0f},
                              (float[3]){0.0f, 0.0f, 0.0f},
                              e->pos.v);
    rspq_block_begin();
    t3d_model_draw(model);
    e->dplEntity = rspq_block_end();
}

void entity_update(Entity *e, const EntityUpdateContext *ctx)
{
    if (!e || !e->dplEntity)
        return;

    if (ctx && e->modelMatFP)
    {
        t3d_matrix_push(&e->modelMatFP[ctx->frameIdx]);
        rspq_block_run(e->dplEntity);
        t3d_matrix_pop(1);
    }
    else
    {
        rspq_block_run(e->dplEntity);
    }
}

void entity_cleanup(Entity *e)
{
    if (!e)
        return;

    if (e->dplEntity)
        rspq_block_free(e->dplEntity);

    if (e->modelMatFP)
        free_uncached(e->modelMatFP);

    e->dplEntity = NULL;
    e->modelMatFP = NULL;
}