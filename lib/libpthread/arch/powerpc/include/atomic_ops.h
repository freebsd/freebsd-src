/*
 * Copyright 2004 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_ATOMIC_OPS_H_
#define	_ATOMIC_OPS_H_

/*
 * Atomic swap:
 *   Atomic (tmp = *dst, *dst = val), then *res = tmp
 *
 * void atomic_swap32(intptr_t *dst, intptr_t val, intptr_t *res);
 */
static inline void
atomic_swap32(intptr_t *dst, intptr_t val, intptr_t *res)
{
	int tmp;

	tmp = 0;	/* should be a better way to quieten cc1... */
#ifdef __GNUC__
	__asm __volatile(
		"1:	lwarx	%0, 0, %4\n"	/* load with reservation */
		"	stwcx.	%3, 0, %4\n"	/* attempt to store val */
		"	bne- 1b\n"		/* interrupted? retry */
		"	stw	%0, %1\n"	/* else, *dst -> *res */
		: "=&r" (tmp), "=m" (*res), "+m" (*dst)
		: "r" (val), "r" (dst)
		: "cc", "memory");
#endif
}

#define	atomic_swap_ptr(d, v, r) \
	atomic_swap32((intptr_t *)d, (intptr_t)v, (intptr_t *)r)

#define	atomic_swap_int(d, v, r) \
	atomic_swap32((intptr_t *)d, (intptr_t)v, (intptr_t *)r)
#endif
