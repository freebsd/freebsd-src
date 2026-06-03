#!/bin/bash
# =============================================================================
# UOS RISC-V - QEMU virt machine Launcher
# =============================================================================
# Launches UOS in QEMU RISC-V virt machine with:
#   - RV64GC (QEMU virt default, matches our rv64imac target)
#   - 4-8 vCPUs, 1-4GB RAM
#   - VirtIO GPU (GTK/SDL), keyboard, mouse
#   - VirtIO network (SSH on host port 2222)
#   - VirtIO block disk, serial console, QEMU monitor
# =============================================================================

set -euo pipefail

BLUE='\033[0;34m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
RED='\033[0;31m'; CYAN='\033[0;36m'; NC='\033[0m'
info()    { echo -e "${BLUE}[QEMU]${NC}  $*"; }
success() { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

FREEBSD_SRC="${FREEBSD_SRC:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
KERNCONF="${1:-UOS-RISCV-QEMU}"
KERNEL_BIN="${FREEBSD_SRC}/mobile/vmlinux.riscv64"
DTB_BIN="${FREEBSD_SRC}/mobile/virtenv/devicetree/riscv-virt-mobile.dtb"
IMG_FILE="${2:-${FREEBSD_SRC}/tools/riscv/images/uos-riscv.img}"
NCPU="${UOS_QEMU_SMP:-4}"
MEM="${UOS_QEMU_MEM:-2G}"

mkdir -p "$(dirname "$IMG_FILE")"

echo ""
echo -e "${CYAN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║   UOS RISC-V - QEMU Launch                              ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""

command -v qemu-system-riscv64 &>/dev/null || error "qemu-system-riscv64 not found. Run setup-env.sh"
[ -f "$KERNEL_BIN" ] || error "Kernel not found: $KERNEL_BIN. Build first with build-riscv-kernel.sh"

KERNEL_SIZE=$(du -h "$KERNEL_BIN" 2>/dev/null | cut -f1 || echo "unknown")
info "Kernel:    $KERNEL_BIN ($KERNEL_SIZE)"
info "Config:    $KERNCONF"
info "CPUs:      $NCPU"
info "Memory:    $MEM"
info "SSH:       localhost:2222 -> guest:22"
info "Monitor:   telnet localhost:4445"

if [ ! -f "$IMG_FILE" ]; then
    warn "Disk image not found: $IMG_FILE - creating blank 4G image"
    qemu-img create -f raw "$IMG_FILE" 4G
fi

DISPLAY_OPTS=()
if [ "${HEADLESS:-}" = "1" ] || { [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; }; then
    warn "No display detected - using VNC on :1"
    DISPLAY_OPTS=(-display vnc=:1)
else
    if qemu-system-riscv64 -display gtk,gl=on -version &>/dev/null 2>&1; then
        DISPLAY_OPTS=(-display gtk,gl=on,show-cursor=on)
        info "Display:   GTK (OpenGL)"
    else
        DISPLAY_OPTS=(-display sdl)
        info "Display:   SDL"
    fi
fi

EXTRA_ARGS=()
if [ -f "$DTB_BIN" ]; then
    EXTRA_ARGS+=(-dtb "$DTB_BIN")
fi

BOOTARG="console=ttyS0 root=/dev/vda rw"

QEMU_ARGS=(
    -M virt
    -cpu rv64
    -smp "$NCPU"
    -m "$MEM"
    -bios /usr/share/qemu/opensbi-riscv64-virt-fw_jump.bin
    -kernel "$KERNEL_BIN"
    -append "$BOOTARG"
    -drive file="$IMG_FILE",format=raw,if=virtio
    -device virtio-gpu-pci,xres=1280,yres=800
    "${DISPLAY_OPTS[@]}"
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

echo ""
info "Starting QEMU RISC-V..."
echo -e "${YELLOW}  Serial:    this terminal${NC}"
echo -e "${YELLOW}  SSH:        ssh -p 2222 root@localhost${NC}"
echo -e "${YELLOW}  Monitor:    telnet localhost 4445${NC}"
echo -e "${YELLOW}  Stop:       Ctrl+A then X${NC}"
echo ""

exec qemu-system-riscv64 "${QEMU_ARGS[@]}"
