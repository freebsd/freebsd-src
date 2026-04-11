#!/bin/bash
# =============================================================================
# UOS Qualcomm - FreeBSD arm64 Kernel Build Script
# =============================================================================
# Cross-compiles the FreeBSD kernel for arm64 (Qualcomm Snapdragon).
#
# Usage:
#   bash build-qcom-kernel.sh [KERNCONF]
#
#   KERNCONF options:
#     QCOM-QEMU  (default) - QEMU virt machine testing
#     QCOM       - Real hardware (Snapdragon)
# =============================================================================

set -euo pipefail

BLUE='\033[0;34m'; GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'
info()    { echo -e "${BLUE}[BUILD]${NC} $*"; }
success() { echo -e "${GREEN}[OK]${NC}    $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

KERNCONF="${1:-QCOM-QEMU}"
FREEBSD_SRC="${FREEBSD_SRC:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
TARGET="arm64"
TARGET_ARCH="aarch64"
MAKEOBJDIRPREFIX="${MAKEOBJDIRPREFIX:-$HOME/obj}"
NCPU=$(nproc 2>/dev/null || echo 4)

echo -e "${BLUE}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   UOS Qualcomm Kernel Build                              ║${NC}"
echo -e "${BLUE}║   Config: $KERNCONF$(printf '%*s' $((48 - ${#KERNCONF})) '')║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════╝${NC}"

# Ensure object directory exists
mkdir -p "$MAKEOBJDIRPREFIX"

# Validate KERNCONF exists
KERNCONF_PATH="$FREEBSD_SRC/sys/arm64/conf/$KERNCONF"
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

KERNEL_DIR="$MAKEOBJDIRPREFIX/$FREEBSD_SRC/arm64.aarch64/sys/$KERNCONF"
KERNEL_BIN="$KERNEL_DIR/kernel"
KERNEL_GZ="$KERNEL_DIR/kernel.gz"

if [ -f "$KERNEL_BIN" ]; then
    success "Kernel built: $KERNEL_BIN"
fi

info "Building Qualcomm DTBs..."
python3 "$FREEBSD_SRC/tools/build/make.py" "${MAKE_ARGS[@]}" -C "$FREEBSD_SRC/sys/modules/dtb/qcom" || \
    echo -e "${BLUE}[WARN]${NC} DTB build for QCOM might not exist yet or failed."

success "Build complete!"
