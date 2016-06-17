/*
 * linux/include/asm-arm/arch-tbox/io.h
 *
 * Copyright (C) 1996-1999 Russell King
 * Copyright (C) 1998, 1999 Philip Blundell
 *
 */
#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffffffff

#define __io(_x)		((_x) << 2)

/*
 * Generic virtual read/write
 */
static inline unsigned int __arch_getw(unsigned long a)
{
	unsigned int value;
	__asm__ __volatile__("ldr%?h	%0, [%1, #0]	@ getw"
		: "=&r" (value)
		: "r" (a));
	return value;
}

static inline void __arch_putw(unsigned int value, unsigned long a)
{
	__asm__ __volatile__("str%?h	%0, [%1, #0]	@ putw"
		: : "r" (value), "r" (a));
}

/* Idem, for devices on the upper byte lanes */
#define inb_u(p)		__arch_getb(__io_pc(p) + 2)
#define inw_u(p)		__arch_getw(__io_pc(p) + 2)

#define outb_u(v,p)		__arch_putb(v,__io_pc(p) + 2)
#define outw_u(v,p)		__arch_putw(v,__io_pc(p) + 2)

#endif
