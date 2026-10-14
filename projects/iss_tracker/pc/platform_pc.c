// SPDX-License-Identifier: Apache-2.0
//
// Simulator wiring: a GSP timer drives the tracker with the host clock and the
// built-in element set. All iss_ui calls run on the render task (single owner).

#include <stdio.h>
#include <sys/time.h>

#include "gsp_sim_bridge.h"
#include "iss_ui.h"

#define FRAME_PERIOD_MS 50

static iss_ui_handle_t s_ui;
static void *s_timer;

static int64_t host_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void tick(esp_gsp_handle_t ui, void *ctx)
{
    (void)ui;
    (void)iss_ui_update(ctx, host_time_ms(), true);
}

esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui)
{
    esp_gsp_err_t err = iss_ui_create(ui, &s_ui);
    if (err != ESP_GSP_OK) {
        return err;
    }
    iss_ui_set_link(s_ui, "SIMULATOR");
    err = iss_ui_update(s_ui, host_time_ms(), true);
    s_timer = esp_gsp_timer_create(ui, FRAME_PERIOD_MS, tick, s_ui);
    return s_timer ? err : ESP_GSP_ERR_NO_MEM;
}

void gsp_bridge_app_deinit(esp_gsp_handle_t ui)
{
    if (s_timer) {
        (void)esp_gsp_timer_delete(ui, s_timer);
        s_timer = NULL;
    }
    iss_ui_delete(s_ui);
    s_ui = NULL;
    fprintf(stderr, "iss_tracker: simulator stopped\n");
}
