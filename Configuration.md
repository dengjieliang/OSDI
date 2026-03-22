# 組態與開發設定 (Configuration & Development Setup)

## 對應總說明章節

- `0.2 建置與執行流程（Build & Run Pipeline）`
- `17. 自動化編譯與建置設定 (Build Automation)`
- `18. 連結腳本 (Linker Scripts)`
- `19. VSCode設定 (VS Code Configuration)`

## 檔案定位

本文檔整合了專案的所有組態、建置自動化與除錯設定，涵蓋：

- Bootloader 與 Kernel 的連結腳本（linker scripts）
- VS Code 的自動化任務設定（tasks.json）
- VS Code 的 GDB 除錯組態（launch.json）
- 相應的說明與使用指南

## 內容概述

- **Linker Scripts** - Bootloader 與 Kernel 的記憶體佈局與連結位址
- **Build Automation** - VS Code 自動化建置與 QEMU 啟停
- **VS Code Configuration** - GDB 除錯組態、符號載入、連線設定

--- 

# Linker Scripts

## `linker_boot.ld` - Bootloader連結腳本

### 檔案定位

此檔案是 Bootloader image 的 linker script，決定 bootloader 最終 ELF / binary 在記憶體中的連結位址、各 section 佈局，以及 startup code 會依賴的特殊符號（`__bss_start`、`__bss_end`、`_stack_top`）。

### 目前提供的功能

#### 入口點設定

- `ENTRY(_start)`
- 指定 Bootloader 連結後的入口符號為 `_start`。
- 對應的實作位於 [Boot/boot.S](Boot/boot.S)，詳細流程請看 [Boot/Boot.md](Boot/Boot.md)。

#### 連結位址

- `. = 0x60000;`
- 指定 bootloader 影像的連結起點為 `0x60000`。
- [Boot/boot.S](Boot/boot.S) 中的 relocation 流程會拿 runtime `_start` 與 linker `_start` 比對，必要時把程式搬到這個位址。

#### Section 佈局

- `.text : { *(.text .text.*) }`
- `.rodata : { *(.rodata .rodata.*) }`
- `.data : { *(.data .data.*) }`
- `.bss : { *(.bss .bss.*) *(COMMON) }`

這代表 bootloader 目前使用最直接的 section 佈局：依序擺放 code、唯讀資料、已初始化資料、未初始化資料。

#### BSS 與 Stack 符號

- `__bss_start = .;`
- `__bss_end = .;`
- `_stack_top = __bss_end + 0x04000;`

這三個符號的用途如下：

- `__bss_start` / `__bss_end`
  - 供 [Boot/boot.S](Boot/boot.S) 的 `clear_bss` 使用。
- `_stack_top`
  - 供 [Boot/boot.S](Boot/boot.S) 在進入 C 入口前設定 `sp`。
  - 目前 stack 空間大小固定為 `0x4000` bytes。

### 現況注意

- Bootloader 與 Kernel 雖然共用 [Boot/boot.S](Boot/boot.S)，但兩者的 linker script 不同，因此 relocation 目標位址與最終影像基底不同。
- 這份 linker script 沒有額外切分 heap、per-CPU stack 或例外向量專用區塊；目前設計偏向最小可用版本。
- `_stack_top = __bss_end + 0x04000` 是固定配置，若未來 `.bss` 或 bootloader 功能明顯增長，需要重新檢查空間佈局是否足夠。

### 跨文件導讀

- 若要理解 `_start`、`clear_bss`、relocation 如何使用這些符號，請看 [Boot/Boot.md](Boot/Boot.md)。
- 若要理解 Makefile 如何把 bootloader 物件檔與這份 linker script 串起來，請看 [Makefile](Makefile)。
- 若要理解 kernel 影像的對應配置，請看下方 `linker_kernel.ld` 章節。

### 閱讀建議

- 想看「Bootloader 為何連到 `0x60000`」：先看本檔，再看 [Boot/Boot.md](Boot/Boot.md)。
- 想看「為何 startup code 能知道 bss 與 stack 範圍」：本檔搭配 [Boot/Boot.md](Boot/Boot.md) 一起看。

---

## `linker_kernel.ld` - Kernel連結腳本

### 檔案定位

此檔案是 Kernel image 的 linker script，決定 kernel ELF / binary 的連結位址、section 佈局與啟動組語依賴的特殊符號。它與 bootloader 共享同樣風格的 section 佈局，但 kernel 的連結基底位址改為 `0x80000`。

### 目前提供的功能

#### 入口點設定

- `ENTRY(_start)`
- kernel 入口同樣是 `_start`，實作位於 [Boot/boot.S](Boot/boot.S)。
- Bootloader 會把 `0x80000` 視為 `kernel_entry_t` 跳轉進來；對應 handoff 流程請看 [Boot/Boot.md](Boot/Boot.md)。

#### 連結位址

- `. = 0x80000;`
- 指定 kernel image 的連結起點為 `0x80000`。
- 這個位址也與 [Board/Board.md](Board/Board.md) 中 `KERNEL_LOAD_ADDRESS` 的概念一致。

#### Section 佈局

- `.text : { *(.text .text.*) }`
- `.rodata : { *(.rodata .rodata.*) }`
- `.data : { *(.data .data.*) }`
- `.bss : { *(.bss .bss.*) *(COMMON) }`

kernel 目前也採最小可用 section 佈局，讓 startup code 能直接使用 `__bss_start` / `__bss_end` 清零 BSS。

#### BSS 與 Stack 符號

- `__bss_start = .;`
- `__bss_end = .;`
- `_stack_top = __bss_end + 0x04000;`

用途如下：

- `__bss_start` / `__bss_end`
  - 供 [Boot/boot.S](Boot/boot.S) 的 `clear_bss` 使用。
- `_stack_top`
  - 供 [Boot/boot.S](Boot/boot.S) 在進入 [Kernel/kernel_main.c](Kernel/kernel_main.c) 前設定 `sp`。

### 現況注意

- 這份 linker script 對應的是 kernel image，而不是 bootloader；兩者共用 startup code，但基底位址不同。
- 若 GDB 只載入 bootloader symbols，kernel 端函式位址會缺少符號資訊，因此下方 `launch.json` 中才需要額外 `add-symbol-file build/kernel8.elf 0x80000`。
- 與 bootloader 一樣，這裡的 stack 大小固定為 `0x4000` bytes；若後續 kernel 功能增長，需要重新評估。

### 跨文件導讀

- 若要理解 Bootloader 如何把 kernel 寫到 `0x80000`，請看 [Boot/Boot.md](Boot/Boot.md)。
- 若要理解 kernel 啟動後做了哪些初始化，請看 [Kernel/Kernel.md](Kernel/Kernel.md)。
- 若要理解 VS Code debug 為何要額外載入 kernel symbol，請看下方 `launch.json` 章節。
- 若要理解 bootloader 的對應 linker 配置，請看上方 `linker_boot.ld` 章節。

### 閱讀建議

- 想看「為何 kernel 連到 `0x80000`」：先看本檔，再看 [Boot/Boot.md](Boot/Boot.md)。
- 想看「debug 時為何要手動 add-symbol-file」：本檔搭配下方 `launch.json` 章節一起看。

---

# Build Automation & VS Code Integration

## tasks.json - 自動化建置任務

### 檔案定位

此檔案位於 `.vscode/tasks.json`，是 VS Code task 自動化設定，負責把目前專案常用的建置與 QEMU 啟停流程包成固定 task，讓使用者可直接從 VS Code 啟動 build、debug 前置 QEMU 與 stop 動作。

### 目前提供的 task

#### `Build All`

- 執行：`make all`
- 工作目錄：`${workspaceFolder}`
- 群組：預設 build task
- problem matcher：`$gcc`

用途：

- 建置 `build/bootloader.img`
- 建置 `build/kernel8.img`
- 建置 `initramfs.cpio`

對應的實際規則請看 [Makefile](Makefile)。

#### `Start QEMU (GDB Mode)`

- 類型：background shell task
- 依賴：`Build All`
- 核心命令：
  - 先 `pkill -9 qemu-system-aarch64`
  - 啟動 `make qemu-gdb`
  - 等待 `127.0.0.1:1234` 與 `127.0.0.1:8888` 兩個 port 就緒
  - 以 `__QEMU_START__` / `__QEMU_READY__` 作為 background problem matcher 的開始 / 結束標記

用途：

- 啟動 QEMU 的 GDB server（`:1234`）
- 啟動 serial server（`:8888`）
- 確保 VS Code 在 debug attach 前，QEMU 真的已經 ready

#### `Stop QEMU`

- 執行：`pkill -9 qemu-system-aarch64 2>/dev/null || true`
- 用途：在 debug 結束或手動停止時清掉背景 QEMU。

### 現況注意

- `Start QEMU (GDB Mode)` 不只是單純執行 `make qemu-gdb`，而是額外包了一層 port-ready 檢查，避免 VS Code 過早 attach 導致 handshake 不穩定。
- `problemMatcher.background` 依賴 `__QEMU_START__` 與 `__QEMU_READY__` 兩個 marker；若未來更動 shell 指令時把 marker 拿掉，VS Code 背景 task 完成判定會失效。
- `Build All` 的實際編譯行為仍以 [Makefile](Makefile) 為準；`tasks.json` 只是把它包成 VS Code 入口。

### 跨文件導讀

- 若要理解 `make qemu-gdb` 的 QEMU 參數從哪裡來，請看 [Makefile](Makefile)。
- 若要理解 debug 啟動時如何搭配 `preLaunchTask` / `postDebugTask`，請看下方 `launch.json` 章節。
- 若要理解 serial server `:8888` 之後如何被 Host tool 使用，請看 [Tools/Tools.md](Tools/Tools.md)。
- 若要理解 GDB server `:1234` 如何被 VS Code attach，請看下方 `launch.json` 章節。

### 閱讀建議

- 想看「為何 VS Code debug 前會先啟動 QEMU」：先看本檔，再看下方 `launch.json` 章節。
- 想看「QEMU 的實際啟動參數」：本檔搭配 [Makefile](Makefile) 一起看。

---

## launch.json - 除錯設定

### 檔案定位

此檔案位於 `.vscode/launch.json`，是 VS Code 的 GDB debug 設定，負責在 QEMU GDB server 啟動後連線到 `:1234`，並把 bootloader 與 kernel 的 symbol 一併載入，讓使用者能在 VS Code 中穩定除錯目前的 boot chain。

### 目前提供的 configuration

#### `OSDI: Debug Lab2 (stable handshake)`

- `type: cppdbg`
- `request: launch`
- `program: ${workspaceFolder}/build/bootloader.elf`
- `miDebuggerPath: /usr/bin/gdb-multiarch`
- `miDebuggerServerAddress: 127.0.0.1:1234`
- `preLaunchTask: Start QEMU (GDB Mode)`
- `postDebugTask: Stop QEMU`
- `stopAtConnect: true`
- `launchCompleteCommand: None`

#### 主要 setup commands

- `-enable-pretty-printing`
- `set architecture aarch64`
- `break kernel_main`
- `add-symbol-file ${workspaceFolder}/build/kernel8.elf 0x80000`

這組設定的意義如下：

- 先以 bootloader ELF 作為主要 debug program，因為 QEMU 一開始執行的是 bootloader。
- 之後再額外把 kernel ELF 以 `0x80000` 載入 symbol table，讓 handoff 後的 kernel 程式也能對應到正確符號。

#### `C/C++ Runner: Debug Session`

- 這是一組較一般的 `cppdbg` 設定。
- `program` 指向 `build/kernel8.elf`。
- `setupCommands` 只保留 pretty-printing 與 `set architecture aarch64`。

這份 configuration 目前不是主要 boot chain debug 路徑；真正與 QEMU GDB workflow 配套的是 `OSDI: Debug Lab2 (stable handshake)`。

### 現況注意

- `add-symbol-file build/kernel8.elf 0x80000` 與上方 `linker_kernel.ld` 中的 kernel 連結位址相互對應；若未來 kernel linker 位址改動，這裡也必須同步修改。
- `break kernel_main` 可能同時命中 bootloader 與 kernel 的 `kernel_main`；目前仍可接受，因為 bootloader 與 kernel 本來就共用這個命名慣例。
- 這份設定假設 `gdb-multiarch` 已安裝，且 QEMU GDB server 已由上方 `tasks.json` 所描述的 preLaunchTask 啟動。

### 跨文件導讀

- 若要理解 preLaunchTask / postDebugTask 來自哪裡，請看上方 `tasks.json` 章節。
- 若要理解 bootloader 與 kernel 各自的 linker 位址，請看上方 `linker_boot.ld` 與 `linker_kernel.ld` 章節。
- 若要理解 Bootloader / Kernel 都叫 `kernel_main` 的啟動邏輯，請看 [Boot/Boot.md](Boot/Boot.md) 與 [Kernel/Kernel.md](Kernel/Kernel.md)。

### 閱讀建議

- 想看「VS Code 如何穩定 attach 到 QEMU」：先看本檔，再看上方 `tasks.json` 章節。
- 想看「為何需要 add-symbol-file kernel8.elf 0x80000」：本檔搭配上方 `linker_kernel.ld` 章節一起看。
