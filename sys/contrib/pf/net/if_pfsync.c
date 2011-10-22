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

#ifdef __FreeBSD__
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_bpf.h"
#include "opt_pf.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef DEV_BPF
#define	NBPFILTER	DEV_BPF
#else
#define	NBPFILTER	0
#endif

#ifdef DEV_PFSYNC
#define	NPFSYNC		DEV_PFSYNC
#else
#define	NPFSYNC		0
#endif

#ifdef DEV_CARP
#define	NCARP		DEV_CARP
#else
#define	NCARP		0
#endif
#endif /* __FreeBSD__ */

#include <sys/param.h>
#include <sys/kernel.h>
#ifdef __FreeBSD__
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/priv.h>
#endif
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#ifdef __FreeBSD__
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sockio.h>
#include <sys/taskqueue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#else
#include <sys/ioctl.h>
#include <sys/timeout.h>
#endif
#include <sys/sysctl.h>
#ifndef __FreeBSD__
#include <sys/pool.h>
#endif

#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_clone.h>
#endif
#include <net/if_types.h>
#include <net/route.h>
#include <net/bpf.h>
#include <net/netisr.h>
#ifdef __FreeBSD__
#include <net/vnet.h>
#endif

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

#ifndef __FreeBSD__
#include "carp.h"
#endif
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#include <net/pfvar.h>
#include <net/if_pfsync.h>

#ifndef __FreeBSD__
#include "bpfilter.h"
#include "pfsync.h"
#endif

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

int	pfsync_input_hmac(struct mbuf *, int);

int	pfsync_upd_tcp(struct pf_state *, struct pfsync_state_peer *,
	    struct pfsync_state_peer *);

int	pfsync_in_clr(struct pfsync_pkt *, struct mbuf *, int, int);
int	pfsync_in_ins(struct pfsync_pkt *, struct mbuf *, int, int);
int	pfsync_in_iack(struct pfsync_pkt *, struct mbuf *, int, int);
int	pfsync_in_upd(struct pfsync_pkt *, struct mbuf *, int, int);
int	pfsync_in_upd_c(struct pfsync_pkt *, struct mbuf *, int, int);
int	pfsync_in_ureq(struct pfsync_pkt *, struct mbuf *, int, int);
int	pfsync_in_del(struct pfsync_pkt *, struct mbuf *, int, int);
int	pfsync_in_del_c(struct pfsync_pkt *, struct mbuf *, int, int);
int	pfsync_in_bus(struct pfsync_pkt *, struct mbuf *, int, int);
int	pfsync_in_tdb(struct pfsync_pkt *, struct mbuf *, int, int);
int	pfsync_in_eof(struct pfsync_pkt *, struct mbuf *, int, int);

int	pfsync_in_error(struct pfsync_pkt *, struct mbuf *, int, int);

int	(*pfsync_acts[])(struct pfsync_pkt *, struct mbuf *, int, int) = {
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
int	pfsync_out_state(struct pf_state *, struct mbuf *, int);
int	pfsync_out_iack(struct pf_state *, struct mbuf *, int);
int	pfsync_out_upd_c(struct pf_state *, struct mbuf *, int);
int	pfsync_out_del(struct pf_state *, struct mbuf *, int);

struct pfsync_q pfsync_qs[] = {
	{ pfsync_out_state, sizeof(struct pfsync_state),   PFSYNC_ACT_INS },
	{ pfsync_out_iack,  sizeof(struct pfsync_ins_ack), PFSYNC_ACT_INS_ACK },
	{ pfsync_out_state, sizeof(struct pfsync_state),   PFSYNC_ACT_UPD },
	{ pfsync_out_upd_c, sizeof(struct pfsync_upd_c),   PFSYNC_ACT_UPD_C },
	{ pfsync_out_del,   sizeof(struct pfsync_del_c),   PFSYNC_ACT_DEL_C }
};

void	pfsync_q_ins(struct pf_state *, int);
void	pfsync_q_del(struct pf_state *);

struct pfsync_upd_req_item {
	TAILQ_ENTRY(pfsync_upd_req_item)	ur_entry;
	struct pfsync_upd_req			ur_msg;
};
TAILQ_HEAD(pfsync_upd_reqs, pfsync_upd_req_item);

struct pfsync_deferral {
	TAILQ_ENTRY(pfsync_deferral)		 pd_entry;
	struct pf_state				*pd_st;
	struct mbuf				*pd_m;
#ifdef __FreeBSD__
	struct callout				 pd_tmo;
#else
	struct timeout				 pd_tmo;
#endif
};
TAILQ_HEAD(pfsync_deferrals, pfsync_deferral);

#define PFSYNC_PLSIZE	MAX(sizeof(struct pfsync_upd_req_item), \
			    sizeof(struct pfsync_deferral))

#ifdef notyet
int	pfsync_out_tdb(struct tdb *, struct mbuf *, int);
#endif

struct pfsync_softc {
#ifdef __FreeBSD__
	struct ifnet		*sc_ifp;
#else
	struct ifnet		 sc_if;
#endif
	struct ifnet		*sc_sync_if;

#ifdef __FreeBSD__
	uma_zone_t		 sc_pool;
#else
	struct pool		 sc_pool;
#endif

	struct ip_moptions	 sc_imo;

	struct in_addr		 sc_sync_peer;
	u_int8_t		 sc_maxupdates;
#ifdef __FreeBSD__
	int			 pfsync_sync_ok;
#endif

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
#ifdef __FreeBSD__
	struct callout		 sc_bulkfail_tmo;
#else
	struct timeout		 sc_bulkfail_tmo;
#endif

	u_int32_t		 sc_ureq_received;
	struct pf_state		*sc_bulk_next;
	struct pf_state		*sc_bulk_last;
#ifdef __FreeBSD__
	struct callout		 sc_bulk_tmo;
#else
	struct timeout		 sc_bulk_tmo;
#endif

	TAILQ_HEAD(, tdb)	 sc_tdb_q;

#ifdef __FreeBSD__
	struct callout		 sc_tmo;
#else
	struct timeout		 sc_tmo;
#endif
#ifdef __FreeBSD__
	eventhandler_tag	 sc_detachtag;
#endif

};

#ifdef __FreeBSD__
static VNET_DEFINE(struct pfsync_softc	*, pfsyncif) = NULL;
#define	V_pfsyncif		VNET(pfsyncif)

static VNET_DEFINE(struct pfsyncstats, pfsyncstats);
#define	V_pfsyncstats		VNET(pfsyncstats)

SYSCTL_NODE(_net, OID_AUTO, pfsync, CTLFLAG_RW, 0, "PFSYNC");
SYSCTL_VNET_STRUCT(_net_pfsync, OID_AUTO, stats, CTLFLAG_RW,
    &VNET_NAME(pfsyncstats), pfsyncstats,
    "PFSYNC statistics (struct pfsyncstats, net/if_pfsync.h)");
#else
struct pfsync_softc	*pfsyncif = NULL;
struct pfsyncstats	 pfsyncstats;
#define	V_pfsyncstats	 pfsyncstats
#endif

#ifdef __FreeBSD__
static void	pfsyncintr(void *);
struct pfsync_swi {
	void *	pfsync_swi_cookie;
};
static struct pfsync_swi	 pfsync_swi;
#define	schednetisr(p)	swi_sched(pfsync_swi.pfsync_swi_cookie, 0)
#define	NETISR_PFSYNC
#endif

void	pfsyncattach(int);
#ifdef __FreeBSD__
int	pfsync_clone_create(struct if_clone *, int, caddr_t);
void	pfsync_clone_destroy(struct ifnet *);
#else
int	pfsync_clone_create(struct if_clone *, int);
int	pfsync_clone_destroy(struct ifnet *);
#endif
int	pfsync_alloc_scrub_memory(struct pfsync_state_peer *,
	    struct pf_state_peer *);
void	pfsync_update_net_tdb(struct pfsync_tdb *);
int	pfsyncoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
#ifdef __FreeBSD__
	    struct route *);
#else
	    struct rtentry *);
#endif
int	pfsyncioctl(struct ifnet *, u_long, caddr_t);
void	pfsyncstart(struct ifnet *);

struct mbuf *pfsync_if_dequeue(struct ifnet *);
struct mbuf *pfsync_get_mbuf(struct pfsync_softc *);

void	pfsync_deferred(struct pf_state *, int);
void	pfsync_undefer(struct pfsync_deferral *, int);
void	pfsync_defer_tmo(void *);

void	pfsync_request_update(u_int32_t, u_int64_t);
void	pfsync_update_state_req(struct pf_state *);

void	pfsync_drop(struct pfsync_softc *);
void	pfsync_sendout(void);
void	pfsync_send_plus(void *, size_t);
int	pfsync_tdb_sendout(struct pfsync_softc *);
int	pfsync_sendout_mbuf(struct pfsync_softc *, struct mbuf *);
void	pfsync_timeout(void *);
void	pfsync_tdb_timeout(void *);
void	pfsync_send_bus(struct pfsync_softc *, u_int8_t);

void	pfsync_bulk_start(void);
void	pfsync_bulk_status(u_int8_t);
void	pfsync_bulk_update(void *);
void	pfsync_bulk_fail(void *);

#ifdef __FreeBSD__
void	pfsync_ifdetach(void *, struct ifnet *);

/* XXX: ugly */
#define	betoh64		(unsigned long long)be64toh
#define	timeout_del	callout_stop
#endif

#define PFSYNC_MAX_BULKTRIES	12
#ifndef __FreeBSD__
int	pfsync_sync_ok;
#endif

#ifdef __FreeBSD__
IFC_SIMPLE_DECLARE(pfsync, 1);
#else
struct if_clone	pfsync_cloner =
    IF_CLONE_INITIALIZER("pfsync", pfsync_clone_create, pfsync_clone_destroy);
#endif

void
pfsyncattach(int npfsync)
{
	if_clone_attach(&pfsync_cloner);
}
int
#ifdef __FreeBSD__
pfsync_clone_create(struct if_clone *ifc, int unit, caddr_t param)
#else
pfsync_clone_create(struct if_clone *ifc, int unit)
#endif
{
	struct pfsync_softc *sc;
	struct ifnet *ifp;
	int q;

	if (unit != 0)
		return (EINVAL);

#ifndef __FreeBSD__
	pfsync_sync_ok = 1;
#endif

	sc = malloc(sizeof(struct pfsync_softc), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc == NULL)
		return (ENOMEM);

	for (q = 0; q < PFSYNC_S_COUNT; q++)
		TAILQ_INIT(&sc->sc_qs[q]);

#ifdef __FreeBSD__
	sc->pfsync_sync_ok = 1;
	sc->sc_pool = uma_zcreate("pfsync", PFSYNC_PLSIZE,
			NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	if (sc->sc_pool == NULL) {
		free(sc, M_DEVBUF);
		return (ENOMEM);
	}
#else
	pool_init(&sc->sc_pool, PFSYNC_PLSIZE, 0, 0, 0, "pfsync", NULL);
#endif
	TAILQ_INIT(&sc->sc_upd_req_list);
	TAILQ_INIT(&sc->sc_deferrals);
	sc->sc_deferred = 0;

	TAILQ_INIT(&sc->sc_tdb_q);

	sc->sc_len = PFSYNC_MINPKT;
	sc->sc_maxupdates = 128;

#ifdef __FreeBSD__
	sc->sc_imo.imo_membership = (struct in_multi **)malloc(
	    (sizeof(struct in_multi *) * IP_MIN_MEMBERSHIPS), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	sc->sc_imo.imo_max_memberships = IP_MIN_MEMBERSHIPS;
	sc->sc_imo.imo_multicast_vif = -1;
#else
	sc->sc_imo.imo_membership = (struct in_multi **)malloc(
	    (sizeof(struct in_multi *) * IP_MIN_MEMBERSHIPS), M_IPMOPTS,
	    M_WAITOK | M_ZERO);
	sc->sc_imo.imo_max_memberships = IP_MIN_MEMBERSHIPS;
#endif

#ifdef __FreeBSD__
	ifp = sc->sc_ifp = if_alloc(IFT_PFSYNC);
	if (ifp == NULL) {
		free(sc->sc_imo.imo_membership, M_DEVBUF);
		uma_zdestroy(sc->sc_pool);
		free(sc, M_DEVBUF);
		return (ENOSPC);
	}
	if_initname(ifp, ifc->ifc_name, unit);

	sc->sc_detachtag = EVENTHANDLER_REGISTER(ifnet_departure_event,
#ifdef __FreeBSD__
	    pfsync_ifdetach, V_pfsyncif, EVENTHANDLER_PRI_ANY);
#else
	    pfsync_ifdetach, pfsyncif, EVENTHANDLER_PRI_ANY);
#endif
	if (sc->sc_detachtag == NULL) {
		if_free(ifp);
		free(sc->sc_imo.imo_membership, M_DEVBUF);
		uma_zdestroy(sc->sc_pool);
		free(sc, M_DEVBUF);
		return (ENOSPC);
	}
#else
	ifp = &sc->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "pfsync%d", unit);
#endif
	ifp->if_softc = sc;
	ifp->if_ioctl = pfsyncioctl;
	ifp->if_output = pfsyncoutput;
	ifp->if_start = pfsyncstart;
	ifp->if_type = IFT_PFSYNC;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_hdrlen = sizeof(struct pfsync_header);
	ifp->if_mtu = 1500; /* XXX */
#ifdef __FreeBSD__
	callout_init(&sc->sc_tmo, CALLOUT_MPSAFE);
	callout_init(&sc->sc_bulk_tmo, CALLOUT_MPSAFE);
	callout_init(&sc->sc_bulkfail_tmo, CALLOUT_MPSAFE);
#else
	ifp->if_hardmtu = MCLBYTES; /* XXX */
	timeout_set(&sc->sc_tmo, pfsync_timeout, sc);
	timeout_set(&sc->sc_bulk_tmo, pfsync_bulk_update, sc);
	timeout_set(&sc->sc_bulkfail_tmo, pfsync_bulk_fail, sc);
#endif

	if_attach(ifp);
#ifndef __FreeBSD__
	if_alloc_sadl(ifp);
#endif

#if NCARP > 0
	if_addgroup(ifp, "carp");
#endif

#if NBPFILTER > 0
#ifdef __FreeBSD__
	bpfattach(ifp, DLT_PFSYNC, PFSYNC_HDRLEN);
#else
	bpfattach(&sc->sc_if.if_bpf, ifp, DLT_PFSYNC, PFSYNC_HDRLEN);
#endif
#endif

#ifdef __FreeBSD__
	V_pfsyncif = sc;
#else
	pfsyncif = sc;
#endif

	return (0);
}

#ifdef __FreeBSD__
void
#else
int
#endif
pfsync_clone_destroy(struct ifnet *ifp)
{
	struct pfsync_softc *sc = ifp->if_softc;

#ifdef __FreeBSD__
	EVENTHANDLER_DEREGISTER(ifnet_departure_event, sc->sc_detachtag);
#endif
	timeout_del(&sc->sc_bulk_tmo);
	timeout_del(&sc->sc_tmo);
#if NCARP > 0
#ifdef notyet
#ifdef __FreeBSD__
	if (!sc->pfsync_sync_ok)
#else
	if (!pfsync_sync_ok)
#endif
		carp_group_demote_adj(&sc->sc_if, -1);
#endif
#endif
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);

	pfsync_drop(sc);

	while (sc->sc_deferred > 0)
		pfsync_undefer(TAILQ_FIRST(&sc->sc_deferrals), 0);

#ifdef __FreeBSD__
	UMA_DESTROY(sc->sc_pool);
#else
	pool_destroy(&sc->sc_pool);
#endif
#ifdef __FreeBSD__
	if_free(ifp);
	free(sc->sc_imo.imo_membership, M_DEVBUF);
#else
	free(sc->sc_imo.imo_membership, M_IPMOPTS);
#endif
	free(sc, M_DEVBUF);

#ifdef __FreeBSD__
	V_pfsyncif = NULL;
#else
	pfsyncif = NULL;
#endif

#ifndef __FreeBSD__
	return (0);
#endif
}

struct mbuf *
pfsync_if_dequeue(struct ifnet *ifp)
{
	struct mbuf *m;
#ifndef __FreeBSD__
	int s;
#endif

#ifdef __FreeBSD__
	IF_LOCK(&ifp->if_snd);
	_IF_DROP(&ifp->if_snd);
	_IF_DEQUEUE(&ifp->if_snd, m);
	IF_UNLOCK(&ifp->if_snd);
#else
	s = splnet();
	IF_DEQUEUE(&ifp->if_snd, m);
	splx(s);
#endif

	return (m);
}

/*
 * Start output on the pfsync interface.
 */
void
pfsyncstart(struct ifnet *ifp)
{
	struct mbuf *m;

	while ((m = pfsync_if_dequeue(ifp)) != NULL) {
#ifndef __FreeBSD__
		IF_DROP(&ifp->if_snd);
#endif
		m_freem(m);
	}
}

int
pfsync_alloc_scrub_memory(struct pfsync_state_peer *s,
    struct pf_state_peer *d)
{
	if (s->scrub.scrub_flag && d->scrub == NULL) {
#ifdef __FreeBSD__
		d->scrub = pool_get(&V_pf_state_scrub_pl, PR_NOWAIT | PR_ZERO);
#else
		d->scrub = pool_get(&pf_state_scrub_pl, PR_NOWAIT | PR_ZERO);
#endif
		if (d->scrub == NULL)
			return (ENOMEM);
	}

	return (0);
}

#ifndef __FreeBSD__
void
pfsync_state_export(struct pfsync_state *sp, struct pf_state *st)
{
	bzero(sp, sizeof(struct pfsync_state));

	/* copy from state key */
	sp->key[PF_SK_WIRE].addr[0] = st->key[PF_SK_WIRE]->addr[0];
	sp->key[PF_SK_WIRE].addr[1] = st->key[PF_SK_WIRE]->addr[1];
	sp->key[PF_SK_WIRE].port[0] = st->key[PF_SK_WIRE]->port[0];
	sp->key[PF_SK_WIRE].port[1] = st->key[PF_SK_WIRE]->port[1];
	sp->key[PF_SK_STACK].addr[0] = st->key[PF_SK_STACK]->addr[0];
	sp->key[PF_SK_STACK].addr[1] = st->key[PF_SK_STACK]->addr[1];
	sp->key[PF_SK_STACK].port[0] = st->key[PF_SK_STACK]->port[0];
	sp->key[PF_SK_STACK].port[1] = st->key[PF_SK_STACK]->port[1];
	sp->proto = st->key[PF_SK_WIRE]->proto;
	sp->af = st->key[PF_SK_WIRE]->af;

	/* copy from state */
	strlcpy(sp->ifname, st->kif->pfik_name, sizeof(sp->ifname));
	bcopy(&st->rt_addr, &sp->rt_addr, sizeof(sp->rt_addr));
	sp->creation = htonl(time_second - st->creation);
	sp->expire = pf_state_expires(st);
	if (sp->expire <= time_second)
		sp->expire = htonl(0);
	else
		sp->expire = htonl(sp->expire - time_second);

	sp->direction = st->direction;
	sp->log = st->log;
	sp->timeout = st->timeout;
	sp->state_flags = st->state_flags;
	if (st->src_node)
		sp->sync_flags |= PFSYNC_FLAG_SRCNODE;
	if (st->nat_src_node)
		sp->sync_flags |= PFSYNC_FLAG_NATSRCNODE;

	bcopy(&st->id, &sp->id, sizeof(sp->id));
	sp->creatorid = st->creatorid;
	pf_state_peer_hton(&st->src, &sp->src);
	pf_state_peer_hton(&st->dst, &sp->dst);

	if (st->rule.ptr == NULL)
		sp->rule = htonl(-1);
	else
		sp->rule = htonl(st->rule.ptr->nr);
	if (st->anchor.ptr == NULL)
		sp->anchor = htonl(-1);
	else
		sp->anchor = htonl(st->anchor.ptr->nr);
	if (st->nat_rule.ptr == NULL)
		sp->nat_rule = htonl(-1);
	else
		sp->nat_rule = htonl(st->nat_rule.ptr->nr);

	pf_state_counter_hton(st->packets[0], sp->packets[0]);
	pf_state_counter_hton(st->packets[1], sp->packets[1]);
	pf_state_counter_hton(st->bytes[0], sp->bytes[0]);
	pf_state_counter_hton(st->bytes[1], sp->bytes[1]);

}
#endif

int
pfsync_state_import(struct pfsync_state *sp, u_int8_t flags)
{
	struct pf_state	*st = NULL;
	struct pf_state_key *skw = NULL, *sks = NULL;
	struct pf_rule *r = NULL;
	struct pfi_kif	*kif;
	int pool_flags;
	int error;

	PF_LOCK_ASSERT();

#ifdef __FreeBSD__
	if (sp->creatorid == 0 && V_pf_status.debug >= PF_DEBUG_MISC) {
#else
	if (sp->creatorid == 0 && pf_status.debug >= PF_DEBUG_MISC) {
#endif
		printf("pfsync_state_import: invalid creator id:"
		    " %08x\n", ntohl(sp->creatorid));
		return (EINVAL);
	}

	if ((kif = pfi_kif_get(sp->ifname)) == NULL) {
#ifdef __FreeBSD__
		if (V_pf_status.debug >= PF_DEBUG_MISC)
#else
		if (pf_status.debug >= PF_DEBUG_MISC)
#endif
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
#ifdef __FreeBSD__
		r = &V_pf_default_rule;
#else
		r = &pf_default_rule;
#endif

	if ((r->max_states && r->states_cur >= r->max_states))
		goto cleanup;

#ifdef __FreeBSD__
	if (flags & PFSYNC_SI_IOCTL)
		pool_flags = PR_WAITOK | PR_ZERO;
	else
		pool_flags = PR_ZERO;

	if ((st = pool_get(&V_pf_state_pl, pool_flags)) == NULL)
		goto cleanup;
#else
	if (flags & PFSYNC_SI_IOCTL)
		pool_flags = PR_WAITOK | PR_LIMITFAIL | PR_ZERO;
	else
		pool_flags = PR_LIMITFAIL | PR_ZERO;

	if ((st = pool_get(&pf_state_pl, pool_flags)) == NULL)
		goto cleanup;
#endif

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
	st->creation = time_second - ntohl(sp->creation);
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

	st->pfsync_time = time_second;
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
#ifdef __FreeBSD__
	if (skw != NULL)
		pool_put(&V_pf_state_key_pl, skw);
	if (sks != NULL)
		pool_put(&V_pf_state_key_pl, sks);
#else
	if (skw != NULL)
		pool_put(&pf_state_key_pl, skw);
	if (sks != NULL)
		pool_put(&pf_state_key_pl, sks);
#endif

cleanup_state:	/* pf_state_insert frees the state keys */
	if (st) {
#ifdef __FreeBSD__
		if (st->dst.scrub)
			pool_put(&V_pf_state_scrub_pl, st->dst.scrub);
		if (st->src.scrub)
			pool_put(&V_pf_state_scrub_pl, st->src.scrub);
		pool_put(&V_pf_state_pl, st);
#else
		if (st->dst.scrub)
			pool_put(&pf_state_scrub_pl, st->dst.scrub);
		if (st->src.scrub)
			pool_put(&pf_state_scrub_pl, st->src.scrub);
		pool_put(&pf_state_pl, st);
#endif
	}
	return (error);
}

void
#ifdef __FreeBSD__
pfsync_input(struct mbuf *m, __unused int off)
#else
pfsync_input(struct mbuf *m, ...)
#endif
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif
	struct pfsync_pkt pkt;
	struct ip *ip = mtod(m, struct ip *);
	struct pfsync_header *ph;
	struct pfsync_subheader subh;

	int offset;
	int rv;

	V_pfsyncstats.pfsyncs_ipackets++;

	/* verify that we have a sync interface configured */
#ifdef __FreeBSD__
	if (!sc || !sc->sc_sync_if || !V_pf_status.running)
#else
	if (!sc || !sc->sc_sync_if || !pf_status.running)
#endif
		goto done;

	/* verify that the packet came in on the right interface */
	if (sc->sc_sync_if != m->m_pkthdr.rcvif) {
		V_pfsyncstats.pfsyncs_badif++;
		goto done;
	}

#ifdef __FreeBSD__
	sc->sc_ifp->if_ipackets++;
	sc->sc_ifp->if_ibytes += m->m_pkthdr.len;
#else
	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;
#endif
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

#if 0
	if (pfsync_input_hmac(m, offset) != 0) {
		/* XXX stats */
		goto done;
	}
#endif

	/* Cheaper to grab this now than having to mess with mbufs later */
	pkt.ip = ip;
	pkt.src = ip->ip_src;
	pkt.flags = 0;

#ifdef __FreeBSD__
	if (!bcmp(&ph->pfcksum, &V_pf_status.pf_chksum, PF_MD5_DIGEST_LENGTH))
#else
	if (!bcmp(&ph->pfcksum, &pf_status.pf_chksum, PF_MD5_DIGEST_LENGTH))
#endif
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

int
pfsync_in_clr(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct pfsync_clr *clr;
	struct mbuf *mp;
	int len = sizeof(*clr) * count;
	int i, offp;

	struct pf_state *st, *nexts;
	struct pf_state_key *sk, *nextsk;
	struct pf_state_item *si;
	u_int32_t creatorid;
	int s;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	clr = (struct pfsync_clr *)(mp->m_data + offp);

	s = splsoftnet();
#ifdef __FreeBSD__
	PF_LOCK();
#endif
	for (i = 0; i < count; i++) {
		creatorid = clr[i].creatorid;

		if (clr[i].ifname[0] == '\0') {
#ifdef __FreeBSD__
			for (st = RB_MIN(pf_state_tree_id, &V_tree_id);
			    st; st = nexts) {
				nexts = RB_NEXT(pf_state_tree_id, &V_tree_id, st);
#else
			for (st = RB_MIN(pf_state_tree_id, &tree_id);
			    st; st = nexts) {
				nexts = RB_NEXT(pf_state_tree_id, &tree_id, st);
#endif
				if (st->creatorid == creatorid) {
					SET(st->state_flags, PFSTATE_NOSYNC);
					pf_unlink_state(st);
				}
			}
		} else {
			if (pfi_kif_get(clr[i].ifname) == NULL)
				continue;

			/* XXX correct? */
#ifdef __FreeBSD__
			for (sk = RB_MIN(pf_state_tree, &V_pf_statetbl);
#else
			for (sk = RB_MIN(pf_state_tree, &pf_statetbl);
#endif
			    sk; sk = nextsk) {
				nextsk = RB_NEXT(pf_state_tree,
#ifdef __FreeBSD__
				    &V_pf_statetbl, sk);
#else
				    &pf_statetbl, sk);
#endif
				TAILQ_FOREACH(si, &sk->states, entry) {
					if (si->s->creatorid == creatorid) {
						SET(si->s->state_flags,
						    PFSTATE_NOSYNC);
						pf_unlink_state(si->s);
					}
				}
			}
		}
	}
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	splx(s);

	return (len);
}

int
pfsync_in_ins(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct mbuf *mp;
	struct pfsync_state *sa, *sp;
	int len = sizeof(*sp) * count;
	int i, offp;

	int s;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	sa = (struct pfsync_state *)(mp->m_data + offp);

	s = splsoftnet();
#ifdef __FreeBSD__
	PF_LOCK();
#endif
	for (i = 0; i < count; i++) {
		sp = &sa[i];

		/* check for invalid values */
		if (sp->timeout >= PFTM_MAX ||
		    sp->src.state > PF_TCPS_PROXY_DST ||
		    sp->dst.state > PF_TCPS_PROXY_DST ||
		    sp->direction > PF_OUT ||
		    (sp->af != AF_INET && sp->af != AF_INET6)) {
#ifdef __FreeBSD__
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
			if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
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
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	splx(s);

	return (len);
}

int
pfsync_in_iack(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct pfsync_ins_ack *ia, *iaa;
	struct pf_state_cmp id_key;
	struct pf_state *st;

	struct mbuf *mp;
	int len = count * sizeof(*ia);
	int offp, i;
	int s;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	iaa = (struct pfsync_ins_ack *)(mp->m_data + offp);

	s = splsoftnet();
#ifdef __FreeBSD__
	PF_LOCK();
#endif
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
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	splx(s);
	/*
	 * XXX this is not yet implemented, but we know the size of the
	 * message so we can skip it.
	 */

	return (count * sizeof(struct pfsync_ins_ack));
}

int
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

int
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
	int s;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	sa = (struct pfsync_state *)(mp->m_data + offp);

	s = splsoftnet();
#ifdef __FreeBSD__
	PF_LOCK();
#endif
	for (i = 0; i < count; i++) {
		sp = &sa[i];

		/* check for invalid values */
		if (sp->timeout >= PFTM_MAX ||
		    sp->src.state > PF_TCPS_PROXY_DST ||
		    sp->dst.state > PF_TCPS_PROXY_DST) {
#ifdef __FreeBSD__
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
			if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
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
#ifdef __FreeBSD__
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
			if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
				printf("pfsync: %s stale update (%d)"
				    " id: %016llx creatorid: %08x\n",
				    (sfail < 7 ?  "ignoring" : "partial"),
				    sfail, betoh64(st->id),
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
		st->pfsync_time = time_second;
	}
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	splx(s);

	return (len);
}

int
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
	int s;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	ua = (struct pfsync_upd_c *)(mp->m_data + offp);

	s = splsoftnet();
#ifdef __FreeBSD__
	PF_LOCK();
#endif
	for (i = 0; i < count; i++) {
		up = &ua[i];

		/* check for invalid values */
		if (up->timeout >= PFTM_MAX ||
		    up->src.state > PF_TCPS_PROXY_DST ||
		    up->dst.state > PF_TCPS_PROXY_DST) {
#ifdef __FreeBSD__
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
			if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
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
#ifdef __FreeBSD__
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
			if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
				printf("pfsync: ignoring stale update "
				    "(%d) id: %016llx "
				    "creatorid: %08x\n", sfail,
				    betoh64(st->id),
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
		st->pfsync_time = time_second;
	}
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	splx(s);

	return (len);
}

int
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

			PF_LOCK();
			pfsync_update_state_req(st);
			PF_UNLOCK();
		}
	}

	return (len);
}

int
pfsync_in_del(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct mbuf *mp;
	struct pfsync_state *sa, *sp;
	struct pf_state_cmp id_key;
	struct pf_state *st;
	int len = count * sizeof(*sp);
	int offp, i;
	int s;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	sa = (struct pfsync_state *)(mp->m_data + offp);

	s = splsoftnet();
#ifdef __FreeBSD__
	PF_LOCK();
#endif
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
		pf_unlink_state(st);
	}
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	splx(s);

	return (len);
}

int
pfsync_in_del_c(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	struct mbuf *mp;
	struct pfsync_del_c *sa, *sp;
	struct pf_state_cmp id_key;
	struct pf_state *st;
	int len = count * sizeof(*sp);
	int offp, i;
	int s;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	sa = (struct pfsync_del_c *)(mp->m_data + offp);

	s = splsoftnet();
#ifdef __FreeBSD__
	PF_LOCK();
#endif
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
		pf_unlink_state(st);
	}
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	splx(s);

	return (len);
}

int
pfsync_in_bus(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif
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
#ifdef __FreeBSD__
		callout_reset(&sc->sc_bulkfail_tmo, 5 * hz, pfsync_bulk_fail,
		    V_pfsyncif);
#else
		timeout_add_sec(&sc->sc_bulkfail_tmo, 5); /* XXX magic */
#endif
#ifdef XXX
		    pf_pool_limits[PF_LIMIT_STATES].limit /
		    (PFSYNC_BULKPACKETS * sc->sc_maxcount));
#endif
#ifdef __FreeBSD__
		if (V_pf_status.debug >= PF_DEBUG_MISC)
#else
		if (pf_status.debug >= PF_DEBUG_MISC)
#endif
			printf("pfsync: received bulk update start\n");
		break;

	case PFSYNC_BUS_END:
		if (time_uptime - ntohl(bus->endtime) >=
		    sc->sc_ureq_sent) {
			/* that's it, we're happy */
			sc->sc_ureq_sent = 0;
			sc->sc_bulk_tries = 0;
			timeout_del(&sc->sc_bulkfail_tmo);
#if NCARP > 0
#ifdef notyet
#ifdef __FreeBSD__
			if (!sc->pfsync_sync_ok)
#else
			if (!pfsync_sync_ok)
#endif
				carp_group_demote_adj(&sc->sc_if, -1);
#endif
#endif
#ifdef __FreeBSD__
			sc->pfsync_sync_ok = 1;
#else
			pfsync_sync_ok = 1;
#endif
#ifdef __FreeBSD__
			if (V_pf_status.debug >= PF_DEBUG_MISC)
#else
			if (pf_status.debug >= PF_DEBUG_MISC)
#endif
				printf("pfsync: received valid "
				    "bulk update end\n");
		} else {
#ifdef __FreeBSD__
			if (V_pf_status.debug >= PF_DEBUG_MISC)
#else
			if (pf_status.debug >= PF_DEBUG_MISC)
#endif
				printf("pfsync: received invalid "
				    "bulk update end: bad timestamp\n");
		}
		break;
	}

	return (len);
}

int
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

	s = splsoftnet();
#ifdef __FreeBSD__
	PF_LOCK();
#endif
	for (i = 0; i < count; i++)
		pfsync_update_net_tdb(&tp[i]);
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	splx(s);
#endif

	return (len);
}

#if defined(IPSEC)
/* Update an in-kernel tdb. Silently fail if no tdb is found. */
void
pfsync_update_net_tdb(struct pfsync_tdb *pt)
{
	struct tdb		*tdb;
	int			 s;

	/* check for invalid values */
	if (ntohl(pt->spi) <= SPI_RESERVED_MAX ||
	    (pt->dst.sa.sa_family != AF_INET &&
	     pt->dst.sa.sa_family != AF_INET6))
		goto bad;

	s = spltdb();
	tdb = gettdb(pt->spi, &pt->dst, pt->sproto);
	if (tdb) {
		pt->rpl = ntohl(pt->rpl);
		pt->cur_bytes = betoh64(pt->cur_bytes);

		/* Neither replay nor byte counter should ever decrease. */
		if (pt->rpl < tdb->tdb_rpl ||
		    pt->cur_bytes < tdb->tdb_cur_bytes) {
			splx(s);
			goto bad;
		}

		tdb->tdb_rpl = pt->rpl;
		tdb->tdb_cur_bytes = pt->cur_bytes;
	}
	splx(s);
	return;

bad:
#ifdef __FreeBSD__
	if (V_pf_status.debug >= PF_DEBUG_MISC)
#else
	if (pf_status.debug >= PF_DEBUG_MISC)
#endif
		printf("pfsync_insert: PFSYNC_ACT_TDB_UPD: "
		    "invalid value\n");
	V_pfsyncstats.pfsyncs_badstate++;
	return;
}
#endif


int
pfsync_in_eof(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	/* check if we are at the right place in the packet */
	if (offset != m->m_pkthdr.len - sizeof(struct pfsync_eof))
		V_pfsyncstats.pfsyncs_badact++;

	/* we're done. free and let the caller return */
	m_freem(m);
	return (-1);
}

int
pfsync_in_error(struct pfsync_pkt *pkt, struct mbuf *m, int offset, int count)
{
	V_pfsyncstats.pfsyncs_badact++;

	m_freem(m);
	return (-1);
}

int
pfsyncoutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
#ifdef __FreeBSD__
	struct route *rt)
#else
	struct rtentry *rt)
#endif
{
	m_freem(m);
	return (0);
}

/* ARGSUSED */
int
pfsyncioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
#ifndef __FreeBSD__
	struct proc *p = curproc;
#endif
	struct pfsync_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ip_moptions *imo = &sc->sc_imo;
	struct pfsyncreq pfsyncr;
	struct ifnet    *sifp;
	struct ip *ip;
	int s, error;

	switch (cmd) {
#if 0
	case SIOCSIFADDR:
	case SIOCAIFADDR:
	case SIOCSIFDSTADDR:
#endif
	case SIOCSIFFLAGS:
#ifdef __FreeBSD__
		if (ifp->if_flags & IFF_UP)
			ifp->if_drv_flags |= IFF_DRV_RUNNING;
		else
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
#else
		if (ifp->if_flags & IFF_UP)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
#endif
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu <= PFSYNC_MINPKT)
			return (EINVAL);
		if (ifr->ifr_mtu > MCLBYTES) /* XXX could be bigger */
			ifr->ifr_mtu = MCLBYTES;
		if (ifr->ifr_mtu < ifp->if_mtu) {
			s = splnet();
#ifdef __FreeBSD__
			PF_LOCK();
#endif
			pfsync_sendout();
#ifdef __FreeBSD__
			PF_UNLOCK();
#endif
			splx(s);
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
#ifdef __FreeBSD__
		if ((error = priv_check(curthread, PRIV_NETINET_PF)) != 0)
#else
		if ((error = suser(p, p->p_acflag)) != 0)
#endif
			return (error);
		if ((error = copyin(ifr->ifr_data, &pfsyncr, sizeof(pfsyncr))))
			return (error);

#ifdef __FreeBSD__
		PF_LOCK();
#endif
		if (pfsyncr.pfsyncr_syncpeer.s_addr == 0)
#ifdef __FreeBSD__
			sc->sc_sync_peer.s_addr = htonl(INADDR_PFSYNC_GROUP);
#else
			sc->sc_sync_peer.s_addr = INADDR_PFSYNC_GROUP;
#endif
		else
			sc->sc_sync_peer.s_addr =
			    pfsyncr.pfsyncr_syncpeer.s_addr;

		if (pfsyncr.pfsyncr_maxupdates > 255)
#ifdef __FreeBSD__
		{
			PF_UNLOCK();
#endif
			return (EINVAL);
#ifdef __FreeBSD__
		}
#endif
		sc->sc_maxupdates = pfsyncr.pfsyncr_maxupdates;

		if (pfsyncr.pfsyncr_syncdev[0] == 0) {
			sc->sc_sync_if = NULL;
#ifdef __FreeBSD__
			PF_UNLOCK();
#endif
			if (imo->imo_num_memberships > 0) {
				in_delmulti(imo->imo_membership[
				    --imo->imo_num_memberships]);
				imo->imo_multicast_ifp = NULL;
			}
			break;
		}

#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		if ((sifp = ifunit(pfsyncr.pfsyncr_syncdev)) == NULL)
			return (EINVAL);

#ifdef __FreeBSD__
		PF_LOCK();
#endif
		s = splnet();
#ifdef __FreeBSD__
		if (sifp->if_mtu < sc->sc_ifp->if_mtu ||
#else
		if (sifp->if_mtu < sc->sc_if.if_mtu ||
#endif
		    (sc->sc_sync_if != NULL &&
		    sifp->if_mtu < sc->sc_sync_if->if_mtu) ||
		    sifp->if_mtu < MCLBYTES - sizeof(struct ip))
			pfsync_sendout();
		sc->sc_sync_if = sifp;

		if (imo->imo_num_memberships > 0) {
#ifdef __FreeBSD__
			PF_UNLOCK();
#endif
			in_delmulti(imo->imo_membership[--imo->imo_num_memberships]);
#ifdef __FreeBSD__
			PF_LOCK();
#endif
			imo->imo_multicast_ifp = NULL;
		}

		if (sc->sc_sync_if &&
#ifdef __FreeBSD__
		    sc->sc_sync_peer.s_addr == htonl(INADDR_PFSYNC_GROUP)) {
#else
		    sc->sc_sync_peer.s_addr == INADDR_PFSYNC_GROUP) {
#endif
			struct in_addr addr;

			if (!(sc->sc_sync_if->if_flags & IFF_MULTICAST)) {
				sc->sc_sync_if = NULL;
#ifdef __FreeBSD__
				PF_UNLOCK();
#endif
				splx(s);
				return (EADDRNOTAVAIL);
			}

#ifdef __FreeBSD__
			addr.s_addr = htonl(INADDR_PFSYNC_GROUP);
#else
			addr.s_addr = INADDR_PFSYNC_GROUP;
#endif

#ifdef __FreeBSD__
			PF_UNLOCK();
#endif
			if ((imo->imo_membership[0] =
			    in_addmulti(&addr, sc->sc_sync_if)) == NULL) {
				sc->sc_sync_if = NULL;
				splx(s);
				return (ENOBUFS);
			}
#ifdef __FreeBSD__
			PF_LOCK();
#endif
			imo->imo_num_memberships++;
			imo->imo_multicast_ifp = sc->sc_sync_if;
			imo->imo_multicast_ttl = PFSYNC_DFLTTL;
			imo->imo_multicast_loop = 0;
		}

		ip = &sc->sc_template;
		bzero(ip, sizeof(*ip));
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(sc->sc_template) >> 2;
		ip->ip_tos = IPTOS_LOWDELAY;
		/* len and id are set later */
#ifdef __FreeBSD__
		ip->ip_off = IP_DF;
#else
		ip->ip_off = htons(IP_DF);
#endif
		ip->ip_ttl = PFSYNC_DFLTTL;
		ip->ip_p = IPPROTO_PFSYNC;
		ip->ip_src.s_addr = INADDR_ANY;
		ip->ip_dst.s_addr = sc->sc_sync_peer.s_addr;

		if (sc->sc_sync_if) {
			/* Request a full state table update. */
			sc->sc_ureq_sent = time_uptime;
#if NCARP > 0
#ifdef notyet
#ifdef __FreeBSD__
			if (sc->pfsync_sync_ok)
#else
			if (pfsync_sync_ok)
#endif
				carp_group_demote_adj(&sc->sc_if, 1);
#endif
#endif
#ifdef __FreeBSD__
			sc->pfsync_sync_ok = 0;
#else
			pfsync_sync_ok = 0;
#endif
#ifdef __FreeBSD__
			if (V_pf_status.debug >= PF_DEBUG_MISC)
#else
			if (pf_status.debug >= PF_DEBUG_MISC)
#endif
				printf("pfsync: requesting bulk update\n");
#ifdef __FreeBSD__
				callout_reset(&sc->sc_bulkfail_tmo, 5 * hz,
				    pfsync_bulk_fail, V_pfsyncif);
#else
			timeout_add_sec(&sc->sc_bulkfail_tmo, 5);
#endif
			pfsync_request_update(0, 0);
		}
#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		splx(s);

		break;

	default:
		return (ENOTTY);
	}

	return (0);
}

int
pfsync_out_state(struct pf_state *st, struct mbuf *m, int offset)
{
	struct pfsync_state *sp = (struct pfsync_state *)(m->m_data + offset);

	pfsync_state_export(sp, st);

	return (sizeof(*sp));
}

int
pfsync_out_iack(struct pf_state *st, struct mbuf *m, int offset)
{
	struct pfsync_ins_ack *iack =
	    (struct pfsync_ins_ack *)(m->m_data + offset);

	iack->id = st->id;
	iack->creatorid = st->creatorid;

	return (sizeof(*iack));
}

int
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

int
pfsync_out_del(struct pf_state *st, struct mbuf *m, int offset)
{
	struct pfsync_del_c *dp = (struct pfsync_del_c *)(m->m_data + offset);

	dp->id = st->id;
	dp->creatorid = st->creatorid;

	SET(st->state_flags, PFSTATE_NOSYNC);

	return (sizeof(*dp));
}

void
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
#ifdef __FreeBSD__
			KASSERT(st->sync_state == q,
				("%s: st->sync_state == q", 
					__FUNCTION__));
#else
			KASSERT(st->sync_state == q);
#endif
#endif
			st->sync_state = PFSYNC_S_NONE;
		}
		TAILQ_INIT(&sc->sc_qs[q]);
	}

	while ((ur = TAILQ_FIRST(&sc->sc_upd_req_list)) != NULL) {
		TAILQ_REMOVE(&sc->sc_upd_req_list, ur, ur_entry);
		pool_put(&sc->sc_pool, ur);
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

void
pfsync_sendout(void)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif
#if NBPFILTER > 0
#ifdef __FreeBSD__
	struct ifnet *ifp = sc->sc_ifp;
#else
	struct ifnet *ifp = &sc->sc_if;
#endif
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
#ifdef __FreeBSD__
	size_t pktlen;
#endif
	int offset;
	int q, count = 0;

#ifdef __FreeBSD__
	PF_LOCK_ASSERT();
#else
	splassert(IPL_NET);
#endif

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

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
#ifdef __FreeBSD__
		sc->sc_ifp->if_oerrors++;
#else
		sc->sc_if.if_oerrors++;
#endif
		V_pfsyncstats.pfsyncs_onomem++;
		pfsync_drop(sc);
		return;
	}

#ifdef __FreeBSD__
	pktlen = max_linkhdr + sc->sc_len;
	if (pktlen > MHLEN) {
		/* Find the right pool to allocate from. */
		/* XXX: This is ugly. */
		m_cljget(m, M_DONTWAIT, pktlen <= MSIZE ? MSIZE :
			pktlen <= MCLBYTES ? MCLBYTES :
#if MJUMPAGESIZE != MCLBYTES
			pktlen <= MJUMPAGESIZE ? MJUMPAGESIZE :
#endif
			pktlen <= MJUM9BYTES ? MJUM9BYTES : MJUM16BYTES);
#else
	if (max_linkhdr + sc->sc_len > MHLEN) {
		MCLGETI(m, M_DONTWAIT, NULL, max_linkhdr + sc->sc_len);
#endif
		if (!ISSET(m->m_flags, M_EXT)) {
			m_free(m);
#ifdef __FreeBSD__
			sc->sc_ifp->if_oerrors++;
#else
			sc->sc_if.if_oerrors++;
#endif
			V_pfsyncstats.pfsyncs_onomem++;
			pfsync_drop(sc);
			return;
		}
	}
	m->m_data += max_linkhdr;
	m->m_len = m->m_pkthdr.len = sc->sc_len;

	/* build the ip header */
	ip = (struct ip *)m->m_data;
	bcopy(&sc->sc_template, ip, sizeof(*ip));
	offset = sizeof(*ip);

#ifdef __FreeBSD__
	ip->ip_len = m->m_pkthdr.len;
#else
	ip->ip_len = htons(m->m_pkthdr.len);
#endif
	ip->ip_id = htons(ip_randomid());

	/* build the pfsync header */
	ph = (struct pfsync_header *)(m->m_data + offset);
	bzero(ph, sizeof(*ph));
	offset += sizeof(*ph);

	ph->version = PFSYNC_VERSION;
	ph->len = htons(sc->sc_len - sizeof(*ip));
#ifdef __FreeBSD__
	bcopy(V_pf_status.pf_chksum, ph->pfcksum, PF_MD5_DIGEST_LENGTH);
#else
	bcopy(pf_status.pf_chksum, ph->pfcksum, PF_MD5_DIGEST_LENGTH);
#endif

	/* walk the queues */
	for (q = 0; q < PFSYNC_S_COUNT; q++) {
		if (TAILQ_EMPTY(&sc->sc_qs[q]))
			continue;

		subh = (struct pfsync_subheader *)(m->m_data + offset);
		offset += sizeof(*subh);

		count = 0;
		TAILQ_FOREACH(st, &sc->sc_qs[q], sync_list) {
#ifdef PFSYNC_DEBUG
#ifdef __FreeBSD__
			KASSERT(st->sync_state == q,
				("%s: st->sync_state == q",
					__FUNCTION__));
#else
			KASSERT(st->sync_state == q);
#endif
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

			pool_put(&sc->sc_pool, ur);

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
#ifdef __FreeBSD__
		BPF_MTAP(ifp, m);
#else
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
		m->m_data -= sizeof(*ip);
		m->m_len = m->m_pkthdr.len = sc->sc_len;
	}

	if (sc->sc_sync_if == NULL) {
		sc->sc_len = PFSYNC_MINPKT;
		m_freem(m);
		return;
	}
#endif

#ifdef __FreeBSD__
	sc->sc_ifp->if_opackets++;
	sc->sc_ifp->if_obytes += m->m_pkthdr.len;
#else
	sc->sc_if.if_opackets++;
	sc->sc_if.if_obytes += m->m_pkthdr.len;
#endif

	sc->sc_len = PFSYNC_MINPKT;
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	if (ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo, NULL) == 0)
#ifdef __FreeBSD__
	{
		PF_LOCK();
#endif
		V_pfsyncstats.pfsyncs_opackets++;
#ifdef __FreeBSD__
	}
#endif
	else
#ifdef __FreeBSD__
	{
		PF_LOCK();
#endif
		V_pfsyncstats.pfsyncs_oerrors++;
#ifdef __FreeBSD__
	}
#endif
}

void
pfsync_insert_state(struct pf_state *st)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif

#ifdef __FreeBSD__
	PF_LOCK_ASSERT();
#else
	splassert(IPL_SOFTNET);
#endif

	if (ISSET(st->rule.ptr->rule_flag, PFRULE_NOSYNC) ||
	    st->key[PF_SK_WIRE]->proto == IPPROTO_PFSYNC) {
		SET(st->state_flags, PFSTATE_NOSYNC);
		return;
	}

	if (sc == NULL || ISSET(st->state_flags, PFSTATE_NOSYNC))
		return;

#ifdef PFSYNC_DEBUG
#ifdef __FreeBSD__
	KASSERT(st->sync_state == PFSYNC_S_NONE,
		("%s: st->sync_state == PFSYNC_S_NONE", __FUNCTION__));
#else
	KASSERT(st->sync_state == PFSYNC_S_NONE);
#endif
#endif

	if (sc->sc_len == PFSYNC_MINPKT)
#ifdef __FreeBSD__
		callout_reset(&sc->sc_tmo, 1 * hz, pfsync_timeout,
		    V_pfsyncif);
#else
		timeout_add_sec(&sc->sc_tmo, 1);
#endif

	pfsync_q_ins(st, PFSYNC_S_INS);

	if (ISSET(st->state_flags, PFSTATE_ACK))
		schednetisr(NETISR_PFSYNC);
	else
		st->sync_updates = 0;
}

int defer = 10;

int
pfsync_defer(struct pf_state *st, struct mbuf *m)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif
	struct pfsync_deferral *pd;

#ifdef __FreeBSD__
	PF_LOCK_ASSERT();
#else
	splassert(IPL_SOFTNET);
#endif

	if (sc->sc_deferred >= 128)
		pfsync_undefer(TAILQ_FIRST(&sc->sc_deferrals), 0);

	pd = pool_get(&sc->sc_pool, M_NOWAIT);
	if (pd == NULL)
		return (0);
	sc->sc_deferred++;

#ifdef __FreeBSD__
	m->m_flags |= M_SKIP_FIREWALL;
#else
	m->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
#endif
	SET(st->state_flags, PFSTATE_ACK);

	pd->pd_st = st;
	pd->pd_m = m;

	TAILQ_INSERT_TAIL(&sc->sc_deferrals, pd, pd_entry);
#ifdef __FreeBSD__
	callout_init(&pd->pd_tmo, CALLOUT_MPSAFE);
	callout_reset(&pd->pd_tmo, defer, pfsync_defer_tmo,
		pd);
#else
	timeout_set(&pd->pd_tmo, pfsync_defer_tmo, pd);
	timeout_add(&pd->pd_tmo, defer);
#endif

	return (1);
}

void
pfsync_undefer(struct pfsync_deferral *pd, int drop)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif
	int s;

#ifdef __FreeBSD__
	PF_LOCK_ASSERT();
#else
	splassert(IPL_SOFTNET);
#endif

	TAILQ_REMOVE(&sc->sc_deferrals, pd, pd_entry);
	sc->sc_deferred--;

	CLR(pd->pd_st->state_flags, PFSTATE_ACK);
	timeout_del(&pd->pd_tmo); /* bah */
	if (drop)
		m_freem(pd->pd_m);
	else {
		s = splnet();
#ifdef __FreeBSD__
		/* XXX: use pf_defered?! */
		PF_UNLOCK();
#endif
		ip_output(pd->pd_m, (void *)NULL, (void *)NULL, 0,
		    (void *)NULL, (void *)NULL);
#ifdef __FreeBSD__
		PF_LOCK();
#endif
		splx(s);
	}

	pool_put(&sc->sc_pool, pd);
}

void
pfsync_defer_tmo(void *arg)
{
#if defined(__FreeBSD__) && defined(VIMAGE)
	struct pfsync_deferral *pd = arg;
#endif
	int s;

	s = splsoftnet();
#ifdef __FreeBSD__
	CURVNET_SET(pd->pd_m->m_pkthdr.rcvif->if_vnet); /* XXX */
	PF_LOCK();
#endif
	pfsync_undefer(arg, 0);
#ifdef __FreeBSD__
	PF_UNLOCK();
	CURVNET_RESTORE();
#endif
	splx(s);
}

void
pfsync_deferred(struct pf_state *st, int drop)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif
	struct pfsync_deferral *pd;

	TAILQ_FOREACH(pd, &sc->sc_deferrals, pd_entry) {
		 if (pd->pd_st == st) {
			pfsync_undefer(pd, drop);
			return;
		}
	}

	panic("pfsync_send_deferred: unable to find deferred state");
}

u_int pfsync_upds = 0;

void
pfsync_update_state(struct pf_state *st)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif
	int sync = 0;

#ifdef __FreeBSD__
	PF_LOCK_ASSERT();
#else
	splassert(IPL_SOFTNET);
#endif

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
#ifdef __FreeBSD__
		callout_reset(&sc->sc_tmo, 1 * hz, pfsync_timeout,
		    V_pfsyncif);
#else
		timeout_add_sec(&sc->sc_tmo, 1);
#endif

	switch (st->sync_state) {
	case PFSYNC_S_UPD_C:
	case PFSYNC_S_UPD:
	case PFSYNC_S_INS:
		/* we're already handling it */

		st->sync_updates++;
		if (st->sync_updates >= sc->sc_maxupdates)
			sync = 1;
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

	if (sync || (time_second - st->pfsync_time) < 2) {
		pfsync_upds++;
		schednetisr(NETISR_PFSYNC);
	}
}

void
pfsync_request_update(u_int32_t creatorid, u_int64_t id)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif
	struct pfsync_upd_req_item *item;
	size_t nlen = sizeof(struct pfsync_upd_req);
	int s;

	PF_LOCK_ASSERT();

	/*
	 * this code does nothing to prevent multiple update requests for the
	 * same state being generated.
	 */

	item = pool_get(&sc->sc_pool, PR_NOWAIT);
	if (item == NULL) {
		/* XXX stats */
		return;
	}

	item->ur_msg.id = id;
	item->ur_msg.creatorid = creatorid;

	if (TAILQ_EMPTY(&sc->sc_upd_req_list))
		nlen += sizeof(struct pfsync_subheader);

#ifdef __FreeBSD__
	if (sc->sc_len + nlen > sc->sc_ifp->if_mtu) {
#else
	if (sc->sc_len + nlen > sc->sc_if.if_mtu) {
#endif
		s = splnet();
		pfsync_sendout();
		splx(s);

		nlen = sizeof(struct pfsync_subheader) +
		    sizeof(struct pfsync_upd_req);
	}

	TAILQ_INSERT_TAIL(&sc->sc_upd_req_list, item, ur_entry);
	sc->sc_len += nlen;

	schednetisr(NETISR_PFSYNC);
}

void
pfsync_update_state_req(struct pf_state *st)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif

	PF_LOCK_ASSERT();

	if (sc == NULL)
		panic("pfsync_update_state_req: nonexistant instance");

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

void
pfsync_delete_state(struct pf_state *st)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif

#ifdef __FreeBSD__
	PF_LOCK_ASSERT();
#else
	splassert(IPL_SOFTNET);
#endif

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
#ifdef __FreeBSD__
		callout_reset(&sc->sc_tmo, 1 * hz, pfsync_timeout,
		    V_pfsyncif);
#else
		timeout_add_sec(&sc->sc_tmo, 1);
#endif

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

void
pfsync_clear_states(u_int32_t creatorid, const char *ifname)
{
	struct {
		struct pfsync_subheader subh;
		struct pfsync_clr clr;
	} __packed r;

#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif

#ifdef __FreeBSD__
	PF_LOCK_ASSERT();
#else
	splassert(IPL_SOFTNET);
#endif

	if (sc == NULL)
		return;

	bzero(&r, sizeof(r));

	r.subh.action = PFSYNC_ACT_CLR;
	r.subh.count = htons(1);

	strlcpy(r.clr.ifname, ifname, sizeof(r.clr.ifname));
	r.clr.creatorid = creatorid;

	pfsync_send_plus(&r, sizeof(r));
}

void
pfsync_q_ins(struct pf_state *st, int q)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif
	size_t nlen = pfsync_qs[q].len;
	int s;

	PF_LOCK_ASSERT();

#ifdef __FreeBSD__
	KASSERT(st->sync_state == PFSYNC_S_NONE,
		("%s: st->sync_state == PFSYNC_S_NONE", __FUNCTION__));
#else
	KASSERT(st->sync_state == PFSYNC_S_NONE);
#endif

#if 1 || defined(PFSYNC_DEBUG)
	if (sc->sc_len < PFSYNC_MINPKT)
#ifdef __FreeBSD__
		panic("pfsync pkt len is too low %zu", sc->sc_len);
#else
		panic("pfsync pkt len is too low %d", sc->sc_len);
#endif
#endif
	if (TAILQ_EMPTY(&sc->sc_qs[q]))
		nlen += sizeof(struct pfsync_subheader);

#ifdef __FreeBSD__
	if (sc->sc_len + nlen > sc->sc_ifp->if_mtu) {
#else
	if (sc->sc_len + nlen > sc->sc_if.if_mtu) {
#endif
		s = splnet();
		pfsync_sendout();
		splx(s);

		nlen = sizeof(struct pfsync_subheader) + pfsync_qs[q].len;
	}

	sc->sc_len += nlen;
	TAILQ_INSERT_TAIL(&sc->sc_qs[q], st, sync_list);
	st->sync_state = q;
}

void
pfsync_q_del(struct pf_state *st)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif
	int q = st->sync_state;

#ifdef __FreeBSD__
	KASSERT(st->sync_state != PFSYNC_S_NONE, 
		("%s: st->sync_state != PFSYNC_S_NONE", __FUNCTION__));
#else
	KASSERT(st->sync_state != PFSYNC_S_NONE);
#endif

	sc->sc_len -= pfsync_qs[q].len;
	TAILQ_REMOVE(&sc->sc_qs[q], st, sync_list);
	st->sync_state = PFSYNC_S_NONE;

	if (TAILQ_EMPTY(&sc->sc_qs[q]))
		sc->sc_len -= sizeof(struct pfsync_subheader);
}

#ifdef notyet
void
pfsync_update_tdb(struct tdb *t, int output)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif
	size_t nlen = sizeof(struct pfsync_tdb);
	int s;

	if (sc == NULL)
		return;

	if (!ISSET(t->tdb_flags, TDBF_PFSYNC)) {
		if (TAILQ_EMPTY(&sc->sc_tdb_q))
			nlen += sizeof(struct pfsync_subheader);

		if (sc->sc_len + nlen > sc->sc_if.if_mtu) {
			s = splnet();
			PF_LOCK();
			pfsync_sendout();
			PF_UNLOCK();
			splx(s);

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

void
pfsync_delete_tdb(struct tdb *t)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif

	if (sc == NULL || !ISSET(t->tdb_flags, TDBF_PFSYNC))
		return;

	sc->sc_len -= sizeof(struct pfsync_tdb);
	TAILQ_REMOVE(&sc->sc_tdb_q, t, tdb_sync_entry);
	CLR(t->tdb_flags, TDBF_PFSYNC);

	if (TAILQ_EMPTY(&sc->sc_tdb_q))
		sc->sc_len -= sizeof(struct pfsync_subheader);
}

int
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

void
pfsync_bulk_start(void)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif

	sc->sc_ureq_received = time_uptime;

	if (sc->sc_bulk_next == NULL)
#ifdef __FreeBSD__
		sc->sc_bulk_next = TAILQ_FIRST(&V_state_list);
#else
		sc->sc_bulk_next = TAILQ_FIRST(&state_list);
#endif
	sc->sc_bulk_last = sc->sc_bulk_next;

#ifdef __FreeBSD__
	if (V_pf_status.debug >= PF_DEBUG_MISC)
#else
	if (pf_status.debug >= PF_DEBUG_MISC)
#endif
		printf("pfsync: received bulk update request\n");

	PF_LOCK();
	pfsync_bulk_status(PFSYNC_BUS_START);
	pfsync_bulk_update(sc);
	PF_UNLOCK();
}

void
pfsync_bulk_update(void *arg)
{
	struct pfsync_softc *sc = arg;
	struct pf_state *st = sc->sc_bulk_next;
	int i = 0;
	int s;

	PF_LOCK_ASSERT();

	s = splsoftnet();
#ifdef __FreeBSD__
	CURVNET_SET(sc->sc_ifp->if_vnet);
#endif
	do {
		if (st->sync_state == PFSYNC_S_NONE &&
		    st->timeout < PFTM_MAX &&
		    st->pfsync_time <= sc->sc_ureq_received) {
			pfsync_update_state_req(st);
			i++;
		}

		st = TAILQ_NEXT(st, entry_list);
		if (st == NULL)
#ifdef __FreeBSD__
			st = TAILQ_FIRST(&V_state_list);
#else
			st = TAILQ_FIRST(&state_list);
#endif

		if (i > 0 && TAILQ_EMPTY(&sc->sc_qs[PFSYNC_S_UPD])) {
			sc->sc_bulk_next = st;
#ifdef __FreeBSD__
			callout_reset(&sc->sc_bulk_tmo, 1,
			    pfsync_bulk_fail, sc);
#else
			timeout_add(&sc->sc_bulk_tmo, 1);
#endif
			goto out;
		}
	} while (st != sc->sc_bulk_last);

	/* we're done */
	sc->sc_bulk_next = NULL;
	sc->sc_bulk_last = NULL;
	pfsync_bulk_status(PFSYNC_BUS_END);

out:
#ifdef __FreeBSD__
	CURVNET_RESTORE();
#endif
	splx(s);
}

void
pfsync_bulk_status(u_int8_t status)
{
	struct {
		struct pfsync_subheader subh;
		struct pfsync_bus bus;
	} __packed r;

#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif

	PF_LOCK_ASSERT();

	bzero(&r, sizeof(r));

	r.subh.action = PFSYNC_ACT_BUS;
	r.subh.count = htons(1);

#ifdef __FreeBSD__
	r.bus.creatorid = V_pf_status.hostid;
#else
	r.bus.creatorid = pf_status.hostid;
#endif
	r.bus.endtime = htonl(time_uptime - sc->sc_ureq_received);
	r.bus.status = status;

	pfsync_send_plus(&r, sizeof(r));
}

void
pfsync_bulk_fail(void *arg)
{
	struct pfsync_softc *sc = arg;

#ifdef __FreeBSD__
	CURVNET_SET(sc->sc_ifp->if_vnet);
#endif

	if (sc->sc_bulk_tries++ < PFSYNC_MAX_BULKTRIES) {
		/* Try again */
#ifdef __FreeBSD__
		callout_reset(&sc->sc_bulkfail_tmo, 5 * hz,
		    pfsync_bulk_fail, V_pfsyncif);
#else
		timeout_add_sec(&sc->sc_bulkfail_tmo, 5);
#endif
		PF_LOCK();
		pfsync_request_update(0, 0);
		PF_UNLOCK();
	} else {
		/* Pretend like the transfer was ok */
		sc->sc_ureq_sent = 0;
		sc->sc_bulk_tries = 0;
#if NCARP > 0
#ifdef notyet
#ifdef __FreeBSD__
		if (!sc->pfsync_sync_ok)
#else
		if (!pfsync_sync_ok)
#endif
			carp_group_demote_adj(&sc->sc_if, -1);
#endif
#endif
#ifdef __FreeBSD__
		sc->pfsync_sync_ok = 1;
#else
		pfsync_sync_ok = 1;
#endif
#ifdef __FreeBSD__
		if (V_pf_status.debug >= PF_DEBUG_MISC)
#else
		if (pf_status.debug >= PF_DEBUG_MISC)
#endif
			printf("pfsync: failed to receive bulk update\n");
	}

#ifdef __FreeBSD__
	CURVNET_RESTORE();
#endif
}

void
pfsync_send_plus(void *plus, size_t pluslen)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif
	int s;

	PF_LOCK_ASSERT();

#ifdef __FreeBSD__
	if (sc->sc_len + pluslen > sc->sc_ifp->if_mtu) {
#else
	if (sc->sc_len + pluslen > sc->sc_if.if_mtu) {
#endif
		s = splnet();
		pfsync_sendout();
		splx(s);
	}

	sc->sc_plus = plus;
	sc->sc_len += (sc->sc_pluslen = pluslen);

	s = splnet();
	pfsync_sendout();
	splx(s);
}

int
pfsync_up(void)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif

#ifdef __FreeBSD__
	if (sc == NULL || !ISSET(sc->sc_ifp->if_flags, IFF_DRV_RUNNING))
#else
	if (sc == NULL || !ISSET(sc->sc_if.if_flags, IFF_RUNNING))
#endif
		return (0);

	return (1);
}

int
pfsync_state_in_use(struct pf_state *st)
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = V_pfsyncif;
#else
	struct pfsync_softc *sc = pfsyncif;
#endif

	if (sc == NULL)
		return (0);

	if (st->sync_state != PFSYNC_S_NONE)
		return (1);

	if (sc->sc_bulk_next == NULL && sc->sc_bulk_last == NULL)
		return (0);

	return (1);
}

u_int pfsync_ints;
u_int pfsync_tmos;

void
pfsync_timeout(void *arg)
{
#if defined(__FreeBSD__) && defined(VIMAGE)
	struct pfsync_softc *sc = arg;
#endif
	int s;

#ifdef __FreeBSD__
	CURVNET_SET(sc->sc_ifp->if_vnet);
#endif

	pfsync_tmos++;

	s = splnet();
#ifdef __FreeBSD__
	PF_LOCK();
#endif
	pfsync_sendout();
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	splx(s);

#ifdef __FreeBSD__
	CURVNET_RESTORE();
#endif
}

/* this is a softnet/netisr handler */
void
#ifdef __FreeBSD__
pfsyncintr(void *arg)
#else
pfsyncintr(void)
#endif
{
#ifdef __FreeBSD__
	struct pfsync_softc *sc = arg;
#endif
	int s;

#ifdef __FreeBSD__
	if (sc == NULL)
		return;

	CURVNET_SET(sc->sc_ifp->if_vnet);
#endif
	pfsync_ints++;

	s = splnet();
#ifdef __FreeBSD__
	PF_LOCK();
#endif
	pfsync_sendout();
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	splx(s);

#ifdef __FreeBSD__
	CURVNET_RESTORE();
#endif
}

int
pfsync_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{

#ifdef notyet
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
#endif
	return (ENOPROTOOPT);
}

#ifdef __FreeBSD__
void
pfsync_ifdetach(void *arg, struct ifnet *ifp)
{
	struct pfsync_softc *sc = (struct pfsync_softc *)arg;
	struct ip_moptions *imo;

	if (sc == NULL || sc->sc_sync_if != ifp)
		return;         /* not for us; unlocked read */

	CURVNET_SET(sc->sc_ifp->if_vnet);

	PF_LOCK();

	/* Deal with a member interface going away from under us. */
	sc->sc_sync_if = NULL;
	imo = &sc->sc_imo;
	if (imo->imo_num_memberships > 0) {
		KASSERT(imo->imo_num_memberships == 1,
		    ("%s: imo_num_memberships != 1", __func__));
		/*
		 * Our event handler is always called after protocol
		 * domains have been detached from the underlying ifnet.
		 * Do not call in_delmulti(); we held a single reference
		 * which the protocol domain has purged in in_purgemaddrs().
		 */
		PF_UNLOCK();
		imo->imo_membership[--imo->imo_num_memberships] = NULL;
		PF_LOCK();
		imo->imo_multicast_ifp = NULL;
	}

	PF_UNLOCK();
	
	CURVNET_RESTORE();
}

static int
vnet_pfsync_init(const void *unused)
{
	int error = 0;

	pfsyncattach(0);

	error = swi_add(NULL, "pfsync", pfsyncintr, V_pfsyncif,
		SWI_NET, INTR_MPSAFE, &pfsync_swi.pfsync_swi_cookie);
	if (error)
		panic("%s: swi_add %d", __func__, error);

	pfsync_state_import_ptr = pfsync_state_import;
	pfsync_up_ptr = pfsync_up;
	pfsync_insert_state_ptr = pfsync_insert_state;
	pfsync_update_state_ptr = pfsync_update_state;
	pfsync_delete_state_ptr = pfsync_delete_state;
	pfsync_clear_states_ptr = pfsync_clear_states;
	pfsync_state_in_use_ptr = pfsync_state_in_use;
	pfsync_defer_ptr = pfsync_defer;

	return (0);
}

static int
vnet_pfsync_uninit(const void *unused)
{

	swi_remove(pfsync_swi.pfsync_swi_cookie);

	pfsync_state_import_ptr = NULL;
	pfsync_up_ptr = NULL;
	pfsync_insert_state_ptr = NULL;
	pfsync_update_state_ptr = NULL;
	pfsync_delete_state_ptr = NULL;
	pfsync_clear_states_ptr = NULL;
	pfsync_state_in_use_ptr = NULL;
	pfsync_defer_ptr = NULL;

	if_clone_detach(&pfsync_cloner);

	return (0);
}

/* Define startup order. */
#define	PFSYNC_SYSINIT_ORDER	SI_SUB_PROTO_IF
#define	PFSYNC_MODEVENT_ORDER	(SI_ORDER_FIRST) /* On boot slot in here. */
#define	PFSYNC_VNET_ORDER	(PFSYNC_MODEVENT_ORDER + 2) /* Later still. */

/*
 * Starting up.
 * VNET_SYSINIT is called for each existing vnet and each new vnet.
 */
VNET_SYSINIT(vnet_pfsync_init, PFSYNC_SYSINIT_ORDER, PFSYNC_VNET_ORDER,
    vnet_pfsync_init, NULL);

/*
 * Closing up shop. These are done in REVERSE ORDER,
 * Not called on reboot.
 * VNET_SYSUNINIT is called for each exiting vnet as it exits.
 */
VNET_SYSUNINIT(vnet_pfsync_uninit, PFSYNC_SYSINIT_ORDER, PFSYNC_VNET_ORDER,
    vnet_pfsync_uninit, NULL);
static int
pfsync_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
#ifndef __FreeBSD__
		pfsyncattach(0);
#endif
		break;
	case MOD_UNLOAD:
#ifndef __FreeBSD__
		if_clone_detach(&pfsync_cloner);
#endif
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

static moduledata_t pfsync_mod = {
	"pfsync",
	pfsync_modevent,
	0
};

#define PFSYNC_MODVER 1

DECLARE_MODULE(pfsync, pfsync_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(pfsync, PFSYNC_MODVER);
MODULE_DEPEND(pfsync, pf, PF_MODVER, PF_MODVER, PF_MODVER);
#endif /* __FreeBSD__ */
