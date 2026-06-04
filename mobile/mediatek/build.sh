#!/bin/sh
#
# MediaTek SoC kernel build script
# Builds FreeBSD kernel and images for MediaTek platforms
#

set -e

# Supported boards
SUPPORTED_BOARDS="MT6893 MT6877 MT6833 MT6895 MT6983 MT6985 MT6989 MT6991"

# Detect board from argument or environment
BOARD="${1:-${MTK_BOARD}}"

if [ -z "$BOARD" ]; then
    echo "Error: No board specified"
    echo "Usage: $0 <BOARD> or set MTK_BOARD environment variable"
    echo "Supported boards: $SUPPORTED_BOARDS"
    exit 1
fi

# Validate board
case "$BOARD" in
    MT6893|MT6877|MT6833|MT6895|MT6983|MT6985|MT6989|MT6991)
        ;;
    *)
        echo "Error: Unsupported board '$BOARD'"
        echo "Supported boards: $SUPPORTED_BOARDS"
        exit 1
        ;;
esac

CONFIG_FILE="configs/${BOARD}.conf"
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Config file not found: $CONFIG_FILE"
    exit 1
fi

# Setup output directory
OUT_DIR="out"
mkdir -p "$OUT_DIR"

# Toolchain setup (clang required for MTK)
CC=${CC:-clang}
CXX=${CXX:-clang++}
AS=${AS:-clang}
LD=${LD:-lld}

# Cross-compilation settings
export TARGET=arm64
export TARGET_ARCH=arm64

# Build kernel
echo "Building kernel for $BOARD..."
make -C "$SRCROOT" KERNCONF="$BOARD" __MAKE_CONF__=/dev/null \
    TOOLCHAIN="llvm" CROSS_TOOLCHAIN="llvm" \
    WITHOUT_CLANG_BOOTSTRAP=1 WITHOUT_CLANG_ISNAN=1 \
    WITHOUT_DEBUG_FILES=1 WITHOUT_KERNEL_SYMBOLS=1

# Build DTB
echo "Building device tree blobs..."
make -C "$SRCROOT" DTB_BOARD="$BOARD"

# Determine image format based on SoC generation
case "$BOARD" in
    MT6893|MT6877|MT6833)
        KERNEL_IMAGE="zImage"
        ;;
    MT6895|MT8983|MT6983|MT6985|MT6989|MT6991)
        KERNEL_IMAGE="fitImage"
        ;;
esac

# Create boot image
echo "Creating boot image..."
KERNEL_BIN="$SRCROOT/sys/arm64/arm64/${KERNEL_IMAGE}"
if [ -f "$KERNEL_BIN" ]; then
    mkimg -s EFI -b "$KERNEL_BIN" -o "$OUT_DIR/boot.img"
fi

# Create dtbo image
echo "Creating dtbo image..."
DTB_DIR="$SRCROOT/sys/boot/fdt/dts/mediatek"
if [ -d "$DTB_DIR" ]; then
    mkimg -s EFI -b "$DTB_DIR/${BOARD}.dtb" -o "$OUT_DIR/dtbo.img"
fi

# Create vendor boot image if vendor resources exist
VENDOR_BOOT=""
if [ -d "vendor" ]; then
    echo "Creating vendor boot image..."
    mkimg -s EFI -b vendor/boot -o "$OUT_DIR/vendor_boot.img"
    VENDOR_BOOT="$OUT_DIR/vendor_boot.img"
fi

echo ""
echo "Build complete for $BOARD"
echo "Output files in $OUT_DIR/:"
ls -la "$OUT_DIR"

exit 0