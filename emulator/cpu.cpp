#include "cpu.hpp"

#include "bus.hpp"

#include <algorithm>

CPU::CPU(Bus* busptr, uint32_t ram_start) : bus(busptr), next_pc(ram_start) {
}
uint32_t CPU::check_access(uint32_t pte, uint32_t access_type) {
    bool r = (pte >> 1) & 0x1;
    bool w = (pte >> 2) & 0x1;
    bool x = (pte >> 3) & 0x1;
    bool u = (pte >> 4) & 0x1;
    if (privilege == 0 && u == 0) {
        page_fault(access_type);
        return 0;
    }
    if (privilege == 1 && u == 1) {
        page_fault(access_type);
        return 0;
    }
    if (access_type == 0 && r == 0) {
        take_trap(CAUSE_PAGE_FAULT_LOAD);
        return 0;
    }
    if (access_type == 1 && w == 0) {
        take_trap(CAUSE_PAGE_FAULT_STORE);
        return 0;
    }
    if (access_type == 2 && x == 0) {
        take_trap(CAUSE_PAGE_FAULT_INST);
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
    uint32_t pte1 = bus->ram.read<uint32_t>(pte1_address);
    bool pte1_valid = pte1 & 0x1;
    if (pte1_valid) {
        // simplified form, extract RWX
        if ((pte1 & 0xE) == 0) {
            // pointer
            uint32_t pte0_address = (((pte1 >> 10) & 0x3FFFFF) * 4096) + (vpn0 * 4);
            uint32_t pte0 = bus->ram.read<uint32_t>(pte0_address);
            bool pte0_valid = pte0 & 0x1;
            if (pte0_valid) {
                if ((pte0 & 0xE) == 0) {
                    page_fault(access_type);
                    return {0, 0};
                } else {
                    if (check_access(pte0, access_type) == 0) {
                        return {0, 0};
                    } else {
                        uint32_t ppn = (pte0 >> 10) & 0x3FFFFF;
                        uint32_t physical_addr = (ppn << 12) | offset;
                        return {physical_addr, pte0};
                    }
                }
            } else {
                page_fault(access_type);
                return {0, 0};
            }
        } else {
            // superpage (4MB)
            uint32_t pte1_ppn = (pte1 >> 10) & 0x3FFFFF;
            if ((pte1_ppn & 0x3FF) != 0) {
                page_fault(access_type);
                return {0, 0};
            } else {
                if (check_access(pte1, access_type) == 0) {
                    return {0, 0};
                } else {
                    uint32_t physical_addr = ((pte1_ppn << 12) | (vpn0 << 12)) | offset;
                    return {physical_addr, pte1};
                }
            }
        }
    } else {
        page_fault(access_type);
        return {0, 0};
    }
}
template <typename T>
T CPU::read_memory(uint32_t virt, uint32_t access_type) {
    if (privilege == 3 || ((csr_file[CSR_SATP] & 0x80000000) == 0)) {
        return bus->ram.read<T>(virt);
    } else {
        // TLB lookup
        uint32_t full_vpn = virt >> 12;
        uint32_t tlb_index = full_vpn & 0xFF;
        uint32_t physical_addr = 0;
        if (tlb[tlb_index].valid && tlb[tlb_index].vpn == full_vpn) {
            if (check_access(tlb[tlb_index].permissions, access_type) == 1) {
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
            return bus->ram.read<T>(physical_addr);
        }
    }
}
void CPU::tick() {
    trap_pending = false;
    // increment instructions executed
    mcycle++;
    minstret++;
    // update pc, current instruction, opcode
    pc = next_pc;
    inst_reg = bus->read<uint32_t>(pc);
    uint32_t opcode = inst_reg & 0x7F;
    // default value for next_pc (can be overriden)
    next_pc = pc + 4;
    // update MSIP (software interrupt pending) and MTIP (timer interrupt pending) from CLINT
    if (bus->clint.MTIP) {
        set_csr(CSR_MIP, get_csr(CSR_MIP) | (1 << 7));
    } else {
        set_csr(CSR_MIP, get_csr(CSR_MIP) & ~(1 << 7));
    }
    if (bus->clint.MSIP) {
        set_csr(CSR_MIP, get_csr(CSR_MIP) | (1 << 3));
    } else {
        set_csr(CSR_MIP, get_csr(CSR_MIP) & ~(1 << 3));
    }
    // handle traps and interrupts
    uint32_t mstatus = get_csr(CSR_MSTATUS);
    uint32_t mie = get_csr(CSR_MIE);
    uint32_t mip = get_csr(CSR_MIP);
    // if accepting interrupts
    bool interrupts_enabled = (privilege < 3) || (privilege == 3 && (mstatus & (1 << 3)));
    if (interrupts_enabled) {
        if ((mip & (1 << 11)) && (mie & (1 << 11))) {
            // external/PLIC interrupt
            take_trap(CAUSE_MEI);
            return;
        } else if ((mip & (1 << 3)) && (mie & (1 << 3))) {
            // software interrupt
            take_trap(CAUSE_MSI);
            return;
        } else if ((mip & (1 << 7)) && (mie & (1 << 7))) {
            // timer interrupt
            take_trap(CAUSE_MTI);
            return;
        }
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
                            take_trap(CAUSE_ILLEGALI);
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
                            take_trap(CAUSE_ILLEGALI);
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
                    take_trap(CAUSE_ILLEGALI);
            }
            break;
        case LOAD:
            switch (funct3()) {
                case 0x0:
                    // Load Byte (sign extend)
                    set_reg(rd(), static_cast<uint32_t>(static_cast<int8_t>(bus->read<uint8_t>(reg_file[rs1()] + imm_i()))));
                    break;
                case 0x1:
                    // Load Halfword (sign extend)
                    set_reg(rd(), static_cast<uint32_t>(static_cast<int16_t>(bus->read<uint16_t>(reg_file[rs1()] + imm_i()))));
                    break;
                case 0x2:
                    // Load Word (sign extend)
                    set_reg(rd(), bus->read<uint32_t>(reg_file[rs1()] + imm_i()));
                    break;
                case 0x4:
                    // Load Byte (zero extend)
                    set_reg(rd(), static_cast<uint32_t>(bus->read<uint8_t>(reg_file[rs1()] + imm_i())));
                    break;
                case 0x5:
                    // Load Halfword (zero extend)
                    set_reg(rd(), static_cast<uint32_t>(bus->read<uint16_t>(reg_file[rs1()] + imm_i())));
                    break;
                default:
                    take_trap(CAUSE_ILLEGALI);
            }
            break;
        case STORE:
            switch (funct3()) {
                case 0x0:
                    // Store Byte
                    bus->write<uint8_t>(reg_file[rs1()] + imm_s(), static_cast<uint8_t>(reg_file[rs2()]));
                    break;
                case 0x1:
                    // Store Halfword
                    bus->write<uint16_t>(reg_file[rs1()] + imm_s(), static_cast<uint16_t>(reg_file[rs2()]));
                    break;
                case 0x2:
                    // Store Word
                    bus->write<uint32_t>(reg_file[rs1()] + imm_s(), reg_file[rs2()]);
                    break;
                default:
                    take_trap(CAUSE_ILLEGALI);
            }
            break;
        case BRANCH:
            switch (funct3()) {
                case 0x0:
                    // BEQ
                    if (reg_file[rs1()] == reg_file[rs2()]) next_pc = pc + imm_b();
                    break;
                case 0x1:
                    // BNE
                    if (reg_file[rs1()] != reg_file[rs2()]) next_pc = pc + imm_b();
                    break;
                case 0x4:
                    // BLT (signed)
                    if (static_cast<int32_t>(reg_file[rs1()]) < static_cast<int32_t>(reg_file[rs2()])) next_pc = pc + imm_b();
                    break;
                case 0x5:
                    // BGE (signed)
                    if (static_cast<int32_t>(reg_file[rs1()]) >= static_cast<int32_t>(reg_file[rs2()])) next_pc = pc + imm_b();
                    break;
                case 0x6:
                    // BLTU
                    if (reg_file[rs1()] < reg_file[rs2()]) next_pc = pc + imm_b();
                    break;
                case 0x7:
                    // BGEU
                    if (reg_file[rs1()] >= reg_file[rs2()]) next_pc = pc + imm_b();
                    break;
                default:
                    take_trap(CAUSE_ILLEGALI);
            }
            break;
        case JAL:
            set_reg(rd(), next_pc);
            next_pc = pc + imm_j();
            break;
        case JALR:
            temp = (reg_file[rs1()] + imm_i()) & ~1;
            set_reg(rd(), next_pc);
            next_pc = temp;
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
                            }
                            break;
                        case 0x001:
                            // EBREAK
                            take_trap(CAUSE_EBREAK);
                            break;
                        case 0x102:
                            // SRET
                            if (privilege == 0) {
                                take_trap(CAUSE_ILLEGALI);
                                break;
                            }
                            next_pc = get_csr(CSR_SEPC);
                            privilege = (get_csr(CSR_MSTATUS) >> 8) & 1;
                            // copy SPIE to SIE
                            set_csr(CSR_MSTATUS, (get_csr(CSR_MSTATUS) & ~(1U << 1)) | (((get_csr(CSR_MSTATUS) >> 5) & 1U) << 1));
                            set_csr(CSR_MSTATUS, get_csr(CSR_MSTATUS) | (1U << 5));
                            set_csr(CSR_MSTATUS, get_csr(CSR_MSTATUS) & ~(1U << 8));
                            break;
                        case 0x302:
                            // MRET
                            if (privilege < 3) {
                                take_trap(CAUSE_ILLEGALI);
                                break;
                            }
                            next_pc = get_csr(CSR_MEPC);
                            privilege = (get_csr(CSR_MSTATUS) >> 11) & 3;
                            // copy MPIE to MIE
                            set_csr(CSR_MSTATUS, (get_csr(CSR_MSTATUS) & ~(1U << 3)) | (((get_csr(CSR_MSTATUS) >> 7) & 1U) << 3));
                            set_csr(CSR_MSTATUS, get_csr(CSR_MSTATUS) | (1U << 7));
                            set_csr(CSR_MSTATUS, get_csr(CSR_MSTATUS) & ~(3U << 11));
                            break;
                        default:
                            take_trap(CAUSE_ILLEGALI);
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
                    // CSRRWI
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
                                take_trap(CAUSE_ILLEGALI);
                                break; 
                            } else {
                                flush_tlb();
                                break;
                            }
                        } else {
                            take_trap(CAUSE_ILLEGALI);
                            break;
                        }
                    } else {
                        take_trap(CAUSE_ILLEGALI);
                    }
            }
            break;
        case ATOMIC:
            if ((reg_file[rs1()] & 0b11) != 0) {
                set_csr(CSR_MEPC, pc);
                if (funct5() == 0x02) {
                    take_trap(CAUSE_ATOMIC_LOAD_ALIGN);
                } else {
                    take_trap(CAUSE_ATOMIC_ALIGN);
                }
                break;
            }
            switch (funct5()) {
                case 0x00:
                    // AMOADD.W
                    reservation_valid = false;
                    temp = bus->read<uint32_t>(reg_file[rs1()]);
                    bus->write<uint32_t>(reg_file[rs1()], temp + reg_file[rs2()]);
                    set_reg(rd(), temp);
                    break;
                case 0x01:
                    // AMOSWAP.W
                    reservation_valid = false;
                    temp = bus->read<uint32_t>(reg_file[rs1()]);
                    bus->write<uint32_t>(reg_file[rs1()], reg_file[rs2()]);
                    set_reg(rd(), temp);
                    break;
                case 0x02:
                    // LR.W
                    temp = reg_file[rs1()];
                    set_reg(rd(), bus->read<uint32_t>(temp));
                    reservation_addr = temp;
                    reservation_valid = true;
                    break;
                case 0x03:
                    // SC.W
                    if (reservation_valid && reg_file[rs1()] == reservation_addr) {
                        bus->write<uint32_t>(reg_file[rs1()], reg_file[rs2()]);
                        reservation_addr = 0xFFFFFFFF;
                        set_reg(rd(), 0);
                    } else {
                        set_reg(rd(), 1);
                    }
                    reservation_valid = false;
                    break;
                case 0x04:
                    // AMOXOR.W
                    reservation_valid = false;
                    temp = bus->read<uint32_t>(reg_file[rs1()]);
                    bus->write<uint32_t>(reg_file[rs1()], temp ^ reg_file[rs2()]);
                    set_reg(rd(), temp);
                    break;
                case 0x08:
                    // AMOOR.W
                    reservation_valid = false;
                    temp = bus->read<uint32_t>(reg_file[rs1()]);
                    bus->write<uint32_t>(reg_file[rs1()], temp | reg_file[rs2()]);
                    set_reg(rd(), temp);
                    break;
                case 0x0C:
                    // AMOAND.W
                    reservation_valid = false;
                    temp = bus->read<uint32_t>(reg_file[rs1()]);
                    bus->write<uint32_t>(reg_file[rs1()], temp & reg_file[rs2()]);
                    set_reg(rd(), temp);
                    break;
                case 0x10:
                    // AMOMIN.W
                    reservation_valid = false;
                    temp = bus->read<uint32_t>(reg_file[rs1()]);
                    bus->write<uint32_t>(reg_file[rs1()], std::min(static_cast<int32_t>(temp), static_cast<int32_t>(reg_file[rs2()])));
                    set_reg(rd(), temp);
                    break;
                case 0x14:
                    // AMOMAX.W
                    reservation_valid = false;
                    temp = bus->read<uint32_t>(reg_file[rs1()]);
                    bus->write<uint32_t>(reg_file[rs1()], std::max(static_cast<int32_t>(temp), static_cast<int32_t>(reg_file[rs2()])));
                    set_reg(rd(), temp);
                    break;
                case 0x18:
                    // AMOMINU.W
                    reservation_valid = false;
                    temp = bus->read<uint32_t>(reg_file[rs1()]);
                    bus->write<uint32_t>(reg_file[rs1()], std::min(temp, reg_file[rs2()]));
                    set_reg(rd(), temp);
                    break;
                case 0x1C:
                    // AMOMAXU.W
                    reservation_valid = false;
                    temp = bus->read<uint32_t>(reg_file[rs1()]);
                    bus->write<uint32_t>(reg_file[rs1()], std::max(temp, reg_file[rs2()]));
                    set_reg(rd(), temp);
                    break;
                default:
                    take_trap(CAUSE_ILLEGALI);
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
                    take_trap(CAUSE_ILLEGALI);
            }
            break;
        default:
            take_trap(CAUSE_ILLEGALI);
    }
};