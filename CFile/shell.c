#include "../header/common.h"
#include "../header/uart.h"
#include "../header/shell.h"
#include "../header/string.h"
#include "../header/mailbox.h"
#include "../header/power_manager.h"

static bool reboot_lock = false;

static void cmd_hello(void);
static void cmd_help(void);
static void cmd_board_info(void);
static void cmd_reboot(void);
static void cmd_cancel_reboot(void);

static const Command_t commands[] = 
{
    {"Hello", "Print Hello World", cmd_hello},
    {"Help", "Show All Command", cmd_help},
    {"Board Info", "Show Board Info", cmd_board_info},
    {"Reboot", "Reboot Computer", cmd_reboot},
    {"Cancel Reboot", "Cancel Reboot Computer", cmd_cancel_reboot},
    {NULL, NULL} // Sentinel to mark the end of the array
};

void shell_main()
{
    uart_puts("\n\n=== RPi3 OS Booting... ===\n"); //顯示已開機

    while (1)
    {
        const char* input_buffer = shell_input_line();
        execute_command(input_buffer);
    }
    
}

const char* shell_input_line()
{
    static char input_buffer[128];

    int buffer_index = 0;

    uart_puts("shell$ ");

    while (true)
    {
        char c = uart_recv();
        
        if (c == '\r' || c == '\n') // Handle Enter key
        {
            uart_puts("\n");
            input_buffer[buffer_index] = '\0'; // Null-terminate the string
            return input_buffer;
        }
        else if (c == '\b' || c == 127) // Handle backspace
        {
            if (buffer_index > 0)
            {
                buffer_index--;
                // 回顯退格
                uart_puts("\b \b");
            }
            continue;
        }
        // 回顯輸入的字元
        uart_send(c);

        // 儲存到緩衝區
        if (buffer_index < sizeof(input_buffer) - 1)
        {
            input_buffer[buffer_index++] = c;
        }
    }
}

// 在收到指令時遍歷所有命令，找到匹配的並執行
void execute_command(const char *cmd_name)
{
    for (int i = 0; commands[i].name != NULL; i++)
    {
        if (reboot_lock && strcmp("Cancel Reboot", cmd_name) != 0)
        {
            uart_puts("Rebooting... Please input 'Cancel Reboot' to abort.");
            return;
        }

        if (strcmp(commands[i].name, cmd_name) == 0)
        {
            commands[i].func();
            return;
        }
    }

    const char *not_found_command_msg = "Command not found:";
    uart_puts("\n");
    uart_puts(not_found_command_msg);
    uart_puts(cmd_name);
    uart_puts("\n");
}

static void cmd_hello(void)
{
    const char *hello_text = "Hello, World!\n";
    uart_puts(hello_text);
}

static void cmd_help(void)
{
    const char *hint_text = "Available commands:\n";
    uart_puts(hint_text);

    for (int i = 0; commands[i].name != NULL; i++)
    {
        uart_puts(" - ");
        uart_puts(commands[i].name);
        uart_puts("：");
        uart_puts(commands[i].description);
        uart_puts("\n");
    }
}

static void cmd_board_info()
{

    //寫入資料到MailBox Buffer
    prepare_board_revision_request();

    //呼叫mailbox_call確認GPU
    mailbox_call(MBOX_CH_PROP);

    if (get_board_status() == 0x80000000)
    {
        uart_puts("Board Revision: ");
        uart_send_hex(get_board_revision());
    }

    prepare_memory_request();
    mailbox_call(MBOX_CH_PROP);

    if (get_memory_status() == 0x80000000)
    {
        uart_puts("Memory Size is: ");
        uart_send_hex(get_memory_size());
    }
}

static void cmd_reboot()
{
    // --- 新增這行 ---
    uart_puts("Rebooting in T-minus 2 seconds...\n"); //reboot前跳提示
    // ----------------
    reset(150000);
    reboot_lock = true;
}

static void cmd_cancel_reboot()
{
    cancel_reset();
    reboot_lock = false;
}

