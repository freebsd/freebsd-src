/*	$OpenBSD: if_trunk.c,v 1.30 2007/01/31 06:20:19 reyk Exp $	*/

/*
 * Copyright (c) 2005, 2006 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2007 Andrew Thompson <thompsa@FreeBSD.org>
 * Copyright (c) 2014 Marcelo Araujo <araujo@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/hash.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/taskqueue.h>
#include <sys/eventhandler.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/bpf.h>

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#include <netinet/ip.h>
#endif
#ifdef INET
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#endif

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/in6_ifattach.h>
#endif

#include <net/if_vlan_var.h>
#include <net/if_lagg.h>
#include <net/ieee8023ad_lacp.h>

/* Special flags we should propagate to the lagg ports. */
static struct {
	int flag;
	int (*func)(struct ifnet *, int);
} lagg_pflags[] = {
	{IFF_PROMISC, ifpromisc},
	{IFF_ALLMULTI, if_allmulti},
	{0, NULL}
};

SLIST_HEAD(__trhead, lagg_softc) lagg_list;	/* list of laggs */
static struct mtx	lagg_list_mtx;
eventhandler_tag	lagg_detach_cookie = NULL;

static int	lagg_clone_create(struct if_clone *, int, caddr_t);
static void	lagg_clone_destroy(struct ifnet *);
static struct if_clone *lagg_cloner;
static const char laggname[] = "lagg";

static void	lagg_lladdr(struct lagg_softc *, uint8_t *);
static void	lagg_capabilities(struct lagg_softc *);
static void	lagg_port_lladdr(struct lagg_port *, uint8_t *);
static void	lagg_port_setlladdr(void *, int);
static int	lagg_port_create(struct lagg_softc *, struct ifnet *);
static int	lagg_port_destroy(struct lagg_port *, int);
static struct mbuf *lagg_input(struct ifnet *, struct mbuf *);
static void	lagg_linkstate(struct lagg_softc *);
static void	lagg_port_state(struct ifnet *, int);
static int	lagg_port_ioctl(struct ifnet *, u_long, caddr_t);
static int	lagg_port_output(struct ifnet *, struct mbuf *,
		    const struct sockaddr *, struct route *);
static void	lagg_port_ifdetach(void *arg __unused, struct ifnet *);
#ifdef LAGG_PORT_STACKING
static int	lagg_port_checkstacking(struct lagg_softc *);
#endif
static void	lagg_port2req(struct lagg_port *, struct lagg_reqport *);
static void	lagg_init(void *);
static void	lagg_stop(struct lagg_softc *);
static int	lagg_ioctl(struct ifnet *, u_long, caddr_t);
static int	lagg_ether_setmulti(struct lagg_softc *);
static int	lagg_ether_cmdmulti(struct lagg_port *, int);
static	int	lagg_setflag(struct lagg_port *, int, int,
		    int (*func)(struct ifnet *, int));
static	int	lagg_setflags(struct lagg_port *, int status);
static int	lagg_transmit(struct ifnet *, struct mbuf *);
static void	lagg_qflush(struct ifnet *);
static int	lagg_media_change(struct ifnet *);
static void	lagg_media_status(struct ifnet *, struct ifmediareq *);
static struct lagg_port *lagg_link_active(struct lagg_softc *,
	    struct lagg_port *);
static const void *lagg_gethdr(struct mbuf *, u_int, u_int, void *);
static int	lagg_sysctl_active(SYSCTL_HANDLER_ARGS);

/* Simple round robin */
static int	lagg_rr_attach(struct lagg_softc *);
static int	lagg_rr_detach(struct lagg_softc *);
static int	lagg_rr_start(struct lagg_softc *, struct mbuf *);
static struct mbuf *lagg_rr_input(struct lagg_softc *, struct lagg_port *,
		    struct mbuf *);

/* Active failover */
static int	lagg_fail_attach(struct lagg_softc *);
static int	lagg_fail_detach(struct lagg_softc *);
static int	lagg_fail_start(struct lagg_softc *, struct mbuf *);
static struct mbuf *lagg_fail_input(struct lagg_softc *, struct lagg_port *,
		    struct mbuf *);

/* Loadbalancing */
static int	lagg_lb_attach(struct lagg_softc *);
static int	lagg_lb_detach(struct lagg_softc *);
static int	lagg_lb_port_create(struct lagg_port *);
static void	lagg_lb_port_destroy(struct lagg_port *);
static int	lagg_lb_start(struct lagg_softc *, struct mbuf *);
static struct mbuf *lagg_lb_input(struct lagg_softc *, struct lagg_port *,
		    struct mbuf *);
static int	lagg_lb_porttable(struct lagg_softc *, struct lagg_port *);

/* Broadcast */
static int    lagg_bcast_attach(struct lagg_softc *);
static int    lagg_bcast_detach(struct lagg_softc *);
static int    lagg_bcast_start(struct lagg_softc *, struct mbuf *);
static struct mbuf *lagg_bcast_input(struct lagg_softc *, struct lagg_port *,
                  struct mbuf *);

/* 802.3ad LACP */
static int	lagg_lacp_attach(struct lagg_softc *);
static int	lagg_lacp_detach(struct lagg_softc *);
static int	lagg_lacp_start(struct lagg_softc *, struct mbuf *);
static struct mbuf *lagg_lacp_input(struct lagg_softc *, struct lagg_port *,
		    struct mbuf *);
static void	lagg_lacp_lladdr(struct lagg_softc *);

static void	lagg_callout(void *);

/* lagg protocol table */
static const struct {
	int			ti_proto;
	int			(*ti_attach)(struct lagg_softc *);
} lagg_protos[] = {
	{ LAGG_PROTO_ROUNDROBIN,	lagg_rr_attach },
	{ LAGG_PROTO_FAILOVER,	lagg_fail_attach },
	{ LAGG_PROTO_LOADBALANCE,	lagg_lb_attach },
	{ LAGG_PROTO_ETHERCHANNEL,	lagg_lb_attach },
	{ LAGG_PROTO_LACP,		lagg_lacp_attach },
	{ LAGG_PROTO_BROADCAST,	lagg_bcast_attach },
	{ LAGG_PROTO_NONE,		NULL }
};

SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, OID_AUTO, lagg, CTLFLAG_RW, 0,
    "Link Aggregation");

static int lagg_failover_rx_all = 0; /* Allow input on any failover links */
SYSCTL_INT(_net_link_lagg, OID_AUTO, failover_rx_all, CTLFLAG_RW,
    &lagg_failover_rx_all, 0,
    "Accept input from any interface in a failover lagg");
static int def_use_flowid = 1; /* Default value for using M_FLOWID */
SYSCTL_INT(_net_link_lagg, OID_AUTO, default_use_flowid, CTLFLAG_RWTUN,
    &def_use_flowid, 0,
    "Default setting for using flow id for load sharing");
static int def_flowid_shift = 16; /* Default value for using M_FLOWID */
SYSCTL_INT(_net_link_lagg, OID_AUTO, default_flowid_shift, CTLFLAG_RWTUN,
    &def_flowid_shift, 0,
    "Default setting for flowid shift for load sharing");

static int
lagg_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		mtx_init(&lagg_list_mtx, "if_lagg list", NULL, MTX_DEF);
		SLIST_INIT(&lagg_list);
		lagg_cloner = if_clone_simple(laggname, lagg_clone_create,
		    lagg_clone_destroy, 0);
		lagg_input_p = lagg_input;
		lagg_linkstate_p = lagg_port_state;
		lagg_detach_cookie = EVENTHANDLER_REGISTER(
		    ifnet_departure_event, lagg_port_ifdetach, NULL,
		    EVENTHANDLER_PRI_ANY);
		break;
	case MOD_UNLOAD:
		EVENTHANDLER_DEREGISTER(ifnet_departure_event,
		    lagg_detach_cookie);
		if_clone_detach(lagg_cloner);
		lagg_input_p = NULL;
		lagg_linkstate_p = NULL;
		mtx_destroy(&lagg_list_mtx);
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t lagg_mod = {
	"if_lagg",
	lagg_modevent,
	0
};

DECLARE_MODULE(if_lagg, lagg_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_lagg, 1);

/*
 * This routine is run via an vlan
 * config EVENT
 */
static void
lagg_register_vlan(void *arg, struct ifnet *ifp, u_int16_t vtag)
{
        struct lagg_softc       *sc = ifp->if_softc;
        struct lagg_port        *lp;
        struct rm_priotracker   tracker;

        if (ifp->if_softc !=  arg)   /* Not our event */
                return;

        LAGG_RLOCK(sc, &tracker);
        if (!SLIST_EMPTY(&sc->sc_ports)) {
                SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
                        EVENTHANDLER_INVOKE(vlan_config, lp->lp_ifp, vtag);
        }
        LAGG_RUNLOCK(sc, &tracker);
}

/*
 * This routine is run via an vlan
 * unconfig EVENT
 */
static void
lagg_unregister_vlan(void *arg, struct ifnet *ifp, u_int16_t vtag)
{
        struct lagg_softc       *sc = ifp->if_softc;
        struct lagg_port        *lp;
        struct rm_priotracker   tracker;

        if (ifp->if_softc !=  arg)   /* Not our event */
                return;

        LAGG_RLOCK(sc, &tracker);
        if (!SLIST_EMPTY(&sc->sc_ports)) {
                SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
                        EVENTHANDLER_INVOKE(vlan_unconfig, lp->lp_ifp, vtag);
        }
        LAGG_RUNLOCK(sc, &tracker);
}

static int
lagg_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct lagg_softc *sc;
	struct ifnet *ifp;
	int i, error = 0;
	static const u_char eaddr[6];	/* 00:00:00:00:00:00 */
	struct sysctl_oid *oid;
	char num[14];			/* sufficient for 32 bits */

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	ifp = sc->sc_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		free(sc, M_DEVBUF);
		return (ENOSPC);
	}

	sc->sc_ipackets = counter_u64_alloc(M_WAITOK);
	sc->sc_opackets = counter_u64_alloc(M_WAITOK);
	sc->sc_ibytes = counter_u64_alloc(M_WAITOK);
	sc->sc_obytes = counter_u64_alloc(M_WAITOK);

	sysctl_ctx_init(&sc->ctx);
	snprintf(num, sizeof(num), "%u", unit);
	sc->use_flowid = def_use_flowid;
	sc->flowid_shift = def_flowid_shift;
	sc->sc_oid = oid = SYSCTL_ADD_NODE(&sc->ctx,
		&SYSCTL_NODE_CHILDREN(_net_link, lagg),
		OID_AUTO, num, CTLFLAG_RD, NULL, "");
	SYSCTL_ADD_INT(&sc->ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"use_flowid", CTLTYPE_INT|CTLFLAG_RW, &sc->use_flowid,
		sc->use_flowid, "Use flow id for load sharing");
	SYSCTL_ADD_INT(&sc->ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"flowid_shift", CTLTYPE_INT|CTLFLAG_RW, &sc->flowid_shift,
		sc->flowid_shift,
		"Shift flowid bits to prevent multiqueue collisions");
	SYSCTL_ADD_INT(&sc->ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"count", CTLTYPE_INT|CTLFLAG_RD, &sc->sc_count, sc->sc_count,
		"Total number of ports");
	SYSCTL_ADD_PROC(&sc->ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"active", CTLTYPE_INT|CTLFLAG_RD, sc, 0, lagg_sysctl_active,
		"I", "Total number of active ports");
	SYSCTL_ADD_INT(&sc->ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"flapping", CTLTYPE_INT|CTLFLAG_RD, &sc->sc_flapping,
		sc->sc_flapping, "Total number of port change events");
	/* Hash all layers by default */
	sc->sc_flags = LAGG_F_HASHL2|LAGG_F_HASHL3|LAGG_F_HASHL4;

	sc->sc_proto = LAGG_PROTO_NONE;
	for (i = 0; lagg_protos[i].ti_proto != LAGG_PROTO_NONE; i++) {
		if (lagg_protos[i].ti_proto == LAGG_PROTO_DEFAULT) {
			sc->sc_proto = lagg_protos[i].ti_proto;
			if ((error = lagg_protos[i].ti_attach(sc)) != 0) {
				if_free(ifp);
				free(sc, M_DEVBUF);
				return (error);
			}
			break;
		}
	}
	LAGG_LOCK_INIT(sc);
	LAGG_CALLOUT_LOCK_INIT(sc);
	SLIST_INIT(&sc->sc_ports);
	TASK_INIT(&sc->sc_lladdr_task, 0, lagg_port_setlladdr, sc);

	/*
	 * This uses the callout lock rather than the rmlock; one can't
	 * hold said rmlock during SWI.
	 */
	callout_init_mtx(&sc->sc_callout, &sc->sc_call_mtx, 0);

	/* Initialise pseudo media types */
	ifmedia_init(&sc->sc_media, 0, lagg_media_change,
	    lagg_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_initname(ifp, laggname, unit);
	ifp->if_softc = sc;
	ifp->if_transmit = lagg_transmit;
	ifp->if_qflush = lagg_qflush;
	ifp->if_init = lagg_init;
	ifp->if_ioctl = lagg_ioctl;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST;
	ifp->if_capenable = ifp->if_capabilities = IFCAP_HWSTATS;

	/*
	 * Attach as an ordinary ethernet device, children will be attached
	 * as special device IFT_IEEE8023ADLAG.
	 */
	ether_ifattach(ifp, eaddr);

	sc->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
		lagg_register_vlan, sc, EVENTHANDLER_PRI_FIRST);
	sc->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
		lagg_unregister_vlan, sc, EVENTHANDLER_PRI_FIRST);

	/* Insert into the global list of laggs */
	mtx_lock(&lagg_list_mtx);
	SLIST_INSERT_HEAD(&lagg_list, sc, sc_entries);
	mtx_unlock(&lagg_list_mtx);

	callout_reset(&sc->sc_callout, hz, lagg_callout, sc);

	return (0);
}

static void
lagg_clone_destroy(struct ifnet *ifp)
{
	struct lagg_softc *sc = (struct lagg_softc *)ifp->if_softc;
	struct lagg_port *lp;

	LAGG_WLOCK(sc);

	lagg_stop(sc);
	ifp->if_flags &= ~IFF_UP;

	EVENTHANDLER_DEREGISTER(vlan_config, sc->vlan_attach);
	EVENTHANDLER_DEREGISTER(vlan_unconfig, sc->vlan_detach);

	/* Shutdown and remove lagg ports */
	while ((lp = SLIST_FIRST(&sc->sc_ports)) != NULL)
		lagg_port_destroy(lp, 1);
	/* Unhook the aggregation protocol */
	if (sc->sc_detach != NULL)
		(*sc->sc_detach)(sc);

	LAGG_WUNLOCK(sc);

	sysctl_ctx_free(&sc->ctx);
	ifmedia_removeall(&sc->sc_media);
	ether_ifdetach(ifp);
	if_free(ifp);

	/* This grabs sc_callout_mtx, serialising it correctly */
	callout_drain(&sc->sc_callout);

	/* At this point it's drained; we can free this */
	counter_u64_free(sc->sc_ipackets);
	counter_u64_free(sc->sc_opackets);
	counter_u64_free(sc->sc_ibytes);
	counter_u64_free(sc->sc_obytes);

	mtx_lock(&lagg_list_mtx);
	SLIST_REMOVE(&lagg_list, sc, lagg_softc, sc_entries);
	mtx_unlock(&lagg_list_mtx);

	taskqueue_drain(taskqueue_swi, &sc->sc_lladdr_task);
	LAGG_LOCK_DESTROY(sc);
	LAGG_CALLOUT_LOCK_DESTROY(sc);
	free(sc, M_DEVBUF);
}

static void
lagg_lladdr(struct lagg_softc *sc, uint8_t *lladdr)
{
	struct ifnet *ifp = sc->sc_ifp;

	if (memcmp(lladdr, IF_LLADDR(ifp), ETHER_ADDR_LEN) == 0)
		return;

	bcopy(lladdr, IF_LLADDR(ifp), ETHER_ADDR_LEN);
	/* Let the protocol know the MAC has changed */
	if (sc->sc_lladdr != NULL)
		(*sc->sc_lladdr)(sc);
	EVENTHANDLER_INVOKE(iflladdr_event, ifp);
}

static void
lagg_capabilities(struct lagg_softc *sc)
{
	struct lagg_port *lp;
	int cap = ~0, ena = ~0;
	u_long hwa = ~0UL;
#if defined(INET) || defined(INET6)
	u_int hw_tsomax = IP_MAXPACKET;	/* Initialize to the maximum value. */
#else
	u_int hw_tsomax = ~0;	/* if_hw_tsomax is only for INET/INET6, but.. */
#endif

	LAGG_WLOCK_ASSERT(sc);

	/* Get capabilities from the lagg ports */
	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		cap &= lp->lp_ifp->if_capabilities;
		ena &= lp->lp_ifp->if_capenable;
		hwa &= lp->lp_ifp->if_hwassist;
		/* Set to the minimum value of the lagg ports. */
		if (lp->lp_ifp->if_hw_tsomax < hw_tsomax &&
		    lp->lp_ifp->if_hw_tsomax > 0)
			hw_tsomax = lp->lp_ifp->if_hw_tsomax;
	}
	cap = (cap == ~0 ? 0 : cap);
	ena = (ena == ~0 ? 0 : ena);
	hwa = (hwa == ~0 ? 0 : hwa);

	if (sc->sc_ifp->if_capabilities != cap ||
	    sc->sc_ifp->if_capenable != ena ||
	    sc->sc_ifp->if_hwassist != hwa ||
	    sc->sc_ifp->if_hw_tsomax != hw_tsomax) {
		sc->sc_ifp->if_capabilities = cap;
		sc->sc_ifp->if_capenable = ena;
		sc->sc_ifp->if_hwassist = hwa;
		sc->sc_ifp->if_hw_tsomax = hw_tsomax;
		getmicrotime(&sc->sc_ifp->if_lastchange);

		if (sc->sc_ifflags & IFF_DEBUG)
			if_printf(sc->sc_ifp,
			    "capabilities 0x%08x enabled 0x%08x\n", cap, ena);
	}
}

static void
lagg_port_lladdr(struct lagg_port *lp, uint8_t *lladdr)
{
	struct lagg_softc *sc = lp->lp_softc;
	struct ifnet *ifp = lp->lp_ifp;
	struct lagg_llq *llq;
	int pending = 0;

	LAGG_WLOCK_ASSERT(sc);

	if (lp->lp_detaching ||
	    memcmp(lladdr, IF_LLADDR(ifp), ETHER_ADDR_LEN) == 0)
		return;

	/* Check to make sure its not already queued to be changed */
	SLIST_FOREACH(llq, &sc->sc_llq_head, llq_entries) {
		if (llq->llq_ifp == ifp) {
			pending = 1;
			break;
		}
	}

	if (!pending) {
		llq = malloc(sizeof(struct lagg_llq), M_DEVBUF, M_NOWAIT);
		if (llq == NULL)	/* XXX what to do */
			return;
	}

	/* Update the lladdr even if pending, it may have changed */
	llq->llq_ifp = ifp;
	bcopy(lladdr, llq->llq_lladdr, ETHER_ADDR_LEN);

	if (!pending)
		SLIST_INSERT_HEAD(&sc->sc_llq_head, llq, llq_entries);

	taskqueue_enqueue(taskqueue_swi, &sc->sc_lladdr_task);
}

/*
 * Set the interface MAC address from a taskqueue to avoid a LOR.
 */
static void
lagg_port_setlladdr(void *arg, int pending)
{
	struct lagg_softc *sc = (struct lagg_softc *)arg;
	struct lagg_llq *llq, *head;
	struct ifnet *ifp;
	int error;

	/* Grab a local reference of the queue and remove it from the softc */
	LAGG_WLOCK(sc);
	head = SLIST_FIRST(&sc->sc_llq_head);
	SLIST_FIRST(&sc->sc_llq_head) = NULL;
	LAGG_WUNLOCK(sc);

	/*
	 * Traverse the queue and set the lladdr on each ifp. It is safe to do
	 * unlocked as we have the only reference to it.
	 */
	for (llq = head; llq != NULL; llq = head) {
		ifp = llq->llq_ifp;

		/* Set the link layer address */
		CURVNET_SET(ifp->if_vnet);
		error = if_setlladdr(ifp, llq->llq_lladdr, ETHER_ADDR_LEN);
		CURVNET_RESTORE();
		if (error)
			printf("%s: setlladdr failed on %s\n", __func__,
			    ifp->if_xname);

		head = SLIST_NEXT(llq, llq_entries);
		free(llq, M_DEVBUF);
	}
}

static int
lagg_port_create(struct lagg_softc *sc, struct ifnet *ifp)
{
	struct lagg_softc *sc_ptr;
	struct lagg_port *lp;
	int error = 0;

	LAGG_WLOCK_ASSERT(sc);

	/* Limit the maximal number of lagg ports */
	if (sc->sc_count >= LAGG_MAX_PORTS)
		return (ENOSPC);

	/* Check if port has already been associated to a lagg */
	if (ifp->if_lagg != NULL) {
		/* Port is already in the current lagg? */
		lp = (struct lagg_port *)ifp->if_lagg;
		if (lp->lp_softc == sc)
			return (EEXIST);
		return (EBUSY);
	}

	/* XXX Disallow non-ethernet interfaces (this should be any of 802) */
	if (ifp->if_type != IFT_ETHER)
		return (EPROTONOSUPPORT);

#ifdef INET6
	/*
	 * The member interface should not have inet6 address because
	 * two interfaces with a valid link-local scope zone must not be
	 * merged in any form.  This restriction is needed to
	 * prevent violation of link-local scope zone.  Attempts to
	 * add a member interface which has inet6 addresses triggers
	 * removal of all inet6 addresses on the member interface.
	 */
	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		if (in6ifa_llaonifp(lp->lp_ifp)) {
			in6_ifdetach(lp->lp_ifp);
			if_printf(sc->sc_ifp,
			    "IPv6 addresses on %s have been removed "
			    "before adding it as a member to prevent "
			    "IPv6 address scope violation.\n",
			    lp->lp_ifp->if_xname);
		}
	}
	if (in6ifa_llaonifp(ifp)) {
		in6_ifdetach(ifp);
		if_printf(sc->sc_ifp,
		    "IPv6 addresses on %s have been removed "
		    "before adding it as a member to prevent "
		    "IPv6 address scope violation.\n",
		    ifp->if_xname);
	}
#endif
	/* Allow the first Ethernet member to define the MTU */
	if (SLIST_EMPTY(&sc->sc_ports))
		sc->sc_ifp->if_mtu = ifp->if_mtu;
	else if (sc->sc_ifp->if_mtu != ifp->if_mtu) {
		if_printf(sc->sc_ifp, "invalid MTU for %s\n",
		    ifp->if_xname);
		return (EINVAL);
	}

	if ((lp = malloc(sizeof(struct lagg_port),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	/* Check if port is a stacked lagg */
	mtx_lock(&lagg_list_mtx);
	SLIST_FOREACH(sc_ptr, &lagg_list, sc_entries) {
		if (ifp == sc_ptr->sc_ifp) {
			mtx_unlock(&lagg_list_mtx);
			free(lp, M_DEVBUF);
			return (EINVAL);
			/* XXX disable stacking for the moment, its untested */
#ifdef LAGG_PORT_STACKING
			lp->lp_flags |= LAGG_PORT_STACK;
			if (lagg_port_checkstacking(sc_ptr) >=
			    LAGG_MAX_STACKING) {
				mtx_unlock(&lagg_list_mtx);
				free(lp, M_DEVBUF);
				return (E2BIG);
			}
#endif
		}
	}
	mtx_unlock(&lagg_list_mtx);

	/* Change the interface type */
	lp->lp_iftype = ifp->if_type;
	ifp->if_type = IFT_IEEE8023ADLAG;
	ifp->if_lagg = lp;
	lp->lp_ioctl = ifp->if_ioctl;
	ifp->if_ioctl = lagg_port_ioctl;
	lp->lp_output = ifp->if_output;
	ifp->if_output = lagg_port_output;

	lp->lp_ifp = ifp;
	lp->lp_softc = sc;

	/* Save port link layer address */
	bcopy(IF_LLADDR(ifp), lp->lp_lladdr, ETHER_ADDR_LEN);

	if (SLIST_EMPTY(&sc->sc_ports)) {
		sc->sc_primary = lp;
		lagg_lladdr(sc, IF_LLADDR(ifp));
	} else {
		/* Update link layer address for this port */
		lagg_port_lladdr(lp, IF_LLADDR(sc->sc_ifp));
	}

	/* Insert into the list of ports */
	SLIST_INSERT_HEAD(&sc->sc_ports, lp, lp_entries);
	sc->sc_count++;

	/* Update lagg capabilities */
	lagg_capabilities(sc);
	lagg_linkstate(sc);

	/* Add multicast addresses and interface flags to this port */
	lagg_ether_cmdmulti(lp, 1);
	lagg_setflags(lp, 1);

	if (sc->sc_port_create != NULL)
		error = (*sc->sc_port_create)(lp);
	if (error) {
		/* remove the port again, without calling sc_port_destroy */
		lagg_port_destroy(lp, 0);
		return (error);
	}

	return (error);
}

#ifdef LAGG_PORT_STACKING
static int
lagg_port_checkstacking(struct lagg_softc *sc)
{
	struct lagg_softc *sc_ptr;
	struct lagg_port *lp;
	int m = 0;

	LAGG_WLOCK_ASSERT(sc);

	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		if (lp->lp_flags & LAGG_PORT_STACK) {
			sc_ptr = (struct lagg_softc *)lp->lp_ifp->if_softc;
			m = MAX(m, lagg_port_checkstacking(sc_ptr));
		}
	}

	return (m + 1);
}
#endif

static int
lagg_port_destroy(struct lagg_port *lp, int runpd)
{
	struct lagg_softc *sc = lp->lp_softc;
	struct lagg_port *lp_ptr;
	struct lagg_llq *llq;
	struct ifnet *ifp = lp->lp_ifp;

	LAGG_WLOCK_ASSERT(sc);

	if (runpd && sc->sc_port_destroy != NULL)
		(*sc->sc_port_destroy)(lp);

	/*
	 * Remove multicast addresses and interface flags from this port and
	 * reset the MAC address, skip if the interface is being detached.
	 */
	if (!lp->lp_detaching) {
		lagg_ether_cmdmulti(lp, 0);
		lagg_setflags(lp, 0);
		lagg_port_lladdr(lp, lp->lp_lladdr);
	}

	/* Restore interface */
	ifp->if_type = lp->lp_iftype;
	ifp->if_ioctl = lp->lp_ioctl;
	ifp->if_output = lp->lp_output;
	ifp->if_lagg = NULL;

	/* Finally, remove the port from the lagg */
	SLIST_REMOVE(&sc->sc_ports, lp, lagg_port, lp_entries);
	sc->sc_count--;

	/* Update the primary interface */
	if (lp == sc->sc_primary) {
		uint8_t lladdr[ETHER_ADDR_LEN];

		if ((lp_ptr = SLIST_FIRST(&sc->sc_ports)) == NULL) {
			bzero(&lladdr, ETHER_ADDR_LEN);
		} else {
			bcopy(lp_ptr->lp_lladdr,
			    lladdr, ETHER_ADDR_LEN);
		}
		lagg_lladdr(sc, lladdr);
		sc->sc_primary = lp_ptr;

		/* Update link layer address for each port */
		SLIST_FOREACH(lp_ptr, &sc->sc_ports, lp_entries)
			lagg_port_lladdr(lp_ptr, lladdr);
	}

	/* Remove any pending lladdr changes from the queue */
	if (lp->lp_detaching) {
		SLIST_FOREACH(llq, &sc->sc_llq_head, llq_entries) {
			if (llq->llq_ifp == ifp) {
				SLIST_REMOVE(&sc->sc_llq_head, llq, lagg_llq,
				    llq_entries);
				free(llq, M_DEVBUF);
				break;	/* Only appears once */
			}
		}
	}

	if (lp->lp_ifflags)
		if_printf(ifp, "%s: lp_ifflags unclean\n", __func__);

	free(lp, M_DEVBUF);

	/* Update lagg capabilities */
	lagg_capabilities(sc);
	lagg_linkstate(sc);

	return (0);
}

static int
lagg_port_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct lagg_reqport *rp = (struct lagg_reqport *)data;
	struct lagg_softc *sc;
	struct lagg_port *lp = NULL;
	int error = 0;
	struct rm_priotracker tracker;

	/* Should be checked by the caller */
	if (ifp->if_type != IFT_IEEE8023ADLAG ||
	    (lp = ifp->if_lagg) == NULL || (sc = lp->lp_softc) == NULL)
		goto fallback;

	switch (cmd) {
	case SIOCGLAGGPORT:
		if (rp->rp_portname[0] == '\0' ||
		    ifunit(rp->rp_portname) != ifp) {
			error = EINVAL;
			break;
		}

		LAGG_RLOCK(sc, &tracker);
		if ((lp = ifp->if_lagg) == NULL || lp->lp_softc != sc) {
			error = ENOENT;
			LAGG_RUNLOCK(sc, &tracker);
			break;
		}

		lagg_port2req(lp, rp);
		LAGG_RUNLOCK(sc, &tracker);
		break;

	case SIOCSIFCAP:
		if (lp->lp_ioctl == NULL) {
			error = EINVAL;
			break;
		}
		error = (*lp->lp_ioctl)(ifp, cmd, data);
		if (error)
			break;

		/* Update lagg interface capabilities */
		LAGG_WLOCK(sc);
		lagg_capabilities(sc);
		LAGG_WUNLOCK(sc);
		break;

	case SIOCSIFMTU:
		/* Do not allow the MTU to be changed once joined */
		error = EINVAL;
		break;

	default:
		goto fallback;
	}

	return (error);

fallback:
	if (lp->lp_ioctl != NULL)
		return ((*lp->lp_ioctl)(ifp, cmd, data));

	return (EINVAL);
}

/*
 * For direct output to child ports.
 */
static int
lagg_port_output(struct ifnet *ifp, struct mbuf *m,
	const struct sockaddr *dst, struct route *ro)
{
	struct lagg_port *lp = ifp->if_lagg;

	switch (dst->sa_family) {
		case pseudo_AF_HDRCMPLT:
		case AF_UNSPEC:
			return ((*lp->lp_output)(ifp, m, dst, ro));
	}

	/* drop any other frames */
	m_freem(m);
	return (ENETDOWN);
}

static void
lagg_port_ifdetach(void *arg __unused, struct ifnet *ifp)
{
	struct lagg_port *lp;
	struct lagg_softc *sc;

	if ((lp = ifp->if_lagg) == NULL)
		return;
	/* If the ifnet is just being renamed, don't do anything. */
	if (ifp->if_flags & IFF_RENAMING)
		return;

	sc = lp->lp_softc;

	LAGG_WLOCK(sc);
	lp->lp_detaching = 1;
	lagg_port_destroy(lp, 1);
	LAGG_WUNLOCK(sc);
}

static void
lagg_port2req(struct lagg_port *lp, struct lagg_reqport *rp)
{
	struct lagg_softc *sc = lp->lp_softc;

	strlcpy(rp->rp_ifname, sc->sc_ifname, sizeof(rp->rp_ifname));
	strlcpy(rp->rp_portname, lp->lp_ifp->if_xname, sizeof(rp->rp_portname));
	rp->rp_prio = lp->lp_prio;
	rp->rp_flags = lp->lp_flags;
	if (sc->sc_portreq != NULL)
		(*sc->sc_portreq)(lp, (caddr_t)&rp->rp_psc);

	/* Add protocol specific flags */
	switch (sc->sc_proto) {
		case LAGG_PROTO_FAILOVER:
			if (lp == sc->sc_primary)
				rp->rp_flags |= LAGG_PORT_MASTER;
			if (lp == lagg_link_active(sc, sc->sc_primary))
				rp->rp_flags |= LAGG_PORT_ACTIVE;
			break;

		case LAGG_PROTO_ROUNDROBIN:
		case LAGG_PROTO_LOADBALANCE:
		case LAGG_PROTO_ETHERCHANNEL:
              case LAGG_PROTO_BROADCAST:
			if (LAGG_PORTACTIVE(lp))
				rp->rp_flags |= LAGG_PORT_ACTIVE;
			break;

		case LAGG_PROTO_LACP:
			/* LACP has a different definition of active */
			if (lacp_isactive(lp))
				rp->rp_flags |= LAGG_PORT_ACTIVE;
			if (lacp_iscollecting(lp))
				rp->rp_flags |= LAGG_PORT_COLLECTING;
			if (lacp_isdistributing(lp))
				rp->rp_flags |= LAGG_PORT_DISTRIBUTING;
			break;
	}

}

static void
lagg_init(void *xsc)
{
	struct lagg_softc *sc = (struct lagg_softc *)xsc;
	struct lagg_port *lp;
	struct ifnet *ifp = sc->sc_ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	LAGG_WLOCK(sc);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	/* Update the port lladdrs */
	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		lagg_port_lladdr(lp, IF_LLADDR(ifp));

	if (sc->sc_init != NULL)
		(*sc->sc_init)(sc);

	LAGG_WUNLOCK(sc);
}

static void
lagg_stop(struct lagg_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	LAGG_WLOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	if (sc->sc_stop != NULL)
		(*sc->sc_stop)(sc);
}

static int
lagg_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct lagg_softc *sc = (struct lagg_softc *)ifp->if_softc;
	struct lagg_reqall *ra = (struct lagg_reqall *)data;
	struct lagg_reqport *rp = (struct lagg_reqport *)data, rpbuf;
	struct lagg_reqflags *rf = (struct lagg_reqflags *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	struct lagg_port *lp;
	struct ifnet *tpif;
	struct thread *td = curthread;
	char *buf, *outbuf;
	int count, buflen, len, error = 0;
	struct rm_priotracker tracker;

	bzero(&rpbuf, sizeof(rpbuf));

	switch (cmd) {
	case SIOCGLAGG:
		LAGG_RLOCK(sc, &tracker);
		count = 0;
		SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
			count++;
		buflen = count * sizeof(struct lagg_reqport);
		LAGG_RUNLOCK(sc, &tracker);

		outbuf = malloc(buflen, M_TEMP, M_WAITOK | M_ZERO);

		LAGG_RLOCK(sc, &tracker);
		ra->ra_proto = sc->sc_proto;
		if (sc->sc_req != NULL)
			(*sc->sc_req)(sc, (caddr_t)&ra->ra_psc);

		count = 0;
		buf = outbuf;
		len = min(ra->ra_size, buflen);
		SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
			if (len < sizeof(rpbuf))
				break;

			lagg_port2req(lp, &rpbuf);
			memcpy(buf, &rpbuf, sizeof(rpbuf));
			count++;
			buf += sizeof(rpbuf);
			len -= sizeof(rpbuf);
		}
		LAGG_RUNLOCK(sc, &tracker);
		ra->ra_ports = count;
		ra->ra_size = count * sizeof(rpbuf);
		error = copyout(outbuf, ra->ra_port, ra->ra_size);
		free(outbuf, M_TEMP);
		break;
	case SIOCSLAGG:
		error = priv_check(td, PRIV_NET_LAGG);
		if (error)
			break;
		if (ra->ra_proto >= LAGG_PROTO_MAX) {
			error = EPROTONOSUPPORT;
			break;
		}
		LAGG_WLOCK(sc);
		if (sc->sc_proto != LAGG_PROTO_NONE) {
			/* Reset protocol first in case detach unlocks */
			sc->sc_proto = LAGG_PROTO_NONE;
			error = sc->sc_detach(sc);
			sc->sc_detach = NULL;
			sc->sc_start = NULL;
			sc->sc_input = NULL;
			sc->sc_port_create = NULL;
			sc->sc_port_destroy = NULL;
			sc->sc_linkstate = NULL;
			sc->sc_init = NULL;
			sc->sc_stop = NULL;
			sc->sc_lladdr = NULL;
			sc->sc_req = NULL;
			sc->sc_portreq = NULL;
		} else if (sc->sc_input != NULL) {
			/* Still detaching */
			error = EBUSY;
		}
		if (error != 0) {
			LAGG_WUNLOCK(sc);
			break;
		}
		for (int i = 0; i < (sizeof(lagg_protos) /
		    sizeof(lagg_protos[0])); i++) {
			if (lagg_protos[i].ti_proto == ra->ra_proto) {
				if (sc->sc_ifflags & IFF_DEBUG)
					printf("%s: using proto %u\n",
					    sc->sc_ifname,
					    lagg_protos[i].ti_proto);
				sc->sc_proto = lagg_protos[i].ti_proto;
				if (sc->sc_proto != LAGG_PROTO_NONE)
					error = lagg_protos[i].ti_attach(sc);
				LAGG_WUNLOCK(sc);
				return (error);
			}
		}
		LAGG_WUNLOCK(sc);
		error = EPROTONOSUPPORT;
		break;
	case SIOCGLAGGFLAGS:
		rf->rf_flags = sc->sc_flags;
		break;
	case SIOCSLAGGHASH:
		error = priv_check(td, PRIV_NET_LAGG);
		if (error)
			break;
		if ((rf->rf_flags & LAGG_F_HASHMASK) == 0) {
			error = EINVAL;
			break;
		}
		LAGG_WLOCK(sc);
		sc->sc_flags &= ~LAGG_F_HASHMASK;
		sc->sc_flags |= rf->rf_flags & LAGG_F_HASHMASK;
		LAGG_WUNLOCK(sc);
		break;
	case SIOCGLAGGPORT:
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}

		LAGG_RLOCK(sc, &tracker);
		if ((lp = (struct lagg_port *)tpif->if_lagg) == NULL ||
		    lp->lp_softc != sc) {
			error = ENOENT;
			LAGG_RUNLOCK(sc, &tracker);
			break;
		}

		lagg_port2req(lp, rp);
		LAGG_RUNLOCK(sc, &tracker);
		break;
	case SIOCSLAGGPORT:
		error = priv_check(td, PRIV_NET_LAGG);
		if (error)
			break;
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}
		LAGG_WLOCK(sc);
		error = lagg_port_create(sc, tpif);
		LAGG_WUNLOCK(sc);
		break;
	case SIOCSLAGGDELPORT:
		error = priv_check(td, PRIV_NET_LAGG);
		if (error)
			break;
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}

		LAGG_WLOCK(sc);
		if ((lp = (struct lagg_port *)tpif->if_lagg) == NULL ||
		    lp->lp_softc != sc) {
			error = ENOENT;
			LAGG_WUNLOCK(sc);
			break;
		}

		error = lagg_port_destroy(lp, 1);
		LAGG_WUNLOCK(sc);
		break;
	case SIOCSIFFLAGS:
		/* Set flags on ports too */
		LAGG_WLOCK(sc);
		SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
			lagg_setflags(lp, 1);
		}
		LAGG_WUNLOCK(sc);

		if (!(ifp->if_flags & IFF_UP) &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			/*
			 * If interface is marked down and it is running,
			 * then stop and disable it.
			 */
			LAGG_WLOCK(sc);
			lagg_stop(sc);
			LAGG_WUNLOCK(sc);
		} else if ((ifp->if_flags & IFF_UP) &&
		    !(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			(*ifp->if_init)(sc);
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		LAGG_WLOCK(sc);
		error = lagg_ether_setmulti(sc);
		LAGG_WUNLOCK(sc);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	case SIOCSIFCAP:
	case SIOCSIFMTU:
		/* Do not allow the MTU or caps to be directly changed */
		error = EINVAL;
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return (error);
}

static int
lagg_ether_setmulti(struct lagg_softc *sc)
{
	struct lagg_port *lp;

	LAGG_WLOCK_ASSERT(sc);

	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		/* First, remove any existing filter entries. */
		lagg_ether_cmdmulti(lp, 0);
		/* copy all addresses from the lagg interface to the port */
		lagg_ether_cmdmulti(lp, 1);
	}
	return (0);
}

static int
lagg_ether_cmdmulti(struct lagg_port *lp, int set)
{
	struct lagg_softc *sc = lp->lp_softc;
	struct ifnet *ifp = lp->lp_ifp;
	struct ifnet *scifp = sc->sc_ifp;
	struct lagg_mc *mc;
	struct ifmultiaddr *ifma;
	int error;

	LAGG_WLOCK_ASSERT(sc);

	if (set) {
		IF_ADDR_WLOCK(scifp);
		TAILQ_FOREACH(ifma, &scifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			mc = malloc(sizeof(struct lagg_mc), M_DEVBUF, M_NOWAIT);
			if (mc == NULL) {
				IF_ADDR_WUNLOCK(scifp);
				return (ENOMEM);
			}
			bcopy(ifma->ifma_addr, &mc->mc_addr,
			    ifma->ifma_addr->sa_len);
			mc->mc_addr.sdl_index = ifp->if_index;
			mc->mc_ifma = NULL;
			SLIST_INSERT_HEAD(&lp->lp_mc_head, mc, mc_entries);
		}
		IF_ADDR_WUNLOCK(scifp);
		SLIST_FOREACH (mc, &lp->lp_mc_head, mc_entries) {
			error = if_addmulti(ifp,
			    (struct sockaddr *)&mc->mc_addr, &mc->mc_ifma);
			if (error)
				return (error);
		}
	} else {
		while ((mc = SLIST_FIRST(&lp->lp_mc_head)) != NULL) {
			SLIST_REMOVE(&lp->lp_mc_head, mc, lagg_mc, mc_entries);
			if (mc->mc_ifma && !lp->lp_detaching)
				if_delmulti_ifma(mc->mc_ifma);
			free(mc, M_DEVBUF);
		}
	}
	return (0);
}

/* Handle a ref counted flag that should be set on the lagg port as well */
static int
lagg_setflag(struct lagg_port *lp, int flag, int status,
	     int (*func)(struct ifnet *, int))
{
	struct lagg_softc *sc = lp->lp_softc;
	struct ifnet *scifp = sc->sc_ifp;
	struct ifnet *ifp = lp->lp_ifp;
	int error;

	LAGG_WLOCK_ASSERT(sc);

	status = status ? (scifp->if_flags & flag) : 0;
	/* Now "status" contains the flag value or 0 */

	/*
	 * See if recorded ports status is different from what
	 * we want it to be.  If it is, flip it.  We record ports
	 * status in lp_ifflags so that we won't clear ports flag
	 * we haven't set.  In fact, we don't clear or set ports
	 * flags directly, but get or release references to them.
	 * That's why we can be sure that recorded flags still are
	 * in accord with actual ports flags.
	 */
	if (status != (lp->lp_ifflags & flag)) {
		error = (*func)(ifp, status);
		if (error)
			return (error);
		lp->lp_ifflags &= ~flag;
		lp->lp_ifflags |= status;
	}
	return (0);
}

/*
 * Handle IFF_* flags that require certain changes on the lagg port
 * if "status" is true, update ports flags respective to the lagg
 * if "status" is false, forcedly clear the flags set on port.
 */
static int
lagg_setflags(struct lagg_port *lp, int status)
{
	int error, i;

	for (i = 0; lagg_pflags[i].flag; i++) {
		error = lagg_setflag(lp, lagg_pflags[i].flag,
		    status, lagg_pflags[i].func);
		if (error)
			return (error);
	}
	return (0);
}

static int
lagg_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct lagg_softc *sc = (struct lagg_softc *)ifp->if_softc;
	int error, len, mcast;
	struct rm_priotracker tracker;

	len = m->m_pkthdr.len;
	mcast = (m->m_flags & (M_MCAST | M_BCAST)) ? 1 : 0;

	LAGG_RLOCK(sc, &tracker);
	/* We need a Tx algorithm and at least one port */
	if (sc->sc_proto == LAGG_PROTO_NONE || sc->sc_count == 0) {
		LAGG_RUNLOCK(sc, &tracker);
		m_freem(m);
		ifp->if_oerrors++;
		return (ENXIO);
	}

	ETHER_BPF_MTAP(ifp, m);

	error = (*sc->sc_start)(sc, m);
	LAGG_RUNLOCK(sc, &tracker);

	if (error == 0) {
		counter_u64_add(sc->sc_opackets, 1);
		counter_u64_add(sc->sc_obytes, len);
		ifp->if_omcasts += mcast;
	} else
		ifp->if_oerrors++;

	return (error);
}

/*
 * The ifp->if_qflush entry point for lagg(4) is no-op.
 */
static void
lagg_qflush(struct ifnet *ifp __unused)
{
}

static struct mbuf *
lagg_input(struct ifnet *ifp, struct mbuf *m)
{
	struct lagg_port *lp = ifp->if_lagg;
	struct lagg_softc *sc = lp->lp_softc;
	struct ifnet *scifp = sc->sc_ifp;
	struct rm_priotracker tracker;

	LAGG_RLOCK(sc, &tracker);
	if ((scifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ||
	    (lp->lp_flags & LAGG_PORT_DISABLED) ||
	    sc->sc_proto == LAGG_PROTO_NONE) {
		LAGG_RUNLOCK(sc, &tracker);
		m_freem(m);
		return (NULL);
	}

	ETHER_BPF_MTAP(scifp, m);

	m = (*sc->sc_input)(sc, lp, m);

	if (m != NULL) {
		counter_u64_add(sc->sc_ipackets, 1);
		counter_u64_add(sc->sc_ibytes, m->m_pkthdr.len);

		if (scifp->if_flags & IFF_MONITOR) {
			m_freem(m);
			m = NULL;
		}
	}

	LAGG_RUNLOCK(sc, &tracker);
	return (m);
}

static int
lagg_media_change(struct ifnet *ifp)
{
	struct lagg_softc *sc = (struct lagg_softc *)ifp->if_softc;

	if (sc->sc_ifflags & IFF_DEBUG)
		printf("%s\n", __func__);

	/* Ignore */
	return (0);
}

static void
lagg_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct lagg_softc *sc = (struct lagg_softc *)ifp->if_softc;
	struct lagg_port *lp;
	struct rm_priotracker tracker;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_ETHER | IFM_AUTO;

	LAGG_RLOCK(sc, &tracker);
	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		if (LAGG_PORTACTIVE(lp))
			imr->ifm_status |= IFM_ACTIVE;
	}
	LAGG_RUNLOCK(sc, &tracker);
}

static void
lagg_linkstate(struct lagg_softc *sc)
{
	struct lagg_port *lp;
	int new_link = LINK_STATE_DOWN;
	uint64_t speed;

	/* Our link is considered up if at least one of our ports is active */
	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		if (lp->lp_link_state == LINK_STATE_UP) {
			new_link = LINK_STATE_UP;
			break;
		}
	}
	if_link_state_change(sc->sc_ifp, new_link);

	/* Update if_baudrate to reflect the max possible speed */
	switch (sc->sc_proto) {
		case LAGG_PROTO_FAILOVER:
			sc->sc_ifp->if_baudrate = sc->sc_primary != NULL ?
			    sc->sc_primary->lp_ifp->if_baudrate : 0;
			break;
		case LAGG_PROTO_ROUNDROBIN:
		case LAGG_PROTO_LOADBALANCE:
		case LAGG_PROTO_ETHERCHANNEL:
              case LAGG_PROTO_BROADCAST:
			speed = 0;
			SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
				speed += lp->lp_ifp->if_baudrate;
			sc->sc_ifp->if_baudrate = speed;
			break;
		case LAGG_PROTO_LACP:
			/* LACP updates if_baudrate itself */
			break;
	}
}

static void
lagg_port_state(struct ifnet *ifp, int state)
{
	struct lagg_port *lp = (struct lagg_port *)ifp->if_lagg;
	struct lagg_softc *sc = NULL;

	if (lp != NULL)
		sc = lp->lp_softc;
	if (sc == NULL)
		return;

	LAGG_WLOCK(sc);
	lagg_linkstate(sc);
	if (sc->sc_linkstate != NULL)
		(*sc->sc_linkstate)(lp);
	LAGG_WUNLOCK(sc);
}

struct lagg_port *
lagg_link_active(struct lagg_softc *sc, struct lagg_port *lp)
{
	struct lagg_port *lp_next, *rval = NULL;
	// int new_link = LINK_STATE_DOWN;

	LAGG_RLOCK_ASSERT(sc);
	/*
	 * Search a port which reports an active link state.
	 */

	if (lp == NULL)
		goto search;
	if (LAGG_PORTACTIVE(lp)) {
		rval = lp;
		goto found;
	}
	if ((lp_next = SLIST_NEXT(lp, lp_entries)) != NULL &&
	    LAGG_PORTACTIVE(lp_next)) {
		rval = lp_next;
		goto found;
	}

search:
	SLIST_FOREACH(lp_next, &sc->sc_ports, lp_entries) {
		if (LAGG_PORTACTIVE(lp_next)) {
			rval = lp_next;
			goto found;
		}
	}

found:
	if (rval != NULL) {
		/*
		 * The IEEE 802.1D standard assumes that a lagg with
		 * multiple ports is always full duplex. This is valid
		 * for load sharing laggs and if at least two links
		 * are active. Unfortunately, checking the latter would
		 * be too expensive at this point.
		 XXX
		if ((sc->sc_capabilities & IFCAP_LAGG_FULLDUPLEX) &&
		    (sc->sc_count > 1))
			new_link = LINK_STATE_FULL_DUPLEX;
		else
			new_link = rval->lp_link_state;
		 */
	}

	return (rval);
}

static const void *
lagg_gethdr(struct mbuf *m, u_int off, u_int len, void *buf)
{
	if (m->m_pkthdr.len < (off + len)) {
		return (NULL);
	} else if (m->m_len < (off + len)) {
		m_copydata(m, off, len, buf);
		return (buf);
	}
	return (mtod(m, char *) + off);
}

static int
lagg_sysctl_active(SYSCTL_HANDLER_ARGS)
{
	struct lagg_softc *sc = (struct lagg_softc *)arg1;
	struct lagg_port *lp;
	int error;

	/* LACP tracks active links automatically, the others do not */
	if (sc->sc_proto != LAGG_PROTO_LACP) {
		sc->sc_active = 0;
		SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
			sc->sc_active += LAGG_PORTACTIVE(lp);
	}

	error = sysctl_handle_int(oidp, &sc->sc_active, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	return (0);
}

uint32_t
lagg_hashmbuf(struct lagg_softc *sc, struct mbuf *m, uint32_t key)
{
	uint16_t etype;
	uint32_t p = key;
	int off;
	struct ether_header *eh;
	const struct ether_vlan_header *vlan;
#ifdef INET
	const struct ip *ip;
	const uint32_t *ports;
	int iphlen;
#endif
#ifdef INET6
	const struct ip6_hdr *ip6;
	uint32_t flow;
#endif
	union {
#ifdef INET
		struct ip ip;
#endif
#ifdef INET6
		struct ip6_hdr ip6;
#endif
		struct ether_vlan_header vlan;
		uint32_t port;
	} buf;


	off = sizeof(*eh);
	if (m->m_len < off)
		goto out;
	eh = mtod(m, struct ether_header *);
	etype = ntohs(eh->ether_type);
	if (sc->sc_flags & LAGG_F_HASHL2) {
		p = hash32_buf(&eh->ether_shost, ETHER_ADDR_LEN, p);
		p = hash32_buf(&eh->ether_dhost, ETHER_ADDR_LEN, p);
	}

	/* Special handling for encapsulating VLAN frames */
	if ((m->m_flags & M_VLANTAG) && (sc->sc_flags & LAGG_F_HASHL2)) {
		p = hash32_buf(&m->m_pkthdr.ether_vtag,
		    sizeof(m->m_pkthdr.ether_vtag), p);
	} else if (etype == ETHERTYPE_VLAN) {
		vlan = lagg_gethdr(m, off,  sizeof(*vlan), &buf);
		if (vlan == NULL)
			goto out;

		if (sc->sc_flags & LAGG_F_HASHL2)
			p = hash32_buf(&vlan->evl_tag, sizeof(vlan->evl_tag), p);
		etype = ntohs(vlan->evl_proto);
		off += sizeof(*vlan) - sizeof(*eh);
	}

	switch (etype) {
#ifdef INET
	case ETHERTYPE_IP:
		ip = lagg_gethdr(m, off, sizeof(*ip), &buf);
		if (ip == NULL)
			goto out;

		if (sc->sc_flags & LAGG_F_HASHL3) {
			p = hash32_buf(&ip->ip_src, sizeof(struct in_addr), p);
			p = hash32_buf(&ip->ip_dst, sizeof(struct in_addr), p);
		}
		if (!(sc->sc_flags & LAGG_F_HASHL4))
			break;
		switch (ip->ip_p) {
			case IPPROTO_TCP:
			case IPPROTO_UDP:
			case IPPROTO_SCTP:
				iphlen = ip->ip_hl << 2;
				if (iphlen < sizeof(*ip))
					break;
				off += iphlen;
				ports = lagg_gethdr(m, off, sizeof(*ports), &buf);
				if (ports == NULL)
					break;
				p = hash32_buf(ports, sizeof(*ports), p);
				break;
		}
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		if (!(sc->sc_flags & LAGG_F_HASHL3))
			break;
		ip6 = lagg_gethdr(m, off, sizeof(*ip6), &buf);
		if (ip6 == NULL)
			goto out;

		p = hash32_buf(&ip6->ip6_src, sizeof(struct in6_addr), p);
		p = hash32_buf(&ip6->ip6_dst, sizeof(struct in6_addr), p);
		flow = ip6->ip6_flow & IPV6_FLOWLABEL_MASK;
		p = hash32_buf(&flow, sizeof(flow), p);	/* IPv6 flow label */
		break;
#endif
	}
out:
	return (p);
}

int
lagg_enqueue(struct ifnet *ifp, struct mbuf *m)
{

	return (ifp->if_transmit)(ifp, m);
}

/*
 * Simple round robin aggregation
 */

static int
lagg_rr_attach(struct lagg_softc *sc)
{
	sc->sc_detach = lagg_rr_detach;
	sc->sc_start = lagg_rr_start;
	sc->sc_input = lagg_rr_input;
	sc->sc_port_create = NULL;
	sc->sc_capabilities = IFCAP_LAGG_FULLDUPLEX;
	sc->sc_seq = 0;

	return (0);
}

static int
lagg_rr_detach(struct lagg_softc *sc)
{
	return (0);
}

static int
lagg_rr_start(struct lagg_softc *sc, struct mbuf *m)
{
	struct lagg_port *lp;
	uint32_t p;

	p = atomic_fetchadd_32(&sc->sc_seq, 1);
	p %= sc->sc_count;
	lp = SLIST_FIRST(&sc->sc_ports);
	while (p--)
		lp = SLIST_NEXT(lp, lp_entries);

	/*
	 * Check the port's link state. This will return the next active
	 * port if the link is down or the port is NULL.
	 */
	if ((lp = lagg_link_active(sc, lp)) == NULL) {
		m_freem(m);
		return (ENETDOWN);
	}

	/* Send mbuf */
	return (lagg_enqueue(lp->lp_ifp, m));
}

static struct mbuf *
lagg_rr_input(struct lagg_softc *sc, struct lagg_port *lp, struct mbuf *m)
{
	struct ifnet *ifp = sc->sc_ifp;

	/* Just pass in the packet to our lagg device */
	m->m_pkthdr.rcvif = ifp;

	return (m);
}

/*
 * Broadcast mode
 */

static int
lagg_bcast_attach(struct lagg_softc *sc)
{
       sc->sc_detach = lagg_bcast_detach;
       sc->sc_start = lagg_bcast_start;
       sc->sc_input = lagg_bcast_input;
       sc->sc_port_create = NULL;
       sc->sc_port_destroy = NULL;
       sc->sc_linkstate = NULL;
       sc->sc_req = NULL;
       sc->sc_portreq = NULL;

       return (0);
}

static int
lagg_bcast_detach(struct lagg_softc *sc)
{
       return (0);
}

static int
lagg_bcast_start(struct lagg_softc *sc, struct mbuf *m)
{
       int active_ports = 0;
       int errors = 0;
       int ret;
       struct lagg_port *lp, *last = NULL;
       struct mbuf *m0;

       SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
              if (!LAGG_PORTACTIVE(lp))
                     continue;

              active_ports++;

              if (last != NULL) {
                     m0 = m_copym(m, 0, M_COPYALL,
                                   M_NOWAIT);
                     if (m0 == NULL) {
                            ret = ENOBUFS;
                            errors++;
                            break;
                     }

                     ret = lagg_enqueue(last->lp_ifp, m0);
                     if (ret != 0)
                            errors++;
              }
              last = lp;
       }
       if (last == NULL) {
              m_freem(m);
              return (ENOENT);
       }
       if ((last = lagg_link_active(sc, last)) == NULL) {
              m_freem(m);
              return (ENETDOWN);
       }

       ret = lagg_enqueue(last->lp_ifp, m);
       if (ret != 0)
              errors++;

       if (errors == 0)
              return (ret);

       return (0);
}

static struct mbuf*
lagg_bcast_input(struct lagg_softc *sc, struct lagg_port *lp,
                     struct mbuf *m)
{
       struct ifnet *ifp = sc->sc_ifp;

       /* Just pass in the packet to our lagg device */
       m->m_pkthdr.rcvif = ifp;
       return (m);
}

/*
 * Active failover
 */

static int
lagg_fail_attach(struct lagg_softc *sc)
{
	sc->sc_detach = lagg_fail_detach;
	sc->sc_start = lagg_fail_start;
	sc->sc_input = lagg_fail_input;
	sc->sc_port_create = NULL;
	sc->sc_port_destroy = NULL;

	return (0);
}

static int
lagg_fail_detach(struct lagg_softc *sc)
{
	return (0);
}

static int
lagg_fail_start(struct lagg_softc *sc, struct mbuf *m)
{
	struct lagg_port *lp;

	/* Use the master port if active or the next available port */
	if ((lp = lagg_link_active(sc, sc->sc_primary)) == NULL) {
		m_freem(m);
		return (ENETDOWN);
	}

	/* Send mbuf */
	return (lagg_enqueue(lp->lp_ifp, m));
}

static struct mbuf *
lagg_fail_input(struct lagg_softc *sc, struct lagg_port *lp, struct mbuf *m)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct lagg_port *tmp_tp;

	if (lp == sc->sc_primary || lagg_failover_rx_all) {
		m->m_pkthdr.rcvif = ifp;
		return (m);
	}

	if (!LAGG_PORTACTIVE(sc->sc_primary)) {
		tmp_tp = lagg_link_active(sc, sc->sc_primary);
		/*
		 * If tmp_tp is null, we've recieved a packet when all
		 * our links are down. Weird, but process it anyways.
		 */
		if ((tmp_tp == NULL || tmp_tp == lp)) {
			m->m_pkthdr.rcvif = ifp;
			return (m);
		}
	}

	m_freem(m);
	return (NULL);
}

/*
 * Loadbalancing
 */

static int
lagg_lb_attach(struct lagg_softc *sc)
{
	struct lagg_port *lp;
	struct lagg_lb *lb;

	if ((lb = (struct lagg_lb *)malloc(sizeof(struct lagg_lb),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	sc->sc_detach = lagg_lb_detach;
	sc->sc_start = lagg_lb_start;
	sc->sc_input = lagg_lb_input;
	sc->sc_port_create = lagg_lb_port_create;
	sc->sc_port_destroy = lagg_lb_port_destroy;
	sc->sc_capabilities = IFCAP_LAGG_FULLDUPLEX;

	lb->lb_key = arc4random();
	sc->sc_psc = (caddr_t)lb;

	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		lagg_lb_port_create(lp);

	return (0);
}

static int
lagg_lb_detach(struct lagg_softc *sc)
{
	struct lagg_lb *lb = (struct lagg_lb *)sc->sc_psc;
	if (lb != NULL)
		free(lb, M_DEVBUF);
	return (0);
}

static int
lagg_lb_porttable(struct lagg_softc *sc, struct lagg_port *lp)
{
	struct lagg_lb *lb = (struct lagg_lb *)sc->sc_psc;
	struct lagg_port *lp_next;
	int i = 0;

	bzero(&lb->lb_ports, sizeof(lb->lb_ports));
	SLIST_FOREACH(lp_next, &sc->sc_ports, lp_entries) {
		if (lp_next == lp)
			continue;
		if (i >= LAGG_MAX_PORTS)
			return (EINVAL);
		if (sc->sc_ifflags & IFF_DEBUG)
			printf("%s: port %s at index %d\n",
			    sc->sc_ifname, lp_next->lp_ifname, i);
		lb->lb_ports[i++] = lp_next;
	}

	return (0);
}

static int
lagg_lb_port_create(struct lagg_port *lp)
{
	struct lagg_softc *sc = lp->lp_softc;
	return (lagg_lb_porttable(sc, NULL));
}

static void
lagg_lb_port_destroy(struct lagg_port *lp)
{
	struct lagg_softc *sc = lp->lp_softc;
	lagg_lb_porttable(sc, lp);
}

static int
lagg_lb_start(struct lagg_softc *sc, struct mbuf *m)
{
	struct lagg_lb *lb = (struct lagg_lb *)sc->sc_psc;
	struct lagg_port *lp = NULL;
	uint32_t p = 0;

	if (sc->use_flowid && (m->m_flags & M_FLOWID))
		p = m->m_pkthdr.flowid >> sc->flowid_shift;
	else
		p = lagg_hashmbuf(sc, m, lb->lb_key);
	p %= sc->sc_count;
	lp = lb->lb_ports[p];

	/*
	 * Check the port's link state. This will return the next active
	 * port if the link is down or the port is NULL.
	 */
	if ((lp = lagg_link_active(sc, lp)) == NULL) {
		m_freem(m);
		return (ENETDOWN);
	}

	/* Send mbuf */
	return (lagg_enqueue(lp->lp_ifp, m));
}

static struct mbuf *
lagg_lb_input(struct lagg_softc *sc, struct lagg_port *lp, struct mbuf *m)
{
	struct ifnet *ifp = sc->sc_ifp;

	/* Just pass in the packet to our lagg device */
	m->m_pkthdr.rcvif = ifp;

	return (m);
}

/*
 * 802.3ad LACP
 */

static int
lagg_lacp_attach(struct lagg_softc *sc)
{
	struct lagg_port *lp;
	int error;

	sc->sc_detach = lagg_lacp_detach;
	sc->sc_port_create = lacp_port_create;
	sc->sc_port_destroy = lacp_port_destroy;
	sc->sc_linkstate = lacp_linkstate;
	sc->sc_start = lagg_lacp_start;
	sc->sc_input = lagg_lacp_input;
	sc->sc_init = lacp_init;
	sc->sc_stop = lacp_stop;
	sc->sc_lladdr = lagg_lacp_lladdr;
	sc->sc_req = lacp_req;
	sc->sc_portreq = lacp_portreq;

	error = lacp_attach(sc);
	if (error)
		return (error);

	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		lacp_port_create(lp);

	return (error);
}

static int
lagg_lacp_detach(struct lagg_softc *sc)
{
	struct lagg_port *lp;
	int error;

	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		lacp_port_destroy(lp);

	/* unlocking is safe here */
	LAGG_WUNLOCK(sc);
	error = lacp_detach(sc);
	LAGG_WLOCK(sc);

	return (error);
}

static void
lagg_lacp_lladdr(struct lagg_softc *sc)
{
	struct lagg_port *lp;

	/* purge all the lacp ports */
	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		lacp_port_destroy(lp);

	/* add them back in */
	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		lacp_port_create(lp);
}

static int
lagg_lacp_start(struct lagg_softc *sc, struct mbuf *m)
{
	struct lagg_port *lp;

	lp = lacp_select_tx_port(sc, m);
	if (lp == NULL) {
		m_freem(m);
		return (ENETDOWN);
	}

	/* Send mbuf */
	return (lagg_enqueue(lp->lp_ifp, m));
}

static struct mbuf *
lagg_lacp_input(struct lagg_softc *sc, struct lagg_port *lp, struct mbuf *m)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ether_header *eh;
	u_short etype;

	eh = mtod(m, struct ether_header *);
	etype = ntohs(eh->ether_type);

	/* Tap off LACP control messages */
	if ((m->m_flags & M_VLANTAG) == 0 && etype == ETHERTYPE_SLOW) {
		m = lacp_input(lp, m);
		if (m == NULL)
			return (NULL);
	}

	/*
	 * If the port is not collecting or not in the active aggregator then
	 * free and return.
	 */
	if (lacp_iscollecting(lp) == 0 || lacp_isactive(lp) == 0) {
		m_freem(m);
		return (NULL);
	}

	m->m_pkthdr.rcvif = ifp;
	return (m);
}

static void
lagg_callout(void *arg)
{
	struct lagg_softc *sc = (struct lagg_softc *)arg;
	struct ifnet *ifp = sc->sc_ifp;

	ifp->if_ipackets = counter_u64_fetch(sc->sc_ipackets);
	ifp->if_opackets = counter_u64_fetch(sc->sc_opackets);
	ifp->if_ibytes = counter_u64_fetch(sc->sc_ibytes);
	ifp->if_obytes = counter_u64_fetch(sc->sc_obytes);

	callout_reset(&sc->sc_callout, hz, lagg_callout, sc);
}
