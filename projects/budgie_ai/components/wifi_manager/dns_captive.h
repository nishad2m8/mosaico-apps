// SPDX-License-Identifier: Apache-2.0
//
// Private: tiny DNS responder that answers every A query with the setup AP's
// address so phones open the configuration page (captive portal).
#pragma once

#include <stdint.h>
#include "esp_err.h"

/** Start answering on UDP port 53 with @p ipv4 (network byte order). Idempotent. */
esp_err_t dns_captive_start(uint32_t ipv4);

/** Stop the responder and wait for its task to exit. Idempotent. */
void dns_captive_stop(void);
