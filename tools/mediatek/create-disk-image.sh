#!/bin/bash
# =============================================================================
# UOS MediaTek - Bootable Disk Image Creator (WSL)
# =============================================================================
# Creates a GPT disk image suitable for QEMU or flashing to eMMC/SD.
#
# Layout:
#   Partition 1: EFI System Partition (FAT32, 256 MB) - bootloader + kernel
#   Partition 2: Root filesystem (UFS, remainder)
#
# Usage:
#   bash create-disk-image.sh [KERNCONF] [IMG_SIZE]
#   bash create-disk-image.sh MEDIATEK-QEMU 8G
# =============================================================================

set -euo pipefail

BLUE='\033[0;34m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
RED='\033[0;31m'; NC='\033[0m'
info()    { echo -e "${BLUE}[IMG]${NC}   $*"; }
success() { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

[ -f "$HOME/.uos-mediatek-env" ] && source "$HOME/.uos-mediatek-env"

KERNCONF="${1:-MEDIATEK-QEMU}"
IMG_SIZE="${2:-${UOS_DISK_SIZE:-8G}}"
IMG_FILE="${UOS_DISK_IMG:-$HOME/uos-mediatek.img}"
FREEBSD_SRC="${FREEBSD_SRC:-/mnt/d/Github_Projects/freebsd-src}"
MAKEOBJDIRPREFIX="${MAKEOBJDIRPREFIX:-$HOME/obj}"
MNT_DIR=$(mktemp -d)
TARGET="${TARGET:-arm64}"
TARGET_ARCH="${TARGET_ARCH:-aarch64}"

trap "cleanup" EXIT
cleanup() {
    # Unmount and remove loop device safely
    sudo umount "$MNT_DIR/boot/efi" 2>/dev/null || true
    sudo umount "$MNT_DIR" 2>/dev/null || true
    [ -n "${LOOP_DEV:-}" ] && sudo losetup -d "$LOOP_DEV" 2>/dev/null || true
    rm -rf "$MNT_DIR"
}

echo ""
echo -e "${BLUE}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   UOS MediaTek - Disk Image Creator                     ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""
info "Config:    $KERNCONF"
info "Image:     $IMG_FILE ($IMG_SIZE)"

# ---- Locate kernel ----
KERNEL_DIR="$MAKEOBJDIRPREFIX/$FREEBSD_SRC/arm64.aarch64/sys/$KERNCONF"
[ -d "$KERNEL_DIR" ] || error "Kernel not built yet. Run build-mediatek-kernel.sh first."

# ---- Create sparse image ----
info "Creating disk image ($IMG_SIZE)..."
qemu-img create -f raw "$IMG_FILE" "$IMG_SIZE"
success "Image created: $IMG_FILE"

# ---- Partition: GPT + EFI SP + UFS root ----
info "Partitioning (GPT: EFI=256M, root=remainder)..."
sudo parted -s "$IMG_FILE" \
    mklabel gpt \
    mkpart ESP fat32 1MiB 257MiB \
    set 1 esp on \
    mkpart root 257MiB 100%

# ---- Attach loop device ----
LOOP_DEV=$(sudo losetup --find --show --partscan "$IMG_FILE")
info "Loop device: $LOOP_DEV"
sleep 1  # Wait for kernel to create partition devices

ESP_DEV="${LOOP_DEV}p1"
ROOT_DEV="${LOOP_DEV}p2"

# ---- Format partitions ----
info "Formatting EFI partition (FAT32)..."
sudo mkfs.fat -F 32 -n "EFI" "$ESP_DEV"

info "Formatting root partition (UFS2)..."
sudo newfs -j -L "UOS_ROOT" "$ROOT_DEV"

# ---- Mount root ----
info "Mounting filesystems..."
sudo mount -t ufs -o ufstype=ufs2 "$ROOT_DEV" "$MNT_DIR" 2>/dev/null || \
    sudo mount "$ROOT_DEV" "$MNT_DIR"
sudo mkdir -p "$MNT_DIR/boot/efi"
sudo mount "$ESP_DEV" "$MNT_DIR/boot/efi"

# ---- Install FreeBSD distribution ----
info "Installing FreeBSD arm64 distribution..."
make -C "$FREEBSD_SRC" installworld \
    DESTDIR="$MNT_DIR" \
    TARGET="$TARGET" TARGET_ARCH="$TARGET_ARCH" \
    MAKEOBJDIRPREFIX="$MAKEOBJDIRPREFIX" \
    WITHOUT_CLEAN=yes 2>/dev/null || warn "installworld failed (first-time build may need buildworld)"

info "Installing kernel ($KERNCONF)..."
make -C "$FREEBSD_SRC" installkernel \
    DESTDIR="$MNT_DIR" \
    TARGET="$TARGET" TARGET_ARCH="$TARGET_ARCH" \
    MAKEOBJDIRPREFIX="$MAKEOBJDIRPREFIX" \
    KERNCONF="$KERNCONF" || warn "installkernel encountered errors"

# ---- Basic system configuration ----
info "Configuring base system..."

# /etc/fstab
sudo tee "$MNT_DIR/etc/fstab" > /dev/null << 'EOF'
# UOS MediaTek fstab
# Device        Mountpoint      FStype  Options         Dump    Pass
/dev/da0s2      /               ufs     rw              1       1
/dev/da0s1      /boot/efi       msdosfs rw              0       0
tmpfs           /tmp            tmpfs   rw,mode=1777    0       0
tmpfs           /var/run        tmpfs   rw              0       0
EOF

# /etc/rc.conf
sudo tee "$MNT_DIR/etc/rc.conf" > /dev/null << 'EOF'
# UOS MediaTek rc.conf
hostname="uos-mediatek"
ifconfig_vtnet0="DHCP"          # VirtIO NIC (QEMU)
ifconfig_gbe0="DHCP"            # Real HW Gigabit Ethernet
sshd_enable="YES"
moused_enable="NO"
sendmail_enable="NONE"
dumpdev="AUTO"
zfs_enable="NO"
# UOS services
uos_binder_enable="YES"         # Android Binder IPC bridge
uos_surfaceflinger_enable="NO"  # Enable when GPU driver ready
EOF

# /boot/loader.conf
sudo tee "$MNT_DIR/boot/loader.conf" > /dev/null << 'EOF'
# UOS MediaTek boot loader configuration
# Kernel selection
kernel="kernel"

# Console: serial (UART0) for debugging
boot_serial="YES"
comconsole_speed="921600"
console="comconsole,efi"

# Early hints
hint.uart.0.at="fdt"
hint.uart.0.compatstr="mediatek,mt6577-uart"

# Android Bionic bridge module
uos_binder_load="YES"

# FDT (Device Tree)
fdt_overlays="mediatek/mt8395-nio-12l-uos.dtbo"
EOF

# Root password (empty for dev)
sudo chroot "$MNT_DIR" /bin/sh -c "echo '' | passwd -p '' root" 2>/dev/null || true

success "Disk image ready: $IMG_FILE"
echo ""
echo -e "${GREEN}Next: bash qemu-mediatek-run.sh${NC}"
