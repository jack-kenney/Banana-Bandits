#ifndef ENTITIES_H
#define ENTITIES_H

#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include <libdragon.h>

#define JUMP_HEIGHT 12.0f
#define ATK_LENGTH 5.0f


typedef struct Weapon Weapon;

typedef struct {
    float hitpoints;
    int isHittable;
    T3DVec3 moveDir;
    bool alive;
    bool attacking;
    T3DVec3 playerPos;
    float currSpeed;
    float rotY;
    int jumpFrame;
    bool asc;
    rspq_block_t *dplPlayer;
    T3DMat4FP *modelMatFP;
    Weapon *weapon;
    bool hasWeapon;
    int attackFrame;
} Player;

struct Weapon {
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
};

extern Player players[4];
extern Weapon pipes[2];

/* Player API */
void player_init(Player *player, T3DVec3 position, T3DModel *model);
void player_update(Player *player, joypad_port_t port, T3DVec3 *camPos);

/* Weapon API */
void weapon_init(Weapon *weapon, T3DVec3 position, T3DModel *model);
void pipe_movement(Weapon *weapon, float globalYrot);

#endif // ENTITIES_H
