#!/bin/bash
# =============================================================================
# UOS MediaTek - WSL Toolchain Setup Script (Fixed)
# =============================================================================
# Fixes:
#   - Package name 'dtc' -> 'device-tree-compiler' on Debian/Ubuntu
#   - set -e removed for apt section (package errors no longer abort script)
#   - LLVM/clang installed explicitly before env file is written
#   - Env file is always written at the end regardless of optional failures
# =============================================================================

set -uo pipefail   # NOTE: no -e here so package failures are non-fatal

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; NC='\033[0m'
info()    { echo -e "${BLUE}[INFO]${NC}  $*"; }
success() { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; }

echo ""
echo -e "${BLUE}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   UOS MediaTek - WSL Build Environment Setup            ║${NC}"
echo -e "${BLUE}║   Target: FreeBSD arm64 / MT8395 / Radxa NIO 12L        ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""

# ---- Detect WSL ----
if grep -qi microsoft /proc/version 2>/dev/null; then
    info "WSL detected ✓"
fi

# ---- Detect distro ----
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    DISTRO="unknown"
fi
info "Detected distro: $DISTRO"

# ---- Install apt packages (non-fatal per-package) ----
info "Installing build dependencies (failures are non-fatal)..."

case "$DISTRO" in
    ubuntu|debian|linuxmint|kali)
        sudo apt-get update -qq 2>/dev/null || warn "apt-get update failed"

        # Core build tools
        for pkg in \
            build-essential git curl wget flex bison python3 \
            rsync xz-utils zstd netcat-openbsd; do
            sudo apt-get install -y "$pkg" 2>/dev/null && \
                success "  installed: $pkg" || warn "  skipped:   $pkg"
        done

        # QEMU - try specific package first, then fallback
        if sudo apt-get install -y qemu-system-aarch64 2>/dev/null; then
            success "  installed: qemu-system-aarch64"
        elif sudo apt-get install -y qemu-system-arm 2>/dev/null; then
            success "  installed: qemu-system-arm (includes aarch64)"
        else
            warn "  skipped: QEMU - install manually: sudo apt-get install qemu-system-arm"
        fi

        # QEMU utilities
        for pkg in qemu-utils qemu-efi-aarch64 ovmf; do
            sudo apt-get install -y "$pkg" 2>/dev/null && \
                success "  installed: $pkg" || warn "  skipped:   $pkg"
        done

        # Device Tree Compiler - correct package name for Debian/Ubuntu
        if sudo apt-get install -y device-tree-compiler 2>/dev/null; then
            success "  installed: device-tree-compiler (dtc)"
        else
            warn "  skipped: device-tree-compiler"
        fi

        # Other tools
        for pkg in fdisk parted dosfstools e2fsprogs gdb-multiarch telnet; do
            sudo apt-get install -y "$pkg" 2>/dev/null && \
                success "  installed: $pkg" || warn "  skipped:   $pkg"
        done
        ;;

    fedora|rhel|centos|rocky)
        sudo dnf install -y \
            gcc make git curl wget flex bison python3 dtc \
            qemu-system-aarch64 qemu-img qemu-efi-aarch64 \
            parted dosfstools e2fsprogs gdb rsync xz zstd || true
        ;;

    arch|manjaro)
        sudo pacman -Sy --noconfirm \
            base-devel git curl wget flex bison python3 \
            qemu-full dtc parted dosfstools e2fsprogs gdb \
            rsync xz zstd || true
        ;;

    *)
        warn "Unknown distro '$DISTRO' - install manually:"
        warn "  qemu-system-aarch64, device-tree-compiler, gdb-multiarch, clang, lld"
        ;;
esac

# ---- Install LLVM/Clang (required for FreeBSD cross-compile) ----
info ""
info "Setting up LLVM/Clang cross-compiler..."

CLANG_OK=0

# Check if clang already installed
for clang_bin in clang-17 clang-16 clang-15 clang; do
    if command -v "$clang_bin" &>/dev/null; then
        CLANG_VER=$("$clang_bin" --version 2>/dev/null | head -1)
        success "Found: $CLANG_VER"
        CLANG_OK=1
        CLANG_CMD="$clang_bin"
        break
    fi
done

if [ "$CLANG_OK" -eq 0 ]; then
    info "Installing LLVM 17 from llvm.org..."
    case "$DISTRO" in
        ubuntu|debian)
            # Install dependencies for llvm.org script
            sudo apt-get install -y lsb-release software-properties-common \
                gnupg wget 2>/dev/null || true

            # Download and run llvm.sh installer
            LLVM_SCRIPT=$(mktemp /tmp/llvm-XXXXXX.sh)
            if wget -qO "$LLVM_SCRIPT" https://apt.llvm.org/llvm.sh; then
                chmod +x "$LLVM_SCRIPT"
                sudo bash "$LLVM_SCRIPT" 17 2>/dev/null && {
                    sudo apt-get install -y \
                        clang-17 lld-17 llvm-17 2>/dev/null || true
                    success "LLVM 17 installed"
                    CLANG_CMD="clang-17"
                    CLANG_OK=1
                }
                rm -f "$LLVM_SCRIPT"
            fi

            # If llvm.sh failed, try direct apt
            if [ "$CLANG_OK" -eq 0 ]; then
                info "Trying direct apt install of clang..."
                sudo apt-get install -y clang lld 2>/dev/null && {
                    success "clang installed from apt"
                    CLANG_CMD="clang"
                    CLANG_OK=1
                } || warn "clang install failed"
            fi
            ;;
        *)
            warn "Please install clang manually for your distro"
            CLANG_CMD="clang"
            ;;
    esac
fi

if [ "$CLANG_OK" -eq 0 ]; then
    warn "No clang found. Build will fail until clang is installed."
    warn "Manual install: sudo apt-get install -y clang lld"
    CLANG_CMD="clang"
fi

# ---- Verify QEMU ----
info ""
info "Verifying QEMU installation..."
if command -v qemu-system-aarch64 &>/dev/null; then
    QEMU_VER=$(qemu-system-aarch64 --version 2>/dev/null | head -1)
    success "QEMU: $QEMU_VER"
else
    warn "qemu-system-aarch64 not found in PATH"
    warn "Try: sudo apt-get install qemu-system-arm"
fi

# ---- Verify dtc ----
info "Verifying device-tree-compiler..."
if command -v dtc &>/dev/null; then
    DTC_VER=$(dtc --version 2>/dev/null | head -1)
    success "dtc: $DTC_VER"
else
    warn "dtc not found (non-fatal for QEMU testing)"
fi

# ---- Find UEFI firmware ----
info ""
info "Looking for ARM64 UEFI firmware..."
UEFI_FD=""
for p in \
    /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \
    /usr/share/qemu/edk2-aarch64-code.fd \
    /usr/share/AAVMF/AAVMF_CODE.fd \
    /usr/share/edk2/aarch64/QEMU_EFI.fd \
    /usr/share/ovmf/OVMF.fd; do
    if [ -f "$p" ]; then
        UEFI_FD="$p"
        success "UEFI firmware: $UEFI_FD"
        break
    fi
done
[ -z "$UEFI_FD" ] && warn "UEFI firmware not found - QEMU will use built-in boot"

# ---- Write environment config (ALWAYS written, even if packages failed) ----
info ""
info "Writing environment configuration..."

FREEBSD_SRC="${FREEBSD_SRC:-/mnt/d/Github_Projects/freebsd-src}"

cat > "$HOME/.uos-mediatek-env" << EOF
# UOS MediaTek Build Environment
# Generated: $(date)
# Add to ~/.bashrc: source ~/.uos-mediatek-env

export FREEBSD_SRC="${FREEBSD_SRC}"
export TARGET=arm64
export TARGET_ARCH=aarch64
export MAKEOBJDIRPREFIX="\$HOME/obj"
export UOS_UEFI_FD="${UEFI_FD}"
export UOS_QEMU_SMP=4
export UOS_QEMU_MEM=4G
export UOS_DISK_SIZE=8G
export UOS_DISK_IMG="\$HOME/uos-mediatek.img"
export KERNCONF=MEDIATEK-QEMU

# Toolchain
export CC=${CLANG_CMD}
export CXX=${CLANG_CMD/clang/clang++}
EOF

success "Environment written to: $HOME/.uos-mediatek-env"

# ---- Add to .bashrc if not already there ----
if ! grep -q "uos-mediatek-env" "$HOME/.bashrc" 2>/dev/null; then
    echo "" >> "$HOME/.bashrc"
    echo "# UOS MediaTek build environment" >> "$HOME/.bashrc"
    echo "[ -f \$HOME/.uos-mediatek-env ] && source \$HOME/.uos-mediatek-env" >> "$HOME/.bashrc"
    info "Added auto-source to ~/.bashrc"
fi

# ---- Final summary ----
echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   Setup Complete!                                        ║${NC}"
echo -e "${GREEN}║                                                          ║${NC}"
echo -e "${GREEN}║   Next steps:                                            ║${NC}"
echo -e "${GREEN}║   1. source ~/.uos-mediatek-env                          ║${NC}"
echo -e "${GREEN}║   2. bash build-mediatek-kernel.sh MEDIATEK-QEMU         ║${NC}"
echo -e "${GREEN}║   3. bash create-disk-image.sh                           ║${NC}"
echo -e "${GREEN}║   4. bash qemu-mediatek-run.sh                           ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""
