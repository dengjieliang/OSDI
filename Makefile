# ==== cross toolchain 設定 ====
CROSS_COMPILE ?= aarch64-linux-gnu-

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

# 定義 QEMU 指令
QEMU    := qemu-system-aarch64

# ==== 路徑 ====
# 原本是 src / include，改成符合你現在的資料夾
INC_DIR   := header
BUILD_DIR := build

# ==== 編譯選項 ====
# 這個參數告訴編譯器：絕對不要用 FPU/SIMD 暫存器，只准用一般暫存器！
CFLAGS  := -Wall -O2 -ffreestanding -nostdlib -nostartfiles -g -mgeneral-regs-only -I$(INC_DIR)
ASFLAGS := -Wall -O2 -ffreestanding -nostdlib -nostartfiles -g

# ==== 找出所有 .c / .S / .s ====
# C 檔在 CFile/，組語在 Assembly/
C_SRCS := $(wildcard CFile/*.c)
S_SRCS := $(wildcard Assembly/*.S)
s_SRCS := $(wildcard Assembly/*.s)

# ==== 修正後：強制讓 boot.o 排在第一個 ====
# 1. 先把 boot.o 放進來
OBJS := $(BUILD_DIR)/boot.o

# 2. 再加入所有的 C 檔案
OBJS += $(patsubst CFile/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))

# 3. 最後加入剩下的 .S 檔案 (要排除掉已經加過的 boot.S，避免重複)
#    這裡用 filter-out 把 Assembly/boot.S 過濾掉
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

# ==== 編譯組合語言（.S / .s） ====
$(BUILD_DIR)/%.o: Assembly/%.S | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: Assembly/%.s | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

# ==== clean ====
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# ==== 用 qemu 測試 (直接跑，不暫停) ====
# 加上 -serial null -serial stdio 來監聽 Mini UART
# 2025/12/08 建立虛擬 PTY，讓 Python 可以連進來
QEMU_OPTS := -machine raspi3b -kernel $(IMG) -display none -serial tcp:127.0.0.1:8888,server,nowait

.PHONY: qemu
qemu: $(IMG)
	$(QEMU) $(QEMU_OPTS)

# ==== 給 gdb 用的 QEMU ====
# 一樣要加上 serial mapping，否則就算 GDB continue 了也看不到字
QEMU_GDB_OPTS := -machine raspi3b -kernel $(IMG) -display none -serial null -serial stdio -S -s

.PHONY: qemu-gdb
qemu-gdb: $(IMG)
	$(QEMU) $(QEMU_GDB_OPTS)
