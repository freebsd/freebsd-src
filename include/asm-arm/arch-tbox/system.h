/*
 * linux/include/asm-arm/arch-tbox/system.h
 *
 * Copyright (c) 1996-1999 Russell King.
 */
#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

static inline void arch_idle(void)
{
	cpu_do_idle();
}

#define arch_reset(mode)	do { } while (0)

#endif
