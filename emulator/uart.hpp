#ifndef UART_HPP
#define UART_HPP

#include <cstdint>
#include <emscripten.h>

#include "plic.hpp"

class UART {
    public:
        static constexpr uint32_t IER = 0x1; // R/W (DLAB=0)
        static constexpr uint32_t IIR = 0x2; // Read
        static constexpr uint32_t FCR = 0x2; // Write
        static constexpr uint32_t LCR = 0x3; // Line Control
        static constexpr uint32_t MCR = 0x4; // Modem Control
        static constexpr uint32_t LSR = 0x5; // Line Status
        static constexpr uint32_t MSR = 0x6; // Modem Status
        static constexpr uint32_t SCR = 0x7; // Scratchpad
        // non-standard registers
        static constexpr uint32_t RBR = 0x8; // Read (DLAB=0)
        static constexpr uint32_t THR = 0x9; // Write (DLAB=0)
        static constexpr uint32_t DLL = 0xA; // R/W (DLAB=1)
        static constexpr uint32_t DLM = 0xB; // R/W (DLAB=1)

        uint8_t regs[12] = {0};

        PLIC *plic;

        UART(PLIC *plic) : plic(plic) {
            regs[LSR] = 0x60;
            regs[IIR] = 0x01;
        }
        void write_reg(uint32_t offset, uint32_t value) {
            uint32_t reg = offset & 0x7;
            uint8_t val = static_cast<uint8_t>(value);
            bool dlab = (regs[LCR] & 0x80) != 0;

            if (reg == 0) {
                if (dlab) {
                    regs[DLL] = val;
                } else {
                    regs[THR] = val;
                    // Transmission always succeeds instantly
                    regs[LSR] |= 0x60; 
                    EM_ASM({ js_uart_write($0); }, val);
                }
            } else if (reg == 1) {
                if (dlab) {
                    regs[DLM] = val;
                } else {
                    regs[IER] = val;
                }
            } else if (reg == 2) {
                // FCR (FIFO Control Register) - write only
                regs[FCR] = val;
            } else if (reg == 3) {
                regs[LCR] = val;
            } else if (reg == 4) {
                regs[MCR] = val;
            } else if (reg == 7) {
                regs[SCR] = val;
            }
            // IIR (2), LSR (5), MSR (6) are read-only from the CPU's perspective
        }
        uint32_t read_reg(uint32_t offset) {
            uint32_t reg = offset & 0x7;
            bool dlab = (regs[LCR] & 0x80) != 0;
            if (reg == 0) {
                if (dlab) {
                    return regs[DLL];
                } else {
                    regs[LSR] &= ~0x01;
                    regs[IIR] = 0x01;
                    return regs[RBR];
                }
            } else if (reg == 1 && dlab) {
                return regs[DLM];
            }
            return regs[reg];
        }
        void rx_push(uint8_t c) {
            regs[RBR] = c;
            regs[LSR] |= 0x01;
            regs[IIR] = 0x04;
            // trigger PLIC
            if (regs[IER] & 0x01) {
                plic->set_pending(PLIC::ID_UART);
            }
        }
};
#endif