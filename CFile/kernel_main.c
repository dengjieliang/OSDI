#include "../header/uart.h"
#include "../header/shell.h"

void kernel_main(void* dtb_addr)
{
    // [新增] Kernel 的除錯鎖
    //volatile int lock = 1;
    //while(lock);

    // 第二次初始化，雖然硬體已經開了，但為了保險，重新設定一次
    uart_init();

    uart_puts("\r\nWelcome to OSDI\r\n");
    
    //獲取使用者輸入
    shell_main();
}