# Board

## 對應總說明章節

- `0.4 Kernel 主流程（Kernel Core / Interactive Loop）`
- `0.5 核心硬體依賴（Drivers & Peripherals）`
- `0.7 Early Boot 通訊與位址基礎（early_uart / early_common）`
- `1. 共同定義 (Common Definitions)`
- `8. Device Tree Blob 解析 (DTB Parser)`
- `9. FDT 處理邏輯與上下文 (FDT Handler & Context)`

## 檔案定位

此資料夾集中與板級硬體描述、共用 MMIO 基底、DTB 解析與 FDT 上下文有關的程式碼。它本身不直接提供高階互動功能，而是替 Boot、Driver、Kernel、Shell 等模組提供「硬體位址從哪裡來」與「initramfs / interrupt controller / UART / GPIO 資訊如何被收集」的基礎。

## 內容概述

- `common.h` / `common.c`：提供 kernel 階段共用型別、巨集與動態 MMIO base。
- `../Lib/base.h`：集中共用基礎定義（`NULL`、`bool`、`ALIGN4/8`、`MAX_*`），已作為各模組 include 的共同來源。
- `early_common.h`：提供 Bootloader early 階段固定 MMIO base 與 `KERNEL_LOAD_ADDRESS`。
- `dtb.h` / `dtb.c`：提供 callback 驅動式 DTB blob traversal。
- `fdtb.h` / `fdtb.c`：提供 `dtb_ctx` 與收集 `/chosen`、UART、GPIO、interrupt controller 等節點資訊的 callback。

## 目前提供的功能

## `common.h`

### 檔案定位

提供 kernel 階段的共用型別 / 巨集與動態 MMIO（Memory-mapped I/O）讀寫工具。`MMIO_BASE` 與 `GPIO_BASE` 並非固定常數，而是由 `common_mmio` 在 runtime 決定。

### 目前提供的功能

- 基本常數 / 型別
  - `NULL`
  - 在非 C++ 編譯時定義：`bool`、`true`、`false`
- 常用巨集
  - `ALIGN4(x)`
  - `ALIGN8(x)`
  - `MAX_ARGS`
  - `MAX_STRING_SIZE`
- 共用 MMIO 狀態與初始化介面
  - `CommonMmioInfoT`
  - `extern CommonMmioInfoT common_mmio`
  - `void common_init_from_dtb(struct ctx* dtb_ctx)`
- 位址巨集
  - `MMIO_BASE`
  - `GPIO_BASE`
  - `GPFSEL1`
  - `GPPUD`
  - `GPPUDCLK0`
- MMIO 讀寫 helper
  - `mmio_write(unsigned long reg, unsigned int data)`
  - `mmio_read(unsigned long reg)`

### 現況注意

- `bool/true/false` 僅在非 C++ 時定義。
- `MMIO_BASE` 的實際值取決於 `common_mmio`；若未呼叫 `common_init_from_dtb()` 或 DTB 不完整，會沿用預設值。
- primitive 定義已由 `base.h` 集中提供；`common.h` 目前聚焦在 MMIO 與 board-level 共用介面。

## `common.c`

### 檔案定位

實作 kernel 階段共用 MMIO base 的保存與初始化邏輯。根據 DTB 解析結果更新 `common_mmio`，讓各模組透過 `common.h` 的位址巨集使用一致基底。

### 目前提供的功能（實作）

#### 1) 預設 base 與全域狀態

- 預設值：
  - `MMIO_BASE_DEFAULT = 0x3F000000`
  - `GPIO_BASE_OFFSET = 0x200000`
  - `AUX_BASE_OFFSET = 0x215000`
  - `IRQ_BASE_OFFSET = 0xB200`
- 全域 `common_mmio` 初始為：
  - `mmio_base = 0x3F000000`
  - `gpio_base = 0x3F200000`

#### 2) `void common_init_from_dtb(struct ctx* dtb_ctx)`

- 依 `dtb_ctx` 內容更新 `common_mmio`
- 更新優先序：
  1. 若 `have_gpio_reg` 為真
  2. 否則若 `have_aux_reg` 為真
  3. 否則若 `interrupt_info.have_arm_ctrl_intc_base` 為真
- 若 `dtb_ctx == NULL`，直接返回

### 現況注意

- 目前只更新 `mmio_base` 與 `gpio_base`；更細的 UART/AUX 寄存器細節仍由 Driver 層自行組裝。

## `early_common.h`

### 檔案定位

提供 Bootloader early 階段的最小共用層。

### 目前提供的功能

- 定義 `KERNEL_LOAD_ADDRESS (0x80000)` 作為 kernel 載入位址
- 定義固定 `MMIO_BASE` 與 GPIO / AUX 相關暫存器推導基礎
- 提供 early path 使用的 `mmio_read/mmio_write`

### 現況注意

- 這條路徑不依賴 DTB，目的在於讓 Bootloader 在最小依賴條件下先能收發 UART、載入 kernel。

## `dtb.h`

### 檔案定位

提供 Device Tree Blob traversal 用的事件列舉、callback 型別與對外 API 宣告。

### 目前提供的功能（宣告）

- `NodeEnum`
  - `ENTER_NODE`
  - `LEAVE_NODE`
  - `PROP_NODE`
- `FdtHandleNodeFunction`
  - 把目前 node path、event type、property name/value 與 `user_data` 傳回給呼叫端
- 對外 API
  - `bool ReadDTBFile(void* header, FdtHandleNodeFunction callBack, void* user_data)`

## `dtb.c`

### 檔案定位

實作一個最小化 DTB parser：解析 header、定位 structure block / strings block，逐 token 走訪並以 callback 回報事件。

### 目前提供的功能（實作）

#### `bool ReadDTBFile(void* header, FdtHandleNodeFunction callBack, void* user_data)`

- 驗證 DTB magic number `0xd00dfeed`
- 解析並換端序取得：
  - `totalsize`
  - `off_dt_struct` / `size_dt_struct`
  - `off_dt_strings` / `size_dt_strings`
- 檢查 structure block 與 strings block 是否越界
- 逐 token 走訪：
  - `FDT_BEGIN_NODE`
  - `FDT_PROP`
  - `FDT_END_NODE`
  - `FDT_END`
- 維護：
  - `nodeStack[MAX_DEPTH]`
  - `depth`
- 在對應時機呼叫 callback

### 現況注意

- `ReadDTBFile()` 名稱雖然叫 File，但實際是解析記憶體中的 DTB blob。
- property 的 `valuePtr/valueLength` 不會自動做語意解析；解碼責任在 callback 端。
- 深度上限固定為 `MAX_DEPTH = 32`。

## `fdtb.h`

### 檔案定位

提供 DTB traversal callback 使用的 context、逐層 node 狀態與數個輔助函式，用來在走訪過程中擷取 initramfs、UART、GPIO、AUX 與 interrupt controller base。

### 目前提供的功能（宣告）

- `MemRegionT`
- `NodeStateT`
- `CtxT`
  - initrd range
  - stdout target path
  - UART / AUX / GPIO base
  - interrupt controller base
  - RAM regions
  - `child_addr_cells[]` / `child_size_cells[]`
  - `node_state[]`
- 對外函式
  - `void InitialDtbCtx(CtxT* dtb_ctx)`
  - `void DtbCollectHandler(...)`
  - `void SaveChildCellAddr(...)`
  - `void SaveChildCellSize(...)`
  - `bool PathEqualsBase(...)`
  - `unsigned long Decode_Initrd_Addr(...)`

## `fdtb.c`

### 檔案定位

實作路徑比對、initrd 位址解碼、context 初始化、child-cell 保存與 `DtbCollectHandler()`。

### 目前提供的功能（實作）

#### `bool PathEqualsBase(char** nodeStack, int depth, string_t* segments, int compareDepth)`

- 比對目前 `nodeStack` 是否符合指定 base path
- 會忽略 node name 中 `@xxxx` 的 unit-address

#### `unsigned long Decode_Initrd_Addr(unsigned long valuePtr, unsigned int valueLength)`

- `valueLength == 4` 時以 32-bit 方式解碼
- `valueLength == 8` 時以 64-bit 方式解碼
- 其他長度回傳 `0`

#### `void InitialDtbCtx(CtxT* dtb_ctx)`

- 清空 context
- 對部分常用 peripheral 先填入預設 base
- 初始化 `node_state[]` 與 child-cell 狀態

#### `void SaveChildCellAddr(...)` / `void SaveChildCellSize(...)`

- 保存當前 node 的 `#address-cells` / `#size-cells`
- 寫入 `child_addr_cells[depth - 1]` / `child_size_cells[depth - 1]`

#### `void DtbCollectHandler(...)`

- 僅在 `event == PROP_NODE` 時工作
- 先處理 child-cell 資訊
- 再收集：
  - `/chosen/linux,initrd-start`
  - `/chosen/linux,initrd-end`
  - `/soc/serial@*`
  - `/soc/aux@*`
  - `/soc/gpio@*`
  - `/soc/interrupt-controller@*`
- 解析 `reg` 時會依 parent 的 `#address-cells/#size-cells` 解碼

## 現況注意

- `early_common.h` 走固定 base，`common.*` / `dtb.*` / `fdtb.*` 走 DTB 解析後的 runtime base；兩者不是同一路徑。
- `dtb.c` 只負責 traversal，不直接決定要保存哪些欄位；真正的收集邏輯在 `fdtb.c`。
- `stdout_target_segments` 與 `mem_regions` 目前屬保留欄位，尚未成為主流程核心依賴。

## 跨文檔導讀

- Boot 階段若要理解 early 固定位址與 kernel 載入位址，請看 [Boot/Boot.md](../Boot/Boot.md)。
- Driver 階段若要理解 dynamic MMIO 與 UART / Mailbox / Timer 的 base 來源，請看 [Driver/Driver.md](../Driver/Driver.md)。
- Kernel 啟動流程若要理解 `dtb_ctx` 是在哪裡初始化與消費，請看 [Kernel/Kernel.md](../Kernel/Kernel.md)。
- Shell 的 `ls` / `cat` 若要理解 initramfs 起點是怎麼來的，請看 [Shell/Shell.md](../Shell/Shell.md) 與 [FileSystem/FileSystem.md](../FileSystem/FileSystem.md)。

## 閱讀建議

- 想看「Bootloader 如何把 DTB 傳給 Kernel」：先看 [Boot/Boot.md](../Boot/Boot.md)，再看本檔的 `dtb/fdtb/common` 段落。
- 想看「Kernel 為何能改用 DTB 中的 UART base」：先看本檔，再看 [Driver/Driver.md](../Driver/Driver.md) 與 [Kernel/Kernel.md](../Kernel/Kernel.md)。
- 想看「initramfs / CPIO 命令資料從哪裡來」：先看本檔的 `fdtb`，再看 [FileSystem/FileSystem.md](../FileSystem/FileSystem.md) 與 [Shell/Shell.md](../Shell/Shell.md)。
