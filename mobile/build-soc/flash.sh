#!/usr/bin/env bash
set -euo pipefail

# Universal flash script for BSD-based Mobile OS
# Usage: flash.sh --soc MT6893 --mode fastboot|recovery|download

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

preflight_check() {
    local mode="$1"
    log_info "Running pre-flash checks..."
    
    # Check bootloader unlock status
    if ! check_bootloader_unlock "$mode"; then
        log_error "Bootloader is locked. Please unlock before flashing."
        return 1
    fi
    
    # Check battery level
    if ! check_battery "$mode"; then
        log_error "Battery level below 50%. Please charge device."
        return 1
    fi
    
    log_info "Pre-flight checks passed."
    return 0
}

check_bootloader_unlock() {
    local mode="$1"
    
    case "$mode" in
        fastboot)
            local status
            status=$(fastboot getvar unlocked 2>/dev/null | grep -oE 'unlocked: (yes|no)' || echo "unlocked: no")
            if [[ "$status" == *"yes"* ]]; then
                return 0
            fi
            return 1
            ;;
        recovery)
            adb shell "getprop sys.usb.config" >/dev/null 2>&1
            return $?
            ;;
        download)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

check_battery() {
    local mode="$1"
    local level
    
    case "$FLASH_MODE" in
        fastboot)
            level=$(fastboot getvar battery-level 2>/dev/null | grep -oE '[0-9]+' || echo "0")
            ;;
        recovery)
            level=$(adb shell "cat /sys/class/power_supply/battery/capacity" 2>/dev/null || echo "0")
            ;;
        download)
            return 0
            ;;
        *) return 1 ;;
    esac
    
    if [[ "${level:-0}" -ge 50 ]]; then
        return 0
    fi
    return 1
}

flash_fastboot() {
    local soc="$1"
    local output="$2"
    
    log_info "Flashing via fastboot..."
    
    preflight_check fastboot || return 1
    
    if [[ "$WIPESYSTEM" == "true" ]]; then
        log_info "Wiping system..."
        fastboot erase system || true
    fi
    
    if [[ "$WIPEDATA" == "true" ]]; then
        log_info "Wiping userdata..."
        fastboot erase userdata || true
    fi
    
    if [[ "$WIPECACHE" == "true" ]]; then
        log_info "Wiping cache..."
        fastboot erase cache || true
    fi
    
    "$SCRIPT_DIR/fastboot_all.sh" --soc "$soc" --output "$output" ${SLOT:+-S "$SLOT"}
    
    if [[ "$NO_VERITY" == "true" ]]; then
        log_info "Disabling verity..."
        fastboot --disable-verity flash boot "$output/boot.img"
    fi
    
    if [[ "$NO_ENCRYPTION" == "true" ]]; then
        log_info "Disabling encryption..."
        fastboot --disable-encryption flash userdata "$output/userdata.img" || true
    fi
    
    fastboot reboot
}

flash_recovery() {
    local output="$1"
    
    log_info "Flashing via recovery sideload..."
    
    preflight_check recovery || return 1
    
    if [[ ! -f "$output/update.zip" ]]; then
        log_error "update.zip not found in $output"
        return 1
    fi
    
    adb sideload "$output/update.zip"
}

flash_download() {
    local soc="$1"
    local output="$2"
    
    log_info "Flashing via MediaTek download mode..."
    
    preflight_check download || return 1
    
    if [[ ! -f "$output/preloader.bin" ]]; then
        log_error "preloader.bin not found"
        return 1
    fi
    
    if command -v sp_flash_tool >/dev/null 2>&1; then
        sp_flash_tool --port "/dev/ttyUSB0" --preloader "$output/preloader.bin" --scatter "$SCRIPT_DIR/configs/mediatek/scatter.txt"
    else
        log_warn "SP Flash Tool not found. manual flashing required."
        log_info "Use SP Flash Tool with scatter file: $SCRIPT_DIR/configs/mediatek/scatter.txt"
    fi
}

# Parse arguments
SOC=""
FLASH_MODE=""
OUTPUT=""
WIPESYSTEM=false
WIPEDATA=false
WIPECACHE=false
NO_VERITY=false
NO_ENCRYPTION=false
SLOT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --soc)
            SOC="$2"
            shift 2
            ;;
        --mode)
            FLASH_MODE="$2"
            shift 2
            ;;
        --output)
            OUTPUT="$2"
            shift 2
            ;;
        --wipesystem)
            WIPESYSTEM=true
            shift
            ;;
        --wipedata)
            WIPEDATA=true
            shift
            ;;
        --wipecache)
            WIPECACHE=true
            shift
            ;;
        --no-verity)
            NO_VERITY=true
            shift
            ;;
        --no-encryption)
            NO_ENCRYPTION=true
            shift
            ;;
        --slot)
            SLOT="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 --soc <SoC> --mode <fastboot|recovery|download> --output <dir>"
            echo "Options:"
            echo "  --wipesystem     Wipe system partition before flash"
            echo "  --wipedata       Wipe userdata partition before flash"
            echo "  --wipecache      Wipe cache partition before flash"
            echo "  --no-verity      Flash with verity disabled"
            echo "  --no-encryption  Flash with encryption disabled"
            echo "  --slot A|B       Target slot for AB devices"
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

if [[ -z "$FLASH_MODE" ]]; then
    log_error "--mode is required (fastboot|recovery|download)"
    exit 1
fi

if [[ -z "$OUTPUT" ]]; then
    OUTPUT="out/$SOC/"
fi

case "$FLASH_MODE" in
    fastboot)
        flash_fastboot "$SOC" "$OUTPUT"
        ;;
    recovery)
        flash_recovery "$OUTPUT"
        ;;
    download)
        flash_download "$SOC" "$OUTPUT"
        ;;
    *)
        log_error "Unknown flash mode: $FLASH_MODE"
        exit 1
        ;;
esac
