OUTPUT_ARCH( "riscv" )
ENTRY( _start )

MEMORY
{
  ram (wxa) : ORIGIN = 0x80000000, LENGTH = 32M
}

SECTIONS
{
  .text : {
    KEEP(*(.text.init))
    *(.text .text.*)
  } > ram

  .rodata : { *(.rodata .rodata.*) } > ram
  .data : { *(.data) } > ram
  .bss : { *(.bss) } > ram
}