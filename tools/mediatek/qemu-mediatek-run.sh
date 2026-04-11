#!/bin/bash
# =============================================================================
# UOS MediaTek - QEMU GUI Launcher (WSL)
# =============================================================================
# Launches UOS in QEMU with:
#   - ARM64 Cortex-A55 (matches MT8395 efficiency cores)
#   - VirtIO GPU in GTK/SDL window (GUI mode)
#   - VirtIO keyboard + mouse
#   - VirtIO network (host SSH forwarding on port 2222)
#   - VirtIO block disk
#   - Serial console on stdio (boot messages)
#   - QEMU monitor on telnet port 4444
#
# Usage:
#   bash qemu-mediatek-run.sh [disk_image] [--headless]
#
# From Windows host: start this from WSL terminal.
# For GUI in WSL2: ensure WSLg or X11 forwarding is configured.
# =============================================================================

set -euo pipefail

BLUE='\033[0;34m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
RED='\033[0;31m'; CYAN='\033[0;36m'; NC='\033[0m'
info()    { echo -e "${BLUE}[QEMU]${NC}  $*"; }
success() { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

[ -f "$HOME/.uos-mediatek-env" ] && source "$HOME/.uos-mediatek-env"

# ---- Configuration ----
DISK_IMG="${1:-${UOS_DISK_IMG:-$HOME/uos-mediatek.img}}"
HEADLESS="${2:-}"
NCPU="${UOS_QEMU_SMP:-4}"
MEM="${UOS_QEMU_MEM:-4G}"
UEFI_FD="${UOS_UEFI_FD:-}"
SSH_PORT=2222
MONITOR_PORT=4444
GDB_PORT=1234

# Auto-detect UEFI firmware
if [ -z "$UEFI_FD" ]; then
    for p in \
        /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \
        /usr/share/qemu/edk2-aarch64-code.fd \
        /usr/share/edk2/aarch64/QEMU_EFI.fd \
        /usr/share/AAVMF/AAVMF_CODE.fd; do
        [ -f "$p" ] && UEFI_FD="$p" && break
    done
fi

echo ""
echo -e "${CYAN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║   UOS MediaTek - QEMU Launch                            ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""

# ---- Pre-flight checks ----
command -v qemu-system-aarch64 &>/dev/null || error "qemu-system-aarch64 not found. Run setup-wsl-toolchain.sh"
if [ ! -f "$DISK_IMG" ]; then
    warn "Disk image not found: $DISK_IMG. Generating a blank virtual drive for GUI testing..."
    qemu-img create -f raw "$DISK_IMG" 8G >/dev/null 2>&1 || true
fi
[ -f "$DISK_IMG" ] || error "Failed to autogenerate disk image."

info "Disk image: $DISK_IMG ($(du -h "$DISK_IMG" | cut -f1))"
info "CPUs:       $NCPU x Cortex-A55"
info "Memory:     $MEM"
info "SSH:        localhost:$SSH_PORT -> guest:22"
info "Monitor:    telnet localhost:$MONITOR_PORT"

if [ -n "$UEFI_FD" ]; then
    info "UEFI:       $UEFI_FD"
else
    warn "No UEFI firmware found - using QEMU default boot"
fi

# ---- Display mode check ----
DISPLAY_OPTS=()
if [ "$HEADLESS" = "--headless" ] || [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    warn "No display detected or headless mode - using VNC on :1 (port 5901)"
    DISPLAY_OPTS=(-display vnc=:1)
    info "VNC:        Connect to localhost:5901"
else
    # Prefer GTK (native Linux), fallback to SDL
    if qemu-system-aarch64 -display gtk,gl=on -version &>/dev/null 2>&1; then
        DISPLAY_OPTS=(-display gtk,gl=on,show-cursor=on)
        info "Display:    GTK (OpenGL)"
    else
        DISPLAY_OPTS=(-display sdl)
        info "Display:    SDL"
    fi
fi

# ---- Build QEMU arguments ----
QEMU_ARGS=(
    # Machine: ARM64 virt with GICv3 (matches MT8395 interrupt model)
    -M virt,gic-version=3,iommu=smmuv3

    # CPU: Cortex-A55 (MT8395 efficiency core - good compatibility)
    -cpu cortex-a55

    # SMP and memory
    -smp "$NCPU"
    -m "$MEM"

    # UEFI firmware
    ${UEFI_FD:+-bios "$UEFI_FD"}

    # VirtIO GPU (GUI framebuffer)
    -device virtio-gpu-pci,xres=1280,yres=800

    # Display output
    "${DISPLAY_OPTS[@]}"

    # Input devices (VirtIO - proper Linux/BSD drivers)
    -device virtio-keyboard-pci
    -device virtio-mouse-pci

    # Storage: VirtIO block (disk image)
    -drive file="$DISK_IMG",format=raw,if=none,id=hd0,cache=writeback
    -device virtio-blk-pci,drive=hd0,bootindex=1

    # Network: VirtIO with host SSH forwarding
    -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56
    -netdev user,id=net0,hostfwd=tcp::"$SSH_PORT"-:22,hostfwd=tcp::8080-:80

    # Serial: FreeBSD early console output to stdio
    -serial stdio

    # QEMU monitor: telnet
    -monitor telnet:127.0.0.1:"$MONITOR_PORT",server=on,wait=off

    # RTC
    -rtc base=localtime

    # No default devices we don't need
    -nodefaults

    # Virtio RNG (good entropy source for kernel)
    -device virtio-rng-pci

    # Shared folder between host WSL and guest (via VirtIO 9P)
    -virtfs local,path="$(pwd)",mount_tag=hostshare,security_model=mapped-xattr,id=fsdev0
)

echo ""
echo -e "${GREEN}Starting QEMU...${NC}"
echo -e "${YELLOW}  Serial console: this terminal${NC}"
echo -e "${YELLOW}  SSH access:     ssh -p $SSH_PORT root@localhost${NC}"
echo -e "${YELLOW}  Monitor:        telnet localhost $MONITOR_PORT${NC}"
echo -e "${YELLOW}  Stop QEMU:      Ctrl+A then X (serial) or monitor: quit${NC}"
echo ""

exec qemu-system-aarch64 "${QEMU_ARGS[@]}"
