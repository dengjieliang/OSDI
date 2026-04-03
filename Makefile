# ==== cross toolchain 設定 ====
CROSS_COMPILE ?= aarch64-linux-gnu-

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
QEMU    := qemu-system-aarch64

# ==== 路徑 ====
BUILD_DIR := build
SRC_DIRS  := Board Boot Driver FileSystem Kernel Lib Memory Shell
INC_DIRS  := $(SRC_DIRS)

# ==== 編譯選項 ====
CFLAGS  := -Wall -O0 -ffreestanding -nostdlib -nostartfiles -g -mgeneral-regs-only $(addprefix -I,$(INC_DIRS))
ASFLAGS := -Wall -O0 -ffreestanding -nostdlib -nostartfiles -g

# ==========================================
# 1. 定義主要檔案 (關鍵修正)
# ==========================================
# 修正：根據你的編譯 Log，你的檔案叫 bootloader_main.c
C_BOOT_SRC      := Boot/bootloader_main.c
C_KERNEL_SRC    := Kernel/kernel_main.c
C_EXCEPTION_SRC := Kernel/exception.c
C_SHELL_SRC     := Shell/shell.c
C_TIME_SRC      := Driver/time.c

# 2. 找出所有原始碼
C_ALL := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.c))
S_ALL := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.S))
s_ALL := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.s))

# 3. 分離 "共用檔"
# 技巧：直接把上面定義的 main 檔案過濾掉，剩下的就是 uart.c, utils.c 等
C_COMMON := $(filter-out $(C_BOOT_SRC) $(C_KERNEL_SRC) $(C_EXCEPTION_SRC) $(C_SHELL_SRC) $(C_TIME_SRC), $(C_ALL))

# ==========================================
# 4. 定義 Object 檔案 (.o)
# ==========================================
# 轉換共用 C 檔 -> .o
OBJ_COMMON_C := $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_COMMON))

# 轉換 Assembly 檔
OBJ_ASM_COMMON := $(patsubst %.S, $(BUILD_DIR)/%.o, $(filter-out Boot/boot.S Kernel/exception_table.S Kernel/user_mode_entry.S, $(S_ALL))) \
				  $(patsubst %.s, $(BUILD_DIR)/%.o, $(s_ALL))

OBJ_ASM_KERNEL := $(BUILD_DIR)/Kernel/exception_table.o $(BUILD_DIR)/Kernel/user_mode_entry.o

# 組合共用 Object 清單
OBJ_COMMON_ALL := $(OBJ_COMMON_C) $(OBJ_ASM_COMMON)
OBJ_COMMON_BOOT := $(filter-out $(BUILD_DIR)/Driver/uart.o $(BUILD_DIR)/FileSystem/cpio.o $(BUILD_DIR)/Kernel/timer_manager.o $(BUILD_DIR)/Kernel/io_task_queue.o,$(OBJ_COMMON_ALL))
OBJ_COMMON_KERNEL := $(OBJ_COMMON_ALL)

# 設定 Entry Point Object (boot.o 必須在最前面)
BOOT_START_OBJ := $(BUILD_DIR)/Boot/boot.o
EXCEPTION_C_OBJ := $(BUILD_DIR)/Kernel/exception.o

# ==== 定義最終兩組 Object 清單 (關鍵修正) ====
# Bootloader = boot.o + boot 專用共用.o + bootloader_main.o
OBJS_FOR_BOOTLOADER := $(BOOT_START_OBJ) $(OBJ_COMMON_BOOT) $(BUILD_DIR)/Boot/bootloader_main.o

# Kernel = boot.o + 共用.o + kernel_main.o
OBJS_FOR_KERNEL     := $(BOOT_START_OBJ) $(OBJ_COMMON_KERNEL) $(OBJ_ASM_KERNEL) $(EXCEPTION_C_OBJ) $(BUILD_DIR)/Driver/time.o $(BUILD_DIR)/Shell/shell.o $(BUILD_DIR)/Kernel/kernel_main.o

# ==========================================
# 5. 定義輸出檔名
# ==========================================
IMG_BOOT   := $(BUILD_DIR)/bootloader.img
ELF_BOOT   := $(BUILD_DIR)/bootloader.elf

IMG_KERNEL := $(BUILD_DIR)/kernel8.img
ELF_KERNEL := $(BUILD_DIR)/kernel8.elf

LABTEST_DIR    := LabTest
INITRAMFS_IMG  := initramfs.cpio

LABTEST_ALL_FILES := $(wildcard $(LABTEST_DIR)/*)
LABTEST_SRC_S     := $(filter %.S,$(LABTEST_ALL_FILES))
LABTEST_SRC_C     := $(filter %.c,$(LABTEST_ALL_FILES))
LABTEST_SRC_H     := $(filter %.h,$(LABTEST_ALL_FILES))
LABTEST_SRC_FILES := $(LABTEST_SRC_S) $(LABTEST_SRC_C) $(LABTEST_SRC_H)

LABTEST_OBJ_S     := $(patsubst $(LABTEST_DIR)/%.S,$(LABTEST_DIR)/%.o,$(LABTEST_SRC_S))
LABTEST_BIN_S     := $(patsubst $(LABTEST_DIR)/%.S,$(LABTEST_DIR)/%.bin,$(LABTEST_SRC_S))
LABTEST_OBJ_C     := $(patsubst $(LABTEST_DIR)/%.c,$(LABTEST_DIR)/%_c.o,$(LABTEST_SRC_C))
LABTEST_OBJ_H     := $(patsubst $(LABTEST_DIR)/%.h,$(LABTEST_DIR)/%_h.o,$(LABTEST_SRC_H))
LABTEST_OBJ_FILES := $(LABTEST_OBJ_S) $(LABTEST_OBJ_C) $(LABTEST_OBJ_H)
LABTEST_GEN_FILES := $(LABTEST_OBJ_FILES) $(LABTEST_BIN_S)

LABTEST_OTHER_FILES := $(filter-out $(LABTEST_SRC_FILES) $(LABTEST_GEN_FILES) $(LABTEST_DIR)/%.o $(LABTEST_DIR)/%.elf,$(LABTEST_ALL_FILES))
LABTEST_PACK_FILES  := $(LABTEST_BIN_S) $(LABTEST_OBJ_C) $(LABTEST_OBJ_H) $(LABTEST_OTHER_FILES)

.PHONY: all clean qemu qemu-gdb compdb

# make all 會同時產生兩個 img
all: $(IMG_BOOT) $(IMG_KERNEL) $(INITRAMFS_IMG)

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

# ---- LabTest sources to object for initramfs ----
$(LABTEST_DIR)/%.o: $(LABTEST_DIR)/%.S
	$(CC) $(ASFLAGS) -c $< -o $@

$(LABTEST_DIR)/%.bin: $(LABTEST_DIR)/%.o
	$(OBJCOPY) -O binary $< $@

$(LABTEST_DIR)/%_c.o: $(LABTEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LABTEST_DIR)/%_h.o: $(LABTEST_DIR)/%.h
	$(CC) $(CFLAGS) -x c -c $< -o $@

$(INITRAMFS_IMG): $(LABTEST_PACK_FILES)
	cd $(LABTEST_DIR) && printf '%s\n' $(notdir $(LABTEST_PACK_FILES)) | cpio -o -H newc > ../$@

# ---- 編譯 C 與 Assembly ----

# Driver/time.c, Kernel/timer_manager.c, Shell/shell.c, and Lib/utils.c must agree on FP ABI
# because timeout APIs and parsing now use double in shared headers.
$(BUILD_DIR)/Driver/time.o: Driver/time.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(filter-out -mgeneral-regs-only,$(CFLAGS)) -c $< -o $@

$(BUILD_DIR)/Kernel/timer_manager.o: Kernel/timer_manager.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(filter-out -mgeneral-regs-only,$(CFLAGS)) -c $< -o $@

$(BUILD_DIR)/Shell/shell.o: Shell/shell.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(filter-out -mgeneral-regs-only,$(CFLAGS)) -c $< -o $@

$(BUILD_DIR)/Lib/utils.o: Lib/utils.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(filter-out -mgeneral-regs-only,$(CFLAGS)) -c $< -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(LABTEST_GEN_FILES) $(INITRAMFS_IMG)

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

compdb:
	@if command -v compiledb >/dev/null 2>&1; then \
		compiledb make all; \
	elif command -v bear >/dev/null 2>&1; then \
		bear -- make all; \
	else \
		echo "Error: neither 'compiledb' nor 'bear' is installed."; \
		echo "Install one of them, then run: make compdb"; \
		exit 1; \
	fi