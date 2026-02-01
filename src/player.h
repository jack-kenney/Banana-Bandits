#ifndef PLAYER_H
#define PLAYER_H

#include "entities.h"

typedef struct Weapon Weapon;

typedef struct PlayerState
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

typedef struct Player
{
    Entity e;
    float hitpoints;
    int isHittable;
    T3DVec3 moveDir;
    bool alive;
    float currSpeed;
    float rotY;
    int jumpFrame;
    bool asc;
    T3DSkeleton *skel;
    T3DSkeleton *skelBlend;
    int handBoneIdx;
    Weapon *weapon;
    T3DAnim animIdle, animPunch, animPunch2, animDodge;
    PlayerState state, prevState;
    int playerIndex;
    EntityType type;
} Player;

/* Player API */
void player_init(Entity *e, T3DVec3 position, T3DModel *model);
void player_update(Player *player, joypad_port_t port, T3DVec3 *camPos, int frameIdx, float deltaTime, Entity *entities[], int numPlayers, BattleState *state);
void player_cleanup(Player *player);
void player_entity_cleanup(Entity *e);
void player_entity_update(Entity *e, const EntityUpdateContext *ctx);
void set_player_state(Player *player, PlayerState newState);
void drop_weapon(Player *player);



#endif // PLAYER_H

