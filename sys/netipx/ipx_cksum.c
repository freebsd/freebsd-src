/*
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1982, 1992, 1993
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
 *	@(#)ipx_cksum.c
 *
 * $FreeBSD: src/sys/netipx/ipx_cksum.c,v 1.9 1999/08/28 18:21:53 jhay Exp $
 */

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/libkern.h>

#include <netipx/ipx.h>
#include <netipx/ipx_var.h>


#define SUMADV	sum += *w++

u_short
ipx_cksum(struct mbuf *m, int len) {
	u_int32_t sum = 0;
	u_short *w;
	u_char oldtc;
	int mlen, words;
	struct ipx *ipx;
	union {
		u_char	b[2];
		u_short	w;
	} buf;

	ipx = mtod(m, struct ipx*);
	oldtc = ipx->ipx_tc;
	ipx->ipx_tc = 0;
	w = &ipx->ipx_len;
	len -= 2;
	mlen = 2;

	for(;;) {
		mlen = imin(m->m_len - mlen, len);
		words = mlen / 2;
		len -= mlen & ~1;
		while (words >= 16) {
			SUMADV;	SUMADV;	SUMADV;	SUMADV;
			SUMADV;	SUMADV;	SUMADV;	SUMADV;
			SUMADV;	SUMADV;	SUMADV;	SUMADV;
			SUMADV;	SUMADV;	SUMADV;	SUMADV;
			words -= 16;
		}
		while (words--)
			SUMADV;
		if (len == 0)
			break;
		mlen &= 1;
		if (mlen) {
			buf.b[0] = *(u_char*)w;
			if (--len == 0) {
				buf.b[1] = 0;
				sum += buf.w;
				break;
			}
		}
		m = m->m_next;
		if (m == NULL)
			break;
		w = mtod(m, u_short*);
		if (mlen) {
			buf.b[1] = *(u_char*)w;
			sum += buf.w;
			((u_char*)w)++;
			if (--len == 0)
				break;
		} 
	}

	ipx->ipx_tc = oldtc;

	sum = (sum & 0xffff) + (sum >> 16);
	if (sum >= 0x10000)
		sum++;
	if (sum)
		sum = ~sum;
	return (sum);
}
