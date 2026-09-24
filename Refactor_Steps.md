# Lab2/Lab3 規格補齊 + 重構施工步驟

依賴 spec：
- https://oscapstone.github.io/labs/lab2.html (BE3 simple_malloc)
- https://oscapstone.github.io/labs/lab3.html (AE1 setTimeout, AE2 concurrent I/O)

**執行順序有依賴性，請照 Stage 0 → 5 做。** Stage 6 是純清理，隨時可做。

> 專案規則提醒（`.github/AGENTS.md`）：新增的程式碼註解一律以 `// +2026/08/01` 開頭。

---

## Stage 0：準備

### S0.1 開分支

```bash
git checkout -b refactor/spec-alignment
```

### S0.2 記錄 baseline

```bash
make clean && make all 2>&1 | grep -i "warning\|error"
```

現在應該只會看到兩個 warning：
- `Shell/shell.c:611: 'print_message' defined but not used`
- `Memory/allocator.c:3: missing terminating " character`

**每個 Stage 做完都重跑一次，warning 數量只能變少不能變多。**

### S0.3 記錄記憶體基準線

```bash
aarch64-linux-gnu-size build/kernel8.elf
aarch64-linux-gnu-nm --size-sort -S build/kernel8.elf | tail -8
```

目前實測（2026/08/01）：

```
   text     data      bss      dec
  38732      464   957705   996901
```

三個物件池佔掉 `.bss` 的 **85%**：

| 符號 | 大小 | 佔比 |
|---|---|---|
| `task_pool` | 273,408 B (267 KB) | 28.5% |
| `dispatch_pool` | 272,384 B (266 KB) | 28.4% |
| `timer_pool` | 265,216 B (259 KB) | 27.7% |
| **小計** | **811,008 B (792 KB)** | **84.7%** |

原因都一樣：為了支援 spec 沒要求的 `setTimeout CALLBACK [ARGS...]`，每個 slot 都帶 `[MAX_ARGS=16][256]` 的 argv 儲存區 = 4KB，再乘以 64 個 slot。

**Stage 2 + S5.1 做完後的預期值**：`dispatch_pool` 整個消失、另兩個池的 slot 從 ~4.1KB 降到 ~300B 以下，`.bss` 應該落在 **170 KB 左右**。這是最客觀的驗收指標，做完回頭跑一次上面的指令對照。

### S0.4 確認能跑起來

```bash
make qemu
```

另一個終端機用 `Tools/send_kernel.py` 送 kernel 進去（QEMU 開在 `127.0.0.1:8888`）。確認 shell prompt 出得來、`help` 有反應。這是後面每個 Stage 的驗證基礎。

---

## Stage 1：Lab2 BE3 `simple_malloc`（10%，暖身用）

目前 `Memory/allocator.c` 有實作但**零呼叫點**，等於沒交。

### S1.1 修 include typo

**檔案**：`Memory/allocator.c` 第 3 行

```c
#include "../Board/common.h""     // 現況：結尾多一個雙引號
```

刪掉多餘的 `"`。GCC 對這種寫法只給 warning 不報錯，所以它一直活著沒被發現。

### S1.2 改成 spec 要求的名稱

spec 原文要求：`void* simple_malloc(size_t size)`

**檔案 1**：`Memory/allocator.h`

- `SimpleAllocator` → `simple_malloc`
- 專案沒有 `stddef.h`（`-nostdlib`），所以 `size_t` 自己在 `Lib/base.h` 加：
  ```c
  #ifndef SIZE_T_DEFINED
  #define SIZE_T_DEFINED
  typedef unsigned long size_t;
  #endif
  ```
  然後 `allocator.h` 加 `#include "../Lib/base.h"`。

**檔案 2**：`Memory/allocator.c`

- 函式名同步改掉。內部邏輯（`__bss_end` 起頭、`ALIGN8`、`MMIO_BASE` 上界檢查）**不用動，是對的**。

### S1.3 加真正的呼叫點

沒有呼叫點就無法 demo。最省事的做法是加一個 shell 命令。

**檔案**：`Shell/shell.c`

1. 頂部 `#include "../Memory/allocator.h"`
2. 在 prototype 區（第 16-37 行那塊）加 `static void cmd_malloc(int argc, char* argv[]);`
3. 在 `commands[]` 陣列（第 39-57 行）加一列：
   ```c
   {"malloc", "Allocate N bytes and print the address", cmd_malloc},
   ```
4. 實作：解析 `argv[1]` 為數字（用現成的 `aoti()` 或 `strtoul()`，在 `Lib/utils.h`），呼叫 `simple_malloc`，把回傳位址用 `async_uart_send_hex()` 印出來。NULL 要印錯誤訊息。

### S1.4 驗證

`malloc 16` 連打三次，位址應該遞增且每次差 16（8-byte 對齊下 16 已對齊）。`malloc 1` 三次，位址應該差 8（因為 `ALIGN8`）。

---

## Stage 2：Lab3 AE1 `setTimeout` 回歸 spec（20%）+ 砍掉三層池

**這是整個計畫最重要的一步。** 現在 `setTimeout CALLBACK [ARGS...] SECONDS` 不符合 spec 的 `setTimeout MESSAGE SECONDS`，demo 打 `setTimeout Hello 5` 會回 `Error: callback command not found` 直接失敗。

而且這個泛化正是三層物件池（`timer_pool` → `dispatch_pool` → `io_task_queue.task_pool`）、兩次深拷貝、int→字串→int 索引編碼的唯一理由。**規格對齊和大幅簡化是同一個動作。**

### S2.0 先理解一個關鍵事實（動手前務必確認）

追一次呼叫鏈：

```
el1_irq_entry (exception_table.S:185)
  → el1_irq_handler_c (exception.c:292)
    → irq_routing (exception.c:128)
      → Phase 1: enqueue deferred_timer_irq_task
      → Phase 2: daif_unmask_all();  ← 中斷在這裡已經打開
                 io_task_run_all();
                   → deferred_timer_irq_task (exception.c:14)
                     → timer_interrupt_router (timer_manager.c:235)
```

**`timer_interrupt_router()` 本身已經是在「IRQ 開啟的 deferred task」裡執行的。**

所以它裡面再把工作包成 `TimerDispatchCtx` 丟回 `io_task_queue` 排一次，是**完全多餘的第二層 deferred**。它可以直接執行到期的工作，AE2 的「不在 IRQ 路徑做重工作」要求依然滿足。

確認這件事之後，下面的刪除才有信心。

### S2.1 改 `add_timer` 簽章

**檔案**：`Kernel/time_manager.h`

```c
// 現況
bool add_timer(CommandFunc callback, int argc, char** argv, double after_seconds);

// 改成
bool add_timer(CommandFunc callback, const char* message, double after_seconds);
```

語意：
- `callback != NULL` → 到期時呼叫 `callback(0, NULL)`
- `callback == NULL && message` 非空 → 到期時印出 `message`
- 兩者皆空 → 純延遲，不輸出任何東西

### S2.2 `timer_event_t` 瘦身

**檔案**：`Kernel/timer_manager.c` 第 14-23 行

```c
typedef struct timer_event
{
    unsigned long long trigger_tick;
    unsigned long long registered_tick;   // +2026/08/01 AE1 要求印出「命令執行當下時間」
    double scheduled_seconds;
    CommandFunc callback;
    char message[MAX_MESSAGE_LENGTH];     // 從 [MAX_ARGS][MAX_MESSAGE_LENGTH] 降成一維
    struct timer_event* next;
    bool in_use;
} timer_event_t;
```

`argc` 欄位直接刪掉。`message` 從 16×256B 變成 256B，`timer_pool[64]` 從 256KB 降到約 20KB。

新增 `registered_tick` 是因為 spec 原文要求：

> prints MESSAGE after SECONDS **with the current time and the command executed time**

### S2.3 刪除整個第二層 dispatch 機制

**檔案**：`Kernel/timer_manager.c`，以下全部刪掉：

| 行號 | 內容 |
|---|---|
| 33-41 | `typedef struct timer_dispatch_ctx` / `TimerDispatchCtx` |
| 43 | `static TimerDispatchCtx dispatch_pool[MAX_TIMERS];` |
| 68-85 | `allocate_dispatch_ctx()` |
| 88-98 | `free_dispatch_ctx()` |
| 103-124 | `timer_execute_fallback()` |
| 129-169 | `timer_dispatch_runner()`（含那段 int→字串→int 解碼） |

連帶可刪的 include：第 10 行 `#include "../Kernel/io_task_queue.h"`、第 6 行 `#include "../Lib/utils.h"`（`uint_to_str` 不再需要）。

> `Lib/utils.c` 的 `uint_to_str()` 會變成沒人用。可以留著（無害）或一併刪，看你。

### S2.4 保護 `allocate_timer` / `free_timer`

**檔案**：`Kernel/timer_manager.c` 第 45-62 行

現在這兩個函式**沒有臨界區保護**。Stage 2 之後 `timer_interrupt_router` 會在 IRQ 開啟下呼叫 `free_timer`，而 shell 或 callback 可能同時呼叫 `add_timer` → `allocate_timer`，兩邊搶同一個 `in_use` 旗標會出事。

在兩個函式內部各包一層：

```c
static timer_event_t* allocate_timer(void)
{
    // +2026/08/01 IRQ 開啟後本函式可能被巢狀呼叫，需臨界區保護 in_use 掃描
    unsigned long saved_daif = local_irq_save();
    timer_event_t* result = NULL;
    for (int i = 0; i < MAX_TIMERS; i++)
    {
        if (timer_pool[i].in_use == false)
        {
            timer_pool[i].in_use = true;
            result = &timer_pool[i];
            break;
        }
    }
    local_irq_restore(saved_daif);
    return result;
}
```

`free_timer` 同理。

### S2.5 重寫 `add_timer`

**檔案**：`Kernel/timer_manager.c` 第 171-224 行

改動範圍：
- 刪掉第 187-191 行的 `for (i < argc) strncpy(new_timer->message[i], ...)` 迴圈
- 改成單一字串複製：`message != NULL` 時 `strncpy(new_timer->message, message, MAX_MESSAGE_LENGTH - 1)` 並補 `'\0'`；`message == NULL` 時設 `new_timer->message[0] = '\0'`
- 刪掉第 181-185 行的 `argc > 0 && argv == NULL` 檢查（不再有 argv）
- 新增 `new_timer->registered_tick = enqueue_tick;`
- **第 199-221 行的排序插入 + `set_core_timer_interrupt_tick` + `local_irq_save/restore` 保持原樣，那段是對的**

### S2.6 重寫 `timer_interrupt_router`

**檔案**：`Kernel/timer_manager.c` 第 235-309 行

這是本 Stage 的核心。新結構：

```c
// +2026/08/01 到期事件的實際輸出與執行；在 IRQ 開啟狀態下呼叫
static void timer_fire(CommandFunc callback,
                       const char* message,
                       double scheduled_seconds,
                       unsigned long long registered_tick)
{
    if (callback == NULL && message[0] == '\0')
    {
        return;   // 純延遲用途，不輸出
    }

    unsigned long reg_int = 0;
    unsigned int reg_dec = 0;
    tansfer_ticks_to_seconds(registered_tick, &reg_int, &reg_dec);

    // 格式：[now=X] [registered=Y] [after=Z] MESSAGE
    // now      → get_current_second_string()
    // registered → reg_int '.' reg_dec  (async_uart_send_decimal_part(reg_dec, 4))
    // after    → scheduled_seconds 拆整數/小數部分，作法照舊 timer_execute_fallback:112-113
    // ...你自己組裝輸出...

    if (callback != NULL)
    {
        callback(0, NULL);
    }
    else
    {
        async_uart_puts(message);
    }
    async_uart_puts("\n");
}

void timer_interrupt_router(void)
{
    while (1)
    {
        // 1. 臨界區內：取出一個到期事件，把要用的資料複製到區域變數，回收節點
        //    （必須複製，因為 free_timer 之後該節點隨時可能被重用）
        // 2. 離開臨界區
        // 3. 在臨界區外呼叫 timer_fire(...)  ← 重工作不佔臨界區
        // 4. 沒有到期事件時 break
    }

    // re-arm：保持原本 301-309 行的邏輯
    //   timer_list_head != NULL → set_core_timer_interrupt_tick(head->trigger_tick)
    //   否則                     → set_core_timer_interrupt_tick(~0ULL)
    // 這段也要包在臨界區內
}
```

三個容易踩的點：

1. **一定要先複製再 `free_timer`**。`callback`、`scheduled_seconds`、`registered_tick` 複製到區域變數，`message` 用區域 `char buf[MAX_MESSAGE_LENGTH]` 接。這正是舊版 `io_task_dequeue` 犯的錯（見 S5.1）。
2. **`get_current_tick()` 要在迴圈內每輪重讀**。callback 執行可能耗時，用舊的 tick 判斷會漏掉期間到期的事件。
3. **`timer_fire()` 必須在臨界區外呼叫**。它會做 UART 輸出，佔著 IRQ 關閉會拖垮延遲。

### S2.7 更新所有 `add_timer` 呼叫點

**檔案**：`Shell/shell.c`，共 4 處：

| 行號 | 現況 | 改成 |
|---|---|---|
| 407 | `add_timer(NULL, 0, NULL, 2.0)` | `add_timer(NULL, NULL, 2.0)` |
| 570 | `add_timer(callback, timeout_argc, &argv[1], after_seconds)` | 見 S2.8 |
| 624 | `add_timer(cmd_update_two_seconds, 0, NULL, 2.0)` | `add_timer(cmd_update_two_seconds, NULL, 2.0)` |
| 632 | `add_timer(cmd_update_two_seconds, 0, NULL, 2.0)` | `add_timer(cmd_update_two_seconds, NULL, 2.0)` |

### S2.8 重寫 `cmd_timeout_print_message`

**檔案**：`Shell/shell.c` 第 540-574 行

```c
static void cmd_timeout_print_message(int argc, char* argv[])
{
    if (argc != 3)
    {
        async_uart_puts("Usage: setTimeout MESSAGE SECONDS\n");
        return;
    }

    if (is_valid_seconds_format(argv[2]) == false)
    {
        async_uart_puts("Error: SECONDS must be a non-negative decimal number\n");
        return;
    }

    if (add_timer(NULL, argv[1], atof(argv[2])) == false)
    {
        async_uart_puts("Error: failed to register timeout event\n");
    }
}
```

**關鍵改變：不再呼叫 `compare_command()`。** MESSAGE 就是字面字串，不是命令名。

同步更新 `commands[]` 第 55 行的說明字串為 `"setTimeout MESSAGE SECONDS (non-blocking)"`。

> 注意：`split_command()` 用空白切詞，所以 MESSAGE 不能含空白。spec demo 通常用單字，先這樣即可。若要支援空白，可改成把 `argv[1]` 到 `argv[argc-2]` 用空白接回一個 buffer。

### S2.9 刪死碼

**檔案**：`Shell/shell.c` 第 611-614 行，`print_message()` 空函式刪掉（就是那個編譯 warning）。它應該是原本符合 spec 的實作被棄置的殘骸。

### S2.10 驗證

```
setTimeout Hello 5          → 5 秒後印出 now/registered/after + Hello
setTimeout A 6              → 三個一起下，必須依 3→5→6 順序觸發（測排序插入）
setTimeout B 3
setTimeout X 3.5            → 小數秒可用
setTimeout X                → 印 Usage
setTimeout X abc            → 印 SECONDS 格式錯誤
```

下 `setTimeout Hello 10` 之後**立刻**打 `help`，`help` 必須馬上有反應 → 證明非阻塞。

背景那個每 2 秒的 `[core timer] uptime=` 應該持續正常。

**檢查點**：`Kernel/timer_manager.c` 應該從 309 行降到 150-170 行左右。若沒有，代表有東西沒刪乾淨。

---

## Stage 3：Lab3 AE2 中斷線 mask/unmask

spec 原文：

> "Masks the device's interrupt line" upon triggering, enqueues processing tasks, then "unmasks the interrupt line to get the next interrupt at the end of the task."

現況：**完全沒做**。用 `exception.c:11-12` 的 `timer_task_queued` / `uart_task_queued` 兩個布林旗標代替。`daif_unmask_all()` 是把整個 CPU 的 DAIF 全開，跟「關掉某一條裝置中斷線」是兩件不同的事 —— 它不會讓裝置停止產生 IRQ。

實際後果：`io_task_run_all()` 在 IRQ 開啟下跑 handler，此時 timer compare 還沒 re-arm、UART pending bit 也沒清，同一條線會立刻重入 `irq_routing`。旗標擋住了重複 enqueue，但擋不住 IRQ 本身重入，每次重入都白跑一輪。

### S3.1 UART 中斷線開關

**檔案 1**：`Driver/uart.c`

在 `UartRegInfoT`（第 9-28 行）加一個欄位：

```c
unsigned long irq_disable1;   // +2026/08/01 DISABLE_IRQS_1
```

在 `uart_assemble_registers()`（第 92-115 行）第 112 行後面加：

```c
uart_reg_info.irq_disable1 = uart_reg_info.irq_base + 0x1C;
```

BCM2835 ARM 中斷控制器暫存器對照（`irq_base` = `0x3F00B200`）：

| 名稱 | offset | 絕對位址 |
|---|---|---|
| IRQ_PENDING_1 | 0x04 | 0x3F00B204 |
| ENABLE_IRQS_1 | 0x10 | 0x3F00B210 |
| **DISABLE_IRQS_1** | **0x1C** | **0x3F00B21C** |

新增兩個函式：

```c
// +2026/08/01 AE2：mask/unmask AUX(Mini UART) 中斷線
// 注意 ENABLE/DISABLE_IRQS 是 write-1-to-act，寫 0 的位元不受影響，
// 因此這裡「不能」做 read-modify-write，直接寫入目標位元即可。
void uart_irq_disable(void)
{
    mmio_write(uart_reg_info.irq_disable1, (1 << 29));
}

void uart_irq_enable(void)
{
    mmio_write(uart_reg_info.irq_enable1, (1 << 29));
}
```

**檔案 2**：`Driver/uart.h` — 加上這兩個宣告。

> 順帶：`uart_open_ier_reg()` 第 202-204 行對 `irq_enable1` 做了 read-modify-write。因為是 write-1-to-act 暫存器，這樣寫碰巧沒錯但語意不對，可以順手簡化成 `mmio_write(uart_reg_info.irq_enable1, (1 << 29));`。

### S3.2 Core timer 中斷線開關

**檔案 1**：`Driver/time.c`

`core_timer_init()` 第 54 行已經在用 `arm_local_intc_base + 0x40`（CORE0_TIMER_IRQCNTL），寫 `2` 代表 unmask bit 1 (nCNTPNSIRQ)。這個暫存器**是一般讀寫暫存器**（不是 write-1-to-act），所以直接寫值就好：

```c
// +2026/08/01 AE2：mask/unmask core timer 中斷線
void core_timer_irq_disable(void)
{
    extern CtxT dtb_ctx;
    if (dtb_ctx.interrupt_info.have_arm_local_intc_base == false) { return; }
    mmio_write(dtb_ctx.interrupt_info.arm_local_intc_base + 0x40, 0);
}

void core_timer_irq_enable(void)
{
    extern CtxT dtb_ctx;
    if (dtb_ctx.interrupt_info.have_arm_local_intc_base == false) { return; }
    mmio_write(dtb_ctx.interrupt_info.arm_local_intc_base + 0x40, 2);
}
```

**檔案 2**：`Driver/time.h` — 加宣告。

### S3.3 `irq_routing` Phase 1 改成先 mask

**檔案**：`Kernel/exception.c` 第 128-221 行

Timer 分支（第 149-168 行）改成：

```c
if (irq_src & (1 << 1))
{
    // +2026/08/01 AE2：先關中斷線，避免 Phase 2 開 IRQ 後同一來源重入
    core_timer_irq_disable();

    bool queued = io_task_enqueue(IO_TASK_TYPE_TIMER_CALLBACK,
                                  IO_TASK_PRIORITY_HIGH,
                                  deferred_timer_irq_task, 0, NULL, 0);

    if (queued == false)
    {
        // +2026/08/01 enqueue 失敗必須立刻 unmask，否則 timer 永久失效
        core_timer_irq_enable();
    }
}
```

UART 分支（第 173-200 行）比照，用 `uart_irq_disable()` / `uart_irq_enable()`。

**`if (!timer_task_queued)` / `if (!uart_task_queued)` 這兩層判斷整個拿掉** —— 硬體 mask 本身就防止了重複觸發，旗標是多餘的補丁。

### S3.4 在 deferred task 尾端 unmask

**檔案**：`Kernel/exception.c` 第 14-33 行

```c
static void deferred_timer_irq_task(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    timer_interrupt_router();
    shell_notify_async_event();

    // +2026/08/01 AE2：task 結束時才重開中斷線
    core_timer_irq_enable();
}

static void deferred_uart_irq_task(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    uart_interrupt_handler();

    // +2026/08/01 AE2：task 結束時才重開中斷線
    uart_irq_enable();
}
```

刪掉第 11-12 行的 `timer_task_queued` / `uart_task_queued` 宣告，以及函式內清旗標的那兩行。

記得補 `#include "../Driver/time.h"`（`exception.c` 目前沒有 include 它）。

### S3.5 驗證

**必做**：這步最容易做出「打字沒反應」或「時間停住」。分開測：

1. 先只做 timer 的 mask/unmask，跑一次 → 背景 2 秒 uptime 必須持續
2. 再做 UART 的，跑一次 → 打字必須有回顯

若打字沒反應 → 幾乎一定是某條路徑 mask 之後沒 unmask（最常見是 enqueue 失敗分支忘了補）。

若 QEMU CPU 100% 空轉 → pending bit 沒清就重開了中斷線。

---

## Stage 4：Lab3 AE2 priority preemption

spec 原文：

> "Implement priority-based task execution where higher-priority tasks preempt currently running lower-priority tasks before returning to previous interrupt contexts."

現況：**沒做**。`io_task_has_higher_priority_than()`（`io_task_queue.c:275-304`，30 行）**零呼叫點**，`io_task_run_all()` 只是 `while(has_pending) run_once()` 的 FIFO drain。

好消息：那個 dead function 的邏輯是**對的**，你只要把它接上去。

### S4.1 加入「目前執行中優先級」狀態

**檔案**：`Kernel/io_task_queue.c`，在第 12 行後面加：

```c
// +2026/08/01 AE2：目前正在執行的 task 優先級。
// IO_TASK_PRIORITY_COUNT 代表「目前沒有 task 在執行」。
static volatile IoTaskPriority current_running_priority;
```

**在 `io_task_queue_init()`（第 86-117 行）裡初始化**，不要用宣告時初始化：

```c
current_running_priority = IO_TASK_PRIORITY_COUNT;
```

原因：非零初始值會落在 `.data` 段，而 `boot.S` 只清 `.bss`。放進 init 函式最保險。

### S4.2 重寫 `io_task_run_all`

**檔案**：`Kernel/io_task_queue.c` 第 328-335 行

```c
void io_task_run_all(void)
{
    // +2026/08/01 AE2 priority preemption：
    // 只執行「嚴格高於目前執行中優先級」的 task。
    // 巢狀 IRQ 進來時，內層 run_all 只會搶佔比外層更高優先級的工作，
    // 執行完就返回，剩下的留給外層迴圈 → 這就是 spec 要的
    // "preempt ... before returning to previous interrupt contexts"。
    while (io_task_has_higher_priority_than(current_running_priority))
    {
        IoTask task;

        if (io_task_dequeue(&task) == false)
        {
            break;
        }

        unsigned long saved_daif = local_irq_save();
        IoTaskPriority previous = current_running_priority;
        current_running_priority = task.priority;
        local_irq_restore(saved_daif);

        if (task.callback != NULL)
        {
            task.callback(task.argc, task.argv);
        }

        saved_daif = local_irq_save();
        current_running_priority = previous;
        local_irq_restore(saved_daif);
    }
}
```

### S4.3 為什麼現有的 guard 函式剛好正確

`io_task_has_higher_priority_than(p)` 逐項驗算：

| `current_running_priority` | 行為 | 對不對 |
|---|---|---|
| `COUNT`(3) 沒東西在跑 | 不觸發 `<=0` 也不觸發 clamp，掃 p=0,1,2 全部 queue | ✅ 外層能執行任何 task |
| `HIGH`(0) | `(int)0 <= 0` → 直接 return false | ✅ 同級不搶佔，HIGH 執行中不被打斷 |
| `MEDIUM`(1) | 掃 p=0 | ✅ 只有 HIGH 能搶佔 |
| `LOW`(2) | 掃 p=0,1 | ✅ HIGH/MEDIUM 能搶佔 |

**完整搶佔情境（自己在紙上走一遍，這是這個 Stage 的重點）：**

```
1. UART IRQ  → mask UART 線 → enqueue deferred_uart_irq_task (MEDIUM)
2. Phase 2   → daif_unmask_all(); io_task_run_all()
               current=COUNT → has_higher(3)=true → pop MEDIUM
               current=MEDIUM → 開始跑 uart handler
3.   ┌─ 執行到一半，timer IRQ 進來（巢狀）
     │  → el1_irq_entry 再入 → mask timer 線 → enqueue (HIGH)
4.   │  巢狀 Phase 2 → io_task_run_all()
     │    current 仍是 MEDIUM → has_higher(MEDIUM)=true（HIGH queue 非空）
     │    → pop HIGH → current=HIGH → 跑 timer handler → 完成
     │    → current 還原 MEDIUM → has_higher(MEDIUM)=false → 內層返回
5.   └─ 巢狀 eret，回到步驟 2 的 uart handler 繼續執行 → 完成
6. current 還原 COUNT → 外層 while 再檢查 → 空 → 返回
```

高優先級確實搶佔了執行中的低優先級，且執行完會回到原本的中斷上下文。這正是 spec 要的。

關鍵在於**內層 `run_all` 不能把低優先級 task 也一起 drain 掉** —— 否則低優先級工作會被夾在高優先級工作「裡面」執行，語意就錯了。`has_higher_priority_than` 這個 guard 就是在擋這件事。

### S4.4 `io_task_run_once` 怎麼辦

第 309-326 行的 `io_task_run_once()` 繞過了優先級 guard，且**目前只被 `run_all` 呼叫**。改寫 `run_all` 後它就沒人用了。

兩個選擇：直接刪掉（推薦），或留著但加註解說明它不受 preemption 保護。留著會讓 header 的 API 表面繼續誤導人。

順帶：`io_task_enqueue_uart_rx()` / `io_task_enqueue_uart_tx()`（第 215-225 行）也是零呼叫點。同樣建議刪掉，`io_task_queue.h` 的對應宣告一併移除。

### S4.5 驗證

搶佔很難用眼睛看出來。加暫時的除錯輸出：

在 `io_task_run_all` 取到 task 後印一行 `[run p=N depth=M]`（M 用一個 static 巢狀計數器），跑起來後**同時**製造 UART 流量（狂打字）和 timer 事件（`setTimeout A 1` 連下幾個），觀察是否出現 `depth=2` 且內層 `p` 小於外層 `p` 的紀錄。

確認後把除錯輸出拿掉。

---

## Stage 5：修剩餘 bug

### S5.1 `io_task_dequeue` 交出懸空 argv 指標

**檔案**：`Kernel/io_task_queue.c` 第 227-251 行

```c
*out_task = *task;        // argv_storage「內容」有被複製到呼叫端
                          // 但 argv[] 是指標陣列，複製後仍指向 pool node！
free_task_unsafe(task);   // node 立刻歸還 free list，隨時可能被重用覆寫
```

修法：複製後把指標重新指向自己。

```c
*out_task = *task;

// +2026/08/01 argv[] 是指標，struct 複製後仍指向 pool node，
// 必須 rebase 到 out_task 自己的 argv_storage，否則 free 後即為懸空指標。
for (int i = 0; i < MAX_ARGS; i += 1)
{
    out_task->argv[i] = out_task->argv_storage[i];
}

free_task_unsafe(task);
```

> Stage 2 完成後這條路徑其實沒人在用了（`exception.c` 都傳 `argc=0, argv=NULL`）。既然如此，**更好的做法是把 `IoTask` 的 `argv_storage[16][256]` 和 `argv[16]` 整個拿掉**，`.bss` 可以再省 256KB。這是 Stage 2 的紅利，但會動到 `io_task_enqueue` 簽章，建議獨立成一個 commit。

### S5.2 `reboot_lock` 檢查位置錯誤

**檔案**：`Shell/shell.c` 第 191-210 行

`compare_command()` 把 `reboot_lock` 檢查寫在 for 迴圈**裡面**（第 197-201 行），每輪都判斷一次。功能碰巧會動，但語意錯位。

把那段檢查移到迴圈外、函式開頭即可。

> Stage 2 之後 `setTimeout` 不再呼叫 `compare_command()`，所以原本「setTimeout 查表時也會被 reboot_lock 攔截並印訊息」的副作用會自動消失。

### S5.3 `async_uart_send` 的 tx ring race

**檔案**：`Driver/uart.c` 第 242-270 行

`async_uart_send()` 在佇列空時會自己推進 `tx_head` 送出第一個 byte（第 255-260 行），而 `uart_interrupt_handler()` 第 226-231 行也在推進 `tx_head`。Stage 3 之後 UART handler 跑在 IRQ 開啟的 deferred task 裡，兩個 consumer 同時動 `tx_head` 沒有保護。

修法：用 `local_irq_save()` / `local_irq_restore()` 包住 `async_uart_send()` 裡對 `tx_head` / `tx_tail` 的存取。注意第 245-248 行那個「佇列滿了就 busy-wait」的迴圈**必須在臨界區外**，否則會死鎖（關著 IRQ 等 IRQ 來清空佇列）。

需要 `#include "../Kernel/exception.h"`。

### S5.4 timer callback 的 NULL 檢查

Stage 2 的 `timer_fire()` 已經處理了（`callback != NULL` 才呼叫）。如果你 S2.6 有照做就不用額外處理，這裡只是提醒對照舊版 `timer_manager.c:165` 和 `:122` 都是無條件呼叫 —— 而 `shell.c:407` 正好傳 NULL。

---

## Stage 6：結構清理（純可讀性，無規格影響）

前面五個 Stage 做完再動這裡。每項獨立，可分開 commit。

### S6.1 `extern CtxT dtb_ctx;` 收斂

現在散在 8 個函式內部：

```
Driver/time.c:48    Driver/uart.c:94    Kernel/exception.c:130
Shell/shell.c:319, 342, 394, 421, 470
```

作法：在 `Board/fdtb.h` 的 `CtxT` 定義後面加一行 `extern CtxT dtb_ctx;`，然後把 8 處函式內的宣告全刪掉。定義維持在 `Kernel/kernel_main.c:11`。

### S6.2 檔名對齊

`Kernel/time_manager.h` ↔ `Kernel/timer_manager.c` 名字對不上。選一個統一（建議都叫 `timer_manager`），改完更新所有 include。

### S6.3 `uart.h` 刪掉不存在的宣告

`Driver/uart.h` 第 14-21 行宣告了 8 個 blocking 版本函式（`uart_send` / `uart_puts` / `uart_recv` / `uart_recv_uint` / `uart_send_integer` / `uart_send_unsigned_long_integer` / `uart_send_decimal_part` / `uart_send_hex`），`uart.c` 裡**一個都沒實作**。全部刪掉。

### S6.4 `shell.c` 拆檔

636 行、身兼六職。建議拆法：

| 移到 | 內容 | 行數 |
|---|---|---|
| `Board/fdtb.c` 新增 `FdtbDump()` / `FdtbDumpIntc()` | `cmd_fdtb`(465-538) + `cmd_dtb_intc`(416-463) 的輸出邏輯 | ~120 |
| `Shell/shell_debug_cmds.c` 新增 | `cmd_test_el1_brk` / `cmd_test_el1_svc` / `cmd_test_el1_bad_read` / `cmd_test_el0_user_mode` | ~60 |

`shell.c` 只留命令表 + 行編輯 + dispatcher，約 350 行。記得同步更新 `Makefile`（`C_ALL` 是 wildcard，新 `.c` 會自動被抓到，但要確認它有進 `OBJS_FOR_KERNEL` 而不是被 `OBJ_COMMON_BOOT` 的 filter-out 影響）。

### S6.5 include 路徑簡化

91 處 `#include "../Driver/uart.h"` 這種相對路徑。`Makefile` 第 16 行已經有 `INC_DIRS := $(SRC_DIRS)` 產生 `-IBoard -IDriver -IKernel ...`，所以可以直接寫 `#include "uart.h"`。

用 sed 批次改，改完務必 `make clean && make all` 確認。

### S6.6 命名風格統一

三種風格混用：

- PascalCase：`CpioGetFilesHeaderName` / `InitialDtbCtx` / `ReadDTBFile` / `PathEqualsBase` / `SimpleAllocator`
- snake_case：`add_timer` / `io_task_enqueue` / `uart_init_dynamic`

建議統一成 snake_case（kernel 慣例）。**這項最後做**，因為它會產生大量 diff，跟其他修改混在一起會很難 review。

---

## 完成檢查表

規格面：

- [ ] `simple_malloc` 名稱符合 spec 且有真實呼叫點（Lab2 BE3, 10%）
- [ ] `setTimeout MESSAGE SECONDS` 可用，MESSAGE 是字面字串（Lab3 AE1, 20%）
- [ ] 到期輸出含 current time 與 command executed time（Lab3 AE1）
- [ ] 多個 timeout 依到期時間排序觸發（Lab3 AE1）
- [ ] IRQ 觸發時 mask 裝置中斷線，task 結束時 unmask（Lab3 AE2）
- [ ] 高優先級 task 會搶佔執行中的低優先級 task（Lab3 AE2）
- [ ] `spsr_el1` / `elr_el1` 有存（Lab3 AE2）→ **已完成，不用動**

程式碼面：

- [ ] `make all` 零 warning
- [ ] `Kernel/timer_manager.c` 從 309 行降到 170 行以下
- [ ] `io_task_has_higher_priority_than()` 有被呼叫
- [ ] 沒有零呼叫點的公開 API
- [ ] `.bss` 用量顯著下降（`aarch64-linux-gnu-size build/kernel8.elf` 對照）
