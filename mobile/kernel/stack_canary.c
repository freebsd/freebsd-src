/*
 * Stack Canary Implementation
 * uOS(m) - User OS Mobile
 */

#include "stack_canary.h"

extern void uart_puts(const char *s);

/* Global canary value */
static uint64_t global_canary = 0;

/* Initialize stack canary */
void stack_canary_init(void) {
    /* Generate a random canary value */
    /* In a real implementation, this would use hardware entropy */
    global_canary = 0xDEADC0DEDEADC0DEULL;
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