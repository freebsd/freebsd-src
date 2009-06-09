/**************************************************************************

Copyright (c) 2008-2009, BitGravity Inc.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef NO_FLOWTABLE
#include <sys/param.h>  
#include <sys/types.h>
#include <sys/bitstring.h>
#include <sys/callout.h>
#include <sys/kernel.h>  
#include <sys/kthread.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/vimage.h>

#include <net/if.h>
#include <net/if_llatbl.h>
#include <net/if_var.h>
#include <net/route.h> 
#include <net/vnet.h>
#include <net/flowtable.h>


#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/sctp.h>

#include <libkern/jenkins.h>

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

struct flentry {
	volatile uint32_t	f_fhash;	/* hash flowing forward */
	uint16_t		f_flags;	/* flow flags */
	uint8_t			f_pad;		/* alignment */
	uint8_t			f_proto;	/* protocol */
	uint32_t		f_uptime;	/* uptime at last access */
	struct flentry		*f_next;	/* pointer to collision entry */
	volatile struct rtentry *f_rt;		/* rtentry for flow */
	volatile struct llentry *f_lle;		/* llentry for flow */
};

struct flentry_v4 {
	struct flentry	fl_entry;
	union ipv4_flow	fl_flow;
};

struct flentry_v6 {
	struct flentry	fl_entry;
	union ipv6_flow	fl_flow;
};

#define	fl_fhash	fl_entry.fl_fhash
#define	fl_flags	fl_entry.fl_flags
#define	fl_proto	fl_entry.fl_proto
#define	fl_uptime	fl_entry.fl_uptime
#define	fl_rt		fl_entry.fl_rt
#define	fl_lle		fl_entry.fl_lle

#define	SECS_PER_HOUR		3600
#define	SECS_PER_DAY		(24*SECS_PER_HOUR)

#define	SYN_IDLE		300
#define	UDP_IDLE		300
#define	FIN_WAIT_IDLE		600
#define	TCP_IDLE		SECS_PER_DAY


typedef	void fl_lock_t(struct flowtable *, uint32_t);
typedef void fl_rtalloc_t(struct route *, uint32_t, u_int);

union flentryp {
	struct flentry		**global;
	struct flentry		**pcpu[MAXCPU];
};

struct flowtable {
	int 		ft_size;
	int 		ft_lock_count;
	uint32_t	ft_flags;
	uint32_t	ft_collisions;
	uint32_t	ft_allocated;
	uint32_t	ft_misses;
	uint64_t	ft_hits;

	uint32_t	ft_udp_idle;
	uint32_t	ft_fin_wait_idle;
	uint32_t	ft_syn_idle;
	uint32_t	ft_tcp_idle;

	fl_lock_t	*ft_lock;
	fl_lock_t 	*ft_unlock;
	fl_rtalloc_t	*ft_rtalloc;
	struct mtx	*ft_locks;

	
	union flentryp	ft_table;
	bitstr_t 	*ft_masks[MAXCPU];
	bitstr_t	*ft_tmpmask;
	struct flowtable *ft_next;
};

static struct proc *flowcleanerproc;
static struct flowtable *flow_list_head;
static uint32_t hashjitter;
static uma_zone_t ipv4_zone;
static uma_zone_t ipv6_zone;

/*
 * TODO:
 * - Make flowtable stats per-cpu, aggregated at sysctl call time,
 *   to avoid extra cache evictions caused by incrementing a shared
 *   counter
 * - add IPv6 support to flow lookup
 * - add sysctls to resize && flush flow tables 
 * - Add per flowtable sysctls for statistics and configuring timeouts
 * - add saturation counter to rtentry to support per-packet load-balancing
 *   add flag to indicate round-robin flow, add list lookup from head
     for flows
 * - add sysctl / device node / syscall to support exporting and importing
 *   of flows with flag to indicate that a flow was imported so should
 *   not be considered for auto-cleaning
 * - support explicit connection state (currently only ad-hoc for DSR)
 */
SYSCTL_NODE(_net_inet, OID_AUTO, flowtable, CTLFLAG_RD, NULL, "flowtable");
int	flowtable_enable = 0;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, enable, CTLFLAG_RW,
    &flowtable_enable, 0, "enable flowtable caching.");
static int flowtable_hits = 0;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, hits, CTLFLAG_RD,
    &flowtable_hits, 0, "# flowtable hits.");
static int flowtable_lookups = 0;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, lookups, CTLFLAG_RD,
    &flowtable_lookups, 0, "# flowtable lookups.");
static int flowtable_misses = 0;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, misses, CTLFLAG_RD,
    &flowtable_misses, 0, "#flowtable misses.");
static int flowtable_frees = 0;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, frees, CTLFLAG_RD,
    &flowtable_frees, 0, "#flows freed.");
static int flowtable_free_checks = 0;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, free_checks, CTLFLAG_RD,
    &flowtable_free_checks, 0, "#flows free checks.");
static int flowtable_max_depth = 0;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, max_depth, CTLFLAG_RD,
    &flowtable_max_depth, 0, "max collision list length.");
static int flowtable_collisions = 0;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, collisions, CTLFLAG_RD,
    &flowtable_collisions, 0, "#flowtable collisions.");

/*
 * XXX This does not end up updating timeouts at runtime
 * and only reflects the value for the last table added :-/
 */
static int flowtable_syn_expire = SYN_IDLE;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, syn_expire, CTLFLAG_RW,
    &flowtable_syn_expire, 0, "seconds after which to remove syn allocated flow.");
static int flowtable_udp_expire = UDP_IDLE;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, udp_expire, CTLFLAG_RW,
    &flowtable_udp_expire, 0, "seconds after which to remove flow allocated to UDP.");
static int flowtable_fin_wait_expire = FIN_WAIT_IDLE;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, fin_wait_expire, CTLFLAG_RW,
    &flowtable_fin_wait_expire, 0, "seconds after which to remove a flow in FIN_WAIT.");
static int flowtable_tcp_expire = TCP_IDLE;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, tcp_expire, CTLFLAG_RW,
    &flowtable_tcp_expire, 0, "seconds after which to remove flow allocated to a TCP connection.");


/*
 * Maximum number of flows that can be allocated of a given type.
 *
 * The table is allocated at boot time (for the pure caching case
 * there is no reason why this could not be changed at runtime)
 * and thus (currently) needs to be set with a tunable.
 */
static int nmbflows = 4096;

static int
sysctl_nmbflows(SYSCTL_HANDLER_ARGS)
{
	int error, newnmbflows;

	newnmbflows = nmbflows;
	error = sysctl_handle_int(oidp, &newnmbflows, 0, req); 
	if (error == 0 && req->newptr) {
		if (newnmbflows > nmbflows) {
			nmbflows = newnmbflows;
			uma_zone_set_max(ipv4_zone, nmbflows);
			uma_zone_set_max(ipv6_zone, nmbflows);
		} else
			error = EINVAL;
	}
	return (error);
}
SYSCTL_PROC(_net_inet_flowtable, OID_AUTO, nmbflows, CTLTYPE_INT|CTLFLAG_RW,
    &nmbflows, 0, sysctl_nmbflows, "IU", "Maximum number of flows allowed");

#ifndef RADIX_MPATH
static void
in_rtalloc_ign_wrapper(struct route *ro, uint32_t hash, u_int fib)
{

	rtalloc_ign_fib(ro, 0, fib);
}
#endif

static void
flowtable_global_lock(struct flowtable *table, uint32_t hash)
{	
	int lock_index = (hash)&(table->ft_lock_count - 1);

	mtx_lock(&table->ft_locks[lock_index]);
}

static void
flowtable_global_unlock(struct flowtable *table, uint32_t hash)
{	
	int lock_index = (hash)&(table->ft_lock_count - 1);

	mtx_unlock(&table->ft_locks[lock_index]);
}

static void
flowtable_pcpu_lock(struct flowtable *table, uint32_t hash)
{

	critical_enter();
}

static void
flowtable_pcpu_unlock(struct flowtable *table, uint32_t hash)
{

	critical_exit();
}

#define FL_ENTRY_INDEX(table, hash)((hash) % (table)->ft_size)
#define FL_ENTRY(table, hash) *flowtable_entry((table), (hash))
#define FL_ENTRY_LOCK(table, hash)  (table)->ft_lock((table), (hash))
#define FL_ENTRY_UNLOCK(table, hash) (table)->ft_unlock((table), (hash))

#define FL_STALE (1<<8)
#define FL_IPV6  (1<<9)

static uint32_t
ipv4_flow_lookup_hash_internal(struct mbuf *m, struct route *ro,
    uint32_t *key, uint16_t *flags, uint8_t *protop)
{
	uint16_t sport = 0, dport = 0;
	struct ip *ip = NULL;
	uint8_t proto = 0;
	int iphlen;
	uint32_t hash;
	struct sockaddr_in *sin;
	struct tcphdr *th;
	struct udphdr *uh;
	struct sctphdr *sh;

	if (flowtable_enable == 0)
		return (0);

	key[1] = key[0] = 0;
	sin = (struct sockaddr_in *)&ro->ro_dst;
	if (m != NULL) {
		ip = mtod(m, struct ip *);
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = ip->ip_dst;
	} else
		*flags &= ~FL_HASH_PORTS;

	key[2] = sin->sin_addr.s_addr;

	if ((*flags & FL_HASH_PORTS) == 0)
		goto skipports;

	proto = ip->ip_p;
	iphlen = ip->ip_hl << 2; /* XXX options? */
	key[1] = ip->ip_src.s_addr;
	
	switch (proto) {
	case IPPROTO_TCP:
		th = (struct tcphdr *)((caddr_t)ip + iphlen);
		sport = ntohs(th->th_sport);
		dport = ntohs(th->th_dport);
		*flags |= th->th_flags;
		if (*flags & TH_RST)
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
		if (*flags & FL_HASH_PORTS)
			goto noop;
		/* no port - hence not a protocol we care about */
		break;;
	
	}
	*protop = proto;

	/*
	 * If this is a transmit route cache then 
	 * hash all flows to a given destination to
	 * the same bucket
	 */
	if ((*flags & FL_HASH_PORTS) == 0)
		proto = sport = dport = 0;

	((uint16_t *)key)[0] = sport;
	((uint16_t *)key)[1] = dport; 

skipports:
	hash = jenkins_hashword(key, 3, hashjitter + proto);
	if (m != NULL && (m->m_flags & M_FLOWID) == 0) {
		m->m_flags |= M_FLOWID;
		m->m_pkthdr.flowid = hash;
	}

	return (hash);
noop:
	*protop = proto;
	return (0);
}

static bitstr_t *
flowtable_mask(struct flowtable *ft)
{
	bitstr_t *mask;
	
	if (ft->ft_flags & FL_PCPU)
		mask = ft->ft_masks[curcpu];
	else
		mask = ft->ft_masks[0];

	return (mask);
}

static struct flentry **
flowtable_entry(struct flowtable *ft, uint32_t hash)
{
	struct flentry **fle;
	int index = (hash % ft->ft_size);

	if (ft->ft_flags & FL_PCPU) {
		KASSERT(&ft->ft_table.pcpu[curcpu][0] != NULL, ("pcpu not set"));
		fle = &ft->ft_table.pcpu[curcpu][index];
	} else {
		KASSERT(&ft->ft_table.global[0] != NULL, ("global not set"));
		fle = &ft->ft_table.global[index];
	}
	
	return (fle);
}

static int
flow_stale(struct flowtable *ft, struct flentry *fle)
{
	time_t idle_time;

	if ((fle->f_fhash == 0)
	    || ((fle->f_rt->rt_flags & RTF_HOST) &&
		((fle->f_rt->rt_flags & (RTF_UP))
		    != (RTF_UP)))
	    || (fle->f_rt->rt_ifp == NULL))
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

static void
flowtable_set_hashkey(struct flentry *fle, uint32_t *key)
{
	uint32_t *hashkey;
	int i, nwords;

	if (fle->f_flags & FL_IPV6) {
		nwords = 9;
		hashkey = ((struct flentry_v4 *)fle)->fl_flow.ipf_key;
	} else {
		nwords = 3;
		hashkey = ((struct flentry_v6 *)fle)->fl_flow.ipf_key;
	}
	
	for (i = 0; i < nwords; i++) 
		hashkey[i] = key[i];
}

static int
flowtable_insert(struct flowtable *ft, uint32_t hash, uint32_t *key,
    uint8_t proto, struct route *ro, uint16_t flags)
{
	struct flentry *fle, *fletail, *newfle, **flep;
	int depth;
	uma_zone_t flezone;
	bitstr_t *mask;

	flezone = (flags & FL_IPV6) ? ipv6_zone : ipv4_zone;
	newfle = uma_zalloc(flezone, M_NOWAIT | M_ZERO);
	if (newfle == NULL)
		return (ENOMEM);

	newfle->f_flags |= (flags & FL_IPV6);
	
	FL_ENTRY_LOCK(ft, hash);
	mask = flowtable_mask(ft);
	flep = flowtable_entry(ft, hash);
	fletail = fle = *flep;

	if (fle == NULL) {
		bit_set(mask, FL_ENTRY_INDEX(ft, hash));
		*flep = fle = newfle;
		goto skip;
	} 
	
	depth = 0;
	flowtable_collisions++;
	/*
	 * find end of list and make sure that we were not
	 * preempted by another thread handling this flow
	 */
	while (fle != NULL) {
		if (fle->f_fhash == hash && !flow_stale(ft, fle)) {
			/*
			 * there was either a hash collision
			 * or we lost a race to insert
			 */
			FL_ENTRY_UNLOCK(ft, hash);
			uma_zfree((newfle->f_flags & FL_IPV6) ?
			    ipv6_zone : ipv4_zone, newfle);
			return (EEXIST);
		}
		/*
		 * re-visit this double condition XXX
		 */
		if (fletail->f_next != NULL)
			fletail = fle->f_next;

		depth++;
		fle = fle->f_next;
	} 

	if (depth > flowtable_max_depth)
		flowtable_max_depth = depth;
	fletail->f_next = newfle;
	fle = newfle;
skip:
	flowtable_set_hashkey(fle, key);

	fle->f_proto = proto;
	fle->f_rt = ro->ro_rt;
	fle->f_lle = ro->ro_lle;
	fle->f_fhash = hash;
	fle->f_uptime = time_uptime;
	FL_ENTRY_UNLOCK(ft, hash);
	return (0);
}

static int
flowtable_key_equal(struct flentry *fle, uint32_t *key)
{
	uint32_t *hashkey;
	int i, nwords;

	if (fle->f_flags & FL_IPV6) {
		nwords = 9;
		hashkey = ((struct flentry_v4 *)fle)->fl_flow.ipf_key;
	} else {
		nwords = 3;
		hashkey = ((struct flentry_v6 *)fle)->fl_flow.ipf_key;
	}
	
	for (i = 0; i < nwords; i++) 
		if (hashkey[i] != key[i])
			return (0);

	return (1);
}

int
flowtable_lookup(struct flowtable *ft, struct mbuf *m, struct route *ro)
{
	uint32_t key[9], hash;
	struct flentry *fle;
	uint16_t flags;
	uint8_t proto = 0;
	int error = 0, fib = 0;
	struct rtentry *rt;
	struct llentry *lle;

	flags = ft->ft_flags;
	ro->ro_rt = NULL;
	ro->ro_lle = NULL;

	/*
	 * The internal hash lookup is the only IPv4 specific bit
	 * remaining
	 *
	 * XXX BZ: to add IPv6 support just add a check for the
	 * address type in m and ro and an equivalent ipv6 lookup
	 * function - the rest of the code should automatically
	 * handle an ipv6 flow (note that m can be NULL in which
	 * case ro will be set)
	 */
	hash = ipv4_flow_lookup_hash_internal(m, ro, key,
	    &flags, &proto);

	/*
	 * Ports are zero and this isn't a transmit cache
	 * - thus not a protocol for which we need to keep 
	 * state
	 * FL_HASH_PORTS => key[0] != 0 for TCP || UDP || SCTP
	 */
	if (hash == 0 || (key[0] == 0 && (ft->ft_flags & FL_HASH_PORTS)))
		return (ENOENT);

	flowtable_lookups++;
	FL_ENTRY_LOCK(ft, hash);
	if ((fle = FL_ENTRY(ft, hash)) == NULL) {
		FL_ENTRY_UNLOCK(ft, hash);
		goto uncached;
	}
keycheck:	
	rt = __DEVOLATILE(struct rtentry *, fle->f_rt);
	lle = __DEVOLATILE(struct llentry *, fle->f_lle);
	if ((rt != NULL)
	    && fle->f_fhash == hash
	    && flowtable_key_equal(fle, key)
	    && (proto == fle->f_proto)
	    && (rt->rt_flags & RTF_UP)
	    && (rt->rt_ifp != NULL)) {
		flowtable_hits++;
		fle->f_uptime = time_uptime;
		fle->f_flags |= flags;
		ro->ro_rt = rt;
		ro->ro_lle = lle;
		FL_ENTRY_UNLOCK(ft, hash);
		return (0);
	} else if (fle->f_next != NULL) {
		fle = fle->f_next;
		goto keycheck;
	}
	FL_ENTRY_UNLOCK(ft, hash);

uncached:
	flowtable_misses++;
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
	if (m != NULL)
		fib = M_GETFIB(m);

	ft->ft_rtalloc(ro, hash, fib);
	if (ro->ro_rt == NULL) 
		error = ENETUNREACH;
	else {
		struct llentry *lle = NULL;
		struct sockaddr *l3addr;
		struct rtentry *rt = ro->ro_rt;
		struct ifnet *ifp = rt->rt_ifp;

		if (rt->rt_flags & RTF_GATEWAY)
			l3addr = rt->rt_gateway;
		else
			l3addr = &ro->ro_dst;
		llentry_update(&lle, LLTABLE(ifp), l3addr, ifp);
		ro->ro_lle = lle;

		if (lle == NULL) {
			RTFREE(rt);
			ro->ro_rt = NULL;
			return (ENOENT);
		}
		error = flowtable_insert(ft, hash, key, proto,
		    ro, flags);
				
		if (error) {
			RTFREE(rt);
			LLE_FREE(lle);
			ro->ro_rt = NULL;
			ro->ro_lle = NULL;
		}
	} 

	return (error);
}

/*
 * used by the bit_alloc macro
 */
#define calloc(count, size) malloc((count)*(size), M_DEVBUF, M_WAITOK|M_ZERO)
	
struct flowtable *
flowtable_alloc(int nentry, int flags)
{
	struct flowtable *ft, *fttail;
	int i;

	if (hashjitter == 0)
		hashjitter = arc4random();

	KASSERT(nentry > 0, ("nentry must be > 0, is %d\n", nentry));

	ft = malloc(sizeof(struct flowtable),
	    M_RTABLE, M_WAITOK | M_ZERO);
	
	ft->ft_flags = flags;
	ft->ft_size = nentry;
#ifdef RADIX_MPATH
	ft->ft_rtalloc = rtalloc_mpath_fib;
#else
	ft->ft_rtalloc = in_rtalloc_ign_wrapper;
#endif
	if (flags & FL_PCPU) {
		ft->ft_lock = flowtable_pcpu_lock;
		ft->ft_unlock = flowtable_pcpu_unlock;

		for (i = 0; i <= mp_maxid; i++) {
			ft->ft_table.pcpu[i] =
			    malloc(nentry*sizeof(struct flentry *),
				M_RTABLE, M_WAITOK | M_ZERO);
			ft->ft_masks[i] = bit_alloc(nentry);
		}
	} else {
		ft->ft_lock_count = 2*(powerof2(mp_maxid + 1) ? (mp_maxid + 1):
		    (fls(mp_maxid + 1) << 1));
		
		ft->ft_lock = flowtable_global_lock;
		ft->ft_unlock = flowtable_global_unlock;
		ft->ft_table.global =
			    malloc(nentry*sizeof(struct flentry *),
				M_RTABLE, M_WAITOK | M_ZERO);
		ft->ft_locks = malloc(ft->ft_lock_count*sizeof(struct mtx),
				M_RTABLE, M_WAITOK | M_ZERO);
		for (i = 0; i < ft->ft_lock_count; i++)
			mtx_init(&ft->ft_locks[i], "flow", NULL, MTX_DEF|MTX_DUPOK);

		ft->ft_masks[0] = bit_alloc(nentry);
	}
	ft->ft_tmpmask = bit_alloc(nentry);

	/*
	 * In the local transmit case the table truly is 
	 * just a cache - so everything is eligible for
	 * replacement after 5s of non-use
	 */
	if (flags & FL_HASH_PORTS) {
		ft->ft_udp_idle = flowtable_udp_expire;
		ft->ft_syn_idle = flowtable_syn_expire;
		ft->ft_fin_wait_idle = flowtable_fin_wait_expire;
		ft->ft_tcp_idle = flowtable_fin_wait_expire;
	} else {
		ft->ft_udp_idle = ft->ft_fin_wait_idle =
		    ft->ft_syn_idle = ft->ft_tcp_idle = 30;
		
	}

	/*
	 * hook in to the cleaner list
	 */
	if (flow_list_head == NULL)
		flow_list_head = ft;
	else {
		fttail = flow_list_head;
		while (fttail->ft_next != NULL)
			fttail = fttail->ft_next;
		fttail->ft_next = ft;
	}

	return (ft);
}

static void
flowtable_setup(void *arg)
{

	ipv4_zone = uma_zcreate("ip4flow", sizeof(struct flentry_v4), NULL,
	    NULL, NULL, NULL, 64, UMA_ZONE_MAXBUCKET);
	ipv6_zone = uma_zcreate("ip6flow", sizeof(struct flentry_v6), NULL,
	    NULL, NULL, NULL, 64, UMA_ZONE_MAXBUCKET);	
	uma_zone_set_max(ipv4_zone, nmbflows);
	uma_zone_set_max(ipv6_zone, nmbflows);
}

SYSINIT(flowtable_setup, SI_SUB_KTHREAD_INIT, SI_ORDER_ANY, flowtable_setup, NULL);

/*
 * The rest of the code is devoted to garbage collection of expired entries.
 * It is a new additon made necessary by the switch to dynamically allocating
 * flow tables.
 * 
 */
static void
fle_free(struct flentry *fle)
{
	struct rtentry *rt;
	struct llentry *lle;

	rt = __DEVOLATILE(struct rtentry *, fle->f_rt);
	lle = __DEVOLATILE(struct llentry *, fle->f_lle);
	RTFREE(rt);
	LLE_FREE(lle);
	uma_zfree((fle->f_flags & FL_IPV6) ? ipv6_zone : ipv4_zone, fle);
}

static void
flowtable_free_stale(struct flowtable *ft)
{
	int curbit = 0, count;
	struct flentry *fle,  **flehead, *fleprev;
	struct flentry *flefreehead, *flefreetail, *fletmp;
	bitstr_t *mask, *tmpmask;
	
	flefreehead = flefreetail = NULL;
	mask = flowtable_mask(ft);
	tmpmask = ft->ft_tmpmask;
	memcpy(tmpmask, mask, ft->ft_size/8);
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
		
		FL_ENTRY_LOCK(ft, curbit);
		flehead = flowtable_entry(ft, curbit);
		fle = fleprev = *flehead;

		flowtable_free_checks++;
#ifdef DIAGNOSTIC
		if (fle == NULL && curbit > 0) {
			log(LOG_ALERT,
			    "warning bit=%d set, but no fle found\n",
			    curbit);
		}
#endif		
		while (fle != NULL) {	
			if (!flow_stale(ft, fle)) {
				fleprev = fle;
				fle = fle->f_next;
				continue;
			}
			/*
			 * delete head of the list
			 */
			if (fleprev == *flehead) {
				fletmp = fleprev;
				if (fle == fleprev) {
					fleprev = *flehead = fle->f_next;
				} else
					fleprev = *flehead = fle;
				fle = fle->f_next;
			} else {
				/*
				 * don't advance fleprev
				 */
				fletmp = fle;
				fleprev->f_next = fle->f_next;
				fle = fleprev->f_next;
			}
			
			if (flefreehead == NULL)
				flefreehead = flefreetail = fletmp;
			else {
				flefreetail->f_next = fletmp;
				flefreetail = fletmp;
			}
			fletmp->f_next = NULL;
		}
		if (*flehead == NULL)
			bit_clear(mask, curbit);
		FL_ENTRY_UNLOCK(ft, curbit);
		bit_clear(tmpmask, curbit);
		bit_ffs(tmpmask, ft->ft_size, &curbit);
	}
	count = 0;
	while ((fle = flefreehead) != NULL) {
		flefreehead = fle->f_next;
		count++;
		flowtable_frees++;
		fle_free(fle);
	}
	if (bootverbose && count)
		log(LOG_DEBUG, "freed %d flow entries\n", count);
}

static void
flowtable_cleaner(void)
{
	struct flowtable *ft;
	int i;

	if (bootverbose)
		log(LOG_INFO, "flowtable cleaner started\n");
	while (1) {
		ft = flow_list_head;
		while (ft != NULL) {
			if (ft->ft_flags & FL_PCPU) {
				for (i = 0; i <= mp_maxid; i++) {
					if (CPU_ABSENT(i))
						continue;

					thread_lock(curthread);
					sched_bind(curthread, i);
					thread_unlock(curthread);

					flowtable_free_stale(ft);

					thread_lock(curthread);
					sched_unbind(curthread);
					thread_unlock(curthread);
				}
			} else {
				flowtable_free_stale(ft);
			}
			ft = ft->ft_next;
		}
		/*
		 * The 20 second interval between cleaning checks
		 * is arbitrary
		 */
		pause("flowcleanwait", 20*hz);
	}
}

static struct kproc_desc flow_kp = {
	"flowcleaner",
	flowtable_cleaner,
	&flowcleanerproc
};
SYSINIT(flowcleaner, SI_SUB_KTHREAD_IDLE, SI_ORDER_ANY, kproc_start, &flow_kp);
#endif /* NO_FLOWTABLE */
