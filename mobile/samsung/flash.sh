#!/bin/bash
#
# Flash script for Samsung Exynos devices
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
            echo "  --full    Flash complete image via download mode"
            exit 0
            ;;
    esac
done

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check for heimdall (Samsung's Odin alternative on Linux)
if command -v heimdall &> /dev/null; then
    FLASH_TOOL="heimdall"
elif command -v odin &> /dev/null; then
    FLASH_TOOL="odin"
else
    log_error "Neither heimdall nor odin found."
    echo "Install heimdall: apt-get install heimdall-flash"
    exit 1
fi

# Check device connection
if [ "$FLASH_TOOL" = "heimdall" ]; then
    if ! heimdall detect; then
        log_error "No device in download mode detected"
        echo "Boot device to download mode: Power + Volume Down + Volume Up (or USB jig)"
        exit 1
    fi
    log_info "Device detected via heimdall, flashing..."
    heimdall flash --BOOT "${OUTPUT_DIR}/boot.img"
else
    log_info "Using Odin - ensure device is connected in Download mode"
    odin -c "${OUTPUT_DIR}/boot.img"
fi

log_info "Flash complete."