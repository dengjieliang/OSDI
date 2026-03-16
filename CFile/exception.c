#include "../header/uart.h"
#include "../header/early_uart.h"
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
    mask_all_exceptions(); // 避免 nested interrupt

    extern CtxT dtb_ctx;

    if (dtb_ctx.interrupt_info.have_arm_local_intc_base == false)
    {
        return;
    }

    // 讀取 Core 0 Interrupt Source
    unsigned int irq_src = mmio_read(dtb_ctx.interrupt_info.arm_local_intc_base + 0x60);

    if (irq_src == 0)
    {
        return;
    }

    // Bit 1 (值為 2) 代表 CNTPNSIRQ (Core Timer Interrupt)
    if (irq_src & (1 << 1))
    {
        // 先 re-arm，避免因為 IRQ 內輸出太慢導致重複觸發
        set_core_timer_interrupt_second(2);

        // IRQ 內只做最小輸出，避免阻塞太久
        early_uart_puts("Core Timer Interrupt!\n");
    }

    // Bit 8 代表 GPU interrupt pending，AUX UART 會經過這條路
    if (irq_src & (1 << 8))
    {
        if (dtb_ctx.interrupt_info.have_arm_ctrl_intc_base == false)
        {
            return;
        }

        unsigned int uart_irq_pending = mmio_read(dtb_ctx.interrupt_info.arm_ctrl_intc_base + 0x04);
        // 判斷是否為 AUX 中斷 (Bit 29)
        if (uart_irq_pending & (1 << 29))
        {
            uart_interrupt_handler();
        }
    }

    if (((irq_src & (1 << 1)) == 0) && ((irq_src & (1 << 8)) == 0))
    {
        // 處理未知的中斷
        early_uart_puts("\n[IRQ] Unknown IRQ source: 0x");
        early_uart_send_hex(irq_src);
        early_uart_puts("\n");
    }
    return;
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