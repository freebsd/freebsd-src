/*-
 * Copyright (c) 2001 McAfee, Inc.
 * Copyright (c) 2006 Andre Oppermann, Internet Business Solutions AG
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Jonathan Lemon
 * and McAfee Research, the Security Research Division of McAfee, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
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

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_mac.h"
#include "opt_tcpdebug.h"
#include "opt_tcp_sack.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mac.h>
#include <sys/mbuf.h>
#include <sys/md5.h>
#include <sys/proc.h>		/* for proc0 declaration */
#include <sys/random.h>
#include <sys/rwlock.h>
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
#include <netinet/ip_options.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#endif
#include <netinet/tcp.h>
#ifdef TCPDEBUG
#include <netinet/tcpip.h>
#endif
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif
#ifdef INET6
#include <netinet6/tcp6_var.h>
#endif

#ifdef IPSEC
#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/ipsec6.h>
#endif
#endif /*IPSEC*/

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#ifdef INET6
#include <netipsec/ipsec6.h>
#endif
#include <netipsec/key.h>
#endif /*FAST_IPSEC*/

#include <machine/in_cksum.h>
#include <vm/uma.h>

static int tcp_syncookies = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, syncookies, CTLFLAG_RW,
    &tcp_syncookies, 0,
    "Use TCP SYN cookies if the syncache overflows");

static void	 syncache_drop(struct syncache *, struct syncache_head *);
static void	 syncache_free(struct syncache *);
static void	 syncache_insert(struct syncache *, struct syncache_head *);
struct syncache *syncache_lookup(struct in_conninfo *, struct syncache_head **);
static int	 syncache_respond(struct syncache *, struct mbuf *);
static struct	 socket *syncache_socket(struct syncache *, struct socket *,
		    struct mbuf *m);
static void	 syncache_timer(void *);
static void	 syncookie_init(void);
static u_int32_t syncookie_generate(struct syncache *, u_int32_t *);
static struct syncache
		 *syncookie_lookup(struct in_conninfo *, struct tcphdr *,
		    struct socket *);

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
	uma_zone_t zone;
	u_int	hashsize;
	u_int	hashmask;
	u_int	bucket_limit;
	u_int	cache_count;		/* XXX: unprotected */
	u_int	cache_limit;
	u_int	rexmt_limit;
	u_int	hash_secret;
};
static struct tcp_syncache tcp_syncache;

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, syncache, CTLFLAG_RW, 0, "TCP SYN cache");

SYSCTL_INT(_net_inet_tcp_syncache, OID_AUTO, bucketlimit, CTLFLAG_RDTUN,
     &tcp_syncache.bucket_limit, 0, "Per-bucket hash limit for syncache");

SYSCTL_INT(_net_inet_tcp_syncache, OID_AUTO, cachelimit, CTLFLAG_RDTUN,
     &tcp_syncache.cache_limit, 0, "Overall entry limit for syncache");

SYSCTL_INT(_net_inet_tcp_syncache, OID_AUTO, count, CTLFLAG_RD,
     &tcp_syncache.cache_count, 0, "Current number of entries in syncache");

SYSCTL_INT(_net_inet_tcp_syncache, OID_AUTO, hashsize, CTLFLAG_RDTUN,
     &tcp_syncache.hashsize, 0, "Size of TCP syncache hashtable");

SYSCTL_INT(_net_inet_tcp_syncache, OID_AUTO, rexmtlimit, CTLFLAG_RW,
     &tcp_syncache.rexmt_limit, 0, "Limit on SYN/ACK retransmissions");

static MALLOC_DEFINE(M_SYNCACHE, "syncache", "TCP syncache");

#define SYNCACHE_HASH(inc, mask)					\
	((tcp_syncache.hash_secret ^					\
	  (inc)->inc_faddr.s_addr ^					\
	  ((inc)->inc_faddr.s_addr >> 16) ^				\
	  (inc)->inc_fport ^ (inc)->inc_lport) & mask)

#define SYNCACHE_HASH6(inc, mask)					\
	((tcp_syncache.hash_secret ^					\
	  (inc)->inc6_faddr.s6_addr32[0] ^				\
	  (inc)->inc6_faddr.s6_addr32[3] ^				\
	  (inc)->inc_fport ^ (inc)->inc_lport) & mask)

#define ENDPTS_EQ(a, b) (						\
	(a)->ie_fport == (b)->ie_fport &&				\
	(a)->ie_lport == (b)->ie_lport &&				\
	(a)->ie_faddr.s_addr == (b)->ie_faddr.s_addr &&			\
	(a)->ie_laddr.s_addr == (b)->ie_laddr.s_addr			\
)

#define ENDPTS6_EQ(a, b) (memcmp(a, b, sizeof(*a)) == 0)

#define SYNCACHE_TIMEOUT(sc, sch, co) do {				\
	(sc)->sc_rxmits++;						\
	(sc)->sc_rxttime = ticks +					\
		TCPTV_RTOBASE * tcp_backoff[(sc)->sc_rxmits - 1];	\
	if ((sch)->sch_nextc > (sc)->sc_rxttime)			\
		(sch)->sch_nextc = (sc)->sc_rxttime;			\
	if (!TAILQ_EMPTY(&(sch)->sch_bucket) && !(co))			\
		callout_reset(&(sch)->sch_timer,			\
			(sch)->sch_nextc - ticks,			\
			syncache_timer, (void *)(sch));			\
} while (0)

#define	SCH_LOCK(sch)		mtx_lock(&(sch)->sch_mtx)
#define	SCH_UNLOCK(sch)		mtx_unlock(&(sch)->sch_mtx)
#define	SCH_LOCK_ASSERT(sch)	mtx_assert(&(sch)->sch_mtx, MA_OWNED)

/*
 * Requires the syncache entry to be already removed from the bucket list.
 */
static void
syncache_free(struct syncache *sc)
{
	if (sc->sc_ipopts)
		(void) m_free(sc->sc_ipopts);

	uma_zfree(tcp_syncache.zone, sc);
}

void
syncache_init(void)
{
	int i;

	tcp_syncache.cache_count = 0;
	tcp_syncache.hashsize = TCP_SYNCACHE_HASHSIZE;
	tcp_syncache.bucket_limit = TCP_SYNCACHE_BUCKETLIMIT;
	tcp_syncache.rexmt_limit = SYNCACHE_MAXREXMTS;
	tcp_syncache.hash_secret = arc4random();

	TUNABLE_INT_FETCH("net.inet.tcp.syncache.hashsize",
	    &tcp_syncache.hashsize);
	TUNABLE_INT_FETCH("net.inet.tcp.syncache.bucketlimit",
	    &tcp_syncache.bucket_limit);
	if (!powerof2(tcp_syncache.hashsize) || tcp_syncache.hashsize == 0) {
		printf("WARNING: syncache hash size is not a power of 2.\n");
		tcp_syncache.hashsize = TCP_SYNCACHE_HASHSIZE;
	}
	tcp_syncache.hashmask = tcp_syncache.hashsize - 1;

	/* Set limits. */
	tcp_syncache.cache_limit =
	    tcp_syncache.hashsize * tcp_syncache.bucket_limit;
	TUNABLE_INT_FETCH("net.inet.tcp.syncache.cachelimit",
	    &tcp_syncache.cache_limit);

	/* Allocate the hash table. */
	MALLOC(tcp_syncache.hashbase, struct syncache_head *,
	    tcp_syncache.hashsize * sizeof(struct syncache_head),
	    M_SYNCACHE, M_WAITOK);

	/* Initialize the hash buckets. */
	for (i = 0; i < tcp_syncache.hashsize; i++) {
		TAILQ_INIT(&tcp_syncache.hashbase[i].sch_bucket);
		mtx_init(&tcp_syncache.hashbase[i].sch_mtx, "tcp_sc_head",
			 NULL, MTX_DEF);
		callout_init_mtx(&tcp_syncache.hashbase[i].sch_timer,
			 &tcp_syncache.hashbase[i].sch_mtx, 0);
		tcp_syncache.hashbase[i].sch_length = 0;
	}

	syncookie_init();

	/* Create the syncache entry zone. */
	tcp_syncache.zone = uma_zcreate("syncache", sizeof(struct syncache),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	uma_zone_set_max(tcp_syncache.zone, tcp_syncache.cache_limit);
}

/*
 * Inserts a syncache entry into the specified bucket row.
 * Locks and unlocks the syncache_head autonomously.
 */
static void
syncache_insert(struct syncache *sc, struct syncache_head *sch)
{
	struct syncache *sc2;

	SCH_LOCK(sch);

	/*
	 * Make sure that we don't overflow the per-bucket limit.
	 * If the bucket is full, toss the oldest element.
	 */
	if (sch->sch_length >= tcp_syncache.bucket_limit) {
		KASSERT(!TAILQ_EMPTY(&sch->sch_bucket),
			("sch->sch_length incorrect"));
		sc2 = TAILQ_LAST(&sch->sch_bucket, sch_head);
		syncache_drop(sc2, sch);
		tcpstat.tcps_sc_bucketoverflow++;
	}

	/* Put it into the bucket. */
	TAILQ_INSERT_HEAD(&sch->sch_bucket, sc, sc_hash);
	sch->sch_length++;

	/* Reinitialize the bucket row's timer. */
	SYNCACHE_TIMEOUT(sc, sch, 1);

	SCH_UNLOCK(sch);

	tcp_syncache.cache_count++;
	tcpstat.tcps_sc_added++;
}

/*
 * Remove and free entry from syncache bucket row.
 * Expects locked syncache head.
 */
static void
syncache_drop(struct syncache *sc, struct syncache_head *sch)
{

	SCH_LOCK_ASSERT(sch);

	TAILQ_REMOVE(&sch->sch_bucket, sc, sc_hash);
	sch->sch_length--;

	syncache_free(sc);
	tcp_syncache.cache_count--;
}

/*
 * Walk the timer queues, looking for SYN,ACKs that need to be retransmitted.
 * If we have retransmitted an entry the maximum number of times, expire it.
 * One separate timer for each bucket row.
 */
static void
syncache_timer(void *xsch)
{
	struct syncache_head *sch = (struct syncache_head *)xsch;
	struct syncache *sc, *nsc;
	int tick = ticks;

	/* NB: syncache_head has already been locked by the callout. */
	SCH_LOCK_ASSERT(sch);

	TAILQ_FOREACH_SAFE(sc, &sch->sch_bucket, sc_hash, nsc) {
		/*
		 * We do not check if the listen socket still exists
		 * and accept the case where the listen socket may be
		 * gone by the time we resend the SYN/ACK.  We do
		 * not expect this to happens often. If it does,
		 * then the RST will be sent by the time the remote
		 * host does the SYN/ACK->ACK.
		 */
		if (sc->sc_rxttime >= tick) {
			if (sc->sc_rxttime < sch->sch_nextc)
				sch->sch_nextc = sc->sc_rxttime;
			continue;
		}

		if (sc->sc_rxmits > tcp_syncache.rexmt_limit) {
			syncache_drop(sc, sch);
			tcpstat.tcps_sc_stale++;
			continue;
		}

		(void) syncache_respond(sc, NULL);
		tcpstat.tcps_sc_retransmitted++;
		SYNCACHE_TIMEOUT(sc, sch, 0);
	}
	if (!TAILQ_EMPTY(&(sch)->sch_bucket))
		callout_reset(&(sch)->sch_timer, (sch)->sch_nextc - tick,
			syncache_timer, (void *)(sch));
}

/*
 * Find an entry in the syncache.
 * Returns always with locked syncache_head plus a matching entry or NULL.
 */
struct syncache *
syncache_lookup(struct in_conninfo *inc, struct syncache_head **schp)
{
	struct syncache *sc;
	struct syncache_head *sch;

#ifdef INET6
	if (inc->inc_isipv6) {
		sch = &tcp_syncache.hashbase[
		    SYNCACHE_HASH6(inc, tcp_syncache.hashmask)];
		*schp = sch;

		SCH_LOCK(sch);

		/* Circle through bucket row to find matching entry. */
		TAILQ_FOREACH(sc, &sch->sch_bucket, sc_hash) {
			if (ENDPTS6_EQ(&inc->inc_ie, &sc->sc_inc.inc_ie))
				return (sc);
		}
	} else
#endif
	{
		sch = &tcp_syncache.hashbase[
		    SYNCACHE_HASH(inc, tcp_syncache.hashmask)];
		*schp = sch;

		SCH_LOCK(sch);

		/* Circle through bucket row to find matching entry. */
		TAILQ_FOREACH(sc, &sch->sch_bucket, sc_hash) {
#ifdef INET6
			if (sc->sc_inc.inc_isipv6)
				continue;
#endif
			if (ENDPTS_EQ(&inc->inc_ie, &sc->sc_inc.inc_ie))
				return (sc);
		}
	}
	SCH_LOCK_ASSERT(*schp);
	return (NULL);			/* always returns with locked sch */
}

/*
 * This function is called when we get a RST for a
 * non-existent connection, so that we can see if the
 * connection is in the syn cache.  If it is, zap it.
 */
void
syncache_chkrst(struct in_conninfo *inc, struct tcphdr *th)
{
	struct syncache *sc;
	struct syncache_head *sch;

	sc = syncache_lookup(inc, &sch);	/* returns locked sch */
	SCH_LOCK_ASSERT(sch);
	if (sc == NULL)
		goto done;

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
done:
	SCH_UNLOCK(sch);
}

void
syncache_badack(struct in_conninfo *inc)
{
	struct syncache *sc;
	struct syncache_head *sch;

	sc = syncache_lookup(inc, &sch);	/* returns locked sch */
	SCH_LOCK_ASSERT(sch);
	if (sc != NULL) {
		syncache_drop(sc, sch);
		tcpstat.tcps_sc_badack++;
	}
	SCH_UNLOCK(sch);
}

void
syncache_unreach(struct in_conninfo *inc, struct tcphdr *th)
{
	struct syncache *sc;
	struct syncache_head *sch;

	sc = syncache_lookup(inc, &sch);	/* returns locked sch */
	SCH_LOCK_ASSERT(sch);
	if (sc == NULL)
		goto done;

	/* If the sequence number != sc_iss, then it's a bogus ICMP msg */
	if (ntohl(th->th_seq) != sc->sc_iss)
		goto done;

	/*
	 * If we've rertransmitted 3 times and this is our second error,
	 * we remove the entry.  Otherwise, we allow it to continue on.
	 * This prevents us from incorrectly nuking an entry during a
	 * spurious network outage.
	 *
	 * See tcp_notify().
	 */
	if ((sc->sc_flags & SCF_UNREACH) == 0 || sc->sc_rxmits < 3 + 1) {
		sc->sc_flags |= SCF_UNREACH;
		goto done;
	}
	syncache_drop(sc, sch);
	tcpstat.tcps_sc_unreach++;
done:
	SCH_UNLOCK(sch);
}

/*
 * Build a new TCP socket structure from a syncache entry.
 */
static struct socket *
syncache_socket(struct syncache *sc, struct socket *lso, struct mbuf *m)
{
	struct inpcb *inp = NULL;
	struct socket *so;
	struct tcpcb *tp;

	NET_ASSERT_GIANT();
	INP_INFO_WLOCK_ASSERT(&tcbinfo);

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
		goto abort2;
	}
#ifdef MAC
	SOCK_LOCK(so);
	mac_set_socket_peer_from_mbuf(m, so);
	SOCK_UNLOCK(so);
#endif

	inp = sotoinpcb(so);
	INP_LOCK(inp);

	/* Insert new socket into PCB hash list. */
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
	/* Copy old policy into new socket's. */
	if (ipsec_copy_pcbpolicy(sotoinpcb(lso)->inp_sp, inp->inp_sp))
		printf("syncache_expand: could not copy policy\n");
#endif
#ifdef FAST_IPSEC
	/* Copy old policy into new socket's. */
	if (ipsec_copy_policy(sotoinpcb(lso)->inp_sp, inp->inp_sp))
		printf("syncache_expand: could not copy policy\n");
#endif
#ifdef INET6
	if (sc->sc_inc.inc_isipv6) {
		struct inpcb *oinp = sotoinpcb(lso);
		struct in6_addr laddr6;
		struct sockaddr_in6 sin6;
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

		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_addr = sc->sc_inc.inc6_faddr;
		sin6.sin6_port = sc->sc_inc.inc_fport;
		sin6.sin6_flowinfo = sin6.sin6_scope_id = 0;
		laddr6 = inp->in6p_laddr;
		if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr))
			inp->in6p_laddr = sc->sc_inc.inc6_laddr;
		if (in6_pcbconnect(inp, (struct sockaddr *)&sin6,
		    thread0.td_ucred)) {
			inp->in6p_laddr = laddr6;
			goto abort;
		}
		/* Override flowlabel from in6_pcbconnect. */
		inp->in6p_flowinfo &= ~IPV6_FLOWLABEL_MASK;
		inp->in6p_flowinfo |= sc->sc_flowlabel;
	} else
#endif
	{
		struct in_addr laddr;
		struct sockaddr_in sin;

		inp->inp_options = ip_srcroute(m);
		if (inp->inp_options == NULL) {
			inp->inp_options = sc->sc_ipopts;
			sc->sc_ipopts = NULL;
		}

		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(sin);
		sin.sin_addr = sc->sc_inc.inc_faddr;
		sin.sin_port = sc->sc_inc.inc_fport;
		bzero((caddr_t)sin.sin_zero, sizeof(sin.sin_zero));
		laddr = inp->inp_laddr;
		if (inp->inp_laddr.s_addr == INADDR_ANY)
			inp->inp_laddr = sc->sc_inc.inc_laddr;
		if (in_pcbconnect(inp, (struct sockaddr *)&sin,
		    thread0.td_ucred)) {
			inp->inp_laddr = laddr;
			goto abort;
		}
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
		tp->snd_scale = sc->sc_requested_s_scale;
		tp->request_r_scale = sc->sc_request_r_scale;
	}
	if (sc->sc_flags & SCF_TIMESTAMP) {
		tp->t_flags |= TF_REQ_TSTMP|TF_RCVD_TSTMP;
		tp->ts_recent = sc->sc_tsrecent;
		tp->ts_recent_age = ticks;
	}
#ifdef TCP_SIGNATURE
	if (sc->sc_flags & SCF_SIGNATURE)
		tp->t_flags |= TF_SIGNATURE;
#endif
	if (sc->sc_flags & SCF_SACK) {
		tp->sack_enable = 1;
		tp->t_flags |= TF_SACK_PERMIT;
	}

	/*
	 * Set up MSS and get cached values from tcp_hostcache.
	 * This might overwrite some of the defaults we just set.
	 */
	tcp_mss(tp, sc->sc_peer_mss);

	/*
	 * If the SYN,ACK was retransmitted, reset cwnd to 1 segment.
	 */
	if (sc->sc_rxmits > 1)
		tp->snd_cwnd = tp->t_maxseg;
	callout_reset(tp->tt_keep, tcp_keepinit, tcp_timer_keep, tp);

	INP_UNLOCK(inp);

	tcpstat.tcps_accepts++;
	return (so);

abort:
	INP_UNLOCK(inp);
abort2:
	if (so != NULL)
		soabort(so);
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
syncache_expand(struct in_conninfo *inc, struct tcphdr *th,
    struct socket **lsop, struct mbuf *m)
{
	struct syncache *sc;
	struct syncache_head *sch;
	struct socket *so;

	/*
	 * Global TCP locks are held because we manipulate the PCB lists
	 * and create a new socket.
	 */
	INP_INFO_WLOCK_ASSERT(&tcbinfo);

	sc = syncache_lookup(inc, &sch);	/* returns locked sch */
	SCH_LOCK_ASSERT(sch);
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
		SCH_UNLOCK(sch);
		sch = NULL;

		if (!tcp_syncookies)
			goto failed;
		sc = syncookie_lookup(inc, th, *lsop);
		if (sc == NULL)
			goto failed;
		tcpstat.tcps_sc_recvcookie++;
	} else {
		/* Pull out the entry to unlock the bucket row. */
		TAILQ_REMOVE(&sch->sch_bucket, sc, sc_hash);
		sch->sch_length--;
		SCH_UNLOCK(sch);
	}

	/*
	 * If seg contains an ACK, but not for our SYN/ACK, send a RST.
	 */
	if (th->th_ack != sc->sc_iss + 1)
		goto failed;

	so = syncache_socket(sc, *lsop, m);

	if (so == NULL) {
#if 0
resetandabort:
		/* XXXjlemon check this - is this correct? */
		(void) tcp_respond(NULL, m, m, th,
		    th->th_seq + tlen, (tcp_seq)0, TH_RST|TH_ACK);
#endif
		m_freem(m);			/* XXX: only needed for above */
		tcpstat.tcps_sc_aborted++;
		if (sch != NULL) {
			syncache_insert(sc, sch);  /* try again later */
			sc = NULL;
		}
		goto failed;
	} else
		tcpstat.tcps_sc_completed++;
	*lsop = so;

	syncache_free(sc);
	return (1);
failed:
	if (sc != NULL)
		syncache_free(sc);
	return (0);
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
syncache_add(struct in_conninfo *inc, struct tcpopt *to, struct tcphdr *th,
    struct inpcb *inp, struct socket **lsop, struct mbuf *m)
{
	struct tcpcb *tp;
	struct socket *so;
	struct syncache *sc = NULL;
	struct syncache_head *sch;
	struct mbuf *ipopts = NULL;
	u_int32_t flowtmp;
	int win, sb_hiwat, ip_ttl, ip_tos;
#ifdef INET6
	int autoflowlabel = 0;
#endif

	INP_INFO_WLOCK_ASSERT(&tcbinfo);
	INP_LOCK_ASSERT(inp);			/* listen socket */

	/*
	 * Combine all so/tp operations very early to drop the INP lock as
	 * soon as possible.
	 */
	so = *lsop;
	tp = sototcpcb(so);

#ifdef INET6
	if (inc->inc_isipv6 &&
	    (inp->in6p_flags & IN6P_AUTOFLOWLABEL))
		autoflowlabel = 1;
#endif
	ip_ttl = inp->inp_ip_ttl;
	ip_tos = inp->inp_ip_tos;
	win = sbspace(&so->so_rcv);
	sb_hiwat = so->so_rcv.sb_hiwat;
	if (tp->t_flags & TF_NOOPT)
		sc->sc_flags = SCF_NOOPT;

	so = NULL;
	tp = NULL;

	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&tcbinfo);

	/*
	 * Remember the IP options, if any.
	 */
#ifdef INET6
	if (!inc->inc_isipv6)
#endif
		ipopts = ip_srcroute(m);

	/*
	 * See if we already have an entry for this connection.
	 * If we do, resend the SYN,ACK, and reset the retransmit timer.
	 *
	 * XXX: should the syncache be re-initialized with the contents
	 * of the new SYN here (which may have different options?)
	 */
	sc = syncache_lookup(inc, &sch);	/* returns locked entry */
	SCH_LOCK_ASSERT(sch);
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
		if (syncache_respond(sc, m) == 0) {
			SYNCACHE_TIMEOUT(sc, sch, 1);
			tcpstat.tcps_sndacks++;
			tcpstat.tcps_sndtotal++;
		}
		SCH_UNLOCK(sch);
		goto done;
	}

	sc = uma_zalloc(tcp_syncache.zone, M_NOWAIT | M_ZERO);
	if (sc == NULL) {
		/*
		 * The zone allocator couldn't provide more entries.
		 * Treat this as if the cache was full; drop the oldest
		 * entry and insert the new one.
		 */
		tcpstat.tcps_sc_zonefail++;
		sc = TAILQ_LAST(&sch->sch_bucket, sch_head);
		syncache_drop(sc, sch);
		SCH_UNLOCK(sch);
		sc = uma_zalloc(tcp_syncache.zone, M_NOWAIT | M_ZERO);
		if (sc == NULL) {
			if (ipopts)
				(void) m_free(ipopts);
			goto done;
		}
	} else
		SCH_UNLOCK(sch);

	/*
	 * Fill in the syncache values.
	 */
	sc->sc_ipopts = ipopts;
	sc->sc_inc.inc_fport = inc->inc_fport;
	sc->sc_inc.inc_lport = inc->inc_lport;
#ifdef INET6
	sc->sc_inc.inc_isipv6 = inc->inc_isipv6;
	if (inc->inc_isipv6) {
		sc->sc_inc.inc6_faddr = inc->inc6_faddr;
		sc->sc_inc.inc6_laddr = inc->inc6_laddr;
	} else
#endif
	{
		sc->sc_inc.inc_faddr = inc->inc_faddr;
		sc->sc_inc.inc_laddr = inc->inc_laddr;
		sc->sc_ip_tos = ip_tos;
		sc->sc_ip_ttl = ip_ttl;
	}
	sc->sc_irs = th->th_seq;
	sc->sc_flags = 0;
	sc->sc_peer_mss = to->to_flags & TOF_MSS ? to->to_mss : 0;
	sc->sc_flowlabel = 0;
	if (tcp_syncookies) {
		sc->sc_iss = syncookie_generate(sc, &flowtmp);
#ifdef INET6
		if (autoflowlabel)
			sc->sc_flowlabel = flowtmp & IPV6_FLOWLABEL_MASK;
#endif
	} else {
		sc->sc_iss = arc4random();
#ifdef INET6
		if (autoflowlabel)
			sc->sc_flowlabel =
			    (htonl(ip6_randomflowlabel()) & IPV6_FLOWLABEL_MASK);
#endif
	}

	/*
	 * Initial receive window: clip sbspace to [0 .. TCP_MAXWIN].
	 * win was derived from socket earlier in the function.
	 */
	win = imax(win, 0);
	win = imin(win, TCP_MAXWIN);
	sc->sc_wnd = win;

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
			    (TCP_MAXWIN << wscale) < sb_hiwat)
				wscale++;
			sc->sc_request_r_scale = wscale;
			sc->sc_requested_s_scale = to->to_requested_s_scale;
			sc->sc_flags |= SCF_WINSCALE;
		}
	}
#ifdef TCP_SIGNATURE
	/*
	 * If listening socket requested TCP digests, and received SYN
	 * contains the option, flag this in the syncache so that
	 * syncache_respond() will do the right thing with the SYN+ACK.
	 * XXX: Currently we always record the option by default and will
	 * attempt to use it in syncache_respond().
	 */
	if (to->to_flags & TOF_SIGNATURE)
		sc->sc_flags |= SCF_SIGNATURE;
#endif

	if (to->to_flags & TOF_SACK)
		sc->sc_flags |= SCF_SACK;

	/*
	 * Do a standard 3-way handshake.
	 */
	if (syncache_respond(sc, m) == 0) {
		syncache_insert(sc, sch);	/* locks and unlocks sch */
		tcpstat.tcps_sndacks++;
		tcpstat.tcps_sndtotal++;
	} else {
		syncache_free(sc);
		tcpstat.tcps_sc_dropped++;
	}

done:
	*lsop = NULL;
	return (1);
}

static int
syncache_respond(struct syncache *sc, struct mbuf *m)
{
	u_int8_t *optp;
	int optlen, error;
	u_int16_t tlen, hlen, mssopt;
	struct ip *ip = NULL;
	struct tcphdr *th;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif
#ifdef MAC
	struct inpcb *inp = NULL;
#endif

	hlen =
#ifdef INET6
	       (sc->sc_inc.inc_isipv6) ? sizeof(struct ip6_hdr) :
#endif
		sizeof(struct ip);

	KASSERT((&sc->sc_inc) != NULL, ("syncache_respond with NULL in_conninfo pointer"));

	/* Determine MSS we advertize to other end of connection. */
	mssopt = tcp_mssopt(&sc->sc_inc);

	/* Compute the size of the TCP options. */
	if (sc->sc_flags & SCF_NOOPT) {
		optlen = 0;
	} else {
		optlen = TCPOLEN_MAXSEG +
		    ((sc->sc_flags & SCF_WINSCALE) ? 4 : 0) +
		    ((sc->sc_flags & SCF_TIMESTAMP) ? TCPOLEN_TSTAMP_APPA : 0);
#ifdef TCP_SIGNATURE
		if (sc->sc_flags & SCF_SIGNATURE)
			optlen += TCPOLEN_SIGNATURE;
#endif
		if (sc->sc_flags & SCF_SACK)
			optlen += TCPOLEN_SACK_PERMITTED;
		optlen = roundup2(optlen, 4);
	}
	tlen = hlen + sizeof(struct tcphdr) + optlen;

	/*
	 * XXX: Assume that the entire packet will fit in a header mbuf.
	 */
	KASSERT(max_linkhdr + tlen <= MHLEN, ("syncache: mbuf too small"));

	/* Create the IP+TCP header from scratch. */
	if (m)
		m_freem(m);

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	m->m_data += max_linkhdr;
	m->m_len = tlen;
	m->m_pkthdr.len = tlen;
	m->m_pkthdr.rcvif = NULL;

#ifdef MAC
	/*
	 * For MAC look up the inpcb to get access to the label information.
	 * We don't store the inpcb pointer in struct syncache to make locking
	 * less complicated and to save locking operations.  However for MAC
	 * this gives a slight overhead as we have to do a full pcblookup here.
	 */
	INP_INFO_RLOCK(&tcbinfo);
	if (inp == NULL) {
#ifdef INET6 /* && MAC */
		if (sc->sc_inc.inc_isipv6)
			inp = in6_pcblookup_hash(&tcbinfo,
				&sc->sc_inc.inc6_laddr, sc->sc_inc.inc_lport,
				&sc->sc_inc.inc6_faddr, sc->sc_inc.inc_fport,
				1, NULL);
		else
#endif /* INET6 */
			inp = in_pcblookup_hash(&tcbinfo,
				sc->sc_inc.inc_laddr, sc->sc_inc.inc_lport,
				sc->sc_inc.inc_faddr, sc->sc_inc.inc_fport,
				1, NULL);
		if (inp == NULL) {
			m_freem(m);
			INP_INFO_RUNLOCK(&tcbinfo);
			return (ESHUTDOWN);
		}
	}
	INP_LOCK(inp);
	if (!inp->inp_socket->so_options & SO_ACCEPTCONN) {
		m_freem(m);
		INP_UNLOCK(inp);
		INP_INFO_RUNLOCK(&tcbinfo);
		return (ESHUTDOWN);
	}
	mac_create_mbuf_from_inpcb(inp, m);
	INP_UNLOCK(inp);
	INP_INFO_RUNLOCK(&tcbinfo);
#endif /* MAC */

#ifdef INET6
	if (sc->sc_inc.inc_isipv6) {
		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_vfc = IPV6_VERSION;
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_src = sc->sc_inc.inc6_laddr;
		ip6->ip6_dst = sc->sc_inc.inc6_faddr;
		ip6->ip6_plen = htons(tlen - hlen);
		/* ip6_hlim is set after checksum */
		ip6->ip6_flow &= ~IPV6_FLOWLABEL_MASK;
		ip6->ip6_flow |= sc->sc_flowlabel;

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
		ip->ip_ttl = sc->sc_ip_ttl;
		ip->ip_tos = sc->sc_ip_tos;

		/*
		 * See if we should do MTU discovery.  Route lookups are
		 * expensive, so we will only unset the DF bit if:
		 *
		 *	1) path_mtu_discovery is disabled
		 *	2) the SCF_UNREACH flag has been set
		 */
		if (path_mtu_discovery && ((sc->sc_flags & SCF_UNREACH) == 0))
		       ip->ip_off |= IP_DF;

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
	if (optlen != 0) {
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

			/* Form timestamp option per appendix A of RFC 1323. */
			*lp++ = htonl(TCPOPT_TSTAMP_HDR);
			*lp++ = htonl(ticks);
			*lp   = htonl(sc->sc_tsrecent);
			optp += TCPOLEN_TSTAMP_APPA;
		}

#ifdef TCP_SIGNATURE
		/*
		 * Handle TCP-MD5 passive opener response.
		 */
		if (sc->sc_flags & SCF_SIGNATURE) {
			u_int8_t *bp = optp;
			int i;

			*bp++ = TCPOPT_SIGNATURE;
			*bp++ = TCPOLEN_SIGNATURE;
			for (i = 0; i < TCP_SIGLEN; i++)
				*bp++ = 0;
			tcp_signature_compute(m, sizeof(struct ip), 0, optlen,
			    optp + 2, IPSEC_DIR_OUTBOUND);
			optp += TCPOLEN_SIGNATURE;
		}
#endif /* TCP_SIGNATURE */

		if (sc->sc_flags & SCF_SACK) {
			*optp++ = TCPOPT_SACK_PERMITTED;
			*optp++ = TCPOLEN_SACK_PERMITTED;
		}

		{
			/* Pad TCP options to a 4 byte boundary */
			int padlen = optlen - (optp - (u_int8_t *)(th + 1));
			while (padlen-- > 0)
				*optp++ = TCPOPT_EOL;
		}
	}

#ifdef INET6
	if (sc->sc_inc.inc_isipv6) {
		th->th_sum = 0;
		th->th_sum = in6_cksum(m, IPPROTO_TCP, hlen, tlen - hlen);
		ip6->ip6_hlim = in6_selecthlim(NULL, NULL);
		error = ip6_output(m, NULL, NULL, 0, NULL, NULL, NULL);
	} else
#endif
	{
		th->th_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
		    htons(tlen - hlen + IPPROTO_TCP));
		m->m_pkthdr.csum_flags = CSUM_TCP;
		m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
		error = ip_output(m, sc->sc_ipopts, NULL, 0, NULL, NULL);
	}
	return (error);
}

/*
 * cookie layers:
 *
 *	|. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .|
 *	| peer iss                                                      |
 *	| MD5(laddr,faddr,secret,lport,fport)             |. . . . . . .|
 *	|                     0                       |(A)|             |
 * (A): peer mss index
 */

/*
 * The values below are chosen to minimize the size of the tcp_secret
 * table, as well as providing roughly a 16 second lifetime for the cookie.
 */

#define SYNCOOKIE_WNDBITS	5	/* exposed bits for window indexing */
#define SYNCOOKIE_TIMESHIFT	1	/* scale ticks to window time units */

#define SYNCOOKIE_WNDMASK	((1 << SYNCOOKIE_WNDBITS) - 1)
#define SYNCOOKIE_NSECRETS	(1 << SYNCOOKIE_WNDBITS)
#define SYNCOOKIE_TIMEOUT \
    (hz * (1 << SYNCOOKIE_WNDBITS) / (1 << SYNCOOKIE_TIMESHIFT))
#define SYNCOOKIE_DATAMASK	((3 << SYNCOOKIE_WNDBITS) | SYNCOOKIE_WNDMASK)

#define SYNCOOKIE_RLOCK(ts)	(rw_rlock(&(ts).ts_rwmtx))
#define SYNCOOKIE_RUNLOCK(ts)	(rw_runlock(&(ts).ts_rwmtx))
#define SYNCOOKIE_TRY_UPGRADE(ts)  (rw_try_upgrade(&(ts).ts_rwmtx))
#define SYNCOOKIE_DOWNGRADE(ts)	(rw_downgrade(&(ts).ts_rwmtx))

static struct {
	struct rwlock	ts_rwmtx;
	u_int		ts_expire;	/* ticks */
	u_int32_t	ts_secbits[4];
} tcp_secret[SYNCOOKIE_NSECRETS];

static int tcp_msstab[] = { 0, 536, 1460, 8960 };

static MD5_CTX syn_ctx;

#define MD5Add(v)	MD5Update(&syn_ctx, (u_char *)&v, sizeof(v))

struct md5_add {
	u_int32_t laddr, faddr;
	u_int32_t secbits[4];
	u_int16_t lport, fport;
};

#ifdef CTASSERT
CTASSERT(sizeof(struct md5_add) == 28);
#endif

/*
 * Consider the problem of a recreated (and retransmitted) cookie.  If the
 * original SYN was accepted, the connection is established.  The second
 * SYN is inflight, and if it arrives with an ISN that falls within the
 * receive window, the connection is killed.
 *
 * However, since cookies have other problems, this may not be worth
 * worrying about.
 */

static void
syncookie_init(void) {
	int idx;

	for (idx = 0; idx < SYNCOOKIE_NSECRETS; idx++) {
		rw_init(&(tcp_secret[idx].ts_rwmtx), "tcp_secret");
	}
}

static u_int32_t
syncookie_generate(struct syncache *sc, u_int32_t *flowid)
{
	u_int32_t md5_buffer[4];
	u_int32_t data;
	int idx, i;
	struct md5_add add;

	idx = ((ticks << SYNCOOKIE_TIMESHIFT) / hz) & SYNCOOKIE_WNDMASK;
	SYNCOOKIE_RLOCK(tcp_secret[idx]);
	if (tcp_secret[idx].ts_expire < time_uptime &&
	    SYNCOOKIE_TRY_UPGRADE(tcp_secret[idx]) ) {
		/* need write access */
		for (i = 0; i < 4; i++)
			tcp_secret[idx].ts_secbits[i] = arc4random();
		tcp_secret[idx].ts_expire = ticks + SYNCOOKIE_TIMEOUT;
		SYNCOOKIE_DOWNGRADE(tcp_secret[idx]);
	}
	for (data = sizeof(tcp_msstab) / sizeof(int) - 1; data > 0; data--)
		if (tcp_msstab[data] <= sc->sc_peer_mss)
			break;
	data = (data << SYNCOOKIE_WNDBITS) | idx;
	data ^= sc->sc_irs;				/* peer's iss */
	MD5Init(&syn_ctx);
#ifdef INET6
	if (sc->sc_inc.inc_isipv6) {
		MD5Add(sc->sc_inc.inc6_laddr);
		MD5Add(sc->sc_inc.inc6_faddr);
		add.laddr = 0;
		add.faddr = 0;
	} else
#endif
	{
		add.laddr = sc->sc_inc.inc_laddr.s_addr;
		add.faddr = sc->sc_inc.inc_faddr.s_addr;
	}
	add.lport = sc->sc_inc.inc_lport;
	add.fport = sc->sc_inc.inc_fport;
	add.secbits[0] = tcp_secret[idx].ts_secbits[0];
	add.secbits[1] = tcp_secret[idx].ts_secbits[1];
	add.secbits[2] = tcp_secret[idx].ts_secbits[2];
	add.secbits[3] = tcp_secret[idx].ts_secbits[3];
	SYNCOOKIE_RUNLOCK(tcp_secret[idx]);
	MD5Add(add);
	MD5Final((u_char *)&md5_buffer, &syn_ctx);
	data ^= (md5_buffer[0] & ~SYNCOOKIE_WNDMASK);
	*flowid = md5_buffer[1];
	return (data);
}

static struct syncache *
syncookie_lookup(struct in_conninfo *inc, struct tcphdr *th, struct socket *so)
{
	u_int32_t md5_buffer[4];
	struct syncache *sc;
	u_int32_t data;
	int wnd, idx;
	struct md5_add add;

	data = (th->th_ack - 1) ^ (th->th_seq - 1);	/* remove ISS */
	idx = data & SYNCOOKIE_WNDMASK;
	SYNCOOKIE_RLOCK(tcp_secret[idx]);
	if (tcp_secret[idx].ts_expire < ticks ||
	    sototcpcb(so)->ts_recent + SYNCOOKIE_TIMEOUT < ticks) {
		SYNCOOKIE_RUNLOCK(tcp_secret[idx]);
		return (NULL);
	}
	MD5Init(&syn_ctx);
#ifdef INET6
	if (inc->inc_isipv6) {
		MD5Add(inc->inc6_laddr);
		MD5Add(inc->inc6_faddr);
		add.laddr = 0;
		add.faddr = 0;
	} else
#endif
	{
		add.laddr = inc->inc_laddr.s_addr;
		add.faddr = inc->inc_faddr.s_addr;
	}
	add.lport = inc->inc_lport;
	add.fport = inc->inc_fport;
	add.secbits[0] = tcp_secret[idx].ts_secbits[0];
	add.secbits[1] = tcp_secret[idx].ts_secbits[1];
	add.secbits[2] = tcp_secret[idx].ts_secbits[2];
	add.secbits[3] = tcp_secret[idx].ts_secbits[3];
	SYNCOOKIE_RUNLOCK(tcp_secret[idx]);
	MD5Add(add);
	MD5Final((u_char *)&md5_buffer, &syn_ctx);
	data ^= md5_buffer[0];
	if ((data & ~SYNCOOKIE_DATAMASK) != 0)
		return (NULL);
	data = data >> SYNCOOKIE_WNDBITS;

	sc = uma_zalloc(tcp_syncache.zone, M_NOWAIT | M_ZERO);
	if (sc == NULL)
		return (NULL);
	/*
	 * Fill in the syncache values.
	 * XXX: duplicate code from syncache_add
	 */
	sc->sc_ipopts = NULL;
	sc->sc_inc.inc_fport = inc->inc_fport;
	sc->sc_inc.inc_lport = inc->inc_lport;
#ifdef INET6
	sc->sc_inc.inc_isipv6 = inc->inc_isipv6;
	if (inc->inc_isipv6) {
		sc->sc_inc.inc6_faddr = inc->inc6_faddr;
		sc->sc_inc.inc6_laddr = inc->inc6_laddr;
		if (sotoinpcb(so)->in6p_flags & IN6P_AUTOFLOWLABEL)
			sc->sc_flowlabel = md5_buffer[1] & IPV6_FLOWLABEL_MASK;
	} else
#endif
	{
		sc->sc_inc.inc_faddr = inc->inc_faddr;
		sc->sc_inc.inc_laddr = inc->inc_laddr;
		sc->sc_ip_ttl = sotoinpcb(so)->inp_ip_ttl;
		sc->sc_ip_tos = sotoinpcb(so)->inp_ip_tos;
	}
	sc->sc_irs = th->th_seq - 1;
	sc->sc_iss = th->th_ack - 1;
	wnd = sbspace(&so->so_rcv);
	wnd = imax(wnd, 0);
	wnd = imin(wnd, TCP_MAXWIN);
	sc->sc_wnd = wnd;
	sc->sc_flags = 0;
	sc->sc_rxmits = 0;
	sc->sc_peer_mss = tcp_msstab[data];
	return (sc);
}
