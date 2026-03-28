#include "../Driver/time.h"
#include "../Driver/uart.h"
#include "../Board/fdtb.h"
#include "../Board/common.h"

static unsigned long long get_system_timer_count();
static unsigned long long get_system_timer_frequency();

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

void core_timer_init()
{
    // 方案A: 先把 compare 設為最大值，確保 enable 後不會立刻觸發中斷
    set_core_timer_interrupt_tick(~0ULL);

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

    extern CtxT dtb_ctx;
    if (dtb_ctx.interrupt_info.have_arm_local_intc_base == false)
    {
        return;
    }

    mmio_write(dtb_ctx.interrupt_info.arm_local_intc_base + 0x40, 2); // unmask timer interrupt
}

unsigned long long get_current_tick()
{
    unsigned long long timer_count = get_system_timer_count();
    return timer_count;
}

double get_current_second()
{
    double timetick = 0;
    unsigned long long timer_count = get_system_timer_count();
    unsigned long long timer_freq = get_system_timer_frequency();
    timetick = (double)timer_count / (double)timer_freq;
    return timetick;
}

void get_current_second_string()
{
    double timetick = get_current_second();

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

unsigned long long tansfer_seconds_to_ticks(double seconds)
{
    unsigned long long timer_freq = get_system_timer_frequency();
    return (unsigned long long)(seconds * (double)timer_freq);
}

void tansfer_ticks_to_seconds(unsigned long long ticks, unsigned long* second_integer_part, unsigned int* second_decimal_part_4digit)
{
    unsigned long long timer_freq = get_system_timer_frequency();

    if (second_integer_part != NULL)
    {
        *second_integer_part = (unsigned long)(ticks / timer_freq);
    }

    if (second_decimal_part_4digit != NULL)
    {
        unsigned long long remain_ticks = ticks % timer_freq;
        *second_decimal_part_4digit = (unsigned int)((remain_ticks * 10000ULL) / timer_freq);
    }
}

void set_core_timer_interrupt_tick(unsigned long long timer_count)
{
    // 改用 cntp_cval_el0 (Compare Value Register)
    // 當 cntpct_el0 (目前時間) >= cntp_cval_el0 時，就會觸發中斷
    asm volatile
    (
        "msr cntp_cval_el0, %0\n" 
        "isb"
        : 
        : "r"(timer_count)
        : "memory"
    );
}