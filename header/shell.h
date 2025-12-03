#ifndef SHELL_H
#define SHELL_H


typedef void(*CommandFunc)(void);

typedef struct Command
{
    const char *name;
    CommandFunc func;
}Command_t;

void shell_main();
void execute_command(const char *cmd_name);
const char* shell_input_line();

#endif