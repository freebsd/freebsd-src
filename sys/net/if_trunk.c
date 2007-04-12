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
#include <net/if_trunk.h>
#include <net/ieee8023ad_lacp.h>

/* Special flags we should propagate to the trunk ports. */
static struct {
	int flag;
	int (*func)(struct ifnet *, int);
} trunk_pflags[] = {
	{IFF_PROMISC, ifpromisc},
	{IFF_ALLMULTI, if_allmulti},
	{0, NULL}
};

SLIST_HEAD(__trhead, trunk_softc) trunk_list;	/* list of trunks */
static struct mtx 	trunk_list_mtx;
eventhandler_tag	trunk_detach_cookie = NULL;

static int	trunk_clone_create(struct if_clone *, int, caddr_t);
static void	trunk_clone_destroy(struct ifnet *);
static void	trunk_lladdr(struct trunk_softc *, uint8_t *);
static int	trunk_capabilities(struct trunk_softc *);
static void	trunk_port_lladdr(struct trunk_port *, uint8_t *);
static int	trunk_port_create(struct trunk_softc *, struct ifnet *);
static int	trunk_port_destroy(struct trunk_port *, int);
static struct mbuf *trunk_input(struct ifnet *, struct mbuf *);
static void	trunk_port_state(struct ifnet *, int);
static int	trunk_port_ioctl(struct ifnet *, u_long, caddr_t);
static int	trunk_port_output(struct ifnet *, struct mbuf *,
		    struct sockaddr *, struct rtentry *);
static void	trunk_port_ifdetach(void *arg __unused, struct ifnet *);
static int	trunk_port_checkstacking(struct trunk_softc *);
static void	trunk_port2req(struct trunk_port *, struct trunk_reqport *);
static void	trunk_init(void *);
static void	trunk_stop(struct trunk_softc *);
static int	trunk_ioctl(struct ifnet *, u_long, caddr_t);
static int	trunk_ether_setmulti(struct trunk_softc *);
static int	trunk_ether_cmdmulti(struct trunk_port *, int);
static void	trunk_ether_purgemulti(struct trunk_softc *);
static	int	trunk_setflag(struct trunk_port *, int, int,
		    int (*func)(struct ifnet *, int));
static	int	trunk_setflags(struct trunk_port *, int status);
static void	trunk_start(struct ifnet *);
static int	trunk_media_change(struct ifnet *);
static void	trunk_media_status(struct ifnet *, struct ifmediareq *);
static struct trunk_port *trunk_link_active(struct trunk_softc *,
	    struct trunk_port *);
static const void *trunk_gethdr(struct mbuf *, u_int, u_int, void *);

IFC_SIMPLE_DECLARE(trunk, 0);

/* Simple round robin */
static int	trunk_rr_attach(struct trunk_softc *);
static int	trunk_rr_detach(struct trunk_softc *);
static void	trunk_rr_port_destroy(struct trunk_port *);
static int	trunk_rr_start(struct trunk_softc *, struct mbuf *);
static struct mbuf *trunk_rr_input(struct trunk_softc *, struct trunk_port *,
		    struct mbuf *);

/* Active failover */
static int	trunk_fail_attach(struct trunk_softc *);
static int	trunk_fail_detach(struct trunk_softc *);
static int	trunk_fail_start(struct trunk_softc *, struct mbuf *);
static struct mbuf *trunk_fail_input(struct trunk_softc *, struct trunk_port *,
		    struct mbuf *);

/* Loadbalancing */
static int	trunk_lb_attach(struct trunk_softc *);
static int	trunk_lb_detach(struct trunk_softc *);
static int	trunk_lb_port_create(struct trunk_port *);
static void	trunk_lb_port_destroy(struct trunk_port *);
static int	trunk_lb_start(struct trunk_softc *, struct mbuf *);
static struct mbuf *trunk_lb_input(struct trunk_softc *, struct trunk_port *,
		    struct mbuf *);
static int	trunk_lb_porttable(struct trunk_softc *, struct trunk_port *);

/* 802.3ad LACP */
static int	trunk_lacp_attach(struct trunk_softc *);
static int	trunk_lacp_detach(struct trunk_softc *);
static int	trunk_lacp_start(struct trunk_softc *, struct mbuf *);
static struct mbuf *trunk_lacp_input(struct trunk_softc *, struct trunk_port *,
		    struct mbuf *);
static void	trunk_lacp_lladdr(struct trunk_softc *);

/* Trunk protocol table */
static const struct {
	int			ti_proto;
	int			(*ti_attach)(struct trunk_softc *);
} trunk_protos[] = {
	{ TRUNK_PROTO_ROUNDROBIN,	trunk_rr_attach },
	{ TRUNK_PROTO_FAILOVER,		trunk_fail_attach },
	{ TRUNK_PROTO_LOADBALANCE,	trunk_lb_attach },
	{ TRUNK_PROTO_ETHERCHANNEL,	trunk_lb_attach },
	{ TRUNK_PROTO_LACP,		trunk_lacp_attach },
	{ TRUNK_PROTO_NONE,		NULL }
};

static int
trunk_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		mtx_init(&trunk_list_mtx, "if_trunk list", NULL, MTX_DEF);
		SLIST_INIT(&trunk_list);
		if_clone_attach(&trunk_cloner);
		trunk_input_p = trunk_input;
		trunk_linkstate_p = trunk_port_state;
		trunk_detach_cookie = EVENTHANDLER_REGISTER(
		    ifnet_departure_event, trunk_port_ifdetach, NULL,
		    EVENTHANDLER_PRI_ANY);
		break;
	case MOD_UNLOAD:
		EVENTHANDLER_DEREGISTER(ifnet_departure_event,
		    trunk_detach_cookie);
		if_clone_detach(&trunk_cloner);
		trunk_input_p = NULL;
		trunk_linkstate_p = NULL;
		mtx_destroy(&trunk_list_mtx);
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t trunk_mod = {
	"if_trunk",
	trunk_modevent,
	0
};

DECLARE_MODULE(if_trunk, trunk_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

static int
trunk_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct trunk_softc *tr;
	struct ifnet *ifp;
	int i, error = 0;
	static const u_char eaddr[6];	/* 00:00:00:00:00:00 */

	tr = malloc(sizeof(*tr), M_DEVBUF, M_WAITOK|M_ZERO);
	ifp = tr->tr_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		free(tr, M_DEVBUF);
		return (ENOSPC);
	}

	tr->tr_proto = TRUNK_PROTO_NONE;
	for (i = 0; trunk_protos[i].ti_proto != TRUNK_PROTO_NONE; i++) {
		if (trunk_protos[i].ti_proto == TRUNK_PROTO_DEFAULT) {
			tr->tr_proto = trunk_protos[i].ti_proto;
			if ((error = trunk_protos[i].ti_attach(tr)) != 0) {
				if_free_type(ifp, IFT_ETHER);
				free(tr, M_DEVBUF);
				return (error);
			}
			break;
		}
	}
	TRUNK_LOCK_INIT(tr);
	SLIST_INIT(&tr->tr_ports);

	/* Initialise pseudo media types */
	ifmedia_init(&tr->tr_media, 0, trunk_media_change,
	    trunk_media_status);
	ifmedia_add(&tr->tr_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&tr->tr_media, IFM_ETHER | IFM_AUTO);

	if_initname(ifp, ifc->ifc_name, unit);
	ifp->if_type = IFT_ETHER;
	ifp->if_softc = tr;
	ifp->if_start = trunk_start;
	ifp->if_init = trunk_init;
	ifp->if_ioctl = trunk_ioctl;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST;

	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Attach as an ordinary ethernet device, childs will be attached
	 * as special device IFT_IEEE8023ADLAG.
	 */
	ether_ifattach(ifp, eaddr);

	/* Insert into the global list of trunks */
	mtx_lock(&trunk_list_mtx);
	SLIST_INSERT_HEAD(&trunk_list, tr, tr_entries);
	mtx_unlock(&trunk_list_mtx);

	return (0);
}

static void
trunk_clone_destroy(struct ifnet *ifp)
{
	struct trunk_softc *tr = (struct trunk_softc *)ifp->if_softc;
	struct trunk_port *tp;

	TRUNK_LOCK(tr);

	trunk_stop(tr);
	ifp->if_flags &= ~IFF_UP;

	/* Shutdown and remove trunk ports */
	while ((tp = SLIST_FIRST(&tr->tr_ports)) != NULL)
		trunk_port_destroy(tp, 1);
	/* Unhook the trunking protocol */
	if (tr->tr_detach != NULL)
		(*tr->tr_detach)(tr);

	/* Remove any multicast groups that we may have joined. */
	trunk_ether_purgemulti(tr);

	TRUNK_UNLOCK(tr);

	ifmedia_removeall(&tr->tr_media);
	ether_ifdetach(ifp);
	if_free_type(ifp, IFT_ETHER);

	mtx_lock(&trunk_list_mtx);
	SLIST_REMOVE(&trunk_list, tr, trunk_softc, tr_entries);
	mtx_unlock(&trunk_list_mtx);

	TRUNK_LOCK_DESTROY(tr);
	free(tr, M_DEVBUF);
}

static void
trunk_lladdr(struct trunk_softc *tr, uint8_t *lladdr)
{
	struct ifnet *ifp = tr->tr_ifp;

	if (memcmp(lladdr, IF_LLADDR(ifp), ETHER_ADDR_LEN) == 0)
		return;

	bcopy(lladdr, IF_LLADDR(ifp), ETHER_ADDR_LEN);
	/* Let the protocol know the MAC has changed */
	if (tr->tr_lladdr != NULL)
		(*tr->tr_lladdr)(tr);
}

static int
trunk_capabilities(struct trunk_softc *tr)
{
	struct trunk_port *tp;
	int cap = ~0, priv;

	TRUNK_LOCK_ASSERT(tr);

	/* Preserve private capabilities */
	priv = tr->tr_capabilities & IFCAP_TRUNK_MASK;

	/* Get capabilities from the trunk ports */
	SLIST_FOREACH(tp, &tr->tr_ports, tp_entries)
		cap &= tp->tp_capabilities;

	if (tr->tr_ifflags & IFF_DEBUG) {
		printf("%s: capabilities 0x%08x\n",
		    tr->tr_ifname, cap == ~0 ? priv : (cap | priv));
	}

	return (cap == ~0 ? priv : (cap | priv));
}

static void
trunk_port_lladdr(struct trunk_port *tp, uint8_t *lladdr)
{
	struct ifnet *ifp = tp->tp_ifp;
	int error;

	if (memcmp(lladdr, IF_LLADDR(ifp), ETHER_ADDR_LEN) == 0)
		return;

	/* Set the link layer address */
	error = if_setlladdr(ifp, lladdr, ETHER_ADDR_LEN);
	if (error)
		printf("%s: setlladdr failed on %s\n", __func__, tp->tp_ifname);

}

static int
trunk_port_create(struct trunk_softc *tr, struct ifnet *ifp)
{
	struct trunk_softc *tr_ptr;
	struct trunk_port *tp;
	int error = 0;

	TRUNK_LOCK_ASSERT(tr);

	/* Limit the maximal number of trunk ports */
	if (tr->tr_count >= TRUNK_MAX_PORTS)
		return (ENOSPC);

	/* New trunk port has to be in an idle state */
	if (ifp->if_drv_flags & IFF_DRV_OACTIVE)
		return (EBUSY);

	/* Check if port has already been associated to a trunk */
	if (ifp->if_trunk != NULL)
		return (EBUSY);

	/* XXX Disallow non-ethernet interfaces (this should be any of 802) */
	if (ifp->if_type != IFT_ETHER)
		return (EPROTONOSUPPORT);

	if ((tp = malloc(sizeof(struct trunk_port),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	/* Check if port is a stacked trunk */
	mtx_lock(&trunk_list_mtx);
	SLIST_FOREACH(tr_ptr, &trunk_list, tr_entries) {
		if (ifp == tr_ptr->tr_ifp) {
			mtx_unlock(&trunk_list_mtx);
			free(tp, M_DEVBUF);
			return (EINVAL);
			/* XXX disable stacking for the moment, its untested
			tp->tp_flags |= TRUNK_PORT_STACK;
			if (trunk_port_checkstacking(tr_ptr) >=
			    TRUNK_MAX_STACKING) {
				mtx_unlock(&trunk_list_mtx);
				free(tp, M_DEVBUF);
				return (E2BIG);
			}
			*/
		}
	}
	mtx_unlock(&trunk_list_mtx);

	/* Change the interface type */
	tp->tp_iftype = ifp->if_type;
	ifp->if_type = IFT_IEEE8023ADLAG;
	ifp->if_trunk = tp;
	tp->tp_ioctl = ifp->if_ioctl;
	ifp->if_ioctl = trunk_port_ioctl;
	tp->tp_output = ifp->if_output;
	ifp->if_output = trunk_port_output;

	tp->tp_ifp = ifp;
	tp->tp_trunk = tr;

	/* Save port link layer address */
	bcopy(IF_LLADDR(ifp), tp->tp_lladdr, ETHER_ADDR_LEN);

	if (SLIST_EMPTY(&tr->tr_ports)) {
		tr->tr_primary = tp;
		trunk_lladdr(tr, IF_LLADDR(ifp));
	} else {
		/* Update link layer address for this port */
		trunk_port_lladdr(tp, IF_LLADDR(tr->tr_ifp));
	}

	/* Insert into the list of ports */
	SLIST_INSERT_HEAD(&tr->tr_ports, tp, tp_entries);
	tr->tr_count++;

	/* Update trunk capabilities */
	tr->tr_capabilities = trunk_capabilities(tr);

	/* Add multicast addresses and interface flags to this port */
	trunk_ether_cmdmulti(tp, 1);
	trunk_setflags(tp, 1);

	if (tr->tr_port_create != NULL)
		error = (*tr->tr_port_create)(tp);
	if (error) {
		/* remove the port again, without calling tr_port_destroy */ 
		trunk_port_destroy(tp, 0);
		return (error);
	}

	return (error);
}

static int
trunk_port_checkstacking(struct trunk_softc *tr)
{
	struct trunk_softc *tr_ptr;
	struct trunk_port *tp;
	int m = 0;

	TRUNK_LOCK_ASSERT(tr);

	SLIST_FOREACH(tp, &tr->tr_ports, tp_entries) {
		if (tp->tp_flags & TRUNK_PORT_STACK) {
			tr_ptr = (struct trunk_softc *)tp->tp_ifp->if_softc;
			m = MAX(m, trunk_port_checkstacking(tr_ptr));
		}
	}

	return (m + 1);
}

static int
trunk_port_destroy(struct trunk_port *tp, int runpd)
{
	struct trunk_softc *tr = tp->tp_trunk;
	struct trunk_port *tp_ptr;
	struct ifnet *ifp = tp->tp_ifp;

	TRUNK_LOCK_ASSERT(tr);

	if (runpd && tr->tr_port_destroy != NULL)
		(*tr->tr_port_destroy)(tp);

	/* Remove multicast addresses and interface flags from this port */
	trunk_ether_cmdmulti(tp, 0);
	trunk_setflags(tp, 0);

	/* Restore interface */
	ifp->if_type = tp->tp_iftype;
	ifp->if_ioctl = tp->tp_ioctl;
	ifp->if_output = tp->tp_output;
	ifp->if_trunk = NULL;

	/* Finally, remove the port from the trunk */
	SLIST_REMOVE(&tr->tr_ports, tp, trunk_port, tp_entries);
	tr->tr_count--;

	/* Update the primary interface */
	if (tp == tr->tr_primary) {
		uint8_t lladdr[ETHER_ADDR_LEN];

		if ((tp_ptr = SLIST_FIRST(&tr->tr_ports)) == NULL) {
			bzero(&lladdr, ETHER_ADDR_LEN);
		} else {
			bcopy(tp_ptr->tp_lladdr,
			    lladdr, ETHER_ADDR_LEN);
		}
		trunk_lladdr(tr, lladdr);
		tr->tr_primary = tp_ptr;

		/* Update link layer address for each port */
		SLIST_FOREACH(tp_ptr, &tr->tr_ports, tp_entries)
			trunk_port_lladdr(tp_ptr, lladdr);
	}

	/* Reset the port lladdr */
	trunk_port_lladdr(tp, tp->tp_lladdr);

	if (tp->tp_ifflags)
		if_printf(ifp, "%s: tp_ifflags unclean\n", __func__);

	free(tp, M_DEVBUF);

	/* Update trunk capabilities */
	tr->tr_capabilities = trunk_capabilities(tr);

	return (0);
}

static int
trunk_port_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct trunk_reqport *rp = (struct trunk_reqport *)data;
	struct trunk_softc *tr;
	struct trunk_port *tp = NULL;
	int error = 0;

	/* Should be checked by the caller */
	if (ifp->if_type != IFT_IEEE8023ADLAG ||
	    (tp = ifp->if_trunk) == NULL || (tr = tp->tp_trunk) == NULL)
		goto fallback;

	switch (cmd) {
	case SIOCGTRUNKPORT:
		TRUNK_LOCK(tr);
		if (rp->rp_portname[0] == '\0' ||
		    ifunit(rp->rp_portname) != ifp) {
			error = EINVAL;
			break;
		}

		if (tp->tp_trunk != tr) {
			error = ENOENT;
			break;
		}

		trunk_port2req(tp, rp);
		TRUNK_UNLOCK(tr);
		break;
	default:
		goto fallback;
	}

	return (error);

fallback:
	if (tp != NULL)
		return ((*tp->tp_ioctl)(ifp, cmd, data));

	return (EINVAL);
}

static int
trunk_port_output(struct ifnet *ifp, struct mbuf *m,
	struct sockaddr *dst, struct rtentry *rt0)
{
	struct trunk_port *tp = ifp->if_trunk;
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
	 * trunked frames take a different path.
	 */
	switch (ntohs(type)) {
		case ETHERTYPE_PAE:	/* EAPOL PAE/802.1x */
			return ((*tp->tp_output)(ifp, m, dst, rt0));
	}

	/* drop any other frames */
	m_freem(m);
	return (EBUSY);
}

static void
trunk_port_ifdetach(void *arg __unused, struct ifnet *ifp)
{
	struct trunk_port *tp;
	struct trunk_softc *tr;

	if ((tp = ifp->if_trunk) == NULL)
		return;

	tr = tp->tp_trunk;

	TRUNK_LOCK(tr);
	trunk_port_destroy(tp, 1);
	TRUNK_UNLOCK(tr);
}

static void
trunk_port2req(struct trunk_port *tp, struct trunk_reqport *rp)
{
	struct trunk_softc *tr = tp->tp_trunk;
	strlcpy(rp->rp_ifname, tr->tr_ifname, sizeof(rp->rp_ifname));
	strlcpy(rp->rp_portname, tp->tp_ifp->if_xname, sizeof(rp->rp_portname));
	rp->rp_prio = tp->tp_prio;
	rp->rp_flags = tp->tp_flags;

	/* Add protocol specific flags */
	switch (tr->tr_proto) {
		case TRUNK_PROTO_FAILOVER:
			if (tp == tr->tr_primary)
				tp->tp_flags |= TRUNK_PORT_MASTER;
			/* FALLTHROUGH */
		case TRUNK_PROTO_ROUNDROBIN:
		case TRUNK_PROTO_LOADBALANCE:
		case TRUNK_PROTO_ETHERCHANNEL:
			if (TRUNK_PORTACTIVE(tp))
				rp->rp_flags |= TRUNK_PORT_ACTIVE;
			break;

		case TRUNK_PROTO_LACP:
			/* LACP has a different definition of active */
			if (lacp_port_isactive(tp))
				rp->rp_flags |= TRUNK_PORT_ACTIVE;
			break;
	}

}

static void
trunk_init(void *xsc)
{
	struct trunk_softc *tr = (struct trunk_softc *)xsc;
	struct trunk_port *tp;
	struct ifnet *ifp = tr->tr_ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	TRUNK_LOCK(tr);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	/* Update the port lladdrs */
	SLIST_FOREACH(tp, &tr->tr_ports, tp_entries)
		trunk_port_lladdr(tp, IF_LLADDR(ifp));

	if (tr->tr_init != NULL)
		(*tr->tr_init)(tr);

	TRUNK_UNLOCK(tr);
}

static void
trunk_stop(struct trunk_softc *tr)
{
	struct ifnet *ifp = tr->tr_ifp;

	TRUNK_LOCK_ASSERT(tr);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	if (tr->tr_stop != NULL)
		(*tr->tr_stop)(tr);
}

static int
trunk_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct trunk_softc *tr = (struct trunk_softc *)ifp->if_softc;
	struct trunk_reqall *ra = (struct trunk_reqall *)data;
	struct trunk_reqport *rp = (struct trunk_reqport *)data, rpbuf;
	struct ifreq *ifr = (struct ifreq *)data;
	struct trunk_port *tp;
	struct ifnet *tpif;
	struct thread *td = curthread;
	int i, error = 0, unlock = 1;

	TRUNK_LOCK(tr);

	bzero(&rpbuf, sizeof(rpbuf));

	switch (cmd) {
	case SIOCGTRUNK:
		ra->ra_proto = tr->tr_proto;
		ra->ra_ports = i = 0;
		tp = SLIST_FIRST(&tr->tr_ports);
		while (tp && ra->ra_size >=
		    i + sizeof(struct trunk_reqport)) {
			trunk_port2req(tp, &rpbuf);
			error = copyout(&rpbuf, (caddr_t)ra->ra_port + i,
			    sizeof(struct trunk_reqport));
			if (error)
				break;
			i += sizeof(struct trunk_reqport);
			ra->ra_ports++;
			tp = SLIST_NEXT(tp, tp_entries);
		}
		break;
	case SIOCSTRUNK:
		error = priv_check(td, PRIV_NET_TRUNK);
		if (error)
			break;
		if (ra->ra_proto >= TRUNK_PROTO_MAX) {
			error = EPROTONOSUPPORT;
			break;
		}
		if (tr->tr_proto != TRUNK_PROTO_NONE) {
			error = tr->tr_detach(tr);
			/* Reset protocol and pointers */
			tr->tr_proto = TRUNK_PROTO_NONE;
			tr->tr_detach = NULL;
			tr->tr_start = NULL;
			tr->tr_input = NULL;
			tr->tr_port_create = NULL;
			tr->tr_port_destroy = NULL;
			tr->tr_linkstate = NULL;
			tr->tr_init = NULL;
			tr->tr_stop = NULL;
			tr->tr_lladdr = NULL;
		}
		if (error != 0)
			break;
		for (i = 0; i < (sizeof(trunk_protos) /
		    sizeof(trunk_protos[0])); i++) {
			if (trunk_protos[i].ti_proto == ra->ra_proto) {
				if (tr->tr_ifflags & IFF_DEBUG)
					printf("%s: using proto %u\n",
					    tr->tr_ifname,
					    trunk_protos[i].ti_proto);
				tr->tr_proto = trunk_protos[i].ti_proto;
				if (tr->tr_proto != TRUNK_PROTO_NONE)
					error = trunk_protos[i].ti_attach(tr);
				goto out;
			}
		}
		error = EPROTONOSUPPORT;
		break;
	case SIOCGTRUNKPORT:
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}

		if ((tp = (struct trunk_port *)tpif->if_trunk) == NULL ||
		    tp->tp_trunk != tr) {
			error = ENOENT;
			break;
		}

		trunk_port2req(tp, rp);
		break;
	case SIOCSTRUNKPORT:
		error = priv_check(td, PRIV_NET_TRUNK);
		if (error)
			break;
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}
		error = trunk_port_create(tr, tpif);
		break;
	case SIOCSTRUNKDELPORT:
		error = priv_check(td, PRIV_NET_TRUNK);
		if (error)
			break;
		if (rp->rp_portname[0] == '\0' ||
		    (tpif = ifunit(rp->rp_portname)) == NULL) {
			error = EINVAL;
			break;
		}

		if ((tp = (struct trunk_port *)tpif->if_trunk) == NULL ||
		    tp->tp_trunk != tr) {
			error = ENOENT;
			break;
		}

		error = trunk_port_destroy(tp, 1);
		break;
	case SIOCSIFFLAGS:
		/* Set flags on ports too */
		SLIST_FOREACH(tp, &tr->tr_ports, tp_entries) {
			trunk_setflags(tp, 1);
		}

		if (!(ifp->if_flags & IFF_UP) &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			/*
			 * If interface is marked down and it is running,
			 * then stop and disable it.
			 */
			trunk_stop(tr);
		} else if ((ifp->if_flags & IFF_UP) &&
		    !(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			TRUNK_UNLOCK(tr);
			unlock = 0;
			(*ifp->if_init)(tr);
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = trunk_ether_setmulti(tr);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		TRUNK_UNLOCK(tr);
		unlock = 0;
		error = ifmedia_ioctl(ifp, ifr, &tr->tr_media, cmd);
		break;
	default:
		TRUNK_UNLOCK(tr);
		unlock = 0;
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

out:
	if (unlock)
		TRUNK_UNLOCK(tr);
	return (error);
}

static int
trunk_ether_setmulti(struct trunk_softc *tr)
{
	struct ifnet		*trifp = tr->tr_ifp;
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma, *rifma = NULL;
	struct trunk_port	*tp;
	struct trunk_mc		*mc;
	struct sockaddr_dl	sdl;
	int			error;

	TRUNK_LOCK_ASSERT(tr);

	bzero((char *)&sdl, sizeof(sdl));
	sdl.sdl_len = sizeof(sdl);
	sdl.sdl_family = AF_LINK;
	sdl.sdl_type = IFT_ETHER;
	sdl.sdl_alen = ETHER_ADDR_LEN;

	/* First, remove any existing filter entries. */
	trunk_ether_purgemulti(tr);

	/* Now program new ones. */
	TAILQ_FOREACH(ifma, &trifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		mc = malloc(sizeof(struct trunk_mc), M_DEVBUF, M_NOWAIT);
		if (mc == NULL)
			return (ENOMEM);
		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    (char *)&mc->mc_addr, ETHER_ADDR_LEN);
		SLIST_INSERT_HEAD(&tr->tr_mc_head, mc, mc_entries);
		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    LLADDR(&sdl), ETHER_ADDR_LEN);

		/* do all the ports */
		SLIST_FOREACH(tp, &tr->tr_ports, tp_entries) {
			ifp = tp->tp_ifp;
			sdl.sdl_index = ifp->if_index;
			error = if_addmulti(ifp, (struct sockaddr *)&sdl, &rifma);
			if (error)
				return (error);
		}	
	}

	return (0);
}

static int
trunk_ether_cmdmulti(struct trunk_port *tp, int set)
{
	struct trunk_softc *tr = tp->tp_trunk;
	struct ifnet *ifp = tp->tp_ifp;;
	struct trunk_mc		*mc;
	struct ifmultiaddr	*rifma = NULL;
	struct sockaddr_dl	sdl;
	int			error;

	TRUNK_LOCK_ASSERT(tr);

	bzero((char *)&sdl, sizeof(sdl));
	sdl.sdl_len = sizeof(sdl);
	sdl.sdl_family = AF_LINK;
	sdl.sdl_type = IFT_ETHER;
	sdl.sdl_alen = ETHER_ADDR_LEN;
	sdl.sdl_index = ifp->if_index;

	SLIST_FOREACH(mc, &tr->tr_mc_head, mc_entries) {
		bcopy((char *)&mc->mc_addr, LLADDR(&sdl), ETHER_ADDR_LEN);

		if (set)
			error = if_addmulti(ifp, (struct sockaddr *)&sdl, &rifma);
		else
			error = if_delmulti(ifp, (struct sockaddr *)&sdl);

		if (error) {
			printf("cmdmulti error on %s, set = %d\n",
			    ifp->if_xname, set);
			return (error);
		}
	}
	return (0);
}

static void
trunk_ether_purgemulti(struct trunk_softc *tr)
{
	struct trunk_port *tp;
	struct trunk_mc *mc;

	TRUNK_LOCK_ASSERT(tr);

	/* remove from ports */
	SLIST_FOREACH(tp, &tr->tr_ports, tp_entries)
		trunk_ether_cmdmulti(tp, 0);

	while ((mc = SLIST_FIRST(&tr->tr_mc_head)) != NULL) {
		SLIST_REMOVE(&tr->tr_mc_head, mc, trunk_mc, mc_entries);
		free(mc, M_DEVBUF);
	}
}

/* Handle a ref counted flag that should be set on the trunk port as well */
static int
trunk_setflag(struct trunk_port *tp, int flag, int status,
	     int (*func)(struct ifnet *, int))
{
	struct trunk_softc *tr = tp->tp_trunk;
	struct ifnet *trifp = tr->tr_ifp;
	struct ifnet *ifp = tp->tp_ifp;
	int error;

	TRUNK_LOCK_ASSERT(tr);

	status = status ? (trifp->if_flags & flag) : 0;
	/* Now "status" contains the flag value or 0 */

	/*
	 * See if recorded ports status is different from what
	 * we want it to be.  If it is, flip it.  We record ports
	 * status in tp_ifflags so that we won't clear ports flag
	 * we haven't set.  In fact, we don't clear or set ports
	 * flags directly, but get or release references to them.
	 * That's why we can be sure that recorded flags still are
	 * in accord with actual ports flags.
	 */
	if (status != (tp->tp_ifflags & flag)) {
		error = (*func)(ifp, status);
		if (error)
			return (error);
		tp->tp_ifflags &= ~flag;
		tp->tp_ifflags |= status;
	}
	return (0);
}

/*
 * Handle IFF_* flags that require certain changes on the trunk port
 * if "status" is true, update ports flags respective to the trunk
 * if "status" is false, forcedly clear the flags set on port.
 */
static int
trunk_setflags(struct trunk_port *tp, int status)
{
	int error, i;
	
	for (i = 0; trunk_pflags[i].flag; i++) {
		error = trunk_setflag(tp, trunk_pflags[i].flag,
		    status, trunk_pflags[i].func);
		if (error)
			return (error);
	}
	return (0);
}

static void
trunk_start(struct ifnet *ifp)
{
	struct trunk_softc *tr = (struct trunk_softc *)ifp->if_softc;
	struct mbuf *m;
	int error = 0;

	for (;; error = 0) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		BPF_MTAP(ifp, m);

		if (tr->tr_proto != TRUNK_PROTO_NONE) {
			TRUNK_LOCK(tr);
			error = (*tr->tr_start)(tr, m);
			TRUNK_UNLOCK(tr);
		} else
			m_free(m);

		if (error == 0)
			ifp->if_opackets++;
		else
			ifp->if_oerrors++;
	}

	return;
}

static struct mbuf *
trunk_input(struct ifnet *ifp, struct mbuf *m)
{
	struct trunk_port *tp = ifp->if_trunk;
	struct trunk_softc *tr = tp->tp_trunk;
	struct ifnet *trifp = tr->tr_ifp;

	if ((trifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ||
	    tr->tr_proto == TRUNK_PROTO_NONE) {
		m_freem(m);
		return (NULL);
	}

	TRUNK_LOCK(tr);
	BPF_MTAP(trifp, m);

	m = (*tr->tr_input)(tr, tp, m);

	if (m != NULL) {
		ifp->if_ipackets++;
		ifp->if_ibytes += m->m_pkthdr.len;
		trifp->if_ipackets++;
		trifp->if_ibytes += m->m_pkthdr.len;
	}

	TRUNK_UNLOCK(tr);
	return (m);
}

static int
trunk_media_change(struct ifnet *ifp)
{
	struct trunk_softc *tr = (struct trunk_softc *)ifp->if_softc;

	if (tr->tr_ifflags & IFF_DEBUG)
		printf("%s\n", __func__);

	/* Ignore */
	return (0);
}

static void
trunk_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct trunk_softc *tr = (struct trunk_softc *)ifp->if_softc;
	struct trunk_port *tp;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_ETHER | IFM_AUTO;

	TRUNK_LOCK(tr);
	tp = tr->tr_primary;
	if (tp != NULL && tp->tp_ifp->if_flags & IFF_UP)
		imr->ifm_status |= IFM_ACTIVE;
	TRUNK_UNLOCK(tr);
}

static void
trunk_port_state(struct ifnet *ifp, int state)
{
	struct trunk_port *tp = (struct trunk_port *)ifp->if_trunk;
	struct trunk_softc *tr = NULL;

	if (tp != NULL)
		tr = tp->tp_trunk;
	if (tr == NULL)
		return;

	TRUNK_LOCK(tr);
	if (tr->tr_linkstate != NULL)
		(*tr->tr_linkstate)(tp);
	TRUNK_UNLOCK(tr);
}

struct trunk_port *
trunk_link_active(struct trunk_softc *tr, struct trunk_port *tp)
{
	struct trunk_port *tp_next, *rval = NULL;
	// int new_link = LINK_STATE_DOWN;

	TRUNK_LOCK_ASSERT(tr);
	/*
	 * Search a port which reports an active link state.
	 */

	if (tp == NULL)
		goto search;
	if (TRUNK_PORTACTIVE(tp)) {
		rval = tp;
		goto found;
	}
	if ((tp_next = SLIST_NEXT(tp, tp_entries)) != NULL &&
	    TRUNK_PORTACTIVE(tp_next)) {
		rval = tp_next;
		goto found;
	}

search:
	SLIST_FOREACH(tp_next, &tr->tr_ports, tp_entries) {
		if (TRUNK_PORTACTIVE(tp_next)) {
			rval = tp_next;
			goto found;
		}
	}

found:
	if (rval != NULL) {
		/*
		 * The IEEE 802.1D standard assumes that a trunk with
		 * multiple ports is always full duplex. This is valid
		 * for load sharing trunks and if at least two links
		 * are active. Unfortunately, checking the latter would
		 * be too expensive at this point.
		 XXX
		if ((tr->tr_capabilities & IFCAP_TRUNK_FULLDUPLEX) &&
		    (tr->tr_count > 1))
			new_link = LINK_STATE_FULL_DUPLEX;
		else
			new_link = rval->tp_link_state;
		 */
	}

	return (rval);
}

static const void *
trunk_gethdr(struct mbuf *m, u_int off, u_int len, void *buf)
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
trunk_hashmbuf(struct mbuf *m, uint32_t key)
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
		vlan = trunk_gethdr(m, off,  sizeof(*vlan), &vlanbuf);
		if (vlan == NULL) 
			goto out;

		p = hash32_buf(&vlan->evl_tag, sizeof(vlan->evl_tag), p);
		etype = ntohs(vlan->evl_proto);
		off += sizeof(*vlan) - sizeof(*eh);
	}

	switch (etype) {
#ifdef INET
	case ETHERTYPE_IP:
		ip = trunk_gethdr(m, off, sizeof(*ip), &ipbuf);
		if (ip == NULL)
			goto out;

		p = hash32_buf(&ip->ip_src, sizeof(struct in_addr), p);
		p = hash32_buf(&ip->ip_dst, sizeof(struct in_addr), p);
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		ip6 = trunk_gethdr(m, off, sizeof(*ip6), &ip6buf);
		if (ip6 == NULL)
			goto out;

		p = hash32_buf(&ip6->ip6_src, sizeof(struct in6_addr), p);
		p = hash32_buf(&ip6->ip6_dst, sizeof(struct in6_addr), p);
		break;
#endif
	}
out:
	return (p);
}

int
trunk_enqueue(struct ifnet *ifp, struct mbuf *m)
{
	int error = 0;

	/* Send mbuf */
	IFQ_ENQUEUE(&ifp->if_snd, m, error);
	if (error)
		return (error);
	if ((ifp->if_drv_flags & IFF_DRV_OACTIVE) == 0)
		(*ifp->if_start)(ifp);

	ifp->if_obytes += m->m_pkthdr.len;
	if (m->m_flags & M_MCAST)
		ifp->if_omcasts++;

	return (error);
}

/*
 * Simple round robin trunking
 */

static int
trunk_rr_attach(struct trunk_softc *tr)
{
	struct trunk_port *tp;

	tr->tr_detach = trunk_rr_detach;
	tr->tr_start = trunk_rr_start;
	tr->tr_input = trunk_rr_input;
	tr->tr_port_create = NULL;
	tr->tr_port_destroy = trunk_rr_port_destroy;
	tr->tr_capabilities = IFCAP_TRUNK_FULLDUPLEX;

	tp = SLIST_FIRST(&tr->tr_ports);
	tr->tr_psc = (caddr_t)tp;

	return (0);
}

static int
trunk_rr_detach(struct trunk_softc *tr)
{
	tr->tr_psc = NULL;
	return (0);
}

static void
trunk_rr_port_destroy(struct trunk_port *tp)
{
	struct trunk_softc *tr = tp->tp_trunk;

	if (tp == (struct trunk_port *)tr->tr_psc)
		tr->tr_psc = NULL;
}

static int
trunk_rr_start(struct trunk_softc *tr, struct mbuf *m)
{
	struct trunk_port *tp = (struct trunk_port *)tr->tr_psc, *tp_next;
	int error = 0;

	if (tp == NULL && (tp = trunk_link_active(tr, NULL)) == NULL)
		return (ENOENT);

	/* Send mbuf */
	error = trunk_enqueue(tp->tp_ifp, m);

	/* Get next active port */
	tp_next = trunk_link_active(tr, SLIST_NEXT(tp, tp_entries));
	tr->tr_psc = (caddr_t)tp_next;

	return (error);
}

static struct mbuf *
trunk_rr_input(struct trunk_softc *tr, struct trunk_port *tp, struct mbuf *m)
{
	struct ifnet *ifp = tr->tr_ifp;

	/* Just pass in the packet to our trunk device */
	m->m_pkthdr.rcvif = ifp;

	return (m);
}

/*
 * Active failover
 */

static int
trunk_fail_attach(struct trunk_softc *tr)
{
	tr->tr_detach = trunk_fail_detach;
	tr->tr_start = trunk_fail_start;
	tr->tr_input = trunk_fail_input;
	tr->tr_port_create = NULL;
	tr->tr_port_destroy = NULL;

	return (0);
}

static int
trunk_fail_detach(struct trunk_softc *tr)
{
	return (0);
}

static int
trunk_fail_start(struct trunk_softc *tr, struct mbuf *m)
{
	struct trunk_port *tp;

	/* Use the master port if active or the next available port */
	if ((tp = trunk_link_active(tr, tr->tr_primary)) == NULL)
		return (ENOENT);

	/* Send mbuf */
	return (trunk_enqueue(tp->tp_ifp, m));
}

static struct mbuf *
trunk_fail_input(struct trunk_softc *tr, struct trunk_port *tp, struct mbuf *m)
{
	struct ifnet *ifp = tr->tr_ifp;
	struct trunk_port *tmp_tp;

	if (tp == tr->tr_primary) {
		m->m_pkthdr.rcvif = ifp;
		return (m);
	}

	if (tr->tr_primary->tp_link_state == LINK_STATE_DOWN) {
		tmp_tp = trunk_link_active(tr, NULL);
		/*
		 * If tmp_tp is null, we've recieved a packet when all
		 * our links are down. Weird, but process it anyways.
		 */
		if ((tmp_tp == NULL || tmp_tp == tp)) {
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
trunk_lb_attach(struct trunk_softc *tr)
{
	struct trunk_port *tp;
	struct trunk_lb *lb;

	if ((lb = (struct trunk_lb *)malloc(sizeof(struct trunk_lb),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	tr->tr_detach = trunk_lb_detach;
	tr->tr_start = trunk_lb_start;
	tr->tr_input = trunk_lb_input;
	tr->tr_port_create = trunk_lb_port_create;
	tr->tr_port_destroy = trunk_lb_port_destroy;
	tr->tr_capabilities = IFCAP_TRUNK_FULLDUPLEX;

	lb->lb_key = arc4random();
	tr->tr_psc = (caddr_t)lb;

	SLIST_FOREACH(tp, &tr->tr_ports, tp_entries)
		trunk_lb_port_create(tp);

	return (0);
}

static int
trunk_lb_detach(struct trunk_softc *tr)
{
	struct trunk_lb *lb = (struct trunk_lb *)tr->tr_psc;
	if (lb != NULL)
		free(lb, M_DEVBUF);
	return (0);
}

static int
trunk_lb_porttable(struct trunk_softc *tr, struct trunk_port *tp)
{
	struct trunk_lb *lb = (struct trunk_lb *)tr->tr_psc;
	struct trunk_port *tp_next;
	int i = 0;

	bzero(&lb->lb_ports, sizeof(lb->lb_ports));
	SLIST_FOREACH(tp_next, &tr->tr_ports, tp_entries) {
		if (tp_next == tp)
			continue;
		if (i >= TRUNK_MAX_PORTS)
			return (EINVAL);
		if (tr->tr_ifflags & IFF_DEBUG)
			printf("%s: port %s at index %d\n",
			    tr->tr_ifname, tp_next->tp_ifname, i);
		lb->lb_ports[i++] = tp_next;
	}

	return (0);
}

static int
trunk_lb_port_create(struct trunk_port *tp)
{
	struct trunk_softc *tr = tp->tp_trunk;
	return (trunk_lb_porttable(tr, NULL));
}

static void
trunk_lb_port_destroy(struct trunk_port *tp)
{
	struct trunk_softc *tr = tp->tp_trunk;
	trunk_lb_porttable(tr, tp);
}

static int
trunk_lb_start(struct trunk_softc *tr, struct mbuf *m)
{
	struct trunk_lb *lb = (struct trunk_lb *)tr->tr_psc;
	struct trunk_port *tp = NULL;
	uint32_t p = 0;
	int idx;

	p = trunk_hashmbuf(m, lb->lb_key);
	if ((idx = p % tr->tr_count) >= TRUNK_MAX_PORTS)
		return (EINVAL);
	tp = lb->lb_ports[idx];

	/*
	 * Check the port's link state. This will return the next active
	 * port if the link is down or the port is NULL.
	 */
	if ((tp = trunk_link_active(tr, tp)) == NULL)
		return (ENOENT);

	/* Send mbuf */
	return (trunk_enqueue(tp->tp_ifp, m));
}

static struct mbuf *
trunk_lb_input(struct trunk_softc *tr, struct trunk_port *tp, struct mbuf *m)
{
	struct ifnet *ifp = tr->tr_ifp;

	/* Just pass in the packet to our trunk device */
	m->m_pkthdr.rcvif = ifp;

	return (m);
}

/*
 * 802.3ad LACP
 */

static int
trunk_lacp_attach(struct trunk_softc *tr)
{
	struct trunk_port *tp;
	int error;

	tr->tr_detach = trunk_lacp_detach;
	tr->tr_port_create = lacp_port_create;
	tr->tr_port_destroy = lacp_port_destroy;
	tr->tr_linkstate = lacp_linkstate;
	tr->tr_start = trunk_lacp_start;
	tr->tr_input = trunk_lacp_input;
	tr->tr_init = lacp_init;
	tr->tr_stop = lacp_stop;
	tr->tr_lladdr = trunk_lacp_lladdr;

	error = lacp_attach(tr);
	if (error)
		return (error);

	SLIST_FOREACH(tp, &tr->tr_ports, tp_entries)
		lacp_port_create(tp);

	return (error);
}

static int
trunk_lacp_detach(struct trunk_softc *tr)
{
	struct trunk_port *tp;
	int error;

	SLIST_FOREACH(tp, &tr->tr_ports, tp_entries)
		lacp_port_destroy(tp);

	/* unlocking is safe here */
	TRUNK_UNLOCK(tr);
	error = lacp_detach(tr);
	TRUNK_LOCK(tr);

	return (error);
}

static void
trunk_lacp_lladdr(struct trunk_softc *tr)
{
	struct trunk_port *tp;

	/* purge all the lacp ports */
	SLIST_FOREACH(tp, &tr->tr_ports, tp_entries)
		lacp_port_destroy(tp);

	/* add them back in */
	SLIST_FOREACH(tp, &tr->tr_ports, tp_entries)
		lacp_port_create(tp);
}

static int
trunk_lacp_start(struct trunk_softc *tr, struct mbuf *m)
{
	struct trunk_port *tp;

	tp = lacp_select_tx_port(tr, m);
	if (tp == NULL)
		return (EBUSY);

	/* Send mbuf */
	return (trunk_enqueue(tp->tp_ifp, m));
}

static struct mbuf *
trunk_lacp_input(struct trunk_softc *tr, struct trunk_port *tp, struct mbuf *m)
{
	struct ifnet *ifp = tr->tr_ifp;
	struct ether_header *eh;
	u_short etype;
	uint8_t subtype;

	eh = mtod(m, struct ether_header *);
	etype = ntohs(eh->ether_type);

	/* Tap off LACP control messages */
	if (etype == ETHERTYPE_SLOW) {
		if (m->m_pkthdr.len < sizeof(*eh) + sizeof(subtype)) {
			m_freem(m);
			return (NULL);
		}

		m_copydata(m, sizeof(*eh), sizeof(subtype), &subtype);
		switch (subtype) {
			case SLOWPROTOCOLS_SUBTYPE_LACP:
				lacp_input(tp, m);
				break;

			case SLOWPROTOCOLS_SUBTYPE_MARKER:
				lacp_marker_input(tp, m);
				break;

			default:
				/* Unknown LACP packet type */
				m_freem(m);
				break;
		}
		return (NULL);
	}

	/*
	 * If the port is not collecting or not in the active aggregator then
	 * free and return.
	 */
	if ((tp->tp_flags & TRUNK_PORT_COLLECTING) == 0 ||
	    lacp_port_isactive(tp) == 0) {
		m_freem(m);
		return (NULL);
	}

	m->m_pkthdr.rcvif = ifp;
	return (m);
}
