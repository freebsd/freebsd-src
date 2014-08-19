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

/*
 * Structures defining a network interface, providing a packet
 * transport mechanism (ala level 0 of the PUP protocols).
 *
 * Each interface accepts output datagrams of a specified maximum
 * length, and provides higher level routines with input datagrams
 * received from its medium.
 *
 * Output occurs when the routine if_output is called, with three parameters:
 *	(*ifp->if_output)(ifp, m, dst, rt)
 * Here m is the mbuf chain to be sent and dst is the destination address.
 * The output routine encapsulates the supplied datagram if necessary,
 * and then transmits it on its medium.
 *
 * On input, each interface unwraps the data received by it, and either
 * places it on the input queue of an internetwork datagram routine
 * and posts the associated software interrupt, or passes the datagram to a raw
 * packet input routine.
 *
 * Routines exist for locating interfaces by their addresses
 * or for locating an interface on a certain network, as well as more general
 * routing and gateway routines maintaining information used to locate
 * interfaces.  These routines live in the files if.c and route.c
 */

struct	rtentry;		/* ifa_rtrequest */
struct	rt_addrinfo;		/* ifa_rtrequest */
struct	socket;
struct	carp_if;
struct	carp_softc;
struct  ifvlantrunk;
struct	route;			/* if_output */
struct	vnet;
struct	ifmedia;

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

#define	IF_DUNIT_NONE	-1

#include <altq/if_altq.h>

TAILQ_HEAD(ifnethead, ifnet);	/* we use TAILQs so that the order of */
TAILQ_HEAD(ifaddrhead, ifaddr);	/* instantiation is preserved in the list */
TAILQ_HEAD(ifmultihead, ifmultiaddr);
TAILQ_HEAD(ifgrouphead, ifg_group);

#ifdef _KERNEL
VNET_DECLARE(struct pfil_head, link_pfil_hook);	/* packet filter hooks */
#define	V_link_pfil_hook	VNET(link_pfil_hook)
#endif /* _KERNEL */

typedef	void (*if_start_fn_t)(struct ifnet *);
typedef	int (*if_ioctl_fn_t)(struct ifnet *, u_long, caddr_t);
typedef	void (*if_init_fn_t)(void *);
typedef void (*if_qflush_fn_t)(struct ifnet *);
typedef int (*if_transmit_fn_t)(struct ifnet *, struct mbuf *);

/* Opaque object pointing to interface structure (ifnet) */
typedef void *if_t;

/*
 * Structure defining a network interface.
 *
 * Size ILP32:  592 (approx)
 *	 LP64: 1048 (approx)
 */
struct ifnet {
	/* General book keeping of interface lists. */
	TAILQ_ENTRY(ifnet) if_link; 	/* all struct ifnets are chained */
	LIST_ENTRY(ifnet) if_clones;	/* interfaces of a cloner */
	TAILQ_HEAD(, ifg_list) if_groups; /* linked list of groups per if */
					/* protected by if_addr_lock */
	u_char	if_alloctype;		/* if_type at time of allocation */

	/* Driver and protocol specific information that remains stable. */
	void	*if_softc;		/* pointer to driver state */
	void	*if_llsoftc;		/* link layer softc */
	void	*if_l2com;		/* pointer to protocol bits */
	const char *if_dname;		/* driver name */
	int	if_dunit;		/* unit or IF_DUNIT_NONE */
	u_short	if_index;		/* numeric abbreviation for this if  */
	short	if_index_reserved;	/* spare space to grow if_index */
	char	if_xname[IFNAMSIZ];	/* external name (name + unit) */
	char	*if_description;	/* interface description */

	/* Variable fields that are touched by the stack and drivers. */
	int	if_flags;		/* up/down, broadcast, etc. */
	int	if_capabilities;	/* interface features & capabilities */
	int	if_capenable;		/* enabled features & capabilities */
	void	*if_linkmib;		/* link-type-specific MIB data */
	size_t	if_linkmiblen;		/* length of above data */
	int	if_drv_flags;		/* driver-managed status flags */
	u_int	if_refcount;		/* reference count */
	struct  ifaltq if_snd;		/* output queue (includes altq) */
	struct	if_data if_data;	/* type information and statistics */
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
	u_int	if_fib;			/* interface FIB */
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

	/* Various procedures of the layer2 encapsulation and drivers. */
	int	(*if_output)		/* output routine (enqueue) */
		(struct ifnet *, struct mbuf *, const struct sockaddr *,
		     struct route *);
	void	(*if_input)		/* input routine (from h/w driver) */
		(struct ifnet *, struct mbuf *);
	if_start_fn_t	if_start;	/* initiate output routine */
	if_ioctl_fn_t	if_ioctl;	/* ioctl routine */
	if_init_fn_t	if_init;	/* Init routine */
	int	(*if_resolvemulti)	/* validate/resolve multicast */
		(struct ifnet *, struct sockaddr **, struct sockaddr *);
	if_qflush_fn_t	if_qflush;	/* flush any queue */	
	if_transmit_fn_t if_transmit;   /* initiate output routine */

	void	(*if_reassign)		/* reassign to vnet routine */
		(struct ifnet *, struct vnet *, char *);

	/* Stuff that's only temporary and doesn't belong here. */
	u_int	if_hw_tsomax;		/* tso burst length limit, the minimum
					 * is (IP_MAXPACKET / 8).
					 * XXXAO: Have to find a better place
					 * for it eventually. */
	/*
	 * Spare fields are added so that we can modify sensitive data
	 * structures without changing the kernel binary interface, and must
	 * be used with care where binary compatibility is required.
	 */
	char	if_cspare[3];
	int	if_ispare[4];
	void	*if_unused[2];
	void	*if_pspare[8];		/* 1 netmap, 7 TDB */
};

#include <net/ifq.h>	/* XXXAO: temporary unconditional include */

/*
 * XXX These aliases are terribly dangerous because they could apply
 * to anything.
 */
#define	if_mtu		if_data.ifi_mtu
#define	if_type		if_data.ifi_type
#define if_physical	if_data.ifi_physical
#define	if_addrlen	if_data.ifi_addrlen
#define	if_hdrlen	if_data.ifi_hdrlen
#define	if_metric	if_data.ifi_metric
#define	if_link_state	if_data.ifi_link_state
#define	if_baudrate	if_data.ifi_baudrate
#define	if_hwassist	if_data.ifi_hwassist
#define	if_ipackets	if_data.ifi_ipackets
#define	if_ierrors	if_data.ifi_ierrors
#define	if_opackets	if_data.ifi_opackets
#define	if_oerrors	if_data.ifi_oerrors
#define	if_collisions	if_data.ifi_collisions
#define	if_ibytes	if_data.ifi_ibytes
#define	if_obytes	if_data.ifi_obytes
#define	if_imcasts	if_data.ifi_imcasts
#define	if_omcasts	if_data.ifi_omcasts
#define	if_iqdrops	if_data.ifi_iqdrops
#define	if_oqdrops	if_data.ifi_oqdrops
#define	if_noproto	if_data.ifi_noproto
#define	if_lastchange	if_data.ifi_lastchange

/* for compatibility with other BSDs */
#define	if_addrlist	if_addrhead
#define	if_list		if_link
#define	if_name(ifp)	((ifp)->if_xname)

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

/*
 * Function variations on locking macros intended to be used by loadable
 * kernel modules in order to divorce them from the internals of address list
 * locking.
 */
void	if_addr_rlock(struct ifnet *ifp);	/* if_addrhead */
void	if_addr_runlock(struct ifnet *ifp);	/* if_addrhead */
void	if_maddr_rlock(if_t ifp);	/* if_multiaddrs */
void	if_maddr_runlock(if_t ifp);	/* if_multiaddrs */

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
	int	ifa_metric;		/* cost of going out this interface */
	int (*ifa_claim_addr)		/* check if an addr goes to this if */
		(struct ifaddr *, struct sockaddr *);

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

#define	IFNET_LOCK_INIT() do {						\
	rw_init_flags(&ifnet_rwlock, "ifnet_rw",  RW_RECURSE);		\
	sx_init_flags(&ifnet_sxlock, "ifnet_sx",  SX_RECURSE);		\
} while(0)

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
struct	ifnet* if_alloc(u_char);
void	if_attach(struct ifnet *);
void	if_dead(struct ifnet *);
int	if_delmulti(struct ifnet *, struct sockaddr *);
void	if_delmulti_ifma(struct ifmultiaddr *);
void	if_detach(struct ifnet *);
void	if_vmove(struct ifnet *, struct vnet *);
void	if_purgeaddrs(struct ifnet *);
void	if_delallmulti(struct ifnet *);
void	if_down(struct ifnet *);
struct ifmultiaddr *
	if_findmulti(struct ifnet *, struct sockaddr *);
void	if_free(struct ifnet *);
void	if_initname(struct ifnet *, const char *, int);
void	if_link_state_change(struct ifnet *, int);
int	if_printf(struct ifnet *, const char *, ...) __printflike(2, 3);
void	if_ref(struct ifnet *);
void	if_rele(struct ifnet *);
int	if_setlladdr(struct ifnet *, const u_char *, int);
void	if_up(struct ifnet *);
int	ifioctl(struct socket *, u_long, caddr_t, struct thread *);
int	ifpromisc(struct ifnet *, int);
struct	ifnet *ifunit(const char *);
struct	ifnet *ifunit_ref(const char *);

int	ifa_add_loopback_route(struct ifaddr *, struct sockaddr *);
int	ifa_del_loopback_route(struct ifaddr *, struct sockaddr *);
int	ifa_switch_loopback_route(struct ifaddr *, struct sockaddr *, int fib);

struct	ifaddr *ifa_ifwithaddr(struct sockaddr *);
int		ifa_ifwithaddr_check(struct sockaddr *);
struct	ifaddr *ifa_ifwithbroadaddr(struct sockaddr *);
struct	ifaddr *ifa_ifwithdstaddr(struct sockaddr *);
struct	ifaddr *ifa_ifwithdstaddr_fib(struct sockaddr *, int);
struct	ifaddr *ifa_ifwithnet(struct sockaddr *, int);
struct	ifaddr *ifa_ifwithnet_fib(struct sockaddr *, int, int);
struct	ifaddr *ifa_ifwithroute(int, struct sockaddr *, struct sockaddr *);
struct	ifaddr *ifa_ifwithroute_fib(int, struct sockaddr *, struct sockaddr *, u_int);
struct	ifaddr *ifaof_ifpforaddr(struct sockaddr *, struct ifnet *);
int	ifa_preferred(struct ifaddr *, struct ifaddr *);

int	if_simloop(struct ifnet *ifp, struct mbuf *m, int af, int hlen);

typedef	void *if_com_alloc_t(u_char type, struct ifnet *ifp);
typedef	void if_com_free_t(void *com, u_char type);
void	if_register_com_alloc(u_char type, if_com_alloc_t *a, if_com_free_t *f);
void	if_deregister_com_alloc(u_char type);

#define IF_LLADDR(ifp)							\
    LLADDR((struct sockaddr_dl *)((ifp)->if_addr->ifa_addr))

uint64_t if_setbaudrate(if_t ifp, uint64_t baudrate);
uint64_t if_getbaudrate(if_t ifp);
int if_setcapabilities(if_t ifp, int capabilities);
int if_setcapabilitiesbit(if_t ifp, int setbit, int clearbit);
int if_getcapabilities(if_t ifp);
int if_togglecapenable(if_t ifp, int togglecap);
int if_setcapenable(if_t ifp, int capenable);
int if_setcapenablebit(if_t ifp, int setcap, int clearcap);
int if_getcapenable(if_t ifp);
const char *if_getdname(if_t ifp);
int if_setdev(if_t ifp, void *dev);
int if_setdrvflagbits(if_t ifp, int if_setflags, int clear_flags);
int if_getdrvflags(if_t ifp);
int if_setdrvflags(if_t ifp, int flags);
int if_clearhwassist(if_t ifp);
int if_sethwassistbits(if_t ifp, int toset, int toclear);
int if_sethwassist(if_t ifp, int hwassist_bit);
int if_gethwassist(if_t ifp);
int if_setsoftc(if_t ifp, void *softc);
void *if_getsoftc(if_t ifp);
int if_setflags(if_t ifp, int flags);
int if_setmtu(if_t ifp, int mtu);
int if_getmtu(if_t ifp);
int if_setflagbits(if_t ifp, int set, int clear);
int if_getflags(if_t ifp);
int if_sendq_empty(if_t ifp);
int if_setsendqready(if_t ifp);
int if_setsendqlen(if_t ifp, int tx_desc_count);
int if_input(if_t ifp, struct mbuf* sendmp);
int if_sendq_prepend(if_t ifp, struct mbuf *m);
struct mbuf *if_dequeue(if_t ifp);
int if_setifheaderlen(if_t ifp, int len);
void if_setrcvif(struct mbuf *m, if_t ifp);
void if_setvtag(struct mbuf *m, u_int16_t tag);
u_int16_t if_getvtag(struct mbuf *m);
int if_vlantrunkinuse(if_t ifp);
caddr_t if_getlladdr(if_t ifp);
void *if_gethandle(u_char);
void if_bpfmtap(if_t ifp, struct mbuf *m);
void if_etherbpfmtap(if_t ifp, struct mbuf *m);
void if_vlancap(if_t ifp);

int if_setupmultiaddr(if_t ifp, void *mta, int *cnt, int max);
int if_multiaddr_array(if_t ifp, void *mta, int *cnt, int max);
int if_multiaddr_count(if_t ifp, int max);

int if_getamcount(if_t ifp);
struct ifaddr * if_getifaddr(if_t ifp);
/* Shim for drivers using drvapi */
int ifmedia_ioctl_drv(if_t ifp, struct ifreq *ifr, struct ifmedia *ifm,
    u_long cmd);

/* Statistics */

int if_incipackets(if_t ifp, int pkt);
int if_incopackets(if_t ifp, int pkts);
int if_incierrors(if_t ifp, int ierrors);
int if_incoerrors(if_t ifp, int oerrors);
int if_inciqdrops(if_t ifp, int val);
int if_setierrors(if_t ifp, int ierrors);
int if_setoerrors(if_t ifp, int oerrors);
int if_setcollisions(if_t ifp, int collisions);
int if_inccollisions(if_t ifp, int collisions);
int if_incobytes(if_t ifp, int bytes);
int if_getiqdrops(if_t ifp);
int if_incimcasts(if_t ifp, int imcasts);
int if_incomcasts(if_t ifp, int imcasts);
int if_setipackets(if_t ifp, int pkts);
int if_setopackets(if_t ifp, int pkts);
int if_setibytes(if_t ifp, int bytes);
int if_setobytes(if_t ifp, int bytes);
int if_setimcasts(if_t ifp, int pkts);

/* Functions */
void if_setinitfn(if_t ifp, void (*)(void *));
void if_setioctlfn(if_t ifp, int (*)(void *, u_long, caddr_t));
void if_setstartfn(if_t ifp, void (*)(void *));
void if_settransmitfn(if_t ifp, if_transmit_fn_t);
void if_setqflushfn(if_t ifp, if_qflush_fn_t);
 

/* Shim functions till all drivers use drvapi */
void arp_ifinit_drv(if_t ifp, struct ifaddr *ifa);
void ether_ifattach_drv(if_t ifp, const u_int8_t *lla);
void ether_ifdetach_drv(if_t ifp);
int ether_ioctl_drv(if_t ifp, u_long cmd, caddr_t data);
void if_free_drv(if_t ifp);
void if_initname_drv(if_t ifp, const char *name, int unit);
void if_linkstate_change_drv(if_t ifp, int link_state);

struct ifmedia;
void ifmedia_init_drv(struct ifmedia *, int, int (*)(void *),
	void (*)(void *, struct ifmediareq *));

void if_addr_rlock_drv(if_t ifp);
void if_addr_runlock_drv(if_t ifp);
void if_qflush_drv(if_t ifp);

/* Revisit the below. These are inline functions originally */
int drbr_inuse_drv(if_t ifp, struct buf_ring *br);
struct mbuf* drbr_dequeue_drv(if_t ifp, struct buf_ring *br);
int drbr_needs_enqueue_drv(if_t ifp, struct buf_ring *br);
int drbr_enqueue_drv(if_t ifp, struct buf_ring *br, struct mbuf *m);

#endif /* _KERNEL */
#endif /* !_NET_IF_VAR_H_ */
