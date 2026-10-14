// SPDX-License-Identifier: Apache-2.0
//
// ISS tracker screen: propagates the ISS, renders the globe into Canvas frames
// and keeps the text readout current. Portable between ESP-Mosaico and the
// GSP simulator.
//
// Single owner: every iss_ui_* call must come from one task (the device app
// task, or the simulator's render-task timer). The only cross-task access is
// from the render task: the Canvas release callback (atomic slot flags) and
// the pointer observer (atomic drag deltas). Dragging spins the globe; it
// returns to following the ISS a few seconds after the last touch.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_gsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct iss_ui *iss_ui_handle_t;

/** Create the screen controller; starts with the built-in element set. */
esp_gsp_err_t iss_ui_create(esp_gsp_handle_t gsp, iss_ui_handle_t *ret_handle);

/** Stop the Canvas, wait for frames to be released and free everything. */
void iss_ui_delete(iss_ui_handle_t handle);

/** Replace the element set (two TLE lines). Keeps the old one on error. */
esp_gsp_err_t iss_ui_set_tle(iss_ui_handle_t handle, const char *line1, const char *line2);

/** Connection summary shown in the status corner, e.g. "LIVE" or "NO WI-FI". */
void iss_ui_set_link(iss_ui_handle_t handle, const char *text);

/**
 * Advance to @p unix_ms and present a frame if a buffer is free.
 * Without valid wall-clock time (@p time_valid false) the globe idles and
 * the ISS is hidden, because its position cannot be known.
 */
esp_gsp_err_t iss_ui_update(iss_ui_handle_t handle, int64_t unix_ms, bool time_valid);

#ifdef __cplusplus
}
#endif
