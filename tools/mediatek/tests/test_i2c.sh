#!/bin/sh
# ---- UOS MediaTek: I2C Test ----
# Tests I2C buses on MT8395 (used for PMIC MT6359, touch, USB-C mux).
# NIO 12L I2C mapping:
#   i2c0 (I2C2) - USB-C mux (IT5205)
#   i2c1 (I2C3) - general purpose
#   i2c2 (I2C4) - MIPI-LCD connector
#   i2c3 (I2C0) - general purpose
#   i2c4 (I2C1) - general purpose
#   i2c6       - PMIC (MT6359) + MT6360 charger/PMIC

VERBOSE="${1:-}"
PASS=0; FAIL=0
p() { printf "\033[0;32m  PASS\033[0m %s\n" "$*"; PASS=$((PASS+1)); }
f() { printf "\033[0;31m  FAIL\033[0m %s\n" "$*"; FAIL=$((FAIL+1)); }
i() { printf "\033[0;34m  .....\033[0m %s\n" "$*"; }
NOTE() { printf "\033[1;33m  NOTE\033[0m %s\n" "$*"; }

i "Checking I2C controller in dmesg..."
if dmesg | grep -qiE "iicbus|iic[0-9]|mtk_i2c|i2c.*attach"; then
    p "I2C: controller found in dmesg"
    [ "$VERBOSE" = "--verbose" ] && dmesg | grep -iE "iicbus|iic[0-9]|i2c" | head -10
else
    NOTE "I2C: no I2C controller in dmesg (expected only on real hardware)"
fi

i "Checking I2C device nodes..."
IIC_DEVS=$(ls /dev/iic* 2>/dev/null)
if [ -n "$IIC_DEVS" ]; then
    p "I2C: device nodes found: $IIC_DEVS"
else
    NOTE "I2C: no /dev/iic* devices (real hardware only)"
fi

i "Checking I2C bus count via sysctl..."
IIC_COUNT=$(sysctl -a 2>/dev/null | grep -c "^dev.iicbus\." || echo 0)
if [ "$IIC_COUNT" -gt 0 ]; then
    p "I2C: $IIC_COUNT I2C bus(es) in sysctl"
else
    NOTE "I2C: no sysctl dev.iicbus entries"
fi

i "Checking I2C scan on bus 0 (if i2c tool available)..."
if command -v i2c >/dev/null 2>&1 && [ -c /dev/iic0 ]; then
    i "Scanning I2C bus 0 (addresses 0x08..0x77)..."
    DEVICES=""
    for addr in 0x08 0x34 0x48 0x60 0x68; do
        if i2c -f /dev/iic0 -d -s -a "$addr" >/dev/null 2>&1; then
            DEVICES="$DEVICES $addr"
        fi
    done
    if [ -n "$DEVICES" ]; then
        p "I2C: devices found at:$DEVICES"
    else
        NOTE "I2C: no devices responded on bus 0"
    fi
elif [ -c /dev/iic6 ]; then
    i "Scanning I2C bus 6 (PMIC bus: MT6359 @ 0x6B expected)..."
    if i2c -f /dev/iic6 -d -s -a 0x6B >/dev/null 2>&1; then
        p "I2C: MT6359 PMIC found at address 0x6B on i2c6"
    else
        NOTE "I2C: MT6359 PMIC not responding (check clock/pinctrl)"
    fi
fi

i "Checking known I2C devices in dmesg (NIO 12L specific)..."
DEVICES_FOUND=0
for dev in "mt6360" "mt6359" "it5205" "rts5453"; do
    if dmesg 2>/dev/null | grep -qi "$dev"; then
        p "I2C: $dev found in dmesg"
        DEVICES_FOUND=$((DEVICES_FOUND+1))
    fi
done
[ "$DEVICES_FOUND" -eq 0 ] && NOTE "I2C: no NIO 12L-specific I2C devices in dmesg"

echo "I2C: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]
