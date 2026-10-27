// Controller pictures for the device in use: an Xbox Series or a DualShock 4 style controller, front view, flat
// colours, built from the shapes of runtime/nvn/shapes.h. Controller units: x across (-0.5 .. 0.5), y down from the
// top edge, both in controller widths; the proportions are measured on the real controllers. No maker's logos: the
// guide and PS buttons are plain. tools/padart_preview.c renders the pictures to PNG.
#include <stdlib.h>

#include "padart.h"
#include "shapes.h"

typedef struct { float r, g, b, a; } Rgba;
// Premultiplied colour, w x h pixels; a controller unit is s pixels, and unit (0, 0) is at pixel (ox, oy)
typedef struct { float *px; int w, h; float s, ox, oy; } Pic;

static Rgba hex(uint32_t c) { return (Rgba){(c >> 16 & 255) / 255.f, (c >> 8 & 255) / 255.f, (c & 255) / 255.f, 1}; }

static void blend(Pic *p, int x, int y, float a, Rgba c) {
    float *q = p->px + 4 * ((size_t)y * p->w + x);
    a *= c.a;
    q[0] = c.r * a + q[0] * (1 - a), q[1] = c.g * a + q[1] * (1 - a), q[2] = c.b * a + q[2] * (1 - a);
    q[3] = a + q[3] * (1 - a);
}

// Colour c over shape s, left of x = xmin cut away (the bowls of a B)
static void paint_clip(Pic *p, Shape s, float xmin, Rgba c) {
    float e = s.r + s.ring, x0, y0, x1, y1;  // its bounds, in units
    switch (s.t) {
    case SH_BOX: x0 = s.p[0] - s.p[2], x1 = s.p[0] + s.p[2], y0 = s.p[1] - s.p[3], y1 = s.p[1] + s.p[3], e = s.ring; break;
    case SH_TRI:
        x0 = minf(minf(s.p[0], s.p[2]), s.p[4]), x1 = maxf(maxf(s.p[0], s.p[2]), s.p[4]);
        y0 = minf(minf(s.p[1], s.p[3]), s.p[5]), y1 = maxf(maxf(s.p[1], s.p[3]), s.p[5]);
        break;
    case SH_SEG: x0 = minf(s.p[0], s.p[2]), x1 = maxf(s.p[0], s.p[2]), y0 = minf(s.p[1], s.p[3]), y1 = maxf(s.p[1], s.p[3]); break;
    default: x0 = x1 = s.p[0], y0 = y1 = s.p[1]; break;
    }
    int px0 = (int)maxf(0, (x0 - e) * p->s + p->ox - 1), px1 = (int)minf(p->w, (x1 + e) * p->s + p->ox + 2);
    int py0 = (int)maxf(0, (y0 - e) * p->s + p->oy - 1), py1 = (int)minf(p->h, (y1 + e) * p->s + p->oy + 2);
    for (int y = py0; y < py1; y++)
        for (int x = px0; x < px1; x++) {
            float ux = (x + 0.5f - p->ox) / p->s, uy = (y + 0.5f - p->oy) / p->s;
            float a = cover(maxf(sd(&s, ux, uy), xmin - ux) * p->s);
            if (a > 0) blend(p, x, y, a, c);
        }
}
static void paint(Pic *p, Shape s, Rgba c) { paint_clip(p, s, -1e9f, c); }

// A smooth union: the body's parts flow into each other
static float smin(float a, float b, float k) {
    float h = clampf(0.5f + 0.5f * (b - a) / k, 0, 1);
    return b + (a - b) * h - k * h * (1 - h);
}

// A capsule tapering from radius ra at (ax, ay) to rb at (bx, by): a grip (the round cone of Inigo Quilez's distance
// functions, in 2D)
static float sd_taper(float px, float py, float ax, float ay, float bx, float by, float ra, float rb) {
    float bax = bx - ax, bay = by - ay, l2 = bax * bax + bay * bay, rr = ra - rb, a2 = l2 - rr * rr;
    float pax = px - ax, pay = py - ay, y = pax * bax + pay * bay, z = y - l2;
    float qx = pax * l2 - bax * y, qy = pay * l2 - bay * y, x2 = qx * qx + qy * qy, y2 = y * y * l2, z2 = z * z * l2;
    float k = (rr > 0 ? 1 : -1) * rr * rr * x2;
    if ((z > 0 ? 1 : -1) * a2 * z2 > k) return sqrtf(x2 + z2) / l2 - rb;
    if ((y > 0 ? 1 : -1) * a2 * y2 < k) return sqrtf(x2 + y2) / l2 - ra;
    return (sqrtf(x2 * a2 / l2) + y * rr) / l2 - ra;
}

// Colour c over the controller's body (body: its distance at a point), shrunk by inset units
static void paint_body(Pic *p, float (*body)(float, float), float inset, Rgba c) {
    for (int y = 0; y < p->h; y++)
        for (int x = 0; x < p->w; x++) {
            float a = cover((body((x + 0.5f - p->ox) / p->s, (y + 0.5f - p->oy) / p->s) + inset) * p->s);
            if (a > 0) blend(p, x, y, a, c);
        }
}

// A, B, X or Y in strokes t thick, capital height ht, centred on (cx, cy)
static void letter(Pic *p, char ch, float cx, float cy, float ht, float t, Rgba c) {
    float w = ht * 0.78f, l = cx - w / 2, r = cx + w / 2, top = cy - ht / 2, bot = cy + ht / 2, h = t / 2;
    switch (ch) {
    case 'A':
        paint(p, seg(l, bot, cx, top, h), c), paint(p, seg(cx, top, r, bot, h), c);
        paint(p, seg(cx - w * 0.27f, cy + ht * 0.17f, cx + w * 0.27f, cy + ht * 0.17f, h), c);
        break;
    case 'X': paint(p, seg(l, top, r, bot, h), c), paint(p, seg(r, top, l, bot, h), c); break;
    case 'Y': paint(p, seg(l, top, cx, cy, h), c), paint(p, seg(r, top, cx, cy, h), c), paint(p, seg(cx, cy, cx, bot, h), c); break;
    case 'B': {
        float sx = cx - ht * 0.33f, bx = cx + ht * 0.08f;  // the stem, where the bowls start
        paint(p, seg(sx, top, sx, bot, h), c);
        for (int i = 0; i < 3; i++) paint(p, seg(sx, top + i * ht / 2, bx, top + i * ht / 2, h), c);
        paint_clip(p, ring(circle(bx, cy - ht / 4, ht / 4), h), bx, c);
        paint_clip(p, ring(circle(bx, cy + ht / 4, ht / 4), h), bx, c);
        break;
    }
    }
}

// ---- Xbox Series controller (robot white)
static float body_xbox(float x, float y) {
    float ax = fabsf(x);
    Shape top = box(0, 0.255f, 0.445f, 0.235f, 0.19f);
    return smin(sd(&top, ax, y), sd_taper(ax, y, 0.355f, 0.43f, 0.39f, 0.615f, 0.145f, 0.09f), 0.08f);
}

static void draw_xbox(Pic *p) {
    Rgba shell = hex(0xf2f3f5), edge = hex(0xb7bbc3), raised = hex(0xf9fafb), dark = hex(0x1d1e20), icon = hex(0x5b5e65);
    for (int s = -1; s <= 1; s += 2) paint(p, box(s * 0.258f, 0.04f, 0.095f, 0.04f, 0.035f), hex(0xd3d6dc));  // bumpers
    paint_body(p, body_xbox, 0, edge);
    paint_body(p, body_xbox, 0.005f, shell);
    static const float sticks[2][2] = {{-0.259f, 0.191f}, {0.132f, 0.343f}};
    for (int i = 0; i < 2; i++) {
        float x = sticks[i][0], y = sticks[i][1];
        paint(p, circle(x, y, 0.075f), edge), paint(p, circle(x, y, 0.071f), raised);  // the raised ring around it
        paint(p, circle(x, y, 0.060f), dark), paint(p, circle(x, y, 0.046f), hex(0x3b3c41));
        paint(p, ring(circle(x, y, 0.053f), 0.0015f), hex(0x2c2d31));  // the cap's grip
    }
    paint(p, circle(-0.132f, 0.356f, 0.080f), hex(0x18191b));  // d-pad
    paint(p, box(-0.132f, 0.356f, 0.068f, 0.025f, 0.006f), hex(0x3a3b40));
    paint(p, box(-0.132f, 0.356f, 0.025f, 0.068f, 0.006f), hex(0x3a3b40));
    for (int s = -1; s <= 1; s += 2) paint(p, circle(s * 0.0735f, 0.19f, 0.0235f), edge), paint(p, circle(s * 0.0735f, 0.19f, 0.0205f), raised);
    paint(p, ring(box(-0.0775f, 0.1865f, 0.0065f, 0.005f, 0.0015f), 0.0012f), icon);  // View: two windows
    paint(p, ring(box(-0.0695f, 0.1935f, 0.0065f, 0.005f, 0.0015f), 0.0012f), icon);
    for (int i = -1; i <= 1; i++) paint(p, seg(0.0655f, 0.19f + i * 0.0045f, 0.0815f, 0.19f + i * 0.0045f, 0.0011f), icon);  // Menu
    paint(p, box(0, 0.246f, 0.031f, 0.0155f, 0.0155f), edge), paint(p, box(0, 0.246f, 0.028f, 0.0125f, 0.0125f), raised);  // Share
    paint(p, seg(0, 0.2505f, 0, 0.2385f, 0.0011f), icon), paint(p, seg(-0.004f, 0.2425f, 0, 0.2385f, 0.0011f), icon);
    paint(p, seg(0.004f, 0.2425f, 0, 0.2385f, 0.0011f), icon);
    paint(p, seg(-0.010f, 0.247f, -0.010f, 0.252f, 0.0011f), icon), paint(p, seg(-0.010f, 0.252f, 0.010f, 0.252f, 0.0011f), icon);
    paint(p, seg(0.010f, 0.252f, 0.010f, 0.247f, 0.0011f), icon);
    paint(p, circle(0, 0.088f, 0.041f), edge), paint(p, circle(0, 0.088f, 0.038f), dark);  // guide button, plain
    static const struct { float x, y; char ch; uint32_t c; } face[4] = {
        {0.258f, 0.126f, 'Y', 0xf2d013}, {0.190f, 0.191f, 'X', 0x2f9be8}, {0.327f, 0.196f, 'B', 0xe2433f}, {0.259f, 0.261f, 'A', 0x58c234}};
    for (int i = 0; i < 4; i++) {
        paint(p, circle(face[i].x, face[i].y, 0.0355f), edge), paint(p, circle(face[i].x, face[i].y, 0.033f), hex(0x121314));
        letter(p, face[i].ch, face[i].x, face[i].y, 0.03f, 0.0075f, hex(face[i].c));
    }
}

// ---- DualShock 4 style controller (black)
static float body_ps(float x, float y) {
    float ax = fabsf(x);
    Shape top = box(0, 0.17f, 0.465f, 0.155f, 0.12f), well = circle(0.162f, 0.307f, 0.105f);
    Shape mid = box(0, 0.33f, 0.24f, 0.075f, 0.06f);  // between the grips: a flat edge under the sticks
    float d = smin(sd(&top, ax, y), sd_taper(ax, y, 0.37f, 0.36f, 0.37f, 0.52f, 0.13f, 0.09f), 0.06f);
    return smin(smin(d, sd(&well, ax, y), 0.05f), sd(&mid, ax, y), 0.04f);
}

static void draw_ps(Pic *p) {
    Rgba shell = hex(0x1f2024), rim = hex(0x50535b), well = hex(0x17181b), key = hex(0x34363d), mark = hex(0x6b6f78);
    for (int s = -1; s <= 1; s += 2) paint(p, box(s * 0.33f, 0.014f, 0.08f, 0.022f, 0.02f), hex(0x9a9ea6));  // L1 / R1
    paint_body(p, body_ps, 0, rim);
    paint_body(p, body_ps, 0.005f, shell);
    paint(p, box(0, 0.098f, 0.168f, 0.095f, 0.02f), rim), paint(p, box(0, 0.098f, 0.164f, 0.091f, 0.017f), hex(0x232529));  // touchpad
    paint(p, box(0, 0.026f, 0.12f, 0.003f, 0.003f), hex(0x3d7cf2));  // its light bar
    float dx = -0.316f, dy = 0.159f, fx = 0.319f, fy = 0.159f;
    paint(p, circle(dx, dy, 0.113f), well), paint(p, circle(fx, fy, 0.113f), well);
    paint(p, box(dx, dy - 0.058f, 0.027f, 0.034f, 0.007f), key), paint(p, box(dx, dy + 0.058f, 0.027f, 0.034f, 0.007f), key);  // d-pad
    paint(p, box(dx - 0.058f, dy, 0.034f, 0.027f, 0.007f), key), paint(p, box(dx + 0.058f, dy, 0.034f, 0.027f, 0.007f), key);
    paint(p, tri(dx, dy - 0.106f, dx - 0.007f, dy - 0.098f, dx + 0.007f, dy - 0.098f), mark);  // the arrows around it
    paint(p, tri(dx, dy + 0.106f, dx - 0.007f, dy + 0.098f, dx + 0.007f, dy + 0.098f), mark);
    paint(p, tri(dx - 0.106f, dy, dx - 0.098f, dy - 0.007f, dx - 0.098f, dy + 0.007f), mark);
    paint(p, tri(dx + 0.106f, dy, dx + 0.098f, dy - 0.007f, dx + 0.098f, dy + 0.007f), mark);
    float t = 0.0026f;
    paint(p, circle(fx, fy - 0.076f, 0.036f), key);  // triangle
    paint(p, ring(tri(fx, fy - 0.095f, fx - 0.0185f, fy - 0.064f, fx + 0.0185f, fy - 0.064f), t), hex(0x41c3a6));
    paint(p, circle(fx + 0.076f, fy, 0.036f), key), paint(p, ring(circle(fx + 0.076f, fy, 0.0165f), t), hex(0xe0566a));  // circle
    paint(p, circle(fx, fy + 0.076f, 0.036f), key);  // cross
    paint(p, seg(fx - 0.014f, fy + 0.062f, fx + 0.014f, fy + 0.09f, t), hex(0x8eb1ec));
    paint(p, seg(fx + 0.014f, fy + 0.062f, fx - 0.014f, fy + 0.09f, t), hex(0x8eb1ec));
    paint(p, circle(fx - 0.076f, fy, 0.036f), key);  // square
    paint(p, ring(box(fx - 0.076f, fy, 0.0135f, 0.0135f, 0.0015f), t), hex(0xdb8fcf));
    for (int s = -1; s <= 1; s += 2) {
        float x = s * 0.162f, y = 0.307f;
        paint(p, circle(x, y, 0.090f), well), paint(p, ring(circle(x, y, 0.090f), 0.002f), hex(0x2c2e33));  // stick
        paint(p, circle(x, y, 0.064f), hex(0x2a2c31)), paint(p, circle(x, y, 0.048f), hex(0x35383e));
        paint(p, ring(circle(x, y, 0.057f), 0.0015f), hex(0x1d1e22));
        paint(p, box(s * 0.209f, 0.057f, 0.0115f, 0.025f, 0.0115f), key);  // Share / Options
    }
    for (int i = 0; i < 9; i++)  // speaker
        paint(p, circle((i < 5 ? i - 2 : i - 6.5f) * 0.012f, i < 5 ? 0.224f : 0.236f, 0.0035f), hex(0x0e0f11));
    paint(p, circle(0, 0.297f, 0.024f), rim), paint(p, circle(0, 0.297f, 0.021f), key);  // PS button, plain
}

void padart_draw(int kind, uint8_t *rgba, uint32_t w, uint32_t h) {
    float height = kind == PADART_PLAYSTATION ? 0.61f : 0.705f;  // in controller widths
    Pic p = {.px = calloc((size_t)w * h * 4, sizeof(float)), .w = (int)w, .h = (int)h};
    if (!p.px) return;
    p.s = minf(w * 0.96f, h * 0.96f / height), p.ox = w / 2.0f, p.oy = (h - height * p.s) / 2;
    if (kind == PADART_PLAYSTATION) draw_ps(&p);
    else draw_xbox(&p);
    for (size_t i = 0; i < (size_t)w * h; i++) {
        float a = p.px[4 * i + 3];
        for (int k = 0; k < 3; k++) rgba[4 * i + k] = (uint8_t)lrintf(a > 0 ? clampf(p.px[4 * i + k] / a, 0, 1) * 255 : 0);
        rgba[4 * i + 3] = (uint8_t)lrintf(a * 255);
    }
    free(p.px);
}
