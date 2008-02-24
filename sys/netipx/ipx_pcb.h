/*-
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2004-2006 Robert N. M. Watson
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
 * Copyright (c) 1995, Mike Mitchell
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
 *	@(#)ipx_pcb.h
 *
 * $FreeBSD: src/sys/netipx/ipx_pcb.h,v 1.29 2007/05/11 10:38:34 rwatson Exp $
 */

#ifndef _NETIPX_IPX_PCB_H_
#define	_NETIPX_IPX_PCB_H_

/*
 * IPX protocol interface control block.
 */
struct ipxpcb {
	LIST_ENTRY(ipxpcb) ipxp_list;
	struct	socket *ipxp_socket;	/* back pointer to socket */
	struct	ipx_addr ipxp_faddr;	/* destination address */
	struct	ipx_addr ipxp_laddr;	/* socket's address */
	caddr_t	ipxp_pcb;		/* protocol specific stuff */
	struct	route ipxp_route;	/* routing information */
	struct	ipx_addr ipxp_lastdst;	/* validate cached route for dg socks*/
	short	ipxp_flags;
	u_char	ipxp_dpt;		/* default packet type for ipx_output */
	u_char	ipxp_rpt;		/* last received packet type by ipx_input() */
	struct	mtx ipxp_mtx;
};

/*
 * Additional IPX pcb-related types and variables.
 */
LIST_HEAD(ipxpcbhead, ipxpcb);
extern struct ipxpcbhead ipxpcb_list;
extern struct ipxpcbhead ipxrawpcb_list;

#ifdef _KERNEL
extern struct mtx	ipxpcb_list_mtx;
#endif

/*
 * IPX/SPX PCB flags.
 */
#define IPXP_IN_ABORT		0x1	/* Calling abort through socket. */
#define IPXP_RAWIN		0x2	/* Show headers on input. */
#define IPXP_RAWOUT		0x4	/* Show header on output. */
#define IPXP_ALL_PACKETS	0x8	/* Turn off higher proto processing. */
#define	IPXP_CHECKSUM		0x10	/* Use checksum on this socket. */
#define	IPXP_DROPPED		0x20	/* Connection dropped. */
#define	IPXP_SPX		0x40	/* SPX PCB. */

#define	IPX_WILDCARD		1

#define ipxp_lport ipxp_laddr.x_port
#define ipxp_fport ipxp_faddr.x_port

#define	sotoipxpcb(so)		((struct ipxpcb *)((so)->so_pcb))

/*
 * Nominal space allocated to an IPX socket.
 */
#define	IPXSNDQ		16384
#define	IPXRCVQ		40960

#ifdef _KERNEL
int	ipx_pcballoc(struct socket *so, struct ipxpcbhead *head,
	    struct thread *p);
int	ipx_pcbbind(struct ipxpcb *ipxp, struct sockaddr *nam,
	    struct thread *p);
int	ipx_pcbconnect(struct ipxpcb *ipxp, struct sockaddr *nam,
	    struct thread *p);
void	ipx_pcbdetach(struct ipxpcb *ipxp);
void	ipx_pcbdisconnect(struct ipxpcb *ipxp);
void	ipx_pcbfree(struct ipxpcb *ipxp);
struct ipxpcb *ipx_pcblookup(struct ipx_addr *faddr, u_short lport, int wildp);
void	ipx_getpeeraddr(struct ipxpcb *ipxp, struct sockaddr **nam);
void	ipx_getsockaddr(struct ipxpcb *ipxp, struct sockaddr **nam);

#define	IPX_LIST_LOCK_INIT()	mtx_init(&ipxpcb_list_mtx, "ipx_list_mtx", \
				    NULL, MTX_DEF | MTX_RECURSE)
#define	IPX_LIST_LOCK()		mtx_lock(&ipxpcb_list_mtx)
#define	IPX_LIST_UNLOCK()	mtx_unlock(&ipxpcb_list_mtx)
#define	IPX_LIST_LOCK_ASSERT()	mtx_assert(&ipxpcb_list_mtx, MA_OWNED)

#define	IPX_LOCK_INIT(ipx)	mtx_init(&(ipx)->ipxp_mtx, "ipx_mtx", NULL, \
				    MTX_DEF)
#define	IPX_LOCK_DESTROY(ipx)	mtx_destroy(&(ipx)->ipxp_mtx)
#define	IPX_LOCK(ipx)		mtx_lock(&(ipx)->ipxp_mtx)
#define	IPX_UNLOCK(ipx)		mtx_unlock(&(ipx)->ipxp_mtx)
#define	IPX_LOCK_ASSERT(ipx)	mtx_assert(&(ipx)->ipxp_mtx, MA_OWNED)
#endif /* _KERNEL */

#endif /* !_NETIPX_IPX_PCB_H_ */
