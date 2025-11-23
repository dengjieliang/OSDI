# ==== cross toolchain 設定 ====
CROSS_COMPILE ?= aarch64-linux-gnu-

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

# ==== 路徑 ====
# 原本是 src / include，改成符合你現在的資料夾
INC_DIR   := header
BUILD_DIR := build

# ==== 編譯選項 ====
CFLAGS  := -Wall -O2 -ffreestanding -nostdlib -nostartfiles -g -I$(INC_DIR)
ASFLAGS := -Wall -O2 -ffreestanding -nostdlib -nostartfiles -g

# ==== 找出所有 .c / .S / .s ====
# C 檔在 CFile/，組語在 Assembly/
C_SRCS := $(wildcard CFile/*.c)
S_SRCS := $(wildcard Assembly/*.S)
s_SRCS := $(wildcard Assembly/*.s)

# 把 CFile/xxx.c、Assembly/xxx.S 轉成 build/xxx.o
OBJS  := $(patsubst CFile/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
OBJS += $(patsubst Assembly/%.S,$(BUILD_DIR)/%.o,$(S_SRCS))
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

# ==== 用 qemu 測試 ====
QEMU      := qemu-system-aarch64
QEMU_OPTS := -machine raspi3b -kernel $(IMG) -display none -d in_asm
# 如果你的 qemu 沒有 raspi3b，可以改成：
# QEMU_OPTS := -machine virt -cpu cortex-a53 -kernel $(IMG) -display none -d in_asm

.PHONY: qemu
qemu: $(IMG)
	$(QEMU) $(QEMU_OPTS)

# ==== 給 gdb 用的 QEMU（停在開機點，開 gdb 1234 port）====
QEMU_GDB_OPTS := -machine raspi3b -kernel $(IMG) -display none -S -s

.PHONY: qemu-gdb
qemu-gdb: $(IMG)
	$(QEMU) $(QEMU_GDB_OPTS)
