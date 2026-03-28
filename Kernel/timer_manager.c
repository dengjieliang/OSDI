#include "../Kernel/time_manager.h"
#include "../Driver/time.h"
#include "../Lib/base.h"
#include "../Kernel/exception.h"
#include "../Lib/string.h"
#include "../Driver/uart.h"
#include "../Shell/shell.h"

#define MAX_TIMERS 64

typedef struct timer_event
{
    unsigned long long trigger_tick;
    double scheduled_seconds;
    CommandFunc callback;
    int argc;
    char message[MAX_ARGS][MAX_MESSAGE_LENGTH];
    struct timer_event* next; // 指向下一個定時器事件的指標
    bool in_use; // 是否正在使用中
} timer_event_t;

// 靜態陣列，開機時因為在 .bss 區段，in_use 預設都會是 false (0)
static timer_event_t timer_pool[MAX_TIMERS];
static timer_event_t* timer_list_head = NULL; // 定時器事件鏈表的頭指標

static timer_event_t* allocate_timer() 
{
    for (int i = 0; i < MAX_TIMERS; i++) 
    {
        if (!timer_pool[i].in_use) 
        {
            timer_pool[i].in_use = true;
            return &timer_pool[i];
        }
    }

    return NULL; // 如果 32 個計時器都滿了，就回傳 NULL
}

static void free_timer(timer_event_t* timer) 
{
    timer->in_use = false; // 時間到了，把 flag 設回 false 就能回收再利用！
}

bool add_timer(CommandFunc callback, int argc, char** argv, double after_seconds)
{
    unsigned long long enqueue_tick = get_current_tick();
    timer_event_t* new_timer = allocate_timer();

    if (new_timer == NULL) 
    {
        return false;
    }

    if (argv == NULL)
    {
        free_timer(new_timer);
        return false;
    }

    for (int i = 0; i < argc; i++) 
    {
        strncpy(new_timer->message[i], argv[i], MAX_MESSAGE_LENGTH - 1);
        new_timer->message[i][MAX_MESSAGE_LENGTH - 1] = '\0';
    }
    new_timer->callback = callback;
    new_timer->argc = argc;
    new_timer->scheduled_seconds = after_seconds;
    new_timer->trigger_tick = enqueue_tick + tansfer_seconds_to_ticks(after_seconds);
    new_timer->next = NULL;
    new_timer->in_use = true;

    mask_all_exceptions();

    if (timer_list_head == NULL || new_timer->trigger_tick < timer_list_head->trigger_tick) 
    {
        new_timer->next = timer_list_head;
        timer_list_head = new_timer;
    } 
    else 
    {
        timer_event_t* current = timer_list_head;
        while (current->next != NULL && current->next->trigger_tick < new_timer->trigger_tick) 
        {
            current = current->next;
        }
        new_timer->next = current->next;
        current->next = new_timer;
    }

    set_core_timer_interrupt_tick(timer_list_head->trigger_tick);
    unmask_all_exceptions();

    return true;
}

void timer_interrupt_router()
{
    unsigned long long current_tick = get_current_tick();

    while (timer_list_head != NULL && timer_list_head->trigger_tick <= current_tick) 
    {
        timer_event_t* expired_timer = timer_list_head;
        timer_list_head = timer_list_head->next;

        // scheduled_seconds already stores the delay in seconds, no need to convert
        unsigned long executed_second_integer_part = (unsigned long)expired_timer->scheduled_seconds;
        unsigned int executed_second_decimal_part_4digit = (unsigned int)((expired_timer->scheduled_seconds - (double)executed_second_integer_part) * 10000);

        char* pass_argv[MAX_ARGS];
        for (int i = 0; i < expired_timer->argc; i++)
        {
            pass_argv[i] = expired_timer->message[i];
        }

        async_uart_puts("\n[");
        get_current_second_string();
        async_uart_puts("] scheduled_delay=");
        async_uart_send_unsigned_long_integer(executed_second_integer_part);
        async_uart_send('.');
        async_uart_send_decimal_part((int)executed_second_decimal_part_4digit, 4);
        async_uart_puts(" message=");
        expired_timer->callback(expired_timer->argc, pass_argv);
        async_uart_puts("\n");

        free_timer(expired_timer);
    }

    if (timer_list_head != NULL)
    {
        // 如果有，把硬體 Timer 的下一次觸發時間，設定為新 Head 的時間
        set_core_timer_interrupt_tick(timer_list_head->trigger_tick);
    }
    else
    {
        // 如果 Queue 空了，可以選擇關閉 Timer 中斷 (Mask)，或是設定一個極大值
        // 避免 Timer 一直狂叫
        set_core_timer_interrupt_tick(~0ULL); // 設定為最大值 (永遠不到期)
    }
}