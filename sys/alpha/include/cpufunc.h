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
#include <machine/chipset.h>
#include <machine/alpha_cpu.h>

#ifdef __GNUC__

static __inline void
breakpoint(void)
{
	__asm __volatile("call_pal 0x81"); /* XXX bugchk */
}

#endif

#define inb(port)		chipset.inb(port)
#define inw(port)		chipset.inw(port)
#define inl(port)		chipset.inl(port)
#define outb(port, data)	chipset.outb(port, data)
#define outw(port, data)	chipset.outw(port, data)
#define outl(port, data)	chipset.outl(port, data)

#define readb(pa)		chipset.readb(pa)
#define readw(pa)		chipset.readw(pa)
#define readl(pa)		chipset.readl(pa)
#define writeb(pa,v)		chipset.writeb(pa,v)
#define writew(pa,v)		chipset.writew(pa,v)
#define writel(pa,v)		chipset.writel(pa,v)

/*
 * Bulk i/o (for IDE driver).
 */
static __inline void insb(u_int32_t port, void *buffer, size_t count)
{
        u_int8_t *p = (u_int8_t *) buffer;
        while (count--)
                *p++ = inb(port);
}

static __inline void insw(u_int32_t port, void *buffer, size_t count)
{
	u_int16_t *p = (u_int16_t *) buffer;
	while (count--)
		*p++ = inw(port);
}

static __inline void insl(u_int32_t port, void *buffer, size_t count)
{
	u_int32_t *p = (u_int32_t *) buffer;
	while (count--)
		*p++ = inl(port);
}

static __inline void outsb(u_int32_t port, const void *buffer, size_t count)
{
        const u_int8_t *p = (const u_int8_t *) buffer;
        while (count--)
                outb(port, *p++);
}

static __inline void outsw(u_int32_t port, const void *buffer, size_t count)
{
	const u_int16_t *p = (const u_int16_t *) buffer;
	while (count--)
		outw(port, *p++);
}

static __inline void outsl(u_int32_t port, const void *buffer, size_t count)
{
	const u_int32_t *p = (const u_int32_t *) buffer;
	while (count--)
		outl(port, *p++);
}

/*
 * String version of IO memory access ops:
 */
extern void memcpy_fromio(void *, u_int32_t, size_t);
extern void memcpy_toio(u_int32_t, void *, size_t);
extern void memcpy_io(u_int32_t, u_int32_t, size_t);
extern void memset_io(u_int32_t, int, size_t);
extern void memsetw(void *, int, size_t);
extern void memsetw_io(u_int32_t, int, size_t);

static __inline void
enable_intr(void)
{
	(void)alpha_pal_swpipl(ALPHA_PSL_IPL_0);
}

static __inline void
disable_intr(void)
{
	(void)alpha_pal_swpipl(ALPHA_PSL_IPL_HIGH);
}


#endif /* _KERNEL */

#endif /* !_MACHINE_CPUFUNC_H_ */
