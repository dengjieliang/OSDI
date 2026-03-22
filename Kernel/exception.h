#ifndef EXCEPTION_H
#define EXCEPTION_H

void set_exception_vector_table(void);
void default_handler_dump_c(unsigned long esr, unsigned long elr, unsigned long spsr);
void el0_sync_handler_c(unsigned long esr, unsigned long elr, unsigned long spsr, unsigned long *ctx);
void el0_irq_handler_c(unsigned long elr, unsigned long spsr, unsigned long *ctx);

#endif