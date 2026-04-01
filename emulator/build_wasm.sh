#!/usr/bin/env bash
set -e

# add new include files here
SOURCES="main.cpp cpu.cpp"

# all exported functions (extern "C" EMSCRIPTEN_KEEPALIVE)
EXPORTS='["_load_rom", "_main", "_malloc", "_free", "_uart_push_byte", "_set_debug", "_load_dtb"]'

# optimized build command
em++ -O3 \
    -std=c++20 \
    -s EXPORTED_FUNCTIONS="$EXPORTS" \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","HEAPU8"]' \
    -s ALLOW_MEMORY_GROWTH=1 \
    -msimd128 \
    -s ENVIRONMENT=web \
    -s MODULARIZE=0 \
    -flto \
    $SOURCES \
    -o emulator.js
echo "Build Complete"