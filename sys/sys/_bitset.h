/*-
 * Copyright (c) 2008, Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Copyright (c) 2008 Nokia Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef _SYS__BITSET_H_
#define	_SYS__BITSET_H_

/*
 * Macros addressing word and bit within it, tuned to make compiler
 * optimize cases when SETSIZE fits into single machine word.
 */
#define	_BITSET_BITS		(sizeof(long) * NBBY)

#define	__bitset_words(_s)	(howmany(_s, _BITSET_BITS))

#define	__bitset_mask(_s, n)						\
	(1L << ((__bitset_words((_s)) == 1) ?				\
	    (__size_t)(n) : ((n) % _BITSET_BITS)))

#define	__bitset_word(_s, n)						\
	((__bitset_words((_s)) == 1) ? 0 : ((n) / _BITSET_BITS))

#define	BITSET_DEFINE(t, _s)						\
struct t {								\
        long    __bits[__bitset_words((_s))];				\
}

#define	BITSET_T_INITIALIZER(x)						\
	{ .__bits = { x } }

#define	BITSET_FSET(n)							\
	[ 0 ... ((n) - 1) ] = (-1L)

#endif /* !_SYS__BITSET_H_ */
