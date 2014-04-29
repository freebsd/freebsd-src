/*-
 * Copyright (c) 2014 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

#ifndef SYS_DEV_RANDOM_UINT128_H_INCLUDED
#define SYS_DEV_RANDOM_UINT128_H_INCLUDED

/* This whole thing is a crock :-(
 *
 * Everyone knows you always need the __uint128_t types!
 */

#ifdef __SIZEOF_INT128__
typedef __uint128_t uint128_t;
#else
typedef uint64_t uint128_t[2];
#endif

static __inline void
uint128_clear(uint128_t *big_uint)
{
#ifdef __SIZEOF_INT128__
	(*big_uint) = 0ULL;
#else
	(*big_uint)[0] = (*big_uint)[1] = 0UL;
#endif
}

static __inline void
uint128_increment(uint128_t *big_uint)
{
#ifdef __SIZEOF_INT128__
	(*big_uint)++;
#else
	(*big_uint)[0]++;
	if ((*big_uint)[0] == 0UL)
		(*big_uint)[1]++;
#endif
}

static __inline int
uint128_is_zero(uint128_t big_uint)
{
#ifdef __SIZEOF_INT128__
	return (big_uint == 0ULL);
#else
	return (big_uint[0] == 0UL && big_uint[1] == 0UL);
#endif
}

#endif /* SYS_DEV_RANDOM_UINT128_H_INCLUDED */
