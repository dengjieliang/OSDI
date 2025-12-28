OSDI Lab Development Progress Tracker
Current Lab: Lab 2 - Booting Target Platform: Raspberry Pi 3 B+ (AArch64) Environment: WSL (Ubuntu) + QEMU + GDB

# 1. Build System & Configuration (建置系統)
# 此區塊管理編譯流程與記憶體佈局設定。


## Makefile 

- Cross-Compilation Setup: 設定 aarch64-linux-gnu- toolchain。

- Split Compilation: 將 Bootloader 與 Kernel 分開編譯，產生兩個獨立的 Image (bootloader.img, kernel8.img) 。

 - Automation Targets:

- make qemu: 啟動 QEMU 模擬。


- make qemu-gdb: 啟動 QEMU 並開啟 GDB Server (Port 1234) 與 Serial Server (Port 8888) 。

- Dependency Management: 修正 .o 檔依賴規則，確保 bootloader_main.c 與 kernel_main.c 不會互相衝突 。


## linker_boot.ld (Bootloader Linker Script) 

- Memory Layout: 設定 Entry Point 為 0x60000，避免與 Kernel (0x80000) 衝突。

- BSS/Stack: 定義 BSS 區段與 Stack Top (0x64000) 。


## linker_kernel.ld (Kernel Linker Script) 

- Memory Layout: 設定 Kernel Entry Point 為 0x80000 (RPi3 預設載入位址)。

- BSS/Stack: 定義 Kernel 的 BSS 區段與 Stack Top 。

## VS Code Configuration

- tasks.json: 設定自動化 make 流程，並加入 pkill 指令防止 QEMU Zombie Process 佔用 Port。

- launch.json: 設定 GDB Debugger 連接至 :1234，並載入 bootloader.elf 符號表。

# 2. Bootloader Implementation (啟動程式)
# 負責系統初始化、自我搬移與載入 Kernel。

## Assembly/boot.S (Entry Point)

- Self-Relocation: 實作 relocate_kernel 與 _relocate_loop，將 Bootloader 程式碼從載入位址搬移至 Linker 設定位址 (0x60000)。

- BSS Clearing: 實作 clear_bss，確保全域變數初始值為 0。

- Stack Initialization: 設定 sp 指標。

- Control Handoff: 執行完畢後跳轉至 C 語言入口 (kernel_main 或 bootloader_main)。

- CFile/bootloader_main.c (Main Logic)

- Hardware Init: 呼叫 uart_init() 初始化 Mini UART。

- Handshake Protocol: 發送 OSDI: Ready 通知 Host 端。

 - Kernel Loading:

1. 接收 Kernel Size (4 bytes)。

2. 接收 Kernel Image Content (Byte-by-byte)。

3. 將資料寫入 KERNEL_LOAD_ADDRESS (0x80000)。

4. Execution Jump: 使用 Function Pointer 強制轉型跳轉至 Kernel Entry Point。

# 3. Kernel Core (核心邏輯)
# 實際的作業系統核心。

## CFile/kernel_main.c

- Re-initialization: 再次初始化 UART (確保硬體狀態正確)。

- Welcome Message: 印出 Welcome to OSDI 確認進入 Kernel 模式。

- Shell Entry: 呼叫 shell_main() 進入互動模式。

# 4. Drivers & Peripherals (驅動程式)
# 硬體控制層。

## CFile/uart.c / header/uart.h

 - Mini UART Initialization: 設定 GPIO 14/15 為 ALT5 功能，設定 Baud Rate (115200)，關閉 Flow Control。

- Polling I/O:

1. uart_send(): 檢查 TX FIFO 是否有空位。

2. uart_recv(): 檢查 RX FIFO 是否有資料。

- Helper Functions: 實作 uart_puts, uart_send_hex, uart_recv_uint 等工具函式。

# 5. Host Tools (主機端工具)
# 在 PC (Host) 端執行的輔助工具。

## Python/send_kernel.py

- Connection: 使用 socket 連接 QEMU Serial Port (127.0.0.1:8888)。

- Loader Protocol:

1. 等待 OSDI: Ready 訊號。

2. 傳送 Kernel Size (Little Endian)。

3. 傳送 kernel8.img 檔案內容。

- Terminal Emulator:

1. 使用 select 實作非阻塞式 I/O (Non-blocking I/O)。

2. 同時監聽 Socket (來自 RPi3 的輸出) 與 Stdin (使用者的鍵盤輸入)。

3. 解決 Python 編碼問題 (支援 UTF-8 decode)。

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
        

- [ ] 步驟 1.3：實作 Cpio Parser

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
            

- [ ] 步驟 1.4：整合至 Kernel

- **檔案**：`CFile/kernel_main.c`
    
- **內容邏輯**：
    
    - 定義一個記憶體位址變數 (指向 QEMU 載入 cpio 的位置，暫時可 Hardcode 為 `0x8000000`)。
        
    - 呼叫你寫的 `cpio_parse` 函式。
        
    - **測試功能**：寫一個類似 `ls` 的功能印出所有檔名，或寫一個 `cat` 功能印出 `test.txt` 的內容。
        

---

## 任務二：Simple Allocator

**目標**：實作一個簡單的動態記憶體配置 (malloc)，供後續 DTB 解析使用。

- [ ] 步驟 2.1：實作配置器

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
                

- [ ] 步驟 2.2：整合測試

- **檔案**：`CFile/kernel_main.c`
    
- **內容邏輯**：呼叫 `simple_malloc` 配置一段字串空間，存入資料並印出，驗證記憶體沒有跟 Kernel 程式碼衝突。
    

---

## 任務三：Devicetree (DTB Parser)

**目標**：不再 Hardcode 硬體位址，而是從 Bootloader 傳遞的 DTB 檔案中解析資訊 (如 initramfs 的位址)。

- [ ] 步驟 3.1：準備與載入 DTB

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