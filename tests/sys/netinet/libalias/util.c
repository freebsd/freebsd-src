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
#include <stdio.h>
#include <stdlib.h>

#include <netinet/in.h>

#include "util.h"

/* common ip ranges */
struct in_addr masq = { htonl(0x01020304) };
struct in_addr pub  = { htonl(0x0102dead) };
struct in_addr prv1 = { htonl(0x0a00dead) };
struct in_addr prv2 = { htonl(0xac10dead) };
struct in_addr prv3 = { htonl(0xc0a8dead) };
struct in_addr cgn  = { htonl(0x6440dead) };
struct in_addr ext  = { htonl(0x12345678) };
struct in_addr ANY_ADDR = { 0 };

#define REQUIRE(x)	do {				\
	if (!(x)) {					\
		fprintf(stderr, "Failed in %s %s:%d.\n",\
		    __FUNCTION__, __FILE__, __LINE__);	\
		exit(-1);				\
	}						\
} while(0)

int
randcmp(const void *a, const void *b)
{
	int res, r = rand();

	(void)a;
	(void)b;
	res = (r/4 < RAND_MAX/9) ? 1
	    : (r/5 < RAND_MAX/9) ? 0
	    : -1;
	return (res);
}

void
hexdump(void *p, size_t len)
{
	size_t i;
	unsigned char *c = p;
	
	for (i = 0; i < len; i++) {
		printf(" %02x", c[i]);
		switch (i & 0xf) {
		case 0xf: printf("\n"); break;
		case 0x7: printf(" "); break;
		default:  break;
		}
	}
	if ((i & 0xf) != 0x0)
		printf("\n");
}

struct ip *
ip_packet(u_char protocol, size_t len)
{
	struct ip * p;

	REQUIRE(len >= 64 && len <= IP_MAXPACKET);

	p = calloc(1, len);
	REQUIRE(p != NULL);

	p->ip_v = IPVERSION;
	p->ip_hl = sizeof(*p)/4;
	p->ip_len = htons(len);
	p->ip_ttl = IPDEFTTL;
	p->ip_p = protocol;
	REQUIRE(p->ip_hl == 5);

	return (p);
}

struct udphdr *
set_udp(struct ip *p, u_short sport, u_short dport) {
	int hlen = p->ip_hl << 2;
	struct udphdr *u = (struct udphdr *)((uintptr_t)p + hlen);
	int payload = ntohs(p->ip_len) - hlen;

	REQUIRE(payload >= (int)sizeof(*u));
	p->ip_p = IPPROTO_UDP;
	u->uh_sport = htons(sport);
	u->uh_dport = htons(dport);
	u->uh_ulen = htons(payload);
	return (u);
}
