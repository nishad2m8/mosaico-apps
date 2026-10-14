// SPDX-License-Identifier: Apache-2.0
//
// Device networking for the ISS tracker: joins the Wi-Fi network saved by Vibe
// Mode, sets the clock over SNTP and downloads fresh ISS orbit data (TLE)
// from CelesTrak, keeping the last good set in NVS.
//
// Singleton owning the Wi-Fi station, its netif and one worker task. All
// functions are thread-safe after iss_net_start(); none block on the network.
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "sgp4.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ISS_NET_TLE_LINE_SIZE (SGP4_TLE_LINE_LEN + 1)

typedef enum {
    ISS_NET_NO_WIFI = 0,     /* Vibe Mode has no saved network */
    ISS_NET_CONNECTING,
    ISS_NET_ONLINE,
} iss_net_state_t;

/** Start Wi-Fi and the worker task. Call once after nvs_flash_init(). */
esp_err_t iss_net_start(void);

iss_net_state_t iss_net_get_state(void);

/** True once the system clock has been set (SNTP or retained RTC time). */
bool iss_net_time_valid(void);

/** Copy the last saved TLE from NVS; ESP_ERR_NOT_FOUND when none is stored. */
esp_err_t iss_net_load_saved_tle(char line1[ISS_NET_TLE_LINE_SIZE], char line2[ISS_NET_TLE_LINE_SIZE]);

/** Copy a newly downloaded TLE once; returns false when nothing new arrived. */
bool iss_net_take_tle(char line1[ISS_NET_TLE_LINE_SIZE], char line2[ISS_NET_TLE_LINE_SIZE]);

#ifdef __cplusplus
}
#endif
