#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include "../Lib/base.h"
#include "../Shell/shell.h"

#define MAX_MESSAGE_LENGTH 256

bool add_timer(CommandFunc callback, int argc, char** argv, double after_seconds);
void timer_interrupt_router();

#endif