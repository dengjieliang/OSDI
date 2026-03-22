# Kernel

## 對應總說明章節

- `0.4 Kernel 主流程（Kernel Core / Interactive Loop）`
- `13. 組合語言啟動程式 (Startup)` 中的 `exception_table.S`、`exception.h`、`exception.c`、`user_mode_entry.S`、`user_mode.h`
- `15. 作業系統核心主程式 (Kernel Main)`

## 檔案定位

此資料夾放的是 kernel image 在進入互動 shell 之前與執行期間最核心的控制邏輯：`kernel_main.c` 負責主初始化流程，`exception_*` 負責 EL1 例外與 IRQ 進入點，`user_mode_*` 負責最小 EL0 切換入口。這一層不是單純 driver，也不是單純工具函式，而是把 boot、DTB、driver、shell 串起來的中樞。

## 內容概述

- `kernel_main.c`：Kernel 啟動主線。
- `exception_table.S`：EL1 向量表與例外入口 wrapper。
- `exception.h` / `exception.c`：C 端 exception / IRQ handler。
- `user_mode_entry.S` / `user_mode.h`：最小 EL0 切換入口。

## 目前提供的功能

## `kernel_main.c`

### 檔案定位

Kernel 的 C 語言入口點。負責解析由 Bootloader 傳入的 DTB、初始化與 DTB 綁定的硬體設定，並進入互動式 shell。

### 內容概述

- `InitialDtbCtx(&dtb_ctx)`：初始化 DTB context
- `ReadDTBFile(dtb_addr, DtbCollectHandler, &dtb_ctx)`：解析 bootloader 傳入的 DTB
- `common_init_from_dtb(&dtb_ctx)`：同步共用 MMIO base
- `uart_init_dynamic()`、`uart_aux_mu_cntl_reg()`、`uart_open_ier_reg()`：重新建立 kernel 階段 UART 與 IRQ 路徑
- `set_exception_vector_table()`：安裝 EL1 向量表
- `msr daifclr, #0xf`：解除 IRQ mask
- `async_uart_puts("Welcome to OSDI")`
- `shell_main()`：進入互動模式

### `void kernel_main(void* dtb_addr)`

1. 接收由 `x0` 傳入的 `dtb_addr`
2. 初始化 `dtb_ctx`
3. 解析 DTB
4. 若解析失敗：
  - `early_uart_init()`
  - `early_uart_puts("Failed to read DTB file.")`
  - 直接返回
5. 初始化 DTB 綁定的共用硬體資訊
6. 重新初始化 UART 與中斷路徑
7. 安裝例外向量表與開啟 IRQ
8. 印出 `Welcome to OSDI`
9. 進入 `shell_main()`

## `exception_table.S`

### 檔案定位

提供 AArch64 EL1 例外向量表與進入點（exception entry stubs），負責建立 `exception_vector_table`、保存 / 還原通用暫存器、轉呼叫 C handler，並提供 `set_exception_vector_table()`。

### 內容概述

- 向量表配置
  - `.balign 0x800`
  - 每個 slot `.align 7`
  - 已特化：
    - Current EL using SP_ELx 的 IRQ -> `el1_irq_entry`
    - Lower EL AArch64 的 Sync -> `el0_sync_entry`
    - Lower EL AArch64 的 IRQ -> `el0_irq_entry`
  - 其餘預設導向 `default_handler`
- 暫存器保存巨集
  - `SAVE_ALL`
  - `RESTORE_ALL`
- C handler 橋接
  - `default_handler_dump_c`
  - `el0_sync_handler_c`
  - `el0_irq_handler_c`
  - `el1_irq_handler_c`

### 子程序說明

#### `default_handler`

- 關閉中斷
- `SAVE_ALL`
- 讀取 `ESR_EL1`、`ELR_EL1`、`SPSR_EL1`
- 呼叫 `default_handler_dump_c`
- 進入 `wfe` 無限迴圈

#### `el0_sync_entry`

- 保存完整 context
- 擷取 syndrome / return 狀態暫存器
- 將 context 指標交給 `el0_sync_handler_c`
- 還原後 `eret`

#### `el0_irq_entry`

- 保存完整 context
- 呼叫 `el0_irq_handler_c`
- 還原後 `eret`

#### `el1_irq_entry`

- 保存完整 context
- 呼叫 `el1_irq_handler_c`
- 還原後 `eret`

#### `set_exception_vector_table`

- 取得 `exception_vector_table` 位址
- `msr vbar_el1, x0`
- `isb`

## `exception.h`

### 檔案定位

例外處理模組的對外介面宣告，讓 `kernel_main.c` 可安裝向量表，並讓 `exception_table.S` 可呼叫 C handler。

### 目前提供的功能

- `void set_exception_vector_table(void)`
- `void default_handler_dump_c(unsigned long esr, unsigned long elr, unsigned long spsr)`
- `void el0_sync_handler_c(unsigned long esr, unsigned long elr, unsigned long spsr, unsigned long *ctx)`
- `void el0_irq_handler_c(unsigned long elr, unsigned long spsr, unsigned long *ctx)`

## `exception.c`

### 檔案定位

`exception_table.S` 的 C 端橋接實作，負責輸出例外診斷、處理部分同步例外，以及依 DTB 解析結果分派 timer / UART IRQ。

### 內容概述

- 共用 helper
  - `mask_all_exceptions()`
  - `read_far_el1()`
- handler 行為
  - `default_handler_dump_c(...)`
  - `el0_sync_handler_c(...)`
  - `irq_routing(...)`
  - `el0_irq_handler_c(...)`
  - `el1_irq_handler_c(...)`

### 目前提供的功能（實作）

#### `default_handler_dump_c(...)`

- 輸出 `ESR`、`ELR`、`SPSR`、`FAR`
- 之後無限迴圈停住

#### `el0_sync_handler_c(...)`

- 輸出 syndrome 資訊
- 若 `EC == 0x15`（AArch64 SVC）
  - 印出 `imm16`
  - 返回
- 其餘同步例外：輸出 `FAR_EL1` 後停住

#### `irq_routing(...)`

- 先 mask 例外
- 依 DTB 解析出的 base 分派 IRQ 來源：
  - local intc `+0x60` bit1 -> core timer 路徑
    - 重設下一次 2 秒
    - 輸出 `Core Timer Interrupt!`
  - local intc bit8 + armctrl `+0x04` bit29 -> AUX UART IRQ
    - 呼叫 `uart_interrupt_handler()`

#### `el0_irq_handler_c(...)` / `el1_irq_handler_c(...)`

- 皆委派到 `irq_routing(...)`

## `user_mode_entry.S`

### 檔案定位

提供從 EL1 切換到 EL0 的最小入口實作 `enter_el0`。它不建立完整 user context，而是接收 user entry 與 EL0 stack top，設定必要系統暫存器後以 `eret` 落入 EL0 執行。

### 目前提供的功能（實作）

- 參數約定
  - `x0 = user_start_addr`
  - `x1 = user_stack_top`
- 進入流程
  1. `msr spsr_el1, 0x0`
  2. `msr elr_el1, x0`
  3. `msr sp_el0, x1`
  4. `isb`
  5. `eret`

## `user_mode.h`

### 檔案定位

宣告 user mode 切換入口函式，讓 C 程式可呼叫 `enter_el0()`。

### 目前提供的功能

- `extern void enter_el0(unsigned long user_start_addr, unsigned long user_stack_top)`

## 現況注意

- `kernel_main.c` 是所有高階功能匯流的地方，通常需要搭配 Board、Driver、Shell 一起看。
- `default_handler_dump_c` 與多數同步例外目前偏除錯導向，不是可恢復流程。
- user mode 路徑目前只有最小 `eret` 切換，不代表已建立完整使用者態執行環境。

## 跨文檔導讀

- 若要理解 DTB context 是如何建立的，請先看 [Board/Board.md](../Board/Board.md)。
- 若要理解 UART、timer IRQ 與 AUX IRQ handler 對接到哪些 driver，請看 [Driver/Driver.md](../Driver/Driver.md)。
- 若要理解 shell 是如何成為 kernel 初始化完成後的主互動迴圈，請看 [Shell/Shell.md](../Shell/Shell.md)。
- 若要理解 Bootloader 如何把 DTB 與控制權交給 kernel，請先看 [Boot/Boot.md](../Boot/Boot.md)。
- 若要理解 kernel linker 位址與 debug symbol 載入，請看 [Configuration.md](../Configuration.md) 中的 Linker Scripts 與 VS Code Configuration 章節。

## 閱讀建議

- 想看「Kernel 啟動主線」：從本檔 `kernel_main.c` 開始，再追 [Board/Board.md](../Board/Board.md) 與 [Driver/Driver.md](../Driver/Driver.md)。
- 想看「IRQ 為何能同時處理 timer 與 UART」：看本檔的 `exception`，再看 [Driver/Driver.md](../Driver/Driver.md)。
- 想看「user mode 測試會走到哪裡」：先看本檔 `user_mode_*`，再看 [Shell/Shell.md](../Shell/Shell.md)。
