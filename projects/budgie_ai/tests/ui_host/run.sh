#!/bin/bash
# Host checks for the Budgie UI (no device needed).
#   ./tests/ui_host/run.sh          interaction checks + text_ascii unit test
#   ./tests/ui_host/run.sh render   also write PNG screenshots to build-host/ui/
# Uses the LVGL 9.5.0 sources fetched by the firmware build (managed_components),
# so run an ESP-IDF build (or reconfigure) once first.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$(cd "$HERE/../.." && pwd)"
LVGL="$PROJ/managed_components/lvgl__lvgl"
UI="$PROJ/ui/budgie_ui"
OUT="$PROJ/build-host/ui"
[ -d "$LVGL/src" ] || { echo "LVGL sources missing: build the firmware once first"; exit 1; }
mkdir -p "$OUT/obj"

CF=(-O1 -w -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE "-I$HERE" "-I$LVGL" "-I$LVGL/src" "-I$UI" "-I$UI/src")
if [ ! -f "$OUT/obj/.lvgl_built" ]; then
    echo "Compiling LVGL (first run only)..."
    (cd "$OUT/obj" && find "$LVGL/src" -name '*.c' | xargs -P 8 -n 25 cc "${CF[@]}" -c && touch .lvgl_built)
fi
UISRC=$(grep -o '${CMAKE_CURRENT_LIST_DIR}/[^ )]*\.c' "$UI/file_list_gen.cmake" | sed "s|\${CMAKE_CURRENT_LIST_DIR}|$UI|")
mkdir -p "$OUT/ui_obj" && (cd "$OUT/ui_obj" && rm -f ./*.o && cc "${CF[@]}" -c $UISRC "$UI/src/budgie_chat.c")

cc "${CF[@]}" "$HERE/interact.c" "$OUT"/obj/*.o "$OUT"/ui_obj/*.o -o "$OUT/interact"
cc -Wall -Wextra "-I$PROJ/main" "$PROJ/main/text_ascii.c" "$HERE/test_text_ascii.c" -o "$OUT/test_text_ascii"

# Camera screen logic (main/app_ui_camera.c) with a fake camera; stubs/ stands in for ESP-IDF headers
CAM_INC=("-I$HERE/stubs" "-I$PROJ/main" "-I$PROJ/components/camera_io/include"
         "-I$PROJ/components/audio_io/include" "-I$PROJ/components/wifi_manager/include"
         "-I$PROJ/components/ai_client/include" "-I$PROJ/components/app_config/include")
cc "${CF[@]}" "${CAM_INC[@]}" "$HERE/camera_flow.c" "$PROJ/main/app_ui_camera.c" \
    "$OUT"/obj/*.o "$OUT"/ui_obj/*.o -o "$OUT/camera_flow"
# Reply bubble scroll + popup (main/app_ui_reply.c), tapped with a pointer
cc "${CF[@]}" "${CAM_INC[@]}" "$HERE/reply_flow.c" "$PROJ/main/app_ui_reply.c" \
    "$OUT"/obj/*.o "$OUT"/ui_obj/*.o -o "$OUT/reply_flow"
# History screen (main/app_ui_history.c) against a fake history store
cc "${CF[@]}" "${CAM_INC[@]}" "$HERE/history_flow.c" "$PROJ/main/app_ui_history.c" "$PROJ/main/app_ui_reply.c" \
    "$PROJ/main/text_ascii.c" "$OUT"/obj/*.o "$OUT"/ui_obj/*.o -o "$OUT/history_flow"

"$OUT/test_text_ascii"
"$OUT/interact" 2>&1 | grep -v '^\[Warn\]\|^Initializing'
"$OUT/camera_flow" "$OUT/camera_preview.raw" 2>&1 | grep -v '^\[Warn\]\|^Initializing'
"$OUT/reply_flow" 2>&1 | grep -v '^\[Warn\]\|^Initializing'
"$OUT/history_flow" 2>&1 | grep -v '^\[Warn\]\|^Initializing'

if [ "${1:-}" = "render" ]; then
    cc "${CF[@]}" "${CAM_INC[@]}" "$HERE/render.c" "$PROJ/main/app_ui_reply.c" "$OUT"/obj/*.o "$OUT"/ui_obj/*.o -o "$OUT/render"
    (cd "$OUT" && ./render >/dev/null 2>&1 && python3 "$HERE/topng.py" ./*.raw && rm -f ./*.raw && ls ./*.png)
fi
