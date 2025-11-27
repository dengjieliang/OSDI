#ifndef SHELL_H
#define SHELL_H


typedef void(*CommandFunc)(void);

typedef struct Command
{
    const char *name;
    CommandFunc func;
}Command_t;

void execute_command(const char *cmd_name);

void cmd_hello(void);
void cmd_help(void);

#endif