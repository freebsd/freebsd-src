/*	$NetBSD: if_bridge.c,v 1.31 2005/06/01 19:45:34 jdc Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1999, 2000 Jason L. Wright (jason@thought.net)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * OpenBSD: if_bridge.c,v 1.60 2001/06/15 03:38:33 itojun Exp
 */

/*
 * Network interface bridge support.
 *
 * TODO:
 *
 *	- Currently only supports Ethernet-like interfaces (Ethernet,
 *	  802.11, VLANs on Ethernet, etc.)  Figure out a nice way
 *	  to bridge other types of interfaces (FDDI-FDDI, and maybe
 *	  consider heterogenous bridges).
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/net/if_bridge.c,v 1.103.2.3 2007/12/21 05:29:15 thompsa Exp $");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_carp.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/protosw.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/socket.h> /* for net/if.h */
#include <sys/sockio.h>
#include <sys/ctype.h>  /* string functions */
#include <sys/kernel.h>
#include <sys/random.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <vm/uma.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/pfil.h>

#include <netinet/in.h> /* for struct arpcom */
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif
#ifdef DEV_CARP
#include <netinet/ip_carp.h>
#endif
#include <machine/in_cksum.h>
#include <netinet/if_ether.h> /* for struct arpcom */
#include <net/bridgestp.h>
#include <net/if_bridgevar.h>
#include <net/if_llc.h>
#include <net/if_vlan_var.h>

#include <net/route.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>

/*
 * Size of the route hash table.  Must be a power of two.
 */
#ifndef BRIDGE_RTHASH_SIZE
#define	BRIDGE_RTHASH_SIZE		1024
#endif

#define	BRIDGE_RTHASH_MASK		(BRIDGE_RTHASH_SIZE - 1)

/*
 * Maximum number of addresses to cache.
 */
#ifndef BRIDGE_RTABLE_MAX
#define	BRIDGE_RTABLE_MAX		100
#endif

/*
 * Timeout (in seconds) for entries learned dynamically.
 */
#ifndef BRIDGE_RTABLE_TIMEOUT
#define	BRIDGE_RTABLE_TIMEOUT		(20 * 60)	/* same as ARP */
#endif

/*
 * Number of seconds between walks of the route list.
 */
#ifndef BRIDGE_RTABLE_PRUNE_PERIOD
#define	BRIDGE_RTABLE_PRUNE_PERIOD	(5 * 60)
#endif

/*
 * List of capabilities to mask on the member interface.
 */
#define	BRIDGE_IFCAPS_MASK		IFCAP_TXCSUM

/*
 * Bridge interface list entry.
 */
struct bridge_iflist {
	LIST_ENTRY(bridge_iflist) bif_next;
	struct ifnet		*bif_ifp;	/* member if */
	struct bstp_port	bif_stp;	/* STP state */
	uint32_t		bif_flags;	/* member if flags */
	int			bif_mutecap;	/* member muted caps */
};

/*
 * Bridge route node.
 */
struct bridge_rtnode {
	LIST_ENTRY(bridge_rtnode) brt_hash;	/* hash table linkage */
	LIST_ENTRY(bridge_rtnode) brt_list;	/* list linkage */
	struct ifnet		*brt_ifp;	/* destination if */
	unsigned long		brt_expire;	/* expiration time */
	uint8_t			brt_flags;	/* address flags */
	uint8_t			brt_addr[ETHER_ADDR_LEN];
	uint16_t		brt_vlan;	/* vlan id */
};

/*
 * Software state for each bridge.
 */
struct bridge_softc {
	struct ifnet		*sc_ifp;	/* make this an interface */
	LIST_ENTRY(bridge_softc) sc_list;
	struct mtx		sc_mtx;
	struct cv		sc_cv;
	uint32_t		sc_brtmax;	/* max # of addresses */
	uint32_t		sc_brtcnt;	/* cur. # of addresses */
	uint32_t		sc_brttimeout;	/* rt timeout in seconds */
	struct callout		sc_brcallout;	/* bridge callout */
	uint32_t		sc_iflist_ref;	/* refcount for sc_iflist */
	uint32_t		sc_iflist_xcnt;	/* refcount for sc_iflist */
	LIST_HEAD(, bridge_iflist) sc_iflist;	/* member interface list */
	LIST_HEAD(, bridge_rtnode) *sc_rthash;	/* our forwarding table */
	LIST_HEAD(, bridge_rtnode) sc_rtlist;	/* list version of above */
	uint32_t		sc_rthash_key;	/* key for hash */
	LIST_HEAD(, bridge_iflist) sc_spanlist;	/* span ports list */
	struct bstp_state	sc_stp;		/* STP state */
	uint32_t		sc_brtexceeded;	/* # of cache drops */
};

static struct mtx 	bridge_list_mtx;
eventhandler_tag	bridge_detach_cookie = NULL;

int	bridge_rtable_prune_period = BRIDGE_RTABLE_PRUNE_PERIOD;

uma_zone_t bridge_rtnode_zone;

static int	bridge_clone_create(struct if_clone *, int, caddr_t);
static void	bridge_clone_destroy(struct ifnet *);

static int	bridge_ioctl(struct ifnet *, u_long, caddr_t);
static void	bridge_mutecaps(struct bridge_iflist *, int);
static void	bridge_ifdetach(void *arg __unused, struct ifnet *);
static void	bridge_init(void *);
static void	bridge_dummynet(struct mbuf *, struct ifnet *);
static void	bridge_stop(struct ifnet *, int);
static void	bridge_start(struct ifnet *);
static struct mbuf *bridge_input(struct ifnet *, struct mbuf *);
static int	bridge_output(struct ifnet *, struct mbuf *, struct sockaddr *,
		    struct rtentry *);
static void	bridge_enqueue(struct bridge_softc *, struct ifnet *,
		    struct mbuf *);
static void	bridge_rtdelete(struct bridge_softc *, struct ifnet *ifp, int);

static void	bridge_forward(struct bridge_softc *, struct bridge_iflist *,
		    struct mbuf *m);

static void	bridge_timer(void *);

static void	bridge_broadcast(struct bridge_softc *, struct ifnet *,
		    struct mbuf *, int);
static void	bridge_span(struct bridge_softc *, struct mbuf *);

static int	bridge_rtupdate(struct bridge_softc *, const uint8_t *,
		    uint16_t, struct bridge_iflist *, int, uint8_t);
static struct ifnet *bridge_rtlookup(struct bridge_softc *, const uint8_t *,
		    uint16_t);
static void	bridge_rttrim(struct bridge_softc *);
static void	bridge_rtage(struct bridge_softc *);
static void	bridge_rtflush(struct bridge_softc *, int);
static int	bridge_rtdaddr(struct bridge_softc *, const uint8_t *,
		    uint16_t);

static int	bridge_rtable_init(struct bridge_softc *);
static void	bridge_rtable_fini(struct bridge_softc *);

static int	bridge_rtnode_addr_cmp(const uint8_t *, const uint8_t *);
static struct bridge_rtnode *bridge_rtnode_lookup(struct bridge_softc *,
		    const uint8_t *, uint16_t);
static int	bridge_rtnode_insert(struct bridge_softc *,
		    struct bridge_rtnode *);
static void	bridge_rtnode_destroy(struct bridge_softc *,
		    struct bridge_rtnode *);
static void	bridge_rtable_expire(struct ifnet *, int);
static void	bridge_state_change(struct ifnet *, int);

static struct bridge_iflist *bridge_lookup_member(struct bridge_softc *,
		    const char *name);
static struct bridge_iflist *bridge_lookup_member_if(struct bridge_softc *,
		    struct ifnet *ifp);
static void	bridge_delete_member(struct bridge_softc *,
		    struct bridge_iflist *, int);
static void	bridge_delete_span(struct bridge_softc *,
		    struct bridge_iflist *);

static int	bridge_ioctl_add(struct bridge_softc *, void *);
static int	bridge_ioctl_del(struct bridge_softc *, void *);
static int	bridge_ioctl_gifflags(struct bridge_softc *, void *);
static int	bridge_ioctl_sifflags(struct bridge_softc *, void *);
static int	bridge_ioctl_scache(struct bridge_softc *, void *);
static int	bridge_ioctl_gcache(struct bridge_softc *, void *);
static int	bridge_ioctl_gifs(struct bridge_softc *, void *);
static int	bridge_ioctl_rts(struct bridge_softc *, void *);
static int	bridge_ioctl_saddr(struct bridge_softc *, void *);
static int	bridge_ioctl_sto(struct bridge_softc *, void *);
static int	bridge_ioctl_gto(struct bridge_softc *, void *);
static int	bridge_ioctl_daddr(struct bridge_softc *, void *);
static int	bridge_ioctl_flush(struct bridge_softc *, void *);
static int	bridge_ioctl_gpri(struct bridge_softc *, void *);
static int	bridge_ioctl_spri(struct bridge_softc *, void *);
static int	bridge_ioctl_ght(struct bridge_softc *, void *);
static int	bridge_ioctl_sht(struct bridge_softc *, void *);
static int	bridge_ioctl_gfd(struct bridge_softc *, void *);
static int	bridge_ioctl_sfd(struct bridge_softc *, void *);
static int	bridge_ioctl_gma(struct bridge_softc *, void *);
static int	bridge_ioctl_sma(struct bridge_softc *, void *);
static int	bridge_ioctl_sifprio(struct bridge_softc *, void *);
static int	bridge_ioctl_sifcost(struct bridge_softc *, void *);
static int	bridge_ioctl_addspan(struct bridge_softc *, void *);
static int	bridge_ioctl_delspan(struct bridge_softc *, void *);
static int	bridge_ioctl_gbparam(struct bridge_softc *, void *);
static int	bridge_ioctl_grte(struct bridge_softc *, void *);
static int	bridge_ioctl_gifsstp(struct bridge_softc *, void *);
static int	bridge_ioctl_sproto(struct bridge_softc *, void *);
static int	bridge_ioctl_stxhc(struct bridge_softc *, void *);
static int	bridge_pfil(struct mbuf **, struct ifnet *, struct ifnet *,
		    int);
static int	bridge_ip_checkbasic(struct mbuf **mp);
#ifdef INET6
static int	bridge_ip6_checkbasic(struct mbuf **mp);
#endif /* INET6 */
static int	bridge_fragment(struct ifnet *, struct mbuf *,
		    struct ether_header *, int, struct llc *);

/* The default bridge vlan is 1 (IEEE 802.1Q-2003 Table 9-2) */
#define	VLANTAGOF(_m)	\
    (_m->m_flags & M_VLANTAG) ? EVL_VLANOFTAG(_m->m_pkthdr.ether_vtag) : 1

static struct bstp_cb_ops bridge_ops = {
	.bcb_state = bridge_state_change,
	.bcb_rtage = bridge_rtable_expire
};

SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, IFT_BRIDGE, bridge, CTLFLAG_RW, 0, "Bridge");

static int pfil_onlyip = 1; /* only pass IP[46] packets when pfil is enabled */
static int pfil_bridge = 1; /* run pfil hooks on the bridge interface */
static int pfil_member = 1; /* run pfil hooks on the member interface */
static int pfil_ipfw = 0;   /* layer2 filter with ipfw */
static int pfil_ipfw_arp = 0;   /* layer2 filter with ipfw */
static int pfil_local_phys = 0; /* run pfil hooks on the physical interface for
                                   locally destined packets */
static int log_stp   = 0;   /* log STP state changes */
SYSCTL_INT(_net_link_bridge, OID_AUTO, pfil_onlyip, CTLFLAG_RW,
    &pfil_onlyip, 0, "Only pass IP packets when pfil is enabled");
SYSCTL_INT(_net_link_bridge, OID_AUTO, ipfw_arp, CTLFLAG_RW,
    &pfil_ipfw_arp, 0, "Filter ARP packets through IPFW layer2");
SYSCTL_INT(_net_link_bridge, OID_AUTO, pfil_bridge, CTLFLAG_RW,
    &pfil_bridge, 0, "Packet filter on the bridge interface");
SYSCTL_INT(_net_link_bridge, OID_AUTO, pfil_member, CTLFLAG_RW,
    &pfil_member, 0, "Packet filter on the member interface");
SYSCTL_INT(_net_link_bridge, OID_AUTO, pfil_local_phys, CTLFLAG_RW,
    &pfil_local_phys, 0,
    "Packet filter on the physical interface for locally destined packets");
SYSCTL_INT(_net_link_bridge, OID_AUTO, log_stp, CTLFLAG_RW,
    &log_stp, 0, "Log STP state changes");

struct bridge_control {
	int	(*bc_func)(struct bridge_softc *, void *);
	int	bc_argsize;
	int	bc_flags;
};

#define	BC_F_COPYIN		0x01	/* copy arguments in */
#define	BC_F_COPYOUT		0x02	/* copy arguments out */
#define	BC_F_SUSER		0x04	/* do super-user check */

const struct bridge_control bridge_control_table[] = {
	{ bridge_ioctl_add,		sizeof(struct ifbreq),
	  BC_F_COPYIN|BC_F_SUSER },
	{ bridge_ioctl_del,		sizeof(struct ifbreq),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_gifflags,	sizeof(struct ifbreq),
	  BC_F_COPYIN|BC_F_COPYOUT },
	{ bridge_ioctl_sifflags,	sizeof(struct ifbreq),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_scache,		sizeof(struct ifbrparam),
	  BC_F_COPYIN|BC_F_SUSER },
	{ bridge_ioctl_gcache,		sizeof(struct ifbrparam),
	  BC_F_COPYOUT },

	{ bridge_ioctl_gifs,		sizeof(struct ifbifconf),
	  BC_F_COPYIN|BC_F_COPYOUT },
	{ bridge_ioctl_rts,		sizeof(struct ifbaconf),
	  BC_F_COPYIN|BC_F_COPYOUT },

	{ bridge_ioctl_saddr,		sizeof(struct ifbareq),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_sto,		sizeof(struct ifbrparam),
	  BC_F_COPYIN|BC_F_SUSER },
	{ bridge_ioctl_gto,		sizeof(struct ifbrparam),
	  BC_F_COPYOUT },

	{ bridge_ioctl_daddr,		sizeof(struct ifbareq),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_flush,		sizeof(struct ifbreq),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_gpri,		sizeof(struct ifbrparam),
	  BC_F_COPYOUT },
	{ bridge_ioctl_spri,		sizeof(struct ifbrparam),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_ght,		sizeof(struct ifbrparam),
	  BC_F_COPYOUT },
	{ bridge_ioctl_sht,		sizeof(struct ifbrparam),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_gfd,		sizeof(struct ifbrparam),
	  BC_F_COPYOUT },
	{ bridge_ioctl_sfd,		sizeof(struct ifbrparam),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_gma,		sizeof(struct ifbrparam),
	  BC_F_COPYOUT },
	{ bridge_ioctl_sma,		sizeof(struct ifbrparam),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_sifprio,		sizeof(struct ifbreq),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_sifcost,		sizeof(struct ifbreq),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_addspan,		sizeof(struct ifbreq),
	  BC_F_COPYIN|BC_F_SUSER },
	{ bridge_ioctl_delspan,		sizeof(struct ifbreq),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_gbparam,		sizeof(struct ifbropreq),
	  BC_F_COPYOUT },

	{ bridge_ioctl_grte,		sizeof(struct ifbrparam),
	  BC_F_COPYOUT },

	{ bridge_ioctl_gifsstp,		sizeof(struct ifbpstpconf),
	  BC_F_COPYIN|BC_F_COPYOUT },

	{ bridge_ioctl_sproto,		sizeof(struct ifbrparam),
	  BC_F_COPYIN|BC_F_SUSER },

	{ bridge_ioctl_stxhc,		sizeof(struct ifbrparam),
	  BC_F_COPYIN|BC_F_SUSER },
};
const int bridge_control_table_size =
    sizeof(bridge_control_table) / sizeof(bridge_control_table[0]);

LIST_HEAD(, bridge_softc) bridge_list;

IFC_SIMPLE_DECLARE(bridge, 0);

static int
bridge_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		mtx_init(&bridge_list_mtx, "if_bridge list", NULL, MTX_DEF);
		if_clone_attach(&bridge_cloner);
		bridge_rtnode_zone = uma_zcreate("bridge_rtnode",
		    sizeof(struct bridge_rtnode), NULL, NULL, NULL, NULL,
		    UMA_ALIGN_PTR, 0);
		LIST_INIT(&bridge_list);
		bridge_input_p = bridge_input;
		bridge_output_p = bridge_output;
		bridge_dn_p = bridge_dummynet;
		bridge_detach_cookie = EVENTHANDLER_REGISTER(
		    ifnet_departure_event, bridge_ifdetach, NULL,
		    EVENTHANDLER_PRI_ANY);
		break;
	case MOD_UNLOAD:
		EVENTHANDLER_DEREGISTER(ifnet_departure_event,
		    bridge_detach_cookie);
		if_clone_detach(&bridge_cloner);
		uma_zdestroy(bridge_rtnode_zone);
		bridge_input_p = NULL;
		bridge_output_p = NULL;
		bridge_dn_p = NULL;
		mtx_destroy(&bridge_list_mtx);
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t bridge_mod = {
	"if_bridge",
	bridge_modevent,
	0
};

DECLARE_MODULE(if_bridge, bridge_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_DEPEND(if_bridge, bridgestp, 1, 1, 1);

/*
 * handler for net.link.bridge.pfil_ipfw
 */
static int
sysctl_pfil_ipfw(SYSCTL_HANDLER_ARGS)
{
	int enable = pfil_ipfw;
	int error;

	error = sysctl_handle_int(oidp, &enable, 0, req);
	enable = (enable) ? 1 : 0;

	if (enable != pfil_ipfw) {
		pfil_ipfw = enable;

		/*
		 * Disable pfil so that ipfw doesnt run twice, if the user
		 * really wants both then they can re-enable pfil_bridge and/or
		 * pfil_member. Also allow non-ip packets as ipfw can filter by
		 * layer2 type.
		 */
		if (pfil_ipfw) {
			pfil_onlyip = 0;
			pfil_bridge = 0;
			pfil_member = 0;
		}
	}

	return (error);
}
SYSCTL_PROC(_net_link_bridge, OID_AUTO, ipfw, CTLTYPE_INT|CTLFLAG_RW,
	    &pfil_ipfw, 0, &sysctl_pfil_ipfw, "I", "Layer2 filter with IPFW");

/*
 * bridge_clone_create:
 *
 *	Create a new bridge instance.
 */
static int
bridge_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct bridge_softc *sc, *sc2;
	struct ifnet *bifp, *ifp;
	u_char eaddr[6];
	int retry;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	ifp = sc->sc_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		free(sc, M_DEVBUF);
		return (ENOSPC);
	}

	BRIDGE_LOCK_INIT(sc);
	sc->sc_brtmax = BRIDGE_RTABLE_MAX;
	sc->sc_brttimeout = BRIDGE_RTABLE_TIMEOUT;

	/* Initialize our routing table. */
	bridge_rtable_init(sc);

	callout_init_mtx(&sc->sc_brcallout, &sc->sc_mtx, 0);

	LIST_INIT(&sc->sc_iflist);
	LIST_INIT(&sc->sc_spanlist);

	ifp->if_softc = sc;
	if_initname(ifp, ifc->ifc_name, unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = bridge_ioctl;
	ifp->if_start = bridge_start;
	ifp->if_init = bridge_init;
	ifp->if_type = IFT_BRIDGE;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Generate a random ethernet address with a locally administered
	 * address.
	 *
	 * Since we are using random ethernet addresses for the bridge, it is
	 * possible that we might have address collisions, so make sure that
	 * this hardware address isn't already in use on another bridge.
	 */
	for (retry = 1; retry != 0;) {
		arc4rand(eaddr, ETHER_ADDR_LEN, 1);
		eaddr[0] &= ~1;		/* clear multicast bit */
		eaddr[0] |= 2;		/* set the LAA bit */
		retry = 0;
		mtx_lock(&bridge_list_mtx);
		LIST_FOREACH(sc2, &bridge_list, sc_list) {
			bifp = sc2->sc_ifp;
			if (memcmp(eaddr, IF_LLADDR(bifp), ETHER_ADDR_LEN) == 0)
				retry = 1;
		}
		mtx_unlock(&bridge_list_mtx);
	}

	bstp_attach(&sc->sc_stp, &bridge_ops);
	ether_ifattach(ifp, eaddr);
	/* Now undo some of the damage... */
	ifp->if_baudrate = 0;
	ifp->if_type = IFT_BRIDGE;

	mtx_lock(&bridge_list_mtx);
	LIST_INSERT_HEAD(&bridge_list, sc, sc_list);
	mtx_unlock(&bridge_list_mtx);

	return (0);
}

/*
 * bridge_clone_destroy:
 *
 *	Destroy a bridge instance.
 */
static void
bridge_clone_destroy(struct ifnet *ifp)
{
	struct bridge_softc *sc = ifp->if_softc;
	struct bridge_iflist *bif;

	BRIDGE_LOCK(sc);

	bridge_stop(ifp, 1);
	ifp->if_flags &= ~IFF_UP;

	while ((bif = LIST_FIRST(&sc->sc_iflist)) != NULL)
		bridge_delete_member(sc, bif, 0);

	while ((bif = LIST_FIRST(&sc->sc_spanlist)) != NULL) {
		bridge_delete_span(sc, bif);
	}

	BRIDGE_UNLOCK(sc);

	callout_drain(&sc->sc_brcallout);

	mtx_lock(&bridge_list_mtx);
	LIST_REMOVE(sc, sc_list);
	mtx_unlock(&bridge_list_mtx);

	bstp_detach(&sc->sc_stp);
	ether_ifdetach(ifp);
	if_free_type(ifp, IFT_ETHER);

	/* Tear down the routing table. */
	bridge_rtable_fini(sc);

	BRIDGE_LOCK_DESTROY(sc);
	free(sc, M_DEVBUF);
}

/*
 * bridge_ioctl:
 *
 *	Handle a control request from the operator.
 */
static int
bridge_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct bridge_softc *sc = ifp->if_softc;
	struct thread *td = curthread;
	union {
		struct ifbreq ifbreq;
		struct ifbifconf ifbifconf;
		struct ifbareq ifbareq;
		struct ifbaconf ifbaconf;
		struct ifbrparam ifbrparam;
		struct ifbropreq ifbropreq;
	} args;
	struct ifdrv *ifd = (struct ifdrv *) data;
	const struct bridge_control *bc;
	int error = 0;

	switch (cmd) {

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCGDRVSPEC:
	case SIOCSDRVSPEC:
		if (ifd->ifd_cmd >= bridge_control_table_size) {
			error = EINVAL;
			break;
		}
		bc = &bridge_control_table[ifd->ifd_cmd];

		if (cmd == SIOCGDRVSPEC &&
		    (bc->bc_flags & BC_F_COPYOUT) == 0) {
			error = EINVAL;
			break;
		}
		else if (cmd == SIOCSDRVSPEC &&
		    (bc->bc_flags & BC_F_COPYOUT) != 0) {
			error = EINVAL;
			break;
		}

		if (bc->bc_flags & BC_F_SUSER) {
			error = priv_check(td, PRIV_NET_BRIDGE);
			if (error)
				break;
		}

		if (ifd->ifd_len != bc->bc_argsize ||
		    ifd->ifd_len > sizeof(args)) {
			error = EINVAL;
			break;
		}

		bzero(&args, sizeof(args));
		if (bc->bc_flags & BC_F_COPYIN) {
			error = copyin(ifd->ifd_data, &args, ifd->ifd_len);
			if (error)
				break;
		}

		BRIDGE_LOCK(sc);
		error = (*bc->bc_func)(sc, &args);
		BRIDGE_UNLOCK(sc);
		if (error)
			break;

		if (bc->bc_flags & BC_F_COPYOUT)
			error = copyout(&args, ifd->ifd_data, ifd->ifd_len);

		break;

	case SIOCSIFFLAGS:
		if (!(ifp->if_flags & IFF_UP) &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			/*
			 * If interface is marked down and it is running,
			 * then stop and disable it.
			 */
			BRIDGE_LOCK(sc);
			bridge_stop(ifp, 1);
			BRIDGE_UNLOCK(sc);
		} else if ((ifp->if_flags & IFF_UP) &&
		    !(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			(*ifp->if_init)(sc);
		}
		break;

	case SIOCSIFMTU:
		/* Do not allow the MTU to be changed on the bridge */
		error = EINVAL;
		break;

	default:
		/*
		 * drop the lock as ether_ioctl() will call bridge_start() and
		 * cause the lock to be recursed.
		 */
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

/*
 * bridge_mutecaps:
 *
 *	Clear or restore unwanted capabilities on the member interface
 */
static void
bridge_mutecaps(struct bridge_iflist *bif, int mute)
{
	struct ifnet *ifp = bif->bif_ifp;
	struct ifreq ifr;
	int error;

	if (ifp->if_ioctl == NULL)
		return;

	bzero(&ifr, sizeof(ifr));
	ifr.ifr_reqcap = ifp->if_capenable;

	if (mute) {
		/* mask off and save capabilities */
		bif->bif_mutecap = ifr.ifr_reqcap & BRIDGE_IFCAPS_MASK;
		if (bif->bif_mutecap != 0)
			ifr.ifr_reqcap &= ~BRIDGE_IFCAPS_MASK;
	} else
		/* restore muted capabilities */
		ifr.ifr_reqcap |= bif->bif_mutecap;


	if (bif->bif_mutecap != 0) {
		IFF_LOCKGIANT(ifp);
		error = (*ifp->if_ioctl)(ifp, SIOCSIFCAP, (caddr_t)&ifr);
		IFF_UNLOCKGIANT(ifp);
	}
}

/*
 * bridge_lookup_member:
 *
 *	Lookup a bridge member interface.
 */
static struct bridge_iflist *
bridge_lookup_member(struct bridge_softc *sc, const char *name)
{
	struct bridge_iflist *bif;
	struct ifnet *ifp;

	BRIDGE_LOCK_ASSERT(sc);

	LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		ifp = bif->bif_ifp;
		if (strcmp(ifp->if_xname, name) == 0)
			return (bif);
	}

	return (NULL);
}

/*
 * bridge_lookup_member_if:
 *
 *	Lookup a bridge member interface by ifnet*.
 */
static struct bridge_iflist *
bridge_lookup_member_if(struct bridge_softc *sc, struct ifnet *member_ifp)
{
	struct bridge_iflist *bif;

	BRIDGE_LOCK_ASSERT(sc);

	LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		if (bif->bif_ifp == member_ifp)
			return (bif);
	}

	return (NULL);
}

/*
 * bridge_delete_member:
 *
 *	Delete the specified member interface.
 */
static void
bridge_delete_member(struct bridge_softc *sc, struct bridge_iflist *bif,
    int gone)
{
	struct ifnet *ifs = bif->bif_ifp;

	BRIDGE_LOCK_ASSERT(sc);

	if (!gone) {
		switch (ifs->if_type) {
		case IFT_ETHER:
		case IFT_L2VLAN:
			/*
			 * Take the interface out of promiscuous mode.
			 */
			(void) ifpromisc(ifs, 0);
			bridge_mutecaps(bif, 0);
			break;

		case IFT_GIF:
			break;

		default:
#ifdef DIAGNOSTIC
			panic("bridge_delete_member: impossible");
#endif
			break;
		}
	}

	if (bif->bif_flags & IFBIF_STP)
		bstp_disable(&bif->bif_stp);

	ifs->if_bridge = NULL;
	BRIDGE_XLOCK(sc);
	LIST_REMOVE(bif, bif_next);
	BRIDGE_XDROP(sc);

	bridge_rtdelete(sc, ifs, IFBF_FLUSHALL);

	BRIDGE_UNLOCK(sc);
	bstp_destroy(&bif->bif_stp);	/* prepare to free */
	BRIDGE_LOCK(sc);
	free(bif, M_DEVBUF);
}

/*
 * bridge_delete_span:
 *
 *	Delete the specified span interface.
 */
static void
bridge_delete_span(struct bridge_softc *sc, struct bridge_iflist *bif)
{
	BRIDGE_LOCK_ASSERT(sc);

	KASSERT(bif->bif_ifp->if_bridge == NULL,
	    ("%s: not a span interface", __func__));

	LIST_REMOVE(bif, bif_next);
	free(bif, M_DEVBUF);
}

static int
bridge_ioctl_add(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif = NULL;
	struct ifnet *ifs;
	int error = 0;

	ifs = ifunit(req->ifbr_ifsname);
	if (ifs == NULL)
		return (ENOENT);

	/* If it's in the span list, it can't be a member. */
	LIST_FOREACH(bif, &sc->sc_spanlist, bif_next)
		if (ifs == bif->bif_ifp)
			return (EBUSY);

	/* Allow the first Ethernet member to define the MTU */
	if (ifs->if_type != IFT_GIF) {
		if (LIST_EMPTY(&sc->sc_iflist))
			sc->sc_ifp->if_mtu = ifs->if_mtu;
		else if (sc->sc_ifp->if_mtu != ifs->if_mtu) {
			if_printf(sc->sc_ifp, "invalid MTU for %s\n",
			    ifs->if_xname);
			return (EINVAL);
		}
	}

	if (ifs->if_bridge == sc)
		return (EEXIST);

	if (ifs->if_bridge != NULL)
		return (EBUSY);

	bif = malloc(sizeof(*bif), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (bif == NULL)
		return (ENOMEM);

	bif->bif_ifp = ifs;
	bif->bif_flags = IFBIF_LEARNING | IFBIF_DISCOVER;

	switch (ifs->if_type) {
	case IFT_ETHER:
	case IFT_L2VLAN:
		/*
		 * Place the interface into promiscuous mode.
		 */
		error = ifpromisc(ifs, 1);
		if (error)
			goto out;

		bridge_mutecaps(bif, 1);
		break;

	case IFT_GIF:
		break;

	default:
		error = EINVAL;
		goto out;
	}

	ifs->if_bridge = sc;
	bstp_create(&sc->sc_stp, &bif->bif_stp, bif->bif_ifp);
	/*
	 * XXX: XLOCK HERE!?!
	 *
	 * NOTE: insert_***HEAD*** should be safe for the traversals.
	 */
	LIST_INSERT_HEAD(&sc->sc_iflist, bif, bif_next);

out:
	if (error) {
		if (bif != NULL)
			free(bif, M_DEVBUF);
	}
	return (error);
}

static int
bridge_ioctl_del(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	bridge_delete_member(sc, bif, 0);

	return (0);
}

static int
bridge_ioctl_gifflags(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;
	struct bstp_port *bp;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	bp = &bif->bif_stp;
	req->ifbr_ifsflags = bif->bif_flags;
	req->ifbr_state = bp->bp_state;
	req->ifbr_priority = bp->bp_priority;
	req->ifbr_path_cost = bp->bp_path_cost;
	req->ifbr_portno = bif->bif_ifp->if_index & 0xfff;
	req->ifbr_proto = bp->bp_protover;
	req->ifbr_role = bp->bp_role;
	req->ifbr_stpflags = bp->bp_flags;

	/* Copy STP state options as flags */
	if (bp->bp_operedge)
		req->ifbr_ifsflags |= IFBIF_BSTP_EDGE;
	if (bp->bp_flags & BSTP_PORT_AUTOEDGE)
		req->ifbr_ifsflags |= IFBIF_BSTP_AUTOEDGE;
	if (bp->bp_ptp_link)
		req->ifbr_ifsflags |= IFBIF_BSTP_PTP;
	if (bp->bp_flags & BSTP_PORT_AUTOPTP)
		req->ifbr_ifsflags |= IFBIF_BSTP_AUTOPTP;
	if (bp->bp_flags & BSTP_PORT_ADMEDGE)
		req->ifbr_ifsflags |= IFBIF_BSTP_ADMEDGE;
	if (bp->bp_flags & BSTP_PORT_ADMCOST)
		req->ifbr_ifsflags |= IFBIF_BSTP_ADMCOST;
	return (0);
}

static int
bridge_ioctl_sifflags(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;
	struct bstp_port *bp;
	int error;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);
	bp = &bif->bif_stp;

	if (req->ifbr_ifsflags & IFBIF_SPAN)
		/* SPAN is readonly */
		return (EINVAL);

	if (req->ifbr_ifsflags & IFBIF_STP) {
		if ((bif->bif_flags & IFBIF_STP) == 0) {
			error = bstp_enable(&bif->bif_stp);
			if (error)
				return (error);
		}
	} else {
		if ((bif->bif_flags & IFBIF_STP) != 0)
			bstp_disable(&bif->bif_stp);
	}

	/* Pass on STP flags */
	bstp_set_edge(bp, req->ifbr_ifsflags & IFBIF_BSTP_EDGE ? 1 : 0);
	bstp_set_autoedge(bp, req->ifbr_ifsflags & IFBIF_BSTP_AUTOEDGE ? 1 : 0);
	bstp_set_ptp(bp, req->ifbr_ifsflags & IFBIF_BSTP_PTP ? 1 : 0);
	bstp_set_autoptp(bp, req->ifbr_ifsflags & IFBIF_BSTP_AUTOPTP ? 1 : 0);

	/* Save the bits relating to the bridge */
	bif->bif_flags = req->ifbr_ifsflags & IFBIFMASK;

	return (0);
}

static int
bridge_ioctl_scache(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	sc->sc_brtmax = param->ifbrp_csize;
	bridge_rttrim(sc);

	return (0);
}

static int
bridge_ioctl_gcache(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	param->ifbrp_csize = sc->sc_brtmax;

	return (0);
}

static int
bridge_ioctl_gifs(struct bridge_softc *sc, void *arg)
{
	struct ifbifconf *bifc = arg;
	struct bridge_iflist *bif;
	struct ifbreq breq;
	char *buf, *outbuf;
	int count, buflen, len, error = 0;

	count = 0;
	LIST_FOREACH(bif, &sc->sc_iflist, bif_next)
		count++;
	LIST_FOREACH(bif, &sc->sc_spanlist, bif_next)
		count++;

	buflen = sizeof(breq) * count;
	if (bifc->ifbic_len == 0) {
		bifc->ifbic_len = buflen;
		return (0);
	}
	BRIDGE_UNLOCK(sc);
	outbuf = malloc(buflen, M_TEMP, M_WAITOK | M_ZERO);
	BRIDGE_LOCK(sc);

	count = 0;
	buf = outbuf;
	len = min(bifc->ifbic_len, buflen);
	bzero(&breq, sizeof(breq));
	LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		if (len < sizeof(breq))
			break;

		strlcpy(breq.ifbr_ifsname, bif->bif_ifp->if_xname,
		    sizeof(breq.ifbr_ifsname));
		/* Fill in the ifbreq structure */
		error = bridge_ioctl_gifflags(sc, &breq);
		if (error)
			break;
		memcpy(buf, &breq, sizeof(breq));
		count++;
		buf += sizeof(breq);
		len -= sizeof(breq);
	}
	LIST_FOREACH(bif, &sc->sc_spanlist, bif_next) {
		if (len < sizeof(breq))
			break;

		strlcpy(breq.ifbr_ifsname, bif->bif_ifp->if_xname,
		    sizeof(breq.ifbr_ifsname));
		breq.ifbr_ifsflags = bif->bif_flags;
		breq.ifbr_portno = bif->bif_ifp->if_index & 0xfff;
		memcpy(buf, &breq, sizeof(breq));
		count++;
		buf += sizeof(breq);
		len -= sizeof(breq);
	}

	BRIDGE_UNLOCK(sc);
	bifc->ifbic_len = sizeof(breq) * count;
	error = copyout(outbuf, bifc->ifbic_req, bifc->ifbic_len);
	BRIDGE_LOCK(sc);
	free(outbuf, M_TEMP);
	return (error);
}

static int
bridge_ioctl_rts(struct bridge_softc *sc, void *arg)
{
	struct ifbaconf *bac = arg;
	struct bridge_rtnode *brt;
	struct ifbareq bareq;
	char *buf, *outbuf;
	int count, buflen, len, error = 0;

	if (bac->ifbac_len == 0)
		return (0);

	count = 0;
	LIST_FOREACH(brt, &sc->sc_rtlist, brt_list)
		count++;
	buflen = sizeof(bareq) * count;

	BRIDGE_UNLOCK(sc);
	outbuf = malloc(buflen, M_TEMP, M_WAITOK | M_ZERO);
	BRIDGE_LOCK(sc);

	count = 0;
	buf = outbuf;
	len = min(bac->ifbac_len, buflen);
	bzero(&bareq, sizeof(bareq));
	LIST_FOREACH(brt, &sc->sc_rtlist, brt_list) {
		if (len < sizeof(bareq))
			goto out;
		strlcpy(bareq.ifba_ifsname, brt->brt_ifp->if_xname,
		    sizeof(bareq.ifba_ifsname));
		memcpy(bareq.ifba_dst, brt->brt_addr, sizeof(brt->brt_addr));
		bareq.ifba_vlan = brt->brt_vlan;
		if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC &&
				time_uptime < brt->brt_expire)
			bareq.ifba_expire = brt->brt_expire - time_uptime;
		else
			bareq.ifba_expire = 0;
		bareq.ifba_flags = brt->brt_flags;

		memcpy(buf, &bareq, sizeof(bareq));
		count++;
		buf += sizeof(bareq);
		len -= sizeof(bareq);
	}
out:
	BRIDGE_UNLOCK(sc);
	bac->ifbac_len = sizeof(bareq) * count;
	error = copyout(outbuf, bac->ifbac_req, bac->ifbac_len);
	BRIDGE_LOCK(sc);
	free(outbuf, M_TEMP);
	return (error);
}

static int
bridge_ioctl_saddr(struct bridge_softc *sc, void *arg)
{
	struct ifbareq *req = arg;
	struct bridge_iflist *bif;
	int error;

	bif = bridge_lookup_member(sc, req->ifba_ifsname);
	if (bif == NULL)
		return (ENOENT);

	error = bridge_rtupdate(sc, req->ifba_dst, req->ifba_vlan, bif, 1,
	    req->ifba_flags);

	return (error);
}

static int
bridge_ioctl_sto(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	sc->sc_brttimeout = param->ifbrp_ctime;
	return (0);
}

static int
bridge_ioctl_gto(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	param->ifbrp_ctime = sc->sc_brttimeout;
	return (0);
}

static int
bridge_ioctl_daddr(struct bridge_softc *sc, void *arg)
{
	struct ifbareq *req = arg;

	return (bridge_rtdaddr(sc, req->ifba_dst, req->ifba_vlan));
}

static int
bridge_ioctl_flush(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;

	bridge_rtflush(sc, req->ifbr_ifsflags);
	return (0);
}

static int
bridge_ioctl_gpri(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;
	struct bstp_state *bs = &sc->sc_stp;

	param->ifbrp_prio = bs->bs_bridge_priority;
	return (0);
}

static int
bridge_ioctl_spri(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	return (bstp_set_priority(&sc->sc_stp, param->ifbrp_prio));
}

static int
bridge_ioctl_ght(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;
	struct bstp_state *bs = &sc->sc_stp;

	param->ifbrp_hellotime = bs->bs_bridge_htime >> 8;
	return (0);
}

static int
bridge_ioctl_sht(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	return (bstp_set_htime(&sc->sc_stp, param->ifbrp_hellotime));
}

static int
bridge_ioctl_gfd(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;
	struct bstp_state *bs = &sc->sc_stp;

	param->ifbrp_fwddelay = bs->bs_bridge_fdelay >> 8;
	return (0);
}

static int
bridge_ioctl_sfd(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	return (bstp_set_fdelay(&sc->sc_stp, param->ifbrp_fwddelay));
}

static int
bridge_ioctl_gma(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;
	struct bstp_state *bs = &sc->sc_stp;

	param->ifbrp_maxage = bs->bs_bridge_max_age >> 8;
	return (0);
}

static int
bridge_ioctl_sma(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	return (bstp_set_maxage(&sc->sc_stp, param->ifbrp_maxage));
}

static int
bridge_ioctl_sifprio(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	return (bstp_set_port_priority(&bif->bif_stp, req->ifbr_priority));
}

static int
bridge_ioctl_sifcost(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	return (bstp_set_path_cost(&bif->bif_stp, req->ifbr_path_cost));
}

static int
bridge_ioctl_addspan(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif = NULL;
	struct ifnet *ifs;

	ifs = ifunit(req->ifbr_ifsname);
	if (ifs == NULL)
		return (ENOENT);

	LIST_FOREACH(bif, &sc->sc_spanlist, bif_next)
		if (ifs == bif->bif_ifp)
			return (EBUSY);

	if (ifs->if_bridge != NULL)
		return (EBUSY);

	switch (ifs->if_type) {
		case IFT_ETHER:
		case IFT_GIF:
		case IFT_L2VLAN:
			break;
		default:
			return (EINVAL);
	}

	bif = malloc(sizeof(*bif), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (bif == NULL)
		return (ENOMEM);

	bif->bif_ifp = ifs;
	bif->bif_flags = IFBIF_SPAN;

	LIST_INSERT_HEAD(&sc->sc_spanlist, bif, bif_next);

	return (0);
}

static int
bridge_ioctl_delspan(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;
	struct ifnet *ifs;

	ifs = ifunit(req->ifbr_ifsname);
	if (ifs == NULL)
		return (ENOENT);

	LIST_FOREACH(bif, &sc->sc_spanlist, bif_next)
		if (ifs == bif->bif_ifp)
			break;

	if (bif == NULL)
		return (ENOENT);

	bridge_delete_span(sc, bif);

	return (0);
}

static int
bridge_ioctl_gbparam(struct bridge_softc *sc, void *arg)
{
	struct ifbropreq *req = arg;
	struct bstp_state *bs = &sc->sc_stp;
	struct bstp_port *root_port;

	req->ifbop_maxage = bs->bs_bridge_max_age >> 8;
	req->ifbop_hellotime = bs->bs_bridge_htime >> 8;
	req->ifbop_fwddelay = bs->bs_bridge_fdelay >> 8;

	root_port = bs->bs_root_port;
	if (root_port == NULL)
		req->ifbop_root_port = 0;
	else
		req->ifbop_root_port = root_port->bp_ifp->if_index;

	req->ifbop_holdcount = bs->bs_txholdcount;
	req->ifbop_priority = bs->bs_bridge_priority;
	req->ifbop_protocol = bs->bs_protover;
	req->ifbop_root_path_cost = bs->bs_root_pv.pv_cost;
	req->ifbop_bridgeid = bs->bs_bridge_pv.pv_dbridge_id;
	req->ifbop_designated_root = bs->bs_root_pv.pv_root_id;
	req->ifbop_designated_bridge = bs->bs_root_pv.pv_dbridge_id;
	req->ifbop_last_tc_time.tv_sec = bs->bs_last_tc_time.tv_sec;
	req->ifbop_last_tc_time.tv_usec = bs->bs_last_tc_time.tv_usec;

	return (0);
}

static int
bridge_ioctl_grte(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	param->ifbrp_cexceeded = sc->sc_brtexceeded;
	return (0);
}

static int
bridge_ioctl_gifsstp(struct bridge_softc *sc, void *arg)
{
	struct ifbpstpconf *bifstp = arg;
	struct bridge_iflist *bif;
	struct bstp_port *bp;
	struct ifbpstpreq bpreq;
	char *buf, *outbuf;
	int count, buflen, len, error = 0;

	count = 0;
	LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		if ((bif->bif_flags & IFBIF_STP) != 0)
			count++;
	}

	buflen = sizeof(bpreq) * count;
	if (bifstp->ifbpstp_len == 0) {
		bifstp->ifbpstp_len = buflen;
		return (0);
	}

	BRIDGE_UNLOCK(sc);
	outbuf = malloc(buflen, M_TEMP, M_WAITOK | M_ZERO);
	BRIDGE_LOCK(sc);

	count = 0;
	buf = outbuf;
	len = min(bifstp->ifbpstp_len, buflen);
	bzero(&bpreq, sizeof(bpreq));
	LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		if (len < sizeof(bpreq))
			break;

		if ((bif->bif_flags & IFBIF_STP) == 0)
			continue;

		bp = &bif->bif_stp;
		bpreq.ifbp_portno = bif->bif_ifp->if_index & 0xfff;
		bpreq.ifbp_fwd_trans = bp->bp_forward_transitions;
		bpreq.ifbp_design_cost = bp->bp_desg_pv.pv_cost;
		bpreq.ifbp_design_port = bp->bp_desg_pv.pv_port_id;
		bpreq.ifbp_design_bridge = bp->bp_desg_pv.pv_dbridge_id;
		bpreq.ifbp_design_root = bp->bp_desg_pv.pv_root_id;

		memcpy(buf, &bpreq, sizeof(bpreq));
		count++;
		buf += sizeof(bpreq);
		len -= sizeof(bpreq);
	}

	BRIDGE_UNLOCK(sc);
	bifstp->ifbpstp_len = sizeof(bpreq) * count;
	error = copyout(outbuf, bifstp->ifbpstp_req, bifstp->ifbpstp_len);
	BRIDGE_LOCK(sc);
	free(outbuf, M_TEMP);
	return (error);
}

static int
bridge_ioctl_sproto(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	return (bstp_set_protocol(&sc->sc_stp, param->ifbrp_proto));
}

static int
bridge_ioctl_stxhc(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	return (bstp_set_holdcount(&sc->sc_stp, param->ifbrp_txhc));
}

/*
 * bridge_ifdetach:
 *
 *	Detach an interface from a bridge.  Called when a member
 *	interface is detaching.
 */
static void
bridge_ifdetach(void *arg __unused, struct ifnet *ifp)
{
	struct bridge_softc *sc = ifp->if_bridge;
	struct bridge_iflist *bif;

	/* Check if the interface is a bridge member */
	if (sc != NULL) {
		BRIDGE_LOCK(sc);

		bif = bridge_lookup_member_if(sc, ifp);
		if (bif != NULL)
			bridge_delete_member(sc, bif, 1);

		BRIDGE_UNLOCK(sc);
		return;
	}

	/* Check if the interface is a span port */
	mtx_lock(&bridge_list_mtx);
	LIST_FOREACH(sc, &bridge_list, sc_list) {
		BRIDGE_LOCK(sc);
		LIST_FOREACH(bif, &sc->sc_spanlist, bif_next)
			if (ifp == bif->bif_ifp) {
				bridge_delete_span(sc, bif);
				break;
			}

		BRIDGE_UNLOCK(sc);
	}
	mtx_unlock(&bridge_list_mtx);
}

/*
 * bridge_init:
 *
 *	Initialize a bridge interface.
 */
static void
bridge_init(void *xsc)
{
	struct bridge_softc *sc = (struct bridge_softc *)xsc;
	struct ifnet *ifp = sc->sc_ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	BRIDGE_LOCK(sc);
	callout_reset(&sc->sc_brcallout, bridge_rtable_prune_period * hz,
	    bridge_timer, sc);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	bstp_init(&sc->sc_stp);		/* Initialize Spanning Tree */

	BRIDGE_UNLOCK(sc);
}

/*
 * bridge_stop:
 *
 *	Stop the bridge interface.
 */
static void
bridge_stop(struct ifnet *ifp, int disable)
{
	struct bridge_softc *sc = ifp->if_softc;

	BRIDGE_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	callout_stop(&sc->sc_brcallout);
	bstp_stop(&sc->sc_stp);

	bridge_rtflush(sc, IFBF_FLUSHDYN);

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
}

/*
 * bridge_enqueue:
 *
 *	Enqueue a packet on a bridge member interface.
 *
 */
static void
bridge_enqueue(struct bridge_softc *sc, struct ifnet *dst_ifp, struct mbuf *m)
{
	int len, err = 0;
	short mflags;
	struct mbuf *m0;

	len = m->m_pkthdr.len;
	mflags = m->m_flags;

	/* We may be sending a fragment so traverse the mbuf */
	for (; m; m = m0) {
		m0 = m->m_nextpkt;
		m->m_nextpkt = NULL;

		/*
		 * If underlying interface can not do VLAN tag insertion itself
		 * then attach a packet tag that holds it.
		 */
		if ((m->m_flags & M_VLANTAG) &&
		    (dst_ifp->if_capenable & IFCAP_VLAN_HWTAGGING) == 0) {
			m = ether_vlanencap(m, m->m_pkthdr.ether_vtag);
			if (m == NULL) {
				if_printf(dst_ifp,
				    "unable to prepend VLAN header\n");
				dst_ifp->if_oerrors++;
				continue;
			}
			m->m_flags &= ~M_VLANTAG;
		}

		if (err == 0)
			IFQ_ENQUEUE(&dst_ifp->if_snd, m, err);
	}

	if (err == 0) {

		sc->sc_ifp->if_opackets++;
		sc->sc_ifp->if_obytes += len;

		dst_ifp->if_obytes += len;

		if (mflags & M_MCAST) {
			sc->sc_ifp->if_omcasts++;
			dst_ifp->if_omcasts++;
		}
	}

	if ((dst_ifp->if_drv_flags & IFF_DRV_OACTIVE) == 0)
		(*dst_ifp->if_start)(dst_ifp);
}

/*
 * bridge_dummynet:
 *
 * 	Receive a queued packet from dummynet and pass it on to the output
 * 	interface.
 *
 *	The mbuf has the Ethernet header already attached.
 */
static void
bridge_dummynet(struct mbuf *m, struct ifnet *ifp)
{
	struct bridge_softc *sc;

	sc = ifp->if_bridge;

	/*
	 * The packet didnt originate from a member interface. This should only
	 * ever happen if a member interface is removed while packets are
	 * queued for it.
	 */
	if (sc == NULL) {
		m_freem(m);
		return;
	}

	if (PFIL_HOOKED(&inet_pfil_hook)
#ifdef INET6
	    || PFIL_HOOKED(&inet6_pfil_hook)
#endif
	    ) {
		if (bridge_pfil(&m, sc->sc_ifp, ifp, PFIL_OUT) != 0)
			return;
		if (m == NULL)
			return;
	}

	bridge_enqueue(sc, ifp, m);
}

/*
 * bridge_output:
 *
 *	Send output from a bridge member interface.  This
 *	performs the bridging function for locally originated
 *	packets.
 *
 *	The mbuf has the Ethernet header already attached.  We must
 *	enqueue or free the mbuf before returning.
 */
static int
bridge_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *sa,
    struct rtentry *rt)
{
	struct ether_header *eh;
	struct ifnet *dst_if;
	struct bridge_softc *sc;
	uint16_t vlan;

	if (m->m_len < ETHER_HDR_LEN) {
		m = m_pullup(m, ETHER_HDR_LEN);
		if (m == NULL)
			return (0);
	}

	eh = mtod(m, struct ether_header *);
	sc = ifp->if_bridge;
	vlan = VLANTAGOF(m);

	BRIDGE_LOCK(sc);

	/*
	 * If bridge is down, but the original output interface is up,
	 * go ahead and send out that interface.  Otherwise, the packet
	 * is dropped below.
	 */
	if ((sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		dst_if = ifp;
		goto sendunicast;
	}

	/*
	 * If the packet is a multicast, or we don't know a better way to
	 * get there, send to all interfaces.
	 */
	if (ETHER_IS_MULTICAST(eh->ether_dhost))
		dst_if = NULL;
	else
		dst_if = bridge_rtlookup(sc, eh->ether_dhost, vlan);
	if (dst_if == NULL) {
		struct bridge_iflist *bif;
		struct mbuf *mc;
		int error = 0, used = 0;

		bridge_span(sc, m);

		BRIDGE_LOCK2REF(sc, error);
		if (error) {
			m_freem(m);
			return (0);
		}

		LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
			dst_if = bif->bif_ifp;

			if (dst_if->if_type == IFT_GIF)
				continue;
			if ((dst_if->if_drv_flags & IFF_DRV_RUNNING) == 0)
				continue;

			/*
			 * If this is not the original output interface,
			 * and the interface is participating in spanning
			 * tree, make sure the port is in a state that
			 * allows forwarding.
			 */
			if (dst_if != ifp && (bif->bif_flags & IFBIF_STP) &&
			    bif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING)
				continue;

			if (LIST_NEXT(bif, bif_next) == NULL) {
				used = 1;
				mc = m;
			} else {
				mc = m_copypacket(m, M_DONTWAIT);
				if (mc == NULL) {
					sc->sc_ifp->if_oerrors++;
					continue;
				}
			}

			bridge_enqueue(sc, dst_if, mc);
		}
		if (used == 0)
			m_freem(m);
		BRIDGE_UNREF(sc);
		return (0);
	}

sendunicast:
	/*
	 * XXX Spanning tree consideration here?
	 */

	bridge_span(sc, m);
	if ((dst_if->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		m_freem(m);
		BRIDGE_UNLOCK(sc);
		return (0);
	}

	BRIDGE_UNLOCK(sc);
	bridge_enqueue(sc, dst_if, m);
	return (0);
}

/*
 * bridge_start:
 *
 *	Start output on a bridge.
 *
 */
static void
bridge_start(struct ifnet *ifp)
{
	struct bridge_softc *sc;
	struct mbuf *m;
	struct ether_header *eh;
	struct ifnet *dst_if;

	sc = ifp->if_softc;

	ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;
		ETHER_BPF_MTAP(ifp, m);

		eh = mtod(m, struct ether_header *);
		dst_if = NULL;

		BRIDGE_LOCK(sc);
		if ((m->m_flags & (M_BCAST|M_MCAST)) == 0) {
			dst_if = bridge_rtlookup(sc, eh->ether_dhost, 1);
		}

		if (dst_if == NULL)
			bridge_broadcast(sc, ifp, m, 0);
		else {
			BRIDGE_UNLOCK(sc);
			bridge_enqueue(sc, dst_if, m);
		}
	}
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
}

/*
 * bridge_forward:
 *
 *	The forwarding function of the bridge.
 *
 *	NOTE: Releases the lock on return.
 */
static void
bridge_forward(struct bridge_softc *sc, struct bridge_iflist *sbif,
    struct mbuf *m)
{
	struct bridge_iflist *dbif;
	struct ifnet *src_if, *dst_if, *ifp;
	struct ether_header *eh;
	uint16_t vlan;

	src_if = m->m_pkthdr.rcvif;
	ifp = sc->sc_ifp;

	sc->sc_ifp->if_ipackets++;
	sc->sc_ifp->if_ibytes += m->m_pkthdr.len;
	vlan = VLANTAGOF(m);

	if ((sbif->bif_flags & IFBIF_STP) &&
	    sbif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
		BRIDGE_UNLOCK(sc);
		m_freem(m);
		return;
	}

	eh = mtod(m, struct ether_header *);

	/*
	 * If the interface is learning, and the source
	 * address is valid and not multicast, record
	 * the address.
	 */
	if ((sbif->bif_flags & IFBIF_LEARNING) != 0 &&
	    ETHER_IS_MULTICAST(eh->ether_shost) == 0 &&
	    (eh->ether_shost[0] == 0 &&
	     eh->ether_shost[1] == 0 &&
	     eh->ether_shost[2] == 0 &&
	     eh->ether_shost[3] == 0 &&
	     eh->ether_shost[4] == 0 &&
	     eh->ether_shost[5] == 0) == 0) {
		(void) bridge_rtupdate(sc, eh->ether_shost, vlan,
		    sbif, 0, IFBAF_DYNAMIC);
	}

	if ((sbif->bif_flags & IFBIF_STP) != 0 &&
	    sbif->bif_stp.bp_state == BSTP_IFSTATE_LEARNING) {
		m_freem(m);
		BRIDGE_UNLOCK(sc);
		return;
	}

	/*
	 * At this point, the port either doesn't participate
	 * in spanning tree or it is in the forwarding state.
	 */

	/*
	 * If the packet is unicast, destined for someone on
	 * "this" side of the bridge, drop it.
	 */
	if ((m->m_flags & (M_BCAST|M_MCAST)) == 0) {
		dst_if = bridge_rtlookup(sc, eh->ether_dhost, vlan);
		if (src_if == dst_if) {
			BRIDGE_UNLOCK(sc);
			m_freem(m);
			return;
		}
	} else {
		/* ...forward it to all interfaces. */
		sc->sc_ifp->if_imcasts++;
		dst_if = NULL;
	}

	/*
	 * If we have a destination interface which is a member of our bridge,
	 * OR this is a unicast packet, push it through the bpf(4) machinery.
	 * For broadcast or multicast packets, don't bother because it will
	 * be reinjected into ether_input. We do this before we pass the packets
	 * through the pfil(9) framework, as it is possible that pfil(9) will
	 * drop the packet, or possibly modify it, making it difficult to debug
	 * firewall issues on the bridge.
	 */
	if (dst_if != NULL || (m->m_flags & (M_BCAST | M_MCAST)) == 0)
		ETHER_BPF_MTAP(ifp, m);

	/* run the packet filter */
	if (PFIL_HOOKED(&inet_pfil_hook)
#ifdef INET6
	    || PFIL_HOOKED(&inet6_pfil_hook)
#endif
	    ) {
		BRIDGE_UNLOCK(sc);
		if (bridge_pfil(&m, ifp, src_if, PFIL_IN) != 0)
			return;
		if (m == NULL)
			return;
		BRIDGE_LOCK(sc);
	}

	if (dst_if == NULL) {
		bridge_broadcast(sc, src_if, m, 1);
		return;
	}

	/*
	 * At this point, we're dealing with a unicast frame
	 * going to a different interface.
	 */
	if ((dst_if->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		BRIDGE_UNLOCK(sc);
		m_freem(m);
		return;
	}
	dbif = bridge_lookup_member_if(sc, dst_if);
	if (dbif == NULL) {
		/* Not a member of the bridge (anymore?) */
		BRIDGE_UNLOCK(sc);
		m_freem(m);
		return;
	}

	/* Private segments can not talk to each other */
	if (sbif->bif_flags & dbif->bif_flags & IFBIF_PRIVATE) {
		BRIDGE_UNLOCK(sc);
		m_freem(m);
		return;
	}

	if ((dbif->bif_flags & IFBIF_STP) &&
	    dbif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
		BRIDGE_UNLOCK(sc);
		m_freem(m);
		return;
	}

	BRIDGE_UNLOCK(sc);

	if (PFIL_HOOKED(&inet_pfil_hook)
#ifdef INET6
	    || PFIL_HOOKED(&inet6_pfil_hook)
#endif
	    ) {
		if (bridge_pfil(&m, sc->sc_ifp, dst_if, PFIL_OUT) != 0)
			return;
		if (m == NULL)
			return;
	}

	bridge_enqueue(sc, dst_if, m);
}

/*
 * bridge_input:
 *
 *	Receive input from a member interface.  Queue the packet for
 *	bridging if it is not for us.
 */
static struct mbuf *
bridge_input(struct ifnet *ifp, struct mbuf *m)
{
	struct bridge_softc *sc = ifp->if_bridge;
	struct bridge_iflist *bif, *bif2;
	struct ifnet *bifp;
	struct ether_header *eh;
	struct mbuf *mc, *mc2;
	uint16_t vlan;

	if ((sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return (m);

	bifp = sc->sc_ifp;
	vlan = VLANTAGOF(m);

	/*
	 * Implement support for bridge monitoring. If this flag has been
	 * set on this interface, discard the packet once we push it through
	 * the bpf(4) machinery, but before we do, increment the byte and
	 * packet counters associated with this interface.
	 */
	if ((bifp->if_flags & IFF_MONITOR) != 0) {
		m->m_pkthdr.rcvif  = bifp;
		ETHER_BPF_MTAP(bifp, m);
		bifp->if_ipackets++;
		bifp->if_ibytes += m->m_pkthdr.len;
		m_freem(m);
		return (NULL);
	}
	BRIDGE_LOCK(sc);
	bif = bridge_lookup_member_if(sc, ifp);
	if (bif == NULL) {
		BRIDGE_UNLOCK(sc);
		return (m);
	}

	eh = mtod(m, struct ether_header *);

	if (memcmp(eh->ether_dhost, IF_LLADDR(bifp),
	    ETHER_ADDR_LEN) == 0) {
		/* Block redundant paths to us */
		if ((bif->bif_flags & IFBIF_STP) &&
		    bif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
			BRIDGE_UNLOCK(sc);
			return (m);
		}

		/*
		 * Filter on the physical interface.
		 */
		if (pfil_local_phys && (PFIL_HOOKED(&inet_pfil_hook)
#ifdef INET6
		    || PFIL_HOOKED(&inet6_pfil_hook)
#endif
		    )) {
			if (bridge_pfil(&m, NULL, ifp, PFIL_IN) != 0 ||
			    m == NULL) {
				BRIDGE_UNLOCK(sc);
				return (NULL);
			}
		}

		/*
		 * If the packet is for us, set the packets source as the
		 * bridge, and return the packet back to ether_input for
		 * local processing.
		 */

		/* Note where to send the reply to */
		if (bif->bif_flags & IFBIF_LEARNING)
			(void) bridge_rtupdate(sc,
			    eh->ether_shost, vlan, bif, 0, IFBAF_DYNAMIC);

		/* Mark the packet as arriving on the bridge interface */
		m->m_pkthdr.rcvif = bifp;
		ETHER_BPF_MTAP(bifp, m);
		bifp->if_ipackets++;

		BRIDGE_UNLOCK(sc);
		return (m);
	}

	bridge_span(sc, m);

	if (m->m_flags & (M_BCAST|M_MCAST)) {
		/* Tap off 802.1D packets; they do not get forwarded. */
		if (memcmp(eh->ether_dhost, bstp_etheraddr,
		    ETHER_ADDR_LEN) == 0) {
			m = bstp_input(&bif->bif_stp, ifp, m);
			if (m == NULL) {
				BRIDGE_UNLOCK(sc);
				return (NULL);
			}
		}

		if ((bif->bif_flags & IFBIF_STP) &&
		    bif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
			BRIDGE_UNLOCK(sc);
			return (m);
		}

		/*
		 * Make a deep copy of the packet and enqueue the copy
		 * for bridge processing; return the original packet for
		 * local processing.
		 */
		mc = m_dup(m, M_DONTWAIT);
		if (mc == NULL) {
			BRIDGE_UNLOCK(sc);
			return (m);
		}

		/* Perform the bridge forwarding function with the copy. */
		bridge_forward(sc, bif, mc);

		/*
		 * Reinject the mbuf as arriving on the bridge so we have a
		 * chance at claiming multicast packets. We can not loop back
		 * here from ether_input as a bridge is never a member of a
		 * bridge.
		 */
		KASSERT(bifp->if_bridge == NULL,
		    ("loop created in bridge_input"));
		mc2 = m_dup(m, M_DONTWAIT);
		if (mc2 != NULL) {
			/* Keep the layer3 header aligned */
			int i = min(mc2->m_pkthdr.len, max_protohdr);
			mc2 = m_copyup(mc2, i, ETHER_ALIGN);
		}
		if (mc2 != NULL) {
			mc2->m_pkthdr.rcvif = bifp;
			(*bifp->if_input)(bifp, mc2);
		}

		/* Return the original packet for local processing. */
		return (m);
	}

	if ((bif->bif_flags & IFBIF_STP) &&
	    bif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
		BRIDGE_UNLOCK(sc);
		return (m);
	}

#ifdef DEV_CARP
#   define OR_CARP_CHECK_WE_ARE_DST(iface) \
	|| ((iface)->if_carp \
	    && carp_forus((iface)->if_carp, eh->ether_dhost))
#   define OR_CARP_CHECK_WE_ARE_SRC(iface) \
	|| ((iface)->if_carp \
	    && carp_forus((iface)->if_carp, eh->ether_shost))
#else
#   define OR_CARP_CHECK_WE_ARE_DST(iface)
#   define OR_CARP_CHECK_WE_ARE_SRC(iface)
#endif

#define GRAB_OUR_PACKETS(iface) \
	if ((iface)->if_type == IFT_GIF) \
		continue; \
	/* It is destined for us. */ \
	if (memcmp(IF_LLADDR((iface)), eh->ether_dhost,  ETHER_ADDR_LEN) == 0 \
	    OR_CARP_CHECK_WE_ARE_DST((iface))				\
	    ) {								\
		if (bif->bif_flags & IFBIF_LEARNING)			\
			(void) bridge_rtupdate(sc, eh->ether_shost,	\
			    vlan, bif, 0, IFBAF_DYNAMIC);		\
		m->m_pkthdr.rcvif = iface;				\
		BRIDGE_UNLOCK(sc);					\
		return (m);						\
	}								\
									\
	/* We just received a packet that we sent out. */		\
	if (memcmp(IF_LLADDR((iface)), eh->ether_shost, ETHER_ADDR_LEN) == 0 \
	    OR_CARP_CHECK_WE_ARE_SRC((iface))			\
	    ) {								\
		BRIDGE_UNLOCK(sc);					\
		m_freem(m);						\
		return (NULL);						\
	}

	/*
	 * Unicast.  Make sure it's not for us.
	 *
	 * Give a chance for ifp at first priority. This will help when	the
	 * packet comes through the interface like VLAN's with the same MACs
	 * on several interfaces from the same bridge. This also will save
	 * some CPU cycles in case the destination interface and the input
	 * interface (eq ifp) are the same.
	 */
	do { GRAB_OUR_PACKETS(ifp) } while (0);

	/* Now check the all bridge members. */
	LIST_FOREACH(bif2, &sc->sc_iflist, bif_next) {
		GRAB_OUR_PACKETS(bif2->bif_ifp)
	}

#undef OR_CARP_CHECK_WE_ARE_DST
#undef OR_CARP_CHECK_WE_ARE_SRC
#undef GRAB_OUR_PACKETS

	/* Perform the bridge forwarding function. */
	bridge_forward(sc, bif, m);

	return (NULL);
}

/*
 * bridge_broadcast:
 *
 *	Send a frame to all interfaces that are members of
 *	the bridge, except for the one on which the packet
 *	arrived.
 *
 *	NOTE: Releases the lock on return.
 */
static void
bridge_broadcast(struct bridge_softc *sc, struct ifnet *src_if,
    struct mbuf *m, int runfilt)
{
	struct bridge_iflist *dbif, *sbif;
	struct mbuf *mc;
	struct ifnet *dst_if;
	int error = 0, used = 0, i;

	sbif = bridge_lookup_member_if(sc, src_if);

	BRIDGE_LOCK2REF(sc, error);
	if (error) {
		m_freem(m);
		return;
	}

	/* Filter on the bridge interface before broadcasting */
	if (runfilt && (PFIL_HOOKED(&inet_pfil_hook)
#ifdef INET6
	    || PFIL_HOOKED(&inet6_pfil_hook)
#endif
	    )) {
		if (bridge_pfil(&m, sc->sc_ifp, NULL, PFIL_OUT) != 0)
			goto out;
		if (m == NULL)
			goto out;
	}

	LIST_FOREACH(dbif, &sc->sc_iflist, bif_next) {
		dst_if = dbif->bif_ifp;
		if (dst_if == src_if)
			continue;

		/* Private segments can not talk to each other */
		if (sbif && (sbif->bif_flags & dbif->bif_flags & IFBIF_PRIVATE))
			continue;

		if ((dbif->bif_flags & IFBIF_STP) &&
		    dbif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING)
			continue;

		if ((dbif->bif_flags & IFBIF_DISCOVER) == 0 &&
		    (m->m_flags & (M_BCAST|M_MCAST)) == 0)
			continue;

		if ((dst_if->if_drv_flags & IFF_DRV_RUNNING) == 0)
			continue;

		if (LIST_NEXT(dbif, bif_next) == NULL) {
			mc = m;
			used = 1;
		} else {
			mc = m_dup(m, M_DONTWAIT);
			if (mc == NULL) {
				sc->sc_ifp->if_oerrors++;
				continue;
			}
		}

		/*
		 * Filter on the output interface. Pass a NULL bridge interface
		 * pointer so we do not redundantly filter on the bridge for
		 * each interface we broadcast on.
		 */
		if (runfilt && (PFIL_HOOKED(&inet_pfil_hook)
#ifdef INET6
		    || PFIL_HOOKED(&inet6_pfil_hook)
#endif
		    )) {
			if (used == 0) {
				/* Keep the layer3 header aligned */
				i = min(mc->m_pkthdr.len, max_protohdr);
				mc = m_copyup(mc, i, ETHER_ALIGN);
				if (mc == NULL) {
					sc->sc_ifp->if_oerrors++;
					continue;
				}
			}
			if (bridge_pfil(&mc, NULL, dst_if, PFIL_OUT) != 0)
				continue;
			if (mc == NULL)
				continue;
		}

		bridge_enqueue(sc, dst_if, mc);
	}
	if (used == 0)
		m_freem(m);

out:
	BRIDGE_UNREF(sc);
}

/*
 * bridge_span:
 *
 *	Duplicate a packet out one or more interfaces that are in span mode,
 *	the original mbuf is unmodified.
 */
static void
bridge_span(struct bridge_softc *sc, struct mbuf *m)
{
	struct bridge_iflist *bif;
	struct ifnet *dst_if;
	struct mbuf *mc;

	if (LIST_EMPTY(&sc->sc_spanlist))
		return;

	LIST_FOREACH(bif, &sc->sc_spanlist, bif_next) {
		dst_if = bif->bif_ifp;

		if ((dst_if->if_drv_flags & IFF_DRV_RUNNING) == 0)
			continue;

		mc = m_copypacket(m, M_DONTWAIT);
		if (mc == NULL) {
			sc->sc_ifp->if_oerrors++;
			continue;
		}

		bridge_enqueue(sc, dst_if, mc);
	}
}

/*
 * bridge_rtupdate:
 *
 *	Add a bridge routing entry.
 */
static int
bridge_rtupdate(struct bridge_softc *sc, const uint8_t *dst, uint16_t vlan,
    struct bridge_iflist *bif, int setflags, uint8_t flags)
{
	struct bridge_rtnode *brt;
	struct ifnet *dst_if = bif->bif_ifp;
	int error;

	BRIDGE_LOCK_ASSERT(sc);

	/* 802.1p frames map to vlan 1 */
	if (vlan == 0)
		vlan = 1;

	/*
	 * A route for this destination might already exist.  If so,
	 * update it, otherwise create a new one.
	 */
	if ((brt = bridge_rtnode_lookup(sc, dst, vlan)) == NULL) {
		if (sc->sc_brtcnt >= sc->sc_brtmax) {
			sc->sc_brtexceeded++;
			return (ENOSPC);
		}

		/*
		 * Allocate a new bridge forwarding node, and
		 * initialize the expiration time and Ethernet
		 * address.
		 */
		brt = uma_zalloc(bridge_rtnode_zone, M_NOWAIT | M_ZERO);
		if (brt == NULL)
			return (ENOMEM);

		if (bif->bif_flags & IFBIF_STICKY)
			brt->brt_flags = IFBAF_STICKY;
		else
			brt->brt_flags = IFBAF_DYNAMIC;

		brt->brt_ifp = dst_if;
		memcpy(brt->brt_addr, dst, ETHER_ADDR_LEN);
		brt->brt_vlan = vlan;

		if ((error = bridge_rtnode_insert(sc, brt)) != 0) {
			uma_zfree(bridge_rtnode_zone, brt);
			return (error);
		}
	}

	if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC)
		brt->brt_ifp = dst_if;
	if ((flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC)
		brt->brt_expire = time_uptime + sc->sc_brttimeout;
	if (setflags)
		brt->brt_flags = flags;

	return (0);
}

/*
 * bridge_rtlookup:
 *
 *	Lookup the destination interface for an address.
 */
static struct ifnet *
bridge_rtlookup(struct bridge_softc *sc, const uint8_t *addr, uint16_t vlan)
{
	struct bridge_rtnode *brt;

	BRIDGE_LOCK_ASSERT(sc);

	if ((brt = bridge_rtnode_lookup(sc, addr, vlan)) == NULL)
		return (NULL);

	return (brt->brt_ifp);
}

/*
 * bridge_rttrim:
 *
 *	Trim the routine table so that we have a number
 *	of routing entries less than or equal to the
 *	maximum number.
 */
static void
bridge_rttrim(struct bridge_softc *sc)
{
	struct bridge_rtnode *brt, *nbrt;

	BRIDGE_LOCK_ASSERT(sc);

	/* Make sure we actually need to do this. */
	if (sc->sc_brtcnt <= sc->sc_brtmax)
		return;

	/* Force an aging cycle; this might trim enough addresses. */
	bridge_rtage(sc);
	if (sc->sc_brtcnt <= sc->sc_brtmax)
		return;

	LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
		if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
			bridge_rtnode_destroy(sc, brt);
			if (sc->sc_brtcnt <= sc->sc_brtmax)
				return;
		}
	}
}

/*
 * bridge_timer:
 *
 *	Aging timer for the bridge.
 */
static void
bridge_timer(void *arg)
{
	struct bridge_softc *sc = arg;

	BRIDGE_LOCK_ASSERT(sc);

	bridge_rtage(sc);

	if (sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING)
		callout_reset(&sc->sc_brcallout,
		    bridge_rtable_prune_period * hz, bridge_timer, sc);
}

/*
 * bridge_rtage:
 *
 *	Perform an aging cycle.
 */
static void
bridge_rtage(struct bridge_softc *sc)
{
	struct bridge_rtnode *brt, *nbrt;

	BRIDGE_LOCK_ASSERT(sc);

	LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
		if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
			if (time_uptime >= brt->brt_expire)
				bridge_rtnode_destroy(sc, brt);
		}
	}
}

/*
 * bridge_rtflush:
 *
 *	Remove all dynamic addresses from the bridge.
 */
static void
bridge_rtflush(struct bridge_softc *sc, int full)
{
	struct bridge_rtnode *brt, *nbrt;

	BRIDGE_LOCK_ASSERT(sc);

	LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
		if (full || (brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC)
			bridge_rtnode_destroy(sc, brt);
	}
}

/*
 * bridge_rtdaddr:
 *
 *	Remove an address from the table.
 */
static int
bridge_rtdaddr(struct bridge_softc *sc, const uint8_t *addr, uint16_t vlan)
{
	struct bridge_rtnode *brt;
	int found = 0;

	BRIDGE_LOCK_ASSERT(sc);

	/*
	 * If vlan is zero then we want to delete for all vlans so the lookup
	 * may return more than one.
	 */
	while ((brt = bridge_rtnode_lookup(sc, addr, vlan)) != NULL) {
		bridge_rtnode_destroy(sc, brt);
		found = 1;
	}

	return (found ? 0 : ENOENT);
}

/*
 * bridge_rtdelete:
 *
 *	Delete routes to a speicifc member interface.
 */
static void
bridge_rtdelete(struct bridge_softc *sc, struct ifnet *ifp, int full)
{
	struct bridge_rtnode *brt, *nbrt;

	BRIDGE_LOCK_ASSERT(sc);

	LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
		if (brt->brt_ifp == ifp && (full ||
			    (brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC))
			bridge_rtnode_destroy(sc, brt);
	}
}

/*
 * bridge_rtable_init:
 *
 *	Initialize the route table for this bridge.
 */
static int
bridge_rtable_init(struct bridge_softc *sc)
{
	int i;

	sc->sc_rthash = malloc(sizeof(*sc->sc_rthash) * BRIDGE_RTHASH_SIZE,
	    M_DEVBUF, M_NOWAIT);
	if (sc->sc_rthash == NULL)
		return (ENOMEM);

	for (i = 0; i < BRIDGE_RTHASH_SIZE; i++)
		LIST_INIT(&sc->sc_rthash[i]);

	sc->sc_rthash_key = arc4random();

	LIST_INIT(&sc->sc_rtlist);

	return (0);
}

/*
 * bridge_rtable_fini:
 *
 *	Deconstruct the route table for this bridge.
 */
static void
bridge_rtable_fini(struct bridge_softc *sc)
{

	free(sc->sc_rthash, M_DEVBUF);
}

/*
 * The following hash function is adapted from "Hash Functions" by Bob Jenkins
 * ("Algorithm Alley", Dr. Dobbs Journal, September 1997).
 */
#define	mix(a, b, c)							\
do {									\
	a -= b; a -= c; a ^= (c >> 13);					\
	b -= c; b -= a; b ^= (a << 8);					\
	c -= a; c -= b; c ^= (b >> 13);					\
	a -= b; a -= c; a ^= (c >> 12);					\
	b -= c; b -= a; b ^= (a << 16);					\
	c -= a; c -= b; c ^= (b >> 5);					\
	a -= b; a -= c; a ^= (c >> 3);					\
	b -= c; b -= a; b ^= (a << 10);					\
	c -= a; c -= b; c ^= (b >> 15);					\
} while (/*CONSTCOND*/0)

static __inline uint32_t
bridge_rthash(struct bridge_softc *sc, const uint8_t *addr)
{
	uint32_t a = 0x9e3779b9, b = 0x9e3779b9, c = sc->sc_rthash_key;

	b += addr[5] << 8;
	b += addr[4];
	a += addr[3] << 24;
	a += addr[2] << 16;
	a += addr[1] << 8;
	a += addr[0];

	mix(a, b, c);

	return (c & BRIDGE_RTHASH_MASK);
}

#undef mix

static int
bridge_rtnode_addr_cmp(const uint8_t *a, const uint8_t *b)
{
	int i, d;

	for (i = 0, d = 0; i < ETHER_ADDR_LEN && d == 0; i++) {
		d = ((int)a[i]) - ((int)b[i]);
	}

	return (d);
}

/*
 * bridge_rtnode_lookup:
 *
 *	Look up a bridge route node for the specified destination. Compare the
 *	vlan id or if zero then just return the first match.
 */
static struct bridge_rtnode *
bridge_rtnode_lookup(struct bridge_softc *sc, const uint8_t *addr, uint16_t vlan)
{
	struct bridge_rtnode *brt;
	uint32_t hash;
	int dir;

	BRIDGE_LOCK_ASSERT(sc);

	hash = bridge_rthash(sc, addr);
	LIST_FOREACH(brt, &sc->sc_rthash[hash], brt_hash) {
		dir = bridge_rtnode_addr_cmp(addr, brt->brt_addr);
		if (dir == 0 && (brt->brt_vlan == vlan || vlan == 0))
			return (brt);
		if (dir > 0)
			return (NULL);
	}

	return (NULL);
}

/*
 * bridge_rtnode_insert:
 *
 *	Insert the specified bridge node into the route table.  We
 *	assume the entry is not already in the table.
 */
static int
bridge_rtnode_insert(struct bridge_softc *sc, struct bridge_rtnode *brt)
{
	struct bridge_rtnode *lbrt;
	uint32_t hash;
	int dir;

	BRIDGE_LOCK_ASSERT(sc);

	hash = bridge_rthash(sc, brt->brt_addr);

	lbrt = LIST_FIRST(&sc->sc_rthash[hash]);
	if (lbrt == NULL) {
		LIST_INSERT_HEAD(&sc->sc_rthash[hash], brt, brt_hash);
		goto out;
	}

	do {
		dir = bridge_rtnode_addr_cmp(brt->brt_addr, lbrt->brt_addr);
		if (dir == 0 && brt->brt_vlan == lbrt->brt_vlan)
			return (EEXIST);
		if (dir > 0) {
			LIST_INSERT_BEFORE(lbrt, brt, brt_hash);
			goto out;
		}
		if (LIST_NEXT(lbrt, brt_hash) == NULL) {
			LIST_INSERT_AFTER(lbrt, brt, brt_hash);
			goto out;
		}
		lbrt = LIST_NEXT(lbrt, brt_hash);
	} while (lbrt != NULL);

#ifdef DIAGNOSTIC
	panic("bridge_rtnode_insert: impossible");
#endif

out:
	LIST_INSERT_HEAD(&sc->sc_rtlist, brt, brt_list);
	sc->sc_brtcnt++;

	return (0);
}

/*
 * bridge_rtnode_destroy:
 *
 *	Destroy a bridge rtnode.
 */
static void
bridge_rtnode_destroy(struct bridge_softc *sc, struct bridge_rtnode *brt)
{
	BRIDGE_LOCK_ASSERT(sc);

	LIST_REMOVE(brt, brt_hash);

	LIST_REMOVE(brt, brt_list);
	sc->sc_brtcnt--;
	uma_zfree(bridge_rtnode_zone, brt);
}

/*
 * bridge_rtable_expire:
 *
 *	Set the expiry time for all routes on an interface.
 */
static void
bridge_rtable_expire(struct ifnet *ifp, int age)
{
	struct bridge_softc *sc = ifp->if_bridge;
	struct bridge_rtnode *brt;

	BRIDGE_LOCK(sc);

	/*
	 * If the age is zero then flush, otherwise set all the expiry times to
	 * age for the interface
	 */
	if (age == 0)
		bridge_rtdelete(sc, ifp, IFBF_FLUSHDYN);
	else {
		LIST_FOREACH(brt, &sc->sc_rtlist, brt_list) {
			/* Cap the expiry time to 'age' */
			if (brt->brt_ifp == ifp &&
			    brt->brt_expire > time_uptime + age &&
			    (brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC)
				brt->brt_expire = time_uptime + age;
		}
	}
	BRIDGE_UNLOCK(sc);
}

/*
 * bridge_state_change:
 *
 *	Callback from the bridgestp code when a port changes states.
 */
static void
bridge_state_change(struct ifnet *ifp, int state)
{
	struct bridge_softc *sc = ifp->if_bridge;
	static const char *stpstates[] = {
		"disabled",
		"listening",
		"learning",
		"forwarding",
		"blocking",
		"discarding"
	};

	if (log_stp)
		log(LOG_NOTICE, "%s: state changed to %s on %s\n",
		    sc->sc_ifp->if_xname, stpstates[state], ifp->if_xname);
}

/*
 * Send bridge packets through pfil if they are one of the types pfil can deal
 * with, or if they are ARP or REVARP.  (pfil will pass ARP and REVARP without
 * question.) If *bifp or *ifp are NULL then packet filtering is skipped for
 * that interface.
 */
static int
bridge_pfil(struct mbuf **mp, struct ifnet *bifp, struct ifnet *ifp, int dir)
{
	int snap, error, i, hlen;
	struct ether_header *eh1, eh2;
	struct ip_fw_args args;
	struct ip *ip;
	struct llc llc1;
	u_int16_t ether_type;

	snap = 0;
	error = -1;	/* Default error if not error == 0 */

#if 0
	/* we may return with the IP fields swapped, ensure its not shared */
	KASSERT(M_WRITABLE(*mp), ("%s: modifying a shared mbuf", __func__));
#endif

	if (pfil_bridge == 0 && pfil_member == 0 && pfil_ipfw == 0)
		return (0); /* filtering is disabled */

	i = min((*mp)->m_pkthdr.len, max_protohdr);
	if ((*mp)->m_len < i) {
	    *mp = m_pullup(*mp, i);
	    if (*mp == NULL) {
		printf("%s: m_pullup failed\n", __func__);
		return (-1);
	    }
	}

	eh1 = mtod(*mp, struct ether_header *);
	ether_type = ntohs(eh1->ether_type);

	/*
	 * Check for SNAP/LLC.
	 */
	if (ether_type < ETHERMTU) {
		struct llc *llc2 = (struct llc *)(eh1 + 1);

		if ((*mp)->m_len >= ETHER_HDR_LEN + 8 &&
		    llc2->llc_dsap == LLC_SNAP_LSAP &&
		    llc2->llc_ssap == LLC_SNAP_LSAP &&
		    llc2->llc_control == LLC_UI) {
			ether_type = htons(llc2->llc_un.type_snap.ether_type);
			snap = 1;
		}
	}

	/*
	 * If we're trying to filter bridge traffic, don't look at anything
	 * other than IP and ARP traffic.  If the filter doesn't understand
	 * IPv6, don't allow IPv6 through the bridge either.  This is lame
	 * since if we really wanted, say, an AppleTalk filter, we are hosed,
	 * but of course we don't have an AppleTalk filter to begin with.
	 * (Note that since pfil doesn't understand ARP it will pass *ALL*
	 * ARP traffic.)
	 */
	switch (ether_type) {
		case ETHERTYPE_ARP:
		case ETHERTYPE_REVARP:
			if (pfil_ipfw_arp == 0)
				return (0); /* Automatically pass */
			break;

		case ETHERTYPE_IP:
#ifdef INET6
		case ETHERTYPE_IPV6:
#endif /* INET6 */
			break;
		default:
			/*
			 * Check to see if the user wants to pass non-ip
			 * packets, these will not be checked by pfil(9) and
			 * passed unconditionally so the default is to drop.
			 */
			if (pfil_onlyip)
				goto bad;
	}

	/* Strip off the Ethernet header and keep a copy. */
	m_copydata(*mp, 0, ETHER_HDR_LEN, (caddr_t) &eh2);
	m_adj(*mp, ETHER_HDR_LEN);

	/* Strip off snap header, if present */
	if (snap) {
		m_copydata(*mp, 0, sizeof(struct llc), (caddr_t) &llc1);
		m_adj(*mp, sizeof(struct llc));
	}

	/*
	 * Check the IP header for alignment and errors
	 */
	if (dir == PFIL_IN) {
		switch (ether_type) {
			case ETHERTYPE_IP:
				error = bridge_ip_checkbasic(mp);
				break;
#ifdef INET6
			case ETHERTYPE_IPV6:
				error = bridge_ip6_checkbasic(mp);
				break;
#endif /* INET6 */
			default:
				error = 0;
		}
		if (error)
			goto bad;
	}

	if (IPFW_LOADED && pfil_ipfw != 0 && dir == PFIL_OUT && ifp != NULL) {
		error = -1;
		args.rule = ip_dn_claim_rule(*mp);
		if (args.rule != NULL && fw_one_pass)
			goto ipfwpass; /* packet already partially processed */

		args.m = *mp;
		args.oif = ifp;
		args.next_hop = NULL;
		args.eh = &eh2;
		args.inp = NULL;	/* used by ipfw uid/gid/jail rules */
		i = ip_fw_chk_ptr(&args);
		*mp = args.m;

		if (*mp == NULL)
			return (error);

		if (DUMMYNET_LOADED && (i == IP_FW_DUMMYNET)) {

			/* put the Ethernet header back on */
			M_PREPEND(*mp, ETHER_HDR_LEN, M_DONTWAIT);
			if (*mp == NULL)
				return (error);
			bcopy(&eh2, mtod(*mp, caddr_t), ETHER_HDR_LEN);

			/*
			 * Pass the pkt to dummynet, which consumes it. The
			 * packet will return to us via bridge_dummynet().
			 */
			args.oif = ifp;
			ip_dn_io_ptr(*mp, DN_TO_IFB_FWD, &args);
			return (error);
		}

		if (i != IP_FW_PASS) /* drop */
			goto bad;
	}

ipfwpass:
	error = 0;

	/*
	 * Run the packet through pfil
	 */
	switch (ether_type) {
	case ETHERTYPE_IP:
		/*
		 * before calling the firewall, swap fields the same as
		 * IP does. here we assume the header is contiguous
		 */
		ip = mtod(*mp, struct ip *);

		ip->ip_len = ntohs(ip->ip_len);
		ip->ip_off = ntohs(ip->ip_off);

		/*
		 * Run pfil on the member interface and the bridge, both can
		 * be skipped by clearing pfil_member or pfil_bridge.
		 *
		 * Keep the order:
		 *   in_if -> bridge_if -> out_if
		 */
		if (pfil_bridge && dir == PFIL_OUT && bifp != NULL)
			error = pfil_run_hooks(&inet_pfil_hook, mp, bifp,
					dir, NULL);

		if (*mp == NULL || error != 0) /* filter may consume */
			break;

		if (pfil_member && ifp != NULL)
			error = pfil_run_hooks(&inet_pfil_hook, mp, ifp,
					dir, NULL);

		if (*mp == NULL || error != 0) /* filter may consume */
			break;

		if (pfil_bridge && dir == PFIL_IN && bifp != NULL)
			error = pfil_run_hooks(&inet_pfil_hook, mp, bifp,
					dir, NULL);

		if (*mp == NULL || error != 0) /* filter may consume */
			break;

		/* check if we need to fragment the packet */
		if (pfil_member && ifp != NULL && dir == PFIL_OUT) {
			i = (*mp)->m_pkthdr.len;
			if (i > ifp->if_mtu) {
				error = bridge_fragment(ifp, *mp, &eh2, snap,
					    &llc1);
				return (error);
			}
		}

		/* Recalculate the ip checksum and restore byte ordering */
		ip = mtod(*mp, struct ip *);
		hlen = ip->ip_hl << 2;
		if (hlen < sizeof(struct ip))
			goto bad;
		if (hlen > (*mp)->m_len) {
			if ((*mp = m_pullup(*mp, hlen)) == 0)
				goto bad;
			ip = mtod(*mp, struct ip *);
			if (ip == NULL)
				goto bad;
		}
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
		ip->ip_sum = 0;
		if (hlen == sizeof(struct ip))
			ip->ip_sum = in_cksum_hdr(ip);
		else
			ip->ip_sum = in_cksum(*mp, hlen);

		break;
#ifdef INET6
	case ETHERTYPE_IPV6:
		if (pfil_bridge && dir == PFIL_OUT && bifp != NULL)
			error = pfil_run_hooks(&inet6_pfil_hook, mp, bifp,
					dir, NULL);

		if (*mp == NULL || error != 0) /* filter may consume */
			break;

		if (pfil_member && ifp != NULL)
			error = pfil_run_hooks(&inet6_pfil_hook, mp, ifp,
					dir, NULL);

		if (*mp == NULL || error != 0) /* filter may consume */
			break;

		if (pfil_bridge && dir == PFIL_IN && bifp != NULL)
			error = pfil_run_hooks(&inet6_pfil_hook, mp, bifp,
					dir, NULL);
		break;
#endif
	default:
		error = 0;
		break;
	}

	if (*mp == NULL)
		return (error);
	if (error != 0)
		goto bad;

	error = -1;

	/*
	 * Finally, put everything back the way it was and return
	 */
	if (snap) {
		M_PREPEND(*mp, sizeof(struct llc), M_DONTWAIT);
		if (*mp == NULL)
			return (error);
		bcopy(&llc1, mtod(*mp, caddr_t), sizeof(struct llc));
	}

	M_PREPEND(*mp, ETHER_HDR_LEN, M_DONTWAIT);
	if (*mp == NULL)
		return (error);
	bcopy(&eh2, mtod(*mp, caddr_t), ETHER_HDR_LEN);

	return (0);

bad:
	m_freem(*mp);
	*mp = NULL;
	return (error);
}

/*
 * Perform basic checks on header size since
 * pfil assumes ip_input has already processed
 * it for it.  Cut-and-pasted from ip_input.c.
 * Given how simple the IPv6 version is,
 * does the IPv4 version really need to be
 * this complicated?
 *
 * XXX Should we update ipstat here, or not?
 * XXX Right now we update ipstat but not
 * XXX csum_counter.
 */
static int
bridge_ip_checkbasic(struct mbuf **mp)
{
	struct mbuf *m = *mp;
	struct ip *ip;
	int len, hlen;
	u_short sum;

	if (*mp == NULL)
		return (-1);

	if (IP_HDR_ALIGNED_P(mtod(m, caddr_t)) == 0) {
		if ((m = m_copyup(m, sizeof(struct ip),
			(max_linkhdr + 3) & ~3)) == NULL) {
			/* XXXJRT new stat, please */
			ipstat.ips_toosmall++;
			goto bad;
		}
	} else if (__predict_false(m->m_len < sizeof (struct ip))) {
		if ((m = m_pullup(m, sizeof (struct ip))) == NULL) {
			ipstat.ips_toosmall++;
			goto bad;
		}
	}
	ip = mtod(m, struct ip *);
	if (ip == NULL) goto bad;

	if (ip->ip_v != IPVERSION) {
		ipstat.ips_badvers++;
		goto bad;
	}
	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip)) { /* minimum header length */
		ipstat.ips_badhlen++;
		goto bad;
	}
	if (hlen > m->m_len) {
		if ((m = m_pullup(m, hlen)) == 0) {
			ipstat.ips_badhlen++;
			goto bad;
		}
		ip = mtod(m, struct ip *);
		if (ip == NULL) goto bad;
	}

	if (m->m_pkthdr.csum_flags & CSUM_IP_CHECKED) {
		sum = !(m->m_pkthdr.csum_flags & CSUM_IP_VALID);
	} else {
		if (hlen == sizeof(struct ip)) {
			sum = in_cksum_hdr(ip);
		} else {
			sum = in_cksum(m, hlen);
		}
	}
	if (sum) {
		ipstat.ips_badsum++;
		goto bad;
	}

	/* Retrieve the packet length. */
	len = ntohs(ip->ip_len);

	/*
	 * Check for additional length bogosity
	 */
	if (len < hlen) {
		ipstat.ips_badlen++;
		goto bad;
	}

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IP header would have us expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len < len) {
		ipstat.ips_tooshort++;
		goto bad;
	}

	/* Checks out, proceed */
	*mp = m;
	return (0);

bad:
	*mp = m;
	return (-1);
}

#ifdef INET6
/*
 * Same as above, but for IPv6.
 * Cut-and-pasted from ip6_input.c.
 * XXX Should we update ip6stat, or not?
 */
static int
bridge_ip6_checkbasic(struct mbuf **mp)
{
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6;

	/*
	 * If the IPv6 header is not aligned, slurp it up into a new
	 * mbuf with space for link headers, in the event we forward
	 * it.  Otherwise, if it is aligned, make sure the entire base
	 * IPv6 header is in the first mbuf of the chain.
	 */
	if (IP6_HDR_ALIGNED_P(mtod(m, caddr_t)) == 0) {
		struct ifnet *inifp = m->m_pkthdr.rcvif;
		if ((m = m_copyup(m, sizeof(struct ip6_hdr),
			    (max_linkhdr + 3) & ~3)) == NULL) {
			/* XXXJRT new stat, please */
			ip6stat.ip6s_toosmall++;
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			goto bad;
		}
	} else if (__predict_false(m->m_len < sizeof(struct ip6_hdr))) {
		struct ifnet *inifp = m->m_pkthdr.rcvif;
		if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
			ip6stat.ip6s_toosmall++;
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			goto bad;
		}
	}

	ip6 = mtod(m, struct ip6_hdr *);

	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		ip6stat.ip6s_badvers++;
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_hdrerr);
		goto bad;
	}

	/* Checks out, proceed */
	*mp = m;
	return (0);

bad:
	*mp = m;
	return (-1);
}
#endif /* INET6 */

/*
 * bridge_fragment:
 *
 *	Return a fragmented mbuf chain.
 */
static int
bridge_fragment(struct ifnet *ifp, struct mbuf *m, struct ether_header *eh,
    int snap, struct llc *llc)
{
	struct mbuf *m0;
	struct ip *ip;
	int error = -1;

	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == NULL)
		goto out;
	ip = mtod(m, struct ip *);

	error = ip_fragment(ip, &m, ifp->if_mtu, ifp->if_hwassist,
		    CSUM_DELAY_IP);
	if (error)
		goto out;

	/* walk the chain and re-add the Ethernet header */
	for (m0 = m; m0; m0 = m0->m_nextpkt) {
		if (error == 0) {
			if (snap) {
				M_PREPEND(m0, sizeof(struct llc), M_DONTWAIT);
				if (m0 == NULL) {
					error = ENOBUFS;
					continue;
				}
				bcopy(llc, mtod(m0, caddr_t),
				    sizeof(struct llc));
			}
			M_PREPEND(m0, ETHER_HDR_LEN, M_DONTWAIT);
			if (m0 == NULL) {
				error = ENOBUFS;
				continue;
			}
			bcopy(eh, mtod(m0, caddr_t), ETHER_HDR_LEN);
		} else 
			m_freem(m);
	}

	if (error == 0)
		ipstat.ips_fragmented++;

	return (error);

out:
	if (m != NULL)
		m_freem(m);
	return (error);
}
