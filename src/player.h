#include <stdio.h>
#include <t3d/t3d.h>
#include <weapon.h>

typedef struct Weapon Weapon;

typedef struct {
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

