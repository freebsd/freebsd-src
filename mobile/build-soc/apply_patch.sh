#!/usr/bin/env bash
set -euo pipefail

# Pre-build SoC-specific kernel patches
# Usage: apply_patch.sh --soc MT6893 --board X01BD

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

# Apply device tree overlays using dtc
apply_dt_overlay() {
    local soc="$1"
    local board="$2"
    local patch_name="$3"
    local dts_file="$SCRIPT_DIR/overlays/${soc}_${board}_${patch_name}.dts"
    local dtb_file="$SCRIPT_DIR/overlays/${soc}_${board}_${patch_name}.dtbo"
    
    if [[ -f "$dts_file" ]]; then
        log_info "Applying $patch_name overlay for $board..."
        
        if command -v dtc >/dev/null 2>&1; then
            dtc -@ -I dts -O dtb -o "$dtb_file" "$dts_file"
            log_info "Generated $dtb_file"
        else
            log_warn "dtc compiler not found, skipping overlay compilation"
        fi
    fi
}

# Apply UFS patch
apply_ufs_patch() {
    local soc="$1"
    local board="$2"
    apply_dt_overlay "$soc" "$board" "ufs"
}

# Apply display patch
apply_display_patch() {
    local soc="$1"
    local board="$2"
    apply_dt_overlay "$soc" "$board" "display"
}

# Apply modem patch
apply_modem_patch() {
    local soc="$1"
    local board="$2"
    apply_dt_overlay "$soc" "$board" "modem"
}

# Apply GPU patch
apply_gpu_patch() {
    local soc="$1"
    local board="$2"
    apply_dt_overlay "$soc" "$board" "gpu"
}

# Apply sensors patch
apply_sensors_patch() {
    local soc="$1"
    local board="$2"
    apply_dt_overlay "$soc" "$board" "sensors"
}

# Apply all patches for a board
apply_all_patches() {
    local soc="$1"
    local board="$2"
    
    log_info "Applying patches for SoC $soc, Board $board..."
    
    mkdir -p "$SCRIPT_DIR/overlays"
    
    apply_ufs_patch "$soc" "$board"
    apply_display_patch "$soc" "$board"
    apply_modem_patch "$soc" "$board"
    apply_gpu_patch "$soc" "$board"
    apply_sensors_patch "$soc" "$board"
    
    log_info "Patches applied successfully"
}

# Generate overlay dts files for a board
generate_overlay_templates() {
    local soc="$1"
    local board="$2"
    local overlay_dir="$SCRIPT_DIR/overlays"
    
    mkdir -p "$overlay_dir"
    
    # UFS overlay template
    cat > "$overlay_dir/${soc}_${board}_ufs.dts" << EOF
/dts-v1/;

/ {
    fragment@0 {
        target = <0xffffffff>;
        __overlay__ {
            ufs {
                compatible = "bsd,ufs";
                reg = <0x1 0x0>;
                bsd-ufs-variant = "$soc";
            };
        };
    };
};
EOF
    
    # Display overlay template
    cat > "$overlay_dir/${soc}_${board}_display.dts" << EOF
/dts-v1/;

/ {
    fragment@0 {
        target = <0xffffffff>;
        __overlay__ {
            panel {
                compatible = "bsd,display-panel";
                bsd-display-soc = "$soc";
            };
        };
    };
};
EOF
    
    # Modem overlay template
    cat > "$overlay_dir/${soc}_${board}_modem.dts" << EOF
/dts-v1/;

/ {
    fragment@0 {
        target = <0xffffffff>;
        __overlay__ {
            modem {
                compatible = "bsd,modem";
                bsd-modem-soc = "$soc";
            };
        };
    };
};
EOF
    
    # GPU overlay template
    cat > "$overlay_dir/${soc}_${board}_gpu.dts" << EOF
/dts-v1/;

/ {
    fragment@0 {
        target = <0xffffffff>;
        __overlay__ {
            gpu {
                compatible = "bsd,gpu";
                bsd-gpu-soc = "$soc";
            };
        };
    };
};
EOF
    
    # Sensors overlay template
    cat > "$overlay_dir/${soc}_${board}_sensors.dts" << EOF
/dts-v1/;

/ {
    fragment@0 {
        target = <0xffffffff>;
        __overlay__ {
            sensors {
                compatible = "bsd,sensors";
                bsd-soc-hw = "$soc";
            };
        };
    };
};
EOF
}

# Parse arguments
SOC=""
BOARD=""
GENERATE_TEMPLATES=false

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
        --generate-templates)
            GENERATE_TEMPLATES=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 --soc <SoC> --board <board> [--generate-templates]"
            echo "Patches: ufs, display, modem, gpu, sensors"
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

if [[ -z "$BOARD" ]]; then
    log_error "--board is required"
    exit 1
fi

if [[ "$GENERATE_TEMPLATES" == "true" ]]; then
    generate_overlay_templates "$SOC" "$BOARD"
    log_info "Generated overlay templates in $SCRIPT_DIR/overlays/"
    exit 0
fi

apply_all_patches "$SOC" "$BOARD"
