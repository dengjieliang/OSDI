ENTRY(_start)
SECTIONS
{
  . = 0x80000;
  .text : { *(.text .text.*) }
  .rodata : {*(.rodata .rodata.*)}
  .data : {*(.data .data.*)}
  . = ALIGN(16);
  __bss_start = .;
  .bss : {*(.bss .bss.*) *(COMMON)}
  __bss_end = .;
  . = ALIGN(16);
  _stack_top = __bss_end + 0x04000;
}
