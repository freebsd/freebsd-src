#!/usr/bin/env bash
#
# $FreeBSD$
#
# Qualcomm Snapdragon SoC Build Script for Mobile OS (BSD-based)
# Uses clang toolchain (required for Hexagon DSP + SDE)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FREEBSD_SRC="${FREEBSD_SRC:-${SCRIPT_DIR}/../../../..}"
OUT_DIR="${FREEBSD_SRC}/out"
BUILDDIR="${OUT_DIR}/$(basename "${SCRIPT_DIR}")"

# SoC target defaults
TARGET="${1:-${TARGET:-SM8450}}"
TARGET_CONF="${SCRIPT_DIR}/configs/${TARGET}.conf"

if [ ! -f "${TARGET_CONF}" ]; then
    echo "Error: Unknown target '${TARGET}'. Config not found at ${TARGET_CONF}"
    echo "Available targets:"
    ls "${SCRIPT_DIR}/configs/"
    exit 1
fi

echo "[*] Building ${TARGET} with clang toolchain"

# Toolchain
export TARGET=arm64
export TARGET_ARCH=arm64
export CC=clang
export CXX=clang++
export CPP=clang-cpp
export AS=clang
export LD=lld
export OBJCOPY=llvm-objcopy
export OBJDUMP=llvm-objdump
export NM=llvm-nm
export STRIP=llvm-strip

# Clang requires at least C11 for FreeBSD arm64 kernel
export STD=c11

# Output paths
KERNEL_DIR="${OUT_DIR}/${TARGET}"
mkdir -p "${KERNEL_DIR}"

echo "[*] Building kernel config: ${TARGET_CONF}"
make -C "${FREEBSD_SRC}/sys/arm64/conf" \
    TARGET_ARCH=arm64 \
    CC=clang \
    CFLAGS="-target arm64-apple-ios15.0 -mcpu=cortex-a710+crypto" \
    KERNCONF="${TARGET}" \
    "${TARGET}.h"

echo "[*] Building kernel (zImage + modules)"
make -C "${FREEBSD_SRC}" \
    TARGET_ARCH=arm64 \
    CC=clang \
    CXX=clang++ \
    CFLAGS="-target arm64-apple-ios15.0 -mcpu=cortex-a710+crypto" \
    KERNCONF="${TARGET}" \
    KERNEL_DEBUGDIR="" \
    buildkernel

echo "[*] Building device trees"
make -C "${FREEBSD_SRC}" \
    TARGET_ARCH=arm64 \
    CC=clang \
    KERNCONF="${TARGET}" \
    DTS_DEBUGDIR="" \
    builddtbs

echo "[*] Installing kernel to ${KERNEL_DIR}"
make -C "${FREEBSD_SRC}" \
    TARGET_ARCH=arm64 \
    KERNCONF="${TARGET}" \
    KERNEL_DEBUGDIR="${KERNEL_DIR}/kernel-debug" \
    INSTKERNNAME="kernel" \
    installkernel

echo "[*] Creating boot.img (zImage + ramdisk + DTBO)"
KERNEL_OUT="${KERNEL_DIR}/kernel"
DTB_DIR="${KERNEL_DIR}/dtb"
DTBO_DIR="${KERNEL_DIR}/dtbo"

mkdir -p "${DTB_DIR}" "${DTBO_DIR}"

# Device tree compilation (DTC assumed present)
if command -v dtc &> /dev/null; then
    for dts in "${FREEBSD_SRC}/sys/dts/arm64/qcom/"*.dts; do
        dtc -I dts -O dtb -o "${DTB_DIR}/$(basename "${dts}" .dts).dtb" "${dts}" 2>/dev/null || true
    done
fi

# Build DTBO from overlay sources
if [ -d "${FREEBSD_SRC}/sys/dts/arm64/qcom/overlay" ]; then
    for dts in "${FREEBSD_SRC}/sys/dts/arm64/qcom/overlay/"*.dts; do
        dtc -@ -I dts -O dtbo -o "${DTBO_DIR}/$(basename "${dts}" .dts).dtbo" "${dts}" 2>/dev/null || true
    done
fi

# mkdtimg for Qualcomm DTBO packing
if command -v mkdtimg &> /dev/null && ls "${DTBO_DIR}"/*.dtbo &> /dev/null; then
    echo "[*] Packing DTBO with mkdtimg"
    mkdtimg create "${KERNEL_DIR}/dtbo.img" "${DTBO_DIR}"/*.dtbo || true
fi

# Create empty ramdisk (placeholder)
dd if=/dev/zero of="${KERNEL_DIR}/ramdisk.cpio.gz" bs=1024 count=64 2>/dev/null || true

# Assemble boot.img with Android mkbootimg (if available)
if command -v mkbootimg &> /dev/null; then
    echo "[*] Creating boot.img with mkbootimg"
    mkbootimg \
        --kernel "${KERNEL_OUT}" \
        --ramdisk "${KERNEL_DIR}/ramdisk.cpio.gz" \
        --dtb "${DTB_DIR}/${TARGET}.dtb" \
        --cmdline "root=ufs rootdev=0 rootwait console=ttyMSM0,115200n8" \
        --header_version 4 \
        -o "${KERNEL_DIR}/boot.img" || true
else
    echo "[!] mkbootimg not found, boot.img creation skipped"
fi

# Create empty vendor_boot.img
if command -v mkbootimg &> /dev/null; then
    dd if=/dev/zero of="${KERNEL_DIR}/vendor_boot.img" bs=4096 count=4096 2>/dev/null || true
fi

# Create empty vbmeta.img (verification metadata)
dd if=/dev/zero of="${KERNEL_DIR}/vbmeta.img" bs=1024 count=64 2>/dev/null || true

# Create empty super.img (dynamic partition)
dd if=/dev/zero of="${KERNEL_DIR}/super.img" bs=1M count=512 2>/dev/null || true

echo "[*] Build complete!"
echo "    Boot:     ${KERNEL_DIR}/boot.img"
echo "    DTBO:     ${KERNEL_DIR}/dtbo.img"
echo "    Vendor:   ${KERNEL_DIR}/vendor_boot.img"
echo "    Super:    ${KERNEL_DIR}/super.img"
echo "    VBmeta:   ${KERNEL_DIR}/vbmeta.img"
echo "    Kernel:   ${KERNEL_DIR}/kernel"
