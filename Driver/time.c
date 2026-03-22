#include "../Driver/time.h"
#include "../Driver/uart.h"
#include "../Board/fdtb.h"
#include "../Board/common.h"

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
    extern CtxT dtb_ctx;
    if (dtb_ctx.interrupt_info.have_arm_local_intc_base == false)
    {
        return;
    }

    mmio_write(dtb_ctx.interrupt_info.arm_local_intc_base + 0x40, 2); // unmask timer interrupt
}

void get_current_timetick(double* timetick)
{
    unsigned long long timer_count = get_system_timer_count();
    unsigned long long timer_freq = get_system_timer_frequency();
    *timetick = (double)timer_count / (double)timer_freq;
}

void get_current_timetick_string()
{
    double timetick = 0;
    get_current_timetick(&timetick);

    int timetick_integer_part = (int)timetick;
    double timetick_decimal_part_double = timetick - (double)timetick_integer_part;
    int timetick_decimal_part = 0;

    while (timetick_decimal_part_double > 0.000001 && timetick_decimal_part_double < 1.0)
    {
        timetick_decimal_part_double *= 10;
    }

    timetick_decimal_part = (int)timetick_decimal_part_double;
    
    async_uart_send_integer(timetick_integer_part);
    async_uart_send('.');
    async_uart_send_decimal_part(timetick_decimal_part, 4);
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