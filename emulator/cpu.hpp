#ifndef CPU_HPP
#define CPU_HPP

#include "bus.hpp"

#include <iostream>

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
        static constexpr uint32_t CSR_SEED = 0x015;
        static constexpr uint32_t CSR_MSTATUS = 0x300;
        static constexpr uint32_t CSR_MISA = 0x301;
        static constexpr uint32_t CSR_MEDELEG = 0x302;
        static constexpr uint32_t CSR_MIDELEG = 0x303;
        static constexpr uint32_t CSR_MIE = 0x304;
        static constexpr uint32_t CSR_MTVEC = 0x305;
        static constexpr uint32_t CSR_MCOUNTEREN = 0x306;
        static constexpr uint32_t CSR_MENVCFG = 0x30A;
        static constexpr uint32_t CSR_MSTATUSH = 0x310;
        static constexpr uint32_t CSR_MCOUNTINHIBIT = 0x320;
        static constexpr uint32_t CSR_MSCRATCH = 0x340;
        static constexpr uint32_t CSR_MEPC = 0x341;
        static constexpr uint32_t CSR_MCAUSE = 0x342;
        static constexpr uint32_t CSR_MTVAL = 0x343;
        static constexpr uint32_t CSR_MIP = 0x344;
        static constexpr uint32_t CSR_PMPCFG = 0x3A0;
        static constexpr uint32_t CSR_TSELECT = 0x7A0;
        static constexpr uint32_t CSR_MCYCLE = 0xB00;
        static constexpr uint32_t CSR_MINSTRET = 0xB02;
        static constexpr uint32_t CSR_MHPMCOUNTER3 = 0xB03;
        static constexpr uint32_t CSR_MHPMCOUNTER31 = 0xB1F;
        static constexpr uint32_t CSR_MCYCLEH = 0xB80;
        static constexpr uint32_t CSR_MHPMCOUNTER3H = 0xB83;
        static constexpr uint32_t CSR_MINSTRETH = 0xB82;
        static constexpr uint32_t CSR_CYCLE = 0xC00;
        static constexpr uint32_t CSR_TIME = 0xC01;
        static constexpr uint32_t CSR_INSTRET = 0xC02;
        static constexpr uint32_t CSR_FB0 = 0xFB0;
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
        bool debug_mode = false;
        // special 64-bit registers
        uint64_t mcycle = 0;
        uint64_t minstret = 0;
        CPU(Bus *busptr, uint32_t ram_start);
        void reset(uint32_t start);
        void tick();
        void dump_state();
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
                    case CSR_MSTATUS:
                        return csr_file[CSR_MSTATUS];
                        break;
                    case CSR_MSTATUSH:
                        return 0;
                        break;
                    case CSR_MISA:
                        return csr_file[CSR_MISA];
                        break;
                    case CSR_MEDELEG:
                        return csr_file[CSR_MEDELEG];
                        break;
                    case CSR_MIDELEG:
                        return csr_file[CSR_MIDELEG];
                        break;
                    case CSR_MIE:
                        return csr_file[CSR_MIE];
                        break;
                    case CSR_MTVEC:
                        return csr_file[CSR_MTVEC];
                        break;
                    case CSR_MSCRATCH:
                        return csr_file[CSR_MSCRATCH];
                        break;
                    case CSR_MEPC:
                        return csr_file[CSR_MEPC];
                        break;
                    case CSR_MCAUSE:
                        return csr_file[CSR_MCAUSE];
                        break;
                    case CSR_MTVAL:
                        return csr_file[CSR_MTVAL];
                        break;
                    case CSR_MIP:
                        return csr_file[CSR_MIP];
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
                    case CSR_MVENDORID:
                    case CSR_MARCHID:
                    case CSR_MIMPID:
                    case CSR_MHARTID:
                        return 0;
                        break;
                    case CSR_SSTATUS:
                        return csr_file[CSR_MSTATUS] & MASK_SSTATUS;
                        break;
                    case CSR_SIE:
                        return csr_file[CSR_MIE] & csr_file[CSR_MIDELEG];
                        break;
                    case CSR_STVEC:
                        return csr_file[CSR_STVEC];
                        break;
                    case CSR_SSCRATCH:
                        return csr_file[CSR_SSCRATCH];
                        break;
                    case CSR_SEPC:
                        return csr_file[CSR_SEPC];
                        break;
                    case CSR_SCAUSE:
                        return csr_file[CSR_SCAUSE];
                        break;
                    case CSR_STVAL:
                        return csr_file[CSR_STVAL];
                        break;
                    case CSR_SIP:
                        return csr_file[CSR_MIP] & csr_file[CSR_MIDELEG];
                        break;
                    case CSR_SATP:
                        return csr_file[CSR_SATP];
                        break;
                    case CSR_SEED:
                        // 31:30 OPST (status), 29:16 Reserved (zero), 15:0 entropy
                        return (2 << 30) | (rand() & 0xFFFF);
                        break;
                    case CSR_PMPCFG:
                    case CSR_MHPMCOUNTER3:
                    case CSR_MHPMCOUNTER3H:
                    case CSR_MHPMCOUNTER31:
                    case CSR_MCOUNTEREN:
                    case CSR_MENVCFG:
                    case CSR_MCOUNTINHIBIT:
                    case CSR_FB0:
                    case CSR_TSELECT:
                    case CSR_CYCLE:
                    case CSR_TIME:
                    case CSR_INSTRET:
                        return 0;
                        break;
                    default:
                        // printf("Illegal CSR: %u", csr);
                        // take_trap(CAUSE_ILLEGALI, inst_reg);
                        return 0;
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
                        case CSR_MSTATUS:
                            csr_file[CSR_MSTATUS] = val;
                            break;
                        case CSR_MSTATUSH:
                            break;
                        case CSR_MEDELEG:
                            csr_file[CSR_MEDELEG] = val;
                            break;
                        case CSR_MIDELEG:
                            csr_file[CSR_MIDELEG] = val;
                            break;
                        case CSR_MIE:
                            csr_file[CSR_MIE] = val;
                            break;
                        case CSR_MTVEC:
                            csr_file[CSR_MTVEC] = val;
                            break;
                        case CSR_MSCRATCH:
                            csr_file[CSR_MSCRATCH] = val;
                            break;
                        case CSR_MEPC:
                            csr_file[CSR_MEPC] = val;
                            break;
                        case CSR_MCAUSE:
                            csr_file[CSR_MCAUSE] = val;
                            break;
                        case CSR_MTVAL:
                            csr_file[CSR_MTVAL] = val;
                            break;
                        case CSR_MIP:
                            csr_file[CSR_MIP] = val;
                            break;
                        case CSR_SSTATUS:
                            csr_file[CSR_MSTATUS] = (csr_file[CSR_MSTATUS] & ~MASK_SSTATUS) | (val & MASK_SSTATUS);
                            break;
                        case CSR_SIE:
                            csr_file[CSR_MIE] = (csr_file[CSR_MIE] & ~csr_file[CSR_MIDELEG]) | (val & csr_file[CSR_MIDELEG]);
                            break;
                        case CSR_STVEC:
                            csr_file[CSR_STVEC] = val;
                            break;
                        case CSR_SSCRATCH:
                            csr_file[CSR_SSCRATCH] = val;
                            break;
                        case CSR_SEPC:
                            csr_file[CSR_SEPC] = val;
                            break;
                        case CSR_SCAUSE:
                            csr_file[CSR_SCAUSE] = val;
                            break;
                        case CSR_STVAL:
                            csr_file[CSR_STVAL] = val;
                            break;
                        case CSR_SIP:
                            csr_file[CSR_MIP] = (csr_file[CSR_MIP] & ~csr_file[CSR_MIDELEG]) | (val & csr_file[CSR_MIDELEG]);
                            break;
                        case CSR_SATP:
                            csr_file[CSR_SATP] = val;
                            flush_tlb();
                            break;
                        case CSR_SEED:
                            // nothing for now
                            break;
                        case CSR_PMPCFG:
                        case CSR_MHPMCOUNTER3:
                        case CSR_MHPMCOUNTER3H:
                        case CSR_MHPMCOUNTER31:
                        case CSR_MCOUNTEREN:
                        case CSR_MENVCFG:
                        case CSR_MCOUNTINHIBIT:
                        case CSR_FB0:
                        case CSR_TSELECT:
                        case CSR_CYCLE:
                        case CSR_TIME:
                        case CSR_INSTRET:
                            break;
                        default:
                            break;
                            // printf("Illegal CSR: %u", csr);
                            // take_trap(CAUSE_ILLEGALI, inst_reg);
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
            if (debug_mode) {
                printf("!!! TRAP: cause=%08x tval=%08x pc=%08x\n", cause, tval, pc);
            }
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
                csr_file[CSR_SCAUSE] = cause;
                csr_file[CSR_STVAL] = tval;
                // mstatus[SPIE] = mstatus[SIE]; mstatus[SIE] = 0; mstatus[SPP] = privilege;
                uint32_t sie = (csr_file[CSR_MSTATUS] >> 1) & 1;
                csr_file[CSR_MSTATUS] = (csr_file[CSR_MSTATUS] & ~(1U << 5)) | (sie << 5); // SPIE
                csr_file[CSR_MSTATUS] &= ~(1U << 1); // SIE = 0
                csr_file[CSR_MSTATUS] = (csr_file[CSR_MSTATUS] & ~(1U << 8)) | ((privilege & 1) << 8); // SPP
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
                csr_file[CSR_MCAUSE] = cause;
                csr_file[CSR_MTVAL] = tval;
                // mstatus[MPIE] = mstatus[MIE]; mstatus[MIE] = 0; mstatus[MPP] = privilege;
                uint32_t mie = (csr_file[CSR_MSTATUS] >> 3) & 1;
                csr_file[CSR_MSTATUS] = (csr_file[CSR_MSTATUS] & ~(1U << 7)) | (mie << 7); // MPIE
                csr_file[CSR_MSTATUS] &= ~(1U << 3); // MIE = 0
                csr_file[CSR_MSTATUS] = (csr_file[CSR_MSTATUS] & ~(3U << 11)) | ((privilege & 3) << 11); // MPP
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