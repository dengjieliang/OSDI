# Boot

## 對應總說明章節

- `0.3 開機交棒流程（Boot Chain）`
- `0.7 Early Boot 通訊與位址基礎（early_uart / early_common）`
- `13. 組合語言啟動程式 (Startup)` 中的 `boot.S`
- `14. 核心載入器邏輯 (Kernel Loader Logic)`

## 檔案定位

此資料夾包含 Bootloader 階段最核心的兩個檔案：AArch64 啟動組語入口，以及透過 early UART 與 Host 端握手、載入 kernel image 的 C 主流程。這一層的責任是把 QEMU 交進來的執行環境整理好，接著把真正的 kernel 讀進 `0x80000`，最後把 DTB 指標一併交棒給 kernel。

## 內容概述

- `boot.S`：負責 startup、relocation、`bss` 清零、stack 初始化與 C 入口跳轉。
- `bootloader_main.c`：負責 early UART 握手、接收 kernel size、載入 image、handoff 給 kernel。

## 目前提供的功能

## `boot.S`

### 檔案定位

此檔案提供 Bootloader 與 Kernel 共用的 ARM64 啟動流程（startup code），負責：

- 接收 QEMU 透過 `x0` 傳入的 DTB 位址並保留
- relocation：必要時將程式搬移到 linker 指定位址
- 初始化 stack pointer
- 清空 `.bss`
- 跳轉到 C 語言入口 `kernel_main(void *dtb_addr)`
- 進入 idle loop 避免返回

### 內容概述

- DTB 指標保存與轉交
  - 進入 `_start` 時 `x0` 帶 DTB 位址；先存到 `x19`，在進入 `kernel_main()` 前再放回 `x0`
- Relocation
  - 比對 runtime `_start` 與 linker `_start`
  - 若不同，將 `[runtime _start .. __bss_end)` 複製到 linker 指定位址，最後 `br` 回 `_start`
- Stack / BSS 初始化
  - 設定 `sp = _stack_top`
  - 呼叫 `clear_bss(__bss_start, __bss_end)`
- 進入 C 與 idle
  - `bl kernel_main`
  - 若返回，進入 `idle_loop`

### 入口流程（`_start`）

1. 保留 DTB 位址到 `x19`
2. 呼叫 `relocate_kernel`
3. 設定 `sp = _stack_top`
4. 清空 `.bss`
5. 把 DTB 放回 `x0`
6. `bl kernel_main`
7. `b idle_loop`

### 子程序說明

#### `relocate_kernel`

- 比對 runtime `_start` 與 linker `_start`
- 若相同，直接返回
- 若不同，以 8-byte 為單位搬移程式與資料到 linker 指定位址
- 搬完後跳回 linker `_start`

#### `clear_bss`

- 參數：`x0 = start`, `x1 = end`
- 以 `str xzr, [x2], #8` 清零 `__bss_start` 到 `__bss_end`

#### `idle_loop`

- 以 `wfe` 無限等待，避免流程落入未知區域

## `bootloader_main.c`

### 檔案定位

Bootloader 的 C 語言主流程。負責初始化 early mini UART、與 Host 端握手並接收 Kernel Image，最後將 DTB 指標傳遞給 Kernel 並跳轉執行。

### 內容概述

- `kernel_main(void *dtb)`
  - 作為 bootloader 的 C 入口，只轉呼叫 `bootloader_main(dtb)`
- early UART handshake 與載入協定
  - `early_uart_init()`
  - 輸出 `OSDI: Ready`
  - `early_uart_recv_uint()` 接收 4-byte kernel size
  - `early_uart_send_hex()` 回送 size 供 Host 確認
- Kernel image 寫入
  - 以 `KERNEL_LOAD_ADDRESS` 為目的位址逐 byte 接收並寫入
- 跳轉與 DTB 轉交
  - 以 `kernel_entry_t` 將 `0x80000` 視為 kernel entry
  - 透過 `entry(dtb)` 交棒給 kernel

### 目前提供的功能（實作）

#### `void kernel_main(void *dtb)`

- 作為 C 語言入口點，由 `boot.S` 呼叫
- 直接轉呼叫 `bootloader_main(dtb)`

#### `static void bootloader_main(void *dtb)`

1. 呼叫 `early_uart_init()`
2. 輸出 `OSDI: Ready` 與等待訊息
3. 以 `early_uart_recv_uint()` 接收 kernel size
4. 以 `early_uart_send_hex()` 回送確認值
5. 逐 byte 將 kernel image 寫入 `KERNEL_LOAD_ADDRESS`
6. 以 `entry(dtb)` 跳到 kernel
7. 以 `__builtin_unreachable()` 告訴編譯器此處不應返回

## 現況注意

- `boot.S` 與 kernel image 共用同一份 startup code，因此 `kernel_main` 這個名稱同時出現在 bootloader 與 kernel 兩條 build 路徑中。
- Bootloader 階段 UART 是 polling 模式；IRQ + ring buffer UART 是進入 kernel 後才切換的路徑。
- `boot.S` 中 relocation 與 `clear_bss` 都以 8-byte 為單位操作，隱含 linker 會提供合適的對齊。

## 跨文檔導讀

- early UART 初始化、收發與固定 MMIO 定義請看 [Driver/Driver.md](../Driver/Driver.md) 與 [Board/Board.md](../Board/Board.md)。
- Bootloader 使用的 linker script 請看 [Configuration.md](../Configuration.md) 中的 Linker Scripts 章節。
- Host 端如何配合這個載入協定送出 `build/kernel8.img`，請看 [Tools/Tools.md](../Tools/Tools.md)。
- Bootloader 跳到 Kernel 之後的後續流程，請看 [Kernel/Kernel.md](../Kernel/Kernel.md)。

## 閱讀建議

- 想看「Bootloader 如何把 kernel 載進記憶體」：先看本檔，再看 [Tools/Tools.md](../Tools/Tools.md)。
- 想看「為何 Boot 與 Kernel 都用 `kernel_main` 作為入口」：先看本檔，再看 [Kernel/Kernel.md](../Kernel/Kernel.md)。
- 想看「為何 Bootloader 連到 `0x60000`，Kernel 連到 `0x80000`」：本檔搭配 [Configuration.md](../Configuration.md) 中的 Linker Scripts 章節一起看。
