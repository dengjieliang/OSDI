SECTIONS
{
  . = 0x80000;
  .text : { *(.text .text*) }
  .rodata : {*(.rodata .rodata.*)}
  .data : {*(.data .data.*)}
  .bss = __bss_start;
  __bss_start : {*(.bss .bss.*)}
  __bss_end = __bss_start;
  .COMMON : {*(COMMON)}
  _stack_top = __bss_end + 0x10000;
}
