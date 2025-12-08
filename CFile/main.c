#include "../header/uart.h"
#include "../header/utils.h"
#include "../header/shell.h"


void kernel_main(void)
{
    //將GPIO接到 mini uart
    uart_init();

    unsigned int size = uart_recv_uint();
    uart_send_hex(size);

    uart_puts("Bootloader: Waiting for Kernel size...");

    //獲取使用者輸入
    shell_main();
}