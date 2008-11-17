/*-
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *	@(#)in_pcb.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

#ifndef _NETINET_IN_PCB_H_
#define _NETINET_IN_PCB_H_

#include <sys/queue.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>
#include <sys/_rwlock.h>

#include <net/route.h>

#ifdef _KERNEL
#include <sys/rwlock.h>
#endif

#define	in6pcb		inpcb	/* for KAME src sync over BSD*'s */
#define	in6p_sp		inp_sp	/* for KAME src sync over BSD*'s */
struct inpcbpolicy;

/*
 * struct inpcb is the common protocol control block structure used in most
 * IP transport protocols.
 *
 * Pointers to local and foreign host table entries, local and foreign socket
 * numbers, and pointers up (to a socket structure) and down (to a
 * protocol-specific control block) are stored here.
 */
LIST_HEAD(inpcbhead, inpcb);
LIST_HEAD(inpcbporthead, inpcbport);
typedef	u_quad_t	inp_gen_t;

/*
 * PCB with AF_INET6 null bind'ed laddr can receive AF_INET input packet.
 * So, AF_INET6 null laddr is also used as AF_INET null laddr, by utilizing
 * the following structure.
 */
struct in_addr_4in6 {
	u_int32_t	ia46_pad32[3];
	struct	in_addr	ia46_addr4;
};

/*
 * NOTE: ipv6 addrs should be 64-bit aligned, per RFC 2553.  in_conninfo has
 * some extra padding to accomplish this.
 */
struct in_endpoints {
	u_int16_t	ie_fport;		/* foreign port */
	u_int16_t	ie_lport;		/* local port */
	/* protocol dependent part, local and foreign addr */
	union {
		/* foreign host table entry */
		struct	in_addr_4in6 ie46_foreign;
		struct	in6_addr ie6_foreign;
	} ie_dependfaddr;
	union {
		/* local host table entry */
		struct	in_addr_4in6 ie46_local;
		struct	in6_addr ie6_local;
	} ie_dependladdr;
#define	ie_faddr	ie_dependfaddr.ie46_foreign.ia46_addr4
#define	ie_laddr	ie_dependladdr.ie46_local.ia46_addr4
#define	ie6_faddr	ie_dependfaddr.ie6_foreign
#define	ie6_laddr	ie_dependladdr.ie6_local
};

/*
 * XXX The defines for inc_* are hacks and should be changed to direct
 * references.
 */
struct in_conninfo {
	u_int8_t	inc_flags;
	u_int8_t	inc_len;
	u_int16_t	inc_fibnum;	/* XXX was pad, 16 bits is plenty */
	/* protocol dependent part */
	struct	in_endpoints inc_ie;
};
#define inc_isipv6	inc_flags	/* temp compatability */
#define	inc_fport	inc_ie.ie_fport
#define	inc_lport	inc_ie.ie_lport
#define	inc_faddr	inc_ie.ie_faddr
#define	inc_laddr	inc_ie.ie_laddr
#define	inc6_faddr	inc_ie.ie6_faddr
#define	inc6_laddr	inc_ie.ie6_laddr

struct	icmp6_filter;

/*-
 * struct inpcb captures the network layer state for TCP, UDP, and raw IPv4
 * and IPv6 sockets.  In the case of TCP, further per-connection state is
 * hung off of inp_ppcb most of the time.  Almost all fields of struct inpcb
 * are static after creation or protected by a per-inpcb rwlock, inp_lock.  A
 * few fields also require the global pcbinfo lock for the inpcb to be held,
 * when modified, such as the global connection lists and hashes, as well as
 * binding information (which affects which hash a connection is on).  This
 * model means that connections can be looked up without holding the
 * per-connection lock, which is important for performance when attempting to
 * find the connection for a packet given its IP and port tuple.  Writing to
 * these fields that write locks be held on both the inpcb and global locks.
 *
 * Key:
 * (c) - Constant after initialization
 * (i) - Protected by the inpcb lock
 * (p) - Protected by the pcbinfo lock for the inpcb
 * (s) - Protected by another subsystem's locks
 * (x) - Undefined locking
 *
 * A few other notes:
 *
 * When a read lock is held, stability of the field is guaranteed; to write
 * to a field, a write lock must generally be held.
 *
 * netinet/netinet6-layer code should not assume that the inp_socket pointer
 * is safe to dereference without inp_lock being held, even for protocols
 * other than TCP (where the inpcb persists during TIMEWAIT even after the
 * socket has been freed), or there may be close(2)-related races.
 *
 * The inp_vflag field is overloaded, and would otherwise ideally be (c).
 */
struct inpcb {
	LIST_ENTRY(inpcb) inp_hash;	/* (i/p) hash list */
	LIST_ENTRY(inpcb) inp_list;	/* (i/p) list for all PCBs for proto */
	void	*inp_ppcb;		/* (i) pointer to per-protocol pcb */
	struct	inpcbinfo *inp_pcbinfo;	/* (c) PCB list info */
	struct	socket *inp_socket;	/* (i)  back pointer to socket */
	struct	ucred	*inp_cred;	/* (c) cache of socket cred */

	u_int32_t	inp_flow;	/* (i) IPv6 flow information */
	int	inp_flags;		/* (i) generic IP/datagram flags */

	u_char	inp_vflag;		/* (i) IP version flag (v4/v6) */
#define	INP_IPV4	0x1
#define	INP_IPV6	0x2
#define	INP_IPV6PROTO	0x4		/* opened under IPv6 protocol */
#define	INP_TIMEWAIT	0x8		/* .. probably doesn't go here */
#define	INP_ONESBCAST	0x10		/* send all-ones broadcast */
#define	INP_DROPPED	0x20		/* protocol drop flag */
#define	INP_SOCKREF	0x40		/* strong socket reference */
	u_char	inp_ip_ttl;		/* (i) time to live proto */
	u_char	inp_ip_p;		/* (c) protocol proto */
	u_char	inp_ip_minttl;		/* (i) minimum TTL or drop */
	uint32_t inp_ispare1;		/* (x) connection id / queue id */
	void	*inp_pspare[2];		/* (x) rtentry / general use */

	/* Local and foreign ports, local and foreign addr. */
	struct	in_conninfo inp_inc;

					/* (i/p) list for PCB's local port */
	struct	label *inp_label;	/* (i) MAC label */
	struct	inpcbpolicy *inp_sp;    /* (s) for IPSEC */

	/* Protocol-dependent part; options. */
	struct {
		u_char	inp4_ip_tos;		/* (i) type of service proto */
		struct	mbuf *inp4_options;	/* (i) IP options */
		struct	ip_moptions *inp4_moptions; /* (i) IP multicast options */
	} inp_depend4;
#define	inp_fport	inp_inc.inc_fport
#define	inp_lport	inp_inc.inc_lport
#define	inp_faddr	inp_inc.inc_faddr
#define	inp_laddr	inp_inc.inc_laddr
#define	inp_ip_tos	inp_depend4.inp4_ip_tos
#define	inp_options	inp_depend4.inp4_options
#define	inp_moptions	inp_depend4.inp4_moptions
	struct {
		/* (i) IP options */
		struct	mbuf *inp6_options;
		/* (i) IP6 options for outgoing packets */
		struct	ip6_pktopts *inp6_outputopts;
		/* (i) IP multicast options */
		struct	ip6_moptions *inp6_moptions;
		/* (i) ICMPv6 code type filter */
		struct	icmp6_filter *inp6_icmp6filt;
		/* (i) IPV6_CHECKSUM setsockopt */
		int	inp6_cksum;
		short	inp6_hops;
	} inp_depend6;
	LIST_ENTRY(inpcb) inp_portlist;	/* (i/p) */
	struct	inpcbport *inp_phd;	/* (i/p) head of this list */
#define inp_zero_size offsetof(struct inpcb, inp_gencnt)
	inp_gen_t	inp_gencnt;	/* (c) generation count of this instance */
	struct rwlock	inp_lock;

#define	in6p_faddr	inp_inc.inc6_faddr
#define	in6p_laddr	inp_inc.inc6_laddr
#define	in6p_hops	inp_depend6.inp6_hops	/* default hop limit */
#define	in6p_ip6_nxt	inp_ip_p
#define	in6p_flowinfo	inp_flow
#define	in6p_vflag	inp_vflag
#define	in6p_options	inp_depend6.inp6_options
#define	in6p_outputopts	inp_depend6.inp6_outputopts
#define	in6p_moptions	inp_depend6.inp6_moptions
#define	in6p_icmp6filt	inp_depend6.inp6_icmp6filt
#define	in6p_cksum	inp_depend6.inp6_cksum
#define	in6p_flags	inp_flags  /* for KAME src sync over BSD*'s */
#define	in6p_socket	inp_socket  /* for KAME src sync over BSD*'s */
#define	in6p_lport	inp_lport  /* for KAME src sync over BSD*'s */
#define	in6p_fport	inp_fport  /* for KAME src sync over BSD*'s */
#define	in6p_ppcb	inp_ppcb  /* for KAME src sync over BSD*'s */
};
/*
 * The range of the generation count, as used in this implementation, is 9e19.
 * We would have to create 300 billion connections per second for this number
 * to roll over in a year.  This seems sufficiently unlikely that we simply
 * don't concern ourselves with that possibility.
 */

/*
 * Interface exported to userland by various protocols which use inpcbs.  Hack
 * alert -- only define if struct xsocket is in scope.
 */
#ifdef _SYS_SOCKETVAR_H_
struct	xinpcb {
	size_t	xi_len;		/* length of this structure */
	struct	inpcb xi_inp;
	struct	xsocket xi_socket;
	u_quad_t	xi_alignment_hack;
};

struct	xinpgen {
	size_t	xig_len;	/* length of this structure */
	u_int	xig_count;	/* number of PCBs at this time */
	inp_gen_t xig_gen;	/* generation count at this time */
	so_gen_t xig_sogen;	/* socket generation count at this time */
};
#endif /* _SYS_SOCKETVAR_H_ */

struct inpcbport {
	LIST_ENTRY(inpcbport) phd_hash;
	struct inpcbhead phd_pcblist;
	u_short phd_port;
};

/*
 * Global data structure for each high-level protocol (UDP, TCP, ...) in both
 * IPv4 and IPv6.  Holds inpcb lists and information for managing them.
 */
struct inpcbinfo {
	/*
	 * Global list of inpcbs on the protocol.
	 */
	struct inpcbhead	*ipi_listhead;
	u_int			 ipi_count;

	/*
	 * Global hash of inpcbs, hashed by local and foreign addresses and
	 * port numbers.
	 */
	struct inpcbhead	*ipi_hashbase;
	u_long			 ipi_hashmask;

	/*
	 * Global hash of inpcbs, hashed by only local port number.
	 */
	struct inpcbporthead	*ipi_porthashbase;
	u_long			 ipi_porthashmask;

	/*
	 * Fields associated with port lookup and allocation.
	 */
	u_short			 ipi_lastport;
	u_short			 ipi_lastlow;
	u_short			 ipi_lasthi;

	/*
	 * UMA zone from which inpcbs are allocated for this protocol.
	 */
	struct	uma_zone	*ipi_zone;

	/*
	 * Generation count--incremented each time a connection is allocated
	 * or freed.
	 */
	u_quad_t		 ipi_gencnt;
	struct rwlock		 ipi_lock;

	/*
	 * vimage 1
	 * general use 1
	 */
	void 			*ipi_pspare[2];
};

#define INP_LOCK_INIT(inp, d, t) \
	rw_init_flags(&(inp)->inp_lock, (t), RW_RECURSE |  RW_DUPOK)
#define INP_LOCK_DESTROY(inp)	rw_destroy(&(inp)->inp_lock)
#define INP_RLOCK(inp)		rw_rlock(&(inp)->inp_lock)
#define INP_WLOCK(inp)		rw_wlock(&(inp)->inp_lock)
#define INP_TRY_RLOCK(inp)	rw_try_rlock(&(inp)->inp_lock)
#define INP_TRY_WLOCK(inp)	rw_try_wlock(&(inp)->inp_lock)
#define INP_RUNLOCK(inp)	rw_runlock(&(inp)->inp_lock)
#define INP_WUNLOCK(inp)	rw_wunlock(&(inp)->inp_lock)
#define INP_LOCK_ASSERT(inp)	rw_assert(&(inp)->inp_lock, RA_LOCKED)
#define	INP_RLOCK_ASSERT(inp)	rw_assert(&(inp)->inp_lock, RA_RLOCKED)
#define	INP_WLOCK_ASSERT(inp)	rw_assert(&(inp)->inp_lock, RA_WLOCKED)
#define	INP_UNLOCK_ASSERT(inp)	rw_assert(&(inp)->inp_lock, RA_UNLOCKED)

#ifdef _KERNEL
/*
 * These locking functions are for inpcb consumers outside of sys/netinet,
 * more specifically, they were added for the benefit of TOE drivers. The
 * macros are reserved for use by the stack.
 */
void inp_wlock(struct inpcb *);
void inp_wunlock(struct inpcb *);
void inp_rlock(struct inpcb *);
void inp_runlock(struct inpcb *);

#ifdef INVARIANTS
void inp_lock_assert(struct inpcb *);
void inp_unlock_assert(struct inpcb *);
#else
static __inline void
inp_lock_assert(struct inpcb *inp __unused)
{
}

static __inline void
inp_unlock_assert(struct inpcb *inp __unused)
{
}

#endif

void	inp_apply_all(void (*func)(struct inpcb *, void *), void *arg);
int 	inp_ip_tos_get(const struct inpcb *inp);
void 	inp_ip_tos_set(struct inpcb *inp, int val);
struct socket *
	inp_inpcbtosocket(struct inpcb *inp);
struct tcpcb *
	inp_inpcbtotcpcb(struct inpcb *inp);
void 	inp_4tuple_get(struct inpcb *inp, uint32_t *laddr, uint16_t *lp,
		uint32_t *faddr, uint16_t *fp);

#endif /* _KERNEL */

#define INP_INFO_LOCK_INIT(ipi, d) \
	rw_init_flags(&(ipi)->ipi_lock, (d), RW_RECURSE)
#define INP_INFO_LOCK_DESTROY(ipi)  rw_destroy(&(ipi)->ipi_lock)
#define INP_INFO_RLOCK(ipi)	rw_rlock(&(ipi)->ipi_lock)
#define INP_INFO_WLOCK(ipi)	rw_wlock(&(ipi)->ipi_lock)
#define INP_INFO_TRY_RLOCK(ipi)	rw_try_rlock(&(ipi)->ipi_lock)
#define INP_INFO_TRY_WLOCK(ipi)	rw_try_wlock(&(ipi)->ipi_lock)
#define INP_INFO_RUNLOCK(ipi)	rw_runlock(&(ipi)->ipi_lock)
#define INP_INFO_WUNLOCK(ipi)	rw_wunlock(&(ipi)->ipi_lock)
#define	INP_INFO_LOCK_ASSERT(ipi)	rw_assert(&(ipi)->ipi_lock, RA_LOCKED)
#define INP_INFO_RLOCK_ASSERT(ipi)	rw_assert(&(ipi)->ipi_lock, RA_RLOCKED)
#define INP_INFO_WLOCK_ASSERT(ipi)	rw_assert(&(ipi)->ipi_lock, RA_WLOCKED)
#define INP_INFO_UNLOCK_ASSERT(ipi)	rw_assert(&(ipi)->ipi_lock, RA_UNLOCKED)

#define INP_PCBHASH(faddr, lport, fport, mask) \
	(((faddr) ^ ((faddr) >> 16) ^ ntohs((lport) ^ (fport))) & (mask))
#define INP_PCBPORTHASH(lport, mask) \
	(ntohs((lport)) & (mask))

/* flags in inp_flags: */
#define	INP_RECVOPTS		0x01	/* receive incoming IP options */
#define	INP_RECVRETOPTS		0x02	/* receive IP options for reply */
#define	INP_RECVDSTADDR		0x04	/* receive IP dst address */
#define	INP_HDRINCL		0x08	/* user supplies entire IP header */
#define	INP_HIGHPORT		0x10	/* user wants "high" port binding */
#define	INP_LOWPORT		0x20	/* user wants "low" port binding */
#define	INP_ANONPORT		0x40	/* port chosen for user */
#define	INP_RECVIF		0x80	/* receive incoming interface */
#define	INP_MTUDISC		0x100	/* user can do MTU discovery */
#define	INP_FAITH		0x200	/* accept FAITH'ed connections */
#define	INP_RECVTTL		0x400	/* receive incoming IP TTL */
#define	INP_DONTFRAG		0x800	/* don't fragment packet */

#define IN6P_IPV6_V6ONLY	0x008000 /* restrict AF_INET6 socket for v6 */

#define	IN6P_PKTINFO		0x010000 /* receive IP6 dst and I/F */
#define	IN6P_HOPLIMIT		0x020000 /* receive hoplimit */
#define	IN6P_HOPOPTS		0x040000 /* receive hop-by-hop options */
#define	IN6P_DSTOPTS		0x080000 /* receive dst options after rthdr */
#define	IN6P_RTHDR		0x100000 /* receive routing header */
#define	IN6P_RTHDRDSTOPTS	0x200000 /* receive dstoptions before rthdr */
#define	IN6P_TCLASS		0x400000 /* receive traffic class value */
#define	IN6P_AUTOFLOWLABEL	0x800000 /* attach flowlabel automatically */
#define	IN6P_RFC2292		0x40000000 /* used RFC2292 API on the socket */
#define	IN6P_MTU		0x80000000 /* receive path MTU */

#define	INP_CONTROLOPTS		(INP_RECVOPTS|INP_RECVRETOPTS|INP_RECVDSTADDR|\
				 INP_RECVIF|INP_RECVTTL|\
				 IN6P_PKTINFO|IN6P_HOPLIMIT|IN6P_HOPOPTS|\
				 IN6P_DSTOPTS|IN6P_RTHDR|IN6P_RTHDRDSTOPTS|\
				 IN6P_TCLASS|IN6P_AUTOFLOWLABEL|IN6P_RFC2292|\
				 IN6P_MTU)
#define	INP_UNMAPPABLEOPTS	(IN6P_HOPOPTS|IN6P_DSTOPTS|IN6P_RTHDR|\
				 IN6P_TCLASS|IN6P_AUTOFLOWLABEL)

 /* for KAME src sync over BSD*'s */
#define	IN6P_HIGHPORT		INP_HIGHPORT
#define	IN6P_LOWPORT		INP_LOWPORT
#define	IN6P_ANONPORT		INP_ANONPORT
#define	IN6P_RECVIF		INP_RECVIF
#define	IN6P_MTUDISC		INP_MTUDISC
#define	IN6P_FAITH		INP_FAITH
#define	IN6P_CONTROLOPTS INP_CONTROLOPTS
	/*
	 * socket AF version is {newer than,or include}
	 * actual datagram AF version
	 */

#define	INPLOOKUP_WILDCARD	1
#define	sotoinpcb(so)	((struct inpcb *)(so)->so_pcb)
#define	sotoin6pcb(so)	sotoinpcb(so) /* for KAME src sync over BSD*'s */

#define	INP_SOCKAF(so) so->so_proto->pr_domain->dom_family

#define	INP_CHECK_SOCKAF(so, af)	(INP_SOCKAF(so) == af)

#ifdef _KERNEL
extern int	ipport_reservedhigh;
extern int	ipport_reservedlow;
extern int	ipport_lowfirstauto;
extern int	ipport_lowlastauto;
extern int	ipport_firstauto;
extern int	ipport_lastauto;
extern int	ipport_hifirstauto;
extern int	ipport_hilastauto;
extern int	ipport_randomized;
extern int	ipport_stoprandom;
extern int	ipport_tcpallocs;
extern struct callout ipport_tick_callout;

void	in_pcbpurgeif0(struct inpcbinfo *, struct ifnet *);
int	in_pcballoc(struct socket *, struct inpcbinfo *);
int	in_pcbbind(struct inpcb *, struct sockaddr *, struct ucred *);
int	in_pcbbind_setup(struct inpcb *, struct sockaddr *, in_addr_t *,
	    u_short *, struct ucred *);
int	in_pcbconnect(struct inpcb *, struct sockaddr *, struct ucred *);
int	in_pcbconnect_setup(struct inpcb *, struct sockaddr *, in_addr_t *,
	    u_short *, in_addr_t *, u_short *, struct inpcb **,
	    struct ucred *);
void	in_pcbdetach(struct inpcb *);
void	in_pcbdisconnect(struct inpcb *);
void	in_pcbdrop(struct inpcb *);
void	in_pcbfree(struct inpcb *);
int	in_pcbinshash(struct inpcb *);
struct inpcb *
	in_pcblookup_local(struct inpcbinfo *,
	    struct in_addr, u_short, int, struct ucred *);
struct inpcb *
	in_pcblookup_hash(struct inpcbinfo *, struct in_addr, u_int,
	    struct in_addr, u_int, int, struct ifnet *);
void	in_pcbnotifyall(struct inpcbinfo *pcbinfo, struct in_addr,
	    int, struct inpcb *(*)(struct inpcb *, int));
void	in_pcbrehash(struct inpcb *);
void	in_pcbsetsolabel(struct socket *so);
int	in_getpeeraddr(struct socket *so, struct sockaddr **nam);
int	in_getsockaddr(struct socket *so, struct sockaddr **nam);
struct sockaddr *
	in_sockaddr(in_port_t port, struct in_addr *addr);
void	in_pcbsosetlabel(struct socket *so);
void	in_pcbremlists(struct inpcb *inp);
void	ipport_tick(void *xtp);

/*
 * Debugging routines compiled in when DDB is present.
 */
void	db_print_inpcb(struct inpcb *inp, const char *name, int indent);

#endif /* _KERNEL */

#endif /* !_NETINET_IN_PCB_H_ */
