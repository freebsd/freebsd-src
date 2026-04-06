/*
 * Stack Canary Protection
 * uOS(m) - User OS Mobile
 */

#ifndef _STACK_CANARY_H_
#define _STACK_CANARY_H_

#include <stdint.h>

/* Initialize stack canary */
void stack_canary_init(void);

/* Generate a random canary value */
uint64_t stack_canary_generate(void);

/* Check stack canary (called on function return) */
void stack_canary_check(uint64_t canary);

#endif /* _STACK_CANARY_H_ */