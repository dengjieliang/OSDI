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

## Basic Exercise 2 - Initial Ramdisk

- [ ] 製作 initramfs.cpio (New ASCII Format)。

- [ ] 修改 Makefile 加入 -initrd 參數。

- [ ] 新增 CFile/cpio.c: 實作 Cpio Parser (Magic number check, filename parsing, content reading)。

- [ ] 在 Kernel 中讀取並印出 initramfs 內的檔案內容。

## Basic Exercise 3 - Simple Allocator

- [ ] 新增 CFile/allocator.c: 實作簡易的 simple_malloc (Bump Allocator)。

- [ ] Advanced Exercise 2 - Devicetree (DTB)

- [ ] 下載 RPi3 DTB 檔案並掛載至 QEMU。

- [ ] 修改 boot.S: 傳遞 x0 (DTB Address) 給 C 語言。

- [ ] 修改 Bootloader: 在跳轉前將 DTB Address 放入 x0。

- [ ] 新增 CFile/dtb.c: 實作 FDT Parser (Big-endian conversion, Token parsing)。

- [ ] 使用 DTB API 動態取得 initramfs 的載入位址。