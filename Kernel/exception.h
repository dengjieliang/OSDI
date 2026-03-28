#ifndef EXCEPTION_H
#define EXCEPTION_H

void mask_all_exceptions(void);
void unmask_all_exceptions(void);
void set_exception_vector_table(void);
void default_handler_dump_c(unsigned long esr, unsigned long elr, unsigned long spsr);
void el0_sync_handler_c(unsigned long esr, unsigned long elr, unsigned long spsr, unsigned long *ctx);
void el0_irq_handler_c(unsigned long elr, unsigned long spsr, unsigned long *ctx);
void el1_irq_handler_c(unsigned long elr, unsigned long spsr, unsigned long *ctx);

#endif