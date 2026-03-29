.section .text.init
.global _start

_start:
    li sp, 0x82000000
    call main

1: j 1b