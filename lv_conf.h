// lv_conf.h — LVGL v8.4 configuration for the Nordpool Monitor on JC4827W543.
// Trimmed for ESP32-S3 + 480x272 + NV3041A; only the features we actually use
// are turned on so flash and RAM stay reasonable.
#pragma once

#define LV_CONF_INCLUDE_SIMPLE 1

// ---- Color depth ----
// NV3041A speaks RGB565 over QSPI. Don't byte-swap — Arduino_GFX hands LVGL a
// buffer it pushes via QSPI in the correct byte order.
#define LV_COLOR_DEPTH      16
#define LV_COLOR_16_SWAP    0

// ---- Memory ----
// LVGL keeps its own heap for object allocations. ESP32-S3 has plenty of RAM
// even without PSRAM but we want PSRAM for the heavy stuff.
#define LV_MEM_CUSTOM       0
#define LV_MEM_SIZE         (96U * 1024U)
#define LV_MEM_ADR          0
#define LV_MEM_AUTO_DEFRAG  1
#define LV_MEM_BUF_MAX_NUM  16

// ---- HAL ----
#define LV_TICK_CUSTOM      1
#define LV_TICK_CUSTOM_INCLUDE  "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR  (millis())

#define LV_DPI_DEF          130

// ---- Drawing ----
#define LV_DRAW_COMPLEX     1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_CIRCLE_CACHE_SIZE 4

#define LV_DISP_ROT_MAX_BUF (10U * 1024U)

#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_NXP_PXP   0
#define LV_USE_GPU_NXP_VG_LITE 0

// ---- Logging ----
#define LV_USE_LOG          0

// ---- Asserts ----
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0
#define LV_ASSERT_HANDLER_INCLUDE   <stdint.h>
#define LV_ASSERT_HANDLER           while(1);

// ---- Other ----
#define LV_ATTRIBUTE_FAST_MEM
#define LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY
#define LV_USE_USER_DATA            1
#define LV_BIG_ENDIAN_SYSTEM        0
#define LV_USE_PERF_MONITOR         0
#define LV_USE_MEM_MONITOR          0
#define LV_USE_REFR_DEBUG           0

// ---- Compiler ----
#define LV_PRINTF_USE_COROUTINE     0
#define LV_USE_LARGE_COORD          0

// ---- Fonts ----
// Currently using LVGL's built-in Montserrat fonts. These are ASCII-only:
// Latvian and Russian text will render with missing-glyph squares for any
// character outside ASCII. Custom font generation (with Latvian + Cyrillic
// coverage) is in tools/build_fonts.sh but not yet wired in.
#define LV_FONT_MONTSERRAT_8        0
#define LV_FONT_MONTSERRAT_10       0
#define LV_FONT_MONTSERRAT_12       1
#define LV_FONT_MONTSERRAT_14       1
#define LV_FONT_MONTSERRAT_16       1
#define LV_FONT_MONTSERRAT_18       0
#define LV_FONT_MONTSERRAT_20       1
#define LV_FONT_MONTSERRAT_22       0
#define LV_FONT_MONTSERRAT_24       1
#define LV_FONT_MONTSERRAT_26       0
#define LV_FONT_MONTSERRAT_28       1
#define LV_FONT_MONTSERRAT_30       0
#define LV_FONT_MONTSERRAT_32       0
#define LV_FONT_MONTSERRAT_34       0
#define LV_FONT_MONTSERRAT_36       0
#define LV_FONT_MONTSERRAT_38       0
#define LV_FONT_MONTSERRAT_40       1
#define LV_FONT_MONTSERRAT_42       0
#define LV_FONT_MONTSERRAT_44       0
#define LV_FONT_MONTSERRAT_46       0
#define LV_FONT_MONTSERRAT_48       1

#define LV_FONT_DEFAULT             &lv_font_montserrat_14

#define LV_FONT_FMT_TXT_LARGE       0
#define LV_USE_FONT_COMPRESSED      0
#define LV_USE_FONT_SUBPX           0

#define LV_FONT_SUBPX_BGR           0

// ---- Themes ----
#define LV_USE_THEME_DEFAULT        1
#define LV_THEME_DEFAULT_DARK       1
#define LV_THEME_DEFAULT_GROW       1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

#define LV_USE_THEME_BASIC          0
#define LV_USE_THEME_MONO           0

// ---- Layouts ----
#define LV_USE_FLEX                 1
#define LV_USE_GRID                 1

// ---- Widgets ----
#define LV_USE_ARC                  1
#define LV_USE_BAR                  1
#define LV_USE_BTN                  1
#define LV_USE_BTNMATRIX            1
#define LV_USE_CANVAS               0
#define LV_USE_CHECKBOX             1
#define LV_USE_DROPDOWN             1
#define LV_USE_IMG                  1
#define LV_USE_LABEL                1
#define LV_LABEL_TEXT_SELECTION     0
#define LV_LABEL_LONG_TXT_HINT      0

#define LV_USE_LINE                 1
#define LV_USE_ROLLER               0
#define LV_USE_SLIDER               1
#define LV_USE_SWITCH               1
#define LV_USE_TEXTAREA             0
#define LV_USE_TABLE                1

// Extra widgets — these are needed for the chart
#define LV_USE_ANIMIMG              0
#define LV_USE_CALENDAR             0
#define LV_USE_CHART                1
#define LV_USE_COLORWHEEL           0
#define LV_USE_IMGBTN               0
#define LV_USE_KEYBOARD             0
#define LV_USE_LED                  1
#define LV_USE_LIST                 0
#define LV_USE_MENU                 1
#define LV_USE_METER                0
#define LV_USE_MSGBOX               0
#define LV_USE_SPAN                 0
#define LV_USE_SPINBOX              0
#define LV_USE_SPINNER              1
#define LV_USE_TABVIEW              0
#define LV_USE_TILEVIEW             0
#define LV_USE_WIN                  0

// ---- Filesystem / images ----
#define LV_USE_FS_STDIO             0
#define LV_USE_FS_POSIX             0
#define LV_USE_FS_WIN32             0
#define LV_USE_FS_FATFS             0

#define LV_USE_PNG                  0
#define LV_USE_BMP                  0
#define LV_USE_SJPG                 0
#define LV_USE_GIF                  0
#define LV_USE_QRCODE               0

// ---- Others ----
#define LV_USE_SNAPSHOT             0
#define LV_USE_MONKEY               0
#define LV_USE_GRIDNAV              0
#define LV_USE_FRAGMENT             0
#define LV_USE_IMGFONT              0
#define LV_USE_MSG                  0
#define LV_USE_IME_PINYIN           0
