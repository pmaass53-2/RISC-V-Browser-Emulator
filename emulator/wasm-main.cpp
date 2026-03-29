#include <emscripten.h>
#include <cstdint>

#include "cpu.hpp"
#include "bus.hpp"

extern "C" EMSCRIPTEN_KEEPALIVE
void emulator_init() {
}
extern "C" EMSCRIPTEN_KEEPALIVE
void emulator_step(int cycles) {
    while (cycles--) {
        // tick
    }
}