# Lib

## 對應總說明章節

- `4. 字串處理函式庫 (String Library)`
- `5. 通用工具 (Utilities)`

## 檔案定位

此資料夾放的是不直接碰硬體、但會被多個模組共用的基礎函式：字串處理、記憶體複製、位元組整合與基本數值轉換。它們支撐 shell 的命令解析、CPIO 比對檔名、DTB / FDT 解碼與其他 helper 邏輯。

## 內容概述

- `base.h`：提供全專案共用的基礎常數 / 型別 / 對齊與容量巨集。
- `string.h` / `string.c`：提供字串比較、字元掃描與長度計算。
- `utils.h` / `utils.c`：提供 `memcpy`、hex 轉換、bit reversal 與 byte-combine helper。

## 目前提供的功能

## `base.h`

### 檔案定位

集中管理專案共用的基礎定義，作為 `common.h` 內 primitive macro/type 的重構遷移目標。

### 目前提供的功能（宣告）

- `NULL`
- `bool` / `true` / `false`（非 C++）
- `ALIGN4(x)`
- `ALIGN8(x)`
- `MAX_ARGS`
- `MAX_STRING_SIZE`

### 現況注意

- 基礎定義已集中到 `base.h`，並完成第一波 include 切換；行為維持與切換前一致。

## `string.h`

### 檔案定位

宣告專案自製的字串 / 字元掃描介面，並提供輕量字串描述型別 `string_t`。

### 目前提供的功能（宣告）

- `typedef struct string { char* string_ptr; unsigned int size; } string_t;`
- `int strcmp(const char *s1, const char *s2);`
- `int strncmp(const char *s1, const char *s2, unsigned long read_byte);`
- `unsigned int strcspn(const char *s, const char reject, int max_len);`
- `unsigned long strlen(const char *s);`
- `char* strncpy(const char *s1, const char *s2, unsigned long read_byte);`

### 現況注意

- `strcspn()` 的介面與標準 libc 不同；此版本只支援單一 reject 字元，並額外提供 `max_len` 上限。

## `string.c`

### 檔案定位

實作 `string.h` 宣告的字串函式；其中 `strlen()` 會使用 `MAX_STRING_SIZE` 作為掃描上限。

### 目前提供的功能（實作）

#### `strcmp(const char* s1, const char* s2)`

- 逐 byte 比較直到任一字元為 `\0` 或發現不同字元
- 回傳 `(unsigned char)*s1 - (unsigned char)*s2`

#### `strncmp(const char *s1, const char *s2, unsigned long read_byte)`

- `read_byte == 0` 時直接回傳 `0`
- 最多比較 `read_byte` 次
- 遇到不同字元則跳出
- 遇到 `\0` 時提前返回 `0`
- 最後回傳最後比較到的字元差值

#### `strcspn(const char *s, const char reject, int max_len)`

- 在最多 `max_len` 的範圍內，尋找第一個 `\0` 或等於 `reject` 的字元
- 找到時回傳其 index
- 若掃描滿 `max_len` 仍未命中，回傳 `max_len`

#### `strlen(const char *s)`

- `s == NULL` 時回傳 `0`
- 先前進到 8-byte 對齊邊界
- 之後以 64-bit word scan 尋找 `\0`
- 若偵測到可能含 `\0` 的 word，再回到 byte 模式逐字確認
- 若掃描到 `MAX_STRING_SIZE` 仍未遇到 `\0`，回傳上限內的長度

#### `strncpy(const char *s1, const char *s2, unsigned long read_byte)`

- 複製來源字串到目的緩衝區，最多 `read_byte` bytes
- 實作會先以 `strlen(s2)` 計算來源長度，再決定 `copy_len`
- 回傳目的位址

### 現況注意

- `strlen()` 的 word-scan 假設 `unsigned long` 為 64-bit，AArch64 環境成立。
- 若字串在 `MAX_STRING_SIZE` 範圍內沒有 `\0`，回傳值會被上限截斷。

## `utils.h`

### 檔案定位

宣告專案通用工具函式介面。

### 目前提供的功能（宣告）

- `void *memcpy(void *dest, const void *src, unsigned long n);`
- `int hex2int(char *hex, int n);`
- `unsigned long hex2UnsignedLong(char *hex, int n);`
- `bool is_all_digits(const char* string);`
- `unsigned int aoti(char* string);`
- `unsigned long strtoul(char* string);`
- `double parse_seconds_to_double(const char* string);`
- `unsigned int reverseint(unsigned int number);`
- `unsigned int BigEndianToLittleEndian(void* byte);`
- `unsigned long long CombineByte(void* byte, unsigned int n);`

## `utils.c`

### 檔案定位

實作 `utils.h` 宣告的工具函式。

### 目前提供的功能（實作）

#### `memcpy(void *dest, const void *src, unsigned long n)`

- 以 byte-by-byte 方式複製 `n` bytes
- 回傳 `dest`

#### `hex2int(char *hex, int n)`

- 將長度為 `n` 的字元序列視為 16 進位數字，轉成 `int`
- 每輪先 `result *= 16`，再依字元範圍累加數值

#### `hex2UnsignedLong(char *hex, int n)`

- 將長度為 `n` 的 16 進位字串轉為 `unsigned long`

#### `is_all_digits(const char* string)`

- 驗證字串是否全部由十進位數字組成
- `NULL` 或空字串會回傳 `false`

#### `aoti(char* string)`

- 將十進位字串轉為 `unsigned int`
- 讀取時最多處理 10 位數

#### `strtoul(char* string)`

- 將十進位字串轉為 `unsigned long`
- 讀取時最多處理 20 位數

#### `parse_seconds_to_double(const char* string)`

- 將非負十進位字串（可含一個 `.`）轉為 `double`
- 以整數段與小數段累加方式解析

#### `unsigned int reverseint(unsigned int number)`

- 將 `number` 的 bit 序做反轉（bit-reversal）
- 以遮罩與 block swap 的方式逐步交換相鄰區塊

#### `unsigned int BigEndianToLittleEndian(void* byte)`

- 將 `byte` 指向的 4 bytes 視為 big-endian 序列，組成一個 `unsigned int`

#### `unsigned long long CombineByte(void* byte, unsigned int n)`

- 以每 4 bytes 一組的方式，連續組合成一個 `unsigned long long`
- 每輪呼叫 `BigEndianToLittleEndian()` 取出 32-bit chunk
- 以 `result = (result << 32) | chunk` 串接

### 現況注意

- `memcpy()` 不做標準庫層級的效能最佳化，也不額外處理重疊區間。
- `hex2int()` / `hex2UnsignedLong()` 沒有完整輸入合法性檢查。
- `parse_seconds_to_double()` 預期輸入已先完成格式驗證。
- `CombineByte()` 若未來要用 `n > 2`，建議重新檢查目前實作的指標推進行為是否符合預期。

## 跨資料夾導讀

- 若要理解這些函式在 shell 中如何被使用，請看 [Shell/Shell.md](../Shell/Shell.md)。
- 若要理解 `utils` 中的位元組轉換如何支援 DTB / FDT 解析，請看 [Board/Board.md](../Board/Board.md)。
- 若要理解字串比較如何影響 CPIO 搜尋，請看 [FileSystem/FileSystem.md](../FileSystem/FileSystem.md)。

## 閱讀建議

- 想看「命令解析或檔名比對」：本檔搭配 [Shell/Shell.md](../Shell/Shell.md) 與 [FileSystem/FileSystem.md](../FileSystem/FileSystem.md)。
- 想看「DTB / FDT 解碼時的 byte order helper」：本檔搭配 [Board/Board.md](../Board/Board.md)。
