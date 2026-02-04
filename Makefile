# ==== cross toolchain 設定 ====
CROSS_COMPILE ?= aarch64-linux-gnu-

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
QEMU    := qemu-system-aarch64

# ==== 路徑 ====
INC_DIR   := header
BUILD_DIR := build

# ==== 編譯選項 ====
CFLAGS  := -Wall -O0 -ffreestanding -nostdlib -nostartfiles -g -mgeneral-regs-only -I$(INC_DIR)
ASFLAGS := -Wall -O0 -ffreestanding -nostdlib -nostartfiles -g

# ==========================================
# 1. 定義主要檔案 (關鍵修正)
# ==========================================
# 修正：根據你的編譯 Log，你的檔案叫 bootloader_main.c
C_BOOT_SRC   := CFile/bootloader_main.c
C_KERNEL_SRC := CFile/kernel_main.c

# 2. 找出所有原始碼
C_ALL := $(wildcard CFile/*.c)
S_ALL := $(wildcard Assembly/*.S)
s_ALL := $(wildcard Assembly/*.s)

# 3. 分離 "共用檔"
# 技巧：直接把上面定義的 main 檔案過濾掉，剩下的就是 uart.c, utils.c 等
C_COMMON := $(filter-out $(C_BOOT_SRC) $(C_KERNEL_SRC) CFile/shell.c, $(C_ALL))

# ==========================================
# 4. 定義 Object 檔案 (.o)
# ==========================================
# 轉換共用 C 檔 -> .o
OBJ_COMMON_C := $(patsubst CFile/%.c, $(BUILD_DIR)/%.o, $(C_COMMON))

# 轉換 Assembly 檔 (排除 boot.S，因為它要放第一個) -> .o
OBJ_ASM := $(patsubst Assembly/%.S, $(BUILD_DIR)/%.o, $(filter-out Assembly/boot.S, $(S_ALL))) \
           $(patsubst Assembly/%.s, $(BUILD_DIR)/%.o, $(s_ALL))

# 組合共用 Object 清單
OBJ_COMMON_ALL := $(OBJ_COMMON_C) $(OBJ_ASM)

# 設定 Entry Point Object (boot.o 必須在最前面)
BOOT_START_OBJ := $(BUILD_DIR)/boot.o

# ==== 定義最終兩組 Object 清單 (關鍵修正) ====
# Bootloader = boot.o + 共用.o + bootloader_main.o
OBJS_FOR_BOOTLOADER := $(BOOT_START_OBJ) $(OBJ_COMMON_ALL) $(BUILD_DIR)/bootloader_main.o

# Kernel = boot.o + 共用.o + kernel_main.o
OBJS_FOR_KERNEL     := $(BOOT_START_OBJ) $(OBJ_COMMON_ALL) $(BUILD_DIR)/shell.o $(BUILD_DIR)/kernel_main.o

# ==========================================
# 5. 定義輸出檔名
# ==========================================
IMG_BOOT   := $(BUILD_DIR)/bootloader.img
ELF_BOOT   := $(BUILD_DIR)/bootloader.elf

IMG_KERNEL := $(BUILD_DIR)/kernel8.img
ELF_KERNEL := $(BUILD_DIR)/kernel8.elf

.PHONY: all clean qemu qemu-gdb

# make all 會同時產生兩個 img
all: $(IMG_BOOT) $(IMG_KERNEL)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ==========================================
# 6. 建置規則
# ==========================================

# ---- Bootloader (搬運工) ----
$(IMG_BOOT): $(ELF_BOOT)
	$(OBJCOPY) -O binary $< $@

$(ELF_BOOT): $(OBJS_FOR_BOOTLOADER) linker_boot.ld | $(BUILD_DIR)
	$(LD) -T linker_boot.ld -o $@ $(OBJS_FOR_BOOTLOADER)

# ---- Kernel (Payload) ----
$(IMG_KERNEL): $(ELF_KERNEL)
	$(OBJCOPY) -O binary $< $@

$(ELF_KERNEL): $(OBJS_FOR_KERNEL) linker_kernel.ld | $(BUILD_DIR)
	$(LD) -T linker_kernel.ld -o $@ $(OBJS_FOR_KERNEL)

# ---- 編譯 C 與 Assembly ----
$(BUILD_DIR)/%.o: CFile/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: Assembly/%.S | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: Assembly/%.s | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

# ==========================================
# 7. QEMU 執行設定
# ==========================================

# 定義 DTB 檔案名稱 (方便管理)
DTB_FILE := bcm2710-rpi-3-b-plus.dtb

# 1. make qemu 用
# [新增] -dtb $(DTB_FILE)
QEMU_OPTS := -machine raspi3b -kernel $(IMG_BOOT) -initrd initramfs.cpio -dtb $(DTB_FILE) -display none -serial null -serial tcp:127.0.0.1:8888,server

qemu: $(IMG_BOOT) $(IMG_KERNEL)
	$(QEMU) $(QEMU_OPTS)

# 2. make qemu-gdb 用
# [新增] -dtb $(DTB_FILE)
QEMU_GDB_OPTS := -machine raspi3b -kernel $(IMG_BOOT) -initrd initramfs.cpio -dtb $(DTB_FILE) -display none -serial null -serial tcp:127.0.0.1:8888,server,nowait -S -s

qemu-gdb: $(IMG_BOOT) $(IMG_KERNEL)
	$(QEMU) $(QEMU_GDB_OPTS)