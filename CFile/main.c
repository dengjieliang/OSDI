#include "../header/uart.h"

void kernel_main(void)
{
    uart_init();

    uart_send('H');
    uart_send('e');
    uart_send('l');
    uart_send('l');
    uart_send('o');
    uart_send('\r');
    uart_send('\n');

    while (1)
    {
        char c = uart_recv();
        uart_send(c + 1);
    }
}