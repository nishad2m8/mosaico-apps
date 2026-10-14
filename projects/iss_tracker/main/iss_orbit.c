// SPDX-License-Identifier: Apache-2.0

#include "iss_orbit.h"

#include <math.h>

#define DEG2RAD           (M_PI / 180.0)
#define RAD2DEG           (180.0 / M_PI)
#define WGS84_A_KM        6378.137
#define WGS84_F           (1.0 / 298.257223563)
#define GEODETIC_ITER     8
#define SECONDS_PER_HOUR  3600.0
#define MS_PER_DAY        86400000.0
#define JD_UNIX_EPOCH     2440587.5
#define JD_J2000          2451545.0

static double wrap180(double deg)
{
    deg = fmod(deg + 180.0, 360.0);
    return (deg < 0.0 ? deg + 360.0 : deg) - 180.0;
}

sgp4_err_t iss_orbit_fix(const sgp4_record_t *rec, int64_t unix_ms, iss_orbit_fix_t *out)
{
    sgp4_state_t st;
    const sgp4_err_t err = sgp4_propagate(rec, unix_ms, &st);
    if (err != SGP4_OK) {
        return err;
    }
    const double e2 = WGS84_F * (2.0 - WGS84_F);
    const double x = st.r[0], y = st.r[1], z = st.r[2];
    const double rxy = sqrt(x * x + y * y);
    double lat = atan2(z, rxy), c = 1.0;
    for (int i = 0; i < GEODETIC_ITER; ++i) {
        const double s = sin(lat);
        c = 1.0 / sqrt(1.0 - e2 * s * s);
        lat = atan2(z + WGS84_A_KM * c * e2 * s, rxy);
    }
    out->lat_deg = lat * RAD2DEG;
    out->lon_deg = wrap180((atan2(y, x) - sgp4_gmst(unix_ms)) * RAD2DEG);
    out->alt_km = rxy / cos(lat) - WGS84_A_KM * c;
    out->speed_kmh = sqrt(st.v[0] * st.v[0] + st.v[1] * st.v[1] + st.v[2] * st.v[2]) * SECONDS_PER_HOUR;
    return SGP4_OK;
}

void iss_orbit_subsolar(int64_t unix_ms, double *lat_deg, double *lon_deg)
{
    const double n = (double)unix_ms / MS_PER_DAY + JD_UNIX_EPOCH - JD_J2000;
    const double mean_lon = 280.460 + 0.9856474 * n;
    const double g = (357.528 + 0.9856003 * n) * DEG2RAD;
    const double ecl = (mean_lon + 1.915 * sin(g) + 0.020 * sin(2.0 * g)) * DEG2RAD;
    const double obliq = (23.439 - 0.0000004 * n) * DEG2RAD;
    const double dec = asin(sin(obliq) * sin(ecl));
    const double ra = atan2(cos(obliq) * sin(ecl), cos(ecl));
    *lat_deg = dec * RAD2DEG;
    *lon_deg = wrap180((ra - sgp4_gmst(unix_ms)) * RAD2DEG);
}
