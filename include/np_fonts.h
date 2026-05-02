// np_fonts.h — declarations for the project's custom LVGL fonts and a
// FONT_NN(n) selector macro that picks the right font at compile time.
//
// When NP_FONTS_AVAILABLE is defined (built by tools/build_fonts.sh and
// linked in), FONT_NN points at the Noto Sans-derived np_font_NN, which
// covers Latin + Latvian + Cyrillic. Otherwise it falls back to LVGL's
// built-in Montserrat fonts (ASCII only — non-ASCII renders as squares).
#pragma once

#include <lvgl.h>

#ifdef NP_FONTS_AVAILABLE

#ifdef __cplusplus
extern "C" {
#endif

LV_FONT_DECLARE(np_font_12);
LV_FONT_DECLARE(np_font_14);
LV_FONT_DECLARE(np_font_16);
LV_FONT_DECLARE(np_font_20);
LV_FONT_DECLARE(np_font_24);
LV_FONT_DECLARE(np_font_28);
LV_FONT_DECLARE(np_font_40);
LV_FONT_DECLARE(np_font_48);

#ifdef __cplusplus
}
#endif

#define FONT_12  (&np_font_12)
#define FONT_14  (&np_font_14)
#define FONT_16  (&np_font_16)
#define FONT_20  (&np_font_20)
#define FONT_24  (&np_font_24)
#define FONT_28  (&np_font_28)
#define FONT_40  (&np_font_40)
#define FONT_48  (&np_font_48)

#else  // ! NP_FONTS_AVAILABLE — Montserrat fallback

#define FONT_12  (&lv_font_montserrat_12)
#define FONT_14  (&lv_font_montserrat_14)
#define FONT_16  (&lv_font_montserrat_16)
#define FONT_20  (&lv_font_montserrat_20)
#define FONT_24  (&lv_font_montserrat_24)
#define FONT_28  (&lv_font_montserrat_28)
#define FONT_40  (&lv_font_montserrat_40)
#define FONT_48  (&lv_font_montserrat_48)

#endif
