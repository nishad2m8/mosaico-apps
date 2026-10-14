// SPDX-License-Identifier: Apache-2.0
//
// ISS element set built into the firmware (CelesTrak, NORAD 25544, fetched
// 2026-10-12). Used until a fresh set is downloaded; positions stay within a
// few km for several days and drift slowly after that.
#pragma once

#define ISS_TLE_DEFAULT_LINE1 "1 25544U 98067A   26285.14422154  .00005771  00000+0  11345-3 0  9992"
#define ISS_TLE_DEFAULT_LINE2 "2 25544  51.6314  81.3064 0006731 252.4466 107.5787 15.48833724589888"
