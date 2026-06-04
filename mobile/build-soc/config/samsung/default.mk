# Samsung/Exynos SoC build configuration

# Can use either toolchain
ifeq ($(USE_CLANG), true)
    TOOLCHAIN_PREFIX := clang
    CLANG_PATH := prebuilts/clang/host/linux-x86/clang-r383902
    CC := $(CLANG_PATH)/bin/clang
else
    TOOLCHAIN_PREFIX := aarch64-none-linux-gnu-
    CC := $(TOOLCHAIN_PREFIX)gcc
endif

# Kernel defconfig
DEFCONFIG := defconfig

# DTB settings
DTBTOOL := $(SCRIPT_DIR)/tools/dtbTool

# Boot image settings
MKBOOTIMG := $(SCRIPT_DIR)/tools/mkbootimg

# Samsung-specific flags
CFLAGS += -march=armv8-a+crc -mtune=cortex-a75

# Signature support for Knox
SIGN_TOOL := $(SCRIPT_DIR)/tools/heimdall

# Heimdall flash support
HEIMDALL_FLASH := true