/*
 * RISC-V HAL Implementation
 * Mobile OS Project
 * 
 * Core HAL functions for RISC-V hardware control
 */

#include "riscv_hal.h"
#include <string.h>

/* Global CPU features cache */
static cpu_features_t g_cpu_features;
static bool g_hal_initialized = false;

/* CSR Read/Write macros */
#define READ_CSR(csr) ({                                    \
    uint64_t __tmp;                                         \
    asm volatile ("csrr %0, " #csr : "=r" (__tmp));        \
    __tmp;                                                  \
})

#define WRITE_CSR(csr, val) ({                              \
    uint64_t __tmp = (uint64_t)(val);                       \
    asm volatile ("csrw " #csr ", %0" : : "r" (__tmp));    \
})

#define SET_CSR(csr, val) ({                                \
    uint64_t __tmp = (uint64_t)(val);                       \
    asm volatile ("csrs " #csr ", %0" : : "r" (__tmp));    \
})

#define CLEAR_CSR(csr, val) ({                              \
    uint64_t __tmp = (uint64_t)(val);                       \
    asm volatile ("csrc " #csr ", %0" : : "r" (__tmp));    \
})

/* ============================================================================
 * CPU Detection
 * ============================================================================ */

void riscv_hal_init(void) {
    if (g_hal_initialized) return;
    
    /* Read machine information registers */
    uint64_t misa = READ_CSR(misa);
    
    /* Parse ISA extensions from misa */
    g_cpu_features.base_isa = (misa >> 62) & 0x3;  /* MXL field */
    g_cpu_features.has_mul_div = (misa & (1UL << ('M' - 'A'))) != 0;
    g_cpu_features.has_atomic = (misa & (1UL << ('A' - 'A'))) != 0;
    g_cpu_features.has_float = (misa & (1UL << ('F' - 'A'))) != 0;
    g_cpu_features.has_double = (misa & (1UL << ('D' - 'A'))) != 0;
    g_cpu_features.has_compressed = (misa & (1UL << ('C' - 'A'))) != 0;
    g_cpu_features.has_vector = (misa & (1UL << ('V' - 'A'))) != 0;
    g_cpu_features.has_crypto = (misa & (1UL << ('K' - 'A'))) != 0;
    g_cpu_features.has_bitmanip = (misa & (1UL << ('B' - 'A'))) != 0;
    
    /* Read vendor information */
    g_cpu_features.mvendorid = (uint32_t)READ_CSR(mvendorid);
    g_cpu_features.marchid = (uint32_t)READ_CSR(marchid);
    g_cpu_features.mimpid = (uint32_t)READ_CSR(mimpid);
    
    g_hal_initialized = true;
}

cpu_features_t* riscv_hal_get_cpu_features(void) {
    if (!g_hal_initialized) {
        riscv_hal_init();
    }
    return &g_cpu_features;
}

bool riscv_hal_has_extension(const char* ext) {
    if (!g_hal_initialized) {
        riscv_hal_init();
    }
    
    if (ext == NULL) return false;
    
    switch (ext[0]) {
        case 'M': return g_cpu_features.has_mul_div;
        case 'A': return g_cpu_features.has_atomic;
        case 'F': return g_cpu_features.has_float;
        case 'D': return g_cpu_features.has_double;
        case 'C': return g_cpu_features.has_compressed;
        case 'V': return g_cpu_features.has_vector;
        case 'K': return g_cpu_features.has_crypto;
        case 'B': return g_cpu_features.has_bitmanip;
        default: return false;
    }
}

uint32_t riscv_hal_get_hart_id(void) {
    return (uint32_t)READ_CSR(mhartid);
}

/* ============================================================================
 * Privilege Mode Management
 * ============================================================================ */

privilege_mode_t riscv_hal_get_privilege_mode(void) {
    uint64_t mstatus = READ_CSR(mstatus);
    return (privilege_mode_t)((mstatus >> 11) & 0x3);  /* MPP field */
}

void riscv_hal_set_privilege_mode(privilege_mode_t mode) {
    uint64_t mstatus = READ_CSR(mstatus);
    mstatus &= ~(0x3UL << 11);  /* Clear MPP */
    mstatus |= ((uint64_t)mode << 11);
    WRITE_CSR(mstatus, mstatus);
}

/* ============================================================================
 * Exception and Interrupt Handling
 * ============================================================================ */

static void (*g_trap_handler)(uint64_t, uint64_t) = NULL;

void riscv_hal_install_trap_handler(void (*handler)(uint64_t mcause, uint64_t mepc)) {
    g_trap_handler = handler;
    /* mtvec should point to the trap handler in real implementation */
}

void riscv_hal_enable_interrupts(void) {
    SET_CSR(mstatus, 0x8);  /* MIE bit */
}

void riscv_hal_disable_interrupts(void) {
    CLEAR_CSR(mstatus, 0x8);  /* MIE bit */
}

bool riscv_hal_interrupts_enabled(void) {
    uint64_t mstatus = READ_CSR(mstatus);
    return (mstatus & 0x8) != 0;
}

/* ============================================================================
 * PLIC Operations (Simplified)
 * ============================================================================ */

static uint64_t g_plic_base = 0;

void riscv_hal_plic_init(uint64_t plic_base) {
    g_plic_base = plic_base;
}

void riscv_hal_plic_enable_interrupt(uint32_t source_id) {
    if (g_plic_base == 0) return;
    
    uint64_t enable_reg = g_plic_base + 0x2000;  /* Enable register base */
    uint32_t hartid = riscv_hal_get_hart_id();
    uint64_t addr = enable_reg + (hartid * 0x100) + ((source_id / 32) * 4);
    
    volatile uint32_t* ptr = (volatile uint32_t*)addr;
    *ptr |= (1U << (source_id % 32));
}

void riscv_hal_plic_disable_interrupt(uint32_t source_id) {
    if (g_plic_base == 0) return;
    
    uint64_t enable_reg = g_plic_base + 0x2000;
    uint32_t hartid = riscv_hal_get_hart_id();
    uint64_t addr = enable_reg + (hartid * 0x100) + ((source_id / 32) * 4);
    
    volatile uint32_t* ptr = (volatile uint32_t*)addr;
    *ptr &= ~(1U << (source_id % 32));
}

void riscv_hal_plic_set_priority(uint32_t source_id, uint32_t priority) {
    if (g_plic_base == 0) return;
    
    uint64_t priority_addr = g_plic_base + (source_id * 4);
    volatile uint32_t* ptr = (volatile uint32_t*)priority_addr;
    *ptr = priority & 0x7;  /* Priority is 3 bits */
}

uint32_t riscv_hal_plic_get_interrupt(void) {
    if (g_plic_base == 0) return 0;
    
    uint32_t hartid = riscv_hal_get_hart_id();
    uint64_t claim_addr = g_plic_base + 0x200000 + (hartid * 0x2000) + 0x4;
    
    volatile uint32_t* ptr = (volatile uint32_t*)claim_addr;
    return *ptr;
}

void riscv_hal_plic_claim(uint32_t interrupt_id) {
    if (g_plic_base == 0) return;
    
    uint32_t hartid = riscv_hal_get_hart_id();
    uint64_t complete_addr = g_plic_base + 0x200000 + (hartid * 0x2000) + 0x4;
    
    volatile uint32_t* ptr = (volatile uint32_t*)complete_addr;
    *ptr = interrupt_id;
}

/* ============================================================================
 * Timer Operations
 * ============================================================================ */

static uint64_t g_timer_freq = 0;

void riscv_hal_timer_init(uint64_t freq_hz) {
    g_timer_freq = freq_hz;
}

uint64_t riscv_hal_timer_get_mtime(void) {
    return READ_CSR(time);
}

void riscv_hal_timer_set_mtimecmp(uint64_t value) {
    /* mtimecmp typically accessed through SBI or memory-mapped */
    WRITE_CSR(mtimecmp, value);
}

/* ============================================================================
 * Virtual Memory Management
 * ============================================================================ */

void riscv_hal_vm_init(vm_mode_t mode) {
    uint64_t satp = READ_CSR(satp);
    satp &= ~(0xFUL << 60);  /* Clear MODE field */
    satp |= ((uint64_t)mode << 60);
    WRITE_CSR(satp, satp);
}

void riscv_hal_vm_set_satp(uint64_t satp_value) {
    WRITE_CSR(satp, satp_value);
}

uint64_t riscv_hal_vm_get_satp(void) {
    return READ_CSR(satp);
}

void riscv_hal_vm_flush_tlb(void) {
    /* SFENCE.VMA with all zeros flushes entire TLB */
    asm volatile ("sfence.vma");
}

void riscv_hal_vm_sfence_vma(uint64_t vaddr, uint64_t asid) {
    /* SFENCE.VMA vaddr, asid */
    asm volatile ("sfence.vma %0, %1" : : "r" (vaddr), "r" (asid));
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void riscv_hal_wfi(void) {
    asm volatile ("wfi");
}

void riscv_hal_fence(void) {
    asm volatile ("fence");
}

void riscv_hal_fence_tso(void) {
    asm volatile ("fence rw, rw");
}

/* ============================================================================
 * Stub Implementations (To be completed)
 * ============================================================================ */

void riscv_hal_pmu_init(void) {
    /* Performance monitoring unit initialization */
}

void riscv_hal_pmu_start_counter(uint32_t counter_id, pmu_event_t event) {
    /* Start performance counter */
}

uint64_t riscv_hal_pmu_read_counter(uint32_t counter_id) {
    /* Read performance counter */
    return 0;
}

int64_t riscv_hal_atomic_add(volatile int64_t* addr, int64_t value) {
    int64_t result;
    asm volatile ("amoadd.d %0, %2, (%1)" 
                  : "=r" (result) 
                  : "r" (addr), "r" (value));
    return result;
}

int64_t riscv_hal_atomic_swap(volatile int64_t* addr, int64_t value) {
    int64_t result;
    asm volatile ("amoswap.d %0, %2, (%1)" 
                  : "=r" (result) 
                  : "r" (addr), "r" (value));
    return result;
}

bool riscv_hal_atomic_cmp_swap(volatile int64_t* addr, int64_t expect, int64_t new) {
    int64_t result;
    asm volatile ("amoswap.d %0, %2, (%1)" 
                  : "=r" (result) 
                  : "r" (addr), "r" (new));
    return result == expect;
}

vector_config_t* riscv_hal_vector_get_config(void) {
    static vector_config_t config = {0};
    return &config;
}

void riscv_hal_vector_set_length(uint32_t vl) {
    /* VL configuration through custom CSRs */
}
