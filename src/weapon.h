#ifndef WEAPON_H
#define WEAPON_H

#include "entities.h"

struct Player;

typedef struct Weapon
{
    Entity e;
    bool equipped;
    float damage;
    float rotY;
    struct Player *attachedPlayer;
    bool isAttack;
    int attackFrame;
    T3DVec3 *hit;
    int bobFrame;
    int boneIndexWeapon;
} Weapon;

/* Weapon API */
void weapon_init(Entity *e, T3DVec3 position, T3DModel *model);
void pipe_movement(Weapon *weapon, float globalYrot, int frameIdx, Entity *entities[], int numPlayers);
void weapon_cleanup(Weapon *weapon);
void weapon_entity_cleanup(Entity *e);
void weapon_entity_update(Entity *e, const EntityUpdateContext *ctx);
void weapon_draw_hitbubble(Weapon *weapon, T3DMat4FP *hitbubbleFP, rspq_block_t *dplHitbubble);
// Updates weapon->e.aabb based on current state (pickup vs attack hitbox)
void weapon_refresh_aabb(Weapon *weapon);

#endif // WEAPON_H