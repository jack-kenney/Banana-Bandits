#include "entities.h"
#include <t3d/t3dmath.h>

void game_reset(T3DVec3 spawnPositions[4]){
    for(int i = 0; i < 4; i++){
        players[i].hitpoints = 100.0f;
        players[i].alive = true;
        players[i].playerPos = spawnPositions[i];
        players[i].currSpeed = 0.0f;
        players[i].moveDir = (T3DVec3){{0,0,0}};
        players[i].rotY = 0.0f;
        players[i].attacking = false;
        players[i].attackFrame = 0;
        players[i].hasWeapon = false;
        if(players[i].weapon){
            players[i].weapon->equipped = false;
            players[i].weapon->attachedPlayer = NULL;
        }
        players[i].weapon = NULL;
    }

    for(int i = 0; i < 2; i++){
        pipes[i].equipped = false;
        pipes[i].attachedPlayer = NULL;
        pipes[i].isAttack = false;
        pipes[i].attackFrame = 0;
    }
    pipes[0].wepPos = (T3DVec3){{0.0f, 0.0f, 0.0f}};
    pipes[1].wepPos = (T3DVec3){{50.0f, 0.0f, 50.0f}};
}


void did_i_win(int *winner){
    int aliveCount = 0;
    int lastAliveIdx = -1;
    for(int i = 0; i < 4; i++){
        if(players[i].alive){
            aliveCount++;
            lastAliveIdx = i;
        }
    }
    //debugf("Alive players: %d\n", aliveCount);
    //debugf("Current winner: %d\n", *winner);
    if(aliveCount == 1){
        *winner = lastAliveIdx;
        debugf("Player %d wins the game!\n", *winner + 1);
    }
}   
