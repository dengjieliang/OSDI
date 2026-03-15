#include "../header/uart.h"
#include "../header/time.h"
#include "../header/fdtb.h"
#include "../header/exception.h"

static inline void mask_all_exceptions(void)
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
    // IRQ 進來時，多半是 timer/uart/其他 device
    mask_all_exceptions(); // 可選：避免印訊息時 nested

    async_uart_puts("\n[EL0 IRQ]\n");
    async_uart_puts("  ELR_EL1  = 0x"); 
    async_uart_send_unsigned_long_integer(elr);  
    async_uart_puts("\n");
    async_uart_puts("  SPSR_EL1 = 0x"); 
    async_uart_send_unsigned_long_integer(spsr); 
    async_uart_puts("\n");

    extern CtxT dtb_ctx;

    // 讀取 Core 0 Interrupt Source
    unsigned int irq_src = *((volatile unsigned int*)dtb_ctx.arm_local_interrupt);

    // Bit 1 (值為 2) 代表 CNTPNSIRQ (Core Timer Interrupt)
    if (irq_src == 2)
    {
        async_uart_puts("Core Timer Interrupt! Time: ");
        get_timetick(); // 印出目前秒數
        async_uart_puts("\n");

        // Exercise 2 規定：下次 timeout 設為 2 秒後
        set_core_timer_interrupt_second(2);
    }
    else if (irq_src == (1 << 8))
    {
        unsigned int uart_irq_pending = *((volatile unsigned int*)dtb_ctx.arm_ctrl_interrupt);
        // 判斷是否為 AUX 中斷 (Bit 29)
        if (uart_irq_pending & (1 << 29))
        {
            uart_interrupt_handler();
        }
    }
    else
    {
        // 處理未知的中斷
        async_uart_puts("\n[EL0 IRQ] Unknown IRQ source: 0x");
        async_uart_send_hex(irq_src);
        async_uart_puts("\n");
    }
    return;
}

void default_handler_dump_c(unsigned long esr, unsigned long elr, unsigned long spsr)
{
    uart_puts("\n[DEFAULT EXCEPTION]\n");
    uart_puts("  ESR_EL1 = 0x"); 
    uart_send_unsigned_long_integer(esr);  
    uart_puts("\n");
    uart_puts("  ELR_EL1 = 0x"); 
    uart_send_unsigned_long_integer(elr);  
    uart_puts("\n");
    uart_puts("  SPSR_EL1= 0x"); 
    uart_send_unsigned_long_integer(spsr); 
    uart_puts("\n");

    unsigned long far = read_far_el1();
    uart_puts("  FAR_EL1 = 0x"); 
    uart_send_unsigned_long_integer(far);
    uart_puts("\n");

    while (1)
    {
        // infinite loop
    }
}

void el0_sync_handler_c(unsigned long esr, unsigned long elr, unsigned long spsr, unsigned long *ctx)
{
    unsigned long exception_class = (esr >> 26) & 0x3F;
    unsigned long instruction_specific_syndrome = esr & 0x01FFFFFF;

    uart_puts("\n[EL0 SYNC]\n");
    uart_puts("  ESR_EL1 = 0x"); 
    uart_send_unsigned_long_integer(esr);  
    uart_puts("\n");
    uart_puts("  ELR_EL1 = 0x"); 
    uart_send_unsigned_long_integer(elr);  
    uart_puts("\n");
    uart_puts("  SPSR_EL1= 0x"); 
    uart_send_unsigned_long_integer(spsr); 
    uart_puts("\n");
    uart_puts("  EC      = 0x"); 
    uart_send_unsigned_long_integer(exception_class);   
    uart_puts("\n");

    if (exception_class == 0x15) // SVC from AArch64
    {
        // SVC 的 immediate 通常在 ISS 的低 16 bits（imm16）
        unsigned long imm16 = instruction_specific_syndrome & 0xffff;
        uart_puts("  ISS     = 0x"); 
        uart_send_unsigned_long_integer(imm16);   
        uart_puts("\n");
        return;
    }

    unsigned long far = read_far_el1();
    uart_puts("  FAR_EL1 = 0x"); 
    uart_send_unsigned_long_integer(far);
    uart_puts("\n");

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