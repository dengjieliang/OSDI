#include "../header/time.h"
#include "../header/uart.h"

#define CORE0_TIMER_IRQ_CTRL 0x40000040
static unsigned long long get_system_timer_count();
static unsigned long long get_system_timer_frequency();
static inline void core_timer_enable();
static inline void unmask_timer_interrupt();

static unsigned long long get_system_timer_count()
{
    unsigned long long count;

    // mrs: Move from System Register to general purpose register
    // %0: 代表變數 count。必須先搬動值到暫存器->編譯器會自動加一行在組語結束後將暫存器的值搬回stack
    asm volatile("mrs %0, cntpct_el0" : "=r"(count)); 
    return count;
}

static unsigned long long get_system_timer_frequency()
{
    unsigned long long freq;

    // mrs: Move from System Register to general purpose register
    // %0: 代表變數 freq。必須先搬動值到暫存器->編譯器會自動加一行在組語結束後將暫存器的值搬回stack
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq)); 
    return freq;
}

static inline void core_timer_enable()
{
    
    // bit 0: enable bit
    // bit 1: interrupt mask bit
    unsigned long long ctl = (0 << 1) | (1 << 0);
    
    // cntp_ctl_el0: Core timer control register
    asm volatile
    (
        "msr cntp_ctl_el0, %0\n" 
        "isb"
        : 
        : "r"(ctl) // enable core timer and unmask interrupt
        : "memory"
    );
}

static inline void unmask_timer_interrupt()
{
    unsigned int* timer_irq_ctrl = (unsigned int*)CORE0_TIMER_IRQ_CTRL;
    *timer_irq_ctrl = 2; // unmask timer interrupt
}

void get_timetick()
{
    unsigned long long timer_count = get_system_timer_count();
    unsigned long long timer_freq = get_system_timer_frequency();
    int timetick_integer_part = timer_count / timer_freq;
    int decimal_part = ((timer_count % timer_freq) * 10000) / timer_freq;
    
    uart_send_integer(timetick_integer_part);
    uart_send('.');
    uart_send_decimal_part(decimal_part, 4);
}

void set_core_timer_interrupt_tick(unsigned long long timer_count)
{
    // msr: Move to System Register from general purpose register
    // cntp_tval_el0: Core timer compare value register
    asm volatile
    (
        "msr cntp_tval_el0, %0\n" 
        "isb"
        : 
        : "r"(timer_count)
        : "memory"
    );
}

void set_core_timer_interrupt_second(unsigned long second)
{
    unsigned long long timer_freq = get_system_timer_frequency();
    unsigned long long timer_count = second * timer_freq;
    set_core_timer_interrupt_tick(timer_count);
}

void core_timer_enable_tick(unsigned long tick)
{
    core_timer_enable();
    set_core_timer_interrupt_tick(tick);
    unmask_timer_interrupt();
}

void core_timer_enable_second(unsigned long second)
{
    core_timer_enable();
    set_core_timer_interrupt_second(second);
    unmask_timer_interrupt();
}