OSDI Lab Development Progress Tracker
Current Lab: Lab 2 - Booting Target Platform: Raspberry Pi 3 B+ (AArch64) Environment: WSL (Ubuntu) + QEMU + GDB

# 0. 系統總覽 (System Overview)

## 0.1 目前可做什麼（Overview / What you can do now）

- 目前可在 QEMU 上啟動 Bootloader，Bootloader 會先透過 UART 對 Host 輸出 `OSDI: Ready`，表示已就緒並等待載入 Kernel。
- Host 端執行 `Python/send_kernel.py` 連線到 QEMU Serial Port（127.0.0.1:8888），並依協定送出 Kernel Size（Little Endian）與 `build/kernel8.img` 內容。
- Bootloader 會把 Kernel 寫入 `KERNEL_LOAD_ADDRESS (0x80000)`，再以 Function Pointer 方式跳轉到 Kernel Entry Point。
- Kernel 啟動後會再次初始化 UART，印出 `Welcome to OSDI`，接著呼叫 `shell_main()` 進入互動模式。
- 整體資料流為：先使用 `make all` 完成建置（產出 `build/bootloader.img` / `build/kernel8.img`）→ 再以 `make qemu-gdb` 啟動 QEMU（GDB Server :1234 + Serial Server :8888）→ Host（send_kernel.py） → Bootloader（UART handshake + load） → Kernel（print + init） → Shell（互動輸入輸出）。

## 0.2 建置與執行流程（Build & Run Pipeline）

- Makefile：設定 aarch64-linux-gnu- 交叉編譯工具鏈，並將 Bootloader / Kernel 分開建置成兩個 Image（`build/bootloader.img`、`build/kernel8.img`）。
- Makefile Targets：提供 `make qemu` 啟動 QEMU 模擬；`make qemu-gdb` 啟動 QEMU 並開啟 GDB Server（:1234）與 Serial Server（:8888）。
- Linker Scripts：
  - `linker_boot.ld`：設定 Bootloader Entry Point 為 0x60000，並定義 BSS 與 Stack Top。
  - `linker_kernel.ld`：設定 Kernel Entry Point 為 0x80000，並定義 BSS 與 Stack Top。
- VS Code：
  - `tasks.json`：自動化 `make all` 與啟動 `make qemu-gdb`（並加上 pkill 避免舊 QEMU 佔用 Port）。
  - `launch.json`：設定 GDB 連線至 :1234，並載入 `build/bootloader.elf` 符號表以便除錯。

## 0.3 開機交棒流程（Boot Chain）

- `Assembly/boot.S`：
  - relocate：將程式碼從載入位址搬移至 Linker 設定位址（Bootloader 為 0x60000）。
  - clear bss：清空 BSS，確保全域變數初始值為 0。
  - set sp：設定 Stack Pointer。
- handoff：跳轉至 C 語言入口（Bootloader / Kernel 皆以 `kernel_main` 作為 entry；Bootloader build 的 `kernel_main` 即 Bootloader 主流程）。
- `CFile/bootloader_main.c`：
  - UART init：初始化 Mini UART。
  - handshake：送出 `OSDI: Ready` 通知 Host 端可以開始傳輸。
  - load：接收 Kernel Size（4 bytes）與 Kernel image（byte-by-byte），寫入 0x80000。
  - jump：使用 Function Pointer 方式跳轉至 Kernel Entry。

## 0.4 Kernel 主流程（Kernel Core / Interactive Loop）

- `CFile/kernel_main.c`：
  - re-init UART：再次初始化 UART（確保硬體狀態正確）。
  - print welcome：印出 `Welcome to OSDI` 確認已進入 Kernel。
  - enter shell：呼叫 `shell_main()` 進入互動模式。

## 0.5 核心硬體依賴（Drivers & Peripherals）

- 目前最核心依賴：Mini UART（因為 Bootloader 載入流程、Kernel 輸出、Shell 互動、Host Tool 通訊都依賴 UART）。
- `CFile/uart.c / header/uart.h`：
  - init：設定 GPIO14/15 為 ALT5、設定 Baud Rate（115200）、關閉 Flow Control。
  - polling I/O：`uart_send()` 輪詢 TX FIFO、`uart_recv()` 輪詢 RX FIFO。
  - helpers：提供 uart_puts、uart_send_hex、uart_recv_uint 等輔助函式。
- （建議後續新增 mailbox / timer / power / cpio / dtb 時）撰寫順序可採「被誰呼叫 → 提供什麼 API → 影響哪個功能」，以維持可讀性與依賴關係清晰。

## 0.6 Host 端工具（Host Tools）

- `Python/send_kernel.py`：
  - connection：以 socket 連線到 QEMU Serial Port（127.0.0.1:8888）。
  - loader protocol：等待 `OSDI: Ready` → 傳送 Kernel Size（Little Endian）→ 傳送 `build/kernel8.img` 檔案內容。
  - terminal emulator：使用 select 做非阻塞 I/O，同時監聽 socket（目標輸出）與 stdin（使用者輸入），並處理 UTF-8 decode。
# 1. 共同定義 (Common Definitions)

## `common.h`

### 檔案定位

提供專案的**共用型別/巨集**與**MMIO（Memory-mapped I/O）**讀寫工具，並定義部分 Raspberry Pi（以 `MMIO_BASE` 為基準）的 GPIO 暫存器位址常數。

### 目前提供的功能

- **基本常數/型別**
    
    - `NULL`
        
    - 在非 C++ 編譯（`#ifndef __cplusplus`）時定義：
        
        - `typedef _Bool bool;`
            
        - `true` / `false`
            
- **常用巨集**
    
    - `ALIGN4(x)`：將 `x` 向上對齊到 4-byte boundary
        
    - `MAX_ARGS`：定義為 `16`
        
    - `MAX_STRING_SIZE`：定義為 `1024`
        
- **位址常數**
    
    - `KERNEL_LOAD_ADDRESS 0x80000`
        
    - `MMIO_BASE 0x3F000000`
        
    - GPIO 相關：
        
        - `GPIO_BASE (MMIO_BASE + 0x200000)`
            
        - `GPFSEL1`, `GPPUD`, `GPPUDCLK0`
            
- **MMIO 讀寫 helper（inline）**
    
    - `mmio_write(unsigned long reg, unsigned int data)`：以 `volatile unsigned int*` 對 `reg` 寫入 `data`
        
    - `mmio_read(unsigned long reg)`：以 `volatile unsigned int*` 讀取 `reg` 並回傳
        

### 目前的限制/假設（就現況描述）

- `bool/true/false/ALIGN4/MAX_ARGS/MAX_STRING_SIZE` 僅在 **非 C++** 時定義；若某些編譯單元以 C++ 編譯，這些符號在該單元可能不存在（取決於你的 include 與編譯設定）。
    
- `MMIO_BASE` 固定為 `0x3F000000`（適用情境取決於你的硬體/平台設定；此檔案本身不做自動判斷）。
# 2. Mailbox 介面 (Mailbox Interface)

## `mailbox.h`

### 檔案定位

定義 Mailbox（Property Channel）相關暫存器位址與 API，用於透過 Mailbox 與 GPU/firmware 溝通（由 `mailbox.c` 實作）。

### 目前提供的功能

- Mailbox base 與暫存器位址巨集（以 `MMIO_BASE` 推導）：
    
    - `MBOX_BASE_OFFSET 0xB880`
        
    - `MBOX_READ / MBOX_PEEK / MBOX_SENDER / MBOX_STATUS / MBOX_CONFIG / MBOX_WRITE`
        
- Property channel 常數：
    
    - `MBOX_CH_PROP (8)`
        
- 對外函式宣告：
    
    - `mailbox_call(unsigned int channel)`
        
    - Board revision 查詢：
        
        - `prepare_board_revision_request()`
            
        - `get_board_status()`
            
        - `get_board_revision()`
            
    - Memory 資訊查詢：
        
        - `prepare_memory_request()`
            
        - `get_memory_status()`
            
        - `get_memory_size()`
            

---

## `mailbox.c`

### 檔案定位

實作 Mailbox property call 的**送出/等待流程**，並提供兩種「準備 request 與讀取 response」的工具函式：

1. Board Revision（tag `0x00010002`）
    
2. Memory Info（tag `0x00010005`）
    

### 目前提供的功能（實作細節）

#### 內部資料結構與緩衝區

- 定義兩個 request/response 结构：
    
    - `mbox_board_recv_t`（7 個 `unsigned int`）
        
    - `mbox_memory_t`（8 個 `unsigned int`）
        
- 使用 `_Static_assert` 檢查 struct 大小是否等於 `unsigned int` 陣列大小（避免 padding 導致格式錯誤）。
    
- 全域 Mailbox buffer：
    
    - `unsigned int mailbox_buffer[8] __attribute__((aligned(16)));`
        
    - 16-byte 對齊，供 mailbox 傳遞用
        

#### `mailbox_call(unsigned int channel)`

- 功能：將 `mailbox_buffer` 的位址 + `channel` 寫入 `MBOX_WRITE`，並輪詢等待回覆。
    
- 具體行為：
    
    1. 輪詢 `MBOX_STATUS`，等待「可寫」（用 `MBOX_CAN_NOT_WRITE 0x80000000` 判斷）
        
    2. `buffer_address = (unsigned int)((unsigned long)mailbox_buffer);`
        
    3. `buffer_address = (buffer_address & ~(0xF)) | channel;`（保留低 4 bits 放 channel）
        
    4. 寫入 `MBOX_WRITE`
        
    5. 輪詢 `MBOX_STATUS`，等待「可讀」（用 `MBOX_CAN_NOT_READ 0x40000000` 判斷）
        
    6. 讀 `MBOX_READ`，僅檢查低 4 bits channel 符合就結束等待
        

> 現況注意：等待回覆時只檢查回傳值的 `channel`（低 4 bits），未檢查回傳內容是否對應到同一個 buffer 位址，也未檢查 property 回覆碼語意。

#### Board revision request / response

- `prepare_board_revision_request()`
    
    - 將 `mailbox_buffer` cast 成 `mbox_board_recv_t*`
        
    - 填入：
        
        - `buffer_size = sizeof(mbox_board_recv_t)`
            
        - `request_and_response = 0`
            
        - `tag = 0x00010002`
            
        - `value_buffer_size = 4`
            
        - `value_buffer_recv_size = 0`
            
        - `value_buffer = 0`
            
        - `end_tag = 0`
            
- `get_board_status()`：回傳 `request_and_response`
    
- `get_board_revision()`：回傳 `value_buffer`
    

#### Memory request / response

- `prepare_memory_request()`
    
    - 將 `mailbox_buffer` cast 成 `mbox_memory_t*`
        
    - 填入：
        
        - `buffer_size = sizeof(mbox_memory_t)`
            
        - `request_and_response = 0`
            
        - `tag = 0x00010005`
            
        - `value_buffer_size = 8`
            
        - `value_buffer_recv_size = 0`
            
        - `base_addr = 0`
            
        - `mem_size = 0`
            
        - `end_tag = 0`
            
- `get_memory_status()`：回傳 `request_and_response`
    
- `get_memory_size()`：回傳 `mem_size`
# 3. 電源管理 (Power Manager)

## `power_manager.h`

### 檔案定位

宣告電源/看門狗（watchdog）相關暫存器常數與 API，用於觸發或取消重啟。

### 目前提供的功能

- 暫存器與常數巨集：
    
    - `PM_PASSWORD (0x5A000000)`
        
    - `PM_RSTC (MMIO_BASE + 0x0010001C)`
        
    - `PM_WDOG (MMIO_BASE + 0x00100024)`
        
- 函式宣告：
    
    - `void reset(int tick);`
        
    - `void cancel_reset(void);`
        

---
## `power_manager.c`

### 檔案定位

實作 `reset` 與 `cancel_reset`，透過 `mmio_write()` 寫入 `PM_RSTC/PM_WDOG`。

### 目前提供的功能（實作）

#### `reset(int tick)`

- 定義：
    
    - `PM_RSTC_FULL_RESET (0x20)`
        
    - `TIMETICK_MASK (~(0XFFF << 20))`
        
- 行為：
    
    1. `mmio_write(PM_RSTC, PM_PASSWORD | PM_RSTC_FULL_RESET);`
        
    2. `tick &= TIMETICK_MASK;`（遮罩處理 tick）
        
    3. `mmio_write(PM_WDOG, PM_PASSWORD | tick);`（設定 watchdog 倒數觸發 reset）
        

#### `cancel_reset()`

- 行為：
    
    - `mmio_write(PM_RSTC, PM_PASSWORD | 0);`
        
    - `mmio_write(PM_WDOG, PM_PASSWORD | 0);`
        
# 4. 字串處理函式庫 (String Library)
## `string.h`

### 檔案定位

宣告專案自製的字串/字元掃描介面（避免依賴 libc），並提供一個輕量的字串描述型別 `string_t`。

### 目前提供的功能（宣告）

- 型別：
    
    - `typedef struct string { char* string_ptr; unsigned int size; } string_t;`
        
        - 用 `(ptr, size)` 方式攜帶字串緩衝區位置與長度資訊（是否使用取決於呼叫端）。
            
- 字串比較：
    
    - `int strcmp(const char *s1, const char *s2);`
        
    - `int strncmp(const char *s1, const char *s2, unsigned long read_byte);`
        
- 字元掃描（bounded）：
    
    - `unsigned int strcspn(const char *s, const char reject, int max_len);`
        
- 長度計算（bounded）：
    
    - `unsigned long strlen(const char *s);`
        

> 現況注意：`strcspn()` 的介面與標準 libc 的 `strcspn(const char*, const char*)` 不同；此專案版本只支援「單一 reject 字元」並額外提供 `max_len` 上限。
    

---

## `string.c`

### 檔案定位

實作 `string.h` 宣告的字串函式；其中 `strlen()` 會使用 `common.h` 內的 `MAX_STRING_SIZE` 作為掃描上限，避免在缺少 `'\0'` 的情況下無限掃描。

### 目前提供的功能（實作）

#### `strcmp(const char* s1, const char* s2)`

- 逐 byte 比較 `s1` 與 `s2`，直到：
    
    - 任一字元為 `'\0'`，或
        
    - 發現不同字元
        
- 回傳：`*(unsigned char *)s1 - *(unsigned char *)s2`
    
    - 使用 `unsigned char` 做差，避免符號位造成差值異常。
        

#### `strncmp(const char *s1, const char *s2, unsigned long read_byte)`

- 若 `read_byte == 0`：直接回傳 `0`
    
- 否則最多比較 `read_byte` 次：
    
    - 若遇到不同字元 → 跳出迴圈
        
    - 若遇到 `'\0'`（以 `s1_tmp` 判斷）→ 立即回傳 `0`
        
- 迴圈結束後回傳：`(unsigned char)s1_tmp - (unsigned char)s2_tmp`
    

> 現況注意：此實作**已**在 `read_byte == 0` 時提前回傳 `0`，因此不會出現「未初始化的 s1_tmp/s2_tmp 被拿來回傳」的未定義行為（這點與你 README 內目前寫的現況注意不同，建議以此段更新）。

#### `strcspn(const char *s, const char reject, int max_len)`

- 目的：在 **最多 `max_len`** 的範圍內，尋找字串中第一個：
    
    - `'\0'`，或
        
    - 等於 `reject` 的字元
        
- 回傳：
    
    - 找到上述條件時回傳其 index
        
    - 若掃描滿 `max_len` 仍未命中，回傳 `max_len`
        

> 現況注意：此版本是「單一 reject 字元」的 bounded 掃描器；若需要 reject-set（多個字元集合）語意，需要呼叫端自行擴充或另寫函式。

#### `strlen(const char *s)`

- 目的：計算字串長度，但**最多掃描到 `MAX_STRING_SIZE`**（上限由 `common.h` 提供）。
    
- 行為流程（就目前實作）：
    
    1. `s == NULL` 時回傳 `0`
        
    2. 先以 byte 方式前進到 8-byte 對齊邊界（中途若遇到 `'\0'` 直接回傳）
        
    3. 之後以 64-bit（`unsigned long`）為單位掃描，使用 magic number 偵測 word 內是否含 `'\0'`
        
    4. 若偵測到可能含 `'\0'`，回到 byte 模式逐字確認並回傳
        
    5. 若掃描到 `MAX_STRING_SIZE` 仍未遇到 `'\0'`，回傳當前累積的 `length`（最大不超過 `MAX_STRING_SIZE`）
        

> 現況注意：此 `strlen()` 的 word-scan 假設 `unsigned long` 為 64-bit（AArch64 環境成立）；且若輸入緩衝區在 `MAX_STRING_SIZE` 範圍內沒有 `'\0'`，回傳值會被「上限截斷」。
# 5. 通用工具 (Utilities)

## `utils.h`

### 檔案定位

宣告專案通用工具函式介面。

### 目前提供的功能（宣告）

- `void *memcpy(void *dest, const void *src, unsigned long n);`
    
- `int hex2int(char *hex, int n);`
    

---

## `utils.c`

### 檔案定位

實作 `utils.h` 宣告的工具函式。

### 目前提供的功能（實作）

#### `memcpy(void *dest, const void *src, unsigned long n)`

- 以 byte-by-byte 方式複製 `n` bytes：`dest[i] = src[i]`
    
- 回傳 `dest`（以 `dest_char` 轉回 `void*` 回傳）
    

> 現況注意：此版本僅做單向逐 byte 複製，未處理標準 `memcpy` 常見的效能最佳化；也未處理「重疊區間」的安全性（重疊時是否安全，取決於實際呼叫方式；此函式本身不做判斷）。

#### `hex2int(char *hex, int n)`

- 將長度為 `n` 的字元序列視為 16 進位數字，轉成 `int`
    
- 每個字元處理：
    
    - 先做 `result *= 16`
        
    - 再依字元範圍加上對應數值：
        
        - `hex[i] >= 'a'` → 視為小寫字母，使用 `10 + (hex[i] - 'a')`
            
        - 否則若 `hex[i] >= 'A'` → 視為大寫字母，使用 `10 + (hex[i] - 'A')`
            
        - 否則 → 視為數字，使用 `(hex[i] - '0')`
            

> 現況注意：條件是用 `>= 'a'` / `>= 'A'`，沒有額外限制到 `'f'`/`'F'`；若輸入含非預期字元，仍會被某個分支吃掉並產生結果（函式本身不做合法性檢查）。


# 6. CPIO 檔案系統解析器 (CPIO Parser)

## `cpio.h`

### 檔案定位

提供 CPIO（initramfs）存取相關的**介面宣告**與一個預設的 initramfs 起始位址常數，供 kernel 其他模組呼叫來：

- 列出 CPIO 內的檔名
    
- 輸出指定檔案內容
    

### 目前提供的功能

- `#define FILE_HEADER 0x8000000`
    
    - 定義一個預期的 CPIO header 起始位址常數（作為呼叫端可用的預設值）。
        
- 函式宣告：
    
    - `int CpioGetFilesHeaderName(void *file_header);`
        
        - 以 `file_header` 為起點，逐筆走訪 CPIO entries，**輸出檔名**並回傳檔案數量（實際輸出格式由 `cpio.c` 決定）。
            
    - `bool CpioGetFileContext(void *file_header, char* file_name);`
        
        - 以 `file_header` 為起點，搜尋檔名等於 `file_name` 的 entry，若找到則**輸出檔案內容**並回傳 `true`，否則回傳 `false`。
            

### 相依性（就現況）

- `#include "../header/common.h"`：使用 `bool/true/false` 與對齊巨集/型別等（由實作端使用）。
    

---

## `cpio.c`

### 檔案定位

提供 CPIO（new ASCII / `070701`）格式的**最小可用解析與輸出**：

- 掃描所有 entries 並將檔名透過 UART 輸出
    
- 依檔名查找 entry 並將檔案內容透過 UART 輸出
    

### 相依性（就現況）

- `../header/cpio.h`：對外 API 宣告
    
- `../header/string.h`（你先前那版）：使用 `strncmp()`、`strlen()`
    
- `../header/utils.h`：使用 `hex2int()` 將 8-byte ASCII hex 欄位轉數值
    
- `../header/uart.h`：使用 `uart_send()`、`uart_puts()`（以及其他 UART 輸出函式由外部提供）
    
- `../header/common.h`：使用 `ALIGN4()` 做 4-byte 對齊
    

### 目前提供的功能（實作）

#### 1) 內部資料結構：`cpio_header_t`

- 以 struct 定義 newc header 的固定欄位（皆為 ASCII hex 字串欄位），包含：
    
    - `c_magic[6]`：用 `"070701"` 判斷 entry 開頭
        
    - `c_namesize[8]`：檔名長度（含結尾 `\0`）
        
    - `c_filesize[8]`：檔案內容大小
        
    - 其餘 newc 欄位（ino/mode/uid/gid/nlink/mtime/dev/rdev/check 等）
        

> 現況注意：程式碼以「header 固定長度 = 110 bytes」為基準位移（newc 的 header 長度）。

---

#### 2) `int CpioGetFilesHeaderName(void *file_header)`

**功能：列出並輸出所有檔名，回傳檔案數量。**

- 入口檢查：
    
    - 先將 `file_header` cast 成 `cpio_header_t*`
        
    - 若 `c_magic` 與 `"070701"` 不符，直接回傳 `0`
        
- 迴圈走訪 entries：
    
    - 讀取 `c_namesize`（8-byte ASCII hex）轉成 `file_name_size`
        
    - 以「檔名長度為 11 且檔名為 `TRAILER!!!`」作為結束條件，遇到即停止
        
    - 取得檔名指標：
        
        - `filename_ptr = (char*)header + 110`（header 後緊接檔名）
            
    - 透過 UART 輸出檔名：
        
        - 逐字輸出 `file_name_size - 1` 個字元（不含結尾 `\0`）
            
    - `file_count += 1`
        
    - 呼叫內部 `GetNextHeader()` 推進到下一個 entry；若回傳 `NULL` 則停止
        
- 回傳：
    
    - 回傳掃描到的 `file_count`
        

> 現況注意（就目前輸出行為）：此函式只逐字輸出檔名，**不額外輸出分隔符（例如換行或空白）**；實際呈現效果取決於呼叫端是否自行補上格式化輸出。

---

#### 3) `bool CpioGetFileContext(void *file_header, char* file_name)`

**功能：搜尋指定檔名並輸出該檔案內容。**

- 入口檢查：
    
    - 若起始 `c_magic` 與 `"070701"` 不符，直接回傳 `false`
        
- 迴圈搜尋：
    
    - 呼叫內部 `CompareFileName()` 比對目前 entry 的檔名是否等於 `file_name`
        
        - 比對長度使用 `strlen(file_name) + 1`（包含 `\0`），因此是「完整字串含結尾」的嚴格匹配
            
    - 若匹配成功：
        
        1. 解析：
            
            - `file_context_size = hex2int(c_filesize, 8)`
                
            - `file_header_size = hex2int(c_namesize, 8)`（實際是檔名長度）
                
        2. 取得內容起始位址：
            
            - `header = (cpio_header_t*)((char*)header + 110)`（移到檔名起點）
                
            - `header = (cpio_header_t*)((char*)header + file_header_size)`（跳過檔名區）
                
            - `current_ptr = ALIGN4((unsigned long)header)`（4-byte 對齊）
                
            - `file_content_ptr = (char*)current_ptr`
                
        3. 輸出內容：
            
            - 逐 byte 輸出 `file_context_size` bytes（`uart_send(*file_content_ptr)`）
                
            - 最後 `uart_puts("\n")`
                
        4. 回傳 `true`
            
    - 若未匹配：
        
        - 透過 `GetNextHeader(file_header)` 推進下一個 entry
            
        - 若下一個 header 為 `NULL`，跳出迴圈並回傳 `false`
            

---

#### 4) 內部 helper：`static void* GetNextHeader(void *file_header)`

**功能：從目前 entry 計算下一個 entry 的 header 位址。**

- 解析當前 entry 的：
    
    - `file_name_size = hex2int(c_namesize, 8)`
        
    - `file_size = hex2int(c_filesize, 8)`
        
- 位移計算流程（就現況）：
    
    1. `header = header + 110`（移到檔名起點）
        
    2. `header = header + file_name_size`（跳過檔名區，含 `\0`）
        
    3. `current_ptr = ALIGN4(current_ptr)`（對齊到 4）
        
    4. `current_ptr = ALIGN4(current_ptr + file_size)`（跳過內容，再對齊到 4）
        
- 回傳：
    
    - 若新位置的 `c_magic` 不是 `"070701"`，回傳 `NULL`
        
    - 否則回傳下一個 header 指標
        

---

#### 5) 內部 helper：`static bool CompareFileName(cpio_header_t* file, char* cmp_name, int cmp_size)`

**功能：比較 entry 的檔名是否與指定名稱相符（以 `strncmp`）。**

- 行為：
    
    - 將 `file` 指標位移 `+110` 到檔名起點
        
    - `return (strncmp((char*)file, cmp_name, cmp_size) == 0);`
        

> 現況注意：此比較完全依賴呼叫端傳入的 `cmp_size`；例如：

- `CpioGetFilesHeaderName()` 用於判斷 `TRAILER!!!` 時，傳入的是 `strlen("TRAILER!!!")`（不含 `\0`）
    
- `CpioGetFileContext()` 搜尋使用者輸入檔名時，傳入 `strlen(file_name) + 1`（含 `\0`）
    
# 7. 系統計時器 (System Timer)
## `time.h`

### 檔案定位

提供時間（system timer）輸出功能的介面宣告。

### 目前提供的功能

- 函式宣告：
    
    - `void get_timetick();`
        
        - 由 `time.c` 實作：讀取 system counter 與頻率，計算並輸出目前「秒與小數」格式的時間值。
            

---


## `time.c`

### 檔案定位

在 AArch64 環境下，透過系統暫存器讀取 system counter（`cntpct_el0`）與 counter frequency（`cntfrq_el0`），並將目前時間以 UART 輸出。

### 相依性（就現況）

- `../header/time.h`
    
- `../header/uart.h`
    
    - 使用 `uart_send_integer()`、`uart_send()`、`uart_send_decimal_part()`
        

### 目前提供的功能（實作）

#### 1) 內部 helper：`static unsigned long long get_system_timer_count()`

- 使用 inline assembly：
    
    - `mrs %0, cntpct_el0`
        
- 回傳 `cntpct_el0` 讀到的計數值（`unsigned long long`）
    

#### 2) 內部 helper：`static unsigned long long get_system_timer_frequency()`

- 使用 inline assembly：
    
    - `mrs %0, cntfrq_el0`
        
- 回傳 `cntfrq_el0` 讀到的計數器頻率（`unsigned long long`）
    

#### 3) 對外 API：`void get_timetick()`

- 行為流程（就現況）：
    
    1. 讀取：
        
        - `timer_count = get_system_timer_count()`
            
        - `timer_freq = get_system_timer_frequency()`
            
    2. 計算：
        
        - `timetick_integer_part = timer_count / timer_freq`
            
        - `decimal_part = ((timer_count % timer_freq) * 10000) / timer_freq`
            
            - 小數部分以 **4 位小數**尺度輸出（0～9999）
                
    3. UART 輸出格式：
        
        - 輸出整數秒：`uart_send_integer(timetick_integer_part)`
            
        - 輸出 `'.'`
            
        - 輸出 4 位小數：`uart_send_decimal_part(decimal_part, 4)`
            

> 現況注意：此函式本身**不輸出換行**；顯示換行與否由呼叫端決定。另外它將計算結果存入 `int`（整數秒與小數），在長時間運行下是否溢位取決於執行時間與 `int` 寬度，但程式碼現況並未處理溢位或格式化邊界。


# 8. 簡易命令列介面 (Shell)

## `shell.h`

### 檔案定位

定義簡易 shell 的命令資料結構（command table）與對外 API。

### 內容概述

此檔案定義簡易 shell 的命令表資料結構與對外介面：

- 命令 handler 介面：`CommandFunc(int argc, char* argv[])`
- 命令表元素：`Command_t { name, description, func }`
- 對外 API：
  - `shell_main()`：shell 主迴圈入口
  - `shell_input_line()`：顯示 prompt、讀入一行並回傳 buffer
  - `execute_command(argc, argv)`：依命令表比對並執行對應 handler
### 目前提供的功能

- 命令函式型別：
    
    - `typedef void(*CommandFunc)(int argc, char* argv[]);`
        
- 命令結構：
    
    `typedef struct Command {     const char *name;     const char *description;     CommandFunc func; } Command_t;`
    
- 對外 API：
    
    - `void shell_main();`
        
    - `void execute_command(int argc, char* argv[]);`
        
    - `char* shell_input_line();`
        

---

## `shell.c`

### 檔案定位

提供互動式 shell（blocking UART I/O）：

- 顯示 prompt（含時間）
    
- 接收一行輸入並進行字串切割（argc/argv）
    
- 依命令表執行對應指令（hello/help/info/time/reboot/cancel/ls/cat）
    

### 內容概述

此檔案實作互動式 shell（blocking UART I/O），核心流程為：

1. 顯示開機訊息後進入無限迴圈（`shell_main()`）
2. 每輪先印出 prompt：`[` + `get_timetick()` + `]` + `:shell$ `
3. 讀入一行輸入（支援 Enter / Backspace，存入固定長度 `input_buffer[128]`）
4. 以空白做就地切割（in-place tokenize）產生 `argc/argv`
5. 在命令表 `commands[]` 中做字串比對，找到就呼叫對應 handler，否則輸出 `Command not found: ...`

內建命令（就現況 `commands[]`）：

- `hello`：輸出 Hello World
- `help`：列出所有命令與描述
- `info`：透過 mailbox 輸出 board revision 與 memory size（hex）
- `time`：輸出目前 timetick
- `reboot`：設定 watchdog reset（並開啟 reboot lock）
- `cancel`：取消 reset（並解除 reboot lock）
- `ls`：列出 initramfs 內檔名（header 來源為 DTB context 的 `initrd_start`）
- `cat`：輸出指定檔案內容（header 來源同上）

現況注意（以程式碼行為為準）：

- `shell_main()` 呼叫 `shell_input_line()` 時不使用回傳值，但因為 tokenization 是直接處理全域 `input_buffer`，所以流程仍成立。
- `input_buffer` 固定 128 bytes：滿了之後仍會回顯輸入，但不再寫入 buffer。
- reboot lock 檢查使用字串 `"Cancel Reboot"`，但實際解除命令名稱是 `cancel`；在 lock 開啟後，依目前邏輯可能導致無法透過既有命令解除（此為現有行為描述）。
### 相依性（就現況）

- `uart.h`：`uart_puts/uart_send/uart_recv/uart_send_hex` 等
    
- `string.h`（你先前那版）：`strcmp()`
    
- `mailbox.h`：board/memory request 與讀回（`prepare_*`, `mailbox_call`, `get_*`）
    
- `power_manager.h`：`reset()`, `cancel_reset()`
    
- `time.h`：`get_timetick()`
    
- `cpio.h`：`FILE_HEADER`, `CpioGetFilesHeaderName()`, `CpioGetFileContext()`
    
- `common.h`：`MAX_ARGS`, `bool/true/false`
    

---

### 目前提供的功能（實作）

#### 1) 命令表 `commands[]`

以靜態陣列定義命令名稱/描述/處理函式，包含：

- `hello`：輸出 Hello World
    
- `help`：列出所有命令與描述
    
- `info`：透過 mailbox 輸出 board revision 與 memory size（hex）
    
- `time`：輸出目前 timetick（並換行）
    
- `reboot`：設定 watchdog reset（`reset(150000)`）並進入 reboot lock
    
- `cancel`：取消 reset（`cancel_reset()`）並解除 reboot lock
    
- `ls`：列出 initramfs 內檔名（以 `FILE_HEADER` 為起點）
    
- `cat`：輸出指定檔案內容（以 `FILE_HEADER` 為起點）
    
- 尾端 sentinel：`{NULL, NULL}`
    

#### 2) `void shell_main()`

**功能：shell 主迴圈。**

- 先輸出開機訊息：`"\n\n=== RPi3 OS Booting... ===\n"`
    
- 進入無限迴圈：
    
    1. 呼叫 `shell_input_line()` 讀入一行（內容寫入內部 `input_buffer`）
        
    2. `split_command(&argc, argv)`：將 `input_buffer` 切成 tokens
        
    3. 若 `argc > 0`：`execute_command(argc, argv)`
        

#### 3) `char* shell_input_line()`

**功能：顯示 prompt 並讀入一行（支援 Enter / Backspace）。**

- prompt 輸出格式（依現況輸出順序）：
    
    - `[` + `get_timetick()` + `]` + `:shell$`
        
- 輸入處理（blocking）：
    
    - Enter（`\r` 或 `\n`）：
        
        - 輸出 `"\n"`
            
        - 在 `input_buffer` 結尾補 `'\0'`
            
        - 回傳 `input_buffer`
            
    - Backspace（`\b` 或 ASCII 127）：
        
        - 若 `buffer_index > 0`，`buffer_index--`
            
        - 回顯 `"\b \b"` 以刪除終端上的字元
            
    - 其他字元：
        
        - 回顯 `uart_send(c)`
            
        - 若 buffer 未滿（< 127），寫入 `input_buffer`
            

> 現況：`input_buffer` 固定 128 bytes，滿了之後仍會回顯輸入，但不再寫入 buffer（以程式碼行為為準）。

#### 4) `static void split_command(int* argc, char* argv[])`

**功能：將 `input_buffer` 以空白分割成 `argv[]`，並設定 `argc`。**

- 逐字掃描：
    
    - 遇到 `' '`：改寫成 `'\0'`（原地切割）並跳過
        
    - 遇到 token 起點：
        
        - 設定 `argv[*argc] = cursor`，`(*argc)++`
            
        - 再移動 cursor 直到遇到空白或 `'\0'`
            
- 若超過 `MAX_ARGS`：
    
    - 輸出 `"Warning: Too many arguments, ignoring the rest.\n"` 並停止
        

#### 5) `void execute_command(int argc, char* argv[])`

**功能：比對 `argv[0]` 與命令表，找到就執行，否則輸出 not found。**

- reboot lock（`static bool reboot_lock`）行為（依現況）：
    
    - 在迴圈中，若 `reboot_lock == true` 且 `strcmp("Cancel Reboot", argv[0]) != 0`：
        
        - 輸出 `"Rebooting... Please input 'Cancel Reboot' to abort."`
            
        - 直接 `return`
            
- 正常情況下：
    
    - `strcmp(commands[i].name, argv[0]) == 0` 時呼叫 `commands[i].func(argc, argv)` 並 return
        
    - 若無匹配：
        
        - 輸出：
            
            - `"\nCommand not found:" + argv[0] + "\n"`
                

> 現況注意：命令表中解除重啟的命令名稱是 `cancel`，但 lock 檢查字串是 `"Cancel Reboot"`；因此在 `reboot_lock` 開啟後，依目前程式碼邏輯可能導致「既有命令無法通過 lock 檢查」的狀況（此為現有行為描述，不推測設計意圖）。

---

### 內建命令（就現況行為）

#### `cmd_hello`

- 輸出：`"Hello, World!\n"`
    

#### `cmd_help`

- 輸出 `"Available commands:\n"`
    
- 逐條列出 `commands[]`：`" - " + name + ":" + description + "\n"`
    

#### `cmd_board_info`（`info`）

- 送 board revision request：
    
    - `prepare_board_revision_request()` → `mailbox_call(MBOX_CH_PROP)`
        
    - 若 `get_board_status() == 0x80000000`：
        
        - 輸出 `"Board Revision: "` + `uart_send_hex(get_board_revision())`
            
- 送 memory request：
    
    - `prepare_memory_request()` → `mailbox_call(MBOX_CH_PROP)`
        
    - 若 `get_memory_status() == 0x80000000`：
        
        - 輸出 `"Memory Size is: "` + `uart_send_hex(get_memory_size())`
            

#### `cmd_get_timer`（`time`）

- 呼叫 `get_timetick()`，再輸出 `"\n"`
    

#### `cmd_reboot`（`reboot`）

- 輸出：`"Rebooting in T-minus 2 seconds...\n"`
    
- 呼叫 `reset(150000)`
    
- 設定 `reboot_lock = true`
    

#### `cmd_cancel_reboot`（`cancel`）

- 呼叫 `cancel_reset()`
    
- 設定 `reboot_lock = false`
    

#### `cmd_get_file_header`（`ls`）

- `header = 由 DTB 解析得到的 initrd_start
    
- `file_count = CpioGetFilesHeaderName(header)`
    
- 若 `file_count <= 0`：輸出 `"Not Find Any File."`
    

#### `cmd_get_file_context`（`cat`）

- 參數檢查：
    
    - `argc < 2`：輸出 `"Please Enter FileName"` 並 return
        
    - `argc != 2`：輸出 `"False Argument"` 並 return
        
- `header = 由 DTB 解析得到的 initrd_start
    
- `CpioGetFileContext(header, argv[1])` 若回傳 `false`：
    
    - 輸出 `"Cannot Find File"`

# 9. Mini UART 驅動程式(Mini UART Driver)
## `uart.h`

### 檔案定位

宣告 mini UART 的初始化與基本 I/O 介面，供 bootloader、kernel、shell 以及其他模組使用。

### 目前提供的功能（宣告）

- 初始化
    
    - `void uart_init();`：初始化 mini UART
        
- 送出（blocking）
    
    - `void uart_send(char c);`：送出單一字元
        
    - `void uart_puts(const char *s);`：送出字串（遇 `\n` 會額外送 `\r`）
        
    - `void uart_send_integer(int number);`：送出十進位整數字串
        
    - `void uart_send_decimal_part(int number, unsigned int digit);`：送出固定寬度十進位數字（常用於小數部分）
        
    - `void uart_send_hex(unsigned int number);`：送出 32-bit hex（固定 8 個 nibble）並換行
        
- 接收（blocking）
    
    - `char uart_recv();`：接收單一字元
        
    - `unsigned int uart_recv_uint();`：接收 4 bytes，組成 `unsigned int`（little-endian）
        
- 延遲
    
    - `void delay_cycles(unsigned int time);`：以 busy-loop + `nop` 延遲
        

---

## `uart.c`

### 檔案定位

實作 Raspberry Pi 3 的 **mini UART（AUX MU）** 初始化與 blocking I/O，並負責設定 GPIO14/15 為 mini UART 腳位。

### 相依性（就現況）

- `common.h`：使用 `MMIO_BASE`、`GPFSEL1/GPPUD/GPPUDCLK0` 等 GPIO MMIO 位址，以及 `mmio_read/mmio_write`
    
- `uart.h`：對外 API 宣告
    

---

### 目前提供的功能（實作）

#### 1) mini UART 寄存器與 bit mask 定義

- 以 `AUX_BASE (MMIO_BASE + 0x215000)` 推導出一組 AUX/MU 暫存器位址（`AUX_ENABLES`, `AUX_MU_IO_REG`, `AUX_MU_LSR_REG`, `AUX_MU_BAUD_REG` 等）。
    
- 定義初始化與狀態輪詢所需的 bit mask，例如：
    
    - 啟用 mini UART：`AUX_ENABLES_MASK`
        
    - 開關 TX/RX：`AUX_TX_RX_DISABLE_MASK` / `AUX_TX_RX_ENABLE_MASK`
        
    - 清 FIFO：`AUX_CLEAR_TX_RX_FIFO`
        
    - TX 可寫條件（LSR bit5）：`AUX_TX_FIFO_EMPTY (1<<5)`
        
    - RX 有資料條件（實作以 LSR bit0 判斷）：`AUX_RX_FIFO_EMPTY 0x01`（名稱如此，但實作是等 bit0 為 1 才讀）
        
    - 8-bit data：`AUX_MINI_UART_DATA_TYPE 3`
        
    - 115200 baud：`AUX_BAUD_RATE_115200 270`
        

#### 2) `void uart_init()`

**功能：初始化 mini UART 並設定 GPIO14/15 為 UART。**

目前流程（依程式碼順序）：

1. 啟用 mini UART（`AUX_ENABLES |= 1`）
    
2. 關閉 TX/RX（先停用收發以避免設定途中異常）
    
3. 關閉中斷（`AUX_MU_IER_REG` 相關位元清除）
    
4. 關閉 flow control（`AUX_MU_MCR_REG = 0`）
    
5. 清空 TX/RX FIFO（`AUX_MU_IIR_REG` 寫入 `AUX_CLEAR_TX_RX_FIFO`）
    
6. 設定資料格式為 8-bit（`AUX_MU_LCR_REG = 3`）
    
7. 設定 baud rate（`AUX_MU_BAUD_REG = 270`）
    
8. 設定 GPIO pull-up/down（以 `GPPUD/GPPUDCLK0` 的標準序列）：
    
    - `GPPUD=0`、delay 150 cycles
        
    - `GPPUDCLK0` 設定 GPIO14/15、delay 150 cycles
        
    - 清 `GPPUD`、清 `GPPUDCLK0`
        
9. 設定 GPIO14/15 為 ALT5（mini UART）：
    
    - 讀 `GPFSEL1` → 清除對應 bits → 設定 ALT5
        
10. 重新開啟 TX/RX（`AUX_MU_CNTL_REG |= 3`）
    

#### 3) `void uart_send(char c)`

**功能：blocking 傳送單一字元。**

- 輪詢 `AUX_MU_LSR_REG` 的 bit5（TX 可寫）直到可寫
    
- 將 `c` 寫入 `AUX_MU_IO_REG`
    

#### 4) `char uart_recv()`

**功能：blocking 接收單一字元。**

- 輪詢 `AUX_MU_LSR_REG` 的 bit0 直到條件成立才讀取（程式碼以 `AUX_RX_FIFO_EMPTY` 這個 mask 判斷）
    
- 讀 `AUX_MU_IO_REG` 並用 `AUX_CHAR_MASK (0xFF)` 取低 8 bits，回傳 `char`
    

> 現況注意：巨集名稱寫作 `AUX_RX_FIFO_EMPTY`，但實際等待條件是「LSR bit0 變成 1 才讀」；就現有程式碼行為而言，它是在等待「可讀」狀態。

#### 5) `void uart_puts(const char *s)`

**功能：輸出 C-string（blocking），並將 `\n` 轉成 `\r\n`。**

- 逐字送出直到 `'\0'`
    
- 若遇到 `'\n'`，會先送出 `'\r'` 再送出 `'\n'`
    

#### 6) `void uart_send_integer(int number)`

**功能：以十進位輸出整數（blocking）。**

- `number == 0` 時直接輸出 `'0'`
    
- 否則將每位數字（`number % 10`）逆序存入 buffer，再倒序輸出
    

> 現況限制：邏輯僅處理 `number > 0` 的拆解；若 `number < 0`，while 不會執行，將不會輸出負號或數字（以目前程式碼行為為準）。

#### 7) `void uart_send_decimal_part(int number, unsigned int digit_size)`

**功能：輸出固定寬度的十進位數字（常用於小數部分補零）。**

- 將 `number` 逐位拆解存入 buffer
    
- 若實際位數不足 `digit_size`，用 `'0'` 補到指定寬度
    
- 倒序輸出，確保輸出位數固定為 `digit_size`
    

> 現況行為：若 `number == 0`，會輸出 `digit_size` 個 `'0'`。

#### 8) `void uart_send_hex(unsigned int number)`

**功能：輸出 32-bit 十六進位（固定 8 個 hex digit，A–F 為大寫），並在結尾輸出換行。**

- 從最高 nibble（bit28..31）到最低 nibble（bit0..3）逐 nibble 取值輸出
    
- `>= 10` 輸出 `'A' + (n-10)`，否則輸出 `'0'+n`
    
- 最後呼叫 `uart_puts("\n")`
    

#### 9) `unsigned int uart_recv_uint()`

**功能：接收 4 bytes，組成 `unsigned int`（little-endian）。**

- loop 4 次：
    
    - `tmp = uart_recv()`
        
    - `size |= ((unsigned char)tmp) << (i*8)`
        
- 回傳 `size`
    

> 現況假設：註解指出「python 是 Little Endian」，因此此函式預期 host 端以 little-endian 傳送 32-bit 整數。

#### 10) `void delay_cycles(unsigned int time)`

**功能：以 `nop` busy-loop 延遲指定迭代數。**

# 10. 組合語言啟動程式 (Startup)
## `boot.S`

### 檔案定位

此檔案提供 **Bootloader 與 Kernel 共用**的 ARM64 啟動流程（startup code），負責：

- 接收 QEMU 透過 `x0` 傳入的 DTB 位址，並嘗試在後續流程中保留/傳遞
- relocation：必要時將程式搬移到 linker 指定位址
- 初始化 Stack Pointer（`sp`）
- 清空 `.bss`
- 跳轉到 C 語言入口 `kernel_main(void *dtb_addr)`（Bootloader / Kernel 皆以 `kernel_main` 作為 entry；Bootloader build 會保留 `dtb_addr`，並在跳轉到實際載入的 Kernel 時一併轉交）。
- 進入 idle loop 避免返回

### 內容概述

此檔案提供 AArch64 的 startup code，負責把「QEMU 交進來的執行環境」整理成可進入 C 程式的狀態，並在必要時做 relocation。

- **DTB 指標保存與轉交**
  - 進入 `_start` 時 `x0` 帶 DTB 位址；先存到 `x19`（跨子程序保存），在進入 `kernel_main()` 前再放回 `x0`。

- **Relocation（必要時）**
  - `relocate_kernel` 會比對：
    - runtime `_start` 位址（`adr x0, _start`）
    - linker 期望 `_start` 位址（`ldr x1, =_start`）
  - 若不同，會用 8-byte 為單位把 `[runtime _start .. __bss_end)` 複製到 linker 指定位址，最後 `br` 回 linker 的 `_start` 重新跑一次初始化流程。

- **Stack / BSS 初始化**
  - 設定 `sp = _stack_top`
  - 呼叫 `clear_bss(__bss_start, __bss_end)` 以 8-byte store 將 `.bss` 清 0

- **進入 C 入口與 idle**
  - `bl kernel_main` 後不應返回；若返回則進入 `idle_loop`，以 `wfe` 無限等待。

現況注意（就現況描述）：
- relocation 與 bss 清零皆以 8-byte 為單位操作；隱含 `.bss` 邊界與複製範圍具備 8-byte 對齊/整除的假設（通常由 linker script 保障）。
### 入口流程（`_start`）

`_start` 的主要步驟如下（依程式碼順序）：

1. **保留 DTB 位址**
   - 進入 `_start` 時，`x0` 由 QEMU 放入 DTB 位址
   - 先以 `mov x19, x0` 將 DTB 暫存到 `x19`（`x19` 屬於 callee-saved，適合作為跨呼叫保存用） :contentReference[oaicite:0]{index=0}

2. **呼叫 relocation**
   - `bl relocate_kernel` :contentReference[oaicite:1]{index=1}

3. **設定 Stack Pointer**
   - `ldr x3, =_stack_top` → `mov sp, x3` :contentReference[oaicite:2]{index=2}

4. **清空 `.bss`**
   - `x0 = __bss_start`, `x1 = __bss_end`
   - `bl clear_bss` :contentReference[oaicite:3]{index=3}

5. **在呼叫 C 入口前，把 DTB 放回 `x0`**
   - `mov x0, x19`
   - `bl kernel_main` :contentReference[oaicite:4]{index=4}

6. **避免返回**
   - `b idle_loop`，用 `wfe` 無限等待 :contentReference[oaicite:5]{index=5} :contentReference[oaicite:6]{index=6}

---

### 子程序說明

#### 1) `relocate_kernel`

**目的：** 如果「程式實際載入位址」與「linker 設定位址」不同，就把程式搬到 linker 指定位址，最後跳回 `_start` 重新走一次流程。

- 取得目前執行中的 `_start` 位址（runtime）：
  - `adr x0, _start` :contentReference[oaicite:7]{index=7}
- 取得 linker 指定的 `_start` 位址（linked）：
  - `ldr x1, =_start` :contentReference[oaicite:8]{index=8}
- 若相同（不需搬移）：
  - `beq _done` → `ret` :contentReference[oaicite:9]{index=9} :contentReference[oaicite:10]{index=10}
- 若不同（需要搬移）：
  - 以 `__bss_end - linked _start` 計算搬移長度到 `x2`
  - 以 8 bytes 為單位 `ldr/str` 搬移
  - 搬完後 `br x1` 跳到 linker 設定的 `_start` :contentReference[oaicite:11]{index=11}

> 就現有行為來看：複製範圍以 `linked _start` 到 `__bss_end` 為界，屬於「把程式與資料搬到 linker 指定位置」的做法；`.bss` 的初始化不由此函式負責（`.bss` 由 `clear_bss` 另行清零）。

#### 2) `clear_bss`

**目的：清零 `__bss_start` 到 `__bss_end`。**

- 參數約定：
  - `x0 = start`, `x1 = end` :contentReference[oaicite:12]{index=12}
- 實作：
  - `mov x2, x0` 用 `x2` 當迴圈指標 :contentReference[oaicite:13]{index=13}
  - 迴圈中使用 `str xzr, [x2], #8` 以 8 bytes 為步進清零 :contentReference[oaicite:14]{index=14}
  - 結束後 `ret` :contentReference[oaicite:15]{index=15}

#### 3) `idle_loop`

- `wfe`（等待事件）後無限迴圈，避免落入未知區域 :contentReference[oaicite:16]{index=16}

---

### 目前的限制/假設（就現況描述）
- `.bss` 清零以 8 bytes 步進，隱含假設 `__bss_start`/`__bss_end` 至少對齊到 8（而 linker script 通常會用 `ALIGN` 保障）。
- relocation 以 8 bytes 複製；若長度非 8 的倍數，現況未見額外尾端處理（多半仰賴 linker 對齊策略）。

- **DTB 傳遞在 relocation 分支目前不完整：**
  - `_start` 會先把 DTB 存在 `x19`，並在 `bl kernel_main` 前用 `mov x0, x19` 放回 `x0`（這段是正確 handoff） :contentReference[oaicite:17]{index=17}
  - 但若發生 relocation，`_relocate_loop_done` 直接 `br x1` 跳回 `_start`，沒有在跳轉前把 `x0` 還原成 DTB；因此重新進 `_start` 時，`x0` 可能不是 DTB，導致 DTB 在 relocation 情境下遺失 :contentReference[oaicite:18]{index=18}


# 11. 核心載入器邏輯(Kernel Loader Logic)
## `bootloader_main.c`

### 檔案定位

Bootloader 的主流程（以 `kernel_main(void *dtb_addr)` 為入口）：透過 UART 與 host 溝通，接收 kernel size 與 kernel image，將 kernel 寫入 `KERNEL_LOAD_ADDRESS` 後跳轉執行。

### 相依性（就現況）

- `../header/common.h`
  - 使用：`KERNEL_LOAD_ADDRESS`
- `../header/uart.h`
  - 使用：`uart_init()`, `uart_puts()`, `uart_recv_uint()`, `uart_send_hex()`, `uart_recv()`

### 目前提供的功能（實作）

#### `void kernel_main(void *dtb_addr)`

1. **初始化 UART**
   - 呼叫 `uart_init()`，確保 bootloader 能與 host 端通訊。

2. **握手/提示輸出**
   - 輸出：`"\r\nOSDI: Ready\r\n"`
   - 輸出：`"Bootloader: Waiting for Kernel size..."`

3. **接收 kernel size（4 bytes）**
   - `unsigned int size = uart_recv_uint();`
   - `uart_send_hex(size);`（以 hex 回送 size）

4. **提示開始接收 kernel**
   - 輸出：`"Bootloader: Waiting for Loding Kernel..."`

5. **接收 kernel image 並寫入固定載入位址**
   - `char* kernel_code = (char*)KERNEL_LOAD_ADDRESS;`
   - 迴圈 `i = 0..size-1`：
     - `c = uart_recv();`
     - `*kernel_code = c; kernel_code++;`

6. **跳轉到 kernel entry**
   - 目前用：
     - `((void (*)(void))KERNEL_LOAD_ADDRESS)(dtb_addr);`
   - 意味著「跳入 kernel entry 時，`x0` 仍帶著 DTB 位址」。

### 目前的限制/假設（就現況描述）

- 假設 host 端會依序送入：
  1. `uart_recv_uint()` 可解析的 4 bytes size（依 `uart_recv_uint()` 的實作行為）
  2. 緊接著送出 `size` bytes 的 kernel image
- 此流程沒有檔案完整性檢查（checksum）、timeout、或錯誤復原；現況是「收到多少寫多少，寫完就跳」。
# 12. 作業系統核心主程式(Kernel Main)
## `kernel_main.c`

### 檔案定位

Kernel 入口：初始化 UART、輸出歡迎訊息，並進入 shell 互動主程式。

### 相依性（就現況）

- `../header/uart.h`（你先前那版）
    
    - 使用：`uart_init()`, `uart_puts()`
        
- `../header/shell.h`
    
    - 使用：`shell_main()`
        

### 目前提供的功能（實作）

#### `void kernel_main(void* dtb_addr)`
> `dtb_addr` 由啟動程式 `boot.S`（以及 Bootloader 的 jump）透過 `x0` 轉交進來。

1. **重新初始化 UART**
    
    - 註解表達「硬體已經開了，但為了保險重新設定一次」。
        
    - 呼叫 `uart_init()`。
        
2. **輸出歡迎訊息**
    
    - `uart_puts("\r\nWelcome to OSDI\r\n");`
        
3. **進入 shell**
    
    - 呼叫 `shell_main()` 以獲取使用者輸入並處理指令。
        

### 目前的限制/假設（就現況描述）

- `kernel_main()` 目前只做 UART + shell，未在此檔案內進行其他子系統初始化（例如中斷、記憶體管理、例外向量表設定等）。

# 13. Python 傳輸腳本 (Python Serial Script)
## `Python/send_kernel.py`

### 檔案定位

Host 端的 **Kernel Loader + 簡易終端機**工具：  
透過 `socket://127.0.0.1:8888` 連到 QEMU 的 serial server，等待 Bootloader 輸出就緒字串後，依協定送出 `build/kernel8.img` 的大小與內容，最後進入互動模式，將使用者輸入轉送到裝置端並顯示裝置端輸出。

---

### 相依性（就現況）

- Python modules
    
    - `serial`（pyserial）：使用 `serial.serial_for_url()` 以 socket URL 方式連線
        
    - `os`：使用 `os.stat()` 取得 `kernel8.img` 檔案大小
        
    - `struct`：使用 `struct.pack('<I', file_size)` 將大小打包成 little-endian uint32
        
    - `sys`：讀寫 stdin、以及程式結束
        
    - `select`：互動模式下做 I/O multiplexing
        
    - `time`：目前僅被 import；程式內沒有實際使用（sleep 註解掉）
        
- 外部環境假設（就現況）
    
    - QEMU serial server：`127.0.0.1:8888`
    - Kernel image 路徑固定：`build/kernel8.img`


---

### 目前提供的功能（實作）

#### 1) 連線到 QEMU Serial

- 啟動時嘗試：
    
    - `serial.serial_for_url('socket://127.0.0.1:8888', 115200)`
        
- 若連線失敗：
    
    - 印出 `Failed to connect to QEMU: ...` 並 `sys.exit(1)`
        

---

#### 2) 等待 Bootloader Ready（握手）

- 印出 `Listening for Bootloader...`
    
- 進入迴圈，持續：
    
    - `raw = ser.readline()`
        
    - `line = raw.decode('utf-8', errors='ignore')`
        
    - 若 `line` 存在則印出：`[Device]: <line>`
        
    - 若 `line` 內包含 `"OSDI: Ready"`：
        
        - 印出 `-> Device is ready! Starting transmission.`
            
        - 結束等待，進入傳輸流程
            

> 現況：程式碼中有「Timeout 機制」註解，但 `time.sleep(0.1)` 目前被註解掉，因此現行行為是持續輪詢等待裝置輸出。

---

#### 3) 傳送 kernel size（Little Endian uint32）

- 固定使用：
    
    - `kernel_path = 'build/kernel8.img'`
        
- 取得檔案大小：
    
    - `file_size = os.stat(kernel_path).st_size`
        
- 打包成 4 bytes header：
    
    - `header = struct.pack('<I', file_size)`（little-endian）
        
- 傳送 header：
    
    - `ser.write(header)`
        
- 顯示傳送資訊：
    
    - `Sending kernel size: <file_size> bytes`
        

---

#### 4) 讀取裝置端回覆（確認 size 已送出）

- 送出 size 後，讀取一行裝置回覆：
    
    - `response = ser.readline().decode('utf-8', errors='ignore').strip()`
        
- 印出：
    
    - `Size Sent. Device replied: <response>`
        

> 現況：註解提到若裝置端 `uart_send_hex` 沒有換行可能會黏在一起；程式仍會照 `readline()` 行為等待行結尾。

---

#### 5) 傳送 kernel image 內容

- 以 binary mode 讀取整個檔案並一次寫出：
    
    - `with open(kernel_path, 'rb') as f: ser.write(f.read())`
        
- 印出：
    
    - `Sending kernel image...`
        
    - `Kernel image sent successfully.`
        

---

#### 6) 互動模式（簡易 Terminal Emulator）

- 進入互動模式後，使用：
    
    - `select.select([ser.fileno(), sys.stdin.fileno()], [], [])`
        
    - 同時監聽：
        
        - **裝置端輸出**（socket/serial）
            
        - **使用者輸入**（stdin）
            
- 若是裝置端可讀：
    
    - `data = ser.read(ser.in_waiting or 1)`
        
    - 若有資料：
        
        - `print(data.decode('utf-8', errors='ignore'), end='', flush=True)`
            
    - 若沒資料（視為連線關閉）：
        
        - 印出 `[Connection closed by device]` 並 `sys.exit(0)`
            
- 若是 stdin 可讀：
    
    - `user_input = sys.stdin.readline()`
        
    - `ser.write(user_input.encode())`（以預設編碼轉 bytes 後送出）
        

---

#### 7) 結束處理

- `KeyboardInterrupt`（Ctrl+C）：
    
    - 印出 `Exiting...`
        
- 其他例外：
    
    - 印出 `[Error]: ...`
        
- 最後一定執行：
    
    - `ser.close()`
        

---

### 目前的限制/假設（就現況描述）

- Kernel 路徑固定為 `build/kernel8.img`：若檔案不存在或路徑不同會直接拋例外（目前未額外處理）。
    
- 傳輸協定固定：
    
    1. 等待 `"OSDI: Ready"`
        
    2. 傳 4-byte little-endian size
        
    3. 傳送 `file_size` bytes payload
        
- `retry_count / max_retries` 目前僅宣告，未被使用；現況不會在等待階段做重試次數上限控制。
    
- 互動模式以「行」讀 stdin（`readline()`），因此送到裝置端的輸入以「每行」為單位。
# 14. 自動化編譯與建置設定(Build Automation)
## `Makefile`

### 檔案定位

提供整個專案的建置流程與 QEMU 執行入口，現況特點是：

- 使用交叉工具鏈編譯 AArch64
    
- 產出兩個映像：
    
    - `build/bootloader.img`（由 `bootloader_main.c` 連結而成）
        
    - `build/kernel8.img`（由 `kernel_main.c` 連結而成）
        
- QEMU 以 `-kernel $(IMG_BOOT)`（現況為 `build/bootloader.img`）啟動，並以 `-initrd initramfs.cpio` 提供 initramfs
    

### 目前提供的功能（內容）

#### 1) 工具鏈與基本變數

- `CROSS_COMPILE ?= aarch64-linux-gnu-`
    
- `CC/LD/OBJCOPY/QEMU` 皆由 `CROSS_COMPILE` 推導
    
- 路徑：
    
    - `INC_DIR := header`
        
    - `BUILD_DIR := build`
        
- 編譯旗標（現況）：
    
    - `CFLAGS := -Wall -O0 -ffreestanding -nostdlib -nostartfiles -g -mgeneral-regs-only -I$(INC_DIR)`
        
    - `ASFLAGS := -Wall -O0 -ffreestanding -nostdlib -nostartfiles -g`
        

#### 2) 原始碼收集與拆分

- 指定兩個「主檔」：
    
    - `C_BOOT_SRC := CFile/bootloader_main.c`
        
    - `C_KERNEL_SRC := CFile/kernel_main.c`
        
- 收集所有 C 與 Assembly：
    
    - `C_ALL := $(wildcard CFile/*.c)`
        
    - `S_ALL := $(wildcard Assembly/*.S)`
        
    - `s_ALL := $(wildcard Assembly/*.s)`
        
- 將其餘 C 檔視為共用檔：
    
    - `C_COMMON := $(filter-out $(C_BOOT_SRC) $(C_KERNEL_SRC), $(C_ALL))`
        

#### 3) object 清單與「boot.o 需置前」

- 共用 C object：
    
    - `OBJ_COMMON_C := ...`（將 `C_COMMON` 映射到 `build/*.o`）
        
- 共用 Assembly object（排除 `Assembly/boot.S`，因為 `boot.o` 需特別置前）：
    
    - `OBJ_ASM := ... $(filter-out Assembly/boot.S, $(S_ALL)) ... + (s_ALL)`
        
- 定義 `BOOT_START_OBJ := $(BUILD_DIR)/boot.o`
    
- 最終兩組連結 object（現況邏輯）：
    
    - `OBJS_FOR_BOOTLOADER := boot.o + 共用.o + bootloader_main.o`
        
    - `OBJS_FOR_KERNEL := boot.o + 共用.o + kernel_main.o`
        

#### 4) 產出檔案命名與 all/clean

- 輸出：
    
    - `IMG_BOOT := build/bootloader.img`
        
    - `ELF_BOOT := build/bootloader.elf`
        
    - `IMG_KERNEL := build/kernel8.img`
        
    - `ELF_KERNEL := build/kernel8.elf`
        
- `all`: 同時產出兩個 img
    
- `clean`: `rm -rf build`
    

#### 5) 連結與 objcopy（產出 .elf / .img）

- Bootloader：
    
    - `ld -T linker_boot.ld -o build/bootloader.elf $(OBJS_FOR_BOOTLOADER)`
    
	- `objcopy -O binary build/bootloader.elf build/bootloader.img`
        
- Kernel：
    
    - `ld -T linker_kernel.ld -o build/kernel8.elf $(OBJS_FOR_KERNEL)`
    
	- `objcopy -O binary build/kernel8.elf build/kernel8.img`
        

#### 6) 編譯規則

- `CFile/%.c -> build/%.o`：使用 `$(CC) $(CFLAGS) -c`
    
- `Assembly/%.S -> build/%.o`：使用 `$(CC) $(ASFLAGS) -c`
    
- `Assembly/%.s -> build/%.o`：使用 `$(CC) $(ASFLAGS) -c`
    

#### 7) QEMU 執行目標

- `qemu`：以 raspi3b 機器啟動，kernel 指向 `$(IMG_BOOT)`（`build/bootloader.img`），並掛載 initramfs 與 serial tcp：
    
    - `-machine raspi3b`
        
    - `-kernel $(IMG_BOOT)`
        
    - `-initrd initramfs.cpio`
        
    - `-display none`
        
    - `-serial null -serial tcp:127.0.0.1:8888,server`
        
- `qemu-gdb`：同上，但加入：
    
    - `-S -s`（等待 gdb 連線、開啟 gdb server）
        
    - serial tcp 使用 `server,nowait`
        

### 目前的限制/假設（就現況描述）

- Makefile 假設目錄結構存在：
    
    - `CFile/`, `Assembly/`, `header/`
        
- QEMU 的 `-kernel` 只載入 `$(IMG_BOOT)`（`build/bootloader.img`）；`build/kernel8.img` 的存在主要供 host 工具透過 UART 傳送，或供你在其他流程使用（Makefile 本身僅負責把它建出來）。
    
- `-initrd initramfs.cpio` 固定使用該檔名；現況未提供自動生成/打包 initramfs 的目標（純使用既有檔案）。

# 15. 連結腳本 (Linker Scripts)
## `linker_boot.ld`

### 檔案定位

Bootloader 的 linker script：定義 bootloader 的 link address、section 佈局、`.bss` 邊界符號與 stack 位置，供 `boot.S` 與 C 程式使用。

### 目前提供的功能（內容）

- `ENTRY(_start)`：指定 entry symbol
    
- 設定 link address：
    
    - `. = 0x60000;`
        
- section 佈局（依序）：
    
    - `.text`, `.rodata`, `.data`
        
- `.bss` 佈局與符號：
    
    - `. = ALIGN(16);`
        
    - `__bss_start = .;`
        
    - `.bss : {*(.bss .bss.*) *(COMMON)}`
        
    - `__bss_end = .;`
        
- stack 位置：
    
    - `. = ALIGN(16);`
        
    - `_stack_top = __bss_end + 0x04000;`（stack 大小配置為 0x4000 bytes）
        

### 目前的限制/假設（就現況描述）

- `boot.S` 依賴 `_stack_top/__bss_start/__bss_end`；此腳本已提供。
    
- 此腳本未定義 heap/其他記憶體區段；現況主要滿足 early boot、BSS、stack。
    

---

## `linker_kernel.ld`

### 檔案定位

Kernel 的 linker script：定義 kernel 的 link address、section 佈局、`.bss` 邊界符號與 stack 位置，供 `boot.S` 與 kernel C 程式使用。

### 目前提供的功能（內容）

- `ENTRY(_start)`：指定 entry symbol
    
- 設定 link address：
    
    - `. = 0x80000;`
        
- section 佈局（依序）：
    
    - `.text`, `.rodata`, `.data`
        
- `.bss` 佈局與符號：
    
    - `. = ALIGN(16);`
        
    - `__bss_start = .;`
        
    - `.bss : {*(.bss .bss.*) *(COMMON)}`
        
    - `__bss_end = .;`
        
- stack 位置：
    
    - `. = ALIGN(16);`
        
    - `_stack_top = __bss_end + 0x04000;`
        

### 目前的限制/假設（就現況描述）

- 同 `linker_boot.ld`：主要支援 early boot、BSS、stack，未額外規劃 heap/更細緻的記憶體映射。

# 16. VSCode設定 (VS Code Configuration)

## launch.json

### 檔案定位

VS Code 的 **Debug 設定檔**：定義可在 VS Code 內啟動/連線 GDB 的偵錯組態（C/C++ `cppdbg`），用於連線到 QEMU 的 GDB server 或其他本機 GDB session。

### 內容概述

此檔案定義 VS Code 的 `cppdbg` 偵錯組態，核心用途是：在 VS Code 內一鍵啟動/連線到 QEMU 的 GDB server，並自動載入 Bootloader 與 Kernel 的 symbols。

- 目前包含 2 個 configurations：

#### 1) `OSDI: Debug Lab2 (stable handshake)`

- **用途**：以 `gdb-multiarch` 連線到 `127.0.0.1:1234`，並透過 VS Code task 自動啟動/停止 QEMU。
- **symbols/ELF**：
  - `program: ${workspaceFolder}/build/bootloader.elf`
  - `add-symbol-file ${workspaceFolder}/build/kernel8.elf 0x80000`（將 kernel symbols 映射到 0x80000）
- **task 串接**：
  - `preLaunchTask: Start QEMU (GDB Mode)`
  - `postDebugTask: Stop QEMU`
- **GDB 初始化**（`setupCommands`）：
  - `set architecture aarch64`
  - `break kernel_main`（允許失敗）
  - `add-symbol-file ... 0x80000`

#### 2) `C/C++ Runner: Debug Session`

- **用途**：看起來是 VS Code extension 自動生成的偵錯組態（非本專案主流程）。
- **現況特徵**：
  - `cwd` / `program` 使用絕對路徑（`/home/marginal/...`），在不同機器/不同 workspace 下通常不可直接使用。
### 目前提供的功能（現有組態）

此檔案目前包含 **2 個 debug configuration**：

#### 1) `OSDI: Debug Lab2 (stable handshake)`

- **用途**：用 `gdb-multiarch` 連到 `127.0.0.1:1234` 的 GDB server，並在啟動/結束除錯流程時自動呼叫 VS Code task 來啟動/停止 QEMU。
- 主要欄位（就現況）：
    
    - `type: "cppdbg"`、`request: "launch"`、`MIMode: "gdb"`
    - `program: "${workspaceFolder}/build/bootloader.elf"`：symbols/ELF 來源指定為 workspace 下的 `build/bootloader.elf`
    - `miDebuggerPath: "/usr/bin/gdb-multiarch"`
    - `miDebuggerServerAddress: "127.0.0.1:1234"`
    - `preLaunchTask: "Start QEMU (GDB Mode)"`：開始除錯前先啟動 QEMU（含建置與等待 port ready 的流程，詳見 `tasks.json`）
    - `postDebugTask: "Stop QEMU"`：結束除錯後清掉 QEMU process
    - `stopAtConnect: true`：連上 GDB server 後先停住（便於確認符號載入與中斷點是否生效）
    - `setupCommands`（啟動後自動送進 gdb）：
        
        - `set architecture aarch64`：固定 target 架構為 AArch64
        - `break kernel_main`：預先掛上 `kernel_main` 中斷點（若符號尚未就緒則允許失敗）
        - `add-symbol-file ${workspaceFolder}/build/kernel8.elf 0x80000`：把 kernel 的 symbols 對應到 `0x80000`（便於進 kernel 後能正確看回呼叫堆疊/對應原始碼）
#### 2) `C/C++ Runner: Debug Session`

- **用途**：另一個 `cppdbg` 的 debug session（看起來是由某個 VS Code extension/工具自動生成的組態）。
- 主要特徵（就現況）：
    
    - 使用 **絕對路徑**：`cwd` 與 `program` 直接寫死在 `/home/marginal/...`
    - `miDebuggerPath: "gdb"`，並僅做 `-enable-pretty-printing`

### 目前的限制/假設（就現況描述）

- `OSDI: Debug Lab2 (stable handshake)` 依賴本機存在 `/usr/bin/gdb-multiarch`，且 QEMU 有開啟並監聽 `127.0.0.1:1234`（此部分會透過 `preLaunchTask` 觸發 `tasks.json` 來啟動）。
- `C/C++ Runner: Debug Session` 使用**絕對路徑**（`/home/marginal/...`）；在不同機器/不同 workspace 路徑下不一定可直接使用（這是目前檔案中的狀態）。

---
## `tasks.json`

### 檔案定位
VS Code 的 **Task 設定檔**：定義可在 VS Code 內執行的建置與啟動命令（build、啟動/停止 QEMU 等），並可提供給 debug pre-launch / post-debug 依賴使用。
### 內容概述

此檔案定義 VS Code tasks，提供「建置」與「啟動/停止 QEMU（含 GDB 模式）」的標準化入口，並供 `launch.json` 的 `preLaunchTask/postDebugTask` 串接使用。

- 目前定義 3 個 tasks：

#### 1) `Build All`
- **用途**：呼叫 `make all` 進行全量建置（預設 build task）。

#### 2) `Start QEMU (GDB Mode)`
- **用途**：在背景啟動 `make qemu-gdb`，並等待除錯所需的 port ready，以提升 attach 穩定性。
- **流程摘要（就現況腳本）**：
  1. `set -e`：任一步驟失敗即退出
  2. `pkill -9 qemu-system-aarch64 || true`：先清理舊 QEMU
  3. `mkdir -p build`，並將 QEMU 啟動輸出導到 `build/qemu-gdb.log`
  4. echo `__QEMU_START__` / `__QEMU_READY__` 作為 background task 的開始/就緒 marker
  5. 透過 `/dev/tcp/127.0.0.1/$p` 輪詢等待：
     - `1234`（GDB server）
     - `8888`（UART/serial 轉發端口）
  6. `wait $QPID`：讓 task 的背景程序生命週期與 QEMU 綁定（QEMU 結束 task 才結束）

#### 3) `Stop QEMU`
- **用途**：結束除錯後清理 QEMU process（`pkill -9 qemu-system-aarch64 || true`）。
### 目前提供的功能（現有 tasks）

此檔案目前定義 **3 個 tasks**：
#### 1) `Build All`

- **用途**：呼叫 Makefile 進行全量建置。
- 內容（就現況）：
    - `command: "make"`、`args: ["all"]`
    - `group.kind: "build"` 且 `isDefault: true`（預設 build task）
    - `problemMatcher: ["$gcc"]`（使用 VS Code 內建 gcc matcher 顯示編譯錯誤/警告）
#### 2) `Start QEMU (GDB Mode)`

- **用途**：啟動 QEMU 的 gdb 模式（背景執行），並在啟動前清掉舊的 QEMU；同時等待除錯所需的 port ready，讓後續 debug attach 更穩定。
- 內容（就現況）：
    - `isBackground: true`（視為背景任務）
    - `command: "bash"` + `args: ["-lc", "..."]`：以 bash 執行一段整合腳本
    - 腳本行為（摘要）：
        
        1. `pkill -9 qemu-system-aarch64 || true`：強制終止舊的 QEMU（若不存在則忽略錯誤）
        2. `(make qemu-gdb > build/qemu-gdb.log 2>&1) &`：背景啟動 `make qemu-gdb`，並把輸出寫到 `build/qemu-gdb.log`
        3. 以 `/dev/tcp/127.0.0.1/$p` 輪詢等待 `1234` 與 `8888` port ready；未 ready 則直接失敗退出
    - `problemMatcher.background`：
        - `beginsPattern: "^__QEMU_START__$"`、`endsPattern: "^__QEMU_READY__$"`：用明確 marker 讓 VS Code 判定「背景任務已可供後續 debug attach 使用」
    - `dependsOn: "Build All"`：啟動 QEMU 前會先完成建置

#### 3) `Stop QEMU`

- **用途**：結束除錯後清掉 QEMU process（對應 `launch.json` 的 `postDebugTask`）。
- 內容（就現況）：`pkill -9 qemu-system-aarch64 || true`

### 目前的限制/假設（就現況描述）

- `Start QEMU (GDB Mode)` 假設環境中存在：
    - `bash`，且支援 `/dev/tcp/...` 這種 TCP 輪詢寫法（常見於 Linux bash）
    - `pkill`、`make`，以及 Makefile 提供 `qemu-gdb` 目標
- 若 `1234` 或 `8888` 無法在指定時間內 ready，task 會直接失敗（屬於目前腳本設計）。

---
## `settings.json`

### 檔案定位

VS Code 的工作區設定檔：此檔案目前主要是針對 **`C_Cpp_Runner`**（一個 VS Code extension）設定編譯器、除錯器、警告旗標、搜尋排除等行為。

settings

### 目前提供的功能（現有設定）

- 編譯器/除錯器路徑（就現況）：
    - `C_Cpp_Runner.cCompilerPath: "gcc"`
    - `C_Cpp_Runner.cppCompilerPath: "g++"`
    - `C_Cpp_Runner.debuggerPath: "gdb"`
- 警告旗標已啟用，並列出一組偏嚴格的 warning 組合（如 `-Wall/-Wextra/-Wpedantic/...`）。
- 搜尋排除路徑（避免掃到 build 與隱藏資料夾、`.vscode` 等）。
- Sanitizer / LTO / compilation time 等選項目前皆停用。

### 目前的限制/假設（就現況描述）

- 以上設定主要影響 `C_Cpp_Runner` extension 的行為；是否影響你的 Makefile 建置流程，取決於你實際採用的 build/launch 方式（此檔案目前未直接改動 Makefile）。

---
## `c_cpp_properties.json`

### 檔案定位

VS Code **C/C++（Microsoft C/C++ extension）**的 IntelliSense/語意分析設定檔，主要用來指定 include 搜尋路徑、compiler 路徑、IntelliSense 模式與語言標準。
### 目前提供的功能（現有設定）
此檔案目前包含 **1 個 configuration**：
#### Configuration：`linux-gcc-x64`
- `includePath: ["${workspaceFolder}/**", "/usr/include", "/usr/local/include"]`
- `compilerPath: "/usr/bin/gcc"`
- `intelliSenseMode: "linux-gcc-x64"`
- `cStandard/cppStandard` 目前使用 `${default}`；`compilerArgs` 目前等同無額外參數（只有空字串）。
### 目前的限制/假設（就現況描述）

- 此組態的 `intelliSenseMode` 為 `linux-gcc-x64`，且 `compilerPath` 指向 `/usr/bin/gcc`；若你的實際 target 為 AArch64/bare-metal，IntelliSense 的內建巨集/型別模型可能與真實編譯環境不完全一致（這是目前檔案內容所呈現的狀態）。
