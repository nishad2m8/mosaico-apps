// SPDX-License-Identifier: Apache-2.0
//
// Earth-fixed helpers on top of SGP4: sub-satellite point, altitude and speed,
// plus the sub-solar point for the day/night terminator. Stateless.
#pragma once

#include <stdint.h>

#include "sgp4.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double lat_deg;     /* geodetic latitude, WGS-84 */
    double lon_deg;     /* -180..180 */
    double alt_km;      /* height above the WGS-84 ellipsoid */
    double speed_kmh;   /* inertial speed */
} iss_orbit_fix_t;

/** Sub-satellite point of @p rec at @p unix_ms. */
sgp4_err_t iss_orbit_fix(const sgp4_record_t *rec, int64_t unix_ms, iss_orbit_fix_t *out);

/** Point where the Sun is overhead at @p unix_ms (low-precision almanac, ~0.01 deg). */
void iss_orbit_subsolar(int64_t unix_ms, double *lat_deg, double *lon_deg);

#ifdef __cplusplus
}
#endif
