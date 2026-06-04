#!/usr/bin/env bash
set -euo pipefail

# Vendor-specific fastboot flash script
# Usage: fastboot_all.sh --soc <SoC> --output <dir> [--slot A|B]

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

flash_mediatek() {
    local output="$1"
    
    log_info "Using MediaTek flash protocol..."
    
    if [[ -f "$output/super.img" ]]; then
        if fastboot flash --super "$output/super.img"; then
            log_info "Flashed super.img"
        else
            fastboot flash boot "$output/boot.img" || true
            fastboot flash dtbo "$output/dtbo.img" || true
        fi
    fi
    
    if [[ -f "$output/boot.img" ]]; then
        fastboot flash boot "$output/boot.img"
        log_info "Flashed boot.img"
    fi
    
    if [[ -f "$output/dtbo.img" ]]; then
        fastboot flash dtbo "$output/dtbo.img"
        log_info "Flashed dtbo.img"
    fi
}

flash_qualcomm() {
    local output="$1"
    local slot="${2:-}"
    
    log_info "Using Qualcomm fastboot protocol..."
    
    local boot_img="$output/boot.img"
    local super_img="$output/super.img"
    local vbmeta_img="$output/vbmeta.img"
    
    if [[ -n "$slot" ]]; then
        fastboot flash --slot "$slot" boot "$boot_img"
        fastboot flash --slot "$slot" super "$super_img"
    else
        fastboot flash boot "$boot_img"
        fastboot flash super "$super_img"
    fi
    
    if [[ -f "$vbmeta_img" ]]; then
        fastboot flash vbmeta "$vbmeta_img"
        log_info "Flashed vbmeta.img"
    fi
    
    if [[ -f "$output/vendor_boot.img" ]]; then
        fastboot flash vendor_boot "$output/vendor_boot.img"
        log_info "Flashed vendor_boot.img"
    fi
    
    log_info "Flashed QCOM images"
}

flash_samsung() {
    local output="$1"
    
    log_info "Using Samsung Heimdall protocol..."
    
    if command -v heimdall >/dev/null 2>&1; then
        if [[ -f "$output/boot.img" ]]; then
            heimdall flash --BOOTLOADER "$output/boot.img"
        fi
        if [[ -f "$output/super.img" ]]; then
            heimdall flash --SYSTEM "$output/super.img"
        fi
    else
        log_error "Heimdall not found. Install heimdall-flash package."
        return 1
    fi
}

flash_google() {
    local output="$1"
    local slot="${2:-}"
    
    log_info "Using Google fastboot protocol..."
    
    local boot_img="$output/boot.img"
    local super_img="$output/super.img"
    
    if [[ -n "$slot" ]]; then
        fastboot flash --slot "$slot" boot "$boot_img"
        fastboot flash --slot "$slot" super "$super_img"
    else
        fastboot flash boot "$boot_img"
        fastboot flash super "$super_img"
    fi
    
    log_info "Flashed Google/Tensor images"
}

# Parse arguments
SOC=""
OUTPUT=""
SLOT=""

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
        --slot)
            SLOT="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 --soc <SoC> --output <dir> [--slot A|B]"
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

if [[ -z "$OUTPUT" ]]; then
    OUTPUT="out/$SOC/"
fi

VENDOR=$(get_vendor "$SOC")

case "$VENDOR" in
    mediatek)
        flash_mediatek "$OUTPUT"
        ;;
    qualcomm)
        flash_qualcomm "$OUTPUT" "$SLOT"
        ;;
    samsung)
        flash_samsung "$OUTPUT"
        ;;
    google)
        flash_google "$OUTPUT" "$SLOT"
        ;;
    *)
        log_error "Unsupported vendor: $VENDOR"
        exit 1
        ;;
esac
