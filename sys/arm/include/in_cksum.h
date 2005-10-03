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
 * $FreeBSD$
 */

#ifndef _MACHINE_IN_CKSUM_H_
#define	_MACHINE_IN_CKSUM_H_	1

#include <sys/cdefs.h>

#ifdef _KERNEL
u_short in_cksum(struct mbuf *m, int len);
u_short in_addword(u_short sum, u_short b);
u_short in_cksum_skip(struct mbuf *m, int len, int skip);
u_int do_cksum(const void *, int);
static __inline u_int
in_cksum_hdr(const struct ip *ip)
{
	u_int sum = 0;
	u_int tmp1, tmp2, tmp3, tmp4;

	if (((vm_offset_t)ip & 0x03) == 0)
		__asm __volatile (
		    "adds %0, %0, %1\n"
		    "adcs %0, %0, %2\n"
		    "adcs %0, %0, %3\n"
		    "adcs %0, %0, %4\n"
		    "adcs %0, %0, %5\n"
		    "adc %0, %0, #0\n"
		    : "+r" (sum)
		    : "r" (((const u_int32_t *)ip)[0]),
		    "r" (((const u_int32_t *)ip)[1]),
		    "r" (((const u_int32_t *)ip)[2]),
		    "r" (((const u_int32_t *)ip)[3]),
		    "r" (((const u_int32_t *)ip)[4])
		    );
	else
		__asm __volatile (
		    "and %1, %5, #3\n"
		    "cmp %1, #0x02\n"
		    "ldrb %2, [%5], #0x01\n"
		    "ldrgeb %3, [%5], #0x01\n"
		    "movlt %3, #0\n"
		    "ldrgtb %4, [%5], #0x01\n"
		    "movle %4, #0x00\n"
#ifdef __ARMEB__
		    "orreq	%0, %3, %2, lsl #8\n"
		    "orreq	%0, %0, %4, lsl #24\n"
		    "orrne	%0, %0, %3, lsl #8\n"
		    "orrne	%0, %0, %4, lsl #16\n"
#else
		    "orreq	%0, %2, %3, lsl #8\n"
		    "orreq	%0, %0, %4, lsl #16\n"
		    "orrne	%0, %3, %2, lsl #8\n"
		    "orrne	%0, %0, %4, lsl #24\n"
#endif
		    "ldmia %5, {%2, %3, %4}\n"
		    "adcs %0, %0, %2\n"
		    "adcs %0, %0, %3\n"
		    "adcs %0, %0, %4\n"
		    "ldrb %2, [%5]\n"
		    "cmp %1, #0x02\n"
		    "ldrgeb %3, [%5, #0x01]\n"
		    "movlt %3, #0x00\n"
		    "ldrgtb %4, [%5, #0x02]\n"
		    "movle %4, #0x00\n"
		    "tst %5, #0x01\n"
#ifdef __ARMEB__
	    	    "orreq	%2, %3, %2, lsl #8\n"
		    "orreq	%2, %2, %4, lsl #24\n"
		    "orrne	%2, %2, %3, lsl #8\n"
		    "orrne	%2, %2, %4, lsl #16\n"
#else
		    "orreq	%2, %2, %3, lsl #8\n"
		    "orreq	%2, %2, %4, lsl #16\n"
		    "orrne	%2, %3, %2, lsl #8\n"
		    "orrne	%2, %2, %4, lsl #24\n"
#endif
		    "adds	%0, %0, %2\n"
		    "adc %0, %0, #0\n"
		    : "+r" (sum), "=r" (tmp1), "=r" (tmp2), "=r" (tmp3),
		    "=r" (tmp4)
		    : "r" (ip));
		    
	sum = (sum & 0xffff) + (sum >> 16);
	if (sum > 0xffff)
		sum -= 0xffff;
	return (~sum & 0xffff);
}

static __inline u_short
in_pseudo(u_int sum, u_int b, u_int c)
{
	__asm __volatile("adds %0, %0, %1\n"
	    		"adcs %0, %0, %2\n"
			"adc %0, %0, #0\n"
			: "+r" (sum) 
			: "r" (b), "r" (c));
	sum = (sum & 0xffff) + (sum >> 16);
	if (sum > 0xffff)
		sum -= 0xffff;
	return (sum);
}

#endif /* _KERNEL */
#endif /* _MACHINE_IN_CKSUM_H_ */
