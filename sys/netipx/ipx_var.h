/*-
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
 *	@(#)ipx_var.h
 *
 * $FreeBSD$
 */

#ifndef _NETIPX_IPX_VAR_H_
#define _NETIPX_IPX_VAR_H_

/*
 * IPX Kernel Structures and Variables
 */
struct	ipxstat {
	u_long	ipxs_total;		/* total packets received */
	u_long	ipxs_badsum;		/* checksum bad */
	u_long	ipxs_tooshort;		/* packet too short */
	u_long	ipxs_toosmall;		/* not enough data */
	u_long	ipxs_forward;		/* packets forwarded */
	u_long	ipxs_cantforward;	/* packets rcvd for unreachable dest */
	u_long	ipxs_delivered;		/* datagrams delivered to upper level*/
	u_long	ipxs_localout;		/* total ipx packets generated here */
	u_long	ipxs_odropped;		/* lost packets due to nobufs, etc. */
	u_long	ipxs_noroute;		/* packets discarded due to no route */
	u_long	ipxs_mtutoosmall;	/* the interface mtu is too small */
};

#ifdef _KERNEL

#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_ipx);
SYSCTL_DECL(_net_ipx_ipx);
#endif

extern int ipxcksum;
extern long ipx_pexseq;
extern struct ipxstat ipxstat;
extern struct ipxpcb ipxrawpcb;
extern struct pr_usrreqs ipx_usrreqs;
extern struct pr_usrreqs ripx_usrreqs;
extern struct sockaddr_ipx ipx_netmask;
extern struct sockaddr_ipx ipx_hostmask;

extern const union ipx_net ipx_zeronet;
extern const union ipx_host ipx_zerohost;
extern const union ipx_net ipx_broadnet;
extern const union ipx_host ipx_broadhost;

struct ifnet;
struct ipx_addr;
struct mbuf;
struct thread;
struct route;
struct sockaddr;
struct socket;
struct sockopt;

u_short	ipx_cksum(struct mbuf *m, int len);
int	ipx_control(struct socket *so, u_long cmd, caddr_t data,
			 struct ifnet *ifp, struct thread *td);
void	ipx_ctlinput(int cmd, struct sockaddr *arg_as_sa, void *dummy);
int	ipx_ctloutput(struct socket *so, struct sockopt *sopt);
void	ipx_drop(struct ipxpcb *ipxp, int errno);
void	ipx_init(void);
void	ipx_input(struct mbuf *m, struct ipxpcb *ipxp);
int	ipx_outputfl(struct mbuf *m0, struct route *ro, int flags);
int	ipx_output_type20(struct mbuf *);
int	ipx_peeraddr(struct socket *so, struct sockaddr **nam);
void	ipx_printhost(struct ipx_addr *addr);
int	ipx_sockaddr(struct socket *so, struct sockaddr **nam);
void	ipx_watch_output(struct mbuf *m, struct ifnet *ifp);

#endif /* _KERNEL */

#endif /* !_NETIPX_IPX_VAR_H_ */
