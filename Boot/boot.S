    .section ".text"         /*程式碼段*/

    .globl relocate_kernel           /* 定義 relocate_kernel 為global */
    .type relocate_kernel, %function   /*重搬kernel function*/
    
    .globl clear_bss           /* 定義 clear_bss 為global */
    .type clear_bss, %function  /* 定義 clear_bss 為function */

    .globl from_el2_to_el1 /*定義 from_el2_to_el1 為global*/
    .type from_el2_to_el1, %function /*定義 from_el2_to_el1 為function*/

    .extern kernel_main        /* 宣告 kernel_main 函式為外部函式 */

    .globl _start
    
_start:
    mov x19, x0         /*將x0 變數搬到x19*/
    bl relocate_kernel
    ldr x3, = _stack_top  /*將堆疊頂部地址載入 x3*/
    mov sp, x3           /*設定堆疊指標 (sp) 為堆疊頂部*/
    ldr x0, = __bss_start /*將bss起始地址載入 x0*/
    ldr x1, = __bss_end  /*將bss結尾地址載入 x1*/
    bl clear_bss         /*呼叫bss清零函式，使用bl的話，參數必須根據x0、x1、x2...順序擺放*/ 
    bl from_el2_to_el1  /*設定從el2的權限移至el1*/
    mov x0, x19         /*將x19 變數搬回到x0*/
    bl kernel_main      /* 呼叫 kernel_main 函式 */
    b idle_loop       /* 呼叫 idle_loop 以避免返回 */

relocate_kernel:
    /*x0 現在會存著 _start 在記憶體中的真實位址 (例如 0x80000)*/
    /*=代表讀此位置而不是此位置的數值 */
    adr x0, _start 

    /* x1 現在會存著 linker.ld 裡設定的位址 (例如 0x60000)*/
    /*=代表讀此位置而不是此位置的數值 */
    ldr x1, =_start 
    cmp x0, x1
    beq _done

    ldr x2, =__bss_end
    sub x2, x2, x1
_relocate_loop:
    cbz x2, _relocate_loop_done
    ldr x3, [x0], #8 
    str x3, [x1], #8
    sub x2, x2, #8 //x2減8
    b _relocate_loop
_relocate_loop_done:
    ldr x1, =_start
    mov x0, x19         /*將x19 變數搬回到x0*/
    br x1

_done:
    ret

clear_bss:
    /* 參數：x0 = start, x1 = end */
    
    mov x2, x0                 /* *** 修正 1: 將起始地址從 x0 複製到 x2 (x2是 Caller-saved) *** */
                               /* x2 將作為迴圈指標，這樣 x0 原始值不會被破壞 */
_bss_clear_loop:
    cmp x2, x1                 /* 比較當前地址 (x2) 是否達到結束地址 (x1) */
    b.ge _bss_clear_done       /* 如果 x2 >= x1，跳轉到完成標籤 */
    
    /* 清零：將 64 位元的 0 (xzr) 存入地址 [x2]，然後 x2 地址加 8 */
    str xzr, [x2], #8          
    
    b _bss_clear_loop          /* 重複迴圈 */
_bss_clear_done:
    ret                        /* 從函式返回 */
    .size clear_bss, . - clear_bss   

from_el2_to_el1:
    mrs  x0, CurrentEL
    lsr  x0, x0, #2
    cmp x0, #2
    b.ne .lalready_el1

    ldr x1, = _stack_top
    and x1, x1, #~0xF   /*align 16 bit*/
    msr sp_el1, x1
    mov x0, (1 << 31)   /*進入EL1時"允許"使用arm 64bit指令*/
    msr hcr_el2, x0     /*寫入指定暫存器*/
    /*M[3:0] = 0101：代表返回後進入 EL1h 模式（即使用 EL1 專屬的堆疊指標 SP_EL1）。*/
    /*M[4] = 0：確保處於 64 位元模式。*/
    /*DAIF = 1111 (位元 6-9)：禁用所有類型的異常/中斷（Debug, SError, IRQ, FIQ），確保切換過程安全，直到內核準備好。*/
    mov x0, 0x3c5
    msr spsr_el2, x0    /*選擇 「AArch64 狀態下的 EL1h 模式」*/
    msr elr_el2, lr
    eret

.lalready_el1:
    ret    

idle_loop:
    wfe
    b idle_loop
