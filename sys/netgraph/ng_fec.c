/*
 * ng_fec.c
 *
 * Copyright (c) 2001 Berkeley Software Design, Inc.
 * Copyright (c) 2000, 2001
 *	Bill Paul <wpaul@osd.bsdi.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
/*
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
 * $Whistle: ng_fec.c,v 1.33 1999/11/01 09:24:51 julian Exp $
 */

/*
 * This module implements ethernet channel bonding using the Cisco
 * Fast EtherChannel mechanism. Two or four ports may be combined
 * into a single aggregate interface.
 *
 * Interfaces are named fec0, fec1, etc.  New nodes take the
 * first available interface name.
 *
 * This node also includes Berkeley packet filter support.
 *
 * Note that this node doesn't need to connect to any other
 * netgraph nodes in order to do its work.
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
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/intrq.h>
#include <net/bpf.h>
#include <net/ethernet.h>

#include "opt_inet.h"
#include "opt_inet6.h"

#include <netinet/in.h>
#ifdef INET
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_fec.h>

#define IFP2NG(ifp)  ((struct ng_node *)((struct arpcom *)(ifp))->ac_netgraph)
#define FEC_INC(x, y)	(x) = (x + 1) % y

/*
 * Current fast etherchannel implementations use either 2 or 4
 * ports, so for now we limit the maximum bundle size to 4 interfaces.
 */
#define FEC_BUNDLESIZ	4

struct ng_fec_portlist {
	struct ifnet		*fec_if;
	int			fec_idx;
	int			fec_ifstat;
	struct ether_addr	fec_mac;
	TAILQ_ENTRY(ng_fec_portlist) fec_list;
};

struct ng_fec_bundle {
	TAILQ_HEAD(,ng_fec_portlist) ng_fec_ports;
	int			fec_ifcnt;
	int			fec_btype;
};

#define FEC_BTYPE_MAC		0x01
#define FEC_BTYPE_INET		0x02
#define FEC_BTYPE_INET6		0x03

/* Node private data */
struct ng_fec_private {
	struct arpcom arpcom;
	struct ifmedia ifmedia;
	int	if_flags;
	int	if_error;		/* XXX */
	int	unit;			/* Interface unit number */
	node_p	node;			/* Our netgraph node */
	struct ng_fec_bundle fec_bundle;/* Aggregate bundle */
	struct callout_handle fec_ch;	/* callout handle for ticker */
};
typedef struct ng_fec_private *priv_p;

/* Interface methods */
static void	ng_fec_input(struct ifnet *, struct mbuf **,
			struct ether_header *);
static void	ng_fec_start(struct ifnet *ifp);
static int	ng_fec_choose_port(struct ng_fec_bundle *b,
			struct mbuf *m, struct ifnet **ifp);
static int	ng_fec_setport(struct ifnet *ifp, u_long cmd, caddr_t data);
static void	ng_fec_init(void *arg);
static void	ng_fec_stop(struct ifnet *ifp);
static int	ng_fec_ifmedia_upd(struct ifnet *ifp);
static void	ng_fec_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr);
static int	ng_fec_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static int	ng_fec_output(struct ifnet *ifp, struct mbuf *m0,
			struct sockaddr *dst, struct rtentry *rt0);
static void	ng_fec_tick(void *arg);
static int	ng_fec_addport(struct ng_fec_private *priv, char *iface);
static int	ng_fec_delport(struct ng_fec_private *priv, char *iface);

#ifdef DEBUG
static void	ng_fec_print_ioctl(struct ifnet *ifp, int cmd, caddr_t data);
#endif

/* ng_ether_input_p - see sys/netgraph/ng_ether.c */
extern void (*ng_ether_input_p)(struct ifnet *ifp, struct mbuf **mp,
			struct ether_header *eh);

/* Netgraph methods */
static ng_constructor_t	ng_fec_constructor;
static ng_rcvmsg_t	ng_fec_rcvmsg;
static ng_shutdown_t	ng_fec_shutdown;

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_fec_cmds[] = {
	{
	  NGM_FEC_COOKIE,
	  NGM_FEC_ADD_IFACE,
	  "add_iface",
	  &ng_parse_string_type,
	  NULL,
	},
	{
	  NGM_FEC_COOKIE,
	  NGM_FEC_DEL_IFACE,
	  "del_iface",
	  &ng_parse_string_type,
	  NULL,
	},
	{
	  NGM_FEC_COOKIE,
	  NGM_FEC_SET_MODE_MAC,
	  "set_mode_mac",
	  NULL,
	  NULL,
	},
	{
	  NGM_FEC_COOKIE,
	  NGM_FEC_SET_MODE_INET,
	  "set_mode_inet",
	  NULL,
	  NULL,
	},
	{ 0 }
};

/* Node type descriptor */
static struct ng_type typestruct = {
	NG_ABI_VERSION,
	NG_FEC_NODE_TYPE,
	NULL,
	ng_fec_constructor,
	ng_fec_rcvmsg,
	ng_fec_shutdown,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ng_fec_cmds
};
NETGRAPH_INIT(fec, &typestruct);

/* We keep a bitmap indicating which unit numbers are free.
   One means the unit number is free, zero means it's taken. */
static int	*ng_fec_units = NULL;
static int	ng_fec_units_len = 0;
static int	ng_units_in_use = 0;

#define UNITS_BITSPERWORD	(sizeof(*ng_fec_units) * NBBY)

/*
 * Find the first free unit number for a new interface.
 * Increase the size of the unit bitmap as necessary.
 */
static __inline__ int
ng_fec_get_unit(int *unit)
{
	int index, bit;

	for (index = 0; index < ng_fec_units_len
	    && ng_fec_units[index] == 0; index++);
	if (index == ng_fec_units_len) {		/* extend array */
		int i, *newarray, newlen;

		newlen = (2 * ng_fec_units_len) + 4;
		MALLOC(newarray, int *, newlen * sizeof(*ng_fec_units),
		    M_NETGRAPH, M_NOWAIT);
		if (newarray == NULL)
			return (ENOMEM);
		bcopy(ng_fec_units, newarray,
		    ng_fec_units_len * sizeof(*ng_fec_units));
		for (i = ng_fec_units_len; i < newlen; i++)
			newarray[i] = ~0;
		if (ng_fec_units != NULL)
			FREE(ng_fec_units, M_NETGRAPH);
		ng_fec_units = newarray;
		ng_fec_units_len = newlen;
	}
	bit = ffs(ng_fec_units[index]) - 1;
	KASSERT(bit >= 0 && bit <= UNITS_BITSPERWORD - 1,
	    ("%s: word=%d bit=%d", __FUNCTION__, ng_fec_units[index], bit));
	ng_fec_units[index] &= ~(1 << bit);
	*unit = (index * UNITS_BITSPERWORD) + bit;
	ng_units_in_use++;
	return (0);
}

/*
 * Free a no longer needed unit number.
 */
static __inline__ void
ng_fec_free_unit(int unit)
{
	int index, bit;

	index = unit / UNITS_BITSPERWORD;
	bit = unit % UNITS_BITSPERWORD;
	KASSERT(index < ng_fec_units_len,
	    ("%s: unit=%d len=%d", __FUNCTION__, unit, ng_fec_units_len));
	KASSERT((ng_fec_units[index] & (1 << bit)) == 0,
	    ("%s: unit=%d is free", __FUNCTION__, unit));
	ng_fec_units[index] |= (1 << bit);
	/*
	 * XXX We could think about reducing the size of ng_fec_units[]
	 * XXX here if the last portion is all ones
	 * XXX At least free it if no more units
	 * Needed if we are to eventually be able to unload.
	 */
	ng_units_in_use--;
	if (ng_units_in_use == 0) { /* XXX make SMP safe */
		FREE(ng_fec_units, M_NETGRAPH);
		ng_fec_units_len = 0;
		ng_fec_units = NULL;
	}
}

/************************************************************************
			INTERFACE STUFF
 ************************************************************************/

static int
ng_fec_addport(struct ng_fec_private *priv, char *iface)
{
	struct ng_fec_bundle	*b;
	struct ifnet		*ifp, *bifp;
	struct arpcom		*ac;
	struct ifaddr		*ifa;
	struct sockaddr_dl	*sdl;
	struct ng_fec_portlist	*p, *new;

	if (priv == NULL || iface == NULL)
		return(EINVAL);

	b = &priv->fec_bundle;
	ifp = &priv->arpcom.ac_if;

	/* Find the interface */
	bifp = ifunit(iface);
	if (bifp == NULL) {
		printf("fec%d: tried to add iface %s, which "
		    "doesn't seem to exist\n", priv->unit, iface);
		return(ENOENT);
	}

	/* See if we have room in the bundle */
	if (b->fec_ifcnt == FEC_BUNDLESIZ) {
		printf("fec%d: can't add new iface; bundle is full\n",
		    priv->unit);
		return(ENOSPC);
	}

	/* See if the interface is already in the bundle */
	TAILQ_FOREACH(p, &b->ng_fec_ports, fec_list) {
		if (p->fec_if == bifp) {
			printf("fec%d: iface %s is already in this "
			    "bundle\n", priv->unit, iface);
			return(EINVAL);
		}
	}

	/* Allocate new list entry. */
	MALLOC(new, struct ng_fec_portlist *,
	    sizeof(struct ng_fec_portlist), M_NETGRAPH, M_NOWAIT);
	if (new == NULL)
		return(ENOMEM);

	ac = (struct arpcom *)bifp;
	ac->ac_netgraph = priv->node;

	/*
	 * If this is the first interface added to the bundle,
	 * use its MAC address for the virtual interface (and,
	 * by extension, all the other ports in the bundle).
	 */
	if (b->fec_ifcnt == 0) {
		ifa = TAILQ_FIRST(&ifp->if_addrhead);
		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		bcopy((char *)ac->ac_enaddr,
		    priv->arpcom.ac_enaddr, ETHER_ADDR_LEN);
		bcopy((char *)ac->ac_enaddr,
		    LLADDR(sdl), ETHER_ADDR_LEN);
	}

	b->fec_btype = FEC_BTYPE_MAC;
	new->fec_idx = b->fec_ifcnt;
	b->fec_ifcnt++;

	/* Save the real MAC address. */
	bcopy((char *)ac->ac_enaddr,
	    (char *)&new->fec_mac, ETHER_ADDR_LEN);

	/* Set up phony MAC address. */
	ifa = TAILQ_FIRST(&bifp->if_addrhead);
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	bcopy(priv->arpcom.ac_enaddr, ac->ac_enaddr, ETHER_ADDR_LEN);
	bcopy(priv->arpcom.ac_enaddr, LLADDR(sdl), ETHER_ADDR_LEN);

	/* Add to the queue */
	new->fec_if = bifp;
	TAILQ_INSERT_TAIL(&b->ng_fec_ports, new, fec_list);

	return(0);
}

static int
ng_fec_delport(struct ng_fec_private *priv, char *iface)
{
	struct ng_fec_bundle	*b;
	struct ifnet		*ifp, *bifp;
	struct arpcom		*ac;
	struct ifaddr		*ifa;
	struct sockaddr_dl	*sdl;
	struct ng_fec_portlist	*p;

	if (priv == NULL || iface == NULL)
		return(EINVAL);

	b = &priv->fec_bundle;
	ifp = &priv->arpcom.ac_if;

	/* Find the interface */
	bifp = ifunit(iface);
	if (bifp == NULL) {
		printf("fec%d: tried to remove iface %s, which "
		    "doesn't seem to exist\n", priv->unit, iface);
		return(ENOENT);
	}

	TAILQ_FOREACH(p, &b->ng_fec_ports, fec_list) {
		if (p->fec_if == bifp)
			break;
	}

	if (p == NULL) {
		printf("fec%d: tried to remove iface %s which "
		    "is not in our bundle\n", priv->unit, iface);
		return(EINVAL);
	}

	/* Stop interface */
	bifp->if_flags &= ~IFF_UP;
	(*bifp->if_ioctl)(bifp, SIOCSIFFLAGS, NULL);

	/* Restore MAC address. */
	ac = (struct arpcom *)bifp;
	ifa = TAILQ_FIRST(&bifp->if_addrhead);
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	bcopy((char *)&p->fec_mac, ac->ac_enaddr, ETHER_ADDR_LEN);
	bcopy((char *)&p->fec_mac, LLADDR(sdl), ETHER_ADDR_LEN);

	/* Delete port */
	TAILQ_REMOVE(&b->ng_fec_ports, p, fec_list);
	FREE(p, M_NETGRAPH);
	b->fec_ifcnt--;

	return(0);
}

/*
 * Pass an ioctl command down to all the underyling interfaces in a
 * bundle. Used for setting multicast filters and flags.
 */

static int 
ng_fec_setport(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ng_fec_private	*priv;
	struct ng_fec_bundle	*b;
	struct ifnet		*oifp;
	struct ng_fec_portlist	*p;

	priv = ifp->if_softc;
	b = &priv->fec_bundle;

	TAILQ_FOREACH(p, &b->ng_fec_ports, fec_list) {
		oifp = p->fec_if;
		if (oifp != NULL)
			(*oifp->if_ioctl)(oifp, command, data);
	}

	return(0);
}

static void
ng_fec_init(void *arg)
{
	struct ng_fec_private	*priv;
	struct ng_fec_bundle	*b;
	struct ifnet		*ifp, *bifp;
	struct ng_fec_portlist	*p;

	ifp = arg;
	priv = ifp->if_softc;
	b = &priv->fec_bundle;

	if (b->fec_ifcnt == 1 || b->fec_ifcnt == 3) {
		printf("fec%d: invalid bundle "
		    "size: %d\n", priv->unit,
		    b->fec_ifcnt);
		return;
	}

	ng_fec_stop(ifp);

	TAILQ_FOREACH(p, &b->ng_fec_ports, fec_list) {
		bifp = p->fec_if;
		bifp->if_flags |= IFF_UP;
                (*bifp->if_ioctl)(bifp, SIOCSIFFLAGS, NULL);
		/* mark iface as up and let the monitor check it */
		p->fec_ifstat = -1;
	}

	priv->fec_ch = timeout(ng_fec_tick, priv, hz);

	return;
}

static void
ng_fec_stop(struct ifnet *ifp)
{
	struct ng_fec_private	*priv;
	struct ng_fec_bundle	*b;
	struct ifnet		*bifp;
	struct ng_fec_portlist	*p;

	priv = ifp->if_softc;
	b = &priv->fec_bundle;

	TAILQ_FOREACH(p, &b->ng_fec_ports, fec_list) {
		bifp = p->fec_if;
		bifp->if_flags &= ~IFF_UP;
                (*bifp->if_ioctl)(bifp, SIOCSIFFLAGS, NULL);
	}

	untimeout(ng_fec_tick, priv, priv->fec_ch);

	return;
}

static void
ng_fec_tick(void *arg)
{
	struct ng_fec_private	*priv;
	struct ng_fec_bundle	*b;
        struct ifmediareq	ifmr;
	struct ifnet		*ifp;
	struct ng_fec_portlist	*p;
	int			error = 0;

	priv = arg;
	b = &priv->fec_bundle;

	TAILQ_FOREACH(p, &b->ng_fec_ports, fec_list) {
		bzero((char *)&ifmr, sizeof(ifmr));
		ifp = p->fec_if;
		error = (*ifp->if_ioctl)(ifp, SIOCGIFMEDIA, (caddr_t)&ifmr);
		if (error) {
			printf("fec%d: failed to check status "
			    "of link %s%d\n", priv->unit, ifp->if_name,
			    ifp->if_unit);
			continue;
		}

        	if (ifmr.ifm_status & IFM_AVALID &&
                    IFM_TYPE(ifmr.ifm_active) == IFM_ETHER) {
			if (ifmr.ifm_status & IFM_ACTIVE) {
				if (p->fec_ifstat == -1 ||
				    p->fec_ifstat == 0) {
					p->fec_ifstat = 1;
					printf("fec%d: port %s%d in bundle "
					    "is up\n", priv->unit,
					    ifp->if_name, ifp->if_unit);
				}
			} else {
				if (p->fec_ifstat == -1 ||
				    p->fec_ifstat == 1) {
					p->fec_ifstat = 0;
					printf("fec%d: port %s%d in bundle "
					    "is down\n", priv->unit,
					    ifp->if_name, ifp->if_unit);
				}
			}
		}
	}

	ifp = &priv->arpcom.ac_if;
	if (ifp->if_flags & IFF_RUNNING)
		priv->fec_ch = timeout(ng_fec_tick, priv, hz);

	return;
}

static int
ng_fec_ifmedia_upd(struct ifnet *ifp)
{
	return(0);
}

static void ng_fec_ifmedia_sts(struct ifnet *ifp,
	struct ifmediareq *ifmr)
{
	struct ng_fec_private	*priv;
	struct ng_fec_bundle	*b;
	struct ng_fec_portlist	*p;

	priv = ifp->if_softc;
	b = &priv->fec_bundle;

	ifmr->ifm_status = IFM_AVALID;
	TAILQ_FOREACH(p, &b->ng_fec_ports, fec_list) {
		if (p->fec_ifstat) {
			ifmr->ifm_status |= IFM_ACTIVE;
			break;
		}
	}

	return;
}

/*
 * Process an ioctl for the virtual interface
 */
static int
ng_fec_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ifreq *const ifr = (struct ifreq *) data;
	int s, error = 0;
	struct ng_fec_private	*priv;
	struct ng_fec_bundle	*b;

	priv = ifp->if_softc;
	b = &priv->fec_bundle;

#ifdef DEBUG
	ng_fec_print_ioctl(ifp, command, data);
#endif
	s = splimp();
	switch (command) {

	/* These two are mostly handled at a higher layer */
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;

	/* Set flags */
	case SIOCSIFFLAGS:
		/*
		 * If the interface is marked up and stopped, then start it.
		 * If it is marked down and running, then stop it.
		 */
		if (ifr->ifr_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING)) {
				/* Sanity. */
				if (b->fec_ifcnt == 1 || b->fec_ifcnt == 3) {
					printf("fec%d: invalid bundle "
					    "size: %d\n", priv->unit,
					    b->fec_ifcnt);
					error = EINVAL;
					break;
				}
				ifp->if_flags &= ~(IFF_OACTIVE);
				ifp->if_flags |= IFF_RUNNING;
				ng_fec_init(ifp);
			}
			/*
			 * Bubble down changes in promisc mode to
			 * underlying interfaces.
			 */
			if ((ifp->if_flags & IFF_PROMISC) !=
			    (priv->if_flags & IFF_PROMISC)) {
				ng_fec_setport(ifp, command, data);
				priv->if_flags = ifp->if_flags;
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
			ng_fec_stop(ifp);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ng_fec_setport(ifp, command, data);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &priv->ifmedia, command);
		break;
	/* Stuff that's not supported */
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
 * This routine spies on mbufs passing through ether_input(). If
 * they come from one of the interfaces that are aggregated into
 * our bundle, we fix up the ifnet pointer and increment our
 * packet counters so that it looks like the frames are actually
 * coming from us.
 */
static void 
ng_fec_input(struct ifnet *ifp, struct mbuf **m0,
		struct ether_header *eh)
{
	struct ng_node		*node;
	struct ng_fec_private	*priv;
	struct ng_fec_bundle	*b;
	struct mbuf		*m;
	struct ifnet		*bifp;
	struct ng_fec_portlist	*p;

	/* Sanity check */
	if (ifp == NULL || m0 == NULL || eh == NULL)
		return;

	node = IFP2NG(ifp);

	/* Sanity check part II */
	if (node == NULL)
		return;

	priv = NG_NODE_PRIVATE(node);
	b = &priv->fec_bundle;
	bifp = &priv->arpcom.ac_if;

	m = *m0;
	TAILQ_FOREACH(p, &b->ng_fec_ports, fec_list) {
		if (p->fec_if == m->m_pkthdr.rcvif)
			break;
	}

	/* Wasn't meant for us; leave this frame alone. */
	if (p == NULL)
		return;

	/* Pretend this is our frame. */
	m->m_pkthdr.rcvif = bifp;
	bifp->if_ipackets++;
	bifp->if_ibytes += m->m_pkthdr.len + sizeof(struct ether_header);

        /* Check for a BPF tap */
	if (bifp->if_bpf != NULL) {
		struct m_hdr mh;

		/* This kludge is OK; BPF treats the "mbuf" as read-only */
		mh.mh_next = m;
		mh.mh_data = (char *)eh;
		mh.mh_len = ETHER_HDR_LEN;
		BPF_MTAP(bifp, (struct mbuf *)&mh);
	}

	return;
}

/*
 * Take a quick peek at the packet and see if it's ok for us to use
 * the inet or inet6 hash methods on it, if they're enabled. We do
 * this by setting flags in the mbuf header. Once we've made up our
 * mind what to do, we pass the frame to ether_output() for further
 * processing.
 */

static int
ng_fec_output(struct ifnet *ifp, struct mbuf *m,
		struct sockaddr *dst, struct rtentry *rt0)
{
	const priv_p priv = (priv_p) ifp->if_softc;
	struct ng_fec_bundle *b;
	int error;

	/* Check interface flags */
	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		m_freem(m);
		return (ENETDOWN);
	}

	b = &priv->fec_bundle;

	switch (b->fec_btype) {
	case FEC_BTYPE_MAC:
		m->m_flags |= M_FEC_MAC;
		break;
#ifdef INET
	case FEC_BTYPE_INET:
		/*
		 * We can't use the INET address port selection
		 * scheme if this isn't an INET packet.
		 */
		if (dst->sa_family == AF_INET)
			m->m_flags |= M_FEC_INET;
#ifdef INET6
		else if (dst->sa_family == AF_INET6)
			m->m_flags |= M_FEC_INET6;
#endif
		else {
#ifdef DEBUG
			printf("fec%d: can't do inet aggregation of non "
			    "inet packet\n", ifp->if_unit);
#endif
			m->m_flags |= M_FEC_MAC;
		}
		break;
#endif
	default:
		printf("fec%d: bogus hash type: %d\n", ifp->if_unit,
		    b->fec_btype);
		m_freem(m);
		return(EINVAL);
		break;
	}

	/*
	 * Pass the frame to ether_output() for all the protocol
	 * handling. This will put the ethernet header on the packet
	 * for us.
	 */
	priv->if_error = 0;
	error = ether_output(ifp, m, dst, rt0);
	if (priv->if_error && !error)
		error = priv->if_error;

	return(error);
}

/*
 * Apply a hash to the source and destination addresses in the packet
 * in order to select an interface. Also check link status and handle
 * dead links accordingly.
 */

static int
ng_fec_choose_port(struct ng_fec_bundle *b,
	struct mbuf *m, struct ifnet **ifp)
{
	struct ether_header	*eh;
	struct mbuf		*m0;
#ifdef INET
	struct ip		*ip;
#ifdef INET6
	struct ip6_hdr		*ip6;
#endif
#endif

	struct ng_fec_portlist	*p;
	int			port = 0, mask;

	/*
	 * If there are only two ports, mask off all but the
	 * last bit for XORing. If there are 4, mask off all
	 * but the last 2 bits.
	 */
	mask = b->fec_ifcnt == 2 ? 0x1 : 0x3;
	eh = mtod(m, struct ether_header *);
#ifdef INET
	ip = (struct ip *)(mtod(m, char *) +
	    sizeof(struct ether_header));
#ifdef INET6
	ip6 = (struct ip6_hdr *)(mtod(m, char *) +
	    sizeof(struct ether_header));
#endif
#endif

	/*
	 * The fg_fec_output() routine is supposed to leave a
	 * flag for us in the mbuf that tells us what hash to
	 * use, but sometimes a new mbuf is prepended to the
	 * chain, so we have to search every mbuf in the chain
	 * to find the flags.
	 */
	m0 = m;
	while (m0) {
		if (m0->m_flags & (M_FEC_MAC|M_FEC_INET|M_FEC_INET6))
			break;
		m0 = m0->m_next;
	}
	if (m0 == NULL)
		return(EINVAL);

	switch (m0->m_flags & (M_FEC_MAC|M_FEC_INET|M_FEC_INET6)) {
	case M_FEC_MAC:
		port = (eh->ether_dhost[5] ^
		    eh->ether_shost[5]) & mask;
		break;
#ifdef INET
	case M_FEC_INET:
		port = (ntohl(ip->ip_dst.s_addr) ^
		    ntohl(ip->ip_src.s_addr)) & mask;
		break;
#ifdef INET6
	case M_FEC_INET6:
		port = (ip6->ip6_dst.s6_addr[15] ^
		    ip6->ip6_dst.s6_addr[15]) & mask;
		break;
#endif
#endif
	default:
		return(EINVAL);
			break;
	}

	TAILQ_FOREACH(p, &b->ng_fec_ports, fec_list) {
		if (port == p->fec_idx)
			break;
	}

	/*
	 * Now that we've chosen a port, make sure it's
	 * alive. If it's not alive, cycle through the bundle
	 * looking for a port that is alive. If we don't find
	 * any, return an error.
	 */
	if (p->fec_ifstat != 1) {
		struct ng_fec_portlist	*n = NULL;

		n = TAILQ_NEXT(p, fec_list);
		if (n == NULL)
			n = TAILQ_FIRST(&b->ng_fec_ports);
		while (n != p) {
			if (n->fec_ifstat == 1)
				break;
			n = TAILQ_NEXT(n, fec_list);
			if (n == NULL)
				n = TAILQ_FIRST(&b->ng_fec_ports);
		}
		if (n == p)
			return(EAGAIN);
		p = n;
	}

	*ifp = p->fec_if;

	return(0);
}

/*
 * Now that the packet has been run through ether_output(), yank it
 * off our own send queue and stick it on the queue for the appropriate
 * underlying physical interface. Note that if the interface's send
 * queue is full, we save an error status in our private netgraph
 * space which will eventually be handed up to ng_fec_output(), which
 * will return it to the rest of the IP stack. We need to do this
 * in order to duplicate the effect of ether_output() returning ENOBUFS
 * when it detects that an interface's send queue is full. There's no
 * other way to signal the error status from here since the if_start()
 * routine is spec'ed to return void.
 *
 * Once the frame is queued, we call ether_output_frame() to initiate
 * transmission.
 */
static void
ng_fec_start(struct ifnet *ifp)
{
	struct ng_fec_private	*priv;
	struct ng_fec_bundle	*b;
	struct ifnet		*oifp = NULL;
	struct mbuf		*m0;
	int			error;

	priv = ifp->if_softc;
	b = &priv->fec_bundle;

	IF_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == NULL)
		return;

	BPF_MTAP(ifp, m0);

	/* Queue up packet on the proper port. */
	error = ng_fec_choose_port(b, m0, &oifp);
	if (error) {
		ifp->if_ierrors++;
		m_freem(m0);
		priv->if_error = ENOBUFS;
		return;
	}
	ifp->if_opackets++;

	priv->if_error = ether_output_frame(oifp, m0);
	return;
}

#ifdef DEBUG
/*
 * Display an ioctl to the virtual interface
 */

static void
ng_fec_print_ioctl(struct ifnet *ifp, int command, caddr_t data)
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
ng_fec_constructor(node_p node)
{
	char ifname[NG_FEC_FEC_NAME_MAX + 1];
	struct ifnet *ifp;
	priv_p priv;
	struct ng_fec_bundle *b;
	int error = 0;

	/* Allocate node and interface private structures */
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH, M_NOWAIT);
	if (priv == NULL)
		return (ENOMEM);
	bzero(priv, sizeof(*priv));

	ifp = &priv->arpcom.ac_if;
	b = &priv->fec_bundle;

	/* Link them together */
	ifp->if_softc = priv;

	/* Get an interface unit number */
	if ((error = ng_fec_get_unit(&priv->unit)) != 0) {
		FREE(ifp, M_NETGRAPH);
		FREE(priv, M_NETGRAPH);
		return (error);
	}

	/* Link together node and private info */
	NG_NODE_SET_PRIVATE(node, priv);
	priv->node = node;
	priv->arpcom.ac_netgraph = node;

	/* Initialize interface structure */
	ifp->if_name = NG_FEC_FEC_NAME;
	ifp->if_unit = priv->unit;
	ifp->if_output = ng_fec_output;
	ifp->if_start = ng_fec_start;
	ifp->if_ioctl = ng_fec_ioctl;
	ifp->if_init = ng_fec_init;
	ifp->if_watchdog = NULL;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	ifp->if_mtu = NG_FEC_MTU_DEFAULT;
	ifp->if_flags = (IFF_SIMPLEX|IFF_BROADCAST|IFF_MULTICAST);
	ifp->if_type = IFT_PROPVIRTUAL;		/* XXX */
	ifp->if_addrlen = 0;			/* XXX */
	ifp->if_hdrlen = 0;			/* XXX */
	ifp->if_baudrate = 100000000;		/* XXX */
	TAILQ_INIT(&ifp->if_addrhead);

	/* Give this node the same name as the interface (if possible) */
	bzero(ifname, sizeof(ifname));
	snprintf(ifname, sizeof(ifname), "%s%d", ifp->if_name, ifp->if_unit);
	if (ng_name_node(node, ifname) != 0)
		log(LOG_WARNING, "%s: can't acquire netgraph name\n", ifname);

	/* Grab hold of the ether_input pipe. */
	if (ng_ether_input_p == NULL)
		ng_ether_input_p = ng_fec_input;

	/* Attach the interface */
	ether_ifattach(ifp, priv->arpcom.ac_enaddr);
	callout_handle_init(&priv->fec_ch);

	TAILQ_INIT(&b->ng_fec_ports);
	b->fec_ifcnt = 0;

	ifmedia_init(&priv->ifmedia, 0,
	    ng_fec_ifmedia_upd, ng_fec_ifmedia_sts);
	ifmedia_add(&priv->ifmedia, IFM_ETHER|IFM_NONE, 0, NULL);
	ifmedia_set(&priv->ifmedia, IFM_ETHER|IFM_NONE);

	/* Done */
	return (0);
}

/*
 * Receive a control message
 */
static int
ng_fec_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct ng_fec_bundle	*b;
	struct ng_mesg *resp = NULL;
	struct ng_mesg *msg;
	char *ifname;
	int error = 0;

	NGI_GET_MSG(item, msg);
	b = &priv->fec_bundle;

	switch (msg->header.typecookie) {
	case NGM_FEC_COOKIE:
		switch (msg->header.cmd) {
		case NGM_FEC_ADD_IFACE:
			ifname = msg->data;
			error = ng_fec_addport(priv, ifname);
			break;
		case NGM_FEC_DEL_IFACE:
			ifname = msg->data;
			error = ng_fec_delport(priv, ifname);
			break;
		case NGM_FEC_SET_MODE_MAC:
			b->fec_btype = FEC_BTYPE_MAC;
			break;
#ifdef INET
		case NGM_FEC_SET_MODE_INET:
			b->fec_btype = FEC_BTYPE_INET;
			break;
#ifdef INET6
		case NGM_FEC_SET_MODE_INET6:
			b->fec_btype = FEC_BTYPE_INET6;
			break;
#endif
#endif
		default:
			error = EINVAL;
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
 * Shutdown and remove the node and its associated interface.
 */
static int
ng_fec_shutdown(node_p node)
{
	const priv_p priv = NG_NODE_PRIVATE(node);
	struct ng_fec_bundle *b;
	struct ng_fec_portlist	*p;
	char ifname[IFNAMSIZ];

	b = &priv->fec_bundle;
	ng_fec_stop(&priv->arpcom.ac_if);

	while (!TAILQ_EMPTY(&b->ng_fec_ports)) {
		p = TAILQ_FIRST(&b->ng_fec_ports);
		sprintf(ifname, "%s%d",
		    p->fec_if->if_name,
		    p->fec_if->if_unit);
		ng_fec_delport(priv, ifname);
	}

	if (ng_ether_input_p != NULL)
		ng_ether_input_p = NULL;
	ether_ifdetach(&priv->arpcom.ac_if);
	ifmedia_removeall(&priv->ifmedia);
	ng_fec_free_unit(priv->unit);
	FREE(priv, M_NETGRAPH);
	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(node);
	return (0);
}
