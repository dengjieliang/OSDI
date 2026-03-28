#ifndef IO_TASK_QUEUE_H
#define IO_TASK_QUEUE_H

#include "../Lib/base.h"
#include "../Shell/shell.h"

// 任務來源/用途分類：用於決定 task 的語意與後續處理流程。
typedef enum io_task_type
{
    IO_TASK_TYPE_GENERIC = 0,
    IO_TASK_TYPE_TIMER_CALLBACK,
    IO_TASK_TYPE_UART_RX,
    IO_TASK_TYPE_UART_TX,
} IoTaskType;

// 任務優先級：數值越小優先級越高。
typedef enum io_task_priority
{
    IO_TASK_PRIORITY_HIGH = 0,
    IO_TASK_PRIORITY_MEDIUM,
    IO_TASK_PRIORITY_LOW,
    IO_TASK_PRIORITY_COUNT,
} IoTaskPriority;

// 佇列容量與單一參數最大字串長度。
#define IO_TASK_MAX_COUNT 64
#define IO_TASK_MAX_ARG_LENGTH 256

// 任務物件：由 queue 內部固定池管理，不由外部動態配置。
typedef struct io_task
{
    // 任務類型（語意分類）
    IoTaskType type;
    // 排程優先級（執行順序控制）
    IoTaskPriority priority;
    // 任務執行函式（可為 NULL，表示純事件型 task）
    CommandFunc callback;
    // callback 參數數量
    int argc;
    // 參數內容的實體儲存，enqueue 時會做深拷貝，避免外部 buffer 失效
    char argv_storage[MAX_ARGS][IO_TASK_MAX_ARG_LENGTH];
    // callback 參數指標陣列，指向 argv_storage 內部元素
    char* argv[MAX_ARGS];
    // 截止時間（單位：tick）。
    // 若某型別 task 不使用 deadline，可設為 0。
    // 注意：若未來某流程仍需second，請在該流程中明確做 tick<->second 轉換。
    unsigned long long deadline_tick;
    // 連結串列指標（free list / ready queue 內部使用）
    struct io_task* next;
    // 固定池使用旗標
    bool in_use;
} IoTask;

// 初始化 queue：建立 free list 並清空各優先級 ready queue。
void io_task_queue_init(void);

// 通用 enqueue 入口。
// deadline_tick 統一使用 tick，若外部邏輯以 second 為主，需在呼叫前先轉換。
bool io_task_enqueue(
    IoTaskType type,
    IoTaskPriority priority,
    CommandFunc callback,
    int argc,
    char** argv,
    unsigned long long deadline_tick
);

// timer 專用包裝：本質仍呼叫通用 enqueue。
bool io_task_enqueue_timer(
    IoTaskPriority priority,
    CommandFunc callback,
    int argc,
    char** argv,
    unsigned long long deadline_tick
);

// UART 事件包裝（無 callback/參數，純事件型 task）。
bool io_task_enqueue_uart_rx(IoTaskPriority priority);
bool io_task_enqueue_uart_tx(IoTaskPriority priority);

// 取出下一個可執行 task（依優先級）。
bool io_task_dequeue(IoTask* out_task);

// queue 查詢輔助。
bool io_task_has_pending(void);
bool io_task_has_higher_priority_than(IoTaskPriority current_priority);

// 執行輔助：run_once 取一個 task 執行；run_all 持續執行到 queue 為空。
void io_task_run_once(void);
void io_task_run_all(void);


#endif