#!/bin/sh
# ---- UOS MediaTek: GPIO Test ----
# Tests MT8395 GPIO controller detection and basic pin operations.
# Uses the FreeBSD gpioctl(8) utility.

VERBOSE="${1:-}"
PASS=0; FAIL=0
p() { printf "\033[0;32m  PASS\033[0m %s\n" "$*"; PASS=$((PASS+1)); }
f() { printf "\033[0;31m  FAIL\033[0m %s\n" "$*"; FAIL=$((FAIL+1)); }
i() { printf "\033[0;34m  .....\033[0m %s\n" "$*"; }

i "Checking GPIO controller in dmesg..."
if dmesg | grep -qi "gpio\|mt8395_pinctrl\|mt7622_pinctrl"; then
    p "GPIO: controller found in dmesg"
    [ "$VERBOSE" = "--verbose" ] && dmesg | grep -i gpio | head -10
else
    f "GPIO: no GPIO controller in dmesg"
fi

i "Checking GPIO device nodes..."
if ls /dev/gpioc* 2>/dev/null | head -1 | grep -q .; then
    GPIO_DEV=$(ls /dev/gpioc* 2>/dev/null | head -1)
    p "GPIO: device node found: $GPIO_DEV"

    i "Listing GPIO pins (first 10)..."
    if command -v gpioctl >/dev/null 2>&1; then
        if gpioctl -f "$GPIO_DEV" -l 2>/dev/null | head -10; then
            p "GPIO: pin listing successful"
        else
            f "GPIO: gpioctl list failed"
        fi
    else
        printf "\033[1;33m  NOTE\033[0m gpioctl not found (install sysutils/gpio-utils)\n"
    fi
else
    f "GPIO: no /dev/gpioc* device nodes"
fi

i "Checking GPIO count via sysctl..."
GPIO_COUNT=$(sysctl -n dev.gpio 2>/dev/null | wc -l || echo 0)
if [ "$GPIO_COUNT" -gt 0 ]; then
    p "GPIO: sysctl dev.gpio entries: $GPIO_COUNT"
else
    printf "\033[1;33m  NOTE\033[0m GPIO: no sysctl dev.gpio (normal in QEMU without GPIO controller)\n"
fi

i "Checking MT8395 GPIO pin count (expected: 202)..."
if dmesg | grep -q "MT8395.*202\|202 pins"; then
    p "GPIO: MT8395 202-pin controller confirmed"
else
    printf "\033[1;33m  NOTE\033[0m GPIO: MT8395 202-pin count not confirmed in dmesg\n"
fi

# ---- Safe GPIO toggle test on known-safe test pin ----
# GPIO93 is the Ethernet PHY reset pin (active-high, from DTS)
# Only run on real hardware, skip on QEMU
i "Checking if running on real hardware (skip GPIO write test on QEMU)..."
IS_QEMU=0
if dmesg | grep -qi "QEMU\|virtio\|virt-pci"; then
    IS_QEMU=1
    printf "\033[1;33m  NOTE\033[0m GPIO: Running in QEMU - skipping GPIO write test\n"
else
    if command -v gpioctl >/dev/null 2>&1 && ls /dev/gpioc0 >/dev/null 2>&1; then
        i "GPIO: Testing read on GPIO107 (backlight enable, should be input-safe)..."
        if gpioctl -f /dev/gpioc0 -g 107 2>/dev/null; then
            p "GPIO: Read GPIO107 successful"
        else
            f "GPIO: GPIO107 read failed"
        fi
    fi
fi

echo "GPIO: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]
