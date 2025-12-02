#ifndef UART_H
#define UART_H

//initializes mini UART
void uart_init();

//blocking UART I/O functions
void uart_send(char c);
char uart_recv();
void uart_puts(const char *s);
void uart_send_hex(unsigned int number);
void delay_cycles(unsigned int time);

#endif

