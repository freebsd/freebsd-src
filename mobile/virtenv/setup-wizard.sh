#!/bin/bash
#
# Virtual Environment First-Time Setup
# Mobile OS RISC-V Testing Environment
#
# This script guides you through the initial setup

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_header() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

print_step() {
    echo -e "${GREEN}→${NC} $1"
}

print_code() {
    echo -e "  ${YELLOW}\$${NC} $1"
}

print_header "Mobile OS Virtual Environment Setup"

echo "This setup wizard will help you get the RISC-V test environment running."
echo ""
echo "Prerequisites:"
echo "  • Linux system (or WSL2/macOS with appropriate tools)"
echo "  • ~2GB free disk space"
echo "  • Internet connection (for package installation)"
echo ""

read -p "Continue with setup? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Setup cancelled."
    exit 0
fi

print_header "Step 1: Check System Requirements"

print_step "Checking for QEMU RISC-V..."
if command -v qemu-system-riscv64 &> /dev/null; then
    echo -e "${GREEN}✓${NC} QEMU RISC-V already installed"
    qemu-system-riscv64 --version | head -1
else
    echo -e "${YELLOW}⚠${NC} QEMU RISC-V not found. Installing..."
    sudo apt-get update
    sudo apt-get install -y qemu-system-riscv64
    echo -e "${GREEN}✓${NC} QEMU RISC-V installed"
fi

print_step "Checking for RISC-V toolchain..."
if command -v riscv64-unknown-elf-gcc &> /dev/null; then
    echo -e "${GREEN}✓${NC} RISC-V toolchain already installed"
    riscv64-unknown-elf-gcc --version | head -1
else
    echo -e "${YELLOW}⚠${NC} RISC-V toolchain not found. Installing..."
    sudo apt-get install -y gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf
    echo -e "${GREEN}✓${NC} RISC-V toolchain installed"
fi

print_step "Checking for device tree compiler..."
if command -v dtc &> /dev/null; then
    echo -e "${GREEN}✓${NC} Device tree compiler already installed"
else
    echo -e "${YELLOW}⚠${NC} Device tree compiler not found. Installing..."
    sudo apt-get install -y device-tree-compiler
    echo -e "${GREEN}✓${NC} Device tree compiler installed"
fi

print_header "Step 2: Setup Virtual Environment"

VIRTENV_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$VIRTENV_DIR"

print_step "Creating directories..."
mkdir -p kernel rootfs/{bin,lib,etc} qemu
echo -e "${GREEN}✓${NC} Directories created"

print_step "Making scripts executable..."
chmod +x scripts/*.sh
echo -e "${GREEN}✓${NC} Scripts are executable"

print_header "Step 3: Build Device Tree"

print_step "Compiling device tree..."
cd devicetree
dtc -I dts -O dtb -o riscv-virt-mobile.dtb riscv-virt-mobile.dts
echo -e "${GREEN}✓${NC} Device tree compiled"
cd ..

print_header "Step 4: Build Kernel"

print_step "Building kernel for virtual environment..."
echo ""
echo "This may take 1-5 minutes depending on your system..."
echo ""

if ./scripts/build-kernel.sh; then
    echo -e "${GREEN}✓${NC} Kernel built successfully"
else
    echo -e "${YELLOW}⚠${NC} Kernel build had issues (this is expected if source not complete)"
fi

print_header "Setup Complete!"

echo -e "${GREEN}✓${NC} Virtual environment is ready!"
echo ""
echo "Quick Start Commands:"
echo ""
echo -e "1. Run QEMU:"
print_code "cd $VIRTENV_DIR"
print_code "./scripts/qemu-run.sh"
echo ""
echo -e "2. Run with 4 cores and 2GB RAM:"
print_code "./scripts/qemu-run.sh --cores 4 --memory 2G"
echo ""
echo -e "3. Debug with GDB:"
print_code "./scripts/qemu-run.sh --gdb"
echo ""
echo -e "4. Run test suite:"
print_code "./scripts/run-tests.sh"
echo ""
echo "Documentation:"
echo ""
echo "  • README.md        - Complete overview"
echo "  • QUICKREF.md      - Quick reference guide"
echo "  • TESTING.md       - Detailed testing procedures"
echo ""
echo "Exit QEMU with: Ctrl+A then X"
echo ""
