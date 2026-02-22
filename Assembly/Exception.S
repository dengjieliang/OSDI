    .section ".text"

    .globl set_exception_vector_table
    .type set_exception_vector_table, %function

set_exception_vector_table:
    adr x0, exception_vector_table
    msr vbar_el1, x0    /* Install vector base for EL1*/
    isb
    ret

    