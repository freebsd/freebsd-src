/*-
 * Copyright (c) 2014 Andrew Turner
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>

int
fubyte(volatile const void *base)
{

	return *(volatile uint8_t *)base;
}

long
fuword(volatile const void *base)
{

	return *(volatile long *)base;
}

int
fuword16(volatile const void *base)
{

	panic("fuword16");
}

int32_t
fuword32(volatile const void *base)
{

	panic("fuword32");
}

int64_t
fuword64(volatile const void *base)
{

	panic("fuword64");
}

int
fuswintr(void *base)
{

	panic("fuswintr");
}

int
subyte(volatile void *base, int byte)
{

	*(volatile uint8_t *)base = byte;

	return 0;
}

int
suword(volatile void *base, long word)
{

	*(volatile long *)base = word;

	return 0;
}

int
suword16(volatile void *base, int word)
{

	panic("suword16");
}

int
suword32(volatile void *base, int32_t word)
{

	*(volatile int32_t *)base = word;

	return 0;
}

int
suword64(volatile void *base, int64_t word)
{

	*(volatile int64_t *)base = word;

	return 0;
}

int
suswintr(void *base, int word)
{

	panic("suswintr");
}

uint32_t
casuword32(volatile uint32_t *base, uint32_t oldval, uint32_t newval)
{

	panic("casuword32");
}

u_long
casuword(volatile u_long *p, u_long oldval, u_long newval)
{

	panic("casuword");
}

void
longjmp(struct _jmp_buf *buf, int val)
{

	panic("longjmp");
}

