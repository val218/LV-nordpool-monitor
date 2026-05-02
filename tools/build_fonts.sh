#!/usr/bin/env bash
# tools/build_fonts.sh
#
# Generates LVGL font C files from a TrueType font, with character ranges
# covering Latin (ASCII), Latin Extended-A (Latvian diacritics), Latin-1
# Supplement, and Cyrillic (Russian).
#
# Output: src/fonts/np_font_*.c
#
# Requirements:
#   - Node.js >= 14
#   - lv_font_conv (installed automatically if missing)
#   - curl
#
# Re-run any time the character set needs to change. CI runs this
# automatically before each build.

set -euo pipefail

# --- Where to write output -----------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
OUT_DIR="$ROOT_DIR/src/fonts"
mkdir -p "$OUT_DIR"

# --- Pull lv_font_conv if not present ------------------------------------
if ! command -v lv_font_conv &>/dev/null; then
    echo "[fonts] installing lv_font_conv..."
    npm install -g lv_font_conv
fi

# --- Get the source TTF --------------------------------------------------
TTF_DIR="$SCRIPT_DIR/.cache"
mkdir -p "$TTF_DIR"
TTF_REGULAR="$TTF_DIR/NotoSans-Regular.ttf"
TTF_BOLD="$TTF_DIR/NotoSans-Bold.ttf"

# Noto Sans is Apache-2.0 — fine for embedding.
# Pulled from Google Fonts CDN on first run, cached afterwards.
if [ ! -f "$TTF_REGULAR" ]; then
    echo "[fonts] downloading NotoSans-Regular.ttf..."
    curl -sSL -o "$TTF_REGULAR" \
        https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf
fi
if [ ! -f "$TTF_BOLD" ]; then
    echo "[fonts] downloading NotoSans-Bold.ttf..."
    curl -sSL -o "$TTF_BOLD" \
        https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-Bold.ttf
fi

# --- Character ranges ----------------------------------------------------
# 0x20-0x7E   Basic Latin (printable ASCII)
# 0xA0-0xFF   Latin-1 Supplement (degree sign, ¢, etc.)
# 0x100-0x17F Latin Extended-A (Latvian diacritics: ā ē ī ū č ģ ķ ļ ņ š ž)
# 0x400-0x4FF Cyrillic
# 0x2010-0x2015, 0x2018-0x201F  punctuation (en/em dash, smart quotes)
# 0x2022      bullet
RANGES="0x20-0x7E,0xA0-0xFF,0x100-0x17F,0x400-0x4FF,0x2010-0x2015,0x2018-0x201F,0x2022"

# --- Sizes we generate ---------------------------------------------------
# Match what ui.cpp uses: 12, 14, 16, 20, 24, 28, 40, 48
# Skipped 18, 22, 26, 30, 32, 34, 36, 38, 42, 44, 46 — not used.
SIZES_REGULAR=(12 14 16 20 24 28 40 48)

# --- Generate ------------------------------------------------------------
for sz in "${SIZES_REGULAR[@]}"; do
    OUT="$OUT_DIR/np_font_${sz}.c"
    echo "[fonts] generating np_font_${sz}.c ..."
    lv_font_conv \
        --bpp 4 \
        --size "$sz" \
        --no-compress \
        --font "$TTF_REGULAR" \
        -r "$RANGES" \
        --format lvgl \
        --no-prefilter \
        -o "$OUT"
done

echo "[fonts] done — wrote $(ls "$OUT_DIR" | wc -l) files to $OUT_DIR"
