// SPDX-License-Identifier: Apache-2.0
//
// Physical buttons the application may use at runtime:
//   Function / AI button (GPIO7)  - push-to-talk
//   Boot button (GPIO61)          - harmless "wake / back / cancel"
//
// Both GPIOs are also boot-time selectors, sampled only at reset:
// AI held at power-on selects Vibe Mode; Boot held at power-on selects ROM
// Download Mode. They are read here only as pulled-up inputs (never driven),
// so the factory key sequences keep working whatever state this app is in.
//
// The POWER key is not visible to the MCU: the SAM8108 power controller
// toggles board power on its own, so it works even if this firmware hangs.
//
// Singleton. Callbacks run in the iot_button timer task; keep them short and
// hand real work to other tasks.
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOARD_BTN_FN_DOWN = 0,     /* Function pressed */
    BOARD_BTN_FN_UP,           /* Function released (any press length) */
    BOARD_BTN_BOOT_CLICK,      /* Boot short click */
    BOARD_BTN_BOOT_LONG,       /* Boot held >= long-press time */
} board_button_event_t;

typedef void (*board_buttons_cb_t)(board_button_event_t event, void *user_ctx);

/** Create both buttons and route their events to @p cb. */
esp_err_t board_buttons_start(board_buttons_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif
