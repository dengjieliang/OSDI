# Memory

## 對應總說明章節

- `6. 簡易記憶體配置 (Simple Allocator)`

## 檔案定位

此資料夾目前只有簡單配置器，提供一個線性前進、不可釋放的最小動態記憶體配置能力。它的定位是教學 / early-stage allocator，而不是完整的一般用途記憶體管理子系統。

## 內容概述

- `allocator.h`：宣告 simple allocator 對外介面。
- `allocator.c`：以 `__bss_end` 為起點、用 bump-pointer 方式逐步向上配置。

## 目前提供的功能

## `allocator.h`

### 檔案定位

宣告一個極簡的記憶體配置器（bump allocator）介面，供專案在沒有 libc malloc/free 的情境下做一次性 / 向上遞增的記憶體配置。

### 目前提供的功能（宣告）

- `void* SimpleAllocator(unsigned long size);`
  - 配置 `size` bytes，成功回傳指標，失敗回傳 `NULL`

## `allocator.c`

### 檔案定位

實作 `SimpleAllocator()`：使用 `__bss_end` 作為 heap 起點，透過靜態 `heap_ptr` 持續向上推進，提供「只增不減」的最小可用配置行為。

### 相依性（就現況）

- `allocator.h`
- `common.h`
  - `MMIO_BASE`
  - `ALIGN8()`
- linker script
  - 依賴 `__bss_end` 符號

### 目前提供的功能（實作）

#### `void* SimpleAllocator(unsigned long size)`

- 第一次呼叫時：
  - `heap_ptr = &__bss_end`
- 配置流程：
  1. `result_ptr = heap_ptr`
  2. `size = ALIGN8(size)`
  3. 若 `(unsigned long)result_ptr + size > MMIO_BASE`，回傳 `NULL`
  4. 成功則把 `heap_ptr` 推進到下一個位置
  5. 回傳 `result_ptr`

## 現況注意

- 只支援向上遞增配置，沒有 `free()`、回收機制與碎片管理。
- 上界以 `MMIO_BASE` 作為 guard，隱含假設「`.bss` 後方到 `MMIO_BASE` 之間」可作為配置空間。
- 這個 allocator 不是完整的 thread-safe / interrupt-safe 記憶體管理子系統。

## 跨資料夾導讀

- 若要理解整體 kernel 啟動與模組配置背景，可參考 [Kernel/Kernel.md](../Kernel/Kernel.md)。
- 若未來 shell / kernel 功能擴充需要動態配置，可從這一層接入。
- 若要理解 `__bss_end` 與 stack / section 佈局從哪裡來，請看 [Configuration.md](../Configuration.md) 中的 Linker Scripts 章節。

## 閱讀建議

- 想看「目前系統是否已有完整記憶體管理」：先看本檔；答案是尚未，現在只有 simple allocator。
- 想看「這個 allocator 的位址邊界假設」：本檔搭配 [Configuration.md](../Configuration.md) 中的 Linker Scripts 章節一起看。
