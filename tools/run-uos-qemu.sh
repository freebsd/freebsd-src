#!/bin/bash
# =============================================================================
# UOS(m) QEMU — Serial console in this terminal (-nographic)
# This is the primary WSL dev/test path.
# =============================================================================
set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; NC='\033[0m'
info()    { echo -e "${CYAN}[INFO]${NC}  $*"; }
success() { echo -e "${GREEN}[ OK ]${NC}  $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERR ]${NC}  $*"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FREEBSD_SRC="${FREEBSD_SRC:-$(cd "$SCRIPT_DIR/.." && pwd)}"
MOBILE_DIR="$FREEBSD_SRC/mobile"
IMG_DIR="$FREEBSD_SRC/tools/riscv/images"
IMG_FILE="$IMG_DIR/uos-riscv.img"
KERNEL_BIN="$MOBILE_DIR/vmlinux.riscv64"
NCPU="${UOS_QEMU_SMP:-4}"
MEM="${UOS_QEMU_MEM:-2G}"
GDB_MODE=0

while [ $# -gt 0 ]; do
    case "$1" in
        --cores|--smp) NCPU="$2"; shift 2 ;;
        --mem)         MEM="$2"; shift 2 ;;
        --gdb)         GDB_MODE=1; shift ;;
        *)             error "Unknown: $1 (use --help)" ;;
    esac
done

echo -e "${CYAN}=== UOS(m) QEMU (nographic / serial) ===${NC}"
echo ""

# 1) Firmware
OPENBI=""
for p in \
    /usr/share/qemu/opensbi-riscv64-virt-fw_jump.bin \
    /usr/share/opensbi/generic/fw_jump.bin \
    /usr/lib/riscv64-linux-gnu/opensbi/generic/fw_jump.bin \
    /usr/libexec/opensbi/generic/fw_jump.bin; do
    [ -f "$p" ] && OPENBI="$p" && break
done
[ -n "$OPENBI" ] || error "OpenSBI not found. Install: sudo apt-get install opensbi"
echo -e "${GREEN}firmware:${NC}  $OPENBI"

# 2) Toolchain
command -v riscv64-unknown-elf-gcc &>/dev/null && \
    echo -e "${GREEN}toolchain:${NC} $(riscv64-unknown-elf-gcc --version | head -1)" || \
    warn "riscv64-unknown-elf-gcc not found"

# 3) Build kernel if needed
mkdir -p "$IMG_DIR"
if [ ! -f "$KERNEL_BIN" ] || [ "$(stat -c %s "$KERNEL_BIN" 2>/dev/null || echo 0)" -eq 0 ]; then
    info "Building kernel..."
    cd "$MOBILE_DIR/kernel"
    make all QEMU=1 CONFIG_QEMU=1 CONFIG_VIRTIO_BLK=1 CONFIG_VIRTIO_NET=1 2>&1 | tail -10
    cd "$FREEBSD_SRC"
fi
[ -f "$KERNEL_BIN" ] || error "Kernel missing: $KERNEL_BIN"
echo -e "${GREEN}kernel:${NC}   $KERNEL_BIN ($(du -h "$KERNEL_BIN" | cut -f1))"

# 4) Disk image
[ -f "$IMG_FILE" ] || { info "Creating 4G image..."; qemu-img create -f raw "$IMG_FILE" 4G; }
echo -e "${GREEN}disk:${NC}     $IMG_FILE ($(du -h "$IMG_FILE" | cut -f1))"
echo ""
echo "Mode:       -nographic (all output to this terminal)"
echo "CPUs:       $NCPU"
echo "Memory:     $MEM"
echo ""
echo -e "${YELLOW}Ctrl+A then X = quit${NC}"
echo ""

GDB_ARGS=""
[ "$GDB_MODE" -eq 1 ] && GDB_ARGS="-gdb tcp::1234 -S" && \
    echo -e "${YELLOW}GDB stub on :1234 (QEMU paused)${NC}"

exec qemu-system-riscv64 \
    -M virt -cpu rv64 -smp "$NCPU" -m "$MEM" \
    -bios "$OPENBI" \
    -kernel "$KERNEL_BIN" \
    -append "console=ttyS0 root=/dev/vda rw earlyprintk=ttyS0" \
    -drive "file=$IMG_FILE,format=raw,if=virtio" \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device virtio-net-pci,netdev=net0 \
    -nographic -monitor none \
    -rtc base=localtime -nodefaults -device virtio-rng-pci \
    $GDB_ARGS
