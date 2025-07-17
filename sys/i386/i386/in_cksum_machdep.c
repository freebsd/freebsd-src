/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <machine/in_cksum.h>

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
 * These asm statements require __volatile because they pass information
 * via the condition codes.  GCC does not currently provide a way to specify
 * the condition codes as an input or output operand.
 *
 * The LOAD macro below is effectively a prefetch into cache.  GCC will
 * load the value into a register but will not use it.  Since modern CPUs
 * reorder operations, this will generally take place in parallel with
 * other calculations.
 */
u_short
in_cksum_skip(struct mbuf *m, int len, int skip)
{
	u_short *w;
	unsigned sum = 0;
	int mlen = 0;
	int byte_swapped = 0;
	union { char	c[2]; u_short	s; } su;

	len -= skip;
	for (; skip && m; m = m->m_next) {
		if (m->m_len > skip) {
			mlen = m->m_len - skip;
			w = (u_short *)(mtod(m, u_char *) + skip);
			goto skip_start;
		} else {
			skip -= m->m_len;
		}
	}

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
skip_start:
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
		 * Advance to a 486 cache line boundary.
		 */
		if (4 & (int) w && mlen >= 4) {
			__asm __volatile (
				"addl %1, %0\n"
				"adcl $0, %0"
				: "+r" (sum)
				: "g" (((const u_int32_t *)w)[0])
			);
			w += 2;
			mlen -= 4;
		}
		if (8 & (int) w && mlen >= 8) {
			__asm __volatile (
				"addl %1, %0\n"
				"adcl %2, %0\n"
				"adcl $0, %0"
				: "+r" (sum)
				: "g" (((const u_int32_t *)w)[0]),
				  "g" (((const u_int32_t *)w)[1])
			);
			w += 4;
			mlen -= 8;
		}
		/*
		 * Do as much of the checksum as possible 32 bits at at time.
		 * In fact, this loop is unrolled to make overhead from
		 * branches &c small.
		 */
		mlen -= 1;
		while ((mlen -= 32) >= 0) {
			/*
			 * Add with carry 16 words and fold in the last
			 * carry by adding a 0 with carry.
			 *
			 * The early ADD(16) and the LOAD(32) are to load
			 * the next 2 cache lines in advance on 486's.  The
			 * 486 has a penalty of 2 clock cycles for loading
			 * a cache line, plus whatever time the external
			 * memory takes to load the first word(s) addressed.
			 * These penalties are unavoidable.  Subsequent
			 * accesses to a cache line being loaded (and to
			 * other external memory?) are delayed until the
			 * whole load finishes.  These penalties are mostly
			 * avoided by not accessing external memory for
			 * 8 cycles after the ADD(16) and 12 cycles after
			 * the LOAD(32).  The loop terminates when mlen
			 * is initially 33 (not 32) to guaranteed that
			 * the LOAD(32) is within bounds.
			 */
			__asm __volatile (
				"addl %1, %0\n"
				"adcl %2, %0\n"
				"adcl %3, %0\n"
				"adcl %4, %0\n"
				"adcl %5, %0\n"
				"mov  %6, %%eax\n"
				"adcl %7, %0\n"
				"adcl %8, %0\n"
				"adcl %9, %0\n"
				"adcl $0, %0"
				: "+r" (sum)
				: "g" (((const u_int32_t *)w)[4]),
				  "g" (((const u_int32_t *)w)[0]),
				  "g" (((const u_int32_t *)w)[1]),
				  "g" (((const u_int32_t *)w)[2]),
				  "g" (((const u_int32_t *)w)[3]),
				  "g" (((const u_int32_t *)w)[8]),
				  "g" (((const u_int32_t *)w)[5]),
				  "g" (((const u_int32_t *)w)[6]),
				  "g" (((const u_int32_t *)w)[7])
				: "eax"
			);
			w += 16;
		}
		mlen += 32 + 1;
		if (mlen >= 32) {
			__asm __volatile (
				"addl %1, %0\n"
				"adcl %2, %0\n"
				"adcl %3, %0\n"
				"adcl %4, %0\n"
				"adcl %5, %0\n"
				"adcl %6, %0\n"
				"adcl %7, %0\n"
				"adcl %8, %0\n"
				"adcl $0, %0"
				: "+r" (sum)
				: "g" (((const u_int32_t *)w)[4]),
				  "g" (((const u_int32_t *)w)[0]),
				  "g" (((const u_int32_t *)w)[1]),
				  "g" (((const u_int32_t *)w)[2]),
				  "g" (((const u_int32_t *)w)[3]),
				  "g" (((const u_int32_t *)w)[5]),
				  "g" (((const u_int32_t *)w)[6]),
				  "g" (((const u_int32_t *)w)[7])
			);
			w += 16;
			mlen -= 32;
		}
		if (mlen >= 16) {
			__asm __volatile (
				"addl %1, %0\n"
				"adcl %2, %0\n"
				"adcl %3, %0\n"
				"adcl %4, %0\n"
				"adcl $0, %0"
				: "+r" (sum)
				: "g" (((const u_int32_t *)w)[0]),
				  "g" (((const u_int32_t *)w)[1]),
				  "g" (((const u_int32_t *)w)[2]),
				  "g" (((const u_int32_t *)w)[3])
			);
			w += 8;
			mlen -= 16;
		}
		if (mlen >= 8) {
			__asm __volatile (
				"addl %1, %0\n"
				"adcl %2, %0\n"
				"adcl $0, %0"
				: "+r" (sum)
				: "g" (((const u_int32_t *)w)[0]),
				  "g" (((const u_int32_t *)w)[1])
			);
			w += 4;
			mlen -= 8;
		}
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
			 * There could be a word split between
			 * this mbuf and the next mbuf.
			 * Save the last byte (to prepend to next mbuf).
			 */
			su.c[0] = *(char *)w;
	}

	if (len)
		printf("%s: out of data by %d\n", __func__, len);
	if (mlen == -1) {
		/* The last mbuf has odd # of bytes. Follow the
		   standard (the odd byte is shifted left by 8 bits) */
		su.c[1] = 0;
		sum += su.s;
	}
	REDUCE;
	return (~sum & 0xffff);
}
