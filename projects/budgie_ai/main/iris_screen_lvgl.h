// SPDX-License-Identifier: Apache-2.0
//
// ESP-Iris screenshot provider for the LVGL UI, so `mosaico.py iris screenshot`
// works for this app like the GSP template's iris_screen_mirror. A capture
// renders the active LVGL screen (RGB565, little-endian) on the LVGL task into
// a PSRAM buffer allocated once at registration.
#pragma once

#include "esp_err.h"

/** Register the provider with ESP-Iris. Call after app_ui_start(). */
esp_err_t iris_screen_lvgl_register(void);
