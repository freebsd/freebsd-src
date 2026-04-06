#!/bin/bash
#
# QEMU RISC-V Virtual Environment Launcher
# Mobile OS Project
#
# Usage: ./qemu-run.sh [OPTIONS]
# Options:
#   --cores N        Number of CPU cores (default: 1)
#   --memory SIZE    RAM size (default: 512M, e.g., 2G, 1024M)
#   --debug          Enable debug output and GDB stub
#   --kernel PATH    Custom kernel image
#   --dtb PATH       Custom device tree binary
#   --gdb            Wait for GDB connection
#   --help           Show this help message

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VIRTENV_DIR="$(dirname "$SCRIPT_DIR")"
MOBILE_DIR="$(dirname "$VIRTENV_DIR")"

# Default configuration
CORES=1
MEMORY="512M"
DEBUG=0
KERNEL="$VIRTENV_DIR/kernel/vmlinux.riscv64"
DTB="$VIRTENV_DIR/devicetree/riscv-virt-mobile.dtb"
GDB_STUB=0
GDB_PORT=1234
MONITOR_PORT=55555
SERIAL_PORT=12345

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Functions
print_help() {
    head -n 14 "$0" | tail -n +2 | sed 's/^# //'
}

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

check_dependencies() {
    print_status "Checking dependencies..."
    
    if ! command -v qemu-system-riscv64 &> /dev/null; then
        print_error "QEMU RISC-V not found. Install with:"
        echo "  sudo apt-get install qemu-system-riscv64"
        exit 1
    fi
    
    if [ "$DEBUG" -eq 1 ] && ! command -v riscv64-unknown-elf-gdb &> /dev/null; then
        print_warning "RISC-V GDB not found. GDB debugging will not work."
    fi
    
    print_status "Dependencies OK"
}

check_files() {
    print_status "Checking required files..."
    
    if [ ! -f "$KERNEL" ]; then
        print_error "Kernel not found: $KERNEL"
        echo "Build with: make -f $MOBILE_DIR/arch/riscv/Makefile.riscv riscv-build"
        exit 1
    fi
    
    if [ ! -f "$DTB" ]; then
        print_warning "Device tree not found: $DTB"
        print_status "Using QEMU default device tree"
        DTB=""
    fi
    
    print_status "Files OK"
}

build_qemu_args() {
    local args=()
    
    # Machine configuration
    args+=("-machine" "virt")
    args+=("-cpu" "rv64")
    
    # Memory and cores
    args+=("-m" "$MEMORY")
    args+=("-smp" "$CORES")
    
    # Kernel and device tree
    args+=("-kernel" "$KERNEL")
    if [ -n "$DTB" ]; then
        args+=("-dtb" "$DTB")
    fi
    
    # Serial console (UART)
    args+=("-serial" "stdio")
    
    # Enable GDB stub
    if [ "$GDB_STUB" -eq 1 ]; then
        args+=("-gdb" "tcp::$GDB_PORT")
        args+=("-S")  # Stop on startup
    fi
    
    # Monitor port
    args+=("-monitor" "telnet:localhost:$MONITOR_PORT,server,nowait")
    
    # Debug output
    if [ "$DEBUG" -eq 1 ]; then
        args+=("-d" "int,cpu_reset,guest_errors")
        args+=("-D" "$VIRTENV_DIR/qemu-debug.log")
    fi
    
    # Additional useful options
    args+=("-nographic")
    
    echo "${args[@]}"
}

run_qemu() {
    print_status "Launching QEMU RISC-V Virtual Machine"
    print_status "Configuration:"
    print_status "  Cores:        $CORES"
    print_status "  Memory:       $MEMORY"
    print_status "  Kernel:       $KERNEL"
    print_status "  Debug:        $DEBUG"
    print_status "  GDB stub:     $GDB_STUB"
    echo ""
    
    if [ "$GDB_STUB" -eq 1 ]; then
        print_status "GDB stub listening on localhost:$GDB_PORT"
        print_status "In another terminal, run:"
        echo "    riscv64-unknown-elf-gdb $KERNEL"
        echo "    (gdb) target remote localhost:$GDB_PORT"
        echo "    (gdb) continue"
        echo ""
    fi
    
    print_status "QEMU Monitor available at: localhost:$MONITOR_PORT"
    print_status "Press Ctrl+A followed by X to exit QEMU"
    echo ""
    
    local qemu_args=$(build_qemu_args)
    
    # Run QEMU with built arguments
    # shellcheck disable=SC2086
    qemu-system-riscv64 $qemu_args
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --cores)
            CORES="$2"
            shift 2
            ;;
        --memory)
            MEMORY="$2"
            shift 2
            ;;
        --debug)
            DEBUG=1
            shift
            ;;
        --kernel)
            KERNEL="$2"
            shift 2
            ;;
        --dtb)
            DTB="$2"
            shift 2
            ;;
        --gdb)
            GDB_STUB=1
            DEBUG=1
            shift
            ;;
        --help)
            print_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Main execution
print_status "Mobile OS RISC-V Virtual Environment"
print_status "========================================"
echo ""

check_dependencies
check_files
run_qemu
