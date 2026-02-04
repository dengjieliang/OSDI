#include "../header/common.h"
#include "../header/uart.h"


typedef void (*kernel_entry_t)(void *);
static void bootloader_main(void *dtb);

void kernel_main(void *dtb)
{
    bootloader_main(dtb);
}

static void bootloader_main(void *dtb)
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

    kernel_entry_t entry = (kernel_entry_t)KERNEL_LOAD_ADDRESS;
    entry(dtb);

    // 正常情況下 kernel 不會返回；告訴編譯器此處不可達（若返回則為未定義行為）
    __builtin_unreachable();
}