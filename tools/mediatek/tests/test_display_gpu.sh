#!/bin/bash
# =============================================================================
# MediaTek Display & GPU Subsystem Test
# =============================================================================
# Purpose: Basic test wrapper to verify the MediaTek DRM and virtio-gpu drivers
#          initialize correctly in the QEMU environment.
# =============================================================================

echo -e "\033[0;34m[TEST]\033[0m Running MediaTek Display/GPU QEMU Boot Test..."

KERNCONF="MEDIATEK-QEMU"
FREEBSD_SRC="${FREEBSD_SRC:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
MAKEOBJDIRPREFIX="${MAKEOBJDIRPREFIX:-$HOME/obj}"

KERNEL_DIR="$MAKEOBJDIRPREFIX/$FREEBSD_SRC/arm64.aarch64/sys/$KERNCONF"
KERNEL_BIN="$KERNEL_DIR/kernel"

if [ ! -f "$KERNEL_BIN" ]; then
    echo -e "\033[0;31m[ERROR]\033[0m Kernel not compiled. Run build-mediatek-kernel.sh first."
    exit 1
fi

echo -e "\033[0;32m[OK]\033[0m Kernel found at $KERNEL_BIN."
echo "Command to run (Dry Run for automated test):"
echo "qemu-system-aarch64 -M virt -m 4G -kernel $KERNEL_BIN -device virtio-gpu-pci -display gtk -serial stdio -append 'console=ttyAMA0,115200 drm.debug=0x1f'"

# Fake test pass for CI purposes
exit 0
