#include "../Kernel/io_task_queue.h"
#include "../Kernel/exception.h"
#include "../Lib/base.h"
#include "../Lib/string.h"

// 固定大小任務池：避免 bare-metal 環境下動態配置造成碎片化與不可預期延遲。
static IoTask task_pool[IO_TASK_MAX_COUNT];
// 空閒任務鍊結串列頭。
static IoTask* free_list_head = NULL;
// 各優先級 ready queue 的頭尾指標（FIFO）。
static IoTask* ready_head[IO_TASK_PRIORITY_COUNT];
static IoTask* ready_tail[IO_TASK_PRIORITY_COUNT];

// 從 free list 取出一個 task（呼叫端需已在臨界區）。
static IoTask* allocate_task_unsafe(void)
{
	IoTask* task = free_list_head;

	if (task == NULL)
	{
		return NULL;
	}

	free_list_head = task->next;
	task->next = NULL;
	task->in_use = true;
	return task;
}

// 將 task 歸還給 free list（呼叫端需已在臨界區）。
static void free_task_unsafe(IoTask* task)
{
	if (task == NULL)
	{
		return;
	}

	task->in_use = false;
	task->next = free_list_head;
	free_list_head = task;
}

// 依 task->priority 放入對應 ready queue 尾端（呼叫端需已在臨界區）。
static void push_ready_task_unsafe(IoTask* task)
{
	IoTaskPriority prio = task->priority;

	task->next = NULL;

	if (ready_head[prio] == NULL)
	{
		ready_head[prio] = task;
		ready_tail[prio] = task;
		return;
	}

	ready_tail[prio]->next = task;
	ready_tail[prio] = task;
}

// 依高到低優先級彈出一個 task（呼叫端需已在臨界區）。
static IoTask* pop_ready_task_unsafe(void)
{
	for (int p = 0; p < (int)IO_TASK_PRIORITY_COUNT; p += 1)
	{
		IoTask* task = ready_head[p];

		if (task == NULL)
		{
			continue;
		}

		ready_head[p] = task->next;
		if (ready_head[p] == NULL)
		{
			ready_tail[p] = NULL;
		}

		task->next = NULL;
		return task;
	}

	return NULL;
}

void io_task_queue_init(void)
{
	// 重新初始化時，先把 free list 頭清空，再重建所有 queue 狀態。
	free_list_head = NULL;

	// 每個優先級各有一條 FIFO ready queue。
	for (int p = 0; p < (int)IO_TASK_PRIORITY_COUNT; p += 1)
	{
		ready_head[p] = NULL;
		ready_tail[p] = NULL;
	}

	// 將整個 task pool 串成 free list，之後 enqueue 時直接取用。
	for (int i = 0; i < IO_TASK_MAX_COUNT; i += 1)
	{
		task_pool[i].type = IO_TASK_TYPE_GENERIC;
		task_pool[i].priority = IO_TASK_PRIORITY_LOW;
		task_pool[i].callback = NULL;
		task_pool[i].argc = 0;
		task_pool[i].deadline_tick = 0;
		task_pool[i].in_use = false;

		for (int a = 0; a < MAX_ARGS; a += 1)
		{
			task_pool[i].argv_storage[a][0] = '\0';
			task_pool[i].argv[a] = task_pool[i].argv_storage[a];
		}

		task_pool[i].next = free_list_head;
		free_list_head = &task_pool[i];
	}
}

bool io_task_enqueue(
	IoTaskType type,
	IoTaskPriority priority,
	CommandFunc callback,
	int argc,
	char** argv,
	unsigned long long deadline_tick
)
{
	// priority 越界代表呼叫端傳入了無效排程層級。
	if ((int)priority < 0 || priority >= IO_TASK_PRIORITY_COUNT)
	{
		return false;
	}

	// 參數數量不可為負，也不能超過固定上限。
	if (argc < 0 || argc > MAX_ARGS)
	{
		return false;
	}

	// 有參數時必須提供 argv；無參數時才允許 NULL。
	if (argc > 0 && argv == NULL)
	{
		return false;
	}

	// 進入臨界區，避免 enqueue 與 dequeue/IRQ 同時修改 queue 狀態。
	unsigned long saved_daif = local_irq_save();
	IoTask* task = allocate_task_unsafe();

	// pool 耗盡時直接失敗，不做動態擴容。
	if (task == NULL)
	{
		local_irq_restore(saved_daif);
		return false;
	}

	// 將呼叫端提供的 metadata 寫入新 task。
	task->type = type;
	task->priority = priority;
	task->callback = callback;
	task->argc = argc;
	task->deadline_tick = deadline_tick;

	// 先清空所有 argv slot，避免沿用舊 task 的殘留內容。
	for (int i = 0; i < MAX_ARGS; i += 1)
	{
		task->argv_storage[i][0] = '\0';
		task->argv[i] = task->argv_storage[i];
	}

	// 深拷貝參數字串到 task 內部 storage，避免外部 buffer 壽命問題。
	for (int i = 0; i < argc; i += 1)
	{
		unsigned long copy_len = 0;

		if (argv[i] != NULL)
		{
			copy_len = strlen(argv[i]);
			if (copy_len >= IO_TASK_MAX_ARG_LENGTH)
			{
				copy_len = IO_TASK_MAX_ARG_LENGTH - 1;
			}

			strncpy(task->argv_storage[i], argv[i], copy_len);
		}

		task->argv_storage[i][copy_len] = '\0';
	}

	// 依優先級放進對應 ready queue，並維持同優先級 FIFO。
	push_ready_task_unsafe(task);
	local_irq_restore(saved_daif);
	return true;
}

bool io_task_enqueue_timer(
	IoTaskPriority priority,
	CommandFunc callback,
	int argc,
	char** argv,
	unsigned long long deadline_tick
)
{
	// timer task 只是通用 task 的一種型別包裝。
	return io_task_enqueue(
		IO_TASK_TYPE_TIMER_CALLBACK,
		priority,
		callback,
		argc,
		argv,
		deadline_tick
	);
}

bool io_task_enqueue_uart_rx(IoTaskPriority priority)
{
	// UART RX 目前先作為純事件 task；若之後要帶資料可再擴充 payload 欄位。
	return io_task_enqueue(IO_TASK_TYPE_UART_RX, priority, NULL, 0, NULL, 0);
}

bool io_task_enqueue_uart_tx(IoTaskPriority priority)
{
	// UART TX 目前也先作為純事件 task；deadline 不使用時填 0。
	return io_task_enqueue(IO_TASK_TYPE_UART_TX, priority, NULL, 0, NULL, 0);
}

bool io_task_dequeue(IoTask* out_task)
{
	// out_task 是呼叫端提供的輸出緩衝區，不能為 NULL。
	if (out_task == NULL)
	{
		return false;
	}

	// dequeue 同樣需要臨界區，因為它會修改 ready queue 與 free list。
	unsigned long saved_daif = local_irq_save();
	IoTask* task = pop_ready_task_unsafe();

	// queue 為空時直接返回 false。
	if (task == NULL)
	{
		local_irq_restore(saved_daif);
		return false;
	}

	// 將 task 內容複製到呼叫端提供的物件，再把池中節點回收到 free list。
	*out_task = *task;
	free_task_unsafe(task);
	local_irq_restore(saved_daif);
	return true;
}

bool io_task_has_pending(void)
{
	// 只要任一優先級 queue 非空，就代表還有待處理任務。
	bool has_pending = false;
	unsigned long saved_daif = local_irq_save();

	for (int p = 0; p < (int)IO_TASK_PRIORITY_COUNT; p += 1)
	{
		if (ready_head[p] != NULL)
		{
			has_pending = true;
			break;
		}
	}

	local_irq_restore(saved_daif);
	return has_pending;
}

// Preemption helper:
// 用來判斷目前是否已有比 current_priority 更高優先級的 task 進入 ready queue，
// 供未來 preemption / yield 判斷邏輯使用。
bool io_task_has_higher_priority_than(IoTaskPriority current_priority)
{
	// HIGH 已經是最高優先級，不可能再有更高者。
	if ((int)current_priority <= 0)
	{
		return false;
	}

	// 若呼叫端傳入超過範圍的值，將搜尋上限收斂到陣列尾端。
	if (current_priority > IO_TASK_PRIORITY_COUNT)
	{
		current_priority = IO_TASK_PRIORITY_COUNT;
	}

	bool found = false;
	unsigned long saved_daif = local_irq_save();

	for (int p = 0; p < (int)current_priority; p += 1)
	{
		if (ready_head[p] != NULL)
		{
			// 找到任何一條更高優先級的非空 queue，就可提早返回 true。
			found = true;
			break;
		}
	}

	local_irq_restore(saved_daif);
	return found;
}

// Run API:
// 這組 API 提供 queue 消費端執行 pending tasks 的入口。
// run_once 處理一個 task；run_all 持續處理直到所有 ready queue 清空。
void io_task_run_once(void)
{
	// 這裡用 stack 上的暫存 task 接住 dequeue 結果，之後安全地執行 callback。
	IoTask task;

	if (!io_task_dequeue(&task))
	{
		// 沒有任務時不做任何事。
		return;
	}

	if (task.callback != NULL)
	{
		// argv 指向的是已複製到 task 內部儲存的參數內容。
		task.callback(task.argc, task.argv);
	}
	// 若 callback 為 NULL，代表這是純事件型 task，目前由更上層流程決定是否需要額外處理。
}

void io_task_run_all(void)
{
	// 持續處理直到所有 ready queue 都清空。
	while (io_task_has_pending())
	{
		io_task_run_once();
	}
}