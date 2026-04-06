/*
 * Address Space Layout Randomization (ASLR)
 * uOS(m) - User OS Mobile
 */

#ifndef _ASLR_H_
#define _ASLR_H_

#include <stdint.h>

/* Initialize ASLR entropy */
void aslr_init(void);

/* Generate random base address for process */
uint64_t aslr_get_random_base(void);

/* Generate random stack offset */
uint64_t aslr_get_random_stack_offset(void);

/* Generate random heap offset */
uint64_t aslr_get_random_heap_offset(void);

#endif /* _ASLR_H_ */