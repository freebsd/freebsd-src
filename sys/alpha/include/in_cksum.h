/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	from tahoe:	in_cksum.c	1.2	86/01/05
 *	from:		@(#)in_cksum.c	1.3 (Berkeley) 1/19/91
 *	from: Id: in_cksum.c,v 1.8 1995/12/03 18:35:19 bde Exp
 *	$Id: in_cksum.h,v 1.5 1997/08/16 19:14:55 wollman Exp $
 */

#ifndef _MACHINE_IN_CKSUM_H_
#define	_MACHINE_IN_CKSUM_H_	1

#include <sys/cdefs.h>

/*
 * It it useful to have an Internet checksum routine which is inlineable
 * and optimized specifically for the task of computing IP header checksums
 * in the normal case (where there are no options and the header length is
 * therefore always exactly five 32-bit words.
 */
#ifdef __GNUC__

static __inline void
in_cksum_update(struct ip *ip)
{
	int __tmpsum;
	__tmpsum = (int)ntohs(ip->ip_sum) + 256;
	ip->ip_sum = htons(__tmpsum + (__tmpsum >> 16));
}

#else

#define	in_cksum_update(ip) \
	do { \
		int __tmpsum; \
		__tmpsum = (int)ntohs(ip->ip_sum) + 256; \
		ip->ip_sum = htons(__tmpsum + (__tmpsum >> 16)); \
	} while(0)

#endif

typedef	unsigned in_psum_t;
#ifdef KERNEL
u_int in_cksum_hdr(const struct ip *ip);
in_psum_t in_cksum_partial(in_psum_t psum, const u_short *w, int len);
int	in_cksum_finalize(in_psum_t psum);
#endif /* KERNEL */

#endif /* _MACHINE_IN_CKSUM_H_ */
