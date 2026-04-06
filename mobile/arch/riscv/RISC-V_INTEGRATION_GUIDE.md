# RISC-V Integration Guide for Mobile OS

## Introduction

This guide provides comprehensive instructions for integrating RISC-V processors into the Mobile OS project. It covers everything from initial setup to advanced optimizations.

## Prerequisites

### System Requirements
- Development machine (Linux/macOS)
- RISC-V toolchain: `riscv64-unknown-elf-gcc`
- 4GB+ RAM for builds
- FreeBSD source tree

### Installing RISC-V Toolchain

```bash
# Ubuntu/Debian
sudo apt-get install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf gdb-riscv64-unknown-elf

# macOS (Homebrew)
brew install riscv-tools

# Verify installation
riscv64-unknown-elf-gcc --version
```

## Project Structure

```
mobile/
├── arch/
│   └── riscv/                          # RISC-V architecture code
│       ├── README.md                   # RISC-V overview
│       ├── RISC-V_ARCHITECTURE.md      # Detailed architecture specs
│       ├── Makefile.riscv              # Build configuration
│       │
│       ├── hal/                        # Hardware Abstraction Layer
│       │   ├── riscv_hal.h             # HAL public interface
│       │   └── riscv_hal.c             # HAL implementation
│       │
│       ├── kernel/                     # Kernel-specific code
│       │   ├── entry.S                 # Boot sequence (to be created)
│       │   ├── head.S                  # Linker script (to be created)
│       │   └── mm.c                    # Memory management (to be created)
│       │
│       ├── drivers/                    # Architecture drivers
│       │   ├── riscv_cpu_driver.h      # CPU driver
│       │   ├── plic_driver.c           # Interrupt controller (to be created)
│       │   ├── timer_driver.c          # Timer management (to be created)
│       │   └── clint_driver.c          # Core interrupt controller (to be created)
│       │
│       └── optimizations/              # Performance-critical code
│           ├── riscv_optimizations.h   # Optimization macros
│           ├── vector_ops.c            # Vector operations (with RVV)
│           └── crypto_ops.c            # Crypto operations (with RVK)
```

## Building for RISC-V

### Basic Build

```bash
cd mobile/

# Check toolchain
make -f arch/riscv/Makefile.riscv riscv-check

# Build kernel
make -f arch/riscv/Makefile.riscv riscv-build

# View build information
make -f arch/riscv/Makefile.riscv riscv-info
```

### Configuration Options

Edit `arch/riscv/Makefile.riscv` to customize:

```makefile
# ISA configuration
RISCV_DEFAULT := rv64imac          # Base configuration

# Feature flags
CONFIG_RISCV_VECTOR=y              # Enable vector support
CONFIG_RISCV_CRYPTO=n              # Disable crypto (future)
CONFIG_POWER_MANAGEMENT=y           # Enable DVFS
CONFIG_CACHE_OPTIMIZE=y             # Cache optimization

# Compiler flags
RISCV_CFLAGS_PERF := -O3 -funroll-loops -fvectorize
```

## HAL Integration

The Hardware Abstraction Layer (HAL) provides a unified interface to RISC-V features:

### Using the HAL in Drivers

```c
#include "arch/riscv/hal/riscv_hal.h"

void my_driver_init(void) {
    // Initialize HAL
    riscv_hal_init();
    
    // Check for features
    if (riscv_hal_has_extension("V")) {
        printf("Vector extension available\n");
    }
    
    // Get CPU capabilities
    cpu_features_t* features = riscv_hal_get_cpu_features();
    printf("Core frequency: %u MHz\n", features->max_frequency_mhz);
    
    // Configure interrupts
    riscv_hal_plic_init(PLIC_BASE_ADDRESS);
    riscv_hal_plic_set_priority(TOUCH_INTERRUPT, PLIC_PRIORITY_TOUCH);
    riscv_hal_plic_enable_interrupt(TOUCH_INTERRUPT);
}
```

### Adding New HAL Functions

1. Declare in `hal/riscv_hal.h`
2. Implement in `hal/riscv_hal.c`
3. Use consistent naming: `riscv_hal_<subsystem>_<operation>`

## Driver Development

### Architecture Driver Template

```c
#include "arch/riscv/hal/riscv_hal.h"

typedef struct {
    uint32_t id;
    volatile void* base_addr;
    bool initialized;
} device_t;

static device_t g_device;

int device_driver_init(void) {
    if (g_device.initialized) return 0;
    
    riscv_hal_init();
    g_device.id = riscv_hal_get_hart_id();
    
    // Initialize hardware
    // ...
    
    g_device.initialized = true;
    return 0;
}
```

### Mobile-Specific Driver Considerations

1. **Interrupt Priority**: Use mobile interrupt priorities
   ```c
   riscv_hal_plic_set_priority(IRQ_TOUCH, PLIC_PRIORITY_TOUCH);    // 7 - highest
   riscv_hal_plic_set_priority(IRQ_NETWORK, PLIC_PRIORITY_NETWORK); // 4
   ```

2. **Power Management**: Implement DVFS support
   ```c
   riscv_cpu_set_frequency(hart_id, 1200);  // Reduce to 1.2 GHz for efficiency
   ```

3. **Low-Latency Operations**: Use optimized primitives
   ```c
   riscv_atomic_inc(&touch_counter);  // Lock-free counter
   riscv_do_event(event);              // Fast event processing
   ```

## Performance Optimization

### Using Optimizations Header

```c
#include "arch/riscv/optimizations/riscv_optimizations.h"

void fast_copy(void* dst, void* src, size_t len) {
    riscv_memcpy_fast(dst, src, len);
}

void queue_event(void* event) {
    if (riscv_enqueue_event(&event_queue, event) < 0) {
        // Handle queue full
    }
}
```

### Profiling and Benchmarking

```bash
# Build with profiling enabled
RISC_CFLAGS="-pg" make -f arch/riscv/Makefile.riscv riscv-build

# Analyze performance
gprof vmlinux.riscv64 gmon.out | head -20
```

## Testing

### Unit Tests

Create test files in `arch/riscv/tests/`:

```c
#include "test_framework.h"
#include "arch/riscv/hal/riscv_hal.h"

void test_hal_cpu_features(void) {
    riscv_hal_init();
    cpu_features_t* features = riscv_hal_get_cpu_features();
    
    assert(features != NULL);
    assert(features->has_atomic == true);  // RV64IMAC includes A
    
    printf("✓ CPU features test passed\n");
}
```

### Integration Testing

```bash
# Boot RISC-V kernel in QEMU
qemu-system-riscv64 -machine virt \
  -kernel vmlinux.riscv64 \
  -m 2G \
  -smp 4 \
  -serial stdio

# Run basic tests
test_hal_cpu_features
test_interrupt_handling
test_timer_operations
```

## SoC-Specific Adaptations

### Supporting Multiple RISC-V SoCs

Create SoC-specific configuration files:

```
arch/riscv/socs/
├── generic/
│   ├── config.h
│   └── memory.ld
├── vendor1/
│   ├── config.h        # Vendor-specific settings
│   └── memory.ld       # Memory layout
└── vendor2/
    ├── config.h
    └── memory.ld
```

### Example: Vendor-Specific Interrupt Mapping

```c
// In soc/vendor1/config.h
#define PLIC_BASE_ADDRESS    0x0C000000
#define CLINT_BASE_ADDRESS   0x02000000
#define UART_BASE_ADDRESS    0x10000000

#define TOUCH_INTERRUPT      5
#define TIMER_INTERRUPT      7
#define UART_INTERRUPT       10
```

## Debugging

### Enable Debug Logging

```bash
# Build with debug symbols
RISC_CFLAGS="-g -DDEBUG" make -f arch/riscv/Makefile.riscv riscv-build

# Use GDB with QEMU
qemu-system-riscv64 -machine virt -kernel vmlinux.riscv64 -s -S &
riscv64-unknown-elf-gdb vmlinux.riscv64
(gdb) target remote localhost:1234
(gdb) continue
```

### Hardware Debug Support

RISC-V supports hardware debugging via **Debug Transport Module (DTM)**:

```bash
# OpenOCD debugging
openocd -f interface/ftdi/olimex-arm-usb-ocd.cfg \
        -f target/riscv.cfg

# In another terminal
riscv64-unknown-elf-gdb vmlinux.riscv64
(gdb) target remote localhost:3333
```

## Future Extensions

### Vector Extension (RVV)

```c
// Once RVV support is complete
#ifdef CONFIG_RISCV_VECTOR
#include "arch/riscv/optimizations/vector_ops.h"

void fast_pixel_convert(pixel_t* dst, uint8_t* src, int count) {
    vector_convert_rgb_to_grayscale(dst, src, count);
}
#endif
```

### Cryptography Extension (RVK)

```c
// Future crypto acceleration
#ifdef CONFIG_RISCV_CRYPTO
#include "arch/riscv/optimizations/crypto_ops.h"

void fast_aes_encrypt(uint8_t* plaintext, uint8_t* key, uint8_t* ciphertext) {
    crypto_aes_encrypt_hw(plaintext, key, ciphertext);
}
#endif
```

## References

- [RISC-V Specification](https://riscv.org/specifications/)
- [RISC-V Privileged ISA](https://riscv.org/technical/specifications/)
- [FreeBSD RISC-V Port](https://wiki.freebsd.org/riscv)
- [Linux RISC-V Port](https://github.com/riscv/riscv-linux)

## Troubleshooting

### Toolchain Not Found

```bash
# Check PATH
echo $PATH

# Update PATH if needed
export PATH=$PATH:/opt/riscv/bin
```

### Build Errors

```
error: ISA extension 'V' was not in baseline ISA
```

Solution: Check `Makefile.riscv` - ensure selected ISA includes required extensions.

### QEMU Simulation Issues

```bash
# Verify QEMU RISC-V support
qemu-system-riscv64 --version

# Use verbose output for debugging
qemu-system-riscv64 -d help  # See available debug options
```

## Contributing RISC-V Improvements

1. Test changes on multiple RISC-V platforms
2. Document ISA requirements (RV32/RV64, extensions)
3. Ensure compatibility with HAL abstraction
4. Add performance benchmarks
5. Submit PR with detailed description

---

For questions or issues, please open a GitHub issue or contact the Mobile OS development team.
