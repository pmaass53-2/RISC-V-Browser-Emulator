#ifndef BUS_HPP
#define BUS_HPP

#include <cstdint>

#include "clint.hpp"
#include "plic.hpp"
#include "uart.hpp"
#include "ram.hpp"

constexpr uint32_t CLINT_BASE = 0x02000000;
constexpr uint32_t PLIC_BASE = 0x0C000000;
constexpr uint32_t UART_BASE = 0x10000000;
constexpr uint32_t RAM_BASE = 0x80000000;

constexpr uint32_t CLINT_SIZE = 0x10000;
constexpr uint32_t PLIC_SIZE = 0x4000000;
constexpr uint32_t UART_SIZE = 0x100;
constexpr uint32_t RAM_SIZE = 0x04000000; // 64MB

class Bus {
    public:
        // hardware components
        CLINT *clint;
        PLIC *plic;
        UART *uart;
        RAM *ram;
        Bus(CLINT *clint, PLIC *plic, UART *uart, RAM *ram) : clint(clint), plic(plic), uart(uart), ram(ram) {
            // init
        }
        // bus interface
        template<typename T>
        inline T read(uint32_t address) {
            // ram first for performance
            if (address >= RAM_BASE && address < RAM_BASE + RAM_SIZE) {
                uint32_t offset = address - RAM_BASE;
                return ram->read<T>(offset);
            } else if (address >= CLINT_BASE && address < CLINT_BASE + CLINT_SIZE) {
                // talk to CLINT
                uint32_t offset = address - CLINT_BASE;
                return clint->read_reg(offset);
            } else if (address >= PLIC_BASE && address < PLIC_BASE + PLIC_SIZE) {
                // talk to PLIC
                uint32_t offset = address - PLIC_BASE;
                return plic->read_reg(offset);
            } else if (address >= UART_BASE && address < UART_BASE + UART_SIZE) {
                // talk to UART
                uint32_t offset = address - UART_BASE;
                return uart->read_reg(offset);
            }
            return static_cast<T>(0xFFFFFFFF);
        }
        template<typename T>
        inline void write(uint32_t address, uint32_t value) {
            // ram first for performance
            if (address >= RAM_BASE && address < RAM_BASE + RAM_SIZE) {
                uint32_t offset = address - RAM_BASE;
                ram->write<T>(offset, value);
                return;
            } else if (address >= CLINT_BASE && address < CLINT_BASE + CLINT_SIZE) {
                // talk to CLINT
                uint32_t offset = address - CLINT_BASE;
                clint->write_reg(offset, value);
                return;
            } else if (address >= PLIC_BASE && address < PLIC_BASE + PLIC_SIZE) {
                // talk to PLIC
                uint32_t offset = address - PLIC_BASE;
                plic->write_reg(offset, value);
                return;
            } else if (address >= UART_BASE && address < UART_BASE + UART_SIZE) {
                // talk to UART
                uint32_t offset = address - UART_BASE;
                uart->write_reg(offset, value);
                return;
            }
        }
};
#endif