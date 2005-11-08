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

#ifdef __STDC__
/*
 * Forward structure declarations for function prototypes [sic].
 */
struct	mbuf;
struct	thread;
struct	rtentry;
struct	rt_addrinfo;
struct	socket;
struct	ether_header;
struct	carp_if;
#endif

#include <sys/queue.h>		/* get TAILQ macros */

#ifdef _KERNEL
#include <sys/mbuf.h>
#include <sys/eventhandler.h>
#endif /* _KERNEL */
#include <sys/lock.h>		/* XXX */
#include <sys/mutex.h>		/* XXX */
#include <sys/event.h>		/* XXX */
#include <sys/_task.h>

#define	IF_DUNIT_NONE	-1

#include <altq/if_altq.h>

TAILQ_HEAD(ifnethead, ifnet);	/* we use TAILQs so that the order of */
TAILQ_HEAD(ifaddrhead, ifaddr);	/* instantiation is preserved in the list */
TAILQ_HEAD(ifprefixhead, ifprefix);
TAILQ_HEAD(ifmultihead, ifmultiaddr);

/*
 * Structure defining a queue for a network interface.
 */
struct	ifqueue {
	struct	mbuf *ifq_head;
	struct	mbuf *ifq_tail;
	int	ifq_len;
	int	ifq_maxlen;
	int	ifq_drops;
	struct	mtx ifq_mtx;
};

/*
 * Structure defining a network interface.
 *
 * (Would like to call this struct ``if'', but C isn't PL/1.)
 */

struct ifnet {
	void	*if_softc;		/* pointer to driver state */
	void	*if_l2com;		/* pointer to protocol bits */
	TAILQ_ENTRY(ifnet) if_link; 	/* all struct ifnets are chained */
	char	if_xname[IFNAMSIZ];	/* external name (name + unit) */
	const char *if_dname;		/* driver name */
	int	if_dunit;		/* unit or IF_DUNIT_NONE */
	struct	ifaddrhead if_addrhead;	/* linked list of addresses per if */
		/*
		 * if_addrhead is the list of all addresses associated to
		 * an interface.
		 * Some code in the kernel assumes that first element
		 * of the list has type AF_LINK, and contains sockaddr_dl
		 * addresses which store the link-level address and the name
		 * of the interface.
		 * However, access to the AF_LINK address through this
		 * field is deprecated. Use ifaddr_byindex() instead.
		 */
	struct	knlist if_klist;	/* events attached to this if */
	int	if_pcount;		/* number of promiscuous listeners */
	struct	carp_if *if_carp;	/* carp interface structure */
	struct	bpf_if *if_bpf;		/* packet filter structure */
	u_short	if_index;		/* numeric abbreviation for this if  */
	short	if_timer;		/* time 'til if_watchdog called */
	u_short	if_nvlans;		/* number of active vlans */
	int	if_flags;		/* up/down, broadcast, etc. */
	int	if_capabilities;	/* interface capabilities */
	int	if_capenable;		/* enabled features */
	void	*if_linkmib;		/* link-type-specific MIB data */
	size_t	if_linkmiblen;		/* length of above data */
	struct	if_data if_data;
	struct	ifmultihead if_multiaddrs; /* multicast addresses configured */
	int	if_amcount;		/* number of all-multicast requests */
/* procedure handles */
	int	(*if_output)		/* output routine (enqueue) */
		(struct ifnet *, struct mbuf *, struct sockaddr *,
		     struct rtentry *);
	void	(*if_input)		/* input routine (from h/w driver) */
		(struct ifnet *, struct mbuf *);
	void	(*if_start)		/* initiate output routine */
		(struct ifnet *);
	int	(*if_ioctl)		/* ioctl routine */
		(struct ifnet *, u_long, caddr_t);
	void	(*if_watchdog)		/* timer routine */
		(struct ifnet *);
	void	(*if_init)		/* Init routine */
		(void *);
	int	(*if_resolvemulti)	/* validate/resolve multicast */
		(struct ifnet *, struct sockaddr **, struct sockaddr *);
	void	*if_spare1;		/* spare pointer 1 */
	void	*if_spare2;		/* spare pointer 2 */
	void	*if_spare3;		/* spare pointer 3 */
	int	if_drv_flags;		/* driver-managed status flags */
	u_int	if_spare_flags2;	/* spare flags 2 */
	struct  ifaltq if_snd;		/* output queue (includes altq) */
	const u_int8_t *if_broadcastaddr; /* linklevel broadcast bytestring */

	void	*if_bridge;		/* bridge glue */

	struct	lltable *lltables;	/* list of L3-L2 resolution tables */

	struct	label *if_label;	/* interface MAC label */

	/* these are only used by IPv6 */
	struct	ifprefixhead if_prefixhead; /* list of prefixes per if */
	void	*if_afdata[AF_MAX];
	int	if_afdata_initialized;
	struct	mtx if_afdata_mtx;
	struct	task if_starttask;	/* task for IFF_NEEDSGIANT */
	struct	task if_linktask;	/* task for link change events */
	struct	mtx if_addr_mtx;	/* mutex to protect address lists */
	LIST_ENTRY(ifnet) if_clones;	/* interfaces of a cloner */
};

typedef void if_init_f_t(void *);

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
#define	if_noproto	if_data.ifi_noproto
#define	if_lastchange	if_data.ifi_lastchange
#define if_recvquota	if_data.ifi_recvquota
#define	if_xmitquota	if_data.ifi_xmitquota
#define if_rawoutput(if, m, sa) if_output(if, m, sa, (struct rtentry *)NULL)

/* for compatibility with other BSDs */
#define	if_addrlist	if_addrhead
#define	if_list		if_link

/*
 * Locks for address lists on the network interface.
 */
#define	IF_ADDR_LOCK_INIT(if)	mtx_init(&(if)->if_addr_mtx,		\
				    "if_addr_mtx", NULL, MTX_DEF)
#define	IF_ADDR_LOCK_DESTROY(if)	mtx_destroy(&(if)->if_addr_mtx)
#define	IF_ADDR_LOCK(if)	mtx_lock(&(if)->if_addr_mtx)
#define	IF_ADDR_UNLOCK(if)	mtx_unlock(&(if)->if_addr_mtx)
#define	IF_ADDR_LOCK_ASSERT(if)	mtx_assert(&(if)->if_addr_mtx, MA_OWNED)

/*
 * Output queues (ifp->if_snd) and slow device input queues (*ifp->if_slowq)
 * are queues of messages stored on ifqueue structures
 * (defined above).  Entries are added to and deleted from these structures
 * by these macros, which should be called with ipl raised to splimp().
 */
#define IF_LOCK(ifq)		mtx_lock(&(ifq)->ifq_mtx)
#define IF_UNLOCK(ifq)		mtx_unlock(&(ifq)->ifq_mtx)
#define	IF_LOCK_ASSERT(ifq)	mtx_assert(&(ifq)->ifq_mtx, MA_OWNED)
#define	_IF_QFULL(ifq)		((ifq)->ifq_len >= (ifq)->ifq_maxlen)
#define	_IF_DROP(ifq)		((ifq)->ifq_drops++)
#define	_IF_QLEN(ifq)		((ifq)->ifq_len)

#define	_IF_ENQUEUE(ifq, m) do { 				\
	(m)->m_nextpkt = NULL;					\
	if ((ifq)->ifq_tail == NULL) 				\
		(ifq)->ifq_head = m; 				\
	else 							\
		(ifq)->ifq_tail->m_nextpkt = m; 		\
	(ifq)->ifq_tail = m; 					\
	(ifq)->ifq_len++; 					\
} while (0)

#define IF_ENQUEUE(ifq, m) do {					\
	IF_LOCK(ifq); 						\
	_IF_ENQUEUE(ifq, m); 					\
	IF_UNLOCK(ifq); 					\
} while (0)

#define	_IF_PREPEND(ifq, m) do {				\
	(m)->m_nextpkt = (ifq)->ifq_head; 			\
	if ((ifq)->ifq_tail == NULL) 				\
		(ifq)->ifq_tail = (m); 				\
	(ifq)->ifq_head = (m); 					\
	(ifq)->ifq_len++; 					\
} while (0)

#define IF_PREPEND(ifq, m) do {		 			\
	IF_LOCK(ifq); 						\
	_IF_PREPEND(ifq, m); 					\
	IF_UNLOCK(ifq); 					\
} while (0)

#define	_IF_DEQUEUE(ifq, m) do { 				\
	(m) = (ifq)->ifq_head; 					\
	if (m) { 						\
		if (((ifq)->ifq_head = (m)->m_nextpkt) == NULL)	\
			(ifq)->ifq_tail = NULL; 		\
		(m)->m_nextpkt = NULL; 				\
		(ifq)->ifq_len--; 				\
	} 							\
} while (0)

#define IF_DEQUEUE(ifq, m) do { 				\
	IF_LOCK(ifq); 						\
	_IF_DEQUEUE(ifq, m); 					\
	IF_UNLOCK(ifq); 					\
} while (0)

#define	_IF_POLL(ifq, m)	((m) = (ifq)->ifq_head)
#define	IF_POLL(ifq, m)		_IF_POLL(ifq, m)

#define _IF_DRAIN(ifq) do { 					\
	struct mbuf *m; 					\
	for (;;) { 						\
		_IF_DEQUEUE(ifq, m); 				\
		if (m == NULL) 					\
			break; 					\
		m_freem(m); 					\
	} 							\
} while (0)

#define IF_DRAIN(ifq) do {					\
	IF_LOCK(ifq);						\
	_IF_DRAIN(ifq);						\
	IF_UNLOCK(ifq);						\
} while(0)

#ifdef _KERNEL
/* interface address change event */
typedef void (*ifaddr_event_handler_t)(void *, struct ifnet *);
EVENTHANDLER_DECLARE(ifaddr_event, ifaddr_event_handler_t);
/* new interface arrival event */
typedef void (*ifnet_arrival_event_handler_t)(void *, struct ifnet *);
EVENTHANDLER_DECLARE(ifnet_arrival_event, ifnet_arrival_event_handler_t);
/* interface departure event */
typedef void (*ifnet_departure_event_handler_t)(void *, struct ifnet *);
EVENTHANDLER_DECLARE(ifnet_departure_event, ifnet_departure_event_handler_t);

#define	IF_AFDATA_LOCK_INIT(ifp)	\
    mtx_init(&(ifp)->if_afdata_mtx, "if_afdata", NULL, MTX_DEF)
#define	IF_AFDATA_LOCK(ifp)	mtx_lock(&(ifp)->if_afdata_mtx)
#define	IF_AFDATA_TRYLOCK(ifp)	mtx_trylock(&(ifp)->if_afdata_mtx)
#define	IF_AFDATA_UNLOCK(ifp)	mtx_unlock(&(ifp)->if_afdata_mtx)
#define	IF_AFDATA_DESTROY(ifp)	mtx_destroy(&(ifp)->if_afdata_mtx)

#define	IFF_LOCKGIANT(ifp) do {						\
	if ((ifp)->if_flags & IFF_NEEDSGIANT)				\
		mtx_lock(&Giant);					\
} while (0)

#define	IFF_UNLOCKGIANT(ifp) do {					\
	if ((ifp)->if_flags & IFF_NEEDSGIANT)				\
		mtx_unlock(&Giant);					\
} while (0)

int	if_handoff(struct ifqueue *ifq, struct mbuf *m, struct ifnet *ifp,
	    int adjust);
#define	IF_HANDOFF(ifq, m, ifp)			\
	if_handoff((struct ifqueue *)ifq, m, ifp, 0)
#define	IF_HANDOFF_ADJ(ifq, m, ifp, adj)	\
	if_handoff((struct ifqueue *)ifq, m, ifp, adj)

void	if_start(struct ifnet *);

#define	IFQ_ENQUEUE(ifq, m, err)					\
do {									\
	IF_LOCK(ifq);							\
	if (ALTQ_IS_ENABLED(ifq))					\
		ALTQ_ENQUEUE(ifq, m, NULL, err);			\
	else {								\
		if (_IF_QFULL(ifq)) {					\
			m_freem(m);					\
			(err) = ENOBUFS;				\
		} else {						\
			_IF_ENQUEUE(ifq, m);				\
			(err) = 0;					\
		}							\
	}								\
	if (err)							\
		(ifq)->ifq_drops++;					\
	IF_UNLOCK(ifq);							\
} while (0)

#define	IFQ_DEQUEUE_NOLOCK(ifq, m)					\
do {									\
	if (TBR_IS_ENABLED(ifq))					\
		(m) = tbr_dequeue_ptr(ifq, ALTDQ_REMOVE);		\
	else if (ALTQ_IS_ENABLED(ifq))					\
		ALTQ_DEQUEUE(ifq, m);					\
	else								\
		_IF_DEQUEUE(ifq, m);					\
} while (0)

#define	IFQ_DEQUEUE(ifq, m)						\
do {									\
	IF_LOCK(ifq);							\
	IFQ_DEQUEUE_NOLOCK(ifq, m);					\
	IF_UNLOCK(ifq);							\
} while (0)

#define	IFQ_POLL_NOLOCK(ifq, m)						\
do {									\
	if (TBR_IS_ENABLED(ifq))					\
		(m) = tbr_dequeue_ptr(ifq, ALTDQ_POLL);			\
	else if (ALTQ_IS_ENABLED(ifq))					\
		ALTQ_POLL(ifq, m);					\
	else								\
		_IF_POLL(ifq, m);					\
} while (0)

#define	IFQ_POLL(ifq, m)						\
do {									\
	IF_LOCK(ifq);							\
	IFQ_POLL_NOLOCK(ifq, m);					\
	IF_UNLOCK(ifq);							\
} while (0)

#define	IFQ_PURGE_NOLOCK(ifq)						\
do {									\
	if (ALTQ_IS_ENABLED(ifq)) {					\
		ALTQ_PURGE(ifq);					\
	} else								\
		_IF_DRAIN(ifq);						\
} while (0)

#define	IFQ_PURGE(ifq)							\
do {									\
	IF_LOCK(ifq);							\
	IFQ_PURGE_NOLOCK(ifq);						\
	IF_UNLOCK(ifq);							\
} while (0)

#define	IFQ_SET_READY(ifq)						\
	do { ((ifq)->altq_flags |= ALTQF_READY); } while (0)

#define	IFQ_LOCK(ifq)			IF_LOCK(ifq)
#define	IFQ_UNLOCK(ifq)			IF_UNLOCK(ifq)
#define	IFQ_LOCK_ASSERT(ifq)		IF_LOCK_ASSERT(ifq)
#define	IFQ_IS_EMPTY(ifq)		((ifq)->ifq_len == 0)
#define	IFQ_INC_LEN(ifq)		((ifq)->ifq_len++)
#define	IFQ_DEC_LEN(ifq)		(--(ifq)->ifq_len)
#define	IFQ_INC_DROPS(ifq)		((ifq)->ifq_drops++)
#define	IFQ_SET_MAXLEN(ifq, len)	((ifq)->ifq_maxlen = (len))

/*
 * The IFF_DRV_OACTIVE test should really occur in the device driver, not in
 * the handoff logic, as that flag is locked by the device driver.
 */
#define	IFQ_HANDOFF_ADJ(ifp, m, adj, err)				\
do {									\
	int len;							\
	short mflags;							\
									\
	len = (m)->m_pkthdr.len;					\
	mflags = (m)->m_flags;						\
	IFQ_ENQUEUE(&(ifp)->if_snd, m, err);				\
	if ((err) == 0) {						\
		(ifp)->if_obytes += len + (adj);			\
		if (mflags & M_MCAST)					\
			(ifp)->if_omcasts++;				\
		if (((ifp)->if_drv_flags & IFF_DRV_OACTIVE) == 0)	\
			if_start(ifp);					\
	}								\
} while (0)

#define	IFQ_HANDOFF(ifp, m, err)					\
	IFQ_HANDOFF_ADJ(ifp, m, 0, err)

#define	IFQ_DRV_DEQUEUE(ifq, m)						\
do {									\
	(m) = (ifq)->ifq_drv_head;					\
	if (m) {							\
		if (((ifq)->ifq_drv_head = (m)->m_nextpkt) == NULL)	\
			(ifq)->ifq_drv_tail = NULL;			\
		(m)->m_nextpkt = NULL;					\
		(ifq)->ifq_drv_len--;					\
	} else {							\
		IFQ_LOCK(ifq);						\
		IFQ_DEQUEUE_NOLOCK(ifq, m);				\
		while ((ifq)->ifq_drv_len < (ifq)->ifq_drv_maxlen) {	\
			struct mbuf *m0;				\
			IFQ_DEQUEUE_NOLOCK(ifq, m0);			\
			if (m0 == NULL)					\
				break;					\
			m0->m_nextpkt = NULL;				\
			if ((ifq)->ifq_drv_tail == NULL)		\
				(ifq)->ifq_drv_head = m0;		\
			else						\
				(ifq)->ifq_drv_tail->m_nextpkt = m0;	\
			(ifq)->ifq_drv_tail = m0;			\
			(ifq)->ifq_drv_len++;				\
		}							\
		IFQ_UNLOCK(ifq);					\
	}								\
} while (0)

#define	IFQ_DRV_PREPEND(ifq, m)						\
do {									\
	(m)->m_nextpkt = (ifq)->ifq_drv_head;				\
	if ((ifq)->ifq_drv_tail == NULL)				\
		(ifq)->ifq_drv_tail = (m);				\
	(ifq)->ifq_drv_head = (m);					\
	(ifq)->ifq_drv_len++;						\
} while (0)

#define	IFQ_DRV_IS_EMPTY(ifq)						\
	(((ifq)->ifq_drv_len == 0) && ((ifq)->ifq_len == 0))

#define	IFQ_DRV_PURGE(ifq)						\
do {									\
	struct mbuf *m, *n = (ifq)->ifq_drv_head;			\
	while((m = n) != NULL) {					\
		n = m->m_nextpkt;					\
		m_freem(m);						\
	}								\
	(ifq)->ifq_drv_head = (ifq)->ifq_drv_tail = NULL;		\
	(ifq)->ifq_drv_len = 0;						\
	IFQ_PURGE(ifq);							\
} while (0)

/*
 * 72 was chosen below because it is the size of a TCP/IP
 * header (40) + the minimum mss (32).
 */
#define	IF_MINMTU	72
#define	IF_MAXMTU	65535

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
struct ifaddr {
	struct	sockaddr *ifa_addr;	/* address of interface */
	struct	sockaddr *ifa_dstaddr;	/* other end of p-to-p link */
#define	ifa_broadaddr	ifa_dstaddr	/* broadcast address interface */
	struct	sockaddr *ifa_netmask;	/* used to determine subnet */
	struct	if_data if_data;	/* not all members are meaningful */
	struct	ifnet *ifa_ifp;		/* back-pointer to interface */
	TAILQ_ENTRY(ifaddr) ifa_link;	/* queue macro glue */
	void	(*ifa_rtrequest)	/* check or clean routes (+ or -)'d */
		(int, struct rtentry *, struct rt_addrinfo *);
	u_short	ifa_flags;		/* mostly rt_flags for cloning */
	u_int	ifa_refcnt;		/* references to this structure */
	int	ifa_metric;		/* cost of going out this interface */
	int (*ifa_claim_addr)		/* check if an addr goes to this if */
		(struct ifaddr *, struct sockaddr *);
	struct mtx ifa_mtx;
};
#define	IFA_ROUTE	RTF_UP		/* route installed */

/* for compatibility with other BSDs */
#define	ifa_list	ifa_link

#define	IFA_LOCK_INIT(ifa)	\
    mtx_init(&(ifa)->ifa_mtx, "ifaddr", NULL, MTX_DEF)
#define	IFA_LOCK(ifa)		mtx_lock(&(ifa)->ifa_mtx)
#define	IFA_UNLOCK(ifa)		mtx_unlock(&(ifa)->ifa_mtx)
#define	IFA_DESTROY(ifa)	mtx_destroy(&(ifa)->ifa_mtx)

/*
 * The prefix structure contains information about one prefix
 * of an interface.  They are maintained by the different address families,
 * are allocated and attached when a prefix or an address is set,
 * and are linked together so all prefixes for an interface can be located.
 */
struct ifprefix {
	struct	sockaddr *ifpr_prefix;	/* prefix of interface */
	struct	ifnet *ifpr_ifp;	/* back-pointer to interface */
	TAILQ_ENTRY(ifprefix) ifpr_list; /* queue macro glue */
	u_char	ifpr_plen;		/* prefix length in bits */
	u_char	ifpr_type;		/* protocol dependent prefix type */
};

/*
 * Multicast address structure.  This is analogous to the ifaddr
 * structure except that it keeps track of multicast addresses.
 * Also, the reference count here is a count of requests for this
 * address, not a count of pointers to this structure.
 */
struct ifmultiaddr {
	TAILQ_ENTRY(ifmultiaddr) ifma_link; /* queue macro glue */
	struct	sockaddr *ifma_addr; 	/* address this membership is for */
	struct	sockaddr *ifma_lladdr;	/* link-layer translation, if any */
	struct	ifnet *ifma_ifp;	/* back-pointer to interface */
	u_int	ifma_refcount;		/* reference count */
	void	*ifma_protospec;	/* protocol-specific state, if any */
};

#ifdef _KERNEL
#define	IFAFREE(ifa)					\
	do {						\
		IFA_LOCK(ifa);				\
		KASSERT((ifa)->ifa_refcnt > 0,		\
		    ("ifa %p !(ifa_refcnt > 0)", ifa));	\
		if (--(ifa)->ifa_refcnt == 0) {		\
			IFA_DESTROY(ifa);		\
			free(ifa, M_IFADDR);		\
		} else 					\
			IFA_UNLOCK(ifa);		\
	} while (0)

#define IFAREF(ifa)					\
	do {						\
		IFA_LOCK(ifa);				\
		++(ifa)->ifa_refcnt;			\
		IFA_UNLOCK(ifa);			\
	} while (0)

extern	struct mtx ifnet_lock;
#define	IFNET_LOCK_INIT() \
    mtx_init(&ifnet_lock, "ifnet", NULL, MTX_DEF | MTX_RECURSE)
#define	IFNET_WLOCK()		mtx_lock(&ifnet_lock)
#define	IFNET_WUNLOCK()		mtx_unlock(&ifnet_lock)
#define	IFNET_RLOCK()		IFNET_WLOCK()
#define	IFNET_RUNLOCK()		IFNET_WUNLOCK()

struct ifindex_entry {
	struct	ifnet *ife_ifnet;
	struct	ifaddr *ife_ifnet_addr;
	struct cdev *ife_dev;
};

#define ifnet_byindex(idx)	ifindex_table[(idx)].ife_ifnet
/*
 * Given the index, ifaddr_byindex() returns the one and only
 * link-level ifaddr for the interface. You are not supposed to use
 * it to traverse the list of addresses associated to the interface.
 */
#define ifaddr_byindex(idx)	ifindex_table[(idx)].ife_ifnet_addr
#define ifdev_byindex(idx)	ifindex_table[(idx)].ife_dev

extern	struct ifnethead ifnet;
extern	struct ifindex_entry *ifindex_table;
extern	int ifqmaxlen;
extern	struct ifnet *loif;	/* first loopback interface */
extern	int if_index;

int	if_addmulti(struct ifnet *, struct sockaddr *, struct ifmultiaddr **);
int	if_allmulti(struct ifnet *, int);
struct	ifnet* if_alloc(u_char);
void	if_attach(struct ifnet *);
int	if_delmulti(struct ifnet *, struct sockaddr *);
void	if_detach(struct ifnet *);
void	if_purgeaddrs(struct ifnet *);
void	if_down(struct ifnet *);
void	if_free(struct ifnet *);
void	if_free_type(struct ifnet *, u_char);
void	if_initname(struct ifnet *, const char *, int);
void	if_link_state_change(struct ifnet *, int);
int	if_printf(struct ifnet *, const char *, ...) __printflike(2, 3);
int	if_setlladdr(struct ifnet *, const u_char *, int);
void	if_up(struct ifnet *);
/*void	ifinit(void);*/ /* declared in systm.h for main() */
int	ifioctl(struct socket *, u_long, caddr_t, struct thread *);
int	ifpromisc(struct ifnet *, int);
struct	ifnet *ifunit(const char *);

struct	ifaddr *ifa_ifwithaddr(struct sockaddr *);
struct	ifaddr *ifa_ifwithdstaddr(struct sockaddr *);
struct	ifaddr *ifa_ifwithnet(struct sockaddr *);
struct	ifaddr *ifa_ifwithroute(int, struct sockaddr *, struct sockaddr *);
struct	ifaddr *ifaof_ifpforaddr(struct sockaddr *, struct ifnet *);

int	if_simloop(struct ifnet *ifp, struct mbuf *m, int af, int hlen);

typedef	void *if_com_alloc_t(u_char type, struct ifnet *ifp);
typedef	void if_com_free_t(void *com, u_char type);
void	if_register_com_alloc(u_char type, if_com_alloc_t *a, if_com_free_t *f);
void	if_deregister_com_alloc(u_char type);

#define IF_LLADDR(ifp)							\
    LLADDR((struct sockaddr_dl *) ifaddr_byindex((ifp)->if_index)->ifa_addr)

#ifdef DEVICE_POLLING
enum poll_cmd {	POLL_ONLY, POLL_AND_CHECK_STATUS };

typedef	void poll_handler_t(struct ifnet *ifp, enum poll_cmd cmd, int count);
int    ether_poll_register(poll_handler_t *h, struct ifnet *ifp);
int    ether_poll_deregister(struct ifnet *ifp);
#endif /* DEVICE_POLLING */

#endif /* _KERNEL */

#endif /* !_NET_IF_VAR_H_ */
