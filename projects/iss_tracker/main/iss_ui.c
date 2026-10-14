// SPDX-License-Identifier: Apache-2.0

#include "iss_ui.h"

#include <ctype.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Canvas APIs take the raw bind ID; no typed helper exists for them. */
#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "bundle_gsp.h"
#include "globe_render.h"
#include "iss_orbit.h"
#include "iss_tle_default.h"
#include "world_map.h"

#define SCREEN_W            480
#define SCREEN_H            480
#define GLOBE_CX            240
#define GLOBE_CY            252
#define GLOBE_RADIUS        166
#define FRAME_SLOTS         2
#define PAST_MIN            45
#define FUTURE_MIN          90
#define TRACK_STEP_S        30
#define PAST_POINTS         (PAST_MIN * 60 / TRACK_STEP_S + 1)
#define FUTURE_POINTS       (FUTURE_MIN * 60 / TRACK_STEP_S + 1)
#define TICK_EVERY_MIN      15
#define TICK_COUNT          (FUTURE_MIN / TICK_EVERY_MIN)
#define TRACK_REFRESH_MS    5000
#define HUD_REFRESH_MS      250
#define VIEW_EASE           0.08f
#define VIEW_LON_OFFSET     28.0f    /* keep the ISS right of centre so its height shows */
#define VIEW_LAT_OFFSET     22.0f    /* ...and above centre */
#define VIEW_LAT_LIMIT      60.0f
#define IDLE_SPIN_DEG       0.15f
#define IDLE_VIEW_LAT       20.0f
#define FLUSH_TIMEOUT_MS    500
#define TEXT_LEN            64
#define LINK_LEN            32
#define MAX_AGE_TENTHS      9999
#define MS_PER_DAY          86400000.0
#define DEG_PER_RAD         57.29578f
#define FOLLOW_RESUME_MS    6000     /* return to the ISS this long after the finger lifts */
#define FLING_DECAY         0.85f    /* per frame, after the finger lifts */
#define FLING_MAX_DEG       6.0f
#define IDLE_FRAME_MS       1000     /* redraw rate while nothing on the globe moves */
#define VIEW_STILL_DEG      0.02f
#define FLING_MIN_DEG       0.02f

enum { HUD_OVER, HUD_DATE, HUD_TIME, HUD_POSITION, HUD_ALTITUDE, HUD_STATUS, HUD_COUNT };

typedef struct {
    struct iss_ui *owner;
    uint16_t *pixels;
    atomic_bool busy;               /* set by the owner, cleared by the release callback */
} frame_slot_t;

struct iss_ui {
    esp_gsp_handle_t gsp;
    globe_render_handle_t globe;
    frame_slot_t slots[FRAME_SLOTS];
    sgp4_record_t tle;
    globe_point_t past[PAST_POINTS];
    globe_point_t future[FUTURE_POINTS];
    globe_tick_t ticks[TICK_COUNT];
    bool track_valid;
    int64_t track_at_ms;
    bool view_ready;
    float view_lon, view_lat;
    int64_t hud_at_ms;
    /* touch: written by the pointer observer (render task), consumed by the owner */
    atomic_int drag_dx;             /* accumulated pixels since the last update */
    atomic_int drag_dy;
    atomic_bool touching;
    int32_t touch_x, touch_y;       /* render task only */
    bool touch_tracking;            /* render task only */
    /* owner-side view control */
    float fling_lon, fling_lat;     /* degrees per frame */
    int64_t manual_until_ms;
    char link[LINK_LEN];
    char shown[HUD_COUNT][TEXT_LEN];   /* last text per label, to skip no-op updates */
    int64_t frame_at_ms;            /* last presented frame */
    float frame_lon, frame_lat;     /* view of that frame */
};


static void frame_released(void *ctx)
{
    frame_slot_t *slot = ctx;
    atomic_store(&slot->busy, false);
}

/* Render task: turn touch samples into drag deltas for the owner task. */
static void on_pointer(esp_gsp_handle_t gsp, int32_t x, int32_t y, bool pressed, void *ctx)
{
    (void)gsp;
    iss_ui_handle_t ui = ctx;
    if (pressed && ui->touch_tracking) {
        atomic_fetch_add(&ui->drag_dx, x - ui->touch_x);
        atomic_fetch_add(&ui->drag_dy, y - ui->touch_y);
    }
    ui->touch_x = x;
    ui->touch_y = y;
    ui->touch_tracking = pressed;
    atomic_store(&ui->touching, pressed);
}

static float wrap180f(float deg)
{
    deg = fmodf(deg + 180.0f, 360.0f);
    return (deg < 0.0f ? deg + 360.0f : deg) - 180.0f;
}

static float ease_angle(float cur, float target, float f)
{
    return wrap180f(cur + wrap180f(target - cur) * f);
}

static const char *ocean_name(double lat, double lon)
{
    if (lat > 66.0) {
        return "Arctic Ocean";
    }
    if (lat < -60.0) {
        return "Southern Ocean";
    }
    if (lon >= 20.0 && lon <= 120.0 && lat < 30.0) {
        return "Indian Ocean";
    }
    if (lon > -70.0 && lon < 20.0) {
        return "Atlantic Ocean";
    }
    return "Pacific Ocean";
}

/* Fit the longest map names beside the clock. */
static const char *short_name(const char *name)
{
    static const char *const LONG_NAMES[][2] = {
        {"United States of America", "United States"},
        {"Fr. S. Antarctic Lands", "French Southern Lands"},
    };
    for (size_t i = 0; i < sizeof(LONG_NAMES) / sizeof(LONG_NAMES[0]); ++i) {
        if (strcmp(name, LONG_NAMES[i][0]) == 0) {
            return LONG_NAMES[i][1];
        }
    }
    return name;
}

static void set_hud(iss_ui_handle_t ui, int which, const char *text)
{
    if (strcmp(ui->shown[which], text) == 0) {
        return;
    }
    esp_gsp_err_t err = ESP_GSP_FAIL;
    switch (which) {
    case HUD_OVER:     err = gsp_iss_over_set_text(ui->gsp, text); break;
    case HUD_DATE:     err = gsp_iss_date_set_text(ui->gsp, text); break;
    case HUD_TIME:     err = gsp_iss_time_set_text(ui->gsp, text); break;
    case HUD_POSITION: err = gsp_iss_position_set_text(ui->gsp, text); break;
    case HUD_ALTITUDE: err = gsp_iss_altitude_set_text(ui->gsp, text); break;
    case HUD_STATUS:   err = gsp_iss_status_set_text(ui->gsp, text); break;
    }
    if (err == ESP_GSP_OK) {
        strlcpy(ui->shown[which], text, TEXT_LEN);
    }
}

static void refresh_track(iss_ui_handle_t ui, int64_t now_ms)
{
    ui->track_valid = false;
    for (int i = 0; i < PAST_POINTS; ++i) {
        iss_orbit_fix_t fix;
        if (iss_orbit_fix(&ui->tle, now_ms + (int64_t)(i - (PAST_POINTS - 1)) * TRACK_STEP_S * 1000, &fix) != SGP4_OK) {
            return;
        }
        ui->past[i] = (globe_point_t){(float)fix.lon_deg, (float)fix.lat_deg};
    }
    for (int i = 0; i < FUTURE_POINTS; ++i) {
        iss_orbit_fix_t fix;
        if (iss_orbit_fix(&ui->tle, now_ms + (int64_t)i * TRACK_STEP_S * 1000, &fix) != SGP4_OK) {
            return;
        }
        ui->future[i] = (globe_point_t){(float)fix.lon_deg, (float)fix.lat_deg};
    }
    for (int i = 0; i < TICK_COUNT; ++i) {
        const int minutes = (i + 1) * TICK_EVERY_MIN;
        ui->ticks[i].point = ui->future[minutes * 60 / TRACK_STEP_S];
        ui->ticks[i].minutes = (uint8_t)minutes;
    }
    ui->track_valid = true;
    ui->track_at_ms = now_ms;
}

static void update_hud(iss_ui_handle_t ui, int64_t now_ms, bool time_valid, const iss_orbit_fix_t *fix)
{
    char text[TEXT_LEN];
    if (time_valid) {
        const double age_days = (double)(now_ms - ui->tle.epoch_unix_ms) / MS_PER_DAY;
        const int tenths = (int)fmin(fmax(age_days * 10.0, 0.0), MAX_AGE_TENTHS);
        snprintf(text, sizeof(text), "%s · TLE %d.%d D", ui->link, tenths / 10, tenths % 10);
    } else {
        strlcpy(text, ui->link, sizeof(text));
    }
    set_hud(ui, HUD_STATUS, text);

    const time_t secs = (time_t)(now_ms / 1000);
    struct tm utc;
    if (time_valid && gmtime_r(&secs, &utc)) {
        strftime(text, sizeof(text), "%d %b %Y · UTC", &utc);
        for (char *c = text; *c; ++c) {
            *c = (char)toupper((unsigned char)*c);
        }
        set_hud(ui, HUD_DATE, text);
        strftime(text, sizeof(text), "%H:%M:%S", &utc);
        set_hud(ui, HUD_TIME, text);
    } else {
        set_hud(ui, HUD_DATE, "SETTING CLOCK");
        set_hud(ui, HUD_TIME, "--:--:--");
    }

    if (!fix) {
        set_hud(ui, HUD_OVER, time_valid ? "ISS position unavailable" : "Waiting for the time");
        set_hud(ui, HUD_POSITION, "LAT --   LON --");
        set_hud(ui, HUD_ALTITUDE, "ALT --   SPEED --");
        return;
    }

    const char *country = world_map_country_at(fix->lat_deg, fix->lon_deg);
    snprintf(text, sizeof(text), "Over %s", country ? short_name(country) : ocean_name(fix->lat_deg, fix->lon_deg));
    set_hud(ui, HUD_OVER, text);

    snprintf(text, sizeof(text), "LAT %.2f°%c   LON %.2f°%c",
             fabs(fix->lat_deg), fix->lat_deg >= 0.0 ? 'N' : 'S',
             fabs(fix->lon_deg), fix->lon_deg >= 0.0 ? 'E' : 'W');
    set_hud(ui, HUD_POSITION, text);

    const long speed = lround(fix->speed_kmh);
    snprintf(text, sizeof(text), "ALT %.0f km   %ld,%03ld km/h", fix->alt_km, speed / 1000, speed % 1000);
    set_hud(ui, HUD_ALTITUDE, text);
}

static frame_slot_t *free_slot(iss_ui_handle_t ui)
{
    for (int i = 0; i < FRAME_SLOTS; ++i) {
        if (!atomic_load(&ui->slots[i].busy)) {
            return &ui->slots[i];
        }
    }
    return NULL;
}

esp_gsp_err_t iss_ui_create(esp_gsp_handle_t gsp, iss_ui_handle_t *ret_handle)
{
    if (!gsp || !ret_handle) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    *ret_handle = NULL;
    iss_ui_handle_t ui = calloc(1, sizeof(*ui));
    if (!ui) {
        return ESP_GSP_ERR_NO_MEM;
    }
    ui->gsp = gsp;
    strlcpy(ui->link, "STARTING", sizeof(ui->link));
    if (sgp4_init(&ui->tle, ISS_TLE_DEFAULT_LINE1, ISS_TLE_DEFAULT_LINE2) != SGP4_OK) {
        free(ui);
        return ESP_GSP_ERR_INVALID_STATE;
    }
    const globe_render_config_t cfg = {
        .width = SCREEN_W, .height = SCREEN_H,
        .center_x = GLOBE_CX, .center_y = GLOBE_CY, .radius = GLOBE_RADIUS,
    };
    esp_gsp_err_t err = globe_render_create(&cfg, &ui->globe);
    for (int i = 0; err == ESP_GSP_OK && i < FRAME_SLOTS; ++i) {
        ui->slots[i].owner = ui;
        atomic_init(&ui->slots[i].busy, false);
        ui->slots[i].pixels = malloc(sizeof(uint16_t) * SCREEN_W * SCREEN_H);
        if (!ui->slots[i].pixels) {
            err = ESP_GSP_ERR_NO_MEM;
        }
    }
    if (err != ESP_GSP_OK) {
        for (int i = 0; i < FRAME_SLOTS; ++i) {
            free(ui->slots[i].pixels);
        }
        globe_render_delete(ui->globe);
        free(ui);
        return err;
    }
    atomic_init(&ui->drag_dx, 0);
    atomic_init(&ui->drag_dy, 0);
    atomic_init(&ui->touching, false);
    err = esp_gsp_set_pointer_observer(gsp, on_pointer, ui);
    if (err != ESP_GSP_OK) {
        for (int i = 0; i < FRAME_SLOTS; ++i) {
            free(ui->slots[i].pixels);
        }
        globe_render_delete(ui->globe);
        free(ui);
        return err;
    }
    *ret_handle = ui;
    return ESP_GSP_OK;
}

void iss_ui_delete(iss_ui_handle_t ui)
{
    if (!ui) {
        return;
    }
    (void)esp_gsp_set_pointer_observer(ui->gsp, NULL, NULL);
    (void)esp_gsp_canvas_stop(ui->gsp, GSP_ISS_BIND_GLOBE);
    if (esp_gsp_flush(ui->gsp, FLUSH_TIMEOUT_MS) != ESP_GSP_OK) {
        /* frames may still be referenced by the renderer: leak rather than free */
        return;
    }
    for (int i = 0; i < FRAME_SLOTS; ++i) {
        free(ui->slots[i].pixels);
    }
    globe_render_delete(ui->globe);
    free(ui);
}

esp_gsp_err_t iss_ui_set_tle(iss_ui_handle_t ui, const char *line1, const char *line2)
{
    if (!ui || !line1 || !line2) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    sgp4_record_t rec;
    if (sgp4_init(&rec, line1, line2) != SGP4_OK) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    ui->tle = rec;
    ui->track_valid = false;
    return ESP_GSP_OK;
}

void iss_ui_set_link(iss_ui_handle_t ui, const char *text)
{
    if (ui && text) {
        strlcpy(ui->link, text, sizeof(ui->link));
        ui->hud_at_ms = 0;
    }
}

esp_gsp_err_t iss_ui_update(iss_ui_handle_t ui, int64_t now_ms, bool time_valid)
{
    if (!ui) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    iss_orbit_fix_t fix;
    bool have_fix = time_valid && iss_orbit_fix(&ui->tle, now_ms, &fix) == SGP4_OK;
    if (have_fix && (!ui->track_valid || llabs(now_ms - ui->track_at_ms) >= TRACK_REFRESH_MS)) {
        refresh_track(ui, now_ms);
    }
    have_fix = have_fix && ui->track_valid;

    /* Finger drags spin the globe: one pixel at the centre moves the surface one pixel. */
    const int dx = atomic_exchange(&ui->drag_dx, 0), dy = atomic_exchange(&ui->drag_dy, 0);
    const bool touching = atomic_load(&ui->touching);
    const float k = DEG_PER_RAD / GLOBE_RADIUS;
    float step_lon, step_lat;
    if (dx || dy || touching) {
        /* follow the finger exactly; remember a capped velocity for the fling */
        step_lon = -dx * k;
        step_lat = dy * k;
        ui->fling_lon = fmaxf(-FLING_MAX_DEG, fminf(FLING_MAX_DEG, step_lon));
        ui->fling_lat = fmaxf(-FLING_MAX_DEG, fminf(FLING_MAX_DEG, step_lat));
        ui->manual_until_ms = now_ms + FOLLOW_RESUME_MS;
    } else {
        ui->fling_lon *= FLING_DECAY;
        ui->fling_lat *= FLING_DECAY;
        if (fabsf(ui->fling_lon) < FLING_MIN_DEG && fabsf(ui->fling_lat) < FLING_MIN_DEG) {
            ui->fling_lon = ui->fling_lat = 0.0f;
        }
        step_lon = ui->fling_lon;
        step_lat = ui->fling_lat;
    }
    const bool manual = touching || now_ms < ui->manual_until_ms;

    if (manual) {
        ui->view_lon = wrap180f(ui->view_lon + step_lon);
        ui->view_lat = fmaxf(-89.0f, fminf(89.0f, ui->view_lat + step_lat));
    } else if (have_fix) {
        const float target_lon = wrap180f((float)fix.lon_deg - VIEW_LON_OFFSET);
        float target_lat = (float)fix.lat_deg - VIEW_LAT_OFFSET;
        target_lat = fmaxf(-VIEW_LAT_LIMIT, fminf(VIEW_LAT_LIMIT, target_lat));
        const float f = ui->view_ready ? VIEW_EASE : 1.0f;
        ui->view_lon = ease_angle(ui->view_lon, target_lon, f);
        ui->view_lat += (target_lat - ui->view_lat) * f;
        ui->view_ready = true;
    } else {
        ui->view_lon = wrap180f(ui->view_lon + IDLE_SPIN_DEG);
        ui->view_lat += (IDLE_VIEW_LAT - ui->view_lat) * VIEW_EASE;
        ui->view_ready = false;
    }

    if (llabs(now_ms - ui->hud_at_ms) >= HUD_REFRESH_MS) {
        update_hud(ui, now_ms, time_valid, have_fix ? &fix : NULL);
        ui->hud_at_ms = now_ms;
    }

    const bool view_moved = fabsf(wrap180f(ui->view_lon - ui->frame_lon)) > VIEW_STILL_DEG ||
                            fabsf(ui->view_lat - ui->frame_lat) > VIEW_STILL_DEG;
    if (!manual && !view_moved && llabs(now_ms - ui->frame_at_ms) < IDLE_FRAME_MS) {
        return ESP_GSP_OK;   /* nothing visible changed */
    }
    frame_slot_t *slot = free_slot(ui);
    if (!slot) {
        return ESP_GSP_OK;   /* both frames still on screen or queued */
    }
    double sun_lat = 0.0, sun_lon = 0.0;
    if (time_valid) {
        iss_orbit_subsolar(now_ms, &sun_lat, &sun_lon);
    } else {
        sun_lon = ui->view_lon - 60.0;   /* pleasant fixed lighting while idle */
    }
    const globe_scene_t scene = {
        .view_lon = ui->view_lon,
        .view_lat = ui->view_lat,
        .sun = {(float)sun_lon, (float)sun_lat},
        .has_iss = have_fix,
        .iss = have_fix ? (globe_point_t){(float)fix.lon_deg, (float)fix.lat_deg} : (globe_point_t){0},
        .past = ui->past,
        .past_count = have_fix ? PAST_POINTS : 0,
        .future = ui->future,
        .future_count = have_fix ? FUTURE_POINTS : 0,
        .ticks = ui->ticks,
        .tick_count = have_fix ? TICK_COUNT : 0,
    };
    globe_render_draw(ui->globe, &scene, slot->pixels, SCREEN_W);

    atomic_store(&slot->busy, true);
    const esp_gsp_err_t err = esp_gsp_canvas_try_push(ui->gsp, GSP_ISS_BIND_GLOBE, slot->pixels,
                              SCREEN_W * sizeof(uint16_t), frame_released, slot);
    if (err != ESP_GSP_OK) {
        atomic_store(&slot->busy, false);   /* not accepted: still ours */
    } else {
        ui->frame_at_ms = now_ms;
        ui->frame_lon = ui->view_lon;
        ui->frame_lat = ui->view_lat;
    }
    return err == ESP_GSP_ERR_TIMEOUT ? ESP_GSP_OK : err;
}
