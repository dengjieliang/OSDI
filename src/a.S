    .section ".text"
    .globl _start
_start:
    wfe
    b _start
