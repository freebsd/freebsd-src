/*-
 * Copyright (c) 1987, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)endian.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

#ifndef _MACHINE_ENDIAN_H_
#define	_MACHINE_ENDIAN_H_

#include <machine/ansi.h>

/*
 * Define the order of 32-bit words in 64-bit words.
 */
#define _QUAD_HIGHWORD 0
#define _QUAD_LOWWORD 1

#ifndef _POSIX_SOURCE
/*
 * Definitions for byte order, according to byte significance from low
 * address to high.
 */
#define	LITTLE_ENDIAN	1234	/* LSB first: i386, vax */
#define	BIG_ENDIAN	4321	/* MSB first: 68000, ibm, net */
#define	PDP_ENDIAN	3412	/* LSB first in word, MSW first in long */

#define	BYTE_ORDER	BIG_ENDIAN
#endif /* !_POSIX_SOURCE */

#ifdef __GNUC__

static __inline __uint16_t
__bswap16(__uint16_t _x)
{

	return ((_x >> 8) | ((_x << 8) & 0xff00));
}

static __inline __uint32_t
__bswap32(__uint32_t _x)
{

	return ((_x >> 24) | ((_x >> 8) & 0xff00) | ((_x << 8) & 0xff0000) |
	    ((_x << 24) & 0xff000000));
}

static __inline __uint64_t
__bswap64(__uint64_t _x)
{

	return ((_x >> 56) | ((_x >> 40) & 0xff00) | ((_x >> 24) & 0xff0000) |
	    ((_x >> 8) & 0xff000000) | ((_x << 8) & ((__uint64_t)0xff << 32)) |
	    ((_x << 24) & ((__uint64_t)0xff << 40)) |
	    ((_x << 40) & ((__uint64_t)0xff << 48)) | ((_x << 56)));
}

#endif /* __GNUC__ */

#define	__htonl(x)	((__uint32_t)(x))
#define	__htons(x)	((__uint16_t)(x))
#define	__ntohl(x)	((__uint32_t)(x))
#define	__ntohs(x)	((__uint16_t)(x))

#endif /* !_MACHINE_ENDIAN_H_ */
