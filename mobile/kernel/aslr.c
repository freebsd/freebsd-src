/*
 * Address Space Layout Randomization Implementation
 * uOS(m) - User OS Mobile
 */

#include "aslr.h"

/* PRNG state */
static uint64_t aslr_seed;

/* Simple linear congruential generator (LCG) */
static uint64_t aslr_rand(void) {
    aslr_seed = aslr_seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return aslr_seed;
}

/* Initialize ASLR entropy from hardware timer + hart ID */
int aslr_init(void) {
    /* Read CLINT mtime (64-bit hardware timer) for entropy */
    volatile uint32_t *mtime_lo = (volatile uint32_t *)0x02004000;
    volatile uint32_t *mtime_hi = (volatile uint32_t *)0x02004004;
    uint32_t hi, lo;
    do {
        hi = *mtime_hi;
        lo = *mtime_lo;
    } while (*mtime_hi != hi);
    uint64_t mtime = ((uint64_t)hi << 32) | lo;

    /* Mix in hart ID */
    uint64_t hartid = 0;
    asm volatile("csrr %0, mhartid" : "=r"(hartid));

    /* Combine with a non-linear mix */
    aslr_seed = mtime ^ (hartid * 0x9E3779B97F4A7C15ULL);
    aslr_seed ^= aslr_seed >> 33;
    aslr_seed *= 0xFF51AFD7ED558CCDULL;
    aslr_seed ^= aslr_seed >> 33;
    return 0;
}

/* Generate random base address for process */
uint64_t aslr_get_random_base(void) {
    /* Randomize within a 256MB range, aligned to 4KB */
    uint64_t random_offset = aslr_rand() % (256 * 1024 * 1024);
    random_offset &= ~0xFFFULL; /* 4KB alignment */
    return 0x400000ULL + random_offset; /* Start from 4MB */
}

/* Generate random stack offset */
uint64_t aslr_get_random_stack_offset(void) {
    /* Randomize stack position within 64MB range */
    uint64_t random_offset = aslr_rand() % (64 * 1024 * 1024);
    random_offset &= ~0xFFFULL; /* 4KB alignment */
    return random_offset;
}

/* Generate random heap offset */
uint64_t aslr_get_random_heap_offset(void) {
    /* Randomize heap start within 128MB range */
    uint64_t random_offset = aslr_rand() % (128 * 1024 * 1024);
    random_offset &= ~0xFFFULL; /* 4KB alignment */
    return random_offset;
}