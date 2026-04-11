#!/bin/bash
# =============================================================================
# UOS RISC-V - Create Mobile Boot Image
# =============================================================================
# Assembles an Android-style boot.img for RISC-V mobile device flashing,
# using the FreeBSD kernel and OpenSBI.
# =============================================================================

set -euo pipefail

BLUE='\033[0;34m'; GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'
info()    { echo -e "${BLUE}[IMAGE]${NC} $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

KERNCONF="${1:-UOS-STARFIVE}"
FREEBSD_SRC="${FREEBSD_SRC:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
MAKEOBJDIRPREFIX="${MAKEOBJDIRPREFIX:-$HOME/obj}"

KERNEL_DIR="$MAKEOBJDIRPREFIX/$FREEBSD_SRC/riscv.riscv64/sys/$KERNCONF"
KERNEL_BIN="$KERNEL_DIR/kernel"
OUT_IMG="uos-riscv-boot-$KERNCONF.img"

if [ ! -f "$KERNEL_BIN" ]; then
    error "Kernel not found at $KERNEL_BIN. Please build it first."
fi

# Convert kernel ELF to binary format for unified booting
KERNEL_BIN_RAW="$KERNEL_DIR/kernel.bin"
info "Converting ELF to raw binary format via objcopy..."
if command -v llvm-objcopy-17 &>/dev/null; then
    llvm-objcopy-17 -O binary "$KERNEL_BIN" "$KERNEL_BIN_RAW"
elif command -v llvm-objcopy &>/dev/null; then
    llvm-objcopy -O binary "$KERNEL_BIN" "$KERNEL_BIN_RAW"
else
    info "llvm-objcopy not found, skipping ELF to BIN packing."
    KERNEL_BIN_RAW="$KERNEL_BIN"
fi

RAMDISK="dummy_ramdisk_riscv.cpio.gz"
if [ ! -f "$RAMDISK" ]; then
    info "Creating dummy ramdisk..."
    mkdir -p tmp_ramdisk
    cd tmp_ramdisk
    echo "UOS Mobile Ramdisk RISC-V" > init
    find . | cpio -H newc -o | gzip > "../$RAMDISK"
    cd ..
    rm -rf tmp_ramdisk
fi

if ! command -v mkbootimg &>/dev/null; then
    info "mkbootimg not found."
    info "For physical RISC-V Mobile boards, you would run:"
    info "  mkbootimg --kernel $KERNEL_BIN_RAW --ramdisk $RAMDISK --base 0x40000000 --os_version 14.0.0 --cmdline 'mobile_os=1 root=/dev/mmcblk0' -o $OUT_IMG"
else
    info "Generating mobile boot.img..."
    mkbootimg \
        --kernel "$KERNEL_BIN_RAW" \
        --ramdisk "$RAMDISK" \
        --base 0x40000000 \
        --pagesize 4096 \
        --os_version 14.0.0 \
        --cmdline "console=ttyS0,115200 root=/dev/mmcblk0 mobile_os=1" \
        -o "$OUT_IMG"
    echo -e "${GREEN}[OK]${NC} Image created: $OUT_IMG"
fi

echo -e "${GREEN}[OK]${NC} Image sequence completed."
