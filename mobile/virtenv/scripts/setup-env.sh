#!/bin/bash
#
# Setup Virtual Environment
# Mobile OS Project
#
# Configures QEMU and kernel for the first time

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VIRTENV_DIR="$(dirname "$SCRIPT_DIR")"

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[INFO]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1" >&2; }

print_status "Setting up Mobile OS Virtual Environment..."
echo ""

# Check QEMU installation
print_status "Checking QEMU installation..."
if ! command -v qemu-system-riscv64 &> /dev/null; then
    print_error "QEMU RISC-V not installed"
    echo "Install with: sudo apt-get install qemu-system-riscv64"
    exit 1
fi
echo "QEMU version: $(qemu-system-riscv64 --version)"
echo ""

# Check toolchain
print_status "Checking RISC-V toolchain..."
if ! command -v riscv64-unknown-elf-gcc &> /dev/null; then
    print_error "RISC-V toolchain not found"
    echo "Install with: sudo apt-get install gcc-riscv64-unknown-elf"
    exit 1
fi
echo "GCC version: $(riscv64-unknown-elf-gcc --version | head -1)"
echo ""

# Create necessary directories
print_status "Creating directories..."
mkdir -p "$VIRTENV_DIR"/{kernel,rootfs/{bin,lib,etc},qemu}
echo "Directories created"
echo ""

# Make scripts executable
print_status "Making scripts executable..."
chmod +x "$SCRIPT_DIR"/{qemu-run,build-kernel,run-tests}.sh
echo "Scripts ready"
echo ""

# Create symlink to kernel build output
print_status "Setting up kernel symlink..."
if [ ! -f "$VIRTENV_DIR/kernel/vmlinux.riscv64" ]; then
    print_status "Kernel not yet built. Build with:"
    echo "  cd /workspaces/freebsd-src/mobile"
    echo "  make -f arch/riscv/Makefile.riscv riscv-build"
fi
echo ""

print_status "Setup complete!"
echo ""
echo "Next steps:"
echo "1. Build kernel: make -f arch/riscv/Makefile.riscv riscv-build"
echo "2. Launch QEMU: $SCRIPT_DIR/qemu-run.sh"
echo "3. Or run tests: $SCRIPT_DIR/run-tests.sh"
echo ""
