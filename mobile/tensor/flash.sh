#!/bin/bash
#
# Flash script for Google Tensor devices
# UOS Mobile OS
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/output"

# Parse arguments
FLASH_FULL=false

for arg in "$@"; do
    case $arg in
        --full)
            FLASH_FULL=true
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --full    Flash complete factory image (boot + dtbo + vbmeta)"
            exit 0
            ;;
    esac
done

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check for fastboot
if ! command -v fastboot &> /dev/null; then
    log_error "fastboot not found. Install Android platform-tools."
    exit 1
fi

# Check device connection
if ! fastboot devices | grep -q "."; then
    log_error "No device in fastboot mode detected"
    echo "Boot device to fastboot: Power + Volume Down"
    exit 1
fi

log_info "Device detected, flashing..."

if [ "$FLASH_FULL" = true ]; then
    # Flash complete factory image
    fastboot flash boot "${OUTPUT_DIR}/boot.img"
    fastboot flash dtbo "${OUTPUT_DIR}/dtbo.img" 2>/dev/null || true
    fastboot flash vbmeta "${OUTPUT_DIR}/vbmeta.img" 2>/dev/null || true
    fastboot flash --disable-verity --disable-verification vbmeta 2>/dev/null || true
else
    # Flash boot.img only
    fastboot flash boot "${OUTPUT_DIR}/boot.img"
fi

fastboot reboot

log_info "Flash complete. Device rebooting..."