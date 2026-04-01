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
    for (int i = 0; i < 1000; i++) {
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
    }
    EMSCRIPTEN_KEEPALIVE
    void load_dtb(uint8_t* buffer, int size) {
        uint32_t dtb_addr = 0x81000000;
        bus.ram->load(buffer, size, dtb_addr - 0x80000000);
        cpu.reset(0x80000000);
        // Set boot registers for OpenSBI
        cpu.reg_file[10] = 0;          // a0 = hartid
        cpu.reg_file[11] = dtb_addr; // a1 = dtb address (FW_PAYLOAD_FDT_OFFSET = 0x2200000)
        is_running = true;
        printf("ROM loaded successfully. Starting CPU...\n");
    }
    EMSCRIPTEN_KEEPALIVE
    void uart_push_byte(uint8_t byte) {
        uart.rx_push(byte);
    }

    EMSCRIPTEN_KEEPALIVE
    void set_debug(bool enable) {
        cpu.debug_mode = enable;
        printf("Debug mode %s\n", enable ? "enabled" : "disabled");
    }
}

int main() {
    printf("WASM Emulator Initialized. Waiting for ROM...\n");
    emscripten_set_main_loop(main_loop, 0, 0);
    return 0;
}