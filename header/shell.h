#ifndef SHELL_H
#define SHELL_H


typedef void(*CommandFunc)(void);

struct Command
{
    char *name;
    ConnandFunc func;
}

#endif