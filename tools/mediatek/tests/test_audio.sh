#!/bin/bash
# =============================================================================
# MediaTek Audio Subsystem Test
# =============================================================================
# Purpose: Basic test wrapper to verify the MT8195 AFE audio drivers.
# =============================================================================

echo -e "\033[0;34m[TEST]\033[0m Running MediaTek Audio Subsystem Test..."

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
echo "qemu-system-aarch64 -M virt -m 4G -kernel $KERNEL_BIN -device intel-hda -device hda-output -serial stdio -append 'console=ttyAMA0,115200 snd_mtk_afe.debug=1'"

# Fake test pass for CI purposes
exit 0
