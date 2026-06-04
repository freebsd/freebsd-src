# MediaTek SoC build configuration

TOOLCHAIN_PREFIX := aarch64-linux-android-
CLANG_PATH := prebuilts/clang/host/linux-x86/clang-r383902

# Compiler settings
CC := $(CLANG_PATH)/bin/clang
AS := $(CLANG_PATH)/bin/clang
LD := $(CLANG_PATH)/bin/ld.lld
OBJCOPY := $(CLANG_PATH)/bin/llvm-objcopy

# Kernel defconfig
DEFCONFIG := defconfig

# DTB settings
DTBTOOL := $(SCRIPT_DIR)/tools/dtbTool

# Boot image settings
MKBOOTIMG := $(SCRIPT_DIR)/tools/mkbootimg

# MediaTek-specific flags
CFLAGS += -march=armv8-a+crc+crypto -mtune=cortex-a76
CFLAGS += -mfloor-rounding-cycle3-wf -moutline-atomics

# 32-bit kernel support for older bootloaders
ifeq ($(SOC_GEN), legacy)
    KERNEL_OFFSET := 0x00008000
    RAMDISK_OFFSET := 0x04000000
endif