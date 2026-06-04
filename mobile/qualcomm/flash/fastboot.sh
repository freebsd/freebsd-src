#!/usr/bin/env bash
#
# $FreeBSD$
#
# Qualcomm Snapdragon SoC Flash Script
# Flashes Mobile OS to device via fastboot
#
# Usage: fastboot.sh [target]
#   target: optional SoC target (SM8450, SM8350, etc.) - defaults to env TARGET or SM8450
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FREEBSD_SRC="${FREEBSD_SRC:-${SCRIPT_DIR}/../../../..}"
OUT_DIR="${FREEBSD_SRC}/out"

TARGET="${1:-${TARGET:-SM8450}}"
KERNEL_DIR="${OUT_DIR}/${TARGET}"

if [ ! -d "${KERNEL_DIR}" ]; then
    echo "Error: Build output not found at ${KERNEL_DIR}"
    echo "Run build.sh first:"
    echo "  cd ${SCRIPT_DIR}/.. && ./build.sh ${TARGET}"
    exit 1
fi

echo "[*] Flashing Mobile OS [${TARGET}] to device via fastboot"

# Verify device is connected
if ! fastboot devices | grep -q .; then
    echo "Error: No fastboot device detected. Ensure device is in fastboot mode."
    exit 1
fi

echo "[*] Unlocking bootloader (if needed)"
fastboot flashing unlock || true

echo "[*] Flashing boot partition"
fastboot flash boot "${KERNEL_DIR}/boot.img"

echo "[*] Flashing dtbo partition"
fastboot flash dtbo "${KERNEL_DIR}/dtbo.img"

echo "[*] Flashing vendor_boot partition"
fastboot flash vendor_boot "${KERNEL_DIR}/vendor_boot.img"

echo "[*] Flashing super partition"
fastboot flash super "${KERNEL_DIR}/super.img"

echo "[*] Flashing vbmeta partition (verity disabled)"
fastboot flash vbmeta "${KERNEL_DIR}/vbmeta.img" --disable-verity --disable-verification

echo "[*] Wiping userdata"
fastboot -w

echo "[*] Rebooting device"
fastboot reboot

echo "[*] Flash complete. Device is rebooting."
