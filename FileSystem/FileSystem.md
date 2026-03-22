# FileSystem

## 對應總說明章節

- `7. CPIO 檔案系統解析器 (CPIO Parser)`
- `15. 作業系統核心主程式 (Kernel Main)` 中與 initramfs / `dtb_ctx.initrd_start` 相關的部分
- `11. 簡易命令列介面 (Shell)` 中的 `ls` / `cat`

## 檔案定位

此資料夾目前只有 CPIO 解析器，負責在 kernel 端讀取 initramfs 內容。它不負責 initramfs 起點如何取得，而是依賴 DTB 解析後保存於 `dtb_ctx` 的 `initrd_start` / `initrd_end`。因此這層的定位是「已知 initramfs 範圍後，提供列目錄、找檔案與讀檔能力」。

## 內容概述

- `cpio.h`：宣告 CPIO header 結構與對外 API。
- `cpio.c`：提供檔名列舉、檔案搜尋、內容輸出與 next-header 走訪邏輯。

## 目前提供的功能

## `cpio.h`

### 檔案定位

提供 CPIO（initramfs）存取相關的介面宣告，供 kernel 其他模組呼叫來列出檔名、搜尋檔案、取得檔案資料或輸出檔案內容。

### 目前提供的功能

- `int CpioGetFilesHeaderName(void *file_header)`
  - 逐筆走訪 CPIO entries，輸出檔名並回傳檔案數量
- `bool CpioGetFileData(void *file_header, char* file_name, void **out_data, unsigned long *out_size)`
  - 搜尋指定檔案，回傳內容起點與大小，不直接輸出內容
- `bool CpioGetFileContext(void *file_header, char* file_name)`
  - 搜尋指定檔案並直接輸出內容

## `cpio.c`

### 檔案定位

提供 CPIO `newc` 格式的最小可用解析與輸出邏輯。

### 目前提供的功能（實作）

#### 1) 內部資料結構：`cpio_header_t`

- 定義 `newc` header 的固定欄位
- 重要欄位包括：
  - `c_magic[6]`
  - `c_namesize[8]`
  - `c_filesize[8]`
- 程式碼以 header 固定長度 110 bytes 為基準進行位移

#### 2) `int CpioGetFilesHeaderName(void *file_header)`

- 檢查起始 `c_magic` 是否為 `070701`
- 逐筆解析 `c_namesize`
- 以 `TRAILER!!!` 作為結束條件
- 以 `filename_ptr = (char*)header + 110` 取得檔名起點
- 逐字輸出檔名並累計 `file_count`
- 透過 `GetNextHeader()` 推進到下一個 entry
- 回傳檔案數量

### 現況輸出行為

- 每個檔名輸出後會補一個換行

#### 3) `bool CpioGetFileContext(void *file_header, char* file_name)`

- 檢查起始 `c_magic`
- 迴圈中透過 `CompareFileName()` 比對檔名
- 若匹配成功：
  - 解析 `c_filesize` 與 `c_namesize`
  - 跳過 header 與檔名區
  - 以 `ALIGN4()` 對齊到內容起點
  - 逐 byte 輸出檔案內容
  - 最後補 `\n`
  - 回傳 `true`
- 若遍歷完整個 CPIO 都沒找到，回傳 `false`

#### 4) `bool CpioGetFileData(...)`

- 搜尋指定檔名
- 找到後回傳資料起點與大小
- 目的是讓其他模組可以直接使用檔案內容，而不一定經過 UART 輸出路徑

#### 5) 內部 helper：`static void* GetNextHeader(void *file_header)`

- 解析目前 entry 的 `c_namesize` 與 `c_filesize`
- 依序：
  - 跳過 110-byte header
  - 跳過檔名區
  - 對齊到 4 bytes
  - 跳過檔案內容
  - 再對齊到 4 bytes
- 若新位置的 `c_magic` 不為 `070701`，回傳 `NULL`

#### 6) 內部 helper：`static bool CompareFileName(cpio_header_t* file, char* cmp_name, int cmp_size)`

- 將 `file` 位移到檔名起點
- 以 `strncmp()` 與呼叫端提供的 `cmp_size` 做比較

## 現況注意

- 這一層目前聚焦在 initramfs 讀取，不處理掛載、寫入或一般檔案系統抽象。
- CPIO parser 是否能開始工作，取決於 `dtb_ctx.initrd_start` 是否已由 DTB 解析正確填入。
- `CompareFileName()` 的精確語意取決於呼叫端傳入的 `cmp_size`。

## 跨資料夾導讀

- 若要理解 initramfs 的起點如何從 DTB `/chosen` 取得，請先看 [Board/Board.md](../Board/Board.md)。
- 若要理解 `ls` / `cat` 命令如何把 `dtb_ctx.initrd_start` 交給 CPIO parser，請看 [Shell/Shell.md](../Shell/Shell.md)。
- 若要理解 kernel 何時完成 DTB 解析並進入 shell，請看 [Kernel/Kernel.md](../Kernel/Kernel.md)。

## 閱讀建議

- 想看「為什麼 shell 可以列出 LabTest 打包進去的檔案」：先看 [Kernel/Kernel.md](../Kernel/Kernel.md) 與 [Board/Board.md](../Board/Board.md)，再看本檔。
- 想看「`ls` / `cat` 的命令端行為」：本檔搭配 [Shell/Shell.md](../Shell/Shell.md) 一起看。
