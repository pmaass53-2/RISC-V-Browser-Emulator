#ifndef UART_H
#define UART_H

volatile unsigned char * const UART0 = (unsigned char *)0x10000000;

void print_str(const char *s) {
    while(*s != '\0') {
        *UART0 = (unsigned char)(*s);
        s++;
    }
}

#endif