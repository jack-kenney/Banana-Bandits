#include "collision.h"  

bool aabbf_overlaps(const AabbF *a, const AabbF *b)
{
    // Use a small epsilon so "just touching" doesn't count as overlap.
    const float eps = 0.01f;
    return (a->min.v[0] < (b->max.v[0] - eps) && a->max.v[0] > (b->min.v[0] + eps)) &&
           (a->min.v[1] < (b->max.v[1] - eps) && a->max.v[1] > (b->min.v[1] + eps)) &&
           (a->min.v[2] < (b->max.v[2] - eps) && a->max.v[2] > (b->min.v[2] + eps));
}
