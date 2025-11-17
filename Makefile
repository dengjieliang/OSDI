# ==== cross toolchain 設定 ====
CROSS_COMPILE ?= aarch64-linux-gnu-

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

# ==== 路徑 ====
SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build

# ==== 編譯選項 ====
CFLAGS  := -Wall -O2 -ffreestanding -nostdlib -nostartfiles -g -I$(INC_DIR)
ASFLAGS := -Wall -O2 -ffreestanding -nostdlib -nostartfiles -g

# ==== 找出所有 .c / .S / .s ====
C_SRCS := $(wildcard $(SRC_DIR)/*.c)
S_SRCS := $(wildcard $(SRC_DIR)/*.S)
s_SRCS := $(wildcard $(SRC_DIR)/*.s)

# 把 src/xxx.* 轉成 build/xxx.o  （這裡只會留下 .o，不會把原始檔混進來）
OBJS  := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
OBJS += $(patsubst $(SRC_DIR)/%.S,$(BUILD_DIR)/%.o,$(S_SRCS))
OBJS += $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/%.o,$(s_SRCS))

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
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ==== 編譯組合語言（.S / .s） ====
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
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
