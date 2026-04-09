#!/bin/bash
# =============================================================================
# UOS MediaTek - QEMU Debug Launcher (WSL)
# =============================================================================
# Launches QEMU with GDB stub for kernel debugging.
#
# Usage:
#   bash qemu-mediatek-debug.sh [disk_image]
#
# Then in another terminal:
#   gdb-multiarch /path/to/kernel
#   (gdb) target remote localhost:1234
#   (gdb) continue
#
# Useful GDB commands for FreeBSD kernel:
#   (gdb) info threads          - list kernel threads
#   (gdb) thread apply all bt   - backtrace all threads
#   (gdb) p/x *curthread        - examine current thread
#   (gdb) break panic           - break on kernel panic
#   (gdb) break mt8395_clk_probe - break on driver probe
# =============================================================================

set -euo pipefail

BLUE='\033[0;34m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
RED='\033[0;31m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "${BLUE}[DEBUG]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

[ -f "$HOME/.uos-mediatek-env" ] && source "$HOME/.uos-mediatek-env"

DISK_IMG="${1:-${UOS_DISK_IMG:-$HOME/uos-mediatek.img}}"
FREEBSD_SRC="${FREEBSD_SRC:-/mnt/d/Github_Projects/freebsd-src}"
MAKEOBJDIRPREFIX="${MAKEOBJDIRPREFIX:-$HOME/obj}"
KERNCONF="${KERNCONF:-MEDIATEK-QEMU}"
NCPU="${UOS_QEMU_SMP:-4}"
MEM="${UOS_QEMU_MEM:-4G}"
GDB_PORT=1234
MONITOR_PORT=4445   # Different port from run.sh

KERNEL_ELF="$MAKEOBJDIRPREFIX/$FREEBSD_SRC/arm64.aarch64/sys/$KERNCONF/kernel.debug"
[ -f "$KERNEL_ELF" ] || KERNEL_ELF="$MAKEOBJDIRPREFIX/$FREEBSD_SRC/arm64.aarch64/sys/$KERNCONF/kernel"

echo ""
echo -e "${CYAN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║   UOS MediaTek - QEMU Debug Mode                        ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""
info "Disk image: $DISK_IMG"
info "Kernel ELF: $KERNEL_ELF"
info "GDB port:   localhost:$GDB_PORT"
info ""
info "QEMU will pause at startup waiting for GDB connection."
info ""
echo -e "${YELLOW}In a second terminal, run:${NC}"
echo -e "  ${GREEN}gdb-multiarch $KERNEL_ELF${NC}"
echo -e "  ${GREEN}(gdb) target remote localhost:$GDB_PORT${NC}"
echo -e "  ${GREEN}(gdb) break mt8395_clk_probe${NC}"
echo -e "  ${GREEN}(gdb) continue${NC}"
echo ""

[ -f "$DISK_IMG" ] || error "Disk image not found: $DISK_IMG"

UEFI_FD="${UOS_UEFI_FD:-}"
if [ -z "$UEFI_FD" ]; then
    for p in /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \
             /usr/share/qemu/edk2-aarch64-code.fd; do
        [ -f "$p" ] && UEFI_FD="$p" && break
    done
fi

exec qemu-system-aarch64 \
    -M virt,gic-version=3 \
    -cpu cortex-a55 \
    -smp "$NCPU" \
    -m "$MEM" \
    ${UEFI_FD:+-bios "$UEFI_FD"} \
    -nographic \
    -serial stdio \
    -monitor telnet:127.0.0.1:"$MONITOR_PORT",server=on,wait=off \
    -drive file="$DISK_IMG",format=raw,if=none,id=hd0 \
    -device virtio-blk-pci,drive=hd0 \
    -device virtio-net-pci,netdev=net0 \
    -netdev user,id=net0,hostfwd=tcp::2223-:22 \
    -device virtio-rng-pci \
    -s \
    -S
    # -s : GDB stub on port 1234
    # -S : Pause CPU at startup, wait for GDB 'continue'
