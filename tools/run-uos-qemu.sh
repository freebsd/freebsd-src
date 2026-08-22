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
KERNEL_BIN="$MOBILE_DIR/kernel/vmlinux.riscv64"
NCPU="${UOS_QEMU_SMP:-4}"
MEM="${UOS_QEMU_MEM:-2G}"
GDB_MODE=0
DISPLAY_MODE="auto"

launch_wayland_stack() {
    local compositor=""
    for candidate in weston labwc phoc; do
        if command -v "$candidate" >/dev/null 2>&1; then
            compositor="$candidate"
            break
        fi
    done

    if [ -z "$compositor" ]; then
        echo -e "${YELLOW}[WARN]${NC}  No Wayland compositor installed. Install one with: sudo apt-get install weston labwc phoc"
        return 1
    fi

    export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/uos-wayland}"
    mkdir -p "$XDG_RUNTIME_DIR"
    chmod 700 "$XDG_RUNTIME_DIR"

    export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-uos-wayland}"
    rm -f "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY"

    echo -e "${CYAN}[INFO]${NC}  Starting Wayland compositor: $compositor"
    case "$compositor" in
        weston)
            weston --socket="$WAYLAND_DISPLAY" --backend=drm --xwayland=false >/tmp/uos-wayland.log 2>&1 &
            ;;
        labwc)
            labwc --socket "$WAYLAND_DISPLAY" >/tmp/uos-wayland.log 2>&1 &
            ;;
        phoc)
            phoc --socket "$WAYLAND_DISPLAY" --output 1280x720 --log-level error >/tmp/uos-wayland.log 2>&1 &
            ;;
    esac

    sleep 2
    echo -e "${GREEN}[ OK ]${NC}  Wayland display manager is active on socket: $WAYLAND_DISPLAY"
    return 0
}

while [ $# -gt 0 ]; do
    case "$1" in
        --cores|--smp) NCPU="$2"; shift 2 ;;
        --mem)         MEM="$2"; shift 2 ;;
        --gdb)         GDB_MODE=1; shift ;;
        --gui)         DISPLAY_MODE="gui"; shift ;;
        --vnc)         DISPLAY_MODE="vnc"; shift ;;
        --headless)    DISPLAY_MODE="headless"; shift ;;
        --wayland)     DISPLAY_MODE="wayland"; shift ;;
        --phoc)        DISPLAY_MODE="wayland"; shift ;;
        --help|-h)
            echo "Usage: $0 [--gui|--vnc|--headless|--wayland] [--cores N] [--mem SIZE] [--gdb]"
            exit 0
            ;;
        *)             error "Unknown: $1 (use --help)" ;;
    esac
done

if [ "$DISPLAY_MODE" = "auto" ]; then
    if [ -n "${WAYLAND_DISPLAY:-}" ]; then
        DISPLAY_MODE="wayland"
    elif [ -n "${DISPLAY:-}" ]; then
        DISPLAY_MODE="gui"
    else
        DISPLAY_MODE="vnc"
    fi
fi

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
case "$DISPLAY_MODE" in
    gui)      DISPLAY_ARGS=(-display gtk,gl=off); echo "Mode:       QEMU GUI window + serial on this terminal" ;;
    vnc)      DISPLAY_ARGS=(-display vnc=0.0.0.0:0,share=allow-exclusive); echo "Mode:       VNC :5900 + serial on this terminal" ;;
    headless) DISPLAY_ARGS=(-display none); echo "Mode:       headless serial console" ;;
    wayland)  DISPLAY_ARGS=(-display none); echo "Mode:       Wayland compositor mode" ;;
esac
echo "CPUs:       $NCPU"
echo "Memory:     $MEM"
echo ""
[ "$DISPLAY_MODE" = "vnc" ] && echo -e "${YELLOW}Connect VNC viewer to localhost:5900${NC}"
[ "$DISPLAY_MODE" = "gui" ] && echo -e "${YELLOW}The QEMU GUI window is the OS preview${NC}"
[ "$DISPLAY_MODE" = "wayland" ] && echo -e "${YELLOW}A Wayland compositor will provide the display manager and window manager${NC}"
echo -e "${YELLOW}Ctrl+A then X = quit${NC}"
echo ""

if [ "$DISPLAY_MODE" = "wayland" ]; then
    launch_wayland_stack || true
fi

GDB_ARGS=""
[ "$GDB_MODE" -eq 1 ] && GDB_ARGS="-gdb tcp::1234 -S" && \
    echo -e "${YELLOW}GDB stub on :1234 (QEMU paused)${NC}"

# QEMU ramfb presents a guest-RAM-backed framebuffer without requiring a
# complete VirtIO-GPU command-ring implementation in the early kernel.
exec qemu-system-riscv64 \
    -M virt -cpu rv64 -smp "$NCPU" -m "$MEM" \
    -bios "$OPENBI" \
    -kernel "$KERNEL_BIN" \
    -append "console=ttyS0 root=/dev/vda rw earlyprintk=ttyS0" \
    -drive "file=$IMG_FILE,format=raw,if=virtio" \
    -netdev user,id=net0 \
    -device virtio-net-pci,netdev=net0 \
    -device ramfb \
    -fw_cfg name=etc/ramfb/base,string=0x81000000 \
    -fw_cfg name=etc/ramfb/size,string=1280x720x32 \
    "${DISPLAY_ARGS[@]}" \
    -serial mon:stdio \
    -rtc base=localtime -nodefaults -device virtio-rng-pci \
    $GDB_ARGS
