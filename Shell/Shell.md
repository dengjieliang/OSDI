# Shell

## 對應總說明章節

- `11. 簡易命令列介面 (Shell)`
- `15. 作業系統核心主程式 (Kernel Main)` 中進入 `shell_main()` 的部分

## 檔案定位

此資料夾提供 kernel 啟動完成後的互動式命令列。它是目前系統對外最直接的操作入口，負責接收 UART 輸入、切分命令、派發到對應 handler，並把 board info、timer、reboot、CPIO 檔案系統與 user mode 測試等功能暴露給使用者。

## 內容概述

- `shell.h`：宣告 shell 主要介面與命令資料結構。
- `shell.c`：實作輸入迴圈、命令切分、命令派發與所有內建命令。

## 目前提供的功能

## `shell.h`

### 檔案定位

定義簡易 shell 的命令資料結構（command table）與對外 API。

### 目前提供的功能

- 命令函式型別
  - `typedef void(*CommandFunc)(int argc, char* argv[])`
- 命令結構
  - `Command_t { name, description, func }`
- 對外 API
  - `shell_main()`
  - `shell_input_line()`
  - `execute_command(argc, argv)`

## `shell.c`

### 檔案定位

提供互動式 shell（async UART I/O）：

- 顯示 prompt（含時間）
- 接收一行輸入並進行字串切割（argc/argv）
- 依命令表執行對應命令

### 內容概述

1. `shell_main()` 顯示開機訊息後進入無限迴圈
2. 每輪先印出 prompt：`[` + `get_timetick()` + `]` + `:shell$ `
3. `shell_input_line()` 讀入一行（支援 Enter / Backspace，寫入 `input_buffer[128]`）
4. `split_command()` 以空白做 in-place tokenize
5. `execute_command()` 依 `commands[]` 比對並執行對應 handler

### 目前提供的功能（實作）

#### 1) 命令表 `commands[]`

目前包含：

- `hello`
- `help`
- `info`
- `time`
- `reboot`
- `cancelReboot`
- `ls`
- `cat`
- `test_brk`
- `test_svc`
- `test_bad_read`
- `test_user_mode`
- `dtb_intc`
- `fdtb`

並以 `{NULL, NULL}` 作為 sentinel 結尾。

#### 2) `void shell_main()`

- 先輸出 `\n\n=== RPi3 OS Booting... ===\n`
- 進入無限迴圈：
  1. `shell_input_line()`
  2. `split_command(&argc, argv)`
  3. 若 `argc > 0`，呼叫 `execute_command(argc, argv)`

#### 3) `char* shell_input_line()`

- prompt 輸出格式：
  - `[` + `get_timetick()` + `]` + `:shell$ `
- Enter（`\r` / `\n`）
  - 輸出 `\n`
  - 在 `input_buffer` 結尾補 `\0`
  - 回傳 `input_buffer`
- Backspace（`\b` / 127）
  - `buffer_index--`
  - 回顯 `\b \b`
- 其他字元
  - `async_uart_send(c)` 回顯
  - 若 buffer 未滿則寫入 `input_buffer`

#### 4) `static void split_command(int* argc, char* argv[])`

- 以空白做原地切割
- 遇到 token 起點時把指標寫入 `argv[*argc]`
- 若超過 `MAX_ARGS`
  - 輸出 `Warning: Too many arguments, ignoring the rest.`

#### 5) `void execute_command(int argc, char* argv[])`

- 若 `reboot_lock == true` 且命令不是 `cancelReboot`
  - 輸出 `Rebooting... Please input 'Cancel Reboot' to abort.`
  - 直接返回
- 否則遍歷 `commands[]`
  - 找到匹配命令就呼叫對應 handler
- 若找不到
  - 輸出 `Command not found: ...`

### 內建命令（就現況行為）

#### `cmd_hello`

- 輸出 `Hello, World!\n`

#### `cmd_help`

- 先輸出 `Available commands:\n`
- 再逐條列出命令名稱與描述

#### `cmd_board_info`（`info`）

- 送 board revision request
- 若狀態成功，輸出 `Board Revision: ` 與 revision 值
- 送 memory request
- 若狀態成功，輸出 `Memory Size is: ` 與 memory size

#### `cmd_get_timer`（`time`）

- 呼叫 `get_timetick()`
- 補一個換行

#### `cmd_reboot`（`reboot`）

- 輸出 `Rebooting in T-minus 2 seconds...`
- 呼叫 `reset(150000)`
- 設定 `reboot_lock = true`

#### `cmd_cancel_reboot`（`cancelReboot`）

- 呼叫 `cancel_reset()`
- 設定 `reboot_lock = false`

#### `cmd_get_file_header`（`ls`）

- 以 `dtb_ctx.initrd_start` 作為 CPIO 起點
- 呼叫 `CpioGetFilesHeaderName(header)`
- 若 `file_count <= 0`，輸出 `Not Find Any File.`

#### `cmd_get_file_context`（`cat`）

- `argc < 2` 時輸出 `Please Enter FileName`
- `argc != 2` 時輸出 `False Argument`
- 否則呼叫 `CpioGetFileContext(header, argv[1])`
- 找不到時輸出 `Cannot Find File`

#### 測試 / 診斷命令

- `test_brk`
  - 在 EL1 觸發 `brk #0`
- `test_svc`
  - 在 EL1 觸發 `svc #0`
- `test_bad_read`
  - 嘗試讀取無效位址
- `test_user_mode`
  - 從 initramfs 讀取指定檔案起始位址，切換到 EL0 執行
- `dtb_intc`
  - 輸出 DTB 解析到的 interrupt controller base 與 diagnostics
- `fdtb`
  - 輸出 `dtb_ctx` 中的 initrd / UART / AUX / GPIO / interrupt controller 資訊

## 現況注意

- Shell 已不是最早期的 blocking UART 版本；目前主路徑依賴 `async_uart_*` 與 IRQ。
- `reboot_lock` 會在 reboot 倒數期間限制大部分命令，只允許取消重開機流程繼續操作。
- `input_buffer` 固定 128 bytes，滿了之後仍會回顯輸入，但不再寫入 buffer。
- `shell_main()` 不使用 `shell_input_line()` 的回傳值，但因為 tokenization 直接處理全域 `input_buffer`，流程仍成立。

## 跨文件導讀

- 若要理解 shell 的收發為何是 IRQ + ring buffer，請看 [Driver/Driver.md](../Driver/Driver.md)。
- 若要理解 `ls` / `cat` 的 initramfs 起點與 CPIO parser，請看 [Board/Board.md](../Board/Board.md) 與 [FileSystem/FileSystem.md](../FileSystem/FileSystem.md)。
- 若要理解 shell 為何在 kernel 初始化最後才進入，請看 [Kernel/Kernel.md](../Kernel/Kernel.md)。
- 若要理解 user mode 測試命令的切入點，請看 [Kernel/Kernel.md](../Kernel/Kernel.md)。

## 閱讀建議

- 想看「使用者在畫面輸入命令後發生什麼事」：先看本檔，再依命令種類追 [Driver/Driver.md](../Driver/Driver.md) 或 [FileSystem/FileSystem.md](../FileSystem/FileSystem.md)。
- 想看「shell 為何能讀取 initramfs 中的檔案」：本檔搭配 [Board/Board.md](../Board/Board.md) 與 [FileSystem/FileSystem.md](../FileSystem/FileSystem.md)。
