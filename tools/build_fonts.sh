#!/usr/bin/env bash
# tools/build_fonts.sh
#
# Generates LVGL font C files for:
#  - Text fonts (Noto Sans): English, Latvian, Russian — sizes 12/14/16/20/24/28/40/48
#  - Icon font (FontAwesome 5 Free Solid): appliance glyphs — sizes 24 and 40
#
# Output: src/fonts/np_font_NN.c and src/fonts/np_icons_NN.c
#
# Both fonts must build successfully or CI fails. The defines
# NP_FONTS_AVAILABLE and NP_ICONS_AVAILABLE in platformio.ini tell the
# C++ code to use these instead of LVGL's built-in Montserrat (ASCII only)
# and ASCII fallback labels.
#
# Requirements:
#   - Node.js >= 14
#   - lv_font_conv (auto-installed if missing)
#   - curl

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
# Output goes into src/fonts/ — picked up by both PIO's default source
# scanner AND by the pre-build script scripts/build_fonts_sources.py.

OUT_DIR="$ROOT_DIR/src/fonts"
mkdir -p "$OUT_DIR"

# --- Tooling -------------------------------------------------------------
if ! command -v lv_font_conv &>/dev/null; then
    echo "[fonts] installing lv_font_conv..."
    npm install -g lv_font_conv@1.5.3
fi
echo "[fonts] lv_font_conv: $(command -v lv_font_conv)"
echo "[fonts] lv_font_conv version: $(lv_font_conv --version 2>&1 | head -1)"

# --- Source TTFs --------------------------------------------------------
TTF_DIR="$SCRIPT_DIR/.cache"
mkdir -p "$TTF_DIR"
TTF_TEXT="$TTF_DIR/NotoSans-Regular.ttf"
TTF_ICONS="$TTF_DIR/fa-solid-900.ttf"

download_ttf() {
    local url="$1" dest="$2"
    echo "[fonts] downloading $(basename "$dest") from $url"
    curl -sSL --fail -o "$dest" "$url"
    local sz
    sz=$(stat -c%s "$dest" 2>/dev/null || stat -f%z "$dest")
    if [ "$sz" -lt 50000 ]; then
        echo "[fonts] ERROR: $dest is only $sz bytes - download failed"
        head -c 200 "$dest"
        rm -f "$dest"
        return 1
    fi
    echo "[fonts] OK: $sz bytes"
}

if [ ! -s "$TTF_TEXT" ]; then
    download_ttf \
        "https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf" \
        "$TTF_TEXT"
fi

if [ ! -s "$TTF_ICONS" ]; then
    # FontAwesome 5 Free 5.15.4 - last version under unified OFL+MIT licensing
    # before they split features. Pinned to a release tag to keep CI stable.
    download_ttf \
        "https://github.com/FortAwesome/Font-Awesome/raw/5.15.4/webfonts/fa-solid-900.ttf" \
        "$TTF_ICONS"
fi

# --- Validate output helper ---------------------------------------------
validate_font() {
    local out="$1" sym="$2"
    if [ ! -s "$out" ]; then
        echo "[fonts] ERROR: ${out} not created or empty"
        exit 1
    fi
    if ! grep -qE "lv_font_t[[:space:]]+${sym}[[:space:]]*=" "$out"; then
        echo "[fonts] ERROR: ${out} does not export 'const lv_font_t ${sym}'"
        echo "        Searching for any lv_font_t exports:"
        grep -nE "lv_font_t[[:space:]]+[a-zA-Z_]+[[:space:]]*=" "$out" || \
            echo "        (none)"
        echo "        Last 20 lines:"
        tail -20 "$out"
        exit 1
    fi
    echo "[fonts]   OK: $(wc -c < "$out") bytes, symbol '${sym}' exported"
}

# --- Generate text fonts ------------------------------------------------
# Character ranges:
#   0x20-0x7E   Basic Latin
#   0xA0-0xFF   Latin-1 Supplement
#   0x100-0x17F Latin Extended-A (Latvian diacritics)
#   0x400-0x4FF Cyrillic (Russian)
#   0x2010-0x2015, 0x2018-0x201F   typographic punctuation
#   0x2022      bullet
TEXT_RANGES="0x20-0x7E,0xA0-0xFF,0x100-0x17F,0x400-0x4FF,0x2010-0x2015,0x2018-0x201F,0x2022"
TEXT_SIZES=(12 14 16 20 24 28 40 48)

echo ""
echo "===== TEXT FONTS ====="
for sz in "${TEXT_SIZES[@]}"; do
    SYM="np_font_${sz}"
    OUT="$OUT_DIR/${SYM}.c"
    echo "[fonts] generating ${SYM}.c ..."
    lv_font_conv \
        --bpp 4 \
        --size "$sz" \
        --no-compress \
        --no-prefilter \
        --font "$TTF_TEXT" -r "$TEXT_RANGES" \
        --format lvgl \
        --lv-include "lvgl.h" \
        -o "$OUT"
    validate_font "$OUT" "$SYM"
done

# --- Generate icon font -------------------------------------------------
# FontAwesome 5 Free Solid codepoints. Keep these in sync with the
# g_iconCatalog table in src/relay_icons.cpp.
#
#   f1e6  plug              (socket)
#   f0eb  lightbulb         (bulb)
#   f06d  fire              (heater)
#   f2cc  shower            (water heater)
#   f2dc  snowflake         (AC)
#   f863  fan
#   f2cb  temperature-low   (fridge)
#   f553  tshirt            (washer / iron)
#   f72e  wind              (dryer)
#   f2e7  utensils          (dishwasher)
#   f7ec  bread-slice       (oven)
#   f0c8  square            (microwave)
#   f0f4  coffee            (kettle)
#   f0e7  bolt              (charger / EV)
#   f043  tint              (pump)
#   f2c9  thermometer-half  (thermostat)
#   f26c  tv
#   f108  desktop           (computer)
#   f185  sun               (lighting)
#   f111  circle            (generic)
ICON_GLYPHS="0xf1e6,0xf0eb,0xf06d,0xf2cc,0xf2dc,0xf863,0xf2cb,0xf553,0xf72e,0xf2e7,0xf7ec,0xf0c8,0xf0f4,0xf0e7,0xf043,0xf2c9,0xf26c,0xf108,0xf185,0xf111"
ICON_SIZES=(24 40)

echo ""
echo "===== ICON FONT ====="
for sz in "${ICON_SIZES[@]}"; do
    SYM="np_icons_${sz}"
    OUT="$OUT_DIR/${SYM}.c"
    echo "[fonts] generating ${SYM}.c ..."
    lv_font_conv \
        --bpp 4 \
        --size "$sz" \
        --no-compress \
        --no-prefilter \
        --font "$TTF_ICONS" -r "$ICON_GLYPHS" \
        --format lvgl \
        --lv-include "lvgl.h" \
        -o "$OUT"
    validate_font "$OUT" "$SYM"
done

echo ""
echo "[fonts] done. Generated files:"
ls -la "$OUT_DIR"
