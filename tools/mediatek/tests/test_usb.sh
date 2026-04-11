#!/bin/sh
# ---- UOS MediaTek: USB Test ----
# Tests DWC3/xHCI USB 3.0 controller on MT8395.
# NIO 12L has: USB-C (OTG) + USB-A 3.0
# On QEMU: VirtIO USB or XHCI emulation

VERBOSE="${1:-}"
PASS=0; FAIL=0
p() { printf "\033[0;32m  PASS\033[0m %s\n" "$*"; PASS=$((PASS+1)); }
f() { printf "\033[0;31m  FAIL\033[0m %s\n" "$*"; FAIL=$((FAIL+1)); }
i() { printf "\033[0;34m  .....\033[0m %s\n" "$*"; }
NOTE() { printf "\033[1;33m  NOTE\033[0m %s\n" "$*"; }

# Check if running in QEMU
IS_QEMU=0
if dmesg | grep -qi "QEMU\|virtio"; then
    IS_QEMU=1
    i "QEMU environment detected. Hardware-specific checks will be treated as non-fatal."
fi

i "Checking xHCI/DWC3 controller in dmesg..."
if dmesg | grep -qiE "xhci|dwc3|usb.*[0-9]"; then
    p "USB: xHCI/DWC3 controller found in dmesg"
    [ "$VERBOSE" = "--verbose" ] && dmesg | grep -iE "xhci|dwc3|usb" | head -15
else
    if [ "$IS_QEMU" -eq 1 ]; then
        NOTE "USB: no xHCI/DWC3 in dmesg (normal in basic QEMU configs)"
    else
        f "USB: no xHCI/DWC3 in dmesg"
    fi
fi

i "Checking USB bus enumeration..."
USB_BUS_COUNT=$(dmesg | grep -c "^usbus[0-9]" || true)
if [ "$USB_BUS_COUNT" -gt 0 ]; then
    p "USB: $USB_BUS_COUNT USB bus(es) enumerated"
else
    if [ "$IS_QEMU" -eq 1 ]; then
        NOTE "USB: no USB buses enumerated (skipping in QEMU)"
    else
        f "USB: no USB buses enumerated"
    fi
fi

i "Checking USB device nodes..."
if ls /dev/usb* 2>/dev/null | head -1 | grep -q .; then
    USB_DEVS=$(ls /dev/usb* 2>/dev/null)
    p "USB: device nodes: $USB_DEVS"
else
    NOTE "USB: no /dev/usb* nodes (normal if no USB devices connected)"
fi

i "Checking usbconfig tool..."
if command -v usbconfig >/dev/null 2>&1; then
    OUTPUT=$(usbconfig 2>/dev/null | head -10)
    if [ -n "$OUTPUT" ]; then
        p "USB: usbconfig shows devices"
        [ "$VERBOSE" = "--verbose" ] && echo "$OUTPUT"
    else
        NOTE "USB: No USB devices connected (insert a USB device to test)"
    fi
else
    NOTE "USB: usbconfig not found (not a failure)"
fi

i "Checking USB 3.0 SuperSpeed support..."
if dmesg | grep -qi "SuperSpeed\|USB 3\|5.0 Gbps"; then
    p "USB: USB 3.0 SuperSpeed detected"
elif [ "$IS_QEMU" -eq 1 ]; then
    NOTE "USB: QEMU mode - USB 3.0 may be emulated as USB 2.0"
else
    NOTE "USB: USB 3.0 SuperSpeed not confirmed"
fi

i "Checking USB-C / Type-C connector (NIO 12L specific)..."
if dmesg | grep -qi "typec\|mt6360-tcpc\|it5205"; then
    p "USB: USB-C Type-C PD controller detected"
else
    NOTE "USB: USB-C PD controller not found (expected on real NIO 12L)"
fi

i "Checking USB OTG / dual-role support..."
if dmesg | grep -qi "OTG\|mtu3\|role.*switch\|dwc3.*host"; then
    p "USB: OTG dual-role support detected"
else
    NOTE "USB: OTG dual-role not confirmed"
fi

echo "USB: $PASS pass, $FAIL fail"
if [ "$IS_QEMU" -eq 1 ]; then
    exit 0
else
    [ "$FAIL" -eq 0 ]
fi
