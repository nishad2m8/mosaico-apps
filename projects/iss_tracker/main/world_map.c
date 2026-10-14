// SPDX-License-Identifier: Apache-2.0

#include "world_map.h"

#include <stdbool.h>
#include <stddef.h>

/* Even-odd ray cast in the plane of one ring. */
static bool ring_contains(uint32_t ring, double lon, double lat)
{
    const world_map_set_t *set = &world_map_country_rings;
    const double s = set->scale;
    bool inside = false;
    const uint32_t a = set->starts[ring], b = set->starts[ring + 1];
    for (uint32_t i = a, j = b - 1; i < b; j = i++) {
        const double xi = set->points[i][0] / s, yi = set->points[i][1] / s;
        const double xj = set->points[j][0] / s, yj = set->points[j][1] / s;
        if ((yi > lat) != (yj > lat) && lon < (xj - xi) * (lat - yi) / (yj - yi) + xi) {
            inside = !inside;
        }
    }
    return inside;
}

const char *world_map_country_at(double lat_deg, double lon_deg)
{
    for (uint32_t c = 0; c < world_map_country_count; ++c) {
        const world_map_country_t *country = &world_map_countries[c];
        for (uint32_t r = 0; r < country->ring_count; ++r) {
            const uint32_t ring = country->first_ring + r;
            /* unwrapped rings span 0..360, so also try the shifted longitude */
            if (ring_contains(ring, lon_deg, lat_deg) ||
                    (lon_deg < 0.0 && ring_contains(ring, lon_deg + 360.0, lat_deg))) {
                return country->name;
            }
        }
    }
    return NULL;
}
