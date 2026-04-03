#include "../Kernel/time_manager.h"
#include "../Driver/time.h"
#include "../Lib/base.h"
#include "../Kernel/exception.h"
#include "../Lib/string.h"
#include "../Lib/utils.h"
#include "../Driver/uart.h"
#include "../Shell/shell.h"
// +2026/03/29 Group 4 變更點：新增 io_task_queue 以將到期 callback 轉為 deferred task。
#include "../Kernel/io_task_queue.h"

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

// +2026/03/29 Group 4 變更點：新增分派上下文池（TimerDispatchCtx）。
// timer_interrupt_router 不再直接執行 callback，而是把到期事件的執行上下文
// 保存在此池中，透過 io_task_queue 以 deferred task 形式排程執行。
// 池大小與 timer_pool 相同（MAX_TIMERS），確保即使所有 timer 同時到期也不會溢出。
typedef struct timer_dispatch_ctx
{
    double scheduled_seconds;        // 原始設定延遲（秒），用於格式化輸出
    CommandFunc callback;            // 到期後要執行的使用者 callback
    int argc;                        // callback 的參數數量
    char argv_storage[MAX_ARGS][MAX_MESSAGE_LENGTH];  // callback 參數的實體儲存
    char* argv[MAX_ARGS];            // 指向 argv_storage 內部元素的指標陣列
    bool in_use;                     // 此槽位是否正在使用中
} TimerDispatchCtx;

static TimerDispatchCtx dispatch_pool[MAX_TIMERS];

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

// +2026/03/29 Group 4 變更點：以下函式均為 timer callback decouple 所新增。

// 從 dispatch_pool 中分配一個可用槽位，回傳其 index；池滿時回傳 -1。
// 以 local_irq_save/restore 保護，可安全在 nested IRQ 或 deferred task 中呼叫。
static int allocate_dispatch_ctx(void)
{
    unsigned long saved_daif = local_irq_save();
    int idx = -1;

    for (int i = 0; i < MAX_TIMERS; i++)
    {
        if (!dispatch_pool[i].in_use)
        {
            dispatch_pool[i].in_use = true;
            idx = i;
            break;
        }
    }

    local_irq_restore(saved_daif);
    return idx;
}

// 釋放 dispatch_pool 中指定索引的槽位。
static void free_dispatch_ctx(int idx)
{
    if (idx < 0 || idx >= MAX_TIMERS)
    {
        return;
    }

    unsigned long saved_daif = local_irq_save();
    dispatch_pool[idx].in_use = false;
    local_irq_restore(saved_daif);
}

// fallback：當 dispatch_pool 或 io_task_queue 耗盡時，直接在當前路徑執行 callback。
// 此情況屬非預期（池大小等於 MAX_TIMERS，正常情況下不應觸發），以 fallback 避免
// callback 完全遺失。
static void timer_execute_fallback(timer_event_t* expired)
{
    char* pass_argv[MAX_ARGS];

    for (int i = 0; i < expired->argc; i++)
    {
        pass_argv[i] = expired->message[i];
    }

    unsigned long int_p = (unsigned long)expired->scheduled_seconds;
    unsigned int dec4 = (unsigned int)((expired->scheduled_seconds - (double)int_p) * 10000);

    async_uart_puts("\n[");
    get_current_second_string();
    async_uart_puts("] scheduled_delay=");
    async_uart_send_unsigned_long_integer(int_p);
    async_uart_send('.');
    async_uart_send_decimal_part((int)dec4, 4);
    async_uart_puts(" message=");
    expired->callback(expired->argc, pass_argv);
    async_uart_puts("\n");
}

// +2026/03/29 Group 4 變更點：每個到期 timer 的 deferred 執行入口。
// 由 io_task_queue 在 IRQ 開啟狀態下呼叫，保證不在 IRQ 路徑中執行重工作。
// argv[0]：dispatch_pool 中對應槽位的十進位索引字串（由 timer_interrupt_router 塞入）。
static void timer_dispatch_runner(int argc, char** argv)
{
    if (argc < 1 || argv == NULL || argv[0] == NULL)
    {
        return;
    }

    // 將 argv[0] 解析為 dispatch pool 索引（io_task_queue 已深拷貝此字串，可安全讀取）
    int idx = 0;
    const char* s = argv[0];

    while (*s >= '0' && *s <= '9')
    {
        idx = idx * 10 + (int)(*s - '0');
        s++;
    }

    if (idx < 0 || idx >= MAX_TIMERS || !dispatch_pool[idx].in_use)
    {
        // 索引失效（不應發生），靜默放棄。
        return;
    }

    TimerDispatchCtx* ctx = &dispatch_pool[idx];

    // 格式化輸出（與 Group 4 前行為完全一致）
    unsigned long int_part = (unsigned long)ctx->scheduled_seconds;
    unsigned int dec4 = (unsigned int)((ctx->scheduled_seconds - (double)int_part) * 10000);

    async_uart_puts("\n[");
    get_current_second_string();
    async_uart_puts("] scheduled_delay=");
    async_uart_send_unsigned_long_integer(int_part);
    async_uart_send('.');
    async_uart_send_decimal_part((int)dec4, 4);
    async_uart_puts(" message=");
    ctx->callback(ctx->argc, ctx->argv);
    async_uart_puts("\n");

    free_dispatch_ctx(idx);
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

    daif_mask_all();

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
    daif_unmask_all();

    return true;
}

// +2026/03/29 Group 4 變更點：IRQ 路徑只做「到期收集 + enqueue + re-arm」，絕不直接執行 callback。
//
// 設計說明：
//   - 此函式由 exception.c 的 deferred_timer_irq_task 呼叫（已在 IRQ 開啟狀態下執行）。
//   - 為每個到期事件分配一個 TimerDispatchCtx 槽位，複製執行上下文（秒數、回呼、參數）。
//   - 透過 io_task_enqueue_timer 將 timer_dispatch_runner 排入 HIGH 優先級 queue。
//   - 實際 callback 執行由 io_task_run_all 統一調度，可被更高優先 IRQ 打斷（nested）。
//   - 硬體 compare re-arm 保留在此函式末段（最小必要流程）。
//   - 原本的排序邏輯在 add_timer 中，此函式不動。
void timer_interrupt_router(void)
{
    unsigned long long current_tick = get_current_tick();

    while (timer_list_head != NULL && timer_list_head->trigger_tick <= current_tick)
    {
        timer_event_t* expired = timer_list_head;
        timer_list_head = expired->next;

        // 為到期事件分配 dispatch context 槽位
        int ctx_idx = allocate_dispatch_ctx();

        if (ctx_idx < 0)
        {
            // dispatch pool 耗盡（非預期，兩池等大，正常不觸發）：fallback 直接執行。
            timer_execute_fallback(expired);
            free_timer(expired);
            continue;
        }

        // 將 timer event 攜帶的執行上下文完整複製到 dispatch context
        TimerDispatchCtx* ctx = &dispatch_pool[ctx_idx];
        ctx->scheduled_seconds = expired->scheduled_seconds;
        ctx->callback = expired->callback;
        ctx->argc = expired->argc;

        for (int i = 0; i < MAX_ARGS; i++)
        {
            ctx->argv_storage[i][0] = '\0';
            ctx->argv[i] = ctx->argv_storage[i];
        }

        for (int i = 0; i < expired->argc; i++)
        {
            strncpy(ctx->argv_storage[i], expired->message[i], MAX_MESSAGE_LENGTH - 1);
            ctx->argv_storage[i][MAX_MESSAGE_LENGTH - 1] = '\0';
        }

        // 將 dispatch pool 索引轉為字串，作為 timer_dispatch_runner 的 argv[0]
        // io_task_enqueue 會深拷貝此字串，stack buffer 在 enqueue 返回後即可廢棄
        char idx_str[8];
        char* enq_argv[1];
        uint_to_str((unsigned int)ctx_idx, idx_str, (int)sizeof(idx_str));
        enq_argv[0] = idx_str;

        bool enqueued = io_task_enqueue_timer(
            IO_TASK_PRIORITY_HIGH,
            timer_dispatch_runner,
            1,
            enq_argv,
            expired->trigger_tick
        );

        if (!enqueued)
        {
            // io_task queue 滿了（非預期）：釋放 dispatch context 並 fallback 直接執行。
            free_dispatch_ctx(ctx_idx);
            timer_execute_fallback(expired);
        }

        // timer_event 在 dispatch context 複製完成後即可回收；
        // dispatch context 則由 timer_dispatch_runner 執行後才釋放。
        free_timer(expired);
    }

    // 硬體 compare re-arm：IRQ 最小必要流程，排序邏輯維持不變。
    if (timer_list_head != NULL)
    {
        set_core_timer_interrupt_tick(timer_list_head->trigger_tick);
    }
    else
    {
        // queue 為空：設極大值避免 timer 頻繁觸發無效 IRQ
        set_core_timer_interrupt_tick(~0ULL);
    }
}