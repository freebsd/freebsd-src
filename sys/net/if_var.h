/*-
 * Copyright (c) 1982, 1986, 1989, 1993
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
 * 4. Neither the name of the University nor the names of its contributors
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
 *	From: @(#)if.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

#ifndef	_NET_IF_VAR_H_
#define	_NET_IF_VAR_H_

struct	rtentry;		/* ifa_rtrequest */
struct	rt_addrinfo;		/* ifa_rtrequest */
struct	socket;
struct	carp_if;
struct	carp_softc;
struct  ifvlantrunk;
struct	ifmedia;
struct	netmap_adapter;

#ifdef _KERNEL
#include <sys/mbuf.h>		/* ifqueue only? */
#include <sys/buf_ring.h>
#include <net/vnet.h>
#endif /* _KERNEL */
#include <sys/counter.h>
#include <sys/lock.h>		/* XXX */
#include <sys/mutex.h>		/* struct ifqueue */
#include <sys/rwlock.h>		/* XXX */
#include <sys/sx.h>		/* XXX */
#include <sys/_task.h>		/* if_link_task */
#include <altq/if_altq.h>

TAILQ_HEAD(ifnethead, ifnet);	/* we use TAILQs so that the order of */
TAILQ_HEAD(ifaddrhead, ifaddr);	/* instantiation is preserved in the list */
TAILQ_HEAD(ifmultihead, ifmultiaddr);
TAILQ_HEAD(ifgrouphead, ifg_group);

#ifdef _KERNEL
VNET_DECLARE(struct pfil_head, link_pfil_hook);	/* packet filter hooks */
#define	V_link_pfil_hook	VNET(link_pfil_hook)
#endif /* _KERNEL */

typedef	void (*iftype_attach_t)(if_t ifp, struct if_attach_args *args);
typedef	void (*iftype_detach_t)(if_t ifp);
struct iftype {
	const ifType		ift_type;
	SLIST_ENTRY(iftype)	ift_next;
	iftype_attach_t		ift_attach;
	iftype_detach_t		ift_detach;
	uint8_t			ift_hdrlen;
	uint8_t			ift_addrlen;
	uint32_t		ift_dlt;
	uint32_t		ift_dlt_hdrlen;
	struct ifops		ift_ops;
};

/*
 * Structure defining a network interface.
 *
 * (Would like to call this struct ``if'', but C isn't PL/1.)
 */
struct ifnet {
	struct ifops	*if_ops;	/* driver ops (or overridden) */
	void		*if_softc;	/* driver soft state */
	struct ifdriver	*if_drv;	/* driver static definition */
	struct iftype	*if_type;	/* if type static def (optional)*/
	struct iftsomax	*if_tsomax;	/* TSO limits */
	
	/* General book keeping of interface lists. */
	TAILQ_ENTRY(ifnet) if_link; 	/* all struct ifnets are chained */
	LIST_ENTRY(ifnet) if_clones;	/* interfaces of a cloner */
	TAILQ_HEAD(, ifg_list) if_groups; /* linked list of groups per if */
					/* protected by if_addr_lock */
	void	*if_llsoftc;		/* link layer softc */
	void	*if_l2com;		/* pointer to protocol bits */
	int	if_dunit;		/* unit or IF_DUNIT_NONE */
	u_short	if_index;		/* numeric abbreviation for this if  */
	short	if_index_reserved;	/* spare space to grow if_index */
	char	if_xname[IFNAMSIZ];	/* external name (name + unit) */
	char	*if_description;	/* interface description */

	/* Variable fields that are touched by the stack and drivers. */
	uint32_t	if_flags;	/* up/down, broadcast, etc. */
	uint32_t	if_capabilities;/* interface features & capabilities */
	uint32_t	if_capenable;	/* enabled features & capabilities */
	void		*if_linkmib;	/* link-type-specific MIB data */
	size_t		if_linkmiblen;	/* length of above data */
	u_int		if_refcount;	/* reference count */
	u_int		if_fib;		/* interface FIB */

	uint8_t		if_link_state;	/* current link state */
	uint32_t	if_mtu;		/* maximum transmission unit */
	uint32_t	if_metric;	/* routing metric (external only) */
	uint64_t	if_baudrate;	/* linespeed */
	uint64_t	if_hwassist;	/* HW offload capabilities, see IFCAP */
	time_t		if_epoch;	/* uptime at attach or stat reset */
	struct timeval	if_lastchange;	/* time of last administrative change */

	struct	task if_linktask;	/* task for link change events */

	/* Addresses of different protocol families assigned to this if. */
	struct	rwlock if_addr_lock;	/* lock to protect address lists */
		/*
		 * if_addrhead is the list of all addresses associated to
		 * an interface.
		 * Some code in the kernel assumes that first element
		 * of the list has type AF_LINK, and contains sockaddr_dl
		 * addresses which store the link-level address and the name
		 * of the interface.
		 * However, access to the AF_LINK address through this
		 * field is deprecated. Use if_addr or ifaddr_byindex() instead.
		 */
	struct	ifaddrhead if_addrhead;	/* linked list of addresses per if */
	struct	ifmultihead if_multiaddrs; /* multicast addresses configured */
	int	if_amcount;		/* number of all-multicast requests */
	struct	ifaddr	*if_addr;	/* pointer to link-level address */
	const u_int8_t *if_broadcastaddr; /* linklevel broadcast bytestring */
	struct	rwlock if_afdata_lock;
	void	*if_afdata[AF_MAX];
	int	if_afdata_initialized;

	/* Additional features hung off the interface. */
	struct	ifqueue *if_snd;	/* software send queue */
	struct	vnet *if_vnet;		/* pointer to network stack instance */
	struct	vnet *if_home_vnet;	/* where this ifnet originates from */
	struct  ifvlantrunk *if_vlantrunk; /* pointer to 802.1q data */
	struct	bpf_if *if_bpf;		/* packet filter structure */
	int	if_pcount;		/* number of promiscuous listeners */
	void	*if_bridge;		/* bridge glue */
	void	*if_lagg;		/* lagg glue */
	void	*if_pf_kif;		/* pf glue */
	struct	carp_if *if_carp;	/* carp interface structure */
	struct	label *if_label;	/* interface MAC label */
	struct	netmap_adapter *if_netmap; /* netmap(4) softc */

	counter_u64_t	if_counters[IFCOUNTERS];	/* Statistics */

	/*
	 * Spare fields to be added before branching a stable branch, so
	 * that structure can be enhanced without changing the kernel
	 * binary interface.
	 */
};

/*
 * Locks for address lists on the network interface.
 */
#define	IF_ADDR_LOCK_INIT(if)	rw_init(&(if)->if_addr_lock, "if_addr_lock")
#define	IF_ADDR_LOCK_DESTROY(if)	rw_destroy(&(if)->if_addr_lock)
#define	IF_ADDR_WLOCK(if)	rw_wlock(&(if)->if_addr_lock)
#define	IF_ADDR_WUNLOCK(if)	rw_wunlock(&(if)->if_addr_lock)
#define	IF_ADDR_RLOCK(if)	rw_rlock(&(if)->if_addr_lock)
#define	IF_ADDR_RUNLOCK(if)	rw_runlock(&(if)->if_addr_lock)
#define	IF_ADDR_LOCK_ASSERT(if)	rw_assert(&(if)->if_addr_lock, RA_LOCKED)
#define	IF_ADDR_WLOCK_ASSERT(if) rw_assert(&(if)->if_addr_lock, RA_WLOCKED)

#ifdef _KERNEL
#ifdef _SYS_EVENTHANDLER_H_
/* interface link layer address change event */
typedef void (*iflladdr_event_handler_t)(void *, struct ifnet *);
EVENTHANDLER_DECLARE(iflladdr_event, iflladdr_event_handler_t);
/* interface address change event */
typedef void (*ifaddr_event_handler_t)(void *, struct ifnet *);
EVENTHANDLER_DECLARE(ifaddr_event, ifaddr_event_handler_t);
/* new interface arrival event */
typedef void (*ifnet_arrival_event_handler_t)(void *, struct ifnet *);
EVENTHANDLER_DECLARE(ifnet_arrival_event, ifnet_arrival_event_handler_t);
/* interface departure event */
typedef void (*ifnet_departure_event_handler_t)(void *, struct ifnet *);
EVENTHANDLER_DECLARE(ifnet_departure_event, ifnet_departure_event_handler_t);
/* Interface link state change event */
typedef void (*ifnet_link_event_handler_t)(void *, struct ifnet *, int);
EVENTHANDLER_DECLARE(ifnet_link_event, ifnet_link_event_handler_t);
#endif /* _SYS_EVENTHANDLER_H_ */

/*
 * interface groups
 */
struct ifg_group {
	char				 ifg_group[IFNAMSIZ];
	u_int				 ifg_refcnt;
	void				*ifg_pf_kif;
	TAILQ_HEAD(, ifg_member)	 ifg_members;
	TAILQ_ENTRY(ifg_group)		 ifg_next;
};

struct ifg_member {
	TAILQ_ENTRY(ifg_member)	 ifgm_next;
	struct ifnet		*ifgm_ifp;
};

struct ifg_list {
	struct ifg_group	*ifgl_group;
	TAILQ_ENTRY(ifg_list)	 ifgl_next;
};

#ifdef _SYS_EVENTHANDLER_H_
/* group attach event */
typedef void (*group_attach_event_handler_t)(void *, struct ifg_group *);
EVENTHANDLER_DECLARE(group_attach_event, group_attach_event_handler_t);
/* group detach event */
typedef void (*group_detach_event_handler_t)(void *, struct ifg_group *);
EVENTHANDLER_DECLARE(group_detach_event, group_detach_event_handler_t);
/* group change event */
typedef void (*group_change_event_handler_t)(void *, const char *);
EVENTHANDLER_DECLARE(group_change_event, group_change_event_handler_t);
#endif /* _SYS_EVENTHANDLER_H_ */

#define	IF_AFDATA_LOCK_INIT(ifp)	\
	rw_init(&(ifp)->if_afdata_lock, "if_afdata")

#define	IF_AFDATA_WLOCK(ifp)	rw_wlock(&(ifp)->if_afdata_lock)
#define	IF_AFDATA_RLOCK(ifp)	rw_rlock(&(ifp)->if_afdata_lock)
#define	IF_AFDATA_WUNLOCK(ifp)	rw_wunlock(&(ifp)->if_afdata_lock)
#define	IF_AFDATA_RUNLOCK(ifp)	rw_runlock(&(ifp)->if_afdata_lock)
#define	IF_AFDATA_LOCK(ifp)	IF_AFDATA_WLOCK(ifp)
#define	IF_AFDATA_UNLOCK(ifp)	IF_AFDATA_WUNLOCK(ifp)
#define	IF_AFDATA_TRYLOCK(ifp)	rw_try_wlock(&(ifp)->if_afdata_lock)
#define	IF_AFDATA_DESTROY(ifp)	rw_destroy(&(ifp)->if_afdata_lock)

#define	IF_AFDATA_LOCK_ASSERT(ifp)	rw_assert(&(ifp)->if_afdata_lock, RA_LOCKED)
#define	IF_AFDATA_RLOCK_ASSERT(ifp)	rw_assert(&(ifp)->if_afdata_lock, RA_RLOCKED)
#define	IF_AFDATA_WLOCK_ASSERT(ifp)	rw_assert(&(ifp)->if_afdata_lock, RA_WLOCKED)
#define	IF_AFDATA_UNLOCK_ASSERT(ifp)	rw_assert(&(ifp)->if_afdata_lock, RA_UNLOCKED)

/*
 * 72 was chosen below because it is the size of a TCP/IP
 * header (40) + the minimum mss (32).
 */
#define	IF_MINMTU	72
#define	IF_MAXMTU	65535

#define	TOEDEV(ifp)	((ifp)->if_llsoftc)

#endif /* _KERNEL */

/*
 * The ifaddr structure contains information about one address
 * of an interface.  They are maintained by the different address families,
 * are allocated and attached when an address is set, and are linked
 * together so all addresses for an interface can be located.
 *
 * NOTE: a 'struct ifaddr' is always at the beginning of a larger
 * chunk of malloc'ed memory, where we store the three addresses
 * (ifa_addr, ifa_dstaddr and ifa_netmask) referenced here.
 */
#if defined(_KERNEL) || defined(_WANT_IFADDR)
struct ifaddr {
	struct	sockaddr *ifa_addr;	/* address of interface */
	struct	sockaddr *ifa_dstaddr;	/* other end of p-to-p link */
#define	ifa_broadaddr	ifa_dstaddr	/* broadcast address interface */
	struct	sockaddr *ifa_netmask;	/* used to determine subnet */
	struct	ifnet *ifa_ifp;		/* back-pointer to interface */
	struct	carp_softc *ifa_carp;	/* pointer to CARP data */
	TAILQ_ENTRY(ifaddr) ifa_link;	/* queue macro glue */
	void	(*ifa_rtrequest)	/* check or clean routes (+ or -)'d */
		(int, struct rtentry *, struct rt_addrinfo *);
	u_short	ifa_flags;		/* mostly rt_flags for cloning */
	u_int	ifa_refcnt;		/* references to this structure */

	counter_u64_t	ifa_ipackets;
	counter_u64_t	ifa_opackets;	 
	counter_u64_t	ifa_ibytes;
	counter_u64_t	ifa_obytes;
};
#endif

#ifdef _KERNEL
#define	IFA_ROUTE	RTF_UP		/* route installed */
#define	IFA_RTSELF	RTF_HOST	/* loopback route to self installed */

/* For compatibility with other BSDs. SCTP uses it. */
#define	ifa_list	ifa_link

struct ifaddr *	ifa_alloc(size_t size, int flags);
void	ifa_free(struct ifaddr *ifa);
void	ifa_ref(struct ifaddr *ifa);
#endif /* _KERNEL */

/*
 * Multicast address structure.  This is analogous to the ifaddr
 * structure except that it keeps track of multicast addresses.
 */
struct ifmultiaddr {
	TAILQ_ENTRY(ifmultiaddr) ifma_link; /* queue macro glue */
	struct	sockaddr *ifma_addr; 	/* address this membership is for */
	struct	sockaddr *ifma_lladdr;	/* link-layer translation, if any */
	struct	ifnet *ifma_ifp;	/* back-pointer to interface */
	u_int	ifma_refcount;		/* reference count */
	void	*ifma_protospec;	/* protocol-specific state, if any */
	struct	ifmultiaddr *ifma_llifma; /* pointer to ifma for ifma_lladdr */
};

#ifdef _KERNEL

extern	struct rwlock ifnet_rwlock;
extern	struct sx ifnet_sxlock;

#define	IFNET_WLOCK() do {						\
	sx_xlock(&ifnet_sxlock);					\
	rw_wlock(&ifnet_rwlock);					\
} while (0)

#define	IFNET_WUNLOCK() do {						\
	rw_wunlock(&ifnet_rwlock);					\
	sx_xunlock(&ifnet_sxlock);					\
} while (0)

/*
 * To assert the ifnet lock, you must know not only whether it's for read or
 * write, but also whether it was acquired with sleep support or not.
 */
#define	IFNET_RLOCK_ASSERT()		sx_assert(&ifnet_sxlock, SA_SLOCKED)
#define	IFNET_RLOCK_NOSLEEP_ASSERT()	rw_assert(&ifnet_rwlock, RA_RLOCKED)
#define	IFNET_WLOCK_ASSERT() do {					\
	sx_assert(&ifnet_sxlock, SA_XLOCKED);				\
	rw_assert(&ifnet_rwlock, RA_WLOCKED);				\
} while (0)

#define	IFNET_RLOCK()		sx_slock(&ifnet_sxlock)
#define	IFNET_RLOCK_NOSLEEP()	rw_rlock(&ifnet_rwlock)
#define	IFNET_RUNLOCK()		sx_sunlock(&ifnet_sxlock)
#define	IFNET_RUNLOCK_NOSLEEP()	rw_runlock(&ifnet_rwlock)

/*
 * Look up an ifnet given its index; the _ref variant also acquires a
 * reference that must be freed using if_rele().  It is almost always a bug
 * to call ifnet_byindex() instead if ifnet_byindex_ref().
 */
struct ifnet	*ifnet_byindex(u_short idx);
struct ifnet	*ifnet_byindex_locked(u_short idx);
struct ifnet	*ifnet_byindex_ref(u_short idx);

/*
 * Given the index, ifaddr_byindex() returns the one and only
 * link-level ifaddr for the interface. You are not supposed to use
 * it to traverse the list of addresses associated to the interface.
 */
struct ifaddr	*ifaddr_byindex(u_short idx);

VNET_DECLARE(struct ifnethead, ifnet);
VNET_DECLARE(struct ifgrouphead, ifg_head);
VNET_DECLARE(int, if_index);
VNET_DECLARE(struct ifnet *, loif);	/* first loopback interface */

#define	V_ifnet		VNET(ifnet)
#define	V_ifg_head	VNET(ifg_head)
#define	V_if_index	VNET(if_index)
#define	V_loif		VNET(loif)

int	if_addgroup(struct ifnet *, const char *);
int	if_delgroup(struct ifnet *, const char *);
int	if_addmulti(struct ifnet *, struct sockaddr *, struct ifmultiaddr **);
int	if_allmulti(struct ifnet *, int);
int	if_delmulti(struct ifnet *, struct sockaddr *);
void	if_delmulti_ifma(struct ifmultiaddr *);
void	if_vmove(struct ifnet *, struct vnet *);
void	if_purgeaddrs(struct ifnet *);
void	if_delallmulti(struct ifnet *);
void	if_down(struct ifnet *);
struct ifmultiaddr *
	if_findmulti(struct ifnet *, struct sockaddr *);
void	if_ref(struct ifnet *);
void	if_rele(struct ifnet *);
int	if_setlladdr(struct ifnet *, const u_char *, int);
void	if_up(struct ifnet *);
int	ifioctl(struct socket *, u_long, caddr_t, struct thread *);
int	ifpromisc(struct ifnet *, int);
struct	ifnet *ifunit(const char *);
struct	ifnet *ifunit_ref(const char *);

void	iftype_register(struct iftype *);
void	iftype_unregister(struct iftype *);

int	ifa_add_loopback_route(struct ifaddr *, struct sockaddr *);
int	ifa_del_loopback_route(struct ifaddr *, struct sockaddr *);
int	ifa_switch_loopback_route(struct ifaddr *, struct sockaddr *, int fib);

struct	ifaddr *ifa_ifwithaddr(struct sockaddr *);
int		ifa_ifwithaddr_check(struct sockaddr *);
struct	ifaddr *ifa_ifwithbroadaddr(struct sockaddr *, int);
struct	ifaddr *ifa_ifwithdstaddr(struct sockaddr *, int);
struct	ifaddr *ifa_ifwithnet(struct sockaddr *, int, int);
struct	ifaddr *ifa_ifwithroute(int, struct sockaddr *, struct sockaddr *, u_int);
struct	ifaddr *ifaof_ifpforaddr(struct sockaddr *, struct ifnet *);
int	ifa_preferred(struct ifaddr *, struct ifaddr *);

int	if_simloop(struct ifnet *ifp, struct mbuf *m, int af, int hlen);

void	if_data_copy(struct ifnet *, struct if_data *);
int	if_getmtu_family(if_t ifp, int family);

int if_setupmultiaddr(if_t ifp, void *mta, int *cnt, int max);
int if_multiaddr_array(if_t ifp, void *mta, int *cnt, int max);
int if_multiaddr_count(if_t ifp, int max);

/* TSO */
void if_tsomax_common(const struct iftsomax *, struct iftsomax *);
int if_tsomax_update(if_t ifp, const struct iftsomax *);

#ifdef DEVICE_POLLING
enum poll_cmd { POLL_ONLY, POLL_AND_CHECK_STATUS };

typedef	int poll_handler_t(if_t ifp, enum poll_cmd cmd, int count);
int    ether_poll_register(poll_handler_t *h, if_t ifp);
int    ether_poll_deregister(if_t ifp);
#endif /* DEVICE_POLLING */

/*
 * Wrappers around ifops. Some ops are optional and can be NULL,
 * others are mandatory.  Those wrappers that driver can invoke
 * theirselves are not inlined, but implemented in if.c.
 */
static inline void
if_init(if_t ifp, void *sc)
{

	if (ifp->if_ops->ifop_init != NULL)
		return (ifp->if_ops->ifop_init(sc));
}

static inline int
if_transmit(if_t ifp, struct mbuf *m)
{

	return (ifp->if_ops->ifop_transmit(ifp, m));
}

static inline void
if_qflush(if_t ifp)
{

	if (ifp->if_ops->ifop_qflush != NULL)
		ifp->if_ops->ifop_qflush(ifp);
}

static inline int
if_output(if_t ifp, struct mbuf *m, const struct sockaddr *dst,
    struct route *ro)
{

	return (ifp->if_ops->ifop_output(ifp, m, dst, ro));
}

static inline int
if_ioctl(if_t ifp, u_long cmd, void *data, struct thread *td)
{
	int error = EOPNOTSUPP;

	if (ifp->if_ops->ifop_ioctl != NULL)
		error = ifp->if_ops->ifop_ioctl(ifp, cmd, data, td);

	if (error == EOPNOTSUPP && ifp->if_type != NULL &&
	    ifp->if_type->ift_ops.ifop_ioctl != NULL)
		error = ifp->if_type->ift_ops.ifop_ioctl(ifp, cmd, data, td);

	return (error);
}

static inline uint64_t
if_get_counter(const if_t ifp, ift_counter cnt)
{

	return (ifp->if_ops->ifop_get_counter(ifp, cnt));
}

static inline int
if_resolvemulti(if_t ifp, struct sockaddr **llsa, struct sockaddr *sa)
{

	if (ifp->if_ops->ifop_resolvemulti != NULL)
		return (ifp->if_ops->ifop_resolvemulti(ifp, llsa, sa));
	else
		return (EOPNOTSUPP);
}

static inline void
if_reassign(if_t ifp, struct vnet *new)
{

	return (ifp->if_ops->ifop_reassign(ifp, new));
}

/*
 * Inliners to shorten code, and make protocols more ifnet-agnostic.
 */
static inline ifType
if_type(const if_t ifp)
{

	return (ifp->if_drv->ifdrv_type);
}

static inline uint8_t
if_addrlen(const if_t ifp)
{

	return (ifp->if_drv->ifdrv_addrlen);
}
#endif /* _KERNEL */
#endif /* !_NET_IF_VAR_H_ */
