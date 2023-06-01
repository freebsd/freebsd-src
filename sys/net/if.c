/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2010 Bjoern A. Zeeb <bz@FreeBSD.org>
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if.c	8.5 (Berkeley) 1/9/95
 * $FreeBSD$
 */

#include "opt_bpf.h"
#include "opt_inet6.h"
#include "opt_inet.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/conf.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/domainset.h>
#include <sys/sbuf.h>
#include <sys/bus.h>
#include <sys/epoch.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/refcount.h>
#include <sys/module.h>
#include <sys/nv.h>
#include <sys/rwlock.h>
#include <sys/sockio.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/taskqueue.h>
#include <sys/domain.h>
#include <sys/jail.h>
#include <sys/priv.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <machine/stdarg.h>
#include <vm/uma.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_clone.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_mib.h>
#include <net/if_private.h>
#include <net/if_vlan_var.h>
#include <net/radix.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/vnet.h>

#if defined(INET) || defined(INET6)
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_carp.h>
#ifdef INET
#include <net/debugnet.h>
#include <netinet/if_ether.h>
#endif /* INET */
#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/in6_ifattach.h>
#endif /* INET6 */
#endif /* INET || INET6 */

#include <security/mac/mac_framework.h>

/*
 * Consumers of struct ifreq such as tcpdump assume no pad between ifr_name
 * and ifr_ifru when it is used in SIOCGIFCONF.
 */
_Static_assert(sizeof(((struct ifreq *)0)->ifr_name) ==
    offsetof(struct ifreq, ifr_ifru), "gap between ifr_name and ifr_ifru");

__read_mostly epoch_t net_epoch_preempt;
#ifdef COMPAT_FREEBSD32
#include <sys/mount.h>
#include <compat/freebsd32/freebsd32.h>

struct ifreq_buffer32 {
	uint32_t	length;		/* (size_t) */
	uint32_t	buffer;		/* (void *) */
};

/*
 * Interface request structure used for socket
 * ioctl's.  All interface ioctl's must have parameter
 * definitions which begin with ifr_name.  The
 * remainder may be interface specific.
 */
struct ifreq32 {
	char	ifr_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	union {
		struct sockaddr	ifru_addr;
		struct sockaddr	ifru_dstaddr;
		struct sockaddr	ifru_broadaddr;
		struct ifreq_buffer32 ifru_buffer;
		short		ifru_flags[2];
		short		ifru_index;
		int		ifru_jid;
		int		ifru_metric;
		int		ifru_mtu;
		int		ifru_phys;
		int		ifru_media;
		uint32_t	ifru_data;
		int		ifru_cap[2];
		u_int		ifru_fib;
		u_char		ifru_vlan_pcp;
	} ifr_ifru;
};
CTASSERT(sizeof(struct ifreq) == sizeof(struct ifreq32));
CTASSERT(__offsetof(struct ifreq, ifr_ifru) ==
    __offsetof(struct ifreq32, ifr_ifru));

struct ifconf32 {
	int32_t	ifc_len;
	union {
		uint32_t	ifcu_buf;
		uint32_t	ifcu_req;
	} ifc_ifcu;
};
#define	SIOCGIFCONF32	_IOWR('i', 36, struct ifconf32)

struct ifdrv32 {
	char		ifd_name[IFNAMSIZ];
	uint32_t	ifd_cmd;
	uint32_t	ifd_len;
	uint32_t	ifd_data;
};
#define SIOCSDRVSPEC32	_IOC_NEWTYPE(SIOCSDRVSPEC, struct ifdrv32)
#define SIOCGDRVSPEC32	_IOC_NEWTYPE(SIOCGDRVSPEC, struct ifdrv32)

struct ifgroupreq32 {
	char	ifgr_name[IFNAMSIZ];
	u_int	ifgr_len;
	union {
		char		ifgru_group[IFNAMSIZ];
		uint32_t	ifgru_groups;
	} ifgr_ifgru;
};
#define	SIOCAIFGROUP32	_IOC_NEWTYPE(SIOCAIFGROUP, struct ifgroupreq32)
#define	SIOCGIFGROUP32	_IOC_NEWTYPE(SIOCGIFGROUP, struct ifgroupreq32)
#define	SIOCDIFGROUP32	_IOC_NEWTYPE(SIOCDIFGROUP, struct ifgroupreq32)
#define	SIOCGIFGMEMB32	_IOC_NEWTYPE(SIOCGIFGMEMB, struct ifgroupreq32)

struct ifmediareq32 {
	char		ifm_name[IFNAMSIZ];
	int		ifm_current;
	int		ifm_mask;
	int		ifm_status;
	int		ifm_active;
	int		ifm_count;
	uint32_t	ifm_ulist;	/* (int *) */
};
#define	SIOCGIFMEDIA32	_IOC_NEWTYPE(SIOCGIFMEDIA, struct ifmediareq32)
#define	SIOCGIFXMEDIA32	_IOC_NEWTYPE(SIOCGIFXMEDIA, struct ifmediareq32)
#endif /* COMPAT_FREEBSD32 */

union ifreq_union {
	struct ifreq	ifr;
#ifdef COMPAT_FREEBSD32
	struct ifreq32	ifr32;
#endif
};

SYSCTL_NODE(_net, PF_LINK, link, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Link layers");
SYSCTL_NODE(_net_link, 0, generic, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Generic link-management");

SYSCTL_INT(_net_link, OID_AUTO, ifqmaxlen, CTLFLAG_RDTUN,
    &ifqmaxlen, 0, "max send queue size");

/* Log link state change events */
static int log_link_state_change = 1;

SYSCTL_INT(_net_link, OID_AUTO, log_link_state_change, CTLFLAG_RW,
	&log_link_state_change, 0,
	"log interface link state change events");

/* Log promiscuous mode change events */
static int log_promisc_mode_change = 1;

SYSCTL_INT(_net_link, OID_AUTO, log_promisc_mode_change, CTLFLAG_RDTUN,
	&log_promisc_mode_change, 1,
	"log promiscuous mode change events");

/* Interface description */
static unsigned int ifdescr_maxlen = 1024;
SYSCTL_UINT(_net, OID_AUTO, ifdescr_maxlen, CTLFLAG_RW,
	&ifdescr_maxlen, 0,
	"administrative maximum length for interface description");

static MALLOC_DEFINE(M_IFDESCR, "ifdescr", "ifnet descriptions");

/* global sx for non-critical path ifdescr */
static struct sx ifdescr_sx;
SX_SYSINIT(ifdescr_sx, &ifdescr_sx, "ifnet descr");

void	(*ng_ether_link_state_p)(struct ifnet *ifp, int state);
void	(*lagg_linkstate_p)(struct ifnet *ifp, int state);
/* These are external hooks for CARP. */
void	(*carp_linkstate_p)(struct ifnet *ifp);
void	(*carp_demote_adj_p)(int, char *);
int	(*carp_master_p)(struct ifaddr *);
#if defined(INET) || defined(INET6)
int	(*carp_forus_p)(struct ifnet *ifp, u_char *dhost);
int	(*carp_output_p)(struct ifnet *ifp, struct mbuf *m,
    const struct sockaddr *sa);
int	(*carp_ioctl_p)(struct ifreq *, u_long, struct thread *);   
int	(*carp_attach_p)(struct ifaddr *, int);
void	(*carp_detach_p)(struct ifaddr *, bool);
#endif
#ifdef INET
int	(*carp_iamatch_p)(struct ifaddr *, uint8_t **);
#endif
#ifdef INET6
struct ifaddr *(*carp_iamatch6_p)(struct ifnet *ifp, struct in6_addr *taddr6);
caddr_t	(*carp_macmatch6_p)(struct ifnet *ifp, struct mbuf *m,
    const struct in6_addr *taddr);
#endif

struct mbuf *(*tbr_dequeue_ptr)(struct ifaltq *, int) = NULL;

/*
 * XXX: Style; these should be sorted alphabetically, and unprototyped
 * static functions should be prototyped. Currently they are sorted by
 * declaration order.
 */
static void	if_attachdomain(void *);
static void	if_attachdomain1(struct ifnet *);
static int	ifconf(u_long, caddr_t);
static void	if_input_default(struct ifnet *, struct mbuf *);
static int	if_requestencap_default(struct ifnet *, struct if_encap_req *);
static int	if_setflag(struct ifnet *, int, int, int *, int);
static int	if_transmit_default(struct ifnet *ifp, struct mbuf *m);
static void	if_unroute(struct ifnet *, int flag, int fam);
static int	if_delmulti_locked(struct ifnet *, struct ifmultiaddr *, int);
static void	do_link_state_change(void *, int);
static int	if_getgroup(struct ifgroupreq *, struct ifnet *);
static int	if_getgroupmembers(struct ifgroupreq *);
static void	if_delgroups(struct ifnet *);
static void	if_attach_internal(struct ifnet *, bool);
static int	if_detach_internal(struct ifnet *, bool);
static void	if_siocaddmulti(void *, int);
static void	if_link_ifnet(struct ifnet *);
static bool	if_unlink_ifnet(struct ifnet *, bool);
#ifdef VIMAGE
static int	if_vmove(struct ifnet *, struct vnet *);
#endif

#ifdef INET6
/*
 * XXX: declare here to avoid to include many inet6 related files..
 * should be more generalized?
 */
extern void	nd6_setmtu(struct ifnet *);
#endif

/* ipsec helper hooks */
VNET_DEFINE(struct hhook_head *, ipsec_hhh_in[HHOOK_IPSEC_COUNT]);
VNET_DEFINE(struct hhook_head *, ipsec_hhh_out[HHOOK_IPSEC_COUNT]);

int	ifqmaxlen = IFQ_MAXLEN;
VNET_DEFINE(struct ifnethead, ifnet);	/* depend on static init XXX */
VNET_DEFINE(struct ifgrouphead, ifg_head);

/* Table of ifnet by index. */
static int if_index;
static int if_indexlim = 8;
static struct ifindex_entry {
	struct ifnet	*ife_ifnet;
	uint16_t	ife_gencnt;
} *ifindex_table;

SYSCTL_NODE(_net_link_generic, IFMIB_SYSTEM, system,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Variables global to all interfaces");
static int
sysctl_ifcount(SYSCTL_HANDLER_ARGS)
{
	int rv = 0;

	IFNET_RLOCK();
	for (int i = 1; i <= if_index; i++)
		if (ifindex_table[i].ife_ifnet != NULL &&
		    ifindex_table[i].ife_ifnet->if_vnet == curvnet)
			rv = i;
	IFNET_RUNLOCK();

	return (sysctl_handle_int(oidp, &rv, 0, req));
}
SYSCTL_PROC(_net_link_generic_system, IFMIB_IFCOUNT, ifcount,
    CTLTYPE_INT | CTLFLAG_VNET | CTLFLAG_RD, NULL, 0, sysctl_ifcount, "I",
    "Maximum known interface index");

/*
 * The global network interface list (V_ifnet) and related state (such as
 * if_index, if_indexlim, and ifindex_table) are protected by an sxlock.
 * This may be acquired to stabilise the list, or we may rely on NET_EPOCH.
 */
struct sx ifnet_sxlock;
SX_SYSINIT_FLAGS(ifnet_sx, &ifnet_sxlock, "ifnet_sx", SX_RECURSE);

struct sx ifnet_detach_sxlock;
SX_SYSINIT_FLAGS(ifnet_detach, &ifnet_detach_sxlock, "ifnet_detach_sx",
    SX_RECURSE);

#ifdef VIMAGE
#define	VNET_IS_SHUTTING_DOWN(_vnet)					\
    ((_vnet)->vnet_shutdown && (_vnet)->vnet_state < SI_SUB_VNET_DONE)
#endif

static	if_com_alloc_t *if_com_alloc[256];
static	if_com_free_t *if_com_free[256];

static MALLOC_DEFINE(M_IFNET, "ifnet", "interface internals");
MALLOC_DEFINE(M_IFADDR, "ifaddr", "interface address");
MALLOC_DEFINE(M_IFMADDR, "ether_multi", "link-level multicast address");

struct ifnet *
ifnet_byindex(u_int idx)
{
	struct ifnet *ifp;

	NET_EPOCH_ASSERT();

	if (__predict_false(idx > if_index))
		return (NULL);

	ifp = ck_pr_load_ptr(&ifindex_table[idx].ife_ifnet);

	if (curvnet != NULL && ifp != NULL && ifp->if_vnet != curvnet)
		ifp = NULL;

	return (ifp);
}

struct ifnet *
ifnet_byindex_ref(u_int idx)
{
	struct ifnet *ifp;

	ifp = ifnet_byindex(idx);
	if (ifp == NULL || (ifp->if_flags & IFF_DYING))
		return (NULL);
	if (!if_try_ref(ifp))
		return (NULL);
	return (ifp);
}

struct ifnet *
ifnet_byindexgen(uint16_t idx, uint16_t gen)
{
	struct ifnet *ifp;

	NET_EPOCH_ASSERT();

	if (__predict_false(idx > if_index))
		return (NULL);

	ifp = ck_pr_load_ptr(&ifindex_table[idx].ife_ifnet);

	if (ifindex_table[idx].ife_gencnt == gen)
		return (ifp);
	else
		return (NULL);
}

/*
 * Network interface utility routines.
 *
 * Routines with ifa_ifwith* names take sockaddr *'s as
 * parameters.
 */

static void
if_init_idxtable(void *arg __unused)
{

	ifindex_table = malloc(if_indexlim * sizeof(*ifindex_table),
	    M_IFNET, M_WAITOK | M_ZERO);
}
SYSINIT(if_init, SI_SUB_INIT_IF, SI_ORDER_SECOND, if_init_idxtable, NULL);

static void
vnet_if_init(const void *unused __unused)
{

	CK_STAILQ_INIT(&V_ifnet);
	CK_STAILQ_INIT(&V_ifg_head);
	vnet_if_clone_init();
}
VNET_SYSINIT(vnet_if_init, SI_SUB_INIT_IF, SI_ORDER_SECOND, vnet_if_init,
    NULL);

static void
if_link_ifnet(struct ifnet *ifp)
{

	IFNET_WLOCK();
	CK_STAILQ_INSERT_TAIL(&V_ifnet, ifp, if_link);
#ifdef VIMAGE
	curvnet->vnet_ifcnt++;
#endif
	IFNET_WUNLOCK();
}

static bool
if_unlink_ifnet(struct ifnet *ifp, bool vmove)
{
	struct ifnet *iter;
	int found = 0;

	IFNET_WLOCK();
	CK_STAILQ_FOREACH(iter, &V_ifnet, if_link)
		if (iter == ifp) {
			CK_STAILQ_REMOVE(&V_ifnet, ifp, ifnet, if_link);
			if (!vmove)
				ifp->if_flags |= IFF_DYING;
			found = 1;
			break;
		}
#ifdef VIMAGE
	curvnet->vnet_ifcnt--;
#endif
	IFNET_WUNLOCK();

	return (found);
}

#ifdef VIMAGE
static void
vnet_if_return(const void *unused __unused)
{
	struct ifnet *ifp, *nifp;
	struct ifnet **pending;
	int found __diagused;
	int i;

	i = 0;

	/*
	 * We need to protect our access to the V_ifnet tailq. Ordinarily we'd
	 * enter NET_EPOCH, but that's not possible, because if_vmove() calls
	 * if_detach_internal(), which waits for NET_EPOCH callbacks to
	 * complete. We can't do that from within NET_EPOCH.
	 *
	 * However, we can also use the IFNET_xLOCK, which is the V_ifnet
	 * read/write lock. We cannot hold the lock as we call if_vmove()
	 * though, as that presents LOR w.r.t ifnet_sx, in_multi_sx and iflib
	 * ctx lock.
	 */
	IFNET_WLOCK();

	pending = malloc(sizeof(struct ifnet *) * curvnet->vnet_ifcnt,
	    M_IFNET, M_WAITOK | M_ZERO);

	/* Return all inherited interfaces to their parent vnets. */
	CK_STAILQ_FOREACH_SAFE(ifp, &V_ifnet, if_link, nifp) {
		if (ifp->if_home_vnet != ifp->if_vnet) {
			found = if_unlink_ifnet(ifp, true);
			MPASS(found);

			pending[i++] = ifp;
		}
	}
	IFNET_WUNLOCK();

	for (int j = 0; j < i; j++) {
		sx_xlock(&ifnet_detach_sxlock);
		if_vmove(pending[j], pending[j]->if_home_vnet);
		sx_xunlock(&ifnet_detach_sxlock);
	}

	free(pending, M_IFNET);
}
VNET_SYSUNINIT(vnet_if_return, SI_SUB_VNET_DONE, SI_ORDER_ANY,
    vnet_if_return, NULL);
#endif

/*
 * Allocate a struct ifnet and an index for an interface.  A layer 2
 * common structure will also be allocated if an allocation routine is
 * registered for the passed type.
 */
static struct ifnet *
if_alloc_domain(u_char type, int numa_domain)
{
	struct ifnet *ifp;
	u_short idx;

	KASSERT(numa_domain <= IF_NODOM, ("numa_domain too large"));
	if (numa_domain == IF_NODOM)
		ifp = malloc(sizeof(struct ifnet), M_IFNET,
		    M_WAITOK | M_ZERO);
	else
		ifp = malloc_domainset(sizeof(struct ifnet), M_IFNET,
		    DOMAINSET_PREF(numa_domain), M_WAITOK | M_ZERO);
	ifp->if_type = type;
	ifp->if_alloctype = type;
	ifp->if_numa_domain = numa_domain;
#ifdef VIMAGE
	ifp->if_vnet = curvnet;
#endif
	if (if_com_alloc[type] != NULL) {
		ifp->if_l2com = if_com_alloc[type](type, ifp);
		KASSERT(ifp->if_l2com, ("%s: if_com_alloc[%u] failed", __func__,
		    type));
	}

	IF_ADDR_LOCK_INIT(ifp);
	TASK_INIT(&ifp->if_linktask, 0, do_link_state_change, ifp);
	TASK_INIT(&ifp->if_addmultitask, 0, if_siocaddmulti, ifp);
	ifp->if_afdata_initialized = 0;
	IF_AFDATA_LOCK_INIT(ifp);
	CK_STAILQ_INIT(&ifp->if_addrhead);
	CK_STAILQ_INIT(&ifp->if_multiaddrs);
	CK_STAILQ_INIT(&ifp->if_groups);
#ifdef MAC
	mac_ifnet_init(ifp);
#endif
	ifq_init(&ifp->if_snd, ifp);

	refcount_init(&ifp->if_refcount, 1);	/* Index reference. */
	for (int i = 0; i < IFCOUNTERS; i++)
		ifp->if_counters[i] = counter_u64_alloc(M_WAITOK);
	ifp->if_get_counter = if_get_counter_default;
	ifp->if_pcp = IFNET_PCP_NONE;

	/* Allocate an ifindex array entry. */
	IFNET_WLOCK();
	/*
	 * Try to find an empty slot below if_index.  If we fail, take the
	 * next slot.
	 */
	for (idx = 1; idx <= if_index; idx++) {
		if (ifindex_table[idx].ife_ifnet == NULL)
			break;
	}

	/* Catch if_index overflow. */
	if (idx >= if_indexlim) {
		struct ifindex_entry *new, *old;
		int newlim;

		newlim = if_indexlim * 2;
		new = malloc(newlim * sizeof(*new), M_IFNET, M_WAITOK | M_ZERO);
		memcpy(new, ifindex_table, if_indexlim * sizeof(*new));
		old = ifindex_table;
		ck_pr_store_ptr(&ifindex_table, new);
		if_indexlim = newlim;
		epoch_wait_preempt(net_epoch_preempt);
		free(old, M_IFNET);
	}
	if (idx > if_index)
		if_index = idx;

	ifp->if_index = idx;
	ifp->if_idxgen = ifindex_table[idx].ife_gencnt;
	ck_pr_store_ptr(&ifindex_table[idx].ife_ifnet, ifp);
	IFNET_WUNLOCK();

	return (ifp);
}

struct ifnet *
if_alloc_dev(u_char type, device_t dev)
{
	int numa_domain;

	if (dev == NULL || bus_get_domain(dev, &numa_domain) != 0)
		return (if_alloc_domain(type, IF_NODOM));
	return (if_alloc_domain(type, numa_domain));
}

struct ifnet *
if_alloc(u_char type)
{

	return (if_alloc_domain(type, IF_NODOM));
}
/*
 * Do the actual work of freeing a struct ifnet, and layer 2 common
 * structure.  This call is made when the network epoch guarantees
 * us that nobody holds a pointer to the interface.
 */
static void
if_free_deferred(epoch_context_t ctx)
{
	struct ifnet *ifp = __containerof(ctx, struct ifnet, if_epoch_ctx);

	KASSERT((ifp->if_flags & IFF_DYING),
	    ("%s: interface not dying", __func__));

	if (if_com_free[ifp->if_alloctype] != NULL)
		if_com_free[ifp->if_alloctype](ifp->if_l2com,
		    ifp->if_alloctype);

#ifdef MAC
	mac_ifnet_destroy(ifp);
#endif /* MAC */
	IF_AFDATA_DESTROY(ifp);
	IF_ADDR_LOCK_DESTROY(ifp);
	ifq_delete(&ifp->if_snd);

	for (int i = 0; i < IFCOUNTERS; i++)
		counter_u64_free(ifp->if_counters[i]);

	if_freedescr(ifp->if_description);
	free(ifp->if_hw_addr, M_IFADDR);
	free(ifp, M_IFNET);
}

/*
 * Deregister an interface and free the associated storage.
 */
void
if_free(struct ifnet *ifp)
{

	ifp->if_flags |= IFF_DYING;			/* XXX: Locking */

	/*
	 * XXXGL: An interface index is really an alias to ifp pointer.
	 * Why would we clear the alias now, and not in the deferred
	 * context?  Indeed there is nothing wrong with some network
	 * thread obtaining ifp via ifnet_byindex() inside the network
	 * epoch and then dereferencing ifp while we perform if_free(),
	 * and after if_free() finished, too.
	 *
	 * This early index freeing was important back when ifindex was
	 * virtualized and interface would outlive the vnet.
	 */
	IFNET_WLOCK();
	MPASS(ifindex_table[ifp->if_index].ife_ifnet == ifp);
	ck_pr_store_ptr(&ifindex_table[ifp->if_index].ife_ifnet, NULL);
	ifindex_table[ifp->if_index].ife_gencnt++;
	while (if_index > 0 && ifindex_table[if_index].ife_ifnet == NULL)
		if_index--;
	IFNET_WUNLOCK();

	if (refcount_release(&ifp->if_refcount))
		NET_EPOCH_CALL(if_free_deferred, &ifp->if_epoch_ctx);
}

/*
 * Interfaces to keep an ifnet type-stable despite the possibility of the
 * driver calling if_free().  If there are additional references, we defer
 * freeing the underlying data structure.
 */
void
if_ref(struct ifnet *ifp)
{
	u_int old __diagused;

	/* We don't assert the ifnet list lock here, but arguably should. */
	old = refcount_acquire(&ifp->if_refcount);
	KASSERT(old > 0, ("%s: ifp %p has 0 refs", __func__, ifp));
}

bool
if_try_ref(struct ifnet *ifp)
{
	NET_EPOCH_ASSERT();
	return (refcount_acquire_if_not_zero(&ifp->if_refcount));
}

void
if_rele(struct ifnet *ifp)
{

	if (!refcount_release(&ifp->if_refcount))
		return;
	NET_EPOCH_CALL(if_free_deferred, &ifp->if_epoch_ctx);
}

void
ifq_init(struct ifaltq *ifq, struct ifnet *ifp)
{

	mtx_init(&ifq->ifq_mtx, ifp->if_xname, "if send queue", MTX_DEF);

	if (ifq->ifq_maxlen == 0) 
		ifq->ifq_maxlen = ifqmaxlen;

	ifq->altq_type = 0;
	ifq->altq_disc = NULL;
	ifq->altq_flags &= ALTQF_CANTCHANGE;
	ifq->altq_tbr  = NULL;
	ifq->altq_ifp  = ifp;
}

void
ifq_delete(struct ifaltq *ifq)
{
	mtx_destroy(&ifq->ifq_mtx);
}

/*
 * Perform generic interface initialization tasks and attach the interface
 * to the list of "active" interfaces.  If vmove flag is set on entry
 * to if_attach_internal(), perform only a limited subset of initialization
 * tasks, given that we are moving from one vnet to another an ifnet which
 * has already been fully initialized.
 *
 * Note that if_detach_internal() removes group membership unconditionally
 * even when vmove flag is set, and if_attach_internal() adds only IFG_ALL.
 * Thus, when if_vmove() is applied to a cloned interface, group membership
 * is lost while a cloned one always joins a group whose name is
 * ifc->ifc_name.  To recover this after if_detach_internal() and
 * if_attach_internal(), the cloner should be specified to
 * if_attach_internal() via ifc.  If it is non-NULL, if_attach_internal()
 * attempts to join a group whose name is ifc->ifc_name.
 *
 * XXX:
 *  - The decision to return void and thus require this function to
 *    succeed is questionable.
 *  - We should probably do more sanity checking.  For instance we don't
 *    do anything to insure if_xname is unique or non-empty.
 */
void
if_attach(struct ifnet *ifp)
{

	if_attach_internal(ifp, false);
}

/*
 * Compute the least common TSO limit.
 */
void
if_hw_tsomax_common(if_t ifp, struct ifnet_hw_tsomax *pmax)
{
	/*
	 * 1) If there is no limit currently, take the limit from
	 * the network adapter.
	 *
	 * 2) If the network adapter has a limit below the current
	 * limit, apply it.
	 */
	if (pmax->tsomaxbytes == 0 || (ifp->if_hw_tsomax != 0 &&
	    ifp->if_hw_tsomax < pmax->tsomaxbytes)) {
		pmax->tsomaxbytes = ifp->if_hw_tsomax;
	}
	if (pmax->tsomaxsegcount == 0 || (ifp->if_hw_tsomaxsegcount != 0 &&
	    ifp->if_hw_tsomaxsegcount < pmax->tsomaxsegcount)) {
		pmax->tsomaxsegcount = ifp->if_hw_tsomaxsegcount;
	}
	if (pmax->tsomaxsegsize == 0 || (ifp->if_hw_tsomaxsegsize != 0 &&
	    ifp->if_hw_tsomaxsegsize < pmax->tsomaxsegsize)) {
		pmax->tsomaxsegsize = ifp->if_hw_tsomaxsegsize;
	}
}

/*
 * Update TSO limit of a network adapter.
 *
 * Returns zero if no change. Else non-zero.
 */
int
if_hw_tsomax_update(if_t ifp, struct ifnet_hw_tsomax *pmax)
{
	int retval = 0;
	if (ifp->if_hw_tsomax != pmax->tsomaxbytes) {
		ifp->if_hw_tsomax = pmax->tsomaxbytes;
		retval++;
	}
	if (ifp->if_hw_tsomaxsegsize != pmax->tsomaxsegsize) {
		ifp->if_hw_tsomaxsegsize = pmax->tsomaxsegsize;
		retval++;
	}
	if (ifp->if_hw_tsomaxsegcount != pmax->tsomaxsegcount) {
		ifp->if_hw_tsomaxsegcount = pmax->tsomaxsegcount;
		retval++;
	}
	return (retval);
}

static void
if_attach_internal(struct ifnet *ifp, bool vmove)
{
	unsigned socksize, ifasize;
	int namelen, masklen;
	struct sockaddr_dl *sdl;
	struct ifaddr *ifa;

	MPASS(ifindex_table[ifp->if_index].ife_ifnet == ifp);

#ifdef VIMAGE
	ifp->if_vnet = curvnet;
	if (ifp->if_home_vnet == NULL)
		ifp->if_home_vnet = curvnet;
#endif

	if_addgroup(ifp, IFG_ALL);

#ifdef VIMAGE
	/* Restore group membership for cloned interface. */
	if (vmove)
		if_clone_restoregroup(ifp);
#endif

	getmicrotime(&ifp->if_lastchange);
	ifp->if_epoch = time_uptime;

	KASSERT((ifp->if_transmit == NULL && ifp->if_qflush == NULL) ||
	    (ifp->if_transmit != NULL && ifp->if_qflush != NULL),
	    ("transmit and qflush must both either be set or both be NULL"));
	if (ifp->if_transmit == NULL) {
		ifp->if_transmit = if_transmit_default;
		ifp->if_qflush = if_qflush;
	}
	if (ifp->if_input == NULL)
		ifp->if_input = if_input_default;

	if (ifp->if_requestencap == NULL)
		ifp->if_requestencap = if_requestencap_default;

	if (!vmove) {
#ifdef MAC
		mac_ifnet_create(ifp);
#endif

		/*
		 * Create a Link Level name for this device.
		 */
		namelen = strlen(ifp->if_xname);
		/*
		 * Always save enough space for any possiable name so we
		 * can do a rename in place later.
		 */
		masklen = offsetof(struct sockaddr_dl, sdl_data[0]) + IFNAMSIZ;
		socksize = masklen + ifp->if_addrlen;
		if (socksize < sizeof(*sdl))
			socksize = sizeof(*sdl);
		socksize = roundup2(socksize, sizeof(long));
		ifasize = sizeof(*ifa) + 2 * socksize;
		ifa = ifa_alloc(ifasize, M_WAITOK);
		sdl = (struct sockaddr_dl *)(ifa + 1);
		sdl->sdl_len = socksize;
		sdl->sdl_family = AF_LINK;
		bcopy(ifp->if_xname, sdl->sdl_data, namelen);
		sdl->sdl_nlen = namelen;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = ifp->if_type;
		ifp->if_addr = ifa;
		ifa->ifa_ifp = ifp;
		ifa->ifa_addr = (struct sockaddr *)sdl;
		sdl = (struct sockaddr_dl *)(socksize + (caddr_t)sdl);
		ifa->ifa_netmask = (struct sockaddr *)sdl;
		sdl->sdl_len = masklen;
		while (namelen != 0)
			sdl->sdl_data[--namelen] = 0xff;
		CK_STAILQ_INSERT_HEAD(&ifp->if_addrhead, ifa, ifa_link);
		/* Reliably crash if used uninitialized. */
		ifp->if_broadcastaddr = NULL;

		if (ifp->if_type == IFT_ETHER) {
			ifp->if_hw_addr = malloc(ifp->if_addrlen, M_IFADDR,
			    M_WAITOK | M_ZERO);
		}

#if defined(INET) || defined(INET6)
		/* Use defaults for TSO, if nothing is set */
		if (ifp->if_hw_tsomax == 0 &&
		    ifp->if_hw_tsomaxsegcount == 0 &&
		    ifp->if_hw_tsomaxsegsize == 0) {
			/*
			 * The TSO defaults needs to be such that an
			 * NFS mbuf list of 35 mbufs totalling just
			 * below 64K works and that a chain of mbufs
			 * can be defragged into at most 32 segments:
			 */
			ifp->if_hw_tsomax = min(IP_MAXPACKET, (32 * MCLBYTES) -
			    (ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN));
			ifp->if_hw_tsomaxsegcount = 35;
			ifp->if_hw_tsomaxsegsize = 2048;	/* 2K */

			/* XXX some drivers set IFCAP_TSO after ethernet attach */
			if (ifp->if_capabilities & IFCAP_TSO) {
				if_printf(ifp, "Using defaults for TSO: %u/%u/%u\n",
				    ifp->if_hw_tsomax,
				    ifp->if_hw_tsomaxsegcount,
				    ifp->if_hw_tsomaxsegsize);
			}
		}
#endif
	}
#ifdef VIMAGE
	else {
		/*
		 * Update the interface index in the link layer address
		 * of the interface.
		 */
		for (ifa = ifp->if_addr; ifa != NULL;
		    ifa = CK_STAILQ_NEXT(ifa, ifa_link)) {
			if (ifa->ifa_addr->sa_family == AF_LINK) {
				sdl = (struct sockaddr_dl *)ifa->ifa_addr;
				sdl->sdl_index = ifp->if_index;
			}
		}
	}
#endif

	if_link_ifnet(ifp);

	if (domain_init_status >= 2)
		if_attachdomain1(ifp);

	EVENTHANDLER_INVOKE(ifnet_arrival_event, ifp);
	if (IS_DEFAULT_VNET(curvnet))
		devctl_notify("IFNET", ifp->if_xname, "ATTACH", NULL);
}

static void
if_epochalloc(void *dummy __unused)
{

	net_epoch_preempt = epoch_alloc("Net preemptible", EPOCH_PREEMPT);
}
SYSINIT(ifepochalloc, SI_SUB_EPOCH, SI_ORDER_ANY, if_epochalloc, NULL);

static void
if_attachdomain(void *dummy)
{
	struct ifnet *ifp;

	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link)
		if_attachdomain1(ifp);
}
SYSINIT(domainifattach, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_SECOND,
    if_attachdomain, NULL);

static void
if_attachdomain1(struct ifnet *ifp)
{
	struct domain *dp;

	/*
	 * Since dp->dom_ifattach calls malloc() with M_WAITOK, we
	 * cannot lock ifp->if_afdata initialization, entirely.
	 */
	IF_AFDATA_LOCK(ifp);
	if (ifp->if_afdata_initialized >= domain_init_status) {
		IF_AFDATA_UNLOCK(ifp);
		log(LOG_WARNING, "%s called more than once on %s\n",
		    __func__, ifp->if_xname);
		return;
	}
	ifp->if_afdata_initialized = domain_init_status;
	IF_AFDATA_UNLOCK(ifp);

	/* address family dependent data region */
	bzero(ifp->if_afdata, sizeof(ifp->if_afdata));
	SLIST_FOREACH(dp, &domains, dom_next) {
		if (dp->dom_ifattach)
			ifp->if_afdata[dp->dom_family] =
			    (*dp->dom_ifattach)(ifp);
	}
}

/*
 * Remove any unicast or broadcast network addresses from an interface.
 */
void
if_purgeaddrs(struct ifnet *ifp)
{
	struct ifaddr *ifa;

#ifdef INET6
	/*
	 * Need to leave multicast addresses of proxy NDP llentries
	 * before in6_purgeifaddr() because the llentries are keys
	 * for in6_multi objects of proxy NDP entries.
	 * in6_purgeifaddr()s clean up llentries including proxy NDPs
	 * then we would lose the keys if they are called earlier.
	 */
	in6_purge_proxy_ndp(ifp);
#endif
	while (1) {
		struct epoch_tracker et;

		NET_EPOCH_ENTER(et);
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != AF_LINK)
				break;
		}
		NET_EPOCH_EXIT(et);

		if (ifa == NULL)
			break;
#ifdef INET
		/* XXX: Ugly!! ad hoc just for INET */
		if (ifa->ifa_addr->sa_family == AF_INET) {
			struct ifaliasreq ifr;

			bzero(&ifr, sizeof(ifr));
			ifr.ifra_addr = *ifa->ifa_addr;
			if (ifa->ifa_dstaddr)
				ifr.ifra_broadaddr = *ifa->ifa_dstaddr;
			if (in_control(NULL, SIOCDIFADDR, (caddr_t)&ifr, ifp,
			    NULL) == 0)
				continue;
		}
#endif /* INET */
#ifdef INET6
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			in6_purgeifaddr((struct in6_ifaddr *)ifa);
			/* ifp_addrhead is already updated */
			continue;
		}
#endif /* INET6 */
		IF_ADDR_WLOCK(ifp);
		CK_STAILQ_REMOVE(&ifp->if_addrhead, ifa, ifaddr, ifa_link);
		IF_ADDR_WUNLOCK(ifp);
		ifa_free(ifa);
	}
}

/*
 * Remove any multicast network addresses from an interface when an ifnet
 * is going away.
 */
static void
if_purgemaddrs(struct ifnet *ifp)
{
	struct ifmultiaddr *ifma;

	IF_ADDR_WLOCK(ifp);
	while (!CK_STAILQ_EMPTY(&ifp->if_multiaddrs)) {
		ifma = CK_STAILQ_FIRST(&ifp->if_multiaddrs);
		CK_STAILQ_REMOVE(&ifp->if_multiaddrs, ifma, ifmultiaddr, ifma_link);
		if_delmulti_locked(ifp, ifma, 1);
	}
	IF_ADDR_WUNLOCK(ifp);
}

/*
 * Detach an interface, removing it from the list of "active" interfaces.
 * If vmove flag is set on entry to if_detach_internal(), perform only a
 * limited subset of cleanup tasks, given that we are moving an ifnet from
 * one vnet to another, where it must be fully operational.
 *
 * XXXRW: There are some significant questions about event ordering, and
 * how to prevent things from starting to use the interface during detach.
 */
void
if_detach(struct ifnet *ifp)
{
	bool found;

	CURVNET_SET_QUIET(ifp->if_vnet);
	found = if_unlink_ifnet(ifp, false);
	if (found) {
		sx_xlock(&ifnet_detach_sxlock);
		if_detach_internal(ifp, false);
		sx_xunlock(&ifnet_detach_sxlock);
	}
	CURVNET_RESTORE();
}

/*
 * The vmove flag, if set, indicates that we are called from a callpath
 * that is moving an interface to a different vnet instance.
 *
 * The shutdown flag, if set, indicates that we are called in the
 * process of shutting down a vnet instance.  Currently only the
 * vnet_if_return SYSUNINIT function sets it.  Note: we can be called
 * on a vnet instance shutdown without this flag being set, e.g., when
 * the cloned interfaces are destoyed as first thing of teardown.
 */
static int
if_detach_internal(struct ifnet *ifp, bool vmove)
{
	struct ifaddr *ifa;
	int i;
	struct domain *dp;
#ifdef VIMAGE
	bool shutdown;

	shutdown = VNET_IS_SHUTTING_DOWN(ifp->if_vnet);
#endif

	/*
	 * At this point we know the interface still was on the ifnet list
	 * and we removed it so we are in a stable state.
	 */
	epoch_wait_preempt(net_epoch_preempt);

	/*
	 * Ensure all pending EPOCH(9) callbacks have been executed. This
	 * fixes issues about late destruction of multicast options
	 * which lead to leave group calls, which in turn access the
	 * belonging ifnet structure:
	 */
	NET_EPOCH_DRAIN_CALLBACKS();

	/*
	 * In any case (destroy or vmove) detach us from the groups
	 * and remove/wait for pending events on the taskq.
	 * XXX-BZ in theory an interface could still enqueue a taskq change?
	 */
	if_delgroups(ifp);

	taskqueue_drain(taskqueue_swi, &ifp->if_linktask);
	taskqueue_drain(taskqueue_swi, &ifp->if_addmultitask);

	if_down(ifp);

#ifdef VIMAGE
	/*
	 * On VNET shutdown abort here as the stack teardown will do all
	 * the work top-down for us.
	 */
	if (shutdown) {
		/* Give interface users the chance to clean up. */
		EVENTHANDLER_INVOKE(ifnet_departure_event, ifp);

		/*
		 * In case of a vmove we are done here without error.
		 * If we would signal an error it would lead to the same
		 * abort as if we did not find the ifnet anymore.
		 * if_detach() calls us in void context and does not care
		 * about an early abort notification, so life is splendid :)
		 */
		goto finish_vnet_shutdown;
	}
#endif

	/*
	 * At this point we are not tearing down a VNET and are either
	 * going to destroy or vmove the interface and have to cleanup
	 * accordingly.
	 */

	/*
	 * Remove routes and flush queues.
	 */
#ifdef ALTQ
	if (ALTQ_IS_ENABLED(&ifp->if_snd))
		altq_disable(&ifp->if_snd);
	if (ALTQ_IS_ATTACHED(&ifp->if_snd))
		altq_detach(&ifp->if_snd);
#endif

	if_purgeaddrs(ifp);

#ifdef INET
	in_ifdetach(ifp);
#endif

#ifdef INET6
	/*
	 * Remove all IPv6 kernel structs related to ifp.  This should be done
	 * before removing routing entries below, since IPv6 interface direct
	 * routes are expected to be removed by the IPv6-specific kernel API.
	 * Otherwise, the kernel will detect some inconsistency and bark it.
	 */
	in6_ifdetach(ifp);
#endif
	if_purgemaddrs(ifp);

	EVENTHANDLER_INVOKE(ifnet_departure_event, ifp);
	if (IS_DEFAULT_VNET(curvnet))
		devctl_notify("IFNET", ifp->if_xname, "DETACH", NULL);

	if (!vmove) {
		/*
		 * Prevent further calls into the device driver via ifnet.
		 */
		if_dead(ifp);

		/*
		 * Clean up all addresses.
		 */
		IF_ADDR_WLOCK(ifp);
		if (!CK_STAILQ_EMPTY(&ifp->if_addrhead)) {
			ifa = CK_STAILQ_FIRST(&ifp->if_addrhead);
			CK_STAILQ_REMOVE(&ifp->if_addrhead, ifa, ifaddr, ifa_link);
			IF_ADDR_WUNLOCK(ifp);
			ifa_free(ifa);
		} else
			IF_ADDR_WUNLOCK(ifp);
	}

	rt_flushifroutes(ifp);

#ifdef VIMAGE
finish_vnet_shutdown:
#endif
	/*
	 * We cannot hold the lock over dom_ifdetach calls as they might
	 * sleep, for example trying to drain a callout, thus open up the
	 * theoretical race with re-attaching.
	 */
	IF_AFDATA_LOCK(ifp);
	i = ifp->if_afdata_initialized;
	ifp->if_afdata_initialized = 0;
	IF_AFDATA_UNLOCK(ifp);
	if (i == 0)
		return (0);
	SLIST_FOREACH(dp, &domains, dom_next) {
		if (dp->dom_ifdetach && ifp->if_afdata[dp->dom_family]) {
			(*dp->dom_ifdetach)(ifp,
			    ifp->if_afdata[dp->dom_family]);
			ifp->if_afdata[dp->dom_family] = NULL;
		}
	}

	return (0);
}

#ifdef VIMAGE
/*
 * if_vmove() performs a limited version of if_detach() in current
 * vnet and if_attach()es the ifnet to the vnet specified as 2nd arg.
 */
static int
if_vmove(struct ifnet *ifp, struct vnet *new_vnet)
{
#ifdef DEV_BPF
	u_int bif_dlt, bif_hdrlen;
#endif
	int rc;

#ifdef DEV_BPF
 	/*
	 * if_detach_internal() will call the eventhandler to notify
	 * interface departure.  That will detach if_bpf.  We need to
	 * safe the dlt and hdrlen so we can re-attach it later.
	 */
	bpf_get_bp_params(ifp->if_bpf, &bif_dlt, &bif_hdrlen);
#endif

	/*
	 * Detach from current vnet, but preserve LLADDR info, do not
	 * mark as dead etc. so that the ifnet can be reattached later.
	 * If we cannot find it, we lost the race to someone else.
	 */
	rc = if_detach_internal(ifp, true);
	if (rc != 0)
		return (rc);

	/*
	 * Perform interface-specific reassignment tasks, if provided by
	 * the driver.
	 */
	if (ifp->if_reassign != NULL)
		ifp->if_reassign(ifp, new_vnet, NULL);

	/*
	 * Switch to the context of the target vnet.
	 */
	CURVNET_SET_QUIET(new_vnet);
	if_attach_internal(ifp, true);

#ifdef DEV_BPF
	if (ifp->if_bpf == NULL)
		bpfattach(ifp, bif_dlt, bif_hdrlen);
#endif

	CURVNET_RESTORE();
	return (0);
}

/*
 * Move an ifnet to or from another child prison/vnet, specified by the jail id.
 */
static int
if_vmove_loan(struct thread *td, struct ifnet *ifp, char *ifname, int jid)
{
	struct prison *pr;
	struct ifnet *difp;
	int error;
	bool found __diagused;
	bool shutdown;

	MPASS(ifindex_table[ifp->if_index].ife_ifnet == ifp);

	/* Try to find the prison within our visibility. */
	sx_slock(&allprison_lock);
	pr = prison_find_child(td->td_ucred->cr_prison, jid);
	sx_sunlock(&allprison_lock);
	if (pr == NULL)
		return (ENXIO);
	prison_hold_locked(pr);
	mtx_unlock(&pr->pr_mtx);

	/* Do not try to move the iface from and to the same prison. */
	if (pr->pr_vnet == ifp->if_vnet) {
		prison_free(pr);
		return (EEXIST);
	}

	/* Make sure the named iface does not exists in the dst. prison/vnet. */
	/* XXX Lock interfaces to avoid races. */
	CURVNET_SET_QUIET(pr->pr_vnet);
	difp = ifunit(ifname);
	if (difp != NULL) {
		CURVNET_RESTORE();
		prison_free(pr);
		return (EEXIST);
	}
	sx_xlock(&ifnet_detach_sxlock);

	/* Make sure the VNET is stable. */
	shutdown = VNET_IS_SHUTTING_DOWN(ifp->if_vnet);
	if (shutdown) {
		sx_xunlock(&ifnet_detach_sxlock);
		CURVNET_RESTORE();
		prison_free(pr);
		return (EBUSY);
	}
	CURVNET_RESTORE();

	found = if_unlink_ifnet(ifp, true);
	if (! found) {
		sx_xunlock(&ifnet_detach_sxlock);
		CURVNET_RESTORE();
		prison_free(pr);
		return (ENODEV);
	}

	/* Move the interface into the child jail/vnet. */
	error = if_vmove(ifp, pr->pr_vnet);

	/* Report the new if_xname back to the userland on success. */
	if (error == 0)
		sprintf(ifname, "%s", ifp->if_xname);

	sx_xunlock(&ifnet_detach_sxlock);

	prison_free(pr);
	return (error);
}

static int
if_vmove_reclaim(struct thread *td, char *ifname, int jid)
{
	struct prison *pr;
	struct vnet *vnet_dst;
	struct ifnet *ifp;
	int error, found __diagused;
 	bool shutdown;

	/* Try to find the prison within our visibility. */
	sx_slock(&allprison_lock);
	pr = prison_find_child(td->td_ucred->cr_prison, jid);
	sx_sunlock(&allprison_lock);
	if (pr == NULL)
		return (ENXIO);
	prison_hold_locked(pr);
	mtx_unlock(&pr->pr_mtx);

	/* Make sure the named iface exists in the source prison/vnet. */
	CURVNET_SET(pr->pr_vnet);
	ifp = ifunit(ifname);		/* XXX Lock to avoid races. */
	if (ifp == NULL) {
		CURVNET_RESTORE();
		prison_free(pr);
		return (ENXIO);
	}

	/* Do not try to move the iface from and to the same prison. */
	vnet_dst = TD_TO_VNET(td);
	if (vnet_dst == ifp->if_vnet) {
		CURVNET_RESTORE();
		prison_free(pr);
		return (EEXIST);
	}

	/* Make sure the VNET is stable. */
	shutdown = VNET_IS_SHUTTING_DOWN(ifp->if_vnet);
	if (shutdown) {
		CURVNET_RESTORE();
		prison_free(pr);
		return (EBUSY);
	}

	/* Get interface back from child jail/vnet. */
	found = if_unlink_ifnet(ifp, true);
	MPASS(found);
	sx_xlock(&ifnet_detach_sxlock);
	error = if_vmove(ifp, vnet_dst);
	sx_xunlock(&ifnet_detach_sxlock);
	CURVNET_RESTORE();

	/* Report the new if_xname back to the userland on success. */
	if (error == 0)
		sprintf(ifname, "%s", ifp->if_xname);

	prison_free(pr);
	return (error);
}
#endif /* VIMAGE */

/*
 * Add a group to an interface
 */
int
if_addgroup(struct ifnet *ifp, const char *groupname)
{
	struct ifg_list		*ifgl;
	struct ifg_group	*ifg = NULL;
	struct ifg_member	*ifgm;
	int 			 new = 0;

	if (groupname[0] && groupname[strlen(groupname) - 1] >= '0' &&
	    groupname[strlen(groupname) - 1] <= '9')
		return (EINVAL);

	IFNET_WLOCK();
	CK_STAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next)
		if (!strcmp(ifgl->ifgl_group->ifg_group, groupname)) {
			IFNET_WUNLOCK();
			return (EEXIST);
		}

	if ((ifgl = malloc(sizeof(*ifgl), M_TEMP, M_NOWAIT)) == NULL) {
	    	IFNET_WUNLOCK();
		return (ENOMEM);
	}

	if ((ifgm = malloc(sizeof(*ifgm), M_TEMP, M_NOWAIT)) == NULL) {
		free(ifgl, M_TEMP);
		IFNET_WUNLOCK();
		return (ENOMEM);
	}

	CK_STAILQ_FOREACH(ifg, &V_ifg_head, ifg_next)
		if (!strcmp(ifg->ifg_group, groupname))
			break;

	if (ifg == NULL) {
		if ((ifg = malloc(sizeof(*ifg), M_TEMP, M_NOWAIT)) == NULL) {
			free(ifgl, M_TEMP);
			free(ifgm, M_TEMP);
			IFNET_WUNLOCK();
			return (ENOMEM);
		}
		strlcpy(ifg->ifg_group, groupname, sizeof(ifg->ifg_group));
		ifg->ifg_refcnt = 0;
		CK_STAILQ_INIT(&ifg->ifg_members);
		CK_STAILQ_INSERT_TAIL(&V_ifg_head, ifg, ifg_next);
		new = 1;
	}

	ifg->ifg_refcnt++;
	ifgl->ifgl_group = ifg;
	ifgm->ifgm_ifp = ifp;

	IF_ADDR_WLOCK(ifp);
	CK_STAILQ_INSERT_TAIL(&ifg->ifg_members, ifgm, ifgm_next);
	CK_STAILQ_INSERT_TAIL(&ifp->if_groups, ifgl, ifgl_next);
	IF_ADDR_WUNLOCK(ifp);

	IFNET_WUNLOCK();

	if (new)
		EVENTHANDLER_INVOKE(group_attach_event, ifg);
	EVENTHANDLER_INVOKE(group_change_event, groupname);

	return (0);
}

/*
 * Helper function to remove a group out of an interface.  Expects the global
 * ifnet lock to be write-locked, and drops it before returning.
 */
static void
_if_delgroup_locked(struct ifnet *ifp, struct ifg_list *ifgl,
    const char *groupname)
{
	struct ifg_member *ifgm;
	bool freeifgl;

	IFNET_WLOCK_ASSERT();

	IF_ADDR_WLOCK(ifp);
	CK_STAILQ_REMOVE(&ifp->if_groups, ifgl, ifg_list, ifgl_next);
	IF_ADDR_WUNLOCK(ifp);

	CK_STAILQ_FOREACH(ifgm, &ifgl->ifgl_group->ifg_members, ifgm_next) {
		if (ifgm->ifgm_ifp == ifp) {
			CK_STAILQ_REMOVE(&ifgl->ifgl_group->ifg_members, ifgm,
			    ifg_member, ifgm_next);
			break;
		}
	}

	if (--ifgl->ifgl_group->ifg_refcnt == 0) {
		CK_STAILQ_REMOVE(&V_ifg_head, ifgl->ifgl_group, ifg_group,
		    ifg_next);
		freeifgl = true;
	} else {
		freeifgl = false;
	}
	IFNET_WUNLOCK();

	epoch_wait_preempt(net_epoch_preempt);
	EVENTHANDLER_INVOKE(group_change_event, groupname);
	if (freeifgl) {
		EVENTHANDLER_INVOKE(group_detach_event, ifgl->ifgl_group);
		free(ifgl->ifgl_group, M_TEMP);
	}
	free(ifgm, M_TEMP);
	free(ifgl, M_TEMP);
}

/*
 * Remove a group from an interface
 */
int
if_delgroup(struct ifnet *ifp, const char *groupname)
{
	struct ifg_list *ifgl;

	IFNET_WLOCK();
	CK_STAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next)
		if (strcmp(ifgl->ifgl_group->ifg_group, groupname) == 0)
			break;
	if (ifgl == NULL) {
		IFNET_WUNLOCK();
		return (ENOENT);
	}

	_if_delgroup_locked(ifp, ifgl, groupname);

	return (0);
}

/*
 * Remove an interface from all groups
 */
static void
if_delgroups(struct ifnet *ifp)
{
	struct ifg_list *ifgl;
	char groupname[IFNAMSIZ];

	IFNET_WLOCK();
	while ((ifgl = CK_STAILQ_FIRST(&ifp->if_groups)) != NULL) {
		strlcpy(groupname, ifgl->ifgl_group->ifg_group, IFNAMSIZ);
		_if_delgroup_locked(ifp, ifgl, groupname);
		IFNET_WLOCK();
	}
	IFNET_WUNLOCK();
}

/*
 * Stores all groups from an interface in memory pointed to by ifgr.
 */
static int
if_getgroup(struct ifgroupreq *ifgr, struct ifnet *ifp)
{
	int			 len, error;
	struct ifg_list		*ifgl;
	struct ifg_req		 ifgrq, *ifgp;

	NET_EPOCH_ASSERT();

	if (ifgr->ifgr_len == 0) {
		CK_STAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next)
			ifgr->ifgr_len += sizeof(struct ifg_req);
		return (0);
	}

	len = ifgr->ifgr_len;
	ifgp = ifgr->ifgr_groups;
	/* XXX: wire */
	CK_STAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next) {
		if (len < sizeof(ifgrq))
			return (EINVAL);
		bzero(&ifgrq, sizeof ifgrq);
		strlcpy(ifgrq.ifgrq_group, ifgl->ifgl_group->ifg_group,
		    sizeof(ifgrq.ifgrq_group));
		if ((error = copyout(&ifgrq, ifgp, sizeof(struct ifg_req))))
			return (error);
		len -= sizeof(ifgrq);
		ifgp++;
	}

	return (0);
}

/*
 * Stores all members of a group in memory pointed to by igfr
 */
static int
if_getgroupmembers(struct ifgroupreq *ifgr)
{
	struct ifg_group	*ifg;
	struct ifg_member	*ifgm;
	struct ifg_req		 ifgrq, *ifgp;
	int			 len, error;

	IFNET_RLOCK();
	CK_STAILQ_FOREACH(ifg, &V_ifg_head, ifg_next)
		if (strcmp(ifg->ifg_group, ifgr->ifgr_name) == 0)
			break;
	if (ifg == NULL) {
		IFNET_RUNLOCK();
		return (ENOENT);
	}

	if (ifgr->ifgr_len == 0) {
		CK_STAILQ_FOREACH(ifgm, &ifg->ifg_members, ifgm_next)
			ifgr->ifgr_len += sizeof(ifgrq);
		IFNET_RUNLOCK();
		return (0);
	}

	len = ifgr->ifgr_len;
	ifgp = ifgr->ifgr_groups;
	CK_STAILQ_FOREACH(ifgm, &ifg->ifg_members, ifgm_next) {
		if (len < sizeof(ifgrq)) {
			IFNET_RUNLOCK();
			return (EINVAL);
		}
		bzero(&ifgrq, sizeof ifgrq);
		strlcpy(ifgrq.ifgrq_member, ifgm->ifgm_ifp->if_xname,
		    sizeof(ifgrq.ifgrq_member));
		if ((error = copyout(&ifgrq, ifgp, sizeof(struct ifg_req)))) {
			IFNET_RUNLOCK();
			return (error);
		}
		len -= sizeof(ifgrq);
		ifgp++;
	}
	IFNET_RUNLOCK();

	return (0);
}

/*
 * Return counter values from counter(9)s stored in ifnet.
 */
uint64_t
if_get_counter_default(struct ifnet *ifp, ift_counter cnt)
{

	KASSERT(cnt < IFCOUNTERS, ("%s: invalid cnt %d", __func__, cnt));

	return (counter_u64_fetch(ifp->if_counters[cnt]));
}

/*
 * Increase an ifnet counter. Usually used for counters shared
 * between the stack and a driver, but function supports them all.
 */
void
if_inc_counter(struct ifnet *ifp, ift_counter cnt, int64_t inc)
{

	KASSERT(cnt < IFCOUNTERS, ("%s: invalid cnt %d", __func__, cnt));

	counter_u64_add(ifp->if_counters[cnt], inc);
}

/*
 * Copy data from ifnet to userland API structure if_data.
 */
void
if_data_copy(struct ifnet *ifp, struct if_data *ifd)
{

	ifd->ifi_type = ifp->if_type;
	ifd->ifi_physical = 0;
	ifd->ifi_addrlen = ifp->if_addrlen;
	ifd->ifi_hdrlen = ifp->if_hdrlen;
	ifd->ifi_link_state = ifp->if_link_state;
	ifd->ifi_vhid = 0;
	ifd->ifi_datalen = sizeof(struct if_data);
	ifd->ifi_mtu = ifp->if_mtu;
	ifd->ifi_metric = ifp->if_metric;
	ifd->ifi_baudrate = ifp->if_baudrate;
	ifd->ifi_hwassist = ifp->if_hwassist;
	ifd->ifi_epoch = ifp->if_epoch;
	ifd->ifi_lastchange = ifp->if_lastchange;

	ifd->ifi_ipackets = ifp->if_get_counter(ifp, IFCOUNTER_IPACKETS);
	ifd->ifi_ierrors = ifp->if_get_counter(ifp, IFCOUNTER_IERRORS);
	ifd->ifi_opackets = ifp->if_get_counter(ifp, IFCOUNTER_OPACKETS);
	ifd->ifi_oerrors = ifp->if_get_counter(ifp, IFCOUNTER_OERRORS);
	ifd->ifi_collisions = ifp->if_get_counter(ifp, IFCOUNTER_COLLISIONS);
	ifd->ifi_ibytes = ifp->if_get_counter(ifp, IFCOUNTER_IBYTES);
	ifd->ifi_obytes = ifp->if_get_counter(ifp, IFCOUNTER_OBYTES);
	ifd->ifi_imcasts = ifp->if_get_counter(ifp, IFCOUNTER_IMCASTS);
	ifd->ifi_omcasts = ifp->if_get_counter(ifp, IFCOUNTER_OMCASTS);
	ifd->ifi_iqdrops = ifp->if_get_counter(ifp, IFCOUNTER_IQDROPS);
	ifd->ifi_oqdrops = ifp->if_get_counter(ifp, IFCOUNTER_OQDROPS);
	ifd->ifi_noproto = ifp->if_get_counter(ifp, IFCOUNTER_NOPROTO);
}

/*
 * Initialization, destruction and refcounting functions for ifaddrs.
 */
struct ifaddr *
ifa_alloc(size_t size, int flags)
{
	struct ifaddr *ifa;

	KASSERT(size >= sizeof(struct ifaddr),
	    ("%s: invalid size %zu", __func__, size));

	ifa = malloc(size, M_IFADDR, M_ZERO | flags);
	if (ifa == NULL)
		return (NULL);

	if ((ifa->ifa_opackets = counter_u64_alloc(flags)) == NULL)
		goto fail;
	if ((ifa->ifa_ipackets = counter_u64_alloc(flags)) == NULL)
		goto fail;
	if ((ifa->ifa_obytes = counter_u64_alloc(flags)) == NULL)
		goto fail;
	if ((ifa->ifa_ibytes = counter_u64_alloc(flags)) == NULL)
		goto fail;

	refcount_init(&ifa->ifa_refcnt, 1);

	return (ifa);

fail:
	/* free(NULL) is okay */
	counter_u64_free(ifa->ifa_opackets);
	counter_u64_free(ifa->ifa_ipackets);
	counter_u64_free(ifa->ifa_obytes);
	counter_u64_free(ifa->ifa_ibytes);
	free(ifa, M_IFADDR);

	return (NULL);
}

void
ifa_ref(struct ifaddr *ifa)
{
	u_int old __diagused;

	old = refcount_acquire(&ifa->ifa_refcnt);
	KASSERT(old > 0, ("%s: ifa %p has 0 refs", __func__, ifa));
}

int
ifa_try_ref(struct ifaddr *ifa)
{

	NET_EPOCH_ASSERT();
	return (refcount_acquire_if_not_zero(&ifa->ifa_refcnt));
}

static void
ifa_destroy(epoch_context_t ctx)
{
	struct ifaddr *ifa;

	ifa = __containerof(ctx, struct ifaddr, ifa_epoch_ctx);
	counter_u64_free(ifa->ifa_opackets);
	counter_u64_free(ifa->ifa_ipackets);
	counter_u64_free(ifa->ifa_obytes);
	counter_u64_free(ifa->ifa_ibytes);
	free(ifa, M_IFADDR);
}

void
ifa_free(struct ifaddr *ifa)
{

	if (refcount_release(&ifa->ifa_refcnt))
		NET_EPOCH_CALL(ifa_destroy, &ifa->ifa_epoch_ctx);
}

/*
 * XXX: Because sockaddr_dl has deeper structure than the sockaddr
 * structs used to represent other address families, it is necessary
 * to perform a different comparison.
 */

#define	sa_dl_equal(a1, a2)	\
	((((const struct sockaddr_dl *)(a1))->sdl_len ==		\
	 ((const struct sockaddr_dl *)(a2))->sdl_len) &&		\
	 (bcmp(CLLADDR((const struct sockaddr_dl *)(a1)),		\
	       CLLADDR((const struct sockaddr_dl *)(a2)),		\
	       ((const struct sockaddr_dl *)(a1))->sdl_alen) == 0))

/*
 * Locate an interface based on a complete address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithaddr(const struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	NET_EPOCH_ASSERT();

	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != addr->sa_family)
				continue;
			if (sa_equal(addr, ifa->ifa_addr)) {
				goto done;
			}
			/* IP6 doesn't have broadcast */
			if ((ifp->if_flags & IFF_BROADCAST) &&
			    ifa->ifa_broadaddr &&
			    ifa->ifa_broadaddr->sa_len != 0 &&
			    sa_equal(ifa->ifa_broadaddr, addr)) {
				goto done;
			}
		}
	}
	ifa = NULL;
done:
	return (ifa);
}

int
ifa_ifwithaddr_check(const struct sockaddr *addr)
{
	struct epoch_tracker et;
	int rc;

	NET_EPOCH_ENTER(et);
	rc = (ifa_ifwithaddr(addr) != NULL);
	NET_EPOCH_EXIT(et);
	return (rc);
}

/*
 * Locate an interface based on the broadcast address.
 */
/* ARGSUSED */
struct ifaddr *
ifa_ifwithbroadaddr(const struct sockaddr *addr, int fibnum)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	NET_EPOCH_ASSERT();
	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if ((fibnum != RT_ALL_FIBS) && (ifp->if_fib != fibnum))
			continue;
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != addr->sa_family)
				continue;
			if ((ifp->if_flags & IFF_BROADCAST) &&
			    ifa->ifa_broadaddr &&
			    ifa->ifa_broadaddr->sa_len != 0 &&
			    sa_equal(ifa->ifa_broadaddr, addr)) {
				goto done;
			}
		}
	}
	ifa = NULL;
done:
	return (ifa);
}

/*
 * Locate the point to point interface with a given destination address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithdstaddr(const struct sockaddr *addr, int fibnum)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	NET_EPOCH_ASSERT();
	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			continue;
		if ((fibnum != RT_ALL_FIBS) && (ifp->if_fib != fibnum))
			continue;
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != addr->sa_family)
				continue;
			if (ifa->ifa_dstaddr != NULL &&
			    sa_equal(addr, ifa->ifa_dstaddr)) {
				goto done;
			}
		}
	}
	ifa = NULL;
done:
	return (ifa);
}

/*
 * Find an interface on a specific network.  If many, choice
 * is most specific found.
 */
struct ifaddr *
ifa_ifwithnet(const struct sockaddr *addr, int ignore_ptp, int fibnum)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct ifaddr *ifa_maybe = NULL;
	u_int af = addr->sa_family;
	const char *addr_data = addr->sa_data, *cplim;

	NET_EPOCH_ASSERT();
	/*
	 * AF_LINK addresses can be looked up directly by their index number,
	 * so do that if we can.
	 */
	if (af == AF_LINK) {
		ifp = ifnet_byindex(
		    ((const struct sockaddr_dl *)addr)->sdl_index);
		return (ifp ? ifp->if_addr : NULL);
	}

	/*
	 * Scan though each interface, looking for ones that have addresses
	 * in this address family and the requested fib.
	 */
	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if ((fibnum != RT_ALL_FIBS) && (ifp->if_fib != fibnum))
			continue;
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			const char *cp, *cp2, *cp3;

			if (ifa->ifa_addr->sa_family != af)
next:				continue;
			if (af == AF_INET && 
			    ifp->if_flags & IFF_POINTOPOINT && !ignore_ptp) {
				/*
				 * This is a bit broken as it doesn't
				 * take into account that the remote end may
				 * be a single node in the network we are
				 * looking for.
				 * The trouble is that we don't know the
				 * netmask for the remote end.
				 */
				if (ifa->ifa_dstaddr != NULL &&
				    sa_equal(addr, ifa->ifa_dstaddr)) {
					goto done;
				}
			} else {
				/*
				 * Scan all the bits in the ifa's address.
				 * If a bit dissagrees with what we are
				 * looking for, mask it with the netmask
				 * to see if it really matters.
				 * (A byte at a time)
				 */
				if (ifa->ifa_netmask == 0)
					continue;
				cp = addr_data;
				cp2 = ifa->ifa_addr->sa_data;
				cp3 = ifa->ifa_netmask->sa_data;
				cplim = ifa->ifa_netmask->sa_len
					+ (char *)ifa->ifa_netmask;
				while (cp3 < cplim)
					if ((*cp++ ^ *cp2++) & *cp3++)
						goto next; /* next address! */
				/*
				 * If the netmask of what we just found
				 * is more specific than what we had before
				 * (if we had one), or if the virtual status
				 * of new prefix is better than of the old one,
				 * then remember the new one before continuing
				 * to search for an even better one.
				 */
				if (ifa_maybe == NULL ||
				    ifa_preferred(ifa_maybe, ifa) ||
				    rn_refines((caddr_t)ifa->ifa_netmask,
				    (caddr_t)ifa_maybe->ifa_netmask)) {
					ifa_maybe = ifa;
				}
			}
		}
	}
	ifa = ifa_maybe;
	ifa_maybe = NULL;
done:
	return (ifa);
}

/*
 * Find an interface address specific to an interface best matching
 * a given address.
 */
struct ifaddr *
ifaof_ifpforaddr(const struct sockaddr *addr, struct ifnet *ifp)
{
	struct ifaddr *ifa;
	const char *cp, *cp2, *cp3;
	char *cplim;
	struct ifaddr *ifa_maybe = NULL;
	u_int af = addr->sa_family;

	if (af >= AF_MAX)
		return (NULL);

	NET_EPOCH_ASSERT();
	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr->sa_family != af)
			continue;
		if (ifa_maybe == NULL)
			ifa_maybe = ifa;
		if (ifa->ifa_netmask == 0) {
			if (sa_equal(addr, ifa->ifa_addr) ||
			    (ifa->ifa_dstaddr &&
			    sa_equal(addr, ifa->ifa_dstaddr)))
				goto done;
			continue;
		}
		if (ifp->if_flags & IFF_POINTOPOINT) {
			if (ifa->ifa_dstaddr && sa_equal(addr, ifa->ifa_dstaddr))
				goto done;
		} else {
			cp = addr->sa_data;
			cp2 = ifa->ifa_addr->sa_data;
			cp3 = ifa->ifa_netmask->sa_data;
			cplim = ifa->ifa_netmask->sa_len + (char *)ifa->ifa_netmask;
			for (; cp3 < cplim; cp3++)
				if ((*cp++ ^ *cp2++) & *cp3)
					break;
			if (cp3 == cplim)
				goto done;
		}
	}
	ifa = ifa_maybe;
done:
	return (ifa);
}

/*
 * See whether new ifa is better than current one:
 * 1) A non-virtual one is preferred over virtual.
 * 2) A virtual in master state preferred over any other state.
 *
 * Used in several address selecting functions.
 */
int
ifa_preferred(struct ifaddr *cur, struct ifaddr *next)
{

	return (cur->ifa_carp && (!next->ifa_carp ||
	    ((*carp_master_p)(next) && !(*carp_master_p)(cur))));
}

struct sockaddr_dl *
link_alloc_sdl(size_t size, int flags)
{

	return (malloc(size, M_TEMP, flags));
}

void
link_free_sdl(struct sockaddr *sa)
{
	free(sa, M_TEMP);
}

/*
 * Fills in given sdl with interface basic info.
 * Returns pointer to filled sdl.
 */
struct sockaddr_dl *
link_init_sdl(struct ifnet *ifp, struct sockaddr *paddr, u_char iftype)
{
	struct sockaddr_dl *sdl;

	sdl = (struct sockaddr_dl *)paddr;
	memset(sdl, 0, sizeof(struct sockaddr_dl));
	sdl->sdl_len = sizeof(struct sockaddr_dl);
	sdl->sdl_family = AF_LINK;
	sdl->sdl_index = ifp->if_index;
	sdl->sdl_type = iftype;

	return (sdl);
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 */
static void
if_unroute(struct ifnet *ifp, int flag, int fam)
{

	KASSERT(flag == IFF_UP, ("if_unroute: flag != IFF_UP"));

	ifp->if_flags &= ~flag;
	getmicrotime(&ifp->if_lastchange);
	ifp->if_qflush(ifp);

	if (ifp->if_carp)
		(*carp_linkstate_p)(ifp);
	rt_ifmsg(ifp, IFF_UP);
}

void	(*vlan_link_state_p)(struct ifnet *);	/* XXX: private from if_vlan */
void	(*vlan_trunk_cap_p)(struct ifnet *);		/* XXX: private from if_vlan */
struct ifnet *(*vlan_trunkdev_p)(struct ifnet *);
struct	ifnet *(*vlan_devat_p)(struct ifnet *, uint16_t);
int	(*vlan_tag_p)(struct ifnet *, uint16_t *);
int	(*vlan_pcp_p)(struct ifnet *, uint16_t *);
int	(*vlan_setcookie_p)(struct ifnet *, void *);
void	*(*vlan_cookie_p)(struct ifnet *);

/*
 * Handle a change in the interface link state. To avoid LORs
 * between driver lock and upper layer locks, as well as possible
 * recursions, we post event to taskqueue, and all job
 * is done in static do_link_state_change().
 */
void
if_link_state_change(struct ifnet *ifp, int link_state)
{
	/* Return if state hasn't changed. */
	if (ifp->if_link_state == link_state)
		return;

	ifp->if_link_state = link_state;

	/* XXXGL: reference ifp? */
	taskqueue_enqueue(taskqueue_swi, &ifp->if_linktask);
}

static void
do_link_state_change(void *arg, int pending)
{
	struct ifnet *ifp;
	int link_state;

	ifp = arg;
	link_state = ifp->if_link_state;

	CURVNET_SET(ifp->if_vnet);
	rt_ifmsg(ifp, 0);
	if (ifp->if_vlantrunk != NULL)
		(*vlan_link_state_p)(ifp);

	if ((ifp->if_type == IFT_ETHER || ifp->if_type == IFT_L2VLAN) &&
	    ifp->if_l2com != NULL)
		(*ng_ether_link_state_p)(ifp, link_state);
	if (ifp->if_carp)
		(*carp_linkstate_p)(ifp);
	if (ifp->if_bridge)
		ifp->if_bridge_linkstate(ifp);
	if (ifp->if_lagg)
		(*lagg_linkstate_p)(ifp, link_state);

	if (IS_DEFAULT_VNET(curvnet))
		devctl_notify("IFNET", ifp->if_xname,
		    (link_state == LINK_STATE_UP) ? "LINK_UP" : "LINK_DOWN",
		    NULL);
	if (pending > 1)
		if_printf(ifp, "%d link states coalesced\n", pending);
	if (log_link_state_change)
		if_printf(ifp, "link state changed to %s\n",
		    (link_state == LINK_STATE_UP) ? "UP" : "DOWN" );
	EVENTHANDLER_INVOKE(ifnet_link_event, ifp, link_state);
	CURVNET_RESTORE();
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 */
void
if_down(struct ifnet *ifp)
{

	EVENTHANDLER_INVOKE(ifnet_event, ifp, IFNET_EVENT_DOWN);
	if_unroute(ifp, IFF_UP, AF_UNSPEC);
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 */
void
if_up(struct ifnet *ifp)
{

	ifp->if_flags |= IFF_UP;
	getmicrotime(&ifp->if_lastchange);
	if (ifp->if_carp)
		(*carp_linkstate_p)(ifp);
	rt_ifmsg(ifp, IFF_UP);
	EVENTHANDLER_INVOKE(ifnet_event, ifp, IFNET_EVENT_UP);
}

/*
 * Flush an interface queue.
 */
void
if_qflush(struct ifnet *ifp)
{
	struct mbuf *m, *n;
	struct ifaltq *ifq;

	ifq = &ifp->if_snd;
	IFQ_LOCK(ifq);
#ifdef ALTQ
	if (ALTQ_IS_ENABLED(ifq))
		ALTQ_PURGE(ifq);
#endif
	n = ifq->ifq_head;
	while ((m = n) != NULL) {
		n = m->m_nextpkt;
		m_freem(m);
	}
	ifq->ifq_head = 0;
	ifq->ifq_tail = 0;
	ifq->ifq_len = 0;
	IFQ_UNLOCK(ifq);
}

/*
 * Map interface name to interface structure pointer, with or without
 * returning a reference.
 */
struct ifnet *
ifunit_ref(const char *name)
{
	struct epoch_tracker et;
	struct ifnet *ifp;

	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if (strncmp(name, ifp->if_xname, IFNAMSIZ) == 0 &&
		    !(ifp->if_flags & IFF_DYING))
			break;
	}
	if (ifp != NULL) {
		if_ref(ifp);
		MPASS(ifindex_table[ifp->if_index].ife_ifnet == ifp);
	}

	NET_EPOCH_EXIT(et);
	return (ifp);
}

struct ifnet *
ifunit(const char *name)
{
	struct epoch_tracker et;
	struct ifnet *ifp;

	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if (strncmp(name, ifp->if_xname, IFNAMSIZ) == 0)
			break;
	}
	NET_EPOCH_EXIT(et);
	return (ifp);
}

void *
ifr_buffer_get_buffer(void *data)
{
	union ifreq_union *ifrup;

	ifrup = data;
#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32))
		return ((void *)(uintptr_t)
		    ifrup->ifr32.ifr_ifru.ifru_buffer.buffer);
#endif
	return (ifrup->ifr.ifr_ifru.ifru_buffer.buffer);
}

static void
ifr_buffer_set_buffer_null(void *data)
{
	union ifreq_union *ifrup;

	ifrup = data;
#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32))
		ifrup->ifr32.ifr_ifru.ifru_buffer.buffer = 0;
	else
#endif
		ifrup->ifr.ifr_ifru.ifru_buffer.buffer = NULL;
}

size_t
ifr_buffer_get_length(void *data)
{
	union ifreq_union *ifrup;

	ifrup = data;
#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32))
		return (ifrup->ifr32.ifr_ifru.ifru_buffer.length);
#endif
	return (ifrup->ifr.ifr_ifru.ifru_buffer.length);
}

static void
ifr_buffer_set_length(void *data, size_t len)
{
	union ifreq_union *ifrup;

	ifrup = data;
#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32))
		ifrup->ifr32.ifr_ifru.ifru_buffer.length = len;
	else
#endif
		ifrup->ifr.ifr_ifru.ifru_buffer.length = len;
}

void *
ifr_data_get_ptr(void *ifrp)
{
	union ifreq_union *ifrup;

	ifrup = ifrp;
#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32))
		return ((void *)(uintptr_t)
		    ifrup->ifr32.ifr_ifru.ifru_data);
#endif
		return (ifrup->ifr.ifr_ifru.ifru_data);
}

struct ifcap_nv_bit_name {
	uint64_t cap_bit;
	const char *cap_name;
};
#define CAPNV(x) {.cap_bit = IFCAP_##x, \
    .cap_name = __CONCAT(IFCAP_, __CONCAT(x, _NAME)) }
const struct ifcap_nv_bit_name ifcap_nv_bit_names[] = {
	CAPNV(RXCSUM),
	CAPNV(TXCSUM),
	CAPNV(NETCONS),
	CAPNV(VLAN_MTU),
	CAPNV(VLAN_HWTAGGING),
	CAPNV(JUMBO_MTU),
	CAPNV(POLLING),
	CAPNV(VLAN_HWCSUM),
	CAPNV(TSO4),
	CAPNV(TSO6),
	CAPNV(LRO),
	CAPNV(WOL_UCAST),
	CAPNV(WOL_MCAST),
	CAPNV(WOL_MAGIC),
	CAPNV(TOE4),
	CAPNV(TOE6),
	CAPNV(VLAN_HWFILTER),
	CAPNV(VLAN_HWTSO),
	CAPNV(LINKSTATE),
	CAPNV(NETMAP),
	CAPNV(RXCSUM_IPV6),
	CAPNV(TXCSUM_IPV6),
	CAPNV(HWSTATS),
	CAPNV(TXRTLMT),
	CAPNV(HWRXTSTMP),
	CAPNV(MEXTPG),
	CAPNV(TXTLS4),
	CAPNV(TXTLS6),
	CAPNV(VXLAN_HWCSUM),
	CAPNV(VXLAN_HWTSO),
	CAPNV(TXTLS_RTLMT),
	{0, NULL}
};
#define CAP2NV(x) {.cap_bit = IFCAP2_BIT(IFCAP2_##x), \
    .cap_name = __CONCAT(IFCAP2_, __CONCAT(x, _NAME)) }
const struct ifcap_nv_bit_name ifcap2_nv_bit_names[] = {
	CAP2NV(RXTLS4),
	CAP2NV(RXTLS6),
	{0, NULL}
};
#undef CAPNV
#undef CAP2NV

int
if_capnv_to_capint(const nvlist_t *nv, int *old_cap,
    const struct ifcap_nv_bit_name *nn, bool all)
{
	int i, res;

	res = 0;
	for (i = 0; nn[i].cap_name != NULL; i++) {
		if (nvlist_exists_bool(nv, nn[i].cap_name)) {
			if (all || nvlist_get_bool(nv, nn[i].cap_name))
				res |= nn[i].cap_bit;
		} else {
			res |= *old_cap & nn[i].cap_bit;
		}
	}
	return (res);
}

void
if_capint_to_capnv(nvlist_t *nv, const struct ifcap_nv_bit_name *nn,
    int ifr_cap, int ifr_req)
{
	int i;

	for (i = 0; nn[i].cap_name != NULL; i++) {
		if ((nn[i].cap_bit & ifr_cap) != 0) {
			nvlist_add_bool(nv, nn[i].cap_name,
			    (nn[i].cap_bit & ifr_req) != 0);
		}
	}
}

/*
 * Hardware specific interface ioctls.
 */
int
ifhwioctl(u_long cmd, struct ifnet *ifp, caddr_t data, struct thread *td)
{
	struct ifreq *ifr;
	int error = 0, do_ifup = 0;
	int new_flags, temp_flags;
	size_t descrlen, nvbuflen;
	char *descrbuf;
	char new_name[IFNAMSIZ];
	void *buf;
	nvlist_t *nvcap;
	struct siocsifcapnv_driver_data drv_ioctl_data;

	ifr = (struct ifreq *)data;
	switch (cmd) {
	case SIOCGIFINDEX:
		ifr->ifr_index = ifp->if_index;
		break;

	case SIOCGIFFLAGS:
		temp_flags = ifp->if_flags | ifp->if_drv_flags;
		ifr->ifr_flags = temp_flags & 0xffff;
		ifr->ifr_flagshigh = temp_flags >> 16;
		break;

	case SIOCGIFCAP:
		ifr->ifr_reqcap = ifp->if_capabilities;
		ifr->ifr_curcap = ifp->if_capenable;
		break;

	case SIOCGIFCAPNV:
		if ((ifp->if_capabilities & IFCAP_NV) == 0) {
			error = EINVAL;
			break;
		}
		buf = NULL;
		nvcap = nvlist_create(0);
		for (;;) {
			if_capint_to_capnv(nvcap, ifcap_nv_bit_names,
			    ifp->if_capabilities, ifp->if_capenable);
			if_capint_to_capnv(nvcap, ifcap2_nv_bit_names,
			    ifp->if_capabilities2, ifp->if_capenable2);
			error = (*ifp->if_ioctl)(ifp, SIOCGIFCAPNV,
			    __DECONST(caddr_t, nvcap));
			if (error != 0) {
				if_printf(ifp,
			    "SIOCGIFCAPNV driver mistake: nvlist error %d\n",
				    error);
				break;
			}
			buf = nvlist_pack(nvcap, &nvbuflen);
			if (buf == NULL) {
				error = nvlist_error(nvcap);
				if (error == 0)
					error = EDOOFUS;
				break;
			}
			if (nvbuflen > ifr->ifr_cap_nv.buf_length) {
				ifr->ifr_cap_nv.length = nvbuflen;
				ifr->ifr_cap_nv.buffer = NULL;
				error = EFBIG;
				break;
			}
			ifr->ifr_cap_nv.length = nvbuflen;
			error = copyout(buf, ifr->ifr_cap_nv.buffer, nvbuflen);
			break;
		}
		free(buf, M_NVLIST);
		nvlist_destroy(nvcap);
		break;

	case SIOCGIFDATA:
	{
		struct if_data ifd;

		/* Ensure uninitialised padding is not leaked. */
		memset(&ifd, 0, sizeof(ifd));

		if_data_copy(ifp, &ifd);
		error = copyout(&ifd, ifr_data_get_ptr(ifr), sizeof(ifd));
		break;
	}

#ifdef MAC
	case SIOCGIFMAC:
		error = mac_ifnet_ioctl_get(td->td_ucred, ifr, ifp);
		break;
#endif

	case SIOCGIFMETRIC:
		ifr->ifr_metric = ifp->if_metric;
		break;

	case SIOCGIFMTU:
		ifr->ifr_mtu = ifp->if_mtu;
		break;

	case SIOCGIFPHYS:
		/* XXXGL: did this ever worked? */
		ifr->ifr_phys = 0;
		break;

	case SIOCGIFDESCR:
		error = 0;
		sx_slock(&ifdescr_sx);
		if (ifp->if_description == NULL)
			error = ENOMSG;
		else {
			/* space for terminating nul */
			descrlen = strlen(ifp->if_description) + 1;
			if (ifr_buffer_get_length(ifr) < descrlen)
				ifr_buffer_set_buffer_null(ifr);
			else
				error = copyout(ifp->if_description,
				    ifr_buffer_get_buffer(ifr), descrlen);
			ifr_buffer_set_length(ifr, descrlen);
		}
		sx_sunlock(&ifdescr_sx);
		break;

	case SIOCSIFDESCR:
		error = priv_check(td, PRIV_NET_SETIFDESCR);
		if (error)
			return (error);

		/*
		 * Copy only (length-1) bytes to make sure that
		 * if_description is always nul terminated.  The
		 * length parameter is supposed to count the
		 * terminating nul in.
		 */
		if (ifr_buffer_get_length(ifr) > ifdescr_maxlen)
			return (ENAMETOOLONG);
		else if (ifr_buffer_get_length(ifr) == 0)
			descrbuf = NULL;
		else {
			descrbuf = if_allocdescr(ifr_buffer_get_length(ifr), M_WAITOK);
			error = copyin(ifr_buffer_get_buffer(ifr), descrbuf,
			    ifr_buffer_get_length(ifr) - 1);
			if (error) {
				if_freedescr(descrbuf);
				break;
			}
		}

		if_setdescr(ifp, descrbuf);
		getmicrotime(&ifp->if_lastchange);
		break;

	case SIOCGIFFIB:
		ifr->ifr_fib = ifp->if_fib;
		break;

	case SIOCSIFFIB:
		error = priv_check(td, PRIV_NET_SETIFFIB);
		if (error)
			return (error);
		if (ifr->ifr_fib >= rt_numfibs)
			return (EINVAL);

		ifp->if_fib = ifr->ifr_fib;
		break;

	case SIOCSIFFLAGS:
		error = priv_check(td, PRIV_NET_SETIFFLAGS);
		if (error)
			return (error);
		/*
		 * Currently, no driver owned flags pass the IFF_CANTCHANGE
		 * check, so we don't need special handling here yet.
		 */
		new_flags = (ifr->ifr_flags & 0xffff) |
		    (ifr->ifr_flagshigh << 16);
		if (ifp->if_flags & IFF_UP &&
		    (new_flags & IFF_UP) == 0) {
			if_down(ifp);
		} else if (new_flags & IFF_UP &&
		    (ifp->if_flags & IFF_UP) == 0) {
			do_ifup = 1;
		}
		/* See if permanently promiscuous mode bit is about to flip */
		if ((ifp->if_flags ^ new_flags) & IFF_PPROMISC) {
			if (new_flags & IFF_PPROMISC)
				ifp->if_flags |= IFF_PROMISC;
			else if (ifp->if_pcount == 0)
				ifp->if_flags &= ~IFF_PROMISC;
			if (log_promisc_mode_change)
                                if_printf(ifp, "permanently promiscuous mode %s\n",
                                    ((new_flags & IFF_PPROMISC) ?
                                     "enabled" : "disabled"));
		}
		ifp->if_flags = (ifp->if_flags & IFF_CANTCHANGE) |
			(new_flags &~ IFF_CANTCHANGE);
		if (ifp->if_ioctl) {
			(void) (*ifp->if_ioctl)(ifp, cmd, data);
		}
		if (do_ifup)
			if_up(ifp);
		getmicrotime(&ifp->if_lastchange);
		break;

	case SIOCSIFCAP:
		error = priv_check(td, PRIV_NET_SETIFCAP);
		if (error != 0)
			return (error);
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		if (ifr->ifr_reqcap & ~ifp->if_capabilities)
			return (EINVAL);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		if (error == 0)
			getmicrotime(&ifp->if_lastchange);
		break;

	case SIOCSIFCAPNV:
		error = priv_check(td, PRIV_NET_SETIFCAP);
		if (error != 0)
			return (error);
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		if ((ifp->if_capabilities & IFCAP_NV) == 0)
			return (EINVAL);
		if (ifr->ifr_cap_nv.length > IFR_CAP_NV_MAXBUFSIZE)
			return (EINVAL);
		nvcap = NULL;
		buf = malloc(ifr->ifr_cap_nv.length, M_TEMP, M_WAITOK);
		for (;;) {
			error = copyin(ifr->ifr_cap_nv.buffer, buf,
			    ifr->ifr_cap_nv.length);
			if (error != 0)
				break;
			nvcap = nvlist_unpack(buf, ifr->ifr_cap_nv.length, 0);
			if (nvcap == NULL) {
				error = EINVAL;
				break;
			}
			drv_ioctl_data.reqcap = if_capnv_to_capint(nvcap,
			    &ifp->if_capenable, ifcap_nv_bit_names, false);
			if ((drv_ioctl_data.reqcap &
			    ~ifp->if_capabilities) != 0) {
				error = EINVAL;
				break;
			}
			drv_ioctl_data.reqcap2 = if_capnv_to_capint(nvcap,
			    &ifp->if_capenable2, ifcap2_nv_bit_names, false);
			if ((drv_ioctl_data.reqcap2 &
			    ~ifp->if_capabilities2) != 0) {
				error = EINVAL;
				break;
			}
			drv_ioctl_data.nvcap = nvcap;
			error = (*ifp->if_ioctl)(ifp, SIOCSIFCAPNV,
			    (caddr_t)&drv_ioctl_data);
			break;
		}
		nvlist_destroy(nvcap);
		free(buf, M_TEMP);
		if (error == 0)
			getmicrotime(&ifp->if_lastchange);
		break;

#ifdef MAC
	case SIOCSIFMAC:
		error = mac_ifnet_ioctl_set(td->td_ucred, ifr, ifp);
		break;
#endif

	case SIOCSIFNAME:
		error = priv_check(td, PRIV_NET_SETIFNAME);
		if (error)
			return (error);
		error = copyinstr(ifr_data_get_ptr(ifr), new_name, IFNAMSIZ,
		    NULL);
		if (error != 0)
			return (error);
		error = if_rename(ifp, new_name);
		break;

#ifdef VIMAGE
	case SIOCSIFVNET:
		error = priv_check(td, PRIV_NET_SETIFVNET);
		if (error)
			return (error);
		error = if_vmove_loan(td, ifp, ifr->ifr_name, ifr->ifr_jid);
		break;
#endif

	case SIOCSIFMETRIC:
		error = priv_check(td, PRIV_NET_SETIFMETRIC);
		if (error)
			return (error);
		ifp->if_metric = ifr->ifr_metric;
		getmicrotime(&ifp->if_lastchange);
		break;

	case SIOCSIFPHYS:
		error = priv_check(td, PRIV_NET_SETIFPHYS);
		if (error)
			return (error);
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		if (error == 0)
			getmicrotime(&ifp->if_lastchange);
		break;

	case SIOCSIFMTU:
	{
		u_long oldmtu = ifp->if_mtu;

		error = priv_check(td, PRIV_NET_SETIFMTU);
		if (error)
			return (error);
		if (ifr->ifr_mtu < IF_MINMTU || ifr->ifr_mtu > IF_MAXMTU)
			return (EINVAL);
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		/* Disallow MTU changes on bridge member interfaces. */
		if (ifp->if_bridge)
			return (EOPNOTSUPP);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		if (error == 0) {
			getmicrotime(&ifp->if_lastchange);
			rt_ifmsg(ifp, 0);
#ifdef INET
			DEBUGNET_NOTIFY_MTU(ifp);
#endif
		}
		/*
		 * If the link MTU changed, do network layer specific procedure.
		 */
		if (ifp->if_mtu != oldmtu)
			if_notifymtu(ifp);
		break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (cmd == SIOCADDMULTI)
			error = priv_check(td, PRIV_NET_ADDMULTI);
		else
			error = priv_check(td, PRIV_NET_DELMULTI);
		if (error)
			return (error);

		/* Don't allow group membership on non-multicast interfaces. */
		if ((ifp->if_flags & IFF_MULTICAST) == 0)
			return (EOPNOTSUPP);

		/* Don't let users screw up protocols' entries. */
		if (ifr->ifr_addr.sa_family != AF_LINK)
			return (EINVAL);

		if (cmd == SIOCADDMULTI) {
			struct epoch_tracker et;
			struct ifmultiaddr *ifma;

			/*
			 * Userland is only permitted to join groups once
			 * via the if_addmulti() KPI, because it cannot hold
			 * struct ifmultiaddr * between calls. It may also
			 * lose a race while we check if the membership
			 * already exists.
			 */
			NET_EPOCH_ENTER(et);
			ifma = if_findmulti(ifp, &ifr->ifr_addr);
			NET_EPOCH_EXIT(et);
			if (ifma != NULL)
				error = EADDRINUSE;
			else
				error = if_addmulti(ifp, &ifr->ifr_addr, &ifma);
		} else {
			error = if_delmulti(ifp, &ifr->ifr_addr);
		}
		if (error == 0)
			getmicrotime(&ifp->if_lastchange);
		break;

	case SIOCSIFPHYADDR:
	case SIOCDIFPHYADDR:
#ifdef INET6
	case SIOCSIFPHYADDR_IN6:
#endif
	case SIOCSIFMEDIA:
	case SIOCSIFGENERIC:
		error = priv_check(td, PRIV_NET_HWIOCTL);
		if (error)
			return (error);
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		if (error == 0)
			getmicrotime(&ifp->if_lastchange);
		break;

	case SIOCGIFSTATUS:
	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
	case SIOCGIFMEDIA:
	case SIOCGIFXMEDIA:
	case SIOCGIFGENERIC:
	case SIOCGIFRSSKEY:
	case SIOCGIFRSSHASH:
	case SIOCGIFDOWNREASON:
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		break;

	case SIOCSIFLLADDR:
		error = priv_check(td, PRIV_NET_SETLLADDR);
		if (error)
			return (error);
		error = if_setlladdr(ifp,
		    ifr->ifr_addr.sa_data, ifr->ifr_addr.sa_len);
		break;

	case SIOCGHWADDR:
		error = if_gethwaddr(ifp, ifr);
		break;

	case SIOCAIFGROUP:
		error = priv_check(td, PRIV_NET_ADDIFGROUP);
		if (error)
			return (error);
		error = if_addgroup(ifp,
		    ((struct ifgroupreq *)data)->ifgr_group);
		if (error != 0)
			return (error);
		break;

	case SIOCGIFGROUP:
	{
		struct epoch_tracker et;

		NET_EPOCH_ENTER(et);
		error = if_getgroup((struct ifgroupreq *)data, ifp);
		NET_EPOCH_EXIT(et);
		break;
	}

	case SIOCDIFGROUP:
		error = priv_check(td, PRIV_NET_DELIFGROUP);
		if (error)
			return (error);
		error = if_delgroup(ifp,
		    ((struct ifgroupreq *)data)->ifgr_group);
		if (error != 0)
			return (error);
		break;

	default:
		error = ENOIOCTL;
		break;
	}
	return (error);
}

/*
 * Interface ioctls.
 */
int
ifioctl(struct socket *so, u_long cmd, caddr_t data, struct thread *td)
{
#ifdef COMPAT_FREEBSD32
	union {
		struct ifconf ifc;
		struct ifdrv ifd;
		struct ifgroupreq ifgr;
		struct ifmediareq ifmr;
	} thunk;
	u_long saved_cmd;
	struct ifconf32 *ifc32;
	struct ifdrv32 *ifd32;
	struct ifgroupreq32 *ifgr32;
	struct ifmediareq32 *ifmr32;
#endif
	struct ifnet *ifp;
	struct ifreq *ifr;
	int error;
	int oif_flags;
#ifdef VIMAGE
	bool shutdown;
#endif

	CURVNET_SET(so->so_vnet);
#ifdef VIMAGE
	/* Make sure the VNET is stable. */
	shutdown = VNET_IS_SHUTTING_DOWN(so->so_vnet);
	if (shutdown) {
		CURVNET_RESTORE();
		return (EBUSY);
	}
#endif

#ifdef COMPAT_FREEBSD32
	saved_cmd = cmd;
	switch (cmd) {
	case SIOCGIFCONF32:
		ifc32 = (struct ifconf32 *)data;
		thunk.ifc.ifc_len = ifc32->ifc_len;
		thunk.ifc.ifc_buf = PTRIN(ifc32->ifc_buf);
		data = (caddr_t)&thunk.ifc;
		cmd = SIOCGIFCONF;
		break;
	case SIOCGDRVSPEC32:
	case SIOCSDRVSPEC32:
		ifd32 = (struct ifdrv32 *)data;
		memcpy(thunk.ifd.ifd_name, ifd32->ifd_name,
		    sizeof(thunk.ifd.ifd_name));
		thunk.ifd.ifd_cmd = ifd32->ifd_cmd;
		thunk.ifd.ifd_len = ifd32->ifd_len;
		thunk.ifd.ifd_data = PTRIN(ifd32->ifd_data);
		data = (caddr_t)&thunk.ifd;
		cmd = _IOC_NEWTYPE(cmd, struct ifdrv);
		break;
	case SIOCAIFGROUP32:
	case SIOCGIFGROUP32:
	case SIOCDIFGROUP32:
	case SIOCGIFGMEMB32:
		ifgr32 = (struct ifgroupreq32 *)data;
		memcpy(thunk.ifgr.ifgr_name, ifgr32->ifgr_name,
		    sizeof(thunk.ifgr.ifgr_name));
		thunk.ifgr.ifgr_len = ifgr32->ifgr_len;
		switch (cmd) {
		case SIOCAIFGROUP32:
		case SIOCDIFGROUP32:
			memcpy(thunk.ifgr.ifgr_group, ifgr32->ifgr_group,
			    sizeof(thunk.ifgr.ifgr_group));
			break;
		case SIOCGIFGROUP32:
		case SIOCGIFGMEMB32:
			thunk.ifgr.ifgr_groups = PTRIN(ifgr32->ifgr_groups);
			break;
		}
		data = (caddr_t)&thunk.ifgr;
		cmd = _IOC_NEWTYPE(cmd, struct ifgroupreq);
		break;
	case SIOCGIFMEDIA32:
	case SIOCGIFXMEDIA32:
		ifmr32 = (struct ifmediareq32 *)data;
		memcpy(thunk.ifmr.ifm_name, ifmr32->ifm_name,
		    sizeof(thunk.ifmr.ifm_name));
		thunk.ifmr.ifm_current = ifmr32->ifm_current;
		thunk.ifmr.ifm_mask = ifmr32->ifm_mask;
		thunk.ifmr.ifm_status = ifmr32->ifm_status;
		thunk.ifmr.ifm_active = ifmr32->ifm_active;
		thunk.ifmr.ifm_count = ifmr32->ifm_count;
		thunk.ifmr.ifm_ulist = PTRIN(ifmr32->ifm_ulist);
		data = (caddr_t)&thunk.ifmr;
		cmd = _IOC_NEWTYPE(cmd, struct ifmediareq);
		break;
	}
#endif

	switch (cmd) {
	case SIOCGIFCONF:
		error = ifconf(cmd, data);
		goto out_noref;
	}

	ifr = (struct ifreq *)data;
	switch (cmd) {
#ifdef VIMAGE
	case SIOCSIFRVNET:
		error = priv_check(td, PRIV_NET_SETIFVNET);
		if (error == 0)
			error = if_vmove_reclaim(td, ifr->ifr_name,
			    ifr->ifr_jid);
		goto out_noref;
#endif
	case SIOCIFCREATE:
	case SIOCIFCREATE2:
		error = priv_check(td, PRIV_NET_IFCREATE);
		if (error == 0)
			error = if_clone_create(ifr->ifr_name,
			    sizeof(ifr->ifr_name), cmd == SIOCIFCREATE2 ?
			    ifr_data_get_ptr(ifr) : NULL);
		goto out_noref;
	case SIOCIFDESTROY:
		error = priv_check(td, PRIV_NET_IFDESTROY);

		if (error == 0) {
			sx_xlock(&ifnet_detach_sxlock);
			error = if_clone_destroy(ifr->ifr_name);
			sx_xunlock(&ifnet_detach_sxlock);
		}
		goto out_noref;

	case SIOCIFGCLONERS:
		error = if_clone_list((struct if_clonereq *)data);
		goto out_noref;

	case SIOCGIFGMEMB:
		error = if_getgroupmembers((struct ifgroupreq *)data);
		goto out_noref;

#if defined(INET) || defined(INET6)
	case SIOCSVH:
	case SIOCGVH:
		if (carp_ioctl_p == NULL)
			error = EPROTONOSUPPORT;
		else
			error = (*carp_ioctl_p)(ifr, cmd, td);
		goto out_noref;
#endif
	}

	ifp = ifunit_ref(ifr->ifr_name);
	if (ifp == NULL) {
		error = ENXIO;
		goto out_noref;
	}

	error = ifhwioctl(cmd, ifp, data, td);
	if (error != ENOIOCTL)
		goto out_ref;

	oif_flags = ifp->if_flags;
	if (so->so_proto == NULL) {
		error = EOPNOTSUPP;
		goto out_ref;
	}

	/*
	 * Pass the request on to the socket control method, and if the
	 * latter returns EOPNOTSUPP, directly to the interface.
	 *
	 * Make an exception for the legacy SIOCSIF* requests.  Drivers
	 * trust SIOCSIFADDR et al to come from an already privileged
	 * layer, and do not perform any credentials checks or input
	 * validation.
	 */
	error = so->so_proto->pr_control(so, cmd, data, ifp, td);
	if (error == EOPNOTSUPP && ifp != NULL && ifp->if_ioctl != NULL &&
	    cmd != SIOCSIFADDR && cmd != SIOCSIFBRDADDR &&
	    cmd != SIOCSIFDSTADDR && cmd != SIOCSIFNETMASK)
		error = (*ifp->if_ioctl)(ifp, cmd, data);

	if (!(oif_flags & IFF_UP) && (ifp->if_flags & IFF_UP))
		if_up(ifp);
out_ref:
	if_rele(ifp);
out_noref:
	CURVNET_RESTORE();
#ifdef COMPAT_FREEBSD32
	if (error != 0)
		return (error);
	switch (saved_cmd) {
	case SIOCGIFCONF32:
		ifc32->ifc_len = thunk.ifc.ifc_len;
		break;
	case SIOCGDRVSPEC32:
		/*
		 * SIOCGDRVSPEC is IOWR, but nothing actually touches
		 * the struct so just assert that ifd_len (the only
		 * field it might make sense to update) hasn't
		 * changed.
		 */
		KASSERT(thunk.ifd.ifd_len == ifd32->ifd_len,
		    ("ifd_len was updated %u -> %zu", ifd32->ifd_len,
			thunk.ifd.ifd_len));
		break;
	case SIOCGIFGROUP32:
	case SIOCGIFGMEMB32:
		ifgr32->ifgr_len = thunk.ifgr.ifgr_len;
		break;
	case SIOCGIFMEDIA32:
	case SIOCGIFXMEDIA32:
		ifmr32->ifm_current = thunk.ifmr.ifm_current;
		ifmr32->ifm_mask = thunk.ifmr.ifm_mask;
		ifmr32->ifm_status = thunk.ifmr.ifm_status;
		ifmr32->ifm_active = thunk.ifmr.ifm_active;
		ifmr32->ifm_count = thunk.ifmr.ifm_count;
		break;
	}
#endif
	return (error);
}

int
if_rename(struct ifnet *ifp, char *new_name)
{
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	size_t namelen, onamelen;
	char old_name[IFNAMSIZ];
	char strbuf[IFNAMSIZ + 8];

	if (new_name[0] == '\0')
		return (EINVAL);
	if (strcmp(new_name, ifp->if_xname) == 0)
		return (0);
	if (ifunit(new_name) != NULL)
		return (EEXIST);

	/*
	 * XXX: Locking.  Nothing else seems to lock if_flags,
	 * and there are numerous other races with the
	 * ifunit() checks not being atomic with namespace
	 * changes (renames, vmoves, if_attach, etc).
	 */
	ifp->if_flags |= IFF_RENAMING;

	EVENTHANDLER_INVOKE(ifnet_departure_event, ifp);

	if_printf(ifp, "changing name to '%s'\n", new_name);

	IF_ADDR_WLOCK(ifp);
	strlcpy(old_name, ifp->if_xname, sizeof(old_name));
	strlcpy(ifp->if_xname, new_name, sizeof(ifp->if_xname));
	ifa = ifp->if_addr;
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	namelen = strlen(new_name);
	onamelen = sdl->sdl_nlen;
	/*
	 * Move the address if needed.  This is safe because we
	 * allocate space for a name of length IFNAMSIZ when we
	 * create this in if_attach().
	 */
	if (namelen != onamelen) {
		bcopy(sdl->sdl_data + onamelen,
		    sdl->sdl_data + namelen, sdl->sdl_alen);
	}
	bcopy(new_name, sdl->sdl_data, namelen);
	sdl->sdl_nlen = namelen;
	sdl = (struct sockaddr_dl *)ifa->ifa_netmask;
	bzero(sdl->sdl_data, onamelen);
	while (namelen != 0)
		sdl->sdl_data[--namelen] = 0xff;
	IF_ADDR_WUNLOCK(ifp);

	EVENTHANDLER_INVOKE(ifnet_arrival_event, ifp);

	ifp->if_flags &= ~IFF_RENAMING;

	snprintf(strbuf, sizeof(strbuf), "name=%s", new_name);
	devctl_notify("IFNET", old_name, "RENAME", strbuf);

	return (0);
}

/*
 * The code common to handling reference counted flags,
 * e.g., in ifpromisc() and if_allmulti().
 * The "pflag" argument can specify a permanent mode flag to check,
 * such as IFF_PPROMISC for promiscuous mode; should be 0 if none.
 *
 * Only to be used on stack-owned flags, not driver-owned flags.
 */
static int
if_setflag(struct ifnet *ifp, int flag, int pflag, int *refcount, int onswitch)
{
	struct ifreq ifr;
	int error;
	int oldflags, oldcount;

	/* Sanity checks to catch programming errors */
	KASSERT((flag & (IFF_DRV_OACTIVE|IFF_DRV_RUNNING)) == 0,
	    ("%s: setting driver-owned flag %d", __func__, flag));

	if (onswitch)
		KASSERT(*refcount >= 0,
		    ("%s: increment negative refcount %d for flag %d",
		    __func__, *refcount, flag));
	else
		KASSERT(*refcount > 0,
		    ("%s: decrement non-positive refcount %d for flag %d",
		    __func__, *refcount, flag));

	/* In case this mode is permanent, just touch refcount */
	if (ifp->if_flags & pflag) {
		*refcount += onswitch ? 1 : -1;
		return (0);
	}

	/* Save ifnet parameters for if_ioctl() may fail */
	oldcount = *refcount;
	oldflags = ifp->if_flags;

	/*
	 * See if we aren't the only and touching refcount is enough.
	 * Actually toggle interface flag if we are the first or last.
	 */
	if (onswitch) {
		if ((*refcount)++)
			return (0);
		ifp->if_flags |= flag;
	} else {
		if (--(*refcount))
			return (0);
		ifp->if_flags &= ~flag;
	}

	/* Call down the driver since we've changed interface flags */
	if (ifp->if_ioctl == NULL) {
		error = EOPNOTSUPP;
		goto recover;
	}
	ifr.ifr_flags = ifp->if_flags & 0xffff;
	ifr.ifr_flagshigh = ifp->if_flags >> 16;
	error = (*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
	if (error)
		goto recover;
	/* Notify userland that interface flags have changed */
	rt_ifmsg(ifp, flag);
	return (0);

recover:
	/* Recover after driver error */
	*refcount = oldcount;
	ifp->if_flags = oldflags;
	return (error);
}

/*
 * Set/clear promiscuous mode on interface ifp based on the truth value
 * of pswitch.  The calls are reference counted so that only the first
 * "on" request actually has an effect, as does the final "off" request.
 * Results are undefined if the "off" and "on" requests are not matched.
 */
int
ifpromisc(struct ifnet *ifp, int pswitch)
{
	int error;
	int oldflags = ifp->if_flags;

	error = if_setflag(ifp, IFF_PROMISC, IFF_PPROMISC,
			   &ifp->if_pcount, pswitch);
	/* If promiscuous mode status has changed, log a message */
	if (error == 0 && ((ifp->if_flags ^ oldflags) & IFF_PROMISC) &&
            log_promisc_mode_change)
		if_printf(ifp, "promiscuous mode %s\n",
		    (ifp->if_flags & IFF_PROMISC) ? "enabled" : "disabled");
	return (error);
}

/*
 * Return interface configuration
 * of system.  List may be used
 * in later ioctl's (above) to get
 * other information.
 */
/*ARGSUSED*/
static int
ifconf(u_long cmd, caddr_t data)
{
	struct ifconf *ifc = (struct ifconf *)data;
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct ifreq ifr;
	struct sbuf *sb;
	int error, full = 0, valid_len, max_len;

	/* Limit initial buffer size to maxphys to avoid DoS from userspace. */
	max_len = maxphys - 1;

	/* Prevent hostile input from being able to crash the system */
	if (ifc->ifc_len <= 0)
		return (EINVAL);

again:
	if (ifc->ifc_len <= max_len) {
		max_len = ifc->ifc_len;
		full = 1;
	}
	sb = sbuf_new(NULL, NULL, max_len + 1, SBUF_FIXEDLEN);
	max_len = 0;
	valid_len = 0;

	IFNET_RLOCK();
	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		struct epoch_tracker et;
		int addrs;

		/*
		 * Zero the ifr to make sure we don't disclose the contents
		 * of the stack.
		 */
		memset(&ifr, 0, sizeof(ifr));

		if (strlcpy(ifr.ifr_name, ifp->if_xname, sizeof(ifr.ifr_name))
		    >= sizeof(ifr.ifr_name)) {
			sbuf_delete(sb);
			IFNET_RUNLOCK();
			return (ENAMETOOLONG);
		}

		addrs = 0;
		NET_EPOCH_ENTER(et);
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			struct sockaddr *sa = ifa->ifa_addr;

			if (prison_if(curthread->td_ucred, sa) != 0)
				continue;
			addrs++;
			if (sa->sa_len <= sizeof(*sa)) {
				if (sa->sa_len < sizeof(*sa)) {
					memset(&ifr.ifr_ifru.ifru_addr, 0,
					    sizeof(ifr.ifr_ifru.ifru_addr));
					memcpy(&ifr.ifr_ifru.ifru_addr, sa,
					    sa->sa_len);
				} else
					ifr.ifr_ifru.ifru_addr = *sa;
				sbuf_bcat(sb, &ifr, sizeof(ifr));
				max_len += sizeof(ifr);
			} else {
				sbuf_bcat(sb, &ifr,
				    offsetof(struct ifreq, ifr_addr));
				max_len += offsetof(struct ifreq, ifr_addr);
				sbuf_bcat(sb, sa, sa->sa_len);
				max_len += sa->sa_len;
			}

			if (sbuf_error(sb) == 0)
				valid_len = sbuf_len(sb);
		}
		NET_EPOCH_EXIT(et);
		if (addrs == 0) {
			sbuf_bcat(sb, &ifr, sizeof(ifr));
			max_len += sizeof(ifr);

			if (sbuf_error(sb) == 0)
				valid_len = sbuf_len(sb);
		}
	}
	IFNET_RUNLOCK();

	/*
	 * If we didn't allocate enough space (uncommon), try again.  If
	 * we have already allocated as much space as we are allowed,
	 * return what we've got.
	 */
	if (valid_len != max_len && !full) {
		sbuf_delete(sb);
		goto again;
	}

	ifc->ifc_len = valid_len;
	sbuf_finish(sb);
	error = copyout(sbuf_data(sb), ifc->ifc_req, ifc->ifc_len);
	sbuf_delete(sb);
	return (error);
}

/*
 * Just like ifpromisc(), but for all-multicast-reception mode.
 */
int
if_allmulti(struct ifnet *ifp, int onswitch)
{

	return (if_setflag(ifp, IFF_ALLMULTI, 0, &ifp->if_amcount, onswitch));
}

struct ifmultiaddr *
if_findmulti(struct ifnet *ifp, const struct sockaddr *sa)
{
	struct ifmultiaddr *ifma;

	IF_ADDR_LOCK_ASSERT(ifp);

	CK_STAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (sa->sa_family == AF_LINK) {
			if (sa_dl_equal(ifma->ifma_addr, sa))
				break;
		} else {
			if (sa_equal(ifma->ifma_addr, sa))
				break;
		}
	}

	return ifma;
}

/*
 * Allocate a new ifmultiaddr and initialize based on passed arguments.  We
 * make copies of passed sockaddrs.  The ifmultiaddr will not be added to
 * the ifnet multicast address list here, so the caller must do that and
 * other setup work (such as notifying the device driver).  The reference
 * count is initialized to 1.
 */
static struct ifmultiaddr *
if_allocmulti(struct ifnet *ifp, struct sockaddr *sa, struct sockaddr *llsa,
    int mflags)
{
	struct ifmultiaddr *ifma;
	struct sockaddr *dupsa;

	ifma = malloc(sizeof *ifma, M_IFMADDR, mflags |
	    M_ZERO);
	if (ifma == NULL)
		return (NULL);

	dupsa = malloc(sa->sa_len, M_IFMADDR, mflags);
	if (dupsa == NULL) {
		free(ifma, M_IFMADDR);
		return (NULL);
	}
	bcopy(sa, dupsa, sa->sa_len);
	ifma->ifma_addr = dupsa;

	ifma->ifma_ifp = ifp;
	ifma->ifma_refcount = 1;
	ifma->ifma_protospec = NULL;

	if (llsa == NULL) {
		ifma->ifma_lladdr = NULL;
		return (ifma);
	}

	dupsa = malloc(llsa->sa_len, M_IFMADDR, mflags);
	if (dupsa == NULL) {
		free(ifma->ifma_addr, M_IFMADDR);
		free(ifma, M_IFMADDR);
		return (NULL);
	}
	bcopy(llsa, dupsa, llsa->sa_len);
	ifma->ifma_lladdr = dupsa;

	return (ifma);
}

/*
 * if_freemulti: free ifmultiaddr structure and possibly attached related
 * addresses.  The caller is responsible for implementing reference
 * counting, notifying the driver, handling routing messages, and releasing
 * any dependent link layer state.
 */
#ifdef MCAST_VERBOSE
extern void kdb_backtrace(void);
#endif
static void
if_freemulti_internal(struct ifmultiaddr *ifma)
{

	KASSERT(ifma->ifma_refcount == 0, ("if_freemulti: refcount %d",
	    ifma->ifma_refcount));

	if (ifma->ifma_lladdr != NULL)
		free(ifma->ifma_lladdr, M_IFMADDR);
#ifdef MCAST_VERBOSE
	kdb_backtrace();
	printf("%s freeing ifma: %p\n", __func__, ifma);
#endif
	free(ifma->ifma_addr, M_IFMADDR);
	free(ifma, M_IFMADDR);
}

static void
if_destroymulti(epoch_context_t ctx)
{
	struct ifmultiaddr *ifma;

	ifma = __containerof(ctx, struct ifmultiaddr, ifma_epoch_ctx);
	if_freemulti_internal(ifma);
}

void
if_freemulti(struct ifmultiaddr *ifma)
{
	KASSERT(ifma->ifma_refcount == 0, ("if_freemulti_epoch: refcount %d",
	    ifma->ifma_refcount));

	NET_EPOCH_CALL(if_destroymulti, &ifma->ifma_epoch_ctx);
}

/*
 * Register an additional multicast address with a network interface.
 *
 * - If the address is already present, bump the reference count on the
 *   address and return.
 * - If the address is not link-layer, look up a link layer address.
 * - Allocate address structures for one or both addresses, and attach to the
 *   multicast address list on the interface.  If automatically adding a link
 *   layer address, the protocol address will own a reference to the link
 *   layer address, to be freed when it is freed.
 * - Notify the network device driver of an addition to the multicast address
 *   list.
 *
 * 'sa' points to caller-owned memory with the desired multicast address.
 *
 * 'retifma' will be used to return a pointer to the resulting multicast
 * address reference, if desired.
 */
int
if_addmulti(struct ifnet *ifp, struct sockaddr *sa,
    struct ifmultiaddr **retifma)
{
	struct ifmultiaddr *ifma, *ll_ifma;
	struct sockaddr *llsa;
	struct sockaddr_dl sdl;
	int error;

#ifdef INET
	IN_MULTI_LIST_UNLOCK_ASSERT();
#endif
#ifdef INET6
	IN6_MULTI_LIST_UNLOCK_ASSERT();
#endif
	/*
	 * If the address is already present, return a new reference to it;
	 * otherwise, allocate storage and set up a new address.
	 */
	IF_ADDR_WLOCK(ifp);
	ifma = if_findmulti(ifp, sa);
	if (ifma != NULL) {
		ifma->ifma_refcount++;
		if (retifma != NULL)
			*retifma = ifma;
		IF_ADDR_WUNLOCK(ifp);
		return (0);
	}

	/*
	 * The address isn't already present; resolve the protocol address
	 * into a link layer address, and then look that up, bump its
	 * refcount or allocate an ifma for that also.
	 * Most link layer resolving functions returns address data which
	 * fits inside default sockaddr_dl structure. However callback
	 * can allocate another sockaddr structure, in that case we need to
	 * free it later.
	 */
	llsa = NULL;
	ll_ifma = NULL;
	if (ifp->if_resolvemulti != NULL) {
		/* Provide called function with buffer size information */
		sdl.sdl_len = sizeof(sdl);
		llsa = (struct sockaddr *)&sdl;
		error = ifp->if_resolvemulti(ifp, &llsa, sa);
		if (error)
			goto unlock_out;
	}

	/*
	 * Allocate the new address.  Don't hook it up yet, as we may also
	 * need to allocate a link layer multicast address.
	 */
	ifma = if_allocmulti(ifp, sa, llsa, M_NOWAIT);
	if (ifma == NULL) {
		error = ENOMEM;
		goto free_llsa_out;
	}

	/*
	 * If a link layer address is found, we'll need to see if it's
	 * already present in the address list, or allocate is as well.
	 * When this block finishes, the link layer address will be on the
	 * list.
	 */
	if (llsa != NULL) {
		ll_ifma = if_findmulti(ifp, llsa);
		if (ll_ifma == NULL) {
			ll_ifma = if_allocmulti(ifp, llsa, NULL, M_NOWAIT);
			if (ll_ifma == NULL) {
				--ifma->ifma_refcount;
				if_freemulti(ifma);
				error = ENOMEM;
				goto free_llsa_out;
			}
			ll_ifma->ifma_flags |= IFMA_F_ENQUEUED;
			CK_STAILQ_INSERT_HEAD(&ifp->if_multiaddrs, ll_ifma,
			    ifma_link);
		} else
			ll_ifma->ifma_refcount++;
		ifma->ifma_llifma = ll_ifma;
	}

	/*
	 * We now have a new multicast address, ifma, and possibly a new or
	 * referenced link layer address.  Add the primary address to the
	 * ifnet address list.
	 */
	ifma->ifma_flags |= IFMA_F_ENQUEUED;
	CK_STAILQ_INSERT_HEAD(&ifp->if_multiaddrs, ifma, ifma_link);

	if (retifma != NULL)
		*retifma = ifma;

	/*
	 * Must generate the message while holding the lock so that 'ifma'
	 * pointer is still valid.
	 */
	rt_newmaddrmsg(RTM_NEWMADDR, ifma);
	IF_ADDR_WUNLOCK(ifp);

	/*
	 * We are certain we have added something, so call down to the
	 * interface to let them know about it.
	 */
	if (ifp->if_ioctl != NULL) {
		if (THREAD_CAN_SLEEP())
			(void )(*ifp->if_ioctl)(ifp, SIOCADDMULTI, 0);
		else
			taskqueue_enqueue(taskqueue_swi, &ifp->if_addmultitask);
	}

	if ((llsa != NULL) && (llsa != (struct sockaddr *)&sdl))
		link_free_sdl(llsa);

	return (0);

free_llsa_out:
	if ((llsa != NULL) && (llsa != (struct sockaddr *)&sdl))
		link_free_sdl(llsa);

unlock_out:
	IF_ADDR_WUNLOCK(ifp);
	return (error);
}

static void
if_siocaddmulti(void *arg, int pending)
{
	struct ifnet *ifp;

	ifp = arg;
#ifdef DIAGNOSTIC
	if (pending > 1)
		if_printf(ifp, "%d SIOCADDMULTI coalesced\n", pending);
#endif
	CURVNET_SET(ifp->if_vnet);
	(void )(*ifp->if_ioctl)(ifp, SIOCADDMULTI, 0);
	CURVNET_RESTORE();
}

/*
 * Delete a multicast group membership by network-layer group address.
 *
 * Returns ENOENT if the entry could not be found. If ifp no longer
 * exists, results are undefined. This entry point should only be used
 * from subsystems which do appropriate locking to hold ifp for the
 * duration of the call.
 * Network-layer protocol domains must use if_delmulti_ifma().
 */
int
if_delmulti(struct ifnet *ifp, struct sockaddr *sa)
{
	struct ifmultiaddr *ifma;
	int lastref;

	KASSERT(ifp, ("%s: NULL ifp", __func__));

	IF_ADDR_WLOCK(ifp);
	lastref = 0;
	ifma = if_findmulti(ifp, sa);
	if (ifma != NULL)
		lastref = if_delmulti_locked(ifp, ifma, 0);
	IF_ADDR_WUNLOCK(ifp);

	if (ifma == NULL)
		return (ENOENT);

	if (lastref && ifp->if_ioctl != NULL) {
		(void)(*ifp->if_ioctl)(ifp, SIOCDELMULTI, 0);
	}

	return (0);
}

/*
 * Delete all multicast group membership for an interface.
 * Should be used to quickly flush all multicast filters.
 */
void
if_delallmulti(struct ifnet *ifp)
{
	struct ifmultiaddr *ifma;
	struct ifmultiaddr *next;

	IF_ADDR_WLOCK(ifp);
	CK_STAILQ_FOREACH_SAFE(ifma, &ifp->if_multiaddrs, ifma_link, next)
		if_delmulti_locked(ifp, ifma, 0);
	IF_ADDR_WUNLOCK(ifp);
}

void
if_delmulti_ifma(struct ifmultiaddr *ifma)
{
	if_delmulti_ifma_flags(ifma, 0);
}

/*
 * Delete a multicast group membership by group membership pointer.
 * Network-layer protocol domains must use this routine.
 *
 * It is safe to call this routine if the ifp disappeared.
 */
void
if_delmulti_ifma_flags(struct ifmultiaddr *ifma, int flags)
{
	struct ifnet *ifp;
	int lastref;
	MCDPRINTF("%s freeing ifma: %p\n", __func__, ifma);
#ifdef INET
	IN_MULTI_LIST_UNLOCK_ASSERT();
#endif
	ifp = ifma->ifma_ifp;
#ifdef DIAGNOSTIC
	if (ifp == NULL) {
		printf("%s: ifma_ifp seems to be detached\n", __func__);
	} else {
		struct epoch_tracker et;
		struct ifnet *oifp;

		NET_EPOCH_ENTER(et);
		CK_STAILQ_FOREACH(oifp, &V_ifnet, if_link)
			if (ifp == oifp)
				break;
		NET_EPOCH_EXIT(et);
		if (ifp != oifp)
			ifp = NULL;
	}
#endif
	/*
	 * If and only if the ifnet instance exists: Acquire the address lock.
	 */
	if (ifp != NULL)
		IF_ADDR_WLOCK(ifp);

	lastref = if_delmulti_locked(ifp, ifma, flags);

	if (ifp != NULL) {
		/*
		 * If and only if the ifnet instance exists:
		 *  Release the address lock.
		 *  If the group was left: update the hardware hash filter.
		 */
		IF_ADDR_WUNLOCK(ifp);
		if (lastref && ifp->if_ioctl != NULL) {
			(void)(*ifp->if_ioctl)(ifp, SIOCDELMULTI, 0);
		}
	}
}

/*
 * Perform deletion of network-layer and/or link-layer multicast address.
 *
 * Return 0 if the reference count was decremented.
 * Return 1 if the final reference was released, indicating that the
 * hardware hash filter should be reprogrammed.
 */
static int
if_delmulti_locked(struct ifnet *ifp, struct ifmultiaddr *ifma, int detaching)
{
	struct ifmultiaddr *ll_ifma;

	if (ifp != NULL && ifma->ifma_ifp != NULL) {
		KASSERT(ifma->ifma_ifp == ifp,
		    ("%s: inconsistent ifp %p", __func__, ifp));
		IF_ADDR_WLOCK_ASSERT(ifp);
	}

	ifp = ifma->ifma_ifp;
	MCDPRINTF("%s freeing %p from %s \n", __func__, ifma, ifp ? ifp->if_xname : "");

	/*
	 * If the ifnet is detaching, null out references to ifnet,
	 * so that upper protocol layers will notice, and not attempt
	 * to obtain locks for an ifnet which no longer exists. The
	 * routing socket announcement must happen before the ifnet
	 * instance is detached from the system.
	 */
	if (detaching) {
#ifdef DIAGNOSTIC
		printf("%s: detaching ifnet instance %p\n", __func__, ifp);
#endif
		/*
		 * ifp may already be nulled out if we are being reentered
		 * to delete the ll_ifma.
		 */
		if (ifp != NULL) {
			rt_newmaddrmsg(RTM_DELMADDR, ifma);
			ifma->ifma_ifp = NULL;
		}
	}

	if (--ifma->ifma_refcount > 0)
		return 0;

	if (ifp != NULL && detaching == 0 && (ifma->ifma_flags & IFMA_F_ENQUEUED)) {
		CK_STAILQ_REMOVE(&ifp->if_multiaddrs, ifma, ifmultiaddr, ifma_link);
		ifma->ifma_flags &= ~IFMA_F_ENQUEUED;
	}
	/*
	 * If this ifma is a network-layer ifma, a link-layer ifma may
	 * have been associated with it. Release it first if so.
	 */
	ll_ifma = ifma->ifma_llifma;
	if (ll_ifma != NULL) {
		KASSERT(ifma->ifma_lladdr != NULL,
		    ("%s: llifma w/o lladdr", __func__));
		if (detaching)
			ll_ifma->ifma_ifp = NULL;	/* XXX */
		if (--ll_ifma->ifma_refcount == 0) {
			if (ifp != NULL) {
				if (ll_ifma->ifma_flags & IFMA_F_ENQUEUED) {
					CK_STAILQ_REMOVE(&ifp->if_multiaddrs, ll_ifma, ifmultiaddr,
						ifma_link);
					ll_ifma->ifma_flags &= ~IFMA_F_ENQUEUED;
				}
			}
			if_freemulti(ll_ifma);
		}
	}
#ifdef INVARIANTS
	if (ifp) {
		struct ifmultiaddr *ifmatmp;

		CK_STAILQ_FOREACH(ifmatmp, &ifp->if_multiaddrs, ifma_link)
			MPASS(ifma != ifmatmp);
	}
#endif
	if_freemulti(ifma);
	/*
	 * The last reference to this instance of struct ifmultiaddr
	 * was released; the hardware should be notified of this change.
	 */
	return 1;
}

/*
 * Set the link layer address on an interface.
 *
 * At this time we only support certain types of interfaces,
 * and we don't allow the length of the address to change.
 *
 * Set noinline to be dtrace-friendly
 */
__noinline int
if_setlladdr(struct ifnet *ifp, const u_char *lladdr, int len)
{
	struct sockaddr_dl *sdl;
	struct ifaddr *ifa;
	struct ifreq ifr;

	ifa = ifp->if_addr;
	if (ifa == NULL)
		return (EINVAL);

	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	if (sdl == NULL)
		return (EINVAL);

	if (len != sdl->sdl_alen)	/* don't allow length to change */
		return (EINVAL);

	switch (ifp->if_type) {
	case IFT_ETHER:
	case IFT_XETHER:
	case IFT_L2VLAN:
	case IFT_BRIDGE:
	case IFT_IEEE8023ADLAG:
		bcopy(lladdr, LLADDR(sdl), len);
		break;
	default:
		return (ENODEV);
	}

	/*
	 * If the interface is already up, we need
	 * to re-init it in order to reprogram its
	 * address filter.
	 */
	if ((ifp->if_flags & IFF_UP) != 0) {
		if (ifp->if_ioctl) {
			ifp->if_flags &= ~IFF_UP;
			ifr.ifr_flags = ifp->if_flags & 0xffff;
			ifr.ifr_flagshigh = ifp->if_flags >> 16;
			(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
			ifp->if_flags |= IFF_UP;
			ifr.ifr_flags = ifp->if_flags & 0xffff;
			ifr.ifr_flagshigh = ifp->if_flags >> 16;
			(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
		}
	}
	EVENTHANDLER_INVOKE(iflladdr_event, ifp);

	return (0);
}

/*
 * Compat function for handling basic encapsulation requests.
 * Not converted stacks (FDDI, IB, ..) supports traditional
 * output model: ARP (and other similar L2 protocols) are handled
 * inside output routine, arpresolve/nd6_resolve() returns MAC
 * address instead of full prepend.
 *
 * This function creates calculated header==MAC for IPv4/IPv6 and
 * returns EAFNOSUPPORT (which is then handled in ARP code) for other
 * address families.
 */
static int
if_requestencap_default(struct ifnet *ifp, struct if_encap_req *req)
{
	if (req->rtype != IFENCAP_LL)
		return (EOPNOTSUPP);

	if (req->bufsize < req->lladdr_len)
		return (ENOMEM);

	switch (req->family) {
	case AF_INET:
	case AF_INET6:
		break;
	default:
		return (EAFNOSUPPORT);
	}

	/* Copy lladdr to storage as is */
	memmove(req->buf, req->lladdr, req->lladdr_len);
	req->bufsize = req->lladdr_len;
	req->lladdr_off = 0;

	return (0);
}

/*
 * Tunnel interfaces can nest, also they may cause infinite recursion
 * calls when misconfigured. We'll prevent this by detecting loops.
 * High nesting level may cause stack exhaustion. We'll prevent this
 * by introducing upper limit.
 *
 * Return 0, if tunnel nesting count is equal or less than limit.
 */
int
if_tunnel_check_nesting(struct ifnet *ifp, struct mbuf *m, uint32_t cookie,
    int limit)
{
	struct m_tag *mtag;
	int count;

	count = 1;
	mtag = NULL;
	while ((mtag = m_tag_locate(m, cookie, 0, mtag)) != NULL) {
		if (*(struct ifnet **)(mtag + 1) == ifp) {
			log(LOG_NOTICE, "%s: loop detected\n", if_name(ifp));
			return (EIO);
		}
		count++;
	}
	if (count > limit) {
		log(LOG_NOTICE,
		    "%s: if_output recursively called too many times(%d)\n",
		    if_name(ifp), count);
		return (EIO);
	}
	mtag = m_tag_alloc(cookie, 0, sizeof(struct ifnet *), M_NOWAIT);
	if (mtag == NULL)
		return (ENOMEM);
	*(struct ifnet **)(mtag + 1) = ifp;
	m_tag_prepend(m, mtag);
	return (0);
}

/*
 * Get the link layer address that was read from the hardware at attach.
 *
 * This is only set by Ethernet NICs (IFT_ETHER), but laggX interfaces re-type
 * their component interfaces as IFT_IEEE8023ADLAG.
 */
int
if_gethwaddr(struct ifnet *ifp, struct ifreq *ifr)
{
	if (ifp->if_hw_addr == NULL)
		return (ENODEV);

	switch (ifp->if_type) {
	case IFT_ETHER:
	case IFT_IEEE8023ADLAG:
		bcopy(ifp->if_hw_addr, ifr->ifr_addr.sa_data, ifp->if_addrlen);
		return (0);
	default:
		return (ENODEV);
	}
}

/*
 * The name argument must be a pointer to storage which will last as
 * long as the interface does.  For physical devices, the result of
 * device_get_name(dev) is a good choice and for pseudo-devices a
 * static string works well.
 */
void
if_initname(struct ifnet *ifp, const char *name, int unit)
{
	ifp->if_dname = name;
	ifp->if_dunit = unit;
	if (unit != IF_DUNIT_NONE)
		snprintf(ifp->if_xname, IFNAMSIZ, "%s%d", name, unit);
	else
		strlcpy(ifp->if_xname, name, IFNAMSIZ);
}

static int
if_vlog(struct ifnet *ifp, int pri, const char *fmt, va_list ap)
{
	char if_fmt[256];

	snprintf(if_fmt, sizeof(if_fmt), "%s: %s", ifp->if_xname, fmt);
	vlog(pri, if_fmt, ap);
	return (0);
}


int
if_printf(struct ifnet *ifp, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if_vlog(ifp, LOG_INFO, fmt, ap);
	va_end(ap);
	return (0);
}

int
if_log(struct ifnet *ifp, int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if_vlog(ifp, pri, fmt, ap);
	va_end(ap);
	return (0);
}

void
if_start(struct ifnet *ifp)
{

	(*(ifp)->if_start)(ifp);
}

/*
 * Backwards compatibility interface for drivers 
 * that have not implemented it
 */
static int
if_transmit_default(struct ifnet *ifp, struct mbuf *m)
{
	int error;

	IFQ_HANDOFF(ifp, m, error);
	return (error);
}

static void
if_input_default(struct ifnet *ifp __unused, struct mbuf *m)
{
	m_freem(m);
}

int
if_handoff(struct ifqueue *ifq, struct mbuf *m, struct ifnet *ifp, int adjust)
{
	int active = 0;

	IF_LOCK(ifq);
	if (_IF_QFULL(ifq)) {
		IF_UNLOCK(ifq);
		if_inc_counter(ifp, IFCOUNTER_OQDROPS, 1);
		m_freem(m);
		return (0);
	}
	if (ifp != NULL) {
		if_inc_counter(ifp, IFCOUNTER_OBYTES, m->m_pkthdr.len + adjust);
		if (m->m_flags & (M_BCAST|M_MCAST))
			if_inc_counter(ifp, IFCOUNTER_OMCASTS, 1);
		active = ifp->if_drv_flags & IFF_DRV_OACTIVE;
	}
	_IF_ENQUEUE(ifq, m);
	IF_UNLOCK(ifq);
	if (ifp != NULL && !active)
		(*(ifp)->if_start)(ifp);
	return (1);
}

void
if_register_com_alloc(u_char type,
    if_com_alloc_t *a, if_com_free_t *f)
{

	KASSERT(if_com_alloc[type] == NULL,
	    ("if_register_com_alloc: %d already registered", type));
	KASSERT(if_com_free[type] == NULL,
	    ("if_register_com_alloc: %d free already registered", type));

	if_com_alloc[type] = a;
	if_com_free[type] = f;
}

void
if_deregister_com_alloc(u_char type)
{

	KASSERT(if_com_alloc[type] != NULL,
	    ("if_deregister_com_alloc: %d not registered", type));
	KASSERT(if_com_free[type] != NULL,
	    ("if_deregister_com_alloc: %d free not registered", type));

	/*
	 * Ensure all pending EPOCH(9) callbacks have been executed. This
	 * fixes issues about late invocation of if_destroy(), which leads
	 * to memory leak from if_com_alloc[type] allocated if_l2com.
	 */
	NET_EPOCH_DRAIN_CALLBACKS();

	if_com_alloc[type] = NULL;
	if_com_free[type] = NULL;
}

/* API for driver access to network stack owned ifnet.*/
uint64_t
if_setbaudrate(struct ifnet *ifp, uint64_t baudrate)
{
	uint64_t oldbrate;

	oldbrate = ifp->if_baudrate;
	ifp->if_baudrate = baudrate;
	return (oldbrate);
}

uint64_t
if_getbaudrate(const if_t ifp)
{
	return (ifp->if_baudrate);
}

int
if_setcapabilities(if_t ifp, int capabilities)
{
	ifp->if_capabilities = capabilities;
	return (0);
}

int
if_setcapabilitiesbit(if_t ifp, int setbit, int clearbit)
{
	ifp->if_capabilities &= ~clearbit;
	ifp->if_capabilities |= setbit;
	return (0);
}

int
if_getcapabilities(const if_t ifp)
{
	return (ifp->if_capabilities);
}

int 
if_setcapenable(if_t ifp, int capabilities)
{
	ifp->if_capenable = capabilities;
	return (0);
}

int 
if_setcapenablebit(if_t ifp, int setcap, int clearcap)
{
	ifp->if_capenable &= ~clearcap;
	ifp->if_capenable |= setcap;
	return (0);
}

int
if_setcapabilities2(if_t ifp, int capabilities)
{
	ifp->if_capabilities2 = capabilities;
	return (0);
}

int
if_setcapabilities2bit(if_t ifp, int setbit, int clearbit)
{
	ifp->if_capabilities2 &= ~clearbit;
	ifp->if_capabilities2 |= setbit;
	return (0);
}

int
if_getcapabilities2(const if_t ifp)
{
	return (ifp->if_capabilities2);
}

int
if_setcapenable2(if_t ifp, int capabilities2)
{
	ifp->if_capenable2 = capabilities2;
	return (0);
}

int
if_setcapenable2bit(if_t ifp, int setcap, int clearcap)
{
	ifp->if_capenable2 &= ~clearcap;
	ifp->if_capenable2 |= setcap;
	return (0);
}

const char *
if_getdname(const if_t ifp)
{
	return (ifp->if_dname);
}

void
if_setdname(if_t ifp, const char *dname)
{
	ifp->if_dname = dname;
}

const char *
if_name(if_t ifp)
{
	return (ifp->if_xname);
}

int
if_setname(if_t ifp, const char *name)
{
	if (strlen(name) > sizeof(ifp->if_xname) - 1)
		return (ENAMETOOLONG);
	strcpy(ifp->if_xname, name);

	return (0);
}

int 
if_togglecapenable(if_t ifp, int togglecap)
{
	ifp->if_capenable ^= togglecap;
	return (0);
}

int
if_getcapenable(const if_t ifp)
{
	return (ifp->if_capenable);
}

int
if_togglecapenable2(if_t ifp, int togglecap)
{
	ifp->if_capenable2 ^= togglecap;
	return (0);
}

int
if_getcapenable2(const if_t ifp)
{
	return (ifp->if_capenable2);
}

int
if_getdunit(const if_t ifp)
{
	return (ifp->if_dunit);
}

int
if_getindex(const if_t ifp)
{
	return (ifp->if_index);
}

int
if_getidxgen(const if_t ifp)
{
	return (ifp->if_idxgen);
}

void
if_setdescr(if_t ifp, char *descrbuf)
{
	sx_xlock(&ifdescr_sx);
	char *odescrbuf = ifp->if_description;
	ifp->if_description = descrbuf;
	sx_xunlock(&ifdescr_sx);

	if_freedescr(odescrbuf);
}

char *
if_allocdescr(size_t sz, int malloc_flag)
{
	malloc_flag &= (M_WAITOK | M_NOWAIT);
	return (malloc(sz, M_IFDESCR, M_ZERO | malloc_flag));
}

void
if_freedescr(char *descrbuf)
{
	free(descrbuf, M_IFDESCR);
}

int
if_getalloctype(const if_t ifp)
{
	return (ifp->if_alloctype);
}

/*
 * This is largely undesirable because it ties ifnet to a device, but does
 * provide flexiblity for an embedded product vendor. Should be used with
 * the understanding that it violates the interface boundaries, and should be
 * a last resort only.
 */
int
if_setdev(if_t ifp, void *dev)
{
	return (0);
}

int
if_setdrvflagbits(if_t ifp, int set_flags, int clear_flags)
{
	ifp->if_drv_flags &= ~clear_flags;
	ifp->if_drv_flags |= set_flags;

	return (0);
}

int
if_getdrvflags(const if_t ifp)
{
	return (ifp->if_drv_flags);
}

int
if_setdrvflags(if_t ifp, int flags)
{
	ifp->if_drv_flags = flags;
	return (0);
}

int
if_setflags(if_t ifp, int flags)
{
	ifp->if_flags = flags;
	return (0);
}

int
if_setflagbits(if_t ifp, int set, int clear)
{
	ifp->if_flags &= ~clear;
	ifp->if_flags |= set;
	return (0);
}

int
if_getflags(const if_t ifp)
{
	return (ifp->if_flags);
}

int
if_clearhwassist(if_t ifp)
{
	ifp->if_hwassist = 0;
	return (0);
}

int
if_sethwassistbits(if_t ifp, int toset, int toclear)
{
	ifp->if_hwassist &= ~toclear;
	ifp->if_hwassist |= toset;

	return (0);
}

int
if_sethwassist(if_t ifp, int hwassist_bit)
{
	ifp->if_hwassist = hwassist_bit;
	return (0);
}

int
if_gethwassist(const if_t ifp)
{
	return (ifp->if_hwassist);
}

int
if_togglehwassist(if_t ifp, int toggle_bits)
{
	ifp->if_hwassist ^= toggle_bits;
	return (0);
}

int
if_setmtu(if_t ifp, int mtu)
{
	ifp->if_mtu = mtu;
	return (0);
}

void
if_notifymtu(if_t ifp)
{
#ifdef INET6
	nd6_setmtu(ifp);
#endif
	rt_updatemtu(ifp);
}

int
if_getmtu(const if_t ifp)
{
	return (ifp->if_mtu);
}

int
if_getmtu_family(const if_t ifp, int family)
{
	struct domain *dp;

	SLIST_FOREACH(dp, &domains, dom_next) {
		if (dp->dom_family == family && dp->dom_ifmtu != NULL)
			return (dp->dom_ifmtu(ifp));
	}

	return (ifp->if_mtu);
}

/*
 * Methods for drivers to access interface unicast and multicast
 * link level addresses.  Driver shall not know 'struct ifaddr' neither
 * 'struct ifmultiaddr'.
 */
u_int
if_lladdr_count(if_t ifp)
{
	struct epoch_tracker et;
	struct ifaddr *ifa;
	u_int count;

	count = 0;
	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (ifa->ifa_addr->sa_family == AF_LINK)
			count++;
	NET_EPOCH_EXIT(et);

	return (count);
}

int
if_foreach(if_foreach_cb_t cb, void *cb_arg)
{
	if_t ifp;
	int error;

	NET_EPOCH_ASSERT();
	MPASS(cb);

	error = 0;
	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		error = cb(ifp, cb_arg);
		if (error != 0)
			break;
	}

	return (error);
}

/*
 * Iterates over the list of interfaces, permitting callback function @cb to sleep.
 * Stops iteration if @cb returns non-zero error code.
 * Returns the last error code from @cb.
 * @match_cb: optional match callback limiting the iteration to only matched interfaces
 * @match_arg: argument to pass to @match_cb
 * @cb: iteration callback
 * @cb_arg: argument to pass to @cb
 */
int
if_foreach_sleep(if_foreach_match_t match_cb, void *match_arg, if_foreach_cb_t cb,
    void *cb_arg)
{
	int match_count = 0, array_size = 16; /* 128 bytes for malloc */
	struct ifnet **match_array = NULL;
	int error = 0;

	MPASS(cb);

	while (true) {
		struct ifnet **new_array;
		int new_size = array_size;
		struct epoch_tracker et;
		struct ifnet *ifp;

		while (new_size < match_count)
			new_size *= 2;
		new_array = malloc(new_size * sizeof(void *), M_TEMP, M_WAITOK);
		if (match_array != NULL)
			memcpy(new_array, match_array, array_size * sizeof(void *));
		free(match_array, M_TEMP);
		match_array = new_array;
		array_size = new_size;

		match_count = 0;
		NET_EPOCH_ENTER(et);
		CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
			if (match_cb != NULL && !match_cb(ifp, match_arg))
				continue;
			if (match_count < array_size) {
				if (if_try_ref(ifp))
					match_array[match_count++] = ifp;
			} else
				match_count++;
		}
		NET_EPOCH_EXIT(et);

		if (match_count > array_size) {
			for (int i = 0; i < array_size; i++)
				if_rele(match_array[i]);
			continue;
		} else {
			for (int i = 0; i < match_count; i++) {
				if (error == 0)
					error = cb(match_array[i], cb_arg);
				if_rele(match_array[i]);
			}
			free(match_array, M_TEMP);
			break;
		}
	}

	return (error);
}


/*
 * Uses just 1 pointer of the 4 available in the public struct.
 */
if_t
if_iter_start(struct if_iter *iter)
{
	if_t ifp;

	NET_EPOCH_ASSERT();

	bzero(iter, sizeof(*iter));
	ifp = CK_STAILQ_FIRST(&V_ifnet);
	if (ifp != NULL)
		iter->context[0] = CK_STAILQ_NEXT(ifp, if_link);
	else
		iter->context[0] = NULL;
	return (ifp);
}

if_t
if_iter_next(struct if_iter *iter)
{
	if_t cur_ifp = iter->context[0];

	if (cur_ifp != NULL)
		iter->context[0] = CK_STAILQ_NEXT(cur_ifp, if_link);
	return (cur_ifp);
}

void
if_iter_finish(struct if_iter *iter)
{
	/* Nothing to do here for now. */
}

u_int
if_foreach_lladdr(if_t ifp, iflladdr_cb_t cb, void *cb_arg)
{
	struct epoch_tracker et;
	struct ifaddr *ifa;
	u_int count;

	MPASS(cb);

	count = 0;
	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		count += (*cb)(cb_arg, (struct sockaddr_dl *)ifa->ifa_addr,
		    count);
	}
	NET_EPOCH_EXIT(et);

	return (count);
}

u_int
if_llmaddr_count(if_t ifp)
{
	struct epoch_tracker et;
	struct ifmultiaddr *ifma;
	int count;

	count = 0;
	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
		if (ifma->ifma_addr->sa_family == AF_LINK)
			count++;
	NET_EPOCH_EXIT(et);

	return (count);
}

bool
if_maddr_empty(if_t ifp)
{

	return (CK_STAILQ_EMPTY(&ifp->if_multiaddrs));
}

u_int
if_foreach_llmaddr(if_t ifp, iflladdr_cb_t cb, void *cb_arg)
{
	struct epoch_tracker et;
	struct ifmultiaddr *ifma;
	u_int count;

	MPASS(cb);

	count = 0;
	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		count += (*cb)(cb_arg, (struct sockaddr_dl *)ifma->ifma_addr,
		    count);
	}
	NET_EPOCH_EXIT(et);

	return (count);
}

u_int
if_foreach_addr_type(if_t ifp, int type, if_addr_cb_t cb, void *cb_arg)
{
	struct epoch_tracker et;
	struct ifaddr *ifa;
	u_int count;

	MPASS(cb);

	count = 0;
	NET_EPOCH_ENTER(et);
	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr->sa_family != type)
			continue;
		count += (*cb)(cb_arg, ifa, count);
	}
	NET_EPOCH_EXIT(et);

	return (count);
}

int
if_setsoftc(if_t ifp, void *softc)
{
	ifp->if_softc = softc;
	return (0);
}

void *
if_getsoftc(const if_t ifp)
{
	return (ifp->if_softc);
}

void 
if_setrcvif(struct mbuf *m, if_t ifp)
{

	MPASS((m->m_pkthdr.csum_flags & CSUM_SND_TAG) == 0);
	m->m_pkthdr.rcvif = (struct ifnet *)ifp;
}

void 
if_setvtag(struct mbuf *m, uint16_t tag)
{
	m->m_pkthdr.ether_vtag = tag;	
}

uint16_t
if_getvtag(struct mbuf *m)
{
	return (m->m_pkthdr.ether_vtag);
}

int
if_sendq_empty(if_t ifp)
{
	return (IFQ_DRV_IS_EMPTY(&ifp->if_snd));
}

struct ifaddr *
if_getifaddr(const if_t ifp)
{
	return (ifp->if_addr);
}

int
if_getamcount(const if_t ifp)
{
	return (ifp->if_amcount);
}

int
if_setsendqready(if_t ifp)
{
	IFQ_SET_READY(&ifp->if_snd);
	return (0);
}

int
if_setsendqlen(if_t ifp, int tx_desc_count)
{
	IFQ_SET_MAXLEN(&ifp->if_snd, tx_desc_count);
	ifp->if_snd.ifq_drv_maxlen = tx_desc_count;
	return (0);
}

void
if_setnetmapadapter(if_t ifp, struct netmap_adapter *na)
{
	ifp->if_netmap = na;
}

struct netmap_adapter *
if_getnetmapadapter(if_t ifp)
{
	return (ifp->if_netmap);
}

int
if_vlantrunkinuse(if_t ifp)
{
	return (ifp->if_vlantrunk != NULL);
}

void
if_init(if_t ifp, void *ctx)
{
	(*ifp->if_init)(ctx);
}

void
if_input(if_t ifp, struct mbuf* sendmp)
{
	(*ifp->if_input)(ifp, sendmp);
}

int
if_transmit(if_t ifp, struct mbuf *m)
{
	return ((*ifp->if_transmit)(ifp, m));
}

int
if_resolvemulti(if_t ifp, struct sockaddr **srcs, struct sockaddr *dst)
{
	if (ifp->if_resolvemulti == NULL)
		return (EOPNOTSUPP);

	return (ifp->if_resolvemulti(ifp, srcs, dst));
}

struct mbuf *
if_dequeue(if_t ifp)
{
	struct mbuf *m;

	IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
	return (m);
}

int
if_sendq_prepend(if_t ifp, struct mbuf *m)
{
	IFQ_DRV_PREPEND(&ifp->if_snd, m);
	return (0);
}

int
if_setifheaderlen(if_t ifp, int len)
{
	ifp->if_hdrlen = len;
	return (0);
}

caddr_t
if_getlladdr(const if_t ifp)
{
	return (IF_LLADDR(ifp));
}

void *
if_gethandle(u_char type)
{
	return (if_alloc(type));
}

void
if_bpfmtap(if_t ifp, struct mbuf *m)
{
	BPF_MTAP(ifp, m);
}

void
if_etherbpfmtap(if_t ifp, struct mbuf *m)
{
	ETHER_BPF_MTAP(ifp, m);
}

void
if_vlancap(if_t ifp)
{
	VLAN_CAPABILITIES(ifp);
}

int
if_sethwtsomax(if_t ifp, u_int if_hw_tsomax)
{
	ifp->if_hw_tsomax = if_hw_tsomax;
        return (0);
}

int
if_sethwtsomaxsegcount(if_t ifp, u_int if_hw_tsomaxsegcount)
{
	ifp->if_hw_tsomaxsegcount = if_hw_tsomaxsegcount;
        return (0);
}

int
if_sethwtsomaxsegsize(if_t ifp, u_int if_hw_tsomaxsegsize)
{
	ifp->if_hw_tsomaxsegsize = if_hw_tsomaxsegsize;
        return (0);
}

u_int
if_gethwtsomax(const if_t ifp)
{
	return (ifp->if_hw_tsomax);
}

u_int
if_gethwtsomaxsegcount(const if_t ifp)
{
	return (ifp->if_hw_tsomaxsegcount);
}

u_int
if_gethwtsomaxsegsize(const if_t ifp)
{
	return (ifp->if_hw_tsomaxsegsize);
}

void
if_setinitfn(if_t ifp, if_init_fn_t init_fn)
{
	ifp->if_init = init_fn;
}

void
if_setinputfn(if_t ifp, if_input_fn_t input_fn)
{
	ifp->if_input = input_fn;
}

if_input_fn_t
if_getinputfn(if_t ifp)
{
	return (ifp->if_input);
}

void
if_setioctlfn(if_t ifp, if_ioctl_fn_t ioctl_fn)
{
	ifp->if_ioctl = ioctl_fn;
}

void
if_setoutputfn(if_t ifp, if_output_fn_t output_fn)
{
	ifp->if_output = output_fn;
}

void
if_setstartfn(if_t ifp, if_start_fn_t start_fn)
{
	ifp->if_start = start_fn;
}

if_start_fn_t
if_getstartfn(if_t ifp)
{
	return (ifp->if_start);
}

void
if_settransmitfn(if_t ifp, if_transmit_fn_t start_fn)
{
	ifp->if_transmit = start_fn;
}

if_transmit_fn_t
if_gettransmitfn(if_t ifp)
{
	return (ifp->if_transmit);
}

void
if_setqflushfn(if_t ifp, if_qflush_fn_t flush_fn)
{
	ifp->if_qflush = flush_fn;
}

void
if_setsndtagallocfn(if_t ifp, if_snd_tag_alloc_t alloc_fn)
{
	ifp->if_snd_tag_alloc = alloc_fn;
}

int
if_snd_tag_alloc(if_t ifp, union if_snd_tag_alloc_params *params,
    struct m_snd_tag **mstp)
{
	if (ifp->if_snd_tag_alloc == NULL)
		return (EOPNOTSUPP);
	return (ifp->if_snd_tag_alloc(ifp, params, mstp));
}

void
if_setgetcounterfn(if_t ifp, if_get_counter_t fn)
{
	ifp->if_get_counter = fn;
}

void
if_setreassignfn(if_t ifp, if_reassign_fn_t fn)
{
	ifp->if_reassign = fn;
}

void
if_setratelimitqueryfn(if_t ifp, if_ratelimit_query_t fn)
{
	ifp->if_ratelimit_query = fn;
}

void
if_setdebugnet_methods(if_t ifp, struct debugnet_methods *m)
{
	ifp->if_debugnet_methods = m;
}

struct label *
if_getmaclabel(if_t ifp)
{
	return (ifp->if_label);
}

void
if_setmaclabel(if_t ifp, struct label *label)
{
	ifp->if_label = label;
}

int
if_gettype(if_t ifp)
{
	return (ifp->if_type);
}

void *
if_getllsoftc(if_t ifp)
{
	return (ifp->if_llsoftc);
}

void
if_setllsoftc(if_t ifp, void *llsoftc)
{
	ifp->if_llsoftc = llsoftc;
};

int
if_getlinkstate(if_t ifp)
{
	return (ifp->if_link_state);
}

const uint8_t *
if_getbroadcastaddr(if_t ifp)
{
	return (ifp->if_broadcastaddr);
}

void
if_setbroadcastaddr(if_t ifp, const uint8_t *addr)
{
	ifp->if_broadcastaddr = addr;
}

int
if_getnumadomain(if_t ifp)
{
	return (ifp->if_numa_domain);
}

uint64_t
if_getcounter(if_t ifp, ift_counter counter)
{
	return (ifp->if_get_counter(ifp, counter));
}

bool
if_altq_is_enabled(if_t ifp)
{
	return (ALTQ_IS_ENABLED(&ifp->if_snd));
}

struct vnet *
if_getvnet(if_t ifp)
{
	return (ifp->if_vnet);
}

void *
if_getafdata(if_t ifp, int af)
{
	return (ifp->if_afdata[af]);
}

u_int
if_getfib(if_t ifp)
{
	return (ifp->if_fib);
}

uint8_t
if_getaddrlen(if_t ifp)
{
	return (ifp->if_addrlen);
}

struct bpf_if *
if_getbpf(if_t ifp)
{
	return (ifp->if_bpf);
}

struct ifvlantrunk *
if_getvlantrunk(if_t ifp)
{
	return (ifp->if_vlantrunk);
}

uint8_t
if_getpcp(if_t ifp)
{
	return (ifp->if_pcp);
}

void *
if_getl2com(if_t ifp)
{
	return (ifp->if_l2com);
}

#ifdef DDB
static void
if_show_ifnet(struct ifnet *ifp)
{
	if (ifp == NULL)
		return;
	db_printf("%s:\n", ifp->if_xname);
#define	IF_DB_PRINTF(f, e)	db_printf("   %s = " f "\n", #e, ifp->e);
	IF_DB_PRINTF("%s", if_dname);
	IF_DB_PRINTF("%d", if_dunit);
	IF_DB_PRINTF("%s", if_description);
	IF_DB_PRINTF("%u", if_index);
	IF_DB_PRINTF("%d", if_idxgen);
	IF_DB_PRINTF("%u", if_refcount);
	IF_DB_PRINTF("%p", if_softc);
	IF_DB_PRINTF("%p", if_l2com);
	IF_DB_PRINTF("%p", if_llsoftc);
	IF_DB_PRINTF("%d", if_amcount);
	IF_DB_PRINTF("%p", if_addr);
	IF_DB_PRINTF("%p", if_broadcastaddr);
	IF_DB_PRINTF("%p", if_afdata);
	IF_DB_PRINTF("%d", if_afdata_initialized);
	IF_DB_PRINTF("%u", if_fib);
	IF_DB_PRINTF("%p", if_vnet);
	IF_DB_PRINTF("%p", if_home_vnet);
	IF_DB_PRINTF("%p", if_vlantrunk);
	IF_DB_PRINTF("%p", if_bpf);
	IF_DB_PRINTF("%u", if_pcount);
	IF_DB_PRINTF("%p", if_bridge);
	IF_DB_PRINTF("%p", if_lagg);
	IF_DB_PRINTF("%p", if_pf_kif);
	IF_DB_PRINTF("%p", if_carp);
	IF_DB_PRINTF("%p", if_label);
	IF_DB_PRINTF("%p", if_netmap);
	IF_DB_PRINTF("0x%08x", if_flags);
	IF_DB_PRINTF("0x%08x", if_drv_flags);
	IF_DB_PRINTF("0x%08x", if_capabilities);
	IF_DB_PRINTF("0x%08x", if_capenable);
	IF_DB_PRINTF("%p", if_snd.ifq_head);
	IF_DB_PRINTF("%p", if_snd.ifq_tail);
	IF_DB_PRINTF("%d", if_snd.ifq_len);
	IF_DB_PRINTF("%d", if_snd.ifq_maxlen);
	IF_DB_PRINTF("%p", if_snd.ifq_drv_head);
	IF_DB_PRINTF("%p", if_snd.ifq_drv_tail);
	IF_DB_PRINTF("%d", if_snd.ifq_drv_len);
	IF_DB_PRINTF("%d", if_snd.ifq_drv_maxlen);
	IF_DB_PRINTF("%d", if_snd.altq_type);
	IF_DB_PRINTF("%x", if_snd.altq_flags);
#undef IF_DB_PRINTF
}

DB_SHOW_COMMAND(ifnet, db_show_ifnet)
{
	if (!have_addr) {
		db_printf("usage: show ifnet <struct ifnet *>\n");
		return;
	}

	if_show_ifnet((struct ifnet *)addr);
}

DB_SHOW_ALL_COMMAND(ifnets, db_show_all_ifnets)
{
	struct ifnet *ifp;
	u_short idx;

	for (idx = 1; idx <= if_index; idx++) {
		ifp = ifindex_table[idx].ife_ifnet;
		if (ifp == NULL)
			continue;
		db_printf( "%20s ifp=%p\n", ifp->if_xname, ifp);
		if (db_pager_quit)
			break;
	}
}
#endif	/* DDB */
