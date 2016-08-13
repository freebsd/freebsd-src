/*-
 * Copyright (c) 2015-2016 Yandex LLC
 * Copyright (c) 2015-2016 Andrey V. Elsukov <ae@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_IP_FW_NAT64_TRANSLATE_H_
#define	_IP_FW_NAT64_TRANSLATE_H_

#ifdef RTALLOC_NOLOCK
#define	IN_LOOKUP_ROUTE(ro, fib)	rtalloc_fib_nolock((ro), 0, (fib))
#define	IN6_LOOKUP_ROUTE(ro, fib)	in6_rtalloc_nolock((ro), (fib))
#define	FREE_ROUTE(ro)
#else
#define	IN_LOOKUP_ROUTE(ro, fib)	rtalloc_ign_fib((ro), 0, (fib))
#define	IN6_LOOKUP_ROUTE(ro, fib)	in6_rtalloc((ro), (fib))
#define	FREE_ROUTE(ro)			RO_RTFREE((ro))
#endif

static inline int
nat64_check_ip6(struct in6_addr *addr)
{

	/* XXX: We should really check /8 */
	if (addr->s6_addr16[0] == 0 || /* 0000::/8 Reserved by IETF */
	    IN6_IS_ADDR_MULTICAST(addr) || IN6_IS_ADDR_LINKLOCAL(addr))
		return (1);
	return (0);
}

extern int nat64_allow_private;
static inline int
nat64_check_private_ip4(in_addr_t ia)
{

	if (nat64_allow_private)
		return (0);
	/* WKPFX must not be used to represent non-global IPv4 addresses */
//	if (cfg->flags & NAT64_WKPFX) {
		/* IN_PRIVATE */
		if ((ia & htonl(0xff000000)) == htonl(0x0a000000) ||
		    (ia & htonl(0xfff00000)) == htonl(0xac100000) ||
		    (ia & htonl(0xffff0000)) == htonl(0xc0a80000))
			return (1);
		/*
		 * RFC 5735:
		 *  192.0.0.0/24 - reserved for IETF protocol assignments
		 *  192.88.99.0/24 - for use as 6to4 relay anycast addresses
		 *  198.18.0.0/15 - for use in benchmark tests
		 *  192.0.2.0/24, 198.51.100.0/24, 203.0.113.0/24 - for use
		 *   in documentation and example code
		 */
		if ((ia & htonl(0xffffff00)) == htonl(0xc0000000) ||
		    (ia & htonl(0xffffff00)) == htonl(0xc0586300) ||
		    (ia & htonl(0xfffffe00)) == htonl(0xc6120000) ||
		    (ia & htonl(0xffffff00)) == htonl(0xc0000200) ||
		    (ia & htonl(0xfffffe00)) == htonl(0xc6336400) ||
		    (ia & htonl(0xffffff00)) == htonl(0xcb007100))
			return (1);
//	}
	return (0);
}

static inline int
nat64_check_ip4(in_addr_t ia)
{

	/* IN_LOOPBACK */
	if ((ia & htonl(0xff000000)) == htonl(0x7f000000))
		return (1);
	/* IN_LINKLOCAL */
	if ((ia & htonl(0xffff0000)) == htonl(0xa9fe0000))
		return (1);
	/* IN_MULTICAST & IN_EXPERIMENTAL */
	if ((ia & htonl(0xe0000000)) == htonl(0xe0000000))
		return (1);
	return (0);
}

#define	nat64_get_ip4(_ip6)		((_ip6)->s6_addr32[3])
#define	nat64_set_ip4(_ip6, _ip4)	(_ip6)->s6_addr32[3] = (_ip4)

int nat64_getlasthdr(struct mbuf *m, int *offset);
int nat64_do_handle_ip4(struct mbuf *m, struct in6_addr *saddr,
    struct in6_addr *daddr, uint16_t lport, nat64_stats_block *stats,
    void *logdata);
int nat64_do_handle_ip6(struct mbuf *m, uint32_t aaddr, uint16_t aport,
    nat64_stats_block *stats, void *logdata);
int nat64_handle_icmp6(struct mbuf *m, int hlen, uint32_t aaddr, uint16_t aport,
    nat64_stats_block *stats, void *logdata);

#endif

