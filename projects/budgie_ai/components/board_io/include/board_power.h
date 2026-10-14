// SPDX-License-Identifier: Apache-2.0
//
// Battery gauge (BQ27220) readings and software power-off.
//
// Power-off only *requests* shutdown from the SAM8108 controller via the BSP
// PWR_SW line; the physical POWER key always works independently. While USB
// supplies the board the request may have no effect, so board_power_off()
// returns ESP_ERR_INVALID_STATE in that case and the caller should tell the
// user to unplug or press POWER.
//
// Thread-safe: battery reads are serialized internally.
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int percent;        /* 0..100 state of charge */
    int voltage_mv;
    int current_ma;     /* > 0 while charging */
    bool charging;
} board_battery_t;

/** Initialize board power rails and the fuel gauge (gauge failure is non-fatal). */
esp_err_t board_power_init(void);

/** Read the gauge. ESP_ERR_NOT_FOUND when no gauge/battery is present. */
esp_err_t board_power_read_battery(board_battery_t *out);

/**
 * Request board shutdown. @p before_off runs first (stop audio, blank the
 * display). Returns only if power stays on after the request.
 */
esp_err_t board_power_off(void (*before_off)(void));

#ifdef __cplusplus
}
#endif
