#include "../header/common.h"
#include "../header/uart.h"
#include "../header/shell.h"
#include "../header/string.h"
#include "../header/mailbox.h"
#include "../header/power_manager.h"
#include "../header/time.h"
#include "../header/cpio.h"
#include "../header/allocator.h"
#include "fdtb.h"

static bool reboot_lock = false;

static void cmd_hello(int argc, char* argv[]);
static void cmd_help(int argc, char* argv[]);
static void cmd_board_info(int argc, char* argv[]);
static void cmd_get_timer(int argc, char* argv[]);
static void cmd_reboot(int argc, char* argv[]);
static void cmd_cancel_reboot(int argc, char* argv[]);
static void cmd_get_file_header(int argc, char* argv[]);
static void cmd_get_file_context(int argc, char* argv[]);

static void split_command(int* argc, char* argv[]);

static const Command_t commands[] = 
{
    {"hello", "Print Hello World", cmd_hello},
    {"help", "Show All Command", cmd_help},
    {"info", "Show Board Info", cmd_board_info},
    {"time", "Show Now Timer", cmd_get_timer},
    {"reboot", "Reboot Computer", cmd_reboot},
    {"cancel", "Cancel Reboot Computer", cmd_cancel_reboot},
    {"ls", "Get All File Header", cmd_get_file_header},
    {"cat", "Get File Context", cmd_get_file_context},
    {NULL, NULL} // Sentinel to mark the end of the array
};

static char input_buffer[128];

void shell_main()
{
    uart_puts("\n\n=== RPi3 OS Booting... ===\n"); //顯示已開機

    while (1)
    {
        shell_input_line();
        int argc = 0;
        char* argv[MAX_ARGS];
        
        split_command(&argc, argv);

        if (argc > 0)
        {
            execute_command(argc, argv);
        }
    }
    
}

char* shell_input_line()
{
    int buffer_index = 0;
    
    uart_puts("[");
    get_timetick();
    uart_puts("]");
    uart_puts(":");
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

static void split_command(int* argc, char* argv[])
{
    *argc = 0;
    char *cursor = input_buffer;

    while (*cursor != '\0')
    {
        if (*cursor == ' ')
        {
            *cursor = '\0';
            cursor += 1;
            continue;
        }

        if (*argc >= MAX_ARGS)
        {
            uart_puts("Warning: Too many arguments, ignoring the rest.\n");
            break;
        }

        argv[*argc] = cursor;
        (*argc) += 1;

        while (*cursor != ' ' && *cursor != '\0')
        {
            cursor += 1;
        }       
    }
}

// 在收到指令時遍歷所有命令，找到匹配的並執行
void execute_command(int argc, char* argv[])
{
    for (int i = 0; commands[i].name != NULL; i++)
    {
        if (reboot_lock && strcmp("Cancel Reboot", argv[0]) != 0)
        {
            uart_puts("Rebooting... Please input 'Cancel Reboot' to abort.");
            return;
        }

        if (strcmp(commands[i].name, argv[0]) == 0)
        {
            commands[i].func(argc, argv);
            return;
        }
    }

    const char *not_found_command_msg = "Command not found:";
    uart_puts("\n");
    uart_puts(not_found_command_msg);
    uart_puts(argv[0]);
    uart_puts("\n");
}

static void cmd_hello(int argc, char* argv[])
{
    (void)argc; // 防止編譯警告
    (void)argv; // 防止編譯警告

    const char *hello_text = "Hello, World!\n";
    uart_puts(hello_text);
}

static void cmd_help(int argc, char* argv[])
{
    (void)argc; // 防止編譯警告
    (void)argv; // 防止編譯警告

    const char *hint_text = "Available commands:\n";
    uart_puts(hint_text);

    for (int i = 0; commands[i].name != NULL; i++)
    {
        uart_puts(" - ");
        uart_puts(commands[i].name);
        uart_puts(":");
        uart_puts(commands[i].description);
        uart_puts("\n");
    }
}

static void cmd_board_info(int argc, char* argv[])
{
    (void)argc; // 防止編譯警告
    (void)argv; // 防止編譯警告

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

static void cmd_get_timer(int argc, char* argv[])
{
    (void)argc; // 防止編譯警告
    (void)argv; // 防止編譯警告

    get_timetick();
    uart_puts("\n");
}

static void cmd_reboot(int argc, char* argv[])
{
    (void)argc; // 防止編譯警告
    (void)argv; // 防止編譯警告

    // --- 新增這行 ---
    uart_puts("Rebooting in T-minus 2 seconds...\n"); //reboot前跳提示
    // ----------------
    reset(150000);
    reboot_lock = true;
}

static void cmd_cancel_reboot(int argc, char* argv[])
{
    (void)argc; // 防止編譯警告
    (void)argv; // 防止編譯警告

    cancel_reset();
    reboot_lock = false;
}

static void cmd_get_file_header(int argc, char* argv[])
{
    (void)argc; // 防止編譯警告
    (void)argv; // 防止編譯警告

    extern CtxT dtb_ctx;
    void * header = (void *)dtb_ctx.initrd_start;
    int file_count = CpioGetFilesHeaderName(header);

    if (file_count <= 0)
    {
        uart_puts("Not Find Any File.");
    }
}

static void cmd_get_file_context(int argc, char* argv[])
{
    if (argc < 2)
    {
        uart_puts("Please Enter FileName");
        return;
    }
    else if (argc != 2)
    {
        uart_puts("False Argument");
        return;
    }

    extern CtxT dtb_ctx;

    void * header = (void *)dtb_ctx.initrd_start;
    bool find_context_result = CpioGetFileContext(header, argv[1]);

    if (find_context_result == false)
    {
        uart_puts("Cannot Find File");
        return;
    }
}

