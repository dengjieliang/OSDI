    .section ".text"         /*程式碼段*/

    .globl relocate_kernel           /* 定義 relocate_kernel 為global */
    .type relocate_kernel, %function   /*重搬kernel function*/
    
    .globl clear_bss           /* 定義 clear_bss 為global */
    .type clear_bss, %function  /* 定義 clear_bss 為function */
    .extern kernel_main        /* 宣告 kernel_main 函式為外部函式 */

    .globl _start
    
_start:
    bl relocate_kernel
    ldr x3, = _stack_top  /*將堆疊頂部地址載入 x3*/
    mov sp, x3           /*設定堆疊指標 (sp) 為堆疊頂部*/
    ldr x0, = __bss_start /*將bss起始地址載入 x0*/
    ldr x1, = __bss_end  /*將bss結尾地址載入 x1*/
    bl clear_bss         /*呼叫bss清零函式，使用bl的話，參數必須根據x0、x1、x2...順序擺放*/
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

idle_loop:
    wfe
    b idle_loop
