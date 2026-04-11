#!/bin/bash
# =============================================================================
# UOS MediaTek - FreeBSD arm64 Kernel Build Script (WSL)
# =============================================================================
# Cross-compiles the FreeBSD kernel for arm64 (MediaTek MT8395) from WSL.
#
# Usage:
#   bash build-mediatek-kernel.sh [KERNCONF]
#
#   KERNCONF options:
#     MEDIATEK-QEMU   (default) - QEMU virt machine testing
#     MEDIATEK        - Real hardware (Radxa NIO 12L)
#
# Prerequisites: Run setup-wsl-toolchain.sh first.
# =============================================================================

set -euo pipefail

BLUE='\033[0;34m'; GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'
info()    { echo -e "${BLUE}[BUILD]${NC} $*"; }
success() { echo -e "${GREEN}[OK]${NC}    $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# ---- Load environment ----
[ -f "$HOME/.uos-mediatek-env" ] && source "$HOME/.uos-mediatek-env"

KERNCONF="${1:-MEDIATEK-QEMU}"
FREEBSD_SRC="${FREEBSD_SRC:-/mnt/d/Github_Projects/freebsd-src}"
TARGET="${TARGET:-arm64}"
TARGET_ARCH="${TARGET_ARCH:-aarch64}"
MAKEOBJDIRPREFIX="${MAKEOBJDIRPREFIX:-$HOME/obj}"
NCPU=$(nproc)

echo ""
echo -e "${BLUE}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   UOS MediaTek Kernel Build                              ║${NC}"
echo -e "${BLUE}║   Config: $KERNCONF$(printf '%*s' $((48 - ${#KERNCONF})) '')║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""

info "Source:  $FREEBSD_SRC"
info "Target:  $TARGET/$TARGET_ARCH"
info "ObjDir:  $MAKEOBJDIRPREFIX"
info "Jobs:    $NCPU"
info ""

# ---- Ensure object directory exists ----
mkdir -p "$MAKEOBJDIRPREFIX"

[ -d "$FREEBSD_SRC" ] || error "FreeBSD source not found: $FREEBSD_SRC"

# ---- Validate KERNCONF exists ----
KERNCONF_PATH="$FREEBSD_SRC/sys/arm64/conf/$KERNCONF"
[ -f "$KERNCONF_PATH" ] || error "Kernel config not found: $KERNCONF_PATH"
info "Kernel config: $KERNCONF_PATH ✓"

# ---- Common make arguments ----
MAKE_ARGS=(
    -j "$NCPU"
    TARGET="$TARGET"
    TARGET_ARCH="$TARGET_ARCH"
    KERNCONF="$KERNCONF"
    WITHOUT_CLEAN=yes      # Incremental builds (remove for clean build)
    NO_CLEAN=yes           # Hard bypass of _cleanworldtmp
    WITH_FDT=yes           # Device Tree support
    WITHOUT_MODULES_OVERRIDE= # Build all modules
)

# For CI / clean builds, comment out WITHOUT_CLEAN above and uncomment:
# MAKE_ARGS+=(CLEAN_EVERYTHING=yes)

# ---- Optional LLVM override ----
if command -v clang-17 &>/dev/null; then
    export XCC=clang-17 XCXX=clang++-17 XCPP=clang-cpp-17
    export XLD=ld.lld-17 XAR=llvm-ar-17 XRANLIB=llvm-ranlib-17
    info "Using LLVM 17 toolchain"
elif command -v clang &>/dev/null; then
    export XCC=clang XCXX=clang++ XCPP=clang-cpp
    export XLD=ld.lld XAR=llvm-ar XRANLIB=llvm-ranlib
    info "Using system clang"
else
    error "No clang found. Run setup-wsl-toolchain.sh first."
fi

# ---- Provide clang-cpp if missing from host PATH ----
if ! command -v clang-cpp &>/dev/null; then
    mkdir -p "$HOME/.uos-bin"
    ln -sf "$(command -v clang)" "$HOME/.uos-bin/clang-cpp"
    export PATH="$HOME/.uos-bin:$PATH"
fi

# ---- Step 1: Build world (only if first time or explicitly requested) ----
BUILD_WORLD="${BUILD_WORLD:-no}"
if [ "$BUILD_WORLD" = "yes" ]; then
    info "Step 1/2: Building world (this takes ~60-90 min)..."
    START=$(date +%s)
    python3 "$FREEBSD_SRC/tools/build/make.py" buildworld "${MAKE_ARGS[@]}"
    END=$(date +%s)
    success "World built in $(( (END - START) / 60 )) minutes"
else
    info "Step 1/2: Skipping world build (set BUILD_WORLD=yes to enable)"
fi

# ---- Step 2: Build kernel ----
info "Step 2/2: Building kernel ($KERNCONF)..."
START=$(date +%s)
    python3 "$FREEBSD_SRC/tools/build/make.py" buildkernel "${MAKE_ARGS[@]}"
END=$(date +%s)
success "Kernel built in $(( (END - START) / 60 )) minutes"

# ---- Locate output ----
KERNEL_DIR="$MAKEOBJDIRPREFIX/$FREEBSD_SRC/arm64.aarch64/sys/$KERNCONF"
KERNEL_BIN="$KERNEL_DIR/kernel"
KERNEL_GZ="$KERNEL_DIR/kernel.gz"

if [ -f "$KERNEL_BIN" ]; then
    success "Kernel:    $KERNEL_BIN ($(du -h "$KERNEL_BIN" | cut -f1))"
fi
if [ -f "$KERNEL_GZ" ]; then
    success "Kernel.gz: $KERNEL_GZ ($(du -h "$KERNEL_GZ" | cut -f1))"
fi

# ---- Build DTBs ----
info "Building MediaTek DTBs..."
python3 "$FREEBSD_SRC/tools/build/make.py" "${MAKE_ARGS[@]}" -C "$FREEBSD_SRC/sys/modules/dtb/mediatek" || \
    echo -e "${YELLOW}[WARN]${NC}  DTB build failed (non-fatal - QEMU uses device tree from firmware)"

echo ""
success "Build complete! Next: bash create-disk-image.sh $KERNCONF"
