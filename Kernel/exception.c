#include "../Driver/uart.h"
#include "../Driver/early_uart.h"
#include "../Kernel/time_manager.h"
#include "../Board/fdtb.h"
#include "../Kernel/exception.h"
#include "../Kernel/io_task_queue.h"
#include "../Board/common.h"
#include "../Shell/shell.h"

void mask_all_exceptions(void)
{
    // daifset 的 immediate bit meaning (AArch64):
    // bit0 -> F (FIQ)
    // bit1 -> I (IRQ)
    // bit2 -> A (SError)
    // bit3 -> D (Debug)
    //
    // #0xf means set all four masks.
    __asm__ volatile(
        "msr daifset, #0xf \n"
        "isb                \n"   // ensure the mask takes effect immediately
        :   //No output
        :   //No input
        : "memory"  // prevent compiler reordering
    );
}

void unmask_all_exceptions(void)
{
    // daifclr 的 immediate bit meaning (AArch64):
    // bit0 -> F (FIQ)
    // bit1 -> I (IRQ)
    // bit2 -> A (SError)
    // bit3 -> D (Debug)
    //
    // #0xf means clear all four masks (enable interrupts).
    __asm__ volatile(
        "msr daifclr, #0xf \n"
        "isb                \n"   // ensure the unmask takes effect immediately
        :   //No output
        :   //No input
        : "memory"  // prevent compiler reordering
    );
}

static inline unsigned long read_far_el1(void)
{
    unsigned long v;
    __asm__ volatile(
        "mrs %0, far_el1 \n"
        : "=r"(v)
        :
        : "memory"
    );
    return v;
}

static void irq_routing(unsigned long elr, unsigned long spsr, unsigned long *ctx)
{
    extern CtxT dtb_ctx;

    // --- Phase 1: Source detection ---
    // IRQs are already masked by hardware on exception entry (DAIF.I set in PSTATE).
    // We do only source-check + minimal data movement here; heavy work is deferred.
    if (dtb_ctx.interrupt_info.have_arm_local_intc_base)
    {
        // 讀取 Core 0 Interrupt Source
        unsigned int irq_src = mmio_read(dtb_ctx.interrupt_info.arm_local_intc_base + 0x60);

        // Bit 1 (CNTPNSIRQ): Core Timer Interrupt
        if (irq_src & (1 << 1))
        {
            // TODO (Group 4): replace with io_task_enqueue_timer() to fully decouple.
            timer_interrupt_router();
            shell_notify_async_event();
        }

        // Bit 8: GPU interrupt pending → AUX UART via second controller Bit 29
        if (irq_src & (1 << 8))
        {
            if (dtb_ctx.interrupt_info.have_arm_ctrl_intc_base)
            {
                unsigned int uart_irq_pending = mmio_read(dtb_ctx.interrupt_info.arm_ctrl_intc_base + 0x04);
                // TODO (Group 5): replace with io_task_enqueue_uart_rx/tx() to fully decouple.
                if (uart_irq_pending & (1 << 29))
                {
                    uart_interrupt_handler();
                }
            }
        }

        if (irq_src && ((irq_src & (1 << 1)) == 0) && ((irq_src & (1 << 8)) == 0))
        {
            // 處理未知的中斷
            early_uart_puts("\n[IRQ] Unknown IRQ source: 0x");
            early_uart_send_hex(irq_src);
            early_uart_puts("\n");
        }
    }

    // --- Phase 2: Run deferred tasks with IRQs re-enabled ---
    // Unmask IRQs so tasks may be preempted by higher-priority interrupts.
    // ERET will restore DAIF from SPSR_EL1, which had IRQs enabled before this handler.
    unmask_all_exceptions();
    io_task_run_all();
}

void default_handler_dump_c(unsigned long esr, unsigned long elr, unsigned long spsr)
{
    early_uart_puts("\n[DEFAULT EXCEPTION]\n");
    early_uart_puts("  ESR_EL1 = 0x"); 
    early_uart_send_unsigned_long_integer(esr);  
    early_uart_puts("\n");
    early_uart_puts("  ELR_EL1 = 0x"); 
    early_uart_send_unsigned_long_integer(elr);  
    early_uart_puts("\n");
    early_uart_puts("  SPSR_EL1= 0x"); 
    early_uart_send_unsigned_long_integer(spsr); 
    early_uart_puts("\n");

    unsigned long far = read_far_el1();
    early_uart_puts("  FAR_EL1 = 0x"); 
    early_uart_send_unsigned_long_integer(far);
    early_uart_puts("\n");

    while (1)
    {
        // infinite loop
    }
}

void el0_sync_handler_c(unsigned long esr, unsigned long elr, unsigned long spsr, unsigned long *ctx)
{
    unsigned long exception_class = (esr >> 26) & 0x3F;
    unsigned long instruction_specific_syndrome = esr & 0x01FFFFFF;

    early_uart_puts("\n[EL0 SYNC]\n");
    early_uart_puts("  ESR_EL1 = 0x"); 
    early_uart_send_unsigned_long_integer(esr);  
    early_uart_puts("\n");
    early_uart_puts("  ELR_EL1 = 0x"); 
    early_uart_send_unsigned_long_integer(elr);  
    early_uart_puts("\n");
    early_uart_puts("  SPSR_EL1= 0x"); 
    early_uart_send_unsigned_long_integer(spsr); 
    early_uart_puts("\n");
    early_uart_puts("  EC      = 0x"); 
    early_uart_send_unsigned_long_integer(exception_class);   
    early_uart_puts("\n");

    if (exception_class == 0x15) // SVC from AArch64
    {
        // SVC 的 immediate 通常在 ISS 的低 16 bits（imm16）
        unsigned long imm16 = instruction_specific_syndrome & 0xffff;
        early_uart_puts("  ISS     = 0x"); 
        early_uart_send_unsigned_long_integer(imm16);   
        early_uart_puts("\n");
        return;
    }

    unsigned long far = read_far_el1();
    early_uart_puts("  FAR_EL1 = 0x"); 
    early_uart_send_unsigned_long_integer(far);
    early_uart_puts("\n");

    while (1)
    {
        // infinite loop
    }
}

void el0_irq_handler_c(unsigned long elr, unsigned long spsr, unsigned long *ctx)
{
    irq_routing(elr, spsr,ctx);
}

void el1_irq_handler_c(unsigned long elr, unsigned long spsr, unsigned long *ctx)
{
    irq_routing(elr, spsr, ctx);
}