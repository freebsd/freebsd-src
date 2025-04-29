/*	$OpenBSD: if_trunk.c,v 1.30 2007/01/31 06:20:19 reyk Exp $	*/

/*
 * Copyright (c) 2005, 2006 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2007 Andrew Thompson <thompsa@FreeBSD.org>
 * Copyright (c) 2014, 2016 Marcelo Araujo <araujo@FreeBSD.org>
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

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_kern_tls.h"
#include "opt_ratelimit.h"

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
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/sx.h>
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
#include <net/if_private.h>
#include <net/bpf.h>
#include <net/route.h>
#include <net/vnet.h>
#include <net/infiniband.h>

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

#ifdef DEV_NETMAP
MODULE_DEPEND(if_lagg, netmap, 1, 1, 1);
#endif

#define	LAGG_SX_INIT(_sc)	sx_init(&(_sc)->sc_sx, "if_lagg sx")
#define	LAGG_SX_DESTROY(_sc)	sx_destroy(&(_sc)->sc_sx)
#define	LAGG_XLOCK(_sc)		sx_xlock(&(_sc)->sc_sx)
#define	LAGG_XUNLOCK(_sc)	sx_xunlock(&(_sc)->sc_sx)
#define	LAGG_XLOCK_ASSERT(_sc)	sx_assert(&(_sc)->sc_sx, SA_XLOCKED)
#define	LAGG_SLOCK(_sc)		sx_slock(&(_sc)->sc_sx)
#define	LAGG_SUNLOCK(_sc)	sx_sunlock(&(_sc)->sc_sx)
#define	LAGG_SXLOCK_ASSERT(_sc)	sx_assert(&(_sc)->sc_sx, SA_LOCKED)

/* Special flags we should propagate to the lagg ports. */
static struct {
	int flag;
	int (*func)(struct ifnet *, int);
} lagg_pflags[] = {
	{IFF_PROMISC, ifpromisc},
	{IFF_ALLMULTI, if_allmulti},
	{0, NULL}
};

struct lagg_snd_tag {
	struct m_snd_tag com;
	struct m_snd_tag *tag;
};

VNET_DEFINE_STATIC(SLIST_HEAD(__trhead, lagg_softc), lagg_list) =
    SLIST_HEAD_INITIALIZER(); /* list of laggs */
#define	V_lagg_list	VNET(lagg_list)
VNET_DEFINE_STATIC(struct mtx, lagg_list_mtx);
#define	V_lagg_list_mtx	VNET(lagg_list_mtx)
#define	LAGG_LIST_LOCK_INIT(x)		mtx_init(&V_lagg_list_mtx, \
					"if_lagg list", NULL, MTX_DEF)
#define	LAGG_LIST_LOCK_DESTROY(x)	mtx_destroy(&V_lagg_list_mtx)
#define	LAGG_LIST_LOCK(x)		mtx_lock(&V_lagg_list_mtx)
#define	LAGG_LIST_UNLOCK(x)		mtx_unlock(&V_lagg_list_mtx)
static eventhandler_tag	lagg_detach_cookie = NULL;

static int	lagg_clone_create(struct if_clone *, char *, size_t,
		    struct ifc_data *, struct ifnet **);
static int	lagg_clone_destroy(struct if_clone *, struct ifnet *, uint32_t);
VNET_DEFINE_STATIC(struct if_clone *, lagg_cloner);
#define	V_lagg_cloner	VNET(lagg_cloner)
static const char laggname[] = "lagg";
static MALLOC_DEFINE(M_LAGG, laggname, "802.3AD Link Aggregation Interface");

static void	lagg_capabilities(struct lagg_softc *);
static int	lagg_port_create(struct lagg_softc *, struct ifnet *);
static int	lagg_port_destroy(struct lagg_port *, int);
static struct mbuf *lagg_input_ethernet(struct ifnet *, struct mbuf *);
static struct mbuf *lagg_input_infiniband(struct ifnet *, struct mbuf *);
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
static void	lagg_if_updown(struct lagg_softc *, bool);
static void	lagg_init(void *);
static void	lagg_stop(struct lagg_softc *);
static int	lagg_ioctl(struct ifnet *, u_long, caddr_t);
#if defined(KERN_TLS) || defined(RATELIMIT)
static int	lagg_snd_tag_alloc(struct ifnet *,
		    union if_snd_tag_alloc_params *,
		    struct m_snd_tag **);
static int	lagg_snd_tag_modify(struct m_snd_tag *,
		    union if_snd_tag_modify_params *);
static int	lagg_snd_tag_query(struct m_snd_tag *,
		    union if_snd_tag_query_params *);
static void	lagg_snd_tag_free(struct m_snd_tag *);
static struct m_snd_tag *lagg_next_snd_tag(struct m_snd_tag *);
static void	lagg_ratelimit_query(struct ifnet *,
		    struct if_ratelimit_query_results *);
#endif
static int	lagg_setmulti(struct lagg_port *);
static int	lagg_clrmulti(struct lagg_port *);
static void	lagg_setcaps(struct lagg_port *, int cap, int cap2);
static int	lagg_setflag(struct lagg_port *, int, int,
		    int (*func)(struct ifnet *, int));
static int	lagg_setflags(struct lagg_port *, int status);
static uint64_t lagg_get_counter(struct ifnet *ifp, ift_counter cnt);
static int	lagg_transmit_ethernet(struct ifnet *, struct mbuf *);
static int	lagg_transmit_infiniband(struct ifnet *, struct mbuf *);
static void	lagg_qflush(struct ifnet *);
static int	lagg_media_change(struct ifnet *);
static void	lagg_media_status(struct ifnet *, struct ifmediareq *);
static struct lagg_port *lagg_link_active(struct lagg_softc *,
		    struct lagg_port *);

/* Simple round robin */
static void	lagg_rr_attach(struct lagg_softc *);
static int	lagg_rr_start(struct lagg_softc *, struct mbuf *);

/* Active failover */
static int	lagg_fail_start(struct lagg_softc *, struct mbuf *);
static struct mbuf *lagg_fail_input(struct lagg_softc *, struct lagg_port *,
		    struct mbuf *);

/* Loadbalancing */
static void	lagg_lb_attach(struct lagg_softc *);
static void	lagg_lb_detach(struct lagg_softc *);
static int	lagg_lb_port_create(struct lagg_port *);
static void	lagg_lb_port_destroy(struct lagg_port *);
static int	lagg_lb_start(struct lagg_softc *, struct mbuf *);
static int	lagg_lb_porttable(struct lagg_softc *, struct lagg_port *);

/* Broadcast */
static int	lagg_bcast_start(struct lagg_softc *, struct mbuf *);

/* 802.3ad LACP */
static void	lagg_lacp_attach(struct lagg_softc *);
static void	lagg_lacp_detach(struct lagg_softc *);
static int	lagg_lacp_start(struct lagg_softc *, struct mbuf *);
static struct mbuf *lagg_lacp_input(struct lagg_softc *, struct lagg_port *,
		    struct mbuf *);
static void	lagg_lacp_lladdr(struct lagg_softc *);

/* Default input */
static struct mbuf *lagg_default_input(struct lagg_softc *, struct lagg_port *,
		    struct mbuf *);

/* lagg protocol table */
static const struct lagg_proto {
	lagg_proto	pr_num;
	void		(*pr_attach)(struct lagg_softc *);
	void		(*pr_detach)(struct lagg_softc *);
	int		(*pr_start)(struct lagg_softc *, struct mbuf *);
	struct mbuf *	(*pr_input)(struct lagg_softc *, struct lagg_port *,
			    struct mbuf *);
	int		(*pr_addport)(struct lagg_port *);
	void		(*pr_delport)(struct lagg_port *);
	void		(*pr_linkstate)(struct lagg_port *);
	void 		(*pr_init)(struct lagg_softc *);
	void 		(*pr_stop)(struct lagg_softc *);
	void 		(*pr_lladdr)(struct lagg_softc *);
	void		(*pr_request)(struct lagg_softc *, void *);
	void		(*pr_portreq)(struct lagg_port *, void *);
} lagg_protos[] = {
    {
	.pr_num = LAGG_PROTO_NONE
    },
    {
	.pr_num = LAGG_PROTO_ROUNDROBIN,
	.pr_attach = lagg_rr_attach,
	.pr_start = lagg_rr_start,
	.pr_input = lagg_default_input,
    },
    {
	.pr_num = LAGG_PROTO_FAILOVER,
	.pr_start = lagg_fail_start,
	.pr_input = lagg_fail_input,
    },
    {
	.pr_num = LAGG_PROTO_LOADBALANCE,
	.pr_attach = lagg_lb_attach,
	.pr_detach = lagg_lb_detach,
	.pr_start = lagg_lb_start,
	.pr_input = lagg_default_input,
	.pr_addport = lagg_lb_port_create,
	.pr_delport = lagg_lb_port_destroy,
    },
    {
	.pr_num = LAGG_PROTO_LACP,
	.pr_attach = lagg_lacp_attach,
	.pr_detach = lagg_lacp_detach,
	.pr_start = lagg_lacp_start,
	.pr_input = lagg_lacp_input,
	.pr_addport = lacp_port_create,
	.pr_delport = lacp_port_destroy,
	.pr_linkstate = lacp_linkstate,
	.pr_init = lacp_init,
	.pr_stop = lacp_stop,
	.pr_lladdr = lagg_lacp_lladdr,
	.pr_request = lacp_req,
	.pr_portreq = lacp_portreq,
    },
    {
	.pr_num = LAGG_PROTO_BROADCAST,
	.pr_start = lagg_bcast_start,
	.pr_input = lagg_default_input,
    },
};

SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, OID_AUTO, lagg, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Link Aggregation");

/* Allow input on any failover links */
VNET_DEFINE_STATIC(int, lagg_failover_rx_all);
#define	V_lagg_failover_rx_all	VNET(lagg_failover_rx_all)
SYSCTL_INT(_net_link_lagg, OID_AUTO, failover_rx_all, CTLFLAG_RW | CTLFLAG_VNET,
    &VNET_NAME(lagg_failover_rx_all), 0,
    "Accept input from any interface in a failover lagg");

/* Default value for using flowid */
VNET_DEFINE_STATIC(int, def_use_flowid) = 0;
#define	V_def_use_flowid	VNET(def_use_flowid)
SYSCTL_INT(_net_link_lagg, OID_AUTO, default_use_flowid,
    CTLFLAG_RWTUN | CTLFLAG_VNET, &VNET_NAME(def_use_flowid), 0,
    "Default setting for using flow id for load sharing");

/* Default value for using numa */
VNET_DEFINE_STATIC(int, def_use_numa) = 1;
#define	V_def_use_numa	VNET(def_use_numa)
SYSCTL_INT(_net_link_lagg, OID_AUTO, default_use_numa,
    CTLFLAG_RWTUN | CTLFLAG_VNET, &VNET_NAME(def_use_numa), 0,
    "Use numa to steer flows");

/* Default value for flowid shift */
VNET_DEFINE_STATIC(int, def_flowid_shift) = 16;
#define	V_def_flowid_shift	VNET(def_flowid_shift)
SYSCTL_INT(_net_link_lagg, OID_AUTO, default_flowid_shift,
    CTLFLAG_RWTUN | CTLFLAG_VNET, &VNET_NAME(def_flowid_shift), 0,
    "Default setting for flowid shift for load sharing");

static void
vnet_lagg_init(const void *unused __unused)
{

	LAGG_LIST_LOCK_INIT();
	struct if_clone_addreq req = {
		.create_f = lagg_clone_create,
		.destroy_f = lagg_clone_destroy,
		.flags = IFC_F_AUTOUNIT,
	};
	V_lagg_cloner = ifc_attach_cloner(laggname, &req);
}
VNET_SYSINIT(vnet_lagg_init, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_lagg_init, NULL);

static void
vnet_lagg_uninit(const void *unused __unused)
{

	ifc_detach_cloner(V_lagg_cloner);
	LAGG_LIST_LOCK_DESTROY();
}
VNET_SYSUNINIT(vnet_lagg_uninit, SI_SUB_INIT_IF, SI_ORDER_ANY,
    vnet_lagg_uninit, NULL);

static int
lagg_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		lagg_input_ethernet_p = lagg_input_ethernet;
		lagg_input_infiniband_p = lagg_input_infiniband;
		lagg_linkstate_p = lagg_port_state;
		lagg_detach_cookie = EVENTHANDLER_REGISTER(
		    ifnet_departure_event, lagg_port_ifdetach, NULL,
		    EVENTHANDLER_PRI_ANY);
		break;
	case MOD_UNLOAD:
		EVENTHANDLER_DEREGISTER(ifnet_departure_event,
		    lagg_detach_cookie);
		lagg_input_ethernet_p = NULL;
		lagg_input_infiniband_p = NULL;
		lagg_linkstate_p = NULL;
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
MODULE_DEPEND(if_lagg, if_infiniband, 1, 1, 1);

static void
lagg_proto_attach(struct lagg_softc *sc, lagg_proto pr)
{

	LAGG_XLOCK_ASSERT(sc);
	KASSERT(sc->sc_proto == LAGG_PROTO_NONE, ("%s: sc %p has proto",
	    __func__, sc));

	if (sc->sc_ifflags & IFF_DEBUG)
		if_printf(sc->sc_ifp, "using proto %u\n", pr);

	if (lagg_protos[pr].pr_attach != NULL)
		lagg_protos[pr].pr_attach(sc);
	sc->sc_proto = pr;
}

static void
lagg_proto_detach(struct lagg_softc *sc)
{
	lagg_proto pr;

	LAGG_XLOCK_ASSERT(sc);
	pr = sc->sc_proto;
	sc->sc_proto = LAGG_PROTO_NONE;

	if (lagg_protos[pr].pr_detach != NULL)
		lagg_protos[pr].pr_detach(sc);
}

static inline int
lagg_proto_start(struct lagg_softc *sc, struct mbuf *m)
{

	return (lagg_protos[sc->sc_proto].pr_start(sc, m));
}

static inline struct mbuf *
lagg_proto_input(struct lagg_softc *sc, struct lagg_port *lp, struct mbuf *m)
{

	return (lagg_protos[sc->sc_proto].pr_input(sc, lp, m));
}

static int
lagg_proto_addport(struct lagg_softc *sc, struct lagg_port *lp)
{

	if (lagg_protos[sc->sc_proto].pr_addport == NULL)
		return (0);
	else
		return (lagg_protos[sc->sc_proto].pr_addport(lp));
}

static void
lagg_proto_delport(struct lagg_softc *sc, struct lagg_port *lp)
{

	if (lagg_protos[sc->sc_proto].pr_delport != NULL)
		lagg_protos[sc->sc_proto].pr_delport(lp);
}

static void
lagg_proto_linkstate(struct lagg_softc *sc, struct lagg_port *lp)
{

	if (lagg_protos[sc->sc_proto].pr_linkstate != NULL)
		lagg_protos[sc->sc_proto].pr_linkstate(lp);
}

static void
lagg_proto_init(struct lagg_softc *sc)
{

	if (lagg_protos[sc->sc_proto].pr_init != NULL)
		lagg_protos[sc->sc_proto].pr_init(sc);
}

static void
lagg_proto_stop(struct lagg_softc *sc)
{

	if (lagg_protos[sc->sc_proto].pr_stop != NULL)
		lagg_protos[sc->sc_proto].pr_stop(sc);
}

static void
lagg_proto_lladdr(struct lagg_softc *sc)
{

	if (lagg_protos[sc->sc_proto].pr_lladdr != NULL)
		lagg_protos[sc->sc_proto].pr_lladdr(sc);
}

static void
lagg_proto_request(struct lagg_softc *sc, void *v)
{

	if (lagg_protos[sc->sc_proto].pr_request != NULL)
		lagg_protos[sc->sc_proto].pr_request(sc, v);
}

static void
lagg_proto_portreq(struct lagg_softc *sc, struct lagg_port *lp, void *v)
{

	if (lagg_protos[sc->sc_proto].pr_portreq != NULL)
		lagg_protos[sc->sc_proto].pr_portreq(lp, v);
}

/*
 * This routine is run via an vlan
 * config EVENT
 */
static void
lagg_register_vlan(void *arg, struct ifnet *ifp, u_int16_t vtag)
{
	struct lagg_softc *sc = ifp->if_softc;
	struct lagg_port *lp;

	if (ifp->if_softc != arg) /* Not our event */
		return;

	LAGG_XLOCK(sc);
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		EVENTHANDLER_INVOKE(vlan_config, lp->lp_ifp, vtag);
	LAGG_XUNLOCK(sc);
}

/*
 * This routine is run via an vlan
 * unconfig EVENT
 */
static void
lagg_unregister_vlan(void *arg, struct ifnet *ifp, u_int16_t vtag)
{
	struct lagg_softc *sc = ifp->if_softc;
	struct lagg_port *lp;

	if (ifp->if_softc != arg) /* Not our event */
		return;

	LAGG_XLOCK(sc);
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		EVENTHANDLER_INVOKE(vlan_unconfig, lp->lp_ifp, vtag);
	LAGG_XUNLOCK(sc);
}

static int
lagg_clone_create(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data *ifd, struct ifnet **ifpp)
{
	struct iflaggparam iflp;
	struct lagg_softc *sc;
	struct ifnet *ifp;
	int if_type;
	int error;
	static const uint8_t eaddr[LAGG_ADDR_LEN];

	if (ifd->params != NULL) {
		error = ifc_copyin(ifd, &iflp, sizeof(iflp));
		if (error)
			return (error);

		switch (iflp.lagg_type) {
		case LAGG_TYPE_ETHERNET:
			if_type = IFT_ETHER;
			break;
		case LAGG_TYPE_INFINIBAND:
			if_type = IFT_INFINIBAND;
			break;
		default:
			return (EINVAL);
		}
	} else {
		if_type = IFT_ETHER;
	}

	sc = malloc(sizeof(*sc), M_LAGG, M_WAITOK | M_ZERO);
	ifp = sc->sc_ifp = if_alloc(if_type);
	LAGG_SX_INIT(sc);

	mtx_init(&sc->sc_mtx, "lagg-mtx", NULL, MTX_DEF);
	callout_init_mtx(&sc->sc_watchdog, &sc->sc_mtx, 0);

	LAGG_XLOCK(sc);
	if (V_def_use_flowid)
		sc->sc_opts |= LAGG_OPT_USE_FLOWID;
	if (V_def_use_numa)
		sc->sc_opts |= LAGG_OPT_USE_NUMA;
	sc->flowid_shift = V_def_flowid_shift;

	/* Hash all layers by default */
	sc->sc_flags = MBUF_HASHFLAG_L2 | MBUF_HASHFLAG_L3 | MBUF_HASHFLAG_L4;

	lagg_proto_attach(sc, LAGG_PROTO_DEFAULT);

	CK_SLIST_INIT(&sc->sc_ports);

	switch (if_type) {
	case IFT_ETHER:
		/* Initialise pseudo media types */
		ifmedia_init(&sc->sc_media, 0, lagg_media_change,
		    lagg_media_status);
		ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
		ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

		if_initname(ifp, laggname, ifd->unit);
		ifp->if_transmit = lagg_transmit_ethernet;
		break;
	case IFT_INFINIBAND:
		if_initname(ifp, laggname, ifd->unit);
		ifp->if_transmit = lagg_transmit_infiniband;
		break;
	default:
		break;
	}
	ifp->if_softc = sc;
	ifp->if_qflush = lagg_qflush;
	ifp->if_init = lagg_init;
	ifp->if_ioctl = lagg_ioctl;
	ifp->if_get_counter = lagg_get_counter;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST;
#if defined(KERN_TLS) || defined(RATELIMIT)
	ifp->if_snd_tag_alloc = lagg_snd_tag_alloc;
	ifp->if_ratelimit_query = lagg_ratelimit_query;
#endif
	ifp->if_capenable = ifp->if_capabilities = IFCAP_HWSTATS;

	/*
	 * Attach as an ordinary ethernet device, children will be attached
	 * as special device IFT_IEEE8023ADLAG or IFT_INFINIBANDLAG.
	 */
	switch (if_type) {
	case IFT_ETHER:
		ether_ifattach(ifp, eaddr);
		break;
	case IFT_INFINIBAND:
		infiniband_ifattach(ifp, eaddr, sc->sc_bcast_addr);
		break;
	default:
		break;
	}

	sc->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
		lagg_register_vlan, sc, EVENTHANDLER_PRI_FIRST);
	sc->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
		lagg_unregister_vlan, sc, EVENTHANDLER_PRI_FIRST);

	/* Insert into the global list of laggs */
	LAGG_LIST_LOCK();
	SLIST_INSERT_HEAD(&V_lagg_list, sc, sc_entries);
	LAGG_LIST_UNLOCK();
	LAGG_XUNLOCK(sc);
	*ifpp = ifp;

	return (0);
}

static int
lagg_clone_destroy(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags)
{
	struct lagg_softc *sc = (struct lagg_softc *)ifp->if_softc;
	struct lagg_port *lp;

	LAGG_XLOCK(sc);
	sc->sc_destroying = 1;
	lagg_stop(sc);
	ifp->if_flags &= ~IFF_UP;

	EVENTHANDLER_DEREGISTER(vlan_config, sc->vlan_attach);
	EVENTHANDLER_DEREGISTER(vlan_unconfig, sc->vlan_detach);

	/* Shutdown and remove lagg ports */
	while ((lp = CK_SLIST_FIRST(&sc->sc_ports)) != NULL)
		lagg_port_destroy(lp, 1);

	/* Unhook the aggregation protocol */
	lagg_proto_detach(sc);
	LAGG_XUNLOCK(sc);

	switch (ifp->if_type) {
	case IFT_ETHER:
		ether_ifdetach(ifp);
		ifmedia_removeall(&sc->sc_media);
		break;
	case IFT_INFINIBAND:
		infiniband_ifdetach(ifp);
		break;
	default:
		break;
	}
	if_free(ifp);

	LAGG_LIST_LOCK();
	SLIST_REMOVE(&V_lagg_list, sc, lagg_softc, sc_entries);
	LAGG_LIST_UNLOCK();

	mtx_destroy(&sc->sc_mtx);
	LAGG_SX_DESTROY(sc);
	free(sc, M_LAGG);

	return (0);
}

static void
lagg_capabilities(struct lagg_softc *sc)
{
	struct lagg_port *lp;
	int cap, cap2, ena, ena2, pena, pena2;
	uint64_t hwa;
	struct ifnet_hw_tsomax hw_tsomax;

	LAGG_XLOCK_ASSERT(sc);

	/* Get common enabled capabilities for the lagg ports */
	ena = ena2 = ~0;
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		ena &= lp->lp_ifp->if_capenable;
		ena2 &= lp->lp_ifp->if_capenable2;
	}
	if (CK_SLIST_FIRST(&sc->sc_ports) == NULL)
		ena = ena2 = 0;

	/*
	 * Apply common enabled capabilities back to the lagg ports.
	 * May require several iterations if they are dependent.
	 */
	do {
		pena = ena;
		pena2 = ena2;
		CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
			lagg_setcaps(lp, ena, ena2);
			ena &= lp->lp_ifp->if_capenable;
			ena2 &= lp->lp_ifp->if_capenable2;
		}
	} while (pena != ena || pena2 != ena2);
	ena2 &= ~IFCAP2_BIT(IFCAP2_IPSEC_OFFLOAD);

	/* Get other capabilities from the lagg ports */
	cap = cap2 = ~0;
	hwa = ~(uint64_t)0;
	memset(&hw_tsomax, 0, sizeof(hw_tsomax));
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		cap &= lp->lp_ifp->if_capabilities;
		cap2 &= lp->lp_ifp->if_capabilities2;
		hwa &= lp->lp_ifp->if_hwassist;
		if_hw_tsomax_common(lp->lp_ifp, &hw_tsomax);
	}
	cap2 &= ~IFCAP2_BIT(IFCAP2_IPSEC_OFFLOAD);
	if (CK_SLIST_FIRST(&sc->sc_ports) == NULL)
		cap = cap2 = hwa = 0;

	if (sc->sc_ifp->if_capabilities != cap ||
	    sc->sc_ifp->if_capenable != ena ||
	    sc->sc_ifp->if_capenable2 != ena2 ||
	    sc->sc_ifp->if_hwassist != hwa ||
	    if_hw_tsomax_update(sc->sc_ifp, &hw_tsomax) != 0) {
		sc->sc_ifp->if_capabilities = cap;
		sc->sc_ifp->if_capabilities2 = cap2;
		sc->sc_ifp->if_capenable = ena;
		sc->sc_ifp->if_capenable2 = ena2;
		sc->sc_ifp->if_hwassist = hwa;
		getmicrotime(&sc->sc_ifp->if_lastchange);

		if (sc->sc_ifflags & IFF_DEBUG)
			if_printf(sc->sc_ifp,
			    "capabilities 0x%08x enabled 0x%08x\n", cap, ena);
	}
}

static int
lagg_port_create(struct lagg_softc *sc, struct ifnet *ifp)
{
	struct lagg_softc *sc_ptr;
	struct lagg_port *lp, *tlp;
	struct ifreq ifr;
	int error, i, oldmtu;
	int if_type;
	uint64_t *pval;

	LAGG_XLOCK_ASSERT(sc);

	if (sc->sc_ifp == ifp) {
		if_printf(sc->sc_ifp,
		    "cannot add a lagg to itself as a port\n");
		return (EINVAL);
	}

	if (sc->sc_destroying == 1)
		return (ENXIO);

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

	switch (sc->sc_ifp->if_type) {
	case IFT_ETHER:
		/* XXX Disallow non-ethernet interfaces (this should be any of 802) */
		if (ifp->if_type != IFT_ETHER && ifp->if_type != IFT_L2VLAN)
			return (EPROTONOSUPPORT);
		if_type = IFT_IEEE8023ADLAG;
		break;
	case IFT_INFINIBAND:
		/* XXX Disallow non-infiniband interfaces */
		if (ifp->if_type != IFT_INFINIBAND)
			return (EPROTONOSUPPORT);
		if_type = IFT_INFINIBANDLAG;
		break;
	default:
		break;
	}

	/* Allow the first Ethernet member to define the MTU */
	oldmtu = -1;
	if (CK_SLIST_EMPTY(&sc->sc_ports)) {
		sc->sc_ifp->if_mtu = ifp->if_mtu;
	} else if (sc->sc_ifp->if_mtu != ifp->if_mtu) {
		if (ifp->if_ioctl == NULL) {
			if_printf(sc->sc_ifp, "cannot change MTU for %s\n",
			    ifp->if_xname);
			return (EINVAL);
		}
		oldmtu = ifp->if_mtu;
		strlcpy(ifr.ifr_name, ifp->if_xname, sizeof(ifr.ifr_name));
		ifr.ifr_mtu = sc->sc_ifp->if_mtu;
		error = (*ifp->if_ioctl)(ifp, SIOCSIFMTU, (caddr_t)&ifr);
		if (error != 0) {
			if_printf(sc->sc_ifp, "invalid MTU for %s\n",
			    ifp->if_xname);
			return (error);
		}
		ifr.ifr_mtu = oldmtu;
	}

	lp = malloc(sizeof(struct lagg_port), M_LAGG, M_WAITOK | M_ZERO);
	lp->lp_softc = sc;

	/* Check if port is a stacked lagg */
	LAGG_LIST_LOCK();
	SLIST_FOREACH(sc_ptr, &V_lagg_list, sc_entries) {
		if (ifp == sc_ptr->sc_ifp) {
			LAGG_LIST_UNLOCK();
			free(lp, M_LAGG);
			if (oldmtu != -1)
				(*ifp->if_ioctl)(ifp, SIOCSIFMTU,
				    (caddr_t)&ifr);
			return (EINVAL);
			/* XXX disable stacking for the moment, its untested */
#ifdef LAGG_PORT_STACKING
			lp->lp_flags |= LAGG_PORT_STACK;
			if (lagg_port_checkstacking(sc_ptr) >=
			    LAGG_MAX_STACKING) {
				LAGG_LIST_UNLOCK();
				free(lp, M_LAGG);
				if (oldmtu != -1)
					(*ifp->if_ioctl)(ifp, SIOCSIFMTU,
					    (caddr_t)&ifr);
				return (E2BIG);
			}
#endif
		}
	}
	LAGG_LIST_UNLOCK();

	if_ref(ifp);
	lp->lp_ifp = ifp;

	bcopy(IF_LLADDR(ifp), lp->lp_lladdr, ifp->if_addrlen);
	lp->lp_ifcapenable = ifp->if_capenable;
	if (CK_SLIST_EMPTY(&sc->sc_ports)) {
		bcopy(IF_LLADDR(ifp), IF_LLADDR(sc->sc_ifp), ifp->if_addrlen);
		lagg_proto_lladdr(sc);
		EVENTHANDLER_INVOKE(iflladdr_event, sc->sc_ifp);
	} else {
		if_setlladdr(ifp, IF_LLADDR(sc->sc_ifp), ifp->if_addrlen);
	}
	lagg_setflags(lp, 1);

	if (CK_SLIST_EMPTY(&sc->sc_ports))
		sc->sc_primary = lp;

	/* Change the interface type */
	lp->lp_iftype = ifp->if_type;
	ifp->if_type = if_type;
	ifp->if_lagg = lp;
	lp->lp_ioctl = ifp->if_ioctl;
	ifp->if_ioctl = lagg_port_ioctl;
	lp->lp_output = ifp->if_output;
	ifp->if_output = lagg_port_output;

	/* Read port counters */
	pval = lp->port_counters.val;
	for (i = 0; i < IFCOUNTERS; i++, pval++)
		*pval = ifp->if_get_counter(ifp, i);

	/*
	 * Insert into the list of ports.
	 * Keep ports sorted by if_index. It is handy, when configuration
	 * is predictable and `ifconfig laggN create ...` command
	 * will lead to the same result each time.
	 */
	CK_SLIST_FOREACH(tlp, &sc->sc_ports, lp_entries) {
		if (tlp->lp_ifp->if_index < ifp->if_index && (
		    CK_SLIST_NEXT(tlp, lp_entries) == NULL ||
		    ((struct lagg_port*)CK_SLIST_NEXT(tlp, lp_entries))->lp_ifp->if_index >
		    ifp->if_index))
			break;
	}
	if (tlp != NULL)
		CK_SLIST_INSERT_AFTER(tlp, lp, lp_entries);
	else
		CK_SLIST_INSERT_HEAD(&sc->sc_ports, lp, lp_entries);
	sc->sc_count++;

	lagg_setmulti(lp);

	if ((error = lagg_proto_addport(sc, lp)) != 0) {
		/* Remove the port, without calling pr_delport. */
		lagg_port_destroy(lp, 0);
		if (oldmtu != -1)
			(*ifp->if_ioctl)(ifp, SIOCSIFMTU, (caddr_t)&ifr);
		return (error);
	}

	/* Update lagg capabilities */
	lagg_capabilities(sc);
	lagg_linkstate(sc);

	return (0);
}

#ifdef LAGG_PORT_STACKING
static int
lagg_port_checkstacking(struct lagg_softc *sc)
{
	struct lagg_softc *sc_ptr;
	struct lagg_port *lp;
	int m = 0;

	LAGG_SXLOCK_ASSERT(sc);
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		if (lp->lp_flags & LAGG_PORT_STACK) {
			sc_ptr = (struct lagg_softc *)lp->lp_ifp->if_softc;
			m = MAX(m, lagg_port_checkstacking(sc_ptr));
		}
	}

	return (m + 1);
}
#endif

static void
lagg_port_destroy_cb(epoch_context_t ec)
{
	struct lagg_port *lp;
	struct ifnet *ifp;

	lp = __containerof(ec, struct lagg_port, lp_epoch_ctx);
	ifp = lp->lp_ifp;

	if_rele(ifp);
	free(lp, M_LAGG);
}

static int
lagg_port_destroy(struct lagg_port *lp, int rundelport)
{
	struct lagg_softc *sc = lp->lp_softc;
	struct lagg_port *lp_ptr, *lp0;
	struct ifnet *ifp = lp->lp_ifp;
	uint64_t *pval, vdiff;
	int i;

	LAGG_XLOCK_ASSERT(sc);

	if (rundelport)
		lagg_proto_delport(sc, lp);

	if (lp->lp_detaching == 0)
		lagg_clrmulti(lp);

	/* Restore interface */
	ifp->if_type = lp->lp_iftype;
	ifp->if_ioctl = lp->lp_ioctl;
	ifp->if_output = lp->lp_output;
	ifp->if_lagg = NULL;

	/* Update detached port counters */
	pval = lp->port_counters.val;
	for (i = 0; i < IFCOUNTERS; i++, pval++) {
		vdiff = ifp->if_get_counter(ifp, i) - *pval;
		sc->detached_counters.val[i] += vdiff;
	}

	/* Finally, remove the port from the lagg */
	CK_SLIST_REMOVE(&sc->sc_ports, lp, lagg_port, lp_entries);
	sc->sc_count--;

	/* Update the primary interface */
	if (lp == sc->sc_primary) {
		uint8_t lladdr[LAGG_ADDR_LEN];

		if ((lp0 = CK_SLIST_FIRST(&sc->sc_ports)) == NULL)
			bzero(&lladdr, LAGG_ADDR_LEN);
		else
			bcopy(lp0->lp_lladdr, lladdr, LAGG_ADDR_LEN);
		sc->sc_primary = lp0;
		if (sc->sc_destroying == 0) {
			bcopy(lladdr, IF_LLADDR(sc->sc_ifp), sc->sc_ifp->if_addrlen);
			lagg_proto_lladdr(sc);
			EVENTHANDLER_INVOKE(iflladdr_event, sc->sc_ifp);

			/*
			 * Update lladdr for each port (new primary needs update
			 * as well, to switch from old lladdr to its 'real' one).
			 * We can skip this if the lagg is being destroyed.
			 */
			CK_SLIST_FOREACH(lp_ptr, &sc->sc_ports, lp_entries)
				if_setlladdr(lp_ptr->lp_ifp, lladdr,
				    lp_ptr->lp_ifp->if_addrlen);
		}
	}

	if (lp->lp_ifflags)
		if_printf(ifp, "%s: lp_ifflags unclean\n", __func__);

	if (lp->lp_detaching == 0) {
		lagg_setflags(lp, 0);
		lagg_setcaps(lp, lp->lp_ifcapenable, lp->lp_ifcapenable2);
		if_setlladdr(ifp, lp->lp_lladdr, ifp->if_addrlen);
	}

	/*
	 * free port and release it's ifnet reference after a grace period has
	 * elapsed.
	 */
	NET_EPOCH_CALL(lagg_port_destroy_cb, &lp->lp_epoch_ctx);
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

	/* Should be checked by the caller */
	switch (ifp->if_type) {
	case IFT_IEEE8023ADLAG:
	case IFT_INFINIBANDLAG:
		if ((lp = ifp->if_lagg) == NULL || (sc = lp->lp_softc) == NULL)
			goto fallback;
		break;
	default:
		goto fallback;
	}

	switch (cmd) {
	case SIOCGLAGGPORT:
		if (rp->rp_portname[0] == '\0' ||
		    ifunit(rp->rp_portname) != ifp) {
			error = EINVAL;
			break;
		}

		LAGG_SLOCK(sc);
		if (__predict_true((lp = ifp->if_lagg) != NULL &&
		    lp->lp_softc == sc))
			lagg_port2req(lp, rp);
		else
			error = ENOENT;	/* XXXGL: can happen? */
		LAGG_SUNLOCK(sc);
		break;

	case SIOCSIFCAP:
	case SIOCSIFCAPNV:
		if (lp->lp_ioctl == NULL) {
			error = EINVAL;
			break;
		}
		error = (*lp->lp_ioctl)(ifp, cmd, data);
		if (error)
			break;

		/* Update lagg interface capabilities */
		LAGG_XLOCK(sc);
		lagg_capabilities(sc);
		LAGG_XUNLOCK(sc);
		VLAN_CAPABILITIES(sc->sc_ifp);
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
	if (lp != NULL && lp->lp_ioctl != NULL)
		return ((*lp->lp_ioctl)(ifp, cmd, data));

	return (EINVAL);
}

/*
 * Requests counter @cnt data.
 *
 * Counter value is calculated the following way:
 * 1) for each port, sum difference between current and "initial" measurements.
 * 2) add lagg logical interface counters.
 * 3) add data from detached_counters array.
 *
 * We also do the following things on ports attach/detach:
 * 1) On port attach we store all counters it has into port_counter array.
 * 2) On port detach we add the different between "initial" and
 *   current counters data to detached_counters array.
 */
static uint64_t
lagg_get_counter(struct ifnet *ifp, ift_counter cnt)
{
	struct epoch_tracker et;
	struct lagg_softc *sc;
	struct lagg_port *lp;
	struct ifnet *lpifp;
	uint64_t newval, oldval, vsum;

	/* Revise this when we've got non-generic counters. */
	KASSERT(cnt < IFCOUNTERS, ("%s: invalid cnt %d", __func__, cnt));

	sc = (struct lagg_softc *)ifp->if_softc;

	vsum = 0;
	NET_EPOCH_ENTER(et);
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		/* Saved attached value */
		oldval = lp->port_counters.val[cnt];
		/* current value */
		lpifp = lp->lp_ifp;
		newval = lpifp->if_get_counter(lpifp, cnt);
		/* Calculate diff and save new */
		vsum += newval - oldval;
	}
	NET_EPOCH_EXIT(et);

	/*
	 * Add counter data which might be added by upper
	 * layer protocols operating on logical interface.
	 */
	vsum += if_get_counter_default(ifp, cnt);

	/*
	 * Add counter data from detached ports counters
	 */
	vsum += sc->detached_counters.val[cnt];

	return (vsum);
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
			if (lp != NULL)
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

	LAGG_XLOCK(sc);
	lp->lp_detaching = 1;
	lagg_port_destroy(lp, 1);
	LAGG_XUNLOCK(sc);
	VLAN_CAPABILITIES(sc->sc_ifp);
}

static void
lagg_port2req(struct lagg_port *lp, struct lagg_reqport *rp)
{
	struct lagg_softc *sc = lp->lp_softc;

	strlcpy(rp->rp_ifname, sc->sc_ifname, sizeof(rp->rp_ifname));
	strlcpy(rp->rp_portname, lp->lp_ifp->if_xname, sizeof(rp->rp_portname));
	rp->rp_prio = lp->lp_prio;
	rp->rp_flags = lp->lp_flags;
	lagg_proto_portreq(sc, lp, &rp->rp_psc);

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
lagg_watchdog_infiniband(void *arg)
{
	struct epoch_tracker et;
	struct lagg_softc *sc;
	struct lagg_port *lp;
	struct ifnet *ifp;
	struct ifnet *lp_ifp;

	sc = arg;

	/*
	 * Because infiniband nodes have a fixed MAC address, which is
	 * generated by the so-called GID, we need to regularly update
	 * the link level address of the parent lagg<N> device when
	 * the active port changes. Possibly we could piggy-back on
	 * link up/down events aswell, but using a timer also provides
	 * a guarantee against too frequent events. This operation
	 * does not have to be atomic.
	 */
	NET_EPOCH_ENTER(et);
	lp = lagg_link_active(sc, sc->sc_primary);
	if (lp != NULL) {
		ifp = sc->sc_ifp;
		lp_ifp = lp->lp_ifp;

		if (ifp != NULL && lp_ifp != NULL &&
		    (memcmp(IF_LLADDR(ifp), IF_LLADDR(lp_ifp), ifp->if_addrlen) != 0 ||
		     memcmp(sc->sc_bcast_addr, lp_ifp->if_broadcastaddr, ifp->if_addrlen) != 0)) {
			memcpy(IF_LLADDR(ifp), IF_LLADDR(lp_ifp), ifp->if_addrlen);
			memcpy(sc->sc_bcast_addr, lp_ifp->if_broadcastaddr, ifp->if_addrlen);

			CURVNET_SET(ifp->if_vnet);
			EVENTHANDLER_INVOKE(iflladdr_event, ifp);
			CURVNET_RESTORE();
		}
	}
	NET_EPOCH_EXIT(et);

	callout_reset(&sc->sc_watchdog, hz, &lagg_watchdog_infiniband, arg);
}

static void
lagg_if_updown(struct lagg_softc *sc, bool up)
{
	struct ifreq ifr = {};
	struct lagg_port *lp;

	LAGG_XLOCK_ASSERT(sc);

	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		if (up)
			if_up(lp->lp_ifp);
		else
			if_down(lp->lp_ifp);

		if (lp->lp_ioctl != NULL)
			lp->lp_ioctl(lp->lp_ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
	}
}

static void
lagg_init(void *xsc)
{
	struct lagg_softc *sc = (struct lagg_softc *)xsc;
	struct ifnet *ifp = sc->sc_ifp;
	struct lagg_port *lp;

	LAGG_XLOCK(sc);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		LAGG_XUNLOCK(sc);
		return;
	}

	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	/*
	 * Update the port lladdrs if needed.
	 * This might be if_setlladdr() notification
	 * that lladdr has been changed.
	 */
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		if (memcmp(IF_LLADDR(ifp), IF_LLADDR(lp->lp_ifp),
		    ifp->if_addrlen) != 0)
			if_setlladdr(lp->lp_ifp, IF_LLADDR(ifp), ifp->if_addrlen);
	}

	lagg_if_updown(sc, true);

	lagg_proto_init(sc);

	if (ifp->if_type == IFT_INFINIBAND) {
		mtx_lock(&sc->sc_mtx);
		lagg_watchdog_infiniband(sc);
		mtx_unlock(&sc->sc_mtx);
	}

	LAGG_XUNLOCK(sc);
}

static void
lagg_stop(struct lagg_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	LAGG_XLOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	lagg_proto_stop(sc);

	mtx_lock(&sc->sc_mtx);
	callout_stop(&sc->sc_watchdog);
	mtx_unlock(&sc->sc_mtx);

	lagg_if_updown(sc, false);

	callout_drain(&sc->sc_watchdog);
}

static int
lagg_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct lagg_softc *sc = (struct lagg_softc *)ifp->if_softc;
	struct lagg_reqall *ra = (struct lagg_reqall *)data;
	struct lagg_reqopts *ro = (struct lagg_reqopts *)data;
	struct lagg_reqport *rp = (struct lagg_reqport *)data, rpbuf;
	struct lagg_reqflags *rf = (struct lagg_reqflags *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	struct lagg_port *lp;
	struct ifnet *tpif;
	struct thread *td = curthread;
	char *buf, *outbuf;
	int count, buflen, len, error = 0, oldmtu;

	bzero(&rpbuf, sizeof(rpbuf));

	/* XXX: This can race with lagg_clone_destroy. */

	switch (cmd) {
	case SIOCGLAGG:
		LAGG_XLOCK(sc);
		buflen = sc->sc_count * sizeof(struct lagg_reqport);
		outbuf = malloc(buflen, M_TEMP, M_WAITOK | M_ZERO);
		ra->ra_proto = sc->sc_proto;
		lagg_proto_request(sc, &ra->ra_psc);
		count = 0;
		buf = outbuf;
		len = min(ra->ra_size, buflen);
		CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
			if (len < sizeof(rpbuf))
				break;

			lagg_port2req(lp, &rpbuf);
			memcpy(buf, &rpbuf, sizeof(rpbuf));
			count++;
			buf += sizeof(rpbuf);
			len -= sizeof(rpbuf);
		}
		LAGG_XUNLOCK(sc);
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
		/* Infiniband only supports the failover protocol. */
		if (ra->ra_proto != LAGG_PROTO_FAILOVER &&
		    ifp->if_type == IFT_INFINIBAND) {
			error = EPROTONOSUPPORT;
			break;
		}
		LAGG_XLOCK(sc);
		lagg_proto_detach(sc);
		lagg_proto_attach(sc, ra->ra_proto);
		LAGG_XUNLOCK(sc);
		break;
	case SIOCGLAGGOPTS:
		LAGG_XLOCK(sc);
		ro->ro_opts = sc->sc_opts;
		if (sc->sc_proto == LAGG_PROTO_LACP) {
			struct lacp_softc *lsc;

			lsc = (struct lacp_softc *)sc->sc_psc;
			if (lsc->lsc_debug.lsc_tx_test != 0)
				ro->ro_opts |= LAGG_OPT_LACP_TXTEST;
			if (lsc->lsc_debug.lsc_rx_test != 0)
				ro->ro_opts |= LAGG_OPT_LACP_RXTEST;
			if (lsc->lsc_strict_mode != 0)
				ro->ro_opts |= LAGG_OPT_LACP_STRICT;
			if (lsc->lsc_fast_timeout != 0)
				ro->ro_opts |= LAGG_OPT_LACP_FAST_TIMO;

			ro->ro_active = sc->sc_active;
		} else {
			ro->ro_active = 0;
			CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
				ro->ro_active += LAGG_PORTACTIVE(lp);
		}
		ro->ro_bkt = sc->sc_stride;
		ro->ro_flapping = sc->sc_flapping;
		ro->ro_flowid_shift = sc->flowid_shift;
		LAGG_XUNLOCK(sc);
		break;
	case SIOCSLAGGOPTS:
		error = priv_check(td, PRIV_NET_LAGG);
		if (error)
			break;

		/*
		 * The stride option was added without defining a corresponding
		 * LAGG_OPT flag, so handle a non-zero value before checking
		 * anything else to preserve compatibility.
		 */
		LAGG_XLOCK(sc);
		if (ro->ro_opts == 0 && ro->ro_bkt != 0) {
			if (sc->sc_proto != LAGG_PROTO_ROUNDROBIN) {
				LAGG_XUNLOCK(sc);
				error = EINVAL;
				break;
			}
			sc->sc_stride = ro->ro_bkt;
		}
		if (ro->ro_opts == 0) {
			LAGG_XUNLOCK(sc);
			break;
		}

		/*
		 * Set options.  LACP options are stored in sc->sc_psc,
		 * not in sc_opts.
		 */
		int valid, lacp;

		switch (ro->ro_opts) {
		case LAGG_OPT_USE_FLOWID:
		case -LAGG_OPT_USE_FLOWID:
		case LAGG_OPT_USE_NUMA:
		case -LAGG_OPT_USE_NUMA:
		case LAGG_OPT_FLOWIDSHIFT:
		case LAGG_OPT_RR_LIMIT:
			valid = 1;
			lacp = 0;
			break;
		case LAGG_OPT_LACP_TXTEST:
		case -LAGG_OPT_LACP_TXTEST:
		case LAGG_OPT_LACP_RXTEST:
		case -LAGG_OPT_LACP_RXTEST:
		case LAGG_OPT_LACP_STRICT:
		case -LAGG_OPT_LACP_STRICT:
		case LAGG_OPT_LACP_FAST_TIMO:
		case -LAGG_OPT_LACP_FAST_TIMO:
			valid = lacp = 1;
			break;
		default:
			valid = lacp = 0;
			break;
		}

		if (valid == 0 ||
		    (lacp == 1 && sc->sc_proto != LAGG_PROTO_LACP)) {
			/* Invalid combination of options specified. */
			error = EINVAL;
			LAGG_XUNLOCK(sc);
			break;	/* Return from SIOCSLAGGOPTS. */
		}

		/*
		 * Store new options into sc->sc_opts except for
		 * FLOWIDSHIFT, RR and LACP options.
		 */
		if (lacp == 0) {
			if (ro->ro_opts == LAGG_OPT_FLOWIDSHIFT)
				sc->flowid_shift = ro->ro_flowid_shift;
			else if (ro->ro_opts == LAGG_OPT_RR_LIMIT) {
				if (sc->sc_proto != LAGG_PROTO_ROUNDROBIN ||
				    ro->ro_bkt == 0) {
					error = EINVAL;
					LAGG_XUNLOCK(sc);
					break;
				}
				sc->sc_stride = ro->ro_bkt;
			} else if (ro->ro_opts > 0)
				sc->sc_opts |= ro->ro_opts;
			else
				sc->sc_opts &= ~ro->ro_opts;
		} else {
			struct lacp_softc *lsc;
			struct lacp_port *lp;

			lsc = (struct lacp_softc *)sc->sc_psc;

			switch (ro->ro_opts) {
			case LAGG_OPT_LACP_TXTEST:
				lsc->lsc_debug.lsc_tx_test = 1;
				break;
			case -LAGG_OPT_LACP_TXTEST:
				lsc->lsc_debug.lsc_tx_test = 0;
				break;
			case LAGG_OPT_LACP_RXTEST:
				lsc->lsc_debug.lsc_rx_test = 1;
				break;
			case -LAGG_OPT_LACP_RXTEST:
				lsc->lsc_debug.lsc_rx_test = 0;
				break;
			case LAGG_OPT_LACP_STRICT:
				lsc->lsc_strict_mode = 1;
				break;
			case -LAGG_OPT_LACP_STRICT:
				lsc->lsc_strict_mode = 0;
				break;
			case LAGG_OPT_LACP_FAST_TIMO:
				LACP_LOCK(lsc);
				LIST_FOREACH(lp, &lsc->lsc_ports, lp_next)
					lp->lp_state |= LACP_STATE_TIMEOUT;
				LACP_UNLOCK(lsc);
				lsc->lsc_fast_timeout = 1;
				break;
			case -LAGG_OPT_LACP_FAST_TIMO:
				LACP_LOCK(lsc);
				LIST_FOREACH(lp, &lsc->lsc_ports, lp_next)
					lp->lp_state &= ~LACP_STATE_TIMEOUT;
				LACP_UNLOCK(lsc);
				lsc->lsc_fast_timeout = 0;
				break;
			}
		}
		LAGG_XUNLOCK(sc);
		break;
	case SIOCGLAGGFLAGS:
		rf->rf_flags = 0;
		LAGG_XLOCK(sc);
		if (sc->sc_flags & MBUF_HASHFLAG_L2)
			rf->rf_flags |= LAGG_F_HASHL2;
		if (sc->sc_flags & MBUF_HASHFLAG_L3)
			rf->rf_flags |= LAGG_F_HASHL3;
		if (sc->sc_flags & MBUF_HASHFLAG_L4)
			rf->rf_flags |= LAGG_F_HASHL4;
		LAGG_XUNLOCK(sc);
		break;
	case SIOCSLAGGHASH:
		error = priv_check(td, PRIV_NET_LAGG);
		if (error)
			break;
		if ((rf->rf_flags & LAGG_F_HASHMASK) == 0) {
			error = EINVAL;
			break;
		}
		LAGG_XLOCK(sc);
		sc->sc_flags = 0;
		if (rf->rf_flags & LAGG_F_HASHL2)
			sc->sc_flags |= MBUF_HASHFLAG_L2;
		if (rf->rf_flags & LAGG_F_HASHL3)
			sc->sc_flags |= MBUF_HASHFLAG_L3;
		if (rf->rf_flags & LAGG_F_HASHL4)
			sc->sc_flags |= MBUF_HASHFLAG_L4;
		LAGG_XUNLOCK(sc);
		break;
	case SIOCGLAGGPORT:
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit_ref(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}

		LAGG_SLOCK(sc);
		if (__predict_true((lp = tpif->if_lagg) != NULL &&
		    lp->lp_softc == sc))
			lagg_port2req(lp, rp);
		else
			error = ENOENT;	/* XXXGL: can happen? */
		LAGG_SUNLOCK(sc);
		if_rele(tpif);
		break;

	case SIOCSLAGGPORT:
		error = priv_check(td, PRIV_NET_LAGG);
		if (error)
			break;
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit_ref(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}
#ifdef INET6
		/*
		 * A laggport interface should not have inet6 address
		 * because two interfaces with a valid link-local
		 * scope zone must not be merged in any form.  This
		 * restriction is needed to prevent violation of
		 * link-local scope zone.  Attempts to add a laggport
		 * interface which has inet6 addresses triggers
		 * removal of all inet6 addresses on the member
		 * interface.
		 */
		if (in6ifa_llaonifp(tpif)) {
			in6_ifdetach(tpif);
				if_printf(sc->sc_ifp,
				    "IPv6 addresses on %s have been removed "
				    "before adding it as a member to prevent "
				    "IPv6 address scope violation.\n",
				    tpif->if_xname);
		}
#endif
		oldmtu = ifp->if_mtu;
		LAGG_XLOCK(sc);
		error = lagg_port_create(sc, tpif);
		LAGG_XUNLOCK(sc);
		if_rele(tpif);

		/*
		 * LAGG MTU may change during addition of the first port.
		 * If it did, do network layer specific procedure.
		 */
		if (ifp->if_mtu != oldmtu)
			if_notifymtu(ifp);

		VLAN_CAPABILITIES(ifp);
		break;
	case SIOCSLAGGDELPORT:
		error = priv_check(td, PRIV_NET_LAGG);
		if (error)
			break;
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit_ref(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}

		LAGG_XLOCK(sc);
		if ((lp = (struct lagg_port *)tpif->if_lagg) == NULL ||
		    lp->lp_softc != sc) {
			error = ENOENT;
			LAGG_XUNLOCK(sc);
			if_rele(tpif);
			break;
		}

		error = lagg_port_destroy(lp, 1);
		LAGG_XUNLOCK(sc);
		if_rele(tpif);
		VLAN_CAPABILITIES(ifp);
		break;
	case SIOCSIFFLAGS:
		/* Set flags on ports too */
		LAGG_XLOCK(sc);
		CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
			lagg_setflags(lp, 1);
		}

		if (!(ifp->if_flags & IFF_UP) &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			/*
			 * If interface is marked down and it is running,
			 * then stop and disable it.
			 */
			lagg_stop(sc);
			LAGG_XUNLOCK(sc);
		} else if ((ifp->if_flags & IFF_UP) &&
		    !(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			LAGG_XUNLOCK(sc);
			(*ifp->if_init)(sc);
		} else
			LAGG_XUNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		LAGG_XLOCK(sc);
		CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
			lagg_clrmulti(lp);
			lagg_setmulti(lp);
		}
		LAGG_XUNLOCK(sc);
		error = 0;
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if (ifp->if_type == IFT_INFINIBAND)
			error = EINVAL;
		else
			error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	case SIOCSIFCAP:
	case SIOCSIFCAPNV:
		LAGG_XLOCK(sc);
		CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
			if (lp->lp_ioctl != NULL)
				(*lp->lp_ioctl)(lp->lp_ifp, cmd, data);
		}
		lagg_capabilities(sc);
		LAGG_XUNLOCK(sc);
		VLAN_CAPABILITIES(ifp);
		error = 0;
		break;

	case SIOCGIFCAPNV:
		error = 0;
		break;

	case SIOCSIFMTU:
		LAGG_XLOCK(sc);
		CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
			if (lp->lp_ioctl != NULL)
				error = (*lp->lp_ioctl)(lp->lp_ifp, cmd, data);
			else
				error = EINVAL;
			if (error != 0) {
				if_printf(ifp,
				    "failed to change MTU to %d on port %s, "
				    "reverting all ports to original MTU (%d)\n",
				    ifr->ifr_mtu, lp->lp_ifp->if_xname, ifp->if_mtu);
				break;
			}
		}
		if (error == 0) {
			ifp->if_mtu = ifr->ifr_mtu;
		} else {
			/* set every port back to the original MTU */
			ifr->ifr_mtu = ifp->if_mtu;
			CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
				if (lp->lp_ioctl != NULL)
					(*lp->lp_ioctl)(lp->lp_ifp, cmd, data);
			}
		}
		lagg_capabilities(sc);
		LAGG_XUNLOCK(sc);
		VLAN_CAPABILITIES(ifp);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return (error);
}

#if defined(KERN_TLS) || defined(RATELIMIT)
#ifdef RATELIMIT
static const struct if_snd_tag_sw lagg_snd_tag_ul_sw = {
	.snd_tag_modify = lagg_snd_tag_modify,
	.snd_tag_query = lagg_snd_tag_query,
	.snd_tag_free = lagg_snd_tag_free,
	.next_snd_tag = lagg_next_snd_tag,
	.type = IF_SND_TAG_TYPE_UNLIMITED
};

static const struct if_snd_tag_sw lagg_snd_tag_rl_sw = {
	.snd_tag_modify = lagg_snd_tag_modify,
	.snd_tag_query = lagg_snd_tag_query,
	.snd_tag_free = lagg_snd_tag_free,
	.next_snd_tag = lagg_next_snd_tag,
	.type = IF_SND_TAG_TYPE_RATE_LIMIT
};
#endif

#ifdef KERN_TLS
static const struct if_snd_tag_sw lagg_snd_tag_tls_sw = {
	.snd_tag_modify = lagg_snd_tag_modify,
	.snd_tag_query = lagg_snd_tag_query,
	.snd_tag_free = lagg_snd_tag_free,
	.next_snd_tag = lagg_next_snd_tag,
	.type = IF_SND_TAG_TYPE_TLS
};

#ifdef RATELIMIT
static const struct if_snd_tag_sw lagg_snd_tag_tls_rl_sw = {
	.snd_tag_modify = lagg_snd_tag_modify,
	.snd_tag_query = lagg_snd_tag_query,
	.snd_tag_free = lagg_snd_tag_free,
	.next_snd_tag = lagg_next_snd_tag,
	.type = IF_SND_TAG_TYPE_TLS_RATE_LIMIT
};
#endif
#endif

static inline struct lagg_snd_tag *
mst_to_lst(struct m_snd_tag *mst)
{

	return (__containerof(mst, struct lagg_snd_tag, com));
}

/*
 * Look up the port used by a specific flow.  This only works for lagg
 * protocols with deterministic port mappings (e.g. not roundrobin).
 * In addition protocols which use a hash to map flows to ports must
 * be configured to use the mbuf flowid rather than hashing packet
 * contents.
 */
static struct lagg_port *
lookup_snd_tag_port(struct ifnet *ifp, uint32_t flowid, uint32_t flowtype,
    uint8_t numa_domain)
{
	struct lagg_softc *sc;
	struct lagg_port *lp;
	struct lagg_lb *lb;
	uint32_t hash, p;
	int err;

	sc = ifp->if_softc;

	switch (sc->sc_proto) {
	case LAGG_PROTO_FAILOVER:
		return (lagg_link_active(sc, sc->sc_primary));
	case LAGG_PROTO_LOADBALANCE:
		if ((sc->sc_opts & LAGG_OPT_USE_FLOWID) == 0 ||
		    flowtype == M_HASHTYPE_NONE)
			return (NULL);
		p = flowid >> sc->flowid_shift;
		p %= sc->sc_count;
		lb = (struct lagg_lb *)sc->sc_psc;
		lp = lb->lb_ports[p];
		return (lagg_link_active(sc, lp));
	case LAGG_PROTO_LACP:
		if ((sc->sc_opts & LAGG_OPT_USE_FLOWID) == 0 ||
		    flowtype == M_HASHTYPE_NONE)
			return (NULL);
		hash = flowid >> sc->flowid_shift;
		return (lacp_select_tx_port_by_hash(sc, hash, numa_domain, &err));
	default:
		return (NULL);
	}
}

static int
lagg_snd_tag_alloc(struct ifnet *ifp,
    union if_snd_tag_alloc_params *params,
    struct m_snd_tag **ppmt)
{
	struct epoch_tracker et;
	const struct if_snd_tag_sw *sw;
	struct lagg_snd_tag *lst;
	struct lagg_port *lp;
	struct ifnet *lp_ifp;
	struct m_snd_tag *mst;
	int error;

	switch (params->hdr.type) {
#ifdef RATELIMIT
	case IF_SND_TAG_TYPE_UNLIMITED:
		sw = &lagg_snd_tag_ul_sw;
		break;
	case IF_SND_TAG_TYPE_RATE_LIMIT:
		sw = &lagg_snd_tag_rl_sw;
		break;
#endif
#ifdef KERN_TLS
	case IF_SND_TAG_TYPE_TLS:
		sw = &lagg_snd_tag_tls_sw;
		break;
	case IF_SND_TAG_TYPE_TLS_RX:
		/* Return tag from port interface directly. */
		sw = NULL;
		break;
#ifdef RATELIMIT
	case IF_SND_TAG_TYPE_TLS_RATE_LIMIT:
		sw = &lagg_snd_tag_tls_rl_sw;
		break;
#endif
#endif
	default:
		return (EOPNOTSUPP);
	}

	NET_EPOCH_ENTER(et);
	lp = lookup_snd_tag_port(ifp, params->hdr.flowid,
	    params->hdr.flowtype, params->hdr.numa_domain);
	if (lp == NULL) {
		NET_EPOCH_EXIT(et);
		return (EOPNOTSUPP);
	}
	if (lp->lp_ifp == NULL) {
		NET_EPOCH_EXIT(et);
		return (EOPNOTSUPP);
	}
	lp_ifp = lp->lp_ifp;
	if_ref(lp_ifp);
	NET_EPOCH_EXIT(et);

	if (sw != NULL) {
		lst = malloc(sizeof(*lst), M_LAGG, M_NOWAIT);
		if (lst == NULL) {
			if_rele(lp_ifp);
			return (ENOMEM);
		}
	} else
		lst = NULL;

	error = m_snd_tag_alloc(lp_ifp, params, &mst);
	if_rele(lp_ifp);
	if (error) {
		free(lst, M_LAGG);
		return (error);
	}

	if (sw != NULL) {
		m_snd_tag_init(&lst->com, ifp, sw);
		lst->tag = mst;

		*ppmt = &lst->com;
	} else
		*ppmt = mst;

	return (0);
}

static struct m_snd_tag *
lagg_next_snd_tag(struct m_snd_tag *mst)
{
	struct lagg_snd_tag *lst;

	lst = mst_to_lst(mst);
	return (lst->tag);
}

static int
lagg_snd_tag_modify(struct m_snd_tag *mst,
    union if_snd_tag_modify_params *params)
{
	struct lagg_snd_tag *lst;

	lst = mst_to_lst(mst);
	return (lst->tag->sw->snd_tag_modify(lst->tag, params));
}

static int
lagg_snd_tag_query(struct m_snd_tag *mst,
    union if_snd_tag_query_params *params)
{
	struct lagg_snd_tag *lst;

	lst = mst_to_lst(mst);
	return (lst->tag->sw->snd_tag_query(lst->tag, params));
}

static void
lagg_snd_tag_free(struct m_snd_tag *mst)
{
	struct lagg_snd_tag *lst;

	lst = mst_to_lst(mst);
	m_snd_tag_rele(lst->tag);
	free(lst, M_LAGG);
}

static void
lagg_ratelimit_query(struct ifnet *ifp __unused, struct if_ratelimit_query_results *q)
{
	/*
	 * For lagg, we have an indirect
	 * interface. The caller needs to
	 * get a ratelimit tag on the actual
	 * interface the flow will go on.
	 */
	q->rate_table = NULL;
	q->flags = RT_IS_INDIRECT;
	q->max_flows = 0;
	q->number_of_rates = 0;
}
#endif

static int
lagg_setmulti(struct lagg_port *lp)
{
	struct lagg_softc *sc = lp->lp_softc;
	struct ifnet *ifp = lp->lp_ifp;
	struct ifnet *scifp = sc->sc_ifp;
	struct lagg_mc *mc;
	struct ifmultiaddr *ifma;
	int error;

	IF_ADDR_WLOCK(scifp);
	CK_STAILQ_FOREACH(ifma, &scifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		mc = malloc(sizeof(struct lagg_mc), M_LAGG, M_NOWAIT);
		if (mc == NULL) {
			IF_ADDR_WUNLOCK(scifp);
			return (ENOMEM);
		}
		bcopy(ifma->ifma_addr, &mc->mc_addr, ifma->ifma_addr->sa_len);
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
	return (0);
}

static int
lagg_clrmulti(struct lagg_port *lp)
{
	struct lagg_mc *mc;

	LAGG_XLOCK_ASSERT(lp->lp_softc);
	while ((mc = SLIST_FIRST(&lp->lp_mc_head)) != NULL) {
		SLIST_REMOVE(&lp->lp_mc_head, mc, lagg_mc, mc_entries);
		if (mc->mc_ifma && lp->lp_detaching == 0)
			if_delmulti_ifma(mc->mc_ifma);
		free(mc, M_LAGG);
	}
	return (0);
}

static void
lagg_setcaps(struct lagg_port *lp, int cap, int cap2)
{
	struct ifreq ifr;
	struct siocsifcapnv_driver_data drv_ioctl_data;

	if (lp->lp_ifp->if_capenable == cap &&
	    lp->lp_ifp->if_capenable2 == cap2)
		return;
	if (lp->lp_ioctl == NULL)
		return;
	/* XXX */
	if ((lp->lp_ifp->if_capabilities & IFCAP_NV) != 0) {
		drv_ioctl_data.reqcap = cap;
		drv_ioctl_data.reqcap2 = cap2;
		drv_ioctl_data.nvcap = NULL;
		(*lp->lp_ioctl)(lp->lp_ifp, SIOCSIFCAPNV,
		    (caddr_t)&drv_ioctl_data);
	} else {
		ifr.ifr_reqcap = cap;
		(*lp->lp_ioctl)(lp->lp_ifp, SIOCSIFCAP, (caddr_t)&ifr);
	}
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

	LAGG_XLOCK_ASSERT(sc);

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
lagg_transmit_ethernet(struct ifnet *ifp, struct mbuf *m)
{
	struct lagg_softc *sc = (struct lagg_softc *)ifp->if_softc;

	NET_EPOCH_ASSERT();
#if defined(KERN_TLS) || defined(RATELIMIT)
	if (m->m_pkthdr.csum_flags & CSUM_SND_TAG)
		MPASS(m->m_pkthdr.snd_tag->ifp == ifp);
#endif
	/* We need a Tx algorithm and at least one port */
	if (sc->sc_proto == LAGG_PROTO_NONE || sc->sc_count == 0) {
		m_freem(m);
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		return (ENXIO);
	}

	ETHER_BPF_MTAP(ifp, m);

	return (lagg_proto_start(sc, m));
}

static int
lagg_transmit_infiniband(struct ifnet *ifp, struct mbuf *m)
{
	struct lagg_softc *sc = (struct lagg_softc *)ifp->if_softc;

	NET_EPOCH_ASSERT();
#if defined(KERN_TLS) || defined(RATELIMIT)
	if (m->m_pkthdr.csum_flags & CSUM_SND_TAG)
		MPASS(m->m_pkthdr.snd_tag->ifp == ifp);
#endif
	/* We need a Tx algorithm and at least one port */
	if (sc->sc_proto == LAGG_PROTO_NONE || sc->sc_count == 0) {
		m_freem(m);
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		return (ENXIO);
	}

	infiniband_bpf_mtap(ifp, m);

	return (lagg_proto_start(sc, m));
}

/*
 * The ifp->if_qflush entry point for lagg(4) is no-op.
 */
static void
lagg_qflush(struct ifnet *ifp __unused)
{
}

static struct mbuf *
lagg_input_ethernet(struct ifnet *ifp, struct mbuf *m)
{
	struct lagg_port *lp = ifp->if_lagg;
	struct lagg_softc *sc = lp->lp_softc;
	struct ifnet *scifp = sc->sc_ifp;

	NET_EPOCH_ASSERT();
	if ((scifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ||
	    lp->lp_detaching != 0 ||
	    sc->sc_proto == LAGG_PROTO_NONE) {
		m_freem(m);
		return (NULL);
	}

	m = lagg_proto_input(sc, lp, m);
	if (m != NULL) {
		ETHER_BPF_MTAP(scifp, m);

		if ((scifp->if_flags & IFF_MONITOR) != 0) {
			m_freem(m);
			m = NULL;
		}
	}

#ifdef DEV_NETMAP
	if (m != NULL && scifp->if_capenable & IFCAP_NETMAP) {
		scifp->if_input(scifp, m);
		m = NULL;
	}
#endif	/* DEV_NETMAP */

	return (m);
}

static struct mbuf *
lagg_input_infiniband(struct ifnet *ifp, struct mbuf *m)
{
	struct lagg_port *lp = ifp->if_lagg;
	struct lagg_softc *sc = lp->lp_softc;
	struct ifnet *scifp = sc->sc_ifp;

	NET_EPOCH_ASSERT();
	if ((scifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ||
	    lp->lp_detaching != 0 ||
	    sc->sc_proto == LAGG_PROTO_NONE) {
		m_freem(m);
		return (NULL);
	}

	m = lagg_proto_input(sc, lp, m);
	if (m != NULL) {
		infiniband_bpf_mtap(scifp, m);

		if ((scifp->if_flags & IFF_MONITOR) != 0) {
			m_freem(m);
			m = NULL;
		}
	}

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
	struct epoch_tracker et;
	struct lagg_softc *sc = (struct lagg_softc *)ifp->if_softc;
	struct lagg_port *lp;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_ETHER | IFM_AUTO;

	NET_EPOCH_ENTER(et);
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		if (LAGG_PORTACTIVE(lp))
			imr->ifm_status |= IFM_ACTIVE;
	}
	NET_EPOCH_EXIT(et);
}

static void
lagg_linkstate(struct lagg_softc *sc)
{
	struct epoch_tracker et;
	struct lagg_port *lp;
	int new_link = LINK_STATE_DOWN;
	uint64_t speed;

	LAGG_XLOCK_ASSERT(sc);

	/* LACP handles link state itself */
	if (sc->sc_proto == LAGG_PROTO_LACP)
		return;

	/* Our link is considered up if at least one of our ports is active */
	NET_EPOCH_ENTER(et);
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		if (lp->lp_ifp->if_link_state == LINK_STATE_UP) {
			new_link = LINK_STATE_UP;
			break;
		}
	}
	NET_EPOCH_EXIT(et);
	if_link_state_change(sc->sc_ifp, new_link);

	/* Update if_baudrate to reflect the max possible speed */
	switch (sc->sc_proto) {
		case LAGG_PROTO_FAILOVER:
			sc->sc_ifp->if_baudrate = sc->sc_primary != NULL ?
			    sc->sc_primary->lp_ifp->if_baudrate : 0;
			break;
		case LAGG_PROTO_ROUNDROBIN:
		case LAGG_PROTO_LOADBALANCE:
		case LAGG_PROTO_BROADCAST:
			speed = 0;
			NET_EPOCH_ENTER(et);
			CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
				speed += lp->lp_ifp->if_baudrate;
			NET_EPOCH_EXIT(et);
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

	LAGG_XLOCK(sc);
	lagg_linkstate(sc);
	lagg_proto_linkstate(sc, lp);
	LAGG_XUNLOCK(sc);
}

struct lagg_port *
lagg_link_active(struct lagg_softc *sc, struct lagg_port *lp)
{
	struct lagg_port *lp_next, *rval = NULL;

	/*
	 * Search a port which reports an active link state.
	 */

#ifdef INVARIANTS
	/*
	 * This is called with either in the network epoch
	 * or with LAGG_XLOCK(sc) held.
	 */
	if (!in_epoch(net_epoch_preempt))
		LAGG_XLOCK_ASSERT(sc);
#endif

	if (lp == NULL)
		goto search;
	if (LAGG_PORTACTIVE(lp)) {
		rval = lp;
		goto found;
	}
	if ((lp_next = CK_SLIST_NEXT(lp, lp_entries)) != NULL &&
	    LAGG_PORTACTIVE(lp_next)) {
		rval = lp_next;
		goto found;
	}

search:
	CK_SLIST_FOREACH(lp_next, &sc->sc_ports, lp_entries) {
		if (LAGG_PORTACTIVE(lp_next)) {
			return (lp_next);
		}
	}
found:
	return (rval);
}

int
lagg_enqueue(struct ifnet *ifp, struct mbuf *m)
{

#if defined(KERN_TLS) || defined(RATELIMIT)
	if (m->m_pkthdr.csum_flags & CSUM_SND_TAG) {
		struct lagg_snd_tag *lst;
		struct m_snd_tag *mst;

		mst = m->m_pkthdr.snd_tag;
		lst = mst_to_lst(mst);
		if (lst->tag->ifp != ifp) {
			m_freem(m);
			return (EAGAIN);
		}
		m->m_pkthdr.snd_tag = m_snd_tag_ref(lst->tag);
		m_snd_tag_rele(mst);
	}
#endif
	return (ifp->if_transmit)(ifp, m);
}

/*
 * Simple round robin aggregation
 */
static void
lagg_rr_attach(struct lagg_softc *sc)
{
	sc->sc_seq = 0;
	sc->sc_stride = 1;
}

static int
lagg_rr_start(struct lagg_softc *sc, struct mbuf *m)
{
	struct lagg_port *lp;
	uint32_t p;

	p = atomic_fetchadd_32(&sc->sc_seq, 1);
	p /= sc->sc_stride;
	p %= sc->sc_count;
	lp = CK_SLIST_FIRST(&sc->sc_ports);

	while (p--)
		lp = CK_SLIST_NEXT(lp, lp_entries);

	/*
	 * Check the port's link state. This will return the next active
	 * port if the link is down or the port is NULL.
	 */
	if ((lp = lagg_link_active(sc, lp)) == NULL) {
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, 1);
		m_freem(m);
		return (ENETDOWN);
	}

	/* Send mbuf */
	return (lagg_enqueue(lp->lp_ifp, m));
}

/*
 * Broadcast mode
 */
static int
lagg_bcast_start(struct lagg_softc *sc, struct mbuf *m)
{
	int errors = 0;
	int ret;
	struct lagg_port *lp, *last = NULL;
	struct mbuf *m0;

	NET_EPOCH_ASSERT();
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries) {
		if (!LAGG_PORTACTIVE(lp))
			continue;

		if (last != NULL) {
			m0 = m_copym(m, 0, M_COPYALL, M_NOWAIT);
			if (m0 == NULL) {
				ret = ENOBUFS;
				errors++;
				break;
			}
			lagg_enqueue(last->lp_ifp, m0);
		}
		last = lp;
	}

	if (last == NULL) {
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, 1);
		m_freem(m);
		return (ENOENT);
	}
	if ((last = lagg_link_active(sc, last)) == NULL) {
		errors++;
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, errors);
		m_freem(m);
		return (ENETDOWN);
	}

	ret = lagg_enqueue(last->lp_ifp, m);
	if (errors != 0)
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, errors);

	return (ret);
}

/*
 * Active failover
 */
static int
lagg_fail_start(struct lagg_softc *sc, struct mbuf *m)
{
	struct lagg_port *lp;

	/* Use the master port if active or the next available port */
	if ((lp = lagg_link_active(sc, sc->sc_primary)) == NULL) {
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, 1);
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

	if (lp == sc->sc_primary || V_lagg_failover_rx_all) {
		m->m_pkthdr.rcvif = ifp;
		return (m);
	}

	if (!LAGG_PORTACTIVE(sc->sc_primary)) {
		tmp_tp = lagg_link_active(sc, sc->sc_primary);
		/*
		 * If tmp_tp is null, we've received a packet when all
		 * our links are down. Weird, but process it anyways.
		 */
		if (tmp_tp == NULL || tmp_tp == lp) {
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
static void
lagg_lb_attach(struct lagg_softc *sc)
{
	struct lagg_port *lp;
	struct lagg_lb *lb;

	LAGG_XLOCK_ASSERT(sc);
	lb = malloc(sizeof(struct lagg_lb), M_LAGG, M_WAITOK | M_ZERO);
	lb->lb_key = m_ether_tcpip_hash_init();
	sc->sc_psc = lb;

	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		lagg_lb_port_create(lp);
}

static void
lagg_lb_detach(struct lagg_softc *sc)
{
	struct lagg_lb *lb;

	lb = (struct lagg_lb *)sc->sc_psc;
	if (lb != NULL)
		free(lb, M_LAGG);
}

static int
lagg_lb_porttable(struct lagg_softc *sc, struct lagg_port *lp)
{
	struct lagg_lb *lb = (struct lagg_lb *)sc->sc_psc;
	struct lagg_port *lp_next;
	int i = 0, rv;

	rv = 0;
	bzero(&lb->lb_ports, sizeof(lb->lb_ports));
	LAGG_XLOCK_ASSERT(sc);
	CK_SLIST_FOREACH(lp_next, &sc->sc_ports, lp_entries) {
		if (lp_next == lp)
			continue;
		if (i >= LAGG_MAX_PORTS) {
			rv = EINVAL;
			break;
		}
		if (sc->sc_ifflags & IFF_DEBUG)
			printf("%s: port %s at index %d\n",
			    sc->sc_ifname, lp_next->lp_ifp->if_xname, i);
		lb->lb_ports[i++] = lp_next;
	}

	return (rv);
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

	if ((sc->sc_opts & LAGG_OPT_USE_FLOWID) &&
	    M_HASHTYPE_GET(m) != M_HASHTYPE_NONE)
		p = m->m_pkthdr.flowid >> sc->flowid_shift;
	else
		p = m_ether_tcpip_hash(sc->sc_flags, m, lb->lb_key);
	p %= sc->sc_count;
	lp = lb->lb_ports[p];

	/*
	 * Check the port's link state. This will return the next active
	 * port if the link is down or the port is NULL.
	 */
	if ((lp = lagg_link_active(sc, lp)) == NULL) {
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, 1);
		m_freem(m);
		return (ENETDOWN);
	}

	/* Send mbuf */
	return (lagg_enqueue(lp->lp_ifp, m));
}

/*
 * 802.3ad LACP
 */
static void
lagg_lacp_attach(struct lagg_softc *sc)
{
	struct lagg_port *lp;

	lacp_attach(sc);
	LAGG_XLOCK_ASSERT(sc);
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		lacp_port_create(lp);
}

static void
lagg_lacp_detach(struct lagg_softc *sc)
{
	struct lagg_port *lp;
	void *psc;

	LAGG_XLOCK_ASSERT(sc);
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		lacp_port_destroy(lp);

	psc = sc->sc_psc;
	sc->sc_psc = NULL;
	lacp_detach(psc);
}

static void
lagg_lacp_lladdr(struct lagg_softc *sc)
{
	struct lagg_port *lp;

	LAGG_SXLOCK_ASSERT(sc);

	/* purge all the lacp ports */
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		lacp_port_destroy(lp);

	/* add them back in */
	CK_SLIST_FOREACH(lp, &sc->sc_ports, lp_entries)
		lacp_port_create(lp);
}

static int
lagg_lacp_start(struct lagg_softc *sc, struct mbuf *m)
{
	struct lagg_port *lp;
	int err;

	lp = lacp_select_tx_port(sc, m, &err);
	if (lp == NULL) {
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, 1);
		m_freem(m);
		return (err);
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
	if (!lacp_iscollecting(lp) || !lacp_isactive(lp)) {
		m_freem(m);
		return (NULL);
	}

	m->m_pkthdr.rcvif = ifp;
	return (m);
}

/* Default input */
static struct mbuf *
lagg_default_input(struct lagg_softc *sc, struct lagg_port *lp, struct mbuf *m)
{
	struct ifnet *ifp = sc->sc_ifp;

	/* Just pass in the packet to our lagg device */
	m->m_pkthdr.rcvif = ifp;

	return (m);
}
