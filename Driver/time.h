#ifndef TIME_H
#define TIME_H

void core_timer_init();
unsigned long long get_current_tick();
double get_current_second();
void get_current_second_string();
unsigned long long tansfer_seconds_to_ticks(unsigned long seconds);
void set_core_timer_interrupt_tick(unsigned long long timer_count);

#endif