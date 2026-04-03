    /*a = SHF_ALLOC, x = SHF_EXECINSTR*/
    .section .text, "ax"

    .globl exception_vector_table
    .type exception_vector_table, %object
    .global default_handler
    .type   default_handler, %function
    .global el0_sync_entry
    .type   el0_sync_entry, %function
    .global el0_irq_entry
    .type   el0_irq_entry, %function
    .global el1_irq_entry
    .type   el1_irq_entry, %function
    .extern el0_sync_handler_c
    .extern el0_irq_handler_c
    .extern default_handler_dump_c
    .extern el1_irq_handler_c
    .globl set_exception_vector_table
    .type set_exception_vector_table, %function

    .macro SAVE_ALL
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x8, x9, [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    stp x16, x17, [sp, #-16]!
    stp x18, x19, [sp, #-16]!
    stp x20, x21, [sp, #-16]!
    stp x22, x23, [sp, #-16]!
    stp x24, x25, [sp, #-16]!
    stp x26, x27, [sp, #-16]!
    stp x28, x29, [sp, #-16]!
    stp x30, xzr, [sp, #-16]!
    .endm

    .macro RESTORE_ALL
    ldp x30, xzr, [sp], #16
    ldp x28, x29, [sp], #16
    ldp x26, x27, [sp], #16
    ldp x24, x25, [sp], #16
    ldp x22, x23, [sp], #16
    ldp x20, x21, [sp], #16
    ldp x18, x19, [sp], #16
    ldp x16, x17, [sp], #16
    ldp x14, x15, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x8, x9, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x4, x5, [sp], #16
    ldp x2, x3, [sp], #16
    ldp x0, x1, [sp], #16
    .endm

    .macro SAVE_ELR_SPSR
    mrs x9, elr_el1
    mrs x10, spsr_el1
    stp x9, x10, [sp, #-16]!
    .endm

    .macro RESTORE_ELR_SPSR
    ldp x9, x10, [sp], #16
    msr elr_el1, x9
    msr spsr_el1, x10
    isb
    .endm

.balign 0x800      // 2KB (0x800)
exception_vector_table:
    // ---- Group 1: Current EL, using SP_EL0 (offset 0x000) ----
    /*0x000: Synchronous*/
    b   default_handler
    .align  7
    /*0x080: IRQ*/
    b   default_handler
    .align  7
    /*0x100: FIQ*/
    b   default_handler
    .align  7
    /*0x180: SError*/
    b   default_handler
    .align  7
    // ---- Group 2: Current EL, using SP_ELx (offset 0x200) ----
    /*0x200: Synchronous*/
    b default_handler
    .align 7
    /*0x280: IRQ*/
    b el1_irq_entry
    .align 7
    /*0x300: FIQ*/
    b default_handler
    .align 7
    /*0x380: SError*/
    b default_handler
    .align 7
    // ---- Group 3: Lower EL, AArch64 (offset 0x400) ----
    /*0x400: Synchronous*/
    b el0_sync_entry
    .align 7
    /*0x480: IRQ*/
    b el0_irq_entry
    .align 7
    /*0x500: FIQ*/
    b default_handler
    .align 7
    /*0x580: SError*/
    b default_handler
    .align 7
    // ---- Group 4: Lower EL, AArch32 (offset 0x600) ----
    /*0x600: Synchronous*/
    b default_handler
    .align 7
    /*0x680: IRQ*/
    b default_handler
    .align 7
    /*0x700: FIQ*/
    b default_handler
    .align 7
    /*0x780: SError*/
    b default_handler
    .align 7

default_handler:
    msr daifset, #0xf
    isb

    SAVE_ALL

    /*MRS = Move from System register*/
    /*x0 = ESR_EL1 (Exception Syndrome / reason)*/
    mrs x0, esr_el1

    /*x1 = ELR_EL1 (return address / faulting PC)*/
    mrs x1, elr_el1

    /*x2 = SPSR_EL1 (saved PSTATE)*/
    mrs x2, spsr_el1
    bl  default_handler_dump_c
    RESTORE_ALL
    
1:  wfe
    b   1b

el0_sync_entry:
    SAVE_ALL
    /*x0 = ESR_EL1 (Exception Syndrome / reason)*/
    mrs x0, esr_el1

    /*x1 = ELR_EL1 (return address / faulting PC)*/
    mrs x1, elr_el1

    /*x2 = SPSR_EL1 (saved PSTATE)*/
    mrs x2, spsr_el1

    /*x3 = pass pointer to saved GPR context*/
    mov x3, sp
    bl  el0_sync_handler_c
    RESTORE_ALL
    eret

el0_irq_entry:
    SAVE_ALL
    SAVE_ELR_SPSR
    /* +2026/03/28 先把 ELR/SPSR 從目前 IRQ frame 讀到 x0/x1，傳給 C handler 作診斷/分派使用。 */
    /* +2026/03/28 不能直接把 sp 當 ctx 傳入，因為 [sp..sp+15] 現在是 ELR/SPSR frame，不是 GPR frame。 */
    /* +2026/03/28 所以 x2 需要用 sp+16，讓 C handler 看到固定且可擴充的 GPR context 佈局。 */
    /*x0 = ELR_EL1 (return address / faulting PC)*/
    ldr x0, [sp]

    /*x1 = SPSR_EL1 (saved PSTATE)*/
    ldr x1, [sp, #8]

    /*x2 = pass pointer to saved GPR context*/
    add x2, sp, #16

    bl  el0_irq_handler_c
    RESTORE_ELR_SPSR
    RESTORE_ALL
    eret

el1_irq_entry:
    SAVE_ALL
    SAVE_ELR_SPSR

    /* +2026/03/28 與 el0_irq_entry 相同：先載入本層保存的 ELR/SPSR，再把 GPR frame 指標傳給 C。 */
    /* +2026/03/28 這能確保 nested IRQ 時每層 return state 都用自己的 stack frame，不會互相覆寫。 */
    /*x0 = ELR_EL1 (return address / faulting PC)*/
    ldr x0, [sp]

    /*x1 = SPSR_EL1 (saved PSTATE)*/
    ldr x1, [sp, #8]

    /*x2 = pass pointer to saved GPR context*/
    add x2, sp, #16
    bl  el1_irq_handler_c

    RESTORE_ELR_SPSR
    RESTORE_ALL
    eret

set_exception_vector_table:
    /*x0 = (address(exception_vector_table) & ~0xFFF)（page base）*/
    adrp x0, exception_vector_table

    /*x0 = page_base(symbol) + (symbol & 0xFFF)*/
    add  x0, x0, :lo12:exception_vector_table
    
    /*MSR = Move to System register*/
    msr vbar_el1, x0    /* Install vector base for EL1*/
    isb
    ret

    