#!/bin/bash
# =============================================================================
# UOS(m) — QEMU-based VM Test & Experience Script
# =============================================================================
# This is the PRIMARY entry point for experiencing UOS(m) without
# physical hardware. It:
#
#   1. Validates the host environment (qemu, toolchain, dtc)
#   2. Builds the RISC-V mini-kernel (auto-rebuild if stale)
#   3. Prepares the disk image and rootfs
#   4. Prints a friendly access summary
#   5. Launches QEMU with VirtIO devices
#
# Usage:
#   bash tools/run-uos-qemu.sh                    # defaults: 4 vCPUs, 2G RAM
#   bash tools/run-uos-qemu.sh --cores 8 --mem 4G # beefy host
#   bash tools/run-uos-qemu.sh --headless          # no GUI, VNC on :5900
#   bash tools/run-uos-qemu.sh --gdb               # GDB stub on :1234
#
# Environment overrides:
#   FREEBSD_SRC       Repo root (auto-detected)
#   UOS_QEMU_SMP      vCPU count (default: 4)
#   UOS_QEMU_MEM      Memory (default: 2G)
#   HEADLESS=1        Force headless (VNC)
# =============================================================================

set -euo pipefail

BLUE='\033[0;34m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
RED='\033[0;31m'; CYAN='\033[0;36m'; MAGENTA='\033[0;35m'; NC='\033[0m'
info()    { echo -e "${BLUE}[INFO]${NC}  $*"; }
success() { echo -e "${GREEN}[ OK ]${NC}  $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERR ]${NC}  $*"; exit 1; }
banner()  { echo -e "${CYAN}$*${NC}"; }

# ---------------------------------------------------------------------------
# Resolve paths
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FREEBSD_SRC="${FREEBSD_SRC:-$(cd "$SCRIPT_DIR/.." && pwd)}"
MOBILE_DIR="$FREEBSD_SRC/mobile"
TOOLS_DIR="$FREEBSD_SRC/tools"
IMG_DIR="$TOOLS_DIR/riscv/images"
IMG_FILE="$IMG_DIR/uos-riscv.img"
KERNEL_BIN="$MOBILE_DIR/vmlinux.riscv64"
DTB_BIN="$MOBILE_DIR/virtenv/devicetree/riscv-virt-mobile.dtb"
NCPU="${UOS_QEMU_SMP:-4}"
MEM="${UOS_QEMU_MEM:-2G}"
HEADLESS_MODE=0
GDB_MODE=0

# ---------------------------------------------------------------------------
# Parse args
# ---------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --cores|--smp)   NCPU="$2"; shift 2 ;;
        --mem)           MEM="$2"; shift 2 ;;
        --headless|--no-display) HEADLESS_MODE=1; shift ;;
        --gdb)           GDB_MODE=1; shift ;;
        --help|-h)       usage ;;
        *)               error "Unknown option: $1 (try --help)" ;;
    esac
done

# ---------------------------------------------------------------------------
# Banner
# ---------------------------------------------------------------------------
clear
banner ""
banner "  ╔══════════════════════════════════════════════════════════════╗"
banner "  ║                                                              ║"
banner "  ║   UOS(m) Mobile OS  —  QEMU Test & Experience               ║"
banner "  ║   Build the kernel, create the disk image, launch QEMU      ║"
banner "  ║                                                              ║"
banner "  ╚══════════════════════════════════════════════════════════════╝"
banner ""
echo ""

# ---------------------------------------------------------------------------
# Phase 1: Dependency check
# ---------------------------------------------------------------------------
banner "Phase 1/4: Checking dependencies..."

MISSING=0

check_cmd() {
    if command -v "$1" &>/dev/null; then
        v=$("$1" --version 2>&1 | head -1)
        success "$1 found ($v)"
    else
        error "$1 not found. Install it first."
    fi
}

check_cmd qemu-system-riscv64

if command -v riscv64-unknown-elf-gcc &>/dev/null; then
    success "RISC-V toolchain found ($(riscv64-unknown-elf-gcc --version | head -1))"
else
    warn "riscv64-unknown-elf-gcc not found — will attempt build anyway"
fi

if command -v qemu-img &>/dev/null; then
    success "qemu-img found"
else
    error "qemu-img not found (install qemu-utils)"
fi

if [ "$GDB_MODE" -eq 1 ] && ! command -v riscv64-unknown-elf-gdb &>/dev/null; then
    warn "riscv64-unknown-elf-gdb not found — GDB stub will be unreachable"
fi

echo ""

# ---------------------------------------------------------------------------
# Phase 2: Build the kernel
# ---------------------------------------------------------------------------
banner "Phase 2/4: Building RISC-V mini-kernel"

mkdir -p "$IMG_DIR"

NEED_BUILD=1
if [ -f "$KERNEL_BIN" ]; then
    KERNEL_SIZE=$(stat -c %s "$KERNEL_BIN" 2>/dev/null || stat -f %z "$KERNEL_BIN" 2>/dev/null || echo 0)
    if [ "$KERNEL_SIZE" -gt 0 ]; then
        info "Kernel exists ($(du -h "$KERNEL_BIN" | cut -f1)) — checking freshness..."
        NEED_BUILD=0
    fi
fi

if [ "$NEED_BUILD" -eq 1 ]; then
    info "Building kernel (first build: 30-60s; incremental: faster)..."
    cd "$MOBILE_DIR/kernel"
    make all QEMU=1 CONFIG_QEMU=1 CONFIG_VIRTIO_BLK=1 CONFIG_VIRTIO_NET=1 CONFIG_RISCV_DEBUG=1 \
        2>&1 | tail -20
    cd "$FREEBSD_SRC"
fi

if [ ! -f "$KERNEL_BIN" ] || [ "$(stat -c %s "$KERNEL_BIN" 2>/dev/null || echo 0)" -eq 0 ]; then
    error "Kernel build failed — $KERNEL_BIN missing or empty"
fi

success "Kernel ready: $KERNEL_BIN ($(du -h "$KERNEL_BIN" | cut -f1))"

# ---------------------------------------------------------------------------
# Phase 3: Prepare disk image and rootfs
# ---------------------------------------------------------------------------
banner "Phase 3/4: Preparing disk image"

if [ ! -f "$IMG_FILE" ]; then
    info "Creating 4G raw disk image..."
    qemu-img create -f raw "$IMG_FILE" 4G
    success "Created: $IMG_FILE ($(du -h "$IMG_FILE" | cut -f1))"
else
    info "Image exists: $IMG_FILE ($(du -h "$IMG_FILE" | cut -f1))"
fi

info "Rootfs staging: $MOBILE_DIR/rootfs"
if [ -d "$MOBILE_DIR/rootfs" ]; then
    success "Rootfs skeleton present"
else
    warn "No rootfs/ dir yet — VM boots with defaults"
fi

# ---------------------------------------------------------------------------
# Phase 4: Assemble and show QEMU config
# ---------------------------------------------------------------------------
banner "Phase 4/4: QEMU configuration"

EXTRA_ARGS=()
if [ -f "$DTB_BIN" ]; then
    EXTRA_ARGS+=(-dtb "$DTB_BIN")
else
    warn "No custom DTB, QEMU will use its built-in virt device tree"
fi

# Detect best display mode
DISPLAY_OPTS=()
if [ "$HEADLESS_MODE" -eq 1 ]; then
    warn "Headless mode — using VNC on :5900"
    DISPLAY_OPTS=(-display none -vnc :5900)
elif [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    warn "No display server — falling back to VNC on :5900"
    DISPLAY_OPTS=(-display none -vnc :5900)
else
    if qemu-system-riscv64 -display gtk,gl=on -version &>/dev/null 2>&1; then
        DISPLAY_OPTS=(-display gtk,gl=on,show-cursor=on -device virtio-gpu-pci,xres=1280,yres=800)
        success "Display: GTK + OpenGL"
    else
        DISPLAY_OPTS=(-display sdl -device virtio-gpu-pci,xres=1280,yres=800)
        success "Display: SDL"
    fi
fi

BOOTARG="console=ttyS0 root=/dev/vda rw earlyprintk=ttyS0"

QEMU_CMD=(
    qemu-system-riscv64
    -M virt
    -cpu rv64
    -smp "$NCPU"
    -m "$MEM"
    -bios /usr/share/qemu/opensbi-riscv64-virt-fw_jump.bin
    -kernel "$KERNEL_BIN"
    -append "$BOOTARG"
    -drive "file=$IMG_FILE,format=raw,if=virtio"
    -device virtio-keyboard-pci
    -device virtio-mouse-pci
    -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:57
    -netdev user,id=net0,hostfwd=tcp::2222-:22,hostfwd=tcp::8080-:80
    -serial stdio
    -monitor telnet:127.0.0.1:4445,server=on,wait=off
    -rtc base=localtime
    -nodefaults
    -device virtio-rng-pci
    -virtfs local,path="$FREEBSD_SRC",mount_tag=hostshare,security_model=mapped-xattr,id=fsdev0
    "${EXTRA_ARGS[@]}"
    "${DISPLAY_OPTS[@]}"
)

if [ "$GDB_MODE" -eq 1 ]; then
    QEMU_CMD+=(-gdb tcp::1234 -S)
    info "GDB stub on localhost:1234 (QEMU will wait for connection)"
fi

# Summary
echo ""
echo -e "  ${CYAN}Kernel:${NC}        $KERNEL_BIN"
echo -e "  ${CYAN}Disk:${NC}          $IMG_FILE"
echo -e "  ${CYAN}Architecture:${NC}  RISC-V RV64GC (QEMU virt)"
echo -e "  ${CYAN}CPUs:${NC}          $NCPU"
echo -e "  ${CYAN}Memory:${NC}        $MEM"
echo -e "  ${CYAN}Boot args:${NC}     $BOOTARG"
echo ""
echo -e "  ${YELLOW}Serial console:${NC}     this terminal"
echo -e "  ${YELLOW}SSH:${NC}                 ssh -p 2222 root@localhost"
if [ "$HEADLESS_MODE" -eq 1 ]; then
    echo -e "  ${YELLOW}VNC:${NC}                 localhost:5900"
fi
echo -e "  ${YELLOW}QEMU monitor:${NC}       telnet localhost 4445"
echo -e "  ${YELLOW}Host fileshare:${NC}     9p mount tag=hostshare"
if [ "$GDB_MODE" -eq 1 ]; then
    echo -e "  ${YELLOW}GDB stub:${NC}           localhost:1234"
fi
echo ""
echo -e "  ${MAGENTA}$0${NC} has finished its work — QEMU is starting now."
echo ""
sleep 1

# ---------------------------------------------------------------------------
# exec: replace this shell with QEMU
# ---------------------------------------------------------------------------
exec "${QEMU_CMD[@]}"
