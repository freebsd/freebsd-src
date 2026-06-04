#!/bin/bash
#
# STRATIFY Build System for Google Tensor SoCs
# UOS Mobile OS (FreeBSD-based)
#
# Uses same Exynos toolchain approach with Google-specific output
#
# Usage:
#   ./build.sh [OPTIONS]
#
# Options:
#   --config=NAME    Build config (redfin, bluejay, zuma)
#   --dtb            Include device tree blob
#   --bootimg        Generate boot.img compatible with Google bootloader
#   --output-dir=DIR Output directory (default: ./output)
#   --skip-verity    Skip AVB verity (testing)
#   --skip-verification Skip AVB verification (testing)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ROOT="$(dirname "$SCRIPT_DIR")/.."
OUTPUT_DIR="${SCRIPT_DIR}/output"

# Default values
CONFIG_NAME=""
BUILD_DTB=false
BUILD_BOOTIMG=false
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
            echo "  --config=NAME         Build config (redfin, bluejay, zuma)"
            echo "  --dtb                 Include device tree blob"
            echo "  --bootimg             Generate boot.img"
            echo "  --skip-verity         Skip AVB verity (testing)"
            echo "  --skip-verification   Skip AVB verification (testing)"
            echo "  --output-dir=DIR      Output directory"
            exit 0
            ;;
    esac
done

if [ -z "$CONFIG_NAME" ]; then
    echo "Error: --config is required"
    echo "Available configs: redfin (G1), bluejay (G2), zuma (G3)"
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

mkdir -p "$OUTPUT_DIR"

# Detect toolchain
if ! command -v aarch64-freebsd-ld &> /dev/null; then
    log_error "FreeBSD aarch64 toolchain not found"
    echo "Install: pkg install llvm-aarch64-cross"
    exit 1
fi

log_info "Toolchain: $(aarch64-freebsd-ld --version | head -1)"

# Build kernel
log_info "Building Tensor ${CONFIG_NAME} kernel..."
cd "$SRC_ROOT"

make -C "$SRC_ROOT" \
    KERNCONF="${CONFIG_NAME}" \
    KERN_DIR="${SCRIPT_DIR}" \
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
        DTB_SOC=google_tensor \
        DTB_MODEL="${CONFIG_NAME}" \
        OUTPUT="$DTB_OUTPUT"
fi

# Generate boot.img for Google bootloader
if [ "$BUILD_BOOTIMG" = true ]; then
    log_info "Generating boot.img for Google bootloader..."
    
    KERNEL_BIN="${SRC_ROOT}/sys/arm64/compile/${CONFIG_NAME}/kernel.bin"
    
    # Build mkbootimg if needed
    if ! command -v mkbootimg &> /dev/null; then
        log_info "Building mkbootimg..."
        make -C "${SRC_ROOT}/tools/mkbootimg"
    fi
    
    BOOTIMG_ARGS="--kernel $KERNEL_BIN --ramdisk ${OUTPUT_DIR}/ramdisk.img"
    BOOTIMG_ARGS="$BOOTIMG_ARGS --pagesize 4096"
    BOOTIMG_ARGS="$BOOTIMG_ARGS --header-version 3"
    
    if [ "$SKIP_VERITY" = true ]; then
        BOOTIMG_ARGS="$BOOTIMG_ARGS --avb-disable-verity"
    fi
    
    if [ "$SKIP_VERIFICATION" = true ]; then
        BOOTIMG_ARGS="$BOOTIMG_ARGS --avb-disable-verification"
    fi
    
    mkbootimg $BOOTIMG_ARGS --output "${OUTPUT_DIR}/boot.img"
    
    log_info "Generated boot.img: ${OUTPUT_DIR}/boot.img"
fi

log_info "Build complete. Output in: $OUTPUT_DIR"
ls -la "$OUTPUT_DIR"