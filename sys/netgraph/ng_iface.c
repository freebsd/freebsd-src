
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
 * Interfaces are named ng0, ng1, etc.  New nodes take the
 * first available interface name.
 *
 * This node also includes Berkeley packet filter support.
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
#include <sys/libkern.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/intrq.h>
#include <net/bpf.h>

#include <netinet/in.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_iface.h>
#include <netgraph/ng_cisco.h>

/* This struct describes one address family */
struct iffam {
	sa_family_t	family;		/* Address family */
	const char	*hookname;	/* Name for hook */
};
typedef const struct iffam *iffam_p;

/* List of address families supported by our interface */
const static struct iffam gFamilies[] = {
	{ AF_INET,	NG_IFACE_HOOK_INET	},
	{ AF_INET6,	NG_IFACE_HOOK_INET6	},
	{ AF_APPLETALK,	NG_IFACE_HOOK_ATALK	},
	{ AF_IPX,	NG_IFACE_HOOK_IPX	},
	{ AF_ATM,	NG_IFACE_HOOK_ATM	},
	{ AF_NATM,	NG_IFACE_HOOK_NATM	},
	{ AF_NS,	NG_IFACE_HOOK_NS	},
};
#define NUM_FAMILIES		(sizeof(gFamilies) / sizeof(*gFamilies))

/* Node private data */
struct ng_iface_private {
	struct	ifnet *ifp;		/* Our interface */
	int	unit;			/* Interface unit number */
	node_p	node;			/* Our netgraph node */
	hook_p	hooks[NUM_FAMILIES];	/* Hook for each address family */
};
typedef struct ng_iface_private *priv_p;

/* Interface methods */
static void	ng_iface_start(struct ifnet *ifp);
static int	ng_iface_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static int	ng_iface_output(struct ifnet *ifp, struct mbuf *m0,
			struct sockaddr *dst, struct rtentry *rt0);
static void	ng_iface_bpftap(struct ifnet *ifp,
			struct mbuf *m, sa_family_t family);
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
static iffam_p	get_iffam_from_af(sa_family_t family);
static iffam_p	get_iffam_from_hook(priv_p priv, hook_p hook);
static iffam_p	get_iffam_from_name(const char *name);
static hook_p  *get_hook_from_iffam(priv_p priv, iffam_p iffam);

/* Parse type for struct ng_iface_ifname */
static const struct ng_parse_fixedstring_info ng_iface_ifname_info = {
	NG_IFACE_IFACE_NAME_MAX + 1
};
static const struct ng_parse_type ng_iface_ifname_type = {
	&ng_parse_fixedstring_type,
	&ng_iface_ifname_info
};

/* Parse type for struct ng_cisco_ipaddr */
static const struct ng_parse_struct_field ng_cisco_ipaddr_type_fields[]
	= NG_CISCO_IPADDR_TYPE_INFO;
static const struct ng_parse_type ng_cisco_ipaddr_type = {
	&ng_parse_struct_type,
	&ng_cisco_ipaddr_type_fields
};

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_iface_cmds[] = {
	{
	  NGM_IFACE_COOKIE,
	  NGM_IFACE_GET_IFNAME,
	  "getifname",
	  NULL,
	  &ng_iface_ifname_type
	},
	{
	  NGM_IFACE_COOKIE,
	  NGM_IFACE_POINT2POINT,
	  "point2point",
	  NULL,
	  NULL
	},
	{
	  NGM_IFACE_COOKIE,
	  NGM_IFACE_BROADCAST,
	  "broadcast",
	  NULL,
	  NULL
	},
	{
	  NGM_CISCO_COOKIE,
	  NGM_CISCO_GET_IPADDR,
	  "getipaddr",
	  NULL,
	  &ng_cisco_ipaddr_type
	},
	{ 0 }
};

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
	ng_iface_cmds
};
NETGRAPH_INIT(iface, &typestruct);

/* We keep a bitmap indicating which unit numbers are free.
   One means the unit number is free, zero means it's taken. */
static int	*ng_iface_units = NULL;
static int	ng_iface_units_len = 0;

#define UNITS_BITSPERWORD	(sizeof(*ng_iface_units) * NBBY)

/************************************************************************
			HELPER STUFF
 ************************************************************************/

/*
 * Get the family descriptor from the family ID
 */
static __inline__ iffam_p
get_iffam_from_af(sa_family_t family)
{
	iffam_p iffam;
	int k;

	for (k = 0; k < NUM_FAMILIES; k++) {
		iffam = &gFamilies[k];
		if (iffam->family == family)
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

/*
 * Find the first free unit number for a new interface.
 * Increase the size of the unit bitmap as necessary.
 */
static __inline__ int
ng_iface_get_unit(int *unit)
{
	int index, bit;

	for (index = 0; index < ng_iface_units_len
	    && ng_iface_units[index] == 0; index++);
	if (index == ng_iface_units_len) {		/* extend array */
		int i, *newarray, newlen;

		newlen = (2 * ng_iface_units_len) + 4;
		MALLOC(newarray, int *, newlen * sizeof(*ng_iface_units),
		    M_NETGRAPH, M_NOWAIT);
		if (newarray == NULL)
			return (ENOMEM);
		bcopy(ng_iface_units, newarray,
		    ng_iface_units_len * sizeof(*ng_iface_units));
		for (i = ng_iface_units_len; i < newlen; i++)
			newarray[i] = ~0;
		if (ng_iface_units != NULL)
			FREE(ng_iface_units, M_NETGRAPH);
		ng_iface_units = newarray;
		ng_iface_units_len = newlen;
	}
	bit = ffs(ng_iface_units[index]) - 1;
	KASSERT(bit >= 0 && bit <= UNITS_BITSPERWORD - 1,
	    ("%s: word=%d bit=%d", __FUNCTION__, ng_iface_units[index], bit));
	ng_iface_units[index] &= ~(1 << bit);
	*unit = (index * UNITS_BITSPERWORD) + bit;
	return (0);
}

/*
 * Free a no longer needed unit number.
 */
static __inline__ void
ng_iface_free_unit(int unit)
{
	int index, bit;

	index = unit / UNITS_BITSPERWORD;
	bit = unit % UNITS_BITSPERWORD;
	KASSERT(index < ng_iface_units_len,
	    ("%s: unit=%d len=%d", __FUNCTION__, unit, ng_iface_units_len));
	KASSERT((ng_iface_units[index] & (1 << bit)) == 0,
	    ("%s: unit=%d is free", __FUNCTION__, unit));
	ng_iface_units[index] |= (1 << bit);
	/*
	 * XXX We could think about reducing the size of ng_iface_units[]
	 * XXX here if the last portion is all ones
	 */
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

	/* BPF writes need to be handled specially */
	if (dst->sa_family == AF_UNSPEC) {
		if (m->m_len < 4 && (m = m_pullup(m, 4)) == NULL)
			return (ENOBUFS);
		dst->sa_family = (sa_family_t)*mtod(m, int32_t *);
		m->m_data += 4;
		m->m_len -= 4;
		m->m_pkthdr.len -= 4;
	}

	/* Berkeley packet filter */
	ng_iface_bpftap(ifp, m, dst->sa_family);

	/* Check address family to determine hook (if known) */
	if (iffam == NULL) {
		m_freem(m);
		log(LOG_WARNING, "%s%d: can't handle af%d\n",
		       ifp->if_name, ifp->if_unit, (int)dst->sa_family);
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

/*
 * Flash a packet by the BPF (requires prepending 4 byte AF header)
 * Note the phoney mbuf; this is OK because BPF treats it read-only.
 */
static void
ng_iface_bpftap(struct ifnet *ifp, struct mbuf *m, sa_family_t family)
{
	int32_t family4 = (int32_t)family;
	struct mbuf m0;

	KASSERT(family != AF_UNSPEC, ("%s: family=AF_UNSPEC", __FUNCTION__));
	if (ifp->if_bpf != NULL) {
		bzero(&m0, sizeof(m0));
		m0.m_next = m;
		m0.m_len = sizeof(family4);
		m0.m_data = (char *)&family4;
		bpf_mtap(ifp, &m0);
	}
}

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
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH, M_NOWAIT);
	if (priv == NULL)
		return (ENOMEM);
	bzero(priv, sizeof(*priv));
	MALLOC(ifp, struct ifnet *, sizeof(*ifp), M_NETGRAPH, M_NOWAIT);
	if (ifp == NULL) {
		FREE(priv, M_NETGRAPH);
		return (ENOMEM);
	}
	bzero(ifp, sizeof(*ifp));

	/* Link them together */
	ifp->if_softc = priv;
	priv->ifp = ifp;

	/* Get an interface unit number */
	if ((error = ng_iface_get_unit(&priv->unit)) != 0) {
		FREE(ifp, M_NETGRAPH);
		FREE(priv, M_NETGRAPH);
		return (error);
	}

	/* Call generic node constructor */
	if ((error = ng_make_node_common(&typestruct, nodep)) != 0) {
		ng_iface_free_unit(priv->unit);
		FREE(ifp, M_NETGRAPH);
		FREE(priv, M_NETGRAPH);
		return (error);
	}
	node = *nodep;

	/* Link together node and private info */
	node->private = priv;
	priv->node = node;

	/* Initialize interface structure */
	ifp->if_name = NG_IFACE_IFACE_NAME;
	ifp->if_unit = priv->unit;
	ifp->if_output = ng_iface_output;
	ifp->if_start = ng_iface_start;
	ifp->if_ioctl = ng_iface_ioctl;
	ifp->if_watchdog = NULL;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	ifp->if_mtu = NG_IFACE_MTU_DEFAULT;
	ifp->if_flags = (IFF_SIMPLEX|IFF_POINTOPOINT|IFF_NOARP|IFF_MULTICAST);
	ifp->if_type = IFT_PROPVIRTUAL;		/* XXX */
	ifp->if_addrlen = 0;			/* XXX */
	ifp->if_hdrlen = 0;			/* XXX */
	ifp->if_baudrate = 64000;		/* XXX */
	TAILQ_INIT(&ifp->if_addrhead);

	/* Give this node the same name as the interface (if possible) */
	bzero(ifname, sizeof(ifname));
	snprintf(ifname, sizeof(ifname), "%s%d", ifp->if_name, ifp->if_unit);
	if (ng_name_node(node, ifname) != 0)
		log(LOG_WARNING, "%s: can't acquire netgraph name\n", ifname);

	/* Attach the interface */
	if_attach(ifp);
	bpfattach(ifp, DLT_NULL, sizeof(u_int));

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
			arg = (struct ng_iface_ifname *)resp->data;
			snprintf(arg->ngif_name, sizeof(arg->ngif_name),
			    "%s%d", ifp->if_name, ifp->if_unit);
			break;
		    }

		case NGM_IFACE_POINT2POINT:
		case NGM_IFACE_BROADCAST:
		    {

			/* Deny request if interface is UP */
			if ((ifp->if_flags & IFF_UP) != 0)
				return (EBUSY);

			/* Change flags */
			switch (msg->header.cmd) {
			case NGM_IFACE_POINT2POINT:
				ifp->if_flags |= IFF_POINTOPOINT;
				ifp->if_flags &= ~IFF_BROADCAST;
				break;
			case NGM_IFACE_BROADCAST:
				ifp->if_flags &= ~IFF_POINTOPOINT;
				ifp->if_flags |= IFF_BROADCAST;
				break;
			}
			break;
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
				struct ng_cisco_ipaddr *ips;

				if (ifa->ifa_addr->sa_family != AF_INET)
					continue;
				NG_MKRESPONSE(resp, msg, sizeof(ips), M_NOWAIT);
				if (resp == NULL) {
					error = ENOMEM;
					break;
				}
				ips = (struct ng_cisco_ipaddr *)resp->data;
				ips->ipaddr = ((struct sockaddr_in *)
						ifa->ifa_addr)->sin_addr;
				ips->netmask = ((struct sockaddr_in *)
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

	/* Berkeley packet filter */
	ng_iface_bpftap(ifp, m, iffam->family);

	/* Ignore any meta-data */
	NG_FREE_META(meta);

	/* Send packet */
	return family_enqueue(iffam->family, m);
}

/*
 * Shutdown and remove the node and its associated interface.
 */
static int
ng_iface_rmnode(node_p node)
{
	const priv_p priv = node->private;

	ng_cutlinks(node);
	ng_unname(node);
	bpfdetach(priv->ifp);
	if_detach(priv->ifp);
	FREE(priv->ifp, M_NETGRAPH);
	priv->ifp = NULL;
	ng_iface_free_unit(priv->unit);
	FREE(priv, M_NETGRAPH);
	node->private = NULL;
	ng_unref(node);
	return (0);
}

/*
 * Hook disconnection. Note that we do *not* shutdown when all
 * hooks have been disconnected.
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

