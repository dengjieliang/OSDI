#include "../header/common.h"
#include "../header/uart.h"
#include "../header/shell.h"


void kernel_main(void)
{
    //第一次初始化，為了讓 Bootloader 能跟 Python 講話
    //將GPIO接到 mini uart
    uart_init();

    uart_puts("\r\nOSDI: Ready\r\n");
    uart_puts("Bootloader: Waiting for Kernel size...");
    unsigned int size = uart_recv_uint();
    uart_send_hex(size);

    uart_puts("Bootloader: Waiting for Loding Kernel...");

    char* kernel_code = (char*) KERNEL_LOAD_ADDRESS;

    for (unsigned int i = 0; i < size; i++)
    {
        char c = uart_recv();
        *kernel_code = c;
        kernel_code++;
    }

    ((void (*)(void))KERNEL_LOAD_ADDRESS)();
}