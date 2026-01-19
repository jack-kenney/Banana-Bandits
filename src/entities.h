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

typedef struct
{
    enum
    {
        STATE_IDLE,
        STATE_WALK,
        STATE_JUMP,
        STATE_ATTACK,
        STATE_DEAD,
        STATE_HITLAG,
        STATE_DODGE
    } s;
    uint8_t frame;
} PlayerState;

typedef struct Weapon Weapon;

typedef struct
{
    float hitpoints;
    int isHittable;
    T3DVec3 moveDir;
    bool alive;
    T3DVec3 playerPos;
    float currSpeed;
    float rotY;
    int jumpFrame;
    bool asc;
    rspq_block_t *dplPlayer;
    T3DMat4FP *modelMatFP;
    T3DSkeleton *skel;
    T3DSkeleton *skelBlend;
    int handBoneIdx;
    Weapon *weapon;
    AabbF aabb;
    T3DAnim animIdle, animPunch, animDodge;
    PlayerState state;
} Player;

struct Weapon
{
    T3DVec3 wepPos;
    bool equipped;
    float damage;
    float rotY;
    rspq_block_t *dplWeapon;
    rspq_block_t *dplIdle;
    rspq_block_t *dplCarry;
    Player *attachedPlayer;
    T3DMat4FP *modelMatFP;
    bool isAttack;
    int attackFrame;
    T3DVec3 *hit;
    int bobFrame;
    int boneIndexWeapon;
    AabbF aabb;
};

extern Player players[4];
extern Weapon pipes[2];

/* Player API */
void player_init(Player *player, T3DVec3 position, T3DModel *model);
void player_update(Player *player, joypad_port_t port, T3DVec3 *camPos, int frameIdx, float deltaTime);
void player_cleanup(Player *player);
void set_player_state(Player *player, PlayerState newState);

/* Weapon API */
void weapon_init(Weapon *weapon, T3DVec3 position, T3DModel *model);
void pipe_movement(Weapon *weapon, float globalYrot, int frameIdx);
void weapon_cleanup(Weapon *weapon);
void drop_weapon(Player *player);

// Updates weapon->aabb based on current state (pickup vs attack hitbox)
void weapon_refresh_aabb(Weapon *weapon);

#endif // ENTITIES_H
