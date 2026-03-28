# Driver

## 對應總說明章節

- `0.5 核心硬體依賴（Drivers & Peripherals）`
- `0.7 Early Boot 通訊與位址基礎（early_uart / early_common）`
- `2. Mailbox 介面 (Mailbox Interface)`
- `3. 電源管理 (Power Manager)`
- `10. 系統計時器 (System Timer)`
- `12. Mini UART 驅動程式 (Mini UART Driver)`

## 檔案定位

此資料夾放的是與周邊硬體直接互動的 driver：Bootloader 階段的 early mini UART、Kernel 階段的 async mini UART、Mailbox、Power Manager 與 Generic Timer。它們共同構成目前系統能夠輸出文字、接受 shell 輸入、查詢板子資訊、設定重開機與處理 timer IRQ 的硬體基礎。

## 內容概述

- `early_uart.*`：Bootloader 階段的 polling UART。
- `uart.*`：Kernel 階段的 mini UART、IRQ 與 ring buffer async I/O。
- `mailbox.*`：與 GPU / firmware 溝通，取得 board 與 memory 資訊。
- `power_manager.*`：延遲重開機與取消重開機。
- `time.*`：Generic Timer 與 core timer IRQ helper。

## 目前提供的功能

## `early_uart.h` / `early_uart.c`

### 檔案定位

提供 Bootloader early 階段的 polling UART I/O。這條路徑依賴固定 `MMIO_BASE`，不需要先完成 DTB 解析。

### 目前提供的功能

- `early_uart_init()`
- `early_uart_send()`
- `early_uart_puts()`
- `early_uart_recv()`
- `early_uart_recv_uint()`
- `early_uart_send_integer()`
- `early_uart_send_unsigned_long_integer()`
- `early_uart_send_decimal_part()`
- `early_uart_send_hex()`
- `early_uart_delay_cycles()`

### `early_uart_init()` 目前流程

1. 啟用 mini UART
2. 關閉 TX/RX
3. 關閉中斷
4. 關閉 flow control
5. 清空 TX/RX FIFO
6. 設定資料格式為 8-bit
7. 設定 baud rate 為 115200
8. 設定 GPPUD / GPPUDCLK0 序列
9. 把 GPIO14/15 設為 ALT5

### 現況注意

- 這條 UART 路徑服務 Bootloader 與 DTB 解析失敗時的 early error output。
- 與 `uart.c` 相比，這裡沒有 IRQ、ring buffer 或 DTB dynamic base。

## `mailbox.h`

### 檔案定位

定義 Mailbox property channel 相關暫存器位址與 API。

### 目前提供的功能

- Mailbox base 與暫存器位址巨集
  - `MBOX_BASE_OFFSET`
  - `MBOX_READ`
  - `MBOX_PEEK`
  - `MBOX_SENDER`
  - `MBOX_STATUS`
  - `MBOX_CONFIG`
  - `MBOX_WRITE`
- Property channel 常數
  - `MBOX_CH_PROP`
- 對外函式宣告
  - `mailbox_call(unsigned int channel)`
  - `prepare_board_revision_request()`
  - `get_board_status()`
  - `get_board_revision()`
  - `prepare_memory_request()`
  - `get_memory_status()`
  - `get_memory_size()`

## `mailbox.c`

### 檔案定位

實作 Mailbox property call 的送出 / 等待流程，並提供 Board Revision 與 Memory Info 兩組 request / response helper。

### 目前提供的功能（實作細節）

#### 內部資料結構與緩衝區

- `mbox_board_recv_t`
- `mbox_memory_t`
- `_Static_assert` 檢查 struct size
- `unsigned int mailbox_buffer[8] __attribute__((aligned(16)))`

#### `mailbox_call(unsigned int channel)`

1. 等待 Mailbox 可寫
2. 將 `mailbox_buffer` 位址與 channel 組合後寫入 `MBOX_WRITE`
3. 等待 Mailbox 可讀
4. 持續輪詢直到回傳值低 4 bits 的 channel 符合

#### Board revision request / response

- `prepare_board_revision_request()`
- `get_board_status()`
- `get_board_revision()`

#### Memory request / response

- `prepare_memory_request()`
- `get_memory_status()`
- `get_memory_size()`

### 現況注意

- 等待回覆時目前只檢查 channel，未額外驗證 buffer 位址與 property 回覆碼語意。
- `mailbox.c` 目前主要只包了 board revision 與 memory size 兩種 request。

## `power_manager.h` / `power_manager.c`

### 檔案定位

宣告並實作 watchdog / reset controller 相關操作，用來觸發或取消重開機。

### 目前提供的功能

- 暫存器與常數
  - `PM_PASSWORD`
  - `PM_RSTC`
  - `PM_WDOG`
  - `PM_RSTC_FULL_RESET`
  - `TIMETICK_MASK`
- API
  - `void reset(int tick)`
  - `void cancel_reset(void)`

### `reset(int tick)`

1. 對 `PM_RSTC` 寫入 `PM_PASSWORD | PM_RSTC_FULL_RESET`
2. 對 `tick` 套用 `TIMETICK_MASK`
3. 對 `PM_WDOG` 寫入 `PM_PASSWORD | tick`

### `cancel_reset()`

- 對 `PM_RSTC` 與 `PM_WDOG` 分別寫入 `PM_PASSWORD | 0`

## `time.h`

### 檔案定位

宣告 AArch64 Generic Timer 相關介面，包含目前時間輸出、core timer compare 設定與啟用 helper。

### 目前提供的功能

- `unsigned long long get_current_tick()`
- `double get_current_second()`
- `void get_current_second_string()`
- `unsigned long long tansfer_seconds_to_ticks(double seconds)`
- `void tansfer_ticks_to_seconds(unsigned long long ticks, unsigned long* second_integer_part, unsigned int* second_decimal_part_4digit)`
- `void set_core_timer_interrupt_tick(unsigned long long timer_count)`
- `void core_timer_init()`

## `time.c`

### 檔案定位

透過 `cntpct_el0`、`cntfrq_el0`、`cntp_cval_el0` 與 `cntp_ctl_el0` 實作 Generic Timer 讀取與 compare 設定，並根據 DTB 中的 ARM local interrupt controller base 開啟 timer IRQ。

### 目前提供的功能（實作）

#### 1) `static unsigned long long get_system_timer_count()`

- 以 `mrs %0, cntpct_el0` 讀取 system counter

#### 2) `static unsigned long long get_system_timer_frequency()`

- 以 `mrs %0, cntfrq_el0` 讀取計數器頻率

#### 3) `void core_timer_init()`

- 先呼叫 `set_core_timer_interrupt_tick(~0ULL)`，避免 enable 後立即命中 compare
- 設定 `cntp_ctl_el0`
  - bit0 = 1（enable）
  - bit1 = 0（unmask）
- 檢查 `dtb_ctx.interrupt_info.have_arm_local_intc_base`
- 若找到 base，對 `arm_local_intc_base + 0x40` 寫入 `2`（unmask local timer interrupt）

#### 5) `unsigned long long get_current_tick()`

- 回傳目前 `cntpct_el0` 的 raw tick 值

#### 6) `double get_current_second()`

- 讀取 `timer_count` 與 `timer_freq`
- 以 `(double)timer_count / (double)timer_freq` 換算目前秒數

#### 7) `void get_current_second_string()`

- 呼叫 `get_current_second()`
- 計算整數秒與 4 位小數
- 以 `async_uart_send_integer()`、`async_uart_send('.')`、`async_uart_send_decimal_part()` 輸出

#### 8) `unsigned long long tansfer_seconds_to_ticks(double seconds)`

- 讀取 `cntfrq_el0`
- 以 `seconds * (double)timer_freq` 換算對應 tick 數

#### 9) `void tansfer_ticks_to_seconds(...)`

- 讀取 `cntfrq_el0`
- 輸出整數秒：`ticks / timer_freq`
- 輸出小數四位：`(ticks % timer_freq) * 10000 / timer_freq`

#### 10) `void set_core_timer_interrupt_tick(unsigned long long timer_count)`

- 以 `msr cntp_cval_el0, %0` 設定 compare value（絕對 trigger tick）

### 現況注意

- `get_current_second_string()` 本身不輸出換行。
- `get_current_second_string()` 會把秒數拆成整數與小數部分，再格式化成 4 位小數輸出。
- `tansfer_seconds_to_ticks()` 函式名稱目前保留原始拼字，對外 API 也是這個名稱。
- `core_timer_init()` 只負責啟用 timer 與 unmask local timer interrupt；真正的 timer re-arm 由 timer manager / IRQ 路徑控制。
- IRQ re-arm 與實際 routing 在 [Kernel/Kernel.md](../Kernel/Kernel.md) 的 `exception.c` 內處理。

## `uart.h`

### 檔案定位

宣告 Kernel 階段 mini UART 的初始化、IRQ 控制與一組 async UART 介面。

### 目前提供的功能（宣告）

- 初始化 / IRQ 控制
  - `uart_init()`
  - `uart_init_dynamic()`
  - `uart_aux_mu_cntl_reg()`
  - `uart_open_ier_reg()`
  - `uart_interrupt_handler()`
- blocking UART I/O 宣告
  - `uart_send()`
  - `uart_puts()`
  - `uart_recv()`
  - `uart_recv_uint()`
  - `uart_send_integer()`
  - `uart_send_unsigned_long_integer()`
  - `uart_send_decimal_part()`
  - `uart_send_hex()`
- async UART I/O 宣告
  - `async_uart_send()`
  - `async_uart_puts()`
  - `async_uart_recv()`
  - `async_uart_recv_uint()`
  - `async_uart_send_integer()`
  - `async_uart_send_unsigned_long_integer()`
  - `async_uart_send_decimal_part()`
  - `async_uart_send_hex()`
- 延遲
  - `delay_cycles()`

### 現況注意

- `uart.h` 目前仍保留部分舊的 blocking `uart_*` 宣告，但 kernel 與 shell 實際主路徑已改為 `async_uart_*`。

## `uart.c`

### 檔案定位

實作 Raspberry Pi 3 的 mini UART 初始化、IRQ 啟用與 async ring-buffer I/O，並負責設定 GPIO14/15 為 mini UART 腳位。

### 目前提供的功能（實作）

#### 1) mini UART 暫存器與 bit mask 定義

- 以 `UartRegInfoT` 保存 `aux_base`、`uart_base`、`irq_base`
- 定義 AUX / MU 初始化與狀態輪詢所需 bit mask
- 定義 `UART_RX_PUMP_BUDGET 16`

#### 2) `static void uart_assemble_registers(void)`

- 根據 `dtb_ctx` 組裝目前 UART driver 實際使用的 MMIO 位址

#### 3) `void uart_init()`

1. 啟用 mini UART
2. 關閉 TX/RX
3. 關閉中斷
4. 關閉 flow control
5. 清 FIFO
6. 設定 8-bit data
7. 設定 baud rate 115200
8. 設定 GPPUD / GPPUDCLK0
9. 設定 GPIO14/15 為 ALT5

#### 4) `void uart_init_dynamic()`

- 先呼叫 `uart_assemble_registers()`
- 再呼叫 `uart_init()`

#### 5) `void uart_aux_mu_cntl_reg()`

- 開啟 TX/RX

#### 6) `void uart_open_ier_reg()`

- 開啟 UART RX interrupt
- 在 interrupt controller 啟用 AUX IRQ

#### 7) `static void uart_rx_pump(unsigned int budget)`

- 將硬體 RX FIFO 內資料搬到 software ring buffer
- 每次 IRQ 最多搬運 `budget` bytes
- 若 buffer 已滿，會前移 `rx_head` 保留最新資料

#### 8) `void uart_interrupt_handler()`

- 先讀取 `IIR` 快照
- RX 路徑：視 `rx_iir_hit` / `LSR` 狀況執行 `uart_rx_pump(UART_RX_PUMP_BUDGET)`
- TX 路徑：每次 IRQ 只送一個 byte；queue 清空則關閉 TX IRQ

#### 9) Async I/O 系列（`async_uart_*`）

- `async_uart_send(char c)`
  - 先寫入 `tx_buffer`
  - 若 queue 原本為空且 TX 可寫，先直接送第一個 byte
  - 若仍有資料則開啟 TX IRQ 讓背景排空
- `async_uart_puts(const char *s)`
  - 逐字呼叫 `async_uart_send()`，並將 `\n` 轉成 `\r\n`
- `async_uart_recv()`
  - 從 `rx_buffer` 取字元，空時 busy wait
- `async_uart_recv_uint()`
  - 以 async recv 組 4-byte little-endian
- 整數 / 十六進位輸出 helper
  - `async_uart_send_integer()`
  - `async_uart_send_unsigned_long_integer()`
  - `async_uart_send_decimal_part()`
  - `async_uart_send_hex()`

#### 10) `void delay_cycles(unsigned int time)`

- 以 `nop` busy-loop 延遲指定迭代數

## 現況注意

- `early_uart` 與 `uart` 是兩條不同階段的 UART 路徑：
  - `early_uart` 依賴固定 MMIO base，服務 Bootloader
  - `uart` 依賴 DTB 解析結果，服務 Kernel 與 Shell
- `uart_interrupt_handler()` 採用「RX budget + TX 一次一個 byte」的 fairness 策略。
- `mailbox.c` 目前不是通用 property framework，而是小型 request/response 包裝。

## 跨文檔導讀

- 若要理解 early path 的固定 MMIO 與 `KERNEL_LOAD_ADDRESS`，請先看 [Board/Board.md](../Board/Board.md)。
- 若要理解 DTB 解析後為何 `uart_init_dynamic()` 能取得正確位址，請看 [Board/Board.md](../Board/Board.md) 與 [Kernel/Kernel.md](../Kernel/Kernel.md)。
- 若要理解 shell 為何能呼叫 `info`、`reboot`、`time` 等命令，請看 [Shell/Shell.md](../Shell/Shell.md)。
- 若要理解 Bootloader 與 Host tool 的 early UART 握手路徑，請看 [Boot/Boot.md](../Boot/Boot.md) 與 [Tools/Tools.md](../Tools/Tools.md)。

## 閱讀建議

- 想看「Bootloader UART」：先看 `early_uart`，再看 [Boot/Boot.md](../Boot/Boot.md)。
- 想看「Kernel UART + IRQ」：先看 `uart`，再看 [Kernel/Kernel.md](../Kernel/Kernel.md) 與 [Shell/Shell.md](../Shell/Shell.md)。
- 想看「板子資訊、重開機、timer 命令如何工作」：本檔搭配 [Shell/Shell.md](../Shell/Shell.md) 一起看。
