#!/bin/bash
# =============================================================================
# UOS - Unified Kernel Build Script
# =============================================================================
# Purpose: Build the default QEMU and Hardware configurations across all
# supported Mobile SoCs (MediaTek, Qualcomm, RISC-V).
# =============================================================================

set -e

BLUE='\033[0;34m'; GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'
FREEBSD_SRC="${FREEBSD_SRC:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"

echo -e "${BLUE}======================================================================${NC}"
echo -e "${BLUE}  UOS Unified Cross-Architecture Builder                              ${NC}"
echo -e "${BLUE}======================================================================${NC}"

# Define the targets (Vendor -> Script -> Targets)
declare -A VENDORS
VENDORS=(
    ["mediatek"]="tools/mediatek/build-mediatek-kernel.sh|MEDIATEK-QEMU MEDIATEK"
    ["qualcomm"]="tools/qualcomm/build-qcom-kernel.sh|QCOM-QEMU QCOM"
    ["riscv"]="tools/riscv/build-riscv-kernel.sh|UOS-RISCV-QEMU UOS-STARFIVE"
)

export BUILD_WORLD=${BUILD_WORLD:-no}
FAILURE_COUNT=0

for VENDOR in "${!VENDORS[@]}"; do
    IFS='|' read -r SCRIPT TARGETS <<< "${VENDORS[$VENDOR]}"
    
    echo -e "\n${GREEN}>>> Next Target Vendor: ${VENDOR^^}${NC}"
    
    if [ ! -f "$FREEBSD_SRC/$SCRIPT" ]; then
        echo -e "${RED}[ERROR]${NC} Script $SCRIPT not found. Skipping $VENDOR."
        ((FAILURE_COUNT++))
        continue
    fi
    
    for TARGET in $TARGETS; do
        echo -e "${BLUE}[BUILD]${NC} Executing $SCRIPT $TARGET"
        if ! bash "$FREEBSD_SRC/$SCRIPT" "$TARGET"; then
            echo -e "${RED}[ERROR]${NC} Build failed for $VENDOR : $TARGET"
            ((FAILURE_COUNT++))
        fi
    done
done

echo -e "\n${BLUE}======================================================================${NC}"
if [ "$FAILURE_COUNT" -eq 0 ]; then
    echo -e "${GREEN}[SUCCESS]${NC} All targets built successfully!"
else
    echo -e "${RED}[WARNING]${NC} Completed with $FAILURE_COUNT failures."
    exit 1
fi
