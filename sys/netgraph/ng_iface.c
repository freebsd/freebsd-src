
/*
 * ng_iface.c
 *
 * Copyright (c) 1996-1999 Whistle Communications, Inc.
 * All rights reserved.
 * 
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Whistle Communications;
 * provided, however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Whistle
 *    Communications, Inc. trademarks, including the mark "WHISTLE
 *    COMMUNICATIONS" on advertising, endorsements, or otherwise except as
 *    such appears in the above copyright notice or in the software.
 * 
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS", AND
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE,
 * INCLUDING WITHOUT LIMITATION, ANY AND ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANTEE, OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE USE OF THIS
 * SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.
 * IN NO EVENT SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS IS ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 *
 * $FreeBSD$
 * $Whistle: ng_iface.c,v 1.33 1999/11/01 09:24:51 julian Exp $
 */

/*
 * This node is also a system networking interface. It has
 * a hook for each protocol (IP, AppleTalk, IPX, etc). Packets
 * are simply relayed between the interface and the hooks.
 *
 * Interfaces are named ng0, ng1, .... FreeBSD does not support
 * the removal of interfaces, so iface nodes are persistent.
 *
 * This node also includes Berkeley packet filter support.
 */

#include "opt_inet.h"
#include "opt_atalk.h"
#include "opt_ipx.h"

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

#include <netinet/in.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_iface.h>
#include <netgraph/ng_cisco.h>

#ifdef INET
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/in_var.h>
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include "bpfilter.h"

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

/* This struct describes one address family */
struct iffam {
	char   *hookname;	/* Name for hook */
	u_char  af;		/* Family number */
	u_char  netisr;		/* or NETISR_NONE */
	union {
		void	*_dummy;			/* avoid warning */
		struct	ifqueue *inq;			/* if netisr */
		void	(*input)(struct mbuf *m);	/* if direct input */
	}       u;
};
typedef const struct iffam *iffam_p;

#define NETISR_NONE	0xff

/* List of address families supported by our interface. Each address
   family has a way to input packets to it, either by calling a function
   directly (such as ip_input()) or by adding the packet to a queue and
   setting a NETISR bit. */
const static struct iffam gFamilies[] = {
#ifdef INET
	{
		NG_IFACE_HOOK_INET,
		AF_INET,
		NETISR_NONE,
		{ ip_input }
	},
#endif
#ifdef NETATALK
	{
		NG_IFACE_HOOK_ATALK,
		AF_APPLETALK,
		NETISR_ATALK,
		{ &atintrq2 }
	},
#endif
#ifdef IPX
	{
		NG_IFACE_HOOK_IPX,
		AF_IPX,
		NETISR_IPX,
		{ &ipxintrq }
	},
#endif
#ifdef NS
	{
		NG_IFACE_HOOK_NS,
		AF_NS,
		NETISR_NS,
		{ &nsintrq }
	},
#endif
};
#define NUM_FAMILIES		(sizeof(gFamilies) / sizeof(*gFamilies))

/* Node private data */
struct ng_iface_private {
	struct	ifnet *ifp;		/* This interface */
	node_p	node;			/* Our netgraph node */
	hook_p	hooks[NUM_FAMILIES];	/* Hook for each address family */
	struct	private *next;		/* When hung on the free list */
};
typedef struct ng_iface_private *priv_p;

/* Interface methods */
static void	ng_iface_start(struct ifnet *ifp);
static int	ng_iface_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static int	ng_iface_output(struct ifnet *ifp, struct mbuf *m0,
		struct sockaddr *dst, struct rtentry *rt0);
#if NBPFILTER > 0
static void	ng_iface_bpftap(struct ifnet *ifp, struct mbuf *m, u_int af);
#endif
#ifdef DEBUG
static void	ng_iface_print_ioctl(struct ifnet *ifp, int cmd, caddr_t data);
#endif

/* Netgraph methods */
static ng_constructor_t	ng_iface_constructor;
static ng_rcvmsg_t	ng_iface_rcvmsg;
static ng_shutdown_t	ng_iface_rmnode;
static ng_newhook_t	ng_iface_newhook;
static ng_rcvdata_t	ng_iface_rcvdata;
static ng_disconnect_t	ng_iface_disconnect;

/* Helper stuff */
static iffam_p	get_iffam_from_af(int af);
static iffam_p	get_iffam_from_hook(priv_p priv, hook_p hook);
static iffam_p	get_iffam_from_name(const char *name);
static hook_p  *get_hook_from_iffam(priv_p priv, iffam_p iffam);

/* Node type descriptor */
static struct ng_type typestruct = {
	NG_VERSION,
	NG_IFACE_NODE_TYPE,
	NULL,
	ng_iface_constructor,
	ng_iface_rcvmsg,
	ng_iface_rmnode,
	ng_iface_newhook,
	NULL,
	NULL,
	ng_iface_rcvdata,
	ng_iface_rcvdata,
	ng_iface_disconnect,
	NULL
};
NETGRAPH_INIT(iface, &typestruct);

static char ng_iface_ifname[] = NG_IFACE_IFACE_NAME;
static int ng_iface_next_unit;

/************************************************************************
			HELPER STUFF
 ************************************************************************/

/*
 * Get the family descriptor from the family ID
 */
static __inline__ iffam_p
get_iffam_from_af(int af)
{
	iffam_p iffam;
	int k;

	for (k = 0; k < NUM_FAMILIES; k++) {
		iffam = &gFamilies[k];
		if (iffam->af == af)
			return (iffam);
	}
	return (NULL);
}

/*
 * Get the family descriptor from the hook
 */
static __inline__ iffam_p
get_iffam_from_hook(priv_p priv, hook_p hook)
{
	int k;

	for (k = 0; k < NUM_FAMILIES; k++)
		if (priv->hooks[k] == hook)
			return (&gFamilies[k]);
	return (NULL);
}

/*
 * Get the hook from the iffam descriptor
 */

static __inline__ hook_p *
get_hook_from_iffam(priv_p priv, iffam_p iffam)
{
	return (&priv->hooks[iffam - gFamilies]);
}

/*
 * Get the iffam descriptor from the name
 */
static __inline__ iffam_p
get_iffam_from_name(const char *name)
{
	iffam_p iffam;
	int k;

	for (k = 0; k < NUM_FAMILIES; k++) {
		iffam = &gFamilies[k];
		if (!strcmp(iffam->hookname, name))
			return (iffam);
	}
	return (NULL);
}

/************************************************************************
			INTERFACE STUFF
 ************************************************************************/

/*
 * Process an ioctl for the virtual interface
 */
static int
ng_iface_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ifreq *const ifr = (struct ifreq *) data;
	int s, error = 0;

#ifdef DEBUG
	ng_iface_print_ioctl(ifp, command, data);
#endif
	s = splimp();
	switch (command) {

	/* These two are mostly handled at a higher layer */
	case SIOCSIFADDR:
		ifp->if_flags |= (IFF_UP | IFF_RUNNING);
		ifp->if_flags &= ~(IFF_OACTIVE);
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
		if (ifr->ifr_mtu > NG_IFACE_MTU_MAX
		    || ifr->ifr_mtu < NG_IFACE_MTU_MIN)
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
	(void) splx(s);
	return (error);
}

/*
 * This routine is called to deliver a packet out the interface.
 * We simply look at the address family and relay the packet to
 * the corresponding hook, if it exists and is connected.
 */

static int
ng_iface_output(struct ifnet *ifp, struct mbuf *m,
		struct sockaddr *dst, struct rtentry *rt0)
{
	const priv_p priv = (priv_p) ifp->if_softc;
	const iffam_p iffam = get_iffam_from_af(dst->sa_family);
	meta_p meta = NULL;
	int len, error = 0;

	/* Check interface flags */
	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		m_freem(m);
		return (ENETDOWN);
	}

	/* Berkeley packet filter */
#if NBPFILTER > 0
	ng_iface_bpftap(ifp, m, dst->sa_family);
#endif

	/* Check address family to determine hook (if known) */
	if (iffam == NULL) {
		m_freem(m);
		log(LOG_WARNING, "%s%d: can't handle af%d\n",
		       ifp->if_name, ifp->if_unit, dst->sa_family);
		return (EAFNOSUPPORT);
	}

	/* Copy length before the mbuf gets invalidated */
	len = m->m_pkthdr.len;

	/* Send packet; if hook is not connected, mbuf will get freed. */
	NG_SEND_DATA(error, *get_hook_from_iffam(priv, iffam), m, meta);

	/* Update stats */
	if (error == 0) {
		ifp->if_obytes += len;
		ifp->if_opackets++;
	}
	return (error);
}

/*
 * This routine should never be called
 */

static void
ng_iface_start(struct ifnet *ifp)
{
	printf("%s%d: %s called?", ifp->if_name, ifp->if_unit, __FUNCTION__);
}

#if NBPFILTER > 0
/*
 * Flash a packet by the BPF (requires prepending 4 byte AF header)
 * Note the phoney mbuf; this is OK because BPF treats it read-only.
 */
static void
ng_iface_bpftap(struct ifnet *ifp, struct mbuf *m, u_int af)
{
	struct mbuf m2;

	if (af == AF_UNSPEC) {
		af = *(mtod(m, int *));
		m->m_len -= sizeof(int);
		m->m_pkthdr.len -= sizeof(int);
		m->m_data += sizeof(int);
	}
	if (!ifp->if_bpf)
		return;
	m2.m_next = m;
	m2.m_len = 4;
	m2.m_data = (char *) &af;
	bpf_mtap(ifp, &m2);
}
#endif /* NBPFILTER > 0 */

#ifdef DEBUG
/*
 * Display an ioctl to the virtual interface
 */

static void
ng_iface_print_ioctl(struct ifnet *ifp, int command, caddr_t data)
{
	char   *str;

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
ng_iface_constructor(node_p *nodep)
{
	char ifname[NG_IFACE_IFACE_NAME_MAX + 1];
	struct ifnet *ifp;
	node_p node;
	priv_p priv;
	int error = 0;

	/* Allocate node and interface private structures */
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH, M_WAITOK);
	if (priv == NULL)
		return (ENOMEM);
	bzero(priv, sizeof(*priv));
	MALLOC(ifp, struct ifnet *, sizeof(*ifp), M_NETGRAPH, M_WAITOK);
	if (ifp == NULL) {
		FREE(priv, M_NETGRAPH);
		return (ENOMEM);
	}
	bzero(ifp, sizeof(*ifp));

	/* Link them together */
	ifp->if_softc = priv;
	priv->ifp = ifp;

	/* Call generic node constructor */
	if ((error = ng_make_node_common(&typestruct, nodep))) {
		FREE(priv, M_NETGRAPH);
		FREE(ifp, M_NETGRAPH);
		return (error);
	}
	node = *nodep;

	/* Link together node and private info */
	node->private = priv;
	priv->node = node;

	/* Initialize interface structure */
	ifp->if_name = ng_iface_ifname;
	ifp->if_unit = ng_iface_next_unit++;
	ifp->if_output = ng_iface_output;
	ifp->if_start = ng_iface_start;
	ifp->if_ioctl = ng_iface_ioctl;
	ifp->if_watchdog = NULL;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	ifp->if_mtu = NG_IFACE_MTU_DEFAULT;
	ifp->if_flags = (IFF_SIMPLEX | IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST);
	ifp->if_type = IFT_PROPVIRTUAL;		/* XXX */
	ifp->if_addrlen = 0;			/* XXX */
	ifp->if_hdrlen = 0;			/* XXX */
	ifp->if_baudrate = 64000;		/* XXX */
	TAILQ_INIT(&ifp->if_addrhead);

	/* Give this node the same name as the interface (if possible) */
	bzero(ifname, sizeof(ifname));
	sprintf(ifname, "%s%d", ifp->if_name, ifp->if_unit);
	(void) ng_name_node(node, ifname);

	/* Attach the interface */
	if_attach(ifp);
#if NBPFILTER > 0
	bpfattach(ifp, DLT_NULL, sizeof(u_int));
#endif

	/* Done */
	return (0);
}

/*
 * Give our ok for a hook to be added
 */
static int
ng_iface_newhook(node_p node, hook_p hook, const char *name)
{
	const iffam_p iffam = get_iffam_from_name(name);
	hook_p *hookptr;

	if (iffam == NULL)
		return (EPFNOSUPPORT);
	hookptr = get_hook_from_iffam((priv_p) node->private, iffam);
	if (*hookptr != NULL)
		return (EISCONN);
	*hookptr = hook;
	return (0);
}

/*
 * Receive a control message
 */
static int
ng_iface_rcvmsg(node_p node, struct ng_mesg *msg,
		const char *retaddr, struct ng_mesg **rptr)
{
	const priv_p priv = node->private;
	struct ifnet *const ifp = priv->ifp;
	struct ng_mesg *resp = NULL;
	int error = 0;

	switch (msg->header.typecookie) {
	case NGM_IFACE_COOKIE:
		switch (msg->header.cmd) {
		case NGM_IFACE_GET_IFNAME:
		    {
			struct ng_iface_ifname *arg;

			NG_MKRESPONSE(resp, msg, sizeof(*arg), M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				break;
			}
			arg = (struct ng_iface_ifname *) resp->data;
			sprintf(arg->ngif_name,
			    "%s%d", ifp->if_name, ifp->if_unit);
			break;
		    }

		case NGM_IFACE_GET_IFADDRS:
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
		}
		break;
	case NGM_CISCO_COOKIE:
		switch (msg->header.cmd) {
		case NGM_CISCO_GET_IPADDR:	/* we understand this too */
		    {
			struct ifaddr *ifa;

			/* Return the first configured IP address */
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				struct in_addr *ips;

				if (ifa->ifa_addr->sa_family != AF_INET)
					continue;
				NG_MKRESPONSE(resp, msg,
				    2 * sizeof(*ips), M_NOWAIT);
				if (resp == NULL) {
					error = ENOMEM;
					break;
				}
				ips = (struct in_addr *) resp->data;
				ips[0] = ((struct sockaddr_in *)
						ifa->ifa_addr)->sin_addr;
				ips[1] = ((struct sockaddr_in *)
						ifa->ifa_netmask)->sin_addr;
				break;
			}

			/* No IP addresses on this interface? */
			if (ifa == NULL)
				error = EADDRNOTAVAIL;
			break;
		    }
		default:
			error = EINVAL;
			break;
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	if (rptr)
		*rptr = resp;
	else if (resp)
		FREE(resp, M_NETGRAPH);
	FREE(msg, M_NETGRAPH);
	return (error);
}

/*
 * Recive data from a hook. Pass the packet to the correct input routine.
 */
static int
ng_iface_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	const priv_p priv = hook->node->private;
	const iffam_p iffam = get_iffam_from_hook(priv, hook);
	struct ifnet *const ifp = priv->ifp;
	int s, error = 0;

	/* Sanity checks */
	KASSERT(iffam != NULL, ("%s: iffam", __FUNCTION__));
	KASSERT(m->m_flags & M_PKTHDR, ("%s: not pkthdr", __FUNCTION__));
	if (m == NULL)
		return (EINVAL);
	if ((ifp->if_flags & IFF_UP) == 0) {
		NG_FREE_DATA(m, meta);
		return (ENETDOWN);
	}

	/* Update interface stats */
	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;

	/* Note receiving interface */
	m->m_pkthdr.rcvif = ifp;

#if NBPFILTER > 0
	/* Berkeley packet filter */
	ng_iface_bpftap(ifp, m, iffam->af);
#endif

	/* Ignore any meta-data */
	NG_FREE_META(meta);

	/* Send packet, either by NETISR or use a direct input function */
	switch (iffam->netisr) {
	case NETISR_NONE:
		(*iffam->u.input)(m);
		break;
	default:
		s = splimp();
		schednetisr(iffam->netisr);
		if (IF_QFULL(iffam->u.inq)) {
			IF_DROP(iffam->u.inq);
			m_freem(m);
			error = ENOBUFS;
		} else
			IF_ENQUEUE(iffam->u.inq, m);
		splx(s);
		break;
	}

	/* Done */
	return (error);
}

/*
 * Because the BSD networking code doesn't support the removal of
 * networking interfaces, iface nodes (once created) are persistent.
 * So this method breaks all connections and marks the interface
 * down, but does not remove the node.
 */
static int
ng_iface_rmnode(node_p node)
{
	const priv_p priv = node->private;
	struct ifnet *const ifp = priv->ifp;

	ng_cutlinks(node);
	node->flags &= ~NG_INVALID;
	ifp->if_flags &= ~(IFF_UP | IFF_RUNNING | IFF_OACTIVE);
	return (0);
}

/*
 * Hook disconnection
 */
static int
ng_iface_disconnect(hook_p hook)
{
	const priv_p priv = hook->node->private;
	const iffam_p iffam = get_iffam_from_hook(priv, hook);

	if (iffam == NULL)
		panic(__FUNCTION__);
	*get_hook_from_iffam(priv, iffam) = NULL;
	return (0);
}

