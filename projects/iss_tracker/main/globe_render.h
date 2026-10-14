// SPDX-License-Identifier: Apache-2.0
//
// Software renderer for the ISS globe: an orthographic Earth with coastlines,
// borders, graticule, night side and the ISS orbit drawn at an exaggerated
// height, so the station floats above its shadow. Output is native RGB565.
//
// A handle owns its pre-rendered background, a cached map layer for the last
// view and the projected map vectors. globe_render_draw() updates that cache,
// so a handle must be used by one task at a time.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_gsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct globe_render *globe_render_handle_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t center_x;
    uint16_t center_y;
    uint16_t radius;            /* globe radius in pixels */
} globe_render_config_t;

typedef struct {
    float lon;
    float lat;
} globe_point_t;

typedef struct {
    globe_point_t point;
    uint8_t minutes;            /* label, e.g. 15 for "+15m" */
} globe_tick_t;

typedef struct {
    float view_lon;             /* globe centre facing the viewer, degrees */
    float view_lat;
    globe_point_t sun;          /* sub-solar point */
    bool has_iss;
    globe_point_t iss;
    const globe_point_t *past;  /* oldest first, ends at the ISS */
    size_t past_count;
    const globe_point_t *future;/* starts at the ISS */
    size_t future_count;
    const globe_tick_t *ticks;
    size_t tick_count;
} globe_scene_t;

esp_gsp_err_t globe_render_create(const globe_render_config_t *config,
                                  globe_render_handle_t *ret_handle);
void globe_render_delete(globe_render_handle_t handle);

/** Draw a full frame into @p dst (width x height RGB565, stride in pixels). */
void globe_render_draw(globe_render_handle_t handle, const globe_scene_t *scene,
                       uint16_t *dst, size_t stride_px);

#ifdef __cplusplus
}
#endif
