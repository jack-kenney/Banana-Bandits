#ifndef COLLISION_H
#define COLLISION_H

#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dmath.h>

typedef struct {
    T3DVec3 min;
    T3DVec3 max;
} AabbF;

#endif