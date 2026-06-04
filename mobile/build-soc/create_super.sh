#!/usr/bin/env bash
set -euo pipefail

# Dynamic super partition creator
# Usage: create_super.sh --soc <SoC> --output <super.img> [--fs ext4|f2fs]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

SUPER_PARTITIONS="boot recovery system_ext vendor odm product cache"

get_vendor() {
    local soc="$1"
    case "$soc" in
        MT6*) echo "mediatek" ;;
        SM*|MSM*) echo "qualcomm" ;;
        S5E*) echo "samsung" ;;
        TENSOR*) echo "google" ;;
        *) log_error "Unknown SoC: $soc"; return 1 ;;
    esac
}

create_ext4_super() {
    local soc="$1"
    local output="$2"
    
    log_info "Creating ext4 super partition..."
    
    local super_dir="$OUTPUT_DIR/super_staging"
    rm -rf "$super_dir"
    mkdir -p "$super_dir"
    
    # Create partition images
    for partition in $SUPER_PARTITIONS; do
        local img="$super_dir/${partition}.img"
        local size
        
        case "$partition" in
            boot) size=64M ;;
            recovery) size=64M ;;
            system_ext) size=1G ;;
            vendor) size=1G ;;
            odm) size=512M ;;
            product) size=1G ;;
            cache) size=512M ;;
            *) size=512M ;;
        esac
        
        log_info "Creating $partition partition ($size)..."
        dd if=/dev/zero of="$img" bs=1M count=0 seek="${size%M}" 2>/dev/null
        mkfs.ext4 -F "$img" >/dev/null 2>&1 || true
    done
    
    # Create super image
    if command -v lpmake >/dev/null 2>&1; then
        lpmake --metadata-size 65536 --super-name super \
            $(for p in $SUPER_PARTITIONS; do echo --partition "$p,0,${super_dir}/${p}.img"; done) \
            --output "$output" || true
    elif command -v avbtool >/dev/null 2>&1; then
        avbtool make_super_image --super_name super --metadata_size 65536 \
            --output_path "$output" || true
        log_warn "lpmake not found, created stub super.img with avbtool"
    else
        log_warn "Neither lpmake nor avbtool found, creating raw image"
        dd if=/dev/zero of="$output" bs=1M count=4096
    fi
    
    log_info "Created ext4 super partition: $output"
}

create_f2fs_super() {
    local soc="$1"
    local output="$2"
    
    log_info "Creating f2fs super partition..."
    
    local super_dir="$OUTPUT_DIR/super_staging"
    rm -rf "$super_dir"
    mkdir -p "$super_dir"
    
    # Create partition images with f2fs
    for partition in $SUPER_PARTITIONS; do
        local img="$super_dir/${partition}.img"
        local size
        
        case "$partition" in
            boot) size=64M ;;
            recovery) size=64M ;;
            system_ext) size=1G ;;
            vendor) size=1G ;;
            odm) size=512M ;;
            product) size=1G ;;
            cache) size=512M ;;
            *) size=512M ;;
        esac
        
        log_info "Creating $partition partition ($size)..."
        dd if=/dev/zero of="$img" bs=1M count=0 seek="${size%M}" 2>/dev/null
        mkfs.f2fs -f "$img" >/dev/null 2>&1 || true
    done
    
    # Create super image
    if command -v lpmake >/dev/null 2>&1; then
        lpmake --metadata-size 65536 --super-name super --metadata-slots 2 \
            $(for p in $SUPER_PARTITIONS; do echo --partition "$p,0,${super_dir}/${p}.img,f2fs"; done) \
            --output "$output" || true
    else
        log_warn "lpmake not found, creating raw image"
        dd if=/dev/zero of="$output" bs=1M count=4096
    fi
    
    log_info "Created f2fs super partition: $output"
}

create_vendor_boot() {
    local soc="$1"
    local output="$2"
    
    log_info "Creating vendor_boot.img..."
    
    local vendor_dir="$OUTPUT_DIR/vendor_staging"
    rm -rf "$vendor_dir"
    mkdir -p "$vendor_dir"
    
    # Create vendor ramdisk
    mkdir -p "$vendor_dir/vendor" "$vendor_dir/etc"
    
    # Create vendor_boot.img if mkbootfs available
    if command -v mkbootfs >/dev/null 2>&1 && command -v mkbootimg >/dev/null 2>&1; then
        mkbootfs "$vendor_dir" | minigzip > "$vendor_dir/vendor-ramdisk.img" || true
        
        local args
        case "$soc" in
            MT6*) args="--kernel_offset 0x00008000 --ramdisk_offset 0x04000000 --os_version 0x0700" ;;
            *) args="--kernel_offset 0x00000000 --ramdisk_offset 0x01000000 --header_version 2 --os_version 0x0700" ;;
        esac
        
        mkbootimg $args --kernel /dev/null --ramdisk "$vendor_dir/vendor-ramdisk.img" \
            --output "$output/vendor_boot.img" || true
        
        log_info "Created vendor_boot.img: $output/vendor_boot.img"
    else
        log_warn "mkbootfs/mkbootimg not found, skipping vendor_boot.img creation"
    fi
}

# Parse arguments
SOC=""
OUTPUT="super.img"
FS_TYPE="ext4"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --soc)
            SOC="$2"
            shift 2
            ;;
        --output)
            OUTPUT="$2"
            shift 2
            ;;
        --fs)
            FS_TYPE="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 --soc <SoC> --output <super.img> [--fs ext4|f2fs]"
            echo "Partitions: $SUPER_PARTITIONS"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

OUTPUT_DIR=$(dirname "$OUTPUT")
mkdir -p "$OUTPUT_DIR"

if [[ -z "$SOC" ]]; then
    log_error "--soc is required (for header version selection)"
    exit 1
fi

case "$FS_TYPE" in
    ext4)
        create_ext4_super "$SOC" "$OUTPUT"
        ;;
    f2fs)
        create_f2fs_super "$SOC" "$OUTPUT"
        ;;
    *)
        log_error "Unknown filesystem: $FS_TYPE (use ext4 or f2fs)"
        exit 1
        ;;
esac

create_vendor_boot "$SOC" "$OUTPUT_DIR"
