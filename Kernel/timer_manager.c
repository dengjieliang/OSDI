#include "../Kernel/time_manager.h"
#include "../Driver/time.h"
#include "../Lib/base.h"
#include "../Lib/string.h"
#include "../Driver/uart.h"

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
    strncpy(new_timer->message, message, strlen(message));
    new_timer->message[sizeof(new_timer->message) - 1] = '\0'; // 確保字串是以 null 結尾的
    new_timer->trigger_tick = get_current_tick() + tansfer_seconds_to_ticks(executeAfterSeconds);
    new_timer->next = NULL;
}