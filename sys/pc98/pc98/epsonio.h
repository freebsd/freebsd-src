/*
 * Copyright (c) KATO Takenori, 1996.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __PC98_PC98_EPSONIO_H__
#define __PC98_PC98_EPSONIO_H__

#include <machine/cpufunc.h>
#include <machine/ipl.h>

static __inline u_char
epson_inb(u_int port)
{
	u_char	data;

	outb(0x43f, 0x42);
	data = inb(port);
	outb(0x43f, 0x40);
	return (data);
}

static __inline void
epson_outb(u_int port, u_char data)
{
	outb(0x43f, 0x42);
	outb(port,data);
	outb(0x43f, 0x40);
}

static __inline u_int16_t
epson_inw(u_int port)
{
	u_int16_t data;

	outb(0x43f, 0x42);
	data = inw(port);
	outb(0x43f, 0x40);
	return (data);
}

static __inline void
epson_insw(u_int port, void *addr, size_t cnt)
{
	int	s;

	s = splbio();
	outb(0x43f, 0x42);
	disable_intr();
	insw((u_int)port, (void *)addr, (size_t)cnt);
	outb(0x43f, 0x40);
	enable_intr();
	splx(s);
}

static __inline void
epson_outsw(u_int port, void *addr, size_t cnt)
{
	int	s;

	s = splbio();
	outb(0x43f, 0x42);
	disable_intr();
	outsw((u_int)port, (void *)addr, (size_t)cnt);
	outb(0x43f, 0x40);
	enable_intr();
	splx(s);
}

#endif
