/**
 * @file budgie_ui.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "budgie_ui.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void budgie_ui_init(const char * asset_path)
{
    LV_LOG("Initializing custom C code using LVGL v%d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    budgie_ui_init_gen(asset_path);

    /* Add your own custom code here if needed */
}

/**********************
 *   STATIC FUNCTIONS
 **********************/