#ifndef EXCEPTION_H
#define EXCEPTION_H

void daif_mask_all(void);
void daif_unmask_all(void);
unsigned long local_irq_save(void);
void local_irq_restore(unsigned long daif);
void set_exception_vector_table(void);
void default_handler_dump_c(unsigned long esr, unsigned long elr, unsigned long spsr);
void el0_sync_handler_c(unsigned long esr, unsigned long elr, unsigned long spsr, unsigned long *ctx);
void el0_irq_handler_c(unsigned long elr, unsigned long spsr, unsigned long *ctx);
void el1_irq_handler_c(unsigned long elr, unsigned long spsr, unsigned long *ctx);

#endif