/*-
 * Copyright (c) 2003 Daniel Eischen <deischen@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#ifndef	_ATOMIC_OPS_H_
#define	_ATOMIC_OPS_H_

/*
 * Atomic swap:
 *   Atomic (tmp = *dst, *dst = val), then *res = tmp
 *
 * void atomic_swap_long(long *dst, long val, long *res);
 */
static inline void
atomic_swap_long(long *dst, long val, long *res)
{
	/* $1 and $2 are t0 and t1 respectively. */
	__asm __volatile (
		"   ldq     $1, %1\n"	/* get cache line before lock */
		"1: ldq_l   $1, %1\n"	/* load *dst asserting lock */
		"   mov     %2, $2\n"	/* save value to be swapped */
		"   stq_c   $2, %1\n"	/* attempt the store; $2 clobbered */
		"   beq     $2, 1b\n"	/* it didn't work, loop */
		"   stq     $1, %0\n"	/* save value of *dst in *res */
		"   mb            \n"
	    : "+m"(*res)
	    : "m"(*dst), "r"(val)
	    : "memory", "$1", "$2");	/* clobber t0 and t1 */
}

static inline void
atomic_swap_int(int *dst, int val, int *res)
{
	/* $1 and $2 are t0 and t1 respectively. */
	__asm __volatile (
		"   ldl     $1, %1\n"	/* get cache line before lock */
		"1: ldl_l   $1, %1\n"	/* load *dst asserting lock */
		"   mov     %2, $2\n"	/* save value to be swapped */
		"   stl_c   $2, %1\n"	/* attempt the store; $2 clobbered */
		"   beq     $2, 1b\n"	/* it didn't work, loop */
		"   stl     $1, %0\n"	/* save value of *dst in *res */
		"   mb            \n"
	    : "+m"(*res)
	    : "m"(*dst), "r"(val)
	    : "memory", "$1", "$2");	/* clobber t0 and t1 */
}

#define	atomic_swap_ptr(d, v, r) \
	atomic_swap_long((long *)(d), (long)(v), (long *)(r))

#endif
