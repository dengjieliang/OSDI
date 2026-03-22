#ifndef TIME_H
#define TIME_H

void get_timetick();
void set_core_timer_interrupt_tick(unsigned long long timer_count);
void set_core_timer_interrupt_second(unsigned long second);
void core_timer_enable_tick(unsigned long tick);
void core_timer_enable_second(unsigned long second);

#endif