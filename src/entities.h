#ifndef ENTITIES_H
#define ENTITIES_H

#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include <libdragon.h>
#include <t3d/t3dskeleton.h>

#define JUMP_HEIGHT 12.0f
#define ATK_LENGTH 30.0f

typedef struct Weapon Weapon;

typedef struct
{
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
    T3DSkeleton *skel;
    T3DSkeleton *skelBlend;
    int handBoneIdx;
    Weapon *weapon;
    bool hasWeapon;
    int attackFrame;
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
};

extern Player players[4];
extern Weapon pipes[2];

/* Player API */
void player_init(Player *player, T3DVec3 position, T3DModel *model);
void player_update(Player *player, joypad_port_t port, T3DVec3 *camPos, int frameIdx);
void player_cleanup(Player *player);

/* Weapon API */
void weapon_init(Weapon *weapon, T3DVec3 position, T3DModel *model);
void pipe_movement(Weapon *weapon, float globalYrot, int frameIdx);
void weapon_cleanup(Weapon *weapon);

#endif // ENTITIES_H
