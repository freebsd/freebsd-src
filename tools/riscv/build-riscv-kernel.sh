#!/bin/bash
# =============================================================================
# UOS RISC-V - FreeBSD Kernel Build Script
# =============================================================================
# Cross-compiles the FreeBSD kernel for riscv64.
#
# Usage:
#   bash build-riscv-kernel.sh [KERNCONF]
#
#   KERNCONF options:
#     UOS-RISCV-QEMU (default) - QEMU virt machine testing
#     UOS-STARFIVE   - Real hardware (StarFive VisionFive 2)
#     riscv-qemu     - UOS mini-kernel for QEMU (mobile/vmlinux.riscv64)
# =============================================================================

set -euo pipefail

BLUE='\033[0;34m'; GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'
info()    { echo -e "${BLUE}[BUILD]${NC} $*"; }
success() { echo -e "${GREEN}[OK]${NC}    $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

KERNCONF="${1:-UOS-RISCV-QEMU}"
FREEBSD_SRC="${FREEBSD_SRC:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
TARGET="riscv"
TARGET_ARCH="riscv64"
MAKEOBJDIRPREFIX="${MAKEOBJDIRPREFIX:-$HOME/obj}"
NCPU=$(nproc 2>/dev/null || echo 4)

echo -e "${BLUE}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   UOS RISC-V Kernel Build                                ║${NC}"
echo -e "${BLUE}║   Config: $KERNCONF$(printf '%*s' $((48 - ${#KERNCONF})) '')║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════╝${NC}"

mkdir -p "$MAKEOBJDIRPREFIX"

if [ "$KERNCONF" = "riscv-qemu" ]; then
    info "Building UOS RISC-V mini-kernel for QEMU virt..."
    make -C "$FREEBSD_SRC/mobile/kernel" \
        ARCH=riscv \
        CFLAGS="-march=rv64imac_zicsr_zifencei -mabi=lp64 -O2 -g -ffreestanding -nostdlib" \
        QEMU=1 \
        CONFIG_QEMU=1 \
        CONFIG_VIRTIO_BLK=1 \
        CONFIG_VIRTIO_NET=1 \
        CONFIG_RISCV_DEBUG=1 \
        vmlinux.riscv64
    KERNEL_OUT="$FREEBSD_SRC/mobile/vmlinux.riscv64"
elif [ -f "$FREEBSD_SRC/sys/riscv/conf/$KERNCONF" ]; then
    KERNCONF_PATH="$FREEBSD_SRC/sys/riscv/conf/$KERNCONF"
    [ -f "$KERNCONF_PATH" ] || error "Kernel config not found: $KERNCONF_PATH"

    MAKE_ARGS=(
        -j "$NCPU"
        TARGET="$TARGET"
        TARGET_ARCH="$TARGET_ARCH"
        KERNCONF="$KERNCONF"
        WITHOUT_CLEAN=yes
        WITH_FDT=yes
    )

    if command -v clang-17 &>/dev/null; then
        export XCC=clang-17 XCXX=clang++-17 XCPP=clang-cpp-17
        export XLD=ld.lld-17 XAR=llvm-ar-17 XRANLIB=llvm-ranlib-17
    elif command -v clang &>/dev/null; then
        export XCC=clang XCXX=clang++ XCPP=clang-cpp
        export XLD=ld.lld XAR=llvm-ar XRANLIB=llvm-ranlib
    else
        error "No clang found. Please install LLVM/Clang."
    fi

    BUILD_WORLD="${BUILD_WORLD:-no}"
    if [ "$BUILD_WORLD" = "yes" ]; then
        info "Building world..."
        python3 "$FREEBSD_SRC/tools/build/make.py" buildworld "${MAKE_ARGS[@]}"
    fi

    info "Building kernel ($KERNCONF)..."
    python3 "$FREEBSD_SRC/tools/build/make.py" buildkernel "${MAKE_ARGS[@]}"

    KERNEL_DIR="$MAKEOBJDIRPREFIX/$FREEBSD_SRC/riscv.riscv64/sys/$KERNCONF"
    KERNEL_OUT="$KERNEL_DIR/kernel"
else
    error "Unknown KERNCONF: $KERNCONF. Use UOS-RISCV-QEMU, UOS-STARFIVE, or riscv-qemu."
fi

if [ -n "${KERNEL_OUT:-}" ] && [ -f "$KERNEL_OUT" ]; then
    success "Kernel built: $KERNEL_OUT ($(du -h "$KERNEL_OUT" | cut -f1))"
else
    warn "Kernel binary not found at expected location"
fi

success "Build complete!"

echo ""
echo "Next steps:"
echo "  QEMU launch:  bash $FREEBSD_SRC/tools/riscv/qemu-riscv-run.sh"
echo "  Tests:        bash $FREEBSD_SRC/tools/riscv/tests/test_riscv_core.sh"
