#!/usr/bin/env bash
set -e

# add new include files here
SOURCES="main.cpp"

# all exported functions (extern "C" EMSCRIPTEN_KEEPALIVE)
EXPORTS='["_emulator_init","_emulator_step","_uart_push_byte"]'

# optimized build command
em++ -O3 \
    -std=c++20 \
    -s EXPORTED_FUNCTIONS="$EXPORTS" \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    -s ALLOW_MEMORY_GROWTH=1 \
    -msimd128 \
    -s ENVIRONMENT=web \
    -s MODULARIZE=0 \
    --llvm-lto 1 \
    $SOURCES \
    -o emulator.js
echo "Build Complete"