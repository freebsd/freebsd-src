#!/bin/bash
# =============================================================================
# UOS Qualcomm - Create Mobile Boot Image
# =============================================================================
# Assembles an Android-style boot.img for standard mobile device flashing,
# using the FreeBSD kernel built for Snapdragon.
# =============================================================================

set -euo pipefail

BLUE='\033[0;34m'; GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'
info()    { echo -e "${BLUE}[IMAGE]${NC} $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

KERNCONF="${1:-QCOM}"
FREEBSD_SRC="${FREEBSD_SRC:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
MAKEOBJDIRPREFIX="${MAKEOBJDIRPREFIX:-$HOME/obj}"

KERNEL_DIR="$MAKEOBJDIRPREFIX/$FREEBSD_SRC/arm64.aarch64/sys/$KERNCONF"
KERNEL_BIN="$KERNEL_DIR/kernel.gz"
OUT_IMG="uos-qcom-boot-$KERNCONF.img"

if [ ! -f "$KERNEL_BIN" ]; then
    error "Kernel not found at $KERNEL_BIN. Please build it first."
fi

# We may need a dummy ramdisk if not provided
RAMDISK="dummy_ramdisk.cpio.gz"
if [ ! -f "$RAMDISK" ]; then
    info "Creating dummy ramdisk..."
    mkdir -p tmp_ramdisk
    cd tmp_ramdisk
    echo "UOS Mobile Ramdisk" > init
    find . | cpio -H newc -o | gzip > "../$RAMDISK"
    cd ..
    rm -rf tmp_ramdisk
fi

if ! command -v mkbootimg &>/dev/null; then
    info "mkbootimg not found, appending to install instructions."
    info "To flash physically, you would run:"
    info "  mkbootimg --kernel $KERNEL_BIN --ramdisk $RAMDISK --base 0x80000000 --cmdline 'boot_device=uos' -o $OUT_IMG"
    info "For QEMU, the raw kernel is sufficient."
else
    info "Generating mobile boot.img..."
    mkbootimg \
        --kernel "$KERNEL_BIN" \
        --ramdisk "$RAMDISK" \
        --base 0x00000000 \
        --pagesize 4096 \
        --cmdline "console=ttyMSM0,115200,n8 earlycon=qcom_geni boot_device=uos uos.mobile=1" \
        -o "$OUT_IMG"
    echo -e "${GREEN}[OK]${NC} Image created: $OUT_IMG"
fi

echo -e "${GREEN}[OK]${NC} Image sequence completed."
