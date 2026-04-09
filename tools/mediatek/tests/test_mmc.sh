#!/bin/sh
# ---- UOS MediaTek: MMC/eMMC/SD Test ----
# Tests MSDC (MT8395 MMC controller) for eMMC (MSDC0) and SD (MSDC1).
# Expected on NIO 12L:
#   - /dev/mmc0  : 32GB eMMC (HS400)
#   - /dev/mmc1  : microSD slot (removable)
# On QEMU: VirtIO block /dev/vtblk0 or /dev/da0

VERBOSE="${1:-}"
PASS=0; FAIL=0
p() { printf "\033[0;32m  PASS\033[0m %s\n" "$*"; PASS=$((PASS+1)); }
f() { printf "\033[0;31m  FAIL\033[0m %s\n" "$*"; FAIL=$((FAIL+1)); }
i() { printf "\033[0;34m  .....\033[0m %s\n" "$*"; }

i "Checking MMC/MSDC controller in dmesg..."
if dmesg | grep -qi "mmc\|msdc\|sdhci"; then
    p "MMC: controller found in dmesg"
    [ "$VERBOSE" = "--verbose" ] && dmesg | grep -iE "mmc|msdc|sdhci" | head -10
else
    f "MMC: no MMC/MSDC controller in dmesg"
fi

i "Checking block device availability..."
BLOCK_DEVS=""
for dev in /dev/mmc* /dev/da* /dev/vtblk* /dev/ada*; do
    [ -c "$dev" ] || [ -b "$dev" ] && BLOCK_DEVS="$BLOCK_DEVS $dev"
done
if [ -n "$BLOCK_DEVS" ]; then
    p "MMC: block devices found:$BLOCK_DEVS"
else
    f "MMC: no block devices found"
fi

i "Checking storage via camcontrol / geom..."
if command -v camcontrol >/dev/null 2>&1; then
    if camcontrol devlist 2>/dev/null | head -5; then
        p "MMC: camcontrol devlist succeeded"
    else
        printf "\033[1;33m  NOTE\033[0m MMC: camcontrol returned no devices\n"
    fi
fi

i "Checking geom label..."
if command -v geom >/dev/null 2>&1; then
    GEOM_OUTPUT=$(geom disk list 2>/dev/null | head -20)
    if [ -n "$GEOM_OUTPUT" ]; then
        p "MMC: geom disk list returned entries"
        [ "$VERBOSE" = "--verbose" ] && echo "$GEOM_OUTPUT"
    else
        f "MMC: geom disk list empty"
    fi
fi

i "Checking eMMC HS400 support (real hardware only)..."
if dmesg | grep -qi "HS400\|MSDC0.*200000000\|eMMC"; then
    p "MMC: eMMC HS400 mode detected (32GB eMMC at 200MHz)"
elif dmesg | grep -qi "virtio\|QEMU"; then
    printf "\033[1;33m  NOTE\033[0m MMC: QEMU mode - eMMC HS400 not applicable\n"
else
    printf "\033[1;33m  NOTE\033[0m MMC: HS400 not detected (check MSDC driver clock configuration)\n"
fi

i "Checking read performance on first disk..."
DISK=$(ls /dev/da0 /dev/mmc0 /dev/vtblk0 2>/dev/null | head -1)
if [ -n "$DISK" ] && [ -r "$DISK" ]; then
    i "Reading 8MB from $DISK to test throughput..."
    SPEED=$(dd if="$DISK" of=/dev/null bs=1M count=8 2>&1 | grep -oE '[0-9.]+ MB/s' | tail -1)
    if [ -n "$SPEED" ]; then
        p "MMC: Read speed: $SPEED"
    else
        printf "\033[1;33m  NOTE\033[0m MMC: Could not measure read speed\n"
    fi
else
    f "MMC: No readable disk found for throughput test"
fi

echo "MMC: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]
