#include "cpu.hpp"

#include "bus.hpp"

#include <algorithm>

CPU::CPU(Bus* busptr, uint32_t ram_start) : bus(busptr), next_pc(ram_start) {
}
uint32_t CPU::check_access(uint32_t virt, uint32_t pte, uint32_t access_type) {
    bool r = (pte >> 1) & 0x1;
    bool w = (pte >> 2) & 0x1;
    bool x = (pte >> 3) & 0x1;
    bool u = (pte >> 4) & 0x1;
    if (privilege == 0 && u == 0) {
        page_fault(virt, access_type);
        return 0;
    }
    if (!((csr_file[CSR_MSTATUS] & 0x40000) >> 18) && privilege == 1 && u == 1) {
        page_fault(virt, access_type);
        return 0;
    }
    if (privilege == 1 && u == 1 && access_type == 2) { // 2 is ACCESS_FETCH
        take_trap(CAUSE_PAGE_FAULT_INST, virt);
        return 0;
    }
    if (access_type == 0 && r == 0) {
        take_trap(CAUSE_PAGE_FAULT_LOAD, virt);
        return 0;
    }
    if (access_type == 1 && w == 0) {
        take_trap(CAUSE_PAGE_FAULT_STORE, virt);
        return 0;
    }
    if (access_type == 2 && x == 0) {
        take_trap(CAUSE_PAGE_FAULT_INST, virt);
        return 0;
    }
    return 1;
}
CPU::Translation CPU::translate(uint32_t virt, uint32_t access_type) {
    // note: direct CSR access for performance
    uint32_t offset = virt & 0xFFF;
    uint32_t vpn0 = (virt >> 12) & 0x3FF;
    uint32_t vpn1 = (virt >> 22) & 0x3FF;
    uint32_t pte1_address = ((csr_file[CSR_SATP] & 0x3FFFFF) * 4096) + (vpn1 * 4);
    uint32_t pte1 = bus->read<uint32_t>(pte1_address);
    bool pte1_valid = pte1 & 0x1;
    if (pte1_valid) {
        // simplified form, extract RWX
        if ((pte1 & 0xE) == 0) {
            // pointer
            uint32_t pte0_address = (((pte1 >> 10) & 0x3FFFFF) * 4096) + (vpn0 * 4);
            uint32_t pte0 = bus->read<uint32_t>(pte0_address);
            bool pte0_valid = pte0 & 0x1;
            if (pte0_valid) {
                if ((pte0 & 0xE) == 0) {
                    page_fault(virt, access_type);
                    return {0, 0};
                } else {
                    if (check_access(virt, pte0, access_type) == 0) {
                        return {0, 0};
                    } else {
                        uint32_t ppn = (pte0 >> 10) & 0x3FFFFF;
                        uint32_t physical_addr = (ppn << 12) | offset;
                        return {physical_addr, pte0};
                    }
                }
            } else {
                page_fault(virt, access_type);
                return {0, 0};
            }
        } else {
            // superpage (4MB)
            uint32_t pte1_ppn = (pte1 >> 10) & 0x3FFFFF;
            if ((pte1_ppn & 0x3FF) != 0) {
                page_fault(virt, access_type);
                return {0, 0};
            } else {
                if (check_access(virt, pte1, access_type) == 0) {
                    return {0, 0};
                } else {
                    uint32_t new_pte = pte1 | 0x40; // Set A bit
                    if (access_type == 1) new_pte |= 0x80; // Set D bit if writing
                    if (new_pte != pte1) bus->write<uint32_t>(pte1_address, new_pte);
                    uint32_t pte1_ppn_updated = (new_pte >> 10) & 0x3FFFFF;
                    uint32_t physical_addr = ((pte1_ppn_updated << 12) | (vpn0 << 12)) | offset;
                    return {physical_addr, new_pte};
                }
            }
        }
    } else {
        page_fault(virt, access_type);
        return {0, 0};
    }
}
template <typename T>
T CPU::read_memory(uint32_t virt, uint32_t access_type) {
    if ((virt & (sizeof(T) - 1)) != 0) {
        take_trap(CAUSE_LOAD_ALIGN, virt);
        return 0;
    }
    uint32_t effective_privilege = privilege;
    if (access_type != ACCESS_FETCH && ((csr_file[CSR_MSTATUS] >> 17) & 1)) {
        effective_privilege = (csr_file[CSR_MSTATUS] >> 11) & 3;
    }
    if (effective_privilege == 3 || ((csr_file[CSR_SATP] & 0x80000000) == 0)) {
        return bus->read<T>(virt);
    } else {
        // TLB lookup
        uint32_t full_vpn = virt >> 12;
        uint32_t tlb_index = full_vpn & 0xFF;
        uint32_t physical_addr = 0;
        if (tlb[tlb_index].valid && tlb[tlb_index].vpn == full_vpn) {
            if (check_access(virt, tlb[tlb_index].permissions, access_type) == 1) {
                uint32_t offset = virt & 0xFFF;
                physical_addr = (tlb[tlb_index].ppn << 12) | offset;
            } else {
                return 0;
            }
        } else {
            Translation result = translate(virt, access_type);
            // update physical_addr for bus->ram.read()
            physical_addr = result.physical_addr;
            if (!trap_pending) {
                // update TLB
                tlb[tlb_index] = TLB_Entry{full_vpn, physical_addr >> 12, result.permissions, 1};
            }
        }
        // check if trapped
        if (trap_pending) {
            return 0;
        } else {
            return bus->read<T>(physical_addr);
        }
    }
}
template <typename T>
void CPU::write_memory(uint32_t virt, T val) {
    if ((virt & (sizeof(T) - 1)) != 0) {
        take_trap(CAUSE_STORE_ALIGN, virt);
        return;
    }
    reservation_valid = false;
    uint32_t effective_privilege = privilege;
    if ((csr_file[CSR_MSTATUS] >> 17) & 1) {
        effective_privilege = (csr_file[CSR_MSTATUS] >> 11) & 3;
    }
    if (effective_privilege == 3 || ((csr_file[CSR_SATP] & 0x80000000) == 0)) {
        bus->write<T>(virt, val);
        return;
    } else {
        // TLB lookup
        uint32_t full_vpn = virt >> 12;
        uint32_t tlb_index = full_vpn & 0xFF;
        uint32_t physical_addr = 0;
        if (tlb[tlb_index].valid && tlb[tlb_index].vpn == full_vpn) {
            if (check_access(virt, tlb[tlb_index].permissions, ACCESS_WRITE) == 1) {
                uint32_t offset = virt & 0xFFF;
                physical_addr = (tlb[tlb_index].ppn << 12) | offset;
            } else {
                return;
            }
        } else {
            Translation result = translate(virt, ACCESS_WRITE);
            // update physical_addr for bus->ram.read()
            physical_addr = result.physical_addr;
            if (!trap_pending) {
                // update TLB
                tlb[tlb_index] = TLB_Entry{full_vpn, physical_addr >> 12, result.permissions, 1};
            }
        }
        // check if trapped
        if (!trap_pending) {
            bus->write<T>(physical_addr, val);
        }
    }
}
void CPU::reset(uint32_t start) {
    next_pc = start;
    for (uint32_t i = 0; i < 32; i++) {
        reg_file[i] = 0;
    }
    for (uint32_t i = 0; i < 4096; i++) {
        csr_file[i] = 0;
    }
    csr_file[CSR_MHARTID] = 0;
    csr_file[CSR_MISA] = 0x40141101;
    privilege = 3;
    mcycle = 0;
    minstret = 0;
    flush_tlb();
}
void CPU::tick() {
    trap_pending = false;
    // increment instructions executed
    mcycle++;
    // update MSIP (software interrupt pending) and MTIP (timer interrupt pending) from CLINT
    csr_file[CSR_MIP] = (csr_file[CSR_MIP] & ~0xA88) | (bus->plic->MEIP << 11) | (bus->plic->SEIP << 9) | (bus->clint->MTIP << 7) | (bus->clint->MSIP << 3);
    // handle traps and interrupts
    uint32_t mstatus = csr_file[CSR_MSTATUS];
    uint32_t mideleg = csr_file[CSR_MIDELEG];
    uint32_t pending = csr_file[CSR_MIP] & csr_file[CSR_MIE];
    bool m_accept = (privilege < 3) || (privilege == 3 && (mstatus & (1 << 3)));
    bool s_accept = (privilege < 1) || (privilege == 1 && (mstatus & (1 << 1)));
    // Split candidates by delegation
    uint32_t m_candidates = pending & ~mideleg;
    uint32_t s_candidates = pending & mideleg;
    // prioritize External > Software > Timer
    if (m_accept && m_candidates) {
        if (m_candidates & (1 << 11)) {
            take_trap(0x8000000B);
            return;
        }
        if (m_candidates & (1 << 3)) {
            take_trap(0x80000003);
            return;
        }
        if (m_candidates & (1 << 7)) {
            take_trap(0x80000007);
            return;
        }
    }
    if (s_accept && s_candidates) {
        if (s_candidates & (1 << 9)) {
            take_trap(0x80000009);
            return;
        }
        if (s_candidates & (1 << 1)) {
            take_trap(0x80000001);
            return;
        }
        if (s_candidates & (1 << 5)) {
            take_trap(0x80000005);
            return;
        }
    }
    // update pc, current instruction, opcode
    pc = next_pc;
    inst_reg = read_memory<uint32_t>(pc, ACCESS_FETCH);
    uint32_t opcode = inst_reg & 0x7F;
    // default value for next_pc (can be overriden)
    next_pc = pc + 4;
    if (trap_pending) {
        // error reading instruction
        return;
    }
    // execute current instruction
    switch (opcode) {
        case OP:
            switch (funct7()) {
                case 0x01:
                    // MUL/DIV
                    switch (funct3()) {
                        case 0x0:
                            // MUL
                            set_reg(rd(), static_cast<uint32_t>(reg_file[rs1()] * reg_file[rs2()]));
                            break;
                        case 0x1:
                            // MULH
                            set_reg(rd(), static_cast<uint32_t>(static_cast<int64_t>(static_cast<int32_t>(reg_file[rs1()])) * static_cast<int64_t>(static_cast<int32_t>(reg_file[rs2()])) >> 32));
                            break;
                        case 0x2:
                            // MULHSU
                            set_reg(rd(), static_cast<uint32_t>((static_cast<int64_t>(static_cast<int32_t>(reg_file[rs1()])) * static_cast<uint64_t>(reg_file[rs2()])) >> 32));
                            break;
                        case 0x3:
                            // MULHU
                            set_reg(rd(), static_cast<uint32_t>((static_cast<uint64_t>(reg_file[rs1()]) * static_cast<uint64_t>(reg_file[rs2()])) >> 32));
                            break;
                        case 0x4:
                            // DIV
                            if (static_cast<int32_t>(reg_file[rs2()]) == 0) {
                                set_reg(rd(), 0xFFFFFFFF);
                            } else if (static_cast<int32_t>(reg_file[rs1()]) == static_cast<int32_t>(0x80000000) && static_cast<int32_t>(reg_file[rs2()]) == -1) {
                                set_reg(rd(), 0x80000000);
                            } else {
                                set_reg(rd(), static_cast<uint32_t>(static_cast<int32_t>(reg_file[rs1()]) / static_cast<int32_t>(reg_file[rs2()])));
                            }
                            break;
                        case 0x5:
                            // DIVU
                            if (reg_file[rs2()] == 0) {
                                set_reg(rd(), 0xFFFFFFFF);
                            } else {
                                set_reg(rd(), reg_file[rs1()] / reg_file[rs2()]);
                            }
                            break;
                        case 0x6:
                            // REM
                            if (static_cast<int32_t>(reg_file[rs2()]) == 0) {
                                set_reg(rd(), static_cast<int32_t>(reg_file[rs1()]));
                            } else if (static_cast<int32_t>(reg_file[rs1()]) == static_cast<int32_t>(0x80000000) && static_cast<int32_t>(reg_file[rs2()]) == -1) {
                                set_reg(rd(), 0);
                            } else {
                                set_reg(rd(), static_cast<uint32_t>(static_cast<int32_t>(reg_file[rs1()]) % static_cast<int32_t>(reg_file[rs2()])));
                            }
                            break;
                        case 0x7:
                            // REMU
                            if (reg_file[rs2()] == 0) {
                                set_reg(rd(), reg_file[rs1()]);
                            } else {
                                set_reg(rd(), reg_file[rs1()] % reg_file[rs2()]);
                            }
                            break;
                        default:
                            take_trap(CAUSE_ILLEGALI, inst_reg);
                    }
                    break;
                default:
                    switch (funct3()) {
                        case 0x0:
                            if (funct7() == 0x20) {
                                // SUB
                                set_reg(rd(), (reg_file[rs1()] - reg_file[rs2()]));
                            } else {
                                // ADD
                                set_reg(rd(), (reg_file[rs1()] + reg_file[rs2()]));
                            }
                            break;
                        case 0x1:
                            // SLL
                            set_reg(rd(), (reg_file[rs1()] << (reg_file[rs2()] & 0x1F)));
                            break;
                        case 0x2:
                            // SLT
                            set_reg(rd(), (static_cast<int32_t>(reg_file[rs1()]) < static_cast<int32_t>(reg_file[rs2()])));
                            break;
                        case 0x3:
                            // SLTU
                            set_reg(rd(), (reg_file[rs1()] < reg_file[rs2()]));
                            break;
                        case 0x4:
                            // XOR
                            set_reg(rd(), (reg_file[rs1()] ^ reg_file[rs2()]));
                            break;
                        case 0x5:
                            if (funct7() == 0x20) {
                                // SRA
                                set_reg(rd(), (static_cast<uint32_t>(static_cast<int32_t>(reg_file[rs1()]) >> (reg_file[rs2()] & 0x1F))));
                            } else {
                                // SRL
                                set_reg(rd(), (reg_file[rs1()] >> (reg_file[rs2()] & 0x1F)));
                            }
                            break;
                        case 0x6:
                            // OR
                            set_reg(rd(), (reg_file[rs1()] | reg_file[rs2()]));
                            break;
                        case 0x7:
                            // AND
                            set_reg(rd(), (reg_file[rs1()] & reg_file[rs2()]));
                            break;
                        default:
                            take_trap(CAUSE_ILLEGALI, inst_reg);
                }
            }
            break;
        case OP_IMM:
            switch (funct3()) {
                case 0x0:
                    // ADDI (No SUBI)
                    set_reg(rd(), (reg_file[rs1()] + imm_i()));
                    break;
                case 0x1:
                    // SLLI
                    set_reg(rd(), (reg_file[rs1()] << (imm_i() & 0x1F)));
                    break;
                case 0x2:
                    // SLTI
                    set_reg(rd(), (static_cast<int32_t>(reg_file[rs1()]) < imm_i()));
                    break;
                case 0x3:
                    // SLTIU
                    set_reg(rd(), (reg_file[rs1()] < static_cast<uint32_t>(imm_i())));
                    break;
                case 0x4:
                    // XORI
                    set_reg(rd(), (reg_file[rs1()] ^ imm_i()));
                    break;
                case 0x5:
                    if (funct7() == 0x20) {
                        // SRAI
                        set_reg(rd(), (static_cast<int32_t>(reg_file[rs1()]) >> (imm_i() & 0x1F)));
                    } else {
                        // SRLI
                        set_reg(rd(), (reg_file[rs1()] >> (imm_i() & 0x1F)));
                    }
                    break;
                case 0x6:
                    // ORI
                    set_reg(rd(), (reg_file[rs1()] | imm_i()));
                    break;
                case 0x7:
                    // ANDI
                    set_reg(rd(), (reg_file[rs1()] & imm_i()));
                    break;
                default:
                    take_trap(CAUSE_ILLEGALI, inst_reg);
            }
            break;
        case LOAD:
            switch (funct3()) {
                case 0x0:
                    // Load Byte (sign extend)
                    temp = read_memory<uint8_t>(reg_file[rs1()] + imm_i(), ACCESS_READ);
                    if (!trap_pending) {
                        set_reg(rd(), static_cast<uint32_t>(static_cast<int8_t>(temp)));
                    }
                    break;
                case 0x1:
                    // Load Halfword (sign extend)
                    temp = read_memory<uint16_t>(reg_file[rs1()] + imm_i(), ACCESS_READ);
                    if (!trap_pending) {
                        set_reg(rd(), static_cast<uint32_t>(static_cast<int16_t>(temp)));
                    }
                    break;
                case 0x2:
                    // Load Word (sign extend)
                    temp = read_memory<uint32_t>(reg_file[rs1()] + imm_i(), ACCESS_READ);
                    if (!trap_pending) {
                        set_reg(rd(), temp);
                    }
                    break;
                case 0x4:
                    // Load Byte (zero extend)
                    temp = read_memory<uint8_t>(reg_file[rs1()] + imm_i(), ACCESS_READ);
                    if (!trap_pending) {
                        set_reg(rd(), static_cast<uint32_t>(temp));
                    }
                    break;
                case 0x5:
                    // Load Halfword (zero extend)
                    temp = read_memory<uint16_t>(reg_file[rs1()] + imm_i(), ACCESS_READ);
                    if (!trap_pending) {
                        set_reg(rd(), static_cast<uint32_t>(temp));
                    }
                    break;
                default:
                    take_trap(CAUSE_ILLEGALI, inst_reg);
            }
            break;
        case STORE:
            switch (funct3()) {
                case 0x0:
                    // Store Byte
                    write_memory<uint8_t>(reg_file[rs1()] + imm_s(), static_cast<uint8_t>(reg_file[rs2()]));
                    break;
                case 0x1:
                    // Store Halfword
                    write_memory<uint16_t>(reg_file[rs1()] + imm_s(), static_cast<uint16_t>(reg_file[rs2()]));
                    break;
                case 0x2:
                    // Store Word
                    write_memory<uint32_t>(reg_file[rs1()] + imm_s(), reg_file[rs2()]);
                    break;
                default:
                    take_trap(CAUSE_ILLEGALI, inst_reg);
            }
            break;
        case BRANCH:
            switch (funct3()) {
                case 0x0:
                    // BEQ
                    temp = pc + imm_b();
                    if (reg_file[rs1()] == reg_file[rs2()]) {
                        if ((temp & 0x3) != 0) {
                            take_trap(CAUSE_INSTRUCTION_ALIGN, temp);
                        } else {
                            next_pc = temp;
                        }
                    }
                    break;
                case 0x1:
                    // BNE
                    temp = pc + imm_b();
                    if (reg_file[rs1()] != reg_file[rs2()]) {
                        if ((temp & 0x3) != 0) {
                            take_trap(CAUSE_INSTRUCTION_ALIGN, temp);
                        } else {
                            next_pc = temp;
                        }
                    }
                    break;
                case 0x4:
                    // BLT (signed)
                    temp = pc + imm_b();
                    if (static_cast<int32_t>(reg_file[rs1()]) < static_cast<int32_t>(reg_file[rs2()])) {
                        if ((temp & 0x3) != 0) {
                            take_trap(CAUSE_INSTRUCTION_ALIGN, temp);
                        } else {
                            next_pc = temp;
                        }
                    }
                    break;
                case 0x5:
                    // BGE (signed)
                    temp = pc + imm_b();
                    if (static_cast<int32_t>(reg_file[rs1()]) >= static_cast<int32_t>(reg_file[rs2()])) {
                        if ((temp & 0x3) != 0) {
                            take_trap(CAUSE_INSTRUCTION_ALIGN, temp);
                        } else {
                            next_pc = temp;
                        }
                    }
                    break;
                case 0x6:
                    // BLTU
                    temp = pc + imm_b();
                    if (reg_file[rs1()] < reg_file[rs2()]) {
                        if ((temp & 0x3) != 0) {
                            take_trap(CAUSE_INSTRUCTION_ALIGN, temp);
                        } else {
                            next_pc = temp;
                        }
                    }
                    break;
                case 0x7:
                    // BGEU
                    temp = pc + imm_b();
                    if (reg_file[rs1()] >= reg_file[rs2()]) {
                        if ((temp & 0x3) != 0) {
                            take_trap(CAUSE_INSTRUCTION_ALIGN, temp);
                        } else {
                            next_pc = temp;
                        }
                    }
                    break;
                default:
                    take_trap(CAUSE_ILLEGALI, inst_reg);
            }
            break;
        case JAL:
            temp = pc + imm_j();
            if ((temp & 0x3) != 0) {
                take_trap(CAUSE_INSTRUCTION_ALIGN, temp);
            } else {
                set_reg(rd(), next_pc);
                next_pc = temp;
            }
            break;
        case JALR:
            temp = (reg_file[rs1()] + imm_i()) & ~1;
            if ((temp & 0x3) != 0) {
                take_trap(CAUSE_INSTRUCTION_ALIGN, temp);
            } else {
                set_reg(rd(), next_pc);
                next_pc = temp;
            }
            break;
        case LUI:
            // imm_u already shifted
            set_reg(rd(), imm_u());
            break;
        case AUIPC:
            // imm_u already shifted
            set_reg(rd(), pc + imm_u());
            break;
        case SYSTEM:
            switch (funct3()) {
                case 0x0:
                    // ECALL/EBREAK
                    switch(imm_i()) {
                        case 0x000:
                            // ECALL
                            switch (privilege) {
                                case 3:
                                    take_trap(CAUSE_ECALL_M);
                                    break;
                                case 1:
                                    take_trap(CAUSE_ECALL_S);
                                    break;
                                case 0:
                                    take_trap(CAUSE_ECALL_U);
                                    break;
                                default:
                                    take_trap(CAUSE_ILLEGALI, inst_reg);
                            }
                            break;
                        case 0x001:
                            // EBREAK
                            take_trap(CAUSE_EBREAK);
                            break;
                        case 0x102:
                            // SRET
                            if (privilege == 0 || privilege == 1 && ((get_csr(CSR_MSTATUS) >> 22) & 1) == 1) {
                                take_trap(CAUSE_ILLEGALI, inst_reg);
                                break;
                            }
                            next_pc = get_csr(CSR_SEPC);
                            privilege = (get_csr(CSR_MSTATUS) >> 8) & 1;
                            // clear SPP after reading
                            set_csr(CSR_MSTATUS, get_csr(CSR_MSTATUS) & ~(1U << 8));
                            // copy SPIE to SIE
                            set_csr(CSR_MSTATUS, (get_csr(CSR_MSTATUS) & ~(1U << 1)) | (((get_csr(CSR_MSTATUS) >> 5) & 1U) << 1));
                            set_csr(CSR_MSTATUS, get_csr(CSR_MSTATUS) | (1U << 5));
                            break;
                        case 0x302:
                            // MRET
                            if (privilege < 3) {
                                take_trap(CAUSE_ILLEGALI, inst_reg);
                                break;
                            }
                            next_pc = get_csr(CSR_MEPC);
                            privilege = (get_csr(CSR_MSTATUS) >> 11) & 3;
                            // copy MPIE to MIE
                            set_csr(CSR_MSTATUS, (get_csr(CSR_MSTATUS) & ~(1U << 3)) | (((get_csr(CSR_MSTATUS) >> 7) & 1U) << 3));
                            set_csr(CSR_MSTATUS, get_csr(CSR_MSTATUS) | (1U << 7));
                            set_csr(CSR_MSTATUS, get_csr(CSR_MSTATUS) & ~(3U << 11));
                            // clear mstatus.MPRV
                            set_csr(CSR_MSTATUS, get_csr(CSR_MSTATUS) & ~(1U << 17));
                            break;
                        default:
                            take_trap(CAUSE_ILLEGALI, inst_reg);
                            break;
                    }
                    break;
                case 0x1:
                    // CSRRW
                    temp = get_csr(csr_addr());
                    set_csr(csr_addr(), reg_file[rs1()]);
                    set_reg(rd(), temp);
                    break;
                case 0x2:
                    // CSRRS
                    temp = get_csr(csr_addr());
                    if (rs1() != 0) {
                        set_csr(csr_addr(), temp | reg_file[rs1()]);
                    }
                    set_reg(rd(), temp);
                    break;
                case 0x3:
                    // CSRRC
                    temp = get_csr(csr_addr());
                    if (rs1() != 0) {
                        set_csr(csr_addr(), temp & ~reg_file[rs1()]);
                    }
                    set_reg(rd(), temp);
                    break;
                case 0x5:
                    // CSRRWI;
                    temp = get_csr(csr_addr());
                    if (rs1() != 0) {
                        set_csr(csr_addr(), rs1());
                    }
                    set_reg(rd(), temp);
                    break;
                case 0x6:
                    // CSRRSI
                    temp = get_csr(csr_addr());
                    if (rs1() != 0) {
                        set_csr(csr_addr(), temp | rs1());
                    }
                    set_reg(rd(), temp);
                    break;
                case 0x7:
                    // CSRRCI
                    temp = get_csr(csr_addr());
                    if (rs1() != 0) {
                        set_csr(csr_addr(), temp & ~rs1());
                    }
                    set_reg(rd(), temp);
                    break;
                default:
                    if (funct7() == 0x09) {
                        // SFENCE.VMA
                        if (privilege > 0) {
                            if (privilege == 1 && ((get_csr(CSR_MSTATUS) >> 20) & 1)) {
                                take_trap(CAUSE_ILLEGALI, inst_reg);
                                break; 
                            } else {
                                flush_tlb();
                                break;
                            }
                        } else {
                            take_trap(CAUSE_ILLEGALI, inst_reg);
                            break;
                        }
                    } else {
                        take_trap(CAUSE_ILLEGALI, inst_reg);
                    }
            }
            break;
        case ATOMIC:
            if ((reg_file[rs1()] & 0b11) != 0) {
                set_csr(CSR_MEPC, pc);
                if (funct5() == 0x02) {
                    take_trap(CAUSE_LOAD_ALIGN);
                } else {
                    take_trap(CAUSE_STORE_ALIGN);
                }
                break;
            }
            switch (funct5()) {
                case 0x00:
                    // AMOADD.W
                    reservation_valid = false;
                    temp = read_memory<uint32_t>(reg_file[rs1()], ACCESS_READ);
                    if (!trap_pending) {
                        write_memory<uint32_t>(reg_file[rs1()], temp + reg_file[rs2()]);
                        set_reg(rd(), temp);
                    }
                    break;
                case 0x01:
                    // AMOSWAP.W
                    reservation_valid = false;
                    temp = read_memory<uint32_t>(reg_file[rs1()], ACCESS_READ);
                    if (!trap_pending) {
                        write_memory<uint32_t>(reg_file[rs1()], reg_file[rs2()]);
                        set_reg(rd(), temp);
                    }
                    break;
                case 0x02:
                    // LR.W
                    temp = reg_file[rs1()];
                    set_reg(rd(), read_memory<uint32_t>(temp, ACCESS_READ));
                    reservation_addr = translate(temp, ACCESS_READ).physical_addr;
                    if (trap_pending) {
                        // translate failed
                        return;
                    }
                    reservation_valid = true;
                    break;
                case 0x03:
                    // SC.W
                    if (reservation_valid && translate(reg_file[rs1()], ACCESS_WRITE).physical_addr == reservation_addr) {
                        if (trap_pending) {
                            // translate failed
                            return;
                        } else {
                            write_memory<uint32_t>(reg_file[rs1()], reg_file[rs2()]);
                            reservation_addr = 0xFFFFFFFF;
                            set_reg(rd(), 0);
                        }
                    } else {
                        set_reg(rd(), 1);
                    }
                    reservation_valid = false;
                    break;
                case 0x04:
                    // AMOXOR.W
                    reservation_valid = false;
                    temp = read_memory<uint32_t>(reg_file[rs1()], ACCESS_READ);
                    if (!trap_pending) {
                        write_memory<uint32_t>(reg_file[rs1()], temp ^ reg_file[rs2()]);
                        set_reg(rd(), temp);
                    }
                    break;
                case 0x08:
                    // AMOOR.W
                    reservation_valid = false;
                    temp = read_memory<uint32_t>(reg_file[rs1()], ACCESS_READ);
                    if (!trap_pending) {
                        write_memory<uint32_t>(reg_file[rs1()], temp | reg_file[rs2()]);
                        set_reg(rd(), temp);
                    }
                    break;
                case 0x0C:
                    // AMOAND.W
                    reservation_valid = false;
                    temp = read_memory<uint32_t>(reg_file[rs1()], ACCESS_READ);
                    if (!trap_pending) {
                        write_memory<uint32_t>(reg_file[rs1()], temp & reg_file[rs2()]);
                        set_reg(rd(), temp);
                    }
                    break;
                case 0x10:
                    // AMOMIN.W
                    reservation_valid = false;
                    temp = read_memory<uint32_t>(reg_file[rs1()], ACCESS_READ);
                    if (!trap_pending) {
                        write_memory<uint32_t>(reg_file[rs1()], std::min(static_cast<int32_t>(temp), static_cast<int32_t>(reg_file[rs2()])));
                        set_reg(rd(), temp);
                    }
                    break;
                case 0x14:
                    // AMOMAX.W
                    reservation_valid = false;
                    temp = read_memory<uint32_t>(reg_file[rs1()], ACCESS_READ);
                    if (!trap_pending) {
                        write_memory<uint32_t>(reg_file[rs1()], std::max(static_cast<int32_t>(temp), static_cast<int32_t>(reg_file[rs2()])));
                        set_reg(rd(), temp);
                    }
                    break;
                case 0x18:
                    // AMOMINU.W
                    reservation_valid = false;
                    temp = read_memory<uint32_t>(reg_file[rs1()], ACCESS_READ);
                    if (!trap_pending) {
                        write_memory<uint32_t>(reg_file[rs1()], std::min(temp, reg_file[rs2()]));
                        set_reg(rd(), temp);
                    }
                    break;
                case 0x1C:
                    // AMOMAXU.W
                    reservation_valid = false;
                    temp = read_memory<uint32_t>(reg_file[rs1()], ACCESS_READ);
                    if (!trap_pending) {
                        write_memory<uint32_t>(reg_file[rs1()], std::max(temp, reg_file[rs2()]));
                        set_reg(rd(), temp);
                    }
                    break;
                default:
                    take_trap(CAUSE_ILLEGALI, inst_reg);
            }
            break;
        case MISC_MEM:
            switch (funct3()) {
                case 0x0:
                    // FENCE
                    break;
                case 0x1:
                    // FENCE.I
                    break;
                default:
                    take_trap(CAUSE_ILLEGALI, inst_reg);
            }
            break;
        default:
            take_trap(CAUSE_ILLEGALI, inst_reg);
    }
    minstret++;
};