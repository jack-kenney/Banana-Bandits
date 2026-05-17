#ifndef ENTITIES_H
#define ENTITIES_H

#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include <libdragon.h>
#include <t3d/t3dskeleton.h>
#include "collision.h"
#include <t3d/t3danim.h>

#define FB_COUNT 3
#define JUMP_HEIGHT 12.0f
#define ATK_LENGTH 30.0f

typedef struct Entity Entity;
typedef struct BattleState BattleState;

typedef void (*EntityInitFunc)(Entity *e, T3DVec3 position, T3DModel *model);
typedef struct EntityUpdateContext {
    float deltaTime;
    int frameIdx;
    T3DVec3 *camPos;
    float globalYrot;
    const joypad_inputs_t *joypadInputs;
    const joypad_buttons_t *joypadPressed;
    Entity **entities;
    int numPlayers;
    BattleState *state;
} EntityUpdateContext;

typedef void (*EntityUpdateFunc)(Entity *e, const EntityUpdateContext *ctx);  
typedef void (*EntityCleanupFunc)(Entity *e);

typedef enum {
    E_PLAYER,
    E_WEAPON,
    E_NPC
} EntityType;

typedef struct Entity {
    T3DVec3 pos;
    rspq_block_t *dplEntity[FB_COUNT];
    T3DMat4FP *modelMatFP;
    AabbF aabb;
    EntityInitFunc init;
    EntityUpdateFunc update;
    EntityCleanupFunc cleanup;
    EntityType type; 
    bool visible;
} Entity;

void entity_init(Entity *e, T3DVec3 position, T3DModel *model, EntityType type);
void entity_update(Entity *e, const EntityUpdateContext *ctx);
void entity_draw(Entity *e, int frameIdx);
void entity_cleanup(Entity *e);

#endif // ENTITIES_H
