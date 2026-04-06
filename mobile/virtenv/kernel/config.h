/*
 * Virtual Environment Kernel Configuration
 * Mobile OS - QEMU RISC-V
 */

#ifndef __VIRT_CONFIG_H__
#define __VIRT_CONFIG_H__

/* ============================================================================
 * Build Configuration
 * ============================================================================ */

#define CONFIG_QEMU 1                   /* Target QEMU virtual machine */
#define CONFIG_ARCH_RISCV64 1           /* 64-bit RISC-V */
#define CONFIG_ISA_RV64IMAC 1           /* RV64IMAC instruction set */

/* ============================================================================
 * Memory Configuration
 * ============================================================================ */

#define KERNEL_BASE         0x80000000  /* Physical kernel start */
#define KERNEL_VIRT_BASE    0xfffffffff0000000  /* Virtual kernel base (Sv39) */
#define KERNEL_SIZE         0x1000000   /* 16MB kernel space */
#define PAGE_SIZE           4096        /* 4K pages */
#define VM_MODE             VM_MODE_SV39  /* 39-bit virtual addressing */

/* Memory areas */
#define DRAM_BASE           0x80000000  /* DRAM starts at 0x80000000 */
#define DRAM_SIZE           0x20000000  /* 512MB by default */

/* ============================================================================
 * Device Configuration
 * ============================================================================ */

/* QEMU virt machine addresses */
#define PLIC_BASE           0x0c000000  /* Platform Level Interrupt Controller */
#define CLINT_BASE          0x02000000  /* Core Local Interruptor */
#define UART_BASE           0x10000000  /* UART0 */
#define UART0_IRQ           10          /* UART interrupt ID */

/* Timer configuration */
#define TIMER_FREQ          10000000    /* 10 MHz */
#define TIMER_TICK_MS       10          /* 10ms tick */

/* ============================================================================
 * Interrupt Configuration
 * ============================================================================ */

#define MAX_IRQS            1024        /* Maximum interrupt sources */
#define UART_IRQ            10
#define TOUCH_IRQ           5
#define SENSOR_IRQ          6
#define GPU_IRQ             7
#define NETWORK_IRQ         1

/* Mobile interrupt priorities */
#define PLIC_PRIORITY_TOUCH     7       /* Highest - touch input */
#define PLIC_PRIORITY_SENSORS   6       /* Real-time sensors */
#define PLIC_PRIORITY_GPU       5       /* GPU operations */
#define PLIC_PRIORITY_NETWORK   4       /* Network I/O */
#define PLIC_PRIORITY_AUDIO     3       /* Audio processing */
#define PLIC_PRIORITY_NORMAL    2       /* General I/O */
#define PLIC_PRIORITY_BG        1       /* Background tasks */

/* ============================================================================
 * Debug Configuration
 * ============================================================================ */

#define CONFIG_DEBUG        1           /* Enable debug output */
#define CONFIG_SERIAL_DEBUG 1           /* Serial console debug */
#define SERIAL_BAUD         115200      /* Serial baud rate */

/* ============================================================================
 * Feature Configuration
 * ============================================================================ */

#define CONFIG_SMP              1       /* Symmetric Multi-Processing */
#define CONFIG_MAX_CPUS         8       /* Maximum 8 CPUs */
#define CONFIG_DYNAMIC_FREQ     1       /* DVFS support */
#define CONFIG_POWER_MANAGEMENT 1       /* Power states */
#define CONFIG_CACHE_OPTIMIZE   1       /* Cache optimizations */

/* ISA Features */
#define CONFIG_ISA_MULITPLY     1       /* M extension */
#define CONFIG_ISA_ATOMIC       1       /* A extension */
#define CONFIG_ISA_COMPRESSED   1       /* C extension */
#define CONFIG_ISA_VECTOR       0       /* V extension (disabled for virt) */

/* ============================================================================
 * Virtual Device Support
 * ============================================================================ */

#define CONFIG_VIRTIO_BLOCK     1       /* Virtio block device */
#define CONFIG_VIRTIO_NET       1       /* Virtio network */

/* ============================================================================
 * Compilation Flags
 * ============================================================================ */

#define CFLAGS_BASE     "-march=rv64imac -mtune=generic"
#define CFLAGS_OPT      "-O3 -fomit-frame-pointer"
#define CFLAGS_DEBUG    "-g -DDEBUG -DCONFIG_DEBUG=1"

#endif /* __VIRT_CONFIG_H__ */
