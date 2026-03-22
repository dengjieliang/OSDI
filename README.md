# OSDI Lab - Bootloader & Kernel Development

**專案名稱：** OSDI Lab 2  
**目標平台：** Raspberry Pi 3 B+ (AArch64)  
**開發環境：** WSL (Ubuntu) + QEMU + GDB

---

# 0. 系統總覽 (System Overview)

此專案為 OSDI Lab 2 的 Bootloader + Kernel 開發環境。

## 0.1 目前可做什麼

詳見 [Board/Board.md](Board/Board.md)，概括如下：

- Bootloader 啟動並等待 Kernel 載入
- Host 端透過 Serial 協定（Python script）傳送 Kernel
- Kernel 啟動後進入互動 Shell 模式

## 0.2 建置與執行流程

請參考：

- [Makefile](Makefile) - 編譯命令與 QEMU 參數
- [Configuration.md](Configuration.md) - 連結腳本與 VS Code 組態

## 0.3 開機交棒流程

請參考 [Boot/Boot.md](Boot/Boot.md) 了解 Bootloader 與 Kernel 的啟動流程。

## 0.4 Kernel 主流程

請參考 [Kernel/Kernel.md](Kernel/Kernel.md) 了解 Kernel 初始化與互動 Shell。

## 0.5 核心硬體依賴

請參考 [Driver/Driver.md](Driver/Driver.md) 了解 UART、Timer 等驅動程式。

## 0.6 Host 端工具

請參考 [Tools/Tools.md](Tools/Tools.md) 了解 `send_kernel.py` 的使用。

## 0.7 Early Boot 通訊與位址基礎

請參考 [Driver/Driver.md](Driver/Driver.md) 了解 `early_uart` 與 `early_common` 的角色。

---

# 1. 共同定義 (Common Definitions)

Kernel 階段的共用型別、巨集與 MMIO 讀寫工具。

**檔案位置：** [Board/common.h](Board/common.h) / [Board/common.c](Board/common.c)

**主要內容：**
- 基本常數與型別（`bool`, `NULL` 等）
- 常用巨集（`ALIGN4`, `ALIGN8`, `MAX_ARGS` 等）
- 動態 MMIO base 初始化（`common_init_from_dtb`）

適合搭配文件閱讀：[Board/Board.md](Board/Board.md)

---

# 2. Mailbox 介面 (Mailbox Interface)

與 GPU/firmware 通訊的 Mailbox Property Channel 實作。

**檔案位置：** [Driver/mailbox.h](Driver/mailbox.h) / [Driver/mailbox.c](Driver/mailbox.c)

**主要功能：**
- Board Revision 查詢
- Memory 資訊查詢
- Property Channel 協定實作

適合搭配文件閱讀：[Driver/Driver.md](Driver/Driver.md)

---

# 3. 電源管理 (Power Manager)

電源/看門狗（watchdog）暫存器與重啟邏輯。

**檔案位置：** [Driver/power_manager.h](Driver/power_manager.h) / [Driver/power_manager.c](Driver/power_manager.c)

**主要功能：**
- `reset(tick)` - 觸發系統重啟
- `cancel_reset()` - 取消重啟

適合搭配文件閱讀：[Driver/Driver.md](Driver/Driver.md)

---

# 4. 字串處理函式庫 (String Library)

基本的字串操作（strlen, strcpy, strcat 等）。

**檔案位置：** [Lib/string.h](Lib/string.h) / [Lib/string.c](Lib/string.c)

適合搭配文件閱讀：[Lib/Lib.md](Lib/Lib.md)

---

# 5. 通用工具 (Utilities)

雜項工具函式（itoa, power, min/max 等）。

**檔案位置：** [Lib/utils.h](Lib/utils.h) / [Lib/utils.c](Lib/utils.c)

適合搭配文件閱讀：[Lib/Lib.md](Lib/Lib.md)

---

# 6. 簡易記憶體配置 (Simple Allocator)

簡單的動態記憶體配置（malloc/free）。

**檔案位置：** [Memory/allocator.h](Memory/allocator.h) / [Memory/allocator.c](Memory/allocator.c)

適合搭配文件閱讀：[Memory/Memory.md](Memory/Memory.md)

---

# 7. CPIO 檔案系統解析器 (CPIO Parser)

CPIO 格式的 initramfs 檔案系統解析。

**檔案位置：** [FileSystem/cpio.h](FileSystem/cpio.h) / [FileSystem/cpio.c](FileSystem/cpio.c)

適合搭配文件閱讀：[FileSystem/FileSystem.md](FileSystem/FileSystem.md)

---

# 8. Device Tree Blob 解析 (DTB Parser)

Device Tree Blob 的完整解析實作。

**檔案位置：** [Board/dtb.h](Board/dtb.h) / [Board/dtb.c](Board/dtb.c)

適合搭配文件閱讀：[Board/Board.md](Board/Board.md)

---

# 9. FDT 處理邏輯與上下文 (FDT Handler & Context)

DTB 解析後的回調邏輯與硬體資訊收集。

**檔案位置：** [Board/fdtb.h](Board/fdtb.h) / [Board/fdtb.c](Board/fdtb.c)

適合搭配文件閱讀：[Board/Board.md](Board/Board.md)

---

# 10. 系統計時器 (System Timer)

Generic Timer 與 Core Timer 的驅動與中斷支援。

**檔案位置：** [Driver/time.h](Driver/time.h) / [Driver/time.c](Driver/time.c)

適合搭配文件閱讀：[Driver/Driver.md](Driver/Driver.md)

---

# 11. 簡易命令列介面 (Shell)

互動式 Shell 命令列處理。

**檔案位置：** [Shell/shell.h](Shell/shell.h) / [Shell/shell.c](Shell/shell.c)

適合搭配文件閱讀：[Shell/Shell.md](Shell/Shell.md)

---

# 12. Mini UART 驅動程式 (Mini UART Driver)

AUX Mini UART 的初始化、IRQ 處理與非同步 I/O。

**檔案位置：** [Driver/uart.h](Driver/uart.h) / [Driver/uart.c](Driver/uart.c)

適合搭配文件閱讀：[Driver/Driver.md](Driver/Driver.md)

---

# 13. 組合語言啟動程式 (Startup)

CPU 初始化、BSS 清零、Stack 設置、Exception Vector 安裝。

**檔案位置：** [Boot/boot.S](Boot/boot.S) / [Kernel/exception_table.S](Kernel/exception_table.S)

適合搭配文件閱讀：[Boot/Boot.md](Boot/Boot.md)

---

# 14. 核心載入器邏輯 (Kernel Loader Logic)

Bootloader 的 Kernel 載入與交棒邏輯。

**檔案位置：** [Boot/bootloader_main.c](Boot/bootloader_main.c)

適合搭配文件閱讀：[Boot/Boot.md](Boot/Boot.md)

---

# 15. 作業系統核心主程式 (Kernel Main)

Kernel 的初始化流程與主控制邏輯。

**檔案位置：** [Kernel/kernel_main.c](Kernel/kernel_main.c)

適合搭配文件閱讀：[Kernel/Kernel.md](Kernel/Kernel.md)

---

# 16. Python 傳輸腳本 (Python Serial Script)

Host 端的 Kernel 載入工具與 Serial 通訊。

**檔案位置：** [Tools/send_kernel.py](Tools/send_kernel.py)

適合搭配文件閱讀：[Tools/Tools.md](Tools/Tools.md)

---

# 17. 自動化編譯與建置設定 (Build Automation)

Makefile 與 VS Code tasks 的自動化編譯與偵錯。

**檔案位置：** [Makefile](Makefile)

VS Code Tasks 與 Debug Config：請見 [Configuration.md](Configuration.md)

---

# 18. 連結腳本 (Linker Scripts)

Bootloader 與 Kernel 的記憶體佈局與連結位址配置。

**檔案位置：** 
- `linker_boot.ld` - Bootloader (0x60000)
- `linker_kernel.ld` - Kernel (0x80000)

**詳細說明：** [Configuration.md](Configuration.md) - Linker Scripts 章節

---

# 19. VSCode設定 (VS Code Configuration)

VS Code 的除錯組態與自動化任務。

**檔案位置：**
- `.vscode/launch.json` - GDB 除錯設定
- `.vscode/tasks.json` - Build / QEMU 自動化

**詳細說明：** [Configuration.md](Configuration.md) - Build Automation & VS Code Integration 章節

---

## 快速開始

1. **編譯專案：** `make all`
2. **啟動 QEMU（GDB Mode）：** VS Code 中執行 `Build All` 與 `Start QEMU (GDB Mode)` tasks
3. **開始 Debug：** VS Code 中選擇 `OSDI: Debug Lab2 (stable handshake)` 並 Launch
4. **傳送 Kernel：** 在另一個終端執行 `python3 Tools/send_kernel.py`

## 關鍵文件導覽

- **系統架構：** [Board/Board.md](Board/Board.md)
- **Bootloader 流程：** [Boot/Boot.md](Boot/Boot.md)
- **Kernel 流程：** [Kernel/Kernel.md](Kernel/Kernel.md)
- **驅動程式：** [Driver/Driver.md](Driver/Driver.md)
- **編譯與除錯配置：** [Configuration.md](Configuration.md)
- **檔案系統：** [FileSystem/FileSystem.md](FileSystem/FileSystem.md)
- **記憶體管理：** [Memory/Memory.md](Memory/Memory.md)
- **Shell 命令列：** [Shell/Shell.md](Shell/Shell.md)
- **工具使用：** [Tools/Tools.md](Tools/Tools.md)
