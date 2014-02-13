/**************************************************************************

Copyright (c) 2008-2010, BitGravity Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the BitGravity Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

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
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/sctp.h>

#include <ddb/ddb.h>

#ifdef INET
struct ipv4_tuple {
	uint16_t 	ip_sport;	/* source port */
	uint16_t 	ip_dport;	/* destination port */
	in_addr_t 	ip_saddr;	/* source address */
	in_addr_t 	ip_daddr;	/* destination address */
};

union ipv4_flow {
	struct ipv4_tuple ipf_ipt;
	uint32_t 	ipf_key[3];
};
#endif

#ifdef INET6
struct ipv6_tuple {
	uint16_t 	ip_sport;	/* source port */
	uint16_t 	ip_dport;	/* destination port */
	struct in6_addr	ip_saddr;	/* source address */
	struct in6_addr	ip_daddr;	/* destination address */
};

union ipv6_flow {
	struct ipv6_tuple ipf_ipt;
	uint32_t 	ipf_key[9];
};
#endif

struct flentry {
	uint32_t		f_fhash;	/* hash flowing forward */
	uint16_t		f_flags;	/* flow flags */
	uint8_t			f_pad;
	uint8_t			f_proto;	/* protocol */
	uint32_t		f_fibnum;	/* fib index */
	uint32_t		f_uptime;	/* uptime at last access */
	SLIST_ENTRY(flentry)	f_next;		/* pointer to collision entry */
	struct rtentry		*f_rt;		/* rtentry for flow */
	struct llentry		*f_lle;		/* llentry for flow */
	union {
#ifdef INET
		union ipv4_flow	v4;
#endif
#ifdef INET6
		union ipv6_flow	v6;
#endif
	} f_flow;
#define	f_flow4	f_flow.v4
#define	f_flow6	f_flow.v6
};
#define	KEYLEN(flags)	((((flags) & FL_IPV6) ? 9 : 3) * 4)

/* Make sure f_flow begins with key. */
#ifdef INET
CTASSERT(offsetof(struct flentry, f_flow) ==
    offsetof(struct flentry, f_flow4.ipf_key));
#endif
#ifdef INET6
CTASSERT(offsetof(struct flentry, f_flow) ==
    offsetof(struct flentry, f_flow6.ipf_key));
#endif

SLIST_HEAD(flist, flentry);
/* Make sure we can use pcpu_zone_ptr for struct flist. */
CTASSERT(sizeof(struct flist) == sizeof(void *));

#define	SECS_PER_HOUR		3600
#define	SECS_PER_DAY		(24*SECS_PER_HOUR)

#define	SYN_IDLE		300
#define	UDP_IDLE		300
#define	FIN_WAIT_IDLE		600
#define	TCP_IDLE		SECS_PER_DAY

struct flowtable {
	counter_u64_t	*ft_stat;
	int 		ft_size;
	uint32_t	ft_flags;
	uint32_t	ft_max_depth;

	/*
	 * ft_table is a malloc(9)ed array of pointers.  Pointers point to
	 * memory from UMA_ZONE_PCPU zone.
	 * ft_masks is per-cpu pointer itself.  Each instance points
	 * to a malloc(9)ed bitset, that is private to corresponding CPU.
	 */
	struct flist	**ft_table;
	bitstr_t 	**ft_masks;
	bitstr_t	*ft_tmpmask;

	uint32_t	ft_udp_idle;
	uint32_t	ft_fin_wait_idle;
	uint32_t	ft_syn_idle;
	uint32_t	ft_tcp_idle;
	boolean_t	ft_full;
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
static uint32_t		flowclean_freq;

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
static VNET_DEFINE(int, flowtable_syn_expire) = SYN_IDLE;
static VNET_DEFINE(int, flowtable_udp_expire) = UDP_IDLE;
static VNET_DEFINE(int, flowtable_fin_wait_expire) = FIN_WAIT_IDLE;
static VNET_DEFINE(int, flowtable_tcp_expire) = TCP_IDLE;

#define	V_flowtable_enable		VNET(flowtable_enable)
#define	V_flowtable_syn_expire		VNET(flowtable_syn_expire)
#define	V_flowtable_udp_expire		VNET(flowtable_udp_expire)
#define	V_flowtable_fin_wait_expire	VNET(flowtable_fin_wait_expire)
#define	V_flowtable_tcp_expire		VNET(flowtable_tcp_expire)

static SYSCTL_NODE(_net, OID_AUTO, flowtable, CTLFLAG_RD, NULL,
    "flowtable");
SYSCTL_VNET_INT(_net_flowtable, OID_AUTO, enable, CTLFLAG_RW,
    &VNET_NAME(flowtable_enable), 0, "enable flowtable caching.");
SYSCTL_UMA_MAX(_net_flowtable, OID_AUTO, maxflows, CTLFLAG_RW,
    &flow_zone, "Maximum number of flows allowed");

/*
 * XXX This does not end up updating timeouts at runtime
 * and only reflects the value for the last table added :-/
 */
SYSCTL_VNET_INT(_net_flowtable, OID_AUTO, syn_expire, CTLFLAG_RW,
    &VNET_NAME(flowtable_syn_expire), 0,
    "seconds after which to remove syn allocated flow.");
SYSCTL_VNET_INT(_net_flowtable, OID_AUTO, udp_expire, CTLFLAG_RW,
    &VNET_NAME(flowtable_udp_expire), 0,
    "seconds after which to remove flow allocated to UDP.");
SYSCTL_VNET_INT(_net_flowtable, OID_AUTO, fin_wait_expire, CTLFLAG_RW,
    &VNET_NAME(flowtable_fin_wait_expire), 0,
    "seconds after which to remove a flow in FIN_WAIT.");
SYSCTL_VNET_INT(_net_flowtable, OID_AUTO, tcp_expire, CTLFLAG_RW,
    &VNET_NAME(flowtable_tcp_expire), 0,
    "seconds after which to remove flow allocated to a TCP connection.");

#define FL_STALE 	(1<<8)

static MALLOC_DEFINE(M_FTABLE, "flowtable", "flowtable hashes and bitstrings");

static struct flentry *flowtable_lookup_common(struct flowtable *,
    struct sockaddr_storage *, struct sockaddr_storage *, struct mbuf *, int);

static __inline int
proto_to_flags(uint8_t proto)
{
	int flag;

	switch (proto) {
	case IPPROTO_TCP:
		flag = FL_TCP;
		break;
	case IPPROTO_SCTP:
		flag = FL_SCTP;
		break;
	case IPPROTO_UDP:
		flag = FL_UDP;
		break;
	default:
		flag = 0;
		break;
	}

	return (flag);
}

static __inline int
flags_to_proto(int flags)
{
	int proto, protoflags;

	protoflags = flags & (FL_TCP|FL_SCTP|FL_UDP);
	switch (protoflags) {
	case FL_TCP:
		proto = IPPROTO_TCP;
		break;
	case FL_SCTP:
		proto = IPPROTO_SCTP;
		break;
	case FL_UDP:
		proto = IPPROTO_UDP;
		break;
	default:
		proto = 0;
		break;
	}
	return (proto);
}

#ifdef INET
static int
ipv4_mbuf_demarshal(struct mbuf *m, struct sockaddr_in *ssin,
    struct sockaddr_in *dsin, uint16_t *flags)
{
	struct ip *ip;
	uint8_t proto;
	int iphlen;
	struct tcphdr *th;
	struct udphdr *uh;
	struct sctphdr *sh;
	uint16_t sport, dport;

	proto = sport = dport = 0;
	ip = mtod(m, struct ip *);
	dsin->sin_family = AF_INET;
	dsin->sin_len = sizeof(*dsin);
	dsin->sin_addr = ip->ip_dst;
	ssin->sin_family = AF_INET;
	ssin->sin_len = sizeof(*ssin);
	ssin->sin_addr = ip->ip_src;

	proto = ip->ip_p;
	if ((*flags & FL_HASH_ALL) == 0)
		goto skipports;

	iphlen = ip->ip_hl << 2; /* XXX options? */

	switch (proto) {
	case IPPROTO_TCP:
		th = (struct tcphdr *)((caddr_t)ip + iphlen);
		sport = th->th_sport;
		dport = th->th_dport;
		if ((*flags & FL_HASH_ALL) &&
		    (th->th_flags & (TH_RST|TH_FIN)))
			*flags |= FL_STALE;
		break;
	case IPPROTO_UDP:
		uh = (struct udphdr *)((caddr_t)ip + iphlen);
		sport = uh->uh_sport;
		dport = uh->uh_dport;
		break;
	case IPPROTO_SCTP:
		sh = (struct sctphdr *)((caddr_t)ip + iphlen);
		sport = sh->src_port;
		dport = sh->dest_port;
		break;
	default:
		return (ENOTSUP);
		/* no port - hence not a protocol we care about */
		break;

	}

skipports:
	*flags |= proto_to_flags(proto);
	ssin->sin_port = sport;
	dsin->sin_port = dport;
	return (0);
}

static uint32_t
ipv4_flow_lookup_hash(
	struct sockaddr_in *ssin, struct sockaddr_in *dsin,
	    uint32_t *key, uint16_t flags)
{
	uint16_t sport, dport;
	uint8_t proto;
	int offset = 0;

	proto = flags_to_proto(flags);
	sport = dport = key[2] = key[1] = key[0] = 0;
	if ((ssin != NULL) && (flags & FL_HASH_ALL)) {
		key[1] = ssin->sin_addr.s_addr;
		sport = ssin->sin_port;
	}
	if (dsin != NULL) {
		key[2] = dsin->sin_addr.s_addr;
		dport = dsin->sin_port;
	}
	if (flags & FL_HASH_ALL) {
		((uint16_t *)key)[0] = sport;
		((uint16_t *)key)[1] = dport;
	} else
		offset = flow_hashjitter + proto;

	return (jenkins_hash32(key, 3, offset));
}

static struct flentry *
flowtable_lookup_ipv4(struct mbuf *m)
{
	struct sockaddr_storage ssa, dsa;
	uint16_t flags;
	struct sockaddr_in *dsin, *ssin;

	dsin = (struct sockaddr_in *)&dsa;
	ssin = (struct sockaddr_in *)&ssa;
	bzero(dsin, sizeof(*dsin));
	bzero(ssin, sizeof(*ssin));
	flags = V_ip4_ft.ft_flags;
	if (ipv4_mbuf_demarshal(m, ssin, dsin, &flags) != 0)
		return (NULL);

	return (flowtable_lookup_common(&V_ip4_ft, &ssa, &dsa, m, flags));
}

void
flow_to_route(struct flentry *fle, struct route *ro)
{
	uint32_t *hashkey = NULL;
	struct sockaddr_in *sin;

	sin = (struct sockaddr_in *)&ro->ro_dst;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	hashkey = fle->f_flow4.ipf_key;
	sin->sin_addr.s_addr = hashkey[2];
	ro->ro_rt = fle->f_rt;
	ro->ro_lle = fle->f_lle;
	ro->ro_flags |= RT_NORTREF;
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
	if ((m)->m_len < x) {						\
		goto receive_failed;					\
	}								\
	p = (mtod(m, char *) + (_len));					\
} while (0)

#define	TCP(p)		((struct tcphdr *)(p))
#define	SCTP(p)		((struct sctphdr *)(p))
#define	UDP(p)		((struct udphdr *)(p))

static int
ipv6_mbuf_demarshal(struct mbuf *m, struct sockaddr_in6 *ssin6,
    struct sockaddr_in6 *dsin6, uint16_t *flags)
{
	struct ip6_hdr *ip6;
	uint8_t proto;
	int hlen;
	uint16_t src_port, dst_port;
	u_short offset;
	void *ulp;

	offset = hlen = src_port = dst_port = 0;
	ulp = NULL;
	ip6 = mtod(m, struct ip6_hdr *);
	hlen = sizeof(struct ip6_hdr);
	proto = ip6->ip6_nxt;

	if ((*flags & FL_HASH_ALL) == 0)
		goto skipports;

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
			dst_port = TCP(ulp)->th_dport;
			src_port = TCP(ulp)->th_sport;
			if ((*flags & FL_HASH_ALL) &&
			    (TCP(ulp)->th_flags & (TH_RST|TH_FIN)))
				*flags |= FL_STALE;
			break;
		case IPPROTO_SCTP:
			PULLUP_TO(hlen, ulp, struct sctphdr);
			src_port = SCTP(ulp)->src_port;
			dst_port = SCTP(ulp)->dest_port;
			break;
		case IPPROTO_UDP:
			PULLUP_TO(hlen, ulp, struct udphdr);
			dst_port = UDP(ulp)->uh_dport;
			src_port = UDP(ulp)->uh_sport;
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

	if (src_port == 0) {
	receive_failed:
		return (ENOTSUP);
	}

skipports:
	dsin6->sin6_family = AF_INET6;
	dsin6->sin6_len = sizeof(*dsin6);
	dsin6->sin6_port = dst_port;
	memcpy(&dsin6->sin6_addr, &ip6->ip6_dst, sizeof(struct in6_addr));

	ssin6->sin6_family = AF_INET6;
	ssin6->sin6_len = sizeof(*ssin6);
	ssin6->sin6_port = src_port;
	memcpy(&ssin6->sin6_addr, &ip6->ip6_src, sizeof(struct in6_addr));
	*flags |= proto_to_flags(proto);

	return (0);
}

#define zero_key(key) 		\
do {				\
	key[0] = 0;		\
	key[1] = 0;		\
	key[2] = 0;		\
	key[3] = 0;		\
	key[4] = 0;		\
	key[5] = 0;		\
	key[6] = 0;		\
	key[7] = 0;		\
	key[8] = 0;		\
} while (0)

static uint32_t
ipv6_flow_lookup_hash(
	struct sockaddr_in6 *ssin6, struct sockaddr_in6 *dsin6,
	    uint32_t *key, uint16_t flags)
{
	uint16_t sport, dport;
	uint8_t proto;
	int offset = 0;

	proto = flags_to_proto(flags);
	zero_key(key);
	sport = dport = 0;
	if (dsin6 != NULL) {
		memcpy(&key[1], &dsin6->sin6_addr, sizeof(struct in6_addr));
		dport = dsin6->sin6_port;
	}
	if ((ssin6 != NULL) && (flags & FL_HASH_ALL)) {
		memcpy(&key[5], &ssin6->sin6_addr, sizeof(struct in6_addr));
		sport = ssin6->sin6_port;
	}
	if (flags & FL_HASH_ALL) {
		((uint16_t *)key)[0] = sport;
		((uint16_t *)key)[1] = dport;
	} else
		offset = flow_hashjitter + proto;

	return (jenkins_hash32(key, 9, offset));
}

static struct flentry *
flowtable_lookup_ipv6(struct mbuf *m)
{
	struct sockaddr_storage ssa, dsa;
	struct sockaddr_in6 *dsin6, *ssin6;
	uint16_t flags;

	dsin6 = (struct sockaddr_in6 *)&dsa;
	ssin6 = (struct sockaddr_in6 *)&ssa;
	bzero(dsin6, sizeof(*dsin6));
	bzero(ssin6, sizeof(*ssin6));
	flags = V_ip6_ft.ft_flags;

	if (ipv6_mbuf_demarshal(m, ssin6, dsin6, &flags) != 0)
		return (NULL);

	return (flowtable_lookup_common(&V_ip6_ft, &ssa, &dsa, m, flags));
}

void
flow_to_route_in6(struct flentry *fle, struct route_in6 *ro)
{
	uint32_t *hashkey = NULL;
	struct sockaddr_in6 *sin6;

	sin6 = (struct sockaddr_in6 *)&ro->ro_dst;

	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(*sin6);
	hashkey = fle->f_flow6.ipf_key;
	memcpy(&sin6->sin6_addr, &hashkey[5], sizeof (struct in6_addr));
	ro->ro_rt = fle->f_rt;
	ro->ro_lle = fle->f_lle;
	ro->ro_flags |= RT_NORTREF;
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
flow_stale(struct flowtable *ft, struct flentry *fle)
{
	time_t idle_time;

	if ((fle->f_fhash == 0)
	    || ((fle->f_rt->rt_flags & RTF_HOST) &&
		((fle->f_rt->rt_flags & (RTF_UP))
		    != (RTF_UP)))
	    || (fle->f_rt->rt_ifp == NULL)
	    || !RT_LINK_IS_UP(fle->f_rt->rt_ifp))
		return (1);

	idle_time = time_uptime - fle->f_uptime;

	if ((fle->f_flags & FL_STALE) ||
	    ((fle->f_flags & (TH_SYN|TH_ACK|TH_FIN)) == 0
		&& (idle_time > ft->ft_udp_idle)) ||
	    ((fle->f_flags & TH_FIN)
		&& (idle_time > ft->ft_fin_wait_idle)) ||
	    ((fle->f_flags & (TH_SYN|TH_ACK)) == TH_SYN
		&& (idle_time > ft->ft_syn_idle)) ||
	    ((fle->f_flags & (TH_SYN|TH_ACK)) == (TH_SYN|TH_ACK)
		&& (idle_time > ft->ft_tcp_idle)) ||
	    ((fle->f_rt->rt_flags & RTF_UP) == 0 ||
		(fle->f_rt->rt_ifp == NULL)))
		return (1);

	return (0);
}

static int
flow_full(struct flowtable *ft)
{
	boolean_t full;
	int count, max;

	full = ft->ft_full;
	count = uma_zone_get_cur(flow_zone);
	max = uma_zone_get_max(flow_zone);

	if (full && (count < (max - (max >> 3))))
		ft->ft_full = FALSE;
	else if (!full && (count > (max - (max >> 5))))
		ft->ft_full = TRUE;

	if (full && !ft->ft_full) {
		flowclean_freq = 4*hz;
		if ((ft->ft_flags & FL_HASH_ALL) == 0)
			ft->ft_udp_idle = ft->ft_fin_wait_idle =
			    ft->ft_syn_idle = ft->ft_tcp_idle = 5;
		cv_broadcast(&flowclean_c_cv);
	} else if (!full && ft->ft_full) {
		flowclean_freq = 20*hz;
		if ((ft->ft_flags & FL_HASH_ALL) == 0)
			ft->ft_udp_idle = ft->ft_fin_wait_idle =
			    ft->ft_syn_idle = ft->ft_tcp_idle = 30;
	}

	return (ft->ft_full);
}

static int
flowtable_insert(struct flowtable *ft, uint32_t hash, uint32_t *key,
    uint32_t fibnum, struct route *ro, uint16_t flags)
{
	struct flist *flist;
	struct flentry *fle, *iter;
	int depth;
	bitstr_t *mask;

	fle = uma_zalloc(flow_zone, M_NOWAIT | M_ZERO);
	if (fle == NULL)
		return (ENOMEM);

	bcopy(key, &fle->f_flow, KEYLEN(flags));
	fle->f_flags |= (flags & FL_IPV6);
	fle->f_proto = flags_to_proto(flags);
	fle->f_rt = ro->ro_rt;
	fle->f_lle = ro->ro_lle;
	fle->f_fhash = hash;
	fle->f_fibnum = fibnum;
	fle->f_uptime = time_uptime;

	critical_enter();
	mask = flowtable_mask(ft);
	flist = flowtable_list(ft, hash);

	if (SLIST_EMPTY(flist)) {
		bit_set(mask, (hash % ft->ft_size));
		SLIST_INSERT_HEAD(flist, fle, f_next);
		goto skip;
	}

	depth = 0;
	FLOWSTAT_INC(ft, ft_collisions);
	/*
	 * find end of list and make sure that we were not
	 * preempted by another thread handling this flow
	 */
	SLIST_FOREACH(iter, flist, f_next) {
		if (iter->f_fhash == hash && !flow_stale(ft, iter)) {
			/*
			 * there was either a hash collision
			 * or we lost a race to insert
			 */
			critical_exit();
			uma_zfree(flow_zone, fle);

			return (EEXIST);
		}
		depth++;
	}

	if (depth > ft->ft_max_depth)
		ft->ft_max_depth = depth;

	SLIST_INSERT_HEAD(flist, fle, f_next);
skip:
	critical_exit();

	return (0);
}

struct flentry *
flowtable_lookup(sa_family_t sa, struct mbuf *m)
{

	switch (sa) {
#ifdef INET
	case AF_INET:
		return (flowtable_lookup_ipv4(m));
#endif
#ifdef INET6
	case AF_INET6:
		return (flowtable_lookup_ipv6(m));
#endif
	default:
		panic("%s: sa %d", __func__, sa);
	}
}

static struct flentry *
flowtable_lookup_common(struct flowtable *ft, struct sockaddr_storage *ssa,
    struct sockaddr_storage *dsa, struct mbuf *m, int flags)
{
	struct route_in6 sro6;
	struct route sro, *ro;
	struct flist *flist;
	struct flentry *fle;
	struct rtentry *rt;
	struct llentry *lle;
	struct sockaddr_storage *l3addr;
	struct ifnet *ifp;
	uint32_t key[9], hash, fibnum;
	uint8_t proto;

	if (V_flowtable_enable == 0)
		return (NULL);

	sro.ro_rt = sro6.ro_rt = NULL;
	sro.ro_lle = sro6.ro_lle = NULL;
	flags |= ft->ft_flags;
	proto = flags_to_proto(flags);
	fibnum = M_GETFIB(m);

	switch (ssa->ss_family) {
#ifdef INET
	case AF_INET: {
		struct sockaddr_in *ssin, *dsin;

		KASSERT(dsa->ss_family == AF_INET,
		    ("%s: dsa family %d\n", __func__, dsa->ss_family));

		ro = &sro;
		memcpy(&ro->ro_dst, dsa, sizeof(struct sockaddr_in));
		/*
		 * The harvested source and destination addresses
		 * may contain port information if the packet is
		 * from a transport protocol (e.g. TCP/UDP). The
		 * port field must be cleared before performing
		 * a route lookup.
		 */
		((struct sockaddr_in *)&ro->ro_dst)->sin_port = 0;
		dsin = (struct sockaddr_in *)dsa;
		ssin = (struct sockaddr_in *)ssa;
		if ((dsin->sin_addr.s_addr == ssin->sin_addr.s_addr) ||
		    (ntohl(dsin->sin_addr.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET ||
		    (ntohl(ssin->sin_addr.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
			return (NULL);

		hash = ipv4_flow_lookup_hash(ssin, dsin, key, flags);
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		struct sockaddr_in6 *ssin6, *dsin6;

		KASSERT(dsa->ss_family == AF_INET6,
		    ("%s: dsa family %d\n", __func__, dsa->ss_family));

		ro = (struct route *)&sro6;
		memcpy(&sro6.ro_dst, dsa,
		    sizeof(struct sockaddr_in6));
		((struct sockaddr_in6 *)&ro->ro_dst)->sin6_port = 0;
		dsin6 = (struct sockaddr_in6 *)dsa;
		ssin6 = (struct sockaddr_in6 *)ssa;

		flags |= FL_IPV6;
		hash = ipv6_flow_lookup_hash(ssin6, dsin6, key, flags);
		break;
	}
#endif
	default:
		panic("%s: ssa family %d", __func__, ssa->ss_family);
	}

	/*
	 * Ports are zero and this isn't a transmit cache
	 * - thus not a protocol for which we need to keep
	 * state
	 * FL_HASH_ALL => key[0] != 0 for TCP || UDP || SCTP
	 */
	if (key[0] == 0 && (ft->ft_flags & FL_HASH_ALL))
		return (NULL);

	FLOWSTAT_INC(ft, ft_lookups);

	critical_enter();
	flist = flowtable_list(ft, hash);
	SLIST_FOREACH(fle, flist, f_next)
		if (fle->f_fhash == hash && bcmp(&fle->f_flow, key,
		    KEYLEN(fle->f_flags)) == 0 &&
		    proto == fle->f_proto && fibnum == fle->f_fibnum &&
		    (fle->f_rt->rt_flags & RTF_UP) &&
		    fle->f_rt->rt_ifp != NULL &&
		    (fle->f_lle->la_flags & LLE_VALID)) {
			fle->f_uptime = time_uptime;
			fle->f_flags |= flags;
			critical_exit();
			FLOWSTAT_INC(ft, ft_hits);
			goto success;
		}
	critical_exit();

	if (flags & FL_NOAUTO || flow_full(ft))
		return (NULL);

	FLOWSTAT_INC(ft, ft_misses);
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

	switch (ssa->ss_family) {
#ifdef INET
	case AF_INET:
		if (rt->rt_flags & RTF_GATEWAY)
			l3addr = (struct sockaddr_storage *)rt->rt_gateway;
		else
			l3addr = (struct sockaddr_storage *)&ro->ro_dst;
		lle = llentry_alloc(ifp, LLTABLE(ifp), l3addr);
		break;
#endif
#ifdef INET6
	case AF_INET6: {
		struct sockaddr_in6 *dsin6;

		dsin6 = (struct sockaddr_in6 *)dsa;
		if (in6_localaddr(&dsin6->sin6_addr)) {
			RTFREE(rt);
			return (NULL);
		}

		if (rt->rt_flags & RTF_GATEWAY)
			l3addr = (struct sockaddr_storage *)rt->rt_gateway;
		else
			l3addr = (struct sockaddr_storage *)&ro->ro_dst;
		lle = llentry_alloc(ifp, LLTABLE6(ifp), l3addr);
		break;
	}
#endif
	}

	if (lle == NULL) {
		RTFREE(rt);
		return (NULL);
	}
	ro->ro_lle = lle;

	if (flowtable_insert(ft, hash, key, fibnum, ro, flags) != 0) {
		RTFREE(rt);
		LLE_FREE(lle);
		return (NULL);
	}

success:
	if (fle != NULL && (m->m_flags & M_FLOWID) == 0) {
		m->m_flags |= M_FLOWID;
		m->m_pkthdr.flowid = fle->f_fhash;
	}
	return (fle);
}

/*
 * used by the bit_alloc macro
 */
#define calloc(count, size) malloc((count)*(size), M_FTABLE, M_WAITOK | M_ZERO) 
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
		*b = bit_alloc(ft->ft_size);
	}
	ft->ft_tmpmask = bit_alloc(ft->ft_size);

	/*
	 * In the local transmit case the table truly is
	 * just a cache - so everything is eligible for
	 * replacement after 5s of non-use
	 */
	if (ft->ft_flags & FL_HASH_ALL) {
		ft->ft_udp_idle = V_flowtable_udp_expire;
		ft->ft_syn_idle = V_flowtable_syn_expire;
		ft->ft_fin_wait_idle = V_flowtable_fin_wait_expire;
		ft->ft_tcp_idle = V_flowtable_fin_wait_expire;
	} else {
		ft->ft_udp_idle = ft->ft_fin_wait_idle =
		    ft->ft_syn_idle = ft->ft_tcp_idle = 30;

	}
}
#undef calloc

static void
flowtable_free_stale(struct flowtable *ft, struct rtentry *rt)
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
			if (!flow_stale(ft, fle)) {
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
		tmpmask += (curbit / 8);
		tmpsize -= (curbit / 8) * 8;
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
flowtable_clean_vnet(struct flowtable *ft, struct rtentry *rt)
{
	int i;

	CPU_FOREACH(i) {
		if (smp_started == 1) {
			thread_lock(curthread);
			sched_bind(curthread, i);
			thread_unlock(curthread);
		}

		flowtable_free_stale(ft, rt);

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

	flowtable_clean_vnet(ft, rt);
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
		VNET_LIST_RLOCK();
		VNET_FOREACH(vnet_iter) {
			CURVNET_SET(vnet_iter);
#ifdef INET
			flowtable_clean_vnet(&V_ip4_ft, NULL);
#endif
#ifdef INET6
			flowtable_clean_vnet(&V_ip6_ft, NULL);
#endif
			CURVNET_RESTORE();
		}
		VNET_LIST_RUNLOCK();

		/*
		 * The 10 second interval between cleaning checks
		 * is arbitrary
		 */
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
	    NULL, NULL, NULL, NULL, UMA_ALIGN_CACHE, UMA_ZONE_MAXBUCKET);
	uma_zone_set_max(flow_zone, 1024 + maxusers * 64 * mp_ncpus);

	cv_init(&flowclean_c_cv, "c_flowcleanwait");
	cv_init(&flowclean_f_cv, "f_flowcleanwait");
	mtx_init(&flowclean_lock, "flowclean lock", NULL, MTX_DEF);
	EVENTHANDLER_REGISTER(ifnet_departure_event, flowtable_flush, NULL,
	    EVENTHANDLER_PRI_ANY);
	flowclean_freq = 20*hz;
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
	uint16_t sport, dport;
	uint32_t *hashkey;
	char saddr[4*sizeof "123"], daddr[4*sizeof "123"];
	volatile struct rtentry *rt;
	struct ifnet *ifp = NULL;

	idle_time = (int)(time_uptime - fle->f_uptime);
	rt = fle->f_rt;
	rt_valid = rt != NULL;
	if (rt_valid)
		ifp = rt->rt_ifp;
	ifp_valid = ifp != NULL;
	hashkey = (uint32_t *)&fle->f_flow;
	if (fle->f_flags & FL_IPV6)
		goto skipaddr;

	inet_ntoa_r(*(struct in_addr *) &hashkey[2], daddr);
	if (ft->ft_flags & FL_HASH_ALL) {
		inet_ntoa_r(*(struct in_addr *) &hashkey[1], saddr);
		sport = ntohs(((uint16_t *)hashkey)[0]);
		dport = ntohs(((uint16_t *)hashkey)[1]);
		db_printf("%s:%d->%s:%d",
		    saddr, sport, daddr,
		    dport);
	} else
		db_printf("%s ", daddr);

skipaddr:
	if (fle->f_flags & FL_STALE)
		db_printf(" FL_STALE ");
	if (fle->f_flags & FL_TCP)
		db_printf(" FL_TCP ");
	if (fle->f_flags & FL_UDP)
		db_printf(" FL_UDP ");
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
	if (fle->f_flags & FL_IPV6)
		db_printf("\n\tkey=%08x:%08x:%08x%08x:%08x:%08x%08x:%08x:%08x",
		    hashkey[0], hashkey[1], hashkey[2],
		    hashkey[3], hashkey[4], hashkey[5],
		    hashkey[6], hashkey[7], hashkey[8]);
	else
		db_printf("\n\tkey=%08x:%08x:%08x ",
		    hashkey[0], hashkey[1], hashkey[2]);
	db_printf("hash=%08x idle_time=%03d"
	    "\n\tfibnum=%02d rt=%p",
	    fle->f_fhash, idle_time, fle->f_fibnum, fle->f_rt);
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
