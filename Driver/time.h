#ifndef TIME_H
#define TIME_H

unsigned long long get_current_tick();
double get_current_second();
void get_current_second_string();
unsigned long long tansfer_seconds_to_ticks(unsigned long seconds);
void set_core_timer_interrupt_tick(unsigned long long timer_count);
void set_core_timer_interrupt_second(unsigned long second);
void core_timer_enable_tick(unsigned long tick);
void core_timer_enable_second(unsigned long second);

#endif