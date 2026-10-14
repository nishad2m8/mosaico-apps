/**
 * @file settings_slider_gen.h
 */

#ifndef LVGL_PRO_SETTINGS_SLIDER_GEN_H
#define LVGL_PRO_SETTINGS_SLIDER_GEN_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
    #include "lvgl_private.h"
#else
    #include "lvgl/lvgl.h"
    #include "lvgl/lvgl_private.h"
#endif

#if defined(LV_USE_XML) && LV_USE_XML
    #include "lv_xml/lv_xml.h"
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

lv_obj_t * settings_slider_create(lv_obj_t * parent, const char * title, lv_subject_t * subject, const char * fmt, int32_t min, int32_t max);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LVGL_PRO_SETTINGS_SLIDER_GEN_H*/