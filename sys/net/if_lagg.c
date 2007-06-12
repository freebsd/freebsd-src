/*	$OpenBSD: if_trunk.c,v 1.30 2007/01/31 06:20:19 reyk Exp $	*/

/*
 * Copyright (c) 2005, 2006 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/rwlock.h>
#include <sys/taskqueue.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#endif

#ifdef INET6
#include <netinet/ip6.h>
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
static void	lagg_lladdr(struct lagg_softc *, uint8_t *);
static int	lagg_capabilities(struct lagg_softc *);
static void	lagg_port_lladdr(struct lagg_port *, uint8_t *);
static void	lagg_port_setlladdr(void *, int);
static int	lagg_port_create(struct lagg_softc *, struct ifnet *);
static int	lagg_port_destroy(struct lagg_port *, int);
static struct mbuf *lagg_input(struct ifnet *, struct mbuf *);
static void	lagg_port_state(struct ifnet *, int);
static int	lagg_port_ioctl(struct ifnet *, u_long, caddr_t);
static int	lagg_port_output(struct ifnet *, struct mbuf *,
		    struct sockaddr *, struct rtentry *);
static void	lagg_port_ifdetach(void *arg __unused, struct ifnet *);
static int	lagg_port_checkstacking(struct lagg_softc *);
static void	lagg_port2req(struct lagg_port *, struct lagg_reqport *);
static void	lagg_init(void *);
static void	lagg_stop(struct lagg_softc *);
static int	lagg_ioctl(struct ifnet *, u_long, caddr_t);
static int	lagg_ether_setmulti(struct lagg_softc *);
static int	lagg_ether_cmdmulti(struct lagg_port *, int);
static	int	lagg_setflag(struct lagg_port *, int, int,
		    int (*func)(struct ifnet *, int));
static	int	lagg_setflags(struct lagg_port *, int status);
static void	lagg_start(struct ifnet *);
static int	lagg_media_change(struct ifnet *);
static void	lagg_media_status(struct ifnet *, struct ifmediareq *);
static struct lagg_port *lagg_link_active(struct lagg_softc *,
	    struct lagg_port *);
static const void *lagg_gethdr(struct mbuf *, u_int, u_int, void *);

IFC_SIMPLE_DECLARE(lagg, 0);

/* Simple round robin */
static int	lagg_rr_attach(struct lagg_softc *);
static int	lagg_rr_detach(struct lagg_softc *);
static void	lagg_rr_port_destroy(struct lagg_port *);
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

/* 802.3ad LACP */
static int	lagg_lacp_attach(struct lagg_softc *);
static int	lagg_lacp_detach(struct lagg_softc *);
static int	lagg_lacp_start(struct lagg_softc *, struct mbuf *);
static struct mbuf *lagg_lacp_input(struct lagg_softc *, struct lagg_port *,
		    struct mbuf *);
static void	lagg_lacp_lladdr(struct lagg_softc *);

/* lagg protocol table */
static const struct {
	int			ti_proto;
	int			(*ti_attach)(struct lagg_softc *);
} lagg_protos[] = {
	{ LAGG_PROTO_ROUNDROBIN,	lagg_rr_attach },
	{ LAGG_PROTO_FAILOVER,		lagg_fail_attach },
	{ LAGG_PROTO_LOADBALANCE,	lagg_lb_attach },
	{ LAGG_PROTO_ETHERCHANNEL,	lagg_lb_attach },
	{ LAGG_PROTO_LACP,		lagg_lacp_attach },
	{ LAGG_PROTO_NONE,		NULL }
};

static int
lagg_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		mtx_init(&lagg_list_mtx, "if_lagg list", NULL, MTX_DEF);
		SLIST_INIT(&lagg_list);
		if_clone_attach(&lagg_cloner);
		lagg_input_p = lagg_input;
		lagg_linkstate_p = lagg_port_state;
		lagg_detach_cookie = EVENTHANDLER_REGISTER(
		    ifnet_departure_event, lagg_port_ifdetach, NULL,
		    EVENTHANDLER_PRI_ANY);
		break;
	case MOD_UNLOAD:
		EVENTHANDLER_DEREGISTER(ifnet_departure_event,
		    lagg_detach_cookie);
		if_clone_detach(&lagg_cloner);
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

static int
lagg_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct lagg_softc *sc;
	struct ifnet *ifp;
	int i, error = 0;
	static const u_char eaddr[6];	/* 00:00:00:00:00:00 */

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	ifp = sc->sc_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		free(sc, M_DEVBUF);
		return (ENOSPC);
	}

	sc->sc_proto = LAGG_PROTO_NONE;
	for (i = 0; lagg_protos[i].ti_proto != LAGG_PROTO_NONE; i++) {
		if (lagg_protos[i].ti_proto == LAGG_PROTO_DEFAULT) {
			sc->sc_proto = lagg_protos[i].ti_proto;
			if ((error = lagg_protos[i].ti_attach(sc)) != 0) {
				if_free_type(ifp, IFT_ETHER);
				free(sc, M_DEVBUF);
				return (error);
			}
			break;
		}
	}
	LAGG_LOCK_INIT(sc);
	SLIST_INIT(&sc->sc_ports);
	TASK_INIT(&sc->sc_lladdr_task, 0, lagg_port_setlladdr, sc);

	/* Initialise pseudo media types */
	ifmedia_init(&sc->sc_media, 0, lagg_media_change,
	    lagg_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_initname(ifp, ifc->ifc_name, unit);
	ifp->if_type = IFT_ETHER;
	ifp->if_softc = sc;
	ifp->if_start = lagg_start;
	ifp->if_init = lagg_init;
	ifp->if_ioctl = lagg_ioctl;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST;

	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Attach as an ordinary ethernet device, childs will be attached
	 * as special device IFT_IEEE8023ADLAG.
	 */
	ether_ifattach(ifp, eaddr);

	/* Insert into the global list of laggs */
	mtx_lock(&lagg_list_mtx);
	SLIST_INSERT_HEAD(&lagg_list, sc, sc_entries);
	mtx_unlock(&lagg_list_mtx);

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

	/* Shutdown and remove lagg ports */
	while ((lp = SLIST_FIRST(&sc->sc_ports)) != NULL)
		lagg_port_destroy(lp, 1);
	/* Unhook the aggregation protocol */
	if (sc->sc_detach != NULL)
		(*sc->sc_detach)(sc);

	LAGG_WUNLOCK(sc);

	ifmedia_removeall(&sc->sc_media);
	ether_ifdetach(ifp);
	if_free_type(ifp, IFT_ETHER);

	mtx_lock(&lagg_list_mtx);
	SLIST_REMOVE(&lagg_list, sc, lagg_softc, sc_entries);
	mtx_unlock(&lagg_list_mtx);

	taskqueue_drain(taskqueue_swi, &sc->sc_lladdr_task);
	LAGG_LOCK_DESTROY(sc);
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
}

static int
lagg_capabilities(struct lagg_softc *sc)
{
	struct lagg_port *lp;
	int cap = ~0, priv;

	LAGG_WLOCK_ASSERT(sc);

	/* Preserve private capabilities */
	priv = sc->sc_capabilities & IFCAP_LAGG_MASK;

	/* Get capabilities from the lagg ports */
	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		cap &= lp->lp_capabilities;

	if (sc->sc_ifflags & IFF_DEBUG) {
		printf("%s: capabilities 0x%08x\n",
		    sc->sc_ifname, cap == ~0 ? priv : (cap | priv));
	}

	return (cap == ~0 ? priv : (cap | priv));
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
		error = if_setlladdr(ifp, llq->llq_lladdr, ETHER_ADDR_LEN);
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

	/* New lagg port has to be in an idle state */
	if (ifp->if_drv_flags & IFF_DRV_OACTIVE)
		return (EBUSY);

	/* Check if port has already been associated to a lagg */
	if (ifp->if_lagg != NULL)
		return (EBUSY);

	/* XXX Disallow non-ethernet interfaces (this should be any of 802) */
	if (ifp->if_type != IFT_ETHER)
		return (EPROTONOSUPPORT);

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
			/* XXX disable stacking for the moment, its untested
			lp->lp_flags |= LAGG_PORT_STACK;
			if (lagg_port_checkstacking(sc_ptr) >=
			    LAGG_MAX_STACKING) {
				mtx_unlock(&lagg_list_mtx);
				free(lp, M_DEVBUF);
				return (E2BIG);
			}
			*/
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
	sc->sc_capabilities = lagg_capabilities(sc);

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
	sc->sc_capabilities = lagg_capabilities(sc);

	return (0);
}

static int
lagg_port_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct lagg_reqport *rp = (struct lagg_reqport *)data;
	struct lagg_softc *sc;
	struct lagg_port *lp = NULL;
	int error = 0;

	/* Should be checked by the caller */
	if (ifp->if_type != IFT_IEEE8023ADLAG ||
	    (lp = ifp->if_lagg) == NULL || (sc = lp->lp_softc) == NULL)
		goto fallback;

	switch (cmd) {
	case SIOCGLAGGPORT:
		LAGG_RLOCK(sc);
		if (rp->rp_portname[0] == '\0' ||
		    ifunit(rp->rp_portname) != ifp) {
			error = EINVAL;
			break;
		}

		if (lp->lp_softc != sc) {
			error = ENOENT;
			break;
		}

		lagg_port2req(lp, rp);
		LAGG_RUNLOCK(sc);
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

static int
lagg_port_output(struct ifnet *ifp, struct mbuf *m,
	struct sockaddr *dst, struct rtentry *rt0)
{
	struct lagg_port *lp = ifp->if_lagg;
	struct ether_header *eh;
	short type = 0;

	switch (dst->sa_family) {
		case pseudo_AF_HDRCMPLT:
		case AF_UNSPEC:
			eh = (struct ether_header *)dst->sa_data;
			type = eh->ether_type;
			break;
	}

	/*
	 * Only allow ethernet types required to initiate or maintain the link,
	 * aggregated frames take a different path.
	 */
	switch (ntohs(type)) {
		case ETHERTYPE_PAE:	/* EAPOL PAE/802.1x */
			return ((*lp->lp_output)(ifp, m, dst, rt0));
	}

	/* drop any other frames */
	m_freem(m);
	return (EBUSY);
}

static void
lagg_port_ifdetach(void *arg __unused, struct ifnet *ifp)
{
	struct lagg_port *lp;
	struct lagg_softc *sc;

	if ((lp = ifp->if_lagg) == NULL)
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

	/* Add protocol specific flags */
	switch (sc->sc_proto) {
		case LAGG_PROTO_FAILOVER:
			if (lp == sc->sc_primary)
				rp->rp_flags |= LAGG_PORT_MASTER;
			/* FALLTHROUGH */
		case LAGG_PROTO_ROUNDROBIN:
		case LAGG_PROTO_LOADBALANCE:
		case LAGG_PROTO_ETHERCHANNEL:
			if (LAGG_PORTACTIVE(lp))
				rp->rp_flags |= LAGG_PORT_ACTIVE;
			break;

		case LAGG_PROTO_LACP:
			/* LACP has a different definition of active */
			if (lacp_port_isactive(lp))
				rp->rp_flags |= LAGG_PORT_ACTIVE;
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
	struct ifreq *ifr = (struct ifreq *)data;
	struct lagg_port *lp;
	struct ifnet *tpif;
	struct thread *td = curthread;
	int i, error = 0, unlock = 1;

	LAGG_WLOCK(sc);

	bzero(&rpbuf, sizeof(rpbuf));

	switch (cmd) {
	case SIOCGLAGG:
		ra->ra_proto = sc->sc_proto;
		ra->ra_ports = i = 0;
		lp = SLIST_FIRST(&sc->sc_ports);
		while (lp && ra->ra_size >=
		    i + sizeof(struct lagg_reqport)) {
			lagg_port2req(lp, &rpbuf);
			error = copyout(&rpbuf, (caddr_t)ra->ra_port + i,
			    sizeof(struct lagg_reqport));
			if (error)
				break;
			i += sizeof(struct lagg_reqport);
			ra->ra_ports++;
			lp = SLIST_NEXT(lp, lp_entries);
		}
		break;
	case SIOCSLAGG:
		error = priv_check(td, PRIV_NET_LAGG);
		if (error)
			break;
		if (ra->ra_proto >= LAGG_PROTO_MAX) {
			error = EPROTONOSUPPORT;
			break;
		}
		if (sc->sc_proto != LAGG_PROTO_NONE) {
			error = sc->sc_detach(sc);
			/* Reset protocol and pointers */
			sc->sc_proto = LAGG_PROTO_NONE;
			sc->sc_detach = NULL;
			sc->sc_start = NULL;
			sc->sc_input = NULL;
			sc->sc_port_create = NULL;
			sc->sc_port_destroy = NULL;
			sc->sc_linkstate = NULL;
			sc->sc_init = NULL;
			sc->sc_stop = NULL;
			sc->sc_lladdr = NULL;
		}
		if (error != 0)
			break;
		for (i = 0; i < (sizeof(lagg_protos) /
		    sizeof(lagg_protos[0])); i++) {
			if (lagg_protos[i].ti_proto == ra->ra_proto) {
				if (sc->sc_ifflags & IFF_DEBUG)
					printf("%s: using proto %u\n",
					    sc->sc_ifname,
					    lagg_protos[i].ti_proto);
				sc->sc_proto = lagg_protos[i].ti_proto;
				if (sc->sc_proto != LAGG_PROTO_NONE)
					error = lagg_protos[i].ti_attach(sc);
				goto out;
			}
		}
		error = EPROTONOSUPPORT;
		break;
	case SIOCGLAGGPORT:
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}

		if ((lp = (struct lagg_port *)tpif->if_lagg) == NULL ||
		    lp->lp_softc != sc) {
			error = ENOENT;
			break;
		}

		lagg_port2req(lp, rp);
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
		error = lagg_port_create(sc, tpif);
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

		if ((lp = (struct lagg_port *)tpif->if_lagg) == NULL ||
		    lp->lp_softc != sc) {
			error = ENOENT;
			break;
		}

		error = lagg_port_destroy(lp, 1);
		break;
	case SIOCSIFFLAGS:
		/* Set flags on ports too */
		SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
			lagg_setflags(lp, 1);
		}

		if (!(ifp->if_flags & IFF_UP) &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			/*
			 * If interface is marked down and it is running,
			 * then stop and disable it.
			 */
			lagg_stop(sc);
		} else if ((ifp->if_flags & IFF_UP) &&
		    !(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			LAGG_WUNLOCK(sc);
			unlock = 0;
			(*ifp->if_init)(sc);
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = lagg_ether_setmulti(sc);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		LAGG_WUNLOCK(sc);
		unlock = 0;
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;
	default:
		LAGG_WUNLOCK(sc);
		unlock = 0;
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

out:
	if (unlock)
		LAGG_WUNLOCK(sc);
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
	struct ifmultiaddr *ifma, *rifma = NULL;
	struct sockaddr_dl sdl;
	int error;

	LAGG_WLOCK_ASSERT(sc);

	bzero((char *)&sdl, sizeof(sdl));
	sdl.sdl_len = sizeof(sdl);
	sdl.sdl_family = AF_LINK;
	sdl.sdl_type = IFT_ETHER;
	sdl.sdl_alen = ETHER_ADDR_LEN;
	sdl.sdl_index = ifp->if_index;

	if (set) {
		TAILQ_FOREACH(ifma, &scifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
			    LLADDR(&sdl), ETHER_ADDR_LEN);

			error = if_addmulti(ifp, (struct sockaddr *)&sdl, &rifma);
			if (error)
				return (error);
			mc = malloc(sizeof(struct lagg_mc), M_DEVBUF, M_NOWAIT);
			if (mc == NULL)
				return (ENOMEM);
			mc->mc_ifma = rifma;
			SLIST_INSERT_HEAD(&lp->lp_mc_head, mc, mc_entries);
		}
	} else {
		while ((mc = SLIST_FIRST(&lp->lp_mc_head)) != NULL) {
			SLIST_REMOVE(&lp->lp_mc_head, mc, lagg_mc, mc_entries);
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

static void
lagg_start(struct ifnet *ifp)
{
	struct lagg_softc *sc = (struct lagg_softc *)ifp->if_softc;
	struct mbuf *m;
	int error = 0;

	LAGG_RLOCK(sc);
	for (;; error = 0) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		BPF_MTAP(ifp, m);

		if (sc->sc_proto != LAGG_PROTO_NONE)
			error = (*sc->sc_start)(sc, m);
		else
			m_freem(m);

		if (error == 0)
			ifp->if_opackets++;
		else {
			m_freem(m); /* sc_start failed */
			ifp->if_oerrors++;
		}
	}
	LAGG_RUNLOCK(sc);

	return;
}

static struct mbuf *
lagg_input(struct ifnet *ifp, struct mbuf *m)
{
	struct lagg_port *lp = ifp->if_lagg;
	struct lagg_softc *sc = lp->lp_softc;
	struct ifnet *scifp = sc->sc_ifp;

	if ((scifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ||
	    (lp->lp_flags & LAGG_PORT_DISABLED) ||
	    sc->sc_proto == LAGG_PROTO_NONE) {
		m_freem(m);
		return (NULL);
	}

	LAGG_RLOCK(sc);
	BPF_MTAP(scifp, m);

	m = (*sc->sc_input)(sc, lp, m);

	if (m != NULL) {
		scifp->if_ipackets++;
		scifp->if_ibytes += m->m_pkthdr.len;
	}

	LAGG_RUNLOCK(sc);
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

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_ETHER | IFM_AUTO;

	LAGG_RLOCK(sc);
	SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		if (LAGG_PORTACTIVE(lp))
			imr->ifm_status |= IFM_ACTIVE;
	}
	LAGG_RUNLOCK(sc);
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

uint32_t
lagg_hashmbuf(struct mbuf *m, uint32_t key)
{
	uint16_t etype;
	uint32_t p = 0;
	int off;
	struct ether_header *eh;
	struct ether_vlan_header vlanbuf;
	const struct ether_vlan_header *vlan;
#ifdef INET
	const struct ip *ip;
	struct ip ipbuf;
#endif
#ifdef INET6
	const struct ip6_hdr *ip6;
	struct ip6_hdr ip6buf;
	uint32_t flow;
#endif

	off = sizeof(*eh);
	if (m->m_len < off)
		goto out;
	eh = mtod(m, struct ether_header *);
	etype = ntohs(eh->ether_type);
	p = hash32_buf(&eh->ether_shost, ETHER_ADDR_LEN, key);
	p = hash32_buf(&eh->ether_dhost, ETHER_ADDR_LEN, p);

	/* Special handling for encapsulating VLAN frames */
	if (m->m_flags & M_VLANTAG) {
		p = hash32_buf(&m->m_pkthdr.ether_vtag,
		    sizeof(m->m_pkthdr.ether_vtag), p);
	} else if (etype == ETHERTYPE_VLAN) {
		vlan = lagg_gethdr(m, off,  sizeof(*vlan), &vlanbuf);
		if (vlan == NULL)
			goto out;

		p = hash32_buf(&vlan->evl_tag, sizeof(vlan->evl_tag), p);
		etype = ntohs(vlan->evl_proto);
		off += sizeof(*vlan) - sizeof(*eh);
	}

	switch (etype) {
#ifdef INET
	case ETHERTYPE_IP:
		ip = lagg_gethdr(m, off, sizeof(*ip), &ipbuf);
		if (ip == NULL)
			goto out;

		p = hash32_buf(&ip->ip_src, sizeof(struct in_addr), p);
		p = hash32_buf(&ip->ip_dst, sizeof(struct in_addr), p);
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		ip6 = lagg_gethdr(m, off, sizeof(*ip6), &ip6buf);
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
	int error = 0;

	IFQ_HANDOFF(ifp, m, error);
	return (error);
}

/*
 * Simple round robin aggregation
 */

static int
lagg_rr_attach(struct lagg_softc *sc)
{
	struct lagg_port *lp;

	sc->sc_detach = lagg_rr_detach;
	sc->sc_start = lagg_rr_start;
	sc->sc_input = lagg_rr_input;
	sc->sc_port_create = NULL;
	sc->sc_port_destroy = lagg_rr_port_destroy;
	sc->sc_capabilities = IFCAP_LAGG_FULLDUPLEX;

	lp = SLIST_FIRST(&sc->sc_ports);
	sc->sc_psc = (caddr_t)lp;

	return (0);
}

static int
lagg_rr_detach(struct lagg_softc *sc)
{
	sc->sc_psc = NULL;
	return (0);
}

static void
lagg_rr_port_destroy(struct lagg_port *lp)
{
	struct lagg_softc *sc = lp->lp_softc;

	if (lp == (struct lagg_port *)sc->sc_psc)
		sc->sc_psc = NULL;
}

static int
lagg_rr_start(struct lagg_softc *sc, struct mbuf *m)
{
	struct lagg_port *lp = (struct lagg_port *)sc->sc_psc, *lp_next;
	int error = 0;

	if (lp == NULL && (lp = lagg_link_active(sc, NULL)) == NULL)
		return (ENOENT);

	/* Send mbuf */
	error = lagg_enqueue(lp->lp_ifp, m);

	/* Get next active port */
	lp_next = lagg_link_active(sc, SLIST_NEXT(lp, lp_entries));
	sc->sc_psc = (caddr_t)lp_next;

	return (error);
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
	if ((lp = lagg_link_active(sc, sc->sc_primary)) == NULL)
		return (ENOENT);

	/* Send mbuf */
	return (lagg_enqueue(lp->lp_ifp, m));
}

static struct mbuf *
lagg_fail_input(struct lagg_softc *sc, struct lagg_port *lp, struct mbuf *m)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct lagg_port *tmp_tp;

	if (lp == sc->sc_primary) {
		m->m_pkthdr.rcvif = ifp;
		return (m);
	}

	if (sc->sc_primary->lp_link_state == LINK_STATE_DOWN) {
		tmp_tp = lagg_link_active(sc, NULL);
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
	int idx;

	p = lagg_hashmbuf(m, lb->lb_key);
	if ((idx = p % sc->sc_count) >= LAGG_MAX_PORTS)
		return (EINVAL);
	lp = lb->lb_ports[idx];

	/*
	 * Check the port's link state. This will return the next active
	 * port if the link is down or the port is NULL.
	 */
	if ((lp = lagg_link_active(sc, lp)) == NULL)
		return (ENOENT);

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
	if (lp == NULL)
		return (EBUSY);

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
	if (etype == ETHERTYPE_SLOW) {
		lacp_input(lp, m);
		return (NULL);
	}

	/*
	 * If the port is not collecting or not in the active aggregator then
	 * free and return.
	 */
	if ((lp->lp_flags & LAGG_PORT_COLLECTING) == 0 ||
	    lacp_port_isactive(lp) == 0) {
		m_freem(m);
		return (NULL);
	}

	m->m_pkthdr.rcvif = ifp;
	return (m);
}
