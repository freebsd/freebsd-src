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
 * $FreeBSD: src/sys/ia64/include/cpufunc.h,v 1.23 2007/06/10 16:53:01 marcel Exp $
 */

#ifndef _MACHINE_CPUFUNC_H_
#define _MACHINE_CPUFUNC_H_

#ifdef _KERNEL

#include <sys/types.h>
#include <machine/ia64_cpu.h>
#include <machine/vmparam.h>

#ifndef _SYS_CDEFS_H_
#error this file needs sys/cdefs.h as a prerequisite
#endif

struct thread;

#define	IA64_FIXED_BREAK	0x84B5D

#ifdef __GNUCLIKE_ASM

static __inline void
breakpoint(void)
{
	__asm __volatile("break.m %0" :: "i"(IA64_FIXED_BREAK));
}

#define	HAVE_INLINE_FFS
#define	ffs(x)	__builtin_ffs(x)

#define	__MEMIO_ADDR(x)		(__volatile void*)(IA64_PHYS_TO_RR6(x))
extern __volatile void *ia64_ioport_address(u_int);
#define	__PIO_ADDR(x)		ia64_ioport_address(x)

/*
 * I/O port reads with ia32 semantics.
 */
static __inline uint8_t
inb(unsigned int port)
{
	__volatile uint8_t *p;
	uint8_t v;
	p = __PIO_ADDR(port);
	ia64_mf();
	v = *p;
	ia64_mf_a();
	ia64_mf();
	return (v);
}

static __inline uint16_t
inw(unsigned int port)
{
	__volatile uint16_t *p;
	uint16_t v;
	p = __PIO_ADDR(port);
	ia64_mf();
	v = *p;
	ia64_mf_a();
	ia64_mf();
	return (v);
}

static __inline uint32_t
inl(unsigned int port)
{
	volatile uint32_t *p;
	uint32_t v;
	p = __PIO_ADDR(port);
	ia64_mf();
	v = *p;
	ia64_mf_a();
	ia64_mf();
	return (v);
}

static __inline void
insb(unsigned int port, void *addr, size_t count)
{
	uint8_t *buf = addr;
	while (count--)
		*buf++ = inb(port);
}

static __inline void
insw(unsigned int port, void *addr, size_t count)
{
	uint16_t *buf = addr;
	while (count--)
		*buf++ = inw(port);
}

static __inline void
insl(unsigned int port, void *addr, size_t count)
{
	uint32_t *buf = addr;
	while (count--)
		*buf++ = inl(port);
}

static __inline void
outb(unsigned int port, uint8_t data)
{
	volatile uint8_t *p;
	p = __PIO_ADDR(port);
	ia64_mf();
	*p = data;
	ia64_mf_a();
	ia64_mf();
}

static __inline void
outw(unsigned int port, uint16_t data)
{
	volatile uint16_t *p;
	p = __PIO_ADDR(port);
	ia64_mf();
	*p = data;
	ia64_mf_a();
	ia64_mf();
}

static __inline void
outl(unsigned int port, uint32_t data)
{
	volatile uint32_t *p;
	p = __PIO_ADDR(port);
	ia64_mf();
	*p = data;
	ia64_mf_a();
	ia64_mf();
}

static __inline void
outsb(unsigned int port, const void *addr, size_t count)
{
	const uint8_t *buf = addr;
	while (count--)
		outb(port, *buf++);
}

static __inline void
outsw(unsigned int port, const void *addr, size_t count)
{
	const uint16_t *buf = addr;
	while (count--)
		outw(port, *buf++);
}

static __inline void
outsl(unsigned int port, const void *addr, size_t count)
{
	const uint32_t *buf = addr;
	while (count--)
		outl(port, *buf++);
}

static __inline void
disable_intr(void)
{
	__asm __volatile ("rsm psr.i");
}

static __inline void
enable_intr(void)
{
	__asm __volatile ("ssm psr.i;; srlz.d");
}

static __inline register_t
intr_disable(void)
{
	register_t psr;
	__asm __volatile ("mov %0=psr;;" : "=r"(psr));
	disable_intr();
	return ((psr & IA64_PSR_I) ? 1 : 0);
}

static __inline void
intr_restore(register_t ie)
{
	if (ie)
		enable_intr();
}

#endif /* __GNUCLIKE_ASM */

#endif /* _KERNEL */

#endif /* !_MACHINE_CPUFUNC_H_ */
