#include "../header/common.h"
#include "../header/uart.h"
#include "../header/shell.h"
#include "../header/string.h"

static const Command_t commands[] = 
{
    {"hello", cmd_hello},
    {"help", cmd_help},
    {NULL, NULL} // Sentinel to mark the end of the array
};

// 在收到指令時遍歷所有命令，找到匹配的並執行
void execute_command(const char *cmd_name)
{
    for (int i = 0; commands[i].name != NULL; i++)
    {
        if (strcmp(commands[i].name, cmd_name) == 0)
        {
            commands[i].func();
            return;
        }
    }
}

void cmd_hello(void)
{
    const char *hello_text = "Hello, World!\n";
    uart_puts(hello_text);
}

void cmd_help(void)
{
    const char *hint_text = "Available commands:\n";
    uart_puts(hint_text);

    for (int i = 0; commands[i].name != NULL; i++)
    {
        uart_puts(" - ");
        uart_puts(commands[i].name);
        uart_puts("\n");
    }
}