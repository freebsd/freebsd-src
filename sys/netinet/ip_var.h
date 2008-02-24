/*-
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)ip_var.h	8.2 (Berkeley) 1/9/95
 * $FreeBSD: src/sys/netinet/ip_var.h,v 1.101 2007/06/12 16:24:53 bms Exp $
 */

#ifndef _NETINET_IP_VAR_H_
#define	_NETINET_IP_VAR_H_

#include <sys/queue.h>

/*
 * Overlay for ip header used by other protocols (tcp, udp).
 */
struct ipovly {
	u_char	ih_x1[9];		/* (unused) */
	u_char	ih_pr;			/* protocol */
	u_short	ih_len;			/* protocol length */
	struct	in_addr ih_src;		/* source internet address */
	struct	in_addr ih_dst;		/* destination internet address */
};

#ifdef _KERNEL
/*
 * Ip reassembly queue structure.  Each fragment
 * being reassembled is attached to one of these structures.
 * They are timed out after ipq_ttl drops to 0, and may also
 * be reclaimed if memory becomes tight.
 */
struct ipq {
	TAILQ_ENTRY(ipq) ipq_list;	/* to other reass headers */
	u_char	ipq_ttl;		/* time for reass q to live */
	u_char	ipq_p;			/* protocol of this fragment */
	u_short	ipq_id;			/* sequence id for reassembly */
	struct mbuf *ipq_frags;		/* to ip headers of fragments */
	struct	in_addr ipq_src,ipq_dst;
	u_char	ipq_nfrags;		/* # frags in this packet */
	struct label *ipq_label;	/* MAC label */
};
#endif /* _KERNEL */

/*
 * Structure stored in mbuf in inpcb.ip_options
 * and passed to ip_output when ip options are in use.
 * The actual length of the options (including ipopt_dst)
 * is in m_len.
 */
#define MAX_IPOPTLEN	40

struct ipoption {
	struct	in_addr ipopt_dst;	/* first-hop dst if source routed */
	char	ipopt_list[MAX_IPOPTLEN];	/* options proper */
};

/*
 * Multicast source list entry.
 */
struct in_msource {
	TAILQ_ENTRY(in_msource) ims_next;	/* next source */
	struct sockaddr_storage ims_addr;	/* address of this source */
};

/*
 * Multicast filter descriptor; there is one instance per group membership
 * on a socket, allocated as an expandable vector hung off ip_moptions.
 * struct in_multi contains separate IPv4-stack-wide state for IGMPv3.
 */
struct in_mfilter {
	uint16_t	imf_fmode;	/* filter mode for this socket/group */
	uint16_t	imf_nsources;	/* # of sources for this socket/group */
	TAILQ_HEAD(, in_msource) imf_sources;	/* source list */
};

/*
 * Structure attached to inpcb.ip_moptions and
 * passed to ip_output when IP multicast options are in use.
 * This structure is lazy-allocated.
 */
struct ip_moptions {
	struct	ifnet *imo_multicast_ifp; /* ifp for outgoing multicasts */
	struct in_addr imo_multicast_addr; /* ifindex/addr on MULTICAST_IF */
	u_long	imo_multicast_vif;	/* vif num outgoing multicasts */
	u_char	imo_multicast_ttl;	/* TTL for outgoing multicasts */
	u_char	imo_multicast_loop;	/* 1 => hear sends if a member */
	u_short	imo_num_memberships;	/* no. memberships this socket */
	u_short	imo_max_memberships;	/* max memberships this socket */
	struct	in_multi **imo_membership;	/* group memberships */
	struct	in_mfilter *imo_mfilters;	/* source filters */
};

struct	ipstat {
	u_long	ips_total;		/* total packets received */
	u_long	ips_badsum;		/* checksum bad */
	u_long	ips_tooshort;		/* packet too short */
	u_long	ips_toosmall;		/* not enough data */
	u_long	ips_badhlen;		/* ip header length < data size */
	u_long	ips_badlen;		/* ip length < ip header length */
	u_long	ips_fragments;		/* fragments received */
	u_long	ips_fragdropped;	/* frags dropped (dups, out of space) */
	u_long	ips_fragtimeout;	/* fragments timed out */
	u_long	ips_forward;		/* packets forwarded */
	u_long	ips_fastforward;	/* packets fast forwarded */
	u_long	ips_cantforward;	/* packets rcvd for unreachable dest */
	u_long	ips_redirectsent;	/* packets forwarded on same net */
	u_long	ips_noproto;		/* unknown or unsupported protocol */
	u_long	ips_delivered;		/* datagrams delivered to upper level*/
	u_long	ips_localout;		/* total ip packets generated here */
	u_long	ips_odropped;		/* lost packets due to nobufs, etc. */
	u_long	ips_reassembled;	/* total packets reassembled ok */
	u_long	ips_fragmented;		/* datagrams successfully fragmented */
	u_long	ips_ofragments;		/* output fragments created */
	u_long	ips_cantfrag;		/* don't fragment flag was set, etc. */
	u_long	ips_badoptions;		/* error in option processing */
	u_long	ips_noroute;		/* packets discarded due to no route */
	u_long	ips_badvers;		/* ip version != 4 */
	u_long	ips_rawout;		/* total raw ip packets generated */
	u_long	ips_toolong;		/* ip length > max ip packet size */
	u_long	ips_notmember;		/* multicasts for unregistered grps */
	u_long	ips_nogif;		/* no match gif found */
	u_long	ips_badaddr;		/* invalid address on header */
};

#ifdef _KERNEL

/* flags passed to ip_output as last parameter */
#define	IP_FORWARDING		0x1		/* most of ip header exists */
#define	IP_RAWOUTPUT		0x2		/* raw ip header exists */
#define	IP_SENDONES		0x4		/* send all-ones broadcast */
#define	IP_SENDTOIF		0x8		/* send on specific ifnet */
#define IP_ROUTETOIF		SO_DONTROUTE	/* 0x10 bypass routing tables */
#define IP_ALLOWBROADCAST	SO_BROADCAST	/* 0x20 can send broadcast packets */

/*
 * mbuf flag used by ip_fastfwd
 */
#define	M_FASTFWD_OURS		M_PROTO1	/* changed dst to local */

#ifdef __NO_STRICT_ALIGNMENT
#define IP_HDR_ALIGNED_P(ip)	1
#else
#define IP_HDR_ALIGNED_P(ip)	((((intptr_t) (ip)) & 3) == 0)
#endif

struct ip;
struct inpcb;
struct route;
struct sockopt;

extern struct	ipstat	ipstat;
extern u_short	ip_id;			/* ip packet ctr, for ids */
extern int	ip_defttl;		/* default IP ttl */
extern int	ipforwarding;		/* ip forwarding */
#ifdef IPSTEALTH
extern int	ipstealth;		/* stealth forwarding */
#endif
extern u_char	ip_protox[];
extern struct socket *ip_rsvpd;		/* reservation protocol daemon */
extern struct socket *ip_mrouter;	/* multicast routing daemon */
extern int	(*legal_vif_num)(int);
extern u_long	(*ip_mcast_src)(int);
extern int rsvp_on;
extern struct	pr_usrreqs rip_usrreqs;

void	inp_freemoptions(struct ip_moptions *);
int	inp_getmoptions(struct inpcb *, struct sockopt *);
int	inp_setmoptions(struct inpcb *, struct sockopt *);

int	ip_ctloutput(struct socket *, struct sockopt *sopt);
void	ip_drain(void);
void	ip_fini(void *xtp);
int	ip_fragment(struct ip *ip, struct mbuf **m_frag, int mtu,
	    u_long if_hwassist_flags, int sw_csum);
void	ip_forward(struct mbuf *m, int srcrt);
void	ip_init(void);
extern int
	(*ip_mforward)(struct ip *, struct ifnet *, struct mbuf *,
	    struct ip_moptions *);
int	ip_output(struct mbuf *,
	    struct mbuf *, struct route *, int, struct ip_moptions *,
	    struct inpcb *);
int	ipproto_register(u_char);
int	ipproto_unregister(u_char);
struct mbuf *
	ip_reass(struct mbuf *);
struct in_ifaddr *
	ip_rtaddr(struct in_addr);
void	ip_savecontrol(struct inpcb *, struct mbuf **, struct ip *,
	    struct mbuf *);
void	ip_slowtimo(void);
u_int16_t	ip_randomid(void);
int	rip_ctloutput(struct socket *, struct sockopt *);
void	rip_ctlinput(int, struct sockaddr *, void *);
void	rip_init(void);
void	rip_input(struct mbuf *, int);
int	rip_output(struct mbuf *, struct socket *, u_long);
void	ipip_input(struct mbuf *, int);
void	rsvp_input(struct mbuf *, int);
int	ip_rsvp_init(struct socket *);
int	ip_rsvp_done(void);
extern int	(*ip_rsvp_vif)(struct socket *, struct sockopt *);
extern void	(*ip_rsvp_force_done)(struct socket *);
extern void	(*rsvp_input_p)(struct mbuf *m, int off);

extern	struct pfil_head inet_pfil_hook;	/* packet filter hooks */

void	in_delayed_cksum(struct mbuf *m);

static __inline uint16_t ip_newid(void);
extern int ip_do_randomid;

static __inline uint16_t
ip_newid(void)
{
	if (ip_do_randomid)
		return ip_randomid();

	return htons(ip_id++);
}

#endif /* _KERNEL */

#endif /* !_NETINET_IP_VAR_H_ */
