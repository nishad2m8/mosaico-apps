// SPDX-License-Identifier: Apache-2.0
//
// Read-only world outlines for the globe (Natural Earth 1:110m via world-atlas).
// Points are {longitude, latitude} in 1/scale degree; polyline i spans
// points[starts[i]] .. points[starts[i + 1] - 1]. Country rings that cross the
// antimeridian use longitudes in 0..360 instead of -180..180.
#pragma once

#include <stdint.h>

typedef struct {
    const int16_t (*points)[2];
    const uint32_t *starts;
    uint32_t polyline_count;
    uint32_t point_count;
    uint16_t scale;
} world_map_set_t;

typedef struct {
    const char *name;          /* UTF-8 */
    uint16_t first_ring;       /* index into world_map_country_rings */
    uint16_t ring_count;
} world_map_country_t;

extern const world_map_set_t world_map_coast;
extern const world_map_set_t world_map_borders;
extern const world_map_set_t world_map_country_rings;
extern const world_map_country_t world_map_countries[];
extern const uint32_t world_map_country_count;

/** Name of the country containing the point, or NULL over water. */
const char *world_map_country_at(double lat_deg, double lon_deg);
