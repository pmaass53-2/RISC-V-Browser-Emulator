volatile unsigned char * const UART0 = (unsigned char *)0x10000000;

void print_str(const char *s) {
    while(*s != '\0') {
        *UART0 = (unsigned char)(*s);
        s++;
    }
}

void test_assert(const char* test_name, int condition) {
    print_str(test_name);
    if (condition) {
        print_str(": PASS\n");
    } else {
        print_str(": FAIL (!!!)\n");
    }
}

int main() {
    print_str("Hello Silas\n");
    print_str("\n--- RISC-V RV32IMA CPU SMOKE TEST ---\n");

    // 1. ARITHMETIC (ADD / SUB)
    volatile int a = 15;
    volatile int b = 27;
    test_assert("Arithmetic ADD", (a + b) == 42);
    test_assert("Arithmetic SUB", (b - a) == 12);

    // 2. LOGIC (AND / OR / XOR)
    volatile unsigned int bits1 = 0xAA; // 10101010
    volatile unsigned int bits2 = 0x55; // 01010101
    test_assert("Logic AND", (bits1 & bits2) == 0x00);
    test_assert("Logic OR",  (bits1 | bits2) == 0xFF);
    test_assert("Logic XOR", (bits1 ^ 0xFF) == 0x55);

    // 3. SHIFTS (SLL / SRL / SRA)
    volatile int shift_val = 1;
    volatile int neg_val = -8; 
    test_assert("Shift Left Logical (SLL)", (shift_val << 3) == 8);
    test_assert("Shift Right Logical (SRL)", ((unsigned int)0x80000000 >> 1) == 0x40000000);
    // Arithmetic right shift preserves the sign bit
    test_assert("Shift Right Arith (SRA)", (neg_val >> 1) == -4); 

    // 4. BRANCHING & COMPARISONS (SLT / BEQ / BNE)
    volatile int small = 5;
    volatile int big = 100;
    test_assert("Branch Less Than (BLT/SLT)", small < big);
    test_assert("Branch Greater Than (BGE)", big > small);

    // 5. MEMORY ALIGNMENT (LB / LH / LW / SB / SH / SW)
    volatile unsigned int mem_test[2]; // Array on the stack
    mem_test[0] = 0xDEADBEEF;
    mem_test[1] = 0x12345678;
    
    volatile unsigned char* byte_ptr = (volatile unsigned char*)&mem_test[0];
    // Little-endian check: The lowest byte of 0xDEADBEEF should be 0xEF
    test_assert("Memory Load Byte (LB)", byte_ptr[0] == 0xEF);
    test_assert("Memory Load Word (LW)", mem_test[1] == 0x12345678);

    // 6. MULTIPLICATION (MUL - tests the 'M' in RV32IMA)
    volatile int m1 = 7;
    volatile int m2 = 6;
    test_assert("Multiply (MUL)", (m1 * m2) == 42);

    print_str("--- TESTS COMPLETE ---\n\n");
    
    // Trap the CPU in a loop when finished
    while (1) {}
    return 0;
}

// Bare-metal startup code
/*asm (
    ".section .text.init\n"
    ".global _start\n"
    "_start:\n"
    "li sp, 0x82000000\n"
    "call main\n"
);*/