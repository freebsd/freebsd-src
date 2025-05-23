/*	$NetBSD: if_bridge.c,v 1.31 2005/06/01 19:45:34 jdc Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
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
 *	  to bridge other types of interfaces (maybe consider
 *	  heterogeneous bridges).
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/protosw.h>
#include <sys/systm.h>
#include <sys/jail.h>
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
#include <net/if_private.h>
#include <net/pfil.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_ifattach.h>
#endif
#if defined(INET) || defined(INET6)
#include <netinet/ip_carp.h>
#endif
#include <machine/in_cksum.h>
#include <netinet/if_ether.h>
#include <net/bridgestp.h>
#include <net/if_bridgevar.h>
#include <net/if_llc.h>
#include <net/if_vlan_var.h>

#include <net/route.h>

/*
 * At various points in the code we need to know if we're hooked into the INET
 * and/or INET6 pfil.  Define some macros to do that based on which IP versions
 * are enabled in the kernel.  This avoids littering the rest of the code with
 * #ifnet INET6 to avoid referencing V_inet6_pfil_head.
 */
#ifdef INET6
#define		PFIL_HOOKED_IN_INET6	PFIL_HOOKED_IN(V_inet6_pfil_head)
#define		PFIL_HOOKED_OUT_INET6	PFIL_HOOKED_OUT(V_inet6_pfil_head)
#else
#define		PFIL_HOOKED_IN_INET6	false
#define		PFIL_HOOKED_OUT_INET6	false
#endif

#ifdef INET
#define		PFIL_HOOKED_IN_INET	PFIL_HOOKED_IN(V_inet_pfil_head)
#define		PFIL_HOOKED_OUT_INET	PFIL_HOOKED_OUT(V_inet_pfil_head)
#else
#define		PFIL_HOOKED_IN_INET	false
#define		PFIL_HOOKED_OUT_INET	false
#endif

#define		PFIL_HOOKED_IN_46	(PFIL_HOOKED_IN_INET6 || PFIL_HOOKED_IN_INET)
#define		PFIL_HOOKED_OUT_46	(PFIL_HOOKED_OUT_INET6 || PFIL_HOOKED_OUT_INET)

/*
 * Size of the route hash table.  Must be a power of two.
 */
#ifndef BRIDGE_RTHASH_SIZE
#define	BRIDGE_RTHASH_SIZE		1024
#endif

#define	BRIDGE_RTHASH_MASK		(BRIDGE_RTHASH_SIZE - 1)

/*
 * Default maximum number of addresses to cache.
 */
#ifndef BRIDGE_RTABLE_MAX
#define	BRIDGE_RTABLE_MAX		2000
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
 * List of capabilities to possibly mask on the member interface.
 */
#define	BRIDGE_IFCAPS_MASK		(IFCAP_TOE|IFCAP_TSO|IFCAP_TXCSUM|\
					 IFCAP_TXCSUM_IPV6|IFCAP_MEXTPG)

/*
 * List of capabilities to strip
 */
#define	BRIDGE_IFCAPS_STRIP		IFCAP_LRO

/*
 * Bridge locking
 *
 * The bridge relies heavily on the epoch(9) system to protect its data
 * structures. This means we can safely use CK_LISTs while in NET_EPOCH, but we
 * must ensure there is only one writer at a time.
 *
 * That is: for read accesses we only need to be in NET_EPOCH, but for write
 * accesses we must hold:
 *
 *  - BRIDGE_RT_LOCK, for any change to bridge_rtnodes
 *  - BRIDGE_LOCK, for any other change
 *
 * The BRIDGE_LOCK is a sleepable lock, because it is held across ioctl()
 * calls to bridge member interfaces and these ioctl()s can sleep.
 * The BRIDGE_RT_LOCK is a non-sleepable mutex, because it is sometimes
 * required while we're in NET_EPOCH and then we're not allowed to sleep.
 */
#define BRIDGE_LOCK_INIT(_sc)		do {			\
	sx_init(&(_sc)->sc_sx, "if_bridge");			\
	mtx_init(&(_sc)->sc_rt_mtx, "if_bridge rt", NULL, MTX_DEF);	\
} while (0)
#define BRIDGE_LOCK_DESTROY(_sc)	do {	\
	sx_destroy(&(_sc)->sc_sx);		\
	mtx_destroy(&(_sc)->sc_rt_mtx);		\
} while (0)
#define BRIDGE_LOCK(_sc)		sx_xlock(&(_sc)->sc_sx)
#define BRIDGE_UNLOCK(_sc)		sx_xunlock(&(_sc)->sc_sx)
#define BRIDGE_LOCK_ASSERT(_sc)		sx_assert(&(_sc)->sc_sx, SX_XLOCKED)
#define BRIDGE_LOCK_OR_NET_EPOCH_ASSERT(_sc)	\
	    MPASS(in_epoch(net_epoch_preempt) || sx_xlocked(&(_sc)->sc_sx))
#define BRIDGE_UNLOCK_ASSERT(_sc)	sx_assert(&(_sc)->sc_sx, SX_UNLOCKED)
#define BRIDGE_RT_LOCK(_sc)		mtx_lock(&(_sc)->sc_rt_mtx)
#define BRIDGE_RT_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_rt_mtx)
#define BRIDGE_RT_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->sc_rt_mtx, MA_OWNED)
#define BRIDGE_RT_LOCK_OR_NET_EPOCH_ASSERT(_sc)	\
	    MPASS(in_epoch(net_epoch_preempt) || mtx_owned(&(_sc)->sc_rt_mtx))

struct bridge_softc;

/*
 * Bridge interface list entry.
 */
struct bridge_iflist {
	CK_LIST_ENTRY(bridge_iflist) bif_next;
	struct ifnet		*bif_ifp;	/* member if */
	struct bridge_softc	*bif_sc;	/* parent bridge */
	struct bstp_port	bif_stp;	/* STP state */
	uint32_t		bif_flags;	/* member if flags */
	int			bif_savedcaps;	/* saved capabilities */
	uint32_t		bif_addrmax;	/* max # of addresses */
	uint32_t		bif_addrcnt;	/* cur. # of addresses */
	uint32_t		bif_addrexceeded;/* # of address violations */
	struct epoch_context	bif_epoch_ctx;
};

/*
 * Bridge route node.
 */
struct bridge_rtnode {
	CK_LIST_ENTRY(bridge_rtnode) brt_hash;	/* hash table linkage */
	CK_LIST_ENTRY(bridge_rtnode) brt_list;	/* list linkage */
	struct bridge_iflist	*brt_dst;	/* destination if */
	unsigned long		brt_expire;	/* expiration time */
	uint8_t			brt_flags;	/* address flags */
	uint8_t			brt_addr[ETHER_ADDR_LEN];
	ether_vlanid_t		brt_vlan;	/* vlan id */
	struct	vnet		*brt_vnet;
	struct	epoch_context	brt_epoch_ctx;
};
#define	brt_ifp			brt_dst->bif_ifp

/*
 * Software state for each bridge.
 */
struct bridge_softc {
	struct ifnet		*sc_ifp;	/* make this an interface */
	LIST_ENTRY(bridge_softc) sc_list;
	struct sx		sc_sx;
	struct mtx		sc_rt_mtx;
	uint32_t		sc_brtmax;	/* max # of addresses */
	uint32_t		sc_brtcnt;	/* cur. # of addresses */
	uint32_t		sc_brttimeout;	/* rt timeout in seconds */
	struct callout		sc_brcallout;	/* bridge callout */
	CK_LIST_HEAD(, bridge_iflist) sc_iflist;	/* member interface list */
	CK_LIST_HEAD(, bridge_rtnode) *sc_rthash;	/* our forwarding table */
	CK_LIST_HEAD(, bridge_rtnode) sc_rtlist;	/* list version of above */
	uint32_t		sc_rthash_key;	/* key for hash */
	CK_LIST_HEAD(, bridge_iflist) sc_spanlist;	/* span ports list */
	struct bstp_state	sc_stp;		/* STP state */
	uint32_t		sc_brtexceeded;	/* # of cache drops */
	struct ifnet		*sc_ifaddr;	/* member mac copied from */
	struct ether_addr	sc_defaddr;	/* Default MAC address */
	if_input_fn_t		sc_if_input;	/* Saved copy of if_input */
	struct epoch_context	sc_epoch_ctx;
};

VNET_DEFINE_STATIC(struct sx, bridge_list_sx);
#define	V_bridge_list_sx	VNET(bridge_list_sx)
static eventhandler_tag bridge_detach_cookie;

int	bridge_rtable_prune_period = BRIDGE_RTABLE_PRUNE_PERIOD;

VNET_DEFINE_STATIC(uma_zone_t, bridge_rtnode_zone);
#define	V_bridge_rtnode_zone	VNET(bridge_rtnode_zone)

static int	bridge_clone_create(struct if_clone *, char *, size_t,
		    struct ifc_data *, struct ifnet **);
static int	bridge_clone_destroy(struct if_clone *, struct ifnet *, uint32_t);

static int	bridge_ioctl(struct ifnet *, u_long, caddr_t);
static void	bridge_mutecaps(struct bridge_softc *);
static void	bridge_set_ifcap(struct bridge_softc *, struct bridge_iflist *,
		    int);
static void	bridge_ifdetach(void *arg __unused, struct ifnet *);
static void	bridge_init(void *);
static void	bridge_dummynet(struct mbuf *, struct ifnet *);
static bool	bridge_same(const void *, const void *);
static void	*bridge_get_softc(struct ifnet *);
static void	bridge_stop(struct ifnet *, int);
static int	bridge_transmit(struct ifnet *, struct mbuf *);
#ifdef ALTQ
static void	bridge_altq_start(if_t);
static int	bridge_altq_transmit(if_t, struct mbuf *);
#endif
static void	bridge_qflush(struct ifnet *);
static struct mbuf *bridge_input(struct ifnet *, struct mbuf *);
static void	bridge_inject(struct ifnet *, struct mbuf *);
static int	bridge_output(struct ifnet *, struct mbuf *, struct sockaddr *,
		    struct rtentry *);
static int	bridge_enqueue(struct bridge_softc *, struct ifnet *,
		    struct mbuf *);
static void	bridge_rtdelete(struct bridge_softc *, struct ifnet *ifp, int);

static void	bridge_forward(struct bridge_softc *, struct bridge_iflist *,
		    struct mbuf *m);
static bool	bridge_member_ifaddrs(void);

static void	bridge_timer(void *);

static void	bridge_broadcast(struct bridge_softc *, struct ifnet *,
		    struct mbuf *, int);
static void	bridge_span(struct bridge_softc *, struct mbuf *);

static int	bridge_rtupdate(struct bridge_softc *, const uint8_t *,
		    ether_vlanid_t, struct bridge_iflist *, int, uint8_t);
static struct ifnet *bridge_rtlookup(struct bridge_softc *, const uint8_t *,
		    ether_vlanid_t);
static void	bridge_rttrim(struct bridge_softc *);
static void	bridge_rtage(struct bridge_softc *);
static void	bridge_rtflush(struct bridge_softc *, int);
static int	bridge_rtdaddr(struct bridge_softc *, const uint8_t *,
		    ether_vlanid_t);

static void	bridge_rtable_init(struct bridge_softc *);
static void	bridge_rtable_fini(struct bridge_softc *);

static int	bridge_rtnode_addr_cmp(const uint8_t *, const uint8_t *);
static struct bridge_rtnode *bridge_rtnode_lookup(struct bridge_softc *,
		    const uint8_t *, ether_vlanid_t);
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
static int	bridge_ioctl_sifmaxaddr(struct bridge_softc *, void *);
static int	bridge_ioctl_addspan(struct bridge_softc *, void *);
static int	bridge_ioctl_delspan(struct bridge_softc *, void *);
static int	bridge_ioctl_gbparam(struct bridge_softc *, void *);
static int	bridge_ioctl_grte(struct bridge_softc *, void *);
static int	bridge_ioctl_gifsstp(struct bridge_softc *, void *);
static int	bridge_ioctl_sproto(struct bridge_softc *, void *);
static int	bridge_ioctl_stxhc(struct bridge_softc *, void *);
static int	bridge_pfil(struct mbuf **, struct ifnet *, struct ifnet *,
		    int);
#ifdef INET
static int	bridge_ip_checkbasic(struct mbuf **mp);
static int	bridge_fragment(struct ifnet *, struct mbuf **mp,
		    struct ether_header *, int, struct llc *);
#endif /* INET */
#ifdef INET6
static int	bridge_ip6_checkbasic(struct mbuf **mp);
#endif /* INET6 */
static void	bridge_linkstate(struct ifnet *ifp);
static void	bridge_linkcheck(struct bridge_softc *sc);

/*
 * Use the "null" value from IEEE 802.1Q-2014 Table 9-2
 * to indicate untagged frames.
 */
#define	VLANTAGOF(_m)	\
    ((_m->m_flags & M_VLANTAG) ? EVL_VLANOFTAG(_m->m_pkthdr.ether_vtag) : DOT1Q_VID_NULL)

static struct bstp_cb_ops bridge_ops = {
	.bcb_state = bridge_state_change,
	.bcb_rtage = bridge_rtable_expire
};

SYSCTL_DECL(_net_link);
static SYSCTL_NODE(_net_link, IFT_BRIDGE, bridge, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Bridge");

/* only pass IP[46] packets when pfil is enabled */
VNET_DEFINE_STATIC(int, pfil_onlyip) = 1;
#define	V_pfil_onlyip	VNET(pfil_onlyip)
SYSCTL_INT(_net_link_bridge, OID_AUTO, pfil_onlyip,
    CTLFLAG_RWTUN | CTLFLAG_VNET, &VNET_NAME(pfil_onlyip), 0,
    "Only pass IP packets when pfil is enabled");

/* run pfil hooks on the bridge interface */
VNET_DEFINE_STATIC(int, pfil_bridge) = 0;
#define	V_pfil_bridge	VNET(pfil_bridge)
SYSCTL_INT(_net_link_bridge, OID_AUTO, pfil_bridge,
    CTLFLAG_RWTUN | CTLFLAG_VNET, &VNET_NAME(pfil_bridge), 0,
    "Packet filter on the bridge interface");

/* layer2 filter with ipfw */
VNET_DEFINE_STATIC(int, pfil_ipfw);
#define	V_pfil_ipfw	VNET(pfil_ipfw)

/* layer2 ARP filter with ipfw */
VNET_DEFINE_STATIC(int, pfil_ipfw_arp);
#define	V_pfil_ipfw_arp	VNET(pfil_ipfw_arp)
SYSCTL_INT(_net_link_bridge, OID_AUTO, ipfw_arp,
    CTLFLAG_RWTUN | CTLFLAG_VNET, &VNET_NAME(pfil_ipfw_arp), 0,
    "Filter ARP packets through IPFW layer2");

/* run pfil hooks on the member interface */
VNET_DEFINE_STATIC(int, pfil_member) = 0;
#define	V_pfil_member	VNET(pfil_member)
SYSCTL_INT(_net_link_bridge, OID_AUTO, pfil_member,
    CTLFLAG_RWTUN | CTLFLAG_VNET, &VNET_NAME(pfil_member), 0,
    "Packet filter on the member interface");

/* run pfil hooks on the physical interface for locally destined packets */
VNET_DEFINE_STATIC(int, pfil_local_phys);
#define	V_pfil_local_phys	VNET(pfil_local_phys)
SYSCTL_INT(_net_link_bridge, OID_AUTO, pfil_local_phys,
    CTLFLAG_RWTUN | CTLFLAG_VNET, &VNET_NAME(pfil_local_phys), 0,
    "Packet filter on the physical interface for locally destined packets");

/* log STP state changes */
VNET_DEFINE_STATIC(int, log_stp);
#define	V_log_stp	VNET(log_stp)
SYSCTL_INT(_net_link_bridge, OID_AUTO, log_stp,
    CTLFLAG_RWTUN | CTLFLAG_VNET, &VNET_NAME(log_stp), 0,
    "Log STP state changes");

/* share MAC with first bridge member */
VNET_DEFINE_STATIC(int, bridge_inherit_mac);
#define	V_bridge_inherit_mac	VNET(bridge_inherit_mac)
SYSCTL_INT(_net_link_bridge, OID_AUTO, inherit_mac,
    CTLFLAG_RWTUN | CTLFLAG_VNET, &VNET_NAME(bridge_inherit_mac), 0,
    "Inherit MAC address from the first bridge member");

VNET_DEFINE_STATIC(int, allow_llz_overlap) = 0;
#define	V_allow_llz_overlap	VNET(allow_llz_overlap)
SYSCTL_INT(_net_link_bridge, OID_AUTO, allow_llz_overlap,
    CTLFLAG_RW | CTLFLAG_VNET, &VNET_NAME(allow_llz_overlap), 0,
    "Allow overlap of link-local scope "
    "zones of a bridge interface and the member interfaces");

/* log MAC address port flapping */
VNET_DEFINE_STATIC(bool, log_mac_flap) = true;
#define	V_log_mac_flap	VNET(log_mac_flap)
SYSCTL_BOOL(_net_link_bridge, OID_AUTO, log_mac_flap,
    CTLFLAG_RW | CTLFLAG_VNET, &VNET_NAME(log_mac_flap), true,
    "Log MAC address port flapping");

/* allow IP addresses on bridge members */
VNET_DEFINE_STATIC(bool, member_ifaddrs) = false;
#define	V_member_ifaddrs	VNET(member_ifaddrs)
SYSCTL_BOOL(_net_link_bridge, OID_AUTO, member_ifaddrs,
    CTLFLAG_RW | CTLFLAG_VNET, &VNET_NAME(member_ifaddrs), false,
    "Allow layer 3 addresses on bridge members");

static bool
bridge_member_ifaddrs(void)
{
	return (V_member_ifaddrs);
}

VNET_DEFINE_STATIC(int, log_interval) = 5;
VNET_DEFINE_STATIC(int, log_count) = 0;
VNET_DEFINE_STATIC(struct timeval, log_last) = { 0 };

#define	V_log_interval	VNET(log_interval)
#define	V_log_count	VNET(log_count)
#define	V_log_last	VNET(log_last)

struct bridge_control {
	int	(*bc_func)(struct bridge_softc *, void *);
	int	bc_argsize;
	int	bc_flags;
};

#define	BC_F_COPYIN		0x01	/* copy arguments in */
#define	BC_F_COPYOUT		0x02	/* copy arguments out */
#define	BC_F_SUSER		0x04	/* do super-user check */

static const struct bridge_control bridge_control_table[] = {
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

	{ bridge_ioctl_sifmaxaddr,	sizeof(struct ifbreq),
	  BC_F_COPYIN|BC_F_SUSER },

};
static const int bridge_control_table_size = nitems(bridge_control_table);

VNET_DEFINE_STATIC(LIST_HEAD(, bridge_softc), bridge_list) =
    LIST_HEAD_INITIALIZER();
#define	V_bridge_list	VNET(bridge_list)
#define	BRIDGE_LIST_LOCK_INIT(x)	sx_init(&V_bridge_list_sx,	\
					    "if_bridge list")
#define	BRIDGE_LIST_LOCK_DESTROY(x)	sx_destroy(&V_bridge_list_sx)
#define	BRIDGE_LIST_LOCK(x)		sx_xlock(&V_bridge_list_sx)
#define	BRIDGE_LIST_UNLOCK(x)		sx_xunlock(&V_bridge_list_sx)

VNET_DEFINE_STATIC(struct if_clone *, bridge_cloner);
#define	V_bridge_cloner	VNET(bridge_cloner)

static const char bridge_name[] = "bridge";

static void
vnet_bridge_init(const void *unused __unused)
{

	V_bridge_rtnode_zone = uma_zcreate("bridge_rtnode",
	    sizeof(struct bridge_rtnode), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	BRIDGE_LIST_LOCK_INIT();

	struct if_clone_addreq req = {
		.create_f = bridge_clone_create,
		.destroy_f = bridge_clone_destroy,
		.flags = IFC_F_AUTOUNIT,
	};
	V_bridge_cloner = ifc_attach_cloner(bridge_name, &req);
}
VNET_SYSINIT(vnet_bridge_init, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_bridge_init, NULL);

static void
vnet_bridge_uninit(const void *unused __unused)
{

	ifc_detach_cloner(V_bridge_cloner);
	V_bridge_cloner = NULL;
	BRIDGE_LIST_LOCK_DESTROY();

	/* Callbacks may use the UMA zone. */
	NET_EPOCH_DRAIN_CALLBACKS();

	uma_zdestroy(V_bridge_rtnode_zone);
}
VNET_SYSUNINIT(vnet_bridge_uninit, SI_SUB_PSEUDO, SI_ORDER_ANY,
    vnet_bridge_uninit, NULL);

static int
bridge_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		bridge_dn_p = bridge_dummynet;
		bridge_same_p = bridge_same;
		bridge_get_softc_p = bridge_get_softc;
		bridge_member_ifaddrs_p = bridge_member_ifaddrs;
		bridge_detach_cookie = EVENTHANDLER_REGISTER(
		    ifnet_departure_event, bridge_ifdetach, NULL,
		    EVENTHANDLER_PRI_ANY);
		break;
	case MOD_UNLOAD:
		EVENTHANDLER_DEREGISTER(ifnet_departure_event,
		    bridge_detach_cookie);
		bridge_dn_p = NULL;
		bridge_same_p = NULL;
		bridge_get_softc_p = NULL;
		bridge_member_ifaddrs_p = NULL;
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
MODULE_VERSION(if_bridge, 1);
MODULE_DEPEND(if_bridge, bridgestp, 1, 1, 1);

/*
 * handler for net.link.bridge.ipfw
 */
static int
sysctl_pfil_ipfw(SYSCTL_HANDLER_ARGS)
{
	int enable = V_pfil_ipfw;
	int error;

	error = sysctl_handle_int(oidp, &enable, 0, req);
	enable &= 1;

	if (enable != V_pfil_ipfw) {
		V_pfil_ipfw = enable;

		/*
		 * Disable pfil so that ipfw doesnt run twice, if the user
		 * really wants both then they can re-enable pfil_bridge and/or
		 * pfil_member. Also allow non-ip packets as ipfw can filter by
		 * layer2 type.
		 */
		if (V_pfil_ipfw) {
			V_pfil_onlyip = 0;
			V_pfil_bridge = 0;
			V_pfil_member = 0;
		}
	}

	return (error);
}
SYSCTL_PROC(_net_link_bridge, OID_AUTO, ipfw,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_VNET | CTLFLAG_NEEDGIANT,
    &VNET_NAME(pfil_ipfw), 0, &sysctl_pfil_ipfw, "I",
    "Layer2 filter with IPFW");

#ifdef VIMAGE
static void
bridge_reassign(struct ifnet *ifp, struct vnet *newvnet, char *arg)
{
	struct bridge_softc *sc = ifp->if_softc;
	struct bridge_iflist *bif;

	BRIDGE_LOCK(sc);

	while ((bif = CK_LIST_FIRST(&sc->sc_iflist)) != NULL)
		bridge_delete_member(sc, bif, 0);

	while ((bif = CK_LIST_FIRST(&sc->sc_spanlist)) != NULL) {
		bridge_delete_span(sc, bif);
	}

	BRIDGE_UNLOCK(sc);

	ether_reassign(ifp, newvnet, arg);
}
#endif

/*
 * bridge_get_softc:
 *
 * Return the bridge softc for an ifnet.
 */
static void *
bridge_get_softc(struct ifnet *ifp)
{
	struct bridge_iflist *bif;

	NET_EPOCH_ASSERT();

	bif = ifp->if_bridge;
	if (bif == NULL)
		return (NULL);
	return (bif->bif_sc);
}

/*
 * bridge_same:
 *
 * Return true if two interfaces are in the same bridge.  This is only used by
 * bridgestp via bridge_same_p.
 */
static bool
bridge_same(const void *bifap, const void *bifbp)
{
	const struct bridge_iflist *bifa = bifap, *bifb = bifbp;

	NET_EPOCH_ASSERT();

	if (bifa == NULL || bifb == NULL)
		return (false);

	return (bifa->bif_sc == bifb->bif_sc);
}

/*
 * bridge_clone_create:
 *
 *	Create a new bridge instance.
 */
static int
bridge_clone_create(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data *ifd, struct ifnet **ifpp)
{
	struct bridge_softc *sc;
	struct ifnet *ifp;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	ifp = sc->sc_ifp = if_alloc(IFT_ETHER);

	BRIDGE_LOCK_INIT(sc);
	sc->sc_brtmax = BRIDGE_RTABLE_MAX;
	sc->sc_brttimeout = BRIDGE_RTABLE_TIMEOUT;

	/* Initialize our routing table. */
	bridge_rtable_init(sc);

	callout_init_mtx(&sc->sc_brcallout, &sc->sc_rt_mtx, 0);

	CK_LIST_INIT(&sc->sc_iflist);
	CK_LIST_INIT(&sc->sc_spanlist);

	ifp->if_softc = sc;
	if_initname(ifp, bridge_name, ifd->unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = bridge_ioctl;
#ifdef ALTQ
	ifp->if_start = bridge_altq_start;
	ifp->if_transmit = bridge_altq_transmit;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = 0;
	IFQ_SET_READY(&ifp->if_snd);
#else
	ifp->if_transmit = bridge_transmit;
#endif
	ifp->if_qflush = bridge_qflush;
	ifp->if_init = bridge_init;
	ifp->if_type = IFT_BRIDGE;

	ether_gen_addr(ifp, &sc->sc_defaddr);

	bstp_attach(&sc->sc_stp, &bridge_ops);
	ether_ifattach(ifp, sc->sc_defaddr.octet);
	/* Now undo some of the damage... */
	ifp->if_baudrate = 0;
	ifp->if_type = IFT_BRIDGE;
#ifdef VIMAGE
	ifp->if_reassign = bridge_reassign;
#endif
	sc->sc_if_input = ifp->if_input;	/* ether_input */
	ifp->if_input = bridge_inject;

	/*
	 * Allow BRIDGE_INPUT() to pass in packets originating from the bridge
	 * itself via bridge_inject().  This is required for netmap but
	 * otherwise has no effect.
	 */
	ifp->if_bridge_input = bridge_input;

	BRIDGE_LIST_LOCK();
	LIST_INSERT_HEAD(&V_bridge_list, sc, sc_list);
	BRIDGE_LIST_UNLOCK();
	*ifpp = ifp;

	return (0);
}

static void
bridge_clone_destroy_cb(struct epoch_context *ctx)
{
	struct bridge_softc *sc;

	sc = __containerof(ctx, struct bridge_softc, sc_epoch_ctx);

	BRIDGE_LOCK_DESTROY(sc);
	free(sc, M_DEVBUF);
}

/*
 * bridge_clone_destroy:
 *
 *	Destroy a bridge instance.
 */
static int
bridge_clone_destroy(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags)
{
	struct bridge_softc *sc = ifp->if_softc;
	struct bridge_iflist *bif;
	struct epoch_tracker et;

	BRIDGE_LOCK(sc);

	bridge_stop(ifp, 1);
	ifp->if_flags &= ~IFF_UP;

	while ((bif = CK_LIST_FIRST(&sc->sc_iflist)) != NULL)
		bridge_delete_member(sc, bif, 0);

	while ((bif = CK_LIST_FIRST(&sc->sc_spanlist)) != NULL) {
		bridge_delete_span(sc, bif);
	}

	/* Tear down the routing table. */
	bridge_rtable_fini(sc);

	BRIDGE_UNLOCK(sc);

	NET_EPOCH_ENTER(et);

	callout_drain(&sc->sc_brcallout);

	BRIDGE_LIST_LOCK();
	LIST_REMOVE(sc, sc_list);
	BRIDGE_LIST_UNLOCK();

	bstp_detach(&sc->sc_stp);
#ifdef ALTQ
	IFQ_PURGE(&ifp->if_snd);
#endif
	NET_EPOCH_EXIT(et);

	ether_ifdetach(ifp);
	if_free(ifp);

	NET_EPOCH_CALL(bridge_clone_destroy_cb, &sc->sc_epoch_ctx);

	return (0);
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
	struct ifreq *ifr = (struct ifreq *)data;
	struct bridge_iflist *bif;
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
	int error = 0, oldmtu;

	BRIDGE_LOCK(sc);

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

		oldmtu = ifp->if_mtu;
		error = (*bc->bc_func)(sc, &args);
		if (error)
			break;

		/*
		 * Bridge MTU may change during addition of the first port.
		 * If it did, do network layer specific procedure.
		 */
		if (ifp->if_mtu != oldmtu)
			if_notifymtu(ifp);

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
			bridge_stop(ifp, 1);
		} else if ((ifp->if_flags & IFF_UP) &&
		    !(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			BRIDGE_UNLOCK(sc);
			(*ifp->if_init)(sc);
			BRIDGE_LOCK(sc);
		}
		break;

	case SIOCSIFMTU:
		oldmtu = sc->sc_ifp->if_mtu;

		if (ifr->ifr_mtu < IF_MINMTU) {
			error = EINVAL;
			break;
		}
		if (CK_LIST_EMPTY(&sc->sc_iflist)) {
			sc->sc_ifp->if_mtu = ifr->ifr_mtu;
			break;
		}
		CK_LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
			error = (*bif->bif_ifp->if_ioctl)(bif->bif_ifp,
			    SIOCSIFMTU, (caddr_t)ifr);
			if (error != 0) {
				log(LOG_NOTICE, "%s: invalid MTU: %u for"
				    " member %s\n", sc->sc_ifp->if_xname,
				    ifr->ifr_mtu,
				    bif->bif_ifp->if_xname);
				error = EINVAL;
				break;
			}
		}
		if (error) {
			/* Restore the previous MTU on all member interfaces. */
			ifr->ifr_mtu = oldmtu;
			CK_LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
				(*bif->bif_ifp->if_ioctl)(bif->bif_ifp,
				    SIOCSIFMTU, (caddr_t)ifr);
			}
		} else {
			sc->sc_ifp->if_mtu = ifr->ifr_mtu;
		}
		break;
	default:
		/*
		 * drop the lock as ether_ioctl() will call bridge_start() and
		 * cause the lock to be recursed.
		 */
		BRIDGE_UNLOCK(sc);
		error = ether_ioctl(ifp, cmd, data);
		BRIDGE_LOCK(sc);
		break;
	}

	BRIDGE_UNLOCK(sc);

	return (error);
}

/*
 * bridge_mutecaps:
 *
 *	Clear or restore unwanted capabilities on the member interface
 */
static void
bridge_mutecaps(struct bridge_softc *sc)
{
	struct bridge_iflist *bif;
	int enabled, mask;

	BRIDGE_LOCK_ASSERT(sc);

	/* Initial bitmask of capabilities to test */
	mask = BRIDGE_IFCAPS_MASK;

	CK_LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		/* Every member must support it or its disabled */
		mask &= bif->bif_savedcaps;
	}

	CK_LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		enabled = bif->bif_ifp->if_capenable;
		enabled &= ~BRIDGE_IFCAPS_STRIP;
		/* strip off mask bits and enable them again if allowed */
		enabled &= ~BRIDGE_IFCAPS_MASK;
		enabled |= mask;
		bridge_set_ifcap(sc, bif, enabled);
	}
}

static void
bridge_set_ifcap(struct bridge_softc *sc, struct bridge_iflist *bif, int set)
{
	struct ifnet *ifp = bif->bif_ifp;
	struct ifreq ifr;
	int error, mask, stuck;

	bzero(&ifr, sizeof(ifr));
	ifr.ifr_reqcap = set;

	if (ifp->if_capenable != set) {
		error = (*ifp->if_ioctl)(ifp, SIOCSIFCAP, (caddr_t)&ifr);
		if (error)
			if_printf(sc->sc_ifp,
			    "error setting capabilities on %s: %d\n",
			    ifp->if_xname, error);
		mask = BRIDGE_IFCAPS_MASK | BRIDGE_IFCAPS_STRIP;
		stuck = ifp->if_capenable & mask & ~set;
		if (stuck != 0)
			if_printf(sc->sc_ifp,
			    "can't disable some capabilities on %s: 0x%x\n",
			    ifp->if_xname, stuck);
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

	BRIDGE_LOCK_OR_NET_EPOCH_ASSERT(sc);

	CK_LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
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
	BRIDGE_LOCK_OR_NET_EPOCH_ASSERT(sc);
	return (member_ifp->if_bridge);
}

static void
bridge_delete_member_cb(struct epoch_context *ctx)
{
	struct bridge_iflist *bif;

	bif = __containerof(ctx, struct bridge_iflist, bif_epoch_ctx);

	free(bif, M_DEVBUF);
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
	struct ifnet *fif = NULL;
	struct bridge_iflist *bifl;

	BRIDGE_LOCK_ASSERT(sc);

	if (bif->bif_flags & IFBIF_STP)
		bstp_disable(&bif->bif_stp);

	ifs->if_bridge = NULL;
	CK_LIST_REMOVE(bif, bif_next);

	/*
	 * If removing the interface that gave the bridge its mac address, set
	 * the mac address of the bridge to the address of the next member, or
	 * to its default address if no members are left.
	 */
	if (V_bridge_inherit_mac && sc->sc_ifaddr == ifs) {
		if (CK_LIST_EMPTY(&sc->sc_iflist)) {
			bcopy(&sc->sc_defaddr,
			    IF_LLADDR(sc->sc_ifp), ETHER_ADDR_LEN);
			sc->sc_ifaddr = NULL;
		} else {
			bifl = CK_LIST_FIRST(&sc->sc_iflist);
			fif = bifl->bif_ifp;
			bcopy(IF_LLADDR(fif),
			    IF_LLADDR(sc->sc_ifp), ETHER_ADDR_LEN);
			sc->sc_ifaddr = fif;
		}
		EVENTHANDLER_INVOKE(iflladdr_event, sc->sc_ifp);
	}

	bridge_linkcheck(sc);
	bridge_mutecaps(sc);	/* recalcuate now this interface is removed */
	BRIDGE_RT_LOCK(sc);
	bridge_rtdelete(sc, ifs, IFBF_FLUSHALL);
	BRIDGE_RT_UNLOCK(sc);
	KASSERT(bif->bif_addrcnt == 0,
	    ("%s: %d bridge routes referenced", __func__, bif->bif_addrcnt));

	ifs->if_bridge_output = NULL;
	ifs->if_bridge_input = NULL;
	ifs->if_bridge_linkstate = NULL;
	if (!gone) {
		switch (ifs->if_type) {
		case IFT_ETHER:
		case IFT_L2VLAN:
			/*
			 * Take the interface out of promiscuous mode, but only
			 * if it was promiscuous in the first place. It might
			 * not be if we're in the bridge_ioctl_add() error path.
			 */
			if (ifs->if_flags & IFF_PROMISC)
				(void) ifpromisc(ifs, 0);
			break;

		case IFT_GIF:
			break;

		default:
#ifdef DIAGNOSTIC
			panic("bridge_delete_member: impossible");
#endif
			break;
		}
		/* reneable any interface capabilities */
		bridge_set_ifcap(sc, bif, bif->bif_savedcaps);
	}
	bstp_destroy(&bif->bif_stp);	/* prepare to free */

	NET_EPOCH_CALL(bridge_delete_member_cb, &bif->bif_epoch_ctx);
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

	CK_LIST_REMOVE(bif, bif_next);

	NET_EPOCH_CALL(bridge_delete_member_cb, &bif->bif_epoch_ctx);
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
	if (ifs->if_ioctl == NULL)	/* must be supported */
		return (EINVAL);

	/* If it's in the span list, it can't be a member. */
	CK_LIST_FOREACH(bif, &sc->sc_spanlist, bif_next)
		if (ifs == bif->bif_ifp)
			return (EBUSY);

	if (ifs->if_bridge) {
		struct bridge_iflist *sbif = ifs->if_bridge;
		if (sbif->bif_sc == sc)
			return (EEXIST);

		return (EBUSY);
	}

	switch (ifs->if_type) {
	case IFT_ETHER:
	case IFT_L2VLAN:
	case IFT_GIF:
		/* permitted interface types */
		break;
	default:
		return (EINVAL);
	}

	/*
	 * If member_ifaddrs is disabled, do not allow an interface with
	 * assigned IP addresses to be added to a bridge.
	 */
	if (!V_member_ifaddrs) {
		struct ifaddr *ifa;

		CK_STAILQ_FOREACH(ifa, &ifs->if_addrhead, ifa_link) {
#ifdef INET
			if (ifa->ifa_addr->sa_family == AF_INET)
				return (EINVAL);
#endif
#ifdef INET6
			if (ifa->ifa_addr->sa_family == AF_INET6)
				return (EINVAL);
#endif
		}
	}

#ifdef INET6
	/*
	 * Two valid inet6 addresses with link-local scope must not be
	 * on the parent interface and the member interfaces at the
	 * same time.  This restriction is needed to prevent violation
	 * of link-local scope zone.  Attempts to add a member
	 * interface which has inet6 addresses when the parent has
	 * inet6 triggers removal of all inet6 addresses on the member
	 * interface.
	 */

	/* Check if the parent interface has a link-local scope addr. */
	if (V_allow_llz_overlap == 0 &&
	    in6ifa_llaonifp(sc->sc_ifp) != NULL) {
		/*
		 * If any, remove all inet6 addresses from the member
		 * interfaces.
		 */
		CK_LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
 			if (in6ifa_llaonifp(bif->bif_ifp)) {
				in6_ifdetach(bif->bif_ifp);
				if_printf(sc->sc_ifp,
				    "IPv6 addresses on %s have been removed "
				    "before adding it as a member to prevent "
				    "IPv6 address scope violation.\n",
				    bif->bif_ifp->if_xname);
			}
		}
		if (in6ifa_llaonifp(ifs)) {
			in6_ifdetach(ifs);
			if_printf(sc->sc_ifp,
			    "IPv6 addresses on %s have been removed "
			    "before adding it as a member to prevent "
			    "IPv6 address scope violation.\n",
			    ifs->if_xname);
		}
	}
#endif
	/* Allow the first Ethernet member to define the MTU */
	if (CK_LIST_EMPTY(&sc->sc_iflist))
		sc->sc_ifp->if_mtu = ifs->if_mtu;
	else if (sc->sc_ifp->if_mtu != ifs->if_mtu) {
		struct ifreq ifr;

		snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s",
		    ifs->if_xname);
		ifr.ifr_mtu = sc->sc_ifp->if_mtu;

		error = (*ifs->if_ioctl)(ifs,
		    SIOCSIFMTU, (caddr_t)&ifr);
		if (error != 0) {
			log(LOG_NOTICE, "%s: invalid MTU: %u for"
			    " new member %s\n", sc->sc_ifp->if_xname,
			    ifr.ifr_mtu,
			    ifs->if_xname);
			return (EINVAL);
		}
	}

	bif = malloc(sizeof(*bif), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (bif == NULL)
		return (ENOMEM);

	bif->bif_sc = sc;
	bif->bif_ifp = ifs;
	bif->bif_flags = IFBIF_LEARNING | IFBIF_DISCOVER;
	bif->bif_savedcaps = ifs->if_capenable;

	/*
	 * Assign the interface's MAC address to the bridge if it's the first
	 * member and the MAC address of the bridge has not been changed from
	 * the default randomly generated one.
	 */
	if (V_bridge_inherit_mac && CK_LIST_EMPTY(&sc->sc_iflist) &&
	    !memcmp(IF_LLADDR(sc->sc_ifp), sc->sc_defaddr.octet, ETHER_ADDR_LEN)) {
		bcopy(IF_LLADDR(ifs), IF_LLADDR(sc->sc_ifp), ETHER_ADDR_LEN);
		sc->sc_ifaddr = ifs;
		EVENTHANDLER_INVOKE(iflladdr_event, sc->sc_ifp);
	}

	ifs->if_bridge = bif;
	ifs->if_bridge_output = bridge_output;
	ifs->if_bridge_input = bridge_input;
	ifs->if_bridge_linkstate = bridge_linkstate;
	bstp_create(&sc->sc_stp, &bif->bif_stp, bif->bif_ifp);
	/*
	 * XXX: XLOCK HERE!?!
	 *
	 * NOTE: insert_***HEAD*** should be safe for the traversals.
	 */
	CK_LIST_INSERT_HEAD(&sc->sc_iflist, bif, bif_next);

	/* Set interface capabilities to the intersection set of all members */
	bridge_mutecaps(sc);
	bridge_linkcheck(sc);

	/* Place the interface into promiscuous mode */
	switch (ifs->if_type) {
		case IFT_ETHER:
		case IFT_L2VLAN:
			error = ifpromisc(ifs, 1);
			break;
	}

	if (error)
		bridge_delete_member(sc, bif, 0);
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
	req->ifbr_addrcnt = bif->bif_addrcnt;
	req->ifbr_addrmax = bif->bif_addrmax;
	req->ifbr_addrexceeded = bif->bif_addrexceeded;

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
	struct epoch_tracker et;
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

	NET_EPOCH_ENTER(et);

	if (req->ifbr_ifsflags & IFBIF_STP) {
		if ((bif->bif_flags & IFBIF_STP) == 0) {
			error = bstp_enable(&bif->bif_stp);
			if (error) {
				NET_EPOCH_EXIT(et);
				return (error);
			}
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

	NET_EPOCH_EXIT(et);

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
	CK_LIST_FOREACH(bif, &sc->sc_iflist, bif_next)
		count++;
	CK_LIST_FOREACH(bif, &sc->sc_spanlist, bif_next)
		count++;

	buflen = sizeof(breq) * count;
	if (bifc->ifbic_len == 0) {
		bifc->ifbic_len = buflen;
		return (0);
	}
	outbuf = malloc(buflen, M_TEMP, M_NOWAIT | M_ZERO);
	if (outbuf == NULL)
		return (ENOMEM);

	count = 0;
	buf = outbuf;
	len = min(bifc->ifbic_len, buflen);
	bzero(&breq, sizeof(breq));
	CK_LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
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
	CK_LIST_FOREACH(bif, &sc->sc_spanlist, bif_next) {
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

	bifc->ifbic_len = sizeof(breq) * count;
	error = copyout(outbuf, bifc->ifbic_req, bifc->ifbic_len);
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
	CK_LIST_FOREACH(brt, &sc->sc_rtlist, brt_list)
		count++;
	buflen = sizeof(bareq) * count;

	outbuf = malloc(buflen, M_TEMP, M_NOWAIT | M_ZERO);
	if (outbuf == NULL)
		return (ENOMEM);

	count = 0;
	buf = outbuf;
	len = min(bac->ifbac_len, buflen);
	bzero(&bareq, sizeof(bareq));
	CK_LIST_FOREACH(brt, &sc->sc_rtlist, brt_list) {
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
	bac->ifbac_len = sizeof(bareq) * count;
	error = copyout(outbuf, bac->ifbac_req, bac->ifbac_len);
	free(outbuf, M_TEMP);
	return (error);
}

static int
bridge_ioctl_saddr(struct bridge_softc *sc, void *arg)
{
	struct ifbareq *req = arg;
	struct bridge_iflist *bif;
	struct epoch_tracker et;
	int error;

	NET_EPOCH_ENTER(et);
	bif = bridge_lookup_member(sc, req->ifba_ifsname);
	if (bif == NULL) {
		NET_EPOCH_EXIT(et);
		return (ENOENT);
	}

	/* bridge_rtupdate() may acquire the lock. */
	error = bridge_rtupdate(sc, req->ifba_dst, req->ifba_vlan, bif, 1,
	    req->ifba_flags);
	NET_EPOCH_EXIT(et);

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
	int vlan = req->ifba_vlan;

	/* Userspace uses '0' to mean 'any vlan' */
	if (vlan == 0)
		vlan = DOT1Q_VID_RSVD_IMPL;

	return (bridge_rtdaddr(sc, req->ifba_dst, vlan));
}

static int
bridge_ioctl_flush(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;

	BRIDGE_RT_LOCK(sc);
	bridge_rtflush(sc, req->ifbr_ifsflags);
	BRIDGE_RT_UNLOCK(sc);

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
bridge_ioctl_sifmaxaddr(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	bif->bif_addrmax = req->ifbr_addrmax;
	return (0);
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

	CK_LIST_FOREACH(bif, &sc->sc_spanlist, bif_next)
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

	CK_LIST_INSERT_HEAD(&sc->sc_spanlist, bif, bif_next);

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

	CK_LIST_FOREACH(bif, &sc->sc_spanlist, bif_next)
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
	CK_LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		if ((bif->bif_flags & IFBIF_STP) != 0)
			count++;
	}

	buflen = sizeof(bpreq) * count;
	if (bifstp->ifbpstp_len == 0) {
		bifstp->ifbpstp_len = buflen;
		return (0);
	}

	outbuf = malloc(buflen, M_TEMP, M_NOWAIT | M_ZERO);
	if (outbuf == NULL)
		return (ENOMEM);

	count = 0;
	buf = outbuf;
	len = min(bifstp->ifbpstp_len, buflen);
	bzero(&bpreq, sizeof(bpreq));
	CK_LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
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

	bifstp->ifbpstp_len = sizeof(bpreq) * count;
	error = copyout(outbuf, bifstp->ifbpstp_req, bifstp->ifbpstp_len);
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
	struct bridge_iflist *bif = ifp->if_bridge;
	struct bridge_softc *sc = NULL;

	if (bif)
		sc = bif->bif_sc;

	if (ifp->if_flags & IFF_RENAMING)
		return;
	if (V_bridge_cloner == NULL) {
		/*
		 * This detach handler can be called after
		 * vnet_bridge_uninit().  Just return in that case.
		 */
		return;
	}
	/* Check if the interface is a bridge member */
	if (sc != NULL) {
		BRIDGE_LOCK(sc);
		bridge_delete_member(sc, bif, 1);
		BRIDGE_UNLOCK(sc);
		return;
	}

	/* Check if the interface is a span port */
	BRIDGE_LIST_LOCK();
	LIST_FOREACH(sc, &V_bridge_list, sc_list) {
		BRIDGE_LOCK(sc);
		CK_LIST_FOREACH(bif, &sc->sc_spanlist, bif_next)
			if (ifp == bif->bif_ifp) {
				bridge_delete_span(sc, bif);
				break;
			}

		BRIDGE_UNLOCK(sc);
	}
	BRIDGE_LIST_UNLOCK();
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

	BRIDGE_RT_LOCK(sc);
	callout_stop(&sc->sc_brcallout);

	bstp_stop(&sc->sc_stp);

	bridge_rtflush(sc, IFBF_FLUSHDYN);
	BRIDGE_RT_UNLOCK(sc);

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
}

/*
 * bridge_enqueue:
 *
 *	Enqueue a packet on a bridge member interface.
 *
 */
static int
bridge_enqueue(struct bridge_softc *sc, struct ifnet *dst_ifp, struct mbuf *m)
{
	int len, err = 0;
	short mflags;
	struct mbuf *m0;

	/* We may be sending a fragment so traverse the mbuf */
	for (; m; m = m0) {
		m0 = m->m_nextpkt;
		m->m_nextpkt = NULL;
		len = m->m_pkthdr.len;
		mflags = m->m_flags;

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
				if_inc_counter(dst_ifp, IFCOUNTER_OERRORS, 1);
				continue;
			}
			m->m_flags &= ~M_VLANTAG;
		}

		M_ASSERTPKTHDR(m); /* We shouldn't transmit mbuf without pkthdr */
		if ((err = dst_ifp->if_transmit(dst_ifp, m))) {
			int n;

			for (m = m0, n = 1; m != NULL; m = m0, n++) {
				m0 = m->m_nextpkt;
				m_freem(m);
			}
			if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, n);
			break;
		}

		if_inc_counter(sc->sc_ifp, IFCOUNTER_OPACKETS, 1);
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OBYTES, len);
		if (mflags & M_MCAST)
			if_inc_counter(sc->sc_ifp, IFCOUNTER_OMCASTS, 1);
	}

	return (err);
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
	struct bridge_iflist *bif = ifp->if_bridge;
	struct bridge_softc *sc = NULL;

	if (bif)
		sc = bif->bif_sc;

	/*
	 * The packet didnt originate from a member interface. This should only
	 * ever happen if a member interface is removed while packets are
	 * queued for it.
	 */
	if (sc == NULL) {
		m_freem(m);
		return;
	}

	if (PFIL_HOOKED_OUT_46) {
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
	struct bridge_iflist *sbif;
	struct ifnet *bifp, *dst_if;
	struct bridge_softc *sc;
	ether_vlanid_t vlan;

	NET_EPOCH_ASSERT();

	if (m->m_len < ETHER_HDR_LEN) {
		m = m_pullup(m, ETHER_HDR_LEN);
		if (m == NULL)
			return (0);
	}

	sbif = ifp->if_bridge;
	sc = sbif->bif_sc;
	bifp = sc->sc_ifp;

	eh = mtod(m, struct ether_header *);
	vlan = VLANTAGOF(m);

	/*
	 * If bridge is down, but the original output interface is up,
	 * go ahead and send out that interface.  Otherwise, the packet
	 * is dropped below.
	 */
	if ((bifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
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
	/* Tap any traffic not passing back out the originating interface */
	if (dst_if != ifp)
		ETHER_BPF_MTAP(bifp, m);
	if (dst_if == NULL) {
		struct bridge_iflist *bif;
		struct mbuf *mc;
		int used = 0;

		bridge_span(sc, m);

		CK_LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
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

			if (CK_LIST_NEXT(bif, bif_next) == NULL) {
				used = 1;
				mc = m;
			} else {
				mc = m_dup(m, M_NOWAIT);
				if (mc == NULL) {
					if_inc_counter(bifp, IFCOUNTER_OERRORS, 1);
					continue;
				}
			}

			bridge_enqueue(sc, dst_if, mc);
		}
		if (used == 0)
			m_freem(m);
		return (0);
	}

sendunicast:
	/*
	 * XXX Spanning tree consideration here?
	 */

	bridge_span(sc, m);
	if ((dst_if->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		m_freem(m);
		return (0);
	}

	bridge_enqueue(sc, dst_if, m);
	return (0);
}

/*
 * bridge_transmit:
 *
 *	Do output on a bridge.
 *
 */
static int
bridge_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct bridge_softc *sc;
	struct ether_header *eh;
	struct ifnet *dst_if;
	int error = 0;

	sc = ifp->if_softc;

	ETHER_BPF_MTAP(ifp, m);

	eh = mtod(m, struct ether_header *);

	if (((m->m_flags & (M_BCAST|M_MCAST)) == 0) &&
	    (dst_if = bridge_rtlookup(sc, eh->ether_dhost, DOT1Q_VID_NULL)) !=
	    NULL) {
		error = bridge_enqueue(sc, dst_if, m);
	} else
		bridge_broadcast(sc, ifp, m, 0);

	return (error);
}

#ifdef ALTQ
static void
bridge_altq_start(if_t ifp)
{
	struct ifaltq *ifq = &ifp->if_snd;
	struct mbuf *m;

	IFQ_LOCK(ifq);
	IFQ_DEQUEUE_NOLOCK(ifq, m);
	while (m != NULL) {
		bridge_transmit(ifp, m);
		IFQ_DEQUEUE_NOLOCK(ifq, m);
	}
	IFQ_UNLOCK(ifq);
}

static int
bridge_altq_transmit(if_t ifp, struct mbuf *m)
{
	int err;

	if (ALTQ_IS_ENABLED(&ifp->if_snd)) {
		IFQ_ENQUEUE(&ifp->if_snd, m, err);
		if (err == 0)
			bridge_altq_start(ifp);
	} else
		err = bridge_transmit(ifp, m);

	return (err);
}
#endif	/* ALTQ */

/*
 * The ifp->if_qflush entry point for if_bridge(4) is no-op.
 */
static void
bridge_qflush(struct ifnet *ifp __unused)
{
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
	uint8_t *dst;
	int error;

	NET_EPOCH_ASSERT();

	src_if = m->m_pkthdr.rcvif;
	ifp = sc->sc_ifp;

	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
	if_inc_counter(ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
	vlan = VLANTAGOF(m);

	if ((sbif->bif_flags & IFBIF_STP) &&
	    sbif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING)
		goto drop;

	eh = mtod(m, struct ether_header *);
	dst = eh->ether_dhost;

	/* If the interface is learning, record the address. */
	if (sbif->bif_flags & IFBIF_LEARNING) {
		error = bridge_rtupdate(sc, eh->ether_shost, vlan,
		    sbif, 0, IFBAF_DYNAMIC);
		/*
		 * If the interface has addresses limits then deny any source
		 * that is not in the cache.
		 */
		if (error && sbif->bif_addrmax)
			goto drop;
	}

	if ((sbif->bif_flags & IFBIF_STP) != 0 &&
	    sbif->bif_stp.bp_state == BSTP_IFSTATE_LEARNING)
		goto drop;

#ifdef DEV_NETMAP
	/*
	 * Hand the packet to netmap only if it wasn't injected by netmap
	 * itself.
	 */
	if ((m->m_flags & M_BRIDGE_INJECT) == 0 &&
	    (if_getcapenable(ifp) & IFCAP_NETMAP) != 0) {
		ifp->if_input(ifp, m);
		return;
	}
	m->m_flags &= ~M_BRIDGE_INJECT;
#endif

	/*
	 * At this point, the port either doesn't participate
	 * in spanning tree or it is in the forwarding state.
	 */

	/*
	 * If the packet is unicast, destined for someone on
	 * "this" side of the bridge, drop it.
	 */
	if ((m->m_flags & (M_BCAST|M_MCAST)) == 0) {
		dst_if = bridge_rtlookup(sc, dst, vlan);
		if (src_if == dst_if)
			goto drop;
	} else {
		/*
		 * Check if its a reserved multicast address, any address
		 * listed in 802.1D section 7.12.6 may not be forwarded by the
		 * bridge.
		 * This is currently 01-80-C2-00-00-00 to 01-80-C2-00-00-0F
		 */
		if (dst[0] == 0x01 && dst[1] == 0x80 &&
		    dst[2] == 0xc2 && dst[3] == 0x00 &&
		    dst[4] == 0x00 && dst[5] <= 0x0f)
			goto drop;

		/* ...forward it to all interfaces. */
		if_inc_counter(ifp, IFCOUNTER_IMCASTS, 1);
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
	if (PFIL_HOOKED_IN_46) {
		if (bridge_pfil(&m, ifp, src_if, PFIL_IN) != 0)
			return;
		if (m == NULL)
			return;
	}

	if (dst_if == NULL) {
		bridge_broadcast(sc, src_if, m, 1);
		return;
	}

	/*
	 * At this point, we're dealing with a unicast frame
	 * going to a different interface.
	 */
	if ((dst_if->if_drv_flags & IFF_DRV_RUNNING) == 0)
		goto drop;

	dbif = bridge_lookup_member_if(sc, dst_if);
	if (dbif == NULL)
		/* Not a member of the bridge (anymore?) */
		goto drop;

	/* Private segments can not talk to each other */
	if (sbif->bif_flags & dbif->bif_flags & IFBIF_PRIVATE)
		goto drop;

	if ((dbif->bif_flags & IFBIF_STP) &&
	    dbif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING)
		goto drop;

	if (PFIL_HOOKED_OUT_46) {
		if (bridge_pfil(&m, ifp, dst_if, PFIL_OUT) != 0)
			return;
		if (m == NULL)
			return;
	}

	bridge_enqueue(sc, dst_if, m);
	return;

drop:
	m_freem(m);
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
	struct bridge_softc *sc = NULL;
	struct bridge_iflist *bif, *bif2;
	struct ifnet *bifp;
	struct ether_header *eh;
	struct mbuf *mc, *mc2;
	ether_vlanid_t vlan;
	int error;

	NET_EPOCH_ASSERT();

	eh = mtod(m, struct ether_header *);
	vlan = VLANTAGOF(m);

	bif = ifp->if_bridge;
	if (bif)
		sc = bif->bif_sc;

	if (sc == NULL) {
		/*
		 * This packet originated from the bridge itself, so it must
		 * have been transmitted by netmap.  Derive the "source"
		 * interface from the source address and drop the packet if the
		 * source address isn't known.
		 */
		KASSERT((m->m_flags & M_BRIDGE_INJECT) != 0,
		    ("%s: ifnet %p missing a bridge softc", __func__, ifp));
		sc = if_getsoftc(ifp);
		ifp = bridge_rtlookup(sc, eh->ether_shost, vlan);
		if (ifp == NULL) {
			if_inc_counter(sc->sc_ifp, IFCOUNTER_IERRORS, 1);
			m_freem(m);
			return (NULL);
		}
		m->m_pkthdr.rcvif = ifp;
	}
	bifp = sc->sc_ifp;
	if ((bifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return (m);

	/*
	 * Implement support for bridge monitoring. If this flag has been
	 * set on this interface, discard the packet once we push it through
	 * the bpf(4) machinery, but before we do, increment the byte and
	 * packet counters associated with this interface.
	 */
	if ((bifp->if_flags & IFF_MONITOR) != 0) {
		m->m_pkthdr.rcvif  = bifp;
		ETHER_BPF_MTAP(bifp, m);
		if_inc_counter(bifp, IFCOUNTER_IPACKETS, 1);
		if_inc_counter(bifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
		m_freem(m);
		return (NULL);
	}

	bridge_span(sc, m);

	if (m->m_flags & (M_BCAST|M_MCAST)) {
		/* Tap off 802.1D packets; they do not get forwarded. */
		if (memcmp(eh->ether_dhost, bstp_etheraddr,
		    ETHER_ADDR_LEN) == 0) {
			bstp_input(&bif->bif_stp, ifp, m); /* consumes mbuf */
			return (NULL);
		}

		if ((bif->bif_flags & IFBIF_STP) &&
		    bif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
			return (m);
		}

		/*
		 * Make a deep copy of the packet and enqueue the copy
		 * for bridge processing; return the original packet for
		 * local processing.
		 */
		mc = m_dup(m, M_NOWAIT);
		if (mc == NULL) {
			return (m);
		}

		/* Perform the bridge forwarding function with the copy. */
		bridge_forward(sc, bif, mc);

#ifdef DEV_NETMAP
		/*
		 * If netmap is enabled and has not already seen this packet,
		 * then it will be consumed by bridge_forward().
		 */
		if ((if_getcapenable(bifp) & IFCAP_NETMAP) != 0 &&
		    (m->m_flags & M_BRIDGE_INJECT) == 0) {
			m_freem(m);
			return (NULL);
		}
#endif

		/*
		 * Reinject the mbuf as arriving on the bridge so we have a
		 * chance at claiming multicast packets. We can not loop back
		 * here from ether_input as a bridge is never a member of a
		 * bridge.
		 */
		KASSERT(bifp->if_bridge == NULL,
		    ("loop created in bridge_input"));
		mc2 = m_dup(m, M_NOWAIT);
		if (mc2 != NULL) {
			/* Keep the layer3 header aligned */
			int i = min(mc2->m_pkthdr.len, max_protohdr);
			mc2 = m_copyup(mc2, i, ETHER_ALIGN);
		}
		if (mc2 != NULL) {
			mc2->m_pkthdr.rcvif = bifp;
			mc2->m_flags &= ~M_BRIDGE_INJECT;
			sc->sc_if_input(bifp, mc2);
		}

		/* Return the original packet for local processing. */
		return (m);
	}

	if ((bif->bif_flags & IFBIF_STP) &&
	    bif->bif_stp.bp_state == BSTP_IFSTATE_DISCARDING) {
		return (m);
	}

#if defined(INET) || defined(INET6)
#define	CARP_CHECK_WE_ARE_DST(iface) \
	((iface)->if_carp && (*carp_forus_p)((iface), eh->ether_dhost))
#define	CARP_CHECK_WE_ARE_SRC(iface) \
	((iface)->if_carp && (*carp_forus_p)((iface), eh->ether_shost))
#else
#define	CARP_CHECK_WE_ARE_DST(iface)	false
#define	CARP_CHECK_WE_ARE_SRC(iface)	false
#endif

#ifdef DEV_NETMAP
#define	GRAB_FOR_NETMAP(ifp, m) do {					\
	if ((if_getcapenable(ifp) & IFCAP_NETMAP) != 0 &&		\
	    ((m)->m_flags & M_BRIDGE_INJECT) == 0) {			\
		(ifp)->if_input(ifp, m);				\
		return (NULL);						\
	}								\
} while (0)
#else
#define	GRAB_FOR_NETMAP(ifp, m)
#endif

#define GRAB_OUR_PACKETS(iface)						\
	if ((iface)->if_type == IFT_GIF)				\
		continue;						\
	/* It is destined for us. */					\
	if (memcmp(IF_LLADDR(iface), eh->ether_dhost, ETHER_ADDR_LEN) == 0 || \
	    CARP_CHECK_WE_ARE_DST(iface)) {				\
		if (bif->bif_flags & IFBIF_LEARNING) {			\
			error = bridge_rtupdate(sc, eh->ether_shost,	\
			    vlan, bif, 0, IFBAF_DYNAMIC);		\
			if (error && bif->bif_addrmax) {		\
				m_freem(m);				\
				return (NULL);				\
			}						\
		}							\
		m->m_pkthdr.rcvif = iface;				\
		if ((iface) == ifp) {					\
			/* Skip bridge processing... src == dest */	\
			return (m);					\
		}							\
		/* It's passing over or to the bridge, locally. */	\
		ETHER_BPF_MTAP(bifp, m);				\
		if_inc_counter(bifp, IFCOUNTER_IPACKETS, 1);		\
		if_inc_counter(bifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);\
		/* Hand the packet over to netmap if necessary. */	\
		GRAB_FOR_NETMAP(bifp, m);				\
		/* Filter on the physical interface. */			\
		if (V_pfil_local_phys && PFIL_HOOKED_IN_46) {		\
			if (bridge_pfil(&m, NULL, ifp,			\
			    PFIL_IN) != 0 || m == NULL) {		\
				return (NULL);				\
			}						\
		}							\
		if ((iface) != bifp)					\
			ETHER_BPF_MTAP(iface, m);			\
		return (m);						\
	}								\
									\
	/* We just received a packet that we sent out. */		\
	if (memcmp(IF_LLADDR(iface), eh->ether_shost, ETHER_ADDR_LEN) == 0 || \
	    CARP_CHECK_WE_ARE_SRC(iface)) {				\
		m_freem(m);						\
		return (NULL);						\
	}

	/*
	 * Unicast.  Make sure it's not for the bridge.
	 */
	do { GRAB_OUR_PACKETS(bifp) } while (0);

	/*
	 * We only need to check members interfaces if member_ifaddrs is
	 * enabled; otherwise we should have never traffic destined for a
	 * member's lladdr.
	 */

	if (V_member_ifaddrs) {
		/*
		 * Give a chance for ifp at first priority. This will help when
		 * the packet comes through the interface like VLAN's with the
		 * same MACs on several interfaces from the same bridge. This
		 * also will save some CPU cycles in case the destination
		 * interface and the input interface (eq ifp) are the same.
		 */
		do { GRAB_OUR_PACKETS(ifp) } while (0);

		/* Now check the all bridge members. */
		CK_LIST_FOREACH(bif2, &sc->sc_iflist, bif_next) {
			GRAB_OUR_PACKETS(bif2->bif_ifp)
		}
	}

#undef CARP_CHECK_WE_ARE_DST
#undef CARP_CHECK_WE_ARE_SRC
#undef GRAB_FOR_NETMAP
#undef GRAB_OUR_PACKETS

	/* Perform the bridge forwarding function. */
	bridge_forward(sc, bif, m);

	return (NULL);
}

/*
 * Inject a packet back into the host ethernet stack.  This will generally only
 * be used by netmap when an application writes to the host TX ring.  The
 * M_BRIDGE_INJECT flag ensures that the packet is re-routed to the bridge
 * interface after ethernet processing.
 */
static void
bridge_inject(struct ifnet *ifp, struct mbuf *m)
{
	struct bridge_softc *sc;

	KASSERT((if_getcapenable(ifp) & IFCAP_NETMAP) != 0,
	    ("%s: iface %s is not running in netmap mode",
	    __func__, if_name(ifp)));
	KASSERT((m->m_flags & M_BRIDGE_INJECT) == 0,
	    ("%s: mbuf %p has M_BRIDGE_INJECT set", __func__, m));

	m->m_flags |= M_BRIDGE_INJECT;
	sc = if_getsoftc(ifp);
	sc->sc_if_input(ifp, m);
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
	int used = 0, i;

	NET_EPOCH_ASSERT();

	sbif = bridge_lookup_member_if(sc, src_if);

	/* Filter on the bridge interface before broadcasting */
	if (runfilt && PFIL_HOOKED_OUT_46) {
		if (bridge_pfil(&m, sc->sc_ifp, NULL, PFIL_OUT) != 0)
			return;
		if (m == NULL)
			return;
	}

	CK_LIST_FOREACH(dbif, &sc->sc_iflist, bif_next) {
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

		if (CK_LIST_NEXT(dbif, bif_next) == NULL) {
			mc = m;
			used = 1;
		} else {
			mc = m_dup(m, M_NOWAIT);
			if (mc == NULL) {
				if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, 1);
				continue;
			}
		}

		/*
		 * Filter on the output interface. Pass a NULL bridge interface
		 * pointer so we do not redundantly filter on the bridge for
		 * each interface we broadcast on.
		 */
		if (runfilt && PFIL_HOOKED_OUT_46) {
			if (used == 0) {
				/* Keep the layer3 header aligned */
				i = min(mc->m_pkthdr.len, max_protohdr);
				mc = m_copyup(mc, i, ETHER_ALIGN);
				if (mc == NULL) {
					if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, 1);
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

	NET_EPOCH_ASSERT();

	if (CK_LIST_EMPTY(&sc->sc_spanlist))
		return;

	CK_LIST_FOREACH(bif, &sc->sc_spanlist, bif_next) {
		dst_if = bif->bif_ifp;

		if ((dst_if->if_drv_flags & IFF_DRV_RUNNING) == 0)
			continue;

		mc = m_dup(m, M_NOWAIT);
		if (mc == NULL) {
			if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, 1);
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
bridge_rtupdate(struct bridge_softc *sc, const uint8_t *dst,
		ether_vlanid_t vlan, struct bridge_iflist *bif,
		int setflags, uint8_t flags)
{
	struct bridge_rtnode *brt;
	struct bridge_iflist *obif;
	int error;

	BRIDGE_LOCK_OR_NET_EPOCH_ASSERT(sc);

	/* Check the source address is valid and not multicast. */
	if (ETHER_IS_MULTICAST(dst) ||
	    (dst[0] == 0 && dst[1] == 0 && dst[2] == 0 &&
	     dst[3] == 0 && dst[4] == 0 && dst[5] == 0) != 0)
		return (EINVAL);

	/*
	 * A route for this destination might already exist.  If so,
	 * update it, otherwise create a new one.
	 */
	if ((brt = bridge_rtnode_lookup(sc, dst, vlan)) == NULL) {
		BRIDGE_RT_LOCK(sc);

		/* Check again, now that we have the lock. There could have
		 * been a race and we only want to insert this once. */
		if (bridge_rtnode_lookup(sc, dst, vlan) != NULL) {
			BRIDGE_RT_UNLOCK(sc);
			return (0);
		}

		if (sc->sc_brtcnt >= sc->sc_brtmax) {
			sc->sc_brtexceeded++;
			BRIDGE_RT_UNLOCK(sc);
			return (ENOSPC);
		}
		/* Check per interface address limits (if enabled) */
		if (bif->bif_addrmax && bif->bif_addrcnt >= bif->bif_addrmax) {
			bif->bif_addrexceeded++;
			BRIDGE_RT_UNLOCK(sc);
			return (ENOSPC);
		}

		/*
		 * Allocate a new bridge forwarding node, and
		 * initialize the expiration time and Ethernet
		 * address.
		 */
		brt = uma_zalloc(V_bridge_rtnode_zone, M_NOWAIT | M_ZERO);
		if (brt == NULL) {
			BRIDGE_RT_UNLOCK(sc);
			return (ENOMEM);
		}
		brt->brt_vnet = curvnet;

		if (bif->bif_flags & IFBIF_STICKY)
			brt->brt_flags = IFBAF_STICKY;
		else
			brt->brt_flags = IFBAF_DYNAMIC;

		memcpy(brt->brt_addr, dst, ETHER_ADDR_LEN);
		brt->brt_vlan = vlan;

		brt->brt_dst = bif;
		if ((error = bridge_rtnode_insert(sc, brt)) != 0) {
			uma_zfree(V_bridge_rtnode_zone, brt);
			BRIDGE_RT_UNLOCK(sc);
			return (error);
		}
		bif->bif_addrcnt++;

		BRIDGE_RT_UNLOCK(sc);
	}

	if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC &&
	    (obif = brt->brt_dst) != bif) {
		MPASS(obif != NULL);

		BRIDGE_RT_LOCK(sc);
		brt->brt_dst->bif_addrcnt--;
		brt->brt_dst = bif;
		brt->brt_dst->bif_addrcnt++;
		BRIDGE_RT_UNLOCK(sc);

		if (V_log_mac_flap &&
		    ppsratecheck(&V_log_last, &V_log_count, V_log_interval)) {
			log(LOG_NOTICE,
			    "%s: mac address %6D vlan %d moved from %s to %s\n",
			    sc->sc_ifp->if_xname,
			    &brt->brt_addr[0], ":",
			    brt->brt_vlan,
			    obif->bif_ifp->if_xname,
			    bif->bif_ifp->if_xname);
		}
	}

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
bridge_rtlookup(struct bridge_softc *sc, const uint8_t *addr,
		ether_vlanid_t vlan)
{
	struct bridge_rtnode *brt;

	NET_EPOCH_ASSERT();

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

	NET_EPOCH_ASSERT();
	BRIDGE_RT_LOCK_ASSERT(sc);

	/* Make sure we actually need to do this. */
	if (sc->sc_brtcnt <= sc->sc_brtmax)
		return;

	/* Force an aging cycle; this might trim enough addresses. */
	bridge_rtage(sc);
	if (sc->sc_brtcnt <= sc->sc_brtmax)
		return;

	CK_LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
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

	BRIDGE_RT_LOCK_ASSERT(sc);

	/* Destruction of rtnodes requires a proper vnet context */
	CURVNET_SET(sc->sc_ifp->if_vnet);
	bridge_rtage(sc);

	if (sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING)
		callout_reset(&sc->sc_brcallout,
		    bridge_rtable_prune_period * hz, bridge_timer, sc);
	CURVNET_RESTORE();
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

	BRIDGE_RT_LOCK_ASSERT(sc);

	CK_LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
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

	BRIDGE_RT_LOCK_ASSERT(sc);

	CK_LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
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
bridge_rtdaddr(struct bridge_softc *sc, const uint8_t *addr,
	       ether_vlanid_t vlan)
{
	struct bridge_rtnode *brt;
	int found = 0;

	BRIDGE_RT_LOCK(sc);

	/*
	 * If vlan is DOT1Q_VID_RSVD_IMPL then we want to delete for all vlans
	 * so the lookup may return more than one.
	 */
	while ((brt = bridge_rtnode_lookup(sc, addr, vlan)) != NULL) {
		bridge_rtnode_destroy(sc, brt);
		found = 1;
	}

	BRIDGE_RT_UNLOCK(sc);

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

	BRIDGE_RT_LOCK_ASSERT(sc);

	CK_LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
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
static void
bridge_rtable_init(struct bridge_softc *sc)
{
	int i;

	sc->sc_rthash = malloc(sizeof(*sc->sc_rthash) * BRIDGE_RTHASH_SIZE,
	    M_DEVBUF, M_WAITOK);

	for (i = 0; i < BRIDGE_RTHASH_SIZE; i++)
		CK_LIST_INIT(&sc->sc_rthash[i]);

	sc->sc_rthash_key = arc4random();
	CK_LIST_INIT(&sc->sc_rtlist);
}

/*
 * bridge_rtable_fini:
 *
 *	Deconstruct the route table for this bridge.
 */
static void
bridge_rtable_fini(struct bridge_softc *sc)
{

	KASSERT(sc->sc_brtcnt == 0,
	    ("%s: %d bridge routes referenced", __func__, sc->sc_brtcnt));
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
bridge_rtnode_lookup(struct bridge_softc *sc, const uint8_t *addr,
		     ether_vlanid_t vlan)
{
	struct bridge_rtnode *brt;
	uint32_t hash;
	int dir;

	BRIDGE_RT_LOCK_OR_NET_EPOCH_ASSERT(sc);

	hash = bridge_rthash(sc, addr);
	CK_LIST_FOREACH(brt, &sc->sc_rthash[hash], brt_hash) {
		dir = bridge_rtnode_addr_cmp(addr, brt->brt_addr);
		if (dir == 0 && (brt->brt_vlan == vlan || vlan == DOT1Q_VID_RSVD_IMPL))
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

	BRIDGE_RT_LOCK_ASSERT(sc);

	hash = bridge_rthash(sc, brt->brt_addr);

	lbrt = CK_LIST_FIRST(&sc->sc_rthash[hash]);
	if (lbrt == NULL) {
		CK_LIST_INSERT_HEAD(&sc->sc_rthash[hash], brt, brt_hash);
		goto out;
	}

	do {
		dir = bridge_rtnode_addr_cmp(brt->brt_addr, lbrt->brt_addr);
		if (dir == 0 && brt->brt_vlan == lbrt->brt_vlan)
			return (EEXIST);
		if (dir > 0) {
			CK_LIST_INSERT_BEFORE(lbrt, brt, brt_hash);
			goto out;
		}
		if (CK_LIST_NEXT(lbrt, brt_hash) == NULL) {
			CK_LIST_INSERT_AFTER(lbrt, brt, brt_hash);
			goto out;
		}
		lbrt = CK_LIST_NEXT(lbrt, brt_hash);
	} while (lbrt != NULL);

#ifdef DIAGNOSTIC
	panic("bridge_rtnode_insert: impossible");
#endif

out:
	CK_LIST_INSERT_HEAD(&sc->sc_rtlist, brt, brt_list);
	sc->sc_brtcnt++;

	return (0);
}

static void
bridge_rtnode_destroy_cb(struct epoch_context *ctx)
{
	struct bridge_rtnode *brt;

	brt = __containerof(ctx, struct bridge_rtnode, brt_epoch_ctx);

	CURVNET_SET(brt->brt_vnet);
	uma_zfree(V_bridge_rtnode_zone, brt);
	CURVNET_RESTORE();
}

/*
 * bridge_rtnode_destroy:
 *
 *	Destroy a bridge rtnode.
 */
static void
bridge_rtnode_destroy(struct bridge_softc *sc, struct bridge_rtnode *brt)
{
	BRIDGE_RT_LOCK_ASSERT(sc);

	CK_LIST_REMOVE(brt, brt_hash);

	CK_LIST_REMOVE(brt, brt_list);
	sc->sc_brtcnt--;
	brt->brt_dst->bif_addrcnt--;

	NET_EPOCH_CALL(bridge_rtnode_destroy_cb, &brt->brt_epoch_ctx);
}

/*
 * bridge_rtable_expire:
 *
 *	Set the expiry time for all routes on an interface.
 */
static void
bridge_rtable_expire(struct ifnet *ifp, int age)
{
	struct bridge_iflist *bif = NULL;
	struct bridge_softc *sc = NULL;
	struct bridge_rtnode *brt;

	CURVNET_SET(ifp->if_vnet);

	bif = ifp->if_bridge;
	if (bif)
		sc = bif->bif_sc;
	MPASS(sc != NULL);
	BRIDGE_RT_LOCK(sc);

	/*
	 * If the age is zero then flush, otherwise set all the expiry times to
	 * age for the interface
	 */
	if (age == 0)
		bridge_rtdelete(sc, ifp, IFBF_FLUSHDYN);
	else {
		CK_LIST_FOREACH(brt, &sc->sc_rtlist, brt_list) {
			/* Cap the expiry time to 'age' */
			if (brt->brt_ifp == ifp &&
			    brt->brt_expire > time_uptime + age &&
			    (brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC)
				brt->brt_expire = time_uptime + age;
		}
	}
	BRIDGE_RT_UNLOCK(sc);
	CURVNET_RESTORE();
}

/*
 * bridge_state_change:
 *
 *	Callback from the bridgestp code when a port changes states.
 */
static void
bridge_state_change(struct ifnet *ifp, int state)
{
	struct bridge_iflist *bif = ifp->if_bridge;
	struct bridge_softc *sc = bif->bif_sc;
	static const char *stpstates[] = {
		"disabled",
		"listening",
		"learning",
		"forwarding",
		"blocking",
		"discarding"
	};

	CURVNET_SET(ifp->if_vnet);
	if (V_log_stp)
		log(LOG_NOTICE, "%s: state changed to %s on %s\n",
		    sc->sc_ifp->if_xname, stpstates[state], ifp->if_xname);
	CURVNET_RESTORE();
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
	int snap, error, i;
	struct ether_header *eh1, eh2;
	struct llc llc1;
	u_int16_t ether_type;
	pfil_return_t rv;
#ifdef INET
	struct ip *ip = NULL;
	int hlen = 0;
#endif

	snap = 0;
	error = -1;	/* Default error if not error == 0 */

#if 0
	/* we may return with the IP fields swapped, ensure its not shared */
	KASSERT(M_WRITABLE(*mp), ("%s: modifying a shared mbuf", __func__));
#endif

	if (V_pfil_bridge == 0 && V_pfil_member == 0 && V_pfil_ipfw == 0)
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
	 * If we're trying to filter bridge traffic, only look at traffic for
	 * protocols available in the kernel (IPv4 and/or IPv6) to avoid
	 * passing traffic for an unsupported protocol to the filter.  This is
	 * lame since if we really wanted, say, an AppleTalk filter, we are
	 * hosed, but of course we don't have an AppleTalk filter to begin
	 * with.  (Note that since pfil doesn't understand ARP it will pass
	 * *ALL* ARP traffic.)
	 */
	switch (ether_type) {
#ifdef INET
		case ETHERTYPE_ARP:
		case ETHERTYPE_REVARP:
			if (V_pfil_ipfw_arp == 0)
				return (0); /* Automatically pass */

			/* FALLTHROUGH */
		case ETHERTYPE_IP:
#endif
#ifdef INET6
		case ETHERTYPE_IPV6:
#endif /* INET6 */
			break;

		default:
			/*
			 * We get here if the packet isn't from a supported
			 * protocol.  Check to see if the user wants to pass
			 * non-IP packets, these will not be checked by pfil(9)
			 * and passed unconditionally so the default is to
			 * drop.
			 */
			if (V_pfil_onlyip)
				goto bad;
	}

	/* Run the packet through pfil before stripping link headers */
	if (PFIL_HOOKED_OUT(V_link_pfil_head) && V_pfil_ipfw != 0 &&
	    dir == PFIL_OUT && ifp != NULL) {
		switch (pfil_mbuf_out(V_link_pfil_head, mp, ifp, NULL)) {
		case PFIL_DROPPED:
			return (EACCES);
		case PFIL_CONSUMED:
			return (0);
		}
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
#ifdef INET
			case ETHERTYPE_IP:
				error = bridge_ip_checkbasic(mp);
				break;
#endif
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

	error = 0;

	/*
	 * Run the packet through pfil
	 */
	rv = PFIL_PASS;
	switch (ether_type) {
#ifdef INET
	case ETHERTYPE_IP:
		/*
		 * Run pfil on the member interface and the bridge, both can
		 * be skipped by clearing pfil_member or pfil_bridge.
		 *
		 * Keep the order:
		 *   in_if -> bridge_if -> out_if
		 */
		if (V_pfil_bridge && dir == PFIL_OUT && bifp != NULL && (rv =
		    pfil_mbuf_out(V_inet_pfil_head, mp, bifp, NULL)) !=
		    PFIL_PASS)
			break;

		if (V_pfil_member && ifp != NULL) {
			rv = (dir == PFIL_OUT) ?
			    pfil_mbuf_out(V_inet_pfil_head, mp, ifp, NULL) :
			    pfil_mbuf_in(V_inet_pfil_head, mp, ifp, NULL);
			if (rv != PFIL_PASS)
				break;
		}

		if (V_pfil_bridge && dir == PFIL_IN && bifp != NULL && (rv =
		    pfil_mbuf_in(V_inet_pfil_head, mp, bifp, NULL)) !=
		    PFIL_PASS)
			break;

		/* check if we need to fragment the packet */
		/* bridge_fragment generates a mbuf chain of packets */
		/* that already include eth headers */
		if (V_pfil_member && ifp != NULL && dir == PFIL_OUT) {
			i = (*mp)->m_pkthdr.len;
			if (i > ifp->if_mtu) {
				error = bridge_fragment(ifp, mp, &eh2, snap,
					    &llc1);
				return (error);
			}
		}

		/* Recalculate the ip checksum. */
		ip = mtod(*mp, struct ip *);
		hlen = ip->ip_hl << 2;
		if (hlen < sizeof(struct ip))
			goto bad;
		if (hlen > (*mp)->m_len) {
			if ((*mp = m_pullup(*mp, hlen)) == NULL)
				goto bad;
			ip = mtod(*mp, struct ip *);
			if (ip == NULL)
				goto bad;
		}
		ip->ip_sum = 0;
		if (hlen == sizeof(struct ip))
			ip->ip_sum = in_cksum_hdr(ip);
		else
			ip->ip_sum = in_cksum(*mp, hlen);

		break;
#endif /* INET */
#ifdef INET6
	case ETHERTYPE_IPV6:
		if (V_pfil_bridge && dir == PFIL_OUT && bifp != NULL && (rv =
		    pfil_mbuf_out(V_inet6_pfil_head, mp, bifp, NULL)) !=
		    PFIL_PASS)
			break;

		if (V_pfil_member && ifp != NULL) {
			rv = (dir == PFIL_OUT) ?
			    pfil_mbuf_out(V_inet6_pfil_head, mp, ifp, NULL) :
			    pfil_mbuf_in(V_inet6_pfil_head, mp, ifp, NULL);
			if (rv != PFIL_PASS)
				break;
		}

		if (V_pfil_bridge && dir == PFIL_IN && bifp != NULL && (rv =
		    pfil_mbuf_in(V_inet6_pfil_head, mp, bifp, NULL)) !=
		    PFIL_PASS)
			break;
		break;
#endif
	}

	switch (rv) {
	case PFIL_CONSUMED:
		return (0);
	case PFIL_DROPPED:
		return (EACCES);
	default:
		break;
	}

	error = -1;

	/*
	 * Finally, put everything back the way it was and return
	 */
	if (snap) {
		M_PREPEND(*mp, sizeof(struct llc), M_NOWAIT);
		if (*mp == NULL)
			return (error);
		bcopy(&llc1, mtod(*mp, caddr_t), sizeof(struct llc));
	}

	M_PREPEND(*mp, ETHER_HDR_LEN, M_NOWAIT);
	if (*mp == NULL)
		return (error);
	bcopy(&eh2, mtod(*mp, caddr_t), ETHER_HDR_LEN);

	return (0);

bad:
	m_freem(*mp);
	*mp = NULL;
	return (error);
}

#ifdef INET
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
			KMOD_IPSTAT_INC(ips_toosmall);
			goto bad;
		}
	} else if (__predict_false(m->m_len < sizeof (struct ip))) {
		if ((m = m_pullup(m, sizeof (struct ip))) == NULL) {
			KMOD_IPSTAT_INC(ips_toosmall);
			goto bad;
		}
	}
	ip = mtod(m, struct ip *);
	if (ip == NULL) goto bad;

	if (ip->ip_v != IPVERSION) {
		KMOD_IPSTAT_INC(ips_badvers);
		goto bad;
	}
	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip)) { /* minimum header length */
		KMOD_IPSTAT_INC(ips_badhlen);
		goto bad;
	}
	if (hlen > m->m_len) {
		if ((m = m_pullup(m, hlen)) == NULL) {
			KMOD_IPSTAT_INC(ips_badhlen);
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
		KMOD_IPSTAT_INC(ips_badsum);
		goto bad;
	}

	/* Retrieve the packet length. */
	len = ntohs(ip->ip_len);

	/*
	 * Check for additional length bogosity
	 */
	if (len < hlen) {
		KMOD_IPSTAT_INC(ips_badlen);
		goto bad;
	}

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IP header would have us expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len < len) {
		KMOD_IPSTAT_INC(ips_tooshort);
		goto bad;
	}

	/* Checks out, proceed */
	*mp = m;
	return (0);

bad:
	*mp = m;
	return (-1);
}
#endif /* INET */

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
			IP6STAT_INC(ip6s_toosmall);
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			goto bad;
		}
	} else if (__predict_false(m->m_len < sizeof(struct ip6_hdr))) {
		struct ifnet *inifp = m->m_pkthdr.rcvif;
		if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
			IP6STAT_INC(ip6s_toosmall);
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			goto bad;
		}
	}

	ip6 = mtod(m, struct ip6_hdr *);

	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		IP6STAT_INC(ip6s_badvers);
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

#ifdef INET
/*
 * bridge_fragment:
 *
 *	Fragment mbuf chain in multiple packets and prepend ethernet header.
 */
static int
bridge_fragment(struct ifnet *ifp, struct mbuf **mp, struct ether_header *eh,
    int snap, struct llc *llc)
{
	struct mbuf *m = *mp, *nextpkt = NULL, *mprev = NULL, *mcur = NULL;
	struct ip *ip;
	int error = -1;

	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == NULL)
		goto dropit;
	ip = mtod(m, struct ip *);

	m->m_pkthdr.csum_flags |= CSUM_IP;
	error = ip_fragment(ip, &m, ifp->if_mtu, ifp->if_hwassist);
	if (error)
		goto dropit;

	/*
	 * Walk the chain and re-add the Ethernet header for
	 * each mbuf packet.
	 */
	for (mcur = m; mcur; mcur = mcur->m_nextpkt) {
		nextpkt = mcur->m_nextpkt;
		mcur->m_nextpkt = NULL;
		if (snap) {
			M_PREPEND(mcur, sizeof(struct llc), M_NOWAIT);
			if (mcur == NULL) {
				error = ENOBUFS;
				if (mprev != NULL)
					mprev->m_nextpkt = nextpkt;
				goto dropit;
			}
			bcopy(llc, mtod(mcur, caddr_t),sizeof(struct llc));
		}

		M_PREPEND(mcur, ETHER_HDR_LEN, M_NOWAIT);
		if (mcur == NULL) {
			error = ENOBUFS;
			if (mprev != NULL)
				mprev->m_nextpkt = nextpkt;
			goto dropit;
		}
		bcopy(eh, mtod(mcur, caddr_t), ETHER_HDR_LEN);

		/*
		 * The previous two M_PREPEND could have inserted one or two
		 * mbufs in front so we have to update the previous packet's
		 * m_nextpkt.
		 */
		mcur->m_nextpkt = nextpkt;
		if (mprev != NULL)
			mprev->m_nextpkt = mcur;
		else {
			/* The first mbuf in the original chain needs to be
			 * updated. */
			*mp = mcur;
		}
		mprev = mcur;
	}

	KMOD_IPSTAT_INC(ips_fragmented);
	return (error);

dropit:
	for (mcur = *mp; mcur; mcur = m) { /* droping the full packet chain */
		m = mcur->m_nextpkt;
		m_freem(mcur);
	}
	return (error);
}
#endif /* INET */

static void
bridge_linkstate(struct ifnet *ifp)
{
	struct bridge_softc *sc = NULL;
	struct bridge_iflist *bif;
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);

	bif = ifp->if_bridge;
	if (bif)
		sc = bif->bif_sc;

	if (sc != NULL) {
		bridge_linkcheck(sc);
		bstp_linkstate(&bif->bif_stp);
	}

	NET_EPOCH_EXIT(et);
}

static void
bridge_linkcheck(struct bridge_softc *sc)
{
	struct bridge_iflist *bif;
	int new_link, hasls;

	BRIDGE_LOCK_OR_NET_EPOCH_ASSERT(sc);

	new_link = LINK_STATE_DOWN;
	hasls = 0;
	/* Our link is considered up if at least one of our ports is active */
	CK_LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		if (bif->bif_ifp->if_capabilities & IFCAP_LINKSTATE)
			hasls++;
		if (bif->bif_ifp->if_link_state == LINK_STATE_UP) {
			new_link = LINK_STATE_UP;
			break;
		}
	}
	if (!CK_LIST_EMPTY(&sc->sc_iflist) && !hasls) {
		/* If no interfaces support link-state then we default to up */
		new_link = LINK_STATE_UP;
	}
	if_link_state_change(sc->sc_ifp, new_link);
}
