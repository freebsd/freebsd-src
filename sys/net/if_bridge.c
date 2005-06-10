/*	$NetBSD: if_bridge.c,v 1.24 2004/04/21 19:10:31 itojun Exp $	*/

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
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

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
#include <sys/sysctl.h>
#include <vm/uma.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>

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
#include <machine/in_cksum.h>
#include <netinet/if_ether.h> /* for struct arpcom */
#include <net/if_bridgevar.h>
#include <net/if_llc.h>

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
 * Spanning tree defaults.
 */
#define	BSTP_DEFAULT_MAX_AGE		(20 * 256)
#define	BSTP_DEFAULT_HELLO_TIME		(2 * 256)
#define	BSTP_DEFAULT_FORWARD_DELAY	(15 * 256)
#define	BSTP_DEFAULT_HOLD_TIME		(1 * 256)
#define	BSTP_DEFAULT_BRIDGE_PRIORITY	0x8000
#define	BSTP_DEFAULT_PORT_PRIORITY	0x80
#define	BSTP_DEFAULT_PATH_COST		55

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

static struct mtx bridge_list_mtx;

extern	struct mbuf *(*bridge_input_p)(struct ifnet *, struct mbuf *);
extern	int (*bridge_output_p)(struct ifnet *, struct mbuf *,
		struct sockaddr *, struct rtentry *);
extern	void (*bridge_dn_p)(struct mbuf *, struct ifnet *);

int	bridge_rtable_prune_period = BRIDGE_RTABLE_PRUNE_PERIOD;

uma_zone_t bridge_rtnode_zone;

int	bridge_clone_create(struct if_clone *, int);
void	bridge_clone_destroy(struct ifnet *);

int	bridge_ioctl(struct ifnet *, u_long, caddr_t);
static void	bridge_init(void *);
void	bridge_stop(struct ifnet *, int);
void	bridge_start(struct ifnet *);

void	bridge_forward(struct bridge_softc *, struct mbuf *m);

void	bridge_timer(void *);

void	bridge_broadcast(struct bridge_softc *, struct ifnet *, struct mbuf *);

int	bridge_rtupdate(struct bridge_softc *, const uint8_t *,
	    struct ifnet *, int, uint8_t);
struct ifnet *bridge_rtlookup(struct bridge_softc *, const uint8_t *);
void	bridge_rttrim(struct bridge_softc *);
void	bridge_rtage(struct bridge_softc *);
void	bridge_rtflush(struct bridge_softc *, int);
int	bridge_rtdaddr(struct bridge_softc *, const uint8_t *);

int	bridge_rtable_init(struct bridge_softc *);
void	bridge_rtable_fini(struct bridge_softc *);

struct bridge_rtnode *bridge_rtnode_lookup(struct bridge_softc *,
	    const uint8_t *);
int	bridge_rtnode_insert(struct bridge_softc *, struct bridge_rtnode *);
void	bridge_rtnode_destroy(struct bridge_softc *, struct bridge_rtnode *);

struct bridge_iflist *bridge_lookup_member(struct bridge_softc *,
	    const char *name);
struct bridge_iflist *bridge_lookup_member_if(struct bridge_softc *,
	    struct ifnet *ifp);
void	bridge_delete_member(struct bridge_softc *, struct bridge_iflist *);

int	bridge_ioctl_add(struct bridge_softc *, void *);
int	bridge_ioctl_del(struct bridge_softc *, void *);
int	bridge_ioctl_gifflags(struct bridge_softc *, void *);
int	bridge_ioctl_sifflags(struct bridge_softc *, void *);
int	bridge_ioctl_scache(struct bridge_softc *, void *);
int	bridge_ioctl_gcache(struct bridge_softc *, void *);
int	bridge_ioctl_gifs(struct bridge_softc *, void *);
int	bridge_ioctl_rts(struct bridge_softc *, void *);
int	bridge_ioctl_saddr(struct bridge_softc *, void *);
int	bridge_ioctl_sto(struct bridge_softc *, void *);
int	bridge_ioctl_gto(struct bridge_softc *, void *);
int	bridge_ioctl_daddr(struct bridge_softc *, void *);
int	bridge_ioctl_flush(struct bridge_softc *, void *);
int	bridge_ioctl_gpri(struct bridge_softc *, void *);
int	bridge_ioctl_spri(struct bridge_softc *, void *);
int	bridge_ioctl_ght(struct bridge_softc *, void *);
int	bridge_ioctl_sht(struct bridge_softc *, void *);
int	bridge_ioctl_gfd(struct bridge_softc *, void *);
int	bridge_ioctl_sfd(struct bridge_softc *, void *);
int	bridge_ioctl_gma(struct bridge_softc *, void *);
int	bridge_ioctl_sma(struct bridge_softc *, void *);
int	bridge_ioctl_sifprio(struct bridge_softc *, void *);
int	bridge_ioctl_sifcost(struct bridge_softc *, void *);
static int bridge_pfil(struct mbuf **, struct ifnet *, struct ifnet *, int);
static int bridge_ip_checkbasic(struct mbuf **mp);
# ifdef INET6
static int bridge_ip6_checkbasic(struct mbuf **mp);
# endif /* INET6 */

SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, IFT_BRIDGE, bridge, CTLFLAG_RW, 0, "Bridge");

static int pfil_bridge = 1; /* run pfil hooks on the bridge interface */
static int pfil_member = 1; /* run pfil hooks on the member interface */
static int pfil_ipfw = 0;   /* layer2 filter with ipfw */
SYSCTL_INT(_net_link_bridge, OID_AUTO, pfil_bridge, CTLFLAG_RW,
    &pfil_bridge, 0, "Packet filter on the bridge interface");
SYSCTL_INT(_net_link_bridge, OID_AUTO, pfil_member, CTLFLAG_RW,
    &pfil_member, 0, "Packet filter on the member interface");

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
		bstp_linkstate_p = bstp_linkstate;
		break;
	case MOD_UNLOAD:
		if_clone_detach(&bridge_cloner);
		while (!LIST_EMPTY(&bridge_list))
			bridge_clone_destroy(LIST_FIRST(&bridge_list)->sc_ifp);
		uma_zdestroy(bridge_rtnode_zone);
		bridge_input_p = NULL;
		bridge_output_p = NULL;
		bridge_dn_p = NULL;
		bstp_linkstate_p = NULL;
		mtx_destroy(&bridge_list_mtx);
		break;
	default:
		return EOPNOTSUPP;
	}
	return 0;
}

static moduledata_t bridge_mod = {
	"if_bridge", 
	bridge_modevent, 
	0
};

DECLARE_MODULE(if_bridge, bridge_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

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
	 * Disable pfil so that ipfw doesnt run twice, if the user really wants
	 * both then they can re-enable pfil_bridge and/or pfil_member.
	 */
	if (pfil_ipfw) {
		pfil_bridge = 0;
		pfil_member = 0;
	}
    }

    return error;
}
SYSCTL_PROC(_net_link_bridge, OID_AUTO, ipfw, CTLTYPE_INT|CTLFLAG_RW,
	    &pfil_ipfw, 0, &sysctl_pfil_ipfw, "I", "Layer2 filter with IPFW");

/*
 * bridge_clone_create:
 *
 *	Create a new bridge instance.
 */
int
bridge_clone_create(struct if_clone *ifc, int unit)
{
	struct bridge_softc *sc;
	struct ifnet *ifp;
	u_char eaddr[6];

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	BRIDGE_LOCK_INIT(sc);
	ifp = sc->sc_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		free(sc, M_DEVBUF);
		return (ENOSPC);
	}

	sc->sc_brtmax = BRIDGE_RTABLE_MAX;
	sc->sc_brttimeout = BRIDGE_RTABLE_TIMEOUT;
	sc->sc_bridge_max_age = BSTP_DEFAULT_MAX_AGE;
	sc->sc_bridge_hello_time = BSTP_DEFAULT_HELLO_TIME;
	sc->sc_bridge_forward_delay = BSTP_DEFAULT_FORWARD_DELAY;
	sc->sc_bridge_priority = BSTP_DEFAULT_BRIDGE_PRIORITY;
	sc->sc_hold_time = BSTP_DEFAULT_HOLD_TIME;

	/* Initialize our routing table. */
	bridge_rtable_init(sc);

	callout_init(&sc->sc_brcallout, 0);
	callout_init(&sc->sc_bstpcallout, 0);

	LIST_INIT(&sc->sc_iflist);

	ifp->if_softc = sc;
	if_initname(ifp, ifc->ifc_name, unit);
	ifp->if_mtu = ETHERMTU;
	ifp->if_ioctl = bridge_ioctl;
	ifp->if_output = bridge_output;
	ifp->if_start = bridge_start;
	ifp->if_init = bridge_init;
	ifp->if_type = IFT_BRIDGE;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_hdrlen = ETHER_HDR_LEN;

	/*
	 * Generate a random ethernet address and use the private AC:DE:48
	 * OUI code.
	 */
	arc4rand(eaddr, ETHER_ADDR_LEN, 1);
	eaddr[0] = 0xAC;
	eaddr[1] = 0xDE;
	eaddr[2] = 0x48;

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
void
bridge_clone_destroy(struct ifnet *ifp)
{
	struct bridge_softc *sc = ifp->if_softc;
	struct bridge_iflist *bif;

	BRIDGE_LOCK(sc);

	bridge_stop(ifp, 1);

	while ((bif = LIST_FIRST(&sc->sc_iflist)) != NULL)
		bridge_delete_member(sc, bif);

	BRIDGE_UNLOCK(sc);

	mtx_lock(&bridge_list_mtx);
	LIST_REMOVE(sc, sc_list);
	mtx_unlock(&bridge_list_mtx);

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
int
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
	} args;
	struct ifdrv *ifd = (struct ifdrv *) data;
	const struct bridge_control *bc;
	int error = 0;

	BRIDGE_LOCK(sc);

	switch (cmd) {

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
			error = suser(td);
			if (error)
				break;
		}

		if (ifd->ifd_len != bc->bc_argsize ||
		    ifd->ifd_len > sizeof(args)) {
			error = EINVAL;
			break;
		}

		if (bc->bc_flags & BC_F_COPYIN) {
			error = copyin(ifd->ifd_data, &args, ifd->ifd_len);
			if (error)
				break;
		}

		error = (*bc->bc_func)(sc, &args);
		if (error)
			break;

		if (bc->bc_flags & BC_F_COPYOUT)
			error = copyout(&args, ifd->ifd_data, ifd->ifd_len);

		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) == IFF_RUNNING) {
			/*
			 * If interface is marked down and it is running,
			 * then stop and disable it.
			 */
			bridge_stop(ifp, 1);
		} else if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) == IFF_UP) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			(*ifp->if_init)(ifp);
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
		BRIDGE_UNLOCK(sc);
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	if (BRIDGE_LOCKED(sc))
		BRIDGE_UNLOCK(sc);

	return (error);
}

/*
 * bridge_lookup_member:
 *
 *	Lookup a bridge member interface.
 */
struct bridge_iflist *
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
struct bridge_iflist *
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
void
bridge_delete_member(struct bridge_softc *sc, struct bridge_iflist *bif)
{
	struct ifnet *ifs = bif->bif_ifp;

	BRIDGE_LOCK_ASSERT(sc);

	switch (ifs->if_type) {
	case IFT_ETHER:
		/*
		 * Take the interface out of promiscuous mode.
		 */
		(void) ifpromisc(ifs, 0);
		break;

	default:
#ifdef DIAGNOSTIC
		panic("bridge_delete_member: impossible");
#endif
		break;
	}

	ifs->if_bridge = NULL;
	BRIDGE_XLOCK(sc);
	LIST_REMOVE(bif, bif_next);
	BRIDGE_XDROP(sc);

	bridge_rtdelete(sc, ifs, IFBF_FLUSHALL);

	free(bif, M_DEVBUF);

	if (sc->sc_ifp->if_flags & IFF_RUNNING)
		bstp_initialization(sc);
}

int
bridge_ioctl_add(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif = NULL;
	struct ifnet *ifs;
	int error = 0;

	BRIDGE_LOCK_ASSERT(sc);

	ifs = ifunit(req->ifbr_ifsname);
	if (ifs == NULL)
		return (ENOENT);

	if (sc->sc_ifp->if_mtu != ifs->if_mtu)
		return (EINVAL);

	if (ifs->if_bridge == sc)
		return (EEXIST);

	if (ifs->if_bridge != NULL)
		return (EBUSY);

	bif = malloc(sizeof(*bif), M_DEVBUF, M_NOWAIT);
	if (bif == NULL)
		return (ENOMEM);

	switch (ifs->if_type) {
	case IFT_ETHER:
	case IFT_L2VLAN:
		/*
		 * Place the interface into promiscuous mode.
		 */
		error = ifpromisc(ifs, 1);
		if (error)
			goto out;
		break;

	default:
		error = EINVAL;
		goto out;
	}

	bif->bif_ifp = ifs;
	bif->bif_flags = IFBIF_LEARNING | IFBIF_DISCOVER;
	bif->bif_priority = BSTP_DEFAULT_PORT_PRIORITY;
	bif->bif_path_cost = BSTP_DEFAULT_PATH_COST;

	ifs->if_bridge = sc;
	/*
	 * XXX: XLOCK HERE!?!
	 *
	 * NOTE: insert_***HEAD*** should be safe for the traversals.
	 */
	LIST_INSERT_HEAD(&sc->sc_iflist, bif, bif_next);

	if (sc->sc_ifp->if_flags & IFF_RUNNING)
		bstp_initialization(sc);
	else
		bstp_stop(sc);

 out:
	if (error) {
		if (bif != NULL)
			free(bif, M_DEVBUF);
	}
	return (error);
}

int
bridge_ioctl_del(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	BRIDGE_LOCK_ASSERT(sc);

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	bridge_delete_member(sc, bif);

	return (0);
}

int
bridge_ioctl_gifflags(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	BRIDGE_LOCK_ASSERT(sc);

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	req->ifbr_ifsflags = bif->bif_flags;
	req->ifbr_state = bif->bif_state;
	req->ifbr_priority = bif->bif_priority;
	req->ifbr_path_cost = bif->bif_path_cost;
	req->ifbr_portno = bif->bif_ifp->if_index & 0xff;

	return (0);
}

int
bridge_ioctl_sifflags(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	BRIDGE_LOCK_ASSERT(sc);

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	if (req->ifbr_ifsflags & IFBIF_STP) {
		switch (bif->bif_ifp->if_type) {
		case IFT_ETHER:
			/* These can do spanning tree. */
			break;

		default:
			/* Nothing else can. */
			return (EINVAL);
		}
	}

	bif->bif_flags = req->ifbr_ifsflags;

	if (sc->sc_ifp->if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return (0);
}

int
bridge_ioctl_scache(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	BRIDGE_LOCK_ASSERT(sc);

	sc->sc_brtmax = param->ifbrp_csize;
	bridge_rttrim(sc);

	return (0);
}

int
bridge_ioctl_gcache(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	BRIDGE_LOCK_ASSERT(sc);

	param->ifbrp_csize = sc->sc_brtmax;

	return (0);
}

int
bridge_ioctl_gifs(struct bridge_softc *sc, void *arg)
{
	struct ifbifconf *bifc = arg;
	struct bridge_iflist *bif;
	struct ifbreq breq;
	int count, len, error = 0;

	BRIDGE_LOCK_ASSERT(sc);

	count = 0;
	LIST_FOREACH(bif, &sc->sc_iflist, bif_next)
		count++;

	if (bifc->ifbic_len == 0) {
		bifc->ifbic_len = sizeof(breq) * count;
		return (0);
	}

	count = 0;
	len = bifc->ifbic_len;
	LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		if (len < sizeof(breq))
			break;

		strlcpy(breq.ifbr_ifsname, bif->bif_ifp->if_xname,
		    sizeof(breq.ifbr_ifsname));
		breq.ifbr_ifsflags = bif->bif_flags;
		breq.ifbr_state = bif->bif_state;
		breq.ifbr_priority = bif->bif_priority;
		breq.ifbr_path_cost = bif->bif_path_cost;
		breq.ifbr_portno = bif->bif_ifp->if_index & 0xff;
		error = copyout(&breq, bifc->ifbic_req + count, sizeof(breq));
		if (error)
			break;
		count++;
		len -= sizeof(breq);
	}

	bifc->ifbic_len = sizeof(breq) * count;
	return (error);
}

int
bridge_ioctl_rts(struct bridge_softc *sc, void *arg)
{
	struct ifbaconf *bac = arg;
	struct bridge_rtnode *brt;
	struct ifbareq bareq;
	struct timeval tv;
	int count = 0, error = 0, len;

	BRIDGE_LOCK_ASSERT(sc);

	if (bac->ifbac_len == 0)
		return (0);

	getmicrotime(&tv);

	len = bac->ifbac_len;
	LIST_FOREACH(brt, &sc->sc_rtlist, brt_list) {
		if (len < sizeof(bareq))
			goto out;
		strlcpy(bareq.ifba_ifsname, brt->brt_ifp->if_xname,
		    sizeof(bareq.ifba_ifsname));
		memcpy(bareq.ifba_dst, brt->brt_addr, sizeof(brt->brt_addr));
		if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC &&
				tv.tv_sec < brt->brt_expire)
			bareq.ifba_expire = brt->brt_expire - tv.tv_sec;
		else
			bareq.ifba_expire = 0;
		bareq.ifba_flags = brt->brt_flags;

		error = copyout(&bareq, bac->ifbac_req + count, sizeof(bareq));
		if (error)
			goto out;
		count++;
		len -= sizeof(bareq);
	}
 out:
	bac->ifbac_len = sizeof(bareq) * count;
	return (error);
}

int
bridge_ioctl_saddr(struct bridge_softc *sc, void *arg)
{
	struct ifbareq *req = arg;
	struct bridge_iflist *bif;
	int error;

	BRIDGE_LOCK_ASSERT(sc);

	bif = bridge_lookup_member(sc, req->ifba_ifsname);
	if (bif == NULL)
		return (ENOENT);

	error = bridge_rtupdate(sc, req->ifba_dst, bif->bif_ifp, 1,
	    req->ifba_flags);

	return (error);
}

int
bridge_ioctl_sto(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	BRIDGE_LOCK_ASSERT(sc);

	sc->sc_brttimeout = param->ifbrp_ctime;

	return (0);
}

int
bridge_ioctl_gto(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	BRIDGE_LOCK_ASSERT(sc);

	param->ifbrp_ctime = sc->sc_brttimeout;

	return (0);
}

int
bridge_ioctl_daddr(struct bridge_softc *sc, void *arg)
{
	struct ifbareq *req = arg;

	BRIDGE_LOCK_ASSERT(sc);

	return (bridge_rtdaddr(sc, req->ifba_dst));
}

int
bridge_ioctl_flush(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;

	BRIDGE_LOCK_ASSERT(sc);

	bridge_rtflush(sc, req->ifbr_ifsflags);

	return (0);
}

int
bridge_ioctl_gpri(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	BRIDGE_LOCK_ASSERT(sc);

	param->ifbrp_prio = sc->sc_bridge_priority;

	return (0);
}

int
bridge_ioctl_spri(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	BRIDGE_LOCK_ASSERT(sc);

	sc->sc_bridge_priority = param->ifbrp_prio;

	if (sc->sc_ifp->if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return (0);
}

int
bridge_ioctl_ght(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	BRIDGE_LOCK_ASSERT(sc);

	param->ifbrp_hellotime = sc->sc_bridge_hello_time >> 8;

	return (0);
}

int
bridge_ioctl_sht(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	BRIDGE_LOCK_ASSERT(sc);

	if (param->ifbrp_hellotime == 0)
		return (EINVAL);
	sc->sc_bridge_hello_time = param->ifbrp_hellotime << 8;

	if (sc->sc_ifp->if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return (0);
}

int
bridge_ioctl_gfd(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	BRIDGE_LOCK_ASSERT(sc);

	param->ifbrp_fwddelay = sc->sc_bridge_forward_delay >> 8;

	return (0);
}

int
bridge_ioctl_sfd(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	BRIDGE_LOCK_ASSERT(sc);

	if (param->ifbrp_fwddelay == 0)
		return (EINVAL);
	sc->sc_bridge_forward_delay = param->ifbrp_fwddelay << 8;

	if (sc->sc_ifp->if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return (0);
}

int
bridge_ioctl_gma(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	BRIDGE_LOCK_ASSERT(sc);

	param->ifbrp_maxage = sc->sc_bridge_max_age >> 8;

	return (0);
}

int
bridge_ioctl_sma(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	BRIDGE_LOCK_ASSERT(sc);

	if (param->ifbrp_maxage == 0)
		return (EINVAL);
	sc->sc_bridge_max_age = param->ifbrp_maxage << 8;

	if (sc->sc_ifp->if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return (0);
}

int
bridge_ioctl_sifprio(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	BRIDGE_LOCK_ASSERT(sc);

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	bif->bif_priority = req->ifbr_priority;

	if (sc->sc_ifp->if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return (0);
}

int
bridge_ioctl_sifcost(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	BRIDGE_LOCK_ASSERT(sc);

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	bif->bif_path_cost = req->ifbr_path_cost;

	if (sc->sc_ifp->if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return (0);
}

/*
 * bridge_ifdetach:
 *
 *	Detach an interface from a bridge.  Called when a member
 *	interface is detaching.
 */
void
bridge_ifdetach(struct ifnet *ifp)
{
	struct bridge_softc *sc = ifp->if_bridge;
	struct ifbreq breq;

	BRIDGE_LOCK_ASSERT(sc);

	memset(&breq, 0, sizeof(breq));
	snprintf(breq.ifbr_ifsname, sizeof(breq.ifbr_ifsname), ifp->if_xname);

	(void) bridge_ioctl_del(sc, &breq);
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

	if (ifp->if_flags & IFF_RUNNING)
		return;

	callout_reset(&sc->sc_brcallout, bridge_rtable_prune_period * hz,
	    bridge_timer, sc);

	ifp->if_flags |= IFF_RUNNING;
	bstp_initialization(sc);
	return;
}

/*
 * bridge_stop:
 *
 *	Stop the bridge interface.
 */
void
bridge_stop(struct ifnet *ifp, int disable)
{
	struct bridge_softc *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	callout_stop(&sc->sc_brcallout);
	bstp_stop(sc);

	bridge_rtflush(sc, IFBF_FLUSHDYN);

	ifp->if_flags &= ~IFF_RUNNING;
}

/*
 * bridge_enqueue:
 *
 *	Enqueue a packet on a bridge member interface.
 *
 */
__inline void
bridge_enqueue(struct bridge_softc *sc, struct ifnet *dst_ifp, struct mbuf *m,
    int runfilt)
{
	int len, err;
	short mflags;

	/*
	 * Clear any in-bound checksum flags for this packet.
	 */
	m->m_pkthdr.csum_flags = 0;

	if (runfilt && inet_pfil_hook.ph_busy_count >= 0) {
		if (bridge_pfil(&m, sc->sc_ifp, dst_ifp, PFIL_OUT) != 0)
			return;
	}
	if (m == NULL)
		return;

	len = m->m_pkthdr.len;
	mflags = m->m_flags;
	IFQ_ENQUEUE(&dst_ifp->if_snd, m, err);
	if (err == 0) {

		sc->sc_ifp->if_opackets++;
		sc->sc_ifp->if_obytes += len;

		dst_ifp->if_obytes += len;

		if (mflags & M_MCAST) {
			sc->sc_ifp->if_omcasts++;
			dst_ifp->if_omcasts++;
		}
	}

	if ((dst_ifp->if_flags & IFF_OACTIVE) == 0)
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
void
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

	bridge_enqueue(sc, ifp, m, 1);
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
int
bridge_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *sa,
    struct rtentry *rt)
{
	struct ether_header *eh;
	struct ifnet *dst_if;
	struct bridge_softc *sc;

	if (m->m_len < ETHER_HDR_LEN) {
		m = m_pullup(m, ETHER_HDR_LEN);
		if (m == NULL)
			return (0);
	}

	eh = mtod(m, struct ether_header *);
	sc = ifp->if_bridge;

	BRIDGE_LOCK(sc);

	/*
	 * If bridge is down, but the original output interface is up,
	 * go ahead and send out that interface.  Otherwise, the packet
	 * is dropped below.
	 */
	if ((sc->sc_ifp->if_flags & IFF_RUNNING) == 0) {
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
		dst_if = bridge_rtlookup(sc, eh->ether_dhost);
	if (dst_if == NULL) {
		struct bridge_iflist *bif;
		struct mbuf *mc;
		int error = 0, used = 0;

		BRIDGE_LOCK2REF(sc, error);
		if (error) {
			m_freem(m);
			return (0);
		}
		LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
			dst_if = bif->bif_ifp;
			if ((dst_if->if_flags & IFF_RUNNING) == 0)
				continue;

			/*
			 * If this is not the original output interface,
			 * and the interface is participating in spanning
			 * tree, make sure the port is in a state that
			 * allows forwarding.
			 */
			if (dst_if != ifp &&
			    (bif->bif_flags & IFBIF_STP) != 0) {
				switch (bif->bif_state) {
				case BSTP_IFSTATE_BLOCKING:
				case BSTP_IFSTATE_LISTENING:
				case BSTP_IFSTATE_DISABLED:
					continue;
				}
			}

			if (LIST_NEXT(bif, bif_next) == NULL) {
				used = 1;
				mc = m;
			} else {
				mc = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
				if (mc == NULL) {
					sc->sc_ifp->if_oerrors++;
					continue;
				}
			}

			bridge_enqueue(sc, dst_if, mc, 0);
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

	if ((dst_if->if_flags & IFF_RUNNING) == 0) {
		m_freem(m);
		BRIDGE_UNLOCK(sc);
		return (0);
	}

	BRIDGE_UNLOCK(sc);
	bridge_enqueue(sc, dst_if, m, 0);
	return (0);
}

/*
 * bridge_start:
 *
 *	Start output on a bridge.
 *
 */
void
bridge_start(struct ifnet *ifp)
{
	struct bridge_softc *sc;
	struct mbuf *m;
	struct ether_header *eh;
	struct ifnet *dst_if;

	sc = ifp->if_softc;

	ifp->if_flags |= IFF_OACTIVE;
	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;
		BPF_MTAP(ifp, m);

		eh = mtod(m, struct ether_header *);
		dst_if = NULL;

		BRIDGE_LOCK(sc);
		if ((m->m_flags & (M_BCAST|M_MCAST)) == 0) {
			dst_if = bridge_rtlookup(sc, eh->ether_dhost);
		}

		if (dst_if == NULL)
			bridge_broadcast(sc, ifp, m);
		else {
			BRIDGE_UNLOCK(sc);
			bridge_enqueue(sc, dst_if, m, 1);
		}
	}
	ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

/*
 * bridge_forward:
 *
 *	The forwarding function of the bridge.
 *
 *	NOTE: Releases the lock on return.
 */
void
bridge_forward(struct bridge_softc *sc, struct mbuf *m)
{
	struct bridge_iflist *bif;
	struct ifnet *src_if, *dst_if, *ifp;
	struct ether_header *eh;

	src_if = m->m_pkthdr.rcvif;
	BRIDGE_LOCK_ASSERT(sc);
	ifp = sc->sc_ifp;

	sc->sc_ifp->if_ipackets++;
	sc->sc_ifp->if_ibytes += m->m_pkthdr.len;

	/*
	 * Look up the bridge_iflist.
	 */
	bif = bridge_lookup_member_if(sc, src_if);
	if (bif == NULL) {
		/* Interface is not a bridge member (anymore?) */
		BRIDGE_UNLOCK(sc);
		m_freem(m);
		return;
	}

	if (bif->bif_flags & IFBIF_STP) {
		switch (bif->bif_state) {
		case BSTP_IFSTATE_BLOCKING:
		case BSTP_IFSTATE_LISTENING:
		case BSTP_IFSTATE_DISABLED:
			BRIDGE_UNLOCK(sc);
			m_freem(m);
			return;
		}
	}

	eh = mtod(m, struct ether_header *);

	/*
	 * If the interface is learning, and the source
	 * address is valid and not multicast, record
	 * the address.
	 */
	if ((bif->bif_flags & IFBIF_LEARNING) != 0 &&
	    ETHER_IS_MULTICAST(eh->ether_shost) == 0 &&
	    (eh->ether_shost[0] == 0 &&
	     eh->ether_shost[1] == 0 &&
	     eh->ether_shost[2] == 0 &&
	     eh->ether_shost[3] == 0 &&
	     eh->ether_shost[4] == 0 &&
	     eh->ether_shost[5] == 0) == 0) {
		(void) bridge_rtupdate(sc, eh->ether_shost,
		    src_if, 0, IFBAF_DYNAMIC);
	}

	if ((bif->bif_flags & IFBIF_STP) != 0 &&
	    bif->bif_state == BSTP_IFSTATE_LEARNING) {
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
		dst_if = bridge_rtlookup(sc, eh->ether_dhost);
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

	/* run the packet filter */
	if (inet_pfil_hook.ph_busy_count >= 0) {
		BRIDGE_UNLOCK(sc);
		if (bridge_pfil(&m, ifp, src_if, PFIL_IN) != 0)
			return;
		BRIDGE_LOCK(sc);
	}
	if (m == NULL) {
		BRIDGE_UNLOCK(sc);
		return;
	}

	if (dst_if == NULL) {
		/* tap off packets passing the bridge */
		BPF_MTAP(ifp, m);

		bridge_broadcast(sc, src_if, m);
		return;
	}

	/*
	 * At this point, we're dealing with a unicast frame
	 * going to a different interface.
	 */
	if ((dst_if->if_flags & IFF_RUNNING) == 0) {
		BRIDGE_UNLOCK(sc);
		m_freem(m);
		return;
	}
	bif = bridge_lookup_member_if(sc, dst_if);
	if (bif == NULL) {
		/* Not a member of the bridge (anymore?) */
		BRIDGE_UNLOCK(sc);
		m_freem(m);
		return;
	}

	if (bif->bif_flags & IFBIF_STP) {
		switch (bif->bif_state) {
		case BSTP_IFSTATE_DISABLED:
		case BSTP_IFSTATE_BLOCKING:
			BRIDGE_UNLOCK(sc);
			m_freem(m);
			return;
		}
	}

	/* tap off packets passing the bridge */
	BPF_MTAP(ifp, m);

	BRIDGE_UNLOCK(sc);
	bridge_enqueue(sc, dst_if, m, 1);
}

/*
 * bridge_input:
 *
 *	Receive input from a member interface.  Queue the packet for
 *	bridging if it is not for us.
 */
struct mbuf *
bridge_input(struct ifnet *ifp, struct mbuf *m)
{
	struct bridge_softc *sc = ifp->if_bridge;
	struct bridge_iflist *bif;
	struct ether_header *eh;
	struct mbuf *mc;

	if ((sc->sc_ifp->if_flags & IFF_RUNNING) == 0)
		return (m);

	BRIDGE_LOCK(sc);
	bif = bridge_lookup_member_if(sc, ifp);
	if (bif == NULL) {
		BRIDGE_UNLOCK(sc);
		return (m);
	}

	eh = mtod(m, struct ether_header *);

	if (memcmp(eh->ether_dhost, IFP2ENADDR(sc->sc_ifp),
	    ETHER_ADDR_LEN) == 0) {
		/*
		 * If the packet is for us, set the packets source as the
		 * bridge, and return the packet back to ether_input for
		 * local processing.
		 */

		/* XXX Do we tap the packet for the member interface too?
		 * BPF_MTAP(&m->m_pkthdr.rcvif, m);
		 */

		/* Mark the packet as arriving on the bridge interface */
		m->m_pkthdr.rcvif = sc->sc_ifp;
		BPF_MTAP(sc->sc_ifp, m);
		sc->sc_ifp->if_ipackets++;

		BRIDGE_UNLOCK(sc);
		return (m);
	}

	if (m->m_flags & (M_BCAST|M_MCAST)) {
		/* Tap off 802.1D packets; they do not get forwarded. */
		if (memcmp(eh->ether_dhost, bstp_etheraddr,
		    ETHER_ADDR_LEN) == 0) {
			m = bstp_input(ifp, m);
			if (m == NULL) {
				BRIDGE_UNLOCK(sc);
				return (NULL);
			}
		}

		if (bif->bif_flags & IFBIF_STP) {
			switch (bif->bif_state) {
			case BSTP_IFSTATE_BLOCKING:
			case BSTP_IFSTATE_LISTENING:
			case BSTP_IFSTATE_DISABLED:
				BRIDGE_UNLOCK(sc);
				return (m);
			}
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
		bridge_forward(sc, mc);

		/* Return the original packet for local processing. */
		return (m);
	}

	if (bif->bif_flags & IFBIF_STP) {
		switch (bif->bif_state) {
		case BSTP_IFSTATE_BLOCKING:
		case BSTP_IFSTATE_LISTENING:
		case BSTP_IFSTATE_DISABLED:
			BRIDGE_UNLOCK(sc);
			return (m);
		}
	}

	/*
	 * Unicast.  Make sure it's not for us.
	 */
	LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		/* It is destined for us. */
		if (memcmp(IF_LLADDR(bif->bif_ifp), eh->ether_dhost,
		    ETHER_ADDR_LEN) == 0) {
			if (bif->bif_flags & IFBIF_LEARNING)
				(void) bridge_rtupdate(sc,
				    eh->ether_shost, ifp, 0, IFBAF_DYNAMIC);
			m->m_pkthdr.rcvif = bif->bif_ifp;
			BRIDGE_UNLOCK(sc);
			return (m);
		}

		/* We just received a packet that we sent out. */
		if (memcmp(IF_LLADDR(bif->bif_ifp), eh->ether_shost,
		    ETHER_ADDR_LEN) == 0) {
			BRIDGE_UNLOCK(sc);
			m_freem(m);
			return (NULL);
		}
	}

	/* Perform the bridge forwarding function. */
	bridge_forward(sc, m);

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
void
bridge_broadcast(struct bridge_softc *sc, struct ifnet *src_if,
    struct mbuf *m)
{
	struct bridge_iflist *bif;
	struct mbuf *mc;
	struct ifnet *dst_if;
	int error = 0, used = 0;

	BRIDGE_LOCK_ASSERT(sc);
	BRIDGE_LOCK2REF(sc, error);
	if (error) {
		m_freem(m);
		return;
	}

	LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		dst_if = bif->bif_ifp;
		if (dst_if == src_if)
			continue;

		if (bif->bif_flags & IFBIF_STP) {
			switch (bif->bif_state) {
			case BSTP_IFSTATE_BLOCKING:
			case BSTP_IFSTATE_DISABLED:
				continue;
			}
		}

		if ((bif->bif_flags & IFBIF_DISCOVER) == 0 &&
		    (m->m_flags & (M_BCAST|M_MCAST)) == 0)
			continue;

		if ((dst_if->if_flags & IFF_RUNNING) == 0)
			continue;

		if (LIST_NEXT(bif, bif_next) == NULL) {
			mc = m;
			used = 1;
		} else {
			mc = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
			if (mc == NULL) {
				sc->sc_ifp->if_oerrors++;
				continue;
			}
		}

		bridge_enqueue(sc, dst_if, mc, 1);
	}
	if (used == 0)
		m_freem(m);

	BRIDGE_UNREF(sc);
}

/*
 * bridge_rtupdate:
 *
 *	Add a bridge routing entry.
 */
int
bridge_rtupdate(struct bridge_softc *sc, const uint8_t *dst,
    struct ifnet *dst_if, int setflags, uint8_t flags)
{
	struct bridge_rtnode *brt;
	struct timeval tv;
	int error;

	BRIDGE_LOCK_ASSERT(sc);

	/*
	 * A route for this destination might already exist.  If so,
	 * update it, otherwise create a new one.
	 */
	getmicrotime(&tv);
	if ((brt = bridge_rtnode_lookup(sc, dst)) == NULL) {
		if (sc->sc_brtcnt >= sc->sc_brtmax)
			return (ENOSPC);

		/*
		 * Allocate a new bridge forwarding node, and
		 * initialize the expiration time and Ethernet
		 * address.
		 */
		brt = uma_zalloc(bridge_rtnode_zone, M_NOWAIT | M_ZERO);
		if (brt == NULL)
			return (ENOMEM);

		brt->brt_expire = tv.tv_sec + sc->sc_brttimeout;
		brt->brt_flags = IFBAF_DYNAMIC;
		memcpy(brt->brt_addr, dst, ETHER_ADDR_LEN);

		if ((error = bridge_rtnode_insert(sc, brt)) != 0) {
			uma_zfree(bridge_rtnode_zone, brt);
			return (error);
		}
	}

	brt->brt_ifp = dst_if;
	if (setflags) {
		brt->brt_flags = flags;
		brt->brt_expire = (flags & IFBAF_STATIC) ? 0 :
		    tv.tv_sec + sc->sc_brttimeout;
	}

	return (0);
}

/*
 * bridge_rtlookup:
 *
 *	Lookup the destination interface for an address.
 */
struct ifnet *
bridge_rtlookup(struct bridge_softc *sc, const uint8_t *addr)
{
	struct bridge_rtnode *brt;

	BRIDGE_LOCK_ASSERT(sc);

	if ((brt = bridge_rtnode_lookup(sc, addr)) == NULL)
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
void
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

	for (brt = LIST_FIRST(&sc->sc_rtlist); brt != NULL; brt = nbrt) {
		nbrt = LIST_NEXT(brt, brt_list);
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
void
bridge_timer(void *arg)
{
	struct bridge_softc *sc = arg;

	BRIDGE_LOCK(sc);
	bridge_rtage(sc);
	BRIDGE_UNLOCK(sc);

	if (sc->sc_ifp->if_flags & IFF_RUNNING)
		callout_reset(&sc->sc_brcallout,
		    bridge_rtable_prune_period * hz, bridge_timer, sc);
}

/*
 * bridge_rtage:
 *
 *	Perform an aging cycle.
 */
void
bridge_rtage(struct bridge_softc *sc)
{
	struct bridge_rtnode *brt, *nbrt;
	struct timeval tv;

	BRIDGE_LOCK_ASSERT(sc);

	getmicrotime(&tv);

	for (brt = LIST_FIRST(&sc->sc_rtlist); brt != NULL; brt = nbrt) {
		nbrt = LIST_NEXT(brt, brt_list);
		if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
			if (tv.tv_sec >= brt->brt_expire)
				bridge_rtnode_destroy(sc, brt);
		}
	}
}

/*
 * bridge_rtflush:
 *
 *	Remove all dynamic addresses from the bridge.
 */
void
bridge_rtflush(struct bridge_softc *sc, int full)
{
	struct bridge_rtnode *brt, *nbrt;

	BRIDGE_LOCK_ASSERT(sc);

	for (brt = LIST_FIRST(&sc->sc_rtlist); brt != NULL; brt = nbrt) {
		nbrt = LIST_NEXT(brt, brt_list);
		if (full || (brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC)
			bridge_rtnode_destroy(sc, brt);
	}
}

/*
 * bridge_rtdaddr:
 *
 *	Remove an address from the table.
 */
int
bridge_rtdaddr(struct bridge_softc *sc, const uint8_t *addr)
{
	struct bridge_rtnode *brt;

	BRIDGE_LOCK_ASSERT(sc);

	if ((brt = bridge_rtnode_lookup(sc, addr)) == NULL)
		return (ENOENT);

	bridge_rtnode_destroy(sc, brt);
	return (0);
}

/*
 * bridge_rtdelete:
 *
 *	Delete routes to a speicifc member interface.
 */
void
bridge_rtdelete(struct bridge_softc *sc, struct ifnet *ifp, int full)
{
	struct bridge_rtnode *brt, *nbrt;

	BRIDGE_LOCK_ASSERT(sc);

	for (brt = LIST_FIRST(&sc->sc_rtlist); brt != NULL; brt = nbrt) {
		nbrt = LIST_NEXT(brt, brt_list);
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
int
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
void
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

/*
 * bridge_rtnode_lookup:
 *
 *	Look up a bridge route node for the specified destination.
 */
struct bridge_rtnode *
bridge_rtnode_lookup(struct bridge_softc *sc, const uint8_t *addr)
{
	struct bridge_rtnode *brt;
	uint32_t hash;
	int dir;

	BRIDGE_LOCK_ASSERT(sc);

	hash = bridge_rthash(sc, addr);
	LIST_FOREACH(brt, &sc->sc_rthash[hash], brt_hash) {
		dir = memcmp(addr, brt->brt_addr, ETHER_ADDR_LEN);
		if (dir == 0)
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
int
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
		dir = memcmp(brt->brt_addr, lbrt->brt_addr, ETHER_ADDR_LEN);
		if (dir == 0)
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
void
bridge_rtnode_destroy(struct bridge_softc *sc, struct bridge_rtnode *brt)
{
	BRIDGE_LOCK_ASSERT(sc);

	LIST_REMOVE(brt, brt_hash);

	LIST_REMOVE(brt, brt_list);
	sc->sc_brtcnt--;
	uma_zfree(bridge_rtnode_zone, brt);
}

/*
 * Send bridge packets through pfil if they are one of the types pfil can deal
 * with, or if they are ARP or REVARP.  (pfil will pass ARP and REVARP without
 * question.)
 */
static int bridge_pfil(struct mbuf **mp, struct ifnet *bifp,
		struct ifnet *ifp, int dir)
{
	int snap, error, i;
	struct ether_header *eh1, eh2;
	struct ip_fw_args args;
	struct ip *ip;
	struct llc llc;
	u_int16_t ether_type;

	snap = 0;
	error = -1;	/* Default error if not error == 0 */

	i = min((*mp)->m_pkthdr.len, max_protohdr);
	if ((*mp)->m_len < i) {
	    *mp = m_pullup(*mp, i);
	    if (*mp == NULL) {
		printf("%s: m_pullup failed\n", __func__);
		return -1;
	    }
	}

	eh1 = mtod(*mp, struct ether_header *);
	ether_type = ntohs(eh1->ether_type);

	/*
	 * Check for SNAP/LLC.
	 */
	if (ether_type < ETHERMTU) {
		struct llc *llc = (struct llc *)(eh1 + 1);

		if ((*mp)->m_len >= ETHER_HDR_LEN + 8 &&
		    llc->llc_dsap == LLC_SNAP_LSAP &&
		    llc->llc_ssap == LLC_SNAP_LSAP &&
		    llc->llc_control == LLC_UI) {
			ether_type = htons(llc->llc_un.type_snap.ether_type);
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
			return 0; /* Automatically pass */
		case ETHERTYPE_IP:
# ifdef INET6
		case ETHERTYPE_IPV6:
# endif /* INET6 */
			break;
		default:
			/*
			 * ipfw allows layer2 protocol filtering using
			 * 'mac-type' so we will let the packet past, if
			 * ipfw is disabled then drop it.
			 */
			if (!IPFW_LOADED || pfil_ipfw == 0)
				goto bad;
	}

	/* Strip off the Ethernet header and keep a copy. */
	m_copydata(*mp, 0, ETHER_HDR_LEN, (caddr_t) &eh2);
	m_adj(*mp, ETHER_HDR_LEN);

	/* Strip off snap header, if present */
	if (snap) {
		m_copydata(*mp, 0, sizeof(struct llc), (caddr_t) &llc);
		m_adj(*mp, sizeof(struct llc));
	}

	if (IPFW_LOADED && pfil_ipfw != 0 && dir == PFIL_OUT) {
		args.rule = ip_dn_claim_rule(*mp);
		if (args.rule != NULL && fw_one_pass)
			goto ipfwpass; /* packet already partially processed */

		args.m = *mp;
		args.oif = ifp;
		args.next_hop = NULL;
		args.eh = &eh2;
		i = ip_fw_chk_ptr(&args);
		*mp = args.m;

		if (*mp == NULL)
			return error;

		if (DUMMYNET_LOADED && (i == IP_FW_DUMMYNET)) {

			/* put the Ethernet header back on */
			M_PREPEND(*mp, ETHER_HDR_LEN, M_DONTWAIT);
			if (*mp == NULL)
				return error;
			bcopy(&eh2, mtod(*mp, caddr_t), ETHER_HDR_LEN);

			/*
			 * Pass the pkt to dummynet, which consumes it. The
			 * packet will return to us via bridge_dummynet().
			 */
			args.oif = ifp;
			ip_dn_io_ptr(*mp, DN_TO_IFB_FWD, &args);
			return error;
		}

		if (i != IP_FW_PASS) /* drop */
			goto bad;
	}

ipfwpass:
	/*
	 * Check basic packet sanity and run pfil through pfil.
	 */
	switch (ether_type)
	{
	case ETHERTYPE_IP :
		error = (dir == PFIL_IN) ? bridge_ip_checkbasic(mp) : 0;
		/*
		 * before calling the firewall, swap fields the same as
		 * IP does. here we assume the header is contiguous
		 */
		if (error == 0) {
			ip = mtod(*mp, struct ip *);

			ip->ip_len = ntohs(ip->ip_len);
			ip->ip_off = ntohs(ip->ip_off);
		} else {
			error = -1;
			break;
		}

		/*
		 * Run pfil on the member interface and the bridge, both can
		 * be skipped by clearing pfil_member or pfil_bridge.
		 *
		 * Keep the order:
		 *   in_if -> bridge_if -> out_if
		 */
		if (error == 0 && pfil_bridge && dir == PFIL_OUT)
			error = pfil_run_hooks(&inet_pfil_hook, mp, bifp,
					dir, NULL);

		if (*mp == NULL) /* filter may consume */
			break;

		if (error == 0 && pfil_member)
			error = pfil_run_hooks(&inet_pfil_hook, mp, ifp,
					dir, NULL);

		if (*mp == NULL) /* filter may consume */
			break;

		if (error == 0 && pfil_bridge && dir == PFIL_IN)
			error = pfil_run_hooks(&inet_pfil_hook, mp, bifp,
					dir, NULL);

		/* Restore ip and the fields ntohs()'d. */
		if (*mp != NULL && error == 0) {
			ip = mtod(*mp, struct ip *);
			ip->ip_len = htons(ip->ip_len);
			ip->ip_off = htons(ip->ip_off);
		}

		break;
# ifdef INET6
	case ETHERTYPE_IPV6 :
		error = (dir == PFIL_IN) ? bridge_ip6_checkbasic(mp) : 0;

		if (error == 0 && pfil_bridge && dir == PFIL_OUT)
			error = pfil_run_hooks(&inet6_pfil_hook, mp, bifp,
					dir, NULL);

		if (*mp == NULL) /* filter may consume */
			break;

		if (error == 0 && pfil_member)
			error = pfil_run_hooks(&inet6_pfil_hook, mp, ifp,
					dir, NULL);

		if (*mp == NULL) /* filter may consume */
			break;

		if (error == 0 && pfil_bridge && dir == PFIL_IN)
			error = pfil_run_hooks(&inet6_pfil_hook, mp, bifp,
					dir, NULL);
		break;
# endif
	default :
		error = 0;
		break;
	}

	if (*mp == NULL)
		return error;
	if (error != 0)
		goto bad;

	error = -1;

	/*
	 * Finally, put everything back the way it was and return
	 */
	if (snap) {
		M_PREPEND(*mp, sizeof(struct llc), M_DONTWAIT);
		if (*mp == NULL)
			return error;
		bcopy(&llc, mtod(*mp, caddr_t), sizeof(struct llc));
	}

	M_PREPEND(*mp, ETHER_HDR_LEN, M_DONTWAIT);
	if (*mp == NULL)
		return error;
	bcopy(&eh2, mtod(*mp, caddr_t), ETHER_HDR_LEN);

	return 0;

    bad:
	m_freem(*mp);
	*mp = NULL;
	return error;
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
		return -1;

	if (__predict_false(m->m_len < sizeof (struct ip))) {
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
	return 0;

    bad:
	*mp = m;
	return -1;
}

# ifdef INET6
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

	if (IP6_HDR_ALIGNED_P(mtod(m, caddr_t)) == 0) {
		struct ifnet *inifp = m->m_pkthdr.rcvif;
		if ((m = m_copyup(m, sizeof(struct ip6_hdr),
			    (max_linkhdr + 3) & ~3)) == NULL) {
			* XXXJRT new stat, please *
			ip6stat.ip6s_toosmall++;
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			goto bad;
		}
	} else */
	if (__predict_false(m->m_len < sizeof(struct ip6_hdr))) {
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
	return 0;

    bad:
	*mp = m;
	return -1;
}
# endif /* INET6 */
