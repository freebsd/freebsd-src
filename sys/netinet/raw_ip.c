/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)raw_ip.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_mroute.h>
#include <netinet/in_pcb.h>

struct inpcb rawinpcb;

/*
 * Nominal space allocated to a raw ip socket.
 */
#define	RIPSNDQ		8192
#define	RIPRCVQ		8192

/*
 * Raw interface to IP protocol.
 */

/*
 * Initialize raw connection block q.
 */
void
rip_init()
{

	rawinpcb.inp_next = rawinpcb.inp_prev = &rawinpcb;
}

struct	sockaddr_in ripsrc = { sizeof(ripsrc), AF_INET };
/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
void
rip_input(m)
	struct mbuf *m;
{
	register struct ip *ip = mtod(m, struct ip *);
	register struct inpcb *inp;
	struct socket *last = 0;

	ripsrc.sin_addr = ip->ip_src;
	for (inp = rawinpcb.inp_next; inp != &rawinpcb; inp = inp->inp_next) {
		if (inp->inp_ip.ip_p && inp->inp_ip.ip_p != ip->ip_p)
			continue;
		if (inp->inp_laddr.s_addr &&
		    inp->inp_laddr.s_addr == ip->ip_dst.s_addr)
			continue;
		if (inp->inp_faddr.s_addr &&
		    inp->inp_faddr.s_addr == ip->ip_src.s_addr)
			continue;
		if (last) {
			struct mbuf *n;
			if (n = m_copy(m, 0, (int)M_COPYALL)) {
				if (sbappendaddr(&last->so_rcv, &ripsrc,
				    n, (struct mbuf *)0) == 0)
					/* should notify about lost packet */
					m_freem(n);
				else
					sorwakeup(last);
			}
		}
		last = inp->inp_socket;
	}
	if (last) {
		if (sbappendaddr(&last->so_rcv, &ripsrc,
		    m, (struct mbuf *)0) == 0)
			m_freem(m);
		else
			sorwakeup(last);
	} else {
		m_freem(m);
		ipstat.ips_noproto++;
		ipstat.ips_delivered--;
	}
}

/*
 * Generate IP header and pass packet to ip_output.
 * Tack on options user may have setup with control call.
 */
int
rip_output(m, so, dst)
	register struct mbuf *m;
	struct socket *so;
	u_long dst;
{
	register struct ip *ip;
	register struct inpcb *inp = sotoinpcb(so);
	struct mbuf *opts;
	int flags = (so->so_options & SO_DONTROUTE) | IP_ALLOWBROADCAST;

	/*
	 * If the user handed us a complete IP packet, use it.
	 * Otherwise, allocate an mbuf for a header and fill it in.
	 */
	if ((inp->inp_flags & INP_HDRINCL) == 0) {
		M_PREPEND(m, sizeof(struct ip), M_WAIT);
		ip = mtod(m, struct ip *);
		ip->ip_tos = 0;
		ip->ip_off = 0;
		ip->ip_p = inp->inp_ip.ip_p;
		ip->ip_len = m->m_pkthdr.len;
		ip->ip_src = inp->inp_laddr;
		ip->ip_dst.s_addr = dst;
		ip->ip_ttl = MAXTTL;
		opts = inp->inp_options;
	} else {
		ip = mtod(m, struct ip *);
		if (ip->ip_id == 0)
			ip->ip_id = htons(ip_id++);
		opts = NULL;
		/* XXX prevent ip_output from overwriting header fields */
		flags |= IP_RAWOUTPUT;
		ipstat.ips_rawout++;
	}
	return (ip_output(m, opts, &inp->inp_route, flags, inp->inp_moptions));
}

/*
 * Raw IP socket option processing.
 */
int
rip_ctloutput(op, so, level, optname, m)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **m;
{
	register struct inpcb *inp = sotoinpcb(so);
	register int error;

	if (level != IPPROTO_IP)
		return (EINVAL);

	switch (optname) {

	case IP_HDRINCL:
		if (op == PRCO_SETOPT || op == PRCO_GETOPT) {
			if (m == 0 || *m == 0 || (*m)->m_len < sizeof (int))
				return (EINVAL);
			if (op == PRCO_SETOPT) {
				if (*mtod(*m, int *))
					inp->inp_flags |= INP_HDRINCL;
				else
					inp->inp_flags &= ~INP_HDRINCL;
				(void)m_free(*m);
			} else {
				(*m)->m_len = sizeof (int);
				*mtod(*m, int *) = inp->inp_flags & INP_HDRINCL;
			}
			return (0);
		}
		break;

	case DVMRP_INIT:
	case DVMRP_DONE:
	case DVMRP_ADD_VIF:
	case DVMRP_DEL_VIF:
	case DVMRP_ADD_LGRP:
	case DVMRP_DEL_LGRP:
	case DVMRP_ADD_MRT:
	case DVMRP_DEL_MRT:
#ifdef MROUTING
		if (op == PRCO_SETOPT) {
			error = ip_mrouter_cmd(optname, so, *m);
			if (*m)
				(void)m_free(*m);
		} else
			error = EINVAL;
		return (error);
#else
		if (op == PRCO_SETOPT && *m)
			(void)m_free(*m);
		return (EOPNOTSUPP);
#endif
	}
	return (ip_ctloutput(op, so, level, optname, m));
}

u_long	rip_sendspace = RIPSNDQ;
u_long	rip_recvspace = RIPRCVQ;

/*ARGSUSED*/
int
rip_usrreq(so, req, m, nam, control)
	register struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
{
	register int error = 0;
	register struct inpcb *inp = sotoinpcb(so);
#ifdef MROUTING
	extern struct socket *ip_mrouter;
#endif
	switch (req) {

	case PRU_ATTACH:
		if (inp)
			panic("rip_attach");
		if ((so->so_state & SS_PRIV) == 0) {
			error = EACCES;
			break;
		}
		if ((error = soreserve(so, rip_sendspace, rip_recvspace)) ||
		    (error = in_pcballoc(so, &rawinpcb)))
			break;
		inp = (struct inpcb *)so->so_pcb;
		inp->inp_ip.ip_p = (int)nam;
		break;

	case PRU_DISCONNECT:
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			error = ENOTCONN;
			break;
		}
		/* FALLTHROUGH */
	case PRU_ABORT:
		soisdisconnected(so);
		/* FALLTHROUGH */
	case PRU_DETACH:
		if (inp == 0)
			panic("rip_detach");
#ifdef MROUTING
		if (so == ip_mrouter)
			ip_mrouter_done();
#endif
		in_pcbdetach(inp);
		break;

	case PRU_BIND:
	    {
		struct sockaddr_in *addr = mtod(nam, struct sockaddr_in *);

		if (nam->m_len != sizeof(*addr)) {
			error = EINVAL;
			break;
		}
		if ((ifnet == 0) ||
		    ((addr->sin_family != AF_INET) &&
		     (addr->sin_family != AF_IMPLINK)) ||
		    (addr->sin_addr.s_addr &&
		     ifa_ifwithaddr((struct sockaddr *)addr) == 0)) {
			error = EADDRNOTAVAIL;
			break;
		}
		inp->inp_laddr = addr->sin_addr;
		break;
	    }
	case PRU_CONNECT:
	    {
		struct sockaddr_in *addr = mtod(nam, struct sockaddr_in *);

		if (nam->m_len != sizeof(*addr)) {
			error = EINVAL;
			break;
		}
		if (ifnet == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		if ((addr->sin_family != AF_INET) &&
		     (addr->sin_family != AF_IMPLINK)) {
			error = EAFNOSUPPORT;
			break;
		}
		inp->inp_faddr = addr->sin_addr;
		soisconnected(so);
		break;
	    }

	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	/*
	 * Mark the connection as being incapable of further input.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	/*
	 * Ship a packet out.  The appropriate raw output
	 * routine handles any massaging necessary.
	 */
	case PRU_SEND:
	    {
		register u_long dst;

		if (so->so_state & SS_ISCONNECTED) {
			if (nam) {
				error = EISCONN;
				break;
			}
			dst = inp->inp_faddr.s_addr;
		} else {
			if (nam == NULL) {
				error = ENOTCONN;
				break;
			}
			dst = mtod(nam, struct sockaddr_in *)->sin_addr.s_addr;
		}
		error = rip_output(m, so, dst);
		m = NULL;
		break;
	    }

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		return (0);

	/*
	 * Not supported.
	 */
	case PRU_RCVOOB:
	case PRU_RCVD:
	case PRU_LISTEN:
	case PRU_ACCEPT:
	case PRU_SENDOOB:
		error = EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:
		in_setsockaddr(inp, nam);
		break;

	case PRU_PEERADDR:
		in_setpeeraddr(inp, nam);
		break;

	default:
		panic("rip_usrreq");
	}
	if (m != NULL)
		m_freem(m);
	return (error);
}
