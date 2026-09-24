#include "../Driver/uart.h"
#include "../Driver/early_uart.h"
#include "../Kernel/time_manager.h"
#include "../Board/fdtb.h"
#include "../Kernel/exception.h"
#include "../Kernel/io_task_queue.h" // AE2: deferred task queue (enqueue in IRQ, run with IRQ enabled)
#include "../Board/common.h"
#include "../Shell/shell.h"
#include "../Driver/time.h"

// +2026/03/29 變更點：新增去重旗標，避免同一來源 IRQ 在 deferred task 尚未執行前重複 enqueue。
static volatile bool timer_task_queued = false;
static volatile bool uart_task_queued = false;

static void deferred_timer_irq_task(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // +2026/03/29 變更點：原本在 irq_routing 直接執行 timer handler，改為在 queue 階段執行。
    timer_interrupt_router();
    shell_notify_async_event();
    timer_task_queued = false;
}

static void deferred_uart_irq_task(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // +2026/03/29 變更點：原本在 irq_routing 直接執行 uart handler，改為 deferred task 執行。
    uart_interrupt_handler();
    uart_task_queued = false;
}

void daif_mask_all(void)
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

void daif_unmask_all(void)
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

unsigned long local_irq_save(void)
    {
        unsigned long daif;

        // 保存目前 DAIF，並只遮罩 IRQ（I bit），保留其他例外遮罩位元原狀。
        __asm__ volatile("mrs %0, daif" : "=r"(daif) :: "memory");
        __asm__ volatile(
            "msr daifset, #0x2\n"
            "isb\n"
            :
            :
            : "memory"
        );

        return daif;
    }

void local_irq_restore(unsigned long daif)
    {
        // 還原進入臨界區之前的完整 DAIF 狀態，而不是無條件直接開 IRQ。
        __asm__ volatile(
            "msr daif, %0\n"
            "isb\n"
            :
            : "r"(daif)
            : "memory"
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

// irq_routing - AE2 兩階段 IRQ 分發器
//
// 設計目標（AE2 spec）：
//   1. 在 IRQ handler 內部，僅做「來源確認 + 最小資料搬移」，不執行耗時的 callback。
//   2. 將需要執行的工作 enqueue 到 io_task_queue（timer/uart handler 轉到 deferred task）。
//   3. 重新開啟 IRQ 後再執行 deferred task，使更高優先級中斷可以搶佔（nested interrupt）。
//
// 為何不在此處呼叫 daif_mask_all()？
//   硬體在 exception entry 時會自動將 DAIF.I 置為 1（即 IRQ masked），
//   因此進入此函式時 IRQ 已經是關閉狀態。不需要再手動 mask，
//   也不應該 double-mask（會造成 daif 計數語意混亂）。
//
// 為何移除先前的多處 early return？
//   舊版多個 early return 會在 daif_mask_all() 之後直接離開，
//   卻沒有對應的 unmask，造成 DAIF 永久被鎖住的 bug。
//   現在改為 Phase 1 用 if-guard 包覆，無論路徑為何都會流到 Phase 2 執行 io_task_run_all()。
static void irq_routing(unsigned long elr, unsigned long spsr, unsigned long *ctx)
{
    extern CtxT dtb_ctx;

    (void)elr;
    (void)spsr;
    (void)ctx;

    // =========================================================
    // Phase 1: 來源偵測（Source Detection）
    // =========================================================
    // 此階段 IRQ 仍處於 masked 狀態（由硬體 exception entry 設置）。
    // 只做暫存器讀取 + 來源判斷，不執行任何耗時邏輯。
    if (dtb_ctx.interrupt_info.have_arm_local_intc_base)
    {
        // 讀取 Core 0 Interrupt Source register（offset 0x60）
        // 此暫存器每個 bit 對應一條中斷線的 pending 狀態。
        unsigned int irq_src = mmio_read(dtb_ctx.interrupt_info.arm_local_intc_base + 0x60);

        // Bit 1 = CNTPNSIRQ：Non-Secure Physical Timer（Core Timer）中斷。
        // 這裡只 enqueue deferred task，不在 IRQ routing 階段執行重工作。
        if (irq_src & (1 << 1))
        {
            set_core_timer_interrupt_tick(~0ULL);

            if (timer_task_queued == false)
            {
                set_core_timer_interrupt_tick(~0ULL);          // ① 先把門鈴線關掉
                
                // +2026/03/29 變更點：irq_routing 僅 enqueue，不做重 callback。
                bool queued = io_task_enqueue(
                    IO_TASK_TYPE_TIMER_CALLBACK,
                    IO_TASK_PRIORITY_HIGH,
                    deferred_timer_irq_task,
                    0,
                    NULL,
                    0
                );

                if (queued)
                {
                    timer_task_queued = true;
                }
            }
        }

        // Bit 8 = GPU interrupt（ARM_IRQPENDING_0）：
        // Broadcom 的 IRQ controller 架構中，AUX UART 中斷通過 GPU domain 路由，
        // 需進一步讀取 ARM Interrupt Controller 的第二層 pending register 確認。
        if (irq_src & (1 << 8))
        {
            if (dtb_ctx.interrupt_info.have_arm_ctrl_intc_base)
            {
                // ARM_IRQ_PENDING1（offset 0x04）：Bit 29 = AUX（包含 Mini UART）中斷。
                unsigned int uart_irq_pending = mmio_read(dtb_ctx.interrupt_info.arm_ctrl_intc_base + 0x04);
                if (uart_irq_pending & (1 << 29))
                {
                    if (!uart_task_queued)
                    {
                        // +2026/03/29 變更點：uart IRQ 來源改為 enqueue deferred task。
                        bool queued = io_task_enqueue(
                            IO_TASK_TYPE_UART_RX,
                            IO_TASK_PRIORITY_MEDIUM,
                            deferred_uart_irq_task,
                            0,
                            NULL,
                            0
                        );

                        if (queued)
                        {
                            uart_task_queued = true;
                        }
                    }
                }
            }
        }

        // 若 irq_src 非零但不屬於上述任何已知來源，印出除錯訊息。
        // 注意：irq_src == 0 時不算「未知中斷」，直接跳到 Phase 2 即可。
        if (irq_src && ((irq_src & (1 << 1)) == 0) && ((irq_src & (1 << 8)) == 0))
        {
            early_uart_puts("\n[IRQ] Unknown IRQ source: 0x");
            early_uart_send_hex(irq_src);
            early_uart_puts("\n");
        }
    }

    // =========================================================
    // Phase 2: 統一出口執行 Deferred Task（IRQ 重新開啟後）
    // =========================================================
    // +2026/03/29 變更點：統一 exit path，修正多處 early return 的 DAIF 不對稱風險。
    // 這是唯一 exit path：避免多處 early return 造成 DAIF 狀態不對稱。
    // 每層 nested handler return 前都會走到這裡，因此優先級檢查可逐層生效。
    daif_unmask_all();
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