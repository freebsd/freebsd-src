/*-
 * Copyright (c) 2001 Networks Associates Technologies, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Jonathan Lemon
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
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
 * $FreeBSD$
 */

#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/md5.h>
#include <sys/proc.h>		/* for proc0 declaration */
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#endif
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#ifdef INET6
#include <netinet6/tcp6_var.h>
#endif

#ifdef IPSEC
#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/ipsec6.h>
#endif
#include <netkey/key.h>
#endif /*IPSEC*/

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#ifdef INET6
#include <netipsec/ipsec6.h>
#endif
#include <netipsec/key.h>
#define	IPSEC
#endif /*FAST_IPSEC*/

#include <machine/in_cksum.h>
#include <vm/vm_zone.h>

static int tcp_syncookies = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, syncookies, CTLFLAG_RW,
    &tcp_syncookies, 0, 
    "Use TCP SYN cookies if the syncache overflows");

static void	 syncache_drop(struct syncache *, struct syncache_head *);
static void	 syncache_free(struct syncache *);
static void	 syncache_insert(struct syncache *, struct syncache_head *);
struct syncache *syncache_lookup(struct in_conninfo *, struct syncache_head **);
static int	 syncache_respond(struct syncache *, struct mbuf *);
static struct 	 socket *syncache_socket(struct syncache *, struct socket *);
static void	 syncache_timer(void *);
static u_int32_t syncookie_generate(struct syncache *);
static struct syncache *syncookie_lookup(struct in_conninfo *,
		    struct tcphdr *, struct socket *);

/*
 * Transmit the SYN,ACK fewer times than TCP_MAXRXTSHIFT specifies.
 * 3 retransmits corresponds to a timeout of (1 + 2 + 4 + 8 == 15) seconds,
 * the odds are that the user has given up attempting to connect by then.
 */
#define SYNCACHE_MAXREXMTS		3

/* Arbitrary values */
#define TCP_SYNCACHE_HASHSIZE		512
#define TCP_SYNCACHE_BUCKETLIMIT	30

struct tcp_syncache {
	struct	syncache_head *hashbase;
	struct	vm_zone *zone;
	u_int	hashsize;
	u_int	hashmask;
	u_int	bucket_limit;
	u_int	cache_count;
	u_int	cache_limit;
	u_int	rexmt_limit;
	u_int	hash_secret;
	u_int	next_reseed;
	TAILQ_HEAD(, syncache) timerq[SYNCACHE_MAXREXMTS + 1];
	struct	callout tt_timerq[SYNCACHE_MAXREXMTS + 1];
};
static struct tcp_syncache tcp_syncache;

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, syncache, CTLFLAG_RW, 0, "TCP SYN cache");

SYSCTL_INT(_net_inet_tcp_syncache, OID_AUTO, bucketlimit, CTLFLAG_RD,
     &tcp_syncache.bucket_limit, 0, "Per-bucket hash limit for syncache");

SYSCTL_INT(_net_inet_tcp_syncache, OID_AUTO, cachelimit, CTLFLAG_RD,
     &tcp_syncache.cache_limit, 0, "Overall entry limit for syncache");

SYSCTL_INT(_net_inet_tcp_syncache, OID_AUTO, count, CTLFLAG_RD,
     &tcp_syncache.cache_count, 0, "Current number of entries in syncache");

SYSCTL_INT(_net_inet_tcp_syncache, OID_AUTO, hashsize, CTLFLAG_RD,
     &tcp_syncache.hashsize, 0, "Size of TCP syncache hashtable");

SYSCTL_INT(_net_inet_tcp_syncache, OID_AUTO, rexmtlimit, CTLFLAG_RW,
     &tcp_syncache.rexmt_limit, 0, "Limit on SYN/ACK retransmissions");

static MALLOC_DEFINE(M_SYNCACHE, "syncache", "TCP syncache");

#define SYNCACHE_HASH(inc, mask) 					\
	((tcp_syncache.hash_secret ^					\
	  (inc)->inc_faddr.s_addr ^					\
	  ((inc)->inc_faddr.s_addr >> 16) ^ 				\
	  (inc)->inc_fport ^ (inc)->inc_lport) & mask)

#define SYNCACHE_HASH6(inc, mask) 					\
	((tcp_syncache.hash_secret ^					\
	  (inc)->inc6_faddr.s6_addr32[0] ^ 				\
	  (inc)->inc6_faddr.s6_addr32[3] ^ 				\
	  (inc)->inc_fport ^ (inc)->inc_lport) & mask)

#define ENDPTS_EQ(a, b) (						\
	(a)->ie_fport == (b)->ie_fport &&				\
	(a)->ie_lport == (b)->ie_lport &&				\
	(a)->ie_faddr.s_addr == (b)->ie_faddr.s_addr &&			\
	(a)->ie_laddr.s_addr == (b)->ie_laddr.s_addr			\
)

#define ENDPTS6_EQ(a, b) (memcmp(a, b, sizeof(*a)) == 0)

#define SYNCACHE_TIMEOUT(sc, slot) do {					\
	sc->sc_rxtslot = slot;						\
	sc->sc_rxttime = ticks + TCPTV_RTOBASE * tcp_backoff[slot];	\
	TAILQ_INSERT_TAIL(&tcp_syncache.timerq[slot], sc, sc_timerq);	\
	if (!callout_active(&tcp_syncache.tt_timerq[slot]))		\
		callout_reset(&tcp_syncache.tt_timerq[slot],		\
		    TCPTV_RTOBASE * tcp_backoff[slot],			\
		    syncache_timer, (void *)((intptr_t)slot));		\
} while (0)

static void
syncache_free(struct syncache *sc)
{
	struct rtentry *rt;

	if (sc->sc_ipopts)
		(void) m_free(sc->sc_ipopts);
#ifdef INET6
	if (sc->sc_inc.inc_isipv6)
		rt = sc->sc_route6.ro_rt;
	else
#endif
		rt = sc->sc_route.ro_rt;
	if (rt != NULL) {
		/*
		 * If this is the only reference to a protocol cloned 
		 * route, remove it immediately.
		 */
		if (rt->rt_flags & RTF_WASCLONED &&
		    (sc->sc_flags & SCF_KEEPROUTE) == 0 &&
		    rt->rt_refcnt == 1)
			rtrequest(RTM_DELETE, rt_key(rt),
			    rt->rt_gateway, rt_mask(rt),
			    rt->rt_flags, NULL);
		RTFREE(rt);
	}
	zfree(tcp_syncache.zone, sc);
}

void
syncache_init(void)
{
	int i;

	tcp_syncache.cache_count = 0;
	tcp_syncache.hashsize = TCP_SYNCACHE_HASHSIZE;
	tcp_syncache.bucket_limit = TCP_SYNCACHE_BUCKETLIMIT;
	tcp_syncache.cache_limit =
	    tcp_syncache.hashsize * tcp_syncache.bucket_limit;
	tcp_syncache.rexmt_limit = SYNCACHE_MAXREXMTS;
	tcp_syncache.next_reseed = 0;
	tcp_syncache.hash_secret = arc4random();

        TUNABLE_INT_FETCH("net.inet.tcp.syncache.hashsize",
	    &tcp_syncache.hashsize);
        TUNABLE_INT_FETCH("net.inet.tcp.syncache.cachelimit",
	    &tcp_syncache.cache_limit);
        TUNABLE_INT_FETCH("net.inet.tcp.syncache.bucketlimit",
	    &tcp_syncache.bucket_limit);
	if (!powerof2(tcp_syncache.hashsize)) {
                printf("WARNING: syncache hash size is not a power of 2.\n");
		tcp_syncache.hashsize = 512;	/* safe default */
        }
	tcp_syncache.hashmask = tcp_syncache.hashsize - 1;

	/* Allocate the hash table. */
	MALLOC(tcp_syncache.hashbase, struct syncache_head *,
	    tcp_syncache.hashsize * sizeof(struct syncache_head),
	    M_SYNCACHE, M_WAITOK);

	/* Initialize the hash buckets. */
	for (i = 0; i < tcp_syncache.hashsize; i++) {
		TAILQ_INIT(&tcp_syncache.hashbase[i].sch_bucket);
		tcp_syncache.hashbase[i].sch_length = 0;
	}

	/* Initialize the timer queues. */
	for (i = 0; i <= SYNCACHE_MAXREXMTS; i++) {
		TAILQ_INIT(&tcp_syncache.timerq[i]);
		callout_init(&tcp_syncache.tt_timerq[i]);
	}

	/*
	 * Allocate the syncache entries.  Allow the zone to allocate one
	 * more entry than cache limit, so a new entry can bump out an
	 * older one.
	 */
	tcp_syncache.cache_limit -= 1;
	tcp_syncache.zone = zinit("syncache", sizeof(struct syncache),
	    tcp_syncache.cache_limit, ZONE_INTERRUPT, 0);
}

static void
syncache_insert(sc, sch)
	struct syncache *sc;
	struct syncache_head *sch;
{
	struct syncache *sc2;
	int s, i;

	/*
	 * Make sure that we don't overflow the per-bucket
	 * limit or the total cache size limit.
	 */
	s = splnet();
	if (sch->sch_length >= tcp_syncache.bucket_limit) {
		/*
		 * The bucket is full, toss the oldest element.
		 */
		sc2 = TAILQ_FIRST(&sch->sch_bucket);
		sc2->sc_tp->ts_recent = ticks;
		syncache_drop(sc2, sch);
		tcpstat.tcps_sc_bucketoverflow++;
	} else if (tcp_syncache.cache_count >= tcp_syncache.cache_limit) {
		/*
		 * The cache is full.  Toss the oldest entry in the
		 * entire cache.  This is the front entry in the
		 * first non-empty timer queue with the largest
		 * timeout value.
		 */
		for (i = SYNCACHE_MAXREXMTS; i >= 0; i--) {
			sc2 = TAILQ_FIRST(&tcp_syncache.timerq[i]);
			if (sc2 != NULL)
				break;
		}
		sc2->sc_tp->ts_recent = ticks;
		syncache_drop(sc2, NULL);
		tcpstat.tcps_sc_cacheoverflow++;
	}

	/* Initialize the entry's timer. */
	SYNCACHE_TIMEOUT(sc, 0);

	/* Put it into the bucket. */
	TAILQ_INSERT_TAIL(&sch->sch_bucket, sc, sc_hash);
	sch->sch_length++;
	tcp_syncache.cache_count++;
	tcpstat.tcps_sc_added++;
	splx(s);
}

static void
syncache_drop(sc, sch)
	struct syncache *sc;
	struct syncache_head *sch;
{
	int s;

	if (sch == NULL) {
#ifdef INET6
		if (sc->sc_inc.inc_isipv6) {
			sch = &tcp_syncache.hashbase[
			    SYNCACHE_HASH6(&sc->sc_inc, tcp_syncache.hashmask)];
		} else
#endif
		{
			sch = &tcp_syncache.hashbase[
			    SYNCACHE_HASH(&sc->sc_inc, tcp_syncache.hashmask)];
		}
	}

	s = splnet();

	TAILQ_REMOVE(&sch->sch_bucket, sc, sc_hash);
	sch->sch_length--;
	tcp_syncache.cache_count--;

	TAILQ_REMOVE(&tcp_syncache.timerq[sc->sc_rxtslot], sc, sc_timerq);
	if (TAILQ_EMPTY(&tcp_syncache.timerq[sc->sc_rxtslot]))
		callout_stop(&tcp_syncache.tt_timerq[sc->sc_rxtslot]);
	splx(s);

	syncache_free(sc);
}

/*
 * Walk the timer queues, looking for SYN,ACKs that need to be retransmitted.
 * If we have retransmitted an entry the maximum number of times, expire it.
 */
static void
syncache_timer(xslot)
	void *xslot;
{
	intptr_t slot = (intptr_t)xslot;
	struct syncache *sc, *nsc;
	struct inpcb *inp;
	int s;

	s = splnet();
        if (callout_pending(&tcp_syncache.tt_timerq[slot]) ||
            !callout_active(&tcp_syncache.tt_timerq[slot])) {
                splx(s);
                return;
        }
        callout_deactivate(&tcp_syncache.tt_timerq[slot]);

        nsc = TAILQ_FIRST(&tcp_syncache.timerq[slot]);
	while (nsc != NULL) {
		if (ticks < nsc->sc_rxttime)
			break;
		sc = nsc;
		inp = sc->sc_tp->t_inpcb;
		if (slot == SYNCACHE_MAXREXMTS ||
		    slot >= tcp_syncache.rexmt_limit ||
		    inp->inp_gencnt != sc->sc_inp_gencnt) {
			nsc = TAILQ_NEXT(sc, sc_timerq);
			syncache_drop(sc, NULL);
			tcpstat.tcps_sc_stale++;
			continue;
		}
		/*
		 * syncache_respond() may call back into the syncache to
		 * to modify another entry, so do not obtain the next
		 * entry on the timer chain until it has completed.
		 */
		(void) syncache_respond(sc, NULL);
		nsc = TAILQ_NEXT(sc, sc_timerq);
		tcpstat.tcps_sc_retransmitted++;
		TAILQ_REMOVE(&tcp_syncache.timerq[slot], sc, sc_timerq);
		SYNCACHE_TIMEOUT(sc, slot + 1);
	}
	if (nsc != NULL)
		callout_reset(&tcp_syncache.tt_timerq[slot],
		    nsc->sc_rxttime - ticks, syncache_timer, (void *)(slot));
	splx(s);
}

/*
 * Find an entry in the syncache.
 */
struct syncache *
syncache_lookup(inc, schp)
	struct in_conninfo *inc;
	struct syncache_head **schp;
{
	struct syncache *sc;
	struct syncache_head *sch;
	int s;

#ifdef INET6
	if (inc->inc_isipv6) {
		sch = &tcp_syncache.hashbase[
		    SYNCACHE_HASH6(inc, tcp_syncache.hashmask)];
		*schp = sch;
		s = splnet();
		TAILQ_FOREACH(sc, &sch->sch_bucket, sc_hash) {
			if (ENDPTS6_EQ(&inc->inc_ie, &sc->sc_inc.inc_ie)) {
				splx(s);
				return (sc);
			}
		}
		splx(s);
	} else
#endif
	{
		sch = &tcp_syncache.hashbase[
		    SYNCACHE_HASH(inc, tcp_syncache.hashmask)];
		*schp = sch;
		s = splnet();
		TAILQ_FOREACH(sc, &sch->sch_bucket, sc_hash) {
#ifdef INET6
			if (sc->sc_inc.inc_isipv6)
				continue;
#endif
			if (ENDPTS_EQ(&inc->inc_ie, &sc->sc_inc.inc_ie)) {
				splx(s);
				return (sc);
			}
		}
		splx(s);
	}
	return (NULL);
}

/*
 * This function is called when we get a RST for a
 * non-existent connection, so that we can see if the
 * connection is in the syn cache.  If it is, zap it.
 */
void
syncache_chkrst(inc, th)
	struct in_conninfo *inc;
	struct tcphdr *th;
{
	struct syncache *sc;
	struct syncache_head *sch;

	sc = syncache_lookup(inc, &sch);
	if (sc == NULL)
		return;
	/*
	 * If the RST bit is set, check the sequence number to see
	 * if this is a valid reset segment.
	 * RFC 793 page 37:
	 *   In all states except SYN-SENT, all reset (RST) segments
	 *   are validated by checking their SEQ-fields.  A reset is
	 *   valid if its sequence number is in the window.
	 *
	 *   The sequence number in the reset segment is normally an
	 *   echo of our outgoing acknowlegement numbers, but some hosts
	 *   send a reset with the sequence number at the rightmost edge
	 *   of our receive window, and we have to handle this case.
	 */
	if (SEQ_GEQ(th->th_seq, sc->sc_irs) &&
	    SEQ_LEQ(th->th_seq, sc->sc_irs + sc->sc_wnd)) {
		syncache_drop(sc, sch);
		tcpstat.tcps_sc_reset++;
	}
}

void
syncache_badack(inc)
	struct in_conninfo *inc;
{
	struct syncache *sc;
	struct syncache_head *sch;

	sc = syncache_lookup(inc, &sch);
	if (sc != NULL) {
		syncache_drop(sc, sch);
		tcpstat.tcps_sc_badack++;
	}
}

void
syncache_unreach(inc, th)
	struct in_conninfo *inc;
	struct tcphdr *th;
{
	struct syncache *sc;
	struct syncache_head *sch;

	/* we are called at splnet() here */
	sc = syncache_lookup(inc, &sch);
	if (sc == NULL)
		return;

	/* If the sequence number != sc_iss, then it's a bogus ICMP msg */
	if (ntohl(th->th_seq) != sc->sc_iss)
		return;

	/*
	 * If we've rertransmitted 3 times and this is our second error,
	 * we remove the entry.  Otherwise, we allow it to continue on.
	 * This prevents us from incorrectly nuking an entry during a
	 * spurious network outage.
	 *
	 * See tcp_notify().
	 */
	if ((sc->sc_flags & SCF_UNREACH) == 0 || sc->sc_rxtslot < 3) {
		sc->sc_flags |= SCF_UNREACH;
		return;
	}
	syncache_drop(sc, sch);
	tcpstat.tcps_sc_unreach++;
}

/*
 * Build a new TCP socket structure from a syncache entry.
 */
static struct socket *
syncache_socket(sc, lso)
	struct syncache *sc;
	struct socket *lso;
{
	struct inpcb *inp = NULL;
	struct socket *so;
	struct tcpcb *tp;

	/*
	 * Ok, create the full blown connection, and set things up
	 * as they would have been set up if we had created the
	 * connection when the SYN arrived.  If we can't create
	 * the connection, abort it.
	 */
	so = sonewconn(lso, SS_ISCONNECTED);
	if (so == NULL) {
		/*
		 * Drop the connection; we will send a RST if the peer
		 * retransmits the ACK,
		 */
		tcpstat.tcps_listendrop++;
		goto abort;
	}

	inp = sotoinpcb(so);

	/*
	 * Insert new socket into hash list.
	 */
	inp->inp_inc.inc_isipv6 = sc->sc_inc.inc_isipv6;
#ifdef INET6
	if (sc->sc_inc.inc_isipv6) {
		inp->in6p_laddr = sc->sc_inc.inc6_laddr;
	} else {
		inp->inp_vflag &= ~INP_IPV6;
		inp->inp_vflag |= INP_IPV4;
#endif
		inp->inp_laddr = sc->sc_inc.inc_laddr;
#ifdef INET6
	}
#endif
	inp->inp_lport = sc->sc_inc.inc_lport;
	if (in_pcbinshash(inp) != 0) {
		/*
		 * Undo the assignments above if we failed to
		 * put the PCB on the hash lists.
		 */
#ifdef INET6
		if (sc->sc_inc.inc_isipv6)
			inp->in6p_laddr = in6addr_any;
       		else
#endif
			inp->inp_laddr.s_addr = INADDR_ANY;
		inp->inp_lport = 0;
		goto abort;
	}
#ifdef IPSEC
	/* copy old policy into new socket's */
	if (ipsec_copy_policy(sotoinpcb(lso)->inp_sp, inp->inp_sp))
		printf("syncache_expand: could not copy policy\n");
#endif
#ifdef INET6
	if (sc->sc_inc.inc_isipv6) {
		struct inpcb *oinp = sotoinpcb(lso);
		struct in6_addr laddr6;
		struct sockaddr_in6 *sin6;
		/*
		 * Inherit socket options from the listening socket.
		 * Note that in6p_inputopts are not (and should not be)
		 * copied, since it stores previously received options and is
		 * used to detect if each new option is different than the
		 * previous one and hence should be passed to a user.
                 * If we copied in6p_inputopts, a user would not be able to
		 * receive options just after calling the accept system call.
		 */
		inp->inp_flags |= oinp->inp_flags & INP_CONTROLOPTS;
		if (oinp->in6p_outputopts)
			inp->in6p_outputopts =
			    ip6_copypktopts(oinp->in6p_outputopts, M_NOWAIT);
		inp->in6p_route = sc->sc_route6;
		sc->sc_route6.ro_rt = NULL;

		MALLOC(sin6, struct sockaddr_in6 *, sizeof *sin6,
		    M_SONAME, M_NOWAIT | M_ZERO);
		if (sin6 == NULL)
			goto abort;
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_addr = sc->sc_inc.inc6_faddr;
		sin6->sin6_port = sc->sc_inc.inc_fport;
		laddr6 = inp->in6p_laddr;
		if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr))
			inp->in6p_laddr = sc->sc_inc.inc6_laddr;
		if (in6_pcbconnect(inp, (struct sockaddr *)sin6, &proc0)) {
			inp->in6p_laddr = laddr6;
			FREE(sin6, M_SONAME);
			goto abort;
		}
		FREE(sin6, M_SONAME);
	} else
#endif
	{
		struct in_addr laddr;
		struct sockaddr_in *sin;

		inp->inp_options = ip_srcroute();
		if (inp->inp_options == NULL) {
			inp->inp_options = sc->sc_ipopts;
			sc->sc_ipopts = NULL;
		}
		inp->inp_route = sc->sc_route;
		sc->sc_route.ro_rt = NULL;

		MALLOC(sin, struct sockaddr_in *, sizeof *sin,
		    M_SONAME, M_NOWAIT | M_ZERO);
		if (sin == NULL)
			goto abort;
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = sc->sc_inc.inc_faddr;
		sin->sin_port = sc->sc_inc.inc_fport;
		bzero((caddr_t)sin->sin_zero, sizeof(sin->sin_zero));
		laddr = inp->inp_laddr;
		if (inp->inp_laddr.s_addr == INADDR_ANY)
			inp->inp_laddr = sc->sc_inc.inc_laddr;
		if (in_pcbconnect(inp, (struct sockaddr *)sin, &proc0)) {
			inp->inp_laddr = laddr;
			FREE(sin, M_SONAME);
			goto abort;
		}
		FREE(sin, M_SONAME);
	}

	tp = intotcpcb(inp);
	tp->t_state = TCPS_SYN_RECEIVED;
	tp->iss = sc->sc_iss;
	tp->irs = sc->sc_irs;
	tcp_rcvseqinit(tp);
	tcp_sendseqinit(tp);
	tp->snd_wl1 = sc->sc_irs;
	tp->rcv_up = sc->sc_irs + 1;
	tp->rcv_wnd = sc->sc_wnd;
	tp->rcv_adv += tp->rcv_wnd;

	tp->t_flags = sototcpcb(lso)->t_flags & (TF_NOPUSH|TF_NODELAY);
	if (sc->sc_flags & SCF_NOOPT)
		tp->t_flags |= TF_NOOPT;
	if (sc->sc_flags & SCF_WINSCALE) {
		tp->t_flags |= TF_REQ_SCALE|TF_RCVD_SCALE;
		tp->requested_s_scale = sc->sc_requested_s_scale;
		tp->request_r_scale = sc->sc_request_r_scale;
	}
	if (sc->sc_flags & SCF_TIMESTAMP) {
		tp->t_flags |= TF_REQ_TSTMP|TF_RCVD_TSTMP;
		tp->ts_recent = sc->sc_tsrecent;
		tp->ts_recent_age = ticks;
	}
	if (sc->sc_flags & SCF_CC) {
		/*
		 * Initialization of the tcpcb for transaction;
		 *   set SND.WND = SEG.WND,
		 *   initialize CCsend and CCrecv.
		 */
		tp->t_flags |= TF_REQ_CC|TF_RCVD_CC;
		tp->cc_send = sc->sc_cc_send;
		tp->cc_recv = sc->sc_cc_recv;
	}

	tcp_mss(tp, sc->sc_peer_mss);

	/*
	 * If the SYN,ACK was retransmitted, reset cwnd to 1 segment.
	 */
	if (sc->sc_rxtslot != 0)
                tp->snd_cwnd = tp->t_maxseg;
	callout_reset(tp->tt_keep, tcp_keepinit, tcp_timer_keep, tp);

	tcpstat.tcps_accepts++;
	return (so);

abort:
	if (so != NULL)
		(void) soabort(so);
	return (NULL);
}

/*
 * This function gets called when we receive an ACK for a
 * socket in the LISTEN state.  We look up the connection
 * in the syncache, and if its there, we pull it out of
 * the cache and turn it into a full-blown connection in
 * the SYN-RECEIVED state.
 */
int
syncache_expand(inc, th, sop, m)
	struct in_conninfo *inc;
	struct tcphdr *th;
	struct socket **sop;
	struct mbuf *m;
{
	struct syncache *sc;
	struct syncache_head *sch;
	struct socket *so;

	sc = syncache_lookup(inc, &sch);
	if (sc == NULL) {
		/*
		 * There is no syncache entry, so see if this ACK is 
		 * a returning syncookie.  To do this, first:
		 *  A. See if this socket has had a syncache entry dropped in
		 *     the past.  We don't want to accept a bogus syncookie
 		 *     if we've never received a SYN.
		 *  B. check that the syncookie is valid.  If it is, then
		 *     cobble up a fake syncache entry, and return.
		 */
		if (!tcp_syncookies)
			return (0);
		sc = syncookie_lookup(inc, th, *sop);
		if (sc == NULL)
			return (0);
		sch = NULL;
		tcpstat.tcps_sc_recvcookie++;
	}

	/*
	 * If seg contains an ACK, but not for our SYN/ACK, send a RST.
	 */
	if (th->th_ack != sc->sc_iss + 1)
		return (0);

	so = syncache_socket(sc, *sop);
	if (so == NULL) {
#if 0
resetandabort:
		/* XXXjlemon check this - is this correct? */
		(void) tcp_respond(NULL, m, m, th,
		    th->th_seq + tlen, (tcp_seq)0, TH_RST|TH_ACK);
#endif
		m_freem(m);			/* XXX only needed for above */
		tcpstat.tcps_sc_aborted++;
	} else {
		sc->sc_flags |= SCF_KEEPROUTE;
		tcpstat.tcps_sc_completed++;
	}
	if (sch == NULL)
		syncache_free(sc);
	else
		syncache_drop(sc, sch);
	*sop = so;
	return (1);
}

/*
 * Given a LISTEN socket and an inbound SYN request, add
 * this to the syn cache, and send back a segment:
 *	<SEQ=ISS><ACK=RCV_NXT><CTL=SYN,ACK>
 * to the source.
 *
 * IMPORTANT NOTE: We do _NOT_ ACK data that might accompany the SYN.
 * Doing so would require that we hold onto the data and deliver it
 * to the application.  However, if we are the target of a SYN-flood
 * DoS attack, an attacker could send data which would eventually
 * consume all available buffer space if it were ACKed.  By not ACKing
 * the data, we avoid this DoS scenario.
 */
int
syncache_add(inc, to, th, sop, m)
	struct in_conninfo *inc;
	struct tcpopt *to;
	struct tcphdr *th;
	struct socket **sop;
	struct mbuf *m;
{
	struct tcpcb *tp;
	struct socket *so;
	struct syncache *sc = NULL;
	struct syncache_head *sch;
	struct mbuf *ipopts = NULL;
	struct rmxp_tao *taop;
	int i, s, win;

	so = *sop;
	tp = sototcpcb(so);

	/*
	 * Remember the IP options, if any.
	 */
#ifdef INET6
	if (!inc->inc_isipv6)
#endif
		ipopts = ip_srcroute();

	/*
	 * See if we already have an entry for this connection.
	 * If we do, resend the SYN,ACK, and reset the retransmit timer.
	 *
	 * XXX
	 * should the syncache be re-initialized with the contents
	 * of the new SYN here (which may have different options?)
	 */
	sc = syncache_lookup(inc, &sch);
	if (sc != NULL) {
		tcpstat.tcps_sc_dupsyn++;
		if (ipopts) {
			/*
			 * If we were remembering a previous source route,
			 * forget it and use the new one we've been given.
			 */
			if (sc->sc_ipopts)
				(void) m_free(sc->sc_ipopts);
			sc->sc_ipopts = ipopts;
		}
		/*
		 * Update timestamp if present.
		 */
		if (sc->sc_flags & SCF_TIMESTAMP)
			sc->sc_tsrecent = to->to_tsval;
		/*
		 * PCB may have changed, pick up new values.
		 */
		sc->sc_tp = tp;
		sc->sc_inp_gencnt = tp->t_inpcb->inp_gencnt;
		if (syncache_respond(sc, m) == 0) {
		        s = splnet();
			TAILQ_REMOVE(&tcp_syncache.timerq[sc->sc_rxtslot],
			    sc, sc_timerq);
			SYNCACHE_TIMEOUT(sc, sc->sc_rxtslot);
		        splx(s);
		 	tcpstat.tcps_sndacks++;
			tcpstat.tcps_sndtotal++;
		}
		*sop = NULL;
		return (1);
	}

	sc = zalloc(tcp_syncache.zone);
	if (sc == NULL) {
		/*
		 * The zone allocator couldn't provide more entries.
		 * Treat this as if the cache was full; drop the oldest 
		 * entry and insert the new one.
		 */
		s = splnet();
		for (i = SYNCACHE_MAXREXMTS; i >= 0; i--) {
			sc = TAILQ_FIRST(&tcp_syncache.timerq[i]);
			if (sc != NULL)
				break;
		}
		sc->sc_tp->ts_recent = ticks;
		syncache_drop(sc, NULL);
		splx(s);
		tcpstat.tcps_sc_zonefail++;
		sc = zalloc(tcp_syncache.zone);
		if (sc == NULL) {
			if (ipopts)
				(void) m_free(ipopts);
			return (0);
		}
	}

	/*
	 * Fill in the syncache values.
	 */
	bzero(sc, sizeof(*sc));
	sc->sc_tp = tp;
	sc->sc_inp_gencnt = tp->t_inpcb->inp_gencnt;
	sc->sc_ipopts = ipopts;
	sc->sc_inc.inc_fport = inc->inc_fport;
	sc->sc_inc.inc_lport = inc->inc_lport;
#ifdef INET6
	sc->sc_inc.inc_isipv6 = inc->inc_isipv6;
	if (inc->inc_isipv6) {
		sc->sc_inc.inc6_faddr = inc->inc6_faddr;
		sc->sc_inc.inc6_laddr = inc->inc6_laddr;
		sc->sc_route6.ro_rt = NULL;
	} else
#endif
	{
		sc->sc_inc.inc_faddr = inc->inc_faddr;
		sc->sc_inc.inc_laddr = inc->inc_laddr;
		sc->sc_route.ro_rt = NULL;
	}
	sc->sc_irs = th->th_seq;
	if (tcp_syncookies)
		sc->sc_iss = syncookie_generate(sc);
	else
		sc->sc_iss = arc4random();

	/* Initial receive window: clip sbspace to [0 .. TCP_MAXWIN] */
	win = sbspace(&so->so_rcv);
	win = imax(win, 0);
	win = imin(win, TCP_MAXWIN);
	sc->sc_wnd = win;

	sc->sc_flags = 0;
	sc->sc_peer_mss = to->to_flags & TOF_MSS ? to->to_mss : 0;
	if (tcp_do_rfc1323) {
		/*
		 * A timestamp received in a SYN makes
		 * it ok to send timestamp requests and replies.
		 */
		if (to->to_flags & TOF_TS) {
			sc->sc_tsrecent = to->to_tsval;
			sc->sc_flags |= SCF_TIMESTAMP;
		}
		if (to->to_flags & TOF_SCALE) {
			int wscale = 0;

			/* Compute proper scaling value from buffer space */
			while (wscale < TCP_MAX_WINSHIFT &&
			    (TCP_MAXWIN << wscale) < so->so_rcv.sb_hiwat)
				wscale++;
			sc->sc_request_r_scale = wscale;
			sc->sc_requested_s_scale = to->to_requested_s_scale;
			sc->sc_flags |= SCF_WINSCALE;
		}
	}
	if (tcp_do_rfc1644) {
		/*
		 * A CC or CC.new option received in a SYN makes
		 * it ok to send CC in subsequent segments.
		 */
		if (to->to_flags & (TOF_CC|TOF_CCNEW)) {
			sc->sc_cc_recv = to->to_cc;
			sc->sc_cc_send = CC_INC(tcp_ccgen);
			sc->sc_flags |= SCF_CC;
		}
	}
	if (tp->t_flags & TF_NOOPT)
		sc->sc_flags = SCF_NOOPT;

	/*
	 * XXX
	 * We have the option here of not doing TAO (even if the segment
	 * qualifies) and instead fall back to a normal 3WHS via the syncache.
	 * This allows us to apply synflood protection to TAO-qualifying SYNs
	 * also. However, there should be a hueristic to determine when to
	 * do this, and is not present at the moment.
	 */

	/*
	 * Perform TAO test on incoming CC (SEG.CC) option, if any.
	 * - compare SEG.CC against cached CC from the same host, if any.
	 * - if SEG.CC > chached value, SYN must be new and is accepted
	 *	immediately: save new CC in the cache, mark the socket
	 *	connected, enter ESTABLISHED state, turn on flag to
	 *	send a SYN in the next segment.
	 *	A virtual advertised window is set in rcv_adv to
	 *	initialize SWS prevention.  Then enter normal segment
	 *	processing: drop SYN, process data and FIN.
	 * - otherwise do a normal 3-way handshake.
	 */
	taop = tcp_gettaocache(&sc->sc_inc);
	if ((to->to_flags & TOF_CC) != 0) {
		if (((tp->t_flags & TF_NOPUSH) != 0) &&
		    sc->sc_flags & SCF_CC && 
		    taop != NULL && taop->tao_cc != 0 &&
		    CC_GT(to->to_cc, taop->tao_cc)) {
			sc->sc_rxtslot = 0;
			so = syncache_socket(sc, *sop);
			if (so != NULL) {
				sc->sc_flags |= SCF_KEEPROUTE;
				taop->tao_cc = to->to_cc;
				*sop = so;
			}
			syncache_free(sc);
			return (so != NULL);
		}
	} else {
		/*
		 * No CC option, but maybe CC.NEW: invalidate cached value.
		 */
		if (taop != NULL)
			taop->tao_cc = 0;
	}
	/*
	 * TAO test failed or there was no CC option,
	 *    do a standard 3-way handshake.
	 */
	if (syncache_respond(sc, m) == 0) {
		syncache_insert(sc, sch);
		tcpstat.tcps_sndacks++;
		tcpstat.tcps_sndtotal++;
	} else {
		syncache_free(sc);
		tcpstat.tcps_sc_dropped++;
	}
	*sop = NULL;
	return (1);
}

static int
syncache_respond(sc, m)
	struct syncache *sc;
	struct mbuf *m;
{
	u_int8_t *optp;
	int optlen, error;
	u_int16_t tlen, hlen, mssopt;
	struct ip *ip = NULL;
	struct rtentry *rt;
	struct tcphdr *th;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif

#ifdef INET6
	if (sc->sc_inc.inc_isipv6) {
		rt = tcp_rtlookup6(&sc->sc_inc);
		if (rt != NULL)
			mssopt = rt->rt_ifp->if_mtu -
			     (sizeof(struct ip6_hdr) + sizeof(struct tcphdr));
		else 
			mssopt = tcp_v6mssdflt;
		hlen = sizeof(struct ip6_hdr);
	} else
#endif
	{
		rt = tcp_rtlookup(&sc->sc_inc);
		if (rt != NULL)
			mssopt = rt->rt_ifp->if_mtu -
			     (sizeof(struct ip) + sizeof(struct tcphdr));
		else 
			mssopt = tcp_mssdflt;
		hlen = sizeof(struct ip);
	}

	/* Compute the size of the TCP options. */
	if (sc->sc_flags & SCF_NOOPT) {
		optlen = 0;
	} else {
		optlen = TCPOLEN_MAXSEG +
		    ((sc->sc_flags & SCF_WINSCALE) ? 4 : 0) +
		    ((sc->sc_flags & SCF_TIMESTAMP) ? TCPOLEN_TSTAMP_APPA : 0) +
		    ((sc->sc_flags & SCF_CC) ? TCPOLEN_CC_APPA * 2 : 0);
	}
	tlen = hlen + sizeof(struct tcphdr) + optlen;

	/*
	 * XXX
	 * assume that the entire packet will fit in a header mbuf
	 */
	KASSERT(max_linkhdr + tlen <= MHLEN, ("syncache: mbuf too small"));

	/*
	 * XXX shouldn't this reuse the mbuf if possible ?
	 * Create the IP+TCP header from scratch.
	 */
	if (m)
		m_freem(m);

	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		return (ENOBUFS);
	m->m_data += max_linkhdr;
	m->m_len = tlen;
	m->m_pkthdr.len = tlen;
	m->m_pkthdr.rcvif = NULL;

#ifdef INET6
	if (sc->sc_inc.inc_isipv6) {
		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_vfc = IPV6_VERSION;
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_src = sc->sc_inc.inc6_laddr;
		ip6->ip6_dst = sc->sc_inc.inc6_faddr;
		ip6->ip6_plen = htons(tlen - hlen);
		/* ip6_hlim is set after checksum */
		/* ip6_flow = ??? */

		th = (struct tcphdr *)(ip6 + 1);
	} else
#endif
	{
		ip = mtod(m, struct ip *);
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(struct ip) >> 2;
		ip->ip_len = tlen;
		ip->ip_id = 0;
		ip->ip_off = 0;
		ip->ip_sum = 0;
		ip->ip_p = IPPROTO_TCP;
		ip->ip_src = sc->sc_inc.inc_laddr;
		ip->ip_dst = sc->sc_inc.inc_faddr;
		ip->ip_ttl = sc->sc_tp->t_inpcb->inp_ip_ttl;   /* XXX */
		ip->ip_tos = sc->sc_tp->t_inpcb->inp_ip_tos;   /* XXX */

		/*
		 * See if we should do MTU discovery.  Route lookups are expensive,
		 * so we will only unset the DF bit if:
		 *
		 *	1) path_mtu_discovery is disabled
		 *	2) the SCF_UNREACH flag has been set
		 */
		if (path_mtu_discovery
		    && ((sc->sc_flags & SCF_UNREACH) == 0)) {
		       ip->ip_off |= IP_DF;
		}

		th = (struct tcphdr *)(ip + 1);
	}
	th->th_sport = sc->sc_inc.inc_lport;
	th->th_dport = sc->sc_inc.inc_fport;

	th->th_seq = htonl(sc->sc_iss);
	th->th_ack = htonl(sc->sc_irs + 1);
	th->th_off = (sizeof(struct tcphdr) + optlen) >> 2;
	th->th_x2 = 0;
	th->th_flags = TH_SYN|TH_ACK;
	th->th_win = htons(sc->sc_wnd);
	th->th_urp = 0;

	/* Tack on the TCP options. */
	if (optlen == 0)
		goto no_options;
	optp = (u_int8_t *)(th + 1);
	*optp++ = TCPOPT_MAXSEG;
	*optp++ = TCPOLEN_MAXSEG;
	*optp++ = (mssopt >> 8) & 0xff;
	*optp++ = mssopt & 0xff;

	if (sc->sc_flags & SCF_WINSCALE) {
		*((u_int32_t *)optp) = htonl(TCPOPT_NOP << 24 |
		    TCPOPT_WINDOW << 16 | TCPOLEN_WINDOW << 8 |
		    sc->sc_request_r_scale);
		optp += 4;
	}

	if (sc->sc_flags & SCF_TIMESTAMP) {
		u_int32_t *lp = (u_int32_t *)(optp);

		/* Form timestamp option as shown in appendix A of RFC 1323. */
		*lp++ = htonl(TCPOPT_TSTAMP_HDR);
		*lp++ = htonl(ticks);
		*lp   = htonl(sc->sc_tsrecent);
		optp += TCPOLEN_TSTAMP_APPA;
	}

	/*
         * Send CC and CC.echo if we received CC from our peer.
         */
        if (sc->sc_flags & SCF_CC) {
		u_int32_t *lp = (u_int32_t *)(optp);

		*lp++ = htonl(TCPOPT_CC_HDR(TCPOPT_CC));
		*lp++ = htonl(sc->sc_cc_send);
		*lp++ = htonl(TCPOPT_CC_HDR(TCPOPT_CCECHO));
		*lp   = htonl(sc->sc_cc_recv);
		optp += TCPOLEN_CC_APPA * 2;
	}
no_options:

#ifdef INET6
	if (sc->sc_inc.inc_isipv6) {
		struct route_in6 *ro6 = &sc->sc_route6;

		th->th_sum = 0;
		th->th_sum = in6_cksum(m, IPPROTO_TCP, hlen, tlen - hlen);
		ip6->ip6_hlim = in6_selecthlim(NULL,
		    ro6->ro_rt ? ro6->ro_rt->rt_ifp : NULL);
		error = ip6_output(m, NULL, ro6, 0, NULL, NULL,
				sc->sc_tp->t_inpcb);
	} else
#endif
	{
        	th->th_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
		    htons(tlen - hlen + IPPROTO_TCP));
		m->m_pkthdr.csum_flags = CSUM_TCP;
		m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
		error = ip_output(m, sc->sc_ipopts, &sc->sc_route, 0, NULL,
				sc->sc_tp->t_inpcb);
	}
	return (error);
}

/*
 * cookie layers:
 *
 *	|. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .|
 *	| peer iss                                                      |
 *	| MD5(laddr,faddr,lport,fport,secret)             |. . . . . . .|
 *	|                     0                       |(A)|             |
 * (A): peer mss index
 */

/*
 * The values below are chosen to minimize the size of the tcp_secret
 * table, as well as providing roughly a 4 second lifetime for the cookie.
 */

#define SYNCOOKIE_HASHSHIFT	2	/* log2(# of 32bit words from hash) */
#define SYNCOOKIE_WNDBITS	7	/* exposed bits for window indexing */
#define SYNCOOKIE_TIMESHIFT	5	/* scale ticks to window time units */

#define SYNCOOKIE_HASHMASK	((1 << SYNCOOKIE_HASHSHIFT) - 1)
#define SYNCOOKIE_WNDMASK	((1 << SYNCOOKIE_WNDBITS) - 1)
#define SYNCOOKIE_NSECRETS	(1 << (SYNCOOKIE_WNDBITS - SYNCOOKIE_HASHSHIFT))
#define SYNCOOKIE_TIMEOUT \
    (hz * (1 << SYNCOOKIE_WNDBITS) / (1 << SYNCOOKIE_TIMESHIFT))
#define SYNCOOKIE_DATAMASK 	((3 << SYNCOOKIE_WNDBITS) | SYNCOOKIE_WNDMASK)

static struct {
	u_int32_t	ts_secbits;
	u_int		ts_expire;
} tcp_secret[SYNCOOKIE_NSECRETS];

static int tcp_msstab[] = { 0, 536, 1460, 8960 };

static MD5_CTX syn_ctx;

#define MD5Add(v)	MD5Update(&syn_ctx, (u_char *)&v, sizeof(v))

/*
 * Consider the problem of a recreated (and retransmitted) cookie.  If the
 * original SYN was accepted, the connection is established.  The second 
 * SYN is inflight, and if it arrives with an ISN that falls within the 
 * receive window, the connection is killed.  
 *
 * However, since cookies have other problems, this may not be worth
 * worrying about.
 */

static u_int32_t
syncookie_generate(struct syncache *sc)
{
	u_int32_t md5_buffer[4];
	u_int32_t data;
	int wnd, idx;

	wnd = ((ticks << SYNCOOKIE_TIMESHIFT) / hz) & SYNCOOKIE_WNDMASK;
	idx = wnd >> SYNCOOKIE_HASHSHIFT;
	if (tcp_secret[idx].ts_expire < ticks) {
		tcp_secret[idx].ts_secbits = arc4random();
		tcp_secret[idx].ts_expire = ticks + SYNCOOKIE_TIMEOUT;
	}
	for (data = sizeof(tcp_msstab) / sizeof(int) - 1; data > 0; data--)
		if (tcp_msstab[data] <= sc->sc_peer_mss)
			break;
	data = (data << SYNCOOKIE_WNDBITS) | wnd;
	data ^= sc->sc_irs;				/* peer's iss */
	MD5Init(&syn_ctx);
#ifdef INET6
	if (sc->sc_inc.inc_isipv6) {
		MD5Add(sc->sc_inc.inc6_laddr);
		MD5Add(sc->sc_inc.inc6_faddr);
	} else
#endif
	{
		MD5Add(sc->sc_inc.inc_laddr);
		MD5Add(sc->sc_inc.inc_faddr);
	}
	MD5Add(sc->sc_inc.inc_lport);
	MD5Add(sc->sc_inc.inc_fport);
	MD5Add(tcp_secret[idx].ts_secbits);
	MD5Final((u_char *)&md5_buffer, &syn_ctx);
	data ^= (md5_buffer[wnd & SYNCOOKIE_HASHMASK] & ~SYNCOOKIE_WNDMASK);
	return (data);
}

static struct syncache *
syncookie_lookup(inc, th, so)
	struct in_conninfo *inc;
	struct tcphdr *th;
	struct socket *so;
{
	u_int32_t md5_buffer[4];
	struct syncache *sc;
	u_int32_t data;
	int wnd, idx;

	data = (th->th_ack - 1) ^ (th->th_seq - 1);	/* remove ISS */
	wnd = data & SYNCOOKIE_WNDMASK;
	idx = wnd >> SYNCOOKIE_HASHSHIFT;
	if (tcp_secret[idx].ts_expire < ticks ||
	    sototcpcb(so)->ts_recent + SYNCOOKIE_TIMEOUT < ticks)
		return (NULL);
	MD5Init(&syn_ctx);
#ifdef INET6
	if (inc->inc_isipv6) {
		MD5Add(inc->inc6_laddr);
		MD5Add(inc->inc6_faddr);
	} else
#endif
	{
		MD5Add(inc->inc_laddr);
		MD5Add(inc->inc_faddr);
	}
	MD5Add(inc->inc_lport);
	MD5Add(inc->inc_fport);
	MD5Add(tcp_secret[idx].ts_secbits);
	MD5Final((u_char *)&md5_buffer, &syn_ctx);
	data ^= md5_buffer[wnd & SYNCOOKIE_HASHMASK];
	if ((data & ~SYNCOOKIE_DATAMASK) != 0)
		return (NULL);
	data = data >> SYNCOOKIE_WNDBITS;

	sc = zalloc(tcp_syncache.zone);
	if (sc == NULL)
		return (NULL);
	/*
	 * Fill in the syncache values.
	 * XXX duplicate code from syncache_add
	 */
	sc->sc_ipopts = NULL;
	sc->sc_inc.inc_fport = inc->inc_fport;
	sc->sc_inc.inc_lport = inc->inc_lport;
#ifdef INET6
	sc->sc_inc.inc_isipv6 = inc->inc_isipv6;
	if (inc->inc_isipv6) {
		sc->sc_inc.inc6_faddr = inc->inc6_faddr;
		sc->sc_inc.inc6_laddr = inc->inc6_laddr;
		sc->sc_route6.ro_rt = NULL;
	} else
#endif
	{
		sc->sc_inc.inc_faddr = inc->inc_faddr;
		sc->sc_inc.inc_laddr = inc->inc_laddr;
		sc->sc_route.ro_rt = NULL;
	}
	sc->sc_irs = th->th_seq - 1;
	sc->sc_iss = th->th_ack - 1;
	wnd = sbspace(&so->so_rcv);
	wnd = imax(wnd, 0);
	wnd = imin(wnd, TCP_MAXWIN);
	sc->sc_wnd = wnd;
	sc->sc_flags = 0;
	sc->sc_rxtslot = 0;
	sc->sc_peer_mss = tcp_msstab[data];
	return (sc);
}
