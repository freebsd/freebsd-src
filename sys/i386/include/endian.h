/*
 * Copyright (c) 1987, 1991 Regents of the University of California.
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
 *	from: @(#)endian.h	7.8 (Berkeley) 4/3/91
 * $FreeBSD$
 */

#ifndef _MACHINE_ENDIAN_H_
#define	_MACHINE_ENDIAN_H_

/*
 * Define the order of 32-bit words in 64-bit words.
 */
#define	_QUAD_HIGHWORD 1
#define	_QUAD_LOWWORD 0

#ifndef _POSIX_SOURCE

/*
 * Definitions for byte order, according to byte significance from low
 * address to high.
 */
#define	LITTLE_ENDIAN	1234	/* LSB first: i386, vax */
#define	BIG_ENDIAN	4321	/* MSB first: 68000, ibm, net */
#define	PDP_ENDIAN	3412	/* LSB first in word, MSW first in long */

#define	BYTE_ORDER	LITTLE_ENDIAN

#ifndef _KERNEL
#include <sys/cdefs.h>
#endif
#include <machine/ansi.h>

__BEGIN_DECLS
__uint32_t	htonl(__uint32_t);
__uint16_t	htons(__uint16_t);
__uint32_t	ntohl(__uint32_t);
__uint16_t	ntohs(__uint16_t);
__END_DECLS

#ifdef __GNUC__

static __inline __uint32_t
__uint16_swap_uint32(__uint32_t __x)
{
	__asm ("rorl $16, %1" : "=r" (__x) : "0" (__x));

	return __x;
}

static __inline __uint32_t
__uint8_swap_uint32(__uint32_t __x)
{
#if defined(_KERNEL) && (defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)) && !defined(I386_CPU)
	__asm ("bswap %0" : "=r" (__x) : "0" (__x));
#else
	__asm ("xchgb %h1, %b1\n\t"
	       "rorl $16, %1\n\t"
	       "xchgb %h1, %b1"
	       : "=q" (__x) : "0" (__x));
#endif
	return __x;
}

static __inline __uint16_t
__uint8_swap_uint16(__uint16_t __x)
{
	__asm ("xchgb %h1, %b1" : "=q" (__x) : "0" (__x));

	return __x;
}

/*
 * Macros for network/external number representation conversion.
 */
#define	ntohl	__uint8_swap_uint32
#define	ntohs	__uint8_swap_uint16
#define	htonl	__uint8_swap_uint32
#define	htons	__uint8_swap_uint16

#endif	/* __GNUC__ */

#define	NTOHL(x)	((x) = ntohl(x))
#define	NTOHS(x)	((x) = ntohs(x))
#define	HTONL(x)	((x) = htonl(x))
#define	HTONS(x)	((x) = htons(x))


#endif /* ! _POSIX_SOURCE */

#endif /* !_MACHINE_ENDIAN_H_ */
