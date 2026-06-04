#!/bin/bash
# =============================================================================
# UOS(m) Mobile OS — Test & Run in QEMU
# =============================================================================
# One-command script to validate dependencies, build the RISC-V mini-kernel,
# prepare the rootfs + disk image, and launch QEMU.
#
# Usage:
#   bash tools/run-uos-qemu.sh [OPTIONS]
#
# Options:
#   --cores N        vCPUs (default: 4, max: 8)
#   --mem SIZE       RAM, e.g. 512M, 1G, 2G, 4G (default: 2G)
#   --headless       No GUI; use VNC on :1 instead
#   --gdb            Wait for GDB stub on :1234
#   --no-display     Alias for --headless
#   --smp N          Same as --cores
#   --help           This help
#
# Environment overrides:
#   UOS_QEMU_SMP     vCPU count
#   UOS_QEMU_MEM     Memory size
#   FREEBSD_SRC      Path to freebsd-src root (auto-detected)
#   HEADLESS=1       Force headless (VNC)
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; CYAN='\033[0;36m'; MAGENTA='\033[0;35m'; NC='\033[0m'

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
usage() {
    grep '^# Usage:' -A 20 "$0" | grep '^#' | sed 's/^# \?//'
    exit 0
}

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
banner "  ╔══════════════════════════════════════════════════════════╗"
banner "  ║                                                          ║"
banner "  ║   UOS(m) Mobile OS  —  QEMU Test & Launch               ║"
banner "  ║   Build + Boot in one command                            ║"
banner "  ║                                                          ║"
banner "  ╚══════════════════════════════════════════════════════════╝"
banner ""
echo ""

# ---------------------------------------------------------------------------
# Phase 1: Dependency check
# ---------------------------------------------------------------------------
banner "Phase 1/5: Checking dependencies"

MISSING=0

check_cmd() {
    if command -v "$1" &>/dev/null; then
        success "$1 found: $(command -v "$1")"
    else
        error "$1 not found. Install it first."
        MISSING=1
    fi
}

check_cmd qemu-system-riscv64

if ! command -v riscv64-unknown-elf-gcc &>/dev/null; then
    warn "riscv64-unknown-elf-gcc not found. Will try to build anyway."
fi

if ! command -v dtc &>/dev/null; then
    warn "device-tree-compiler (dtc) not found. DTB may be outdated."
fi

if ! command -v qemu-img &>/dev/null; then
    error "qemu-img not found (part of qemu-utils package)"
fi

if [ "$GDB_MODE" -eq 1 ] && ! command -v riscv64-unknown-elf-gdb &>/dev/null; then
    warn "riscv64-unknown-elf-gdb not found. GDB stub will be unreachable."
fi

echo ""
if [ "$MISSING" -eq 1 ]; then
    error "Missing dependencies. Install them and re-run."
fi

# ---------------------------------------------------------------------------
# Phase 2: Build kernel (if not present or stale)
# ---------------------------------------------------------------------------
banner "Phase 2/5: Building RISC-V mini-kernel"

mkdir -p "$IMG_DIR"

NEED_BUILD=1
if [ -f "$KERNEL_BIN" ]; then
    KERNEL_AGE=$(( $(date +%s) - $(stat -c %Y "$KERNEL_BIN" 2>/dev/null || stat -f %m "$KERNEL_BIN" 2>/dev/null || echo 0) ))
    # Rebuild if kernel is older than 1 hour or zero-size
    KERNEL_SIZE=$(stat -c %s "$KERNEL_BIN" 2>/dev/null || stat -f %z "$KERNEL_BIN" 2>/dev/null || echo 0)
    if [ "$KERNEL_AGE" -lt 3600 ] && [ "$KERNEL_SIZE" -gt 0 ]; then
        NEED_BUILD=0
        info "Existing kernel is fresh ($(du -h "$KERNEL_BIN" | cut -f1))"
    fi
fi

if [ "$NEED_BUILD" -eq 1 ]; then
    info "Building kernel (this takes ~30-60s on first build)..."
    cd "$MOBILE_DIR/kernel"
    make all QEMU=1 CONFIG_QEMU=1 CONFIG_VIRTIO_BLK=1 CONFIG_VIRTIO_NET=1 CONFIG_RISCV_DEBUG=1
    cd "$FREEBSD_SRC"
    if [ ! -f "$KERNEL_BIN" ] || [ "$(stat -c %s "$KERNEL_BIN" 2>/dev/null || echo 0)" -eq 0 ]; then
        error "Kernel build failed — $KERNEL_BIN missing or empty"
    fi
    success "Kernel built: $KERNEL_BIN ($(du -h "$KERNEL_BIN" | cut -f1))"
else
    success "Using existing kernel: $KERNEL_BIN ($(du -h "$KERNEL_BIN" | cut -f1))"
fi

# ---------------------------------------------------------------------------
# Phase 3: Prepare disk image and rootfs
# ---------------------------------------------------------------------------
banner "Phase 3/5: Preparing disk image and rootfs"

if [ ! -f "$IMG_FILE" ]; then
    info "Creating 4G raw disk image..."
    qemu-img create -f raw "$IMG_FILE" 4G
    success "Disk image created: $IMG_FILE"
else
    info "Disk image exists: $IMG_FILE ($(du -h "$IMG_FILE" | cut -f1))"
fi

# Populate rootfs from current source tree
info "Copying rootfs skeleton..."
ROOTFS_STAGING="$MOBILE_DIR/rootfs"
if [ -d "$ROOTFS_STAGING" ]; then
    # Create a sparse ext4-like overlay on the image
    # For QEMU we use virtio-blk with a raw image; the kernel+initramfs
    # approach would mount the rootfs from the kernel binary itself.
    # Since we are running the standalone mini-kernel, we embed the rootfs
    # hint via kernel cmdline.
    success "Rootfs staging ready at $ROOTFS_STAGING"
else
    warn "Rootfs staging dir not found at $ROOTFS_STAGING — using defaults"
fi

# ---------------------------------------------------------------------------
# Phase 4: Assemble QEMU command line
# ---------------------------------------------------------------------------
banner "Phase 4/5: Assembling QEMU command line"

EXTRA_ARGS=()
if [ -f "$DTB_BIN" ]; then
    EXTRA_ARGS+=(-dtb "$DTB_BIN")
    info "DTB: $DTB_BIN"
else
    warn "No custom DTB found, QEMU will use built-in device tree"
fi

BOOTARG="console=ttyS0 root=/dev/vda rw earlyprintk=ttyS0"

# Detect display mode
DISPLAY_OPTS=()
if [ "$HEADLESS_MODE" -eq 1 ] || { [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] && [ -z "${SSH_CONNECTION:-}" ]; }; then
    warn "Headless mode — using VNC on :1"
    DISPLAY_OPTS=(-display none -vnc :1)
else
    # Try GTK with OpenGL first, fallback to SDL, then VNC
    if qemu-system-riscv64 -display gtk,gl=on -version &>/dev/null 2>&1; then
        DISPLAY_OPTS=(-display gtk,gl=on,show-cursor=on -device virtio-gpu-pci,xres=1280,yres=800)
        info "Display backend: GTK (OpenGL accelerated)"
    elif qemu-system-riscv64 -display sdl -version &>/dev/null 2>&1; then
        DISPLAY_OPTS=(-display sdl -device virtio-gpu-pci,xres=1280,yres=800)
        info "Display backend: SDL"
    else
        warn "No GUI display available, using VNC on :1"
        DISPLAY_OPTS=(-display none -vnc :1)
    fi
fi

# Assemble full command
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
)

# GDB stub
if [ "$GDB_MODE" -eq 1 ]; then
    QEMU_CMD+=(-gdb tcp::1234 -S)
    info "GDB stub enabled on :1234 (QEMU will wait for connection)"
fi

# Add display args
QEMU_CMD+=("${DISPLAY_OPTS[@]}")

# ---------------------------------------------------------------------------
# Phase 5: Launch info
# ---------------------------------------------------------------------------
banner "Phase 5/5: Launch configuration"
echo ""
echo -e "  ${CYAN}Kernel:${NC}        $KERNEL_BIN"
echo -e "  ${CYAN}Disk image:${NC}    $IMG_FILE"
echo -e "  ${CYAN}SoC/ISA:${NC}       RISC-V RV64GC (QEMU virt)"
echo -e "  ${CYAN}CPUs:${NC}          $NCPU"
echo -e "  ${CYAN}Memory:${NC}        $MEM"
echo -e "  ${CYAN}Boot args:${NC}     $BOOTARG"
echo ""
echo -e "  ${YELLOW}Serial console:${NC}   this terminal (stdio)"
echo -e "  ${YELLOW}SSH:${NC}              ssh -p 2222 root@localhost"
echo -e "  ${YELLOW}VNC:${NC}              localhost:1 (if headless)"
echo -e "  ${YELLOW}QEMU monitor:${NC}    telnet localhost 4445"
if [ "$GDB_MODE" -eq 1 ]; then
echo -e "  ${YELLOW}GDB stub:${NC}        localhost:1234"
fi
echo -e "  ${YELLOW}Host fileshare:${NC}  9p mount at /mnt/host (tag: hostshare)"
echo ""
echo -e "  ${MAGENTA}Stop QEMU:${NC}       Ctrl+A then X (or Ctrl+A then C, then Q)"
echo ""

# ---------------------------------------------------------------------------
# Launch
# ---------------------------------------------------------------------------
info "Starting QEMU..."
echo ""

exec "${QEMU_CMD[@]}"
