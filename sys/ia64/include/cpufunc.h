/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_CPUFUNC_H_
#define _MACHINE_CPUFUNC_H_

#ifdef _KERNEL

#include <sys/types.h>
#include <machine/ia64_cpu.h>

#define	CRITICAL_FORK	(ia64_get_psr() |= IA64_PSR_I)

#ifdef __GNUC__

static __inline void
breakpoint(void)
{
	__asm __volatile("break 0x80100"); /* XXX use linux value */
}

#endif

extern u_int64_t	ia64_port_base;

static __inline volatile void *
ia64_port_address(u_int port)
{
    return (volatile void *)(ia64_port_base
			     | ((port >> 2) << 12)
			     | (port & ((1 << 12) - 1)));
}

static __inline volatile void *
ia64_memory_address(u_int64_t addr)
{
	return (volatile void *) IA64_PHYS_TO_RR6(addr);
}

static __inline u_int8_t
inb(u_int port)
{
	volatile u_int8_t *p = ia64_port_address(port);
	u_int8_t v = *p;
	ia64_mf_a();
	ia64_mf();
	return v;
}

static __inline u_int16_t
inw(u_int port)
{
	volatile u_int16_t *p = ia64_port_address(port);
	u_int16_t v = *p;
	ia64_mf_a();
	ia64_mf();
	return v;
}

static __inline u_int32_t
inl(u_int port)
{
	volatile u_int32_t *p = ia64_port_address(port);
	u_int32_t v = *p;
	ia64_mf_a();
	ia64_mf();
	return v;
}

static __inline void
insb(u_int port, void *addr, size_t count)
{
	u_int8_t *p = addr;
	while (count--)
		*p++ = inb(port);
}

static __inline void
insw(u_int port, void *addr, size_t count)
{
	u_int16_t *p = addr;
	while (count--)
		*p++ = inw(port);
}

static __inline void
insl(u_int port, void *addr, size_t count)
{
	u_int32_t *p = addr;
	while (count--)
		*p++ = inl(port);
}

static __inline void
outb(u_int port, u_int8_t data)
{
	volatile u_int8_t *p = ia64_port_address(port);
	*p = data;
	ia64_mf_a();
	ia64_mf();
}

static __inline void
outw(u_int port, u_int16_t data)
{
	volatile u_int16_t *p = ia64_port_address(port);
	*p = data;
	ia64_mf_a();
	ia64_mf();
}

static __inline void
outl(u_int port, u_int32_t data)
{
	volatile u_int32_t *p = ia64_port_address(port);
	*p = data;
	ia64_mf_a();
	ia64_mf();
}

static __inline void
outsb(u_int port, const void *addr, size_t count)
{
	const u_int8_t *p = addr;
	while (count--)
		outb(port, *p++);
}

static __inline void
outsw(u_int port, const void *addr, size_t count)
{
	const u_int16_t *p = addr;
	while (count--)
		outw(port, *p++);
}

static __inline void
outsl(u_int port, const void *addr, size_t count)
{
	const u_int32_t *p = addr;
	while (count--)
		outl(port, *p++);
}

static __inline u_int8_t
readb(u_int addr)
{
	volatile u_int8_t *p = ia64_memory_address(addr);
	u_int8_t v = *p;
	ia64_mf_a();
	ia64_mf();
	return v;
}

static __inline u_int16_t
readw(u_int addr)
{
	volatile u_int16_t *p = ia64_memory_address(addr);
	u_int16_t v = *p;
	ia64_mf_a();
	ia64_mf();
	return v;
}

static __inline u_int32_t
readl(u_int addr)
{
	volatile u_int32_t *p = ia64_memory_address(addr);
	u_int32_t v = *p;
	ia64_mf_a();
	ia64_mf();
	return v;
}

static __inline void
writeb(u_int addr, u_int8_t data)
{
	volatile u_int8_t *p = ia64_memory_address(addr);
	*p = data;
	ia64_mf_a();
	ia64_mf();
}

static __inline void
writew(u_int addr, u_int16_t data)
{
	volatile u_int16_t *p = ia64_memory_address(addr);
	*p = data;
	ia64_mf_a();
	ia64_mf();
}

static __inline void
writel(u_int addr, u_int32_t data)
{
	volatile u_int32_t *p = ia64_memory_address(addr);
	*p = data;
	ia64_mf_a();
	ia64_mf();
}

static __inline void
memcpy_fromio(u_int8_t *addr, size_t ofs, size_t count)
{
	volatile u_int8_t *p = ia64_memory_address(ofs);
	while (count--)
		*addr++ = *p++;
}

static __inline void
memcpy_io(size_t dst, size_t src, size_t count)
{
	volatile u_int8_t *dp = ia64_memory_address(dst);
	volatile u_int8_t *sp = ia64_memory_address(src);
	while (count--)
		*dp++ = *sp++;
}

static __inline void
memcpy_toio(size_t ofs, u_int8_t *addr, size_t count)
{
	volatile u_int8_t *p = ia64_memory_address(ofs);
	while (count--)
		*p++ = *addr++;
}

static __inline void
memset_io(size_t ofs, u_int8_t value, size_t count)
{
	volatile u_int8_t *p = ia64_memory_address(ofs);
	while (count--)
		*p++ = value;
}

static __inline void
memsetw(u_int16_t *addr, int val, size_t size)
{
	while (size--)
		*addr++ = val;
}

static __inline void
memsetw_io(size_t ofs, u_int16_t value, size_t count)
{
	volatile u_int16_t *p = ia64_memory_address(ofs);
	while (count--)
		*p++ = value;
}

static __inline void
disable_intr(void)
{
	__asm __volatile ("rsm psr.i;;");
}

static __inline void
enable_intr(void)
{
	__asm __volatile (";; ssm psr.i;; srlz.d");
}

static __inline critical_t
cpu_critical_enter(void)
{
	critical_t psr;

	__asm __volatile ("mov %0=psr;;" : "=r" (psr));
	disable_intr();
	return (psr);
}

static __inline void
cpu_critical_exit(critical_t psr)
{
	__asm __volatile ("mov psr.l=%0;; srlz.d" :: "r" (psr));
}

#endif /* _KERNEL */

#endif /* !_MACHINE_CPUFUNC_H_ */
