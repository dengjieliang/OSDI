#ifndef SHELL_H
#define SHELL_H


typedef void(*CommandFunc)(int argc, char* argv[]);

typedef struct Command
{
    const char *name;
    const char *description;
    CommandFunc func;
}Command_t;

void shell_main();
void execute_command(int argc, char* argv[]);
char* shell_input_line();
void shell_notify_async_event();

#endif