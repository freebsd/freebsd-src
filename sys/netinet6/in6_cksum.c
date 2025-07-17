/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$KAME: in6_cksum.c,v 1.10 2000/12/03 00:53:59 itojun Exp $
 */

/*-
 * Copyright (c) 1988, 1992, 1993
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
 */

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/scope6_var.h>

/*
 * Checksum routine for Internet Protocol family headers (Portable Version).
 *
 * This routine is very heavily used in the network
 * code and should be modified for each CPU to be as fast as possible.
 */

#define ADDCARRY(x)  (x > 65535 ? x -= 65535 : x)
#define REDUCE {l_util.l = sum; sum = l_util.s[0] + l_util.s[1]; (void)ADDCARRY(sum);}

union l_util {
	uint16_t	s[2];
	uint32_t	l;
};

union s_util {
	uint8_t		c[2];
	uint16_t	s;
};

static int
_in6_cksum_pseudo(struct ip6_hdr *ip6, uint32_t len, uint8_t nxt, uint16_t csum)
{
	int sum;
	uint16_t scope, *w;
	union {
		u_int16_t phs[4];
		struct {
			u_int32_t	ph_len;
			u_int8_t	ph_zero[3];
			u_int8_t	ph_nxt;
		} __packed ph;
	} uph;

	sum = csum;

	/*
	 * First create IP6 pseudo header and calculate a summary.
	 */
	uph.ph.ph_len = htonl(len);
	uph.ph.ph_zero[0] = uph.ph.ph_zero[1] = uph.ph.ph_zero[2] = 0;
	uph.ph.ph_nxt = nxt;

	/* Payload length and upper layer identifier. */
	sum += uph.phs[0];  sum += uph.phs[1];
	sum += uph.phs[2];  sum += uph.phs[3];

	/* IPv6 source address. */
	scope = in6_getscope(&ip6->ip6_src);
	w = (u_int16_t *)&ip6->ip6_src;
	sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3];
	sum += w[4]; sum += w[5]; sum += w[6]; sum += w[7];
	if (scope != 0)
		sum -= scope;

	/* IPv6 destination address. */
	scope = in6_getscope(&ip6->ip6_dst);
	w = (u_int16_t *)&ip6->ip6_dst;
	sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3];
	sum += w[4]; sum += w[5]; sum += w[6]; sum += w[7];
	if (scope != 0)
		sum -= scope;

	return (sum);
}

int
in6_cksum_pseudo(struct ip6_hdr *ip6, uint32_t len, uint8_t nxt, uint16_t csum)
{
	union l_util l_util;
	int sum;

	sum = _in6_cksum_pseudo(ip6, len, nxt, csum);
	REDUCE;
	return (sum);
}

static int
in6_cksumdata(void *data, int *lenp, uint8_t *residp, int rlen)
{
	union l_util l_util;
	union s_util s_util;
	uint16_t *w;
	int len, sum;
	bool byte_swapped;

	KASSERT(*lenp >= 0, ("%s: negative len %d", __func__, *lenp));
	KASSERT(rlen == 0 || rlen == 1, ("%s: rlen %d", __func__, rlen));

	len = *lenp;
	sum = 0;

	if (len == 0) {
		len = rlen;
		goto out;
	}

	byte_swapped = false;
	w = data;

	/*
	 * Do we have a residual byte left over from the previous buffer?
	 */
	if (rlen == 1) {
		s_util.c[0] = *residp;
		s_util.c[1] = *(uint8_t *)w;
		sum += s_util.s;
		w = (uint16_t *)((uint8_t *)w + 1);
		len--;
		rlen = 0;
	}

	/*
	 * Force to even boundary.
	 */
	if ((1 & (uintptr_t)w) && len > 0) {
		REDUCE;
		sum <<= 8;
		s_util.c[0] = *(uint8_t *)w;
		w = (uint16_t *)((uint8_t *)w + 1);
		len--;
		byte_swapped = true;
	}

	/*
	 * Unroll the loop to make overhead from branches &c small.
	 */
	while ((len -= 32) >= 0) {
		sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3];
		sum += w[4]; sum += w[5]; sum += w[6]; sum += w[7];
		sum += w[8]; sum += w[9]; sum += w[10]; sum += w[11];
		sum += w[12]; sum += w[13]; sum += w[14]; sum += w[15];
		w += 16;
	}
	len += 32;
	while ((len -= 8) >= 0) {
		sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3];
		w += 4;
	}
	len += 8;
	if (len == 0 && !byte_swapped)
		goto out;
	REDUCE;
	while ((len -= 2) >= 0) {
		sum += *w++;
	}
	if (byte_swapped) {
		REDUCE;
		sum <<= 8;
		if (len == -1) {
			s_util.c[1] = *(uint8_t *)w;
			sum += s_util.s;
		} else /* len == -2 */
			*residp = s_util.c[0];
		len++;
	} else if (len == -1)
		*residp = *(uint8_t *)w;
out:
	*lenp = len & 1;
	return (sum);
}

struct in6_cksum_partial_arg {
	int	sum;
	int	rlen;
	uint8_t	resid;
};

static int
in6_cksum_partial_one(void *_arg, void *data, u_int len)
{
	struct in6_cksum_partial_arg *arg = _arg;

	arg->sum += in6_cksumdata(data, &len, &arg->resid, arg->rlen);
	arg->rlen = len;
	return (0);
}

/*
 * m MUST contain a contiguous IP6 header.
 * off_l3 is an offset where ipv6 header starts.
 * off_l4 is an offset where TCP/UDP/ICMP6 header starts.
 * len is a total length of a transport segment.
 * (e.g. TCP header + TCP payload)
 * cov is the number of bytes to be taken into account for the checksum
 */
int
in6_cksum_partial_l2(struct mbuf *m, uint8_t nxt, uint32_t off_l3,
    uint32_t off_l4, uint32_t len, uint32_t cov)
{
	struct in6_cksum_partial_arg arg;
	union l_util l_util;
	union s_util s_util;
	struct ip6_hdr *ip6;
	uint16_t *w, scope;
	int sum;
	union {
		uint16_t phs[4];
		struct {
			uint32_t	ph_len;
			uint8_t		ph_zero[3];
			uint8_t		ph_nxt;
		} __packed ph;
	} uph;

	/* Sanity check. */
	KASSERT(m->m_pkthdr.len >= off_l4 + len,
	    ("%s: mbuf len (%d) < off(%d)+len(%d)",
	    __func__, m->m_pkthdr.len, off_l4, len));
	KASSERT(m->m_len >= off_l3 + sizeof(*ip6),
	    ("%s: mbuf len %d < sizeof(ip6)", __func__, m->m_len));

	/*
	 * First create IP6 pseudo header and calculate a summary.
	 */
	uph.ph.ph_len = htonl(len);
	uph.ph.ph_zero[0] = uph.ph.ph_zero[1] = uph.ph.ph_zero[2] = 0;
	uph.ph.ph_nxt = nxt;

	/* Payload length and upper layer identifier. */
	sum = uph.phs[0];  sum += uph.phs[1];
	sum += uph.phs[2];  sum += uph.phs[3];

	ip6 = mtodo(m, off_l3);

	/* IPv6 source address. */
	scope = in6_getscope(&ip6->ip6_src);
	w = (uint16_t *)&ip6->ip6_src;
	sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3];
	sum += w[4]; sum += w[5]; sum += w[6]; sum += w[7];
	if (scope != 0)
		sum -= scope;

	/* IPv6 destination address. */
	scope = in6_getscope(&ip6->ip6_dst);
	w = (uint16_t *)&ip6->ip6_dst;
	sum += w[0]; sum += w[1]; sum += w[2]; sum += w[3];
	sum += w[4]; sum += w[5]; sum += w[6]; sum += w[7];
	if (scope != 0)
		sum -= scope;

	/*
	 * Loop over the rest of the mbuf chain and compute the rest of the
	 * checksum.  m_apply() handles unmapped mbufs.
	 */
	arg.sum = sum;
	arg.rlen = 0;
	(void)m_apply(m, off_l4, cov, in6_cksum_partial_one, &arg);
	sum = arg.sum;

	/*
	 * Handle a residual byte.
	 */
	if (arg.rlen == 1) {
		s_util.c[0] = arg.resid;
		s_util.c[1] = 0;
		sum += s_util.s;
	}
	REDUCE;
	return (~sum & 0xffff);
}

int
in6_cksum_partial(struct mbuf *m, uint8_t nxt, uint32_t off, uint32_t len,
    uint32_t cov)
{
	return (in6_cksum_partial_l2(m, nxt, 0, off, len, cov));
}

int
in6_cksum(struct mbuf *m, uint8_t nxt, uint32_t off, uint32_t len)
{
	return (in6_cksum_partial(m, nxt, off, len, len));
}
