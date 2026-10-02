#!/bin/sh
# Builds the web (WebAssembly) version of the game with Emscripten.
# Usage: sh web/build.sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LIBS_DIR="$SCRIPT_DIR/wasm-libs"
STATIC_DIR="$SCRIPT_DIR/static"
OUT_DIR="$SCRIPT_DIR/dist"

. "$SCRIPT_DIR/emenv.sh"

mkdir -p "$OUT_DIR"

em++ \
    -std=c++20 \
    -O2 \
    -I"$LIBS_DIR/include" \
    "$PROJECT_DIR/BettingSquare.cpp" \
    "$PROJECT_DIR/Card.cpp" \
    "$PROJECT_DIR/Game.cpp" \
    "$PROJECT_DIR/Hand.cpp" \
    "$PROJECT_DIR/mina.cpp" \
    "$PROJECT_DIR/Person.cpp" \
    "$PROJECT_DIR/Table.cpp" \
    "$LIBS_DIR/lib/libSDL3_image.a" \
    "$LIBS_DIR/lib/libSDL3.a" \
    "$LIBS_DIR/lib/libpng16.a" \
    "$LIBS_DIR/lib/libzlibstatic.a" \
    --preload-file "$PROJECT_DIR/Cards.png@Cards.png" \
    --preload-file "$PROJECT_DIR/Table5Player.png@Table5Player.png" \
    --preload-file "$PROJECT_DIR/Arrow.png@Arrow.png" \
    --preload-file "$PROJECT_DIR/Chips.png@Chips.png" \
    $(for f in "$PROJECT_DIR"/sounds/*.wav; do printf -- '--preload-file %s@%s ' "$f" "$(basename "$f")"; done) \
    --shell-file "$SCRIPT_DIR/shell.html" \
    -sUSE_WEBGL2=1 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sASYNCIFY=0 \
    -sEXIT_RUNTIME=0 \
    -sEXPORTED_RUNTIME_METHODS=ccall \
    -o "$OUT_DIR/index.html"

# PWA files: manifest + icons are copied as-is; the service worker gets a
# fresh cache-version stamp each build so returning-online installs pick up
# the update instead of being stuck on a stale cached copy forever.
cp "$STATIC_DIR/manifest.json" "$OUT_DIR/manifest.json"
cp "$STATIC_DIR/icon-192.png" "$OUT_DIR/icon-192.png"
cp "$STATIC_DIR/icon-512.png" "$OUT_DIR/icon-512.png"
cp "$STATIC_DIR/social-card.png" "$OUT_DIR/social-card.png"

CACHE_VERSION="$(date +%Y%m%d%H%M%S)"
sed "s/__CACHE_VERSION__/$CACHE_VERSION/" "$STATIC_DIR/sw.js.template" > "$OUT_DIR/sw.js"

echo "Built: $OUT_DIR/index.html (cache version $CACHE_VERSION)"
