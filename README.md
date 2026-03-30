1. Register State Corruption on Traps
  The most critical bug is that many instructions do not stop execution when a trap (exception) occurs. For
  example, in the LOAD implementation:

   1 set_reg(rd(), static_cast<uint32_t>(static_cast<int8_t>(read_memory<uint8_t>(...))));
  If read_memory triggers a page fault, take_trap is called, which updates next_pc to the trap handler.
  However, read_memory returns 0, and set_reg still executes, overwriting the destination register rd with
  0. The instruction should instead abort immediately to preserve the CPU state for the trap handler.

  2. Missing Exception Metadata (MTVAL/STVAL)
  The take_trap function accepts a tval parameter (which maps to mtval or stval), but most callers (like
  page_fault) don't provide the faulting virtual address. This makes it impossible for an OS (like Linux) to
  handle page faults or address misalignments properly. Similarly, illegal instructions should ideally
  report the faulting instruction in mtval.

  3. Incomplete Interrupt Delegation
  The tick() function only checks for Machine-mode interrupts (MIE/MIP). It doesn't correctly handle
  Supervisor-mode interrupts when they are delegated via mideleg. If an interrupt is delegated to S-mode,
  the CPU should check sstatus.SIE and sie to determine if the trap should be taken while in S-mode or
  U-mode.

  4. Missing Privilege and Virtual Memory Checks
  Several CSR-related architectural checks are missing:
   * mstatus.TVM (Trap Virtual Memory): Writing to satp or executing SFENCE.VMA in S-mode should trap if
     TVM=1. The emulator checks this for SFENCE.VMA but not for satp writes.
   * mstatus.TSR (Trap SRET): Executing SRET in S-mode should trap if TSR=1.
   * mstatus.MXR (Make Executable Readable): In check_access, the emulator doesn't honor the MXR bit, which
     allows loads to succeed on executable-only pages.
   * mstatus.MPRV (Modify Privilege): Not implemented; loads and stores should use mstatus.MPP privilege
     when MPRV=1.

  5. Instruction Alignment Check
  The emulator assumes 4-byte aligned instructions but doesn't implement the C (Compressed) extension.
  However, it also doesn't check if pc or next_pc is 4-byte aligned. Jumping to a misaligned address (e.g.,
  via JALR) should trigger an instruction address misaligned exception.

  6. CSR Access Control
  The get_csr function has a default case that simply returns csr_file[csr]. If a program accesses a
  non-existent or unsupported CSR, the emulator should trigger an illegal instruction trap instead of
  returning a potentially stale value. Additionally, MISA (Machine Instruction Set Architecture) and other
  read-only IDs are not initialized.

  7. AMO Instruction Atoms
  While the emulator is single-threaded, the Atomic Memory Operations (AMOs) and LR/SC logic should ideally
  check for write permissions before performing the read to ensure the operation is truly atomic from the
  perspective of the page table. Currently, if the write fails after a successful read, the register file
  might still be updated (similar to the LOAD bug).

  8. UART DLAB Handling
  In UART::write_reg, the DLAB bit (bit 7 of LCR) is correctly used to switch between DLL/THR and DLM/IER.
  However, the code uses hardcoded indices regs[8] through regs[11] for these registers, which are outside
  the standard 0-7 register range. While this works internally, it's non-standard and could lead to
  confusion.

  Would you like me to help fix any of these issues, starting with the register state corruption or the
  interrupt logic?