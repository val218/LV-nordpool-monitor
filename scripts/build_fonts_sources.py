"""
build_fonts_sources.py - PlatformIO pre-build extra script.

Why this exists:
  Empirically, in our environment (espressif32 6.10 + framework=arduino),
  PlatformIO's source scanner only picks up .cpp files in src/ — pure .c
  files are silently ignored, even though the documented default
  build_src_filter is "+<*>". This is reported by other users on the
  PIO community forum.

  The LVGL fonts produced by lv_font_conv are .c files. We can't simply
  rename them to .cpp because lv_font_conv emits out-of-order designated
  initializers (e.g. {.bitmap_index=0, .adv_w=99, .box_h=0, .box_w=0...})
  which g++ rejects but gcc accepts. So they MUST be compiled as C.

  Solution: hand the .c files directly to SCons via env.BuildSources().
  This is the documented PIO approach for arbitrary external sources
  (https://docs.platformio.org/en/latest/scripting/examples/external_sources.html).
"""
import os
import glob

Import("env")

PROJECT_DIR = env.subst("$PROJECT_DIR")
SRC_DIR     = os.path.join(PROJECT_DIR, "src")
BUILD_DIR   = os.path.join(env.subst("$BUILD_DIR"), "fonts_extra")

# Find every np_font_*.c and np_icons_*.c at the top of src/
patterns = ["np_font_*.c", "np_icons_*.c"]
files = []
for p in patterns:
    files.extend(sorted(glob.glob(os.path.join(SRC_DIR, p))))

print("=" * 60)
print("[build_fonts_sources] explicit C font registration")
print("=" * 60)
print(f"[build_fonts_sources] PROJECT_DIR = {PROJECT_DIR}")
print(f"[build_fonts_sources] SRC_DIR     = {SRC_DIR}")
print(f"[build_fonts_sources] BUILD_DIR   = {BUILD_DIR}")

if not files:
    print("[build_fonts_sources] WARNING: no font .c files found!")
    print("[build_fonts_sources] Listing src/ contents:")
    try:
        for f in sorted(os.listdir(SRC_DIR)):
            print(f"    {f}")
    except OSError as e:
        print(f"    (cannot list: {e})")
else:
    print(f"[build_fonts_sources] Found {len(files)} font source file(s):")
    for f in files:
        print(f"    {os.path.relpath(f, PROJECT_DIR)}  ({os.path.getsize(f)} bytes)")

    # Build a src_filter that explicitly includes only our font files.
    # Using just basenames because env.BuildSources() interprets the filter
    # relative to the source directory passed as the second arg.
    filter_parts = ["+<{}>".format(os.path.basename(f)) for f in files]
    src_filter = " ".join(filter_parts)
    print(f"[build_fonts_sources] src_filter = {src_filter}")

    env.BuildSources(BUILD_DIR, SRC_DIR, src_filter)
    print(f"[build_fonts_sources] Registered {len(files)} files for compilation.")
print("=" * 60)
