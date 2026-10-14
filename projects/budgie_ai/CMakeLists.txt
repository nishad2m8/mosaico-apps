cmake_minimum_required(VERSION 3.16)
set(MOSAICO_UTILS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../submodule/esp-mosaico-utils")
set(MOSAICO_BSP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../submodule/esp-mosaico-bsp")
if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
endif()
if(NOT SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.application.defaults")
endif()
# The UI is LVGL (ui/budgie_ui, wrapped by components/budgie_ui), so the
# template's GSP bundle/mirror components are not used. Vibe Mode integration
# (esp_mosaico_app_recovery) is kept as required by the retained contract.
list(APPEND EXTRA_COMPONENT_DIRS
    "${MOSAICO_UTILS_ROOT}/esp-mosaico-recovery/components/esp_mosaico_app_recovery")
include("${MOSAICO_UTILS_ROOT}/esp-mosaico-recovery/cmake/mosaico_idf_project.cmake")
project(budgie_ai VERSION 1.0.0)
include("${MOSAICO_UTILS_ROOT}/mosaico-tools/cmake/system_update.cmake")
