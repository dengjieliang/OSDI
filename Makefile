# ==== cross toolchain 設定 ====
CROSS_COMPILE ?= aarch64-linux-gnu-

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

# 定義 QEMU 指令
QEMU    := qemu-system-aarch64

# ==== 路徑 ====
INC_DIR   := header
BUILD_DIR := build

# ==== 編譯選項 ====
# [修正 1] 強制關閉最佳化 (-O0)，避免 Driver 讀取迴圈被優化掉
CFLAGS  := -Wall -O0 -ffreestanding -nostdlib -nostartfiles -g -mgeneral-regs-only -I$(INC_DIR)
# [修正 2] 組合語言也同步改成 -O0，方便 GDB 除錯
ASFLAGS := -Wall -O0 -ffreestanding -nostdlib -nostartfiles -g

# ==== 找出所有 .c / .S / .s ====
C_SRCS := $(wildcard CFile/*.c)
S_SRCS := $(wildcard Assembly/*.S)
s_SRCS := $(wildcard Assembly/*.s)

# ==== Object 檔建立 ====
# 1. 先把 boot.o 放進來 (Entry Point)
OBJS := $(BUILD_DIR)/boot.o

# 2. 再加入所有的 C 檔案
OBJS += $(patsubst CFile/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))

# 3. 最後加入剩下的 .S 檔案 (排除 boot.S)
S_REST := $(filter-out Assembly/boot.S, $(S_SRCS))
OBJS += $(patsubst Assembly/%.S,$(BUILD_DIR)/%.o,$(S_REST))

# 4. 加入小寫 .s 檔案
OBJS += $(patsubst Assembly/%.s,$(BUILD_DIR)/%.o,$(s_SRCS))

ELF := $(BUILD_DIR)/kernel8.elf
IMG := $(BUILD_DIR)/kernel8.img

# ==== default target ====
.PHONY: all
all: $(IMG)

# 確保 build/ 目錄存在
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ==== 產生 binary image ====
$(IMG): $(ELF)
	$(OBJCOPY) -O binary $< $@

# ==== 連結 ====
$(ELF): $(OBJS) linker.ld | $(BUILD_DIR)
	$(LD) -T linker.ld -o $@ $(OBJS)

# ==== 編譯 C 檔 ====
$(BUILD_DIR)/%.o: CFile/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ==== 編譯組合語言 ====
$(BUILD_DIR)/%.o: Assembly/%.S | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: Assembly/%.s | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

# ==== clean ====
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# ==== QEMU 參數設定 (Python 傳檔用) ====
# [修正 3] 關鍵修正！
# 第一個 -serial null 是給 UART0 (我們沒用)
# 第二個 -serial tcp... 是給 UART1 (Mini UART，我們在用的)
# 使用 server (blocking) 讓 QEMU 暫停等待 Python 連線
QEMU_OPTS := -machine raspi3b -kernel $(IMG) -display none -serial null -serial tcp:127.0.0.1:8888,server

.PHONY: qemu
qemu: $(IMG)
	$(QEMU) $(QEMU_OPTS)

# ==== GDB 用的 QEMU (除錯用) ====
# [修正 4] 同步修正 UART Mapping
# 這裡使用 nowait，避免如果你只開 GDB 沒開 Python 時 QEMU 卡在啟動階段
QEMU_GDB_OPTS := -machine raspi3b -kernel $(IMG) -display none -serial null -serial tcp:127.0.0.1:8888,server,nowait -S -s

.PHONY: qemu-gdb
qemu-gdb: $(IMG)
	$(QEMU) $(QEMU_GDB_OPTS)