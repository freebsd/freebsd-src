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

#ifdef __GNUC__

static __inline void
breakpoint(void)
{
	__asm __volatile("break 0x80100"); /* XXX use linux value */
}

#endif

static __inline u_int8_t
inb(u_int port)
{
	return 0;		/* TODO: implement this */
}

static __inline u_int16_t
inw(u_int port)
{
	return 0;		/* TODO: implement this */
}

static __inline u_int32_t
inl(u_int port)
{
	return 0;		/* TODO: implement this */
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
	return;			/* TODO: implement this */
}

static __inline void
outw(u_int port, u_int16_t data)
{
	return;			/* TODO: implement this */
}

static __inline void
outl(u_int port, u_int32_t data)
{
	return;			/* TODO: implement this */
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
	return 0;		/* TODO: implement this */
}

static __inline u_int16_t
readw(u_int addr)
{
	return 0;		/* TODO: implement this */
}

static __inline u_int32_t
readl(u_int addr)
{
	return 0;		/* TODO: implement this */
}

static __inline void
writeb(u_int addr, u_int8_t data)
{
	return;			/* TODO: implement this */
}

static __inline void
writew(u_int addr, u_int16_t data)
{
	return;			/* TODO: implement this */
}

static __inline void
writel(u_int addr, u_int32_t data)
{
	return;			/* TODO: implement this */
}

/*
 * Bogus interrupt manipulation
 */
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

static __inline u_int
save_intr(void)
{
	u_int psr;
	__asm __volatile ("mov %0=psr;;" : "=r" (psr));
	return psr;
}

static __inline void
restore_intr(u_int psr)
{
	__asm __volatile ("mov psr.l=%0;; srlz.d" :: "r" (psr));
}

#endif /* _KERNEL */

#endif /* !_MACHINE_CPUFUNC_H_ */
