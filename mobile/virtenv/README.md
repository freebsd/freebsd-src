# Virtual Environment for Mobile OS Testing

This directory contains everything needed to test the Mobile OS in a virtual environment using QEMU.

## Quick Start

### Prerequisites
```bash
# Install QEMU with RISC-V support
sudo apt-get install qemu-system-riscv64

# Verify installation
qemu-system-riscv64 --version
```

### Running the Virtual Environment

```bash
# Basic test (minimal configuration)
cd virtenv
./scripts/qemu-run.sh

# With more resources (4 cores, 2GB RAM)
./scripts/qemu-run.sh --cores 4 --memory 2G

# With debugging enabled
./scripts/qemu-run.sh --debug

# Custom kernel
./scripts/qemu-run.sh --kernel /path/to/kernel
```

## Virtual Environment Contents

```
virtenv/
├── README.md                           # This file
├── TESTING.md                          # Testing guide and procedures
├── qemu/
│   ├── qemu-virt.conf                 # QEMU machine configuration
│   ├── launch-minimal.sh               # Minimal boot
│   └── launch-full.sh                  # Full-featured boot
│
├── scripts/
│   ├── qemu-run.sh                    # Main launch script
│   ├── setup-env.sh                   # Environment setup
│   ├── build-kernel.sh                # Kernel build script
│   └── run-tests.sh                   # Test runner
│
├── devicetree/
│   ├── riscv-virt-mobile.dts          # Device tree source
│   ├── riscv-virt-mobile.dtb          # Device tree binary (compiled)
│   └── bindings/                      # Custom device bindings
│
├── kernel/
│   ├── entry.S                        # Bootloader entry point
│   ├── head.S                         # Boot code and linker script
│   ├── config.h                       # Kernel configuration
│   └── boot.mk                        # Boot build rules
│
└── rootfs/
    ├── init                           # Init script
    ├── bin/                           # Binary executables
    ├── lib/                           # Libraries
    └── etc/                           # Configuration files
```

## Supported Virtual Machines

### 1. QEMU virt (Default)
- **Machine**: qemu virt board
- **CPU**: SiFive U54/U74 compatible
- **RAM**: 0.5GB - 16GB (configurable)
- **Cores**: 1-8 (configurable)
- **Devices**: UART, Timer, Interrupt Controller (PLIC)

### 2. OpenSBI-based Boot
- **Bootloader**: OpenSBI (SBI interface)
- **Mode**: Supervisor mode execution
- **Features**: Full privileged mode support

## Architecture

```
[QEMU virt board]
    ↓
[OpenSBI - M-mode firmware]
    ↓
[FreeBSD RISC-V Kernel - S-mode]
    ↓
[Mobile OS Services]
    ↓
[Applications]
```

## Key Configuration Files

### qemu-virt.conf
Defines QEMU machine configuration:
- CPU model and frequency
- Memory layout
- Device connectivity
- Interrupt routing

### riscv-virt-mobile.dts
Device tree describing hardware:
- Memory map
- CPU configuration
- Interrupt controller (PLIC)
- Timer configurations
- Custom mobile devices

## Building for Virtual Environment

### Build kernel for QEMU
```bash
cd mobile/
make -f arch/riscv/Makefile.riscv riscv-build QEMU=1
```

### Build rootfs
```bash
./virtenv/scripts/build-rootfs.sh
```

### Build complete system
```bash
./virtenv/scripts/build-all.sh
```

## Testing Capabilities

### Boot Testing
- [x] QEMU startup
- [x] Bootloader initialization
- [x] Kernel boot
- [ ] Full OS initialization

### Hardware Simulation
- [x] UART console
- [x] System timer
- [x] Interrupt controller (PLIC)
- [ ] Storage devices
- [ ] Network interfaces

### Performance Analysis
- [ ] Boot time measurement
- [ ] CPU frequency scaling
- [ ] Memory usage profiling
- [ ] Interrupt latency

## Port Mappings

```
Console:  localhost:12345  (telnet for interactive debugging)
GDB:      localhost:1234   (for kernel debugging)
Monitor:  localhost:55555  (QEMU monitor)
```

## Troubleshooting

### QEMU Not Found
```bash
sudo apt-get install qemu-system-riscv64
```

### Boot Hangs
- Check device tree configuration
- Verify kernel entry point
- Enable debug output: `--debug` flag

### Kernel Panic
- Review kernel output
- Check memory configuration
- Validate device bindings

## Development Workflow

1. **Make changes** to kernel or firmware
2. **Build**: `./scripts/build-all.sh`
3. **Test**: `./scripts/qemu-run.sh --debug`
4. **Debug**: Attach GDB: `riscv64-unknown-elf-gdb vmlinux`
5. **Benchmark**: `./scripts/run-tests.sh`

## Next Steps

1. See [TESTING.md](TESTING.md) for detailed testing procedures
2. Review [../RISC-V_INTEGRATION_GUIDE.md](../RISC-V_INTEGRATION_GUIDE.md) for development
3. Check [../RISC-V_PERFORMANCE_PORTING.md](../RISC-V_PERFORMANCE_PORTING.md) for optimization

## Resources

- [QEMU RISC-V Documentation](https://wiki.qemu.org/Documentation/Platforms/RISCV)
- [Device Tree Specification](https://devicetree-specification.readthedocs.io/)
- [FreeBSD RISC-V Wiki](https://wiki.freebsd.org/riscv)
- [OpenSBI Project](https://github.com/riscv-software-foundation/opensbi)

## Support

For issues or questions:
1. Check TESTING.md troubleshooting section
2. Review QEMU debug output
3. consult kernel logs
4. Open issue on project repository
