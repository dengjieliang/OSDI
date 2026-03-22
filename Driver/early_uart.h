#ifndef EARLY_UART_H
#define EARLY_UART_H

void early_uart_init();//blocking UART I/O functions
void early_uart_send(char c);
void early_uart_puts(const char *s);
char early_uart_recv();
unsigned int early_uart_recv_uint();
void early_uart_send_integer(int number);
void early_uart_send_unsigned_long_integer(unsigned long number);
void early_uart_send_decimal_part(int number, unsigned int digit_size);
void early_uart_send_hex(unsigned int number);
void early_uart_delay_cycles(unsigned int time);

#endif