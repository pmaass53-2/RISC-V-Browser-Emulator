riscv-none-elf-gcc -O2 -march=rv32ima -mabi=ilp32 -ffreestanding -nostdlib -T linker.ld start.S main.c -o payload.elf
riscv-none-elf-objcopy -O binary payload.elf payload.bin