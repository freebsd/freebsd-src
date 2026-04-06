# Virtual Environment Setup Complete ✓

## What Was Created

A complete RISC-V virtual testing environment for the Mobile OS project has been created in `/workspaces/freebsd-src/mobile/virtenv/`.

### Directory Structure

```
virtenv/                                  # Virtual environment root
├── setup-wizard.sh                       # Automated setup script (NEW)
├── README.md                             # Complete overview
├── QUICKREF.md                           # Quick reference guide
├── TESTING.md                            # Testing procedures
│
├── scripts/                              # Executable scripts
│   ├── qemu-run.sh                      # Main QEMU launcher ⭐
│   ├── setup-env.sh                      # Environment setup
│   ├── build-kernel.sh                   # Kernel builder
│   └── run-tests.sh                      # Test runner
│
├── kernel/                               # Kernel configuration
│   ├── config.h                          # Kernel settings
│   ├── entry.S                           # Boot code (RISC-V asm)
│   └── vmlinux.riscv64                   # (Will be created by build)
│
├── devicetree/                           # Hardware description
│   ├── riscv-virt-mobile.dts            # Device tree source
│   └── riscv-virt-mobile.dtb            # (Will be compiled)
│
├── rootfs/                               # Root filesystem
│   ├── bin/                              # (For future binaries)
│   ├── lib/                              # (For future libraries)
│   └── etc/                              # (For future configs)
│
└── qemu/                                 # QEMU configurations
    └── (Future config files)
```

## Getting Started

### Option 1: Automated Setup (Recommended)

```bash
cd /workspaces/freebsd-src/mobile/virtenv
./setup-wizard.sh
```

This script will:
- ✓ Check system requirements
- ✓ Install missing dependencies (QEMU, toolchain, etc.)
- ✓ Create directories
- ✓ Compile device tree
- ✓ Build kernel

### Option 2: Manual Setup

```bash
# 1. Install dependencies
sudo apt-get update
sudo apt-get install qemu-system-riscv64 gcc-riscv64-unknown-elf device-tree-compiler

# 2. Navigate to virtual environment
cd /workspaces/freebsd-src/mobile/virtenv

# 3. Build kernel
./scripts/build-kernel.sh

# 4. Run virtual machine
./scripts/qemu-run.sh
```

## Quick Start Commands

```bash
# Navigate to virtual environment
cd /workspaces/freebsd-src/mobile/virtenv

# Basic launch (1 core, 512MB RAM)
./scripts/qemu-run.sh

# With more resources
./scripts/qemu-run.sh --cores 4 --memory 2G

# With debugging enabled
./scripts/qemu-run.sh --debug

# With GDB support for kernel debugging
./scripts/qemu-run.sh --gdb

# Run test suite
./scripts/run-tests.sh
```

## Understanding the Components

### 1. QEMU Launcher (`scripts/qemu-run.sh`)

The main script that launches the QEMU RISC-V virtual machine with configurable:
- **CPU Cores**: 1-8 (default: 1)
- **Memory**: 256M-16GB (default: 512M)
- **Debug Mode**: Full console output and GDB stub
- **Custom Kernel**: Start with different kernels

**Usage:**
```bash
./scripts/qemu-run.sh [OPTIONS]
  --cores N        Number of CPU cores
  --memory SIZE    RAM size (e.g., 2G, 1024M)
  --debug          Enable debug logging
  --kernel PATH    Custom kernel binary
  --gdb            Enable GDB debugging stub
```

### 2. Kernel Builder (`scripts/build-kernel.sh`)

Compiles the RISC-V kernel from source using the FreeBSD Mobile OS build system.

**What it does:**
- Checks RISC-V toolchain
- Builds kernel with QEMU optimizations
- Copies output to `kernel/vmlinux.riscv64`

### 3. Device Tree (`devicetree/riscv-virt-mobile.dts`)

Hardware description in device tree format. Defines:
- CPU configuration (RV64IMAC ISA)
- Memory layout (16MB-8GB)
- Interrupt controller (PLIC)
- Timer (CLINT)
- Serial console (UART)
- Virtio devices (block storage, networking)
- Mobile-specific devices (touch input, sensors)
- Power management configuration

### 4. Kernel Configuration (`kernel/config.h`)

Constants and settings including:
- Memory addresses and sizes
- Device base addresses
- Interrupt mappings
- Performance settings
- Feature flags

### 5. Boot Code (`kernel/entry.S`)

RISC-V assembly code that:
- Serves as kernel entry point (executed by bootloader)
- Sets up stack
- Clears BSS section
- Jumps to kernel initialization

## Virtual Machine Specifications

### Simulated Hardware
- **CPU**: RISC-V RV64IMAC (configurable cores)
- **Memory**: DDR4 at 0x80000000 (512MB default, up to 8GB)
- **Interrupt Controller**: PLIC (Platform Level Interrupt Controller)
- **Timer**: CLINT with 10MHz frequency
- **Console**: UART at 115200 baud
- **Storage**: Virtio block device
- **Network**: Virtio network interface
- **Input**: Simulated touchscreen
- **Sensors**: Simulated IMU/sensors

### Device Memory Map

| Component | Address | Size | Purpose |
|-----------|---------|------|---------|
| DRAM | 0x80000000 | 512MB | Main memory |
| CLINT | 0x02000000 | 64KB | Timer/IPI |
| PLIC | 0x0c000000 | 64MB | Interrupts |
| UART0 | 0x10000000 | 4KB | Console |
| Virtio Block | 0x10001000 | 4KB | Storage |
| Virtio Net | 0x10002000 | 4KB | Network |

## Testing Capabilities

### Boot Tests
- Single-core boot
- Multi-core boot
- Memory detection
- Device initialization

### Hardware Tests
- UART console I/O
- Timer interrupts
- Interrupt controller (PLIC)
- Virtual device detection

### Performance Tests
- Boot time measurement
- CPU frequency verification
- Interrupt latency
- Memory bandwidth

### Compatibility Tests
- RV64IMAC ISA support
- Atomic operations
- Compressed instructions
- Standard POSIX features

## Debugging Setup

### GDB for Kernel Debugging

**Terminal 1** - Start QEMU with GDB stub:
```bash
./scripts/qemu-run.sh --gdb
```

**Terminal 2** - Connect debugger:
```bash
riscv64-unknown-elf-gdb kernel/vmlinux.riscv64
(gdb) target remote localhost:1234
(gdb) break kernel_init
(gdb) continue
(gdb) step
(gdb) info registers
```

### View Debug Output

```bash
# Run with debug enabled
./scripts/qemu-run.sh --debug

# Monitor debug log (in another terminal)
tail -f qemu-debug.log
```

## Important Files Reference

| File | Purpose | Edit When... |
|------|---------|--------------|
| `qemu-run.sh` | VM launcher | Need custom QEMU options |
| `build-kernel.sh` | Build script | Changing build flags |
| `config.h` | Kernel settings | Changing device addresses |
| `entry.S` | Boot code | Modifying boot sequence |
| `riscv-virt-mobile.dts` | Hardware description | Adding/removing devices |

## Documentation Files

Read in this order:

1. **QUICKREF.md** - Quick reference for common commands
2. **README.md** - Complete overview and architecture
3. **TESTING.md** - Detailed testing procedures
4. **../RISC-V_ARCHITECTURE.md** - Architecture deep dive
5. **../RISC-V_INTEGRATION_GUIDE.md** - Integration details

## Performance Baseline

Expected performance on virtual environment:

| Metric | Target |
|--------|--------|
| Boot time (1 core) | 2-3 seconds |
| Boot time (4 cores) | 3-5 seconds |
| App launch | < 500ms |
| Frame rate | 60 FPS (simulated) |
| Memory | 256MB minimum, 512MB standard |

## Troubleshooting

### QEMU Not Found
```bash
sudo apt-get install qemu-system-riscv64
```

### Toolchain Not Found
```bash
sudo apt-get install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf
```

### Device Tree Compiler Not Found
```bash
sudo apt-get install device-tree-compiler
```

### Kernel Won't Build
- Check RISC-V toolchain is installed: `riscv64-unknown-elf-gcc --version`
- Review build output for errors
- Ensure FreeBSD source tree is available

### Boot Hangs in QEMU
1. Enable debug mode: `./scripts/qemu-run.sh --debug`
2. Review qemu-debug.log for clues
3. Use GDB to step through boot process
4. Check device tree configuration

### GDB Connection Refused
- Ensure QEMU started with `--gdb` flag
- Check port 1234 is not in use: `lsof -i :1234`
- Verify RISC-V GDB installed: `riscv64-unknown-elf-gdb --version`

## Next Steps

1. ✅ Read QUICKREF.md for essential commands
2. ✅ Create test configuration: `./scripts/run-tests.sh`
3. ✅ Review device tree: `cat devicetree/riscv-virt-mobile.dts`
4. ✅ Understand boot process: `cat kernel/entry.S`
5. ✅ Test kernel changes with GDB debugging

## Project Integration

This virtual environment integrates with:
- `/workspaces/freebsd-src/mobile/arch/riscv/` - RISC-V architecture code
- Build system: `arch/riscv/Makefile.riscv`
- HAL: `arch/riscv/hal/riscv_hal.h`
- Drivers: `arch/riscv/drivers/`

## Advanced Configuration

### Multiple Virtual Machines

Run different configurations simultaneously:

```bash
# Terminal 1: Single core, minimal
./scripts/qemu-run.sh --cores 1 --memory 256M

# Terminal 2: 4-core, production config
./scripts/qemu-run.sh --cores 4 --memory 2G --debug

# Terminal 3: Run tests
./scripts/run-tests.sh
```

### Custom Kernel

Build and test different kernel configurations:

```bash
# Build variant
make -f arch/riscv/Makefile.riscv riscv-build CONFIG_RISCV_VECTOR=y

# Test variant
./scripts/qemu-run.sh --kernel /path/to/custom/kernel
```

## Support & Documentation

- **README.md** - Overview and structure
- **QUICKREF.md** - Common commands
- **TESTING.md** - Test procedures
- **../RISC-V_ARCHITECTURE.md** - ISA details
- **../RISC-V_INTEGRATION_GUIDE.md** - Development guide

## Summary

Your virtual environment is **ready to test the Mobile OS**! 

🎉 You now have:
- ✓ Complete RISC-V QEMU simulator
- ✓ Configurable hardware (cores, memory, debug)
- ✓ Device tree with mobile devices
- ✓ Kernel builder and launcher scripts
- ✓ Test framework and procedures
- ✓ GDB debugging support
- ✓ Comprehensive documentation

**Start testing:**
```bash
cd /workspaces/freebsd-src/mobile/virtenv
./scripts/qemu-run.sh
```

Press `Ctrl+A` then `X` to exit QEMU.

Happy testing! 🚀
