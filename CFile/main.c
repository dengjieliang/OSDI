#include "../header/uart.h"
#include "../header/shell.h"


void kernel_main(void)
{
    //將GPIO接到 mini uart
    uart_init();

    //獲取使用者輸入
    shell_main();
}