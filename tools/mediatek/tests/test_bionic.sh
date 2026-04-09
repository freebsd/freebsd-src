#!/bin/sh
# ---- UOS MediaTek: Bionic / Android Compatibility Test ----
# Tests the Bionic ABI bridge devices (/dev/binder, /dev/ashmem)
# that allow Android NDK binaries to run on UOS.

VERBOSE="${1:-}"
PASS=0; FAIL=0
p() { printf "\033[0;32m  PASS\033[0m %s\n" "$*"; PASS=$((PASS+1)); }
f() { printf "\033[0;31m  FAIL\033[0m %s\n" "$*"; FAIL=$((FAIL+1)); }
i() { printf "\033[0;34m  .....\033[0m %s\n" "$*"; }
NOTE() { printf "\033[1;33m  NOTE\033[0m %s\n" "$*"; }

i "Checking Linux emulation layer (COMPAT_LINUX64)..."
if kldstat -v 2>/dev/null | grep -qi "linux64\|linux_elf"; then
    p "Bionic: Linux 64-bit emulation layer loaded"
elif sysctl -n compat.linux.osrelease >/dev/null 2>&1; then
    p "Bionic: Linux compat sysctl found"
else
    NOTE "Bionic: Linux emulation not detected (load linux64.ko manually)"
fi

i "Checking Binder IPC device (/dev/binder)..."
if [ -c /dev/binder ]; then
    p "Bionic: /dev/binder device exists"
    if [ -r /dev/binder ]; then
        p "Bionic: /dev/binder is accessible"
    else
        f "Bionic: /dev/binder not readable (check permissions)"
    fi
else
    NOTE "Bionic: /dev/binder not found (load uos_binder.ko)"
fi

i "Checking HW Binder device (/dev/hwbinder)..."
if [ -c /dev/hwbinder ]; then
    p "Bionic: /dev/hwbinder device exists"
else
    NOTE "Bionic: /dev/hwbinder not found (optional, for HAL services)"
fi

i "Checking Ashmem device (/dev/ashmem)..."
if [ -c /dev/ashmem ]; then
    p "Bionic: /dev/ashmem device exists (Android shared memory)"
else
    NOTE "Bionic: /dev/ashmem not found (load uos_ashmem.ko)"
fi

i "Checking Android system directory layout..."
ANDROID_DIRS="/system /system/lib64 /system/bin /data /vendor"
FOUND_DIRS=0
for dir in $ANDROID_DIRS; do
    if [ -d "$dir" ]; then
        FOUND_DIRS=$((FOUND_DIRS+1))
    fi
done
if [ "$FOUND_DIRS" -gt 2 ]; then
    p "Bionic: Android directory layout found ($FOUND_DIRS dirs)"
else
    NOTE "Bionic: Android filesystem not set up (/system, /data, /vendor missing)"
    NOTE "        Copy an Android arm64 system image to set up the runtime"
fi

i "Checking linker64 (Android dynamic linker)..."
if [ -f /system/bin/linker64 ]; then
    p "Bionic: /system/bin/linker64 found"
elif [ -f /system/lib64/ld-musl-aarch64.so.1 ]; then
    p "Bionic: musl libc linker found (alternative to Bionic)"
else
    NOTE "Bionic: Android linker not found at /system/bin/linker64"
fi

i "Checking Linux syscall compatibility via brandelf..."
if command -v brandelf >/dev/null 2>&1; then
    NOTE "Bionic: brandelf available - use 'brandelf -t Linux <binary>' to brand Android ELFs"
else
    NOTE "Bionic: brandelf not found (install misc/brandelf)"
fi

i "Checking UOS Bionic bridge module (uos_binder.ko)..."
if kldstat 2>/dev/null | grep -q "uos_binder"; then
    p "Bionic: uos_binder.ko loaded"
else
    NOTE "Bionic: uos_binder.ko not loaded (kldload uos_binder)"
fi

echo "Bionic: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]
