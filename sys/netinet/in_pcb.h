/*
 * Copyright (c) 1982, 1986, 1990 Regents of the University of California.
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
 *	from: @(#)in_pcb.h	7.6 (Berkeley) 6/28/90
 *	$Id: in_pcb.h,v 1.5 1993/11/25 01:35:06 wollman Exp $
 */

#ifndef _NETINET_IN_PCB_H_
#define _NETINET_IN_PCB_H_ 1

/*
 * Common structure pcb for internet protocol implementation.
 * Here are stored pointers to local and foreign host table
 * entries, local and foreign socket numbers, and pointers
 * up (to a socket structure) and down (to a protocol-specific)
 * control block.
 */
struct inpcb {
	struct	inpcb *inp_next,*inp_prev;
					/* pointers to other pcb's */
	struct	inpcb *inp_head;	/* pointer back to chain of inpcb's
					   for this protocol */
	struct	in_addr inp_faddr;	/* foreign host table entry */
	u_short	inp_fport;		/* foreign port */
	struct	in_addr inp_laddr;	/* local host table entry */
	u_short	inp_lport;		/* local port */
	struct	socket *inp_socket;	/* back pointer to socket */
	caddr_t	inp_ppcb;		/* pointer to per-protocol pcb */
	struct	route inp_route;	/* placeholder for routing entry */
	int	inp_flags;		/* generic IP/datagram flags */
	struct	ip inp_ip;		/* header prototype; should have more */
	struct	mbuf *inp_options;	/* IP options */
#ifdef MTUDISC
	int	inp_pmtu;		/* path mtu if INP_MTUDISCOVERED */
	int	inp_mtutimer;		/* decremented once a minute to
					   try to increase mtu */
	int	(*inp_mtunotify)(struct inpcb *, int);
				/* function to call when MTU may have
				 * changed */
#endif /* MTUDISC */
};

/* flags in inp_flags: */
#define	INP_RECVOPTS		0x01	/* receive incoming IP options */
#define	INP_RECVRETOPTS		0x02	/* receive IP options for reply */
#define	INP_RECVDSTADDR		0x04	/* receive IP dst address */
#ifdef MTUDISC
#define	INP_DISCOVERMTU		0x08	/* practice Path MTU discovery */
#define	INP_MTUDISCOVERED	0x10	/* we were able to get such a route */
#endif /* MTUDISC */
#define	INP_CONTROLOPTS		(INP_RECVOPTS|INP_RECVRETOPTS|INP_RECVDSTADDR)

#ifdef sotorawcb
/*
 * Common structure pcb for raw internet protocol access.
 * Here are internet specific extensions to the raw control block,
 * and space is allocated to the necessary sockaddrs.
 */
struct raw_inpcb {
	struct	rawcb rinp_rcb;	/* common control block prefix */
	struct	mbuf *rinp_options;	/* IP options */
	int	rinp_flags;		/* flags, e.g. raw sockopts */
#define	RINPF_HDRINCL	0x1		/* user supplies entire IP header */
	struct	sockaddr_in rinp_faddr;	/* foreign address */
	struct	sockaddr_in rinp_laddr;	/* local address */
	struct	route rinp_route;	/* placeholder for routing entry */
};
#endif

#define	INPLOOKUP_WILDCARD	1
#define	INPLOOKUP_SETLOCAL	2

#define	sotoinpcb(so)	((struct inpcb *)(so)->so_pcb)
#define	sotorawinpcb(so)	((struct raw_inpcb *)(so)->so_pcb)

#ifdef KERNEL
/* From in_pcb.h: */
extern int in_pcballoc(struct socket *, struct inpcb *);
extern int in_pcbbind(struct inpcb *, struct mbuf *);
extern int in_pcbconnect(struct inpcb *, struct mbuf *);
extern void in_pcbdisconnect(struct inpcb *);
extern void in_pcbdetach(struct inpcb *);
extern void in_setsockaddr(struct inpcb *, struct mbuf *);
extern void in_setpeeraddr(struct inpcb *, struct mbuf *);
extern void in_pcbnotify(struct inpcb *, struct sockaddr *, int, struct in_addr, int, int, void (*)(struct inpcb *, int));
extern void in_losing(struct inpcb *);
extern void in_rtchange(struct inpcb *, int);
extern struct inpcb *in_pcblookup(struct inpcb *, struct in_addr, int, struct in_addr, int, int);


#ifdef MTUDISC
extern void	in_pcbmtu(struct inpcb *);
extern void	in_mtunotify(struct inpcb *);
extern void	in_bumpmtu(struct inpcb *);
#endif /* MTUDISC */
#endif /* KERNEL */
#endif /* _NETINET_IN_PCB_H_ */
