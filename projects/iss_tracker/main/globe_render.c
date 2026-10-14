// SPDX-License-Identifier: Apache-2.0

#include "globe_render.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "world_map.h"

#define DEG2RAD            ((float)M_PI / 180.0f)
#define ORBIT_SCALE        1.13f   /* drawn orbit radius / globe radius (real ISS ~1.065) */
#define FAR_SIDE_ALPHA     0.35f   /* orbit seen past the limb, behind the globe */
#define FOOTPRINT_DEG      20.0f   /* ground area that can see the ISS */
#define MAX_TRACK_POINTS   512
#define GRID_STEP_DEG      15
#define GRID_SAMPLE_DEG    3
#define GRID_MERIDIANS     (360 / GRID_STEP_DEG)
#define GRID_MERIDIAN_PTS  (180 / GRID_SAMPLE_DEG + 1)
#define GRID_PARALLELS     (180 / GRID_STEP_DEG - 1)
#define GRID_PARALLEL_PTS  (360 / GRID_SAMPLE_DEG + 1)
#define GRID_POINTS        (GRID_MERIDIANS * GRID_MERIDIAN_PTS + GRID_PARALLELS * GRID_PARALLEL_PTS)
#define GRID_LINES         (GRID_MERIDIANS + GRID_PARALLELS)
#define FONT_W             5
#define FONT_H             7
#define FONT_SCALE         2
#define ALPHA_ONE          32      /* blend alpha scale (5-bit, see blend565) */
#define CACHE_VIEW_DEG     0.1f    /* re-render the surface layer after this much view motion */
#define CACHE_SUN_DEG      0.05f
#define FOOTPRINT_EXTENT   0.35f   /* 2 sin(FOOTPRINT_DEG / 2): screen radius bound of the cap, in R */

typedef struct { uint8_t r, g, b; } rgb_t;
typedef struct { float x, y, z; } vec3_t;
typedef struct { float x, y; float alpha; bool vis; } screen_pt_t;
typedef struct { float on, off, pos; } dash_t;

/* Palette matched to the browser mockup */
static const rgb_t SKY_TOP      = {248, 250, 253};
static const rgb_t SKY_BOTTOM   = {226, 232, 240};
static const rgb_t SHADOW       = {51, 65, 85};
static const rgb_t DROP_SHADOW  = {71, 85, 105};
static const rgb_t SPHERE_HI    = {255, 255, 255};
static const rgb_t SPHERE_MID   = {241, 244, 248};
static const rgb_t SPHERE_EDGE  = {211, 219, 230};
static const rgb_t RIM          = {203, 213, 225};
static const rgb_t GRID         = {148, 163, 184};
static const rgb_t COAST        = {143, 161, 186};
static const rgb_t BORDER       = {160, 176, 198};
static const rgb_t NIGHT        = {51, 65, 90};
static const rgb_t ACCENT       = {59, 123, 242};
static const rgb_t ACCENT_DARK  = {30, 58, 110};
static const rgb_t GROUND_SHADE = {30, 41, 59};
static const rgb_t WHITE        = {255, 255, 255};
static const rgb_t TICK_LABEL   = {91, 107, 128};

static const uint8_t BAYER4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

struct globe_render {
    globe_render_config_t cfg;
    uint16_t *base;                 /* static background + shaded sphere */
    uint16_t *surface;              /* base + map + night side for the cached view */
    bool surface_valid;
    float surface_lon, surface_lat, surface_sun_lon, surface_sun_lat;
    vec3_t *coast;
    vec3_t *borders;
    vec3_t *grid;
    uint32_t grid_starts[GRID_LINES + 1];
    screen_pt_t *track;             /* scratch for one projected track */
    /* per-frame state */
    float m[3][3];                  /* Earth-fixed -> view (x right, y up, z toward viewer) */
    uint16_t *dst;
    size_t stride;
};

/* ---------- pixels ---------- */

static inline uint16_t pack(float r, float g, float b, int x, int y)
{
    const int d = BAYER4[(y & 3) * 4 + (x & 3)] * 16 + 8;
    int r5 = ((int)r * 31 + d) / 255, g6 = ((int)g * 63 + d) / 255, b5 = ((int)b * 31 + d) / 255;
    r5 = r5 > 31 ? 31 : r5;
    g6 = g6 > 63 ? 63 : g6;
    b5 = b5 > 31 ? 31 : b5;
    return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}

static inline uint16_t rgb565(rgb_t c)
{
    return (uint16_t)(((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3));
}

/* Integer RGB565 blend: spread the channels with gaps so one multiply does all three. */
static inline uint16_t blend565(uint16_t dst, uint16_t src, uint32_t a32)
{
    const uint32_t d = (dst | ((uint32_t)dst << 16)) & 0x07E0F81Fu;
    const uint32_t s = (src | ((uint32_t)src << 16)) & 0x07E0F81Fu;
    const uint32_t r = (d + (((s - d) * a32) >> 5)) & 0x07E0F81Fu;
    return (uint16_t)(r | (r >> 16));
}

static inline void blend_px(globe_render_handle_t gr, int x, int y, uint16_t c, uint32_t a32)
{
    if ((unsigned)x >= gr->cfg.width || (unsigned)y >= gr->cfg.height || a32 == 0) {
        return;
    }
    uint16_t *p = &gr->dst[(size_t)y * gr->stride + (size_t)x];
    *p = a32 >= ALPHA_ONE ? c : blend565(*p, c, a32);
}

static inline void blend(globe_render_handle_t gr, int x, int y, rgb_t c, float a)
{
    if (a <= 0.0f) {
        return;
    }
    const int a32 = (int)(a * ALPHA_ONE + 0.5f);
    blend_px(gr, x, y, rgb565(c), (uint32_t)(a32 > ALPHA_ONE ? ALPHA_ONE : a32));
}

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float mixf(float a, float b, float t)
{
    return a + (b - a) * t;
}

/* ---------- primitives ---------- */

/* Anti-aliased segment by distance; no start cap so joints are not drawn twice. */
static void segment(globe_render_handle_t gr, float x0, float y0, float x1, float y1,
                    float width, rgb_t c, float a, bool start_cap)
{
    const float hw = width * 0.5f;
    const int minx = (int)floorf(fminf(x0, x1) - hw - 1.0f), maxx = (int)ceilf(fmaxf(x0, x1) + hw + 1.0f);
    const int miny = (int)floorf(fminf(y0, y1) - hw - 1.0f), maxy = (int)ceilf(fmaxf(y0, y1) + hw + 1.0f);
    const float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    for (int py = miny < 0 ? 0 : miny; py <= maxy && py < gr->cfg.height; ++py) {
        for (int px = minx < 0 ? 0 : minx; px <= maxx && px < gr->cfg.width; ++px) {
            const float fx = px + 0.5f, fy = py + 0.5f;
            float t = len2 > 0.0f ? ((fx - x0) * dx + (fy - y0) * dy) / len2 : 0.0f;
            if (t < 0.0f) {
                if (!start_cap) {
                    continue;
                }
                t = 0.0f;
            }
            t = t > 1.0f ? 1.0f : t;
            const float ex = x0 + t * dx - fx, ey = y0 + t * dy - fy;
            const float cov = hw + 0.5f - sqrtf(ex * ex + ey * ey);
            if (cov > 0.0f) {
                blend(gr, px, py, c, a * (cov > 1.0f ? 1.0f : cov));
            }
        }
    }
}

/* Segment split into dash pieces; @p dash may be NULL for a solid line. */
static void dashed_segment(globe_render_handle_t gr, float x0, float y0, float x1, float y1,
                           float width, rgb_t c, float a, dash_t *dash, bool start_cap)
{
    if (!dash) {
        segment(gr, x0, y0, x1, y1, width, c, a, start_cap);
        return;
    }
    const float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    const float period = dash->on + dash->off;
    float done = 0.0f;
    while (done < len) {
        const float phase = fmodf(dash->pos, period);
        const bool on = phase < dash->on;
        float piece = (on ? dash->on - phase : period - phase);
        piece = fminf(piece, len - done);
        if (on && piece > 0.0f) {
            const float t0 = done / len, t1 = (done + piece) / len;
            segment(gr, mixf(x0, x1, t0), mixf(y0, y1, t0), mixf(x0, x1, t1), mixf(y0, y1, t1),
                    width, c, a, true);
        }
        done += piece;
        dash->pos += piece;
    }
}

static void disc(globe_render_handle_t gr, float cx, float cy, float r, rgb_t c, float a)
{
    for (int py = (int)(cy - r - 1); py <= (int)(cy + r + 1); ++py) {
        for (int px = (int)(cx - r - 1); px <= (int)(cx + r + 1); ++px) {
            const float dx = px + 0.5f - cx, dy = py + 0.5f - cy;
            const float cov = r + 0.5f - sqrtf(dx * dx + dy * dy);
            if (cov > 0.0f) {
                blend(gr, px, py, c, a * (cov > 1.0f ? 1.0f : cov));
            }
        }
    }
}

static void ring(globe_render_handle_t gr, float cx, float cy, float r, float width, rgb_t c, float a)
{
    const float ext = r + width + 1.0f;
    for (int py = (int)(cy - ext); py <= (int)(cy + ext); ++py) {
        for (int px = (int)(cx - ext); px <= (int)(cx + ext); ++px) {
            const float dx = px + 0.5f - cx, dy = py + 0.5f - cy;
            const float cov = width * 0.5f + 0.5f - fabsf(sqrtf(dx * dx + dy * dy) - r);
            if (cov > 0.0f) {
                blend(gr, px, py, c, a * (cov > 1.0f ? 1.0f : cov));
            }
        }
    }
}

/* Convex quad with 2x2 supersampling; corners in drawing order. */
static void quad(globe_render_handle_t gr, const float q[4][2], rgb_t c, float a)
{
    float minx = q[0][0], maxx = q[0][0], miny = q[0][1], maxy = q[0][1];
    for (int i = 1; i < 4; ++i) {
        minx = fminf(minx, q[i][0]);
        maxx = fmaxf(maxx, q[i][0]);
        miny = fminf(miny, q[i][1]);
        maxy = fmaxf(maxy, q[i][1]);
    }
    float area = 0.0f;
    for (int i = 0; i < 4; ++i) {
        const int j = (i + 1) & 3;
        area += q[i][0] * q[j][1] - q[j][0] * q[i][1];
    }
    const float sign = area < 0.0f ? -1.0f : 1.0f;
    static const float SUB[4][2] = {{0.25f, 0.25f}, {0.75f, 0.25f}, {0.25f, 0.75f}, {0.75f, 0.75f}};
    for (int py = (int)floorf(miny); py <= (int)ceilf(maxy); ++py) {
        for (int px = (int)floorf(minx); px <= (int)ceilf(maxx); ++px) {
            int hits = 0;
            for (int s = 0; s < 4; ++s) {
                const float sx = px + SUB[s][0], sy = py + SUB[s][1];
                bool inside = true;
                for (int i = 0; i < 4 && inside; ++i) {
                    const int j = (i + 1) & 3;
                    const float e = (q[j][0] - q[i][0]) * (sy - q[i][1]) - (q[j][1] - q[i][1]) * (sx - q[i][0]);
                    inside = e * sign >= 0.0f;
                }
                hits += inside;
            }
            if (hits) {
                blend(gr, px, py, c, a * hits * 0.25f);
            }
        }
    }
}

/* Xiaolin Wu anti-aliased hairline: two pixels per step, no square roots. */
static void wu_line(globe_render_handle_t gr, float x0, float y0, float x1, float y1, uint16_t c, float a)
{
    const bool steep = fabsf(y1 - y0) > fabsf(x1 - x0);
    if (steep) {
        float t = x0; x0 = y0; y0 = t;
        t = x1; x1 = y1; y1 = t;
    }
    if (x0 > x1) {
        float t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }
    const float dx = x1 - x0;
    const float grad = dx > 0.0f ? (y1 - y0) / dx : 0.0f;
    const float amax = a * ALPHA_ONE;
    int xs = (int)floorf(x0 + 0.5f), xe = (int)floorf(x1 + 0.5f);
    float y = y0 + grad * ((float)xs + 0.5f - x0) - 0.5f;
    for (int x = xs; x < xe; ++x, y += grad) {
        const int yi = (int)floorf(y);
        const float f = y - (float)yi;
        const uint32_t a_lo = (uint32_t)(amax * (1.0f - f) + 0.5f), a_hi = (uint32_t)(amax * f + 0.5f);
        if (steep) {
            blend_px(gr, yi, x, c, a_lo);
            blend_px(gr, yi + 1, x, c, a_hi);
        } else {
            blend_px(gr, x, yi, c, a_lo);
            blend_px(gr, x, yi + 1, c, a_hi);
        }
    }
}

static const uint8_t *glyph(char ch)
{
    static const uint8_t DIGITS[10][FONT_H] = {
        {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
        {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
        {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    };
    static const uint8_t PLUS[FONT_H] = {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
    static const uint8_t M[FONT_H] = {0x00, 0x00, 0x1A, 0x15, 0x15, 0x11, 0x11};
    static const uint8_t I[FONT_H] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t S[FONT_H] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    if (ch >= '0' && ch <= '9') {
        return DIGITS[ch - '0'];
    }
    switch (ch) {
    case '+': return PLUS;
    case 'm': return M;
    case 'I': return I;
    case 'S': return S;
    default: return NULL;
    }
}

static void text(globe_render_handle_t gr, int x, int y, const char *s, rgb_t c, float a)
{
    for (; *s; ++s, x += (FONT_W + 1) * FONT_SCALE) {
        const uint8_t *g = glyph(*s);
        for (int row = 0; g && row < FONT_H; ++row) {
            for (int col = 0; col < FONT_W; ++col) {
                if (!(g[row] & (0x10 >> col))) {
                    continue;
                }
                for (int k = 0; k < FONT_SCALE * FONT_SCALE; ++k) {
                    blend(gr, x + col * FONT_SCALE + k % FONT_SCALE, y + row * FONT_SCALE + k / FONT_SCALE, c, a);
                }
            }
        }
    }
}

/* ---------- projection ---------- */

static vec3_t ecef(float lon_deg, float lat_deg)
{
    const float lon = lon_deg * DEG2RAD, lat = lat_deg * DEG2RAD;
    return (vec3_t){cosf(lat) * cosf(lon), cosf(lat) * sinf(lon), sinf(lat)};
}

static void set_view(globe_render_handle_t gr, float lon_deg, float lat_deg)
{
    const float lon = lon_deg * DEG2RAD, lat = lat_deg * DEG2RAD;
    const float so = sinf(lon), co = cosf(lon), sa = sinf(lat), ca = cosf(lat);
    const float m[3][3] = {{-so, co, 0.0f}, {-sa * co, -sa * so, ca}, {ca * co, ca * so, sa}};
    memcpy(gr->m, m, sizeof(m));
}

static vec3_t to_view(globe_render_handle_t gr, vec3_t p)
{
    return (vec3_t){
        gr->m[0][0] * p.x + gr->m[0][1] * p.y + gr->m[0][2] * p.z,
        gr->m[1][0] * p.x + gr->m[1][1] * p.y + gr->m[1][2] * p.z,
        gr->m[2][0] * p.x + gr->m[2][1] * p.y + gr->m[2][2] * p.z,
    };
}

static void to_screen(globe_render_handle_t gr, vec3_t v, float k, float *x, float *y)
{
    *x = gr->cfg.center_x + gr->cfg.radius * k * v.x;
    *y = gr->cfg.center_y - gr->cfg.radius * k * v.y;
}

/* Surface polylines, clipped at the limb. */
static void stroke_surface(globe_render_handle_t gr, const vec3_t *pts, const uint32_t *starts,
                           uint32_t lines, rgb_t color, float a)
{
    const uint16_t c = rgb565(color);
    for (uint32_t l = 0; l < lines; ++l) {
        bool have_prev = false;
        vec3_t pv = {0};
        for (uint32_t i = starts[l]; i < starts[l + 1]; ++i) {
            const vec3_t v = to_view(gr, pts[i]);
            if (have_prev && (pv.z >= 0.0f || v.z >= 0.0f)) {
                vec3_t a0 = pv, a1 = v;
                if (pv.z < 0.0f || v.z < 0.0f) {
                    const float t = pv.z / (pv.z - v.z);
                    vec3_t limb = {mixf(pv.x, v.x, t), mixf(pv.y, v.y, t), 0.0f};
                    const float n = sqrtf(limb.x * limb.x + limb.y * limb.y);
                    if (n > 0.0f) {
                        limb.x /= n;
                        limb.y /= n;
                    }
                    if (pv.z < 0.0f) {
                        a0 = limb;
                    } else {
                        a1 = limb;
                    }
                }
                float x0, y0, x1, y1;
                to_screen(gr, a0, 1.0f, &x0, &y0);
                to_screen(gr, a1, 1.0f, &x1, &y1);
                wu_line(gr, x0, y0, x1, y1, c, a);
            }
            pv = v;
            have_prev = true;
        }
    }
}

static size_t project_track(globe_render_handle_t gr, const globe_point_t *pts, size_t n, float k)
{
    n = n > MAX_TRACK_POINTS ? MAX_TRACK_POINTS : n;
    for (size_t i = 0; i < n; ++i) {
        const vec3_t v = to_view(gr, ecef(pts[i].lon, pts[i].lat));
        screen_pt_t *s = &gr->track[i];
        to_screen(gr, v, k, &s->x, &s->y);
        s->vis = v.z > 0.0f || (v.x * v.x + v.y * v.y) * k * k > 1.0f;
        s->alpha = v.z > 0.0f ? 1.0f : FAR_SIDE_ALPHA;
    }
    return n;
}

static void stroke_track(globe_render_handle_t gr, size_t n, float width, rgb_t c, float a, dash_t *dash)
{
    bool prev_drawn = false;
    for (size_t i = 1; i < n; ++i) {
        const screen_pt_t *p0 = &gr->track[i - 1], *p1 = &gr->track[i];
        if (p0->vis && p1->vis) {
            dashed_segment(gr, p0->x, p0->y, p1->x, p1->y, width, c, a * p1->alpha, dash, !prev_drawn);
            prev_drawn = true;
        } else {
            prev_drawn = false;
            if (dash) {
                dash->pos += sqrtf((p1->x - p0->x) * (p1->x - p0->x) + (p1->y - p0->y) * (p1->y - p0->y));
            }
        }
    }
}

/* ---------- layers ---------- */

static void build_base(globe_render_handle_t gr)
{
    const globe_render_config_t *c = &gr->cfg;
    const float R = c->radius, cx = c->center_x, cy = c->center_y;
    const float hlx = cx - 0.35f * R, hly = cy - 0.40f * R;
    const float shx = R * 0.95f, shy = shx * 0.14f, shcy = cy + R * 1.1f;
    for (int y = 0; y < c->height; ++y) {
        const float ty = (float)y / (float)(c->height - 1);
        for (int x = 0; x < c->width; ++x) {
            const float fx = x + 0.5f, fy = y + 0.5f;
            float r = mixf(SKY_TOP.r, SKY_BOTTOM.r, ty), g = mixf(SKY_TOP.g, SKY_BOTTOM.g, ty),
                  b = mixf(SKY_TOP.b, SKY_BOTTOM.b, ty);
            /* soft contact shadow under the floating globe */
            const float ex = (fx - cx) / shx, ey = (fy - shcy) / shy;
            const float ed = sqrtf(ex * ex + ey * ey);
            if (ed < 1.0f) {
                const float a = 0.34f * (1.0f - ed);
                r = mixf(r, SHADOW.r, a);
                g = mixf(g, SHADOW.g, a);
                b = mixf(b, SHADOW.b, a);
            }
            /* blurred drop shadow, offset downwards */
            const float dd = sqrtf((fx - cx) * (fx - cx) + (fy - cy - 0.07f * R) * (fy - cy - 0.07f * R));
            float da = clampf(1.0f - (dd - R * 0.92f) / (R * 0.22f), 0.0f, 1.0f);
            da = 0.30f * da * da;
            r = mixf(r, DROP_SHADOW.r, da);
            g = mixf(g, DROP_SHADOW.g, da);
            b = mixf(b, DROP_SHADOW.b, da);
            /* sphere body lit from the upper left */
            const float d = sqrtf((fx - cx) * (fx - cx) + (fy - cy) * (fy - cy));
            const float cov = clampf(R + 0.5f - d, 0.0f, 1.0f);
            if (cov > 0.0f) {
                const float t = clampf(sqrtf((fx - hlx) * (fx - hlx) + (fy - hly) * (fy - hly)) / (R * 1.35f), 0.0f, 1.0f);
                const rgb_t lo = t < 0.6f ? SPHERE_HI : SPHERE_MID, hi = t < 0.6f ? SPHERE_MID : SPHERE_EDGE;
                const float u = t < 0.6f ? t / 0.6f : (t - 0.6f) / 0.4f;
                r = mixf(r, mixf(lo.r, hi.r, u), cov);
                g = mixf(g, mixf(lo.g, hi.g, u), cov);
                b = mixf(b, mixf(lo.b, hi.b, u), cov);
            }
            const float rim = clampf(0.6f + 0.5f - fabsf(d - R), 0.0f, 1.0f) * 0.9f;
            r = mixf(r, RIM.r, rim);
            g = mixf(g, RIM.g, rim);
            b = mixf(b, RIM.b, rim);
            gr->base[(size_t)y * c->width + x] = pack(r, g, b, x, y);
        }
    }
}

/* Night side, darkening towards the unlit hemisphere with a soft terminator. */
static void shade_night(globe_render_handle_t gr, globe_point_t sun_pt)
{
    const float R = gr->cfg.radius, cx = gr->cfg.center_x, cy = gr->cfg.center_y;
    const vec3_t sun = to_view(gr, ecef(sun_pt.lon, sun_pt.lat));
    const uint16_t night = rgb565(NIGHT);
    for (int py = (int)(cy - R); py <= (int)(cy + R); ++py) {
        const float vy = (cy - (py + 0.5f)) / R;
        for (int px = (int)(cx - R); px <= (int)(cx + R); ++px) {
            const float vx = (px + 0.5f - cx) / R;
            const float z2 = 1.0f - vx * vx - vy * vy;
            if (z2 <= 0.0f) {
                continue;
            }
            const float ds = vx * sun.x + vy * sun.y + sqrtf(z2) * sun.z;
            if (ds < 0.08f) {
                const float t = clampf((0.08f - ds) / 0.16f, 0.0f, 1.0f);
                blend_px(gr, px, py, night, (uint32_t)(0.12f * ALPHA_ONE * t * t * (3.0f - 2.0f * t) + 0.5f));
            }
        }
    }
}

/* Ground area that can see the ISS, limited to the cap's screen bounds. */
static void shade_footprint(globe_render_handle_t gr, globe_point_t iss_pt)
{
    const float R = gr->cfg.radius, cx = gr->cfg.center_x, cy = gr->cfg.center_y;
    const vec3_t iss = to_view(gr, ecef(iss_pt.lon, iss_pt.lat));
    const float cos_fp = cosf(FOOTPRINT_DEG * DEG2RAD);
    const uint16_t accent = rgb565(ACCENT);
    const float ix = cx + R * iss.x, iy = cy - R * iss.y, ext = R * FOOTPRINT_EXTENT + 2.0f;
    for (int py = (int)(iy - ext); py <= (int)(iy + ext); ++py) {
        const float vy = (cy - (py + 0.5f)) / R;
        for (int px = (int)(ix - ext); px <= (int)(ix + ext); ++px) {
            const float vx = (px + 0.5f - cx) / R;
            const float z2 = 1.0f - vx * vx - vy * vy;
            if (z2 <= 0.0f) {
                continue;
            }
            const float di = vx * iss.x + vy * iss.y + sqrtf(z2) * iss.z;
            if (di > cos_fp) {
                blend_px(gr, px, py, accent, 3);
            }
            if (fabsf(di - cos_fp) < 0.0022f) {
                blend_px(gr, px, py, accent, 9);
            }
        }
    }
}

static void ground_shadow(globe_render_handle_t gr, float gx, float gy, float depth)
{
    const float cx = gr->cfg.center_x, cy = gr->cfg.center_y, rad = 14.0f;
    const float ang = atan2f(gy - cy, gx - cx), ca = cosf(ang), sa = sinf(ang);
    const float squash = fmaxf(depth, 0.25f);
    for (int py = (int)(gy - rad - 1); py <= (int)(gy + rad + 1); ++py) {
        for (int px = (int)(gx - rad - 1); px <= (int)(gx + rad + 1); ++px) {
            const float dx = px + 0.5f - gx, dy = py + 0.5f - gy;
            const float u = (dx * ca + dy * sa) / squash, v = -dx * sa + dy * ca;
            const float r = sqrtf(u * u + v * v);
            if (r < rad) {
                blend(gr, px, py, GROUND_SHADE, 0.42f * (1.0f - r / rad));
            }
        }
    }
}

static void station(globe_render_handle_t gr, float x, float y, float heading)
{
    const float c = cosf(heading), s = sinf(heading);
    /* local rectangles {x, y, w, h}: truss along the heading, arrays across it */
    static const float PANELS[4][4] = {{-2.5f, 5, 5, 9}, {-2.5f, -14, 5, 9}, {5, 4, 4, 7}, {5, -11, 4, 7}};
    static const float TRUSS[4] = {-9, -2, 20, 4};
    for (int i = 0; i < 5; ++i) {
        const float *rc = i < 4 ? PANELS[i] : TRUSS;
        const float lx[4] = {rc[0], rc[0] + rc[2], rc[0] + rc[2], rc[0]};
        const float ly[4] = {rc[1], rc[1], rc[1] + rc[3], rc[1] + rc[3]};
        float q[4][2];
        for (int k = 0; k < 4; ++k) {
            q[k][0] = x + lx[k] * c - ly[k] * s;
            q[k][1] = y + lx[k] * s + ly[k] * c;
        }
        quad(gr, q, i < 4 ? ACCENT : WHITE, 1.0f);
        for (int k = 0; k < 4; ++k) {
            const int j = (k + 1) & 3;
            segment(gr, q[k][0], q[k][1], q[j][0], q[j][1], 1.1f, ACCENT_DARK, 0.9f, true);
        }
    }
}

/* ---------- public API ---------- */

static vec3_t *convert_set(const world_map_set_t *set)
{
    vec3_t *out = malloc(sizeof(vec3_t) * set->point_count);
    if (!out) {
        return NULL;
    }
    for (uint32_t i = 0; i < set->point_count; ++i) {
        out[i] = ecef((float)set->points[i][0] / set->scale, (float)set->points[i][1] / set->scale);
    }
    return out;
}

static void build_grid(globe_render_handle_t gr)
{
    uint32_t n = 0, line = 0;
    for (int m = 0; m < GRID_MERIDIANS; ++m) {
        gr->grid_starts[line++] = n;
        for (int lat = -90; lat <= 90; lat += GRID_SAMPLE_DEG) {
            gr->grid[n++] = ecef((float)(m * GRID_STEP_DEG - 180), (float)lat);
        }
    }
    for (int p = 1; p <= GRID_PARALLELS; ++p) {
        gr->grid_starts[line++] = n;
        for (int lon = -180; lon <= 180; lon += GRID_SAMPLE_DEG) {
            gr->grid[n++] = ecef((float)lon, (float)(p * GRID_STEP_DEG - 90));
        }
    }
    gr->grid_starts[line] = n;
}

esp_gsp_err_t globe_render_create(const globe_render_config_t *config, globe_render_handle_t *ret_handle)
{
    if (!config || !ret_handle || !config->width || !config->height || !config->radius) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    *ret_handle = NULL;
    globe_render_handle_t gr = calloc(1, sizeof(*gr));
    if (!gr) {
        return ESP_GSP_ERR_NO_MEM;
    }
    gr->cfg = *config;
    gr->base = malloc(sizeof(uint16_t) * config->width * config->height);
    gr->surface = malloc(sizeof(uint16_t) * config->width * config->height);
    gr->coast = convert_set(&world_map_coast);
    gr->borders = convert_set(&world_map_borders);
    gr->grid = malloc(sizeof(vec3_t) * GRID_POINTS);
    gr->track = malloc(sizeof(screen_pt_t) * MAX_TRACK_POINTS);
    if (!gr->base || !gr->surface || !gr->coast || !gr->borders || !gr->grid || !gr->track) {
        globe_render_delete(gr);
        return ESP_GSP_ERR_NO_MEM;
    }
    build_grid(gr);
    build_base(gr);
    *ret_handle = gr;
    return ESP_GSP_OK;
}

void globe_render_delete(globe_render_handle_t gr)
{
    if (!gr) {
        return;
    }
    free(gr->base);
    free(gr->surface);
    free(gr->coast);
    free(gr->borders);
    free(gr->grid);
    free(gr->track);
    free(gr);
}

void globe_render_draw(globe_render_handle_t gr, const globe_scene_t *s, uint16_t *dst, size_t stride_px)
{
    const globe_render_config_t *c = &gr->cfg;
    const size_t frame_px = (size_t)c->width * c->height;

    /* The map and night side change only when the view or the Sun moves noticeably. */
    const bool stale = !gr->surface_valid ||
        fabsf(remainderf(s->view_lon - gr->surface_lon, 360.0f)) > CACHE_VIEW_DEG ||
        fabsf(s->view_lat - gr->surface_lat) > CACHE_VIEW_DEG ||
        fabsf(remainderf(s->sun.lon - gr->surface_sun_lon, 360.0f)) > CACHE_SUN_DEG ||
        fabsf(s->sun.lat - gr->surface_sun_lat) > CACHE_SUN_DEG;
    if (stale) {
        gr->dst = gr->surface;
        gr->stride = c->width;
        memcpy(gr->surface, gr->base, sizeof(uint16_t) * frame_px);
        set_view(gr, s->view_lon, s->view_lat);
        stroke_surface(gr, gr->grid, gr->grid_starts, GRID_LINES, GRID, 0.16f);
        stroke_surface(gr, gr->coast, world_map_coast.starts, world_map_coast.polyline_count, COAST, 0.85f);
        stroke_surface(gr, gr->borders, world_map_borders.starts, world_map_borders.polyline_count, BORDER, 0.45f);
        shade_night(gr, s->sun);
        gr->surface_valid = true;
        gr->surface_lon = s->view_lon;
        gr->surface_lat = s->view_lat;
        gr->surface_sun_lon = s->sun.lon;
        gr->surface_sun_lat = s->sun.lat;
    }

    gr->dst = dst;
    gr->stride = stride_px;
    if (stride_px == c->width) {
        memcpy(dst, gr->surface, sizeof(uint16_t) * frame_px);
    } else {
        for (int y = 0; y < c->height; ++y) {
            memcpy(&dst[(size_t)y * stride_px], &gr->surface[(size_t)y * c->width], sizeof(uint16_t) * c->width);
        }
    }
    set_view(gr, s->view_lon, s->view_lat);
    if (s->has_iss) {
        shade_footprint(gr, s->iss);
    }

    /* the orbit's shadow on the ground */
    size_t n = project_track(gr, s->past, s->past_count, 1.0f);
    for (size_t i = 0; i < n; ++i) {
        gr->track[i].vis = gr->track[i].alpha >= 1.0f;
    }
    stroke_track(gr, n, 3.0f, SHADOW, 0.13f, NULL);
    n = project_track(gr, s->future, s->future_count, 1.0f);
    for (size_t i = 0; i < n; ++i) {
        gr->track[i].vis = gr->track[i].alpha >= 1.0f;
    }
    stroke_track(gr, n, 3.0f, SHADOW, 0.13f, NULL);

    float gx = 0, gy = 0, sx = 0, sy = 0;
    vec3_t ground = {0}, sky = {0};
    bool sky_vis = false;
    if (s->has_iss) {
        ground = to_view(gr, ecef(s->iss.lon, s->iss.lat));
        to_screen(gr, ground, 1.0f, &gx, &gy);
        to_screen(gr, ground, ORBIT_SCALE, &sx, &sy);
        sky = ground;
        sky_vis = sky.z > 0.0f || (sky.x * sky.x + sky.y * sky.y) * ORBIT_SCALE * ORBIT_SCALE > 1.0f;
        if (ground.z > 0.0f) {
            ground_shadow(gr, gx, gy, ground.z);
        }
    }

    /* orbit in the sky: past solid, projected dashed */
    n = project_track(gr, s->past, s->past_count, ORBIT_SCALE);
    stroke_track(gr, n, 2.2f, ACCENT, 0.5f, NULL);
    n = project_track(gr, s->future, s->future_count, ORBIT_SCALE);
    dash_t dash = {.on = 7.0f, .off = 6.0f, .pos = 0.0f};
    stroke_track(gr, n, 2.0f, ACCENT, 1.0f, &dash);

    for (size_t i = 0; i < s->tick_count; ++i) {
        const vec3_t v = to_view(gr, ecef(s->ticks[i].point.lon, s->ticks[i].point.lat));
        if (v.z <= 0.0f) {
            continue;
        }
        float tx, ty;
        to_screen(gr, v, ORBIT_SCALE, &tx, &ty);
        disc(gr, tx, ty, 3.0f, WHITE, 1.0f);
        ring(gr, tx, ty, 3.0f, 1.5f, ACCENT, 1.0f);
        char label[8] = "+";
        const unsigned m = s->ticks[i].minutes;
        label[1] = (char)('0' + (m / 10) % 10);
        label[2] = (char)('0' + m % 10);
        label[3] = 'm';
        label[4] = '\0';
        text(gr, (int)tx + 7, (int)ty - 18, m < 10 ? "" : label, TICK_LABEL, 1.0f);
    }

    if (s->has_iss && sky_vis) {
        if (ground.z > 0.0f) {
            dash_t dots = {.on = 2.0f, .off = 3.0f, .pos = 0.0f};
            dashed_segment(gr, gx, gy, sx, sy, 1.0f, ACCENT, 0.45f, &dots, true);
        }
        float heading = 0.0f;
        if (s->future_count > 1) {
            const vec3_t nv = to_view(gr, ecef(s->future[1].lon, s->future[1].lat));
            float nx, ny;
            to_screen(gr, nv, ORBIT_SCALE, &nx, &ny);
            heading = atan2f(ny - sy, nx - sx);
        }
        station(gr, sx, sy, heading);
        text(gr, (int)sx + 15, (int)sy - 22, "ISS", ACCENT_DARK, 1.0f);
    }
}
