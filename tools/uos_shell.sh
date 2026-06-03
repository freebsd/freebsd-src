#!/bin/bash
# =============================================================================
# UOS - Interactive Task Shell
# =============================================================================
# Purpose: Unified task runner to test, configure, and build kernels for UOS.
# =============================================================================

set -e

# Colors
BLUE='\033[0;34m'
GREEN='\033[0;32m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

FREEBSD_SRC="${FREEBSD_SRC:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"

show_header() {
    clear
    echo -e "${BLUE}======================================================================${NC}"
    echo -e "${CYAN}                    UOS TASK RUNNER SHELL                             ${NC}"
    echo -e "${BLUE}======================================================================${NC}"
}

run_menu() {
    while true; do
        show_header
        echo -e " What would you like to do?\n"
        echo -e "  ${GREEN}1)${NC} Run Unified Tests Across All Archs"
        echo -e "  ${GREEN}2)${NC} Build MediaTek (MT8395) Kernel"
        echo -e "  ${GREEN}3)${NC} Build Qualcomm Kernel"
        echo -e "  ${GREEN}4)${NC} Build RISC-V Kernel"
        echo -e "  ${GREEN}5)${NC} Launch MediaTek QEMU (GUI Test)"
        echo -e "  ${GREEN}6)${NC} Launch RISC-V QEMU (GUI Test)"
        echo -e "  ${GREEN}7)${NC} Create MediaTek Disk Image"
        echo -e "  ${GREEN}8)${NC} Clean & Full Build (World + Kernel)"
        echo -e "  ${GREEN}0)${NC} Exit\n"
        
        read -p "> " choice
        case $choice in
            1)
                echo -e "\n${BLUE}>>> Launching Unified Tests...${NC}"
                if [ -f "$FREEBSD_SRC/tools/run_unified_tests.sh" ]; then
                    bash "$FREEBSD_SRC/tools/run_unified_tests.sh"
                else
                    echo -e "${RED}Error: run_unified_tests.sh not found.${NC}"
                fi
                read -p "Press Enter to return to menu..."
                ;;
            2)
                echo -e "\n${BLUE}>>> Building MediaTek Kernel...${NC}"
                if [ -f "$FREEBSD_SRC/tools/mediatek/build-mediatek-kernel.sh" ]; then
                    bash "$FREEBSD_SRC/tools/mediatek/build-mediatek-kernel.sh"
                else
                    echo -e "${RED}Error: build-mediatek-kernel.sh not found.${NC}"
                fi
                read -p "Press Enter to return to menu..."
                ;;
            3)
                echo -e "\n${BLUE}>>> Building Qualcomm Kernel...${NC}"
                if [ -f "$FREEBSD_SRC/tools/qualcomm/build-qcom-kernel.sh" ]; then
                    bash "$FREEBSD_SRC/tools/qualcomm/build-qcom-kernel.sh"
                else
                    # Fallback to general build command if specific script missing
                    echo -e "${BLUE}Using general build interface...${NC}"
                    python3 "$FREEBSD_SRC/tools/build/make.py" buildkernel TARGET=arm64 TARGET_ARCH=aarch64 KERNCONF=QCOM-QEMU
                fi
                read -p "Press Enter to return to menu..."
                ;;
            4)
                echo -e "\n${BLUE}>>> Building RISC-V Kernel...${NC}"
                if [ -f "$FREEBSD_SRC/tools/riscv/build-riscv-kernel.sh" ]; then
                    bash "$FREEBSD_SRC/tools/riscv/build-riscv-kernel.sh"
                else
                    echo -e "${BLUE}Using general build interface...${NC}"
                    python3 "$FREEBSD_SRC/tools/build/make.py" buildkernel TARGET=riscv TARGET_ARCH=riscv64 KERNCONF=UOS-RISCV-QEMU
                fi
                read -p "Press Enter to return to menu..."
                ;;
            5)
                echo -e "\n${CYAN}>>> Launching MediaTek QEMU Environment...${NC}"
                if [ -f "$FREEBSD_SRC/tools/mediatek/qemu-mediatek-run.sh" ]; then
                    bash "$FREEBSD_SRC/tools/mediatek/qemu-mediatek-run.sh"
                else
                    echo -e "${RED}Error: qemu-mediatek-run.sh not found.${NC}"
                fi
                read -p "Press Enter to return to menu..."
                ;;
            6)
                echo -e "\n${CYAN}>>> Launching RISC-V QEMU Environment...${NC}"
                if [ -f "$FREEBSD_SRC/tools/riscv/qemu-riscv-run.sh" ]; then
                    bash "$FREEBSD_SRC/tools/riscv/qemu-riscv-run.sh"
                else
                    echo -e "${RED}Error: qemu-riscv-run.sh not found.${NC}"
                fi
                read -p "Press Enter to return to menu..."
                ;;
            7)
                echo -e "\n${CYAN}>>> Creating MediaTek Disk Image...${NC}"
                if [ -f "$FREEBSD_SRC/tools/mediatek/create-disk-image.sh" ]; then
                    bash "$FREEBSD_SRC/tools/mediatek/create-disk-image.sh"
                else
                    echo -e "${RED}Error: create-disk-image.sh not found.${NC}"
                fi
                read -p "Press Enter to return to menu..."
                ;;
            8)
                echo -e "\n${BLUE}>>> Initiating Clean Full build (World + MediaTek Kernel)...${NC}"
                echo -e "${RED}WARNING: This takes 1-2 hours.${NC}"
                read -p "Proceed? (y/N) " confirm
                if [[ $confirm == [yY] ]]; then
                    BUILD_WORLD=yes bash "$FREEBSD_SRC/tools/mediatek/build-mediatek-kernel.sh"
                fi
                read -p "Press Enter to return to menu..."
                ;;
            0)
                echo -e "${CYAN}Exiting ${BLUE}UOS${CYAN} Shell. Goodbye!${NC}"
                exit 0
                ;;
            *)
                echo -e "${RED}Invalid option selected.${NC}"
                sleep 1
                ;;
        esac
    done
}

run_menu
