/*
 * Copyright (c) 2002 Andre Oppermann, Internet Business Solutions AG
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 *
 * $FreeBSD: src/sys/netinet/tcp_hostcache.c,v 1.7 2004/08/16 18:32:07 rwatson Exp $
 */

/*
 * The tcp_hostcache moves the tcp specific cached metrics from the routing
 * table into a dedicated structure indexed by the remote IP address. It
 * keeps information on the measured tcp parameters of past tcp sessions
 * to have better initial start values for following connections from the
 * same source. Depending on the network parameters (delay, bandwidth, max
 * MTU, congestion window) between local and remote site this can lead to
 * significant speedups for new tcp connections after the first one.
 *
 * Due to this new tcp_hostcache all tcp specific metrics information in
 * the routing table has been removed. The INPCB no longer keeps a pointer
 * to the routing entry and protocol initiated route cloning has been
 * removed as well. With these changes the routing table has gone back
 * to being more lightwight and only carries information related to packet
 * forwarding.
 *
 * Tcp_hostcache is designed for multiple concurrent access in SMP
 * environments and high contention. All bucket rows have their own
 * lock and thus multiple lookups and modifies can be done at the same
 * time as long as they are in different bucket rows. If a request for
 * insertion of a new record can't be satisfied it simply returns an
 * empty structure. Nobody and nothing shall ever point directly to
 * any entry in tcp_hostcache. All communication is done in an object
 * oriented way and only funtions of tcp_hostcache will manipulate hostcache
 * entries. Otherwise we are unable to achieve good behaviour in concurrent
 * access situations. Since tcp_hostcache is only caching information there
 * are no fatal consequences if we either can't satisfy any particular request
 * or have to drop/overwrite an existing entry because of bucket limit
 * memory constrains.
 */

/*
 * Many thanks to jlemon for basic structure of tcp_syncache which is being
 * followed here.
 */

#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#ifdef INET6
#include <netinet6/tcp6_var.h>
#endif

#include <vm/uma.h>


TAILQ_HEAD(hc_qhead, hc_metrics);

struct hc_head {
	struct hc_qhead	hch_bucket;
	u_int		hch_length;
	struct mtx	hch_mtx;
};

struct hc_metrics {
	/* housekeeping */
	TAILQ_ENTRY(hc_metrics) rmx_q;
	struct	hc_head *rmx_head; /* head of bucket tail queue */
	struct	in_addr ip4;	/* IP address */
	struct	in6_addr ip6;	/* IP6 address */
	/* endpoint specific values for tcp */
	u_long	rmx_mtu;	/* MTU for this path */
	u_long	rmx_ssthresh;	/* outbound gateway buffer limit */
	u_long	rmx_rtt;	/* estimated round trip time */
	u_long	rmx_rttvar;	/* estimated rtt variance */
	u_long	rmx_bandwidth;	/* estimated bandwidth */
	u_long	rmx_cwnd;	/* congestion window */
	u_long	rmx_sendpipe;	/* outbound delay-bandwidth product */
	u_long	rmx_recvpipe;	/* inbound delay-bandwidth product */
	struct	rmxp_tao rmx_tao; /* TAO cache for T/TCP */
	/* tcp hostcache internal data */
	int	rmx_expire;	/* lifetime for object */
	u_long	rmx_hits;	/* number of hits */
	u_long	rmx_updates;	/* number of updates */
};

/* Arbitrary values */
#define TCP_HOSTCACHE_HASHSIZE		512
#define TCP_HOSTCACHE_BUCKETLIMIT	30
#define TCP_HOSTCACHE_EXPIRE		60*60	/* one hour */
#define TCP_HOSTCACHE_PRUNE		5*60	/* every 5 minutes */

struct tcp_hostcache {
	struct	hc_head *hashbase;
	uma_zone_t zone;
	u_int	hashsize;
	u_int	hashmask;
	u_int	bucket_limit;
	u_int	cache_count;
	u_int	cache_limit;
	int	expire;
	int	purgeall;
};
static struct tcp_hostcache tcp_hostcache;

static struct callout tcp_hc_callout;

static struct hc_metrics *tcp_hc_lookup(struct in_conninfo *);
static struct hc_metrics *tcp_hc_insert(struct in_conninfo *);
static int sysctl_tcp_hc_list(SYSCTL_HANDLER_ARGS);
static void tcp_hc_purge(void *);

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, hostcache, CTLFLAG_RW, 0, "TCP Host cache");

SYSCTL_INT(_net_inet_tcp_hostcache, OID_AUTO, cachelimit, CTLFLAG_RDTUN,
     &tcp_hostcache.cache_limit, 0, "Overall entry limit for hostcache");

SYSCTL_INT(_net_inet_tcp_hostcache, OID_AUTO, hashsize, CTLFLAG_RDTUN,
     &tcp_hostcache.hashsize, 0, "Size of TCP hostcache hashtable");

SYSCTL_INT(_net_inet_tcp_hostcache, OID_AUTO, bucketlimit, CTLFLAG_RDTUN,
     &tcp_hostcache.bucket_limit, 0, "Per-bucket hash limit for hostcache");

SYSCTL_INT(_net_inet_tcp_hostcache, OID_AUTO, count, CTLFLAG_RD,
     &tcp_hostcache.cache_count, 0, "Current number of entries in hostcache");

SYSCTL_INT(_net_inet_tcp_hostcache, OID_AUTO, expire, CTLFLAG_RW,
     &tcp_hostcache.expire, 0, "Expire time of TCP hostcache entries");

SYSCTL_INT(_net_inet_tcp_hostcache, OID_AUTO, purge, CTLFLAG_RW,
     &tcp_hostcache.purgeall, 0, "Expire all entires on next purge run");

SYSCTL_PROC(_net_inet_tcp_hostcache, OID_AUTO, list,
	CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_SKIP, 0, 0,
	sysctl_tcp_hc_list, "A", "List of all hostcache entries");


static MALLOC_DEFINE(M_HOSTCACHE, "hostcache", "TCP hostcache");

#define HOSTCACHE_HASH(ip) \
	(((ip)->s_addr ^ ((ip)->s_addr >> 7) ^ ((ip)->s_addr >> 17)) &	\
	  tcp_hostcache.hashmask)

/* XXX: What is the recommended hash to get good entropy for IPv6 addresses? */
#define HOSTCACHE_HASH6(ip6)				\
	(((ip6)->s6_addr32[0] ^				\
	  (ip6)->s6_addr32[1] ^				\
	  (ip6)->s6_addr32[2] ^				\
	  (ip6)->s6_addr32[3]) &			\
	 tcp_hostcache.hashmask)

#define THC_LOCK(lp)		mtx_lock(lp)
#define THC_UNLOCK(lp)		mtx_unlock(lp)

void
tcp_hc_init(void)
{
	int i;

	/*
	 * Initialize hostcache structures
	 */
	tcp_hostcache.cache_count = 0;
	tcp_hostcache.hashsize = TCP_HOSTCACHE_HASHSIZE;
	tcp_hostcache.bucket_limit = TCP_HOSTCACHE_BUCKETLIMIT;
	tcp_hostcache.cache_limit =
	    tcp_hostcache.hashsize * tcp_hostcache.bucket_limit;
	tcp_hostcache.expire = TCP_HOSTCACHE_EXPIRE;

	TUNABLE_INT_FETCH("net.inet.tcp.hostcache.hashsize",
	    &tcp_hostcache.hashsize);
	TUNABLE_INT_FETCH("net.inet.tcp.hostcache.cachelimit",
	    &tcp_hostcache.cache_limit);
	TUNABLE_INT_FETCH("net.inet.tcp.hostcache.bucketlimit",
	    &tcp_hostcache.bucket_limit);
	if (!powerof2(tcp_hostcache.hashsize)) {
		printf("WARNING: hostcache hash size is not a power of 2.\n");
		tcp_hostcache.hashsize = 512;	/* safe default */
	}
	tcp_hostcache.hashmask = tcp_hostcache.hashsize - 1;

	/*
	 * Allocate the hash table
	 */
	tcp_hostcache.hashbase = (struct hc_head *)
	    malloc(tcp_hostcache.hashsize * sizeof(struct hc_head),
		   M_HOSTCACHE, M_WAITOK | M_ZERO);

	/*
	 * Initialize the hash buckets
	 */
	for (i = 0; i < tcp_hostcache.hashsize; i++) {
		TAILQ_INIT(&tcp_hostcache.hashbase[i].hch_bucket);
		tcp_hostcache.hashbase[i].hch_length = 0;
		mtx_init(&tcp_hostcache.hashbase[i].hch_mtx, "tcp_hc_entry",
			  NULL, MTX_DEF);
	}

	/*
	 * Allocate the hostcache entries.
	 */
	tcp_hostcache.zone = uma_zcreate("hostcache", sizeof(struct hc_metrics),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	uma_zone_set_max(tcp_hostcache.zone, tcp_hostcache.cache_limit);

	/*
	 * Set up periodic cache cleanup.
	 */
	callout_init(&tcp_hc_callout, CALLOUT_MPSAFE);
	callout_reset(&tcp_hc_callout, TCP_HOSTCACHE_PRUNE * hz, tcp_hc_purge, 0);
}

/*
 * Internal function: lookup an entry in the hostcache or return NULL.
 *
 * If an entry has been returned, the caller becomes responsible for
 * unlocking the bucket row after he is done reading/modifying the entry.
 */
static struct hc_metrics *
tcp_hc_lookup(struct in_conninfo *inc)
{
	int hash;
	struct hc_head *hc_head;
	struct hc_metrics *hc_entry;

	KASSERT(inc != NULL, ("tcp_hc_lookup with NULL in_conninfo pointer"));

	/*
	 * Hash the foreign ip address.
	 */
	if (inc->inc_isipv6)
		hash = HOSTCACHE_HASH6(&inc->inc6_faddr);
	else
		hash = HOSTCACHE_HASH(&inc->inc_faddr);

	hc_head = &tcp_hostcache.hashbase[hash];

	/*
	 * aquire lock for this bucket row
	 * we release the lock if we don't find an entry,
	 * otherwise the caller has to unlock after he is done
	 */
	THC_LOCK(&hc_head->hch_mtx);

	/*
	 * circle through entries in bucket row looking for a match
	 */
	TAILQ_FOREACH(hc_entry, &hc_head->hch_bucket, rmx_q) {
		if (inc->inc_isipv6) {
			if (memcmp(&inc->inc6_faddr, &hc_entry->ip6,
			    sizeof(inc->inc6_faddr)) == 0)
				return hc_entry;
		} else {
			if (memcmp(&inc->inc_faddr, &hc_entry->ip4,
			    sizeof(inc->inc_faddr)) == 0)
				return hc_entry;
		}
	}

	/*
	 * We were unsuccessful and didn't find anything
	 */
	THC_UNLOCK(&hc_head->hch_mtx);
	return NULL;
}

/*
 * Internal function: insert an entry into the hostcache or return NULL
 * if unable to allocate a new one.
 *
 * If an entry has been returned, the caller becomes responsible for
 * unlocking the bucket row after he is done reading/modifying the entry.
 */
static struct hc_metrics *
tcp_hc_insert(struct in_conninfo *inc)
{
	int hash;
	struct hc_head *hc_head;
	struct hc_metrics *hc_entry;

	KASSERT(inc != NULL, ("tcp_hc_insert with NULL in_conninfo pointer"));

	/*
	 * Hash the foreign ip address
	 */
	if (inc->inc_isipv6)
		hash = HOSTCACHE_HASH6(&inc->inc6_faddr);
	else
		hash = HOSTCACHE_HASH(&inc->inc_faddr);

	hc_head = &tcp_hostcache.hashbase[hash];

	/*
	 * aquire lock for this bucket row
	 * we release the lock if we don't find an entry,
	 * otherwise the caller has to unlock after he is done
	 */
	THC_LOCK(&hc_head->hch_mtx);

	/*
	 * If the bucket limit is reached reuse the least used element
	 */
	if (hc_head->hch_length >= tcp_hostcache.bucket_limit ||
	    tcp_hostcache.cache_count >= tcp_hostcache.cache_limit) {
		hc_entry = TAILQ_LAST(&hc_head->hch_bucket, hc_qhead);
		/*
		 * At first we were dropping the last element, just to
		 * reaquire it in the next two lines again which ain't
		 * very efficient. Instead just reuse the least used element.
		 * Maybe we drop something that is still "in-use" but we can
		 * be "lossy".
		 */
		TAILQ_REMOVE(&hc_head->hch_bucket, hc_entry, rmx_q);
		tcp_hostcache.hashbase[hash].hch_length--;
		tcp_hostcache.cache_count--;
		tcpstat.tcps_hc_bucketoverflow++;
#if 0
		uma_zfree(tcp_hostcache.zone, hc_entry);
#endif
	} else {
		/*
		 * Allocate a new entry, or balk if not possible
		 */
		hc_entry = uma_zalloc(tcp_hostcache.zone, M_NOWAIT);
		if (hc_entry == NULL) {
			THC_UNLOCK(&hc_head->hch_mtx);
			return NULL;
		}
	}

	/*
	 * Initialize basic information of hostcache entry
	 */
	bzero(hc_entry, sizeof(*hc_entry));
	if (inc->inc_isipv6)
		bcopy(&inc->inc6_faddr, &hc_entry->ip6, sizeof(hc_entry->ip6));
	else
		hc_entry->ip4 = inc->inc_faddr;
	hc_entry->rmx_head = hc_head;
	hc_entry->rmx_expire = tcp_hostcache.expire;

	/*
	 * Put it upfront
	 */
	TAILQ_INSERT_HEAD(&hc_head->hch_bucket, hc_entry, rmx_q);
	tcp_hostcache.hashbase[hash].hch_length++;
	tcp_hostcache.cache_count++;
	tcpstat.tcps_hc_added++;

	return hc_entry;
}

/*
 * External function: lookup an entry in the hostcache and fill out the
 * supplied tcp metrics structure.  Fills in null when no entry was found
 * or a value is not set.
 */
void
tcp_hc_get(struct in_conninfo *inc, struct hc_metrics_lite *hc_metrics_lite)
{
	struct hc_metrics *hc_entry;

	/*
	 * Find the right bucket
	 */
	hc_entry = tcp_hc_lookup(inc);

	/*
	 * If we don't have an existing object
	 */
	if (hc_entry == NULL) {
		bzero(hc_metrics_lite, sizeof(*hc_metrics_lite));
		return;
	}
	hc_entry->rmx_hits++;
	hc_entry->rmx_expire = tcp_hostcache.expire; /* start over again */

	hc_metrics_lite->rmx_mtu = hc_entry->rmx_mtu;
	hc_metrics_lite->rmx_ssthresh = hc_entry->rmx_ssthresh;
	hc_metrics_lite->rmx_rtt = hc_entry->rmx_rtt;
	hc_metrics_lite->rmx_rttvar = hc_entry->rmx_rttvar;
	hc_metrics_lite->rmx_bandwidth = hc_entry->rmx_bandwidth;
	hc_metrics_lite->rmx_cwnd = hc_entry->rmx_cwnd;
	hc_metrics_lite->rmx_sendpipe = hc_entry->rmx_sendpipe;
	hc_metrics_lite->rmx_recvpipe = hc_entry->rmx_recvpipe;

	/*
	 * unlock bucket row
	 */
	THC_UNLOCK(&hc_entry->rmx_head->hch_mtx);
}

/*
 * External function: lookup an entry in the hostcache and return the
 * discovered path mtu.  Returns null if no entry found or value not is set.
 */
u_long
tcp_hc_getmtu(struct in_conninfo *inc)
{
	struct hc_metrics *hc_entry;
	u_long mtu;

	hc_entry = tcp_hc_lookup(inc);
	if (hc_entry == NULL) {
		return 0;
	}
	hc_entry->rmx_hits++;
	hc_entry->rmx_expire = tcp_hostcache.expire; /* start over again */

	mtu = hc_entry->rmx_mtu;
	THC_UNLOCK(&hc_entry->rmx_head->hch_mtx);
	return mtu;
}

/*
 * External function: lookup an entry in the hostcache and fill out the
 * supplied t/tcp tao structure.  Fills in null when no entry was found
 * or a value is not set.
 */
void
tcp_hc_gettao(struct in_conninfo *inc, struct rmxp_tao *tao)
{
	struct hc_metrics *hc_entry;

	hc_entry = tcp_hc_lookup(inc);
	if (hc_entry == NULL) {
		bzero(tao, sizeof(*tao));
		return;
	}
	hc_entry->rmx_hits++;
	hc_entry->rmx_expire = tcp_hostcache.expire; /* start over again */

	bcopy(&hc_entry->rmx_tao, tao, sizeof(*tao));
	THC_UNLOCK(&hc_entry->rmx_head->hch_mtx);
}

/*
 * External function: update the mtu value of an entry in the hostcache.
 * Creates a new entry if none was found.
 */
void
tcp_hc_updatemtu(struct in_conninfo *inc, u_long mtu)
{
	struct hc_metrics *hc_entry;

	/*
	 * Find the right bucket
	 */
	hc_entry = tcp_hc_lookup(inc);

	/*
	 * If we don't have an existing object try to insert a new one
	 */
	if (hc_entry == NULL) {
		hc_entry = tcp_hc_insert(inc);
		if (hc_entry == NULL)
			return;
	}
	hc_entry->rmx_updates++;
	hc_entry->rmx_expire = tcp_hostcache.expire; /* start over again */

	hc_entry->rmx_mtu = mtu;

	/*
	 * put it upfront so we find it faster next time
	 */
	TAILQ_REMOVE(&hc_entry->rmx_head->hch_bucket, hc_entry, rmx_q);
	TAILQ_INSERT_HEAD(&hc_entry->rmx_head->hch_bucket, hc_entry, rmx_q);

	/*
	 * unlock bucket row
	 */
	THC_UNLOCK(&hc_entry->rmx_head->hch_mtx);
}

/*
 * External function: update the tcp metrics of an entry in the hostcache.
 * Creates a new entry if none was found.
 */
void
tcp_hc_update(struct in_conninfo *inc, struct hc_metrics_lite *hcml)
{
	struct hc_metrics *hc_entry;

	hc_entry = tcp_hc_lookup(inc);
	if (hc_entry == NULL) {
		hc_entry = tcp_hc_insert(inc);
		if (hc_entry == NULL)
			return;
	}
	hc_entry->rmx_updates++;
	hc_entry->rmx_expire = tcp_hostcache.expire; /* start over again */

	if (hcml->rmx_rtt != 0) {
		if (hc_entry->rmx_rtt == 0)
			hc_entry->rmx_rtt = hcml->rmx_rtt;
		else
			hc_entry->rmx_rtt =
			    (hc_entry->rmx_rtt + hcml->rmx_rtt) / 2;
		tcpstat.tcps_cachedrtt++;
	}
	if (hcml->rmx_rttvar != 0) {
	        if (hc_entry->rmx_rttvar == 0)
			hc_entry->rmx_rttvar = hcml->rmx_rttvar;
		else
			hc_entry->rmx_rttvar =
			    (hc_entry->rmx_rttvar + hcml->rmx_rttvar) / 2;
		tcpstat.tcps_cachedrttvar++;
	}
	if (hcml->rmx_ssthresh != 0) {
		if (hc_entry->rmx_ssthresh == 0)
			hc_entry->rmx_ssthresh = hcml->rmx_ssthresh;
		else
			hc_entry->rmx_ssthresh =
			    (hc_entry->rmx_ssthresh + hcml->rmx_ssthresh) / 2;
		tcpstat.tcps_cachedssthresh++;
	}
	if (hcml->rmx_bandwidth != 0) {
		if (hc_entry->rmx_bandwidth == 0)
			hc_entry->rmx_bandwidth = hcml->rmx_bandwidth;
		else
			hc_entry->rmx_bandwidth =
			    (hc_entry->rmx_bandwidth + hcml->rmx_bandwidth) / 2;
		/* tcpstat.tcps_cachedbandwidth++; */
	}
	if (hcml->rmx_cwnd != 0) {
		if (hc_entry->rmx_cwnd == 0)
			hc_entry->rmx_cwnd = hcml->rmx_cwnd;
		else
			hc_entry->rmx_cwnd =
			    (hc_entry->rmx_cwnd + hcml->rmx_cwnd) / 2;
		/* tcpstat.tcps_cachedcwnd++; */
	}
	if (hcml->rmx_sendpipe != 0) {
		if (hc_entry->rmx_sendpipe == 0)
			hc_entry->rmx_sendpipe = hcml->rmx_sendpipe;
		else
			hc_entry->rmx_sendpipe =
			    (hc_entry->rmx_sendpipe + hcml->rmx_sendpipe) /2;
		/* tcpstat.tcps_cachedsendpipe++; */
	}
	if (hcml->rmx_recvpipe != 0) {
		if (hc_entry->rmx_recvpipe == 0)
			hc_entry->rmx_recvpipe = hcml->rmx_recvpipe;
		else
			hc_entry->rmx_recvpipe =
			    (hc_entry->rmx_recvpipe + hcml->rmx_recvpipe) /2;
		/* tcpstat.tcps_cachedrecvpipe++; */
	}

	TAILQ_REMOVE(&hc_entry->rmx_head->hch_bucket, hc_entry, rmx_q);
	TAILQ_INSERT_HEAD(&hc_entry->rmx_head->hch_bucket, hc_entry, rmx_q);
	THC_UNLOCK(&hc_entry->rmx_head->hch_mtx);
}

/*
 * External function: update the t/tcp tao of an entry in the hostcache.
 * Creates a new entry if none was found.
 */
void
tcp_hc_updatetao(struct in_conninfo *inc, int field, tcp_cc ccount, u_short mss)
{
	struct hc_metrics *hc_entry;

	hc_entry = tcp_hc_lookup(inc);
	if (hc_entry == NULL) {
		hc_entry = tcp_hc_insert(inc);
		if (hc_entry == NULL)
			return;
	}
	hc_entry->rmx_updates++;
	hc_entry->rmx_expire = tcp_hostcache.expire; /* start over again */

	switch(field) {
		case TCP_HC_TAO_CC:
			hc_entry->rmx_tao.tao_cc = ccount;
			break;

		case TCP_HC_TAO_CCSENT:
			hc_entry->rmx_tao.tao_ccsent = ccount;
			break;

		case TCP_HC_TAO_MSSOPT:
			hc_entry->rmx_tao.tao_mssopt = mss;
			break;
	}

	TAILQ_REMOVE(&hc_entry->rmx_head->hch_bucket, hc_entry, rmx_q);
	TAILQ_INSERT_HEAD(&hc_entry->rmx_head->hch_bucket, hc_entry, rmx_q);
	THC_UNLOCK(&hc_entry->rmx_head->hch_mtx);
}

/*
 * Sysctl function: prints the list and values of all hostcache entries in
 * unsorted order.
 */
static int
sysctl_tcp_hc_list(SYSCTL_HANDLER_ARGS)
{
	int bufsize;
	int linesize = 128;
	char *p, *buf;
	int len, i, error;
	struct hc_metrics *hc_entry;

	bufsize = linesize * (tcp_hostcache.cache_count + 1);

	p = buf = (char *)malloc(bufsize, M_TEMP, M_WAITOK|M_ZERO);

	len = snprintf(p, linesize,
		"\nIP address        MTU  SSTRESH      RTT   RTTVAR BANDWIDTH "
		"    CWND SENDPIPE RECVPIPE HITS  UPD  EXP\n");
	p += len;

#define msec(u) (((u) + 500) / 1000)
	for (i = 0; i < tcp_hostcache.hashsize; i++) {
		THC_LOCK(&tcp_hostcache.hashbase[i].hch_mtx);
		TAILQ_FOREACH(hc_entry, &tcp_hostcache.hashbase[i].hch_bucket,
			      rmx_q) {
			len = snprintf(p, linesize,
			    "%-15s %5lu %8lu %6lums %6lums %9lu %8lu %8lu %8lu "
			    "%4lu %4lu %4i\n",
			    hc_entry->ip4.s_addr ? inet_ntoa(hc_entry->ip4) :
#ifdef INET6
				ip6_sprintf(&hc_entry->ip6),
#else
				"IPv6?",
#endif
			    hc_entry->rmx_mtu,
			    hc_entry->rmx_ssthresh,
			    msec(hc_entry->rmx_rtt *
				(RTM_RTTUNIT / (hz * TCP_RTT_SCALE))),
			    msec(hc_entry->rmx_rttvar *
				(RTM_RTTUNIT / (hz * TCP_RTT_SCALE))),
			    hc_entry->rmx_bandwidth * 8,
			    hc_entry->rmx_cwnd,
			    hc_entry->rmx_sendpipe,
			    hc_entry->rmx_recvpipe,
			    hc_entry->rmx_hits,
			    hc_entry->rmx_updates,
			    hc_entry->rmx_expire);
			p += len;
		}
		THC_UNLOCK(&tcp_hostcache.hashbase[i].hch_mtx);
	}
#undef msec
	error = SYSCTL_OUT(req, buf, p - buf);
	free(buf, M_TEMP);
	return(error);
}

/*
 * Expire and purge (old|all) entries in the tcp_hostcache.  Runs periodically
 * from the callout.
 */
static void
tcp_hc_purge(void *arg)
{
	struct hc_metrics *hc_entry, *hc_next;
	int all = (intptr_t)arg;
	int i;

	if (tcp_hostcache.purgeall) {
		all = 1;
		tcp_hostcache.purgeall = 0;
	}

	for (i = 0; i < tcp_hostcache.hashsize; i++) {
		THC_LOCK(&tcp_hostcache.hashbase[i].hch_mtx);
		TAILQ_FOREACH_SAFE(hc_entry, &tcp_hostcache.hashbase[i].hch_bucket,
			      rmx_q, hc_next) {
			if (all || hc_entry->rmx_expire <= 0) {
				TAILQ_REMOVE(&tcp_hostcache.hashbase[i].hch_bucket,
					      hc_entry, rmx_q);
				uma_zfree(tcp_hostcache.zone, hc_entry);
				tcp_hostcache.hashbase[i].hch_length--;
				tcp_hostcache.cache_count--;
			} else
				hc_entry->rmx_expire -= TCP_HOSTCACHE_PRUNE;
		}
		THC_UNLOCK(&tcp_hostcache.hashbase[i].hch_mtx);
	}
	callout_reset(&tcp_hc_callout, TCP_HOSTCACHE_PRUNE * hz, tcp_hc_purge, 0);
}
