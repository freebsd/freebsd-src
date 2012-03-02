/*	$OpenBSD: if_pfsync.c,v 1.110 2009/02/24 05:39:19 dlg Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2009 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Revisions picked from OpenBSD after revision 1.110 import:
 * 1.118, 1.124, 1.148, 1.149, 1.151, 1.171 - fixes to bulk updates
 * 1.120, 1.175 - use monotonic time_uptime
 * 1.122 - reduce number of updates for non-TCP sessions
 * 1.128 - cleanups
 * 1.170 - SIOCSIFMTU checks
 */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_pf.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define	NBPFILTER	1

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sockio.h>
#include <sys/taskqueue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/bpf.h>
#include <net/netisr.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>

#ifdef	INET
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#endif

#ifdef INET6
#include <netinet6/nd6.h>
#endif /* INET6 */

#include <netinet/ip_carp.h>

#include <net/pfvar.h>
#include <net/if_pfsync.h>


#define PFSYNC_MINPKT ( \
	sizeof(struct ip) + \
	sizeof(struct pfsync_header) + \
	sizeof(struct pfsync_subheader) + \
	sizeof(struct pfsync_eof))

struct pfsync_pkt {
	struct ip *ip;
	struct in_addr src;
	u_int8_t flags;
};

static int	pfsync_upd_tcp(struct pf_state *, struct pfsync_state_peer *,
		    struct pfsync_state_peer *);
static int	pfsync_in_clr(struct pfsync_pkt *, struct mbuf *, int, int);
static int	pfsync_in_ins(struct pfsync_pkt *, struct mbuf *, int, int);
static int	pfsync_in_iack(struct pfsync_pkt *, struct mbuf *, int, int);
static int	pfsync_in_upd(struct pfsync_pkt *, struct mbuf *, int, int);
static int	pfsync_in_upd_c(struct pfsync_pkt *, struct mbuf *, int, int);
static int	pfsync_in_ureq(struct pfsync_pkt *, struct mbuf *, int, int);
static int	pfsync_in_del(struct pfsync_pkt *, struct mbuf *, int, int);
static int	pfsync_in_del_c(struct pfsync_pkt *, struct mbuf *, int, int);
static int	pfsync_in_bus(struct pfsync_pkt *, struct mbuf *, int, int);
static int	pfsync_in_tdb(struct pfsync_pkt *, struct mbuf *, int, int);
static int	pfsync_in_eof(struct pfsync_pkt *, struct mbuf *, int, int);
static int	pfsync_in_error(struct pfsync_pkt *, struct mbuf *, int, int);

static int (*pfsync_acts[])(struct pfsync_pkt *, struct mbuf *, int, int) = {
	pfsync_in_clr,			/* PFSYNC_ACT_CLR */
	pfsync_in_ins,			/* PFSYNC_ACT_INS */
	pfsync_in_iack,			/* PFSYNC_ACT_INS_ACK */
	pfsync_in_upd,			/* PFSYNC_ACT_UPD */
	pfsync_in_upd_c,		/* PFSYNC_ACT_UPD_C */
	pfsync_in_ureq,			/* PFSYNC_ACT_UPD_REQ */
	pfsync_in_del,			/* PFSYNC_ACT_DEL */
	pfsync_in_del_c,		/* PFSYNC_ACT_DEL_C */
	pfsync_in_error,		/* PFSYNC_ACT_INS_F */
	pfsync_in_error,		/* PFSYNC_ACT_DEL_F */
	pfsync_in_bus,			/* PFSYNC_ACT_BUS */
	pfsync_in_tdb,			/* PFSYNC_ACT_TDB */
	pfsync_in_eof			/* PFSYNC_ACT_EOF */
};

struct pfsync_q {
	int		(*write)(struct pf_state *, struct mbuf *, int);
	size_t		len;
	u_int8_t	action;
};

/* we have one of these for every PFSYNC_S_ */
static int	pfsync_out_state(struct pf_state *, struct mbuf *, int);
static int	pfsync_out_iack(struct pf_state *, struct mbuf *, int);
static int	pfsync_out_upd_c(struct pf_state *, struct mbuf *, int);
static int	pfsync_out_del(struct pf_state *, struct mbuf *, int);

static struct pfsync_q pfsync_qs[] = {
	{ pfsync_out_state, sizeof(struct pfsync_state),   PFSYNC_ACT_INS },
	{ pfsync_out_iack,  sizeof(struct pfsync_ins_ack), PFSYNC_ACT_INS_ACK },
	{ pfsync_out_state, sizeof(struct pfsync_state),   PFSYNC_ACT_UPD },
	{ pfsync_out_upd_c, sizeof(struct pfsync_upd_c),   PFSYNC_ACT_UPD_C },
	{ pfsync_out_del,   sizeof(struct pfsync_del_c),   PFSYNC_ACT_DEL_C }
};

static void	pfsync_q_ins(struct pf_state *, int);
static void	pfsync_q_del(struct pf_state *);

static void	pfsync_update_state(struct pf_state *);

struct pfsync_upd_req_item {
	TAILQ_ENTRY(pfsync_upd_req_item)	ur_entry;
	struct pfsync_upd_req			ur_msg;
};
TAILQ_HEAD(pfsync_upd_reqs, pfsync_upd_req_item);

struct pfsync_deferral {
	TAILQ_ENTRY(pfsync_deferral)		 pd_entry;
	struct pf_state				*pd_st;
	struct mbuf				*pd_m;
	struct callout				 pd_tmo;
};
TAILQ_HEAD(pfsync_deferrals, pfsync_deferral);

#define PFSYNC_PLSIZE	MAX(sizeof(struct pfsync_upd_req_item), \
			    sizeof(struct pfsync_deferral))

#ifdef notyet
static int	pfsync_out_tdb(struct tdb *, struct mbuf *, int);
#endif

struct pfsync_softc {
	struct ifnet		*sc_ifp;
	struct ifnet		*sc_sync_if;

	uma_zone_t		 sc_pool;

	struct ip_moptions	 sc_imo;

	struct in_addr		 sc_sync_peer;
	u_int8_t		 sc_maxupdates;
	int			 pfsync_sync_ok;

	struct ip		 sc_template;

	struct pf_state_queue	 sc_qs[PFSYNC_S_COUNT];
	size_t			 sc_len;

	struct pfsync_upd_reqs	 sc_upd_req_list;

	struct pfsync_deferrals	 sc_deferrals;
	u_int			 sc_deferred;

	void			*sc_plus;
	size_t			 sc_pluslen;

	u_int32_t		 sc_ureq_sent;
	int			 sc_bulk_tries;
	struct callout		 sc_bulkfail_tmo;

	u_int32_t		 sc_ureq_received;
	struct pf_state		*sc_bulk_next;
	struct pf_state		*sc_bulk_last;
	struct callout		 sc_bulk_tmo;

	TAILQ_HEAD(, tdb)	 sc_tdb_q;

	struct callout		 sc_tmo;
};

static MALLOC_DEFINE(M_PFSYNC, "pfsync", "pfsync data");
static VNET_DEFINE(struct pfsync_softc	*, pfsyncif) = NULL;
#define	V_pfsyncif		VNET(pfsyncif)
static VNET_DEFINE(void *, pfsync_swi_cookie) = NULL;
#define	V_pfsync_swi_cookie	VNET(pfsync_swi_cookie)
static VNET_DEFINE(struct pfsyncstats, pfsyncstats);
#define	V_pfsyncstats		VNET(pfsyncstats)
static VNET_DEFINE(int, pfsync_carp_adj) = CARP_MAXSKEW;
#define	V_pfsync_carp_adj	VNET(pfsync_carp_adj)

static void	pfsyncintr(void *);
static int	pfsync_multicast_setup(struct pfsync_softc *);
static void	pfsync_multicast_cleanup(struct pfsync_softc *);
static int	pfsync_init(void);
static void	pfsync_uninit(void);
static void	pfsync_sendout1(int);

#define	schednetisr(NETISR_PFSYNC)	swi_sched(V_pfsync_swi_cookie, 0)

SYSCTL_NODE(_net, OID_AUTO, pfsync, CTLFLAG_RW, 0, "PFSYNC");
SYSCTL_VNET_STRUCT(_net_pfsync, OID_AUTO, stats, CTLFLAG_RW,
    &VNET_NAME(pfsyncstats), pfsyncstats,
    "PFSYNC statistics (struct pfsyncstats, net/if_pfsync.h)");
SYSCTL_INT(_net_pfsync, OID_AUTO, carp_demotion_factor, CTLFLAG_RW,
    &VNET_NAME(pfsync_carp_adj), 0, "pfsync's CARP demotion factor adjustment");

static int	pfsync_clone_create(struct if_clone *, int, caddr_t);
static void	pfsync_clone_destroy(struct ifnet *);
static int	pfsync_alloc_scrub_memory(struct pfsync_state_peer *,
		    struct pf_state_peer *);
static int	pfsyncoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
		    struct route *);
static int	pfsyncioctl(struct ifnet *, u_long, caddr_t);
static void	pfsyncstart(struct ifnet *);

static struct mbuf	*pfsync_if_dequeue(struct ifnet *);

static void	pfsync_deferred(struct pf_state *, int);
static void	pfsync_undefer(struct pfsync_deferral *, int);
static void	pfsync_defer_tmo(void *);

static void	pfsync_request_update(u_int32_t, u_int64_t);
static void	pfsync_update_state_req(struct pf_state *);

static void	pfsync_drop(struct pfsync_softc *);
static void	pfsync_sendout(void);
static void	pfsync_send_plus(void *, size_t);
static void	pfsync_timeout(void *);

static void	pfsync_bulk_start(void);
static void	pfsync_bulk_status(u_int8_t);
static void	pfsync_bulk_update(void *);
static void	pfsync_bulk_fail(void *);

#ifdef IPSEC
static void	pfsync_update_net_tdb(struct pfsync_tdb *);
#endif

#define PFSYNC_MAX_BULKTRIES	12

VNET_DEFINE(struct ifc_simple_data, pfsync_cloner_data);
VNET_DEFINE(struct if_clone, pfsync_cloner);
#define	V_pfsync_cloner_data	VNET(pfsync_cloner_data)
#define	V_pfsync_cloner		VNET(pfsync_cloner)
IFC_SIMPLE_DECLARE(pfsync, 1);

static int
pfsync_clone_create(struct if_clone *ifc, int unit, caddr_t param)
{
	struct pfsync_softc *sc;
	struct ifnet *ifp;
	int q;

	if (unit != 0)
		return (EINVAL);

	sc = malloc(sizeof(struct pfsync_softc), M_PFSYNC, M_WAITOK | M_ZERO);
	sc->pfsync_sync_ok = 1;

	for (q = 0; q < PFSYNC_S_COUNT; q++)
		TAILQ_INIT(&sc->sc_qs[q]);

	sc->sc_pool = uma_zcreate("pfsync", PFSYNC_PLSIZE, NULL, NULL, NULL,
	    NULL, UMA_ALIGN_PTR, 0);
	TAILQ_INIT(&sc->sc_upd_req_list);
	TAILQ_INIT(&sc->sc_deferrals);
	sc->sc_deferred = 0;

	TAILQ_INIT(&sc->sc_tdb_q);

	sc->sc_len = PFSYNC_MINPKT;
	sc->sc_maxupdates = 128;


	ifp = sc->sc_ifp = if_alloc(IFT_PFSYNC);
	if (ifp == NULL) {
		uma_zdestroy(sc->sc_pool);
		free(sc, M_PFSYNC);
		return (ENOSPC);
	}
	if_initname(ifp, ifc->ifc_name, unit);
	ifp->if_softc = sc;
	ifp->if_ioctl = pfsyncioctl;
	ifp->if_output = pfsyncoutput;
	ifp->if_start = pfsyncstart;
	ifp->if_type = IFT_PFSYNC;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_hdrlen = sizeof(struct pfsync_header);
	ifp->if_mtu = ETHERMTU;
	callout_init(&sc->sc_tmo, CALLOUT_MPSAFE);
	callout_init_mtx(&sc->sc_bulk_tmo, &pf_mtx, 0);
	callout_init(&sc->sc_bulkfail_tmo, CALLOUT_MPSAFE);

	if_attach(ifp);

#if NBPFILTER > 0
	bpfattach(ifp, DLT_PFSYNC, PFSYNC_HDRLEN);
#endif

	V_pfsyncif = sc;

	return (0);
}

static void
pfsync_clone_destroy(struct ifnet *ifp)
{
	struct pfsync_softc *sc = ifp->if_softc;

	PF_LOCK();
	callout_stop(&sc->sc_bulkfail_tmo);
	callout_stop(&sc->sc_bulk_tmo);
	callout_stop(&sc->sc_tmo);
	PF_UNLOCK();
	if (!sc->pfsync_sync_ok && carp_demote_adj_p)
		(*carp_demote_adj_p)(-V_pfsync_carp_adj, "pfsync destroy");
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);

	pfsync_drop(sc);

	while (sc->sc_deferred > 0)
		pfsync_undefer(TAILQ_FIRST(&sc->sc_deferrals), 0);

	uma_zdestroy(sc->sc_pool);
	if_free(ifp);
	if (sc->sc_imo.imo_membership)
		pfsync_multicast_cleanup(sc);
	free(sc, M_PFSYNC);

	V_pfsyncif = NULL;

}

static struct mbuf *
pfsync_if_dequeue(struct ifnet *ifp)
{
	struct mbuf *m;

	IF_LOCK(&ifp->if_snd);
	_IF_DROP(&ifp->if_snd);
	_IF_DEQUEUE(&ifp->if_snd, m);
	IF_UNLOCK(&ifp->if_snd);

	return (m);
}

/*
 * Start output on the pfsync interface.
 */
static void
pfsyncstart(struct ifnet *ifp)
{
	struct mbuf *m;

	while ((m = pfsync_if_dequeue(ifp)) != NULL) {
		m_freem(m);
	}
}

static int
pfsync_alloc_scrub_memory(struct pfsync_state_peer *s,
    struct pf_state_peer *d)
{
	if (s->scrub.scrub_flag && d->scrub == NULL) {
		d->scrub = uma_zalloc(V_pf_state_scrub_z, M_NOWAIT | M_ZERO);
		if (d->scrub == NULL)
			return (ENOMEM);
	}

	return (0);
}


static int
pfsync_state_import(struct pfsync_state *sp, u_int8_t flags)
{
	struct pf_state	*st = NULL;
	struct pf_state_key *skw = NULL, *sks = NULL;
	struct pf_rule *r = NULL;
	struct pfi_kif	*kif;
	int pool_flags;
	int error;

	PF_LOCK_ASSERT();

	if (sp->creatorid == 0 && V_pf_status.debug >= PF_DEBUG_MISC) {
		printf("pfsync_state_import: invalid creator id:"
		    " %08x\n", ntohl(sp->creatorid));
		return (EINVAL);
	}

	if ((kif = pfi_kif_get(sp->ifname)) == NULL) {
		if (V_pf_status.debug >= PF_DEBUG_MISC)
			printf("pfsync_state_import: "
			    "unknown interface: %s\n", sp->ifname);
		if (flags & PFSYNC_SI_IOCTL)
			return (EINVAL);
		return (0);	/* skip this state */
	}

	/*
	 * If the ruleset checksums match or the state is coming from the ioctl,
	 * it's safe to associate the state with the rule of that number.
	 */
	if (sp->rule != htonl(-1) && sp->anchor == htonl(-1) &&
	    (flags & (PFSYNC_SI_IOCTL | PFSYNC_SI_CKSUM)) && ntohl(sp->rule) <
	    pf_main_ruleset.rules[PF_RULESET_FILTER].active.rcount)
		r = pf_main_ruleset.rules[
		    PF_RULESET_FILTER].active.ptr_array[ntohl(sp->rule)];
	else
		r = &V_pf_default_rule;

	if ((r->max_states && r->states_cur >= r->max_states))
		goto cleanup;

	if (flags & PFSYNC_SI_IOCTL)
		pool_flags = M_WAITOK | M_ZERO;
	else
		pool_flags = M_NOWAIT | M_ZERO;

	if ((st = uma_zalloc(V_pf_state_z, pool_flags)) == NULL)
		goto cleanup;

	if ((skw = pf_alloc_state_key(pool_flags)) == NULL)
		goto cleanup;

	if (PF_ANEQ(&sp->key[PF_SK_WIRE].addr[0],
	    &sp->key[PF_SK_STACK].addr[0], sp->af) ||
	    PF_ANEQ(&sp->key[PF_SK_WIRE].addr[1],
	    &sp->key[PF_SK_STACK].addr[1], sp->af) ||
	    sp->key[PF_SK_WIRE].port[0] != sp->key[PF_SK_STACK].port[0] ||
	    sp->key[PF_SK_WIRE].port[1] != sp->key[PF_SK_STACK].port[1]) {
		if ((sks = pf_alloc_state_key(pool_flags)) == NULL)
			goto cleanup;
	} else
		sks = skw;

	/* allocate memory for scrub info */
	if (pfsync_alloc_scrub_memory(&sp->src, &st->src) ||
	    pfsync_alloc_scrub_memory(&sp->dst, &st->dst))
		goto cleanup;

	/* copy to state key(s) */
	skw->addr[0] = sp->key[PF_SK_WIRE].addr[0];
	skw->addr[1] = sp->key[PF_SK_WIRE].addr[1];
	skw->port[0] = sp->key[PF_SK_WIRE].port[0];
	skw->port[1] = sp->key[PF_SK_WIRE].port[1];
	skw->proto = sp->proto;
	skw->af = sp->af;
	if (sks != skw) {
		sks->addr[0] = sp->key[PF_SK_STACK].addr[0];
		sks->addr[1] = sp->key[PF_SK_STACK].addr[1];
		sks->port[0] = sp->key[PF_SK_STACK].port[0];
		sks->port[1] = sp->key[PF_SK_STACK].port[1];
		sks->proto = sp->proto;
		sks->af = sp->af;
	}

	/* copy to state */
	bcopy(&sp->rt_addr, &st->rt_addr, sizeof(st->rt_addr));
	st->creation = time_uptime - ntohl(sp->creation);
	st->expire = time_second;
	if (sp->expire) {
		/* XXX No adaptive scaling. */
		st->expire -= r->timeout[sp->timeout] - ntohl(sp->expire);
	}

	st->expire = ntohl(sp->expire) + time_second;
	st->direction = sp->direction;
	st->log = sp->log;
	st->timeout = sp->timeout;
	st->state_flags = sp->state_flags;

	bcopy(sp->id, &st->id, sizeof(st->id));
	st->creatorid = sp->creatorid;
	pf_state_peer_ntoh(&sp->src, &st->src);
	pf_state_peer_ntoh(&sp->dst, &st->dst);

	st->rule.ptr = r;
	st->nat_rule.ptr = NULL;
	st->anchor.ptr = NULL;
	st->rt_kif = NULL;

	st->pfsync_time = time_uptime;
	st->sync_state = PFSYNC_S_NONE;

	/* XXX when we have nat_rule/anchors, use STATE_INC_COUNTERS */
	r->states_cur++;
	r->states_tot++;

	if (!ISSET(flags, PFSYNC_SI_IOCTL))
		SET(st->state_flags, PFSTATE_NOSYNC);

	if ((error = pf_state_insert(kif, skw, sks, st)) != 0) {
		/* XXX when we have nat_rule/anchors, use STATE_DEC_COUNTERS */
		r->states_cur--;
		goto cleanup_state;
	}

	if (!ISSET(flags, PFSYNC_SI_IOCTL)) {
		CLR(st->state_flags, PFSTATE_NOSYNC);
		if (ISSET(st->state_flags, PFSTATE_ACK)) {
			pfsync_q_ins(st, PFSYNC_S_IACK);
			schednetisr(NETISR_PFSYNC);
		}
	}
	CLR(st->state_flags, PFSTATE_ACK);

	return (0);

cleanup:
	error = ENOMEM;
	if (skw == sks)
		sks = NULL;
	if (skw != NULL)
		uma_zfree(V_pf_state_key_z, skw);
	if (sks != NULL)
		uma_zfree(V_pf_state_key_z, sks);

cleanup_state:	/* pf_state_insert frees the state keys */
	if (st) {
		if (st->dst.scrub)
			uma_zfree(V_pf_state_scrub_z, st->dst.scrub);
		if (st->src.scrub)
			uma_zfree(V_pf_state_scrub_z, st->src.scrub);
		uma_zfree(V_pf_state_z, st);
	}
	return (error);
}

static void
pfsync_input(struct mbuf *m, __unused int off)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_pkt pkt;
	struct ip *ip = mtod(m, struct ip *);
	struct pfsync_header *ph;
	struct pfsync_subheader subh;

	int offset;
	int rv;

	V_pfsyncstats.pfsyncs_ipackets++;

	/* verify that we have a sync interface configured */
	if (!sc || !sc->sc_sync_if || !V_pf_status.running)
		goto done;

	/* verify that the packet came in on the right interface */
	if (sc->sc_sync_if != m->m_pkthdr.rcvif) {
		V_pfsyncstats.pfsyncs_badif++;
		goto done;
	}

	sc->sc_ifp->if_ipackets++;
	sc->sc_ifp->if_ibytes += m->m_pkthdr.len;
	/* verify that the IP TTL is 255. */
	if (ip->ip_ttl != PFSYNC_DFLTTL) {
		V_pfsyncstats.pfsyncs_badttl++;
		goto done;
	}

	offset = ip->ip_hl << 2;
	if (m->m_pkthdr.len < offset + sizeof(*ph)) {
		V_pfsyncstats.pfsyncs_hdrops++;
		goto done;
	}

	if (offset + sizeof(*ph) > m->m_len) {
		if (m_pullup(m, offset + sizeof(*ph)) == NULL) {
			V_pfsyncstats.pfsyncs_hdrops++;
			return;
		}
		ip = mtod(m, struct ip *);
	}
	ph = (struct pfsync_header *)((char *)ip + offset);

	/* verify the version */
	if (ph->version != PFSYNC_VERSION) {
		V_pfsyncstats.pfsyncs_badver++;
		goto done;
	}

	/* Cheaper to grab this now than having to mess with mbufs later */
	pkt.ip = ip;
	pkt.src = ip->ip_src;
	pkt.flags = 0;

	if (!bcmp(&ph->pfcksum, &V_pf_status.pf_chksum, PF_MD5_DIGEST_LENGTH))
		pkt.flags |= PFSYNC_SI_CKSUM;

	offset += sizeof(*ph);
	for (;;) {
		m_copydata(m, offset, sizeof(subh), (caddr_t)&subh);
		offset += sizeof(subh);

		if (subh.action >= PFSYNC_ACT_MAX) {
			V_pfsyncstats.pfsyncs_badact++;
			goto done;
		}

		rv = (*pfsync_acts[subh.action])(&pkt, m, offset,
		    ntohs(subh.count));
		if (rv == -1)
			return;

		offset += rv;
	}

done:
	m_freem(m);
}

static int
pfsync_in_clr(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct pfsync_clr *clr;
	struct mbuf *mp;
	int len = sizeof(*clr) * count;
	int i, offp;

	struct pf_state *si, *st, *nexts;
	struct pf_state_key *sk, *nextsk;
	u_int32_t creatorid;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	clr = (struct pfsync_clr *)(mp->m_data + offp);

	PF_LOCK();
	for (i = 0; i < count; i++) {
		creatorid = clr[i].creatorid;

		if (clr[i].ifname[0] == '\0') {
			PF_KEYS_LOCK();
			PF_IDS_LOCK();
			for (st = RB_MIN(pf_state_tree_id, &V_tree_id);
			    st; st = nexts) {
				nexts = RB_NEXT(pf_state_tree_id, &V_tree_id, st);
				if (st->creatorid == creatorid) {
					SET(st->state_flags, PFSTATE_NOSYNC);
					pf_unlink_state(st, 1);
				}
			}
			PF_IDS_UNLOCK();
			PF_KEYS_UNLOCK();
		} else {
			if (pfi_kif_get(clr[i].ifname) == NULL)
				continue;

			PF_KEYS_LOCK();
			/* XXX correct? */
			for (sk = RB_MIN(pf_state_tree, &V_pf_statetbl);
			    sk; sk = nextsk) {
				nextsk = RB_NEXT(pf_state_tree,
				    &V_pf_statetbl, sk);
				TAILQ_FOREACH(si, &sk->states, key_list) {
					if (si->creatorid == creatorid) {
						SET(si->state_flags,
						    PFSTATE_NOSYNC);
						pf_unlink_state(si, 0);
					}
				}
			}
			PF_KEYS_UNLOCK();
		}
	}
	PF_UNLOCK();

	return (len);
}

static int
pfsync_in_ins(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct mbuf *mp;
	struct pfsync_state *sa, *sp;
	int len = sizeof(*sp) * count;
	int i, offp;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	sa = (struct pfsync_state *)(mp->m_data + offp);

	PF_LOCK();
	for (i = 0; i < count; i++) {
		sp = &sa[i];

		/* check for invalid values */
		if (sp->timeout >= PFTM_MAX ||
		    sp->src.state > PF_TCPS_PROXY_DST ||
		    sp->dst.state > PF_TCPS_PROXY_DST ||
		    sp->direction > PF_OUT ||
		    (sp->af != AF_INET && sp->af != AF_INET6)) {
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
				printf("pfsync_input: PFSYNC5_ACT_INS: "
				    "invalid value\n");
			}
			V_pfsyncstats.pfsyncs_badval++;
			continue;
		}

		if (pfsync_state_import(sp, pkt->flags) == ENOMEM) {
			/* drop out, but process the rest of the actions */
			break;
		}
	}
	PF_UNLOCK();

	return (len);
}

static int
pfsync_in_iack(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct pfsync_ins_ack *ia, *iaa;
	struct pf_state_cmp id_key;
	struct pf_state *st;

	struct mbuf *mp;
	int len = count * sizeof(*ia);
	int offp, i;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	iaa = (struct pfsync_ins_ack *)(mp->m_data + offp);

	PF_LOCK();
	for (i = 0; i < count; i++) {
		ia = &iaa[i];

		bcopy(&ia->id, &id_key.id, sizeof(id_key.id));
		id_key.creatorid = ia->creatorid;

		st = pf_find_state_byid(&id_key);
		if (st == NULL)
			continue;

		if (ISSET(st->state_flags, PFSTATE_ACK))
			pfsync_deferred(st, 0);
	}
	PF_UNLOCK();
	/*
	 * XXX this is not yet implemented, but we know the size of the
	 * message so we can skip it.
	 */

	return (count * sizeof(struct pfsync_ins_ack));
}

static int
pfsync_upd_tcp(struct pf_state *st, struct pfsync_state_peer *src,
    struct pfsync_state_peer *dst)
{
	int sfail = 0;

	/*
	 * The state should never go backwards except
	 * for syn-proxy states.  Neither should the
	 * sequence window slide backwards.
	 */
	if (st->src.state > src->state &&
	    (st->src.state < PF_TCPS_PROXY_SRC ||
	    src->state >= PF_TCPS_PROXY_SRC))
		sfail = 1;
	else if (SEQ_GT(st->src.seqlo, ntohl(src->seqlo)))
		sfail = 3;
	else if (st->dst.state > dst->state) {
		/* There might still be useful
		 * information about the src state here,
		 * so import that part of the update,
		 * then "fail" so we send the updated
		 * state back to the peer who is missing
		 * our what we know. */
		pf_state_peer_ntoh(src, &st->src);
		/* XXX do anything with timeouts? */
		sfail = 7;
	} else if (st->dst.state >= TCPS_SYN_SENT &&
	    SEQ_GT(st->dst.seqlo, ntohl(dst->seqlo)))
		sfail = 4;

	return (sfail);
}

static int
pfsync_in_upd(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct pfsync_state *sa, *sp;
	struct pf_state_cmp id_key;
	struct pf_state_key *sk;
	struct pf_state *st;
	int sfail;

	struct mbuf *mp;
	int len = count * sizeof(*sp);
	int offp, i;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	sa = (struct pfsync_state *)(mp->m_data + offp);

	PF_LOCK();
	for (i = 0; i < count; i++) {
		sp = &sa[i];

		/* check for invalid values */
		if (sp->timeout >= PFTM_MAX ||
		    sp->src.state > PF_TCPS_PROXY_DST ||
		    sp->dst.state > PF_TCPS_PROXY_DST) {
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
				printf("pfsync_input: PFSYNC_ACT_UPD: "
				    "invalid value\n");
			}
			V_pfsyncstats.pfsyncs_badval++;
			continue;
		}

		bcopy(sp->id, &id_key.id, sizeof(id_key.id));
		id_key.creatorid = sp->creatorid;

		st = pf_find_state_byid(&id_key);
		if (st == NULL) {
			/* insert the update */
			if (pfsync_state_import(sp, 0))
				V_pfsyncstats.pfsyncs_badstate++;
			continue;
		}

		if (ISSET(st->state_flags, PFSTATE_ACK))
			pfsync_deferred(st, 1);

		sk = st->key[PF_SK_WIRE];	/* XXX right one? */
		sfail = 0;
		if (sk->proto == IPPROTO_TCP)
			sfail = pfsync_upd_tcp(st, &sp->src, &sp->dst);
		else {
			/*
			 * Non-TCP protocol state machine always go
			 * forwards
			 */
			if (st->src.state > sp->src.state)
				sfail = 5;
			else if (st->dst.state > sp->dst.state)
				sfail = 6;
		}

		if (sfail) {
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
				printf("pfsync: %s stale update (%d)"
				    " id: %016llx creatorid: %08x\n",
				    (sfail < 7 ?  "ignoring" : "partial"),
				    sfail, (unsigned long long)be64toh(st->id),
				    ntohl(st->creatorid));
			}
			V_pfsyncstats.pfsyncs_stale++;

			pfsync_update_state(st);
			schednetisr(NETISR_PFSYNC);
			continue;
		}
		pfsync_alloc_scrub_memory(&sp->dst, &st->dst);
		pf_state_peer_ntoh(&sp->src, &st->src);
		pf_state_peer_ntoh(&sp->dst, &st->dst);
		st->expire = ntohl(sp->expire) + time_second;
		st->timeout = sp->timeout;
		st->pfsync_time = time_uptime;
	}
	PF_UNLOCK();

	return (len);
}

static int
pfsync_in_upd_c(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct pfsync_upd_c *ua, *up;
	struct pf_state_key *sk;
	struct pf_state_cmp id_key;
	struct pf_state *st;

	int len = count * sizeof(*up);
	int sfail;

	struct mbuf *mp;
	int offp, i;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	ua = (struct pfsync_upd_c *)(mp->m_data + offp);

	PF_LOCK();
	for (i = 0; i < count; i++) {
		up = &ua[i];

		/* check for invalid values */
		if (up->timeout >= PFTM_MAX ||
		    up->src.state > PF_TCPS_PROXY_DST ||
		    up->dst.state > PF_TCPS_PROXY_DST) {
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
				printf("pfsync_input: "
				    "PFSYNC_ACT_UPD_C: "
				    "invalid value\n");
			}
			V_pfsyncstats.pfsyncs_badval++;
			continue;
		}

		bcopy(&up->id, &id_key.id, sizeof(id_key.id));
		id_key.creatorid = up->creatorid;

		st = pf_find_state_byid(&id_key);
		if (st == NULL) {
			/* We don't have this state. Ask for it. */
			pfsync_request_update(id_key.creatorid, id_key.id);
			continue;
		}

		if (ISSET(st->state_flags, PFSTATE_ACK))
			pfsync_deferred(st, 1);

		sk = st->key[PF_SK_WIRE]; /* XXX right one? */
		sfail = 0;
		if (sk->proto == IPPROTO_TCP)
			sfail = pfsync_upd_tcp(st, &up->src, &up->dst);
		else {
			/*
			 * Non-TCP protocol state machine always go forwards
			 */
			if (st->src.state > up->src.state)
				sfail = 5;
			else if (st->dst.state > up->dst.state)
				sfail = 6;
		}

		if (sfail) {
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
				printf("pfsync: ignoring stale update "
				    "(%d) id: %016llx "
				    "creatorid: %08x\n", sfail,
				    (unsigned long long)be64toh(st->id),
				    ntohl(st->creatorid));
			}
			V_pfsyncstats.pfsyncs_stale++;

			pfsync_update_state(st);
			schednetisr(NETISR_PFSYNC);
			continue;
		}
		pfsync_alloc_scrub_memory(&up->dst, &st->dst);
		pf_state_peer_ntoh(&up->src, &st->src);
		pf_state_peer_ntoh(&up->dst, &st->dst);
		st->expire = ntohl(up->expire) + time_second;
		st->timeout = up->timeout;
		st->pfsync_time = time_uptime;
	}
	PF_UNLOCK();

	return (len);
}

static int
pfsync_in_ureq(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct pfsync_upd_req *ur, *ura;
	struct mbuf *mp;
	int len = count * sizeof(*ur);
	int i, offp;

	struct pf_state_cmp id_key;
	struct pf_state *st;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	ura = (struct pfsync_upd_req *)(mp->m_data + offp);

	PF_LOCK();
	for (i = 0; i < count; i++) {
		ur = &ura[i];

		bcopy(&ur->id, &id_key.id, sizeof(id_key.id));
		id_key.creatorid = ur->creatorid;

		if (id_key.id == 0 && id_key.creatorid == 0)
			pfsync_bulk_start();
		else {
			st = pf_find_state_byid(&id_key);
			if (st == NULL) {
				V_pfsyncstats.pfsyncs_badstate++;
				continue;
			}
			if (ISSET(st->state_flags, PFSTATE_NOSYNC))
				continue;

			pfsync_update_state_req(st);
		}
	}
	PF_UNLOCK();

	return (len);
}

static int
pfsync_in_del(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct mbuf *mp;
	struct pfsync_state *sa, *sp;
	struct pf_state_cmp id_key;
	struct pf_state *st;
	int len = count * sizeof(*sp);
	int offp, i;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	sa = (struct pfsync_state *)(mp->m_data + offp);

	PF_LOCK();
	for (i = 0; i < count; i++) {
		sp = &sa[i];

		bcopy(sp->id, &id_key.id, sizeof(id_key.id));
		id_key.creatorid = sp->creatorid;

		st = pf_find_state_byid(&id_key);
		if (st == NULL) {
			V_pfsyncstats.pfsyncs_badstate++;
			continue;
		}
		SET(st->state_flags, PFSTATE_NOSYNC);
		pf_unlink_state(st, 0);
	}
	PF_UNLOCK();

	return (len);
}

static int
pfsync_in_del_c(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct mbuf *mp;
	struct pfsync_del_c *sa, *sp;
	struct pf_state_cmp id_key;
	struct pf_state *st;
	int len = count * sizeof(*sp);
	int offp, i;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	sa = (struct pfsync_del_c *)(mp->m_data + offp);

	PF_LOCK();
	for (i = 0; i < count; i++) {
		sp = &sa[i];

		bcopy(&sp->id, &id_key.id, sizeof(id_key.id));
		id_key.creatorid = sp->creatorid;

		st = pf_find_state_byid(&id_key);
		if (st == NULL) {
			V_pfsyncstats.pfsyncs_badstate++;
			continue;
		}

		SET(st->state_flags, PFSTATE_NOSYNC);
		pf_unlink_state(st, 0);
	}
	PF_UNLOCK();

	return (len);
}

static int
pfsync_in_bus(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_bus *bus;
	struct mbuf *mp;
	int len = count * sizeof(*bus);
	int offp;

	/* If we're not waiting for a bulk update, who cares. */
	if (sc->sc_ureq_sent == 0)
		return (len);

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	bus = (struct pfsync_bus *)(mp->m_data + offp);

	switch (bus->status) {
	case PFSYNC_BUS_START:
		callout_reset(&sc->sc_bulkfail_tmo, 4 * hz +
		    V_pf_pool_limits[PF_LIMIT_STATES].limit /
		    ((sc->sc_ifp->if_mtu - PFSYNC_MINPKT) /
		    sizeof(struct pfsync_state)),
		    pfsync_bulk_fail, V_pfsyncif);
		if (V_pf_status.debug >= PF_DEBUG_MISC)
			printf("pfsync: received bulk update start\n");
		break;

	case PFSYNC_BUS_END:
		if (time_uptime - ntohl(bus->endtime) >=
		    sc->sc_ureq_sent) {
			/* that's it, we're happy */
			sc->sc_ureq_sent = 0;
			sc->sc_bulk_tries = 0;
			callout_stop(&sc->sc_bulkfail_tmo);
			if (!sc->pfsync_sync_ok && carp_demote_adj_p)
				(*carp_demote_adj_p)(-V_pfsync_carp_adj,
				    "pfsync bulk done");
			sc->pfsync_sync_ok = 1;
			if (V_pf_status.debug >= PF_DEBUG_MISC)
				printf("pfsync: received valid "
				    "bulk update end\n");
		} else {
			if (V_pf_status.debug >= PF_DEBUG_MISC)
				printf("pfsync: received invalid "
				    "bulk update end: bad timestamp\n");
		}
		break;
	}

	return (len);
}

static int
pfsync_in_tdb(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	int len = count * sizeof(struct pfsync_tdb);

#if defined(IPSEC)
	struct pfsync_tdb *tp;
	struct mbuf *mp;
	int offp;
	int i;
	int s;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	tp = (struct pfsync_tdb *)(mp->m_data + offp);

	PF_LOCK();
	for (i = 0; i < count; i++)
		pfsync_update_net_tdb(&tp[i]);
	PF_UNLOCK();
#endif

	return (len);
}

#if defined(IPSEC)
/* Update an in-kernel tdb. Silently fail if no tdb is found. */
static void
pfsync_update_net_tdb(struct pfsync_tdb *pt)
{
	struct tdb		*tdb;
	int			 s;

	/* check for invalid values */
	if (ntohl(pt->spi) <= SPI_RESERVED_MAX ||
	    (pt->dst.sa.sa_family != AF_INET &&
	     pt->dst.sa.sa_family != AF_INET6))
		goto bad;

	tdb = gettdb(pt->spi, &pt->dst, pt->sproto);
	if (tdb) {
		pt->rpl = ntohl(pt->rpl);
		pt->cur_bytes = (unsigned long long)be64toh(pt->cur_bytes);

		/* Neither replay nor byte counter should ever decrease. */
		if (pt->rpl < tdb->tdb_rpl ||
		    pt->cur_bytes < tdb->tdb_cur_bytes) {
			goto bad;
		}

		tdb->tdb_rpl = pt->rpl;
		tdb->tdb_cur_bytes = pt->cur_bytes;
	}
	return;

bad:
	if (V_pf_status.debug >= PF_DEBUG_MISC)
		printf("pfsync_insert: PFSYNC_ACT_TDB_UPD: "
		    "invalid value\n");
	V_pfsyncstats.pfsyncs_badstate++;
	return;
}
#endif


static int
pfsync_in_eof(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	/* check if we are at the right place in the packet */
	if (offset != m->m_pkthdr.len - sizeof(struct pfsync_eof))
		V_pfsyncstats.pfsyncs_badact++;

	/* we're done. free and let the caller return */
	m_freem(m);
	return (-1);
}

static int
pfsync_in_error(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	V_pfsyncstats.pfsyncs_badact++;

	m_freem(m);
	return (-1);
}

static int
pfsyncoutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct route *rt)
{
	m_freem(m);
	return (0);
}

/* ARGSUSED */
static int
pfsyncioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct pfsync_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ip_moptions *imo = &sc->sc_imo;
	struct pfsyncreq pfsyncr;
	struct ifnet    *sifp;
	struct ip *ip;
	int error;

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ifp->if_drv_flags |= IFF_DRV_RUNNING;
		else
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		break;
	case SIOCSIFMTU:
		if (!sc->sc_sync_if ||
		    ifr->ifr_mtu <= PFSYNC_MINPKT ||
		    ifr->ifr_mtu > sc->sc_sync_if->if_mtu)
			return (EINVAL);
		if (ifr->ifr_mtu < ifp->if_mtu) {
			PF_LOCK();
			pfsync_sendout();
			PF_UNLOCK();
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCGETPFSYNC:
		bzero(&pfsyncr, sizeof(pfsyncr));
		if (sc->sc_sync_if) {
			strlcpy(pfsyncr.pfsyncr_syncdev,
			    sc->sc_sync_if->if_xname, IFNAMSIZ);
		}
		pfsyncr.pfsyncr_syncpeer = sc->sc_sync_peer;
		pfsyncr.pfsyncr_maxupdates = sc->sc_maxupdates;
		return (copyout(&pfsyncr, ifr->ifr_data, sizeof(pfsyncr)));

	case SIOCSETPFSYNC:
		if ((error = priv_check(curthread, PRIV_NETINET_PF)) != 0)
			return (error);
		if ((error = copyin(ifr->ifr_data, &pfsyncr, sizeof(pfsyncr))))
			return (error);

		PF_LOCK();
		if (pfsyncr.pfsyncr_syncpeer.s_addr == 0)
			sc->sc_sync_peer.s_addr = htonl(INADDR_PFSYNC_GROUP);
		else
			sc->sc_sync_peer.s_addr =
			    pfsyncr.pfsyncr_syncpeer.s_addr;

		if (pfsyncr.pfsyncr_maxupdates > 255)
		{
			PF_UNLOCK();
			return (EINVAL);
		}
		sc->sc_maxupdates = pfsyncr.pfsyncr_maxupdates;

		if (pfsyncr.pfsyncr_syncdev[0] == 0) {
			sc->sc_sync_if = NULL;
			PF_UNLOCK();
			if (imo->imo_membership)
				pfsync_multicast_cleanup(sc);
			break;
		}

		PF_UNLOCK();
		if ((sifp = ifunit(pfsyncr.pfsyncr_syncdev)) == NULL)
			return (EINVAL);

		PF_LOCK();
		if (sifp->if_mtu < sc->sc_ifp->if_mtu ||
		    (sc->sc_sync_if != NULL &&
		    sifp->if_mtu < sc->sc_sync_if->if_mtu) ||
		    sifp->if_mtu < MCLBYTES - sizeof(struct ip))
			pfsync_sendout();
		sc->sc_sync_if = sifp;

		if (imo->imo_membership) {
			PF_UNLOCK();
			pfsync_multicast_cleanup(sc);
			PF_LOCK();
		}

		if (sc->sc_sync_if &&
		    sc->sc_sync_peer.s_addr == htonl(INADDR_PFSYNC_GROUP)) {
			PF_UNLOCK();
			error = pfsync_multicast_setup(sc);
			if (error)
				return (error);
			PF_LOCK();
		}

		ip = &sc->sc_template;
		bzero(ip, sizeof(*ip));
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(sc->sc_template) >> 2;
		ip->ip_tos = IPTOS_LOWDELAY;
		/* len and id are set later */
		ip->ip_off = IP_DF;
		ip->ip_ttl = PFSYNC_DFLTTL;
		ip->ip_p = IPPROTO_PFSYNC;
		ip->ip_src.s_addr = INADDR_ANY;
		ip->ip_dst.s_addr = sc->sc_sync_peer.s_addr;

		if (sc->sc_sync_if) {
			/* Request a full state table update. */
			sc->sc_ureq_sent = time_uptime;
			if (sc->pfsync_sync_ok && carp_demote_adj_p)
				(*carp_demote_adj_p)(V_pfsync_carp_adj,
				    "pfsync bulk start");
			sc->pfsync_sync_ok = 0;
			if (V_pf_status.debug >= PF_DEBUG_MISC)
				printf("pfsync: requesting bulk update\n");
			callout_reset(&sc->sc_bulkfail_tmo, 5 * hz,
			    pfsync_bulk_fail, V_pfsyncif);
			pfsync_request_update(0, 0);
		}
		PF_UNLOCK();

		break;

	default:
		return (ENOTTY);
	}

	return (0);
}

static int
pfsync_out_state(struct pf_state *st, struct mbuf *m, int offset)
{
	struct pfsync_state *sp = (struct pfsync_state *)(m->m_data + offset);

	pfsync_state_export(sp, st);

	return (sizeof(*sp));
}

static int
pfsync_out_iack(struct pf_state *st, struct mbuf *m, int offset)
{
	struct pfsync_ins_ack *iack =
	    (struct pfsync_ins_ack *)(m->m_data + offset);

	iack->id = st->id;
	iack->creatorid = st->creatorid;

	return (sizeof(*iack));
}

static int
pfsync_out_upd_c(struct pf_state *st, struct mbuf *m, int offset)
{
	struct pfsync_upd_c *up = (struct pfsync_upd_c *)(m->m_data + offset);

	up->id = st->id;
	pf_state_peer_hton(&st->src, &up->src);
	pf_state_peer_hton(&st->dst, &up->dst);
	up->creatorid = st->creatorid;

	up->expire = pf_state_expires(st);
	if (up->expire <= time_second)
		up->expire = htonl(0);
	else
		up->expire = htonl(up->expire - time_second);
	up->timeout = st->timeout;

	bzero(up->_pad, sizeof(up->_pad)); /* XXX */

	return (sizeof(*up));
}

static int
pfsync_out_del(struct pf_state *st, struct mbuf *m, int offset)
{
	struct pfsync_del_c *dp = (struct pfsync_del_c *)(m->m_data + offset);

	dp->id = st->id;
	dp->creatorid = st->creatorid;

	SET(st->state_flags, PFSTATE_NOSYNC);

	return (sizeof(*dp));
}

static void
pfsync_drop(struct pfsync_softc *sc)
{
	struct pf_state *st;
	struct pfsync_upd_req_item *ur;
#ifdef notyet
	struct tdb *t;
#endif
	int q;

	for (q = 0; q < PFSYNC_S_COUNT; q++) {
		if (TAILQ_EMPTY(&sc->sc_qs[q]))
			continue;

		TAILQ_FOREACH(st, &sc->sc_qs[q], sync_list) {
#ifdef PFSYNC_DEBUG
			KASSERT(st->sync_state == q,
				("%s: st->sync_state == q",
					__func__));
#endif
			st->sync_state = PFSYNC_S_NONE;
		}
		TAILQ_INIT(&sc->sc_qs[q]);
	}

	while ((ur = TAILQ_FIRST(&sc->sc_upd_req_list)) != NULL) {
		TAILQ_REMOVE(&sc->sc_upd_req_list, ur, ur_entry);
		uma_zfree(sc->sc_pool, ur);
	}

	sc->sc_plus = NULL;

#ifdef notyet
	if (!TAILQ_EMPTY(&sc->sc_tdb_q)) {
		TAILQ_FOREACH(t, &sc->sc_tdb_q, tdb_sync_entry)
			CLR(t->tdb_flags, TDBF_PFSYNC);

		TAILQ_INIT(&sc->sc_tdb_q);
	}
#endif

	sc->sc_len = PFSYNC_MINPKT;
}

static void
pfsync_sendout()
{
	pfsync_sendout1(1);
}

static void
pfsync_sendout1(int schedswi)
{
	struct pfsync_softc *sc = V_pfsyncif;
#if NBPFILTER > 0
	struct ifnet *ifp = sc->sc_ifp;
#endif
	struct mbuf *m;
	struct ip *ip;
	struct pfsync_header *ph;
	struct pfsync_subheader *subh;
	struct pf_state *st;
	struct pfsync_upd_req_item *ur;
#ifdef notyet
	struct tdb *t;
#endif
	int offset;
	int q, count = 0;

	PF_LOCK_ASSERT();

	if (sc == NULL || sc->sc_len == PFSYNC_MINPKT)
		return;

#if NBPFILTER > 0
	if (ifp->if_bpf == NULL && sc->sc_sync_if == NULL) {
#else
	if (sc->sc_sync_if == NULL) {
#endif
		pfsync_drop(sc);
		return;
	}

	m = m_get2(M_NOWAIT, MT_DATA, M_PKTHDR, max_linkhdr + sc->sc_len);
	if (m == NULL) {
		sc->sc_ifp->if_oerrors++;
		V_pfsyncstats.pfsyncs_onomem++;
		return;
	}
	m->m_data += max_linkhdr;
	m->m_len = m->m_pkthdr.len = sc->sc_len;

	/* build the ip header */
	ip = (struct ip *)m->m_data;
	bcopy(&sc->sc_template, ip, sizeof(*ip));
	offset = sizeof(*ip);

	ip->ip_len = m->m_pkthdr.len;
	ip->ip_id = htons(ip_randomid());

	/* build the pfsync header */
	ph = (struct pfsync_header *)(m->m_data + offset);
	bzero(ph, sizeof(*ph));
	offset += sizeof(*ph);

	ph->version = PFSYNC_VERSION;
	ph->len = htons(sc->sc_len - sizeof(*ip));
	bcopy(V_pf_status.pf_chksum, ph->pfcksum, PF_MD5_DIGEST_LENGTH);

	/* walk the queues */
	for (q = 0; q < PFSYNC_S_COUNT; q++) {
		if (TAILQ_EMPTY(&sc->sc_qs[q]))
			continue;

		subh = (struct pfsync_subheader *)(m->m_data + offset);
		offset += sizeof(*subh);

		count = 0;
		TAILQ_FOREACH(st, &sc->sc_qs[q], sync_list) {
#ifdef PFSYNC_DEBUG
			KASSERT(st->sync_state == q,
				("%s: st->sync_state == q",
					__func__));
#endif

			offset += pfsync_qs[q].write(st, m, offset);
			st->sync_state = PFSYNC_S_NONE;
			count++;
		}
		TAILQ_INIT(&sc->sc_qs[q]);

		bzero(subh, sizeof(*subh));
		subh->action = pfsync_qs[q].action;
		subh->count = htons(count);
	}

	if (!TAILQ_EMPTY(&sc->sc_upd_req_list)) {
		subh = (struct pfsync_subheader *)(m->m_data + offset);
		offset += sizeof(*subh);

		count = 0;
		while ((ur = TAILQ_FIRST(&sc->sc_upd_req_list)) != NULL) {
			TAILQ_REMOVE(&sc->sc_upd_req_list, ur, ur_entry);

			bcopy(&ur->ur_msg, m->m_data + offset,
			    sizeof(ur->ur_msg));
			offset += sizeof(ur->ur_msg);

			uma_zfree(sc->sc_pool, ur);

			count++;
		}

		bzero(subh, sizeof(*subh));
		subh->action = PFSYNC_ACT_UPD_REQ;
		subh->count = htons(count);
	}

	/* has someone built a custom region for us to add? */
	if (sc->sc_plus != NULL) {
		bcopy(sc->sc_plus, m->m_data + offset, sc->sc_pluslen);
		offset += sc->sc_pluslen;

		sc->sc_plus = NULL;
	}

#ifdef notyet
	if (!TAILQ_EMPTY(&sc->sc_tdb_q)) {
		subh = (struct pfsync_subheader *)(m->m_data + offset);
		offset += sizeof(*subh);

		count = 0;
		TAILQ_FOREACH(t, &sc->sc_tdb_q, tdb_sync_entry) {
			offset += pfsync_out_tdb(t, m, offset);
			CLR(t->tdb_flags, TDBF_PFSYNC);

			count++;
		}
		TAILQ_INIT(&sc->sc_tdb_q);

		bzero(subh, sizeof(*subh));
		subh->action = PFSYNC_ACT_TDB;
		subh->count = htons(count);
	}
#endif

	subh = (struct pfsync_subheader *)(m->m_data + offset);
	offset += sizeof(*subh);

	bzero(subh, sizeof(*subh));
	subh->action = PFSYNC_ACT_EOF;
	subh->count = htons(1);

	/* XXX write checksum in EOF here */

	/* we're done, let's put it on the wire */
#if NBPFILTER > 0
	if (ifp->if_bpf) {
		m->m_data += sizeof(*ip);
		m->m_len = m->m_pkthdr.len = sc->sc_len - sizeof(*ip);
		BPF_MTAP(ifp, m);
		m->m_data -= sizeof(*ip);
		m->m_len = m->m_pkthdr.len = sc->sc_len;
	}

	if (sc->sc_sync_if == NULL) {
		sc->sc_len = PFSYNC_MINPKT;
		m_freem(m);
		return;
	}
#endif

	sc->sc_ifp->if_opackets++;
	sc->sc_ifp->if_obytes += m->m_pkthdr.len;
	sc->sc_len = PFSYNC_MINPKT;

	if (!_IF_QFULL(&sc->sc_ifp->if_snd))
		_IF_ENQUEUE(&sc->sc_ifp->if_snd, m);
	else {
		m_freem(m);
                sc->sc_ifp->if_snd.ifq_drops++;
	}
	if (schedswi)
		swi_sched(V_pfsync_swi_cookie, 0);
}

static void
pfsync_insert_state(struct pf_state *st)
{
	struct pfsync_softc *sc = V_pfsyncif;

	PF_LOCK_ASSERT();

	if (ISSET(st->rule.ptr->rule_flag, PFRULE_NOSYNC) ||
	    st->key[PF_SK_WIRE]->proto == IPPROTO_PFSYNC) {
		SET(st->state_flags, PFSTATE_NOSYNC);
		return;
	}

	if (sc == NULL || ISSET(st->state_flags, PFSTATE_NOSYNC))
		return;

#ifdef PFSYNC_DEBUG
	KASSERT(st->sync_state == PFSYNC_S_NONE,
		("%s: st->sync_state == PFSYNC_S_NONE", __func__));
#endif

	if (sc->sc_len == PFSYNC_MINPKT)
		callout_reset(&sc->sc_tmo, 1 * hz, pfsync_timeout,
		    V_pfsyncif);

	pfsync_q_ins(st, PFSYNC_S_INS);

	if (ISSET(st->state_flags, PFSTATE_ACK))
		schednetisr(NETISR_PFSYNC);
	else
		st->sync_updates = 0;
}

static int defer = 10;

static int
pfsync_defer(struct pf_state *st, struct mbuf *m)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_deferral *pd;

	PF_LOCK_ASSERT();

	if (sc->sc_deferred >= 128)
		pfsync_undefer(TAILQ_FIRST(&sc->sc_deferrals), 0);

	pd = uma_zalloc(sc->sc_pool, M_NOWAIT);
	if (pd == NULL)
		return (0);
	sc->sc_deferred++;

	m->m_flags |= M_SKIP_FIREWALL;
	SET(st->state_flags, PFSTATE_ACK);

	pd->pd_st = st;
	pd->pd_m = m;

	TAILQ_INSERT_TAIL(&sc->sc_deferrals, pd, pd_entry);
	callout_init(&pd->pd_tmo, CALLOUT_MPSAFE);
	callout_reset(&pd->pd_tmo, defer, pfsync_defer_tmo,
		pd);

	return (1);
}

static void
pfsync_undefer(struct pfsync_deferral *pd, int drop)
{
	struct pfsync_softc *sc = V_pfsyncif;

	PF_LOCK_ASSERT();

	TAILQ_REMOVE(&sc->sc_deferrals, pd, pd_entry);
	sc->sc_deferred--;

	CLR(pd->pd_st->state_flags, PFSTATE_ACK);
	callout_stop(&pd->pd_tmo); /* bah */
	if (drop)
		m_freem(pd->pd_m);
	else {
		/* XXX: use pf_defered?! */
		PF_UNLOCK();
		ip_output(pd->pd_m, (void *)NULL, (void *)NULL, 0,
		    (void *)NULL, (void *)NULL);
		PF_LOCK();
	}

	uma_zfree(sc->sc_pool, pd);
}

static void
pfsync_defer_tmo(void *arg)
{
#ifdef VIMAGE
	struct pfsync_deferral *pd = arg;
#endif

	CURVNET_SET(pd->pd_m->m_pkthdr.rcvif->if_vnet); /* XXX */
	PF_LOCK();
	pfsync_undefer(arg, 0);
	PF_UNLOCK();
	CURVNET_RESTORE();
}

static void
pfsync_deferred(struct pf_state *st, int drop)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_deferral *pd;

	TAILQ_FOREACH(pd, &sc->sc_deferrals, pd_entry) {
		 if (pd->pd_st == st) {
			pfsync_undefer(pd, drop);
			return;
		}
	}

	panic("pfsync_send_deferred: unable to find deferred state");
}

static u_int pfsync_upds = 0;

static void
pfsync_update_state(struct pf_state *st)
{
	struct pfsync_softc *sc = V_pfsyncif;
	int sync = 0;

	PF_LOCK_ASSERT();

	if (sc == NULL)
		return;

	if (ISSET(st->state_flags, PFSTATE_ACK))
		pfsync_deferred(st, 0);
	if (ISSET(st->state_flags, PFSTATE_NOSYNC)) {
		if (st->sync_state != PFSYNC_S_NONE)
			pfsync_q_del(st);
		return;
	}

	if (sc->sc_len == PFSYNC_MINPKT)
		callout_reset(&sc->sc_tmo, 1 * hz, pfsync_timeout,
		    V_pfsyncif);

	switch (st->sync_state) {
	case PFSYNC_S_UPD_C:
	case PFSYNC_S_UPD:
	case PFSYNC_S_INS:
		/* we're already handling it */

		if (st->key[PF_SK_WIRE]->proto == IPPROTO_TCP) {
			st->sync_updates++;
			if (st->sync_updates >= sc->sc_maxupdates)
				sync = 1;
		}
		break;

	case PFSYNC_S_IACK:
		pfsync_q_del(st);
	case PFSYNC_S_NONE:
		pfsync_q_ins(st, PFSYNC_S_UPD_C);
		st->sync_updates = 0;
		break;

	default:
		panic("pfsync_update_state: unexpected sync state %d",
		    st->sync_state);
	}

	if (sync || (time_uptime - st->pfsync_time) < 2) {
		pfsync_upds++;
		schednetisr(NETISR_PFSYNC);
	}
}

static void
pfsync_request_update(u_int32_t creatorid, u_int64_t id)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_upd_req_item *item;
	size_t nlen = sizeof(struct pfsync_upd_req);

	PF_LOCK_ASSERT();

	/*
	 * this code does nothing to prevent multiple update requests for the
	 * same state being generated.
	 */

	item = uma_zalloc(sc->sc_pool, M_NOWAIT);
	if (item == NULL) {
		/* XXX stats */
		return;
	}

	item->ur_msg.id = id;
	item->ur_msg.creatorid = creatorid;

	if (TAILQ_EMPTY(&sc->sc_upd_req_list))
		nlen += sizeof(struct pfsync_subheader);

	if (sc->sc_len + nlen > sc->sc_ifp->if_mtu) {
		pfsync_sendout();

		nlen = sizeof(struct pfsync_subheader) +
		    sizeof(struct pfsync_upd_req);
	}

	TAILQ_INSERT_TAIL(&sc->sc_upd_req_list, item, ur_entry);
	sc->sc_len += nlen;

	schednetisr(NETISR_PFSYNC);
}

static void
pfsync_update_state_req(struct pf_state *st)
{
	struct pfsync_softc *sc = V_pfsyncif;

	PF_LOCK_ASSERT();

	KASSERT(sc != NULL, ("%s: nonexistent instance", __func__));

	if (ISSET(st->state_flags, PFSTATE_NOSYNC)) {
		if (st->sync_state != PFSYNC_S_NONE)
			pfsync_q_del(st);
		return;
	}

	switch (st->sync_state) {
	case PFSYNC_S_UPD_C:
	case PFSYNC_S_IACK:
		pfsync_q_del(st);
	case PFSYNC_S_NONE:
		pfsync_q_ins(st, PFSYNC_S_UPD);
		schednetisr(NETISR_PFSYNC);
		return;

	case PFSYNC_S_INS:
	case PFSYNC_S_UPD:
	case PFSYNC_S_DEL:
		/* we're already handling it */
		return;

	default:
		panic("pfsync_update_state_req: unexpected sync state %d",
		    st->sync_state);
	}
}

static void
pfsync_delete_state(struct pf_state *st)
{
	struct pfsync_softc *sc = V_pfsyncif;

	PF_LOCK_ASSERT();

	if (sc == NULL)
		return;

	if (ISSET(st->state_flags, PFSTATE_ACK))
		pfsync_deferred(st, 1);
	if (ISSET(st->state_flags, PFSTATE_NOSYNC)) {
		if (st->sync_state != PFSYNC_S_NONE)
			pfsync_q_del(st);
		return;
	}

	if (sc->sc_len == PFSYNC_MINPKT)
		callout_reset(&sc->sc_tmo, 1 * hz, pfsync_timeout,
		    V_pfsyncif);

	switch (st->sync_state) {
	case PFSYNC_S_INS:
		/* we never got to tell the world so just forget about it */
		pfsync_q_del(st);
		return;

	case PFSYNC_S_UPD_C:
	case PFSYNC_S_UPD:
	case PFSYNC_S_IACK:
		pfsync_q_del(st);
		/* FALLTHROUGH to putting it on the del list */

	case PFSYNC_S_NONE:
		pfsync_q_ins(st, PFSYNC_S_DEL);
		return;

	default:
		panic("pfsync_delete_state: unexpected sync state %d",
		    st->sync_state);
	}
}

static void
pfsync_clear_states(u_int32_t creatorid, const char *ifname)
{
	struct {
		struct pfsync_subheader subh;
		struct pfsync_clr clr;
	} __packed r;

	struct pfsync_softc *sc = V_pfsyncif;

	PF_LOCK_ASSERT();

	if (sc == NULL)
		return;

	bzero(&r, sizeof(r));

	r.subh.action = PFSYNC_ACT_CLR;
	r.subh.count = htons(1);

	strlcpy(r.clr.ifname, ifname, sizeof(r.clr.ifname));
	r.clr.creatorid = creatorid;

	pfsync_send_plus(&r, sizeof(r));
}

static void
pfsync_q_ins(struct pf_state *st, int q)
{
	struct pfsync_softc *sc = V_pfsyncif;
	size_t nlen = pfsync_qs[q].len;

	PF_LOCK_ASSERT();

	KASSERT(st->sync_state == PFSYNC_S_NONE,
		("%s: st->sync_state == PFSYNC_S_NONE", __func__));
	KASSERT(sc->sc_len >= PFSYNC_MINPKT, ("pfsync pkt len is too low %zu",
	    sc->sc_len));

	if (TAILQ_EMPTY(&sc->sc_qs[q]))
		nlen += sizeof(struct pfsync_subheader);

	if (sc->sc_len + nlen > sc->sc_ifp->if_mtu) {
		pfsync_sendout();

		nlen = sizeof(struct pfsync_subheader) + pfsync_qs[q].len;
	}

	sc->sc_len += nlen;
	TAILQ_INSERT_TAIL(&sc->sc_qs[q], st, sync_list);
	st->sync_state = q;
}

static void
pfsync_q_del(struct pf_state *st)
{
	struct pfsync_softc *sc = V_pfsyncif;
	int q = st->sync_state;

	KASSERT(st->sync_state != PFSYNC_S_NONE,
		("%s: st->sync_state != PFSYNC_S_NONE", __func__));

	sc->sc_len -= pfsync_qs[q].len;
	TAILQ_REMOVE(&sc->sc_qs[q], st, sync_list);
	st->sync_state = PFSYNC_S_NONE;

	if (TAILQ_EMPTY(&sc->sc_qs[q]))
		sc->sc_len -= sizeof(struct pfsync_subheader);
}

#ifdef notyet
static void
pfsync_update_tdb(struct tdb *t, int output)
{
	struct pfsync_softc *sc = V_pfsyncif;
	size_t nlen = sizeof(struct pfsync_tdb);
	int s;

	if (sc == NULL)
		return;

	if (!ISSET(t->tdb_flags, TDBF_PFSYNC)) {
		if (TAILQ_EMPTY(&sc->sc_tdb_q))
			nlen += sizeof(struct pfsync_subheader);

		if (sc->sc_len + nlen > sc->sc_if.if_mtu) {
			PF_LOCK();
			pfsync_sendout();
			PF_UNLOCK();

			nlen = sizeof(struct pfsync_subheader) +
			    sizeof(struct pfsync_tdb);
		}

		sc->sc_len += nlen;
		TAILQ_INSERT_TAIL(&sc->sc_tdb_q, t, tdb_sync_entry);
		SET(t->tdb_flags, TDBF_PFSYNC);
		t->tdb_updates = 0;
	} else {
		if (++t->tdb_updates >= sc->sc_maxupdates)
			schednetisr(NETISR_PFSYNC);
	}

	if (output)
		SET(t->tdb_flags, TDBF_PFSYNC_RPL);
	else
		CLR(t->tdb_flags, TDBF_PFSYNC_RPL);
}

static void
pfsync_delete_tdb(struct tdb *t)
{
	struct pfsync_softc *sc = V_pfsyncif;

	if (sc == NULL || !ISSET(t->tdb_flags, TDBF_PFSYNC))
		return;

	sc->sc_len -= sizeof(struct pfsync_tdb);
	TAILQ_REMOVE(&sc->sc_tdb_q, t, tdb_sync_entry);
	CLR(t->tdb_flags, TDBF_PFSYNC);

	if (TAILQ_EMPTY(&sc->sc_tdb_q))
		sc->sc_len -= sizeof(struct pfsync_subheader);
}

static int
pfsync_out_tdb(struct tdb *t, struct mbuf *m, int offset)
{
	struct pfsync_tdb *ut = (struct pfsync_tdb *)(m->m_data + offset);

	bzero(ut, sizeof(*ut));
	ut->spi = t->tdb_spi;
	bcopy(&t->tdb_dst, &ut->dst, sizeof(ut->dst));
	/*
	 * When a failover happens, the master's rpl is probably above
	 * what we see here (we may be up to a second late), so
	 * increase it a bit for outbound tdbs to manage most such
	 * situations.
	 *
	 * For now, just add an offset that is likely to be larger
	 * than the number of packets we can see in one second. The RFC
	 * just says the next packet must have a higher seq value.
	 *
	 * XXX What is a good algorithm for this? We could use
	 * a rate-determined increase, but to know it, we would have
	 * to extend struct tdb.
	 * XXX pt->rpl can wrap over MAXINT, but if so the real tdb
	 * will soon be replaced anyway. For now, just don't handle
	 * this edge case.
	 */
#define RPL_INCR 16384
	ut->rpl = htonl(t->tdb_rpl + (ISSET(t->tdb_flags, TDBF_PFSYNC_RPL) ?
	    RPL_INCR : 0));
	ut->cur_bytes = htobe64(t->tdb_cur_bytes);
	ut->sproto = t->tdb_sproto;

	return (sizeof(*ut));
}
#endif

static void
pfsync_bulk_start(void)
{
	struct pfsync_softc *sc = V_pfsyncif;

	if (V_pf_status.debug >= PF_DEBUG_MISC)
		printf("pfsync: received bulk update request\n");

	PF_LOCK_ASSERT();
	if (TAILQ_EMPTY(&V_state_list))
		pfsync_bulk_status(PFSYNC_BUS_END);
	else {
		sc->sc_ureq_received = time_uptime;
		if (sc->sc_bulk_next == NULL)
			sc->sc_bulk_next = TAILQ_FIRST(&V_state_list);
		sc->sc_bulk_last = sc->sc_bulk_next;

		pfsync_bulk_status(PFSYNC_BUS_START);
		callout_reset(&sc->sc_bulk_tmo, 1, pfsync_bulk_update, sc);
	}
}

static void
pfsync_bulk_update(void *arg)
{
	struct pfsync_softc *sc = arg;
	struct pf_state *st = sc->sc_bulk_next;
	int i = 0;

	PF_LOCK_ASSERT();

	CURVNET_SET(sc->sc_ifp->if_vnet);
	for (;;) {
		if (st->sync_state == PFSYNC_S_NONE &&
		    st->timeout < PFTM_MAX &&
		    st->pfsync_time <= sc->sc_ureq_received) {
			pfsync_update_state_req(st);
			i++;
		}

		PF_LIST_RLOCK();
		st = TAILQ_NEXT(st, entry_list);
		if (st == NULL)
			st = TAILQ_FIRST(&V_state_list);
		PF_LIST_RUNLOCK();

		if (st == sc->sc_bulk_last) {
			/* we're done */
			sc->sc_bulk_next = NULL;
			sc->sc_bulk_last = NULL;
			pfsync_bulk_status(PFSYNC_BUS_END);
			break;
		}

		if (i > 1 && (sc->sc_ifp->if_mtu - sc->sc_len) <
		    sizeof(struct pfsync_state)) {
			/* we've filled a packet */
			sc->sc_bulk_next = st;
			callout_reset(&sc->sc_bulk_tmo, 1,
			    pfsync_bulk_update, sc);
			break;
		}
	}

	CURVNET_RESTORE();
}

static void
pfsync_bulk_status(u_int8_t status)
{
	struct {
		struct pfsync_subheader subh;
		struct pfsync_bus bus;
	} __packed r;

	struct pfsync_softc *sc = V_pfsyncif;

	PF_LOCK_ASSERT();

	bzero(&r, sizeof(r));

	r.subh.action = PFSYNC_ACT_BUS;
	r.subh.count = htons(1);

	r.bus.creatorid = V_pf_status.hostid;
	r.bus.endtime = htonl(time_uptime - sc->sc_ureq_received);
	r.bus.status = status;

	pfsync_send_plus(&r, sizeof(r));
}

static void
pfsync_bulk_fail(void *arg)
{
	struct pfsync_softc *sc = arg;

	CURVNET_SET(sc->sc_ifp->if_vnet);

	if (sc->sc_bulk_tries++ < PFSYNC_MAX_BULKTRIES) {
		/* Try again */
		callout_reset(&sc->sc_bulkfail_tmo, 5 * hz,
		    pfsync_bulk_fail, V_pfsyncif);
		PF_LOCK();
		pfsync_request_update(0, 0);
		PF_UNLOCK();
	} else {
		/* Pretend like the transfer was ok */
		sc->sc_ureq_sent = 0;
		sc->sc_bulk_tries = 0;
		if (!sc->pfsync_sync_ok && carp_demote_adj_p)
			(*carp_demote_adj_p)(-V_pfsync_carp_adj,
			    "pfsync bulk fail");
		sc->pfsync_sync_ok = 1;
		if (V_pf_status.debug >= PF_DEBUG_MISC)
			printf("pfsync: failed to receive bulk update\n");
	}

	CURVNET_RESTORE();
}

static void
pfsync_send_plus(void *plus, size_t pluslen)
{
	struct pfsync_softc *sc = V_pfsyncif;

	PF_LOCK_ASSERT();

	if (sc->sc_len + pluslen > sc->sc_ifp->if_mtu) {
		pfsync_sendout();
	}

	sc->sc_plus = plus;
	sc->sc_len += (sc->sc_pluslen = pluslen);

	pfsync_sendout();
}

static int
pfsync_up(void)
{
	struct pfsync_softc *sc = V_pfsyncif;

	if (sc == NULL || !ISSET(sc->sc_ifp->if_flags, IFF_DRV_RUNNING))
		return (0);

	return (1);
}

static int
pfsync_state_in_use(struct pf_state *st)
{
	struct pfsync_softc *sc = V_pfsyncif;

	if (sc == NULL)
		return (0);

	if (st->sync_state != PFSYNC_S_NONE ||
	    st == sc->sc_bulk_next ||
	    st == sc->sc_bulk_last)
		return (1);

	return (0);
}

static u_int pfsync_ints;
static u_int pfsync_tmos;

static void
pfsync_timeout(void *arg)
{
#ifdef VIMAGE
	struct pfsync_softc *sc = arg;
#endif

	CURVNET_SET(sc->sc_ifp->if_vnet);

	pfsync_tmos++;

	PF_LOCK();
	pfsync_sendout();
	PF_UNLOCK();

	CURVNET_RESTORE();
}

/* this is a softnet/netisr handler */
static void
pfsyncintr(void *arg)
{
	struct pfsync_softc *sc = arg;
	struct mbuf *m, *n;

	CURVNET_SET(sc->sc_ifp->if_vnet);
	pfsync_ints++;

	PF_LOCK();
	if (sc->sc_len > PFSYNC_MINPKT)
		pfsync_sendout1(0);
	_IF_DEQUEUE_ALL(&sc->sc_ifp->if_snd, m);
	PF_UNLOCK();

	for (; m != NULL; m = n) {

		n = m->m_nextpkt;
		m->m_nextpkt = NULL;
		if (ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo, NULL)
		    == 0)
			V_pfsyncstats.pfsyncs_opackets++;
		else
			V_pfsyncstats.pfsyncs_oerrors++;
	}
	CURVNET_RESTORE();
}

#ifdef notyet
static int
pfsync_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case PFSYNCCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &V_pfsyncstats, sizeof(V_pfsyncstats)));
	}
	return (ENOPROTOOPT);
}
#endif

static int
pfsync_multicast_setup(struct pfsync_softc *sc)
{
	struct ip_moptions *imo = &sc->sc_imo;
	int error;

	if (!(sc->sc_sync_if->if_flags & IFF_MULTICAST)) {
		sc->sc_sync_if = NULL;
		return (EADDRNOTAVAIL);
	}

	imo->imo_membership = (struct in_multi **)malloc(
	    (sizeof(struct in_multi *) * IP_MIN_MEMBERSHIPS), M_PFSYNC,
	    M_WAITOK | M_ZERO);
	imo->imo_max_memberships = IP_MIN_MEMBERSHIPS;
	imo->imo_multicast_vif = -1;

	if ((error = in_joingroup(sc->sc_sync_if, &sc->sc_sync_peer, NULL,
	    &imo->imo_membership[0])) != 0) {
		free(imo->imo_membership, M_PFSYNC);
		return (error);
	}
	imo->imo_num_memberships++;
	imo->imo_multicast_ifp = sc->sc_sync_if;
	imo->imo_multicast_ttl = PFSYNC_DFLTTL;
	imo->imo_multicast_loop = 0;

	return (0);
}

static void
pfsync_multicast_cleanup(struct pfsync_softc *sc)
{
	struct ip_moptions *imo = &sc->sc_imo;

	in_leavegroup(imo->imo_membership[0], NULL);
	free(imo->imo_membership, M_PFSYNC);
	imo->imo_membership = NULL;
	imo->imo_multicast_ifp = NULL;
}

#ifdef INET
extern  struct domain inetdomain;
static struct protosw in_pfsync_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_PFSYNC,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		pfsync_input,
	.pr_output =		(pr_output_t *)rip_output,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
};
#endif

static int
pfsync_init()
{
	VNET_ITERATOR_DECL(vnet_iter);
	int error = 0;

	VNET_LIST_RLOCK();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		V_pfsync_cloner = pfsync_cloner;
		V_pfsync_cloner_data = pfsync_cloner_data;
		V_pfsync_cloner.ifc_data = &V_pfsync_cloner_data;
		if_clone_attach(&V_pfsync_cloner);
		error = swi_add(NULL, "pfsync", pfsyncintr, V_pfsyncif,
		    SWI_NET, INTR_MPSAFE, &V_pfsync_swi_cookie);
		CURVNET_RESTORE();
		if (error)
			goto fail_locked;
	}
	VNET_LIST_RUNLOCK();
#ifdef INET
	error = pf_proto_register(PF_INET, &in_pfsync_protosw);
	if (error)
		goto fail;
	error = ipproto_register(IPPROTO_PFSYNC);
	if (error) {
		pf_proto_unregister(PF_INET, IPPROTO_PFSYNC, SOCK_RAW);
		goto fail;
	}
#endif
	PF_LOCK();
	pfsync_state_import_ptr = pfsync_state_import;
	pfsync_up_ptr = pfsync_up;
	pfsync_insert_state_ptr = pfsync_insert_state;
	pfsync_update_state_ptr = pfsync_update_state;
	pfsync_delete_state_ptr = pfsync_delete_state;
	pfsync_clear_states_ptr = pfsync_clear_states;
	pfsync_state_in_use_ptr = pfsync_state_in_use;
	pfsync_defer_ptr = pfsync_defer;
	PF_UNLOCK();

	return (0);

fail:
	VNET_LIST_RLOCK();
fail_locked:
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		if (V_pfsync_swi_cookie) {
			swi_remove(V_pfsync_swi_cookie);
			if_clone_detach(&V_pfsync_cloner);
		}
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK();

	return (error);
}

static void
pfsync_uninit()
{
	VNET_ITERATOR_DECL(vnet_iter);

	PF_LOCK();
	pfsync_state_import_ptr = NULL;
	pfsync_up_ptr = NULL;
	pfsync_insert_state_ptr = NULL;
	pfsync_update_state_ptr = NULL;
	pfsync_delete_state_ptr = NULL;
	pfsync_clear_states_ptr = NULL;
	pfsync_state_in_use_ptr = NULL;
	pfsync_defer_ptr = NULL;
	PF_UNLOCK();

	ipproto_unregister(IPPROTO_PFSYNC);
	pf_proto_unregister(PF_INET, IPPROTO_PFSYNC, SOCK_RAW);
	VNET_LIST_RLOCK();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		swi_remove(V_pfsync_swi_cookie);
		if_clone_detach(&V_pfsync_cloner);
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK();
}

static int
pfsync_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		error = pfsync_init();
		break;
	case MOD_QUIESCE:
		/*
		 * Module should not be unloaded due to race conditions.
		 */
		error = EPERM;
		break;
	case MOD_UNLOAD:
		pfsync_uninit();
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static moduledata_t pfsync_mod = {
	"pfsync",
	pfsync_modevent,
	0
};

#define PFSYNC_MODVER 1

DECLARE_MODULE(pfsync, pfsync_mod, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY);
MODULE_VERSION(pfsync, PFSYNC_MODVER);
MODULE_DEPEND(pfsync, pf, PF_MODVER, PF_MODVER, PF_MODVER);
