# Virtual Environment Testing Guide

## Overview

This guide covers testing the Mobile OS in the QEMU RISC-V virtual environment. The virtual environment simulates a mobile device with:

- RISC-V RV64IMAC CPU (1-8 cores configurable)
- PLIC (Platform Level Interrupt Controller)
- CLINT (Core Local Interruptor) for timers
- UART serial console
- Virtio block and network devices
- Simulated touch input
- Sensor support

## Quick Start

### 1. Setup Environment

```bash
cd /workspaces/freebsd-src/mobile/virtenv
./scripts/setup-env.sh
```

### 2. Build Kernel

```bash
./scripts/build-kernel.sh
```

### 3. Run Virtual Machine

```bash
# Simple boot
./scripts/qemu-run.sh

# With 4 cores and 2GB RAM
./scripts/qemu-run.sh --cores 4 --memory 2G

# With debugging enabled
./scripts/qemu-run.sh --debug

# With GDB stub for kernel debugging
./scripts/qemu-run.sh --gdb
```

## Testing Categories

### 1. Boot Tests

#### 1.1 Basic Boot

```bash
./scripts/qemu-run.sh --cores 1 --memory 256M
```

**Expected Output:**
- QEMU starts successfully
- OpenSBI bootloader messages appear
- Kernel begins initialization
- UART console responds

**Pass Criteria:**
- Boot completes without hangs
- Console is responsive

#### 1.2 Multi-Core Boot

```bash
./scripts/qemu-run.sh --cores 4 --memory 1G
```

**Expected Output:**
- All 4 CPU cores initialized
- Each core reports online status
- Load balancing available

**Pass Criteria:**
- All cores appear in boot log
- No deadlocks or stalls

#### 1.3 Memory Configuration

```bash
# Test with different memory sizes
./scripts/qemu-run.sh --memory 256M
./scripts/qemu-run.sh --memory 512M
./scripts/qemu-run.sh --memory 1G
./scripts/qemu-run.sh --memory 2G
```

**Expected Output:**
- Kernel detects correct memory size
- Memory layout printed during boot

**Pass Criteria:**
- Memory size matches configuration
- No corruption or overlap errors

### 2. Hardware Functionality Tests

#### 2.1 UART Console

```bash
./scripts/qemu-run.sh
# At QEMU prompt: help
# At QEMU prompt: info registers
```

**Test Points:**
- Console input/output working
- UART interrupt handling
- Baud rate correct (115200)

#### 2.2 Timer and Interrupts

```bash
./scripts/qemu-run.sh --debug
# Monitor timer interrupts in debug output
```

**Watch for:**
- Regular timer tick messages
- CLINT counter incrementing
- No lost interrupts

#### 2.3 PLIC (Interrupt Controller)

```bash
./scripts/qemu-run.sh --debug
# Look for PLIC initialization messages
# Verify interrupt routing
```

#### 2.4 Virtual Devices

```bash
./scripts/qemu-run.sh
# Monitor boot for Virtio device detection
```

**Expected:**
- Virtio block device initialized
- Virtio network device recognized
- Device capabilities logged

### 3. Performance Tests

#### 3.1 Boot Time Measurement

```bash
time ./scripts/qemu-run.sh --cores 2 --memory 512M
```

**Baseline Goals:**
- Single core: < 3 seconds
- Multi-core: < 5 seconds

#### 3.2 CPU Frequency

```bash
./scripts/qemu-run.sh --debug
# Monitor clock frequency in logs
```

**Verify:**
- Base frequency: 1.8 GHz (configurable)
- Frequency scaling working (if DVFS enabled)

#### 3.3 Interrupt Latency

```bash
./scripts/qemu-run.sh --debug
# Monitor interrupt handling times
# Look for latency in debug output
```

**Goals:**
- < 100 microseconds typical
- < 1 millisecond worst case

### 4. Architecture Compatibility Tests

#### 4.1 RV64IMAC ISA

Test that all required ISA extensions work:

```bash
./scripts/qemu-run.sh --debug
# Verify in boot log: ISA=RV64IMAC
```

**Extensions to verify:**
- **I**: Integer base operations
- **M**: Multiply/divide operations
- **A**: Atomic operations (for synchronization)
- **C**: Compressed instructions

#### 4.2 Atomic Operations

Create test program:

```c
#include <stdint.h>

int main() {
    volatile int64_t x = 10;
    
    // Atomic add
    asm volatile ("amoadd.d %0, %2, (%1)"
                  : "=r" (x)
                  : "r" (&x), "r" (5)
                  : "memory");
    
    return x == 15 ? 0 : 1;
}
```

Expected: Program returns 0 (success)

#### 4.3 Compressed Instructions

```bash
# Build with compressed instruction extension
riscv64-unknown-elf-gcc -march=rv64imac -O3 -c test.c -o test.o

# Check instruction mix (should be ~30% compressed)
riscv64-unknown-elf-objdump -d test.o | grep -c "c\." | \
  awk '{printf "Compressed: %.1f%%\n", $1 * 100 / NR}'
```

### 5. Debugger Integration Tests

#### 5.1 QEMU GDB Stub

```bash
# Terminal 1: Start QEMU with GDB stub
./scripts/qemu-run.sh --gdb

# Terminal 2: Connect with GDB
riscv64-unknown-elf-gdb vmlinux
(gdb) target remote localhost:1234
(gdb) info registers
(gdb) stepi
(gdb) continue
```

**Test Points:**
- Breakpoints work
- Step execution works
- Register inspection works
- Memory inspection works

#### 5.2 Kernel Debugging

```bash
# With debug symbols
./scripts/build-kernel.sh
./scripts/qemu-run.sh --gdb

# In GDB:
(gdb) target remote localhost:1234
(gdb) file vmlinux.riscv64
(gdb) break _start
(gdb) continue
(gdb) next
```

### 6. Issue Diagnosis

#### Boot Hangs

If QEMU boots but hangs:

```bash
# Enable detailed debugging
./scripts/qemu-run.sh --debug

# Check debug log
tail -100 qemu-debug.log

# Check for:
# - Infinite loops in boot code
# - Dealock in spinlocks
# - Missing device initialization
```

#### Kernel Panic

If kernel panics:

```bash
# Check panic message in console output
# Note the instruction pointer (PC)
# Use GDB to inspect that location:

riscv64-unknown-elf-gdb vmlinux
(gdb) info line *0x<PC>
(gdb) list
```

#### Device Not Found

If devices aren't detected:

```bash
# Verify device tree
cd virtenv/devicetree
dtc -I dts -O dts riscv-virt-mobile.dts | less

# Check QEMU device support
qemu-system-riscv64 -M virt -device ?
```

## Test Automation

### Run Full Test Suite

```bash
./scripts/run-tests.sh
```

Choose option "5" to run all tests.

### Continuous Integration

Create `.github/workflows/test.yml`:

```yaml
name: Virtual Environment Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install dependencies
        run: |
          sudo apt-get install qemu-system-riscv64 gcc-riscv64-unknown-elf
      - name: Setup environment
        run: cd mobile && ./virtenv/scripts/setup-env.sh
      - name: Build kernel
        run: cd mobile && ./virtenv/scripts/build-kernel.sh
      - name: Run tests
        run: cd mobile && ./virtenv/scripts/run-tests.sh
```

## Performance Baseline

Target Performance Metrics:

| Metric | Target | Notes |
|--------|--------|-------|
| Boot time | < 3s | Single core |
| Boot time | < 5s | 4-core |
| App launch | < 500ms | Typical app |
| Frame rate | 60 FPS | Smooth scrolling |
| Memory | 256M minimum | Simulation |
| CPU cores | 1-8 | Configurable |

## Troubleshooting

### "QEMU not found"

```bash
sudo apt-get install qemu-system-riscv64
```

### "Kernel not found"

```bash
cd mobile
./virtenv/scripts/build-kernel.sh
```

### "Device tree not compiled"

```bash
cd mobile/virtenv/devicetree
sudo apt-get install device-tree-compiler
dtc -I dts -O dtb -o riscv-virt-mobile.dtb riscv-virt-mobile.dts
```

### "GDB connection refused"

```bash
# Make sure QEMU was started with --gdb flag
./scripts/qemu-run.sh --gdb

# Check port is available
lsof -i :1234
```

## Next Steps

1. Boot into fully functional OS
2. Test application frameworks
3. Profile performance hotspots
4. Optimize critical paths
5. Extend with additional features

## Resources

- [QEMU RISC-V](https://wiki.qemu.org/Documentation/Platforms/RISCV)
- [Device Tree Specification](https://devicetree-specification.readthedocs.io/)
- [RISC-V ISA Specification](https://riscv.org/specifications/)
- [GDB RISC-V Documentation](https://sourceware.org/gdb/current/onlinedocs/)
