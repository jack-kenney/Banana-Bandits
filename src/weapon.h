#include <stdio.h>
#include <t3d/t3d.h>
#include <player.h>


struct Weapon {
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
}; 