#ifndef UART_H
#define UART_H

//initializes mini UART
void uart_init();
void aux_mu_cntl_reg();
void uart_open_ier_reg();
void uart_interrupt_handler();

//blocking UART I/O functions
void uart_send(char c);
void uart_puts(const char *s);
char uart_recv();
unsigned int uart_recv_uint();
void uart_send_integer(int number);
void uart_send_unsigned_long_integer(unsigned long number);
void uart_send_decimal_part(int number, unsigned int digit);
void uart_send_hex(unsigned int number);
void async_uart_send(char c);
void async_uart_puts(const char *s);
char async_uart_recv();
unsigned int async_uart_recv_uint();
void async_uart_send_integer(int number);
void async_uart_send_unsigned_long_integer(unsigned long number);
void async_uart_send_decimal_part(int number, unsigned int digit);
void async_uart_send_hex(unsigned int number);
void delay_cycles(unsigned int time);

#endif

