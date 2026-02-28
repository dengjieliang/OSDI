    /*a = SHF_ALLOC, x = SHF_EXECINSTR*/
    .section .text, "ax"

    .globl enter_el0
    .type   enter_el0, %function

/*C 語言宣告：void enter_el0(unsigned long user_start_addr, unsigned long user_stack_top);*/
/*參數 x0 = user_start_addr*/
/*參數 x1 = user_stack_top*/
enter_el0:

    /*return 時切換到 EL0，並且關閉所有硬體中斷*/
    /* 0x000 = EL0t (bit [3:0] = 0b0000)*/
    mov x2, 0x0
    msr spsr_el1, x2

    /*return 時從這個位址開始執行第一行 User Code。*/
    msr elr_el1, x0

    /*準備：EL0 的 stack*/
    msr sp_el0, x1

    isb
    eret