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
 * $FreeBSD$
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
#include <net/if_dl.h>
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
	  NGM_EIFACE_SET,
	  "set",
	  &ng_parse_enaddr_type,
	  NULL
	},
	{ 0 }
};

/* Node private data */
struct ng_eiface_private {
	struct arpcom	arpcom;		/* per-interface network data */
	struct ifnet	*ifp;		/* This interface */
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
static ng_constructor_t	ng_eiface_constructor;
static ng_rcvmsg_t	ng_eiface_rcvmsg;
static ng_shutdown_t	ng_eiface_rmnode;
static ng_newhook_t	ng_eiface_newhook;
static ng_rcvdata_t	ng_eiface_rcvdata;
static ng_connect_t	ng_eiface_connect;
static ng_disconnect_t	ng_eiface_disconnect;

/* Node type descriptor */
static struct ng_type typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_EIFACE_NODE_TYPE,
	.constructor =	ng_eiface_constructor,
	.rcvmsg =	ng_eiface_rcvmsg,
	.shutdown =	ng_eiface_rmnode,
	.newhook =	ng_eiface_newhook,
	.connect =	ng_eiface_connect,
	.rcvdata =	ng_eiface_rcvdata,
	.rcvdataq =	ng_eiface_rcvdata,
	.disconnect =	ng_eiface_disconnect,
	.cmdlist =	ng_eiface_cmdlist
};
NETGRAPH_INIT(eiface, &typestruct);

static char ng_eiface_ifname[] = NG_EIFACE_EIFACE_NAME;
static int ng_eiface_next_unit;

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
		if (ifr->ifr_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING)) {
				ifp->if_flags &= ~(IFF_OACTIVE);
				ifp->if_flags |= IFF_RUNNING;
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
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

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);
}

/*
 * This routine is called to deliver a packet out the interface.
 * We simply relay the packet to
 * the ether hook, if it is connected.
 */
static void
ng_eiface_start(struct ifnet *ifp)
{
	const priv_p priv = (priv_p)ifp->if_softc;
	meta_p meta = NULL;
	int len, error = 0;
	struct mbuf *m;

	/* Check interface flags */
	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) != (IFF_UP | IFF_RUNNING))
		return;

	/* Don't do anything if output is active */
	if (ifp->if_flags & IFF_OACTIVE)
		return;

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Grab a packet to transmit.
	 */
	IF_DEQUEUE(&ifp->if_snd, m);

	/* If there's nothing to send, return. */
	if (m == NULL) {
		ifp->if_flags &= ~IFF_OACTIVE;
		return;
	}

	/*
	 * Berkeley packet filter.
	 * Pass packet to bpf if there is a listener.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp, m);

	/* Copy length before the mbuf gets invalidated */
	len = m->m_pkthdr.len;

	/*
	 * Send packet; if hook is not connected, mbuf will get
	 * freed.
	 */
	NG_SEND_DATA(error, priv->ether, m, meta);

	/* Update stats */
	if (error == 0) {
		ifp->if_obytes += len;
		ifp->if_opackets++;
	}

	ifp->if_flags &= ~IFF_OACTIVE;

	return;
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
	log(LOG_DEBUG, "%s%d: %s('%c', %d, char[%d])\n",
	    ifp->if_name, ifp->if_unit,
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
ng_eiface_constructor(node_p *nodep)
{
	struct ifnet *ifp;
	node_p node;
	priv_p priv;
	int error = 0;

	/* Allocate node and interface private structures */
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH, M_WAITOK);
	if (priv == NULL)
		return (ENOMEM);
	bzero(priv, sizeof(*priv));

	ifp = &(priv->arpcom.ac_if);

	/* Link them together */
	ifp->if_softc = priv;
	priv->ifp = ifp;

	/* Call generic node constructor */
	if ((error = ng_make_node_common(&typestruct, nodep))) {
		FREE(priv, M_NETGRAPH);
		return (error);
	}
	node = *nodep;

	/* Link together node and private info */
	NG_NODE_SET_PRIVATE(node, priv);
	priv->node = node;

	/* Initialize interface structure */
	ifp->if_name = ng_eiface_ifname;
	ifp->if_unit = ng_eiface_next_unit++;
	ifp->if_init = ng_eiface_init;
	ifp->if_output = ether_output;
	ifp->if_start = ng_eiface_start;
	ifp->if_ioctl = ng_eiface_ioctl;
	ifp->if_watchdog = NULL;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	ifp->if_flags = (IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST);

	TAILQ_INIT(&ifp->if_addrhead);

#if 0
	/* Give this node name */
	bzero(ifname, sizeof(ifname));
	sprintf(ifname, "if%s%d", ifp->if_name, ifp->if_unit);
	(void)ng_name_node(node, ifname);
#endif

	/* Attach the interface */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);

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

	if (strcmp(name, NG_EIFACE_HOOK_ETHER))
		return (EPFNOSUPPORT);
	if (priv->ether != NULL)
		return (EISCONN);
	priv->ether = hook;
	NG_HOOK_SET_PRIVATE(hook, &priv->ether);

	return (0);
}

/*
 * Receive a control message
 */
static int
ng_eiface_rcvmsg(node_p node, struct ng_mesg *msg,
    const char *retaddr, struct ng_mesg **rptr)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct ifnet *const ifp = priv->ifp;
	struct ng_mesg *resp = NULL;
	int error = 0;

	switch (msg->header.typecookie) {
	case NGM_EIFACE_COOKIE:
		switch (msg->header.cmd) {

		case NGM_EIFACE_SET:
		    {
			struct ether_addr *eaddr;
			struct ifaddr *ifa;
			struct sockaddr_dl *sdl;

			if (msg->header.arglen != sizeof(struct ether_addr)) {
				error = EINVAL;
				break;
			}
			eaddr = (struct ether_addr *)(msg->data);
			bcopy(eaddr, priv->arpcom.ac_enaddr, ETHER_ADDR_LEN);

			/* And put it in the ifaddr list */
#define	IFP2AC(ifp) ((struct arpcom *)(ifp))
			TAILQ_FOREACH(ifa, &(ifp->if_addrhead), ifa_link) {
				sdl = (struct sockaddr_dl *)ifa->ifa_addr;
				if (sdl->sdl_type == IFT_ETHER) {
					bcopy((IFP2AC(ifp))->ac_enaddr,
						LLADDR(sdl), ifp->if_addrlen);
					break;
				}
			}
			break;
		    }

		case NGM_EIFACE_GET_IFNAME:
		    {
			struct ng_eiface_ifname *arg;

			NG_MKRESPONSE(resp, msg, sizeof(*arg), M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			arg = (struct ng_eiface_ifname *)resp->data;
			sprintf(arg->ngif_name,
			    "%s%d", ifp->if_name, ifp->if_unit);
			break;
		    }

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
					log(LOG_ERR, "%s%d: len changed?\n",
					    ifp->if_name, ifp->if_unit);
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
	default:
		error = EINVAL;
		break;
	}
	NG_RESPOND_MSG(error, node, retaddr, resp, rptr);
	NG_FREE_MSG(msg);
	return (error);
}

/*
 * Receive data from a hook. Pass the packet to the ether_input routine.
 */
static int
ng_eiface_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	const priv_p priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct ifnet *const ifp = priv->ifp;
	int s;
	struct ether_header *eh;
	u_short ether_type;

	NG_FREE_META(meta);

	if (m == NULL) {
		printf("ng_eiface: mbuf is null.\n");
		return (EINVAL);
	}

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) !=
	    (IFF_UP | IFF_RUNNING)) {
		NG_FREE_M(m);
		return (ENETDOWN);
	}

	/* Note receiving interface */
	m->m_pkthdr.rcvif = ifp;

	/* Update interface stats */
	ifp->if_ipackets++;

	eh = mtod(m, struct ether_header *);
	ether_type = ntohs(eh->ether_type);

	s = splimp();
	m->m_pkthdr.len -= sizeof(*eh);
	m->m_len -= sizeof(*eh);
	if (m->m_len)
		m->m_data += sizeof(*eh);
	else if (ether_type == ETHERTYPE_ARP) {
		m->m_len = m->m_next->m_len;
		m->m_data = m->m_next->m_data;
	}
	splx(s);

	ether_input(ifp, eh, m);

	/* Done */
	return (0);
}

/*
 * Because the BSD networking code doesn't support the removal of
 * networking interfaces, iface nodes (once created) are persistent.
 * So this method breaks all connections and marks the interface
 * down, but does not remove the node.
 */
static int
ng_eiface_rmnode(node_p node)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct ifnet *const ifp = priv->ifp;

	ng_cutlinks(node);
	node->flags &= ~NG_INVALID;
	ifp->if_flags &= ~(IFF_UP | IFF_RUNNING | IFF_OACTIVE);
	return (0);
}


/*
 * This is called once we've already connected a new hook to the other node.
 * It gives us a chance to balk at the last minute.
 */
static int
ng_eiface_connect(hook_p hook)
{
	/* be really amiable and just say "YUP that's OK by me! " */
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
