/*
 * RISC-V Hardware Abstraction Layer (HAL)
 * Mobile OS Project
 * 
 * Provides unified interface to RISC-V hardware features
 * Abstracts CPU differences and enables portable driver code
 */

#ifndef _RISCV_HAL_H_
#define _RISCV_HAL_H_

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * CPU Feature Flags
 * ============================================================================ */

typedef struct {
    uint64_t base_isa;           /* Base ISA: 32/64 */
    bool has_mul_div;            /* M extension */
    bool has_atomic;             /* A extension */
    bool has_float;              /* F extension */
    bool has_double;             /* D extension */
    bool has_compressed;         /* C extension */
    bool has_vector;             /* V extension */
    bool has_crypto;             /* K extension */
    bool has_bitmanip;           /* B extension */
    
    uint32_t mvendorid;          /* Manufacturer ID */
    uint32_t marchid;            /* Architecture ID */
    uint32_t mimpid;             /* Implementation ID */
    
    uint32_t max_frequency_mhz;  /* Peak frequency */
    uint32_t l1_cache_size;      /* L1 cache in KB */
    uint32_t l2_cache_size;      /* L2 cache in KB */
} cpu_features_t;

/* ============================================================================
 * Privilege Modes
 * ============================================================================ */

typedef enum {
    PRIV_USER       = 0x0,       /* User mode (U) */
    PRIV_SUPERVISOR = 0x1,       /* Supervisor mode (S) */
    PRIV_HYPERVISOR = 0x2,       /* Hypervisor mode (H) - future */
    PRIV_MACHINE    = 0x3        /* Machine mode (M) */
} privilege_mode_t;

/* ============================================================================
 * Exception and Interrupt Codes
 * ============================================================================ */

typedef enum {
    /* Exceptions */
    EXC_MISALIGNED_FETCH        = 0,
    EXC_FETCH_ACCESS            = 1,
    EXC_ILLEGAL_INSTRUCTION     = 2,
    EXC_BREAKPOINT              = 3,
    EXC_MISALIGNED_LOAD         = 4,
    EXC_LOAD_ACCESS             = 5,
    EXC_MISALIGNED_STORE        = 6,
    EXC_STORE_ACCESS            = 7,
    EXC_UMODE_ECALL             = 8,
    EXC_SMODE_ECALL             = 9,
    EXC_HMODE_ECALL             = 10,
    EXC_MMODE_ECALL             = 11,
    
    /* Interrupts (add 0x8000000000000000 for interrupt bit) */
    INT_USOFT                   = 0,
    INT_SSOFT                   = 1,
    INT_HSOFT                   = 2,
    INT_MSOFT                   = 3,
    INT_UTIMER                  = 4,
    INT_STIMER                  = 5,
    INT_HTIMER                  = 6,
    INT_MTIMER                  = 7,
    INT_UEXT                    = 8,
    INT_SEXT                    = 9,
    INT_HEXT                    = 10,
    INT_MEXT                    = 11
} exception_code_t;

/* ============================================================================
 * Interrupt Controller (PLIC)
 * ============================================================================ */

typedef struct {
    uint32_t source_id;          /* Interrupt source 1-1024 */
    uint32_t priority;           /* Priority 0-7 (0=disabled) */
    bool is_external;            /* External vs internal */
} plic_source_t;

/* Mobile-specific interrupt priorities */
typedef enum {
    PLIC_PRIORITY_TOUCH         = 7,  /* Touchscreen - highest priority */
    PLIC_PRIORITY_SENSORS       = 6,  /* IMU, accelerometer */
    PLIC_PRIORITY_GPU           = 5,  /* GPU/display */
    PLIC_PRIORITY_NETWORK       = 4,  /* Ethernet/WiFi */
    PLIC_PRIORITY_AUDIO         = 3,  /* Audio codec */
    PLIC_PRIORITY_NORMAL        = 2,  /* General I/O */
    PLIC_PRIORITY_BACKGROUND    = 1   /* Background tasks */
} mobile_interrupt_priority_t;

/* ============================================================================
 * Timer Support
 * ============================================================================ */

typedef struct {
    uint64_t mtime;              /* Current time */
    uint64_t mtimecmp;           /* Timer compare value */
    uint64_t freq_hz;            /* Timer frequency */
} timer_t;

/* ============================================================================
 * Virtual Memory Management
 * ============================================================================ */

typedef enum {
    VM_MODE_BARE   = 0,          /* No virtual memory */
    VM_MODE_SV32   = 1,          /* 32-bit virtual addressing */
    VM_MODE_SV39   = 8,          /* 39-bit virtual addressing (mobile) */
    VM_MODE_SV48   = 9,          /* 48-bit virtual addressing (future) */
    VM_MODE_SV57   = 10          /* 57-bit virtual addressing (future) */
} vm_mode_t;

typedef struct {
    uint64_t ppn;                /* Physical page number */
    bool valid;                  /* Valid bit */
    bool readable;               /* Read permission */
    bool writable;               /* Write permission */
    bool executable;             /* Execute permission */
    bool user;                   /* User-accessible */
    bool global;                 /* Global mapping */
    bool accessed;               /* Accessed flag */
    bool dirty;                  /* Dirty flag */
} pte_t;

/* ============================================================================
 * Performance Monitoring Unit (PMU)
 * ============================================================================ */

typedef enum {
    PMU_EVENT_CYCLES            = 0,
    PMU_EVENT_INSTRUCTIONS      = 2,
    PMU_EVENT_L1_MISS           = 3,
    PMU_EVENT_L2_MISS           = 4,
    PMU_EVENT_MEMORY_STALL      = 5,
    PMU_EVENT_BRANCH_MISS       = 6,
    PMU_EVENT_LOAD_LATENCY      = 7
} pmu_event_t;

/* ============================================================================
 * Atomic Operations (A Extension)
 * ============================================================================ */

typedef enum {
    ATOMIC_ADD                  = 0,
    ATOMIC_SWAP                 = 1,
    ATOMIC_COMPARE_SWAP         = 2,
    ATOMIC_AND                  = 3,
    ATOMIC_OR                   = 4,
    ATOMIC_XOR                  = 5,
    ATOMIC_MAX                  = 6,
    ATOMIC_MIN                  = 7
} atomic_op_t;

/* ============================================================================
 * Vector Extension (RVV) - Optional
 * ============================================================================ */

typedef struct {
    uint32_t vlen;               /* Vector length in bits */
    uint32_t elen;               /* Max element width */
    uint32_t selen;              /* Max sub-element width */
    bool fractional_lmul;        /* Fractional LMUL support */
} vector_config_t;

/* ============================================================================
 * HAL Function Declarations
 * ============================================================================ */

/* CPU Detection and Feature Discovery */
void riscv_hal_init(void);
cpu_features_t* riscv_hal_get_cpu_features(void);
bool riscv_hal_has_extension(const char* ext);
uint32_t riscv_hal_get_hart_id(void);

/* Privilege Mode Management */
privilege_mode_t riscv_hal_get_privilege_mode(void);
void riscv_hal_set_privilege_mode(privilege_mode_t mode);

/* Exception and Interrupt Handling */
void riscv_hal_install_trap_handler(void (*handler)(uint64_t mcause, uint64_t mepc));
void riscv_hal_enable_interrupts(void);
void riscv_hal_disable_interrupts(void);
bool riscv_hal_interrupts_enabled(void);

/* PLIC Operations */
void riscv_hal_plic_init(uint64_t plic_base);
void riscv_hal_plic_enable_interrupt(uint32_t source_id);
void riscv_hal_plic_disable_interrupt(uint32_t source_id);
void riscv_hal_plic_set_priority(uint32_t source_id, uint32_t priority);
uint32_t riscv_hal_plic_get_interrupt(void);
void riscv_hal_plic_claim(uint32_t interrupt_id);

/* Timer Operations */
void riscv_hal_timer_init(uint64_t freq_hz);
uint64_t riscv_hal_timer_get_mtime(void);
void riscv_hal_timer_set_mtimecmp(uint64_t value);

/* Memory Management */
void riscv_hal_vm_init(vm_mode_t mode);
void riscv_hal_vm_set_satp(uint64_t satp_value);
uint64_t riscv_hal_vm_get_satp(void);
void riscv_hal_vm_flush_tlb(void);
void riscv_hal_vm_sfence_vma(uint64_t vaddr, uint64_t asid);

/* Performance Monitoring */
void riscv_hal_pmu_init(void);
void riscv_hal_pmu_start_counter(uint32_t counter_id, pmu_event_t event);
uint64_t riscv_hal_pmu_read_counter(uint32_t counter_id);

/* Atomic Operations */
int64_t riscv_hal_atomic_add(volatile int64_t* addr, int64_t value);
int64_t riscv_hal_atomic_swap(volatile int64_t* addr, int64_t value);
bool riscv_hal_atomic_cmp_swap(volatile int64_t* addr, int64_t expect, int64_t new);

/* Vector Operations (if available) */
vector_config_t* riscv_hal_vector_get_config(void);
void riscv_hal_vector_set_length(uint32_t vl);

/* Utility Functions */
void riscv_hal_wfi(void);                       /* Wait for interrupt */
void riscv_hal_fence(void);                     /* Memory fence */
void riscv_hal_fence_tso(void);                 /* TSO fence */

#endif /* _RISCV_HAL_H_ */
