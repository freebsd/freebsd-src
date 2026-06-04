#!/usr/bin/env bash
set -euo pipefail

# mkbootimg wrapper with correct offsets per SoC
# Usage: gen_bootimg.sh --soc <SoC> --kernel <kernel> --ramdisk <ramdisk> --dtb <dtb> --out <boot.img>

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

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

get_bootimg_args() {
    local soc="$1"
    local vendor
    vendor=$(get_vendor "$soc")
    
    case "$vendor" in
        mediatek)
            echo "--kernel_offset 0x00008000 --ramdisk_offset 0x04000000 --tags_offset 0x0e000000 --os_version 0x0700 --os_patch_level 01-01-2024"
            ;;
        qualcomm)
            echo "--kernel_offset 0x00000000 --ramdisk_offset 0x01000000 --dtb_offset 0x000000 --header_version 2 --os_version 0x0700 --os_patch_level 01-01-2024"
            ;;
        samsung)
            echo "--kernel_offset 0x00000000 --ramdisk_offset 0x01000000 --dtb_offset 0x000000 --header_version 1 --os_version 0x0700 --os_patch_level 01-01-2024"
            ;;
        google)
            echo "--kernel_offset 0x00000000 --ramdisk_offset 0x01000000 --dtb_offset 0x000000 --header_version 2 --os_version 0x0700 --os_patch_level 01-01-2024"
            ;;
        *)
            return 1
            ;;
    esac
}

gen_bootimg_mediatek() {
    local kernel="$1"
    local ramdisk="$2"
    local out="$3"
    
    local args
    args=$(get_bootimg_args "$SOC")
    
    mkbootimg $args --kernel "$kernel" --ramdisk "$ramdisk" --cmdline "bootopt=64S3,32N2,64N2 androidboot.selinux=permissive" --output "$out"
    
    log_info "Generated MediaTek boot.img: $out"
}

gen_bootimg_qualcomm() {
    local kernel="$1"
    local ramdisk="$2"
    local dtb="$3"
    local out="$4"
    
    local args
    args=$(get_bootimg_args "$SOC")
    
    if [[ -n "$dtb" ]] && [[ -f "$dtb" ]]; then
        mkbootimg $args --kernel "$kernel" --ramdisk "$ramdisk" --dtb "$dtb" --output "$out"
    else
        mkbootimg $args --kernel "$kernel" --ramdisk "$ramdisk" --output "$out"
    fi
    
    log_info "Generated Qualcomm boot.img: $out"
}

gen_bootimg_samsung() {
    local kernel="$1"
    local ramdisk="$2"
    local dtb="$3"
    local out="$4"
    
    local args
    args=$(get_bootimg_args "$SOC")
    
    mkbootimg $args --kernel "$kernel" --ramdisk "$ramdisk" --dtb "$dtb" --output "$out"
    
    log_info "Generated Samsung boot.img: $out"
}

gen_bootimg_google() {
    local kernel="$1"
    local ramdisk="$2"
    local dtb="$3"
    local out="$4"
    
    local args
    args=$(get_bootimg_args "$SOC")
    
    if [[ -n "$dtb" ]] && [[ -f "$dtb" ]]; then
        mkbootimg $args --kernel "$kernel" --ramdisk "$ramdisk" --dtb "$dtb" --output "$out"
    else
        mkbootimg $args --kernel "$kernel" --ramdisk "$ramdisk" --output "$out"
    fi
    
    log_info "Generated Google/Tensor boot.img: $out"
}

# Parse arguments
SOC=""
KERNEL=""
RAMDISK=""
DTB=""
OUT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --soc)
            SOC="$2"
            shift 2
            ;;
        --kernel)
            KERNEL="$2"
            shift 2
            ;;
        --ramdisk)
            RAMDISK="$2"
            shift 2
            ;;
        --dtb)
            DTB="$2"
            shift 2
            ;;
        --out)
            OUT="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 --soc <SoC> --kernel <kernel> --ramdisk <ramdisk> [--dtb <dtb>] --out <boot.img>"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

if [[ -z "$SOC" ]]; then
    log_error "--soc is required"
    exit 1
fi

if [[ -z "$KERNEL" ]]; then
    log_error "--kernel is required"
    exit 1
fi

if [[ -z "$RAMDISK" ]]; then
    log_error "--ramdisk is required"
    exit 1
fi

if [[ -z "$OUT" ]]; then
    OUT="boot.img"
fi

VENDOR=$(get_vendor "$SOC")

case "$VENDOR" in
    mediatek)
        gen_bootimg_mediatek "$KERNEL" "$RAMDISK" "$OUT"
        ;;
    qualcomm)
        gen_bootimg_qualcomm "$KERNEL" "$RAMDISK" "$DTB" "$OUT"
        ;;
    samsung)
        gen_bootimg_samsung "$KERNEL" "$RAMDISK" "$DTB" "$OUT"
        ;;
    google)
        gen_bootimg_google "$KERNEL" "$RAMDISK" "$DTB" "$OUT"
        ;;
    *)
        log_error "Unsupported vendor: $VENDOR"
        exit 1
        ;;
esac
