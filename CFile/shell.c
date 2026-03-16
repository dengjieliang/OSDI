#include "../header/common.h"
#include "../header/uart.h"
#include "../header/shell.h"
#include "../header/string.h"
#include "../header/mailbox.h"
#include "../header/power_manager.h"
#include "../header/time.h"
#include "../header/cpio.h"
#include "../header/fdtb.h"
#include "../header/user_mode.h"

static bool reboot_lock = false;

static void cmd_hello(int argc, char* argv[]);
static void cmd_help(int argc, char* argv[]);
static void cmd_board_info(int argc, char* argv[]);
static void cmd_get_timer(int argc, char* argv[]);
static void cmd_reboot(int argc, char* argv[]);
static void cmd_cancel_reboot(int argc, char* argv[]);
static void cmd_get_file_header(int argc, char* argv[]);
static void cmd_get_file_context(int argc, char* argv[]);
static void cmd_test_el1_brk(int argc, char* argv[]);
static void cmd_test_el1_svc(int argc, char* argv[]);
static void cmd_test_el1_bad_read(int argc, char* argv[]);
static void cmd_test_el0_user_mode(int argc, char* argv[]);

static void split_command(int* argc, char* argv[]);
static void cmd_dtb_intc(int argc, char* argv[]);
static void cmd_fdtb(int argc, char* argv[]);

static const Command_t commands[] = 
{
    {"hello", "Print Hello World", cmd_hello},
    {"help", "Show All Command", cmd_help},
    {"info", "Show Board Info", cmd_board_info},
    {"time", "Show Now Timer", cmd_get_timer},
    {"reboot", "Reboot Computer", cmd_reboot},
    {"cancelReboot", "Cancel Reboot Computer", cmd_cancel_reboot},
    {"ls", "Get All File Header", cmd_get_file_header},
    {"cat", "Get File Context", cmd_get_file_context},
    {"test_brk", "Test EL1 BRK", cmd_test_el1_brk},
    {"test_svc", "Test EL1 SVC", cmd_test_el1_svc},
    {"test_bad_read", "Test EL1 Bad Read", cmd_test_el1_bad_read},
    {"test_user_mode", "Test User Mode", cmd_test_el0_user_mode},
    {"dtb_intc", "Show parsed interrupt controller base addresses", cmd_dtb_intc},
    {"fdtb", "Dump parsed DTB context (initrd, UART, GPIO, INTC bases)", cmd_fdtb},
    {NULL, NULL} // Sentinel to mark the end of the array
};

static char input_buffer[128];

void shell_main()
{
    async_uart_puts("\n\n=== RPi3 OS Booting... ===\n"); //顯示已開機

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
    
    async_uart_puts("[");
    get_timetick();
    async_uart_puts("]");
    async_uart_puts(":");
    async_uart_puts("shell$ ");
    

    while (true)
    {
        char c = async_uart_recv();
        
        if ((c == '\r' || c == '\n') && buffer_index == 0)
        {
            continue;
        }
        if (c == '\r' || c == '\n') // Handle Enter key
        {
            async_uart_puts("\n");
            input_buffer[buffer_index] = '\0'; // Null-terminate the string
            return input_buffer;
        }
        else if (c == '\b' || c == 127) // Handle backspace
        {
            if (buffer_index > 0)
            {
                buffer_index--;
                // 回顯退格
                async_uart_puts("\b \b");
            }
            continue;
        }
        // 回顯輸入的字元
        async_uart_send(c);

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
            async_uart_puts("Warning: Too many arguments, ignoring the rest.\n");
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
        if (reboot_lock && strcmp("cancelReboot", argv[0]) != 0)
        {
            async_uart_puts("Rebooting... Please input 'Cancel Reboot' to abort.");
            return;
        }

        if (strcmp(commands[i].name, argv[0]) == 0)
        {
            commands[i].func(argc, argv);
            return;
        }
    }

    const char *not_found_command_msg = "Command not found:";
    async_uart_puts("\n");
    async_uart_puts(not_found_command_msg);
    async_uart_puts(argv[0]);
    async_uart_puts("\n");
}

static void cmd_hello(int argc, char* argv[])
{
    (void)argc; // 防止編譯警告
    (void)argv; // 防止編譯警告

    const char *hello_text = "Hello, World!\n";
    async_uart_puts(hello_text);
}

static void cmd_help(int argc, char* argv[])
{
    (void)argc; // 防止編譯警告
    (void)argv; // 防止編譯警告

    const char *hint_text = "Available commands:\n";
    async_uart_puts(hint_text);

    for (int i = 0; commands[i].name != NULL; i++)
    {
        async_uart_puts(" - ");
        async_uart_puts(commands[i].name);
        async_uart_puts(":");
        async_uart_puts(commands[i].description);
        async_uart_puts("\n");
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
        async_uart_puts("Board Revision: ");
        async_uart_send_hex(get_board_revision());
    }

    prepare_memory_request();
    mailbox_call(MBOX_CH_PROP);

    if (get_memory_status() == 0x80000000)
    {
        async_uart_puts("Memory Size is: ");
        async_uart_send_hex(get_memory_size());
    }
}

static void cmd_get_timer(int argc, char* argv[])
{
    (void)argc; // 防止編譯警告
    (void)argv; // 防止編譯警告

    get_timetick();
    async_uart_puts("\n");
}

static void cmd_reboot(int argc, char* argv[])
{
    (void)argc; // 防止編譯警告
    (void)argv; // 防止編譯警告

    // --- 新增這行 ---
    async_uart_puts("Rebooting in T-minus 2 seconds...\n"); //reboot前跳提示
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
        async_uart_puts("Not Find Any File.");
    }
}

static void cmd_get_file_context(int argc, char* argv[])
{
    if (argc < 2)
    {
        async_uart_puts("Please Enter FileName");
        return;
    }
    else if (argc != 2)
    {
        async_uart_puts("False Argument");
        return;
    }

    extern CtxT dtb_ctx;

    void * header = (void *)dtb_ctx.initrd_start;
    bool find_context_result = CpioGetFileContext(header, argv[1]);

    if (find_context_result == false)
    {
        async_uart_puts("Cannot Find File");
        return;
    }
}

static void cmd_test_el1_brk(int argc, char* argv[]) 
{
    async_uart_puts("[TEST] EL1 BRK -> expect default_handler dump then hang\r\n");
    asm volatile ("brk #0");
    async_uart_puts("[TEST] should not reach here\r\n");
}

static void cmd_test_el1_svc(int argc, char* argv[]) 
{
    async_uart_puts("[TEST] EL1 SVC -> expect current-EL sync vector (default_handler) then hang\r\n");
    asm volatile ("svc #0");
    async_uart_puts("[TEST] should not reach here\r\n");
}

static void cmd_test_el1_bad_read(int argc, char* argv[]) 
{
    async_uart_puts("[TEST] EL1 bad read -> expect data abort dump then hang\r\n");
    volatile unsigned long *p = (unsigned long *)0xFFFFFFFFFFFF0000UL; // 依你的平台可換更明顯無效位址
    volatile unsigned long v = *p;
    (void)v;
    async_uart_puts("[TEST] should not reach here\r\n");
}

static void cmd_test_el0_user_mode(int argc, char* argv[])
{
    async_uart_puts("[TEST] EL0 user mode -> expect switch to EL0 then print user mode message\r\n");
    void* user_start_addr = 0;; // 依你的平台可換更明顯的EL0程式位址
    unsigned long user_size = 0;

    extern CtxT dtb_ctx;

    void * header = (void *)dtb_ctx.initrd_start;
    bool get_file_data_success = CpioGetFileData(header, argv[1], &user_start_addr, &user_size);


    if (get_file_data_success == false)
    {
        async_uart_puts("Cannot Find File");
        return;
    }
    else
    {
        core_timer_enable_second(1);
        static unsigned char user_stack[4096] __attribute__((aligned(16)));
        unsigned long user_stack_top = (unsigned long)(user_stack + sizeof(user_stack));
        enter_el0((unsigned long)user_start_addr, user_stack_top);
    }
    async_uart_puts("[TEST] should not reach here\r\n");

}

static void cmd_dtb_intc(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    extern CtxT dtb_ctx;
    InterruptInfoT* info = &dtb_ctx.interrupt_info;

    // --- diagnostics ---
    async_uart_puts("[diag] child_addr_cells[0]=");
    async_uart_send_hex(dtb_ctx.child_addr_cells[0]);
    async_uart_puts(" [1]=");
    async_uart_send_hex(dtb_ctx.child_addr_cells[1]);
    async_uart_puts(" [2]=");
    async_uart_send_hex(dtb_ctx.child_addr_cells[2]);
    async_uart_puts("\n");

    async_uart_puts("[diag] debug_l1_intc_depth=");
    async_uart_send_integer(dtb_ctx.interrupt_info.debug_l1_intc_depth);
    async_uart_puts(" debug_armctrl_depth=");
    async_uart_send_integer(dtb_ctx.interrupt_info.debug_armctrl_depth);
    async_uart_puts(" (0=never matched, expect both=3)\n");
    // --- end diagnostics ---

    async_uart_puts("arm_local_intc_base: ");
    if (info->have_arm_local_intc_base)
    {
        async_uart_puts("0x");
        async_uart_send_hex((unsigned int)info->arm_local_intc_base);
        async_uart_puts("  (expect 0x40000000)\n");
    }
    else
    {
        async_uart_puts("NOT FOUND\n");
    }

    async_uart_puts("arm_ctrl_intc_base:  ");
    if (info->have_arm_ctrl_intc_base)
    {
        async_uart_puts("0x");
        async_uart_send_hex((unsigned int)info->arm_ctrl_intc_base);
        async_uart_puts("  (expect 0x3F00B200)\n");
    }
    else
    {
        async_uart_puts("NOT FOUND\n");
    }
}

static void cmd_fdtb(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    extern CtxT dtb_ctx;

    async_uart_puts("=== Parsed DTB Context ===\n");

    async_uart_puts("initrd_start: ");
    if (dtb_ctx.have_initrd_start)
    {
        async_uart_send_hex((unsigned int)(dtb_ctx.initrd_start >> 32));
        async_uart_send_hex((unsigned int)(dtb_ctx.initrd_start & 0xFFFFFFFFUL));
        async_uart_puts("\n");
    }
    else { async_uart_puts("NOT FOUND\n"); }

    async_uart_puts("initrd_end:   ");
    if (dtb_ctx.have_initrd_end)
    {
        async_uart_send_hex((unsigned int)(dtb_ctx.initrd_end >> 32));
        async_uart_send_hex((unsigned int)(dtb_ctx.initrd_end & 0xFFFFFFFFUL));
        async_uart_puts("\n");
    }
    else { async_uart_puts("NOT FOUND\n"); }

    async_uart_puts("uart_mmio:    ");
    if (dtb_ctx.have_uart_reg)
    {
        async_uart_puts("base=0x");
        async_uart_send_hex((unsigned int)dtb_ctx.uart_mmio_base);
        async_uart_puts(" size=0x");
        async_uart_send_hex(dtb_ctx.uart_mmio_size);
        async_uart_puts("\n");
    }
    else { async_uart_puts("NOT FOUND\n"); }

    async_uart_puts("aux_mmio:     ");
    if (dtb_ctx.have_aux_reg)
    {
        async_uart_puts("base=0x");
        async_uart_send_hex((unsigned int)dtb_ctx.aux_mmio_base);
        async_uart_puts("\n");
    }
    else { async_uart_puts("NOT FOUND\n"); }

    async_uart_puts("gpio_mmio:    ");
    if (dtb_ctx.have_gpio_reg)
    {
        async_uart_puts("base=0x");
        async_uart_send_hex((unsigned int)dtb_ctx.gpio_mmio_base);
        async_uart_puts("\n");
    }
    else { async_uart_puts("NOT FOUND\n"); }

    async_uart_puts("arm_local_intc: ");
    if (dtb_ctx.interrupt_info.have_arm_local_intc_base)
    {
        async_uart_puts("0x");
        async_uart_send_hex((unsigned int)dtb_ctx.interrupt_info.arm_local_intc_base);
        async_uart_puts(" (expect 0x40000000)\n");
    }
    else { async_uart_puts("NOT FOUND\n"); }

    async_uart_puts("arm_ctrl_intc:  ");
    if (dtb_ctx.interrupt_info.have_arm_ctrl_intc_base)
    {
        async_uart_puts("0x");
        async_uart_send_hex((unsigned int)dtb_ctx.interrupt_info.arm_ctrl_intc_base);
        async_uart_puts(" (expect 0x3F00B200)\n");
    }
    else { async_uart_puts("NOT FOUND\n"); }
}