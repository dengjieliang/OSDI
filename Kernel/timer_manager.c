#include "../Kernel/time_manager.h"
#include "../Driver/time.h"
#include "../Lib/base.h"
#include "../Kernel/exception.h"

#define MAX_TIMERS 64

typedef struct timer_event
{
    unsigned long long trigger_tick;
    timer_callback_t callback; // 定時器到期後要執行的函式
    char* message; // 定時器到期後要傳遞的訊息
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

bool AddTaskToTimerManager(timer_callback_t task, char* message, unsigned long executeAfterSeconds)
{
    timer_event_t* new_timer = allocate_timer();

    if (new_timer == NULL) 
    {
        return false;
    }

    new_timer->callback = task;
    new_timer->message = (char*)message;
    new_timer->trigger_tick = get_current_tick() + tansfer_seconds_to_ticks(executeAfterSeconds);
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

    unmask_all_exceptions();
    set_core_timer_interrupt_tick(new_timer->trigger_tick);

    return true;
}