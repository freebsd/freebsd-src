/*
 * Copyright (C) 2000, 2001 Broadcom Corporation
 * Copyright (C) 2002 Ralf Baechle
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __ASM_SIBYTE_64BIT_H
#define __ASM_SIBYTE_64BIT_H

#include <linux/config.h>
#include <linux/types.h>

#ifdef CONFIG_MIPS32

#include <asm/system.h>

/*
 * This is annoying...we can't actually write the 64-bit IO register properly
 * without having access to 64-bit registers...  which doesn't work by default
 * in o32 format...grrr...
 */
static inline void __out64(u64 val, unsigned long addr)
{
	u64 tmp;

	__asm__ __volatile__ (
		"	.set	mips3				\n"
		"	dsll32	%L0, %L0, 0	# __out64	\n"
		"	dsrl32	%L0, %L0, 0			\n"
		"	dsll32	%M0, %M0, 0			\n"
		"	or	%L0, %L0, %M0			\n"
		"	sd	%L0, (%2)			\n"
		"	.set	mips0				\n"
		: "=r" (tmp)
		: "0" (val), "r" (addr));
}

static inline void out64(u64 val, unsigned long addr)
{
	unsigned long flags;

	local_irq_save(flags);
	__out64(val, addr);
	local_irq_restore(flags);
}

static inline u64 __in64(unsigned long addr)
{
	u64 res;

	__asm__ __volatile__ (
		"	.set	mips3		# __in64	\n"
		"	ld	%L0, (%1)			\n"
		"	dsra32	%M0, %L0, 0			\n"
		"	sll	%L0, %L0, 0			\n"
		"	.set	mips0				\n"
		: "=r" (res)
		: "r" (addr));

	return res;
}

static inline u64 in64(unsigned long addr)
{
	unsigned long flags;
	u64 res;

	local_irq_save(flags);
	res = __in64(addr);
	local_irq_restore(flags);

	return res;
}

#endif /* CONFIG_MIPS32 */

#ifdef CONFIG_MIPS64

/*
 * These are provided so as to be able to use common
 * driver code for the 32-bit and 64-bit trees
 */
static inline void out64(u64 val, unsigned long addr)
{
	*(volatile unsigned long *)addr = val;
}

static inline u64 in64(unsigned long addr)
{
	return *(volatile unsigned long *)addr;
}

#define __in64(a)	in64(a)
#define __out64(v,a)	out64(v,a)

#endif /* CONFIG_MIPS64 */

/*
 * Avoid interrupt mucking, just adjust the address for 4-byte access.
 * Assume the addresses are 8-byte aligned.
 */

#ifdef __MIPSEB__
#define __CSR_32_ADJUST 4
#else
#define __CSR_32_ADJUST 0
#endif

#define csr_out32(v,a) (*(volatile u32 *)((unsigned long)(a) + __CSR_32_ADJUST) = (v))
#define csr_in32(a)    (*(volatile u32 *)((unsigned long)(a) + __CSR_32_ADJUST))

#endif /* __ASM_SIBYTE_64BIT_H */
