#ifndef UART_H
#define UART_H

//initializes mini UART
void uart_init();

//blocking UART I/O functions
void uart_send(char c);
void uart_send_integer(int number);
void uart_send_unsigned_long_integer(unsigned long number);
void uart_send_decimal_part(int number, unsigned int digit);
void uart_send_hex(unsigned int number);
unsigned int uart_recv_uint();
char uart_recv();
void uart_puts(const char *s);
void delay_cycles(unsigned int time);

#endif

