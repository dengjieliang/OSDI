    .section ".stack_mem"          /*未初始化資料段*/
    .align 16                 /*對齊到16位元組邊界*/
_stack_start:
    .skip 4096 * 4       /*預留 16KB (4頁) 作為堆疊空間*/
_stack_top:              /*堆疊頂部標籤*/

    .section ".bss"          /*未初始化資料段*/
    .align 8                  /*對齊到8位元組邊界*/
    .section ".text"         /*程式碼段*/
    .globl _start
    
_start:
    ldr x0, = _stack_top  /*將堆疊頂部地址載入 x0*/
    mov sp, x0           /*設定堆疊指標 (sp) 為堆疊頂部*/
    ldr x1, = __bss_start /*將bss起始地址載入 x1*/
    ldr x2, = __bss_end  /*將bss結尾地址載入 x2*/

_bss_clear_loop:
    cmp x1, x2           /*比較當前地址與bss結尾地址*/
    b.ge __bss_end   /*如果當前地址 >= 結尾地址，跳轉到完成標籤*/
    str xzr, [x1], #8    /*將0寫入當前地址，然後地址加8*/
    b _bss_clear_loop     /*重複清除下一個位元組*/

__bss_end:
    wfe
    b _start
