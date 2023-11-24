/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2021 Lutz Donnerhacke
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#ifndef _UTIL_H
#define _UTIL_H

/* common ip ranges */
extern struct in_addr masq, pub, prv1, prv2, prv3, cgn, ext, ANY_ADDR;

int		randcmp(const void *a, const void *b);
void		hexdump(void *p, size_t len);
struct ip *	ip_packet(u_char protocol, size_t len);
struct udphdr * set_udp(struct ip *p, u_short sport, u_short dport);

static inline int
addr_eq(struct in_addr a, struct in_addr b)
{
	return a.s_addr == b.s_addr;
}

#define a2h(a)	ntohl(a.s_addr)

static inline int
rand_range(int min, int max)
{
	return min + rand()%(max - min);
}

#define NAT_CHECK(pip, src, dst, msq)	do {	\
	int res;				\
	int len = ntohs(pip->ip_len);		\
	pip->ip_src = src;			\
	pip->ip_dst = dst;			\
	res = LibAliasOut(la, pip, len);	\
	ATF_CHECK_MSG(res == PKT_ALIAS_OK,	\
	    ">%d< not met PKT_ALIAS_OK", res);	\
	ATF_CHECK(addr_eq(msq, pip->ip_src));	\
	ATF_CHECK(addr_eq(dst, pip->ip_dst));	\
} while(0)

#define NAT_FAIL(pip, src, dst)	do {		\
	int res;				\
	int len = ntohs(pip->ip_len);		\
	pip->ip_src = src;			\
	pip->ip_dst = dst;			\
	res = LibAliasOut(la, pip, len);	\
	ATF_CHECK_MSG(res != PKT_ALIAS_OK,	\
	    ">%d< not met !PKT_ALIAS_OK", res);	\
	ATF_CHECK(addr_eq(src, pip->ip_src));	\
	ATF_CHECK(addr_eq(dst, pip->ip_dst));	\
} while(0)

#define UNNAT_CHECK(pip, src, dst, rel)	do {	\
	int res;				\
	int len = ntohs(pip->ip_len);		\
	pip->ip_src = src;			\
	pip->ip_dst = dst;			\
	res = LibAliasIn(la, pip, len);		\
	ATF_CHECK_MSG(res == PKT_ALIAS_OK,	\
	    ">%d< not met PKT_ALIAS_OK", res);	\
	ATF_CHECK(addr_eq(src, pip->ip_src));	\
	ATF_CHECK(addr_eq(rel, pip->ip_dst));	\
} while(0)

#define UNNAT_FAIL(pip, src, dst)	do {	\
	int res;				\
	int len = ntohs(pip->ip_len);		\
	pip->ip_src = src;			\
	pip->ip_dst = dst;			\
	res = LibAliasIn(la, pip, len);		\
	ATF_CHECK_MSG(res != PKT_ALIAS_OK,	\
	    ">%d< not met !PKT_ALIAS_OK", res);	\
	ATF_CHECK(addr_eq(src, pip->ip_src));	\
	ATF_CHECK(addr_eq(dst, pip->ip_dst));	\
} while(0)

#define UDP_NAT_CHECK(p, u, si, sp, di, dp, mi)	do {	\
	u = set_udp(p, (sp), (dp));			\
	NAT_CHECK(p, (si), (di), (mi));			\
	ATF_CHECK(u->uh_dport == htons(dp));		\
} while(0)

#define UDP_NAT_FAIL(p, u, si, sp, di, dp)	do {	\
	u = set_udp(p, (sp), (dp));			\
	NAT_FAIL(p, (si), (di));			\
} while(0)

#define UDP_UNNAT_CHECK(p, u, si, sp, mi, mp, di, dp)	\
do {							\
	u = set_udp(p, (sp), (mp));			\
	UNNAT_CHECK(p, (si), (mi), (di));		\
	ATF_CHECK(u->uh_sport == htons(sp));		\
	ATF_CHECK(u->uh_dport == htons(dp));		\
} while(0)

#define UDP_UNNAT_FAIL(p, u, si, sp, mi, mp)	do {	\
	u = set_udp(p, (sp), (mp));			\
	UNNAT_FAIL(p, (si), (mi));			\
} while(0)

#endif /* _UTIL_H */
