    .section .text "ax"
    .globl user_program

user_program:
    mov x0, 0
1:
    add x0, x0, 1
    svc 0
    cmp x0, 5
    blt 1b //less than則跳回最近的1 label
2:
    b 2b