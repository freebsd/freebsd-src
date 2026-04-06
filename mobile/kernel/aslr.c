/*
 * Address Space Layout Randomization Implementation
 * uOS(m) - User OS Mobile
 */

#include "aslr.h"

/* Simple PRNG state */
static uint64_t aslr_seed = 0x123456789ABCDEF0ULL;

/* Simple linear congruential generator */
static uint64_t aslr_rand(void) {
    aslr_seed = aslr_seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return aslr_seed;
}

/* Initialize ASLR entropy */
void aslr_init(void) {
    /* Use system time or hardware entropy if available */
    /* For now, use a fixed seed that can be improved */
    aslr_seed = 0xDEADBEEFDEADBEEFULL;
}

/* Generate random base address for process */
uint64_t aslr_get_random_base(void) {
    /* Randomize within a 1GB range, aligned to 4KB */
    uint64_t random_offset = aslr_rand() % (256 * 1024 * 1024); /* 256MB range */
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