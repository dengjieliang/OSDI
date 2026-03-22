# Tools

## 對應總說明章節

- `0.6 Host 端工具（Host Tools）`
- `16. Python 傳輸腳本 (Python Serial Script)`

## 檔案定位

此資料夾放的是 Host 端工具。以目前專案來說，核心工具是 `send_kernel.py`，它不是 target 上執行的 kernel code，而是在開發機上配合 QEMU serial server 送出 kernel image，並在載入完成後切成互動終端機。

## 內容概述

- `send_kernel.py`：負責連線 QEMU serial server、等待 Bootloader ready、傳送 `build/kernel8.img`，再切入互動模式。

## 目前提供的功能

## `send_kernel.py`

### 檔案定位

Host 端的 Kernel Loader + 簡易終端機工具：透過 `socket://127.0.0.1:8888` 連到 QEMU 的 serial server，等待 Bootloader 就緒後依協定送出 `build/kernel8.img`，最後進入互動模式。

### 相依性（就現況）

- Python modules
  - `serial`（pyserial）
  - `os`
  - `struct`
  - `sys`
  - `select`
  - `time`（目前 import 但未實際使用）
- 外部環境假設
  - QEMU serial server：`127.0.0.1:8888`
  - Kernel image 路徑：`build/kernel8.img`

### 目前提供的功能（實作）

#### 1) 連線到 QEMU Serial

- 啟動時呼叫：
  - `serial.serial_for_url('socket://127.0.0.1:8888', 115200)`
- 若連線失敗：
  - 輸出 `Failed to connect to QEMU: ...`
  - `sys.exit(1)`

#### 2) 等待 Bootloader Ready（握手）

- 輸出 `Listening for Bootloader...`
- 迴圈中持續：
  - `raw = ser.readline()`
  - `line = raw.decode('utf-8', errors='ignore')`
- 若讀到包含 `OSDI: Ready` 的行：
  - 輸出 `-> Device is ready! Starting transmission.`
  - 跳出等待迴圈

#### 3) 傳送 kernel size

- 讀取 `build/kernel8.img` 大小
- `header = struct.pack('<I', file_size)`
- 輸出 `Sending kernel size: ... bytes`
- `ser.write(header)`
- 讀回 Bootloader 用 `uart_send_hex` / `early_uart_send_hex` 回送的確認訊息

#### 4) 傳送 kernel image

- 以 binary 模式開啟 `build/kernel8.img`
- `ser.write(f.read())`
- 輸出 `Kernel image sent successfully.`

#### 5) 互動模式

- 印出 `Entering interactive mode (Ctrl+C to exit)...`
- 使用 `select.select([ser.fileno(), sys.stdin.fileno()], [], [])`
- 若 serial 有資料：
  - `ser.read(ser.in_waiting or 1)`
  - decode 後直接印到終端
- 若 stdin 有資料：
  - `sys.stdin.readline()`
  - `ser.write(user_input.encode())`

### 現況注意

- 這個工具依賴 Bootloader 的 early UART 協定，因此 `OSDI: Ready` 與 4-byte little-endian size header 是固定配套的一部分。
- 互動模式不是一般 shell 模擬器，而是直接把 Host stdin / serial output 做雙向轉發。
- `time` module 雖然被 import，但目前流程中沒有實際使用。

## 跨文檔導讀

- 若要理解 Bootloader 為何等待 `OSDI: Ready`、為何先收 4-byte size，請看 [Boot/Boot.md](../Boot/Boot.md)。
- 若要理解 serial server `:8888` 與 GDB server `:1234` 如何被 VS Code workflow 啟動，請看 [Configuration.md](../Configuration.md) 中的 Build Automation & VS Code Integration 章節。
- 若要理解載入完成後互動模式實際接到哪個 shell，請看 [Kernel/Kernel.md](../Kernel/Kernel.md) 與 [Shell/Shell.md](../Shell/Shell.md)。
- 若要理解 early UART 與固定 MMIO base 為何能在 boot 階段先工作，請看 [Driver/Driver.md](../Driver/Driver.md) 與 [Board/Board.md](../Board/Board.md)。

## 閱讀建議

- 想看「Host 端如何把 kernel 傳進 QEMU」：先看本檔，再看 [Boot/Boot.md](../Boot/Boot.md)。
- 想看「VS Code 啟動 debug 時背後發生什麼事」：本檔搭配 [Configuration.md](../Configuration.md) 一起看。
