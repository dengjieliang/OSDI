//initializes mini UART
void uart_init();

//blocking UART I/O functions
void uart_send(char c);
char uart_recv();
void uart_puts(const char *s);

