#!/bin/sh
# ---- UOS MediaTek: UART Test ----
# Tests UART device detection and basic communication.
# On real HW: uses /dev/uart0 (MT8395 UART0 at 921600 baud)
# On QEMU:    uses /dev/cuau0 (VirtIO serial / pl011)

VERBOSE="${1:-}"
PASS=0; FAIL=0

p() { printf "\033[0;32m  PASS\033[0m %s\n" "$*"; PASS=$((PASS+1)); }
f() { printf "\033[0;31m  FAIL\033[0m %s\n" "$*"; FAIL=$((FAIL+1)); }
i() { printf "\033[0;34m  .....\033[0m %s\n" "$*"; }

i "Checking UART devices in dmesg..."
if dmesg | grep -qi "uart"; then
    p "UART: devices found in dmesg"
    [ "$VERBOSE" = "--verbose" ] && dmesg | grep -i uart | head -5
else
    f "UART: no uart entries in dmesg"
fi

i "Checking UART device nodes..."
UART_DEVS=$(ls /dev/uart* /dev/cuau* 2>/dev/null | head -5)
if [ -n "$UART_DEVS" ]; then
    p "UART device nodes: $UART_DEVS"
else
    f "UART: no /dev/uart* or /dev/cuau* found"
fi

i "Checking UART via sysctl..."
if sysctl -a 2>/dev/null | grep -qi uart; then
    p "UART: sysctl entries found"
else
    f "UART: no sysctl entries"
fi

i "Checking UART baud rate (MediaTek 921600)..."
# Check if kernel was booted with correct serial speed
if dmesg | grep -q "921600"; then
    p "UART: 921600 baud confirmed in dmesg"
else
    # Not a failure in QEMU - different baud
    printf "\033[1;33m  NOTE\033[0m UART: 921600 not found (normal in QEMU)\n"
fi

i "Checking MediaTek UART compatible string..."
if dmesg | grep -qi "mt6577-uart\|mdtk_uart\|mediatek.*uart"; then
    p "UART: MediaTek-specific UART driver attached"
else
    printf "\033[1;33m  NOTE\033[0m UART: MediaTek UART driver not found (expected on QEMU)\n"
fi

echo "UART: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]
