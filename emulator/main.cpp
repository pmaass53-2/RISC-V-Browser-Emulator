#include <cstdint>
#include <iostream>
#include <emscripten.h>

#include "clint.hpp"
#include "plic.hpp"
#include "uart.hpp"
#include "ram.hpp"
#include "bus.hpp"
#include "cpu.hpp"

CLINT clint;
PLIC plic;
UART uart(&plic);
RAM ram;
Bus bus(&clint, &plic, &uart, &ram);
CPU cpu(&bus, 0x80000000);

bool is_running = false;

void main_loop() {
    if (!is_running) return;
    // Run a batch of instructions per frame (e.g., 10,000) 
    // to keep the emulator fast while letting the browser breathe.
    for (int i = 0; i < 10000; i++) {
        cpu.tick();
        if (cpu.mcycle % 10 == 0) {
            bus.clint->tick();
        }
    }
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void load_rom(uint8_t* buffer, int size) {
        // Load the bytes into RAM
        bus.ram->load(buffer, size);
        cpu.reset(0x80000000);
        is_running = true;
        printf("ROM loaded successfully. Starting CPU...\n");
    }
}

int main() {
    printf("WASM Emulator Initialized. Waiting for ROM...\n");
    emscripten_set_main_loop(main_loop, 0, 0);
    return 0;
}