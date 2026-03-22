#include "../Driver/early_uart.h"
#include "../Driver/uart.h"
#include "../Shell/shell.h"
#include "../Board/dtb.h"
#include "../Board/fdtb.h"
#include "../Kernel/exception.h"

CtxT dtb_ctx;

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


    // 會直接吃.S檔案內的 exception_vector_table
    set_exception_vector_table();

    asm volatile("msr daifclr, #0xf");

    async_uart_puts("\r\nWelcome to OSDI\r\n");
    
    //獲取使用者輸入
    shell_main();
}