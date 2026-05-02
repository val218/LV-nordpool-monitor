"""
build_fonts_sources.py - PlatformIO pre-build extra script that explicitly
compiles every .c file under src/fonts/ into the firmware.

Why this exists: PlatformIO's default `build_src_filter = +<*>` is supposed
to recurse into subdirectories, but in practice it sometimes skips files
under src/fonts/ — especially when those files are generated AFTER PIO
has already cached its source-file list. We hit this exact failure mode
in CI: the .c files were on disk and validated, but PIO never compiled
them, so the linker couldn't find np_font_* / np_icons_* symbols.

This script bypasses LDF / src_filter entirely by directly handing the .c
files to SCons via env.BuildSources(). The resulting object files are
linked into firmware.elf the same way as everything else.
"""
import os
import glob

Import("env")

PROJECT_DIR = env.subst("$PROJECT_DIR")
FONTS_DIR   = os.path.join(PROJECT_DIR, "src", "fonts")
BUILD_DIR   = os.path.join(env.subst("$BUILD_DIR"), "fonts")

if not os.path.isdir(FONTS_DIR):
    print("[build_fonts_sources] No src/fonts/ directory; skipping.")
else:
    c_files = sorted(glob.glob(os.path.join(FONTS_DIR, "*.c")))
    if not c_files:
        print("[build_fonts_sources] src/fonts/ exists but is empty.")
    else:
        print(f"[build_fonts_sources] Found {len(c_files)} font source files:")
        for f in c_files:
            print(f"    {os.path.relpath(f, PROJECT_DIR)}")
        # env.BuildSources(build_dir, source_dir, src_filter)
        env.BuildSources(BUILD_DIR, FONTS_DIR, "+<*.c>")
        print(f"[build_fonts_sources] Registered {len(c_files)} files for compilation.")

