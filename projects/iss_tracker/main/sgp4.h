// SPDX-License-Identifier: Apache-2.0
//
// Near-Earth SGP4 propagator (WGS-72, Vallado/Spacetrack Report #3 model).
// Covers orbits with periods below 225 minutes, such as the ISS; deep-space
// (SDP4) terms are not implemented and such element sets are rejected.
//
// Stateless utility: sgp4_init() fills a caller-owned record that is read-only
// afterwards, so one record may be propagated from several tasks.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SGP4_TLE_LINE_LEN 70   /* characters per TLE line, without line ending */

typedef enum {
    SGP4_OK = 0,
    SGP4_ERR_PARSE,           /* malformed TLE line or bad checksum */
    SGP4_ERR_DEEP_SPACE,      /* period >= 225 min, needs SDP4 */
    SGP4_ERR_ECCENTRICITY,    /* propagated eccentricity out of range */
    SGP4_ERR_DECAYED,         /* propagated radius below Earth's surface */
} sgp4_err_t;

typedef struct {
    int64_t epoch_unix_ms;     /* element set epoch (UTC) */
    /* mean elements, radians and radians/minute */
    double bstar, inclo, nodeo, ecco, argpo, mo, no;
    /* initialized propagation constants */
    double con41, cc1, cc4, cc5, d2, d3, d4, delmo, eta, argpdot, omgcof,
           sinmao, t2cof, t3cof, t4cof, t5cof, x1mth2, x7thm1, mdot,
           nodedot, xlcof, xmcof, nodecf, aycof;
    bool isimp;
} sgp4_record_t;

/** Position (km) and velocity (km/s) in the TEME inertial frame. */
typedef struct {
    double r[3];
    double v[3];
} sgp4_state_t;

/** Parse two TLE lines (trailing whitespace/CR allowed) and initialize @p rec. */
sgp4_err_t sgp4_init(sgp4_record_t *rec, const char *line1, const char *line2);

/** Propagate to @p unix_ms (UTC milliseconds). */
sgp4_err_t sgp4_propagate(const sgp4_record_t *rec, int64_t unix_ms,
                          sgp4_state_t *out);

/** Greenwich mean sidereal time (radians) at @p unix_ms. */
double sgp4_gmst(int64_t unix_ms);

#ifdef __cplusplus
}
#endif
