/*
 *  linux/include/asm-arm/arch-arc/system.h
 *
 *  Copyright (C) 1996-1999 Russell King and Dave Gilbert
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

static inline void arch_idle(void)
{
}

static inline void arch_reset(char mode)
{
	/*
	 * copy branch instruction to reset location and call it
	 */
	*(unsigned long *)0 = *(unsigned long *)0x03800000;
	((void(*)(void))0)();
}
