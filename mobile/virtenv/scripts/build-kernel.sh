#!/bin/bash
#
# Build Kernel for Virtual Environment
# Mobile OS Project

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VIRTENV_DIR="$(dirname "$SCRIPT_DIR")"
MOBILE_DIR="$(dirname "$VIRTENV_DIR")"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[INFO]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1" >&2; }

print_status "Building kernel for virtual environment..."
echo ""

# Check toolchain
if ! command -v riscv64-unknown-elf-gcc &> /dev/null; then
    print_error "RISC-V toolchain not found"
    echo "Install: sudo apt-get install gcc-riscv64-unknown-elf"
    exit 1
fi

print_status "Building with: riscv64-unknown-elf-gcc"
riscv64-unknown-elf-gcc --version | head -1
echo ""

# Build kernel
print_status "Building RISC-V kernel..."
cd "$MOBILE_DIR"

make -f arch/riscv/Makefile.riscv \
    riscv-check \
    riscv-build \
    QEMU=1 \
    CONFIG_QEMU=y \
    CONFIG_RISCV_DEBUG=y

print_status "Kernel build complete"
echo ""

# Copy kernel to virtual environment
if [ -f "vmlinux.riscv64" ]; then
    print_status "Copying kernel to virtual environment..."
    cp vmlinux.riscv64 "$VIRTENV_DIR/kernel/"
    print_status "Kernel ready at: $VIRTENV_DIR/kernel/vmlinux.riscv64"
else
    print_warning "Kernel binary not found"
fi

echo ""
print_status "Next: Run with ./qemu-run.sh"
