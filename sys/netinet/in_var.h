/*
 * Copyright (c) 1985, 1986 Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	from: @(#)in_var.h	7.6 (Berkeley) 6/28/90
 *	$Id: in_var.h,v 1.6 1993/12/19 21:43:26 wollman Exp $
 */

#ifndef _NETINET_IN_VAR_H_
#define _NETINET_IN_VAR_H_ 1

/*
 * Interface address, Internet version.  One of these structures
 * is allocated for each interface with an Internet address.
 * The ifaddr structure contains the protocol-independent part
 * of the structure and is assumed to be first.
 */
struct in_ifaddr {
	struct	ifaddr ia_ifa;		/* protocol-independent info */
#define	ia_ifp		ia_ifa.ifa_ifp
#define ia_flags	ia_ifa.ifa_flags
					/* ia_{,sub}net{,mask} in host order */
	u_long	ia_net;			/* network number of interface */
	u_long	ia_netmask;		/* mask of net part */
	u_long	ia_subnet;		/* subnet number, including net */
	u_long	ia_subnetmask;		/* mask of subnet part */
	struct	in_addr ia_netbroadcast; /* to recognize net broadcasts */
	struct	in_ifaddr *ia_next;	/* next in list of internet addresses */
	struct	sockaddr_in ia_addr;	/* reserve space for interface name */
	struct	sockaddr_in ia_dstaddr; /* reserve space for broadcast addr */
#define	ia_broadaddr	ia_dstaddr
	struct	sockaddr_in ia_sockmask; /* reserve space for general netmask */
};

struct	in_aliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	sockaddr_in ifra_addr;
	struct	sockaddr_in ifra_broadaddr;
#define ifra_dstaddr ifra_broadaddr
	struct	sockaddr_in ifra_mask;
};
/*
 * Given a pointer to an in_ifaddr (ifaddr),
 * return a pointer to the addr as a sockaddr_in.
 */
#define	IA_SIN(ia) (&(((struct in_ifaddr *)(ia))->ia_addr))

struct ip;			/* forward declaration */
typedef void in_input_t(struct mbuf *, int);
/*
 * Grrr... `netstat' expects to be able to include this file
 * with KERNEL defined, to get all sorts of interesting structures,
 * but without having to get all these prototypes.  (Well, it's not
 * really netstat's fault, but this should get fixed when KERNEL gets
 * changed to _KERNEL.)
 */
struct socket;
typedef int in_output_t(struct mbuf *, struct socket *);
typedef void in_ctlinput_t(int, struct sockaddr *, struct ip *);
typedef int in_ctloutput_t(int, struct socket *, int, int, struct mbuf **);

/*
 * This structure is a pun for `struct protosw'. The difference is that it
 * has appropriate interprotocol hook prototypes for the Internet family.
 */
struct in_protosw {
	short	pr_type;		/* socket type used for */
	struct	domain *pr_domain;	/* domain protocol a member of */
	short	pr_protocol;		/* protocol number */
	short	pr_flags;		/* see below */
/* protocol-protocol hooks */
	in_input_t *pr_input;
	in_output_t *pr_output;
	in_ctlinput_t *pr_ctlinput;
	in_ctloutput_t *pr_ctloutput;
/* user-protocol hook */
	int	(*pr_usrreq)(struct socket *, int, struct mbuf *, 
			     struct mbuf *, struct mbuf *, struct mbuf *);
/* utility hooks */
	void	(*pr_init)(void); /* initialization hook */
	void	(*pr_fasttimo)(void);	/* fast timeout (200ms) */
	void	(*pr_slowtimo)(void);	/* slow timeout (500ms) */
	void	(*pr_drain)(void); /* flush any excess space possible */
};


#ifdef	KERNEL
extern struct in_ifaddr *in_ifaddr;
extern struct in_ifaddr *in_iaonnetof(u_long);
extern struct in_ifaddr *ifptoia(struct ifnet *);
extern int in_ifinit(struct ifnet *, struct in_ifaddr *, struct sockaddr_in *, int);

extern int in_cksum(struct mbuf *, int);

extern struct ifqueue ipintrq;	/* ip packet input queue */
extern struct in_protosw inetsw[];
extern struct domain inetdomain;
extern u_char ip_protox[];
extern u_char inetctlerrmap[];
extern struct in_addr zeroin_addr;

/* From in_var.c: */
struct route;
extern int subnetsarelocal;	/* obsolescent */
extern int ipqmaxlen;
extern u_long *ip_ifmatrix;
extern int ipforwarding;
extern struct sockaddr_in ipaddr;
extern struct route ipforward_rt;
extern int ipsendredirects;


#ifdef MTUDISC
extern unsigned in_nextmtu(unsigned, int);
extern int	in_routemtu(struct route *);
extern void	in_mtureduce(struct in_addr, unsigned);
extern void	in_mtutimer(caddr_t, int);
#endif /* MTUDISC */
#endif /* KERNEL */
#endif /* _NETINET_IN_VAR_H_ */
