#include "../header/uart.h"
#include "../header/shell.h"

void kernel_main(void)
{
    uart_init();

    shell_main();
}