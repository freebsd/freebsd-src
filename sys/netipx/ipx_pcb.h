/*
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
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
 *	@(#)ipx_pcb.h
 *
 * $FreeBSD: src/sys/netipx/ipx_pcb.h,v 1.15 1999/12/29 04:46:08 peter Exp $
 */

#ifndef _NETIPX_IPX_PCB_H_
#define	_NETIPX_IPX_PCB_H_

/*
 * IPX protocol interface control block.
 */
struct ipxpcb {
	struct	ipxpcb *ipxp_next;	/* doubly linked list */
	struct	ipxpcb *ipxp_prev;
	struct	ipxpcb *ipxp_head;
	struct	socket *ipxp_socket;	/* back pointer to socket */
	struct	ipx_addr ipxp_faddr;	/* destination address */
	struct	ipx_addr ipxp_laddr;	/* socket's address */
	caddr_t	ipxp_pcb;		/* protocol specific stuff */
	struct	route ipxp_route;	/* routing information */
	struct	ipx_addr ipxp_lastdst;	/* validate cached route for dg socks*/
	long	ipxp_notify_param;	/* extra info passed via ipx_pcbnotify*/
	short	ipxp_flags;
	u_char	ipxp_dpt;		/* default packet type for ipx_output */
	u_char	ipxp_rpt;		/* last received packet type by ipx_input() */
};

/* possible flags */

#define IPXP_IN_ABORT		0x1	/* calling abort through socket */
#define IPXP_RAWIN		0x2	/* show headers on input */
#define IPXP_RAWOUT		0x4	/* show header on output */
#define IPXP_ALL_PACKETS	0x8	/* Turn off higher proto processing */
#define	IPXP_CHECKSUM		0x10	/* use checksum on this socket */

#define	IPX_WILDCARD		1

#define ipxp_lport ipxp_laddr.x_port
#define ipxp_fport ipxp_faddr.x_port

#define	sotoipxpcb(so)		((struct ipxpcb *)((so)->so_pcb))

/*
 * Nominal space allocated to a IPX socket.
 */
#define	IPXSNDQ		16384
#define	IPXRCVQ		40960

#ifdef _KERNEL
extern struct ipxpcb ipxpcb;			/* head of list */

int	ipx_pcballoc __P((struct socket *so, struct ipxpcb *head,
			  struct proc *p));
int	ipx_pcbbind __P((struct ipxpcb *ipxp, struct sockaddr *nam,
			 struct proc *p));
int	ipx_pcbconnect __P((struct ipxpcb *ipxp, struct sockaddr *nam,
			    struct proc *p));
void	ipx_pcbdetach __P((struct ipxpcb *ipxp));
void	ipx_pcbdisconnect __P((struct ipxpcb *ipxp));
struct ipxpcb *
	ipx_pcblookup __P((struct ipx_addr *faddr, int lport, int wildp));
void	ipx_pcbnotify __P((struct ipx_addr *dst, int errno,
			   void (*notify)(struct ipxpcb *), long param));
void	ipx_setpeeraddr __P((struct ipxpcb *ipxp, struct sockaddr **nam));
void	ipx_setsockaddr __P((struct ipxpcb *ipxp, struct sockaddr **nam));
#endif /* _KERNEL */

#endif /* !_NETIPX_IPX_PCB_H_ */
