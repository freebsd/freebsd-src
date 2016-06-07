/*-
 * Copyright (c) 2014 Gleb Smirnoff <glebius@FreeBSD.org>
 * Copyright (c) 2008-2010, BitGravity Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Neither the name of the BitGravity Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_route.h"
#include "opt_mpath.h"
#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/bitstring.h>
#include <sys/condvar.h>
#include <sys/callout.h>
#include <sys/hash.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <vm/uma.h>

#include <net/if.h>
#include <net/if_llatbl.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/flowtable.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif
#ifdef FLOWTABLE_HASH_ALL
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/sctp.h>
#endif

#include <ddb/ddb.h>

#ifdef	FLOWTABLE_HASH_ALL
#define	KEY_PORTS	(sizeof(uint16_t) * 2)
#define	KEY_ADDRS	2
#else
#define	KEY_PORTS	0
#define	KEY_ADDRS	1
#endif

#ifdef	INET6
#define	KEY_ADDR_LEN	sizeof(struct in6_addr)
#else
#define	KEY_ADDR_LEN	sizeof(struct in_addr)
#endif

#define	KEYLEN	((KEY_ADDR_LEN * KEY_ADDRS + KEY_PORTS) / sizeof(uint32_t))

struct flentry {
	uint32_t		f_hash;		/* hash flowing forward */
	uint32_t		f_key[KEYLEN];	/* address(es and ports) */
	uint32_t		f_uptime;	/* uptime at last access */
	uint16_t		f_fibnum;	/* fib index */
#ifdef FLOWTABLE_HASH_ALL
	uint8_t			f_proto;	/* protocol */
	uint8_t			f_flags;	/* stale? */
#define FL_STALE 		1
#endif
	SLIST_ENTRY(flentry)	f_next;		/* pointer to collision entry */
	struct rtentry		*f_rt;		/* rtentry for flow */
	struct llentry		*f_lle;		/* llentry for flow */
};
#undef KEYLEN

SLIST_HEAD(flist, flentry);
/* Make sure we can use pcpu_zone_ptr for struct flist. */
CTASSERT(sizeof(struct flist) == sizeof(void *));

struct flowtable {
	counter_u64_t	*ft_stat;
	int 		ft_size;
	/*
	 * ft_table is a malloc(9)ed array of pointers.  Pointers point to
	 * memory from UMA_ZONE_PCPU zone.
	 * ft_masks is per-cpu pointer itself.  Each instance points
	 * to a malloc(9)ed bitset, that is private to corresponding CPU.
	 */
	struct flist	**ft_table;
	bitstr_t 	**ft_masks;
	bitstr_t	*ft_tmpmask;
};

#define	FLOWSTAT_ADD(ft, name, v)	\
	counter_u64_add((ft)->ft_stat[offsetof(struct flowtable_stat, name) / sizeof(uint64_t)], (v))
#define	FLOWSTAT_INC(ft, name)	FLOWSTAT_ADD(ft, name, 1)

static struct proc *flowcleanerproc;
static uint32_t flow_hashjitter;

static struct cv 	flowclean_f_cv;
static struct cv 	flowclean_c_cv;
static struct mtx	flowclean_lock;
static uint32_t		flowclean_cycles;

/*
 * TODO:
 * - add sysctls to resize && flush flow tables
 * - Add per flowtable sysctls for statistics and configuring timeouts
 * - add saturation counter to rtentry to support per-packet load-balancing
 *   add flag to indicate round-robin flow, add list lookup from head
     for flows
 * - add sysctl / device node / syscall to support exporting and importing
 *   of flows with flag to indicate that a flow was imported so should
 *   not be considered for auto-cleaning
 * - support explicit connection state (currently only ad-hoc for DSR)
 * - idetach() cleanup for options VIMAGE builds.
 */
#ifdef INET
static VNET_DEFINE(struct flowtable, ip4_ft);
#define	V_ip4_ft	VNET(ip4_ft)
#endif
#ifdef INET6
static VNET_DEFINE(struct flowtable, ip6_ft);
#define	V_ip6_ft	VNET(ip6_ft)
#endif

static uma_zone_t flow_zone;

static VNET_DEFINE(int, flowtable_enable) = 1;
#define	V_flowtable_enable		VNET(flowtable_enable)

static SYSCTL_NODE(_net, OID_AUTO, flowtable, CTLFLAG_RD, NULL,
    "flowtable");
SYSCTL_INT(_net_flowtable, OID_AUTO, enable, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(flowtable_enable), 0, "enable flowtable caching.");
SYSCTL_UMA_MAX(_net_flowtable, OID_AUTO, maxflows, CTLFLAG_RW,
    &flow_zone, "Maximum number of flows allowed");

static MALLOC_DEFINE(M_FTABLE, "flowtable", "flowtable hashes and bitstrings");

static struct flentry *
flowtable_lookup_common(struct flowtable *, uint32_t *, int, uint32_t);

#ifdef INET
static struct flentry *
flowtable_lookup_ipv4(struct mbuf *m, struct route *ro)
{
	struct flentry *fle;
	struct sockaddr_in *sin;
	struct ip *ip;
	uint32_t fibnum;
#ifdef FLOWTABLE_HASH_ALL
	uint32_t key[3];
	int iphlen;
	uint16_t sport, dport;
	uint8_t proto;
#endif

	ip = mtod(m, struct ip *);

	if (ip->ip_src.s_addr == ip->ip_dst.s_addr ||
	    (ntohl(ip->ip_dst.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET ||
	    (ntohl(ip->ip_src.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
		return (NULL);

	fibnum = M_GETFIB(m);

#ifdef FLOWTABLE_HASH_ALL
	iphlen = ip->ip_hl << 2;
	proto = ip->ip_p;

	switch (proto) {
	case IPPROTO_TCP: {
		struct tcphdr *th;

		th = (struct tcphdr *)((char *)ip + iphlen);
		sport = th->th_sport;
		dport = th->th_dport;
		if (th->th_flags & (TH_RST|TH_FIN))
			fibnum |= (FL_STALE << 24);
		break;
	}
	case IPPROTO_UDP: {
		struct udphdr *uh;

		uh = (struct udphdr *)((char *)ip + iphlen);
		sport = uh->uh_sport;
		dport = uh->uh_dport;
		break;
	}
	case IPPROTO_SCTP: {
		struct sctphdr *sh;

		sh = (struct sctphdr *)((char *)ip + iphlen);
		sport = sh->src_port;
		dport = sh->dest_port;
		/* XXXGL: handle stale? */
		break;
	}
	default:
		sport = dport = 0;
		break;
	}

	key[0] = ip->ip_dst.s_addr;
	key[1] = ip->ip_src.s_addr;
	key[2] = (dport << 16) | sport;
	fibnum |= proto << 16;

	fle = flowtable_lookup_common(&V_ip4_ft, key, 3 * sizeof(uint32_t),
	    fibnum);

#else	/* !FLOWTABLE_HASH_ALL */

	fle = flowtable_lookup_common(&V_ip4_ft, (uint32_t *)&ip->ip_dst,
	    sizeof(struct in_addr), fibnum);

#endif	/* FLOWTABLE_HASH_ALL */

	if (fle == NULL)
		return (NULL);

	sin = (struct sockaddr_in *)&ro->ro_dst;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_addr = ip->ip_dst;

	return (fle);
}
#endif /* INET */

#ifdef INET6
/*
 * PULLUP_TO(len, p, T) makes sure that len + sizeof(T) is contiguous,
 * then it sets p to point at the offset "len" in the mbuf. WARNING: the
 * pointer might become stale after other pullups (but we never use it
 * this way).
 */
#define PULLUP_TO(_len, p, T)						\
do {									\
	int x = (_len) + sizeof(T);					\
	if ((m)->m_len < x)						\
		return (NULL);						\
	p = (mtod(m, char *) + (_len));					\
} while (0)

#define	TCP(p)		((struct tcphdr *)(p))
#define	SCTP(p)		((struct sctphdr *)(p))
#define	UDP(p)		((struct udphdr *)(p))

static struct flentry *
flowtable_lookup_ipv6(struct mbuf *m, struct route *ro)
{
	struct flentry *fle;
	struct sockaddr_in6 *sin6;
	struct ip6_hdr *ip6;
	uint32_t fibnum;
#ifdef FLOWTABLE_HASH_ALL
	uint32_t key[9];
	void *ulp;
	int hlen;
	uint16_t sport, dport;
	u_short offset;
	uint8_t proto;
#else
	uint32_t key[4];
#endif

	ip6 = mtod(m, struct ip6_hdr *);
	if (in6_localaddr(&ip6->ip6_dst))
		return (NULL);

	fibnum = M_GETFIB(m);

#ifdef	FLOWTABLE_HASH_ALL
	hlen = sizeof(struct ip6_hdr);
	proto = ip6->ip6_nxt;
	offset = sport = dport = 0;
	ulp = NULL;
	while (ulp == NULL) {
		switch (proto) {
		case IPPROTO_ICMPV6:
		case IPPROTO_OSPFIGP:
		case IPPROTO_PIM:
		case IPPROTO_CARP:
		case IPPROTO_ESP:
		case IPPROTO_NONE:
			ulp = ip6;
			break;
		case IPPROTO_TCP:
			PULLUP_TO(hlen, ulp, struct tcphdr);
			dport = TCP(ulp)->th_dport;
			sport = TCP(ulp)->th_sport;
			if (TCP(ulp)->th_flags & (TH_RST|TH_FIN))
				fibnum |= (FL_STALE << 24);
			break;
		case IPPROTO_SCTP:
			PULLUP_TO(hlen, ulp, struct sctphdr);
			dport = SCTP(ulp)->src_port;
			sport = SCTP(ulp)->dest_port;
			/* XXXGL: handle stale? */
			break;
		case IPPROTO_UDP:
			PULLUP_TO(hlen, ulp, struct udphdr);
			dport = UDP(ulp)->uh_dport;
			sport = UDP(ulp)->uh_sport;
			break;
		case IPPROTO_HOPOPTS:	/* RFC 2460 */
			PULLUP_TO(hlen, ulp, struct ip6_hbh);
			hlen += (((struct ip6_hbh *)ulp)->ip6h_len + 1) << 3;
			proto = ((struct ip6_hbh *)ulp)->ip6h_nxt;
			ulp = NULL;
			break;
		case IPPROTO_ROUTING:	/* RFC 2460 */
			PULLUP_TO(hlen, ulp, struct ip6_rthdr);
			hlen += (((struct ip6_rthdr *)ulp)->ip6r_len + 1) << 3;
			proto = ((struct ip6_rthdr *)ulp)->ip6r_nxt;
			ulp = NULL;
			break;
		case IPPROTO_FRAGMENT:	/* RFC 2460 */
			PULLUP_TO(hlen, ulp, struct ip6_frag);
			hlen += sizeof (struct ip6_frag);
			proto = ((struct ip6_frag *)ulp)->ip6f_nxt;
			offset = ((struct ip6_frag *)ulp)->ip6f_offlg &
			    IP6F_OFF_MASK;
			ulp = NULL;
			break;
		case IPPROTO_DSTOPTS:	/* RFC 2460 */
			PULLUP_TO(hlen, ulp, struct ip6_hbh);
			hlen += (((struct ip6_hbh *)ulp)->ip6h_len + 1) << 3;
			proto = ((struct ip6_hbh *)ulp)->ip6h_nxt;
			ulp = NULL;
			break;
		case IPPROTO_AH:	/* RFC 2402 */
			PULLUP_TO(hlen, ulp, struct ip6_ext);
			hlen += (((struct ip6_ext *)ulp)->ip6e_len + 2) << 2;
			proto = ((struct ip6_ext *)ulp)->ip6e_nxt;
			ulp = NULL;
			break;
		default:
			PULLUP_TO(hlen, ulp, struct ip6_ext);
			break;
		}
	}

	bcopy(&ip6->ip6_dst, &key[0], sizeof(struct in6_addr));
	bcopy(&ip6->ip6_src, &key[4], sizeof(struct in6_addr));
	key[8] = (dport << 16) | sport;
	fibnum |= proto << 16;

	fle = flowtable_lookup_common(&V_ip6_ft, key, 9 * sizeof(uint32_t),
	    fibnum);
#else	/* !FLOWTABLE_HASH_ALL */
	bcopy(&ip6->ip6_dst, &key[0], sizeof(struct in6_addr));
	fle = flowtable_lookup_common(&V_ip6_ft, key, sizeof(struct in6_addr),
	    fibnum);
#endif	/* FLOWTABLE_HASH_ALL */

	if (fle == NULL)
		return (NULL);

	sin6 = (struct sockaddr_in6 *)&ro->ro_dst;
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(*sin6);
	bcopy(&ip6->ip6_dst, &sin6->sin6_addr, sizeof(struct in6_addr));

	return (fle);
}
#endif /* INET6 */

static bitstr_t *
flowtable_mask(struct flowtable *ft)
{

	/*
	 * flowtable_free_stale() calls w/o critical section, but
	 * with sched_bind(). Since pointer is stable throughout
	 * ft lifetime, it is safe, otherwise...
	 *
	 * CRITICAL_ASSERT(curthread);
	 */

	return (*(bitstr_t **)zpcpu_get(ft->ft_masks));
}

static struct flist *
flowtable_list(struct flowtable *ft, uint32_t hash)
{

	CRITICAL_ASSERT(curthread);
	return (zpcpu_get(ft->ft_table[hash % ft->ft_size]));
}

static int
flow_stale(struct flowtable *ft, struct flentry *fle, int maxidle)
{

	if (((fle->f_rt->rt_flags & RTF_UP) == 0) ||
	    (fle->f_rt->rt_ifp == NULL) ||
	    !RT_LINK_IS_UP(fle->f_rt->rt_ifp) ||
	    (fle->f_lle->la_flags & LLE_VALID) == 0)
		return (1);

	if (time_uptime - fle->f_uptime > maxidle)
		return (1);

#ifdef FLOWTABLE_HASH_ALL
	if (fle->f_flags & FL_STALE)
		return (1);
#endif

	return (0);
}

static int
flow_full(void)
{
	int count, max;

	count = uma_zone_get_cur(flow_zone);
	max = uma_zone_get_max(flow_zone);

	return (count > (max - (max >> 3)));
}

static int
flow_matches(struct flentry *fle, uint32_t *key, int keylen, uint32_t fibnum)
{
#ifdef FLOWTABLE_HASH_ALL
	uint8_t proto;

	proto = (fibnum >> 16) & 0xff;
	fibnum &= 0xffff;
#endif

	CRITICAL_ASSERT(curthread);

	/* Microoptimization for IPv4: don't use bcmp(). */
	if (((keylen == sizeof(uint32_t) && (fle->f_key[0] == key[0])) ||
	    (bcmp(fle->f_key, key, keylen) == 0)) &&
	    fibnum == fle->f_fibnum &&
#ifdef FLOWTABLE_HASH_ALL
	    proto == fle->f_proto &&
#endif
	    (fle->f_rt->rt_flags & RTF_UP) &&
	    fle->f_rt->rt_ifp != NULL &&
	    (fle->f_lle->la_flags & LLE_VALID))
		return (1);

	return (0);
}

static struct flentry *
flowtable_insert(struct flowtable *ft, uint32_t hash, uint32_t *key,
    int keylen, uint32_t fibnum0)
{
#ifdef INET6
	struct route_in6 sro6;
#endif
#ifdef INET
	struct route sro;
#endif
	struct route *ro = NULL;
	struct rtentry *rt;
	struct lltable *lt = NULL;
	struct llentry *lle;
	struct sockaddr_storage *l3addr;
	struct ifnet *ifp;
	struct flist *flist;
	struct flentry *fle, *iter;
	bitstr_t *mask;
	uint16_t fibnum = fibnum0;
#ifdef FLOWTABLE_HASH_ALL
	uint8_t proto;

	proto = (fibnum0 >> 16) & 0xff;
	fibnum = fibnum0 & 0xffff;
#endif

	/*
	 * This bit of code ends up locking the
	 * same route 3 times (just like ip_output + ether_output)
	 * - at lookup
	 * - in rt_check when called by arpresolve
	 * - dropping the refcount for the rtentry
	 *
	 * This could be consolidated to one if we wrote a variant
	 * of arpresolve with an rt_check variant that expected to
	 * receive the route locked
	 */
#ifdef INET
	if (ft == &V_ip4_ft) {
		struct sockaddr_in *sin;

		ro = &sro;
		bzero(&sro.ro_dst, sizeof(sro.ro_dst));

		sin = (struct sockaddr_in *)&sro.ro_dst;
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr.s_addr = key[0];
	}
#endif
#ifdef INET6
	if (ft == &V_ip6_ft) {
		struct sockaddr_in6 *sin6;

		ro = (struct route *)&sro6;
		sin6 = &sro6.ro_dst;

		bzero(sin6, sizeof(*sin6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		bcopy(key, &sin6->sin6_addr, sizeof(struct in6_addr));
	}
#endif

	ro->ro_rt = NULL;
#ifdef RADIX_MPATH
	rtalloc_mpath_fib(ro, hash, fibnum);
#else
	rtalloc_ign_fib(ro, 0, fibnum);
#endif
	if (ro->ro_rt == NULL)
		return (NULL);

	rt = ro->ro_rt;
	ifp = rt->rt_ifp;

	if (ifp->if_flags & (IFF_POINTOPOINT | IFF_LOOPBACK)) {
		RTFREE(rt);
		return (NULL);
	}

#ifdef INET
	if (ft == &V_ip4_ft)
		lt = LLTABLE(ifp);
#endif
#ifdef INET6
	if (ft == &V_ip6_ft)
		lt = LLTABLE6(ifp);
#endif

	if (rt->rt_flags & RTF_GATEWAY)
		l3addr = (struct sockaddr_storage *)rt->rt_gateway;
	else
		l3addr = (struct sockaddr_storage *)&ro->ro_dst;
	lle = llentry_alloc(ifp, lt, l3addr);

	if (lle == NULL) {
		RTFREE(rt);
		return (NULL);
	}

	/* Don't insert the entry if the ARP hasn't yet finished resolving. */
	if ((lle->la_flags & LLE_VALID) == 0) {
		RTFREE(rt);
		LLE_FREE(lle);
		FLOWSTAT_INC(ft, ft_fail_lle_invalid);
		return (NULL);
	}

	fle = uma_zalloc(flow_zone, M_NOWAIT | M_ZERO);
	if (fle == NULL) {
		RTFREE(rt);
		LLE_FREE(lle);
		return (NULL);
	}

	fle->f_hash = hash;
	bcopy(key, &fle->f_key, keylen);
	fle->f_rt = rt;
	fle->f_lle = lle;
	fle->f_fibnum = fibnum;
	fle->f_uptime = time_uptime;
#ifdef FLOWTABLE_HASH_ALL
	fle->f_proto = proto;
	fle->f_flags = fibnum0 >> 24;
#endif

	critical_enter();
	mask = flowtable_mask(ft);
	flist = flowtable_list(ft, hash);

	if (SLIST_EMPTY(flist)) {
		bit_set(mask, (hash % ft->ft_size));
		SLIST_INSERT_HEAD(flist, fle, f_next);
		goto skip;
	}

	/*
	 * find end of list and make sure that we were not
	 * preempted by another thread handling this flow
	 */
	SLIST_FOREACH(iter, flist, f_next) {
		KASSERT(iter->f_hash % ft->ft_size == hash % ft->ft_size,
		    ("%s: wrong hash", __func__));
		if (flow_matches(iter, key, keylen, fibnum)) {
			/*
			 * We probably migrated to an other CPU after
			 * lookup in flowtable_lookup_common() failed.
			 * It appeared that this CPU already has flow
			 * entry.
			 */
			iter->f_uptime = time_uptime;
#ifdef FLOWTABLE_HASH_ALL
			iter->f_flags |= fibnum >> 24;
#endif
			critical_exit();
			FLOWSTAT_INC(ft, ft_collisions);
			uma_zfree(flow_zone, fle);
			return (iter);
		}
	}

	SLIST_INSERT_HEAD(flist, fle, f_next);
skip:
	critical_exit();
	FLOWSTAT_INC(ft, ft_inserts);

	return (fle);
}

int
flowtable_lookup(sa_family_t sa, struct mbuf *m, struct route *ro)
{
	struct flentry *fle;
	struct llentry *lle;

	if (V_flowtable_enable == 0)
		return (ENXIO);

	switch (sa) {
#ifdef INET
	case AF_INET:
		fle = flowtable_lookup_ipv4(m, ro);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		fle = flowtable_lookup_ipv6(m, ro);
		break;
#endif
	default:
		panic("%s: sa %d", __func__, sa);
	}

	if (fle == NULL)
		return (EHOSTUNREACH);

	if (M_HASHTYPE_GET(m) == M_HASHTYPE_NONE) {
		M_HASHTYPE_SET(m, M_HASHTYPE_OPAQUE_HASH);
		m->m_pkthdr.flowid = fle->f_hash;
	}

	ro->ro_rt = fle->f_rt;
	ro->ro_flags |= RT_NORTREF;
	lle = fle->f_lle;
	if (lle != NULL && (lle->la_flags & LLE_VALID))
		ro->ro_lle = lle;	/* share ref with fle->f_lle */

	return (0);
}

static struct flentry *
flowtable_lookup_common(struct flowtable *ft, uint32_t *key, int keylen,
    uint32_t fibnum)
{
	struct flist *flist;
	struct flentry *fle;
	uint32_t hash;

	FLOWSTAT_INC(ft, ft_lookups);

	hash = jenkins_hash32(key, keylen / sizeof(uint32_t), flow_hashjitter);

	critical_enter();
	flist = flowtable_list(ft, hash);
	SLIST_FOREACH(fle, flist, f_next) {
		KASSERT(fle->f_hash % ft->ft_size == hash % ft->ft_size,
		    ("%s: wrong hash", __func__));
		if (flow_matches(fle, key, keylen, fibnum)) {
			fle->f_uptime = time_uptime;
#ifdef FLOWTABLE_HASH_ALL
			fle->f_flags |= fibnum >> 24;
#endif
			critical_exit();
			FLOWSTAT_INC(ft, ft_hits);
			return (fle);
		}
	}
	critical_exit();

	FLOWSTAT_INC(ft, ft_misses);

	return (flowtable_insert(ft, hash, key, keylen, fibnum));
}

static void
flowtable_alloc(struct flowtable *ft)
{

	ft->ft_table = malloc(ft->ft_size * sizeof(struct flist),
	    M_FTABLE, M_WAITOK);
	for (int i = 0; i < ft->ft_size; i++)
		ft->ft_table[i] = uma_zalloc(pcpu_zone_ptr, M_WAITOK | M_ZERO);

	ft->ft_masks = uma_zalloc(pcpu_zone_ptr, M_WAITOK);
	for (int i = 0; i < mp_ncpus; i++) {
		bitstr_t **b;

		b = zpcpu_get_cpu(ft->ft_masks, i);
		*b = bit_alloc(ft->ft_size, M_FTABLE, M_WAITOK);
	}
	ft->ft_tmpmask = bit_alloc(ft->ft_size, M_FTABLE, M_WAITOK);
}

static void
flowtable_free_stale(struct flowtable *ft, struct rtentry *rt, int maxidle)
{
	struct flist *flist, freelist;
	struct flentry *fle, *fle1, *fleprev;
	bitstr_t *mask, *tmpmask;
	int curbit, tmpsize;

	SLIST_INIT(&freelist);
	mask = flowtable_mask(ft);
	tmpmask = ft->ft_tmpmask;
	tmpsize = ft->ft_size;
	memcpy(tmpmask, mask, ft->ft_size/8);
	curbit = 0;
	fleprev = NULL; /* pacify gcc */
	/*
	 * XXX Note to self, bit_ffs operates at the byte level
	 * and thus adds gratuitous overhead
	 */
	bit_ffs(tmpmask, ft->ft_size, &curbit);
	while (curbit != -1) {
		if (curbit >= ft->ft_size || curbit < -1) {
			log(LOG_ALERT,
			    "warning: bad curbit value %d \n",
			    curbit);
			break;
		}

		FLOWSTAT_INC(ft, ft_free_checks);

		critical_enter();
		flist = flowtable_list(ft, curbit);
#ifdef DIAGNOSTIC
		if (SLIST_EMPTY(flist) && curbit > 0) {
			log(LOG_ALERT,
			    "warning bit=%d set, but no fle found\n",
			    curbit);
		}
#endif
		SLIST_FOREACH_SAFE(fle, flist, f_next, fle1) {
			if (rt != NULL && fle->f_rt != rt) {
				fleprev = fle;
				continue;
			}
			if (!flow_stale(ft, fle, maxidle)) {
				fleprev = fle;
				continue;
			}

			if (fle == SLIST_FIRST(flist))
				SLIST_REMOVE_HEAD(flist, f_next);
			else
				SLIST_REMOVE_AFTER(fleprev, f_next);
			SLIST_INSERT_HEAD(&freelist, fle, f_next);
		}
		if (SLIST_EMPTY(flist))
			bit_clear(mask, curbit);
		critical_exit();

		bit_clear(tmpmask, curbit);
		bit_ffs(tmpmask, tmpsize, &curbit);
	}

	SLIST_FOREACH_SAFE(fle, &freelist, f_next, fle1) {
		FLOWSTAT_INC(ft, ft_frees);
		if (fle->f_rt != NULL)
			RTFREE(fle->f_rt);
		if (fle->f_lle != NULL)
			LLE_FREE(fle->f_lle);
		uma_zfree(flow_zone, fle);
	}
}

static void
flowtable_clean_vnet(struct flowtable *ft, struct rtentry *rt, int maxidle)
{
	int i;

	CPU_FOREACH(i) {
		if (smp_started == 1) {
			thread_lock(curthread);
			sched_bind(curthread, i);
			thread_unlock(curthread);
		}

		flowtable_free_stale(ft, rt, maxidle);

		if (smp_started == 1) {
			thread_lock(curthread);
			sched_unbind(curthread);
			thread_unlock(curthread);
		}
	}
}

void
flowtable_route_flush(sa_family_t sa, struct rtentry *rt)
{
	struct flowtable *ft;

	switch (sa) {
#ifdef INET
	case AF_INET:
		ft = &V_ip4_ft;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		ft = &V_ip6_ft;
		break;
#endif
	default:
		panic("%s: sa %d", __func__, sa);
	}

	flowtable_clean_vnet(ft, rt, 0);
}

static void
flowtable_cleaner(void)
{
	VNET_ITERATOR_DECL(vnet_iter);
	struct thread *td;

	if (bootverbose)
		log(LOG_INFO, "flowtable cleaner started\n");
	td = curthread;
	while (1) {
		uint32_t flowclean_freq, maxidle;

		/*
		 * The maximum idle time, as well as frequency are arbitrary.
		 */
		if (flow_full())
			maxidle = 5;
		else
			maxidle = 30;

		VNET_LIST_RLOCK();
		VNET_FOREACH(vnet_iter) {
			CURVNET_SET(vnet_iter);
#ifdef INET
			flowtable_clean_vnet(&V_ip4_ft, NULL, maxidle);
#endif
#ifdef INET6
			flowtable_clean_vnet(&V_ip6_ft, NULL, maxidle);
#endif
			CURVNET_RESTORE();
		}
		VNET_LIST_RUNLOCK();

		if (flow_full())
			flowclean_freq = 4*hz;
		else
			flowclean_freq = 20*hz;
		mtx_lock(&flowclean_lock);
		thread_lock(td);
		sched_prio(td, PPAUSE);
		thread_unlock(td);
		flowclean_cycles++;
		cv_broadcast(&flowclean_f_cv);
		cv_timedwait(&flowclean_c_cv, &flowclean_lock, flowclean_freq);
		mtx_unlock(&flowclean_lock);
	}
}

static void
flowtable_flush(void *unused __unused)
{
	uint64_t start;

	mtx_lock(&flowclean_lock);
	start = flowclean_cycles;
	while (start == flowclean_cycles) {
		cv_broadcast(&flowclean_c_cv);
		cv_wait(&flowclean_f_cv, &flowclean_lock);
	}
	mtx_unlock(&flowclean_lock);
}

static struct kproc_desc flow_kp = {
	"flowcleaner",
	flowtable_cleaner,
	&flowcleanerproc
};
SYSINIT(flowcleaner, SI_SUB_KTHREAD_IDLE, SI_ORDER_ANY, kproc_start, &flow_kp);

static int
flowtable_get_size(char *name)
{
	int size;

	if (TUNABLE_INT_FETCH(name, &size)) {
		if (size < 256)
			size = 256;
		if (!powerof2(size)) {
			printf("%s must be power of 2\n", name);
			size = 2048;
		}
	} else {
		/*
		 * round up to the next power of 2
		 */
		size = 1 << fls((1024 + maxusers * 64) - 1);
	}

	return (size);
}

static void
flowtable_init(const void *unused __unused)
{

	flow_hashjitter = arc4random();

	flow_zone = uma_zcreate("flows", sizeof(struct flentry),
	    NULL, NULL, NULL, NULL, (64-1), UMA_ZONE_MAXBUCKET);
	uma_zone_set_max(flow_zone, 1024 + maxusers * 64 * mp_ncpus);

	cv_init(&flowclean_c_cv, "c_flowcleanwait");
	cv_init(&flowclean_f_cv, "f_flowcleanwait");
	mtx_init(&flowclean_lock, "flowclean lock", NULL, MTX_DEF);
	EVENTHANDLER_REGISTER(ifnet_departure_event, flowtable_flush, NULL,
	    EVENTHANDLER_PRI_ANY);
}
SYSINIT(flowtable_init, SI_SUB_PROTO_BEGIN, SI_ORDER_FIRST,
    flowtable_init, NULL);

#ifdef INET
static SYSCTL_NODE(_net_flowtable, OID_AUTO, ip4, CTLFLAG_RD, NULL,
    "Flowtable for IPv4");

static VNET_PCPUSTAT_DEFINE(struct flowtable_stat, ip4_ftstat);
VNET_PCPUSTAT_SYSINIT(ip4_ftstat);
VNET_PCPUSTAT_SYSUNINIT(ip4_ftstat);
SYSCTL_VNET_PCPUSTAT(_net_flowtable_ip4, OID_AUTO, stat, struct flowtable_stat,
    ip4_ftstat, "Flowtable statistics for IPv4 "
    "(struct flowtable_stat, net/flowtable.h)");

static void
flowtable_init_vnet_v4(const void *unused __unused)
{

	V_ip4_ft.ft_size = flowtable_get_size("net.flowtable.ip4.size");
	V_ip4_ft.ft_stat = VNET(ip4_ftstat);
	flowtable_alloc(&V_ip4_ft);
}
VNET_SYSINIT(ft_vnet_v4, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    flowtable_init_vnet_v4, NULL);
#endif /* INET */

#ifdef INET6
static SYSCTL_NODE(_net_flowtable, OID_AUTO, ip6, CTLFLAG_RD, NULL,
    "Flowtable for IPv6");

static VNET_PCPUSTAT_DEFINE(struct flowtable_stat, ip6_ftstat);
VNET_PCPUSTAT_SYSINIT(ip6_ftstat);
VNET_PCPUSTAT_SYSUNINIT(ip6_ftstat);
SYSCTL_VNET_PCPUSTAT(_net_flowtable_ip6, OID_AUTO, stat, struct flowtable_stat,
    ip6_ftstat, "Flowtable statistics for IPv6 "
    "(struct flowtable_stat, net/flowtable.h)");

static void
flowtable_init_vnet_v6(const void *unused __unused)
{

	V_ip6_ft.ft_size = flowtable_get_size("net.flowtable.ip6.size");
	V_ip6_ft.ft_stat = VNET(ip6_ftstat);
	flowtable_alloc(&V_ip6_ft);
}
VNET_SYSINIT(flowtable_init_vnet_v6, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    flowtable_init_vnet_v6, NULL);
#endif /* INET6 */

#ifdef DDB
static bitstr_t *
flowtable_mask_pcpu(struct flowtable *ft, int cpuid)
{

	return (zpcpu_get_cpu(*ft->ft_masks, cpuid));
}

static struct flist *
flowtable_list_pcpu(struct flowtable *ft, uint32_t hash, int cpuid)
{

	return (zpcpu_get_cpu(&ft->ft_table[hash % ft->ft_size], cpuid));
}

static void
flow_show(struct flowtable *ft, struct flentry *fle)
{
	int idle_time;
	int rt_valid, ifp_valid;
	volatile struct rtentry *rt;
	struct ifnet *ifp = NULL;
	uint32_t *hashkey = fle->f_key;

	idle_time = (int)(time_uptime - fle->f_uptime);
	rt = fle->f_rt;
	rt_valid = rt != NULL;
	if (rt_valid)
		ifp = rt->rt_ifp;
	ifp_valid = ifp != NULL;

#ifdef INET
	if (ft == &V_ip4_ft) {
		char daddr[4*sizeof "123"];
#ifdef FLOWTABLE_HASH_ALL
		char saddr[4*sizeof "123"];
		uint16_t sport, dport;
#endif

		inet_ntoa_r(*(struct in_addr *) &hashkey[0], daddr);
#ifdef FLOWTABLE_HASH_ALL
		inet_ntoa_r(*(struct in_addr *) &hashkey[1], saddr);
		dport = ntohs((uint16_t)(hashkey[2] >> 16));
		sport = ntohs((uint16_t)(hashkey[2] & 0xffff));
		db_printf("%s:%d->%s:%d", saddr, sport, daddr, dport);
#else
		db_printf("%s ", daddr);
#endif
	}
#endif /* INET */
#ifdef INET6
	if (ft == &V_ip6_ft) {
#ifdef FLOWTABLE_HASH_ALL
		db_printf("\n\tkey=%08x:%08x:%08x%08x:%08x:%08x%08x:%08x:%08x",
		    hashkey[0], hashkey[1], hashkey[2],
		    hashkey[3], hashkey[4], hashkey[5],
		    hashkey[6], hashkey[7], hashkey[8]);
#else
		db_printf("\n\tkey=%08x:%08x:%08x ",
		    hashkey[0], hashkey[1], hashkey[2]);
#endif
	}
#endif /* INET6 */

	db_printf("hash=%08x idle_time=%03d"
	    "\n\tfibnum=%02d rt=%p",
	    fle->f_hash, idle_time, fle->f_fibnum, fle->f_rt);

#ifdef FLOWTABLE_HASH_ALL
	if (fle->f_flags & FL_STALE)
		db_printf(" FL_STALE ");
#endif
	if (rt_valid) {
		if (rt->rt_flags & RTF_UP)
			db_printf(" RTF_UP ");
	}
	if (ifp_valid) {
		if (ifp->if_flags & IFF_LOOPBACK)
			db_printf(" IFF_LOOPBACK ");
		if (ifp->if_flags & IFF_UP)
			db_printf(" IFF_UP ");
		if (ifp->if_flags & IFF_POINTOPOINT)
			db_printf(" IFF_POINTOPOINT ");
	}
	db_printf("\n");
}

static void
flowtable_show(struct flowtable *ft, int cpuid)
{
	int curbit = 0;
	bitstr_t *mask, *tmpmask;

	if (cpuid != -1)
		db_printf("cpu: %d\n", cpuid);
	mask = flowtable_mask_pcpu(ft, cpuid);
	tmpmask = ft->ft_tmpmask;
	memcpy(tmpmask, mask, ft->ft_size/8);
	/*
	 * XXX Note to self, bit_ffs operates at the byte level
	 * and thus adds gratuitous overhead
	 */
	bit_ffs(tmpmask, ft->ft_size, &curbit);
	while (curbit != -1) {
		struct flist *flist;
		struct flentry *fle;

		if (curbit >= ft->ft_size || curbit < -1) {
			db_printf("warning: bad curbit value %d \n",
			    curbit);
			break;
		}

		flist = flowtable_list_pcpu(ft, curbit, cpuid);

		SLIST_FOREACH(fle, flist, f_next)
			flow_show(ft, fle);
		bit_clear(tmpmask, curbit);
		bit_ffs(tmpmask, ft->ft_size, &curbit);
	}
}

static void
flowtable_show_vnet(struct flowtable *ft)
{

	int i;

	CPU_FOREACH(i)
		flowtable_show(ft, i);
}

DB_SHOW_COMMAND(flowtables, db_show_flowtables)
{
	VNET_ITERATOR_DECL(vnet_iter);

	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
#ifdef VIMAGE
		db_printf("vnet %p\n", vnet_iter);
#endif
#ifdef INET
		printf("IPv4:\n");
		flowtable_show_vnet(&V_ip4_ft);
#endif
#ifdef INET6
		printf("IPv6:\n");
		flowtable_show_vnet(&V_ip6_ft);
#endif
		CURVNET_RESTORE();
	}
}
#endif
