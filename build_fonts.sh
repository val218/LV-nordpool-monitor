#!/usr/bin/env bash
# tools/build_fonts.sh
#
# Generates LVGL font C files from a TrueType font, with character ranges
# covering Latin (ASCII), Latin Extended-A (Latvian diacritics), Latin-1
# Supplement, and Cyrillic (Russian).
#
# Output: src/fonts/np_font_*.c — one file per size, each defining a
#         single LVGL font symbol named np_font_NN.
#
# Requirements:
#   - Node.js >= 14
#   - lv_font_conv (installed automatically if missing)
#   - curl

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
OUT_DIR="$ROOT_DIR/src/fonts"
mkdir -p "$OUT_DIR"

# --- Tooling -------------------------------------------------------------
if ! command -v lv_font_conv &>/dev/null; then
    echo "[fonts] installing lv_font_conv..."
    npm install -g lv_font_conv
fi
echo "[fonts] lv_font_conv: $(command -v lv_font_conv)"
echo "[fonts] lv_font_conv version: $(lv_font_conv --version 2>&1 | head -1)"

# --- Source TTF ---------------------------------------------------------
TTF_DIR="$SCRIPT_DIR/.cache"
mkdir -p "$TTF_DIR"
TTF_REGULAR="$TTF_DIR/NotoSans-Regular.ttf"

# Download with verification — fail fast if the URL is dead or returns HTML.
download_ttf() {
    local url="$1" dest="$2"
    echo "[fonts] downloading $(basename "$dest")..."
    curl -sSL --fail -o "$dest" "$url"
    local sz
    sz=$(stat -c%s "$dest" 2>/dev/null || stat -f%z "$dest")
    if [ "$sz" -lt 100000 ]; then
        echo "[fonts] ERROR: $dest is only $sz bytes — download failed"
        head -c 200 "$dest"
        rm -f "$dest"
        return 1
    fi
    echo "[fonts] OK: $sz bytes"
}

if [ ! -s "$TTF_REGULAR" ]; then
    download_ttf \
        "https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf" \
        "$TTF_REGULAR"
fi

# --- Character ranges ---------------------------------------------------
# 0x20-0x7E   Basic Latin
# 0xA0-0xFF   Latin-1 Supplement
# 0x100-0x17F Latin Extended-A (Latvian diacritics)
# 0x400-0x4FF Cyrillic (Russian)
# 0x2010-0x2015, 0x2018-0x201F  typographic punctuation
# 0x2022      bullet
RANGES="0x20-0x7E,0xA0-0xFF,0x100-0x17F,0x400-0x4FF,0x2010-0x2015,0x2018-0x201F,0x2022"

# --- Sizes --------------------------------------------------------------
# Match what ui.cpp uses.
SIZES=(12 14 16 20 24 28 40 48)

# --- Generate -----------------------------------------------------------
# Important: each font is a separate run with --font + --range. The default
# C symbol name is the output filename's stem, so np_font_NN.c → np_font_NN.
for sz in "${SIZES[@]}"; do
    SYM="np_font_${sz}"
    OUT="$OUT_DIR/${SYM}.c"
    echo "[fonts] generating ${SYM}.c ..."
    lv_font_conv \
        --bpp 4 \
        --size "$sz" \
        --no-compress \
        --no-prefilter \
        --font "$TTF_REGULAR" -r "$RANGES" \
        --format lvgl \
        --lv-include "lvgl.h" \
        -o "$OUT"

    # Validate output: file should be non-empty and contain the symbol name.
    if [ ! -s "$OUT" ]; then
        echo "[fonts] ERROR: ${OUT} was not created or is empty"
        exit 1
    fi
    if ! grep -q "${SYM}" "$OUT"; then
        echo "[fonts] ERROR: ${OUT} does not contain symbol '${SYM}'"
        echo "        First 20 lines:"
        head -20 "$OUT"
        exit 1
    fi
    echo "[fonts]   OK: $(wc -c < "$OUT") bytes, symbol '${SYM}' found"
done

echo "[fonts] done — generated ${#SIZES[@]} font files in $OUT_DIR"
ls -la "$OUT_DIR"
