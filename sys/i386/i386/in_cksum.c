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
 *	$Id: in_cksum.c,v 1.2 1993/10/16 14:15:00 rgrimes Exp $
 */

#include "param.h"
#include "sys/mbuf.h"

/*
 * Checksum routine for Internet Protocol family headers.
 *
 * This routine is very heavily used in the network
 * code and should be modified for each CPU to be as fast as possible.
 * 
 * This implementation is 386 version.
 */

#undef	ADDCARRY
#define ADDCARRY(x)     if ((x) > 0xffff) (x) -= 0xffff
#define REDUCE          {sum = (sum & 0xffff) + (sum >> 16); ADDCARRY(sum);}

/*
 * Thanks to gcc we don't have to guess
 * which registers contain sum & w.
 */
#define CLC     asm("clc")
#define ADD(n)  asm("adcl " #n "(%2), %0": "=r"(sum): "0"(sum), "r"(w))
#define MOP     asm("adcl $0, %0":         "=r"(sum): "0"(sum))

in_cksum(m, len)
	register struct mbuf *m;
	register int len;
{
	register u_short *w;
	register unsigned sum = 0;
	register int mlen = 0;
	int byte_swapped = 0;
	union { char	c[2]; u_short	s; } su;

	for (;m && len; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		w = mtod(m, u_short *);
		if (mlen == -1) {
			/*
			 * The first byte of this mbuf is the continuation
			 * of a word spanning between this mbuf and the
			 * last mbuf.
			 */

			/* su.c[0] is already saved when scanning previous 
			 * mbuf.  sum was REDUCEd when we found mlen == -1
			 */
			su.c[1] = *(u_char *)w;
			sum += su.s;
			w = (u_short *)((char *)w + 1);
			mlen = m->m_len - 1;
			len--;
		} else
			mlen = m->m_len;
		if (len < mlen)
			mlen = len;
		len -= mlen;
		/*
		 * Force to long boundary so we do longword aligned
		 * memory operations
		 */
		if (3 & (int) w) {
			REDUCE;
			if ((1 & (int) w) && (mlen > 0)) {
				sum <<= 8;
				su.c[0] = *(char *)w;
				w = (u_short *)((char *)w + 1);
				mlen--;
				byte_swapped = 1;
			}
			if ((2 & (int) w) && (mlen >= 2)) {
				sum += *w++;
				mlen -= 2;
			}
		}
		/*
		 * Do as much of the checksum as possible 32 bits at at time.
		 * In fact, this loop is unrolled to make overhead from
		 * branches &c small.
		 */
		while ((mlen -= 32) >= 0) {
			/*
			 * Clear the carry flag, add with carry 16 words
			 * and fold-in last carry by adding a 0 with carry.
			 */
			CLC;
			ADD(0);  ADD(4);  ADD(8);  ADD(12);
			ADD(16); ADD(20); ADD(24); ADD(28);
			MOP; w += 16;
		}
		mlen += 32;
		while ((mlen -= 8) >= 0) {
			CLC;
			ADD(0); ADD(4);
			MOP;
			w += 4;
		}
		mlen += 8;
		if (mlen == 0 && byte_swapped == 0)
			continue;       /* worth 1% maybe ?? */
		REDUCE;
		while ((mlen -= 2) >= 0) {
			sum += *w++;
		}
		if (byte_swapped) {
			sum <<= 8;
			byte_swapped = 0;
			if (mlen == -1) {
				su.c[1] = *(char *)w;
				sum += su.s;
				mlen = 0;
			} else
				mlen = -1;
		} else if (mlen == -1)
			/*
			 * This mbuf has odd number of bytes.
			 * There could be a word split betwen
			 * this mbuf and the next mbuf.
			 * Save the last byte (to prepend to next mbuf).
			 */
			su.c[0] = *(char *)w;
	}

	if (len)
		printf("cksum: out of data\n");
	if (mlen == -1) {
		/* The last mbuf has odd # of bytes. Follow the
		   standard (the odd byte is shifted left by 8 bits) */
		su.c[1] = 0;
		sum += su.s;
	}
	REDUCE;
	return (~sum & 0xffff);
}

