#ifndef ENTITIES_H
#define ENTITIES_H

#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include <libdragon.h>
#include <t3d/t3dskeleton.h>
#include "collision.h"
#include <t3d/t3danim.h>

#define JUMP_HEIGHT 12.0f
#define ATK_LENGTH 30.0f

typedef struct Entity Entity;

typedef void (*EntityInitFunc)(Entity *e, T3DVec3 position, T3DModel *model);
typedef struct EntityUpdateContext {
    float deltaTime;
    int frameIdx;
    T3DVec3 *camPos;
    float globalYrot;
    Entity **entities;
    int numPlayers;
} EntityUpdateContext;

typedef void (*EntityUpdateFunc)(Entity *e, const EntityUpdateContext *ctx);  
typedef void (*EntityCleanupFunc)(Entity *e);

typedef enum {
    E_PLAYER,
    E_WEAPON
} EntityType;

typedef struct Entity {
    T3DVec3 pos;
    rspq_block_t *dplEntity;
    T3DMat4FP *modelMatFP;
    AabbF aabb;
    EntityInitFunc init;
    EntityUpdateFunc update;
    EntityCleanupFunc cleanup;
    EntityType type; 
} Entity;

typedef struct
{
    enum
    {
        STATE_IDLE,
        STATE_WALK,
        STATE_JUMP,
        STATE_ATTACK,
        STATE_ATTACK2,
        STATE_DEAD,
        STATE_HITLAG,
        STATE_DODGE
    } s;
    uint8_t frame;
} PlayerState;

typedef struct Weapon Weapon;

typedef struct
{
    Entity e;
    float hitpoints;
    int isHittable;
    T3DVec3 moveDir;
    bool alive;
    //T3DVec3 playerPos;
    float currSpeed;
    float rotY;
    int jumpFrame;
    bool asc;
    //rspq_block_t *dplPlayer;
    //T3DMat4FP *modelMatFP;
    T3DSkeleton *skel;
    T3DSkeleton *skelBlend;
    int handBoneIdx;
    Weapon *weapon;
    AabbF aabb;
    T3DAnim animIdle, animPunch, animPunch2, animDodge;
    PlayerState state, prevState;
    int playerIndex;
    EntityType type;
} Player;

struct Weapon
{
    Entity e;
    T3DVec3 wepPos;
    bool equipped;
    float damage;
    float rotY;
    rspq_block_t *dplWeapon;
    Player *attachedPlayer;
    T3DMat4FP *modelMatFP;
    bool isAttack;
    int attackFrame;
    T3DVec3 *hit;
    int bobFrame;
    int boneIndexWeapon;
    AabbF aabb;
};

//extern Player players[4];
//extern Weapon pipes[2];
/* Player API */
void player_init(Entity *e, T3DVec3 position, T3DModel *model);
void player_update(Player *player, joypad_port_t port, T3DVec3 *camPos, int frameIdx, float deltaTime);
void player_cleanup(Player *player);
void player_entity_cleanup(Entity *e);
void player_entity_update(Entity *e, const EntityUpdateContext *ctx);
void set_player_state(Player *player, PlayerState newState);

/* Weapon API */
void weapon_init(Entity *e, T3DVec3 position, T3DModel *model);
void pipe_movement(Weapon *weapon, float globalYrot, int frameIdx, Entity *entities[], int numPlayers);
void weapon_cleanup(Weapon *weapon);
void weapon_entity_cleanup(Entity *e);
void weapon_entity_update(Entity *e, const EntityUpdateContext *ctx);
void weapon_draw_hitbubble(Weapon *weapon, T3DMat4FP *hitbubbleFP, rspq_block_t *dplHitbubble);
void drop_weapon(Player *player);

// Updates weapon->aabb based on current state (pickup vs attack hitbox)
void weapon_refresh_aabb(Weapon *weapon);

#endif // ENTITIES_H
