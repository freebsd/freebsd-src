/*-
 * SPDX-License-Identifier: (BSD-2-Clause AND ISC)
 *
 * Copyright (c) 2002 Michael Shalayeff
 * Copyright (c) 2012 Gleb Smirnoff <glebius@FreeBSD.org>
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

/*-
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
 * $OpenBSD: if_pfsync.c,v 1.110 2009/02/24 05:39:19 dlg Exp $
 *
 * Revisions picked from OpenBSD after revision 1.110 import:
 * 1.119 - don't m_copydata() beyond the len of mbuf in pfsync_input()
 * 1.118, 1.124, 1.148, 1.149, 1.151, 1.171 - fixes to bulk updates
 * 1.120, 1.175 - use monotonic time_uptime
 * 1.122 - reduce number of updates for non-TCP sessions
 * 1.125, 1.127 - rewrite merge or stale processing
 * 1.128 - cleanups
 * 1.146 - bzero() mbuf before sparsely filling it with data
 * 1.170 - SIOCSIFMTU checks
 * 1.126, 1.142 - deferred packets processing
 * 1.173 - correct expire time processing
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_pf.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/nv.h>
#include <sys/priv.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_private.h>
#include <net/if_types.h>
#include <net/vnet.h>
#include <net/pfvar.h>
#include <net/route.h>
#include <net/if_pfsync.h>

#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/in6_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_carp.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>

#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>

#include <netpfil/pf/pfsync_nv.h>

#define	DPFPRINTF(n, x)	if (V_pf_status.debug >= (n)) printf x

struct pfsync_bucket;
struct pfsync_softc;

union inet_template {
	struct ip	ipv4;
	struct ip6_hdr	ipv6;
};

#define PFSYNC_MINPKT ( \
	sizeof(union inet_template) + \
	sizeof(struct pfsync_header) + \
	sizeof(struct pfsync_subheader) )

static int	pfsync_upd_tcp(struct pf_kstate *, struct pfsync_state_peer *,
		    struct pfsync_state_peer *);
static int	pfsync_in_clr(struct mbuf *, int, int, int, int);
static int	pfsync_in_ins(struct mbuf *, int, int, int, int);
static int	pfsync_in_iack(struct mbuf *, int, int, int, int);
static int	pfsync_in_upd(struct mbuf *, int, int, int, int);
static int	pfsync_in_upd_c(struct mbuf *, int, int, int, int);
static int	pfsync_in_ureq(struct mbuf *, int, int, int, int);
static int	pfsync_in_del_c(struct mbuf *, int, int, int, int);
static int	pfsync_in_bus(struct mbuf *, int, int, int, int);
static int	pfsync_in_tdb(struct mbuf *, int, int, int, int);
static int	pfsync_in_eof(struct mbuf *, int, int, int, int);
static int	pfsync_in_error(struct mbuf *, int, int, int, int);

static int (*pfsync_acts[])(struct mbuf *, int, int, int, int) = {
	pfsync_in_clr,			/* PFSYNC_ACT_CLR */
	pfsync_in_ins,			/* PFSYNC_ACT_INS_1301 */
	pfsync_in_iack,			/* PFSYNC_ACT_INS_ACK */
	pfsync_in_upd,			/* PFSYNC_ACT_UPD_1301 */
	pfsync_in_upd_c,		/* PFSYNC_ACT_UPD_C */
	pfsync_in_ureq,			/* PFSYNC_ACT_UPD_REQ */
	pfsync_in_error,		/* PFSYNC_ACT_DEL */
	pfsync_in_del_c,		/* PFSYNC_ACT_DEL_C */
	pfsync_in_error,		/* PFSYNC_ACT_INS_F */
	pfsync_in_error,		/* PFSYNC_ACT_DEL_F */
	pfsync_in_bus,			/* PFSYNC_ACT_BUS */
	pfsync_in_tdb,			/* PFSYNC_ACT_TDB */
	pfsync_in_eof,			/* PFSYNC_ACT_EOF */
	pfsync_in_ins,			/* PFSYNC_ACT_INS_1400 */
	pfsync_in_upd,			/* PFSYNC_ACT_UPD_1400 */
};

struct pfsync_q {
	void		(*write)(struct pf_kstate *, void *);
	size_t		len;
	u_int8_t	action;
};

/* We have the following sync queues */
enum pfsync_q_id {
	PFSYNC_Q_INS_1301,
	PFSYNC_Q_INS_1400,
	PFSYNC_Q_IACK,
	PFSYNC_Q_UPD_1301,
	PFSYNC_Q_UPD_1400,
	PFSYNC_Q_UPD_C,
	PFSYNC_Q_DEL_C,
	PFSYNC_Q_COUNT,
};

/* Functions for building messages for given queue */
static void	pfsync_out_state_1301(struct pf_kstate *, void *);
static void	pfsync_out_state_1400(struct pf_kstate *, void *);
static void	pfsync_out_iack(struct pf_kstate *, void *);
static void	pfsync_out_upd_c(struct pf_kstate *, void *);
static void	pfsync_out_del_c(struct pf_kstate *, void *);

/* Attach those functions to queue */
static struct pfsync_q pfsync_qs[] = {
	{ pfsync_out_state_1301, sizeof(struct pfsync_state_1301), PFSYNC_ACT_INS_1301 },
	{ pfsync_out_state_1400, sizeof(struct pfsync_state_1400), PFSYNC_ACT_INS_1400 },
	{ pfsync_out_iack,       sizeof(struct pfsync_ins_ack),    PFSYNC_ACT_INS_ACK },
	{ pfsync_out_state_1301, sizeof(struct pfsync_state_1301), PFSYNC_ACT_UPD_1301 },
	{ pfsync_out_state_1400, sizeof(struct pfsync_state_1400), PFSYNC_ACT_UPD_1400 },
	{ pfsync_out_upd_c,      sizeof(struct pfsync_upd_c),      PFSYNC_ACT_UPD_C },
	{ pfsync_out_del_c,      sizeof(struct pfsync_del_c),      PFSYNC_ACT_DEL_C }
};

/* Map queue to pf_kstate->sync_state */
static u_int8_t pfsync_qid_sstate[] = {
	PFSYNC_S_INS,   /* PFSYNC_Q_INS_1301 */
	PFSYNC_S_INS,   /* PFSYNC_Q_INS_1400 */
	PFSYNC_S_IACK,  /* PFSYNC_Q_IACK */
	PFSYNC_S_UPD,   /* PFSYNC_Q_UPD_1301 */
	PFSYNC_S_UPD,   /* PFSYNC_Q_UPD_1400 */
	PFSYNC_S_UPD_C, /* PFSYNC_Q_UPD_C */
	PFSYNC_S_DEL_C, /* PFSYNC_Q_DEL_C */
};

/* Map pf_kstate->sync_state to queue */
static enum pfsync_q_id pfsync_sstate_to_qid(u_int8_t);

static void	pfsync_q_ins(struct pf_kstate *, int sync_state, bool);
static void	pfsync_q_del(struct pf_kstate *, bool, struct pfsync_bucket *);

static void	pfsync_update_state(struct pf_kstate *);
static void	pfsync_tx(struct pfsync_softc *, struct mbuf *);

struct pfsync_upd_req_item {
	TAILQ_ENTRY(pfsync_upd_req_item)	ur_entry;
	struct pfsync_upd_req			ur_msg;
};

struct pfsync_deferral {
	struct pfsync_softc		*pd_sc;
	TAILQ_ENTRY(pfsync_deferral)	pd_entry;
	struct callout			pd_tmo;

	struct pf_kstate		*pd_st;
	struct mbuf			*pd_m;
};

struct pfsync_bucket
{
	int			b_id;
	struct pfsync_softc	*b_sc;
	struct mtx		b_mtx;
	struct callout		b_tmo;
	int			b_flags;
#define	PFSYNCF_BUCKET_PUSH	0x00000001

	size_t			b_len;
	TAILQ_HEAD(, pf_kstate)			b_qs[PFSYNC_Q_COUNT];
	TAILQ_HEAD(, pfsync_upd_req_item)	b_upd_req_list;
	TAILQ_HEAD(, pfsync_deferral)		b_deferrals;
	u_int			b_deferred;
	uint8_t			*b_plus;
	size_t			b_pluslen;

	struct  ifaltq b_snd;
};

struct pfsync_softc {
	/* Configuration */
	struct ifnet		*sc_ifp;
	struct ifnet		*sc_sync_if;
	struct ip_moptions	sc_imo;
	struct ip6_moptions	sc_im6o;
	struct sockaddr_storage	sc_sync_peer;
	uint32_t		sc_flags;
	uint8_t			sc_maxupdates;
	union inet_template     sc_template;
	struct mtx		sc_mtx;
	uint32_t		sc_version;

	/* Queued data */
	struct pfsync_bucket	*sc_buckets;

	/* Bulk update info */
	struct mtx		sc_bulk_mtx;
	uint32_t		sc_ureq_sent;
	int			sc_bulk_tries;
	uint32_t		sc_ureq_received;
	int			sc_bulk_hashid;
	uint64_t		sc_bulk_stateid;
	uint32_t		sc_bulk_creatorid;
	struct callout		sc_bulk_tmo;
	struct callout		sc_bulkfail_tmo;
};

#define	PFSYNC_LOCK(sc)		mtx_lock(&(sc)->sc_mtx)
#define	PFSYNC_UNLOCK(sc)	mtx_unlock(&(sc)->sc_mtx)
#define	PFSYNC_LOCK_ASSERT(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)

#define PFSYNC_BUCKET_LOCK(b)		mtx_lock(&(b)->b_mtx)
#define PFSYNC_BUCKET_UNLOCK(b)		mtx_unlock(&(b)->b_mtx)
#define PFSYNC_BUCKET_LOCK_ASSERT(b)	mtx_assert(&(b)->b_mtx, MA_OWNED)

#define	PFSYNC_BLOCK(sc)	mtx_lock(&(sc)->sc_bulk_mtx)
#define	PFSYNC_BUNLOCK(sc)	mtx_unlock(&(sc)->sc_bulk_mtx)
#define	PFSYNC_BLOCK_ASSERT(sc)	mtx_assert(&(sc)->sc_bulk_mtx, MA_OWNED)

#define PFSYNC_DEFER_TIMEOUT	20

static const char pfsyncname[] = "pfsync";
static MALLOC_DEFINE(M_PFSYNC, pfsyncname, "pfsync(4) data");
VNET_DEFINE_STATIC(struct pfsync_softc	*, pfsyncif) = NULL;
#define	V_pfsyncif		VNET(pfsyncif)
VNET_DEFINE_STATIC(void *, pfsync_swi_cookie) = NULL;
#define	V_pfsync_swi_cookie	VNET(pfsync_swi_cookie)
VNET_DEFINE_STATIC(struct intr_event *, pfsync_swi_ie);
#define	V_pfsync_swi_ie		VNET(pfsync_swi_ie)
VNET_DEFINE_STATIC(struct pfsyncstats, pfsyncstats);
#define	V_pfsyncstats		VNET(pfsyncstats)
VNET_DEFINE_STATIC(int, pfsync_carp_adj) = CARP_MAXSKEW;
#define	V_pfsync_carp_adj	VNET(pfsync_carp_adj)
VNET_DEFINE_STATIC(unsigned int, pfsync_defer_timeout) = PFSYNC_DEFER_TIMEOUT;
#define	V_pfsync_defer_timeout	VNET(pfsync_defer_timeout)

static void	pfsync_timeout(void *);
static void	pfsync_push(struct pfsync_bucket *);
static void	pfsync_push_all(struct pfsync_softc *);
static void	pfsyncintr(void *);
static int	pfsync_multicast_setup(struct pfsync_softc *, struct ifnet *,
		    struct in_mfilter *, struct in6_mfilter *);
static void	pfsync_multicast_cleanup(struct pfsync_softc *);
static void	pfsync_pointers_init(void);
static void	pfsync_pointers_uninit(void);
static int	pfsync_init(void);
static void	pfsync_uninit(void);

static unsigned long pfsync_buckets;

SYSCTL_NODE(_net, OID_AUTO, pfsync, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "PFSYNC");
SYSCTL_STRUCT(_net_pfsync, OID_AUTO, stats, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(pfsyncstats), pfsyncstats,
    "PFSYNC statistics (struct pfsyncstats, net/if_pfsync.h)");
SYSCTL_INT(_net_pfsync, OID_AUTO, carp_demotion_factor, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(pfsync_carp_adj), 0, "pfsync's CARP demotion factor adjustment");
SYSCTL_ULONG(_net_pfsync, OID_AUTO, pfsync_buckets, CTLFLAG_RDTUN,
    &pfsync_buckets, 0, "Number of pfsync hash buckets");
SYSCTL_UINT(_net_pfsync, OID_AUTO, defer_delay, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(pfsync_defer_timeout), 0, "Deferred packet timeout (in ms)");

static int	pfsync_clone_create(struct if_clone *, int, caddr_t);
static void	pfsync_clone_destroy(struct ifnet *);
static int	pfsync_alloc_scrub_memory(struct pfsync_state_peer *,
		    struct pf_state_peer *);
static int	pfsyncoutput(struct ifnet *, struct mbuf *,
		    const struct sockaddr *, struct route *);
static int	pfsyncioctl(struct ifnet *, u_long, caddr_t);

static int	pfsync_defer(struct pf_kstate *, struct mbuf *);
static void	pfsync_undefer(struct pfsync_deferral *, int);
static void	pfsync_undefer_state_locked(struct pf_kstate *, int);
static void	pfsync_undefer_state(struct pf_kstate *, int);
static void	pfsync_defer_tmo(void *);

static void	pfsync_request_update(u_int32_t, u_int64_t);
static bool	pfsync_update_state_req(struct pf_kstate *);

static void	pfsync_drop_all(struct pfsync_softc *);
static void	pfsync_drop(struct pfsync_softc *, int);
static void	pfsync_sendout(int, int);
static void	pfsync_send_plus(void *, size_t);

static void	pfsync_bulk_start(void);
static void	pfsync_bulk_status(u_int8_t);
static void	pfsync_bulk_update(void *);
static void	pfsync_bulk_fail(void *);

static void	pfsync_detach_ifnet(struct ifnet *);

static int pfsync_pfsyncreq_to_kstatus(struct pfsyncreq *,
    struct pfsync_kstatus *);
static int pfsync_kstatus_to_softc(struct pfsync_kstatus *,
    struct pfsync_softc *);

#ifdef IPSEC
static void	pfsync_update_net_tdb(struct pfsync_tdb *);
#endif
static struct pfsync_bucket	*pfsync_get_bucket(struct pfsync_softc *,
		    struct pf_kstate *);

#define PFSYNC_MAX_BULKTRIES	12

VNET_DEFINE(struct if_clone *, pfsync_cloner);
#define	V_pfsync_cloner	VNET(pfsync_cloner)

const struct in6_addr in6addr_linklocal_pfsync_group =
	{{{ 0xff, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0 }}};
static int
pfsync_clone_create(struct if_clone *ifc, int unit, caddr_t param)
{
	struct pfsync_softc *sc;
	struct ifnet *ifp;
	struct pfsync_bucket *b;
	int c;
	enum pfsync_q_id q;

	if (unit != 0)
		return (EINVAL);

	if (! pfsync_buckets)
		pfsync_buckets = mp_ncpus * 2;

	sc = malloc(sizeof(struct pfsync_softc), M_PFSYNC, M_WAITOK | M_ZERO);
	sc->sc_flags |= PFSYNCF_OK;
	sc->sc_maxupdates = 128;
	sc->sc_version = PFSYNC_MSG_VERSION_DEFAULT;

	ifp = sc->sc_ifp = if_alloc(IFT_PFSYNC);
	if_initname(ifp, pfsyncname, unit);
	ifp->if_softc = sc;
	ifp->if_ioctl = pfsyncioctl;
	ifp->if_output = pfsyncoutput;
	ifp->if_type = IFT_PFSYNC;
	ifp->if_hdrlen = sizeof(struct pfsync_header);
	ifp->if_mtu = ETHERMTU;
	mtx_init(&sc->sc_mtx, pfsyncname, NULL, MTX_DEF);
	mtx_init(&sc->sc_bulk_mtx, "pfsync bulk", NULL, MTX_DEF);
	callout_init_mtx(&sc->sc_bulk_tmo, &sc->sc_bulk_mtx, 0);
	callout_init_mtx(&sc->sc_bulkfail_tmo, &sc->sc_bulk_mtx, 0);

	if_attach(ifp);

	bpfattach(ifp, DLT_PFSYNC, PFSYNC_HDRLEN);

	sc->sc_buckets = mallocarray(pfsync_buckets, sizeof(*sc->sc_buckets),
	    M_PFSYNC, M_ZERO | M_WAITOK);
	for (c = 0; c < pfsync_buckets; c++) {
		b = &sc->sc_buckets[c];
		mtx_init(&b->b_mtx, "pfsync bucket", NULL, MTX_DEF);

		b->b_id = c;
		b->b_sc = sc;
		b->b_len = PFSYNC_MINPKT;

		for (q = 0; q < PFSYNC_Q_COUNT; q++)
			TAILQ_INIT(&b->b_qs[q]);

		TAILQ_INIT(&b->b_upd_req_list);
		TAILQ_INIT(&b->b_deferrals);

		callout_init(&b->b_tmo, 1);

		b->b_snd.ifq_maxlen = ifqmaxlen;
	}

	V_pfsyncif = sc;

	return (0);
}

static void
pfsync_clone_destroy(struct ifnet *ifp)
{
	struct pfsync_softc *sc = ifp->if_softc;
	struct pfsync_bucket *b;
	int c, ret;

	for (c = 0; c < pfsync_buckets; c++) {
		b = &sc->sc_buckets[c];
		/*
		 * At this stage, everything should have already been
		 * cleared by pfsync_uninit(), and we have only to
		 * drain callouts.
		 */
		PFSYNC_BUCKET_LOCK(b);
		while (b->b_deferred > 0) {
			struct pfsync_deferral *pd =
			    TAILQ_FIRST(&b->b_deferrals);

			ret = callout_stop(&pd->pd_tmo);
			PFSYNC_BUCKET_UNLOCK(b);
			if (ret > 0) {
				pfsync_undefer(pd, 1);
			} else {
				callout_drain(&pd->pd_tmo);
			}
			PFSYNC_BUCKET_LOCK(b);
		}
		MPASS(b->b_deferred == 0);
		MPASS(TAILQ_EMPTY(&b->b_deferrals));
		PFSYNC_BUCKET_UNLOCK(b);

		free(b->b_plus, M_PFSYNC);
		b->b_plus = NULL;
		b->b_pluslen = 0;

		callout_drain(&b->b_tmo);
	}

	callout_drain(&sc->sc_bulkfail_tmo);
	callout_drain(&sc->sc_bulk_tmo);

	if (!(sc->sc_flags & PFSYNCF_OK) && carp_demote_adj_p)
		(*carp_demote_adj_p)(-V_pfsync_carp_adj, "pfsync destroy");
	bpfdetach(ifp);
	if_detach(ifp);

	pfsync_drop_all(sc);

	if_free(ifp);
	pfsync_multicast_cleanup(sc);
	mtx_destroy(&sc->sc_mtx);
	mtx_destroy(&sc->sc_bulk_mtx);

	free(sc->sc_buckets, M_PFSYNC);
	free(sc, M_PFSYNC);

	V_pfsyncif = NULL;
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
pfsync_state_import(union pfsync_state_union *sp, int flags, int msg_version)
{
	struct pfsync_softc *sc = V_pfsyncif;
#ifndef	__NO_STRICT_ALIGNMENT
	struct pfsync_state_key key[2];
#endif
	struct pfsync_state_key *kw, *ks;
	struct pf_kstate	*st = NULL;
	struct pf_state_key	*skw = NULL, *sks = NULL;
	struct pf_krule		*r = NULL;
	struct pfi_kkif		*kif;
	struct pfi_kkif		*rt_kif = NULL;
	struct pf_kpooladdr	*rpool_first;
	int			 error;
	uint8_t			 rt = 0;

	PF_RULES_RASSERT();

	if (sp->pfs_1301.creatorid == 0) {
		if (V_pf_status.debug >= PF_DEBUG_MISC)
			printf("%s: invalid creator id: %08x\n", __func__,
			    ntohl(sp->pfs_1301.creatorid));
		return (EINVAL);
	}

	if ((kif = pfi_kkif_find(sp->pfs_1301.ifname)) == NULL) {
		if (V_pf_status.debug >= PF_DEBUG_MISC)
			printf("%s: unknown interface: %s\n", __func__,
			    sp->pfs_1301.ifname);
		if (flags & PFSYNC_SI_IOCTL)
			return (EINVAL);
		return (0);	/* skip this state */
	}

	/*
	 * If the ruleset checksums match or the state is coming from the ioctl,
	 * it's safe to associate the state with the rule of that number.
	 */
	if (sp->pfs_1301.rule != htonl(-1) && sp->pfs_1301.anchor == htonl(-1) &&
	    (flags & (PFSYNC_SI_IOCTL | PFSYNC_SI_CKSUM)) && ntohl(sp->pfs_1301.rule) <
	    pf_main_ruleset.rules[PF_RULESET_FILTER].active.rcount)
		r = pf_main_ruleset.rules[
		    PF_RULESET_FILTER].active.ptr_array[ntohl(sp->pfs_1301.rule)];
	else
		r = &V_pf_default_rule;

	/*
	 * Check routing interface early on. Do it before allocating memory etc.
	 * because there is a high chance there will be a lot more such states.
	 */
	switch (msg_version) {
	case PFSYNC_MSG_VERSION_1301:
		/*
		 * On FreeBSD <= 13 the routing interface and routing operation
		 * are not sent over pfsync. If the ruleset is identical,
		 * though, we might be able to recover the routing information
		 * from the local ruleset.
		 */
		if (r != &V_pf_default_rule) {
			struct pf_kpool		*pool = &r->route;

			/* Backwards compatibility. */
			if (TAILQ_EMPTY(&pool->list))
				pool = &r->rdr;

			/*
			 * The ruleset is identical, try to recover. If the rule
			 * has a redirection pool with a single interface, there
			 * is a chance that this interface is identical as on
			 * the pfsync peer. If there's more than one interface,
			 * give up, as we can't be sure that we will pick the
			 * same one as the pfsync peer did.
			 */
			rpool_first = TAILQ_FIRST(&(pool->list));
			if ((rpool_first == NULL) ||
			    (TAILQ_NEXT(rpool_first, entries) != NULL)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("%s: can't recover routing information "
				    "because of empty or bad redirection pool\n",
				    __func__));
				return ((flags & PFSYNC_SI_IOCTL) ? EINVAL : 0);
			}
			rt = r->rt;
			rt_kif = rpool_first->kif;
		} else if (!PF_AZERO(&sp->pfs_1301.rt_addr, sp->pfs_1301.af)) {
			/*
			 * Ruleset different, routing *supposedly* requested,
			 * give up on recovering.
			 */
			DPFPRINTF(PF_DEBUG_MISC,
			    ("%s: can't recover routing information "
			    "because of different ruleset\n", __func__));
			return ((flags & PFSYNC_SI_IOCTL) ? EINVAL : 0);
		}
	break;
	case PFSYNC_MSG_VERSION_1400:
		/*
		 * On FreeBSD 14 and above we're not taking any chances.
		 * We use the information synced to us.
		 */
		if (sp->pfs_1400.rt) {
			rt_kif = pfi_kkif_find(sp->pfs_1400.rt_ifname);
			if (rt_kif == NULL) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("%s: unknown route interface: %s\n",
				    __func__, sp->pfs_1400.rt_ifname));
				return ((flags & PFSYNC_SI_IOCTL) ? EINVAL : 0);
			}
			rt = sp->pfs_1400.rt;
		}
	break;
	}

	if ((r->max_states &&
	    counter_u64_fetch(r->states_cur) >= r->max_states))
		goto cleanup;

	/*
	 * XXXGL: consider M_WAITOK in ioctl path after.
	 */
	st = pf_alloc_state(M_NOWAIT);
	if (__predict_false(st == NULL))
		goto cleanup;

	if ((skw = uma_zalloc(V_pf_state_key_z, M_NOWAIT)) == NULL)
		goto cleanup;

#ifndef	__NO_STRICT_ALIGNMENT
	bcopy(&sp->pfs_1301.key, key, sizeof(struct pfsync_state_key) * 2);
	kw = &key[PF_SK_WIRE];
	ks = &key[PF_SK_STACK];
#else
	kw = &sp->pfs_1301.key[PF_SK_WIRE];
	ks = &sp->pfs_1301.key[PF_SK_STACK];
#endif

	if (PF_ANEQ(&kw->addr[0], &ks->addr[0], sp->pfs_1301.af) ||
	    PF_ANEQ(&kw->addr[1], &ks->addr[1], sp->pfs_1301.af) ||
	    kw->port[0] != ks->port[0] ||
	    kw->port[1] != ks->port[1]) {
		sks = uma_zalloc(V_pf_state_key_z, M_NOWAIT);
		if (sks == NULL)
			goto cleanup;
	} else
		sks = skw;

	/* allocate memory for scrub info */
	if (pfsync_alloc_scrub_memory(&sp->pfs_1301.src, &st->src) ||
	    pfsync_alloc_scrub_memory(&sp->pfs_1301.dst, &st->dst))
		goto cleanup;

	/* Copy to state key(s). */
	skw->addr[0] = kw->addr[0];
	skw->addr[1] = kw->addr[1];
	skw->port[0] = kw->port[0];
	skw->port[1] = kw->port[1];
	skw->proto = sp->pfs_1301.proto;
	skw->af = sp->pfs_1301.af;
	if (sks != skw) {
		sks->addr[0] = ks->addr[0];
		sks->addr[1] = ks->addr[1];
		sks->port[0] = ks->port[0];
		sks->port[1] = ks->port[1];
		sks->proto = sp->pfs_1301.proto;
		sks->af = sp->pfs_1301.af;
	}

	/* copy to state */
	bcopy(&sp->pfs_1301.rt_addr, &st->act.rt_addr, sizeof(st->act.rt_addr));
	st->creation = (time_uptime - ntohl(sp->pfs_1301.creation)) * 1000;
	st->expire = pf_get_uptime();
	if (sp->pfs_1301.expire) {
		uint32_t timeout;

		timeout = r->timeout[sp->pfs_1301.timeout];
		if (!timeout)
			timeout = V_pf_default_rule.timeout[sp->pfs_1301.timeout];

		/* sp->expire may have been adaptively scaled by export. */
		st->expire -= (timeout - ntohl(sp->pfs_1301.expire)) * 1000;
	}

	st->direction = sp->pfs_1301.direction;
	st->act.log = sp->pfs_1301.log;
	st->timeout = sp->pfs_1301.timeout;

	st->act.rt = rt;
	st->act.rt_kif = rt_kif;

	switch (msg_version) {
		case PFSYNC_MSG_VERSION_1301:
			st->state_flags = sp->pfs_1301.state_flags;
			/*
			 * In FreeBSD 13 pfsync lacks many attributes. Copy them
			 * from the rule if possible. If rule can't be matched
			 * clear any set options as we can't recover their
			 * parameters.
			*/
			if (r == &V_pf_default_rule) {
				st->state_flags &= ~PFSTATE_SETMASK;
			} else {
				/*
				 * Similar to pf_rule_to_actions(). This code
				 * won't set the actions properly if they come
				 * from multiple "match" rules as only rule
				 * creating the state is send over pfsync.
				 */
				st->act.qid = r->qid;
				st->act.pqid = r->pqid;
				st->act.rtableid = r->rtableid;
				if (r->scrub_flags & PFSTATE_SETTOS)
					st->act.set_tos = r->set_tos;
				st->act.min_ttl = r->min_ttl;
				st->act.max_mss = r->max_mss;
				st->state_flags |= (r->scrub_flags &
				    (PFSTATE_NODF|PFSTATE_RANDOMID|
				    PFSTATE_SETTOS|PFSTATE_SCRUB_TCP|
				    PFSTATE_SETPRIO));
				if (r->dnpipe || r->dnrpipe) {
					if (r->free_flags & PFRULE_DN_IS_PIPE)
						st->state_flags |= PFSTATE_DN_IS_PIPE;
					else
						st->state_flags &= ~PFSTATE_DN_IS_PIPE;
				}
				st->act.dnpipe = r->dnpipe;
				st->act.dnrpipe = r->dnrpipe;
			}
			break;
		case PFSYNC_MSG_VERSION_1400:
			st->state_flags = ntohs(sp->pfs_1400.state_flags);
			st->act.qid = ntohs(sp->pfs_1400.qid);
			st->act.pqid = ntohs(sp->pfs_1400.pqid);
			st->act.dnpipe = ntohs(sp->pfs_1400.dnpipe);
			st->act.dnrpipe = ntohs(sp->pfs_1400.dnrpipe);
			st->act.rtableid = ntohl(sp->pfs_1400.rtableid);
			st->act.min_ttl = sp->pfs_1400.min_ttl;
			st->act.set_tos = sp->pfs_1400.set_tos;
			st->act.max_mss = ntohs(sp->pfs_1400.max_mss);
			st->act.set_prio[0] = sp->pfs_1400.set_prio[0];
			st->act.set_prio[1] = sp->pfs_1400.set_prio[1];
			break;
		default:
			panic("%s: Unsupported pfsync_msg_version %d",
			    __func__, msg_version);
	}

	st->id = sp->pfs_1301.id;
	st->creatorid = sp->pfs_1301.creatorid;
	pf_state_peer_ntoh(&sp->pfs_1301.src, &st->src);
	pf_state_peer_ntoh(&sp->pfs_1301.dst, &st->dst);

	st->rule = r;
	st->nat_rule = NULL;
	st->anchor = NULL;

	st->pfsync_time = time_uptime;
	st->sync_state = PFSYNC_S_NONE;

	if (!(flags & PFSYNC_SI_IOCTL))
		st->state_flags |= PFSTATE_NOSYNC;

	if ((error = pf_state_insert(kif, kif, skw, sks, st)) != 0)
		goto cleanup_state;

	/* XXX when we have nat_rule/anchors, use STATE_INC_COUNTERS */
	counter_u64_add(r->states_cur, 1);
	counter_u64_add(r->states_tot, 1);

	if (!(flags & PFSYNC_SI_IOCTL)) {
		st->state_flags &= ~PFSTATE_NOSYNC;
		if (st->state_flags & PFSTATE_ACK) {
			struct pfsync_bucket *b = pfsync_get_bucket(sc, st);
			PFSYNC_BUCKET_LOCK(b);
			pfsync_q_ins(st, PFSYNC_S_IACK, true);
			PFSYNC_BUCKET_UNLOCK(b);

			pfsync_push_all(sc);
		}
	}
	st->state_flags &= ~PFSTATE_ACK;
	PF_STATE_UNLOCK(st);

	return (0);

cleanup:
	error = ENOMEM;

	if (skw == sks)
		sks = NULL;
	uma_zfree(V_pf_state_key_z, skw);
	uma_zfree(V_pf_state_key_z, sks);

cleanup_state:	/* pf_state_insert() frees the state keys. */
	if (st) {
		st->timeout = PFTM_UNLINKED; /* appease an assert */
		pf_free_state(st);
	}
	return (error);
}

#ifdef INET
static int
pfsync_input(struct mbuf **mp, int *offp __unused, int proto __unused)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct mbuf *m = *mp;
	struct ip *ip = mtod(m, struct ip *);
	struct pfsync_header *ph;
	struct pfsync_subheader subh;

	int offset, len, flags = 0;
	int rv;
	uint16_t count;

	PF_RULES_RLOCK_TRACKER;

	*mp = NULL;
	V_pfsyncstats.pfsyncs_ipackets++;

	/* Verify that we have a sync interface configured. */
	if (!sc || !sc->sc_sync_if || !V_pf_status.running ||
	    (sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		goto done;

	/* verify that the packet came in on the right interface */
	if (sc->sc_sync_if != m->m_pkthdr.rcvif) {
		V_pfsyncstats.pfsyncs_badif++;
		goto done;
	}

	if_inc_counter(sc->sc_ifp, IFCOUNTER_IPACKETS, 1);
	if_inc_counter(sc->sc_ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
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
			return (IPPROTO_DONE);
		}
		ip = mtod(m, struct ip *);
	}
	ph = (struct pfsync_header *)((char *)ip + offset);

	/* verify the version */
	if (ph->version != PFSYNC_VERSION) {
		V_pfsyncstats.pfsyncs_badver++;
		goto done;
	}

	len = ntohs(ph->len) + offset;
	if (m->m_pkthdr.len < len) {
		V_pfsyncstats.pfsyncs_badlen++;
		goto done;
	}

	/*
	 * Trusting pf_chksum during packet processing, as well as seeking
	 * in interface name tree, require holding PF_RULES_RLOCK().
	 */
	PF_RULES_RLOCK();
	if (!bcmp(&ph->pfcksum, &V_pf_status.pf_chksum, PF_MD5_DIGEST_LENGTH))
		flags = PFSYNC_SI_CKSUM;

	offset += sizeof(*ph);
	while (offset <= len - sizeof(subh)) {
		m_copydata(m, offset, sizeof(subh), (caddr_t)&subh);
		offset += sizeof(subh);

		if (subh.action >= PFSYNC_ACT_MAX) {
			V_pfsyncstats.pfsyncs_badact++;
			PF_RULES_RUNLOCK();
			goto done;
		}

		count = ntohs(subh.count);
		V_pfsyncstats.pfsyncs_iacts[subh.action] += count;
		rv = (*pfsync_acts[subh.action])(m, offset, count, flags, subh.action);
		if (rv == -1) {
			PF_RULES_RUNLOCK();
			return (IPPROTO_DONE);
		}

		offset += rv;
	}
	PF_RULES_RUNLOCK();

done:
	m_freem(m);
	return (IPPROTO_DONE);
}
#endif

#ifdef INET6
static int
pfsync6_input(struct mbuf **mp, int *offp __unused, int proto __unused)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct pfsync_header *ph;
	struct pfsync_subheader subh;

	int offset, len, flags = 0;
	int rv;
	uint16_t count;

	PF_RULES_RLOCK_TRACKER;

	*mp = NULL;
	V_pfsyncstats.pfsyncs_ipackets++;

	/* Verify that we have a sync interface configured. */
	if (!sc || !sc->sc_sync_if || !V_pf_status.running ||
	    (sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		goto done;

	/* verify that the packet came in on the right interface */
	if (sc->sc_sync_if != m->m_pkthdr.rcvif) {
		V_pfsyncstats.pfsyncs_badif++;
		goto done;
	}

	if_inc_counter(sc->sc_ifp, IFCOUNTER_IPACKETS, 1);
	if_inc_counter(sc->sc_ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
	/* verify that the IP TTL is 255. */
	if (ip6->ip6_hlim != PFSYNC_DFLTTL) {
		V_pfsyncstats.pfsyncs_badttl++;
		goto done;
	}


	offset = sizeof(*ip6);
	if (m->m_pkthdr.len < offset + sizeof(*ph)) {
		V_pfsyncstats.pfsyncs_hdrops++;
		goto done;
	}

	if (offset + sizeof(*ph) > m->m_len) {
		if (m_pullup(m, offset + sizeof(*ph)) == NULL) {
			V_pfsyncstats.pfsyncs_hdrops++;
			return (IPPROTO_DONE);
		}
		ip6 = mtod(m, struct ip6_hdr *);
	}
	ph = (struct pfsync_header *)((char *)ip6 + offset);

	/* verify the version */
	if (ph->version != PFSYNC_VERSION) {
		V_pfsyncstats.pfsyncs_badver++;
		goto done;
	}

	len = ntohs(ph->len) + offset;
	if (m->m_pkthdr.len < len) {
		V_pfsyncstats.pfsyncs_badlen++;
		goto done;
	}

	/*
	 * Trusting pf_chksum during packet processing, as well as seeking
	 * in interface name tree, require holding PF_RULES_RLOCK().
	 */
	PF_RULES_RLOCK();
	if (!bcmp(&ph->pfcksum, &V_pf_status.pf_chksum, PF_MD5_DIGEST_LENGTH))
		flags = PFSYNC_SI_CKSUM;

	offset += sizeof(*ph);
	while (offset <= len - sizeof(subh)) {
		m_copydata(m, offset, sizeof(subh), (caddr_t)&subh);
		offset += sizeof(subh);

		if (subh.action >= PFSYNC_ACT_MAX) {
			V_pfsyncstats.pfsyncs_badact++;
			PF_RULES_RUNLOCK();
			goto done;
		}

		count = ntohs(subh.count);
		V_pfsyncstats.pfsyncs_iacts[subh.action] += count;
		rv = (*pfsync_acts[subh.action])(m, offset, count, flags, subh.action);
		if (rv == -1) {
			PF_RULES_RUNLOCK();
			return (IPPROTO_DONE);
		}

		offset += rv;
	}
	PF_RULES_RUNLOCK();

done:
	m_freem(m);
	return (IPPROTO_DONE);
}
#endif

static int
pfsync_in_clr(struct mbuf *m, int offset, int count, int flags, int action)
{
	struct pfsync_clr *clr;
	struct mbuf *mp;
	int len = sizeof(*clr) * count;
	int i, offp;
	u_int32_t creatorid;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	clr = (struct pfsync_clr *)(mp->m_data + offp);

	for (i = 0; i < count; i++) {
		creatorid = clr[i].creatorid;

		if (clr[i].ifname[0] != '\0' &&
		    pfi_kkif_find(clr[i].ifname) == NULL)
			continue;

		for (int i = 0; i <= V_pf_hashmask; i++) {
			struct pf_idhash *ih = &V_pf_idhash[i];
			struct pf_kstate *s;
relock:
			PF_HASHROW_LOCK(ih);
			LIST_FOREACH(s, &ih->states, entry) {
				if (s->creatorid == creatorid) {
					s->state_flags |= PFSTATE_NOSYNC;
					pf_remove_state(s);
					goto relock;
				}
			}
			PF_HASHROW_UNLOCK(ih);
		}
	}

	return (len);
}

static int
pfsync_in_ins(struct mbuf *m, int offset, int count, int flags, int action)
{
	struct mbuf *mp;
	union pfsync_state_union *sa, *sp;
	int i, offp, total_len, msg_version, msg_len;

	switch (action) {
		case PFSYNC_ACT_INS_1301:
			msg_len = sizeof(struct pfsync_state_1301);
			total_len = msg_len * count;
			msg_version = PFSYNC_MSG_VERSION_1301;
			break;
		case PFSYNC_ACT_INS_1400:
			msg_len = sizeof(struct pfsync_state_1400);
			total_len = msg_len * count;
			msg_version = PFSYNC_MSG_VERSION_1400;
			break;
		default:
			V_pfsyncstats.pfsyncs_badact++;
			return (-1);
	}

	mp = m_pulldown(m, offset, total_len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	sa = (union pfsync_state_union *)(mp->m_data + offp);

	for (i = 0; i < count; i++) {
		sp = (union pfsync_state_union *)((char *)sa + msg_len * i);

		/* Check for invalid values. */
		if (sp->pfs_1301.timeout >= PFTM_MAX ||
		    sp->pfs_1301.src.state > PF_TCPS_PROXY_DST ||
		    sp->pfs_1301.dst.state > PF_TCPS_PROXY_DST ||
		    sp->pfs_1301.direction > PF_OUT ||
		    (sp->pfs_1301.af != AF_INET &&
		    sp->pfs_1301.af != AF_INET6)) {
			if (V_pf_status.debug >= PF_DEBUG_MISC)
				printf("%s: invalid value\n", __func__);
			V_pfsyncstats.pfsyncs_badval++;
			continue;
		}

		if (pfsync_state_import(sp, flags, msg_version) == ENOMEM)
			/* Drop out, but process the rest of the actions. */
			break;
	}

	return (total_len);
}

static int
pfsync_in_iack(struct mbuf *m, int offset, int count, int flags, int action)
{
	struct pfsync_ins_ack *ia, *iaa;
	struct pf_kstate *st;

	struct mbuf *mp;
	int len = count * sizeof(*ia);
	int offp, i;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	iaa = (struct pfsync_ins_ack *)(mp->m_data + offp);

	for (i = 0; i < count; i++) {
		ia = &iaa[i];

		st = pf_find_state_byid(ia->id, ia->creatorid);
		if (st == NULL)
			continue;

		if (st->state_flags & PFSTATE_ACK) {
			pfsync_undefer_state(st, 0);
		}
		PF_STATE_UNLOCK(st);
	}
	/*
	 * XXX this is not yet implemented, but we know the size of the
	 * message so we can skip it.
	 */

	return (count * sizeof(struct pfsync_ins_ack));
}

static int
pfsync_upd_tcp(struct pf_kstate *st, struct pfsync_state_peer *src,
    struct pfsync_state_peer *dst)
{
	int sync = 0;

	PF_STATE_LOCK_ASSERT(st);

	/*
	 * The state should never go backwards except
	 * for syn-proxy states.  Neither should the
	 * sequence window slide backwards.
	 */
	if ((st->src.state > src->state &&
	    (st->src.state < PF_TCPS_PROXY_SRC ||
	    src->state >= PF_TCPS_PROXY_SRC)) ||

	    (st->src.state == src->state &&
	    SEQ_GT(st->src.seqlo, ntohl(src->seqlo))))
		sync++;
	else
		pf_state_peer_ntoh(src, &st->src);

	if ((st->dst.state > dst->state) ||

	    (st->dst.state >= TCPS_SYN_SENT &&
	    SEQ_GT(st->dst.seqlo, ntohl(dst->seqlo))))
		sync++;
	else
		pf_state_peer_ntoh(dst, &st->dst);

	return (sync);
}

static int
pfsync_in_upd(struct mbuf *m, int offset, int count, int flags, int action)
{
	struct pfsync_softc *sc = V_pfsyncif;
	union pfsync_state_union *sa, *sp;
	struct pf_kstate *st;
	struct mbuf *mp;
	int sync, offp, i, total_len, msg_len, msg_version;

	switch (action) {
		case PFSYNC_ACT_UPD_1301:
			msg_len = sizeof(struct pfsync_state_1301);
			total_len = msg_len * count;
			msg_version = PFSYNC_MSG_VERSION_1301;
			break;
		case PFSYNC_ACT_UPD_1400:
			msg_len = sizeof(struct pfsync_state_1400);
			total_len = msg_len * count;
			msg_version = PFSYNC_MSG_VERSION_1400;
			break;
		default:
			V_pfsyncstats.pfsyncs_badact++;
			return (-1);
	}

	mp = m_pulldown(m, offset, total_len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	sa = (union pfsync_state_union *)(mp->m_data + offp);

	for (i = 0; i < count; i++) {
		sp = (union pfsync_state_union *)((char *)sa + msg_len * i);

		/* check for invalid values */
		if (sp->pfs_1301.timeout >= PFTM_MAX ||
		    sp->pfs_1301.src.state > PF_TCPS_PROXY_DST ||
		    sp->pfs_1301.dst.state > PF_TCPS_PROXY_DST) {
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
				printf("pfsync_input: PFSYNC_ACT_UPD: "
				    "invalid value\n");
			}
			V_pfsyncstats.pfsyncs_badval++;
			continue;
		}

		st = pf_find_state_byid(sp->pfs_1301.id, sp->pfs_1301.creatorid);
		if (st == NULL) {
			/* insert the update */
			if (pfsync_state_import(sp, flags, msg_version))
				V_pfsyncstats.pfsyncs_badstate++;
			continue;
		}

		if (st->state_flags & PFSTATE_ACK) {
			pfsync_undefer_state(st, 1);
		}

		if (st->key[PF_SK_WIRE]->proto == IPPROTO_TCP)
			sync = pfsync_upd_tcp(st, &sp->pfs_1301.src, &sp->pfs_1301.dst);
		else {
			sync = 0;

			/*
			 * Non-TCP protocol state machine always go
			 * forwards
			 */
			if (st->src.state > sp->pfs_1301.src.state)
				sync++;
			else
				pf_state_peer_ntoh(&sp->pfs_1301.src, &st->src);
			if (st->dst.state > sp->pfs_1301.dst.state)
				sync++;
			else
				pf_state_peer_ntoh(&sp->pfs_1301.dst, &st->dst);
		}
		if (sync < 2) {
			pfsync_alloc_scrub_memory(&sp->pfs_1301.dst, &st->dst);
			pf_state_peer_ntoh(&sp->pfs_1301.dst, &st->dst);
			st->expire = pf_get_uptime();
			st->timeout = sp->pfs_1301.timeout;
		}
		st->pfsync_time = time_uptime;

		if (sync) {
			V_pfsyncstats.pfsyncs_stale++;

			pfsync_update_state(st);
			PF_STATE_UNLOCK(st);
			pfsync_push_all(sc);
			continue;
		}
		PF_STATE_UNLOCK(st);
	}

	return (total_len);
}

static int
pfsync_in_upd_c(struct mbuf *m, int offset, int count, int flags, int action)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_upd_c *ua, *up;
	struct pf_kstate *st;
	int len = count * sizeof(*up);
	int sync;
	struct mbuf *mp;
	int offp, i;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	ua = (struct pfsync_upd_c *)(mp->m_data + offp);

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

		st = pf_find_state_byid(up->id, up->creatorid);
		if (st == NULL) {
			/* We don't have this state. Ask for it. */
			PFSYNC_BUCKET_LOCK(&sc->sc_buckets[0]);
			pfsync_request_update(up->creatorid, up->id);
			PFSYNC_BUCKET_UNLOCK(&sc->sc_buckets[0]);
			continue;
		}

		if (st->state_flags & PFSTATE_ACK) {
			pfsync_undefer_state(st, 1);
		}

		if (st->key[PF_SK_WIRE]->proto == IPPROTO_TCP)
			sync = pfsync_upd_tcp(st, &up->src, &up->dst);
		else {
			sync = 0;

			/*
			 * Non-TCP protocol state machine always go
			 * forwards
			 */
			if (st->src.state > up->src.state)
				sync++;
			else
				pf_state_peer_ntoh(&up->src, &st->src);
			if (st->dst.state > up->dst.state)
				sync++;
			else
				pf_state_peer_ntoh(&up->dst, &st->dst);
		}
		if (sync < 2) {
			pfsync_alloc_scrub_memory(&up->dst, &st->dst);
			pf_state_peer_ntoh(&up->dst, &st->dst);
			st->expire = pf_get_uptime();
			st->timeout = up->timeout;
		}
		st->pfsync_time = time_uptime;

		if (sync) {
			V_pfsyncstats.pfsyncs_stale++;

			pfsync_update_state(st);
			PF_STATE_UNLOCK(st);
			pfsync_push_all(sc);
			continue;
		}
		PF_STATE_UNLOCK(st);
	}

	return (len);
}

static int
pfsync_in_ureq(struct mbuf *m, int offset, int count, int flags, int action)
{
	struct pfsync_upd_req *ur, *ura;
	struct mbuf *mp;
	int len = count * sizeof(*ur);
	int i, offp;

	struct pf_kstate *st;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	ura = (struct pfsync_upd_req *)(mp->m_data + offp);

	for (i = 0; i < count; i++) {
		ur = &ura[i];

		if (ur->id == 0 && ur->creatorid == 0)
			pfsync_bulk_start();
		else {
			st = pf_find_state_byid(ur->id, ur->creatorid);
			if (st == NULL) {
				V_pfsyncstats.pfsyncs_badstate++;
				continue;
			}
			if (st->state_flags & PFSTATE_NOSYNC) {
				PF_STATE_UNLOCK(st);
				continue;
			}

			pfsync_update_state_req(st);
			PF_STATE_UNLOCK(st);
		}
	}

	return (len);
}

static int
pfsync_in_del_c(struct mbuf *m, int offset, int count, int flags, int action)
{
	struct mbuf *mp;
	struct pfsync_del_c *sa, *sp;
	struct pf_kstate *st;
	int len = count * sizeof(*sp);
	int offp, i;

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	sa = (struct pfsync_del_c *)(mp->m_data + offp);

	for (i = 0; i < count; i++) {
		sp = &sa[i];

		st = pf_find_state_byid(sp->id, sp->creatorid);
		if (st == NULL) {
			V_pfsyncstats.pfsyncs_badstate++;
			continue;
		}

		st->state_flags |= PFSTATE_NOSYNC;
		pf_remove_state(st);
	}

	return (len);
}

static int
pfsync_in_bus(struct mbuf *m, int offset, int count, int flags, int action)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_bus *bus;
	struct mbuf *mp;
	int len = count * sizeof(*bus);
	int offp;

	PFSYNC_BLOCK(sc);

	/* If we're not waiting for a bulk update, who cares. */
	if (sc->sc_ureq_sent == 0) {
		PFSYNC_BUNLOCK(sc);
		return (len);
	}

	mp = m_pulldown(m, offset, len, &offp);
	if (mp == NULL) {
		PFSYNC_BUNLOCK(sc);
		V_pfsyncstats.pfsyncs_badlen++;
		return (-1);
	}
	bus = (struct pfsync_bus *)(mp->m_data + offp);

	switch (bus->status) {
	case PFSYNC_BUS_START:
		callout_reset(&sc->sc_bulkfail_tmo, 4 * hz +
		    V_pf_limits[PF_LIMIT_STATES].limit /
		    ((sc->sc_ifp->if_mtu - PFSYNC_MINPKT) /
		    sizeof(union pfsync_state_union)),
		    pfsync_bulk_fail, sc);
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
			if (!(sc->sc_flags & PFSYNCF_OK) && carp_demote_adj_p)
				(*carp_demote_adj_p)(-V_pfsync_carp_adj,
				    "pfsync bulk done");
			sc->sc_flags |= PFSYNCF_OK;
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
	PFSYNC_BUNLOCK(sc);

	return (len);
}

static int
pfsync_in_tdb(struct mbuf *m, int offset, int count, int flags, int action)
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

	for (i = 0; i < count; i++)
		pfsync_update_net_tdb(&tp[i]);
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
pfsync_in_eof(struct mbuf *m, int offset, int count, int flags, int action)
{
	/* check if we are at the right place in the packet */
	if (offset != m->m_pkthdr.len)
		V_pfsyncstats.pfsyncs_badlen++;

	/* we're done. free and let the caller return */
	m_freem(m);
	return (-1);
}

static int
pfsync_in_error(struct mbuf *m, int offset, int count, int flags, int action)
{
	V_pfsyncstats.pfsyncs_badact++;

	m_freem(m);
	return (-1);
}

static int
pfsyncoutput(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
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
	struct pfsyncreq pfsyncr;
	size_t nvbuflen;
	int error;
	int c;

	switch (cmd) {
	case SIOCSIFFLAGS:
		PFSYNC_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			ifp->if_drv_flags |= IFF_DRV_RUNNING;
			PFSYNC_UNLOCK(sc);
			pfsync_pointers_init();
		} else {
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			PFSYNC_UNLOCK(sc);
			pfsync_pointers_uninit();
		}
		break;
	case SIOCSIFMTU:
		if (!sc->sc_sync_if ||
		    ifr->ifr_mtu <= PFSYNC_MINPKT ||
		    ifr->ifr_mtu > sc->sc_sync_if->if_mtu)
			return (EINVAL);
		if (ifr->ifr_mtu < ifp->if_mtu) {
			for (c = 0; c < pfsync_buckets; c++) {
				PFSYNC_BUCKET_LOCK(&sc->sc_buckets[c]);
				if (sc->sc_buckets[c].b_len > PFSYNC_MINPKT)
					pfsync_sendout(1, c);
				PFSYNC_BUCKET_UNLOCK(&sc->sc_buckets[c]);
			}
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCGETPFSYNC:
		bzero(&pfsyncr, sizeof(pfsyncr));
		PFSYNC_LOCK(sc);
		if (sc->sc_sync_if) {
			strlcpy(pfsyncr.pfsyncr_syncdev,
			    sc->sc_sync_if->if_xname, IFNAMSIZ);
		}
		pfsyncr.pfsyncr_syncpeer = ((struct sockaddr_in *)&sc->sc_sync_peer)->sin_addr;
		pfsyncr.pfsyncr_maxupdates = sc->sc_maxupdates;
		pfsyncr.pfsyncr_defer = sc->sc_flags;
		PFSYNC_UNLOCK(sc);
		return (copyout(&pfsyncr, ifr_data_get_ptr(ifr),
		    sizeof(pfsyncr)));

	case SIOCGETPFSYNCNV:
	    {
		nvlist_t *nvl_syncpeer;
		nvlist_t *nvl = nvlist_create(0);

		if (nvl == NULL)
			return (ENOMEM);

		if (sc->sc_sync_if)
			nvlist_add_string(nvl, "syncdev", sc->sc_sync_if->if_xname);
		nvlist_add_number(nvl, "maxupdates", sc->sc_maxupdates);
		nvlist_add_number(nvl, "flags", sc->sc_flags);
		nvlist_add_number(nvl, "version", sc->sc_version);
		if ((nvl_syncpeer = pfsync_sockaddr_to_syncpeer_nvlist(&sc->sc_sync_peer)) != NULL)
			nvlist_add_nvlist(nvl, "syncpeer", nvl_syncpeer);

		void *packed = NULL;
		packed = nvlist_pack(nvl, &nvbuflen);
		if (packed == NULL) {
			free(packed, M_NVLIST);
			nvlist_destroy(nvl);
			return (ENOMEM);
		}

		if (nvbuflen > ifr->ifr_cap_nv.buf_length) {
			ifr->ifr_cap_nv.length = nvbuflen;
			ifr->ifr_cap_nv.buffer = NULL;
			free(packed, M_NVLIST);
			nvlist_destroy(nvl);
			return (EFBIG);
		}

		ifr->ifr_cap_nv.length = nvbuflen;
		error = copyout(packed, ifr->ifr_cap_nv.buffer, nvbuflen);

		nvlist_destroy(nvl);
		nvlist_destroy(nvl_syncpeer);
		free(packed, M_NVLIST);
		break;
	    }

	case SIOCSETPFSYNC:
	    {
		struct pfsync_kstatus status;

		if ((error = priv_check(curthread, PRIV_NETINET_PF)) != 0)
			return (error);
		if ((error = copyin(ifr_data_get_ptr(ifr), &pfsyncr,
		    sizeof(pfsyncr))))
			return (error);

		memset((char *)&status, 0, sizeof(struct pfsync_kstatus));
		pfsync_pfsyncreq_to_kstatus(&pfsyncr, &status);

		error = pfsync_kstatus_to_softc(&status, sc);
		return (error);
	    }
	case SIOCSETPFSYNCNV:
	    {
		struct pfsync_kstatus status;
		void *data;
		nvlist_t *nvl;

		if ((error = priv_check(curthread, PRIV_NETINET_PF)) != 0)
			return (error);
		if (ifr->ifr_cap_nv.length > IFR_CAP_NV_MAXBUFSIZE)
			return (EINVAL);

		data = malloc(ifr->ifr_cap_nv.length, M_TEMP, M_WAITOK);

		if ((error = copyin(ifr->ifr_cap_nv.buffer, data,
		    ifr->ifr_cap_nv.length)) != 0) {
			free(data, M_TEMP);
			return (error);
		}

		if ((nvl = nvlist_unpack(data, ifr->ifr_cap_nv.length, 0)) == NULL) {
			free(data, M_TEMP);
			return (EINVAL);
		}

		memset((char *)&status, 0, sizeof(struct pfsync_kstatus));
		pfsync_nvstatus_to_kstatus(nvl, &status);

		nvlist_destroy(nvl);
		free(data, M_TEMP);

		error = pfsync_kstatus_to_softc(&status, sc);
		return (error);
	    }
	default:
		return (ENOTTY);
	}

	return (0);
}

static void
pfsync_out_state_1301(struct pf_kstate *st, void *buf)
{
	union pfsync_state_union *sp = buf;

	pfsync_state_export(sp, st, PFSYNC_MSG_VERSION_1301);
}

static void
pfsync_out_state_1400(struct pf_kstate *st, void *buf)
{
	union pfsync_state_union *sp = buf;

	pfsync_state_export(sp, st, PFSYNC_MSG_VERSION_1400);
}

static void
pfsync_out_iack(struct pf_kstate *st, void *buf)
{
	struct pfsync_ins_ack *iack = buf;

	iack->id = st->id;
	iack->creatorid = st->creatorid;
}

static void
pfsync_out_upd_c(struct pf_kstate *st, void *buf)
{
	struct pfsync_upd_c *up = buf;

	bzero(up, sizeof(*up));
	up->id = st->id;
	pf_state_peer_hton(&st->src, &up->src);
	pf_state_peer_hton(&st->dst, &up->dst);
	up->creatorid = st->creatorid;
	up->timeout = st->timeout;
}

static void
pfsync_out_del_c(struct pf_kstate *st, void *buf)
{
	struct pfsync_del_c *dp = buf;

	dp->id = st->id;
	dp->creatorid = st->creatorid;
	st->state_flags |= PFSTATE_NOSYNC;
}

static void
pfsync_drop_all(struct pfsync_softc *sc)
{
	struct pfsync_bucket *b;
	int c;

	for (c = 0; c < pfsync_buckets; c++) {
		b = &sc->sc_buckets[c];

		PFSYNC_BUCKET_LOCK(b);
		pfsync_drop(sc, c);
		PFSYNC_BUCKET_UNLOCK(b);
	}
}

static void
pfsync_drop(struct pfsync_softc *sc, int c)
{
	struct pf_kstate *st, *next;
	struct pfsync_upd_req_item *ur;
	struct pfsync_bucket *b;
	enum pfsync_q_id q;

	b = &sc->sc_buckets[c];
	PFSYNC_BUCKET_LOCK_ASSERT(b);

	for (q = 0; q < PFSYNC_Q_COUNT; q++) {
		if (TAILQ_EMPTY(&b->b_qs[q]))
			continue;

		TAILQ_FOREACH_SAFE(st, &b->b_qs[q], sync_list, next) {
			KASSERT(st->sync_state == pfsync_qid_sstate[q],
				("%s: st->sync_state %d == q %d",
					__func__, st->sync_state, q));
			st->sync_state = PFSYNC_S_NONE;
			pf_release_state(st);
		}
		TAILQ_INIT(&b->b_qs[q]);
	}

	while ((ur = TAILQ_FIRST(&b->b_upd_req_list)) != NULL) {
		TAILQ_REMOVE(&b->b_upd_req_list, ur, ur_entry);
		free(ur, M_PFSYNC);
	}

	b->b_len = PFSYNC_MINPKT;
	free(b->b_plus, M_PFSYNC);
	b->b_plus = NULL;
	b->b_pluslen = 0;
}

static void
pfsync_sendout(int schedswi, int c)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	struct pfsync_header *ph;
	struct pfsync_subheader *subh;
	struct pf_kstate *st, *st_next;
	struct pfsync_upd_req_item *ur;
	struct pfsync_bucket *b = &sc->sc_buckets[c];
	size_t len;
	int aflen, offset, count = 0;
	enum pfsync_q_id q;

	KASSERT(sc != NULL, ("%s: null sc", __func__));
	KASSERT(b->b_len > PFSYNC_MINPKT,
	    ("%s: sc_len %zu", __func__, b->b_len));
	PFSYNC_BUCKET_LOCK_ASSERT(b);

	if (!bpf_peers_present(ifp->if_bpf) && sc->sc_sync_if == NULL) {
		pfsync_drop(sc, c);
		return;
	}

	m = m_get2(max_linkhdr + b->b_len, M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL) {
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, 1);
		V_pfsyncstats.pfsyncs_onomem++;
		return;
	}
	m->m_data += max_linkhdr;
	bzero(m->m_data, b->b_len);

	len = b->b_len;

	/* build the ip header */
	switch (sc->sc_sync_peer.ss_family) {
#ifdef INET
	case AF_INET:
	    {
		struct ip *ip;

		ip = mtod(m, struct ip *);
		bcopy(&sc->sc_template.ipv4, ip, sizeof(*ip));
		aflen = offset = sizeof(*ip);

		len -= sizeof(union inet_template) - sizeof(struct ip);
		ip->ip_len = htons(len);
		ip_fillid(ip, V_ip_random_id);
		break;
	    }
#endif
#ifdef INET6
	case AF_INET6:
		{
		struct ip6_hdr *ip6;

		ip6 = mtod(m, struct ip6_hdr *);
		bcopy(&sc->sc_template.ipv6, ip6, sizeof(*ip6));
		aflen = offset = sizeof(*ip6);

		len -= sizeof(union inet_template) - sizeof(struct ip6_hdr);
		ip6->ip6_plen = htons(len);
		break;
		}
#endif
	default:
		m_freem(m);
		pfsync_drop(sc, c);
		return;
	}
	m->m_len = m->m_pkthdr.len = len;

	/* build the pfsync header */
	ph = (struct pfsync_header *)(m->m_data + offset);
	offset += sizeof(*ph);

	ph->version = PFSYNC_VERSION;
	ph->len = htons(len - aflen);
	bcopy(V_pf_status.pf_chksum, ph->pfcksum, PF_MD5_DIGEST_LENGTH);

	/* walk the queues */
	for (q = 0; q < PFSYNC_Q_COUNT; q++) {
		if (TAILQ_EMPTY(&b->b_qs[q]))
			continue;

		subh = (struct pfsync_subheader *)(m->m_data + offset);
		offset += sizeof(*subh);

		count = 0;
		TAILQ_FOREACH_SAFE(st, &b->b_qs[q], sync_list, st_next) {
			KASSERT(st->sync_state == pfsync_qid_sstate[q],
				("%s: st->sync_state == q",
					__func__));
			/*
			 * XXXGL: some of write methods do unlocked reads
			 * of state data :(
			 */
			pfsync_qs[q].write(st, m->m_data + offset);
			offset += pfsync_qs[q].len;
			st->sync_state = PFSYNC_S_NONE;
			pf_release_state(st);
			count++;
		}
		TAILQ_INIT(&b->b_qs[q]);

		subh->action = pfsync_qs[q].action;
		subh->count = htons(count);
		V_pfsyncstats.pfsyncs_oacts[pfsync_qs[q].action] += count;
	}

	if (!TAILQ_EMPTY(&b->b_upd_req_list)) {
		subh = (struct pfsync_subheader *)(m->m_data + offset);
		offset += sizeof(*subh);

		count = 0;
		while ((ur = TAILQ_FIRST(&b->b_upd_req_list)) != NULL) {
			TAILQ_REMOVE(&b->b_upd_req_list, ur, ur_entry);

			bcopy(&ur->ur_msg, m->m_data + offset,
			    sizeof(ur->ur_msg));
			offset += sizeof(ur->ur_msg);
			free(ur, M_PFSYNC);
			count++;
		}

		subh->action = PFSYNC_ACT_UPD_REQ;
		subh->count = htons(count);
		V_pfsyncstats.pfsyncs_oacts[PFSYNC_ACT_UPD_REQ] += count;
	}

	/* has someone built a custom region for us to add? */
	if (b->b_plus != NULL) {
		bcopy(b->b_plus, m->m_data + offset, b->b_pluslen);
		offset += b->b_pluslen;

		free(b->b_plus, M_PFSYNC);
		b->b_plus = NULL;
		b->b_pluslen = 0;
	}

	subh = (struct pfsync_subheader *)(m->m_data + offset);
	offset += sizeof(*subh);

	subh->action = PFSYNC_ACT_EOF;
	subh->count = htons(1);
	V_pfsyncstats.pfsyncs_oacts[PFSYNC_ACT_EOF]++;

	/* we're done, let's put it on the wire */
	if (bpf_peers_present(ifp->if_bpf)) {
		m->m_data += aflen;
		m->m_len = m->m_pkthdr.len = len - aflen;
		bpf_mtap(ifp->if_bpf, m);
		m->m_data -= aflen;
		m->m_len = m->m_pkthdr.len = len;
	}

	if (sc->sc_sync_if == NULL) {
		b->b_len = PFSYNC_MINPKT;
		m_freem(m);
		return;
	}

	if_inc_counter(sc->sc_ifp, IFCOUNTER_OPACKETS, 1);
	if_inc_counter(sc->sc_ifp, IFCOUNTER_OBYTES, m->m_pkthdr.len);
	b->b_len = PFSYNC_MINPKT;

	if (!_IF_QFULL(&b->b_snd))
		_IF_ENQUEUE(&b->b_snd, m);
	else {
		m_freem(m);
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OQDROPS, 1);
	}
	if (schedswi)
		swi_sched(V_pfsync_swi_cookie, 0);
}

static void
pfsync_insert_state(struct pf_kstate *st)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_bucket *b = pfsync_get_bucket(sc, st);

	if (st->state_flags & PFSTATE_NOSYNC)
		return;

	if ((st->rule->rule_flag & PFRULE_NOSYNC) ||
	    st->key[PF_SK_WIRE]->proto == IPPROTO_PFSYNC) {
		st->state_flags |= PFSTATE_NOSYNC;
		return;
	}

	KASSERT(st->sync_state == PFSYNC_S_NONE,
		("%s: st->sync_state %u", __func__, st->sync_state));

	PFSYNC_BUCKET_LOCK(b);
	if (b->b_len == PFSYNC_MINPKT)
		callout_reset(&b->b_tmo, 1 * hz, pfsync_timeout, b);

	pfsync_q_ins(st, PFSYNC_S_INS, true);
	PFSYNC_BUCKET_UNLOCK(b);

	st->sync_updates = 0;
}

static int
pfsync_defer(struct pf_kstate *st, struct mbuf *m)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_deferral *pd;
	struct pfsync_bucket *b;

	if (m->m_flags & (M_BCAST|M_MCAST))
		return (0);

	if (sc == NULL)
		return (0);

	b = pfsync_get_bucket(sc, st);

	PFSYNC_LOCK(sc);

	if (!(sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING) ||
	    !(sc->sc_flags & PFSYNCF_DEFER)) {
		PFSYNC_UNLOCK(sc);
		return (0);
	}

	PFSYNC_BUCKET_LOCK(b);
	PFSYNC_UNLOCK(sc);

	if (b->b_deferred >= 128)
		pfsync_undefer(TAILQ_FIRST(&b->b_deferrals), 0);

	pd = malloc(sizeof(*pd), M_PFSYNC, M_NOWAIT);
	if (pd == NULL) {
		PFSYNC_BUCKET_UNLOCK(b);
		return (0);
	}
	b->b_deferred++;

	m->m_flags |= M_SKIP_FIREWALL;
	st->state_flags |= PFSTATE_ACK;

	pd->pd_sc = sc;
	pd->pd_st = st;
	pf_ref_state(st);
	pd->pd_m = m;

	TAILQ_INSERT_TAIL(&b->b_deferrals, pd, pd_entry);
	callout_init_mtx(&pd->pd_tmo, &b->b_mtx, CALLOUT_RETURNUNLOCKED);
	callout_reset(&pd->pd_tmo, (V_pfsync_defer_timeout * hz) / 1000,
	    pfsync_defer_tmo, pd);

	pfsync_push(b);
	PFSYNC_BUCKET_UNLOCK(b);

	return (1);
}

static void
pfsync_undefer(struct pfsync_deferral *pd, int drop)
{
	struct pfsync_softc *sc = pd->pd_sc;
	struct mbuf *m = pd->pd_m;
	struct pf_kstate *st = pd->pd_st;
	struct pfsync_bucket *b = pfsync_get_bucket(sc, st);

	PFSYNC_BUCKET_LOCK_ASSERT(b);

	TAILQ_REMOVE(&b->b_deferrals, pd, pd_entry);
	b->b_deferred--;
	pd->pd_st->state_flags &= ~PFSTATE_ACK;	/* XXX: locking! */
	free(pd, M_PFSYNC);
	pf_release_state(st);

	if (drop)
		m_freem(m);
	else {
		_IF_ENQUEUE(&b->b_snd, m);
		pfsync_push(b);
	}
}

static void
pfsync_defer_tmo(void *arg)
{
	struct epoch_tracker et;
	struct pfsync_deferral *pd = arg;
	struct pfsync_softc *sc = pd->pd_sc;
	struct mbuf *m = pd->pd_m;
	struct pf_kstate *st = pd->pd_st;
	struct pfsync_bucket *b;

	CURVNET_SET(sc->sc_ifp->if_vnet);

	b = pfsync_get_bucket(sc, st);

	PFSYNC_BUCKET_LOCK_ASSERT(b);

	TAILQ_REMOVE(&b->b_deferrals, pd, pd_entry);
	b->b_deferred--;
	pd->pd_st->state_flags &= ~PFSTATE_ACK;	/* XXX: locking! */
	PFSYNC_BUCKET_UNLOCK(b);
	free(pd, M_PFSYNC);

	if (sc->sc_sync_if == NULL) {
		pf_release_state(st);
		m_freem(m);
		CURVNET_RESTORE();
		return;
	}

	NET_EPOCH_ENTER(et);

	pfsync_tx(sc, m);

	pf_release_state(st);

	CURVNET_RESTORE();
	NET_EPOCH_EXIT(et);
}

static void
pfsync_undefer_state_locked(struct pf_kstate *st, int drop)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_deferral *pd;
	struct pfsync_bucket *b = pfsync_get_bucket(sc, st);

	PFSYNC_BUCKET_LOCK_ASSERT(b);

	TAILQ_FOREACH(pd, &b->b_deferrals, pd_entry) {
		 if (pd->pd_st == st) {
			if (callout_stop(&pd->pd_tmo) > 0)
				pfsync_undefer(pd, drop);

			return;
		}
	}

	panic("%s: unable to find deferred state", __func__);
}

static void
pfsync_undefer_state(struct pf_kstate *st, int drop)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_bucket *b = pfsync_get_bucket(sc, st);

	PFSYNC_BUCKET_LOCK(b);
	pfsync_undefer_state_locked(st, drop);
	PFSYNC_BUCKET_UNLOCK(b);
}

static struct pfsync_bucket*
pfsync_get_bucket(struct pfsync_softc *sc, struct pf_kstate *st)
{
	int c = PF_IDHASH(st) % pfsync_buckets;
	return &sc->sc_buckets[c];
}

static void
pfsync_update_state(struct pf_kstate *st)
{
	struct pfsync_softc *sc = V_pfsyncif;
	bool sync = false, ref = true;
	struct pfsync_bucket *b = pfsync_get_bucket(sc, st);

	PF_STATE_LOCK_ASSERT(st);
	PFSYNC_BUCKET_LOCK(b);

	if (st->state_flags & PFSTATE_ACK)
		pfsync_undefer_state_locked(st, 0);
	if (st->state_flags & PFSTATE_NOSYNC) {
		if (st->sync_state != PFSYNC_S_NONE)
			pfsync_q_del(st, true, b);
		PFSYNC_BUCKET_UNLOCK(b);
		return;
	}

	if (b->b_len == PFSYNC_MINPKT)
		callout_reset(&b->b_tmo, 1 * hz, pfsync_timeout, b);

	switch (st->sync_state) {
	case PFSYNC_S_UPD_C:
	case PFSYNC_S_UPD:
	case PFSYNC_S_INS:
		/* we're already handling it */

		if (st->key[PF_SK_WIRE]->proto == IPPROTO_TCP) {
			st->sync_updates++;
			if (st->sync_updates >= sc->sc_maxupdates)
				sync = true;
		}
		break;

	case PFSYNC_S_IACK:
		pfsync_q_del(st, false, b);
		ref = false;
		/* FALLTHROUGH */

	case PFSYNC_S_NONE:
		pfsync_q_ins(st, PFSYNC_S_UPD_C, ref);
		st->sync_updates = 0;
		break;

	default:
		panic("%s: unexpected sync state %d", __func__, st->sync_state);
	}

	if (sync || (time_uptime - st->pfsync_time) < 2)
		pfsync_push(b);

	PFSYNC_BUCKET_UNLOCK(b);
}

static void
pfsync_request_update(u_int32_t creatorid, u_int64_t id)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_bucket *b = &sc->sc_buckets[0];
	struct pfsync_upd_req_item *item;
	size_t nlen = sizeof(struct pfsync_upd_req);

	PFSYNC_BUCKET_LOCK_ASSERT(b);

	/*
	 * This code does a bit to prevent multiple update requests for the
	 * same state being generated. It searches current subheader queue,
	 * but it doesn't lookup into queue of already packed datagrams.
	 */
	TAILQ_FOREACH(item, &b->b_upd_req_list, ur_entry)
		if (item->ur_msg.id == id &&
		    item->ur_msg.creatorid == creatorid)
			return;

	item = malloc(sizeof(*item), M_PFSYNC, M_NOWAIT);
	if (item == NULL)
		return; /* XXX stats */

	item->ur_msg.id = id;
	item->ur_msg.creatorid = creatorid;

	if (TAILQ_EMPTY(&b->b_upd_req_list))
		nlen += sizeof(struct pfsync_subheader);

	if (b->b_len + nlen > sc->sc_ifp->if_mtu) {
		pfsync_sendout(0, 0);

		nlen = sizeof(struct pfsync_subheader) +
		    sizeof(struct pfsync_upd_req);
	}

	TAILQ_INSERT_TAIL(&b->b_upd_req_list, item, ur_entry);
	b->b_len += nlen;

	pfsync_push(b);
}

static bool
pfsync_update_state_req(struct pf_kstate *st)
{
	struct pfsync_softc *sc = V_pfsyncif;
	bool ref = true, full = false;
	struct pfsync_bucket *b = pfsync_get_bucket(sc, st);

	PF_STATE_LOCK_ASSERT(st);
	PFSYNC_BUCKET_LOCK(b);

	if (st->state_flags & PFSTATE_NOSYNC) {
		if (st->sync_state != PFSYNC_S_NONE)
			pfsync_q_del(st, true, b);
		PFSYNC_BUCKET_UNLOCK(b);
		return (full);
	}

	switch (st->sync_state) {
	case PFSYNC_S_UPD_C:
	case PFSYNC_S_IACK:
		pfsync_q_del(st, false, b);
		ref = false;
		/* FALLTHROUGH */

	case PFSYNC_S_NONE:
		pfsync_q_ins(st, PFSYNC_S_UPD, ref);
		pfsync_push(b);
		break;

	case PFSYNC_S_INS:
	case PFSYNC_S_UPD:
	case PFSYNC_S_DEL_C:
		/* we're already handling it */
		break;

	default:
		panic("%s: unexpected sync state %d", __func__, st->sync_state);
	}

	if ((sc->sc_ifp->if_mtu - b->b_len) < sizeof(union pfsync_state_union))
		full = true;

	PFSYNC_BUCKET_UNLOCK(b);

	return (full);
}

static void
pfsync_delete_state(struct pf_kstate *st)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_bucket *b = pfsync_get_bucket(sc, st);
	bool ref = true;

	PFSYNC_BUCKET_LOCK(b);
	if (st->state_flags & PFSTATE_ACK)
		pfsync_undefer_state_locked(st, 1);
	if (st->state_flags & PFSTATE_NOSYNC) {
		if (st->sync_state != PFSYNC_S_NONE)
			pfsync_q_del(st, true, b);
		PFSYNC_BUCKET_UNLOCK(b);
		return;
	}

	if (b->b_len == PFSYNC_MINPKT)
		callout_reset(&b->b_tmo, 1 * hz, pfsync_timeout, b);

	switch (st->sync_state) {
	case PFSYNC_S_INS:
		/* We never got to tell the world so just forget about it. */
		pfsync_q_del(st, true, b);
		break;

	case PFSYNC_S_UPD_C:
	case PFSYNC_S_UPD:
	case PFSYNC_S_IACK:
		pfsync_q_del(st, false, b);
		ref = false;
		/* FALLTHROUGH */

	case PFSYNC_S_NONE:
		pfsync_q_ins(st, PFSYNC_S_DEL_C, ref);
		break;

	default:
		panic("%s: unexpected sync state %d", __func__, st->sync_state);
	}

	PFSYNC_BUCKET_UNLOCK(b);
}

static void
pfsync_clear_states(u_int32_t creatorid, const char *ifname)
{
	struct {
		struct pfsync_subheader subh;
		struct pfsync_clr clr;
	} __packed r;

	bzero(&r, sizeof(r));

	r.subh.action = PFSYNC_ACT_CLR;
	r.subh.count = htons(1);
	V_pfsyncstats.pfsyncs_oacts[PFSYNC_ACT_CLR]++;

	strlcpy(r.clr.ifname, ifname, sizeof(r.clr.ifname));
	r.clr.creatorid = creatorid;

	pfsync_send_plus(&r, sizeof(r));
}

static enum pfsync_q_id
pfsync_sstate_to_qid(u_int8_t sync_state)
{
	struct pfsync_softc *sc = V_pfsyncif;

	switch (sync_state) {
		case PFSYNC_S_INS:
			switch (sc->sc_version) {
				case PFSYNC_MSG_VERSION_1301:
					return PFSYNC_Q_INS_1301;
				case PFSYNC_MSG_VERSION_1400:
					return PFSYNC_Q_INS_1400;
			}
			break;
		case PFSYNC_S_IACK:
			return PFSYNC_Q_IACK;
		case PFSYNC_S_UPD:
			switch (sc->sc_version) {
				case PFSYNC_MSG_VERSION_1301:
					return PFSYNC_Q_UPD_1301;
				case PFSYNC_MSG_VERSION_1400:
					return PFSYNC_Q_UPD_1400;
			}
			break;
		case PFSYNC_S_UPD_C:
			return PFSYNC_Q_UPD_C;
		case PFSYNC_S_DEL_C:
			return PFSYNC_Q_DEL_C;
		default:
			panic("%s: Unsupported st->sync_state 0x%02x",
			__func__, sync_state);
	}

	panic("%s: Unsupported pfsync_msg_version %d",
	    __func__, sc->sc_version);
}

static void
pfsync_q_ins(struct pf_kstate *st, int sync_state, bool ref)
{
	enum pfsync_q_id q = pfsync_sstate_to_qid(sync_state);
	struct pfsync_softc *sc = V_pfsyncif;
	size_t nlen = pfsync_qs[q].len;
	struct pfsync_bucket *b = pfsync_get_bucket(sc, st);

	PFSYNC_BUCKET_LOCK_ASSERT(b);

	KASSERT(st->sync_state == PFSYNC_S_NONE,
		("%s: st->sync_state %u", __func__, st->sync_state));
	KASSERT(b->b_len >= PFSYNC_MINPKT, ("pfsync pkt len is too low %zu",
	    b->b_len));

	if (TAILQ_EMPTY(&b->b_qs[q]))
		nlen += sizeof(struct pfsync_subheader);

	if (b->b_len + nlen > sc->sc_ifp->if_mtu) {
		pfsync_sendout(1, b->b_id);

		nlen = sizeof(struct pfsync_subheader) + pfsync_qs[q].len;
	}

	b->b_len += nlen;
	st->sync_state = pfsync_qid_sstate[q];
	TAILQ_INSERT_TAIL(&b->b_qs[q], st, sync_list);
	if (ref)
		pf_ref_state(st);
}

static void
pfsync_q_del(struct pf_kstate *st, bool unref, struct pfsync_bucket *b)
{
	enum pfsync_q_id q;

	PFSYNC_BUCKET_LOCK_ASSERT(b);
	KASSERT(st->sync_state != PFSYNC_S_NONE,
		("%s: st->sync_state != PFSYNC_S_NONE", __func__));

	q =  pfsync_sstate_to_qid(st->sync_state);
	b->b_len -= pfsync_qs[q].len;
	TAILQ_REMOVE(&b->b_qs[q], st, sync_list);
	st->sync_state = PFSYNC_S_NONE;
	if (unref)
		pf_release_state(st);

	if (TAILQ_EMPTY(&b->b_qs[q]))
		b->b_len -= sizeof(struct pfsync_subheader);
}

static void
pfsync_bulk_start(void)
{
	struct pfsync_softc *sc = V_pfsyncif;

	if (V_pf_status.debug >= PF_DEBUG_MISC)
		printf("pfsync: received bulk update request\n");

	PFSYNC_BLOCK(sc);

	sc->sc_ureq_received = time_uptime;
	sc->sc_bulk_hashid = 0;
	sc->sc_bulk_stateid = 0;
	pfsync_bulk_status(PFSYNC_BUS_START);
	callout_reset(&sc->sc_bulk_tmo, 1, pfsync_bulk_update, sc);
	PFSYNC_BUNLOCK(sc);
}

static void
pfsync_bulk_update(void *arg)
{
	struct pfsync_softc *sc = arg;
	struct pf_kstate *s;
	int i;

	PFSYNC_BLOCK_ASSERT(sc);
	CURVNET_SET(sc->sc_ifp->if_vnet);

	/*
	 * Start with last state from previous invocation.
	 * It may had gone, in this case start from the
	 * hash slot.
	 */
	s = pf_find_state_byid(sc->sc_bulk_stateid, sc->sc_bulk_creatorid);

	if (s != NULL)
		i = PF_IDHASH(s);
	else
		i = sc->sc_bulk_hashid;

	for (; i <= V_pf_hashmask; i++) {
		struct pf_idhash *ih = &V_pf_idhash[i];

		if (s != NULL)
			PF_HASHROW_ASSERT(ih);
		else {
			PF_HASHROW_LOCK(ih);
			s = LIST_FIRST(&ih->states);
		}

		for (; s; s = LIST_NEXT(s, entry)) {
			if (s->sync_state == PFSYNC_S_NONE &&
			    s->timeout < PFTM_MAX &&
			    s->pfsync_time <= sc->sc_ureq_received) {
				if (pfsync_update_state_req(s)) {
					/* We've filled a packet. */
					sc->sc_bulk_hashid = i;
					sc->sc_bulk_stateid = s->id;
					sc->sc_bulk_creatorid = s->creatorid;
					PF_HASHROW_UNLOCK(ih);
					callout_reset(&sc->sc_bulk_tmo, 1,
					    pfsync_bulk_update, sc);
					goto full;
				}
			}
		}
		PF_HASHROW_UNLOCK(ih);
	}

	/* We're done. */
	pfsync_bulk_status(PFSYNC_BUS_END);
full:
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

	bzero(&r, sizeof(r));

	r.subh.action = PFSYNC_ACT_BUS;
	r.subh.count = htons(1);
	V_pfsyncstats.pfsyncs_oacts[PFSYNC_ACT_BUS]++;

	r.bus.creatorid = V_pf_status.hostid;
	r.bus.endtime = htonl(time_uptime - sc->sc_ureq_received);
	r.bus.status = status;

	pfsync_send_plus(&r, sizeof(r));
}

static void
pfsync_bulk_fail(void *arg)
{
	struct pfsync_softc *sc = arg;
	struct pfsync_bucket *b = &sc->sc_buckets[0];

	CURVNET_SET(sc->sc_ifp->if_vnet);

	PFSYNC_BLOCK_ASSERT(sc);

	if (sc->sc_bulk_tries++ < PFSYNC_MAX_BULKTRIES) {
		/* Try again */
		callout_reset(&sc->sc_bulkfail_tmo, 5 * hz,
		    pfsync_bulk_fail, V_pfsyncif);
		PFSYNC_BUCKET_LOCK(b);
		pfsync_request_update(0, 0);
		PFSYNC_BUCKET_UNLOCK(b);
	} else {
		/* Pretend like the transfer was ok. */
		sc->sc_ureq_sent = 0;
		sc->sc_bulk_tries = 0;
		PFSYNC_LOCK(sc);
		if (!(sc->sc_flags & PFSYNCF_OK) && carp_demote_adj_p)
			(*carp_demote_adj_p)(-V_pfsync_carp_adj,
			    "pfsync bulk fail");
		sc->sc_flags |= PFSYNCF_OK;
		PFSYNC_UNLOCK(sc);
		if (V_pf_status.debug >= PF_DEBUG_MISC)
			printf("pfsync: failed to receive bulk update\n");
	}

	CURVNET_RESTORE();
}

static void
pfsync_send_plus(void *plus, size_t pluslen)
{
	struct pfsync_softc *sc = V_pfsyncif;
	struct pfsync_bucket *b = &sc->sc_buckets[0];
	uint8_t *newplus;

	PFSYNC_BUCKET_LOCK(b);

	if (b->b_len + pluslen > sc->sc_ifp->if_mtu)
		pfsync_sendout(1, b->b_id);

	newplus = malloc(pluslen + b->b_pluslen, M_PFSYNC, M_NOWAIT);
	if (newplus == NULL)
		goto out;

	if (b->b_plus != NULL) {
		memcpy(newplus, b->b_plus, b->b_pluslen);
		free(b->b_plus, M_PFSYNC);
	} else {
		MPASS(b->b_pluslen == 0);
	}
	memcpy(newplus + b->b_pluslen, plus, pluslen);

	b->b_plus = newplus;
	b->b_pluslen += pluslen;
	b->b_len += pluslen;

	pfsync_sendout(1, b->b_id);

out:
	PFSYNC_BUCKET_UNLOCK(b);
}

static void
pfsync_timeout(void *arg)
{
	struct pfsync_bucket *b = arg;

	CURVNET_SET(b->b_sc->sc_ifp->if_vnet);
	PFSYNC_BUCKET_LOCK(b);
	pfsync_push(b);
	PFSYNC_BUCKET_UNLOCK(b);
	CURVNET_RESTORE();
}

static void
pfsync_push(struct pfsync_bucket *b)
{

	PFSYNC_BUCKET_LOCK_ASSERT(b);

	b->b_flags |= PFSYNCF_BUCKET_PUSH;
	swi_sched(V_pfsync_swi_cookie, 0);
}

static void
pfsync_push_all(struct pfsync_softc *sc)
{
	int c;
	struct pfsync_bucket *b;

	for (c = 0; c < pfsync_buckets; c++) {
		b = &sc->sc_buckets[c];

		PFSYNC_BUCKET_LOCK(b);
		pfsync_push(b);
		PFSYNC_BUCKET_UNLOCK(b);
	}
}

static void
pfsync_tx(struct pfsync_softc *sc, struct mbuf *m)
{
	struct ip *ip;
	int af, error = 0;

	ip = mtod(m, struct ip *);
	MPASS(ip->ip_v == IPVERSION || ip->ip_v == (IPV6_VERSION >> 4));

	af = ip->ip_v == IPVERSION ? AF_INET : AF_INET6;

	/*
	 * We distinguish between a deferral packet and our
	 * own pfsync packet based on M_SKIP_FIREWALL
	 * flag. This is XXX.
	 */
	switch (af) {
#ifdef INET
	case AF_INET:
		if (m->m_flags & M_SKIP_FIREWALL) {
			error = ip_output(m, NULL, NULL, 0,
			    NULL, NULL);
		} else {
			error = ip_output(m, NULL, NULL,
			    IP_RAWOUTPUT, &sc->sc_imo, NULL);
		}
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if (m->m_flags & M_SKIP_FIREWALL) {
			error = ip6_output(m, NULL, NULL, 0,
			    NULL, NULL, NULL);
		} else {
			error = ip6_output(m, NULL, NULL, 0,
				&sc->sc_im6o, NULL, NULL);
		}
		break;
#endif
	}

	if (error == 0)
		V_pfsyncstats.pfsyncs_opackets++;
	else
		V_pfsyncstats.pfsyncs_oerrors++;

}

static void
pfsyncintr(void *arg)
{
	struct epoch_tracker et;
	struct pfsync_softc *sc = arg;
	struct pfsync_bucket *b;
	struct mbuf *m, *n;
	int c;

	NET_EPOCH_ENTER(et);
	CURVNET_SET(sc->sc_ifp->if_vnet);

	for (c = 0; c < pfsync_buckets; c++) {
		b = &sc->sc_buckets[c];

		PFSYNC_BUCKET_LOCK(b);
		if ((b->b_flags & PFSYNCF_BUCKET_PUSH) && b->b_len > PFSYNC_MINPKT) {
			pfsync_sendout(0, b->b_id);
			b->b_flags &= ~PFSYNCF_BUCKET_PUSH;
		}
		_IF_DEQUEUE_ALL(&b->b_snd, m);
		PFSYNC_BUCKET_UNLOCK(b);

		for (; m != NULL; m = n) {
			n = m->m_nextpkt;
			m->m_nextpkt = NULL;

			pfsync_tx(sc, m);
		}
	}
	CURVNET_RESTORE();
	NET_EPOCH_EXIT(et);
}

static int
pfsync_multicast_setup(struct pfsync_softc *sc, struct ifnet *ifp,
    struct in_mfilter* imf, struct in6_mfilter* im6f)
{
#ifdef  INET
	struct ip_moptions *imo = &sc->sc_imo;
#endif
#ifdef INET6
	struct ip6_moptions *im6o = &sc->sc_im6o;
	struct sockaddr_in6 *syncpeer_sa6 = NULL;
#endif

	if (!(ifp->if_flags & IFF_MULTICAST))
		return (EADDRNOTAVAIL);

	switch (sc->sc_sync_peer.ss_family) {
#ifdef INET
	case AF_INET:
	{
		int error;

		ip_mfilter_init(&imo->imo_head);
		imo->imo_multicast_vif = -1;
		if ((error = in_joingroup(ifp,
		    &((struct sockaddr_in *)&sc->sc_sync_peer)->sin_addr, NULL,
		    &imf->imf_inm)) != 0)
			return (error);

		ip_mfilter_insert(&imo->imo_head, imf);
		imo->imo_multicast_ifp = ifp;
		imo->imo_multicast_ttl = PFSYNC_DFLTTL;
		imo->imo_multicast_loop = 0;
		break;
	}
#endif
#ifdef INET6
	case AF_INET6:
	{
		int error;

		syncpeer_sa6 = (struct sockaddr_in6 *)&sc->sc_sync_peer;
		if ((error = in6_setscope(&syncpeer_sa6->sin6_addr, ifp, NULL)))
			return (error);

		ip6_mfilter_init(&im6o->im6o_head);
		if ((error = in6_joingroup(ifp, &syncpeer_sa6->sin6_addr, NULL,
		    &(im6f->im6f_in6m), 0)) != 0)
			return (error);

		ip6_mfilter_insert(&im6o->im6o_head, im6f);
		im6o->im6o_multicast_ifp = ifp;
		im6o->im6o_multicast_hlim = PFSYNC_DFLTTL;
		im6o->im6o_multicast_loop = 0;
		break;
	}
#endif
	}

	return (0);
}

static void
pfsync_multicast_cleanup(struct pfsync_softc *sc)
{
#ifdef INET
	struct ip_moptions *imo = &sc->sc_imo;
	struct in_mfilter *imf;

	while ((imf = ip_mfilter_first(&imo->imo_head)) != NULL) {
		ip_mfilter_remove(&imo->imo_head, imf);
		in_leavegroup(imf->imf_inm, NULL);
		ip_mfilter_free(imf);
	}
	imo->imo_multicast_ifp = NULL;
#endif

#ifdef INET6
	struct ip6_moptions *im6o = &sc->sc_im6o;
	struct in6_mfilter *im6f;

	while ((im6f = ip6_mfilter_first(&im6o->im6o_head)) != NULL) {
		ip6_mfilter_remove(&im6o->im6o_head, im6f);
		in6_leavegroup(im6f->im6f_in6m, NULL);
		ip6_mfilter_free(im6f);
	}
	im6o->im6o_multicast_ifp = NULL;
#endif
}

void
pfsync_detach_ifnet(struct ifnet *ifp)
{
	struct pfsync_softc *sc = V_pfsyncif;

	if (sc == NULL)
		return;

	PFSYNC_LOCK(sc);

	if (sc->sc_sync_if == ifp) {
		/* We don't need mutlicast cleanup here, because the interface
		 * is going away. We do need to ensure we don't try to do
		 * cleanup later.
		 */
		ip_mfilter_init(&sc->sc_imo.imo_head);
		sc->sc_imo.imo_multicast_ifp = NULL;
		sc->sc_im6o.im6o_multicast_ifp = NULL;
		sc->sc_sync_if = NULL;
	}

	PFSYNC_UNLOCK(sc);
}

static int
pfsync_pfsyncreq_to_kstatus(struct pfsyncreq *pfsyncr, struct pfsync_kstatus *status)
{
	struct sockaddr_storage sa;
	status->maxupdates = pfsyncr->pfsyncr_maxupdates;
	status->flags = pfsyncr->pfsyncr_defer;

	strlcpy(status->syncdev, pfsyncr->pfsyncr_syncdev, IFNAMSIZ);

	memset(&sa, 0, sizeof(sa));
	if (pfsyncr->pfsyncr_syncpeer.s_addr != 0) {
		struct sockaddr_in *in = (struct sockaddr_in *)&sa;
		in->sin_family = AF_INET;
		in->sin_len = sizeof(*in);
		in->sin_addr.s_addr = pfsyncr->pfsyncr_syncpeer.s_addr;
	}
	status->syncpeer = sa;

	return 0;
}

static int
pfsync_kstatus_to_softc(struct pfsync_kstatus *status, struct pfsync_softc *sc)
{
	struct ifnet *sifp;
	struct in_mfilter *imf = NULL;
	struct in6_mfilter *im6f = NULL;
	int error;
	int c;

	if ((status->maxupdates < 0) || (status->maxupdates > 255))
		return (EINVAL);

	if (status->syncdev[0] == '\0')
		sifp = NULL;
	else if ((sifp = ifunit_ref(status->syncdev)) == NULL)
		return (EINVAL);

	switch (status->syncpeer.ss_family) {
#ifdef INET
	case AF_UNSPEC:
	case AF_INET: {
		struct sockaddr_in *status_sin;
		status_sin = (struct sockaddr_in *)&(status->syncpeer);
		if (sifp != NULL) {
			if (status_sin->sin_addr.s_addr == 0 ||
			    status_sin->sin_addr.s_addr ==
			    htonl(INADDR_PFSYNC_GROUP)) {
				status_sin->sin_family = AF_INET;
				status_sin->sin_len = sizeof(*status_sin);
				status_sin->sin_addr.s_addr =
				    htonl(INADDR_PFSYNC_GROUP);
			}

			if (IN_MULTICAST(ntohl(status_sin->sin_addr.s_addr))) {
				imf = ip_mfilter_alloc(M_WAITOK, 0, 0);
			}
		}
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		struct sockaddr_in6 *status_sin6;
		status_sin6 = (struct sockaddr_in6*)&(status->syncpeer);
		if (sifp != NULL) {
			if (IN6_IS_ADDR_UNSPECIFIED(&status_sin6->sin6_addr) ||
			    IN6_ARE_ADDR_EQUAL(&status_sin6->sin6_addr,
				&in6addr_linklocal_pfsync_group)) {
				status_sin6->sin6_family = AF_INET6;
				status_sin6->sin6_len = sizeof(*status_sin6);
				status_sin6->sin6_addr =
				    in6addr_linklocal_pfsync_group;
			}

			if (IN6_IS_ADDR_MULTICAST(&status_sin6->sin6_addr)) {
				im6f = ip6_mfilter_alloc(M_WAITOK, 0, 0);
			}
		}
		break;
	}
#endif
	}

	PFSYNC_LOCK(sc);

	switch (status->version) {
		case PFSYNC_MSG_VERSION_UNSPECIFIED:
			sc->sc_version = PFSYNC_MSG_VERSION_DEFAULT;
			break;
		case PFSYNC_MSG_VERSION_1301:
		case PFSYNC_MSG_VERSION_1400:
			sc->sc_version = status->version;
			break;
		default:
			PFSYNC_UNLOCK(sc);
			return (EINVAL);
	}

	switch (status->syncpeer.ss_family) {
	case AF_INET: {
		struct sockaddr_in *status_sin = (struct sockaddr_in *)&(status->syncpeer);
		struct sockaddr_in *sc_sin = (struct sockaddr_in *)&sc->sc_sync_peer;
		sc_sin->sin_family = AF_INET;
		sc_sin->sin_len = sizeof(*sc_sin);
		if (status_sin->sin_addr.s_addr == 0) {
			sc_sin->sin_addr.s_addr = htonl(INADDR_PFSYNC_GROUP);
		} else {
			sc_sin->sin_addr.s_addr = status_sin->sin_addr.s_addr;
		}
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6 *status_sin = (struct sockaddr_in6 *)&(status->syncpeer);
		struct sockaddr_in6 *sc_sin = (struct sockaddr_in6 *)&sc->sc_sync_peer;
		sc_sin->sin6_family = AF_INET6;
		sc_sin->sin6_len = sizeof(*sc_sin);
		if(IN6_IS_ADDR_UNSPECIFIED(&status_sin->sin6_addr)) {
			sc_sin->sin6_addr = in6addr_linklocal_pfsync_group;
		} else {
			sc_sin->sin6_addr = status_sin->sin6_addr;
		}
		break;
	}
	}

	sc->sc_maxupdates = status->maxupdates;
	if (status->flags & PFSYNCF_DEFER) {
		sc->sc_flags |= PFSYNCF_DEFER;
		V_pfsync_defer_ptr = pfsync_defer;
	} else {
		sc->sc_flags &= ~PFSYNCF_DEFER;
		V_pfsync_defer_ptr = NULL;
	}

	if (sifp == NULL) {
		if (sc->sc_sync_if)
			if_rele(sc->sc_sync_if);
		sc->sc_sync_if = NULL;
		pfsync_multicast_cleanup(sc);
		PFSYNC_UNLOCK(sc);
		return (0);
	}

	for (c = 0; c < pfsync_buckets; c++) {
		PFSYNC_BUCKET_LOCK(&sc->sc_buckets[c]);
		if (sc->sc_buckets[c].b_len > PFSYNC_MINPKT &&
		    (sifp->if_mtu < sc->sc_ifp->if_mtu ||
			(sc->sc_sync_if != NULL &&
			    sifp->if_mtu < sc->sc_sync_if->if_mtu) ||
			sifp->if_mtu < MCLBYTES - sizeof(struct ip)))
			pfsync_sendout(1, c);
		PFSYNC_BUCKET_UNLOCK(&sc->sc_buckets[c]);
	}

	pfsync_multicast_cleanup(sc);

	if (((sc->sc_sync_peer.ss_family == AF_INET) &&
	    IN_MULTICAST(ntohl(((struct sockaddr_in *)
	        &sc->sc_sync_peer)->sin_addr.s_addr))) ||
	    ((sc->sc_sync_peer.ss_family == AF_INET6) &&
	    IN6_IS_ADDR_MULTICAST(&((struct sockaddr_in6*)
	        &sc->sc_sync_peer)->sin6_addr))) {
		error = pfsync_multicast_setup(sc, sifp, imf, im6f);
		if (error) {
			if_rele(sifp);
			PFSYNC_UNLOCK(sc);
#ifdef INET
			if (imf != NULL)
				ip_mfilter_free(imf);
#endif
#ifdef INET6
			if (im6f != NULL)
				ip6_mfilter_free(im6f);
#endif
			return (error);
		}
	}
	if (sc->sc_sync_if)
		if_rele(sc->sc_sync_if);
	sc->sc_sync_if = sifp;

	switch (sc->sc_sync_peer.ss_family) {
#ifdef INET
	case AF_INET: {
		struct ip *ip;
		ip = &sc->sc_template.ipv4;
		bzero(ip, sizeof(*ip));
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(sc->sc_template.ipv4) >> 2;
		ip->ip_tos = IPTOS_LOWDELAY;
		/* len and id are set later. */
		ip->ip_off = htons(IP_DF);
		ip->ip_ttl = PFSYNC_DFLTTL;
		ip->ip_p = IPPROTO_PFSYNC;
		ip->ip_src.s_addr = INADDR_ANY;
		ip->ip_dst = ((struct sockaddr_in *)&sc->sc_sync_peer)->sin_addr;
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr *ip6;
		ip6 = &sc->sc_template.ipv6;
		bzero(ip6, sizeof(*ip6));
		ip6->ip6_vfc = IPV6_VERSION;
		ip6->ip6_hlim = PFSYNC_DFLTTL;
		ip6->ip6_nxt = IPPROTO_PFSYNC;
		ip6->ip6_dst = ((struct sockaddr_in6 *)&sc->sc_sync_peer)->sin6_addr;

		struct epoch_tracker et;
		NET_EPOCH_ENTER(et);
		in6_selectsrc_addr(if_getfib(sc->sc_sync_if), &ip6->ip6_dst, 0,
		    sc->sc_sync_if, &ip6->ip6_src, NULL);
		NET_EPOCH_EXIT(et);
		break;
	}
#endif
	}

	/* Request a full state table update. */
	if ((sc->sc_flags & PFSYNCF_OK) && carp_demote_adj_p)
		(*carp_demote_adj_p)(V_pfsync_carp_adj,
		    "pfsync bulk start");
	sc->sc_flags &= ~PFSYNCF_OK;
	if (V_pf_status.debug >= PF_DEBUG_MISC)
		printf("pfsync: requesting bulk update\n");
	PFSYNC_UNLOCK(sc);
	PFSYNC_BUCKET_LOCK(&sc->sc_buckets[0]);
	pfsync_request_update(0, 0);
	PFSYNC_BUCKET_UNLOCK(&sc->sc_buckets[0]);
	PFSYNC_BLOCK(sc);
	sc->sc_ureq_sent = time_uptime;
	callout_reset(&sc->sc_bulkfail_tmo, 5 * hz, pfsync_bulk_fail, sc);
	PFSYNC_BUNLOCK(sc);
	return (0);
}

static void
pfsync_pointers_init(void)
{

	PF_RULES_WLOCK();
	V_pfsync_state_import_ptr = pfsync_state_import;
	V_pfsync_insert_state_ptr = pfsync_insert_state;
	V_pfsync_update_state_ptr = pfsync_update_state;
	V_pfsync_delete_state_ptr = pfsync_delete_state;
	V_pfsync_clear_states_ptr = pfsync_clear_states;
	V_pfsync_defer_ptr = pfsync_defer;
	PF_RULES_WUNLOCK();
}

static void
pfsync_pointers_uninit(void)
{

	PF_RULES_WLOCK();
	V_pfsync_state_import_ptr = NULL;
	V_pfsync_insert_state_ptr = NULL;
	V_pfsync_update_state_ptr = NULL;
	V_pfsync_delete_state_ptr = NULL;
	V_pfsync_clear_states_ptr = NULL;
	V_pfsync_defer_ptr = NULL;
	PF_RULES_WUNLOCK();
}

static void
vnet_pfsync_init(const void *unused __unused)
{
	int error;

	V_pfsync_cloner = if_clone_simple(pfsyncname,
	    pfsync_clone_create, pfsync_clone_destroy, 1);
	error = swi_add(&V_pfsync_swi_ie, pfsyncname, pfsyncintr, V_pfsyncif,
	    SWI_NET, INTR_MPSAFE, &V_pfsync_swi_cookie);
	if (error) {
		if_clone_detach(V_pfsync_cloner);
		log(LOG_INFO, "swi_add() failed in %s\n", __func__);
	}

	pfsync_pointers_init();
}
VNET_SYSINIT(vnet_pfsync_init, SI_SUB_PROTO_FIREWALL, SI_ORDER_ANY,
    vnet_pfsync_init, NULL);

static void
vnet_pfsync_uninit(const void *unused __unused)
{
	int ret __diagused;

	pfsync_pointers_uninit();

	if_clone_detach(V_pfsync_cloner);
	ret = swi_remove(V_pfsync_swi_cookie);
	MPASS(ret == 0);
	ret = intr_event_destroy(V_pfsync_swi_ie);
	MPASS(ret == 0);
}

VNET_SYSUNINIT(vnet_pfsync_uninit, SI_SUB_PROTO_FIREWALL, SI_ORDER_FOURTH,
    vnet_pfsync_uninit, NULL);

static int
pfsync_init(void)
{
	int error;

	pfsync_detach_ifnet_ptr = pfsync_detach_ifnet;

#ifdef INET
	error = ipproto_register(IPPROTO_PFSYNC, pfsync_input, NULL);
	if (error)
		return (error);
#endif
#ifdef INET6
	error = ip6proto_register(IPPROTO_PFSYNC, pfsync6_input, NULL);
	if (error) {
		ipproto_unregister(IPPROTO_PFSYNC);
		return (error);
	}
#endif

	return (0);
}

static void
pfsync_uninit(void)
{
	pfsync_detach_ifnet_ptr = NULL;

#ifdef INET
	ipproto_unregister(IPPROTO_PFSYNC);
#endif
#ifdef INET6
	ip6proto_unregister(IPPROTO_PFSYNC);
#endif
}

static int
pfsync_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		error = pfsync_init();
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
	pfsyncname,
	pfsync_modevent,
	0
};

#define PFSYNC_MODVER 1

/* Stay on FIREWALL as we depend on pf being initialized and on inetdomain. */
DECLARE_MODULE(pfsync, pfsync_mod, SI_SUB_PROTO_FIREWALL, SI_ORDER_ANY);
MODULE_VERSION(pfsync, PFSYNC_MODVER);
MODULE_DEPEND(pfsync, pf, PF_MODVER, PF_MODVER, PF_MODVER);
