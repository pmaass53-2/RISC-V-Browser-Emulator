#ifndef CPU_HPP
#define CPU_HPP

#include "bus.hpp"

class CPU {
    public:
        // structs
        struct TLB_Entry {
            uint32_t vpn, ppn, permissions, valid;
        };
        struct Translation {
            uint32_t physical_addr, permissions;
        };
        // config
        static constexpr uint32_t TLB_SIZE = 256;
        // opcode types
        static constexpr uint8_t OP = 0b0110011;
        static constexpr uint8_t OP_IMM = 0b0010011;
        static constexpr uint8_t LOAD = 0b0000011;
        static constexpr uint8_t STORE = 0b0100011;
        static constexpr uint8_t BRANCH = 0b1100011;
        static constexpr uint8_t JAL = 0b1101111;
        static constexpr uint8_t JALR = 0b1100111;
        static constexpr uint8_t LUI = 0b0110111;
        static constexpr uint8_t AUIPC = 0b0010111;
        static constexpr uint8_t SYSTEM = 0b1110011;
        static constexpr uint8_t ATOMIC = 0b0101111;
        static constexpr uint8_t MISC_MEM = 0b0001111;
        // Machine mode CSRs
        static constexpr uint32_t CSR_MSTATUS = 0x300;
        static constexpr uint32_t CSR_MISA = 0x301;
        static constexpr uint32_t CSR_MEDELEG = 0x302;
        static constexpr uint32_t CSR_MIDELEG = 0x303;
        static constexpr uint32_t CSR_MIE = 0x304;
        static constexpr uint32_t CSR_MTVEC = 0x305;
        static constexpr uint32_t CSR_MSCRATCH = 0x340;
        static constexpr uint32_t CSR_MEPC = 0x341;
        static constexpr uint32_t CSR_MCAUSE = 0x342;
        static constexpr uint32_t CSR_MTVAL = 0x343;
        static constexpr uint32_t CSR_MIP = 0x344;
        static constexpr uint32_t CSR_MCYCLE = 0xB00;
        static constexpr uint32_t CSR_MINSTRET = 0xB02;
        static constexpr uint32_t CSR_MCYCLEH = 0xB80;
        static constexpr uint32_t CSR_MINSTRETH = 0xB82;
        // read only CSRs
        static constexpr uint32_t CSR_MVENDORID = 0xF11;
        static constexpr uint32_t CSR_MARCHID = 0xF12;
        static constexpr uint32_t CSR_MIMPID = 0xF13;
        static constexpr uint32_t CSR_MHARTID = 0xF14;
        // Supervisor mode CSRs
        static constexpr uint32_t CSR_SSTATUS = 0x100;
        static constexpr uint32_t CSR_SIE = 0x104;
        static constexpr uint32_t CSR_STVEC = 0x105;
        static constexpr uint32_t CSR_SSCRATCH = 0x140;
        static constexpr uint32_t CSR_SEPC = 0x141;
        static constexpr uint32_t CSR_SCAUSE = 0x142;
        static constexpr uint32_t CSR_STVAL = 0x143;
        static constexpr uint32_t CSR_SIP = 0x144;
        static constexpr uint32_t CSR_SATP = 0x180;
        // special CSR masks
        static constexpr uint32_t MASK_SSTATUS = 0x800DE162;
        // interrupt causes
        static constexpr uint32_t CAUSE_MSI = 0x80000003;
        static constexpr uint32_t CAUSE_MTI = 0x80000007;
        static constexpr uint32_t CAUSE_MEI = 0x8000000B;
        static constexpr uint32_t CAUSE_INSTRUCTION_ALIGN = 0x00000000;
        static constexpr uint32_t CAUSE_ILLEGALI = 0x00000002;
        static constexpr uint32_t CAUSE_EBREAK = 0x00000003;
        static constexpr uint32_t CAUSE_LOAD_ALIGN = 0x00000004;
        static constexpr uint32_t CAUSE_STORE_ALIGN = 0x00000006;
        static constexpr uint32_t CAUSE_ECALL_U = 0x00000008;
        static constexpr uint32_t CAUSE_ECALL_S = 0x00000009;
        static constexpr uint32_t CAUSE_ECALL_M = 0x0000000B;
        static constexpr uint32_t CAUSE_PAGE_FAULT_INST = 0x0000000C;
        static constexpr uint32_t CAUSE_PAGE_FAULT_LOAD = 0x0000000D;
        static constexpr uint32_t CAUSE_PAGE_FAULT_STORE = 0x0000000F;
        // access types
        static constexpr uint32_t ACCESS_READ = 0;
        static constexpr uint32_t ACCESS_WRITE = 1;
        static constexpr uint32_t ACCESS_FETCH = 2;
        // registers
        uint32_t reg_file[32] = {0};
        uint32_t csr_file[4096] = {0};
        TLB_Entry tlb[TLB_SIZE] = {};
        uint32_t inst_reg = 0;
        uint32_t pc = 0;
        uint32_t next_pc = 0;
        uint32_t temp = 0;
        uint32_t reservation_addr = 0xFFFFFFFF;
        uint32_t privilege = 3;
        bool reservation_valid = false;
        bool trap_pending = false;
        // special 64-bit registers
        uint64_t mcycle = 0;
        uint64_t minstret = 0;
        CPU(Bus *busptr, uint32_t ram_start);
        void tick();
        uint32_t check_access(uint32_t virt, uint32_t pte, uint32_t access_type);
        Translation translate(uint32_t virt, uint32_t access_type);
        template <typename T>
        T read_memory(uint32_t virt, uint32_t access_type);
        template <typename T>
        void write_memory(uint32_t virt, T val);
    private:
        Bus *bus;
        // helper function
        inline void page_fault(uint32_t virt, uint32_t access_type) {
            switch (access_type) {
                case 0:
                    take_trap(CAUSE_PAGE_FAULT_LOAD, virt);
                    break;
                case 1:
                    take_trap(CAUSE_PAGE_FAULT_STORE, virt);
                    break;
                default:
                    take_trap(CAUSE_PAGE_FAULT_INST, virt);
            }
        }
        inline void flush_tlb() {
            for (int i = 0; i < 256; i++) {
                tlb[i].valid = false;
            }
        }
        inline void set_reg(uint8_t rd, uint32_t val) {
            if (rd != 0) reg_file[rd] = val;
        }
        inline uint32_t csr_addr() const { 
            return (inst_reg >> 20) & 0xFFF; 
        }
        inline uint32_t get_csr(uint32_t csr) {
            if (((csr >> 8) & 3) > privilege) {
                // not high enough permission
                take_trap(CAUSE_ILLEGALI, inst_reg);
                return 0;
            } else {
                switch (csr) {
                    case CSR_SSTATUS:
                        return csr_file[CSR_MSTATUS] & MASK_SSTATUS;
                        break;
                    case CSR_SIE:
                        return csr_file[CSR_MIE] & csr_file[CSR_MIDELEG];
                        break;
                    case CSR_SIP:
                        return csr_file[CSR_MIP] & csr_file[CSR_MIDELEG];
                        break;
                    case CSR_MCYCLE:
                        return static_cast<uint32_t>(mcycle);
                        break;
                    case CSR_MCYCLEH:
                        return static_cast<uint32_t>(mcycle >> 32);
                        break;
                    case CSR_MINSTRET:
                        return static_cast<uint32_t>(minstret);
                        break;
                    case CSR_MINSTRETH:
                        return static_cast<uint32_t>(minstret >> 32);
                        break;
                    default:
                        return csr_file[csr];
                }
            }
        }
        inline void set_csr(uint32_t csr, uint32_t val) {
            if (((csr >> 10) & 3) == 0b11) {
                // readonly
                take_trap(CAUSE_ILLEGALI, inst_reg);
            } else {
                if (((csr >> 8) & 3) > privilege) {
                    // not high enough permission
                    take_trap(CAUSE_ILLEGALI, inst_reg);
                } else {
                    switch (csr) {
                        case 0:
                            break;
                        case CSR_SSTATUS:
                            csr_file[CSR_MSTATUS] = (csr_file[CSR_MSTATUS] & ~MASK_SSTATUS) | (val & MASK_SSTATUS);
                            break;
                        case CSR_SIE:
                            csr_file[CSR_MIE] = (csr_file[CSR_MIE] & ~csr_file[CSR_MIDELEG]) | (val & csr_file[CSR_MIDELEG]);
                            break;
                        case CSR_SIP:
                            csr_file[CSR_MIP] = (csr_file[CSR_MIP] & ~csr_file[CSR_MIDELEG]) | (val & csr_file[CSR_MIDELEG]);
                            break;
                        case CSR_SATP:
                            csr_file[CSR_SATP] = val;
                            flush_tlb();
                            break;
                        default:
                            csr_file[csr] = val;
                    }
                }
            }
        }
        // decoding helper functions
        inline uint8_t rd() const { return (inst_reg >> 7) & 0x1F; }
        inline uint8_t rs1() const { return (inst_reg >> 15) & 0x1F; }
        inline uint8_t rs2() const { return (inst_reg >> 20) & 0x1F; }
        inline uint8_t funct3() const { return (inst_reg >> 12) & 0x7; }
        inline uint8_t funct5() const { return (inst_reg >> 27) & 0x1F; }
        inline uint8_t funct7() const { return (inst_reg >> 25) & 0x7F; }
        // immediate decoding - type specific
        inline int32_t imm_i() const { 
            return static_cast<int32_t>(inst_reg) >> 20; 
        }
        inline int32_t imm_s() const {
            return (static_cast<int32_t>(inst_reg & 0xFE000000) >> 20) | 
                ((inst_reg >> 7) & 0x1F);
        }
        inline int32_t imm_u() const {
            return static_cast<int32_t>(inst_reg & 0xFFFFF000);
        }
        inline int32_t imm_b() const {
            return (static_cast<int32_t>(inst_reg & 0x80000000) >> 19) |
                ((inst_reg & 0x80) << 4) |
                ((inst_reg >> 20) & 0x7E0) |
                ((inst_reg >> 7) & 0x1E);
        }
        inline int32_t imm_j() const {
            return (static_cast<int32_t>(inst_reg & 0x80000000) >> 11) |
                (inst_reg & 0xFF000) |
                ((inst_reg >> 9) & 0x800) |
                ((inst_reg >> 20) & 0x7FE);
        }
        void take_trap(uint32_t cause, uint32_t tval = 0) {
            reservation_valid = false;
            trap_pending = true;
            uint32_t exception_code = cause & 0x7FFFFFFF;
            bool delegate = false;
            if (privilege <= 1) {
                if ((cause & 0x80000000) != 0) {
                    // interrupt
                    delegate = (get_csr(CSR_MIDELEG) >> exception_code) & 1;
                } else {
                    // exception
                    delegate = (get_csr(CSR_MEDELEG) >> exception_code) & 1;
                }
            }
            if (delegate) {
                // handle in S-Mode
                csr_file[CSR_SEPC] = pc;
                // just write to M-mode registers for simplicity
                csr_file[CSR_SCAUSE] = cause;
                csr_file[CSR_STVAL] = tval;
                csr_file[CSR_MSTATUS] = (csr_file[CSR_MSTATUS] & ~(1U << 8)) | (privilege & 1U) << 8;
                csr_file[CSR_MSTATUS] = (csr_file[CSR_MSTATUS] & ~(1U << 5)) | (((csr_file[CSR_MSTATUS] >> 1) & 1U) << 5);
                csr_file[CSR_MSTATUS] &= ~(1U << 1);
                privilege = 1;
                if ((cause & 0x80000000) && ((csr_file[CSR_STVEC] & 3) == 1)) {
                    next_pc = (csr_file[CSR_STVEC] & ~3) + (4 * exception_code);
                    return;
                } else {
                    next_pc = csr_file[CSR_STVEC] & ~3;
                }
            } else {
                // handle in M-Mode
                csr_file[CSR_MEPC] = pc;
                // write to csr_file directly for performance
                csr_file[CSR_MCAUSE] = cause;
                csr_file[CSR_MTVAL] = tval;
                csr_file[CSR_MSTATUS] = (csr_file[CSR_MSTATUS] & ~(3U << 11)) | (privilege & 3U) << 11;
                csr_file[CSR_MSTATUS] = (csr_file[CSR_MSTATUS] & ~(1U << 7)) | (((csr_file[CSR_MSTATUS] >> 3) & 1U) << 7);
                csr_file[CSR_MSTATUS] &= ~(1U << 3);
                privilege = 3;
                if ((cause & 0x80000000) && ((csr_file[CSR_MTVEC] & 3) == 1)) {
                    next_pc = (csr_file[CSR_MTVEC] & ~3) + (4 * exception_code);
                    return;
                } else {
                    next_pc = csr_file[CSR_MTVEC] & ~3;
                }
            }
        }
};

#endif