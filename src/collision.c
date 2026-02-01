#include "collision.h"
#include "entities.h"
#include "util.h"
#include <libdragon.h>
#include <math.h>
#include <stdint.h>

static inline float clampf(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static inline void *align_pointer(void *ptr, uint32_t alignment)
{
    uintptr_t p = (uintptr_t)ptr;
    uintptr_t mask = (uintptr_t)alignment - 1u;
    return (void *)((p + mask) & ~mask);
}

static inline T3DVec3 vec3_from_s16(const int16_t *p)
{
    return (T3DVec3){{(float)p[0], (float)p[1], (float)p[2]}};
}

static inline void entity_translate(Entity *e, const T3DVec3 *delta)
{
    e->pos.v[0] += delta->v[0];
    e->pos.v[1] += delta->v[1];
    e->pos.v[2] += delta->v[2];
    e->aabb.min.v[0] += delta->v[0];
    e->aabb.min.v[1] += delta->v[1];
    e->aabb.min.v[2] += delta->v[2];
    e->aabb.max.v[0] += delta->v[0];
    e->aabb.max.v[1] += delta->v[1];
    e->aabb.max.v[2] += delta->v[2];
}
static T3DVec3 closest_point_on_triangle(const T3DVec3 *p, const T3DVec3 *a, const T3DVec3 *b, const T3DVec3 *c)
{
    //debugf("closest_point_on_triangle\n");
    T3DVec3 ab, ac, ap;
    t3d_vec3_diff(&ab, b, a);
    t3d_vec3_diff(&ac, c, a);
    t3d_vec3_diff(&ap, p, a);

    float d1 = t3d_vec3_dot(&ab, &ap);
    float d2 = t3d_vec3_dot(&ac, &ap);
    if (d1 <= 0.0f && d2 <= 0.0f)
        return *a;

    T3DVec3 bp;
    t3d_vec3_diff(&bp, p, b);
    float d3 = t3d_vec3_dot(&ab, &bp);
    float d4 = t3d_vec3_dot(&ac, &bp);
    if (d3 >= 0.0f && d4 <= d3)
        return *b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        T3DVec3 res;
        t3d_vec3_scale(&res, &ab, v);
        t3d_vec3_add(&res, a, &res);
        return res;
    }

    T3DVec3 cp;
    t3d_vec3_diff(&cp, p, c);
    float d5 = t3d_vec3_dot(&ab, &cp);
    float d6 = t3d_vec3_dot(&ac, &cp);
    if (d6 >= 0.0f && d5 <= d6)
        return *c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        float w = d2 / (d2 - d6);
        T3DVec3 res;
        t3d_vec3_scale(&res, &ac, w);
        t3d_vec3_add(&res, a, &res);
        return res;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        T3DVec3 bc;
        t3d_vec3_diff(&bc, c, b);
        T3DVec3 res;
        t3d_vec3_scale(&res, &bc, w);
        t3d_vec3_add(&res, b, &res);
        return res;
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    T3DVec3 res;
    res.v[0] = a->v[0] + ab.v[0] * v + ac.v[0] * w;
    res.v[1] = a->v[1] + ab.v[1] * v + ac.v[1] * w;
    res.v[2] = a->v[2] + ab.v[2] * v + ac.v[2] * w;
    return res;
}

static void segment_segment_closest(const T3DVec3 *p1, const T3DVec3 *q1, const T3DVec3 *p2, const T3DVec3 *q2,
                                    T3DVec3 *c1, T3DVec3 *c2)
{
    T3DVec3 d1, d2, r;
    t3d_vec3_diff(&d1, q1, p1);
    t3d_vec3_diff(&d2, q2, p2);
    t3d_vec3_diff(&r, p1, p2);

    float a = t3d_vec3_dot(&d1, &d1);
    float e = t3d_vec3_dot(&d2, &d2);
    float f = t3d_vec3_dot(&d2, &r);

    float s = 0.0f;
    float t = 0.0f;

    if (a <= 1e-6f && e <= 1e-6f)
    {
        *c1 = *p1;
        *c2 = *p2;
        return;
    }

    if (a <= 1e-6f)
    {
        s = 0.0f;
        t = clampf(f / e, 0.0f, 1.0f);
    }
    else
    {
        float c = t3d_vec3_dot(&d1, &r);
        if (e <= 1e-6f)
        {
            t = 0.0f;
            s = clampf(-c / a, 0.0f, 1.0f);
        }
        else
        {
            float b = t3d_vec3_dot(&d1, &d2);
            float denom = a * e - b * b;
            if (denom != 0.0f)
                s = clampf((b * f - c * e) / denom, 0.0f, 1.0f);
            else
                s = 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f)
            {
                t = 0.0f;
                s = clampf(-c / a, 0.0f, 1.0f);
            }
            else if (t > 1.0f)
            {
                t = 1.0f;
                s = clampf((b - c) / a, 0.0f, 1.0f);
            }
        }
    }

    c1->v[0] = p1->v[0] + d1.v[0] * s;
    c1->v[1] = p1->v[1] + d1.v[1] * s;
    c1->v[2] = p1->v[2] + d1.v[2] * s;
    c2->v[0] = p2->v[0] + d2.v[0] * t;
    c2->v[1] = p2->v[1] + d2.v[1] * t;
    c2->v[2] = p2->v[2] + d2.v[2] * t;
}

static bool point_in_triangle(const T3DVec3 *p, const T3DVec3 *a, const T3DVec3 *b, const T3DVec3 *c)
{
    T3DVec3 ab, bc, ca, ap, bp, cp, n1, n2, n3;
    t3d_vec3_diff(&ab, b, a);
    t3d_vec3_diff(&bc, c, b);
    t3d_vec3_diff(&ca, a, c);
    t3d_vec3_diff(&ap, p, a);
    t3d_vec3_diff(&bp, p, b);
    t3d_vec3_diff(&cp, p, c);

    t3d_vec3_cross(&n1, &ab, &ap);
    t3d_vec3_cross(&n2, &bc, &bp);
    t3d_vec3_cross(&n3, &ca, &cp);

    float d1 = t3d_vec3_dot(&n1, &n2);
    float d2 = t3d_vec3_dot(&n1, &n3);
    return (d1 >= 0.0f && d2 >= 0.0f);
}
static float segment_triangle_closest(const T3DVec3 *p0, const T3DVec3 *p1,
                                      const T3DVec3 *a, const T3DVec3 *b, const T3DVec3 *c,
                                      T3DVec3 *outSeg, T3DVec3 *outTri)
{
    T3DVec3 ab, ac, dir, n;
    t3d_vec3_diff(&ab, b, a);
    t3d_vec3_diff(&ac, c, a);
    t3d_vec3_diff(&dir, p1, p0);
    t3d_vec3_cross(&n, &ab, &ac);
    float denom = t3d_vec3_dot(&n, &dir);
    if (fabsf(denom) > 1e-6f)
    {
        T3DVec3 ap;
        t3d_vec3_diff(&ap, a, p0);
        float t = t3d_vec3_dot(&n, &ap) / denom;
        if (t >= 0.0f && t <= 1.0f)
        {
            T3DVec3 ip;
            ip.v[0] = p0->v[0] + dir.v[0] * t;
            ip.v[1] = p0->v[1] + dir.v[1] * t;
            ip.v[2] = p0->v[2] + dir.v[2] * t;
            if (point_in_triangle(&ip, a, b, c))
            {
                *outSeg = ip;
                *outTri = ip;
                return 0.0f;
            }
        }
    }

    // Check endpoints vs triangle.
    T3DVec3 cp0 = closest_point_on_triangle(p0, a, b, c);
    T3DVec3 cp1 = closest_point_on_triangle(p1, a, b, c);
    float d0 = t3d_vec3_distance2(p0, &cp0);
    float d1 = t3d_vec3_distance2(p1, &cp1);

    float best = d0;
    *outSeg = *p0;
    *outTri = cp0;
    if (d1 < best)
    {
        best = d1;
        *outSeg = *p1;
        *outTri = cp1;
    }

    // Check segment vs triangle edges.
    T3DVec3 e0, e1, c1, c2;
    e0 = *a;
    e1 = *b;
    segment_segment_closest(p0, p1, &e0, &e1, &c1, &c2);
    float de = t3d_vec3_distance2(&c1, &c2);
    if (de < best)
    {
        best = de;
        *outSeg = c1;
        *outTri = c2;
    }

    e0 = *b;
    e1 = *c;
    segment_segment_closest(p0, p1, &e0, &e1, &c1, &c2);
    de = t3d_vec3_distance2(&c1, &c2);
    if (de < best)
    {
        best = de;
        *outSeg = c1;
        *outTri = c2;
    }

    e0 = *c;
    e1 = *a;
    segment_segment_closest(p0, p1, &e0, &e1, &c1, &c2);
    de = t3d_vec3_distance2(&c1, &c2);
    if (de < best)
    {
        best = de;
        *outSeg = c1;
        *outTri = c2;
    }

    return best;
}
static inline T3DVec3 vert_pos(const T3DVertPacked *verts, int idx)
{
    return vec3_from_s16(t3d_vertbuffer_get_pos((T3DVertPacked *)verts, idx));
}

static inline bool map_part_index(const T3DObjectPart *part, int idx, int *outLocal)
{
    int local = idx - (int)part->vertDestOffset;
    if (local < 0 || (uint32_t)local >= part->vertLoadCount)
        return false;
    *outLocal = local;
    return true;
}

static inline int decode_strip_index(int16_t val)
{
    uint16_t raw = (uint16_t)val;
    raw &= 0x3FFF; // strip restart/end flags are in bits 15/14
    const uint16_t base = 0x0408; // low 16 bits of RSP_T3D_VERT_BUFFER
    const uint16_t stride = 36;   // VERT_OUTPUT_SIZE in tiny3d
    if (raw >= base)
        return (int)((raw - base) / stride);
    return (int)raw;
}

static bool resolve_capsule_triangle(Entity *entity, float radius, T3DVec3 *segA, T3DVec3 *segB,
                                     const T3DVec3 *v0, const T3DVec3 *v1, const T3DVec3 *v2, bool *onGround)
{
    T3DVec3 cSeg, cTri;
    float dist2 = segment_triangle_closest(segA, segB, v0, v1, v2, &cSeg, &cTri);
    if (dist2 >= radius * radius)
        return false;

    float dist = sqrtf(fmaxf(dist2, 1e-6f));
    float pen = radius - dist;
    T3DVec3 delta;
    T3DVec3 diff;
    t3d_vec3_diff(&diff, &cSeg, &cTri);

    T3DVec3 ab, ac, n;
    t3d_vec3_diff(&ab, v1, v0);
    t3d_vec3_diff(&ac, v2, v0);
    t3d_vec3_cross(&n, &ab, &ac);
    float nlen2 = t3d_vec3_len2(&n);
    if (nlen2 > 1e-6f)
        t3d_vec3_norm(&n);

    float diffDotN = (nlen2 > 1e-6f) ? t3d_vec3_dot(&diff, &n) : 0.0f;
    if (nlen2 > 1e-6f && diffDotN < 0.0f)
    {
        n.v[0] = -n.v[0];
        n.v[1] = -n.v[1];
        n.v[2] = -n.v[2];
        diffDotN = -diffDotN;
    }

    // Favor pushing along the triangle normal for floor-like surfaces.
    if (nlen2 > 1e-6f && n.v[1] > 0.4f && diffDotN >= 0.0f)
    {
        t3d_vec3_scale(&delta, &n, pen);
    }
    else if (dist > 1e-5f)
    {
        t3d_vec3_scale(&delta, &diff, pen / dist);
    }
    else if (nlen2 > 1e-6f)
    {
        t3d_vec3_scale(&delta, &n, pen);
    }
    else
    {
        return false;
    }

    entity_translate(entity, &delta);
    segA->v[0] += delta.v[0];
    segA->v[1] += delta.v[1];
    segA->v[2] += delta.v[2];
    segB->v[0] += delta.v[0];
    segB->v[1] += delta.v[1];
    segB->v[2] += delta.v[2];

    if (onGround && (delta.v[1] > 0.0f || (nlen2 > 1e-6f && n.v[1] > 0.7f && diffDotN >= 0.0f)))
        *onGround = true;

    return true;
}

bool aabbf_overlaps(const AabbF *a, const AabbF *b)
{
    // Use a small epsilon so "just touching" doesn't count as overlap.
    const float eps = 0.01f;
    return (a->min.v[0] < (b->max.v[0] - eps) && a->max.v[0] > (b->min.v[0] + eps)) &&
           (a->min.v[1] < (b->max.v[1] - eps) && a->max.v[1] > (b->min.v[1] + eps)) &&
           (a->min.v[2] < (b->max.v[2] - eps) && a->max.v[2] > (b->min.v[2] + eps));
}

bool collision_resolve_entity_vs_map(Entity *entity, const T3DModel *model, bool *onGround)
{
    if (!entity || !model)
        return false;

    if (onGround)
        *onGround = false;

    float widthX = entity->aabb.max.v[0] - entity->aabb.min.v[0];
    float widthZ = entity->aabb.max.v[2] - entity->aabb.min.v[2];
    float height = entity->aabb.max.v[1] - entity->aabb.min.v[1];
    float radius = 0.5f * fmaxf(widthX, widthZ);
    if (radius < 0.01f)
        radius = 0.01f;

    float segLen = height - 2.0f * radius;
    if (segLen < 0.0f)
        segLen = 0.0f;

    T3DVec3 segA = (T3DVec3){{entity->pos.v[0], entity->pos.v[1] + radius, entity->pos.v[2]}};
    T3DVec3 segB = (T3DVec3){{entity->pos.v[0], entity->pos.v[1] + radius + segLen, entity->pos.v[2]}};

    bool collided = false;
    static int debug_frames = 0;
    bool debug_this = (debug_frames++ % 120) == 0;

    T3DModelIter it = t3d_model_iter_create(model, T3D_CHUNK_TYPE_OBJECT);
    while (t3d_model_iter_next(&it))
    {
        const T3DObject *obj = it.object;
        if (!obj)
            continue;
        if (obj->name && starts_with_ci(obj->name, "spawn"))
            continue;

        if (debug_this)
            debugf("[collision] obj=%s parts=%u aabb=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f)\n",
                   obj->name ? obj->name : "(null)", obj->numParts,
                   (float)obj->aabbMin[0], (float)obj->aabbMin[1], (float)obj->aabbMin[2],
                   (float)obj->aabbMax[0], (float)obj->aabbMax[1], (float)obj->aabbMax[2]);
        AabbF objAabb = {
            .min = (T3DVec3){{(float)obj->aabbMin[0], (float)obj->aabbMin[1], (float)obj->aabbMin[2]}},
            .max = (T3DVec3){{(float)obj->aabbMax[0], (float)obj->aabbMax[1], (float)obj->aabbMax[2]}},
        };

        if (!aabbf_overlaps(&entity->aabb, &objAabb))
            continue;

        for (uint32_t p = 0; p < obj->numParts; p++)
        {
            const T3DObjectPart *part = &obj->parts[p];
            const T3DVertPacked *partVerts = part->vert;

            /*if (debug_this)
                debugf("[collision] part=%u vertDest=%u vertLoad=%u idx=%u idxSeq=%u strip0=%u\n",
                       p, part->vertDestOffset, part->vertLoadCount, part->numIndices,
                       part->idxSeqCount, part->numStripIndices[0]);*/

            for (uint32_t i = 0; i + 2 < part->numIndices; i += 3)
            {
                int idx0 = part->indices[i];
                int idx1 = part->indices[i + 1];
                int idx2 = part->indices[i + 2];
                int l0, l1, l2;
                if (!map_part_index(part, idx0, &l0) ||
                    !map_part_index(part, idx1, &l1) ||
                    !map_part_index(part, idx2, &l2))
                {
                    if (debug_this)
                        debugf("[collision] idx map fail: %d %d %d (dest=%u load=%u)\n",
                               idx0, idx1, idx2, part->vertDestOffset, part->vertLoadCount);
                    continue;
                }

                T3DVec3 v0 = vert_pos(partVerts, l0);
                T3DVec3 v1 = vert_pos(partVerts, l1);
                T3DVec3 v2 = vert_pos(partVerts, l2);

                if (resolve_capsule_triangle(entity, radius, &segA, &segB, &v0, &v1, &v2, onGround))
                    collided = true;
            }

            if (part->idxSeqCount != 0)
            {
                for (uint32_t t = 0; t < part->idxSeqCount; t++)
                {
                    int base = part->idxSeqBase + (int)(t * 3);
                    int idx0 = base;
                    int idx1 = base + 1;
                    int idx2 = base + 2;
                    int l0, l1, l2;
                    if (!map_part_index(part, idx0, &l0) ||
                        !map_part_index(part, idx1, &l1) ||
                        !map_part_index(part, idx2, &l2))
                    {
                        if (debug_this)
                            debugf("[collision] seq map fail: %d %d %d (dest=%u load=%u)\n",
                                   idx0, idx1, idx2, part->vertDestOffset, part->vertLoadCount);
                        continue;
                    }

                    T3DVec3 v0 = vert_pos(partVerts, l0);
                    T3DVec3 v1 = vert_pos(partVerts, l1);
                    T3DVec3 v2 = vert_pos(partVerts, l2);

                    if (resolve_capsule_triangle(entity, radius, &segA, &segB, &v0, &v1, &v2, onGround))
                        collided = true;
                }
            }

            if (part->numStripIndices[0] != 0)
            {
                uint8_t *idxPtrBase = (uint8_t *)align_pointer(part->indices + part->numIndices, 8);
                for (int s = 0; s < 4; s++)
                {
                    int count = part->numStripIndices[s];
                    if (count <= 0)
                        break;

                    int16_t *idx = (int16_t *)idxPtrBase;
                    int s0 = -1;
                    int s1 = -1;
                    bool flip = false;
                    for (int i = 0; i < count; i++)
                    {
                        int16_t val = idx[i];
                        bool restart = (val & 0x8000) != 0;
                        int v = decode_strip_index(val);
                        if (restart)
                        {
                            s0 = -1;
                            s1 = -1;
                            flip = false;
                        }

                        if (s0 < 0)
                        {
                            s0 = v;
                            continue;
                        }
                        if (s1 < 0)
                        {
                            s1 = v;
                            continue;
                        }

                        int i0 = flip ? s1 : s0;
                        int i1 = flip ? s0 : s1;
                        int i2 = v;
                        flip = !flip;

                        s0 = s1;
                        s1 = v;

                        int l0, l1, l2;
                        if (!map_part_index(part, i0, &l0) ||
                            !map_part_index(part, i1, &l1) ||
                            !map_part_index(part, i2, &l2))
                        {
                            if (debug_this)
                                debugf("[collision] strip map fail: %d %d %d (dest=%u load=%u)\n",
                                       i0, i1, i2, part->vertDestOffset, part->vertLoadCount);
                            continue;
                        }

                        T3DVec3 v0 = vert_pos(partVerts, l0);
                        T3DVec3 v1 = vert_pos(partVerts, l1);
                        T3DVec3 v2 = vert_pos(partVerts, l2);

                        if (resolve_capsule_triangle(entity, radius, &segA, &segB, &v0, &v1, &v2, onGround))
                            collided = true;
                    }

                    idxPtrBase = (uint8_t *)align_pointer(idxPtrBase + count * sizeof(int16_t), 8);
                }
            }
        }
    }

    return collided;
}
