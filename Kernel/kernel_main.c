#include "../Driver/early_uart.h"
#include "../Driver/uart.h"
#include "../Shell/shell.h"
#include "../Board/dtb.h"
#include "../Board/fdtb.h"
#include "../Kernel/exception.h"
#include "../Kernel/io_task_queue.h"
#include "../Board/common.h"
#include "../Driver/time.h"

CtxT dtb_ctx;
static bool kernel_banner_printed = false;

void kernel_main(void* dtb_addr)
{
    // [新增] Kernel 的除錯鎖
    //volatile int lock = 1;
    //while(lock);
    

    InitialDtbCtx(&dtb_ctx);
    if (ReadDTBFile(dtb_addr, DtbCollectHandler, (void*)&dtb_ctx) == false)
    {
        early_uart_init();
        early_uart_puts("Failed to read DTB file.\n");
        return;
    }

    common_init_from_dtb(&dtb_ctx);

    // 第二次初始化，雖然硬體已經開了，但為了保險，重新設定一次
    //uart_init();
    uart_init_dynamic();
    uart_aux_mu_cntl_reg();
    uart_open_ier_reg();

    //初始化timer
    core_timer_init();

    // +2026/03/29 變更點：新增 queue 初始化，確保 IRQ routing 的 enqueue 路徑可正常運作。
    // AE2: 在開 IRQ 前初始化 deferred task queue。
    io_task_queue_init();


    // 會直接吃.S檔案內的 exception_vector_table
    set_exception_vector_table();
    daif_unmask_all();

    if (kernel_banner_printed == false)
    {
        async_uart_puts("\r\nWelcome to OSDI\r\n");
        kernel_banner_printed = true;
    }

    //獲取使用者輸入
    shell_main();
}