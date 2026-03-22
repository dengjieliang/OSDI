#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include "../Lib/base.h"

typedef void (*timer_callback_t)(int, char*);
bool AddTaskToTimerManager(timer_callback_t task, char* message, unsigned long executeAfterSeconds);

#endif