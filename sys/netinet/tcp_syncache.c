/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 McAfee, Inc.
 * Copyright (c) 2006,2013 Andre Oppermann, Internet Business Solutions AG
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Jonathan Lemon
 * and McAfee Research, the Security Research Division of McAfee, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program. [2001 McAfee, Inc.]
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
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/hash.h>
#include <sys/refcount.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>		/* for proc0 declaration */
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/ucred.h>

#include <sys/md5.h>
#include <crypto/siphash/siphash.h>

#include <vm/uma.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
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
#include <netinet/tcp_fastopen.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_syncache.h>
#include <netinet/tcp_ecn.h>
#ifdef TCP_BLACKBOX
#include <netinet/tcp_log_buf.h>
#endif
#ifdef TCP_OFFLOAD
#include <netinet/toecore.h>
#endif
#include <netinet/udp.h>

#include <netipsec/ipsec_support.h>

#include <machine/in_cksum.h>

#include <security/mac/mac_framework.h>

VNET_DEFINE_STATIC(int, tcp_syncookies) = 1;
#define	V_tcp_syncookies		VNET(tcp_syncookies)
SYSCTL_INT(_net_inet_tcp, OID_AUTO, syncookies, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_syncookies), 0,
    "Use TCP SYN cookies if the syncache overflows");

VNET_DEFINE_STATIC(int, tcp_syncookiesonly) = 0;
#define	V_tcp_syncookiesonly		VNET(tcp_syncookiesonly)
SYSCTL_INT(_net_inet_tcp, OID_AUTO, syncookies_only, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_syncookiesonly), 0,
    "Use only TCP SYN cookies");

#ifdef TCP_OFFLOAD
#define ADDED_BY_TOE(sc) ((sc)->sc_tod != NULL)
#endif

static void	 syncache_drop(struct syncache *, struct syncache_head *);
static void	 syncache_free(struct syncache *);
static void	 syncache_insert(struct syncache *, struct syncache_head *);
static int	 syncache_respond(struct syncache *, const struct mbuf *, int);
static struct	 socket *syncache_socket(struct syncache *, struct socket *,
		    struct mbuf *m);
static void	 syncache_timeout(struct syncache *sc, struct syncache_head *sch,
		    int docallout);
static void	 syncache_timer(void *);

static uint32_t	 syncookie_mac(struct in_conninfo *, tcp_seq, uint8_t,
		    uint8_t *, uintptr_t);
static tcp_seq	 syncookie_generate(struct syncache_head *, struct syncache *);
static bool	syncookie_expand(struct in_conninfo *,
		    const struct syncache_head *, struct syncache *,
		    struct tcphdr *, struct tcpopt *, struct socket *,
		    uint16_t);
static void	syncache_pause(struct in_conninfo *);
static void	syncache_unpause(void *);
static void	 syncookie_reseed(void *);
#ifdef INVARIANTS
static void	syncookie_cmp(struct in_conninfo *,
		    const struct syncache_head *, struct syncache *,
		    struct tcphdr *, struct tcpopt *, struct socket *,
		    uint16_t);
#endif

/*
 * Transmit the SYN,ACK fewer times than TCP_MAXRXTSHIFT specifies.
 * 3 retransmits corresponds to a timeout with default values of
 * tcp_rexmit_initial * (             1 +
 *                       tcp_backoff[1] +
 *                       tcp_backoff[2] +
 *                       tcp_backoff[3]) + 3 * tcp_rexmit_slop,
 * 1000 ms * (1 + 2 + 4 + 8) +  3 * 200 ms = 15600 ms,
 * the odds are that the user has given up attempting to connect by then.
 */
#define SYNCACHE_MAXREXMTS		3

/* Arbitrary values */
#define TCP_SYNCACHE_HASHSIZE		512
#define TCP_SYNCACHE_BUCKETLIMIT	30

VNET_DEFINE_STATIC(struct tcp_syncache, tcp_syncache);
#define	V_tcp_syncache			VNET(tcp_syncache)

static SYSCTL_NODE(_net_inet_tcp, OID_AUTO, syncache,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "TCP SYN cache");

SYSCTL_UINT(_net_inet_tcp_syncache, OID_AUTO, bucketlimit, CTLFLAG_VNET | CTLFLAG_RDTUN,
    &VNET_NAME(tcp_syncache.bucket_limit), 0,
    "Per-bucket hash limit for syncache");

SYSCTL_UINT(_net_inet_tcp_syncache, OID_AUTO, cachelimit, CTLFLAG_VNET | CTLFLAG_RDTUN,
    &VNET_NAME(tcp_syncache.cache_limit), 0,
    "Overall entry limit for syncache");

SYSCTL_UMA_CUR(_net_inet_tcp_syncache, OID_AUTO, count, CTLFLAG_VNET,
    &VNET_NAME(tcp_syncache.zone), "Current number of entries in syncache");

SYSCTL_UINT(_net_inet_tcp_syncache, OID_AUTO, hashsize, CTLFLAG_VNET | CTLFLAG_RDTUN,
    &VNET_NAME(tcp_syncache.hashsize), 0,
    "Size of TCP syncache hashtable");

SYSCTL_BOOL(_net_inet_tcp_syncache, OID_AUTO, see_other, CTLFLAG_VNET |
    CTLFLAG_RW, &VNET_NAME(tcp_syncache.see_other), 0,
    "All syncache(4) entries are visible, ignoring UID/GID, jail(2) "
    "and mac(4) checks");

static int
sysctl_net_inet_tcp_syncache_rexmtlimit_check(SYSCTL_HANDLER_ARGS)
{
	int error;
	u_int new;

	new = V_tcp_syncache.rexmt_limit;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if ((error == 0) && (req->newptr != NULL)) {
		if (new > TCP_MAXRXTSHIFT)
			error = EINVAL;
		else
			V_tcp_syncache.rexmt_limit = new;
	}
	return (error);
}

SYSCTL_PROC(_net_inet_tcp_syncache, OID_AUTO, rexmtlimit,
    CTLFLAG_VNET | CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(tcp_syncache.rexmt_limit), 0,
    sysctl_net_inet_tcp_syncache_rexmtlimit_check, "IU",
    "Limit on SYN/ACK retransmissions");

VNET_DEFINE(int, tcp_sc_rst_sock_fail) = 1;
SYSCTL_INT(_net_inet_tcp_syncache, OID_AUTO, rst_on_sock_fail,
    CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(tcp_sc_rst_sock_fail), 0,
    "Send reset on socket allocation failure");

static MALLOC_DEFINE(M_SYNCACHE, "syncache", "TCP syncache");

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
		(void)m_free(sc->sc_ipopts);
	if (sc->sc_cred)
		crfree(sc->sc_cred);
#ifdef MAC
	mac_syncache_destroy(&sc->sc_label);
#endif

	uma_zfree(V_tcp_syncache.zone, sc);
}

void
syncache_init(void)
{
	int i;

	V_tcp_syncache.hashsize = TCP_SYNCACHE_HASHSIZE;
	V_tcp_syncache.bucket_limit = TCP_SYNCACHE_BUCKETLIMIT;
	V_tcp_syncache.rexmt_limit = SYNCACHE_MAXREXMTS;
	V_tcp_syncache.hash_secret = arc4random();

	TUNABLE_INT_FETCH("net.inet.tcp.syncache.hashsize",
	    &V_tcp_syncache.hashsize);
	TUNABLE_INT_FETCH("net.inet.tcp.syncache.bucketlimit",
	    &V_tcp_syncache.bucket_limit);
	if (!powerof2(V_tcp_syncache.hashsize) ||
	    V_tcp_syncache.hashsize == 0) {
		printf("WARNING: syncache hash size is not a power of 2.\n");
		V_tcp_syncache.hashsize = TCP_SYNCACHE_HASHSIZE;
	}
	V_tcp_syncache.hashmask = V_tcp_syncache.hashsize - 1;

	/* Set limits. */
	V_tcp_syncache.cache_limit =
	    V_tcp_syncache.hashsize * V_tcp_syncache.bucket_limit;
	TUNABLE_INT_FETCH("net.inet.tcp.syncache.cachelimit",
	    &V_tcp_syncache.cache_limit);

	/* Allocate the hash table. */
	V_tcp_syncache.hashbase = malloc(V_tcp_syncache.hashsize *
	    sizeof(struct syncache_head), M_SYNCACHE, M_WAITOK | M_ZERO);

#ifdef VIMAGE
	V_tcp_syncache.vnet = curvnet;
#endif

	/* Initialize the hash buckets. */
	for (i = 0; i < V_tcp_syncache.hashsize; i++) {
		TAILQ_INIT(&V_tcp_syncache.hashbase[i].sch_bucket);
		mtx_init(&V_tcp_syncache.hashbase[i].sch_mtx, "tcp_sc_head",
			 NULL, MTX_DEF);
		callout_init_mtx(&V_tcp_syncache.hashbase[i].sch_timer,
			 &V_tcp_syncache.hashbase[i].sch_mtx, 0);
		V_tcp_syncache.hashbase[i].sch_length = 0;
		V_tcp_syncache.hashbase[i].sch_sc = &V_tcp_syncache;
		V_tcp_syncache.hashbase[i].sch_last_overflow =
		    -(SYNCOOKIE_LIFETIME + 1);
	}

	/* Create the syncache entry zone. */
	V_tcp_syncache.zone = uma_zcreate("syncache", sizeof(struct syncache),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	V_tcp_syncache.cache_limit = uma_zone_set_max(V_tcp_syncache.zone,
	    V_tcp_syncache.cache_limit);

	/* Start the SYN cookie reseeder callout. */
	callout_init(&V_tcp_syncache.secret.reseed, 1);
	arc4rand(V_tcp_syncache.secret.key[0], SYNCOOKIE_SECRET_SIZE, 0);
	arc4rand(V_tcp_syncache.secret.key[1], SYNCOOKIE_SECRET_SIZE, 0);
	callout_reset(&V_tcp_syncache.secret.reseed, SYNCOOKIE_LIFETIME * hz,
	    syncookie_reseed, &V_tcp_syncache);

	/* Initialize the pause machinery. */
	mtx_init(&V_tcp_syncache.pause_mtx, "tcp_sc_pause", NULL, MTX_DEF);
	callout_init_mtx(&V_tcp_syncache.pause_co, &V_tcp_syncache.pause_mtx,
	    0);
	V_tcp_syncache.pause_until = time_uptime - TCP_SYNCACHE_PAUSE_TIME;
	V_tcp_syncache.pause_backoff = 0;
	V_tcp_syncache.paused = false;
}

#ifdef VIMAGE
void
syncache_destroy(void)
{
	struct syncache_head *sch;
	struct syncache *sc, *nsc;
	int i;

	/*
	 * Stop the re-seed timer before freeing resources.  No need to
	 * possibly schedule it another time.
	 */
	callout_drain(&V_tcp_syncache.secret.reseed);

	/* Stop the SYN cache pause callout. */
	mtx_lock(&V_tcp_syncache.pause_mtx);
	if (callout_stop(&V_tcp_syncache.pause_co) == 0) {
		mtx_unlock(&V_tcp_syncache.pause_mtx);
		callout_drain(&V_tcp_syncache.pause_co);
	} else
		mtx_unlock(&V_tcp_syncache.pause_mtx);

	/* Cleanup hash buckets: stop timers, free entries, destroy locks. */
	for (i = 0; i < V_tcp_syncache.hashsize; i++) {
		sch = &V_tcp_syncache.hashbase[i];
		callout_drain(&sch->sch_timer);

		SCH_LOCK(sch);
		TAILQ_FOREACH_SAFE(sc, &sch->sch_bucket, sc_hash, nsc)
			syncache_drop(sc, sch);
		SCH_UNLOCK(sch);
		KASSERT(TAILQ_EMPTY(&sch->sch_bucket),
		    ("%s: sch->sch_bucket not empty", __func__));
		KASSERT(sch->sch_length == 0, ("%s: sch->sch_length %d not 0",
		    __func__, sch->sch_length));
		mtx_destroy(&sch->sch_mtx);
	}

	KASSERT(uma_zone_get_cur(V_tcp_syncache.zone) == 0,
	    ("%s: cache_count not 0", __func__));

	/* Free the allocated global resources. */
	uma_zdestroy(V_tcp_syncache.zone);
	free(V_tcp_syncache.hashbase, M_SYNCACHE);
	mtx_destroy(&V_tcp_syncache.pause_mtx);
}
#endif

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
	if (sch->sch_length >= V_tcp_syncache.bucket_limit) {
		KASSERT(!TAILQ_EMPTY(&sch->sch_bucket),
			("sch->sch_length incorrect"));
		syncache_pause(&sc->sc_inc);
		sc2 = TAILQ_LAST(&sch->sch_bucket, sch_head);
		sch->sch_last_overflow = time_uptime;
		syncache_drop(sc2, sch);
	}

	/* Put it into the bucket. */
	TAILQ_INSERT_HEAD(&sch->sch_bucket, sc, sc_hash);
	sch->sch_length++;

#ifdef TCP_OFFLOAD
	if (ADDED_BY_TOE(sc)) {
		struct toedev *tod = sc->sc_tod;

		tod->tod_syncache_added(tod, sc->sc_todctx);
	}
#endif

	/* Reinitialize the bucket row's timer. */
	if (sch->sch_length == 1)
		sch->sch_nextc = ticks + INT_MAX;
	syncache_timeout(sc, sch, 1);

	SCH_UNLOCK(sch);

	TCPSTATES_INC(TCPS_SYN_RECEIVED);
	TCPSTAT_INC(tcps_sc_added);
}

/*
 * Remove and free entry from syncache bucket row.
 * Expects locked syncache head.
 */
static void
syncache_drop(struct syncache *sc, struct syncache_head *sch)
{

	SCH_LOCK_ASSERT(sch);

	TCPSTATES_DEC(TCPS_SYN_RECEIVED);
	TAILQ_REMOVE(&sch->sch_bucket, sc, sc_hash);
	sch->sch_length--;

#ifdef TCP_OFFLOAD
	if (ADDED_BY_TOE(sc)) {
		struct toedev *tod = sc->sc_tod;

		tod->tod_syncache_removed(tod, sc->sc_todctx);
	}
#endif

	syncache_free(sc);
}

/*
 * Engage/reengage time on bucket row.
 */
static void
syncache_timeout(struct syncache *sc, struct syncache_head *sch, int docallout)
{
	int rexmt;

	if (sc->sc_rxmits == 0)
		rexmt = tcp_rexmit_initial;
	else
		TCPT_RANGESET(rexmt,
		    tcp_rexmit_initial * tcp_backoff[sc->sc_rxmits],
		    tcp_rexmit_min, tcp_rexmit_max);
	sc->sc_rxttime = ticks + rexmt;
	sc->sc_rxmits++;
	if (TSTMP_LT(sc->sc_rxttime, sch->sch_nextc)) {
		sch->sch_nextc = sc->sc_rxttime;
		if (docallout)
			callout_reset(&sch->sch_timer, sch->sch_nextc - ticks,
			    syncache_timer, (void *)sch);
	}
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
	struct epoch_tracker et;
	int tick = ticks;
	char *s;
	bool paused;

	CURVNET_SET(sch->sch_sc->vnet);

	/* NB: syncache_head has already been locked by the callout. */
	SCH_LOCK_ASSERT(sch);

	/*
	 * In the following cycle we may remove some entries and/or
	 * advance some timeouts, so re-initialize the bucket timer.
	 */
	sch->sch_nextc = tick + INT_MAX;

	/*
	 * If we have paused processing, unconditionally remove
	 * all syncache entries.
	 */
	mtx_lock(&V_tcp_syncache.pause_mtx);
	paused = V_tcp_syncache.paused;
	mtx_unlock(&V_tcp_syncache.pause_mtx);

	TAILQ_FOREACH_SAFE(sc, &sch->sch_bucket, sc_hash, nsc) {
		if (paused) {
			syncache_drop(sc, sch);
			continue;
		}
		/*
		 * We do not check if the listen socket still exists
		 * and accept the case where the listen socket may be
		 * gone by the time we resend the SYN/ACK.  We do
		 * not expect this to happens often. If it does,
		 * then the RST will be sent by the time the remote
		 * host does the SYN/ACK->ACK.
		 */
		if (TSTMP_GT(sc->sc_rxttime, tick)) {
			if (TSTMP_LT(sc->sc_rxttime, sch->sch_nextc))
				sch->sch_nextc = sc->sc_rxttime;
			continue;
		}
		if (sc->sc_rxmits > V_tcp_ecn_maxretries) {
			sc->sc_flags &= ~SCF_ECN_MASK;
		}
		if (sc->sc_rxmits > V_tcp_syncache.rexmt_limit) {
			if ((s = tcp_log_addrs(&sc->sc_inc, NULL, NULL, NULL))) {
				log(LOG_DEBUG, "%s; %s: Retransmits exhausted, "
				    "giving up and removing syncache entry\n",
				    s, __func__);
				free(s, M_TCPLOG);
			}
			syncache_drop(sc, sch);
			TCPSTAT_INC(tcps_sc_stale);
			continue;
		}
		if ((s = tcp_log_addrs(&sc->sc_inc, NULL, NULL, NULL))) {
			log(LOG_DEBUG, "%s; %s: Response timeout, "
			    "retransmitting (%u) SYN|ACK\n",
			    s, __func__, sc->sc_rxmits);
			free(s, M_TCPLOG);
		}

		NET_EPOCH_ENTER(et);
		if (syncache_respond(sc, NULL, TH_SYN|TH_ACK) == 0) {
			syncache_timeout(sc, sch, 0);
			TCPSTAT_INC(tcps_sndacks);
			TCPSTAT_INC(tcps_sndtotal);
			TCPSTAT_INC(tcps_sc_retransmitted);
		} else {
			syncache_drop(sc, sch);
			TCPSTAT_INC(tcps_sc_dropped);
		}
		NET_EPOCH_EXIT(et);
	}
	if (!TAILQ_EMPTY(&(sch)->sch_bucket))
		callout_reset(&(sch)->sch_timer, (sch)->sch_nextc - tick,
			syncache_timer, (void *)(sch));
	CURVNET_RESTORE();
}

/*
 * Returns true if the system is only using cookies at the moment.
 * This could be due to a sysadmin decision to only use cookies, or it
 * could be due to the system detecting an attack.
 */
static inline bool
syncache_cookiesonly(void)
{

	return (V_tcp_syncookies && (V_tcp_syncache.paused ||
	    V_tcp_syncookiesonly));
}

/*
 * Find the hash bucket for the given connection.
 */
static struct syncache_head *
syncache_hashbucket(struct in_conninfo *inc)
{
	uint32_t hash;

	/*
	 * The hash is built on foreign port + local port + foreign address.
	 * We rely on the fact that struct in_conninfo starts with 16 bits
	 * of foreign port, then 16 bits of local port then followed by 128
	 * bits of foreign address.  In case of IPv4 address, the first 3
	 * 32-bit words of the address always are zeroes.
	 */
	hash = jenkins_hash32((uint32_t *)&inc->inc_ie, 5,
	    V_tcp_syncache.hash_secret) & V_tcp_syncache.hashmask;

	return (&V_tcp_syncache.hashbase[hash]);
}

/*
 * Find an entry in the syncache.
 * Returns always with locked syncache_head plus a matching entry or NULL.
 */
static struct syncache *
syncache_lookup(struct in_conninfo *inc, struct syncache_head **schp)
{
	struct syncache *sc;
	struct syncache_head *sch;

	*schp = sch = syncache_hashbucket(inc);
	SCH_LOCK(sch);

	/* Circle through bucket row to find matching entry. */
	TAILQ_FOREACH(sc, &sch->sch_bucket, sc_hash)
		if (bcmp(&inc->inc_ie, &sc->sc_inc.inc_ie,
		    sizeof(struct in_endpoints)) == 0)
			break;

	return (sc);	/* Always returns with locked sch. */
}

/*
 * This function is called when we get a RST for a
 * non-existent connection, so that we can see if the
 * connection is in the syn cache.  If it is, zap it.
 * If required send a challenge ACK.
 */
void
syncache_chkrst(struct in_conninfo *inc, struct tcphdr *th, struct mbuf *m,
    uint16_t port)
{
	struct syncache *sc;
	struct syncache_head *sch;
	char *s = NULL;

	if (syncache_cookiesonly())
		return;
	sc = syncache_lookup(inc, &sch);	/* returns locked sch */
	SCH_LOCK_ASSERT(sch);

	/*
	 * No corresponding connection was found in syncache.
	 * If syncookies are enabled and possibly exclusively
	 * used, or we are under memory pressure, a valid RST
	 * may not find a syncache entry.  In that case we're
	 * done and no SYN|ACK retransmissions will happen.
	 * Otherwise the RST was misdirected or spoofed.
	 */
	if (sc == NULL) {
		if ((s = tcp_log_addrs(inc, th, NULL, NULL)))
			log(LOG_DEBUG, "%s; %s: Spurious RST without matching "
			    "syncache entry (possibly syncookie only), "
			    "segment ignored\n", s, __func__);
		TCPSTAT_INC(tcps_badrst);
		goto done;
	}

	/* The remote UDP encaps port does not match. */
	if (sc->sc_port != port) {
		if ((s = tcp_log_addrs(inc, th, NULL, NULL)))
			log(LOG_DEBUG, "%s; %s: Spurious RST with matching "
			    "syncache entry but non-matching UDP encaps port, "
			    "segment ignored\n", s, __func__);
		TCPSTAT_INC(tcps_badrst);
		goto done;
	}

	/*
	 * If the RST bit is set, check the sequence number to see
	 * if this is a valid reset segment.
	 *
	 * RFC 793 page 37:
	 *   In all states except SYN-SENT, all reset (RST) segments
	 *   are validated by checking their SEQ-fields.  A reset is
	 *   valid if its sequence number is in the window.
	 *
	 * RFC 793 page 69:
	 *   There are four cases for the acceptability test for an incoming
	 *   segment:
	 *
	 * Segment Receive  Test
	 * Length  Window
	 * ------- -------  -------------------------------------------
	 *    0       0     SEG.SEQ = RCV.NXT
	 *    0      >0     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
	 *   >0       0     not acceptable
	 *   >0      >0     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
	 *               or RCV.NXT =< SEG.SEQ+SEG.LEN-1 < RCV.NXT+RCV.WND
	 *
	 * Note that when receiving a SYN segment in the LISTEN state,
	 * IRS is set to SEG.SEQ and RCV.NXT is set to SEG.SEQ+1, as
	 * described in RFC 793, page 66.
	 */
	if ((SEQ_GEQ(th->th_seq, sc->sc_irs + 1) &&
	    SEQ_LT(th->th_seq, sc->sc_irs + 1 + sc->sc_wnd)) ||
	    (sc->sc_wnd == 0 && th->th_seq == sc->sc_irs + 1)) {
		if (V_tcp_insecure_rst ||
		    th->th_seq == sc->sc_irs + 1) {
			syncache_drop(sc, sch);
			if ((s = tcp_log_addrs(inc, th, NULL, NULL)))
				log(LOG_DEBUG,
				    "%s; %s: Our SYN|ACK was rejected, "
				    "connection attempt aborted by remote "
				    "endpoint\n",
				    s, __func__);
			TCPSTAT_INC(tcps_sc_reset);
		} else {
			TCPSTAT_INC(tcps_badrst);
			/* Send challenge ACK. */
			if ((s = tcp_log_addrs(inc, th, NULL, NULL)))
				log(LOG_DEBUG, "%s; %s: RST with invalid "
				    " SEQ %u != NXT %u (+WND %u), "
				    "sending challenge ACK\n",
				    s, __func__,
				    th->th_seq, sc->sc_irs + 1, sc->sc_wnd);
			if (syncache_respond(sc, m, TH_ACK) == 0) {
				TCPSTAT_INC(tcps_sndacks);
				TCPSTAT_INC(tcps_sndtotal);
			} else {
				syncache_drop(sc, sch);
				TCPSTAT_INC(tcps_sc_dropped);
			}
		}
	} else {
		if ((s = tcp_log_addrs(inc, th, NULL, NULL)))
			log(LOG_DEBUG, "%s; %s: RST with invalid SEQ %u != "
			    "NXT %u (+WND %u), segment ignored\n",
			    s, __func__,
			    th->th_seq, sc->sc_irs + 1, sc->sc_wnd);
		TCPSTAT_INC(tcps_badrst);
	}

done:
	if (s != NULL)
		free(s, M_TCPLOG);
	SCH_UNLOCK(sch);
}

void
syncache_badack(struct in_conninfo *inc, uint16_t port)
{
	struct syncache *sc;
	struct syncache_head *sch;

	if (syncache_cookiesonly())
		return;
	sc = syncache_lookup(inc, &sch);	/* returns locked sch */
	SCH_LOCK_ASSERT(sch);
	if ((sc != NULL) && (sc->sc_port == port)) {
		syncache_drop(sc, sch);
		TCPSTAT_INC(tcps_sc_badack);
	}
	SCH_UNLOCK(sch);
}

void
syncache_unreach(struct in_conninfo *inc, tcp_seq th_seq, uint16_t port)
{
	struct syncache *sc;
	struct syncache_head *sch;

	if (syncache_cookiesonly())
		return;
	sc = syncache_lookup(inc, &sch);	/* returns locked sch */
	SCH_LOCK_ASSERT(sch);
	if (sc == NULL)
		goto done;

	/* If the port != sc_port, then it's a bogus ICMP msg */
	if (port != sc->sc_port)
		goto done;

	/* If the sequence number != sc_iss, then it's a bogus ICMP msg */
	if (ntohl(th_seq) != sc->sc_iss)
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
	TCPSTAT_INC(tcps_sc_unreach);
done:
	SCH_UNLOCK(sch);
}

/*
 * Build a new TCP socket structure from a syncache entry.
 *
 * On success return the newly created socket with its underlying inp locked.
 */
static struct socket *
syncache_socket(struct syncache *sc, struct socket *lso, struct mbuf *m)
{
	struct inpcb *inp = NULL;
	struct socket *so;
	struct tcpcb *tp;
	int error;
	char *s;

	NET_EPOCH_ASSERT();

	/*
	 * Ok, create the full blown connection, and set things up
	 * as they would have been set up if we had created the
	 * connection when the SYN arrived.
	 */
	if ((so = solisten_clone(lso)) == NULL)
		goto allocfail;
#ifdef MAC
	mac_socketpeer_set_from_mbuf(m, so);
#endif
	error = in_pcballoc(so, &V_tcbinfo);
	if (error) {
		sodealloc(so);
		goto allocfail;
	}
	inp = sotoinpcb(so);
	if ((tp = tcp_newtcpcb(inp, sototcpcb(lso))) == NULL) {
		in_pcbfree(inp);
		sodealloc(so);
		goto allocfail;
	}
	inp->inp_inc.inc_flags = sc->sc_inc.inc_flags;
#ifdef INET6
	if (sc->sc_inc.inc_flags & INC_ISIPV6) {
		inp->inp_vflag &= ~INP_IPV4;
		inp->inp_vflag |= INP_IPV6;
		inp->in6p_laddr = sc->sc_inc.inc6_laddr;
	} else {
		inp->inp_vflag &= ~INP_IPV6;
		inp->inp_vflag |= INP_IPV4;
#endif
		inp->inp_ip_ttl = sc->sc_ip_ttl;
		inp->inp_ip_tos = sc->sc_ip_tos;
		inp->inp_laddr = sc->sc_inc.inc_laddr;
#ifdef INET6
	}
#endif

	/*
	 * If there's an mbuf and it has a flowid, then let's initialise the
	 * inp with that particular flowid.
	 */
	if (m != NULL && M_HASHTYPE_GET(m) != M_HASHTYPE_NONE) {
		inp->inp_flowid = m->m_pkthdr.flowid;
		inp->inp_flowtype = M_HASHTYPE_GET(m);
#ifdef NUMA
		inp->inp_numa_domain = m->m_pkthdr.numa_domain;
#endif
	}

	inp->inp_lport = sc->sc_inc.inc_lport;
#ifdef INET6
	if (inp->inp_vflag & INP_IPV6PROTO) {
		struct inpcb *oinp = sotoinpcb(lso);

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
		inp->in6p_hops = oinp->in6p_hops;
	}

	if (sc->sc_inc.inc_flags & INC_ISIPV6) {
		struct sockaddr_in6 sin6;

		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_addr = sc->sc_inc.inc6_faddr;
		sin6.sin6_port = sc->sc_inc.inc_fport;
		sin6.sin6_flowinfo = sin6.sin6_scope_id = 0;
		INP_HASH_WLOCK(&V_tcbinfo);
		error = in6_pcbconnect(inp, &sin6, thread0.td_ucred, false);
		INP_HASH_WUNLOCK(&V_tcbinfo);
		if (error != 0)
			goto abort;
		/* Override flowlabel from in6_pcbconnect. */
		inp->inp_flow &= ~IPV6_FLOWLABEL_MASK;
		inp->inp_flow |= sc->sc_flowlabel;
	}
#endif /* INET6 */
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		struct sockaddr_in sin;

		inp->inp_options = (m) ? ip_srcroute(m) : NULL;

		if (inp->inp_options == NULL) {
			inp->inp_options = sc->sc_ipopts;
			sc->sc_ipopts = NULL;
		}

		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(sin);
		sin.sin_addr = sc->sc_inc.inc_faddr;
		sin.sin_port = sc->sc_inc.inc_fport;
		bzero((caddr_t)sin.sin_zero, sizeof(sin.sin_zero));
		INP_HASH_WLOCK(&V_tcbinfo);
		error = in_pcbconnect(inp, &sin, thread0.td_ucred);
		INP_HASH_WUNLOCK(&V_tcbinfo);
		if (error != 0)
			goto abort;
	}
#endif /* INET */
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	/* Copy old policy into new socket's. */
	if (ipsec_copy_pcbpolicy(sotoinpcb(lso), inp) != 0)
		printf("syncache_socket: could not copy policy\n");
#endif
	tp->t_state = TCPS_SYN_RECEIVED;
	tp->iss = sc->sc_iss;
	tp->irs = sc->sc_irs;
	tp->t_port = sc->sc_port;
	tcp_rcvseqinit(tp);
	tcp_sendseqinit(tp);
	tp->snd_wl1 = sc->sc_irs;
	tp->snd_max = tp->iss + 1;
	tp->snd_nxt = tp->iss + 1;
	tp->rcv_up = sc->sc_irs + 1;
	tp->rcv_wnd = sc->sc_wnd;
	tp->rcv_adv += tp->rcv_wnd;
	tp->last_ack_sent = tp->rcv_nxt;

	tp->t_flags = sototcpcb(lso)->t_flags &
	    (TF_LRD|TF_NOPUSH|TF_NODELAY);
	if (sc->sc_flags & SCF_NOOPT)
		tp->t_flags |= TF_NOOPT;
	else {
		if (sc->sc_flags & SCF_WINSCALE) {
			tp->t_flags |= TF_REQ_SCALE|TF_RCVD_SCALE;
			tp->snd_scale = sc->sc_requested_s_scale;
			tp->request_r_scale = sc->sc_requested_r_scale;
		}
		if (sc->sc_flags & SCF_TIMESTAMP) {
			tp->t_flags |= TF_REQ_TSTMP|TF_RCVD_TSTMP;
			tp->ts_recent = sc->sc_tsreflect;
			tp->ts_recent_age = tcp_ts_getticks();
			tp->ts_offset = sc->sc_tsoff;
		}
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		if (sc->sc_flags & SCF_SIGNATURE)
			tp->t_flags |= TF_SIGNATURE;
#endif
		if (sc->sc_flags & SCF_SACK)
			tp->t_flags |= TF_SACK_PERMIT;
	}

	tcp_ecn_syncache_socket(tp, sc);

	/*
	 * Set up MSS and get cached values from tcp_hostcache.
	 * This might overwrite some of the defaults we just set.
	 */
	tcp_mss(tp, sc->sc_peer_mss);

	/*
	 * If the SYN,ACK was retransmitted, indicate that CWND to be
	 * limited to one segment in cc_conn_init().
	 * NB: sc_rxmits counts all SYN,ACK transmits, not just retransmits.
	 */
	if (sc->sc_rxmits > 1)
		tp->snd_cwnd = 1;

#ifdef TCP_OFFLOAD
	/*
	 * Allow a TOE driver to install its hooks.  Note that we hold the
	 * pcbinfo lock too and that prevents tcp_usr_accept from accepting a
	 * new connection before the TOE driver has done its thing.
	 */
	if (ADDED_BY_TOE(sc)) {
		struct toedev *tod = sc->sc_tod;

		tod->tod_offload_socket(tod, sc->sc_todctx, so);
	}
#endif
#ifdef TCP_BLACKBOX
	/*
	 * Inherit the log state from the listening socket, if
	 * - the log state of the listening socket is not off and
	 * - the listening socket was not auto selected from all sessions and
	 * - a log id is not set on the listening socket.
	 * This avoids inheriting a log state which was automatically set.
	 */
	if ((tcp_get_bblog_state(sototcpcb(lso)) != TCP_LOG_STATE_OFF) &&
	    ((sototcpcb(lso)->t_flags2 & TF2_LOG_AUTO) == 0) &&
	    (sototcpcb(lso)->t_lib == NULL)) {
		tcp_log_state_change(tp, tcp_get_bblog_state(sototcpcb(lso)));
	}
#endif
	/*
	 * Copy and activate timers.
	 */
	tp->t_maxunacktime = sototcpcb(lso)->t_maxunacktime;
	tp->t_keepinit = sototcpcb(lso)->t_keepinit;
	tp->t_keepidle = sototcpcb(lso)->t_keepidle;
	tp->t_keepintvl = sototcpcb(lso)->t_keepintvl;
	tp->t_keepcnt = sototcpcb(lso)->t_keepcnt;
	tcp_timer_activate(tp, TT_KEEP, TP_KEEPINIT(tp));

	TCPSTAT_INC(tcps_accepts);
	TCP_PROBE6(state__change, NULL, tp, NULL, tp, NULL, TCPS_LISTEN);

	if (!solisten_enqueue(so, SS_ISCONNECTED))
		tp->t_flags |= TF_SONOTCONN;
	/* Can we inherit anything from the listener? */
	if (tp->t_fb->tfb_inherit != NULL) {
		(*tp->t_fb->tfb_inherit)(tp, sotoinpcb(lso));
	}
	return (so);

allocfail:
	/*
	 * Drop the connection; we will either send a RST or have the peer
	 * retransmit its SYN again after its RTO and try again.
	 */
	if ((s = tcp_log_addrs(&sc->sc_inc, NULL, NULL, NULL))) {
		log(LOG_DEBUG, "%s; %s: Socket create failed "
		    "due to limits or memory shortage\n",
		    s, __func__);
		free(s, M_TCPLOG);
	}
	TCPSTAT_INC(tcps_listendrop);
	return (NULL);

abort:
	tcp_discardcb(tp);
	in_pcbfree(inp);
	sodealloc(so);
	if ((s = tcp_log_addrs(&sc->sc_inc, NULL, NULL, NULL))) {
		log(LOG_DEBUG, "%s; %s: in%s_pcbconnect failed with error %i\n",
		    s, __func__, (sc->sc_inc.inc_flags & INC_ISIPV6) ? "6" : "",
		    error);
		free(s, M_TCPLOG);
	}
	TCPSTAT_INC(tcps_listendrop);
	return (NULL);
}

/*
 * This function gets called when we receive an ACK for a
 * socket in the LISTEN state.  We look up the connection
 * in the syncache, and if its there, we pull it out of
 * the cache and turn it into a full-blown connection in
 * the SYN-RECEIVED state.
 *
 * On syncache_socket() success the newly created socket
 * has its underlying inp locked.
 */
int
syncache_expand(struct in_conninfo *inc, struct tcpopt *to, struct tcphdr *th,
    struct socket **lsop, struct mbuf *m, uint16_t port)
{
	struct syncache *sc;
	struct syncache_head *sch;
	struct syncache scs;
	char *s;
	bool locked;

	NET_EPOCH_ASSERT();
	KASSERT((tcp_get_flags(th) & (TH_RST|TH_ACK|TH_SYN)) == TH_ACK,
	    ("%s: can handle only ACK", __func__));

	if (syncache_cookiesonly()) {
		sc = NULL;
		sch = syncache_hashbucket(inc);
		locked = false;
	} else {
		sc = syncache_lookup(inc, &sch);	/* returns locked sch */
		locked = true;
		SCH_LOCK_ASSERT(sch);
	}

#ifdef INVARIANTS
	/*
	 * Test code for syncookies comparing the syncache stored
	 * values with the reconstructed values from the cookie.
	 */
	if (sc != NULL)
		syncookie_cmp(inc, sch, sc, th, to, *lsop, port);
#endif

	if (sc == NULL) {
		/*
		 * There is no syncache entry, so see if this ACK is
		 * a returning syncookie.  To do this, first:
		 *  A. Check if syncookies are used in case of syncache
		 *     overflows
		 *  B. See if this socket has had a syncache entry dropped in
		 *     the recent past. We don't want to accept a bogus
		 *     syncookie if we've never received a SYN or accept it
		 *     twice.
		 *  C. check that the syncookie is valid.  If it is, then
		 *     cobble up a fake syncache entry, and return.
		 */
		if (locked && !V_tcp_syncookies) {
			SCH_UNLOCK(sch);
			TCPSTAT_INC(tcps_sc_spurcookie);
			if ((s = tcp_log_addrs(inc, th, NULL, NULL)))
				log(LOG_DEBUG, "%s; %s: Spurious ACK, "
				    "segment rejected (syncookies disabled)\n",
				    s, __func__);
			goto failed;
		}
		if (locked && !V_tcp_syncookiesonly &&
		    sch->sch_last_overflow < time_uptime - SYNCOOKIE_LIFETIME) {
			SCH_UNLOCK(sch);
			TCPSTAT_INC(tcps_sc_spurcookie);
			if ((s = tcp_log_addrs(inc, th, NULL, NULL)))
				log(LOG_DEBUG, "%s; %s: Spurious ACK, "
				    "segment rejected (no syncache entry)\n",
				    s, __func__);
			goto failed;
		}
		if (locked)
			SCH_UNLOCK(sch);
		bzero(&scs, sizeof(scs));
		if (syncookie_expand(inc, sch, &scs, th, to, *lsop, port)) {
			sc = &scs;
			TCPSTAT_INC(tcps_sc_recvcookie);
		} else {
			TCPSTAT_INC(tcps_sc_failcookie);
			if ((s = tcp_log_addrs(inc, th, NULL, NULL)))
				log(LOG_DEBUG, "%s; %s: Segment failed "
				    "SYNCOOKIE authentication, segment rejected "
				    "(probably spoofed)\n", s, __func__);
			goto failed;
		}
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		/* If received ACK has MD5 signature, check it. */
		if ((to->to_flags & TOF_SIGNATURE) != 0 &&
		    (!TCPMD5_ENABLED() ||
		    TCPMD5_INPUT(m, th, to->to_signature) != 0)) {
			/* Drop the ACK. */
			if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
				log(LOG_DEBUG, "%s; %s: Segment rejected, "
				    "MD5 signature doesn't match.\n",
				    s, __func__);
				free(s, M_TCPLOG);
			}
			TCPSTAT_INC(tcps_sig_err_sigopt);
			return (-1); /* Do not send RST */
		}
#endif /* TCP_SIGNATURE */
		TCPSTATES_INC(TCPS_SYN_RECEIVED);
	} else {
		if (sc->sc_port != port) {
			SCH_UNLOCK(sch);
			return (0);
		}
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		/*
		 * If listening socket requested TCP digests, check that
		 * received ACK has signature and it is correct.
		 * If not, drop the ACK and leave sc entry in th cache,
		 * because SYN was received with correct signature.
		 */
		if (sc->sc_flags & SCF_SIGNATURE) {
			if ((to->to_flags & TOF_SIGNATURE) == 0) {
				/* No signature */
				TCPSTAT_INC(tcps_sig_err_nosigopt);
				SCH_UNLOCK(sch);
				if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
					log(LOG_DEBUG, "%s; %s: Segment "
					    "rejected, MD5 signature wasn't "
					    "provided.\n", s, __func__);
					free(s, M_TCPLOG);
				}
				return (-1); /* Do not send RST */
			}
			if (!TCPMD5_ENABLED() ||
			    TCPMD5_INPUT(m, th, to->to_signature) != 0) {
				/* Doesn't match or no SA */
				SCH_UNLOCK(sch);
				if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
					log(LOG_DEBUG, "%s; %s: Segment "
					    "rejected, MD5 signature doesn't "
					    "match.\n", s, __func__);
					free(s, M_TCPLOG);
				}
				return (-1); /* Do not send RST */
			}
		}
#endif /* TCP_SIGNATURE */

		/*
		 * RFC 7323 PAWS: If we have a timestamp on this segment and
		 * it's less than ts_recent, drop it.
		 * XXXMT: RFC 7323 also requires to send an ACK.
		 *        In tcp_input.c this is only done for TCP segments
		 *        with user data, so be consistent here and just drop
		 *        the segment.
		 */
		if (sc->sc_flags & SCF_TIMESTAMP && to->to_flags & TOF_TS &&
		    TSTMP_LT(to->to_tsval, sc->sc_tsreflect)) {
			SCH_UNLOCK(sch);
			if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
				log(LOG_DEBUG,
				    "%s; %s: SEG.TSval %u < TS.Recent %u, "
				    "segment dropped\n", s, __func__,
				    to->to_tsval, sc->sc_tsreflect);
				free(s, M_TCPLOG);
			}
			return (-1);  /* Do not send RST */
		}

		/*
		 * If timestamps were not negotiated during SYN/ACK and a
		 * segment with a timestamp is received, ignore the
		 * timestamp and process the packet normally.
		 * See section 3.2 of RFC 7323.
		 */
		if (!(sc->sc_flags & SCF_TIMESTAMP) &&
		    (to->to_flags & TOF_TS)) {
			if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
				log(LOG_DEBUG, "%s; %s: Timestamp not "
				    "expected, segment processed normally\n",
				    s, __func__);
				free(s, M_TCPLOG);
				s = NULL;
			}
		}

		/*
		 * If timestamps were negotiated during SYN/ACK and a
		 * segment without a timestamp is received, silently drop
		 * the segment, unless the missing timestamps are tolerated.
		 * See section 3.2 of RFC 7323.
		 */
		if ((sc->sc_flags & SCF_TIMESTAMP) &&
		    !(to->to_flags & TOF_TS)) {
			if (V_tcp_tolerate_missing_ts) {
				if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
					log(LOG_DEBUG,
					    "%s; %s: Timestamp missing, "
					    "segment processed normally\n",
					    s, __func__);
					free(s, M_TCPLOG);
				}
			} else {
				SCH_UNLOCK(sch);
				if ((s = tcp_log_addrs(inc, th, NULL, NULL))) {
					log(LOG_DEBUG,
					    "%s; %s: Timestamp missing, "
					    "segment silently dropped\n",
					    s, __func__);
					free(s, M_TCPLOG);
				}
				return (-1);  /* Do not send RST */
			}
		}
		TAILQ_REMOVE(&sch->sch_bucket, sc, sc_hash);
		sch->sch_length--;
#ifdef TCP_OFFLOAD
		if (ADDED_BY_TOE(sc)) {
			struct toedev *tod = sc->sc_tod;

			tod->tod_syncache_removed(tod, sc->sc_todctx);
		}
#endif
		SCH_UNLOCK(sch);
	}

	/*
	 * Segment validation:
	 * ACK must match our initial sequence number + 1 (the SYN|ACK).
	 */
	if (th->th_ack != sc->sc_iss + 1) {
		if ((s = tcp_log_addrs(inc, th, NULL, NULL)))
			log(LOG_DEBUG, "%s; %s: ACK %u != ISS+1 %u, segment "
			    "rejected\n", s, __func__, th->th_ack, sc->sc_iss);
		goto failed;
	}

	/*
	 * The SEQ must fall in the window starting at the received
	 * initial receive sequence number + 1 (the SYN).
	 */
	if (SEQ_LEQ(th->th_seq, sc->sc_irs) ||
	    SEQ_GT(th->th_seq, sc->sc_irs + sc->sc_wnd)) {
		if ((s = tcp_log_addrs(inc, th, NULL, NULL)))
			log(LOG_DEBUG, "%s; %s: SEQ %u != IRS+1 %u, segment "
			    "rejected\n", s, __func__, th->th_seq, sc->sc_irs);
		goto failed;
	}

	*lsop = syncache_socket(sc, *lsop, m);

	if (__predict_false(*lsop == NULL)) {
		TCPSTAT_INC(tcps_sc_aborted);
		TCPSTATES_DEC(TCPS_SYN_RECEIVED);
	} else
		TCPSTAT_INC(tcps_sc_completed);

/* how do we find the inp for the new socket? */
	if (sc != &scs)
		syncache_free(sc);
	return (1);
failed:
	if (sc != NULL) {
		TCPSTATES_DEC(TCPS_SYN_RECEIVED);
		if (sc != &scs)
			syncache_free(sc);
	}
	if (s != NULL)
		free(s, M_TCPLOG);
	*lsop = NULL;
	return (0);
}

static struct socket *
syncache_tfo_expand(struct syncache *sc, struct socket *lso, struct mbuf *m,
    uint64_t response_cookie)
{
	struct inpcb *inp;
	struct tcpcb *tp;
	unsigned int *pending_counter;
	struct socket *so;

	NET_EPOCH_ASSERT();

	pending_counter = intotcpcb(sotoinpcb(lso))->t_tfo_pending;
	so = syncache_socket(sc, lso, m);
	if (so == NULL) {
		TCPSTAT_INC(tcps_sc_aborted);
		atomic_subtract_int(pending_counter, 1);
	} else {
		soisconnected(so);
		inp = sotoinpcb(so);
		tp = intotcpcb(inp);
		tp->t_flags |= TF_FASTOPEN;
		tp->t_tfo_cookie.server = response_cookie;
		tp->snd_max = tp->iss;
		tp->snd_nxt = tp->iss;
		tp->t_tfo_pending = pending_counter;
		TCPSTATES_INC(TCPS_SYN_RECEIVED);
		TCPSTAT_INC(tcps_sc_completed);
	}

	return (so);
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
 *
 * The exception to the above is when a SYN with a valid TCP Fast Open (TFO)
 * cookie is processed and a new socket is created.  In this case, any data
 * accompanying the SYN will be queued to the socket by tcp_input() and will
 * be ACKed either when the application sends response data or the delayed
 * ACK timer expires, whichever comes first.
 */
struct socket *
syncache_add(struct in_conninfo *inc, struct tcpopt *to, struct tcphdr *th,
    struct inpcb *inp, struct socket *so, struct mbuf *m, void *tod,
    void *todctx, uint8_t iptos, uint16_t port)
{
	struct tcpcb *tp;
	struct socket *rv = NULL;
	struct syncache *sc = NULL;
	struct syncache_head *sch;
	struct mbuf *ipopts = NULL;
	u_int ltflags;
	int win, ip_ttl, ip_tos;
	char *s;
#ifdef INET6
	int autoflowlabel = 0;
#endif
#ifdef MAC
	struct label *maclabel = NULL;
#endif
	struct syncache scs;
	uint64_t tfo_response_cookie;
	unsigned int *tfo_pending = NULL;
	int tfo_cookie_valid = 0;
	int tfo_response_cookie_valid = 0;
	bool locked;

	INP_RLOCK_ASSERT(inp);			/* listen socket */
	KASSERT((tcp_get_flags(th) & (TH_RST|TH_ACK|TH_SYN)) == TH_SYN,
	    ("%s: unexpected tcp flags", __func__));

	/*
	 * Combine all so/tp operations very early to drop the INP lock as
	 * soon as possible.
	 */
	KASSERT(SOLISTENING(so), ("%s: %p not listening", __func__, so));
	tp = sototcpcb(so);

#ifdef INET6
	if (inc->inc_flags & INC_ISIPV6) {
		if (inp->inp_flags & IN6P_AUTOFLOWLABEL) {
			autoflowlabel = 1;
		}
		ip_ttl = in6_selecthlim(inp, NULL);
		if ((inp->in6p_outputopts == NULL) ||
		    (inp->in6p_outputopts->ip6po_tclass == -1)) {
			ip_tos = 0;
		} else {
			ip_tos = inp->in6p_outputopts->ip6po_tclass;
		}
	}
#endif
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
	{
		ip_ttl = inp->inp_ip_ttl;
		ip_tos = inp->inp_ip_tos;
	}
#endif
	win = so->sol_sbrcv_hiwat;
	ltflags = (tp->t_flags & (TF_NOOPT | TF_SIGNATURE));

	if (V_tcp_fastopen_server_enable && (tp->t_flags & TF_FASTOPEN) &&
	    (tp->t_tfo_pending != NULL) &&
	    (to->to_flags & TOF_FASTOPEN)) {
		/*
		 * Limit the number of pending TFO connections to
		 * approximately half of the queue limit.  This prevents TFO
		 * SYN floods from starving the service by filling the
		 * listen queue with bogus TFO connections.
		 */
		if (atomic_fetchadd_int(tp->t_tfo_pending, 1) <=
		    (so->sol_qlimit / 2)) {
			int result;

			result = tcp_fastopen_check_cookie(inc,
			    to->to_tfo_cookie, to->to_tfo_len,
			    &tfo_response_cookie);
			tfo_cookie_valid = (result > 0);
			tfo_response_cookie_valid = (result >= 0);
		}

		/*
		 * Remember the TFO pending counter as it will have to be
		 * decremented below if we don't make it to syncache_tfo_expand().
		 */
		tfo_pending = tp->t_tfo_pending;
	}

#ifdef MAC
	if (mac_syncache_init(&maclabel) != 0) {
		INP_RUNLOCK(inp);
		goto done;
	} else
		mac_syncache_create(maclabel, inp);
#endif
	if (!tfo_cookie_valid)
		INP_RUNLOCK(inp);

	/*
	 * Remember the IP options, if any.
	 */
#ifdef INET6
	if (!(inc->inc_flags & INC_ISIPV6))
#endif
#ifdef INET
		ipopts = (m) ? ip_srcroute(m) : NULL;
#else
		ipopts = NULL;
#endif

#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
	/*
	 * When the socket is TCP-MD5 enabled check that,
	 *  - a signed packet is valid
	 *  - a non-signed packet does not have a security association
	 *
	 *  If a signed packet fails validation or a non-signed packet has a
	 *  security association, the packet will be dropped.
	 */
	if (ltflags & TF_SIGNATURE) {
		if (to->to_flags & TOF_SIGNATURE) {
			if (!TCPMD5_ENABLED() ||
			    TCPMD5_INPUT(m, th, to->to_signature) != 0)
				goto done;
		} else {
			if (TCPMD5_ENABLED() &&
			    TCPMD5_INPUT(m, NULL, NULL) != ENOENT)
				goto done;
		}
	} else if (to->to_flags & TOF_SIGNATURE)
		goto done;
#endif	/* TCP_SIGNATURE */
	/*
	 * See if we already have an entry for this connection.
	 * If we do, resend the SYN,ACK, and reset the retransmit timer.
	 *
	 * XXX: should the syncache be re-initialized with the contents
	 * of the new SYN here (which may have different options?)
	 *
	 * XXX: We do not check the sequence number to see if this is a
	 * real retransmit or a new connection attempt.  The question is
	 * how to handle such a case; either ignore it as spoofed, or
	 * drop the current entry and create a new one?
	 */
	if (syncache_cookiesonly()) {
		sc = NULL;
		sch = syncache_hashbucket(inc);
		locked = false;
	} else {
		sc = syncache_lookup(inc, &sch);	/* returns locked sch */
		locked = true;
		SCH_LOCK_ASSERT(sch);
	}
	if (sc != NULL) {
		if (tfo_cookie_valid)
			INP_RUNLOCK(inp);
		TCPSTAT_INC(tcps_sc_dupsyn);
		if (ipopts) {
			/*
			 * If we were remembering a previous source route,
			 * forget it and use the new one we've been given.
			 */
			if (sc->sc_ipopts)
				(void)m_free(sc->sc_ipopts);
			sc->sc_ipopts = ipopts;
		}
		/*
		 * Update timestamp if present.
		 */
		if ((sc->sc_flags & SCF_TIMESTAMP) && (to->to_flags & TOF_TS))
			sc->sc_tsreflect = to->to_tsval;
		else
			sc->sc_flags &= ~SCF_TIMESTAMP;
		/*
		 * Adjust ECN response if needed, e.g. different
		 * IP ECN field, or a fallback by the remote host.
		 */
		if (sc->sc_flags & SCF_ECN_MASK) {
			sc->sc_flags &= ~SCF_ECN_MASK;
			sc->sc_flags |= tcp_ecn_syncache_add(tcp_get_flags(th), iptos);
		}
#ifdef MAC
		/*
		 * Since we have already unconditionally allocated label
		 * storage, free it up.  The syncache entry will already
		 * have an initialized label we can use.
		 */
		mac_syncache_destroy(&maclabel);
#endif
		TCP_PROBE5(receive, NULL, NULL, m, NULL, th);
		/* Retransmit SYN|ACK and reset retransmit count. */
		if ((s = tcp_log_addrs(&sc->sc_inc, th, NULL, NULL))) {
			log(LOG_DEBUG, "%s; %s: Received duplicate SYN, "
			    "resetting timer and retransmitting SYN|ACK\n",
			    s, __func__);
			free(s, M_TCPLOG);
		}
		if (syncache_respond(sc, m, TH_SYN|TH_ACK) == 0) {
			sc->sc_rxmits = 0;
			syncache_timeout(sc, sch, 1);
			TCPSTAT_INC(tcps_sndacks);
			TCPSTAT_INC(tcps_sndtotal);
		} else {
			syncache_drop(sc, sch);
			TCPSTAT_INC(tcps_sc_dropped);
		}
		SCH_UNLOCK(sch);
		goto donenoprobe;
	}

	KASSERT(sc == NULL, ("sc(%p) != NULL", sc));
	/*
	 * Skip allocating a syncache entry if we are just going to discard
	 * it later.
	 */
	if (!locked || tfo_cookie_valid) {
		bzero(&scs, sizeof(scs));
		sc = &scs;
	} else {
		sc = uma_zalloc(V_tcp_syncache.zone, M_NOWAIT | M_ZERO);
		if (sc == NULL) {
			/*
			 * The zone allocator couldn't provide more entries.
			 * Treat this as if the cache was full; drop the oldest
			 * entry and insert the new one.
			 */
			TCPSTAT_INC(tcps_sc_zonefail);
			sc = TAILQ_LAST(&sch->sch_bucket, sch_head);
			if (sc != NULL) {
				sch->sch_last_overflow = time_uptime;
				syncache_drop(sc, sch);
				syncache_pause(inc);
			}
			sc = uma_zalloc(V_tcp_syncache.zone, M_NOWAIT | M_ZERO);
			if (sc == NULL) {
				if (V_tcp_syncookies) {
					bzero(&scs, sizeof(scs));
					sc = &scs;
				} else {
					KASSERT(locked,
					    ("%s: bucket unexpectedly unlocked",
					    __func__));
					SCH_UNLOCK(sch);
					goto done;
				}
			}
		}
	}

	KASSERT(sc != NULL, ("sc == NULL"));
	if (!tfo_cookie_valid && tfo_response_cookie_valid)
		sc->sc_tfo_cookie = &tfo_response_cookie;

	/*
	 * Fill in the syncache values.
	 */
#ifdef MAC
	sc->sc_label = maclabel;
#endif
	/*
	 * sc_cred is only used in syncache_pcblist() to list TCP endpoints in
	 * TCPS_SYN_RECEIVED state when V_tcp_syncache.see_other is false.
	 * Therefore, store the credentials and take a reference count only
	 * when needed:
	 * - sc is allocated from the zone and not using the on stack instance.
	 * - the sysctl variable net.inet.tcp.syncache.see_other is false.
	 * The reference count is decremented when a zone allocated sc is
	 * freed in syncache_free().
	 */
	if (sc != &scs && !V_tcp_syncache.see_other)
		sc->sc_cred = crhold(so->so_cred);
	else
		sc->sc_cred = NULL;
	sc->sc_port = port;
	sc->sc_ipopts = ipopts;
	bcopy(inc, &sc->sc_inc, sizeof(struct in_conninfo));
	sc->sc_ip_tos = ip_tos;
	sc->sc_ip_ttl = ip_ttl;
#ifdef TCP_OFFLOAD
	sc->sc_tod = tod;
	sc->sc_todctx = todctx;
#endif
	sc->sc_irs = th->th_seq;
	sc->sc_flags = 0;
	sc->sc_flowlabel = 0;

	/*
	 * Initial receive window: clip sbspace to [0 .. TCP_MAXWIN].
	 * win was derived from socket earlier in the function.
	 */
	win = imax(win, 0);
	win = imin(win, TCP_MAXWIN);
	sc->sc_wnd = win;

	if (V_tcp_do_rfc1323 &&
	    !(ltflags & TF_NOOPT)) {
		/*
		 * A timestamp received in a SYN makes
		 * it ok to send timestamp requests and replies.
		 */
		if ((to->to_flags & TOF_TS) && (V_tcp_do_rfc1323 != 2)) {
			sc->sc_tsreflect = to->to_tsval;
			sc->sc_flags |= SCF_TIMESTAMP;
			sc->sc_tsoff = tcp_new_ts_offset(inc);
		}
		if ((to->to_flags & TOF_SCALE) && (V_tcp_do_rfc1323 != 3)) {
			int wscale = 0;

			/*
			 * Pick the smallest possible scaling factor that
			 * will still allow us to scale up to sb_max, aka
			 * kern.ipc.maxsockbuf.
			 *
			 * We do this because there are broken firewalls that
			 * will corrupt the window scale option, leading to
			 * the other endpoint believing that our advertised
			 * window is unscaled.  At scale factors larger than
			 * 5 the unscaled window will drop below 1500 bytes,
			 * leading to serious problems when traversing these
			 * broken firewalls.
			 *
			 * With the default maxsockbuf of 256K, a scale factor
			 * of 3 will be chosen by this algorithm.  Those who
			 * choose a larger maxsockbuf should watch out
			 * for the compatibility problems mentioned above.
			 *
			 * RFC1323: The Window field in a SYN (i.e., a <SYN>
			 * or <SYN,ACK>) segment itself is never scaled.
			 */
			while (wscale < TCP_MAX_WINSHIFT &&
			    (TCP_MAXWIN << wscale) < sb_max)
				wscale++;
			sc->sc_requested_r_scale = wscale;
			sc->sc_requested_s_scale = to->to_wscale;
			sc->sc_flags |= SCF_WINSCALE;
		}
	}
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
	/*
	 * If incoming packet has an MD5 signature, flag this in the
	 * syncache so that syncache_respond() will do the right thing
	 * with the SYN+ACK.
	 */
	if (to->to_flags & TOF_SIGNATURE)
		sc->sc_flags |= SCF_SIGNATURE;
#endif	/* TCP_SIGNATURE */
	if (to->to_flags & TOF_SACKPERM)
		sc->sc_flags |= SCF_SACK;
	if (to->to_flags & TOF_MSS)
		sc->sc_peer_mss = to->to_mss;	/* peer mss may be zero */
	if (ltflags & TF_NOOPT)
		sc->sc_flags |= SCF_NOOPT;
	/* ECN Handshake */
	if (V_tcp_do_ecn && (tp->t_flags2 & TF2_CANNOT_DO_ECN) == 0)
		sc->sc_flags |= tcp_ecn_syncache_add(tcp_get_flags(th), iptos);

	if (V_tcp_syncookies)
		sc->sc_iss = syncookie_generate(sch, sc);
	else
		sc->sc_iss = arc4random();
#ifdef INET6
	if (autoflowlabel) {
		if (V_tcp_syncookies)
			sc->sc_flowlabel = sc->sc_iss;
		else
			sc->sc_flowlabel = ip6_randomflowlabel();
		sc->sc_flowlabel = htonl(sc->sc_flowlabel) & IPV6_FLOWLABEL_MASK;
	}
#endif
	if (locked)
		SCH_UNLOCK(sch);

	if (tfo_cookie_valid) {
		rv = syncache_tfo_expand(sc, so, m, tfo_response_cookie);
		/* INP_RUNLOCK(inp) will be performed by the caller */
		goto tfo_expanded;
	}

	TCP_PROBE5(receive, NULL, NULL, m, NULL, th);
	/*
	 * Do a standard 3-way handshake.
	 */
	if (syncache_respond(sc, m, TH_SYN|TH_ACK) == 0) {
		if (sc != &scs)
			syncache_insert(sc, sch);   /* locks and unlocks sch */
		TCPSTAT_INC(tcps_sndacks);
		TCPSTAT_INC(tcps_sndtotal);
	} else {
		if (sc != &scs)
			syncache_free(sc);
		TCPSTAT_INC(tcps_sc_dropped);
	}
	goto donenoprobe;

done:
	TCP_PROBE5(receive, NULL, NULL, m, NULL, th);
donenoprobe:
	if (m)
		m_freem(m);
	/*
	 * If tfo_pending is not NULL here, then a TFO SYN that did not
	 * result in a new socket was processed and the associated pending
	 * counter has not yet been decremented.  All such TFO processing paths
	 * transit this point.
	 */
	if (tfo_pending != NULL)
		tcp_fastopen_decrement_counter(tfo_pending);

tfo_expanded:
	if (sc == NULL || sc == &scs) {
#ifdef MAC
		mac_syncache_destroy(&maclabel);
#endif
		if (ipopts)
			(void)m_free(ipopts);
	}
	return (rv);
}

/*
 * Send SYN|ACK or ACK to the peer.  Either in response to a peer's segment,
 * i.e. m0 != NULL, or upon 3WHS ACK timeout, i.e. m0 == NULL.
 */
static int
syncache_respond(struct syncache *sc, const struct mbuf *m0, int flags)
{
	struct ip *ip = NULL;
	struct mbuf *m;
	struct tcphdr *th = NULL;
	struct udphdr *udp = NULL;
	int optlen, error = 0;	/* Make compiler happy */
	u_int16_t hlen, tlen, mssopt, ulen;
	struct tcpopt to;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif

	NET_EPOCH_ASSERT();

	hlen =
#ifdef INET6
	       (sc->sc_inc.inc_flags & INC_ISIPV6) ? sizeof(struct ip6_hdr) :
#endif
		sizeof(struct ip);
	tlen = hlen + sizeof(struct tcphdr);
	if (sc->sc_port) {
		tlen += sizeof(struct udphdr);
	}
	/* Determine MSS we advertize to other end of connection. */
	mssopt = tcp_mssopt(&sc->sc_inc);
	if (sc->sc_port)
		mssopt -= V_tcp_udp_tunneling_overhead;
	mssopt = max(mssopt, V_tcp_minmss);

	/* XXX: Assume that the entire packet will fit in a header mbuf. */
	KASSERT(max_linkhdr + tlen + TCP_MAXOLEN <= MHLEN,
	    ("syncache: mbuf too small: hlen %u, sc_port %u, max_linkhdr %d + "
	    "tlen %d + TCP_MAXOLEN %ju <= MHLEN %d", hlen, sc->sc_port,
	    max_linkhdr, tlen, (uintmax_t)TCP_MAXOLEN, MHLEN));

	/* Create the IP+TCP header from scratch. */
	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
#ifdef MAC
	mac_syncache_create_mbuf(sc->sc_label, m);
#endif
	m->m_data += max_linkhdr;
	m->m_len = tlen;
	m->m_pkthdr.len = tlen;
	m->m_pkthdr.rcvif = NULL;

#ifdef INET6
	if (sc->sc_inc.inc_flags & INC_ISIPV6) {
		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_vfc = IPV6_VERSION;
		ip6->ip6_src = sc->sc_inc.inc6_laddr;
		ip6->ip6_dst = sc->sc_inc.inc6_faddr;
		ip6->ip6_plen = htons(tlen - hlen);
		/* ip6_hlim is set after checksum */
		/* Zero out traffic class and flow label. */
		ip6->ip6_flow &= ~IPV6_FLOWINFO_MASK;
		ip6->ip6_flow |= sc->sc_flowlabel;
		if (sc->sc_port != 0) {
			ip6->ip6_nxt = IPPROTO_UDP;
			udp = (struct udphdr *)(ip6 + 1);
			udp->uh_sport = htons(V_tcp_udp_tunneling_port);
			udp->uh_dport = sc->sc_port;
			ulen = (tlen - sizeof(struct ip6_hdr));
			th = (struct tcphdr *)(udp + 1);
		} else {
			ip6->ip6_nxt = IPPROTO_TCP;
			th = (struct tcphdr *)(ip6 + 1);
		}
		ip6->ip6_flow |= htonl(sc->sc_ip_tos << IPV6_FLOWLABEL_LEN);
	}
#endif
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
	{
		ip = mtod(m, struct ip *);
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(struct ip) >> 2;
		ip->ip_len = htons(tlen);
		ip->ip_id = 0;
		ip->ip_off = 0;
		ip->ip_sum = 0;
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
		if (V_path_mtu_discovery && ((sc->sc_flags & SCF_UNREACH) == 0))
		       ip->ip_off |= htons(IP_DF);
		if (sc->sc_port == 0) {
			ip->ip_p = IPPROTO_TCP;
			th = (struct tcphdr *)(ip + 1);
		} else {
			ip->ip_p = IPPROTO_UDP;
			udp = (struct udphdr *)(ip + 1);
			udp->uh_sport = htons(V_tcp_udp_tunneling_port);
			udp->uh_dport = sc->sc_port;
			ulen = (tlen - sizeof(struct ip));
			th = (struct tcphdr *)(udp + 1);
		}
	}
#endif /* INET */
	th->th_sport = sc->sc_inc.inc_lport;
	th->th_dport = sc->sc_inc.inc_fport;

	if (flags & TH_SYN)
		th->th_seq = htonl(sc->sc_iss);
	else
		th->th_seq = htonl(sc->sc_iss + 1);
	th->th_ack = htonl(sc->sc_irs + 1);
	th->th_off = sizeof(struct tcphdr) >> 2;
	th->th_win = htons(sc->sc_wnd);
	th->th_urp = 0;

	flags = tcp_ecn_syncache_respond(flags, sc);
	tcp_set_flags(th, flags);

	/* Tack on the TCP options. */
	if ((sc->sc_flags & SCF_NOOPT) == 0) {
		to.to_flags = 0;

		if (flags & TH_SYN) {
			to.to_mss = mssopt;
			to.to_flags = TOF_MSS;
			if (sc->sc_flags & SCF_WINSCALE) {
				to.to_wscale = sc->sc_requested_r_scale;
				to.to_flags |= TOF_SCALE;
			}
			if (sc->sc_flags & SCF_SACK)
				to.to_flags |= TOF_SACKPERM;
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
			if (sc->sc_flags & SCF_SIGNATURE)
				to.to_flags |= TOF_SIGNATURE;
#endif
			if (sc->sc_tfo_cookie) {
				to.to_flags |= TOF_FASTOPEN;
				to.to_tfo_len = TCP_FASTOPEN_COOKIE_LEN;
				to.to_tfo_cookie = sc->sc_tfo_cookie;
				/* don't send cookie again when retransmitting response */
				sc->sc_tfo_cookie = NULL;
			}
		}
		if (sc->sc_flags & SCF_TIMESTAMP) {
			to.to_tsval = sc->sc_tsoff + tcp_ts_getticks();
			to.to_tsecr = sc->sc_tsreflect;
			to.to_flags |= TOF_TS;
		}
		optlen = tcp_addoptions(&to, (u_char *)(th + 1));

		/* Adjust headers by option size. */
		th->th_off = (sizeof(struct tcphdr) + optlen) >> 2;
		m->m_len += optlen;
		m->m_pkthdr.len += optlen;
#ifdef INET6
		if (sc->sc_inc.inc_flags & INC_ISIPV6)
			ip6->ip6_plen = htons(ntohs(ip6->ip6_plen) + optlen);
		else
#endif
			ip->ip_len = htons(ntohs(ip->ip_len) + optlen);
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		if (sc->sc_flags & SCF_SIGNATURE) {
			KASSERT(to.to_flags & TOF_SIGNATURE,
			    ("tcp_addoptions() didn't set tcp_signature"));

			/* NOTE: to.to_signature is inside of mbuf */
			if (!TCPMD5_ENABLED() ||
			    TCPMD5_OUTPUT(m, th, to.to_signature) != 0) {
				m_freem(m);
				return (EACCES);
			}
		}
#endif
	} else
		optlen = 0;

	if (udp) {
		ulen += optlen;
		udp->uh_ulen = htons(ulen);
	}
	M_SETFIB(m, sc->sc_inc.inc_fibnum);
	/*
	 * If we have peer's SYN and it has a flowid, then let's assign it to
	 * our SYN|ACK.  ip6_output() and ip_output() will not assign flowid
	 * to SYN|ACK due to lack of inp here.
	 */
	if (m0 != NULL && M_HASHTYPE_GET(m0) != M_HASHTYPE_NONE) {
		m->m_pkthdr.flowid = m0->m_pkthdr.flowid;
		M_HASHTYPE_SET(m, M_HASHTYPE_GET(m0));
	}
#ifdef INET6
	if (sc->sc_inc.inc_flags & INC_ISIPV6) {
		if (sc->sc_port) {
			m->m_pkthdr.csum_flags = CSUM_UDP_IPV6;
			m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
			udp->uh_sum = in6_cksum_pseudo(ip6, ulen,
			      IPPROTO_UDP, 0);
			th->th_sum = htons(0);
		} else {
			m->m_pkthdr.csum_flags = CSUM_TCP_IPV6;
			m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
			th->th_sum = in6_cksum_pseudo(ip6, tlen + optlen - hlen,
			    IPPROTO_TCP, 0);
		}
		ip6->ip6_hlim = sc->sc_ip_ttl;
#ifdef TCP_OFFLOAD
		if (ADDED_BY_TOE(sc)) {
			struct toedev *tod = sc->sc_tod;

			error = tod->tod_syncache_respond(tod, sc->sc_todctx, m);

			return (error);
		}
#endif
		TCP_PROBE5(send, NULL, NULL, ip6, NULL, th);
		error = ip6_output(m, NULL, NULL, 0, NULL, NULL, NULL);
	}
#endif
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
	{
		if (sc->sc_port) {
			m->m_pkthdr.csum_flags = CSUM_UDP;
			m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
			udp->uh_sum = in_pseudo(ip->ip_src.s_addr,
			      ip->ip_dst.s_addr, htons(ulen + IPPROTO_UDP));
			th->th_sum = htons(0);
		} else {
			m->m_pkthdr.csum_flags = CSUM_TCP;
			m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
			th->th_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
			    htons(tlen + optlen - hlen + IPPROTO_TCP));
		}
#ifdef TCP_OFFLOAD
		if (ADDED_BY_TOE(sc)) {
			struct toedev *tod = sc->sc_tod;

			error = tod->tod_syncache_respond(tod, sc->sc_todctx, m);

			return (error);
		}
#endif
		TCP_PROBE5(send, NULL, NULL, ip, NULL, th);
		error = ip_output(m, sc->sc_ipopts, NULL, 0, NULL, NULL);
	}
#endif
	return (error);
}

/*
 * The purpose of syncookies is to handle spoofed SYN flooding DoS attacks
 * that exceed the capacity of the syncache by avoiding the storage of any
 * of the SYNs we receive.  Syncookies defend against blind SYN flooding
 * attacks where the attacker does not have access to our responses.
 *
 * Syncookies encode and include all necessary information about the
 * connection setup within the SYN|ACK that we send back.  That way we
 * can avoid keeping any local state until the ACK to our SYN|ACK returns
 * (if ever).  Normally the syncache and syncookies are running in parallel
 * with the latter taking over when the former is exhausted.  When matching
 * syncache entry is found the syncookie is ignored.
 *
 * The only reliable information persisting the 3WHS is our initial sequence
 * number ISS of 32 bits.  Syncookies embed a cryptographically sufficient
 * strong hash (MAC) value and a few bits of TCP SYN options in the ISS
 * of our SYN|ACK.  The MAC can be recomputed when the ACK to our SYN|ACK
 * returns and signifies a legitimate connection if it matches the ACK.
 *
 * The available space of 32 bits to store the hash and to encode the SYN
 * option information is very tight and we should have at least 24 bits for
 * the MAC to keep the number of guesses by blind spoofing reasonably high.
 *
 * SYN option information we have to encode to fully restore a connection:
 * MSS: is imporant to chose an optimal segment size to avoid IP level
 *   fragmentation along the path.  The common MSS values can be encoded
 *   in a 3-bit table.  Uncommon values are captured by the next lower value
 *   in the table leading to a slight increase in packetization overhead.
 * WSCALE: is necessary to allow large windows to be used for high delay-
 *   bandwidth product links.  Not scaling the window when it was initially
 *   negotiated is bad for performance as lack of scaling further decreases
 *   the apparent available send window.  We only need to encode the WSCALE
 *   we received from the remote end.  Our end can be recalculated at any
 *   time.  The common WSCALE values can be encoded in a 3-bit table.
 *   Uncommon values are captured by the next lower value in the table
 *   making us under-estimate the available window size halving our
 *   theoretically possible maximum throughput for that connection.
 * SACK: Greatly assists in packet loss recovery and requires 1 bit.
 * TIMESTAMP and SIGNATURE is not encoded because they are permanent options
 *   that are included in all segments on a connection.  We enable them when
 *   the ACK has them.
 *
 * Security of syncookies and attack vectors:
 *
 * The MAC is computed over (faddr||laddr||fport||lport||irs||flags||secmod)
 * together with the gloabl secret to make it unique per connection attempt.
 * Thus any change of any of those parameters results in a different MAC output
 * in an unpredictable way unless a collision is encountered.  24 bits of the
 * MAC are embedded into the ISS.
 *
 * To prevent replay attacks two rotating global secrets are updated with a
 * new random value every 15 seconds.  The life-time of a syncookie is thus
 * 15-30 seconds.
 *
 * Vector 1: Attacking the secret.  This requires finding a weakness in the
 * MAC itself or the way it is used here.  The attacker can do a chosen plain
 * text attack by varying and testing the all parameters under his control.
 * The strength depends on the size and randomness of the secret, and the
 * cryptographic security of the MAC function.  Due to the constant updating
 * of the secret the attacker has at most 29.999 seconds to find the secret
 * and launch spoofed connections.  After that he has to start all over again.
 *
 * Vector 2: Collision attack on the MAC of a single ACK.  With a 24 bit MAC
 * size an average of 4,823 attempts are required for a 50% chance of success
 * to spoof a single syncookie (birthday collision paradox).  However the
 * attacker is blind and doesn't know if one of his attempts succeeded unless
 * he has a side channel to interfere success from.  A single connection setup
 * success average of 90% requires 8,790 packets, 99.99% requires 17,578 packets.
 * This many attempts are required for each one blind spoofed connection.  For
 * every additional spoofed connection he has to launch another N attempts.
 * Thus for a sustained rate 100 spoofed connections per second approximately
 * 1,800,000 packets per second would have to be sent.
 *
 * NB: The MAC function should be fast so that it doesn't become a CPU
 * exhaustion attack vector itself.
 *
 * References:
 *  RFC4987 TCP SYN Flooding Attacks and Common Mitigations
 *  SYN cookies were first proposed by cryptographer Dan J. Bernstein in 1996
 *   http://cr.yp.to/syncookies.html    (overview)
 *   http://cr.yp.to/syncookies/archive (details)
 *
 *
 * Schematic construction of a syncookie enabled Initial Sequence Number:
 *  0        1         2         3
 *  12345678901234567890123456789012
 * |xxxxxxxxxxxxxxxxxxxxxxxxWWWMMMSP|
 *
 *  x 24 MAC (truncated)
 *  W  3 Send Window Scale index
 *  M  3 MSS index
 *  S  1 SACK permitted
 *  P  1 Odd/even secret
 */

/*
 * Distribution and probability of certain MSS values.  Those in between are
 * rounded down to the next lower one.
 * [An Analysis of TCP Maximum Segment Sizes, S. Alcock and R. Nelson, 2011]
 *                            .2%  .3%   5%    7%    7%    20%   15%   45%
 */
static int tcp_sc_msstab[] = { 216, 536, 1200, 1360, 1400, 1440, 1452, 1460 };

/*
 * Distribution and probability of certain WSCALE values.  We have to map the
 * (send) window scale (shift) option with a range of 0-14 from 4 bits into 3
 * bits based on prevalence of certain values.  Where we don't have an exact
 * match for are rounded down to the next lower one letting us under-estimate
 * the true available window.  At the moment this would happen only for the
 * very uncommon values 3, 5 and those above 8 (more than 16MB socket buffer
 * and window size).  The absence of the WSCALE option (no scaling in either
 * direction) is encoded with index zero.
 * [WSCALE values histograms, Allman, 2012]
 *                            X 10 10 35  5  6 14 10%   by host
 *                            X 11  4  5  5 18 49  3%   by connections
 */
static int tcp_sc_wstab[] = { 0, 0, 1, 2, 4, 6, 7, 8 };

/*
 * Compute the MAC for the SYN cookie.  SIPHASH-2-4 is chosen for its speed
 * and good cryptographic properties.
 */
static uint32_t
syncookie_mac(struct in_conninfo *inc, tcp_seq irs, uint8_t flags,
    uint8_t *secbits, uintptr_t secmod)
{
	SIPHASH_CTX ctx;
	uint32_t siphash[2];

	SipHash24_Init(&ctx);
	SipHash_SetKey(&ctx, secbits);
	switch (inc->inc_flags & INC_ISIPV6) {
#ifdef INET
	case 0:
		SipHash_Update(&ctx, &inc->inc_faddr, sizeof(inc->inc_faddr));
		SipHash_Update(&ctx, &inc->inc_laddr, sizeof(inc->inc_laddr));
		break;
#endif
#ifdef INET6
	case INC_ISIPV6:
		SipHash_Update(&ctx, &inc->inc6_faddr, sizeof(inc->inc6_faddr));
		SipHash_Update(&ctx, &inc->inc6_laddr, sizeof(inc->inc6_laddr));
		break;
#endif
	}
	SipHash_Update(&ctx, &inc->inc_fport, sizeof(inc->inc_fport));
	SipHash_Update(&ctx, &inc->inc_lport, sizeof(inc->inc_lport));
	SipHash_Update(&ctx, &irs, sizeof(irs));
	SipHash_Update(&ctx, &flags, sizeof(flags));
	SipHash_Update(&ctx, &secmod, sizeof(secmod));
	SipHash_Final((u_int8_t *)&siphash, &ctx);

	return (siphash[0] ^ siphash[1]);
}

static tcp_seq
syncookie_generate(struct syncache_head *sch, struct syncache *sc)
{
	u_int i, secbit, wscale;
	uint32_t iss, hash;
	uint8_t *secbits;
	union syncookie cookie;

	cookie.cookie = 0;

	/* Map our computed MSS into the 3-bit index. */
	for (i = nitems(tcp_sc_msstab) - 1;
	     tcp_sc_msstab[i] > sc->sc_peer_mss && i > 0;
	     i--)
		;
	cookie.flags.mss_idx = i;

	/*
	 * Map the send window scale into the 3-bit index but only if
	 * the wscale option was received.
	 */
	if (sc->sc_flags & SCF_WINSCALE) {
		wscale = sc->sc_requested_s_scale;
		for (i = nitems(tcp_sc_wstab) - 1;
		    tcp_sc_wstab[i] > wscale && i > 0;
		     i--)
			;
		cookie.flags.wscale_idx = i;
	}

	/* Can we do SACK? */
	if (sc->sc_flags & SCF_SACK)
		cookie.flags.sack_ok = 1;

	/* Which of the two secrets to use. */
	secbit = V_tcp_syncache.secret.oddeven & 0x1;
	cookie.flags.odd_even = secbit;

	secbits = V_tcp_syncache.secret.key[secbit];
	hash = syncookie_mac(&sc->sc_inc, sc->sc_irs, cookie.cookie, secbits,
	    (uintptr_t)sch);

	/*
	 * Put the flags into the hash and XOR them to get better ISS number
	 * variance.  This doesn't enhance the cryptographic strength and is
	 * done to prevent the 8 cookie bits from showing up directly on the
	 * wire.
	 */
	iss = hash & ~0xff;
	iss |= cookie.cookie ^ (hash >> 24);

	TCPSTAT_INC(tcps_sc_sendcookie);
	return (iss);
}

static bool
syncookie_expand(struct in_conninfo *inc, const struct syncache_head *sch,
    struct syncache *sc, struct tcphdr *th, struct tcpopt *to,
    struct socket *lso, uint16_t port)
{
	uint32_t hash;
	uint8_t *secbits;
	tcp_seq ack, seq;
	int wnd, wscale = 0;
	union syncookie cookie;

	/*
	 * Pull information out of SYN-ACK/ACK and revert sequence number
	 * advances.
	 */
	ack = th->th_ack - 1;
	seq = th->th_seq - 1;

	/*
	 * Unpack the flags containing enough information to restore the
	 * connection.
	 */
	cookie.cookie = (ack & 0xff) ^ (ack >> 24);

	/* Which of the two secrets to use. */
	secbits = V_tcp_syncache.secret.key[cookie.flags.odd_even];

	hash = syncookie_mac(inc, seq, cookie.cookie, secbits, (uintptr_t)sch);

	/* The recomputed hash matches the ACK if this was a genuine cookie. */
	if ((ack & ~0xff) != (hash & ~0xff))
		return (false);

	/* Fill in the syncache values. */
	sc->sc_flags = 0;
	bcopy(inc, &sc->sc_inc, sizeof(struct in_conninfo));
	sc->sc_ipopts = NULL;

	sc->sc_irs = seq;
	sc->sc_iss = ack;

	switch (inc->inc_flags & INC_ISIPV6) {
#ifdef INET
	case 0:
		sc->sc_ip_ttl = sotoinpcb(lso)->inp_ip_ttl;
		sc->sc_ip_tos = sotoinpcb(lso)->inp_ip_tos;
		break;
#endif
#ifdef INET6
	case INC_ISIPV6:
		if (sotoinpcb(lso)->inp_flags & IN6P_AUTOFLOWLABEL)
			sc->sc_flowlabel =
			    htonl(sc->sc_iss) & IPV6_FLOWLABEL_MASK;
		break;
#endif
	}

	sc->sc_peer_mss = tcp_sc_msstab[cookie.flags.mss_idx];

	/* We can simply recompute receive window scale we sent earlier. */
	while (wscale < TCP_MAX_WINSHIFT && (TCP_MAXWIN << wscale) < sb_max)
		wscale++;

	/* Only use wscale if it was enabled in the orignal SYN. */
	if (cookie.flags.wscale_idx > 0) {
		sc->sc_requested_r_scale = wscale;
		sc->sc_requested_s_scale = tcp_sc_wstab[cookie.flags.wscale_idx];
		sc->sc_flags |= SCF_WINSCALE;
	}

	wnd = lso->sol_sbrcv_hiwat;
	wnd = imax(wnd, 0);
	wnd = imin(wnd, TCP_MAXWIN);
	sc->sc_wnd = wnd;

	if (cookie.flags.sack_ok)
		sc->sc_flags |= SCF_SACK;

	if (to->to_flags & TOF_TS) {
		sc->sc_flags |= SCF_TIMESTAMP;
		sc->sc_tsreflect = to->to_tsval;
		sc->sc_tsoff = tcp_new_ts_offset(inc);
	}

	if (to->to_flags & TOF_SIGNATURE)
		sc->sc_flags |= SCF_SIGNATURE;

	sc->sc_rxmits = 0;

	sc->sc_port = port;

	return (true);
}

#ifdef INVARIANTS
static void
syncookie_cmp(struct in_conninfo *inc, const struct syncache_head *sch,
    struct syncache *sc, struct tcphdr *th, struct tcpopt *to,
    struct socket *lso, uint16_t port)
{
	struct syncache scs;
	char *s;

	bzero(&scs, sizeof(scs));
	if (syncookie_expand(inc, sch, &scs, th, to, lso, port) &&
	    (sc->sc_peer_mss != scs.sc_peer_mss ||
	     sc->sc_requested_r_scale != scs.sc_requested_r_scale ||
	     sc->sc_requested_s_scale != scs.sc_requested_s_scale ||
	     (sc->sc_flags & SCF_SACK) != (scs.sc_flags & SCF_SACK))) {

		if ((s = tcp_log_addrs(inc, th, NULL, NULL)) == NULL)
			return;

		if (sc->sc_peer_mss != scs.sc_peer_mss)
			log(LOG_DEBUG, "%s; %s: mss different %i vs %i\n",
			    s, __func__, sc->sc_peer_mss, scs.sc_peer_mss);

		if (sc->sc_requested_r_scale != scs.sc_requested_r_scale)
			log(LOG_DEBUG, "%s; %s: rwscale different %i vs %i\n",
			    s, __func__, sc->sc_requested_r_scale,
			    scs.sc_requested_r_scale);

		if (sc->sc_requested_s_scale != scs.sc_requested_s_scale)
			log(LOG_DEBUG, "%s; %s: swscale different %i vs %i\n",
			    s, __func__, sc->sc_requested_s_scale,
			    scs.sc_requested_s_scale);

		if ((sc->sc_flags & SCF_SACK) != (scs.sc_flags & SCF_SACK))
			log(LOG_DEBUG, "%s; %s: SACK different\n", s, __func__);

		free(s, M_TCPLOG);
	}
}
#endif /* INVARIANTS */

static void
syncookie_reseed(void *arg)
{
	struct tcp_syncache *sc = arg;
	uint8_t *secbits;
	int secbit;

	/*
	 * Reseeding the secret doesn't have to be protected by a lock.
	 * It only must be ensured that the new random values are visible
	 * to all CPUs in a SMP environment.  The atomic with release
	 * semantics ensures that.
	 */
	secbit = (sc->secret.oddeven & 0x1) ? 0 : 1;
	secbits = sc->secret.key[secbit];
	arc4rand(secbits, SYNCOOKIE_SECRET_SIZE, 0);
	atomic_add_rel_int(&sc->secret.oddeven, 1);

	/* Reschedule ourself. */
	callout_schedule(&sc->secret.reseed, SYNCOOKIE_LIFETIME * hz);
}

/*
 * We have overflowed a bucket. Let's pause dealing with the syncache.
 * This function will increment the bucketoverflow statistics appropriately
 * (once per pause when pausing is enabled; otherwise, once per overflow).
 */
static void
syncache_pause(struct in_conninfo *inc)
{
	time_t delta;
	const char *s;

	/* XXX:
	 * 2. Add sysctl read here so we don't get the benefit of this
	 * change without the new sysctl.
	 */

	/*
	 * Try an unlocked read. If we already know that another thread
	 * has activated the feature, there is no need to proceed.
	 */
	if (V_tcp_syncache.paused)
		return;

	/* Are cookied enabled? If not, we can't pause. */
	if (!V_tcp_syncookies) {
		TCPSTAT_INC(tcps_sc_bucketoverflow);
		return;
	}

	/*
	 * We may be the first thread to find an overflow. Get the lock
	 * and evaluate if we need to take action.
	 */
	mtx_lock(&V_tcp_syncache.pause_mtx);
	if (V_tcp_syncache.paused) {
		mtx_unlock(&V_tcp_syncache.pause_mtx);
		return;
	}

	/* Activate protection. */
	V_tcp_syncache.paused = true;
	TCPSTAT_INC(tcps_sc_bucketoverflow);

	/*
	 * Determine the last backoff time. If we are seeing a re-newed
	 * attack within that same time after last reactivating the syncache,
	 * consider it an extension of the same attack.
	 */
	delta = TCP_SYNCACHE_PAUSE_TIME << V_tcp_syncache.pause_backoff;
	if (V_tcp_syncache.pause_until + delta - time_uptime > 0) {
		if (V_tcp_syncache.pause_backoff < TCP_SYNCACHE_MAX_BACKOFF) {
			delta <<= 1;
			V_tcp_syncache.pause_backoff++;
		}
	} else {
		delta = TCP_SYNCACHE_PAUSE_TIME;
		V_tcp_syncache.pause_backoff = 0;
	}

	/* Log a warning, including IP addresses, if able. */
	if (inc != NULL)
		s = tcp_log_addrs(inc, NULL, NULL, NULL);
	else
		s = (const char *)NULL;
	log(LOG_WARNING, "TCP syncache overflow detected; using syncookies for "
	    "the next %lld seconds%s%s%s\n", (long long)delta,
	    (s != NULL) ? " (last SYN: " : "", (s != NULL) ? s : "",
	    (s != NULL) ? ")" : "");
	free(__DECONST(void *, s), M_TCPLOG);

	/* Use the calculated delta to set a new pause time. */
	V_tcp_syncache.pause_until = time_uptime + delta;
	callout_reset(&V_tcp_syncache.pause_co, delta * hz, syncache_unpause,
	    &V_tcp_syncache);
	mtx_unlock(&V_tcp_syncache.pause_mtx);
}

/* Evaluate whether we need to unpause. */
static void
syncache_unpause(void *arg)
{
	struct tcp_syncache *sc;
	time_t delta;

	sc = arg;
	mtx_assert(&sc->pause_mtx, MA_OWNED | MA_NOTRECURSED);
	callout_deactivate(&sc->pause_co);

	/*
	 * Check to make sure we are not running early. If the pause
	 * time has expired, then deactivate the protection.
	 */
	if ((delta = sc->pause_until - time_uptime) > 0)
		callout_schedule(&sc->pause_co, delta * hz);
	else
		sc->paused = false;
}

/*
 * Exports the syncache entries to userland so that netstat can display
 * them alongside the other sockets.  This function is intended to be
 * called only from tcp_pcblist.
 *
 * Due to concurrency on an active system, the number of pcbs exported
 * may have no relation to max_pcbs.  max_pcbs merely indicates the
 * amount of space the caller allocated for this function to use.
 */
int
syncache_pcblist(struct sysctl_req *req)
{
	struct xtcpcb xt;
	struct syncache *sc;
	struct syncache_head *sch;
	int error, i;

	bzero(&xt, sizeof(xt));
	xt.xt_len = sizeof(xt);
	xt.t_state = TCPS_SYN_RECEIVED;
	xt.xt_inp.xi_socket.xso_protocol = IPPROTO_TCP;
	xt.xt_inp.xi_socket.xso_len = sizeof (struct xsocket);
	xt.xt_inp.xi_socket.so_type = SOCK_STREAM;
	xt.xt_inp.xi_socket.so_state = SS_ISCONNECTING;

	for (i = 0; i < V_tcp_syncache.hashsize; i++) {
		sch = &V_tcp_syncache.hashbase[i];
		SCH_LOCK(sch);
		TAILQ_FOREACH(sc, &sch->sch_bucket, sc_hash) {
			if (sc->sc_cred != NULL &&
			    cr_cansee(req->td->td_ucred, sc->sc_cred) != 0)
				continue;
			if (sc->sc_inc.inc_flags & INC_ISIPV6)
				xt.xt_inp.inp_vflag = INP_IPV6;
			else
				xt.xt_inp.inp_vflag = INP_IPV4;
			xt.xt_encaps_port = sc->sc_port;
			bcopy(&sc->sc_inc, &xt.xt_inp.inp_inc,
			    sizeof (struct in_conninfo));
			error = SYSCTL_OUT(req, &xt, sizeof xt);
			if (error) {
				SCH_UNLOCK(sch);
				return (0);
			}
		}
		SCH_UNLOCK(sch);
	}

	return (0);
}
