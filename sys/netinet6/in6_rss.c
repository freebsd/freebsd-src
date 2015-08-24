/*-
 * Copyright (c) 2010-2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert N. M. Watson under contract
 * to Juniper Networks, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#include "opt_inet6.h"
#include "opt_pcbgroup.h"

#ifndef PCBGROUP
#error "options RSS depends on options PCBGROUP"
#endif

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/priv.h>
#include <sys/kernel.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/sbuf.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/netisr.h>
#include <net/rss_config.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_rss.h>
#include <netinet/in_var.h>

/* for software rss hash support */
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

/*
 * Hash an IPv6 2-tuple.
 */
uint32_t
rss_hash_ip6_2tuple(const struct in6_addr *src, const struct in6_addr *dst)
{
	uint8_t data[sizeof(*src) + sizeof(*dst)];
	u_int datalen;

	datalen = 0;
	bcopy(src, &data[datalen], sizeof(*src));
	datalen += sizeof(*src);
	bcopy(dst, &data[datalen], sizeof(*dst));
	datalen += sizeof(*dst);
	return (rss_hash(datalen, data));
}

/*
 * Hash an IPv6 4-tuple.
 */
uint32_t
rss_hash_ip6_4tuple(const struct in6_addr *src, u_short srcport,
    const struct in6_addr *dst, u_short dstport)
{
	uint8_t data[sizeof(*src) + sizeof(*dst) + sizeof(srcport) +
	    sizeof(dstport)];
	u_int datalen;

	datalen = 0;
	bcopy(src, &data[datalen], sizeof(*src));
	datalen += sizeof(*src);
	bcopy(dst, &data[datalen], sizeof(*dst));
	datalen += sizeof(*dst);
	bcopy(&srcport, &data[datalen], sizeof(srcport));
	datalen += sizeof(srcport);
	bcopy(&dstport, &data[datalen], sizeof(dstport));
	datalen += sizeof(dstport);
	return (rss_hash(datalen, data));
}

/*
 * Calculate an appropriate ipv6 2-tuple or 4-tuple given the given
 * IPv6 source/destination address, UDP or TCP source/destination ports
 * and the protocol type.
 *
 * The protocol code may wish to do a software hash of the given
 * tuple.  This depends upon the currently configured RSS hash types.
 *
 * This assumes that the packet in question isn't a fragment.
 *
 * It also assumes the packet source/destination address
 * are in "incoming" packet order (ie, source is "far" address.)
 */
int
rss_proto_software_hash_v6(const struct in6_addr *s, const struct in6_addr *d,
    u_short sp, u_short dp, int proto,
    uint32_t *hashval, uint32_t *hashtype)
{
	uint32_t hash;

	/*
	 * Next, choose the hash type depending upon the protocol
	 * identifier.
	 */
	if ((proto == IPPROTO_TCP) &&
	    (rss_gethashconfig() & RSS_HASHTYPE_RSS_TCP_IPV6)) {
		hash = rss_hash_ip6_4tuple(s, sp, d, dp);
		*hashval = hash;
		*hashtype = M_HASHTYPE_RSS_TCP_IPV6;
		return (0);
	} else if ((proto == IPPROTO_UDP) &&
	    (rss_gethashconfig() & RSS_HASHTYPE_RSS_UDP_IPV6)) {
		hash = rss_hash_ip6_4tuple(s, sp, d, dp);
		*hashval = hash;
		*hashtype = M_HASHTYPE_RSS_UDP_IPV6;
		return (0);
	} else if (rss_gethashconfig() & RSS_HASHTYPE_RSS_IPV6) {
		/* RSS doesn't hash on other protocols like SCTP; so 2-tuple */
		hash = rss_hash_ip6_2tuple(s, d);
		*hashval = hash;
		*hashtype = M_HASHTYPE_RSS_IPV6;
		return (0);
	}

	/* No configured available hashtypes! */
	printf("%s: no available hashtypes!\n", __func__);
	return (-1);
}
