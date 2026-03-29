riscv-none-embed-gcc -O2 -march=rv32ima -mabi=ilp32 -ffreestanding -nostdlib -T linker.ld start.S main.c -o payload.elf
riscv-none-embed-objcopy -O binary payload.elf payload.bin