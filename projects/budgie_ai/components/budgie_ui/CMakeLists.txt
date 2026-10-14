# ESP-IDF wrapper for the LVGL Pro project in ui/budgie_ui.
#
# The UI is designed and exported with the LVGL Editor (XML in ui/budgie_ui,
# generated *_gen.c next to it). This component only compiles that export, so
# re-exporting from the Editor needs no change here. The Editor's own
# CMakeLists.txt in ui/budgie_ui is used by its desktop simulator only.
set(BUDGIE_UI_DIR "${CMAKE_CURRENT_LIST_DIR}/../../ui/budgie_ui")

set(LV_EDITOR_PROJECT_SOURCES)
include("${BUDGIE_UI_DIR}/user_config.cmake")      # hand-written sources (src/)
include("${BUDGIE_UI_DIR}/file_list_gen.cmake")    # Editor-generated sources

idf_component_register(
    SRCS ${LV_EDITOR_PROJECT_SOURCES}
    INCLUDE_DIRS "${BUDGIE_UI_DIR}" "${BUDGIE_UI_DIR}/src"
    REQUIRES lvgl__lvgl)

# LV_LVGL_H_INCLUDE_SIMPLE: include "lvgl.h" (ESP-IDF component layout).
# BUDGIE_CHAT_EXTERNAL_SEND: the application implements budgie_chat_on_send().
target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_LVGL_H_INCLUDE_SIMPLE BUDGIE_CHAT_EXTERNAL_SEND)
# Editor output is kept verbatim; silence warnings that are not ours to fix.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function)
