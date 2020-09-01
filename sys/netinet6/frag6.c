/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * Copyright (c) 2019 Netflix, Inc.
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
 *	$KAME: frag6.c,v 1.33 2002/01/07 11:34:48 kjc Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_rss.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/eventhandler.h>
#include <sys/hash.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet/in_systm.h>	/* For ECN definitions. */
#include <netinet/ip.h>		/* For ECN definitions. */

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif

/*
 * A "big picture" of how IPv6 fragment queues are all linked together.
 *
 * struct ip6qbucket ip6qb[...];			hashed buckets
 * ||||||||
 * |
 * +--- TAILQ(struct ip6q, packets) *q6;		tailq entries holding
 *      ||||||||					fragmented packets
 *      |						(1 per original packet)
 *      |
 *      +--- TAILQ(struct ip6asfrag, ip6q_frags) *af6;	tailq entries of IPv6
 *           |                                   *ip6af;fragment packets
 *           |						for one original packet
 *           + *mbuf
 */

/* Reassembly headers are stored in hash buckets. */
#define	IP6REASS_NHASH_LOG2	10
#define	IP6REASS_NHASH		(1 << IP6REASS_NHASH_LOG2)
#define	IP6REASS_HMASK		(IP6REASS_NHASH - 1)

TAILQ_HEAD(ip6qhead, ip6q);
struct ip6qbucket {
	struct ip6qhead	packets;
	struct mtx	lock;
	int		count;
};

struct ip6asfrag {
	TAILQ_ENTRY(ip6asfrag) ip6af_tq;
	struct mbuf	*ip6af_m;
	int		ip6af_offset;	/* Offset in ip6af_m to next header. */
	int		ip6af_frglen;	/* Fragmentable part length. */
	int		ip6af_off;	/* Fragment offset. */
	bool		ip6af_mff;	/* More fragment bit in frag off. */
};

static MALLOC_DEFINE(M_FRAG6, "frag6", "IPv6 fragment reassembly header");

#ifdef VIMAGE
/* A flag to indicate if IPv6 fragmentation is initialized. */
VNET_DEFINE_STATIC(bool,		frag6_on);
#define	V_frag6_on			VNET(frag6_on)
#endif

/* System wide (global) maximum and count of packets in reassembly queues. */
static int ip6_maxfrags;
static volatile u_int frag6_nfrags = 0;

/* Maximum and current packets in per-VNET reassembly queue. */
VNET_DEFINE_STATIC(int,			ip6_maxfragpackets);
VNET_DEFINE_STATIC(volatile u_int,	frag6_nfragpackets);
#define	V_ip6_maxfragpackets		VNET(ip6_maxfragpackets)
#define	V_frag6_nfragpackets		VNET(frag6_nfragpackets)

/* Maximum per-VNET reassembly queues per bucket and fragments per packet. */
VNET_DEFINE_STATIC(int,			ip6_maxfragbucketsize);
VNET_DEFINE_STATIC(int,			ip6_maxfragsperpacket);
#define	V_ip6_maxfragbucketsize		VNET(ip6_maxfragbucketsize)
#define	V_ip6_maxfragsperpacket		VNET(ip6_maxfragsperpacket)

/* Per-VNET reassembly queue buckets. */
VNET_DEFINE_STATIC(struct ip6qbucket,	ip6qb[IP6REASS_NHASH]);
VNET_DEFINE_STATIC(uint32_t,		ip6qb_hashseed);
#define	V_ip6qb				VNET(ip6qb)
#define	V_ip6qb_hashseed		VNET(ip6qb_hashseed)

#define	IP6QB_LOCK(_b)		mtx_lock(&V_ip6qb[(_b)].lock)
#define	IP6QB_TRYLOCK(_b)	mtx_trylock(&V_ip6qb[(_b)].lock)
#define	IP6QB_LOCK_ASSERT(_b)	mtx_assert(&V_ip6qb[(_b)].lock, MA_OWNED)
#define	IP6QB_UNLOCK(_b)	mtx_unlock(&V_ip6qb[(_b)].lock)
#define	IP6QB_HEAD(_b)		(&V_ip6qb[(_b)].packets)

/*
 * By default, limit the number of IP6 fragments across all reassembly
 * queues to  1/32 of the total number of mbuf clusters.
 *
 * Limit the total number of reassembly queues per VNET to the
 * IP6 fragment limit, but ensure the limit will not allow any bucket
 * to grow above 100 items. (The bucket limit is
 * IP_MAXFRAGPACKETS / (IPREASS_NHASH / 2), so the 50 is the correct
 * multiplier to reach a 100-item limit.)
 * The 100-item limit was chosen as brief testing seems to show that
 * this produces "reasonable" performance on some subset of systems
 * under DoS attack.
 */
#define	IP6_MAXFRAGS		(nmbclusters / 32)
#define	IP6_MAXFRAGPACKETS	(imin(IP6_MAXFRAGS, IP6REASS_NHASH * 50))

/*
 * Sysctls and helper function.
 */
SYSCTL_DECL(_net_inet6_ip6);

SYSCTL_UINT(_net_inet6_ip6, OID_AUTO, frag6_nfrags,
	CTLFLAG_RD, __DEVOLATILE(u_int *, &frag6_nfrags), 0,
	"Global number of IPv6 fragments across all reassembly queues.");

static void
frag6_set_bucketsize(void)
{
	int i;

	if ((i = V_ip6_maxfragpackets) > 0)
		V_ip6_maxfragbucketsize = imax(i / (IP6REASS_NHASH / 2), 1);
}

SYSCTL_INT(_net_inet6_ip6, IPV6CTL_MAXFRAGS, maxfrags,
	CTLFLAG_RW, &ip6_maxfrags, 0,
	"Maximum allowed number of outstanding IPv6 packet fragments. "
	"A value of 0 means no fragmented packets will be accepted, while a "
	"a value of -1 means no limit");

static int
sysctl_ip6_maxfragpackets(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = V_ip6_maxfragpackets;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || !req->newptr)
		return (error);
	V_ip6_maxfragpackets = val;
	frag6_set_bucketsize();
	return (0);
}
SYSCTL_PROC(_net_inet6_ip6, IPV6CTL_MAXFRAGPACKETS, maxfragpackets,
	CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	NULL, 0, sysctl_ip6_maxfragpackets, "I",
	"Default maximum number of outstanding fragmented IPv6 packets. "
	"A value of 0 means no fragmented packets will be accepted, while a "
	"a value of -1 means no limit");
SYSCTL_UINT(_net_inet6_ip6, OID_AUTO, frag6_nfragpackets,
	CTLFLAG_VNET | CTLFLAG_RD,
	__DEVOLATILE(u_int *, &VNET_NAME(frag6_nfragpackets)), 0,
	"Per-VNET number of IPv6 fragments across all reassembly queues.");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_MAXFRAGSPERPACKET, maxfragsperpacket,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_maxfragsperpacket), 0,
	"Maximum allowed number of fragments per packet");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_MAXFRAGBUCKETSIZE, maxfragbucketsize,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_maxfragbucketsize), 0,
	"Maximum number of reassembly queues per hash bucket");

/*
 * Remove the IPv6 fragmentation header from the mbuf.
 */
int
ip6_deletefraghdr(struct mbuf *m, int offset, int wait __unused)
{
	struct ip6_hdr *ip6;

	KASSERT(m->m_len >= offset + sizeof(struct ip6_frag),
	    ("%s: ext headers not contigous in mbuf %p m_len %d >= "
	    "offset %d + %zu\n", __func__, m, m->m_len, offset,
	    sizeof(struct ip6_frag)));

	/* Delete frag6 header. */
	ip6 = mtod(m, struct ip6_hdr *);
	bcopy(ip6, (char *)ip6 + sizeof(struct ip6_frag), offset);
	m->m_data += sizeof(struct ip6_frag);
	m->m_len -= sizeof(struct ip6_frag);
	m->m_flags |= M_FRAGMENTED;

	return (0);
}

/*
 * Free a fragment reassembly header and all associated datagrams.
 */
static void
frag6_freef(struct ip6q *q6, uint32_t bucket)
{
	struct ip6_hdr *ip6;
	struct ip6asfrag *af6;
	struct mbuf *m;

	IP6QB_LOCK_ASSERT(bucket);

	while ((af6 = TAILQ_FIRST(&q6->ip6q_frags)) != NULL) {
		m = af6->ip6af_m;
		TAILQ_REMOVE(&q6->ip6q_frags, af6, ip6af_tq);

		/*
		 * Return ICMP time exceeded error for the 1st fragment.
		 * Just free other fragments.
		 */
		if (af6->ip6af_off == 0 && m->m_pkthdr.rcvif != NULL) {
			/* Adjust pointer. */
			ip6 = mtod(m, struct ip6_hdr *);

			/* Restore source and destination addresses. */
			ip6->ip6_src = q6->ip6q_src;
			ip6->ip6_dst = q6->ip6q_dst;

			icmp6_error(m, ICMP6_TIME_EXCEEDED,
			    ICMP6_TIME_EXCEED_REASSEMBLY, 0);
		} else
			m_freem(m);

		free(af6, M_FRAG6);
	}

	TAILQ_REMOVE(IP6QB_HEAD(bucket), q6, ip6q_tq);
	V_ip6qb[bucket].count--;
	atomic_subtract_int(&frag6_nfrags, q6->ip6q_nfrag);
#ifdef MAC
	mac_ip6q_destroy(q6);
#endif
	free(q6, M_FRAG6);
	atomic_subtract_int(&V_frag6_nfragpackets, 1);
}

/*
 * Drain off all datagram fragments belonging to
 * the given network interface.
 */
static void
frag6_cleanup(void *arg __unused, struct ifnet *ifp)
{
	struct ip6qhead *head;
	struct ip6q *q6;
	struct ip6asfrag *af6;
	uint32_t bucket;

	KASSERT(ifp != NULL, ("%s: ifp is NULL", __func__));

	CURVNET_SET_QUIET(ifp->if_vnet);
#ifdef VIMAGE
	/*
	 * Skip processing if IPv6 reassembly is not initialised or
	 * torn down by frag6_destroy().
	 */
	if (!V_frag6_on) {
		CURVNET_RESTORE();
		return;
	}
#endif

	for (bucket = 0; bucket < IP6REASS_NHASH; bucket++) {
		IP6QB_LOCK(bucket);
		head = IP6QB_HEAD(bucket);
		/* Scan fragment list. */
		TAILQ_FOREACH(q6, head, ip6q_tq) {
			TAILQ_FOREACH(af6, &q6->ip6q_frags, ip6af_tq) {
				/* Clear no longer valid rcvif pointer. */
				if (af6->ip6af_m->m_pkthdr.rcvif == ifp)
					af6->ip6af_m->m_pkthdr.rcvif = NULL;
			}
		}
		IP6QB_UNLOCK(bucket);
	}
	CURVNET_RESTORE();
}
EVENTHANDLER_DEFINE(ifnet_departure_event, frag6_cleanup, NULL, 0);

/*
 * Like in RFC2460, in RFC8200, fragment and reassembly rules do not agree with
 * each other, in terms of next header field handling in fragment header.
 * While the sender will use the same value for all of the fragmented packets,
 * receiver is suggested not to check for consistency.
 *
 * Fragment rules (p18,p19):
 *	(2)  A Fragment header containing:
 *	The Next Header value that identifies the first header
 *	after the Per-Fragment headers of the original packet.
 *		-> next header field is same for all fragments
 *
 * Reassembly rule (p20):
 *	The Next Header field of the last header of the Per-Fragment
 *	headers is obtained from the Next Header field of the first
 *	fragment's Fragment header.
 *		-> should grab it from the first fragment only
 *
 * The following note also contradicts with fragment rule - no one is going to
 * send different fragment with different next header field.
 *
 * Additional note (p22) [not an error]:
 *	The Next Header values in the Fragment headers of different
 *	fragments of the same original packet may differ.  Only the value
 *	from the Offset zero fragment packet is used for reassembly.
 *		-> should grab it from the first fragment only
 *
 * There is no explicit reason given in the RFC.  Historical reason maybe?
 */
/*
 * Fragment input.
 */
int
frag6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m, *t;
	struct ip6_hdr *ip6;
	struct ip6_frag *ip6f;
	struct ip6qhead *head;
	struct ip6q *q6;
	struct ip6asfrag *af6, *ip6af, *af6tmp;
	struct in6_ifaddr *ia6;
	struct ifnet *dstifp, *srcifp;
	uint32_t hashkey[(sizeof(struct in6_addr) * 2 +
		    sizeof(ip6f->ip6f_ident)) / sizeof(uint32_t)];
	uint32_t bucket, *hashkeyp;
	int fragoff, frgpartlen;	/* Must be larger than uint16_t. */
	int nxt, offset, plen;
	uint8_t ecn, ecn0;
	bool only_frag;
#ifdef RSS
	struct ip6_direct_ctx *ip6dc;
	struct m_tag *mtag;
#endif

	m = *mp;
	offset = *offp;

	M_ASSERTPKTHDR(m);

	if (m->m_len < offset + sizeof(struct ip6_frag)) {
		m = m_pullup(m, offset + sizeof(struct ip6_frag));
		if (m == NULL) {
			IP6STAT_INC(ip6s_exthdrtoolong);
			*mp = NULL;
			return (IPPROTO_DONE);
		}
	}
	ip6 = mtod(m, struct ip6_hdr *);

	dstifp = NULL;
	/* Find the destination interface of the packet. */
	ia6 = in6ifa_ifwithaddr(&ip6->ip6_dst, 0 /* XXX */);
	if (ia6 != NULL) {
		dstifp = ia6->ia_ifp;
		ifa_free(&ia6->ia_ifa);
	}

	/* Jumbo payload cannot contain a fragment header. */
	if (ip6->ip6_plen == 0) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER, offset);
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		*mp = NULL;
		return (IPPROTO_DONE);
	}

	/*
	 * Check whether fragment packet's fragment length is a
	 * multiple of 8 octets (unless it is the last one).
	 * sizeof(struct ip6_frag) == 8
	 * sizeof(struct ip6_hdr) = 40
	 */
	ip6f = (struct ip6_frag *)((caddr_t)ip6 + offset);
	if ((ip6f->ip6f_offlg & IP6F_MORE_FRAG) &&
	    (((ntohs(ip6->ip6_plen) - offset) & 0x7) != 0)) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
		    offsetof(struct ip6_hdr, ip6_plen));
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		*mp = NULL;
		return (IPPROTO_DONE);
	}

	IP6STAT_INC(ip6s_fragments);
	in6_ifstat_inc(dstifp, ifs6_reass_reqd);

	/*
	 * Handle "atomic" fragments (offset and m bit set to 0) upfront,
	 * unrelated to any reassembly.  We need to remove the frag hdr
	 * which is ugly.
	 * See RFC 6946 and section 4.5 of RFC 8200.
	 */
	if ((ip6f->ip6f_offlg & ~IP6F_RESERVED_MASK) == 0) {
		IP6STAT_INC(ip6s_atomicfrags);
		nxt = ip6f->ip6f_nxt;
		/*
		 * Set nxt(-hdr field value) to the original value.
		 * We cannot just set ip6->ip6_nxt as there might be
		 * an unfragmentable part with extension headers and
		 * we must update the last one.
		 */
		m_copyback(m, ip6_get_prevhdr(m, offset), sizeof(uint8_t),
		    (caddr_t)&nxt);
		ip6->ip6_plen = htons(ntohs(ip6->ip6_plen) -
		    sizeof(struct ip6_frag));
		if (ip6_deletefraghdr(m, offset, M_NOWAIT) != 0)
			goto dropfrag2;
		m->m_pkthdr.len -= sizeof(struct ip6_frag);
		in6_ifstat_inc(dstifp, ifs6_reass_ok);
		*mp = m;
		return (nxt);
	}

	/* Offset now points to data portion. */
	offset += sizeof(struct ip6_frag);

	/* Get fragment length and discard 0-byte fragments. */
	frgpartlen = sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen) - offset;
	if (frgpartlen == 0) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
		    offsetof(struct ip6_hdr, ip6_plen));
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		IP6STAT_INC(ip6s_fragdropped);
		*mp = NULL;
		return (IPPROTO_DONE);
	}

	/*
	 * Enforce upper bound on number of fragments for the entire system.
	 * If maxfrag is 0, never accept fragments.
	 * If maxfrag is -1, accept all fragments without limitation.
	 */
	if (ip6_maxfrags < 0)
		;
	else if (atomic_load_int(&frag6_nfrags) >= (u_int)ip6_maxfrags)
		goto dropfrag2;

	/*
	 * Validate that a full header chain to the ULP is present in the
	 * packet containing the first fragment as per RFC RFC7112 and
	 * RFC 8200 pages 18,19:
	 * The first fragment packet is composed of:
	 * (3)  Extension headers, if any, and the Upper-Layer header.  These
	 *      headers must be in the first fragment.  ...
	 */
	fragoff = ntohs(ip6f->ip6f_offlg & IP6F_OFF_MASK);
	/* XXX TODO.  thj has D16851 open for this. */
	/* Send ICMPv6 4,3 in case of violation. */

	/* Store receive network interface pointer for later. */
	srcifp = m->m_pkthdr.rcvif;

	/* Generate a hash value for fragment bucket selection. */
	hashkeyp = hashkey;
	memcpy(hashkeyp, &ip6->ip6_src, sizeof(struct in6_addr));
	hashkeyp += sizeof(struct in6_addr) / sizeof(*hashkeyp);
	memcpy(hashkeyp, &ip6->ip6_dst, sizeof(struct in6_addr));
	hashkeyp += sizeof(struct in6_addr) / sizeof(*hashkeyp);
	*hashkeyp = ip6f->ip6f_ident;
	bucket = jenkins_hash32(hashkey, nitems(hashkey), V_ip6qb_hashseed);
	bucket &= IP6REASS_HMASK;
	IP6QB_LOCK(bucket);
	head = IP6QB_HEAD(bucket);

	TAILQ_FOREACH(q6, head, ip6q_tq)
		if (ip6f->ip6f_ident == q6->ip6q_ident &&
		    IN6_ARE_ADDR_EQUAL(&ip6->ip6_src, &q6->ip6q_src) &&
		    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst, &q6->ip6q_dst)
#ifdef MAC
		    && mac_ip6q_match(m, q6)
#endif
		    )
			break;

	only_frag = false;
	if (q6 == NULL) {
		/* A first fragment to arrive creates a reassembly queue. */
		only_frag = true;

		/*
		 * Enforce upper bound on number of fragmented packets
		 * for which we attempt reassembly;
		 * If maxfragpackets is 0, never accept fragments.
		 * If maxfragpackets is -1, accept all fragments without
		 * limitation.
		 */
		if (V_ip6_maxfragpackets < 0)
			;
		else if (V_ip6qb[bucket].count >= V_ip6_maxfragbucketsize ||
		    atomic_load_int(&V_frag6_nfragpackets) >=
		    (u_int)V_ip6_maxfragpackets)
			goto dropfrag;

		/* Allocate IPv6 fragement packet queue entry. */
		q6 = (struct ip6q *)malloc(sizeof(struct ip6q), M_FRAG6,
		    M_NOWAIT | M_ZERO);
		if (q6 == NULL)
			goto dropfrag;
#ifdef MAC
		if (mac_ip6q_init(q6, M_NOWAIT) != 0) {
			free(q6, M_FRAG6);
			goto dropfrag;
		}
		mac_ip6q_create(m, q6);
#endif
		atomic_add_int(&V_frag6_nfragpackets, 1);

		/* ip6q_nxt will be filled afterwards, from 1st fragment. */
		TAILQ_INIT(&q6->ip6q_frags);
		q6->ip6q_ident	= ip6f->ip6f_ident;
		q6->ip6q_ttl	= IPV6_FRAGTTL;
		q6->ip6q_src	= ip6->ip6_src;
		q6->ip6q_dst	= ip6->ip6_dst;
		q6->ip6q_ecn	=
		    (ntohl(ip6->ip6_flow) >> 20) & IPTOS_ECN_MASK;
		q6->ip6q_unfrglen = -1;	/* The 1st fragment has not arrived. */

		/* Add the fragemented packet to the bucket. */
		TAILQ_INSERT_HEAD(head, q6, ip6q_tq);
		V_ip6qb[bucket].count++;
	}

	/*
	 * If it is the 1st fragment, record the length of the
	 * unfragmentable part and the next header of the fragment header.
	 * Assume the first 1st fragement to arrive will be correct.
	 * We do not have any duplicate checks here yet so another packet
	 * with fragoff == 0 could come and overwrite the ip6q_unfrglen
	 * and worse, the next header, at any time.
	 */
	if (fragoff == 0 && q6->ip6q_unfrglen == -1) {
		q6->ip6q_unfrglen = offset - sizeof(struct ip6_hdr) -
		    sizeof(struct ip6_frag);
		q6->ip6q_nxt = ip6f->ip6f_nxt;
		/* XXX ECN? */
	}

	/*
	 * Check that the reassembled packet would not exceed 65535 bytes
	 * in size.
	 * If it would exceed, discard the fragment and return an ICMP error.
	 */
	if (q6->ip6q_unfrglen >= 0) {
		/* The 1st fragment has already arrived. */
		if (q6->ip6q_unfrglen + fragoff + frgpartlen > IPV6_MAXPACKET) {
			if (only_frag) {
				TAILQ_REMOVE(head, q6, ip6q_tq);
				V_ip6qb[bucket].count--;
				atomic_subtract_int(&V_frag6_nfragpackets, 1);
#ifdef MAC
				mac_ip6q_destroy(q6);
#endif
				free(q6, M_FRAG6);
			}
			IP6QB_UNLOCK(bucket);
			icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			    offset - sizeof(struct ip6_frag) +
			    offsetof(struct ip6_frag, ip6f_offlg));
			*mp = NULL;
			return (IPPROTO_DONE);
		}
	} else if (fragoff + frgpartlen > IPV6_MAXPACKET) {
		if (only_frag) {
			TAILQ_REMOVE(head, q6, ip6q_tq);
			V_ip6qb[bucket].count--;
			atomic_subtract_int(&V_frag6_nfragpackets, 1);
#ifdef MAC
			mac_ip6q_destroy(q6);
#endif
			free(q6, M_FRAG6);
		}
		IP6QB_UNLOCK(bucket);
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
		    offset - sizeof(struct ip6_frag) +
		    offsetof(struct ip6_frag, ip6f_offlg));
		*mp = NULL;
		return (IPPROTO_DONE);
	}

	/*
	 * If it is the first fragment, do the above check for each
	 * fragment already stored in the reassembly queue.
	 */
	if (fragoff == 0 && !only_frag) {
		TAILQ_FOREACH_SAFE(af6, &q6->ip6q_frags, ip6af_tq, af6tmp) {
			if (q6->ip6q_unfrglen + af6->ip6af_off +
			    af6->ip6af_frglen > IPV6_MAXPACKET) {
				struct ip6_hdr *ip6err;
				struct mbuf *merr;
				int erroff;

				merr = af6->ip6af_m;
				erroff = af6->ip6af_offset;

				/* Dequeue the fragment. */
				TAILQ_REMOVE(&q6->ip6q_frags, af6, ip6af_tq);
				q6->ip6q_nfrag--;
				atomic_subtract_int(&frag6_nfrags, 1);
				free(af6, M_FRAG6);

				/* Set a valid receive interface pointer. */
				merr->m_pkthdr.rcvif = srcifp;

				/* Adjust pointer. */
				ip6err = mtod(merr, struct ip6_hdr *);

				/*
				 * Restore source and destination addresses
				 * in the erroneous IPv6 header.
				 */
				ip6err->ip6_src = q6->ip6q_src;
				ip6err->ip6_dst = q6->ip6q_dst;

				icmp6_error(merr, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff - sizeof(struct ip6_frag) +
				    offsetof(struct ip6_frag, ip6f_offlg));
			}
		}
	}

	/* Allocate an IPv6 fragement queue entry for this fragmented part. */
	ip6af = (struct ip6asfrag *)malloc(sizeof(struct ip6asfrag), M_FRAG6,
	    M_NOWAIT | M_ZERO);
	if (ip6af == NULL)
		goto dropfrag;
	ip6af->ip6af_mff = (ip6f->ip6f_offlg & IP6F_MORE_FRAG) ? true : false;
	ip6af->ip6af_off = fragoff;
	ip6af->ip6af_frglen = frgpartlen;
	ip6af->ip6af_offset = offset;
	ip6af->ip6af_m = m;

	if (only_frag) {
		/*
		 * Do a manual insert rather than a hard-to-understand cast
		 * to a different type relying on data structure order to work.
		 */
		TAILQ_INSERT_HEAD(&q6->ip6q_frags, ip6af, ip6af_tq);
		goto postinsert;
	}

	/* Do duplicate, condition, and boundry checks. */
	/*
	 * Handle ECN by comparing this segment with the first one;
	 * if CE is set, do not lose CE.
	 * Drop if CE and not-ECT are mixed for the same packet.
	 */
	ecn = (ntohl(ip6->ip6_flow) >> 20) & IPTOS_ECN_MASK;
	ecn0 = q6->ip6q_ecn;
	if (ecn == IPTOS_ECN_CE) {
		if (ecn0 == IPTOS_ECN_NOTECT) {
			free(ip6af, M_FRAG6);
			goto dropfrag;
		}
		if (ecn0 != IPTOS_ECN_CE)
			q6->ip6q_ecn = IPTOS_ECN_CE;
	}
	if (ecn == IPTOS_ECN_NOTECT && ecn0 != IPTOS_ECN_NOTECT) {
		free(ip6af, M_FRAG6);
		goto dropfrag;
	}

	/* Find a fragmented part which begins after this one does. */
	TAILQ_FOREACH(af6, &q6->ip6q_frags, ip6af_tq)
		if (af6->ip6af_off > ip6af->ip6af_off)
			break;

	/*
	 * If the incoming framgent overlaps some existing fragments in
	 * the reassembly queue, drop both the new fragment and the
	 * entire reassembly queue.  However, if the new fragment
	 * is an exact duplicate of an existing fragment, only silently
	 * drop the existing fragment and leave the fragmentation queue
	 * unchanged, as allowed by the RFC.  (RFC 8200, 4.5)
	 */
	if (af6 != NULL)
		af6tmp = TAILQ_PREV(af6, ip6fraghead, ip6af_tq);
	else
		af6tmp = TAILQ_LAST(&q6->ip6q_frags, ip6fraghead);
	if (af6tmp != NULL) {
		if (af6tmp->ip6af_off + af6tmp->ip6af_frglen -
		    ip6af->ip6af_off > 0) {
			if (af6tmp->ip6af_off != ip6af->ip6af_off ||
			    af6tmp->ip6af_frglen != ip6af->ip6af_frglen)
				frag6_freef(q6, bucket);
			free(ip6af, M_FRAG6);
			goto dropfrag;
		}
	}
	if (af6 != NULL) {
		if (ip6af->ip6af_off + ip6af->ip6af_frglen -
		    af6->ip6af_off > 0) {
			if (af6->ip6af_off != ip6af->ip6af_off ||
			    af6->ip6af_frglen != ip6af->ip6af_frglen)
				frag6_freef(q6, bucket);
			free(ip6af, M_FRAG6);
			goto dropfrag;
		}
	}

#ifdef MAC
	mac_ip6q_update(m, q6);
#endif

	/*
	 * Stick new segment in its place; check for complete reassembly.
	 * If not complete, check fragment limit.  Move to front of packet
	 * queue, as we are the most recently active fragmented packet.
	 */
	if (af6 != NULL)
		TAILQ_INSERT_BEFORE(af6, ip6af, ip6af_tq);
	else
		TAILQ_INSERT_TAIL(&q6->ip6q_frags, ip6af, ip6af_tq);
postinsert:
	atomic_add_int(&frag6_nfrags, 1);
	q6->ip6q_nfrag++;

	plen = 0;
	TAILQ_FOREACH(af6, &q6->ip6q_frags, ip6af_tq) {
		if (af6->ip6af_off != plen) {
			if (q6->ip6q_nfrag > V_ip6_maxfragsperpacket) {
				IP6STAT_ADD(ip6s_fragdropped, q6->ip6q_nfrag);
				frag6_freef(q6, bucket);
			}
			IP6QB_UNLOCK(bucket);
			*mp = NULL;
			return (IPPROTO_DONE);
		}
		plen += af6->ip6af_frglen;
	}
	af6 = TAILQ_LAST(&q6->ip6q_frags, ip6fraghead);
	if (af6->ip6af_mff) {
		if (q6->ip6q_nfrag > V_ip6_maxfragsperpacket) {
			IP6STAT_ADD(ip6s_fragdropped, q6->ip6q_nfrag);
			frag6_freef(q6, bucket);
		}
		IP6QB_UNLOCK(bucket);
		*mp = NULL;
		return (IPPROTO_DONE);
	}

	/* Reassembly is complete; concatenate fragments. */
	ip6af = TAILQ_FIRST(&q6->ip6q_frags);
	t = m = ip6af->ip6af_m;
	TAILQ_REMOVE(&q6->ip6q_frags, ip6af, ip6af_tq);
	while ((af6 = TAILQ_FIRST(&q6->ip6q_frags)) != NULL) {
		m->m_pkthdr.csum_flags &=
		    af6->ip6af_m->m_pkthdr.csum_flags;
		m->m_pkthdr.csum_data +=
		    af6->ip6af_m->m_pkthdr.csum_data;

		TAILQ_REMOVE(&q6->ip6q_frags, af6, ip6af_tq);
		t = m_last(t);
		m_adj(af6->ip6af_m, af6->ip6af_offset);
		m_demote_pkthdr(af6->ip6af_m);
		m_cat(t, af6->ip6af_m);
		free(af6, M_FRAG6);
	}

	while (m->m_pkthdr.csum_data & 0xffff0000)
		m->m_pkthdr.csum_data = (m->m_pkthdr.csum_data & 0xffff) +
		    (m->m_pkthdr.csum_data >> 16);

	/* Adjust offset to point where the original next header starts. */
	offset = ip6af->ip6af_offset - sizeof(struct ip6_frag);
	free(ip6af, M_FRAG6);
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_plen = htons((u_short)plen + offset - sizeof(struct ip6_hdr));
	if (q6->ip6q_ecn == IPTOS_ECN_CE)
		ip6->ip6_flow |= htonl(IPTOS_ECN_CE << 20);
	nxt = q6->ip6q_nxt;

	TAILQ_REMOVE(head, q6, ip6q_tq);
	V_ip6qb[bucket].count--;
	atomic_subtract_int(&frag6_nfrags, q6->ip6q_nfrag);

	ip6_deletefraghdr(m, offset, M_NOWAIT);

	/* Set nxt(-hdr field value) to the original value. */
	m_copyback(m, ip6_get_prevhdr(m, offset), sizeof(uint8_t),
	    (caddr_t)&nxt);

#ifdef MAC
	mac_ip6q_reassemble(q6, m);
	mac_ip6q_destroy(q6);
#endif
	free(q6, M_FRAG6);
	atomic_subtract_int(&V_frag6_nfragpackets, 1);

	if (m->m_flags & M_PKTHDR) { /* Isn't it always true? */

		plen = 0;
		for (t = m; t; t = t->m_next)
			plen += t->m_len;
		m->m_pkthdr.len = plen;
		/* Set a valid receive interface pointer. */
		m->m_pkthdr.rcvif = srcifp;
	}

#ifdef RSS
	mtag = m_tag_alloc(MTAG_ABI_IPV6, IPV6_TAG_DIRECT, sizeof(*ip6dc),
	    M_NOWAIT);
	if (mtag == NULL)
		goto dropfrag;

	ip6dc = (struct ip6_direct_ctx *)(mtag + 1);
	ip6dc->ip6dc_nxt = nxt;
	ip6dc->ip6dc_off = offset;

	m_tag_prepend(m, mtag);
#endif

	IP6QB_UNLOCK(bucket);
	IP6STAT_INC(ip6s_reassembled);
	in6_ifstat_inc(dstifp, ifs6_reass_ok);

#ifdef RSS
	/* Queue/dispatch for reprocessing. */
	netisr_dispatch(NETISR_IPV6_DIRECT, m);
	*mp = NULL;
	return (IPPROTO_DONE);
#endif

	/* Tell launch routine the next header. */
	*mp = m;
	*offp = offset;

	return (nxt);

dropfrag:
	IP6QB_UNLOCK(bucket);
dropfrag2:
	in6_ifstat_inc(dstifp, ifs6_reass_fail);
	IP6STAT_INC(ip6s_fragdropped);
	m_freem(m);
	*mp = NULL;
	return (IPPROTO_DONE);
}

/*
 * IPv6 reassembling timer processing;
 * if a timer expires on a reassembly queue, discard it.
 */
void
frag6_slowtimo(void)
{
	VNET_ITERATOR_DECL(vnet_iter);
	struct ip6qhead *head;
	struct ip6q *q6, *q6tmp;
	uint32_t bucket;

	VNET_LIST_RLOCK_NOSLEEP();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		for (bucket = 0; bucket < IP6REASS_NHASH; bucket++) {
			IP6QB_LOCK(bucket);
			head = IP6QB_HEAD(bucket);
			TAILQ_FOREACH_SAFE(q6, head, ip6q_tq, q6tmp)
				if (--q6->ip6q_ttl == 0) {
					IP6STAT_ADD(ip6s_fragtimeout,
						q6->ip6q_nfrag);
					/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
					frag6_freef(q6, bucket);
				}
			/*
			 * If we are over the maximum number of fragments
			 * (due to the limit being lowered), drain off
			 * enough to get down to the new limit.
			 * Note that we drain all reassembly queues if
			 * maxfragpackets is 0 (fragmentation is disabled),
			 * and do not enforce a limit when maxfragpackets
			 * is negative.
			 */
			while ((V_ip6_maxfragpackets == 0 ||
			    (V_ip6_maxfragpackets > 0 &&
			    V_ip6qb[bucket].count > V_ip6_maxfragbucketsize)) &&
			    (q6 = TAILQ_LAST(head, ip6qhead)) != NULL) {
				IP6STAT_ADD(ip6s_fragoverflow, q6->ip6q_nfrag);
				/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
				frag6_freef(q6, bucket);
			}
			IP6QB_UNLOCK(bucket);
		}
		/*
		 * If we are still over the maximum number of fragmented
		 * packets, drain off enough to get down to the new limit.
		 */
		bucket = 0;
		while (V_ip6_maxfragpackets >= 0 &&
		    atomic_load_int(&V_frag6_nfragpackets) >
		    (u_int)V_ip6_maxfragpackets) {
			IP6QB_LOCK(bucket);
			q6 = TAILQ_LAST(IP6QB_HEAD(bucket), ip6qhead);
			if (q6 != NULL) {
				IP6STAT_ADD(ip6s_fragoverflow, q6->ip6q_nfrag);
				/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
				frag6_freef(q6, bucket);
			}
			IP6QB_UNLOCK(bucket);
			bucket = (bucket + 1) % IP6REASS_NHASH;
		}
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK_NOSLEEP();
}

/*
 * Eventhandler to adjust limits in case nmbclusters change.
 */
static void
frag6_change(void *tag)
{
	VNET_ITERATOR_DECL(vnet_iter);

	ip6_maxfrags = IP6_MAXFRAGS;
	VNET_LIST_RLOCK_NOSLEEP();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		V_ip6_maxfragpackets = IP6_MAXFRAGPACKETS;
		frag6_set_bucketsize();
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK_NOSLEEP();
}

/*
 * Initialise reassembly queue and fragment identifier.
 */
void
frag6_init(void)
{
	uint32_t bucket;

	V_ip6_maxfragpackets = IP6_MAXFRAGPACKETS;
	frag6_set_bucketsize();
	for (bucket = 0; bucket < IP6REASS_NHASH; bucket++) {
		TAILQ_INIT(IP6QB_HEAD(bucket));
		mtx_init(&V_ip6qb[bucket].lock, "ip6qb", NULL, MTX_DEF);
		V_ip6qb[bucket].count = 0;
	}
	V_ip6qb_hashseed = arc4random();
	V_ip6_maxfragsperpacket = 64;
#ifdef VIMAGE
	V_frag6_on = true;
#endif
	if (!IS_DEFAULT_VNET(curvnet))
		return;

	ip6_maxfrags = IP6_MAXFRAGS;
	EVENTHANDLER_REGISTER(nmbclusters_change,
	    frag6_change, NULL, EVENTHANDLER_PRI_ANY);
}

/*
 * Drain off all datagram fragments.
 */
static void
frag6_drain_one(void)
{
	struct ip6q *q6;
	uint32_t bucket;

	for (bucket = 0; bucket < IP6REASS_NHASH; bucket++) {
		IP6QB_LOCK(bucket);
		while ((q6 = TAILQ_FIRST(IP6QB_HEAD(bucket))) != NULL) {
			IP6STAT_INC(ip6s_fragdropped);
			/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
			frag6_freef(q6, bucket);
		}
		IP6QB_UNLOCK(bucket);
	}
}

void
frag6_drain(void)
{
	VNET_ITERATOR_DECL(vnet_iter);

	VNET_LIST_RLOCK_NOSLEEP();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		frag6_drain_one();
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK_NOSLEEP();
}

#ifdef VIMAGE
/*
 * Clear up IPv6 reassembly structures.
 */
void
frag6_destroy(void)
{
	uint32_t bucket;

	frag6_drain_one();
	V_frag6_on = false;
	for (bucket = 0; bucket < IP6REASS_NHASH; bucket++) {
		KASSERT(V_ip6qb[bucket].count == 0,
		    ("%s: V_ip6qb[%d] (%p) count not 0 (%d)", __func__,
		    bucket, &V_ip6qb[bucket], V_ip6qb[bucket].count));
		mtx_destroy(&V_ip6qb[bucket].lock);
	}
}
#endif
