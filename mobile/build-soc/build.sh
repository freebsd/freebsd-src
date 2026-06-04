#!/usr/bin/env bash
set -euo pipefail

# Master multi-SoC build script for BSD-based Mobile OS
# Usage: ./build.sh --soc MT6893 --board X01BD --output out/MT6893/ --config default

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DRY_RUN=false

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }
log_debug() { echo -e "${BLUE}[DEBUG]${NC} $*"; }

# Supported SoCs
SUPPORTED_SOCs=(
    MT6893 MT6877 MT6833 MT6895
    MT6983 MT6985 MT6989 MT6991
    SM8450 SM8350 SM7450 SM7350 SM6375 SM6450 SM6625
    MSM8998
    S5E9925 S5E9810 S5E8845 S5E8535 S5E9830 S5E9825 S5E5510
    TENSOR_REDFIN TENSOR_BLUEJAY TENSOR_ZUMA
)

# Vendor detection
get_vendor() {
    local soc="$1"
    case "$soc" in
        MT6*|MT68*) echo "mediatek" ;;
        SM*|MSM*) echo "qualcomm" ;;
        S5E*) echo "samsung" ;;
        TENSOR*) echo "google" ;;
        *) log_error "Unknown SoC: $soc"; return 1 ;;
    esac
}

# Supported boards per SoC
get_boards() {
    local soc="$1"
    case "$soc" in
        MT6893) echo "X01BD mojito" ;;
        MT6877) echo "veux" ;;
        MT6833) echo "evergo" ;;
        MT6895) echo "plato" ;;
        MT6983) echo "cupid" ;;
        MT6985) echo "n8q" ;;
        MT6989) echo "houjiao" ;;
        MT6991) echo "v2309" ;;
        SM8450) echo "lavender lemonade" ;;
        SM8350) echo "lilac" ;;
        SM7450) echo "zen22" ;;
        SM7350) echo "rhodium" ;;
        SM6375) echo "pong" ;;
        SM6450) echo "ocean" ;;
        SM6625) echo "generic_lp5" ;;
        MSM8998) echo "dumpling" ;;
        S5E9925) echo "lavender g0 tq" ;;
        S5E9810) echo "exynos2100 o1s" ;;
        S5E8845) echo "a54x" ;;
        S5E8535) echo "a14x" ;;
        S5E9830) echo "s5e9830 c2s" ;;
        S5E9825) echo "c2 beyond" ;;
        S5E5510) echo "a04e" ;;
        TENSOR_REDFIN) echo "oriole raven bluejay-alt" ;;
        TENSOR_BLUEJAY) echo "panther cheetah bluejay" ;;
        TENSOR_ZUMA) echo "shiba felix husky" ;;
        *) log_error "Unknown SoC: $soc"; return 1 ;;
    esac
}

# Detect SoC from device tree
detect_soc() {
    if [[ -f /proc/device-tree/compatible ]]; then
        local compat
        compat=$(cat /proc/device-tree/compatible 2>/dev/null || echo "")
        case "$compat" in
            *"MT6893"*) echo "MT6893" ;;
            *"MT6877"*) echo "MT6877" ;;
            *"MT6833"*) echo "MT6833" ;;
            *"MT6895"*) echo "MT6895" ;;
            *"MT6983"*) echo "MT6983" ;;
            *"MT6985"*) echo "MT6985" ;;
            *"MT6989"*) echo "MT6989" ;;
            *"MT6991"*) echo "MT6991" ;;
            *"SM8450"*) echo "SM8450" ;;
            *"SM8350"*) echo "SM8350" ;;
            *"SM7450"*) echo "SM7450" ;;
            *"SM7350"*) echo "SM7350" ;;
            *"SM6375"*) echo "SM6375" ;;
            *"SM6450"*) echo "SM6450" ;;
            *"SM6625"*) echo "SM6625" ;;
            *"MSM8998"*) echo "MSM8998" ;;
            *"S5E9925"*) echo "S5E9925" ;;
            *"S5E9810"*) echo "S5E9810" ;;
            *"S5E8845"*) echo "S5E8845" ;;
            *"S5E8535"*) echo "S5E8535" ;;
            *"S5E9830"*) echo "S5E9830" ;;
            *"S5E9825"*) echo "S5E9825" ;;
            *"S5E5510"*) echo "S5E5510" ;;
            "Tensor"*) echo "TENSOR_BLUEJAY" ;;
            *) log_error "Cannot detect SoC from device tree"; return 1 ;;
        esac
    else
        log_error "Device tree not found at /proc/device-tree/compatible"
        return 1
    fi
}

# Toolchain selection
get_toolchain() {
    local vendor="$1"
    case "$vendor" in
        mediatek) echo "clang aarch64-elf-" ;;
        qualcomm) echo "clang aarch64-elf-" ;;
        samsung) echo "aarch64-elf- clang" ;;
        google) echo "clang aarch64-elf-" ;;
        *) log_error "Unknown vendor: $vendor"; return 1 ;;
    esac
}

# Validate SoC
validate_soc() {
    local soc="$1"
    for s in "${SUPPORTED_SOCs[@]}"; do
        if [[ "$s" == "$soc" ]]; then
            return 0
        fi
    done
    log_error "Unsupported SoC: $soc"
    log_info "Supported SoCs: ${SUPPORTED_SOCs[*]}"
    return 1
}

# Validate board
validate_board() {
    local soc="$1"
    local board="$2"
    local boards
    boards=$(get_boards "$soc")
    for b in $boards; do
        if [[ "$b" == "$board" ]]; then
            return 0
        fi
    done
    log_error "Unsupported board $board for SoC $soc"
    log_info "Supported boards for $soc: $boards"
    return 1
}

# Select config file
select_config() {
    local soc="$1"
    local config="$2"
    local vendor
    vendor=$(get_vendor "$soc")
    
    local config_file="$SCRIPT_DIR/configs/${vendor}/${config}.mk"
    if [[ ! -f "$config_file" ]]; then
        log_error "Config file not found: $config_file"
        return 1
    fi
    echo "$config_file"
}

# Build function
build_target() {
    local soc="$1"
    local board="$2"
    local output="$3"
    local config="$4"
    
    local vendor config_file
    vendor=$(get_vendor "$soc")
    config_file=$(select_config "$soc" "$config")
    
    log_info "Building for SoC: $soc, Board: $board, Vendor: $vendor"
    
    # Create output directory
    mkdir -p "$output"
    
    # Set toolchain environment
    local tc
    tc=$(get_toolchain "$vendor")
    log_debug "Using toolchain: $tc"
    
    if [[ "$DRY_RUN" == "true" ]]; then
        log_info "[DRY-RUN] Would run: make -f Makefile SOC=$soc BOARD=$board CONFIG=$config_file OUTPUT=$output"
        return 0
    fi
    
    # Apply patches first
    "$SCRIPT_DIR/apply_patch.sh" --soc "$soc" --board "$board"
    
    # Run vendor-specific make target
    make -f "$SCRIPT_DIR/Makefile" SOC="$soc" BOARD="$board" CONFIG="$config_file" OUTPUT="$output"
    
    # Copy artifacts to output
    copy_artifacts "$soc" "$output"
    
    log_info "Build complete. Output in: $output"
}

copy_artifacts() {
    local soc="$1"
    local output="$2"
    
    local vendor
    vendor=$(get_vendor "$soc")
    
    local artifacts="boot.img dtbo.img super.img"
    if [[ "$vendor" == "qualcomm" ]]; then
        artifacts="$artifacts vbmeta.img vendor_boot.img"
    fi
    
    for artifact in $artifacts; do
        if [[ -f "$artifact" ]]; then
            cp "$artifact" "$output/"
            log_info "Copied $artifact to $output/"
        else
            log_warn "Artifact $artifact not found, skipping"
        fi
    done
    
    # Generate update.zip
    generate_update_zip "$soc" "$output"
}

generate_update_zip() {
    local soc="$1"
    local output="$2"
    local vendor
    vendor=$(get_vendor "$soc")
    
    local update_zip="$output/update.zip"
    mkdir -p "$output/UPDATE"
    
    # Create update.zip structure
    mkdir -p "$output/UPDATE/META-INF/com/google/android"
    cat > "$output/UPDATE/META-INF/com/google/android/updater-script" << 'EOF'
ui_print("Installing BSD Mobile update...");
EOF
    
    case "$vendor" in
        mediatek)
            echo 'package_extract_file("boot.img", "RAMDISK");' >> "$output/UPDATE/META-INF/com/google/android/updater-script"
            echo 'package_extract_file("super.img", "/dev/block/platform/by-name/super");' >> "$output/UPDATE/META-INF/com/google/android/updater-script"
            ;;
        qualcomm)
            echo 'package_extract_file("boot.img", "/dev/block/bootdevice/by-name/boot_a");' >> "$output/UPDATE/META-INF/com/google/android/updater-script"
            echo 'package_extract_file("super.img", "/dev/block/bootdevice/by-name/super");' >> "$output/UPDATE/META-INF/com/google/android/updater-script"
            ;;
        samsung)
            echo 'write_raw_image_package("boot.img", "boot");' >> "$output/UPDATE/META-INF/com/google/android/updater-script"
            echo 'write_raw_image_package("super.img", "super");' >> "$output/UPDATE/META-INF/com/google/android/updater-script"
            ;;
        google)
            echo 'package_extract_file("boot.img", "/dev/block/platform/by-name/boot_a");' >> "$output/UPDATE/META-INF/com/google/android/updater-script"
            echo 'package_extract_file("super.img", "/dev/block/platform/by-name/super");' >> "$output/UPDATE/META-INF/com/google/android/updater-script"
            ;;
    esac
    
    (cd "$output" && zip -r "$update_zip" UPDATE/ 2>/dev/null || true)
    log_info "Generated $update_zip"
}

# Parse arguments
SOC=""
BOARD=""
OUTPUT="out/"
CONFIG="default"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --soc)
            SOC="$2"
            shift 2
            ;;
        --board)
            BOARD="$2"
            shift 2
            ;;
        --output)
            OUTPUT="$2"
            shift 2
            ;;
        --config)
            CONFIG="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --detect)
            SOC=$(detect_soc)
            shift
            ;;
        -h|--help)
            echo "Usage: $0 --soc <SoC> --board <board> --output <dir> --config <config>"
            echo "Supported SoCs: ${SUPPORTED_SOCs[*]}"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Validate required args
if [[ -z "$SOC" ]]; then
    log_error "--soc is required (or use --detect)"
    exit 1
fi

if [[ -z "$BOARD" ]]; then
    log_error "--board is required"
    exit 1
fi

validate_soc "$SOC"
validate_board "$SOC" "$BOARD"

build_target "$SOC" "$BOARD" "$OUTPUT" "$CONFIG"
