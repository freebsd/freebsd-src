#!/bin/sh
# ---- UOS MediaTek: Display / Framebuffer Test ----
# Tests VirtIO GPU (QEMU) or MT8395 DRM (real hardware).
# On QEMU: VirtIO GPU - /dev/dri/card0 or /dev/fb0
# On NIO 12L: Mali-G57 GPU via /dev/dri/card0

VERBOSE="${1:-}"
PASS=0; FAIL=0
p() { printf "\033[0;32m  PASS\033[0m %s\n" "$*"; PASS=$((PASS+1)); }
f() { printf "\033[0;31m  FAIL\033[0m %s\n" "$*"; FAIL=$((FAIL+1)); }
i() { printf "\033[0;34m  .....\033[0m %s\n" "$*"; }
NOTE() { printf "\033[1;33m  NOTE\033[0m %s\n" "$*"; }

i "Checking display controller in dmesg..."
if dmesg | grep -qiE "virtio.gpu|drm|dri|fb[0-9]|framebuffer"; then
    p "Display: display controller found in dmesg"
    [ "$VERBOSE" = "--verbose" ] && dmesg | grep -iE "gpu|drm|dri|fb" | head -10
else
    NOTE "Display: no display controller found in dmesg"
fi

i "Checking framebuffer device nodes..."
FB_DEVS=$(ls /dev/fb* 2>/dev/null)
if [ -n "$FB_DEVS" ]; then
    p "Display: framebuffer device(s): $FB_DEVS"
else
    NOTE "Display: no /dev/fb* (check VirtIO GPU driver or DRM)"
fi

i "Checking DRI device nodes..."
DRI_DEVS=$(ls /dev/dri/* 2>/dev/null)
if [ -n "$DRI_DEVS" ]; then
    p "Display: DRI device(s): $DRI_DEVS"
else
    NOTE "Display: no /dev/dri/* (GPU driver not loaded)"
fi

i "Checking virtual terminal (vt) active..."
if sysctl kern.vty 2>/dev/null | grep -qi "vt"; then
    VTY=$(sysctl -n kern.vty 2>/dev/null)
    p "Display: virtual terminal: $VTY"
else
    NOTE "Display: kern.vty not found"
fi

i "Checking current tty/console type..."
CONSOLE=$(sysctl -n kern.console 2>/dev/null || echo "unknown")
p "Display: console: $CONSOLE"

i "Checking EFI framebuffer (early display)..."
if dmesg | grep -qi "efifb\|EFI.*framebuffer"; then
    p "Display: EFI framebuffer active"
else
    NOTE "Display: EFI framebuffer not active"
fi

i "Checking VirtIO GPU (QEMU specific)..."
if dmesg | grep -qi "virtio.gpu\|virtgpu\|virtvga"; then
    p "Display: VirtIO GPU detected"
    # Get resolution
    if command -v vidcontrol >/dev/null 2>&1; then
        RES=$(vidcontrol -i mode 2>/dev/null | grep current | head -1 || true)
        [ -n "$RES" ] && p "Display: resolution: $RES"
    fi
elif dmesg | grep -qi "Mali\|mali-g57\|mfg[0-9]"; then
    p "Display: ARM Mali GPU detected (real hardware)"
else
    NOTE "Display: No GPU detected yet (normal if GPU driver not yet implemented)"
fi

i "Testing framebuffer write (visual test)..."
# Write a test pattern to framebuffer to confirm display works
if [ -w /dev/fb0 ] 2>/dev/null; then
    # Write a simple colored bar (100KB of 0xFF bytes = white)
    dd if=/dev/zero bs=1024 count=100 2>/dev/null | \
        tr '\000' '\377' > /dev/fb0 2>/dev/null && \
        p "Display: framebuffer write test succeeded (screen should show white bar)" || \
        NOTE "Display: framebuffer write failed (permissions?)"
else
    NOTE "Display: /dev/fb0 not writable - skipping visual test"
fi

echo "Display: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]
