/*
 * Stack Canary Implementation
 * uOS(m) - User OS Mobile
 */

#include "stack_canary.h"

extern void uart_puts(const char *s);

/* Global canary value */
static uint64_t global_canary = 0;

/* Initialize stack canary */
int stack_canary_init(void) {
    /* Generate a canary value from hardware timer */
    volatile uint32_t *mtime_lo = (volatile uint32_t *)0x02004000;
    volatile uint32_t *mtime_hi = (volatile uint32_t *)0x02004004;
    uint32_t hi, lo;
    do {
        hi = *mtime_hi;
        lo = *mtime_lo;
    } while (*mtime_hi != hi);
    global_canary = ((uint64_t)hi << 32) | lo;
    global_canary ^= 0xDEADC0DEDEADC0DEULL;
    return 0;
}

/* Generate a random canary value */
uint64_t stack_canary_generate(void) {
    return global_canary;
}

/* Check stack canary (called on function return) */
void stack_canary_check(uint64_t canary) {
    if (canary != global_canary) {
        /* Stack corruption detected! */
        uart_puts("*** STACK CORRUPTION DETECTED ***\n");
        uart_puts("System halting for security...\n");

        /* In a real kernel, this would:
         * 1. Log the incident
         * 2. Terminate the offending process
         * 3. Possibly trigger a security response
         */
        while (1) {
            asm volatile("wfi"); /* Wait for interrupt - effectively halt */
        }
    }
}