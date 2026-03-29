OUTPUT_ARCH( "riscv" )
ENTRY( _start )

MEMORY
{
  ram (wxa) : ORIGIN = 0x80000000, LENGTH = 32M
}

SECTIONS
{
  .text : {
    *(.text.init)
    *(.text)
  } > ram

  .data : { *(.data) } > ram
  .bss : { *(.bss) } > ram
}