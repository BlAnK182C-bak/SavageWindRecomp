// Shapes as signed distances (negative inside, in the caller's units) for what Savage Wind draws itself: the button
// icons (runtime/nvn/glyphs.c) and the controller pictures (runtime/nvn/padart.c).
#pragma once
#include <math.h>
#include <stdbool.h>

typedef enum { SH_CIRCLE, SH_BOX, SH_SEG, SH_TRI } ShapeType;
typedef struct { ShapeType t; float p[6], r, ring; } Shape;  // ring > 0: only a band that thick on its edge

static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static inline float sd_seg(float px, float py, float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay, h = clampf(((px - ax) * dx + (py - ay) * dy) / maxf(dx * dx + dy * dy, 1e-6f), 0, 1);
    return hypotf(px - ax - dx * h, py - ay - dy * h);
}

static inline float sd(const Shape *s, float x, float y) {
    float d = 0;
    switch (s->t) {
    case SH_CIRCLE: d = hypotf(x - s->p[0], y - s->p[1]) - s->r; break;
    case SH_BOX: {  // centre, half sizes, corner radius r
        float qx = fabsf(x - s->p[0]) - s->p[2] + s->r, qy = fabsf(y - s->p[1]) - s->p[3] + s->r;
        d = hypotf(maxf(qx, 0), maxf(qy, 0)) + minf(maxf(qx, qy), 0) - s->r;
        break;
    }
    case SH_SEG: d = sd_seg(x, y, s->p[0], s->p[1], s->p[2], s->p[3]) - s->r; break;
    case SH_TRI: {  // three corners (any order)
        float e = minf(minf(sd_seg(x, y, s->p[0], s->p[1], s->p[2], s->p[3]), sd_seg(x, y, s->p[2], s->p[3], s->p[4], s->p[5])),
                       sd_seg(x, y, s->p[4], s->p[5], s->p[0], s->p[1]));
        float c1 = (s->p[2] - s->p[0]) * (y - s->p[1]) - (s->p[3] - s->p[1]) * (x - s->p[0]);
        float c2 = (s->p[4] - s->p[2]) * (y - s->p[3]) - (s->p[5] - s->p[3]) * (x - s->p[2]);
        float c3 = (s->p[0] - s->p[4]) * (y - s->p[5]) - (s->p[1] - s->p[5]) * (x - s->p[4]);
        bool in = (c1 >= 0 && c2 >= 0 && c3 >= 0) || (c1 <= 0 && c2 <= 0 && c3 <= 0);
        d = (in ? -e : e) - s->r;
        break;
    }
    }
    return s->ring > 0 ? fabsf(d) - s->ring : d;
}

static inline float cover(float d) { return clampf(0.5f - d, 0, 1); }  // pixel coverage of a distance in pixels
static inline Shape circle(float x, float y, float r) { return (Shape){SH_CIRCLE, {x, y}, r, 0}; }
static inline Shape box(float x, float y, float hw, float hh, float r) { return (Shape){SH_BOX, {x, y, hw, hh}, r, 0}; }
static inline Shape seg(float ax, float ay, float bx, float by, float r) { return (Shape){SH_SEG, {ax, ay, bx, by}, r, 0}; }
static inline Shape tri(float ax, float ay, float bx, float by, float cx, float cy) { return (Shape){SH_TRI, {ax, ay, bx, by, cx, cy}, 0, 0}; }
static inline Shape ring(Shape s, float t) { s.ring = t; return s; }
