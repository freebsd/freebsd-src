#!/bin/bash
#
# STRATIFY Build System for Samsung Exynos SoCs
# UOS Mobile OS (FreeBSD-based)
#
# Inspired by MediaTek's STRATIFY build approach for modular kernel builds
#
# Usage:
#   ./build.sh [OPTIONS]
#
# Options:
#   --config=NAME    Build config (S5E9925, S5E9810, S5E8845, etc.)
#   --dtb            Include device tree blob
#   --bootimg        Generate boot.img via mkbootimg
#   --dtbo           Generate dtbo.img for device tree overlays
#   --output-dir=DIR Output directory (default: ./output)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ROOT="$(dirname "$SCRIPT_DIR")/.."
BUILD_DIR="${SRC_ROOT}/..
OUTPUT_DIR="${SCRIPT_DIR}/output"
BUILD_TOP="${SRC_ROOT}/.."

# Default values
CONFIG_NAME=""
BUILD_DTB=false
BUILD_BOOTIMG=false
BUILD_DTBO=false
SKIP_VERITY=false
SKIP_VERIFICATION=false

# Parse arguments
for arg in "$@"; do
    case $arg in
        --config=*)
            CONFIG_NAME="${arg#*=}"
            ;;
        --dtb)
            BUILD_DTB=true
            ;;
        --bootimg)
            BUILD_BOOTIMG=true
            ;;
        --dtbo)
            BUILD_DTBO=true
            ;;
        --skip-verity)
            SKIP_VERITY=true
            ;;
        --skip-verification)
            SKIP_VERIFICATION=true
            ;;
        --output-dir=*)
            OUTPUT_DIR="${arg#*=}"
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --config=NAME    Build config (S5E9925, S5E9810, S5E8845, etc.)"
            echo "  --dtb            Include device tree blob"
            echo "  --bootimg        Generate boot.img via mkbootimg"
            echo "  --dtbo           Generate dtbo.img"
            echo "  --output-dir=DIR Output directory"
            exit 0
            ;;
    esac
done

if [ -z "$CONFIG_NAME" ]; then
    echo "Error: --config is required"
    echo "Available configs: S5E9925, S5E9810, S5E8845, S5E8535, S5E9830, S5E9825, S5E5510"
    exit 1
fi

CONFIG_FILE="${SCRIPT_DIR}/configs/${CONFIG_NAME}.conf"
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Config file not found: $CONFIG_FILE"
    exit 1
fi

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Detect toolchain
if ! command -v aarch64-freebsd-ld &> /dev/null; then
    log_error "FreeBSD aarch64 toolchain not found"
    echo "Install: pkg install llvm-aarch64-cross"
    exit 1
fi

log_info "Toolchain: $(aarch64-freebsd-ld --version | head -1)"

# Build kernel using FreeBSD build system
log_info "Building kernel for Exynos ${CONFIG_NAME}..."
cd "$SRC_ROOT"

make -C "$SRC_ROOT" \
    KERNCONF="${CONFIG_NAME}" \
    KERN_DIR="${SCRIPT_DIR}" \
    __MAKE_CONF="${SCRIPT_DIR}/make.conf" \
    buildkernel

if [ $? -ne 0 ]; then
    log_error "Kernel build failed"
    exit 1
fi

log_info "Kernel build complete"

# Build device tree
if [ "$BUILD_DTB" = true ]; then
    log_info "Building device tree..."
    DTB_OUTPUT="${OUTPUT_DIR}/${CONFIG_NAME}.dtb"
    make -C "${SRC_ROOT}/sys/boot/fdt" \
        DTB_SOC=samsung_exynos \
        DTB_MODEL="${CONFIG_NAME}" \
        OUTPUT="$DTB_OUTPUT"
fi

# Generate boot.img for Samsung bootloader (Loke/Spring)
if [ "$BUILD_BOOTIMG" = true ]; then
    log_info "Generating boot.img for Samsung bootloader..."
    
    KERNEL_BIN="${SRC_ROOT}/sys/arm64/compile/${CONFIG_NAME}/kernel.bin"
    
    # Build mkbootimg if needed
    if ! command -v mkbootimg &> /dev/null; then
        log_info "Building mkbootimg..."
        make -C "${SRC_ROOT}/tools/mkbootimg"
    fi
    
    # Samsung bootloader uses ELF format with custom header
    # The ELF loader handles Loke/Spring bootloader protocols
    BOOTIMG_ARGS="--kernel $KERNEL_BIN --ramdisk ${OUTPUT_DIR}/ramdisk.img"
    
    if [ "$SKIP_VERITY" = true ]; then
        BOOTIMG_ARGS="$BOOTIMG_ARGS --avb-disable-verity"
    fi
    
    if [ "$SKIP_VERIFICATION" = true ]; then
        BOOTIMG_ARGS="$BOOTIMG_ARGS --avb-disable-verification"
    fi
    
    mkbootimg $BOOTIMG_ARGS --output "${OUTPUT_DIR}/boot.img"
    
    log_info "Generated boot.img: ${OUTPUT_DIR}/boot.img"
fi

# Generate dtbo.img
if [ "$BUILD_DTBO" = true ]; then
    log_info "Generating device tree overlay image..."
    
    make -C "${SRC_ROOT}/sys/boot/fdt" \
        DTB_OVERLAY=true \
        OUTPUT="${OUTPUT_DIR}/dtbo.img"
fi

log_info "Build complete. Output in: $OUTPUT_DIR"
ls -la "$OUTPUT_DIR"