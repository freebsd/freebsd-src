# RISC-V Performance Porting Guide

## Converting ARM Code to RISC-V

This guide helps developers efficiently port and optimize ARM-based code for RISC-V processors in the Mobile OS.

## Table of Contents
1. [Instruction Set Mapping](#instruction-set-mapping)
2. [Register Conventions](#register-conventions)
3. [Function Call Interface](#function-call-interface)
4. [Memory Operations](#memory-operations)
5. [Performance Tips](#performance-tips)
6. [Benchmarking](#benchmarking)

## Instruction Set Mapping

### Common Operations

| Operation | ARM | RISC-V |
|-----------|-----|--------|
| Load register | `ldr r0, [r1]` | `ld x10, 0(x11)` |
| Store register | `str r0, [r1]` | `sd x10, 0(x11)` |
| Add immediate | `add r0, r1, #5` | `addi x10, x11, 5` |
| Load immediate | `mov r0, #0x1000` | `li x10, 0x1000` |
| Branch | `b label` | `jal x0, label` |
| And | `and r0, r1, r2` | `and x10, x11, x12` |
| Compare | `cmp r0, r1` | `sub x10, x10, x11` |
| Bit shift left | `lsl r0, r1, #2` | `slli x10, x11, 2` |

### NEON to RVV Mapping (Vector Extensions)

| ARM NEON | RISC-V RVV | Purpose |
|----------|-----------|---------|
| `vadd.i32` | `vadd.vi` | Vector add immediate |
| `vmul.i32` | `vmul.vv` | Vector multiply |
| `vld1.32` | `vle32.v` | Vector load 32-bit |
| `vst1.32` | `vse32.v` | Vector store 32-bit |
| `vmax.i32` | `vmax.vv` | Vector max |
| `vmin.i32` | `vmin.vv` | Vector min |

## Register Conventions

### ARM vs RISC-V Register File

| Purpose | ARM (ABI) | RISC-V (ABI) | Notes |
|---------|-----------|-------------|-------|
| Return value | r0-r1 | x10-x11 | Preserves value across calls |
| Arguments | r0-r3 | x10-x17 | More in RISC-V (8 vs 4) |
| Temporary | r12, r14 | x5-x7, x28-x31 | Caller's responsibility |
| Saved | r4-r11 | x1, x3-x4, x8-x9, x18-x27 | Callee must preserve |
| Stack pointer | sp (r13) | sp (x2) | Points to top of stack |
| Link register | lr (r14) | ra (x1) | Return address |
| Program counter | pc (r15) | N/A | Implicit in jumps |

## Function Call Interface

### ARM Function Prologue/Epilogue

```asm
// ARM function
my_function:
    push {r4, r5, lr}           @ Save callee-saved registers
    sub sp, sp, #8              @ Allocate local variables
    
    ; ... function body ...
    
    add sp, sp, #8              @ Deallocate local variables
    pop {r4, r5, pc}            @ Restore and return
```

### RISC-V Equivalent

```asm
// RISC-V function
my_function:
    addi sp, sp, -16            # Allocate 16 bytes for locals
    sd x8, 0(sp)                # Save s0 (x8)
    sd x9, 8(sp)                # Save s1 (x9)
    
    ; ... function body ...
    
    ld x8, 0(sp)                # Restore s0
    ld x9, 8(sp)                # Restore s1
    addi sp, sp, 16             # Deallocate
    jalr x0, 0(x1)              # Return (implicit in ra = x1)
```

## Memory Operations

### Aligned Memory Access

**ARM** (can access unaligned):
```c
int* ptr = (int*)0x12345001;    // Unaligned - works on most ARM
int val = *ptr;
```

**RISC-V** (strict alignment for efficiency):
```c
int* ptr = (int*)0x12345000;    // Must be 4-byte aligned
int val = *ptr;
```

**Best Practice**:
```c
// Portable code
#ifdef __riscv
    uint8_t* bytes = (uint8_t*)addr;
    int val = (bytes[0] << 24) | (bytes[1] << 16) | 
              (bytes[2] << 8) | bytes[3];
#else
    int val = *(int*)addr;
#endif
```

### Volatile Access Optimization

**ARM approach**:
```c
volatile uint32_t* reg = (volatile uint32_t*)0x10000000;
*reg = value;
```

**RISC-V optimized**:
```c
volatile uint32_t* reg = (volatile uint32_t*)0x10000000;
uint32_t temp = value;
asm volatile ("sw %0, 0(%1)" : : "r"(temp), "r"(reg) : "memory");
```

## Performance Tips

### 1. Atomic Operations

**ARM**:
```c
// ARM with exclusive load-store
uint32_t atomicAdd(volatile uint32_t* addr, uint32_t val) {
    uint32_t temp;
    asm volatile(
        "ldrex %0, [%1]\n"
        "add %0, %0, %2\n"
        "strex r12, %0, [%1]\n"
        : "=&r" (temp)
        : "r"(addr), "r"(val)
    );
    return temp;
}
```

**RISC-V (A extension)**:
```c
// RISC-V atomic operation - single instruction!
uint64_t atomicAdd(volatile uint64_t* addr, uint64_t val) {
    uint64_t result;
    asm volatile ("amoadd.d %0, %2, (%1)"
                  : "=r" (result)
                  : "r"(addr), "r"(val)
                  : "memory");
    return result;
}
```

**Advantage**: RISC-V atomic operations are single instructions!

### 2. Compressed Instructions (RV64C)

Enable compressed instruction extension for ~30% code size reduction:

```makefile
CFLAGS := -march=rv64imac  # 'c' = compressed
```

Before (without C):
```
00000000 <function>:
   0:   64 47       addi    a4,a4,-4
   2:   e3 7d       beq     a5,a4,...  # 2 bytes
```

After (with C):
```
00000000 <function>:
   0:   7d 47       c.addi  a4,-4
   2:   d1 61       c.beq   a5,a4,...  # 2 bytes each!
```

### 3. Loop Optimization

**Before** (ARM-style):
```c
void copy_array(int* dst, int* src, int count) {
    for (int i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}
```

**After** (RISC-V optimized with unrolling):
```c
void copy_array(int* dst, int* src, int count) {
    int i = 0;
    // Process 4 elements per iteration
    for (; i + 3 < count; i += 4) {
        asm volatile(
            "ld x10, %0\n"
            "ld x11, %1\n"
            "ld x12, %2\n"
            "ld x13, %3\n"
            "sd x10, 0(%4)\n"
            "sd x11, 8(%4)\n"
            "sd x12, 16(%4)\n"
            "sd x13, 24(%4)\n"
            :
            : "m"(src[i]), "m"(src[i+1]), "m"(src[i+2]), "m"(src[i+3]), "r"(&dst[i])
        );
    }
    // Handle remainder
    for (; i < count; i++) {
        dst[i] = src[i];
    }
}
```

### 4. Cache-Line Optimization

```c
#define CACHE_LINE_SIZE 64

// Align structures to cache lines for performance
struct touch_event {
    uint32_t x;
    uint32_t y;
    uint32_t pressure;
    // ... more fields
} __attribute__((aligned(CACHE_LINE_SIZE)));

// Prefetch for sequential access
void prefetch_events(struct touch_event* events, int count) {
    for (int i = 0; i < count; i += CACHE_LINE_SIZE/sizeof(*events)) {
        riscv_prefetch(&events[i]);
    }
}
```

### 5. Branch Prediction

```c
// Hint compiler about branch likelihood
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

// Use sparingly for hot paths
if (LIKELY(event_type == TOUCH_EVENT)) {
    process_touch(event);
} else if (UNLIKELY(event_type == ERROR)) {
    handle_error(event);
}
```

## Benchmarking

### Creating Benchmark Tests

```c
#include <stdint.h>
#include "arch/riscv/hal/riscv_hal.h"

typedef struct {
    const char* name;
    uint64_t cycles;
    uint64_t instructions;
} benchmark_result_t;

benchmark_result_t measure_operation(void (*op)(void)) {
    benchmark_result_t result;
    
    riscv_hal_pmu_init();
    riscv_hal_pmu_start_counter(0, PMU_EVENT_CYCLES);
    riscv_hal_pmu_start_counter(1, PMU_EVENT_INSTRUCTIONS);
    
    // Warm-up
    for (int i = 0; i < 100; i++) op();
    
    // Measurement
    uint64_t start_cycles = riscv_hal_pmu_read_counter(0);
    uint64_t start_instrs = riscv_hal_pmu_read_counter(1);
    
    for (int i = 0; i < 10000; i++) op();
    
    result.cycles = riscv_hal_pmu_read_counter(0) - start_cycles;
    result.instructions = riscv_hal_pmu_read_counter(1) - start_instrs;
    
    return result;
}
```

### Comparing ARM vs RISC-V Performance

```bash
# Build for ARM
arm-linux-gnueabihf-gcc -march=armv7-a -O3 -o arm_bench benchmark.c

# Build for RISC-V
riscv64-unknown-elf-gcc -march=rv64imac -O3 -o riscv_bench benchmark.c

# Run benchmarks
./arm_bench
./riscv_bench  # Or in QEMU simulation
```

### Key Metrics

| Metric | How to Measure |
|--------|----------------|
| **Cycles per operation** | `PMU_EVENT_CYCLES / iterations` |
| **Instructions count** | `PMU_EVENT_INSTRUCTIONS / iterations` |
| **CPI (Cycles per Instruction)** | `cycles / instructions` |
| **Code size** | `size binary` |
| **Power consumption** | Measure voltage/current with multimeter |

## Migration Checklist

- [ ] Identify ARM-specific code sections
- [ ] Check for unaligned memory access
- [ ] Verify register calling conventions
- [ ] Test atomic operations with A extension
- [ ] Enable compressed instructions (C extension)
- [ ] Benchmark critical paths
- [ ] Profile with PMU counters
- [ ] Document performance improvements
- [ ] Write unit tests for compatibility

## Resources

- [RISC-V ABI Specification](https://github.com/riscv-non-profit/riscv-elf-psabi-doc)
- [ARM ABI Specification](https://github.com/ARM-software/abi-aa)
- [RISC-V Optimization Guide](https://arxiv.org/abs/1911.08966)
- [FreeBSD RISC-V Performance Notes](https://wiki.freebsd.org/riscv)
