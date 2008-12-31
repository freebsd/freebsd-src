/*-
 *
 * Copyright (c) 1999-2001, Vitaly V Belekhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $FreeBSD: src/sys/netgraph/ng_eiface.c,v 1.39.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_eiface.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if_arp.h>

static const struct ng_cmdlist ng_eiface_cmdlist[] = {
	{
	  NGM_EIFACE_COOKIE,
	  NGM_EIFACE_GET_IFNAME,
	  "getifname",
	  NULL,
	  &ng_parse_string_type
	},
	{
	  NGM_EIFACE_COOKIE,
	  NGM_EIFACE_SET,
	  "set",
	  &ng_parse_enaddr_type,
	  NULL
	},
	{ 0 }
};

/* Node private data */
struct ng_eiface_private {
	struct ifnet	*ifp;		/* per-interface network data */
	int		unit;		/* Interface unit number */
	node_p		node;		/* Our netgraph node */
	hook_p		ether;		/* Hook for ethernet stream */
};
typedef struct ng_eiface_private *priv_p;

/* Interface methods */
static void	ng_eiface_init(void *xsc);
static void	ng_eiface_start(struct ifnet *ifp);
static int	ng_eiface_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
#ifdef DEBUG
static void	ng_eiface_print_ioctl(struct ifnet *ifp, int cmd, caddr_t data);
#endif

/* Netgraph methods */
static int		ng_eiface_mod_event(module_t, int, void *);
static ng_constructor_t	ng_eiface_constructor;
static ng_rcvmsg_t	ng_eiface_rcvmsg;
static ng_shutdown_t	ng_eiface_rmnode;
static ng_newhook_t	ng_eiface_newhook;
static ng_rcvdata_t	ng_eiface_rcvdata;
static ng_disconnect_t	ng_eiface_disconnect;

/* Node type descriptor */
static struct ng_type typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_EIFACE_NODE_TYPE,
	.mod_event =	ng_eiface_mod_event,
	.constructor =	ng_eiface_constructor,
	.rcvmsg =	ng_eiface_rcvmsg,
	.shutdown =	ng_eiface_rmnode,
	.newhook =	ng_eiface_newhook,
	.rcvdata =	ng_eiface_rcvdata,
	.disconnect =	ng_eiface_disconnect,
	.cmdlist =	ng_eiface_cmdlist
};
NETGRAPH_INIT(eiface, &typestruct);

static struct unrhdr	*ng_eiface_unit;

/************************************************************************
			INTERFACE STUFF
 ************************************************************************/

/*
 * Process an ioctl for the virtual interface
 */
static int
ng_eiface_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ifreq *const ifr = (struct ifreq *)data;
	int s, error = 0;

#ifdef DEBUG
	ng_eiface_print_ioctl(ifp, command, data);
#endif
	s = splimp();
	switch (command) {

	/* These two are mostly handled at a higher layer */
	case SIOCSIFADDR:
		error = ether_ioctl(ifp, command, data);
		break;
	case SIOCGIFADDR:
		break;

	/* Set flags */
	case SIOCSIFFLAGS:
		/*
		 * If the interface is marked up and stopped, then start it.
		 * If it is marked down and running, then stop it.
		 */
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				ifp->if_drv_flags &= ~(IFF_DRV_OACTIVE);
				ifp->if_drv_flags |= IFF_DRV_RUNNING;
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				ifp->if_drv_flags &= ~(IFF_DRV_RUNNING |
				    IFF_DRV_OACTIVE);
		}
		break;

	/* Set the interface MTU */
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > NG_EIFACE_MTU_MAX ||
		    ifr->ifr_mtu < NG_EIFACE_MTU_MIN)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	/* Stuff that's not supported */
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = 0;
		break;
	case SIOCSIFPHYS:
		error = EOPNOTSUPP;
		break;

	default:
		error = EINVAL;
		break;
	}
	splx(s);
	return (error);
}

static void
ng_eiface_init(void *xsc)
{
	priv_p sc = xsc;
	struct ifnet *ifp = sc->ifp;
	int s;

	s = splimp();

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	splx(s);
}

/*
 * We simply relay the packet to the "ether" hook, if it is connected.
 * We have been through the netgraph locking and are guaranteed to
 * be the only code running in this node at this time.
 */
static void
ng_eiface_start2(node_p node, hook_p hook, void *arg1, int arg2)
{
	struct ifnet *ifp = arg1;
	const priv_p priv = (priv_p)ifp->if_softc;
	int error = 0;
	struct mbuf *m;

	/* Check interface flags */

	if (!((ifp->if_flags & IFF_UP) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING)))
		return;

	for (;;) {
		/*
		 * Grab a packet to transmit.
		 */
		IF_DEQUEUE(&ifp->if_snd, m);

		/* If there's nothing to send, break. */
		if (m == NULL)
			break;

		/*
		 * Berkeley packet filter.
		 * Pass packet to bpf if there is a listener.
		 * XXX is this safe? locking?
		 */
		BPF_MTAP(ifp, m);

		if (ifp->if_flags & IFF_MONITOR) {
			ifp->if_ipackets++;
			m_freem(m);
			continue;
		}

		/*
		 * Send packet; if hook is not connected, mbuf will get
		 * freed.
		 */
		NG_SEND_DATA_ONLY(error, priv->ether, m);

		/* Update stats */
		if (error == 0)
			ifp->if_opackets++;
		else
			ifp->if_oerrors++;
	}

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	return;
}

/*
 * This routine is called to deliver a packet out the interface.
 * We simply queue the netgraph version to be called when netgraph locking
 * allows it to happen.
 * Until we know what the rest of the networking code is doing for
 * locking, we don't know how we will interact with it.
 * Take comfort from the fact that the ifnet struct is part of our
 * private info and can't go away while we are queued.
 * [Though we don't know it is still there now....]
 * it is possible we don't gain anything from this because
 * we would like to get the mbuf and queue it as data
 * somehow, but we can't and if we did would we solve anything?
 */
static void
ng_eiface_start(struct ifnet *ifp)
{

	const priv_p priv = (priv_p)ifp->if_softc;

	/* Don't do anything if output is active */
	if (ifp->if_drv_flags & IFF_DRV_OACTIVE)
		return;

	ifp->if_drv_flags |= IFF_DRV_OACTIVE;

	if (ng_send_fn(priv->node, NULL, &ng_eiface_start2, ifp, 0) != 0)
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
}

#ifdef DEBUG
/*
 * Display an ioctl to the virtual interface
 */

static void
ng_eiface_print_ioctl(struct ifnet *ifp, int command, caddr_t data)
{
	char *str;

	switch (command & IOC_DIRMASK) {
	case IOC_VOID:
		str = "IO";
		break;
	case IOC_OUT:
		str = "IOR";
		break;
	case IOC_IN:
		str = "IOW";
		break;
	case IOC_INOUT:
		str = "IORW";
		break;
	default:
		str = "IO??";
	}
	log(LOG_DEBUG, "%s: %s('%c', %d, char[%d])\n",
	    ifp->if_xname,
	    str,
	    IOCGROUP(command),
	    command & 0xff,
	    IOCPARM_LEN(command));
}
#endif /* DEBUG */

/************************************************************************
			NETGRAPH NODE STUFF
 ************************************************************************/

/*
 * Constructor for a node
 */
static int
ng_eiface_constructor(node_p node)
{
	struct ifnet *ifp;
	priv_p priv;
	u_char eaddr[6] = {0,0,0,0,0,0};

	/* Allocate node and interface private structures */
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH, M_NOWAIT | M_ZERO);
	if (priv == NULL)
		return (ENOMEM);

	ifp = priv->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		free(priv, M_NETGRAPH);
		return (ENOSPC);
	}

	/* Link them together */
	ifp->if_softc = priv;

	/* Get an interface unit number */
	priv->unit = alloc_unr(ng_eiface_unit);

	/* Link together node and private info */
	NG_NODE_SET_PRIVATE(node, priv);
	priv->node = node;

	/* Initialize interface structure */
	if_initname(ifp, NG_EIFACE_EIFACE_NAME, priv->unit);
	ifp->if_init = ng_eiface_init;
	ifp->if_output = ether_output;
	ifp->if_start = ng_eiface_start;
	ifp->if_ioctl = ng_eiface_ioctl;
	ifp->if_watchdog = NULL;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	ifp->if_flags = (IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST);

#if 0
	/* Give this node name */
	bzero(ifname, sizeof(ifname));
	sprintf(ifname, "if%s", ifp->if_xname);
	(void)ng_name_node(node, ifname);
#endif

	/* Attach the interface */
	ether_ifattach(ifp, eaddr);

	/* Done */
	return (0);
}

/*
 * Give our ok for a hook to be added
 */
static int
ng_eiface_newhook(node_p node, hook_p hook, const char *name)
{
	priv_p priv = NG_NODE_PRIVATE(node);
	struct ifnet *ifp = priv->ifp;

	if (strcmp(name, NG_EIFACE_HOOK_ETHER))
		return (EPFNOSUPPORT);
	if (priv->ether != NULL)
		return (EISCONN);
	priv->ether = hook;
	NG_HOOK_SET_PRIVATE(hook, &priv->ether);

	if_link_state_change(ifp, LINK_STATE_UP);

	return (0);
}

/*
 * Receive a control message
 */
static int
ng_eiface_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct ifnet *const ifp = priv->ifp;
	struct ng_mesg *resp = NULL;
	int error = 0;
	struct ng_mesg *msg;

	NGI_GET_MSG(item, msg);
	switch (msg->header.typecookie) {
	case NGM_EIFACE_COOKIE:
		switch (msg->header.cmd) {

		case NGM_EIFACE_SET:
		    {
			if (msg->header.arglen != ETHER_ADDR_LEN) {
				error = EINVAL;
				break;
			}
			error = if_setlladdr(priv->ifp,
			    (u_char *)msg->data, ETHER_ADDR_LEN);
			break;
		    }

		case NGM_EIFACE_GET_IFNAME:
			NG_MKRESPONSE(resp, msg, IFNAMSIZ, M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			strlcpy(resp->data, ifp->if_xname, IFNAMSIZ);
			break;

		case NGM_EIFACE_GET_IFADDRS:
		    {
			struct ifaddr *ifa;
			caddr_t ptr;
			int buflen;

#define SA_SIZE(s)	((s)->sa_len<sizeof(*(s))? sizeof(*(s)):(s)->sa_len)

			/* Determine size of response and allocate it */
			buflen = 0;
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
				buflen += SA_SIZE(ifa->ifa_addr);
			NG_MKRESPONSE(resp, msg, buflen, M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}

			/* Add addresses */
			ptr = resp->data;
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				const int len = SA_SIZE(ifa->ifa_addr);

				if (buflen < len) {
					log(LOG_ERR, "%s: len changed?\n",
					    ifp->if_xname);
					break;
				}
				bcopy(ifa->ifa_addr, ptr, len);
				ptr += len;
				buflen -= len;
			}
			break;
#undef SA_SIZE
		    }

		default:
			error = EINVAL;
			break;
		} /* end of inner switch() */
		break;
	case NGM_FLOW_COOKIE:
		switch (msg->header.cmd) {
		case NGM_LINK_IS_UP:
			if_link_state_change(ifp, LINK_STATE_UP);
			break;
		case NGM_LINK_IS_DOWN:
			if_link_state_change(ifp, LINK_STATE_DOWN);
			break;
		default:
			break;
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);
	return (error);
}

/*
 * Receive data from a hook. Pass the packet to the ether_input routine.
 */
static int
ng_eiface_rcvdata(hook_p hook, item_p item)
{
	const priv_p priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct ifnet *const ifp = priv->ifp;
	struct mbuf *m;

	NGI_GET_M(item, m);
	NG_FREE_ITEM(item);

	if (!((ifp->if_flags & IFF_UP) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING))) {
		NG_FREE_M(m);
		return (ENETDOWN);
	}

	if (m->m_len < ETHER_HDR_LEN) {
		m = m_pullup(m, ETHER_HDR_LEN);
		if (m == NULL)
			return (EINVAL);
	}

	/* Note receiving interface */
	m->m_pkthdr.rcvif = ifp;

	/* Update interface stats */
	ifp->if_ipackets++;

	(*ifp->if_input)(ifp, m);

	/* Done */
	return (0);
}

/*
 * Shutdown processing.
 */
static int
ng_eiface_rmnode(node_p node)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct ifnet *const ifp = priv->ifp;

	ether_ifdetach(ifp);
	if_free(ifp);
	free_unr(ng_eiface_unit, priv->unit);
	FREE(priv, M_NETGRAPH);
	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(node);
	return (0);
}

/*
 * Hook disconnection
 */
static int
ng_eiface_disconnect(hook_p hook)
{
	const priv_p priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));

	priv->ether = NULL;
	return (0);
}

/*
 * Handle loading and unloading for this node type.
 */
static int
ng_eiface_mod_event(module_t mod, int event, void *data)
{
	int error = 0;

	switch (event) {
	case MOD_LOAD:
		ng_eiface_unit = new_unrhdr(0, 0xffff, NULL);
		break;
	case MOD_UNLOAD:
		delete_unrhdr(ng_eiface_unit);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}
