/**
 * \file drm_atomic.h
 * Atomic operations used in the DRM which may or may not be provided by the OS.
 * 
 * \author Eric Anholt <anholt@FreeBSD.org>
 */

/*-
 * Copyright 2004 Eric Anholt
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* Many of these implementations are rather fake, but good enough. */

typedef u_int32_t atomic_t;

#ifdef __FreeBSD__
#define atomic_set(p, v)	(*(p) = (v))
#define atomic_read(p)		(*(p))
#define atomic_inc(p)		atomic_add_int(p, 1)
#define atomic_dec(p)		atomic_subtract_int(p, 1)
#define atomic_add(n, p)	atomic_add_int(p, n)
#define atomic_sub(n, p)	atomic_subtract_int(p, n)
#else /* __FreeBSD__ */
/* FIXME */
#define atomic_set(p, v)	(*(p) = (v))
#define atomic_read(p)		(*(p))
#define atomic_inc(p)		(*(p) += 1)
#define atomic_dec(p)		(*(p) -= 1)
#define atomic_add(n, p)	(*(p) += (n))
#define atomic_sub(n, p)	(*(p) -= (n))
/* FIXME */
#define atomic_add_int(p, v)      *(p) += v
#define atomic_subtract_int(p, v) *(p) -= v
#define atomic_set_int(p, bits)   *(p) |= (bits)
#define atomic_clear_int(p, bits) *(p) &= ~(bits)
#endif /* !__FreeBSD__ */

#if !defined(__FreeBSD_version) || (__FreeBSD_version < 500000)
#if defined(__i386__)
/* The extra atomic functions from 5.0 haven't been merged to 4.x */
static __inline int
atomic_cmpset_int(volatile u_int *dst, u_int exp, u_int src)
{
	int res = exp;

	__asm __volatile (
	"	lock ;			"
	"	cmpxchgl %1,%2 ;	"
	"       setz	%%al ;		"
	"	movzbl	%%al,%0 ;	"
	"1:				"
	"# atomic_cmpset_int"
	: "+a" (res)			/* 0 (result) */
	: "r" (src),			/* 1 */
	  "m" (*(dst))			/* 2 */
	: "memory");				 

	return (res);
}
#else /* __i386__ */
static __inline int
atomic_cmpset_int(__volatile__ int *dst, int old, int new)
{
	int s = splhigh();
	if (*dst==old) {
		*dst = new;
		splx(s);
		return 1;
	}
	splx(s);
	return 0;
}
#endif /* !__i386__ */
#endif /* !__FreeBSD_version || __FreeBSD_version < 500000 */

static __inline atomic_t
test_and_set_bit(int b, volatile void *p)
{
	int s = splhigh();
	unsigned int m = 1<<b;
	unsigned int r = *(volatile int *)p & m;
	*(volatile int *)p |= m;
	splx(s);
	return r;
}

static __inline void
clear_bit(int b, volatile void *p)
{
	atomic_clear_int(((volatile int *)p) + (b >> 5), 1 << (b & 0x1f));
}

static __inline void
set_bit(int b, volatile void *p)
{
	atomic_set_int(((volatile int *)p) + (b >> 5), 1 << (b & 0x1f));
}

static __inline int
test_bit(int b, volatile void *p)
{
	return ((volatile int *)p)[b >> 5] & (1 << (b & 0x1f));
}

static __inline int
find_first_zero_bit(volatile void *p, int max)
{
	int b;
	volatile int *ptr = (volatile int *)p;

	for (b = 0; b < max; b += 32) {
		if (ptr[b >> 5] != ~0) {
			for (;;) {
				if ((ptr[b >> 5] & (1 << (b & 0x1f))) == 0)
					return b;
				b++;
			}
		}
	}
	return max;
}
