# Quick Reference - Virtual Environment

## Overview

The virtual environment simulates a mobile device with RISC-V processor using QEMU.

## Quick Commands

```bash
# Setup
cd /workspaces/freebsd-src/mobile/virtenv
./scripts/setup-env.sh              # One-time setup

# Build
./scripts/build-kernel.sh           # Build kernel

# Run
./scripts/qemu-run.sh               # Default (1 core, 512MB RAM)
./scripts/qemu-run.sh --cores 4 --memory 2G  # Custom config
./scripts/qemu-run.sh --gdb         # With debugger

# Test
./scripts/run-tests.sh              # Interactive test suite
```

## Configuration Guide

### Cores
```bash
./scripts/qemu-run.sh --cores 1     # Single core (fastest boot)
./scripts/qemu-run.sh --cores 2     # Dual core
./scripts/qemu-run.sh --cores 4     # Quad core (typical mobile)
./scripts/qemu-run.sh --cores 8     # Octa core (high-end)
```

### Memory
```bash
./scripts/qemu-run.sh --memory 256M   # Minimum (low-end device)
./scripts/qemu-run.sh --memory 512M   # Standard
./scripts/qemu-run.sh --memory 1G     # High-end
./scripts/qemu-run.sh --memory 2G     # Top-tier
```

### Debug Modes
```bash
./scripts/qemu-run.sh --debug       # Debug output to file
./scripts/qemu-run.sh --gdb         # Kernel debugger support
./scripts/qemu-run.sh --kernel X    # Custom kernel binary
./scripts/qemu-run.sh --dtb Y       # Custom device tree
```

## Files Structure

| File | Purpose |
|------|---------|
| `qemu-run.sh` | Main QEMU launcher |
| `setup-env.sh` | Environment setup |
| `build-kernel.sh` | Kernel compilation |
| `run-tests.sh` | Test runner |
| `riscv-virt-mobile.dts` | Device tree (hardware description) |
| `config.h` | Kernel configuration constants |
| `entry.S` | Boot code |

## Device Layout

| Device | Address | Purpose |
|--------|---------|---------|
| DRAM | 0x80000000 | Main memory (512MB) |
| CLINT | 0x02000000 | Timer & IPI |
| PLIC | 0x0c000000 | Interrupt controller |
| UART | 0x10000000 | Serial console |
| Virtio Block | 0x10001000 | Storage |
| Virtio Network | 0x10002000 | Networking |

## Exit QEMU

```
Press: Ctrl+A then X
Or:    quit (in QEMU monitor)
```

## Enable GDB Debugging

**Terminal 1** - Start QEMU with GDB:
```bash
./scripts/qemu-run.sh --gdb
```

**Terminal 2** - Debug kernel:
```bash
riscv64-unknown-elf-gdb vmlinux/vmlinux.riscv64
(gdb) target remote localhost:1234
(gdb) break kernel_init
(gdb) continue
(gdb) next
(gdb) print $pc
```

## Common GDB Commands

```
break <function>        Set breakpoint
continue               Run until breakpoint
step/next              Single step
info registers         Show CPU registers
print <expr>           Print variable
backtrace              Call stack
disassemble <func>     Show assembly
memory <addr> <len>    Memory dump
```

## Performance Metrics

Expected performance on virtual environment:

| Metric | Expected |
|--------|----------|
| Boot time | 2-5 seconds |
| App launch | < 500ms |
| FPS | 60 (simulated) |
| Memory | 256MB+ |

## Troubleshooting

```bash
# Check QEMU version
qemu-system-riscv64 --version

# Check toolchain
riscv64-unknown-elf-gcc --version

# Check kernel built
ls -l kernel/vmlinux.riscv64

# View debug output
tail -f qemu-debug.log

# Test QEMU functionality
qemu-system-riscv64 -machine virt -id
```

## Pro Tips

1. **Fast testing**: Use 1 core and 256M RAM for quick iterations
2. **Debugging**: Build with `-g` flags and use GDB
3. **Performance**: Use 4-8 cores with 1-2GB RAM
4. **Persistent**: Add custom kernel changes before building
5. **Network**: Configure Virtio network for testing

## Next Steps

- Build kernel: `./scripts/build-kernel.sh`
- Run tests: `./scripts/run-tests.sh`
- Debug kernel: `./scripts/qemu-run.sh --gdb`
- Review TESTING.md for full test procedures

See `README.md` for complete documentation.
