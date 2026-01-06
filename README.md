OSDI Lab Development Progress Tracker
Current Lab: Lab 2 - Booting Target Platform: Raspberry Pi 3 B+ (AArch64) Environment: WSL (Ubuntu) + QEMU + GDB

# 0. 系統總覽 (System Overview)

## 0.1 目前可做什麼（Overview / What you can do now）

- 目前可在 QEMU 上啟動 Bootloader，Bootloader 會先透過 UART 對 Host 輸出 `OSDI: Ready`，表示已就緒並等待載入 Kernel。
- Host 端執行 `Python/send_kernel.py` 連線到 QEMU Serial Port（127.0.0.1:8888），並依協定送出 Kernel Size（Little Endian）與 `kernel8.img` 內容。
- Bootloader 會把 Kernel 寫入 `KERNEL_LOAD_ADDRESS (0x80000)`，再以 Function Pointer 方式跳轉到 Kernel Entry Point。
- Kernel 啟動後會再次初始化 UART，印出 `Welcome to OSDI`，接著呼叫 `shell_main()` 進入互動模式。
- 整體資料流為：使用make qemu-gdb 編譯檔案 → Host（send_kernel.py） → Bootloader（UART handshake + load） → Kernel（print + init） → Shell（互動輸入輸出）。

## 0.2 建置與執行流程（Build & Run Pipeline）

- Makefile：設定 aarch64-linux-gnu- 交叉編譯工具鏈，並將 Bootloader / Kernel 分開編譯成兩個 Image（bootloader.img、kernel8.img）。
- Makefile Targets：提供 `make qemu` 啟動 QEMU 模擬；`make qemu-gdb` 啟動 QEMU 並開啟 GDB Server（:1234）與 Serial Server（:8888）。
- Linker Scripts：
  - `linker_boot.ld`：設定 Bootloader Entry Point 為 0x60000，並定義 BSS 與 Stack Top。
  - `linker_kernel.ld`：設定 Kernel Entry Point 為 0x80000，並定義 BSS 與 Stack Top。
- VS Code：
  - `tasks.json`：自動化 `make all` 與啟動 `make qemu-gdb`（並加上 pkill 避免舊 QEMU 佔用 Port）。
  - `launch.json`：設定 GDB 連線至 :1234，並載入 `bootloader.elf` 符號表以便除錯。

## 0.3 開機交棒流程（Boot Chain）

- `Assembly/boot.S`：
  - relocate：將程式碼從載入位址搬移至 Linker 設定位址（Bootloader 為 0x60000）。
  - clear bss：清空 BSS，確保全域變數初始值為 0。
  - set sp：設定 Stack Pointer。
  - handoff：跳轉至 C 語言入口（kernel_main 或 bootloader_main）。
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
  - loader protocol：等待 `OSDI: Ready` → 傳送 Kernel Size（Little Endian）→ 傳送 `kernel8.img` 檔案內容。
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

宣告專案自製的字串函式介面（避免依賴 libc）。

### 目前提供的功能（宣告）

- `int strcmp(const char *s1, const char *s2);`
    
- `int strncmp(const char *s1, const char *s2, unsigned long read_byte);`
    
- `unsigned long strlen(const char *s);`
    

---

## `string.c`

### 檔案定位

實作 `string.h` 宣告的字串函式，並使用 `common.h` 的 `MAX_STRING_SIZE` 作為 `strlen` 的掃描上限。

### 目前提供的功能（實作）

#### `strcmp(const char* s1, const char* s2)`

- 逐 byte 比較 `s1` 與 `s2`，直到：
    
    - 任一字元為 `'\0'`，或
        
    - 發現不同字元
        
- 回傳：`*(unsigned char *)s1 - *(unsigned char *)s2`  
    （用 `unsigned char` 避免符號位造成的差值異常）
    

#### `strncmp(const char *s1, const char *s2, unsigned long read_byte)`

- 最多比較 `read_byte` 次：
    
    - 若遇到不同字元 → 跳出迴圈
        
    - 若遇到 `'\0'` → 立即回傳 `0`
        
- 迴圈結束後回傳：`(unsigned char)s1_tmp - (unsigned char)s2_tmp`
    

> 現況注意：當 `read_byte == 0` 時，for-loop 不會執行，`s1_tmp/s2_tmp` 未被初始化就被拿來回傳差值，行為在 C 語言層面屬未定義（這是「目前程式碼實際狀態」）。

#### `strlen(const char *s)`

- 目的：計算字串長度，但**最多掃描到 `MAX_STRING_SIZE`（1024）**。
    
- 行為流程（就目前實作）：
    
    1. `s == NULL` 時回傳 `0`
        
    2. 先以 byte 方式前進到 8-byte 對齊邊界（中途若遇到 `'\0'` 直接回傳）
        
    3. 之後以 64-bit（`unsigned long`）為單位掃描，使用 magic number 偵測 word 內是否含 `'\0'`
        
    4. 若偵測到可能含 `'\0'`，回到 byte 模式逐字確認並回傳
        
    5. 若掃描到 `MAX_STRING_SIZE` 仍未遇到 `'\0'`，回傳當前累積的 `length`（最大不超過 `MAX_STRING_SIZE`）
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

- `header = (void*)FILE_HEADER`
    
- `file_count = CpioGetFilesHeaderName(header)`
    
- 若 `file_count <= 0`：輸出 `"Not Find Any File."`
    

#### `cmd_get_file_context`（`cat`）

- 參數檢查：
    
    - `argc < 2`：輸出 `"Please Enter FileName"` 並 return
        
    - `argc != 2`：輸出 `"False Argument"` 並 return
        
- `header = (void*)FILE_HEADER`
    
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

AArch64 的 early boot 程式（提供 `_start`）：負責  
-（必要時）將自身程式碼 **self-relocation** 到 linker 指定的 `_start` 位置

- 設定 stack pointer
    
- 清零 `.bss`
    
- 呼叫 C 入口 `kernel_main()`
    
- 返回後進入 idle loop
    

> 同一份 `boot.S` 同時被 bootloader 與 kernel 的連結流程使用（Makefile 會把 `boot.o` 放在兩邊的 object 清單最前面）；其最終的 link address 由各自的 linker script 決定。

### 相依性（就現況）

- 連結腳本提供的符號：
    
    - `_start`, `_stack_top`, `__bss_start`, `__bss_end`
        
- 外部 C 入口：
    
    - `kernel_main`（以 `.extern kernel_main` 宣告）
        

### 目前提供的功能（實作）

#### 1) `_start`

流程（依程式碼順序）：

1. `bl relocate_kernel`：進行 self-relocation（若目前執行位址與 link 位址不同）
    
2. `ldr x3, =_stack_top` / `mov sp, x3`：設定 stack pointer 到 `_stack_top`
    
3. `ldr x0, =__bss_start`、`ldr x1, =__bss_end`：準備清 BSS 的參數
    
4. `bl clear_bss`：清零 `.bss`（以 8 bytes 為步進）
    
5. `bl kernel_main`：進入 C 世界的主入口
    
6. `b idle_loop`：避免返回後跑飛
    

#### 2) `relocate_kernel`

**目的：若 `_start` 的「目前執行位址」不等於「link-time 位址」，則搬移一段記憶體並跳到 link 位址繼續執行。**

核心邏輯（依現有指令）：

- `adr x0, _start`：取得 `_start` 的**目前執行位址**
    
- `ldr x1, =_start`：取得 `_start` 的**link-time 位址**
    
- 若 `x0 == x1`：直接 `_done: ret`
    
- 否則：
    
    - `ldr x2, =__bss_end`
        
    - `sub x2, x2, x1`：以 `(__bss_end - linked _start)` 計算要複製的 bytes 範圍（以 8-byte 步進）
        
    - 迴圈 `_relocate_loop`：
        
        - 若 `x2 == 0` 結束
            
        - `ldr x3, [x0], #8` 從來源讀 8 bytes
            
        - `str x3, [x1], #8` 寫到目的地 8 bytes
            
        - `sub x2, x2, #8` 遞減剩餘長度
            
    - `_relocate_loop_done`：
        
        - `ldr x1, =_start`
            
        - `br x1`：跳到 link-time `_start` 重新執行
            

> 就現有行為來看：複製範圍以 `linked _start` 到 `__bss_end` 為界，屬於「把程式與資料搬到 linker 指定位置」的做法；是否會包含 `.bss` 的內容不由此函式初始化（`.bss` 由 `clear_bss` 另行清零）。

#### 3) `clear_bss`

**目的：清零 `__bss_start` 到 `__bss_end`。**

- 參數約定（依註解與實作）：
    
    - `x0 = start`, `x1 = end`
        
- 實作：
    
    - `mov x2, x0`：用 `x2` 當迴圈指標
        
    - `_bss_clear_loop`：
        
        - `cmp x2, x1`；若 `x2 >= x1` 跳 `_bss_clear_done`
            
        - `str xzr, [x2], #8`：寫入 0，指標加 8
            
        - 回到迴圈
            
    - `_bss_clear_done: ret`
        

#### 4) `idle_loop`

- `wfe`（等待事件）後無限迴圈，避免落入未知區域。
    

### 目前的限制/假設（就現況描述）

- `.bss` 清零以 8 bytes 步進，隱含假設 `__bss_start`/`__bss_end` 至少對齊到 8（而 linker script 以 `ALIGN(16)` 對齊）。
    
- relocation 以 8 bytes 複製，並以 `__bss_end - linked _start` 計算長度；長度若非 8 的倍數，現況未見額外尾端處理（以目前腳本對齊策略通常可避免）。

# 11. 核心載入器邏輯(Kernel Loader Logic)
## `bootloader_main.c`

### 檔案定位

Bootloader 的主流程（以 `kernel_main()` 為入口）：透過 UART 與 host 溝通，接收 kernel size 與 kernel image，將 kernel 寫入 `KERNEL_LOAD_ADDRESS` 後跳轉執行。

### 相依性（就現況）

- `../header/common.h`
    
    - 使用：`KERNEL_LOAD_ADDRESS`
        
- `../header/uart.h`（你先前那版）
    
    - 使用：`uart_init()`, `uart_puts()`, `uart_recv_uint()`, `uart_send_hex()`, `uart_recv()`
        

### 目前提供的功能（實作）

#### `void kernel_main(void)`

1. **初始化 UART**
    
    - 呼叫 `uart_init()`，確保 bootloader 能與 host 端通訊。
        
2. **握手/提示輸出**
    
    - 輸出：`"\r\nOSDI: Ready\r\n"`
        
    - 輸出：`"Bootloader: Waiting for Kernel size..."`
        
3. **接收 kernel size（4 bytes）**
    
    - `unsigned int size = uart_recv_uint();`
        
    - `uart_send_hex(size);`（以 hex 回送 size，並在該函式內部換行）
        
4. **提示開始接收 kernel**
    
    - 輸出：`"Bootloader: Waiting for Loding Kernel..."`
        
5. **接收 kernel image 並寫入固定載入位址**
    
    - `char* kernel_code = (char*)KERNEL_LOAD_ADDRESS;`
        
    - 迴圈 `i = 0..size-1`：
        
        - `c = uart_recv();`
            
        - `*kernel_code = c; kernel_code++;`
            
6. **跳轉到 kernel entry**
    
    - `((void (*)(void))KERNEL_LOAD_ADDRESS)();`
        

### 目前的限制/假設（就現況描述）

- 假設 host 端會依序送入：
    
    1. `uart_recv_uint()` 可解析的 4 bytes size（此檔案以 little-endian 的 `uart_recv_uint()` 行為為依據）
        
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

#### `void kernel_main(void)`

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
## `send_kernel.py`

### 檔案定位

Host 端的 **Kernel Loader + 簡易終端機**工具：  
透過 `socket://127.0.0.1:8888` 連到 QEMU 的 serial server，等待 Bootloader 輸出就緒字串後，依協定送出 `kernel8.img` 的大小與內容，最後進入互動模式，將使用者輸入轉送到裝置端並顯示裝置端輸出。

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
    
    - `bootloader.img`（由 `bootloader_main.c` 連結而成）
        
    - `kernel8.img`（由 `kernel_main.c` 連結而成）
        
- QEMU 以 `-kernel bootloader.img` 啟動，並以 `-initrd initramfs.cpio` 提供 initramfs
    

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
    
    - `ld -T linker_boot.ld -o bootloader.elf $(OBJS_FOR_BOOTLOADER)`
        
    - `objcopy -O binary bootloader.elf bootloader.img`
        
- Kernel：
    
    - `ld -T linker_kernel.ld -o kernel8.elf $(OBJS_FOR_KERNEL)`
        
    - `objcopy -O binary kernel8.elf kernel8.img`
        

#### 6) 編譯規則

- `CFile/%.c -> build/%.o`：使用 `$(CC) $(CFLAGS) -c`
    
- `Assembly/%.S -> build/%.o`：使用 `$(CC) $(ASFLAGS) -c`
    
- `Assembly/%.s -> build/%.o`：使用 `$(CC) $(ASFLAGS) -c`
    

#### 7) QEMU 執行目標

- `qemu`：以 raspi3b 機器啟動，kernel 指向 `bootloader.img`，並掛載 initramfs 與 serial tcp：
    
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
        
- QEMU 的 `-kernel` 只載入 `bootloader.img`；`kernel8.img` 的存在主要供 host 工具透過 UART 傳送，或供你在其他流程使用（Makefile 本身僅負責把它建出來）。
    
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
## `launch.json`

### 檔案定位

VS Code 的 **Debug 設定檔**：定義可在 VS Code 內啟動/連線 GDB 的偵錯組態（C/C++ `cppdbg`），用於連線到 QEMU 的 GDB server 或其他本機 GDB session。

### 目前提供的功能（現有組態）

此檔案目前包含 **2 個 debug configuration**：

#### 1) `OSDI: Manual Debug`

- **用途**：用 `gdb-multiarch` 連到 `127.0.0.1:1234` 的 GDB server（通常對應 `qemu-system-aarch64 -s -S`）。
    
- 主要欄位（就現況）：
    
    - `type: "cppdbg"`、`request: "launch"`、`MIMode: "gdb"`
        
    - `program: "${workspaceFolder}/build/bootloader.elf"`
        
        - 指定 symbols/ELF 來源為 workspace 下的 `build/bootloader.elf`
            
    - `miDebuggerPath: "/usr/bin/gdb-multiarch"`
        
    - `miDebuggerServerAddress: "127.0.0.1:1234"`
        
    - `cwd: "${workspaceFolder}"`
        
    - `externalConsole: false`
        
    - `stopAtEntry: false`
        
    - `setupCommands`：
        
        - `-enable-pretty-printing`
            
        - `set architecture aarch64`
            

#### 2) `C/C++ Runner: Debug Session`

- **用途**：另一個 `cppdbg` 的 debug session（看起來是由某個 VS Code extension/工具自動生成的組態）。
    
- 主要欄位（就現況）：
    
    - `request: "launch"`
        
    - `cwd: "/home/marginal/osdi-wsl-starter/osdi-lab0/CFile"`
        
    - `program: "/home/marginal/osdi-wsl-starter/osdi-lab0/CFile/build/Debug/outDebug"`
        
    - `miDebuggerPath: "gdb"`
        
    - `setupCommands`：
        
        - `-enable-pretty-printing`
            

### 目前的限制/假設（就現況描述）

- `OSDI: Manual Debug` 依賴本機存在 `/usr/bin/gdb-multiarch`，且 `127.0.0.1:1234` 有啟動中的 GDB server。
    
- `C/C++ Runner: Debug Session` 使用**絕對路徑**（`/home/marginal/...`）；在不同機器/不同 workspace 路徑下不一定可直接使用（這是目前檔案中的狀態）。
    

---

## `tasks.json`

### 檔案定位

VS Code 的 **Task 設定檔**：定義可在 VS Code 內執行的建置與啟動命令（build、啟動 QEMU 等），並可提供給 debug pre-launch 依賴使用。

> 檔案中包含 `// ...` 註解，屬於 VS Code 支援的 JSON with Comments（JSONC）用法。

### 目前提供的功能（現有 tasks）

此檔案目前定義 **2 個 tasks**：

#### 1) `Build All`

- **用途**：呼叫 Makefile 進行全量建置。
    
- 內容（就現況）：
    
    - `type: "shell"`
        
    - `command: "make"`
        
    - `args: ["all"]`
        
    - `group`：
        
        - `kind: "build"`
            
        - `isDefault: true`（預設 build task）
            
    - `problemMatcher: ["$gcc"]`（使用 VS Code 內建 gcc matcher 顯示編譯錯誤/警告）
        

#### 2) `Start QEMU (GDB Mode)`

- **用途**：啟動 QEMU 的 gdb 模式（背景執行），並在啟動前清掉舊的 QEMU。
    
- 內容（就現況）：
    
    - `type: "shell"`
        
    - `isBackground: true`（視為背景任務）
        
    - `command`：
        
        - `pkill -9 qemu-system-aarch64 || true && make qemu-gdb`
            
        - 行為包含：
            
            1. 強制終止舊的 `qemu-system-aarch64`（若不存在則忽略錯誤）
                
            2. 執行 `make qemu-gdb`
                
    - `presentation`：
        
        - `echo: true`
            
        - `reveal: "silent"`
            
        - `focus: false`
            
        - `panel: "shared"`
            
        - `showReuseMessage: false`
            
        - `clear: true`
            
    - `problemMatcher`（自訂）：
        
        - `pattern.regexp: "."`（基本占位 pattern）
            
        - `background.activeOnStart: true`
            
        - `background.beginsPattern: "^.*qemu-system-aarch64"`
            
        - `background.endsPattern: "^.*$"`
            
        - 用於讓 VS Code 判定該背景任務「已開始可供後續流程使用」
            
    - `dependsOn: "Build All"`（啟動 QEMU 前先執行建置）
        

### 目前的限制/假設（就現況描述）

- `Start QEMU (GDB Mode)` 假設環境中存在 `pkill`、以及 Makefile 提供 `qemu-gdb` 目標。
    
- `problemMatcher.background` 的結束條件 `endsPattern: "^.*$"` 會在遇到任一行輸出即可能判定結束（這是目前檔案的設定狀態）。
    

---

## `settings.json`

### 檔案定位

VS Code 的工作區設定檔：此檔案目前主要是針對 **`C_Cpp_Runner`**（一個 VS Code extension）設定編譯器、除錯器、警告旗標、搜尋排除等行為。

### 目前提供的功能（現有設定）

#### 1) 編譯器 / 除錯器路徑

- `C_Cpp_Runner.cCompilerPath: "gcc"`
    
- `C_Cpp_Runner.cppCompilerPath: "g++"`
    
- `C_Cpp_Runner.debuggerPath: "gdb"`
    

#### 2) 語言標準 / MSVC 相關（現況為未指定或停用）

- `C_Cpp_Runner.cStandard: ""`
    
- `C_Cpp_Runner.cppStandard: ""`
    
- `C_Cpp_Runner.useMsvc: false`
    
- `C_Cpp_Runner.msvcBatchPath: ""`
    
- `C_Cpp_Runner.msvcSecureNoWarnings: false`
    

#### 3) 警告旗標

- GCC/Clang 警告（`C_Cpp_Runner.warnings`）目前包含：
    
    - `-Wall`, `-Wextra`, `-Wpedantic`, `-Wshadow`, `-Wformat=2`,
        
    - `-Wcast-align`, `-Wconversion`, `-Wsign-conversion`, `-Wnull-dereference`
        
- MSVC 警告（`C_Cpp_Runner.msvcWarnings`）目前包含：
    
    - `/W4`, `/permissive-`, `/w14242`, `/w14287`, `/w14296`, `/w14311`,
        
    - `/w14826`, `/w44062`, `/w44242`, `/w14905`, `/w14906`, `/w14263`,
        
    - `/w44265`, `/w14928`
        
- 其他警告控制：
    
    - `C_Cpp_Runner.enableWarnings: true`
        
    - `C_Cpp_Runner.warningsAsError: false`
        

#### 4) 編譯/連結額外參數（目前為空）

- `C_Cpp_Runner.compilerArgs: []`
    
- `C_Cpp_Runner.linkerArgs: []`
    

#### 5) Include 設定（目前為空或全量搜尋）

- `C_Cpp_Runner.includePaths: []`
    
- `C_Cpp_Runner.includeSearch: ["*", "**/*"]`
    

#### 6) 排除搜尋路徑

- `C_Cpp_Runner.excludeSearch` 目前排除：
    
    - `**/build`, `**/build/**`
        
    - `**/.*`, `**/.*/**`
        
    - `**/.vscode`, `**/.vscode/**`
        

#### 7) Sanitizer 與其他編譯選項（現況皆停用）

- `C_Cpp_Runner.useAddressSanitizer: false`
    
- `C_Cpp_Runner.useUndefinedSanitizer: false`
    
- `C_Cpp_Runner.useLeakSanitizer: false`
    
- `C_Cpp_Runner.showCompilationTime: false`
    
- `C_Cpp_Runner.useLinkTimeOptimization: false`
    

### 目前的限制/假設（就現況描述）

- 以上設定主要影響 `C_Cpp_Runner` extension 的行為；是否影響你的 Makefile 建置流程，取決於你實際採用的 build/launch 方式（此檔案目前未直接改動 Makefile）。

---
## `c_cpp_properties.json`

### 檔案定位

VS Code **C/C++（Microsoft C/C++ extension）**的 IntelliSense/語意分析設定檔，主要用來指定：

- include 搜尋路徑（讓 VS Code 能找到標頭檔、提供補全與錯誤提示）
    
- compiler 路徑與 IntelliSense 模式（決定解析器採用的 target/語法/預設巨集等）
    
- C/C++ 語言標準（此檔案目前使用預設值）
    

### 目前提供的功能（現有設定）

此檔案目前包含 **1 個 configuration**：

#### Configuration：`linux-gcc-x64`

- `name: "linux-gcc-x64"`
    
    - 配置名稱（在 VS Code 中用來選取該組 IntelliSense 設定）。
        
- `includePath`
    
    - `["${workspaceFolder}/**", "/usr/include", "/usr/local/include"]`
        
    - 功能：
        
        - `${workspaceFolder}/**`：將整個 workspace 內的所有子目錄納入 include 搜尋（遞迴）。
            
        - `/usr/include`、`/usr/local/include`：納入系統與本機安裝的標準 include 位置。
            
- `compilerPath: "/usr/bin/gcc"`
    
    - 功能：指定 IntelliSense 用來推導預設 include、內建巨集與 target 設定的編譯器路徑（以 gcc 為基準）。
        
- `intelliSenseMode: "linux-gcc-x64"`
    
    - 功能：指定 IntelliSense 解析模式為 linux + gcc + x64（影響內建型別大小、預設巨集、解析器行為等）。
        
- `cStandard: "${default}"`、`cppStandard: "${default}"`
    
    - 功能：C/C++ 語言標準使用 extension 的預設值（此檔案目前未強制指定如 `c11`/`gnu11`/`c++17` 等）。
        
- `compilerArgs: [""]`
    
    - 功能：提供額外的編譯器參數給 IntelliSense 解析器使用。
        
    - 現況：陣列中只有空字串，等同於「沒有額外參數」的效果（以目前內容而言）。
        

### 目前的限制/假設（就現況描述）

- 此配置的 `intelliSenseMode` 為 `linux-gcc-x64`，若你的實際目標是 **AArch64/裸機（freestanding）**，那麼 VS Code 的 IntelliSense 可能會以 x64/Linux 的預設模型解析，導致：
    
    - 部分 target-specific 內建巨集/型別大小/ABI 假設與實際不一致（就目前設定而言是可能發生的現象）。
        
- `includePath` 使用 `${workspaceFolder}/**` 會讓搜尋範圍非常廣；這是現有設定的行為，可能帶來較多索引成本（但檔案本身的功能就是如此配置）。
    
- 此檔案僅影響 VS Code 的語意分析/補全與診斷；不會直接改變你 Makefile 的實際編譯結果（除非你另有使用 VS Code extension 的 build 流程）。
# Pending Tasks (待辦事項 - Lab 2)
## 依據 Lab 2 規格書，尚未完成的項目。

## 任務一：Initial Ramdisk (Cpio Parser)

**目標**：讓 Kernel 能讀取並解析記憶體中的檔案系統 (initramfs)，並實作類似 `cat` 的功能。

- [x] 步驟 1.1：製作 initramfs.cpio 檔案 (外部準備)

- **動作**：在你的 Linux 環境 (WSL) 建立一個測試用的資料夾 (例如 `rootfs`)，裡面放一個純文字檔 (例如 `test.txt`，寫入一些內容)。
    
- **指令**：使用 `cpio` 指令將該資料夾打包成 `New ASCII Format` 的 cpio 檔案。
    
- **產出**：一個名為 `initramfs.cpio` 的檔案。
    

- [x] 步驟 1.2：修改 Makefile 載入檔案

- **檔案**：`Makefile`
    
- **內容邏輯**：
    
    - 找到 `QEMU` 的執行指令。
        
    - 加入參數 `-initrd initramfs.cpio`。
        
    - **原理**：這告訴 QEMU 把這個檔案載入到記憶體的某個特定位置 (通常預設是 `0x8000000`，但之後我們會用 DTB 動態抓取)。
        

- [x] 步驟 1.3：實作 Cpio Parser

- **檔案**：`header/cpio.h` (新建立)
    
    - **內容邏輯**：
        
        - 定義 Cpio Header 的結構 (struct)。根據 `New ASCII Format` 規格，Header 包含一系列長度為 8 的字串 (例如 `c_magic`, `c_filesize`, `c_namesize` 等)。
            
        - 宣告解析函式的 Prototype (例如 `cpio_parse`, `cpio_get_file_content`)。
            
- **檔案**：`CFile/cpio.c` (新建立)
    
    - **內容邏輯**：
        
        - **Magic Number 檢查**：確認 Header 開頭是否為 `070701` (這是 New ASCII 的標記)。
            
        - **十六進位字串轉整數**：Cpio Header 裡的數字是「16 進位的字串」，你需要寫一個 Helper function 把字串轉成 `int` (例如字串 "0000000A" 轉成整數 10)。
            
        - **指標移動邏輯 (關鍵)**：
            
            1. 讀取 Header。
                
            2. 取得檔名長度 (`namesize`) 和檔案內容長度 (`filesize`)。
                
            3. **Alignment (對齊) 處理**：Cpio 格式規定 Header+檔名 必須是 4-byte aligned，檔案內容也必須是 4-byte aligned。如果長度不是 4 的倍數，會補 0。你需要計算 padding 跳過這些 0。
                
            4. **下一個 Header**：目前的位址 + Header 長度 + 檔名長度 + Padding + 檔案內容長度 + Padding = 下一個檔案 Header 的位址。
                
        - **終止條件**：當檔名讀到 `TRAILER!!!` 時，代表結束。
            

- [x] 步驟 1.4：整合至 Kernel

- **檔案**：`CFile/kernel_main.c`
    
- **內容邏輯**：
    
    - 定義一個記憶體位址變數 (指向 QEMU 載入 cpio 的位置，暫時可 Hardcode 為 `0x8000000`)。
        
    - 呼叫你寫的 `cpio_parse` 函式。
        
    - **測試功能**：寫一個類似 `ls` 的功能印出所有檔名，或寫一個 `cat` 功能印出 `test.txt` 的內容。
        

---

## 任務二：Simple Allocator

**目標**：實作一個簡單的動態記憶體配置 (malloc)，供後續 DTB 解析使用。

- [x] 步驟 2.1：實作配置器

- **檔案**：`header/allocator.h` (新建立)
    
    - **內容邏輯**：宣告 `simple_malloc` 函式。
        
- **檔案**：`CFile/allocator.c` (新建立)
    
    - **內容邏輯**：
        
        - **維護一個全域指標**：這個指標指向目前 Heap 的「頂端」。
            
        - **初始化**：將指標指向 Kernel 程式碼結束的地方 (通常利用 Linker Script 定義的 `_end` 或 `__bss_end` 符號)。為了安全，可以預留一些緩衝空間。
            
        - **配置邏輯 (`malloc`)**：
            
            1. 輸入參數為 `size`。
                
            2. 回傳目前的指標位置 (作為分配出去的記憶體起始點)。
                
            3. 將全域指標往後移動 `size` 的大小。
                
            4. (選做) 考慮 Alignment，確保回傳的指標是 8-byte 或 16-byte 對齊。
                

- [x] 步驟 2.2：整合測試

- **檔案**：`CFile/kernel_main.c`
    
- **內容邏輯**：呼叫 `simple_malloc` 配置一段字串空間，存入資料並印出，驗證記憶體沒有跟 Kernel 程式碼衝突。
    

---

## 任務三：Devicetree (DTB Parser)

**目標**：不再 Hardcode 硬體位址，而是從 Bootloader 傳遞的 DTB 檔案中解析資訊 (如 initramfs 的位址)。

- [x] 步驟 3.1：準備與載入 DTB

- **動作**：下載 RPi3 的 `.dtb` 檔案 (例如 `bcm2710-rpi-3-b-plus.dtb`)。
    
- **檔案**：`Makefile`
    
- **內容邏輯**：在 QEMU 指令加入 `-dtb bcm2710-rpi-3-b-plus.dtb`。
    

 - [ ] 步驟 3.2：修改 Bootloader 傳遞參數 (關鍵！)

- **原理**：QEMU 啟動時，會把 DTB 的記憶體位址放在 CPU 的 `x0` 暫存器。你需要一路把這個值傳給 Kernel。
    
- **檔案**：`Assembly/boot.S` (Bootloader 的)
    
    - **內容邏輯**：在 `_start` 時，`x0` 存著 DTB 位址。在呼叫 C 語言的 `main` 之前，確保這個 `x0` 被當作參數傳入 (ARM64 Calling Convention: 第一個參數放在 `x0`)。
        
- **檔案**：`CFile/bootloader_main.c`
    
    - **內容邏輯**：
        
        - 修改 `main` 函式接收參數：`void main(void *dtb_addr)`。
            
        - 在搬運 Kernel 之前，先把這個 `dtb_addr` 存起來。
            
        - **跳轉修改**：在跳轉到 Kernel (`0x80000`) 之前，必須把 `dtb_addr` 放回 **`x0` 暫存器**。這無法用純 C 語言做，你需要寫一行 Inline Assembly (`asm volatile("mov x0, %0" :: "r"(dtb_addr));`)，然後再跳轉。
            

- [ ] 步驟 3.3：修改 Kernel 接收參數

- **檔案**：`Assembly/boot.S` (Kernel 的)
    
    - **內容邏輯**：Kernel 啟動時，`x0` 現在是 Bootloader 傳過來的 DTB 位址。同樣地，確保呼叫 `kernel_main` 時 `x0` 被傳入。
        
- **檔案**：`CFile/kernel_main.c`
    
    - **內容邏輯**：修改函式簽章 `void kernel_main(void *dtb_addr)`。
        

- [ ] 步驟 3.4：實作 FDT (Flattened Device Tree) Parser

- **檔案**：`header/dtb.h` (新建立)
    
    - **內容邏輯**：定義 DTB Header 結構 (包含 Magic, Totalsize, Off_dt_struct, Off_dt_strings 等)。定義 Token 常數 (FDT_BEGIN_NODE, FDT_PROP, FDT_END_NODE)。
        
- **檔案**：`CFile/dtb.c` (新建立)
    
    - **內容邏輯**：
        
        - **Big Endian 轉換**：DTB 儲存數字是 Big Endian，RPi3 (ARM64) 是 Little Endian。你需要寫一個函式 (`uint32_t bswap32(uint32_t)`) 把讀進來的數字翻轉。
            
        - **解析結構**：
            
            1. 讀取 Header，驗證 Magic Number。
                
            2. 找到 Structure Block (`off_dt_struct`)。
                
            3. **遍歷 Token**：寫一個迴圈讀取 Token (4 bytes)。
                
                - 如果是 `BEGIN_NODE`：讀取節點名稱。
                    
                - 如果是 `PROP` (屬性)：讀取屬性長度、名稱 offset (指向 Strings Block)，並讀取屬性值。
                    
                - 如果是 `END_NODE`：結束當前節點。
                    
        - **API 設計**：實作 `fdt_traverse(callback_function)`。這個函式會走訪整棵樹，每當發現一個 Property，就呼叫 callback 讓外部判斷。
            

- [ ] 步驟 3.5：應用 - 自動抓取 initramfs 位址

- **檔案**：`CFile/kernel_main.c`
    
- **內容邏輯**：
    
    - 寫一個 Callback function。
        
    - 在 Callback 中檢查 Property 名稱是否為 `linux,initrd-start`。
        
    - 如果是，讀取其數值 (這就是 initramfs 在記憶體的真實位址)。
        
    - 將這個位址傳給 **步驟 1** 的 `cpio_parse`，取代原本 Hardcode 的 `0x8000000`。