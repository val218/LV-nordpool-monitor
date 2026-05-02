// np_fonts_lib.h - tiny header for the np_fonts project-local library.
//
// Including this header from any project source file is what makes
// PlatformIO's Library Dependency Finder (LDF) discover the np_fonts
// library and compile its .c files (the LVGL fonts produced by
// lv_font_conv) into the firmware.
//
// This header itself does nothing — the real font symbol declarations
// live in include/np_fonts.h. We keep them separate so np_fonts.h can
// be included without forcing the library lookup, but if you need the
// linker to see the font data, include this file from a .cpp source.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Just a sentinel so the header has at least one declaration. The actual
// font symbols (np_font_*, np_icons_*) are declared in np_fonts.h.
extern const char np_fonts_lib_marker;

#ifdef __cplusplus
}
#endif
