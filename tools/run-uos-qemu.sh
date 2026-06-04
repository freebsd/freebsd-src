#!/bin/bash
# =============================================================================
# UOS(m) QEMU boot for WSL / headless Linux
# =============================================================================
# WSL cannot open a QEMU GTK/SDL window and VNC over WSL is unreliable,
# so this script uses QEMU -nographic and prints everything to this
# terminal (serial console + QEMU monitor multiplexed on stdio).
#
# Usage:
#   bash tools/run-uos-qemu.sh            # 4 vCPUs, 2G RAM, -nographic
#   bash tools/run-uos-qemu.sh --cores 8  # bigger VM
#   bash tools/run-uos-qemu.sh --gdb      # GDB stub on :1234
# =============================================================================

set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; NC='\033[0m'
info()    { echo -e "${CYAN}[INFO]${NC}  $*"; }
success() { echo -e "${GREEN}[ OK ]${NC}  $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERR ]${NC}  $*"; exit 1; }
banner()  { echo -e "${CYAN}$*${NC}"; }

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FREEBSD_SRC="${FREEBSD_SRC:-$(cd "$SCRIPT_DIR/.." && pwd)}"
MOBILE_DIR="$FREEBSD_SRC/mobile"
IMG_DIR="$FREEBSD_SRC/tools/riscv/images"
IMG_FILE="$IMG_DIR/uos-riscv.img"
KERNEL_BIN="$MOBILE_DIR/vmlinux.riscv64"
DTB_BIN="$MOBILE_DIR/virtenv/devicetree/riscv-virt-mobile.dtb"
NCPU="${UOS_QEMU_SMP:-4}"
MEM="${UOS_QEMU_MEM:-2G}"
GDB_MODE=0

# ---------------------------------------------------------------------------
# Args
# ---------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --cores|--smp) NCPU="$2"; shift 2 ;;
        --mem)         MEM="$2"; shift 2 ;;
        --gdb)         GDB_MODE=1; shift ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --cores, --smp N   vCPU count (default: 4)"
            echo "  --mem SIZE         RAM (default: 2G)"
            echo "  --gdb              Wait for GDB on :1234"
            echo "  --help             This help"
            echo ""
            echo "This script runs QEMU in -nographic mode."
            echo "All kernel output appears in this terminal."
            echo ""
            echo "  Ctrl+A then X  = quit QEMU"
            echo "  Ctrl+A then C  = enter QEMU monitor"
            exit 0
            ;;
        *) error "Unknown option: $1 (try --help)" ;;
    esac
done

# ---------------------------------------------------------------------------
# Banner
# ---------------------------------------------------------------------------
clear
banner ""
banner "  UOS(m) Mobile OS  —  QEMU (nographic / serial console)"
banner ""
echo ""

# ---------------------------------------------------------------------------
# Phase 1: Dependencies
# ---------------------------------------------------------------------------
banner "Phase 1/4: Dependencies"

check_cmd() {
    if command -v "$1" &>/dev/null; then
        success "$1 found"
    else
        error "$1 not found. Install: sudo apt-get install $1"
    fi
}

check_cmd qemu-system-riscv64
check_cmd qemu-img

if command -v riscv64-unknown-elf-gcc &>/dev/null; then
    success "RISC-V toolchain found"
else
    warn "riscv64-unknown-elf-gcc not found — build may fail"
fi

if [ "$GDB_MODE" -eq 1 ] && ! command -v riscv64-unknown-elf-gdb &>/dev/null; then
    warn "riscv64-unknown-elf-gdb not found"
fi

echo ""

# ---------------------------------------------------------------------------
# Phase 2: OpenSBI firmware
# ---------------------------------------------------------------------------
banner "Phase 2/4: OpenSBI firmware"

OPENBI_CANDIDATES=(
    "/usr/share/qemu/opensbi-riscv64-virt-fw_jump.bin"
    "/usr/share/opensbi/generic/fw_jump.bin"
    "/usr/lib/riscv64-linux-gnu/opensbi/generic/fw_jump.bin"
    "/usr/libexec/opensbi/generic/fw_jump.bin"
    "/usr/share/opensbi/qemu-riscv64-fw_jump.bin"
)

OPENBI_FOUND=""
for p in "${OPENBI_CANDIDATES[@]}"; do
    if [ -f "$p" ]; then OPENBI_FOUND="$p"; break; fi
done

if [ -z "$OPENBI_FOUND" ]; then
    error "OpenSBI not found. Install: sudo apt-get install opensbi"
fi

success "Firmware: $OPENBI_FOUND"
echo ""

# ---------------------------------------------------------------------------
# Phase 3: Kernel (build if missing / empty)
# ---------------------------------------------------------------------------
banner "Phase 3/4: Kernel"

mkdir -p "$IMG_DIR"

NEED_BUILD=1
if [ -f "$KERNEL_BIN" ]; then
    KS=$(stat -c %s "$KERNEL_BIN" 2>/dev/null || echo 0)
    if [ "$KS" -gt 0 ]; then
        info "Existing kernel ($(du -h "$KERNEL_BIN" | cut -f1)) — skipping build"
        NEED_BUILD=0
    fi
fi

if [ "$NEED_BUILD" -eq 1 ]; then
    info "Building kernel..."
    cd "$MOBILE_DIR/kernel"
    make all QEMU=1 CONFIG_QEMU=1 CONFIG_VIRTIO_BLK=1 CONFIG_VIRTIO_NET=1 \
        2>&1 | tail -25
    cd "$FREEBSD_SRC"
fi

if [ ! -f "$KERNEL_BIN" ] || [ "$(stat -c %s "$KERNEL_BIN" 2>/dev/null || echo 0)" -eq 0 ]; then
    error "Kernel build failed"
fi

success "Kernel: $KERNEL_BIN ($(du -h "$KERNEL_BIN" | cut -f1))"

# ---------------------------------------------------------------------------
# Phase 4: Disk image
# ---------------------------------------------------------------------------
banner "Phase 4/4: Disk image"

if [ ! -f "$IMG_FILE" ]; then
    info "Creating 4G raw image..."
    qemu-img create -f raw "$IMG_FILE" 4G
fi

success "Image: $IMG_FILE ($(du -h "$IMG_FILE" | cut -f1))"
echo ""

# ---------------------------------------------------------------------------
# Summary + launch
# ---------------------------------------------------------------------------
banner "Launching QEMU — -nographic (serial on this terminal)"
echo ""
echo "  Kernel:     $KERNEL_BIN"
echo "  Firmware:   $OPENBI_FOUND"
echo "  CPUs:       $NCPU"
echo "  Memory:     $MEM"
echo "  Mode:       -nographic (serial = this terminal)"
echo ""

if [ "$GDB_MODE" -eq 1 ]; then
    echo -e "  ${YELLOW}GDB stub:${NC}  localhost:1234 (QEMU paused until you connect)"
    echo ""
fi

echo -e "  ${YELLOW}Quit:${NC}      Ctrl+A then X"
echo -e "  ${YELLOW}Monitor:${NC}   Ctrl+A then C  (type 'quit' to exit monitor)"
echo ""

sleep 1

exec qemu-system-riscv64 \
    -M virt \
    -cpu rv64 \
    -smp "$NCPU" \
    -m "$MEM" \
    -bios "$OPENBI_FOUND" \
    -kernel "$KERNEL_BIN" \
    -append "console=ttyS0 root=/dev/vda rw earlyprintk=ttyS0" \
    -drive "file=$IMG_FILE,format=raw,if=virtio" \
    -device virtio-keyboard-pci \
    -device virtio-mouse-pci \
    -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:57 \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -nographic \
    -monitor none \
    -rtc base=localtime \
    -nodefaults \
    -device virtio-rng-pci \
    ${GDB_MODE:+-gdb tcp::1234 -S} \
    ${DTB_BIN:+-dtb "$DTB_BIN"}
