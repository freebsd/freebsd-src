#!/bin/sh
#
# Fastboot flash script for MediaTek devices
# Flashes boot, dtbo, super, and vbmeta images
#

set -e

OUT_DIR="out"

# Check for fastboot
if ! command -v fastboot >/dev/null 2>&1; then
    echo "Error: fastboot not found in PATH"
    exit 1
fi

# Verify device is in fastboot mode
if ! fastboot devices | grep -q "."; then
    echo "Error: No device found in fastboot mode"
    echo "Please connect device and ensure it's in fastboot mode"
    exit 1
fi

echo "Found fastboot device:"
fastboot devices

# Flash boot image
if [ -f "$OUT_DIR/boot.img" ]; then
    echo "Flashing boot image..."
    fastboot flash boot "$OUT_DIR/boot.img"
else
    echo "Warning: $OUT_DIR/boot.img not found, skipping"
fi

# Flash dtbo image
if [ -f "$OUT_DIR/dtbo.img" ]; then
    echo "Flashing dtbo image..."
    fastboot flash dtbo "$OUT_DIR/dtbo.img"
else
    echo "Warning: $OUT_DIR/dtbo.img not found, skipping"
fi

# Flash super partition (if exists)
if [ -f "$OUT_DIR/super.img" ]; then
    echo "Flashing super partition..."
    fastboot flash super "$OUT_DIR/super.img"
else
    echo "Warning: $OUT_DIR/super.img not found, skipping"
fi

# Flash vbmeta (if exists)
if [ -f "$OUT_DIR/vbmeta.img" ]; then
    echo "Flashing vbmeta..."
    fastboot flash vbmeta "$OUT_DIR/vbmeta.img"
    fastboot flash vbmeta_system "$OUT_DIR/vbmeta.img" 2>/dev/null || true
fi

# Flash vendor_boot if exists
if [ -f "$OUT_DIR/vendor_boot.img" ]; then
    echo "Flashing vendor_boot image..."
    fastboot flash vendor_boot "$OUT_DIR/vendor_boot.img"
fi

# Reboot device
echo "Rebooting device..."
fastboot reboot

echo "Flash complete!"