# Google Tensor SoC build configuration

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

# Boot image settings - Same as Qualcomm for Tensor
MKBOOTIMG := $(SCRIPT_DIR)/tools/mkbootimg
BOOT_HEADER_VERSION := 2

# AVB settings
AVBTOOL := $(SCRIPT_DIR)/tools/avb.py

# Google-specific flags
CFLAGS += -march=armv8-a+crc+crypto -mtune=cortex-a78
CFLAGS += -moutline-atomics

# DTB appended
DTB_APPEND := true