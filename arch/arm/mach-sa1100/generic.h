/*
 * linux/arch/arm/mach-sa1100/generic.h
 *
 * Author: Nicolas Pitre
 */

extern void __init sa1100_map_io(void);
extern void __init sa1100_init_irq(void);

#define SET_BANK(__nr,__start,__size) \
	mi->bank[__nr].start = (__start), \
	mi->bank[__nr].size = (__size), \
	mi->bank[__nr].node = (((unsigned)(__start) - PHYS_OFFSET) >> 27)

