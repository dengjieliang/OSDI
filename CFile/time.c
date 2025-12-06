#include "../header/time.h"
#include "../header/uart.h"

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