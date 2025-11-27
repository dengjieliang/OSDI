#include "../header/uart.h"
#include "../header/shell.h"

void kernel_main(void)
{
    uart_init();

    cmd_hello();
    cmd_help();
}