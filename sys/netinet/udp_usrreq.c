/*
 * Copyright (c) 1982, 1986, 1988, 1990 Regents of the University of California.
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
 *	from: @(#)udp_usrreq.c	7.20 (Berkeley) 4/20/91
 *	$Id: udp_usrreq.c,v 1.2 1993/10/16 18:26:43 rgrimes Exp $
 */

#include "param.h"
#include "malloc.h"
#include "mbuf.h"
#include "protosw.h"
#include "socket.h"
#include "socketvar.h"
#include "stat.h"

#include "../net/if.h"
#include "../net/route.h"

#include "in.h"
#include "in_systm.h"
#include "ip.h"
#include "in_pcb.h"
#include "ip_var.h"
#include "ip_icmp.h"
#include "udp.h"
#include "udp_var.h"

struct	inpcb *udp_last_inpcb = &udb;

/*
 * UDP protocol implementation.
 * Per RFC 768, August, 1980.
 */
udp_init()
{

	udb.inp_next = udb.inp_prev = &udb;
}

#ifndef	COMPAT_42
int	udpcksum = 1;
#else
int	udpcksum = 0;		/* XXX */
#endif
int	udp_ttl = UDP_TTL;

struct	sockaddr_in udp_in = { sizeof(udp_in), AF_INET };

udp_input(m, iphlen)
	register struct mbuf *m;
	int iphlen;
{
	register struct ip *ip;
	register struct udphdr *uh;
	register struct inpcb *inp;
	struct mbuf *opts = 0;
	int len;
	struct ip save_ip;

	udpstat.udps_ipackets++;

	/*
	 * Strip IP options, if any; should skip this,
	 * make available to user, and use on returned packets,
	 * but we don't yet have a way to check the checksum
	 * with options still present.
	 */
	if (iphlen > sizeof (struct ip)) {
		ip_stripoptions(m, (struct mbuf *)0);
		iphlen = sizeof(struct ip);
	}

	/*
	 * Get IP and UDP header together in first mbuf.
	 */
	ip = mtod(m, struct ip *);
	if (m->m_len < iphlen + sizeof(struct udphdr)) {
		if ((m = m_pullup(m, iphlen + sizeof(struct udphdr))) == 0) {
			udpstat.udps_hdrops++;
			return;
		}
		ip = mtod(m, struct ip *);
	}
	uh = (struct udphdr *)((caddr_t)ip + iphlen);

	/*
	 * Make mbuf data length reflect UDP length.
	 * If not enough data to reflect UDP length, drop.
	 */
	len = ntohs((u_short)uh->uh_ulen);
	if (ip->ip_len != len) {
		if (len > ip->ip_len) {
			udpstat.udps_badlen++;
			goto bad;
		}
		m_adj(m, len - ip->ip_len);
		/* ip->ip_len = len; */
	}
	/*
	 * Save a copy of the IP header in case we want restore it
	 * for sending an ICMP error message in response.
	 */
	save_ip = *ip;

	/*
	 * Checksum extended UDP header and data.
	 */
	if (udpcksum && uh->uh_sum) {
		((struct ipovly *)ip)->ih_next = 0;
		((struct ipovly *)ip)->ih_prev = 0;
		((struct ipovly *)ip)->ih_x1 = 0;
		((struct ipovly *)ip)->ih_len = uh->uh_ulen;
		if (uh->uh_sum = in_cksum(m, len + sizeof (struct ip))) {
			udpstat.udps_badsum++;
			m_freem(m);
			return;
		}
	}

	/*
	 * Locate pcb for datagram.
	 */
	inp = udp_last_inpcb;
	if (inp->inp_lport != uh->uh_dport ||
	    inp->inp_fport != uh->uh_sport ||
	    inp->inp_faddr.s_addr != ip->ip_src.s_addr ||
	    inp->inp_laddr.s_addr != ip->ip_dst.s_addr) {
		inp = in_pcblookup(&udb, ip->ip_src, uh->uh_sport,
		    ip->ip_dst, uh->uh_dport, INPLOOKUP_WILDCARD);
		if (inp)
			udp_last_inpcb = inp;
		udpstat.udpps_pcbcachemiss++;
	}
	if (inp == 0) {
		/* don't send ICMP response for broadcast packet */
		udpstat.udps_noport++;
		if (m->m_flags & M_BCAST) {
			udpstat.udps_noportbcast++;
			goto bad;
		}
		*ip = save_ip;
		ip->ip_len += iphlen;
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT);
		return;
	}

	/*
	 * Construct sockaddr format source address.
	 * Stuff source address and datagram in user buffer.
	 */
	udp_in.sin_port = uh->uh_sport;
	udp_in.sin_addr = ip->ip_src;
	if (inp->inp_flags & INP_CONTROLOPTS) {
		struct mbuf **mp = &opts;
		struct mbuf *udp_saveopt();

		if (inp->inp_flags & INP_RECVDSTADDR) {
			*mp = udp_saveopt((caddr_t) &ip->ip_dst,
			    sizeof(struct in_addr), IP_RECVDSTADDR);
			if (*mp)
				mp = &(*mp)->m_next;
		}
#ifdef notyet
		/* options were tossed above */
		if (inp->inp_flags & INP_RECVOPTS) {
			*mp = udp_saveopt((caddr_t) opts_deleted_above,
			    sizeof(struct in_addr), IP_RECVOPTS);
			if (*mp)
				mp = &(*mp)->m_next;
		}
		/* ip_srcroute doesn't do what we want here, need to fix */
		if (inp->inp_flags & INP_RECVRETOPTS) {
			*mp = udp_saveopt((caddr_t) ip_srcroute(),
			    sizeof(struct in_addr), IP_RECVRETOPTS);
			if (*mp)
				mp = &(*mp)->m_next;
		}
#endif
	}
	iphlen += sizeof(struct udphdr);
	m->m_len -= iphlen;
	m->m_pkthdr.len -= iphlen;
	m->m_data += iphlen;
	if (sbappendaddr(&inp->inp_socket->so_rcv, (struct sockaddr *)&udp_in,
	    m, opts) == 0) {
		udpstat.udps_fullsock++;
		goto bad;
	}
	sorwakeup(inp->inp_socket);
	return;
bad:
	m_freem(m);
	if (opts)
		m_freem(opts);
}

/*
 * Create a "control" mbuf containing the specified data
 * with the specified type for presentation with a datagram.
 */
struct mbuf *
udp_saveopt(p, size, type)
	caddr_t p;
	register int size;
	int type;
{
	register struct cmsghdr *cp;
	struct mbuf *m;

	if ((m = m_get(M_DONTWAIT, MT_CONTROL)) == NULL)
		return ((struct mbuf *) NULL);
	cp = (struct cmsghdr *) mtod(m, struct cmsghdr *);
	bcopy(p, (caddr_t)(cp + 1), size);
	size += sizeof(*cp);
	m->m_len = size;
	cp->cmsg_len = size;
	cp->cmsg_level = IPPROTO_IP;
	cp->cmsg_type = type;
	return (m);
}

/*
 * Notify a udp user of an asynchronous error;
 * just wake up so that he can collect error status.
 */
udp_notify(inp, errno)
	register struct inpcb *inp;
{

	inp->inp_socket->so_error = errno;
	sorwakeup(inp->inp_socket);
	sowwakeup(inp->inp_socket);
}

udp_ctlinput(cmd, sa, ip)
	int cmd;
	struct sockaddr *sa;
	register struct ip *ip;
{
	register struct udphdr *uh;
	extern struct in_addr zeroin_addr;
	extern u_char inetctlerrmap[];

	if ((unsigned)cmd > PRC_NCMDS || inetctlerrmap[cmd] == 0)
		return;
	if (ip) {
		uh = (struct udphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		in_pcbnotify(&udb, sa, uh->uh_dport, ip->ip_src, uh->uh_sport,
			cmd, udp_notify);
	} else
		in_pcbnotify(&udb, sa, 0, zeroin_addr, 0, cmd, udp_notify);
}

udp_output(inp, m, addr, control)
	register struct inpcb *inp;
	register struct mbuf *m;
	struct mbuf *addr, *control;
{
	register struct udpiphdr *ui;
	register int len = m->m_pkthdr.len;
	struct in_addr laddr;
	int s, error = 0;

	if (control)
		m_freem(control);		/* XXX */

	if (addr) {
		laddr = inp->inp_laddr;
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
			error = EISCONN;
			goto release;
		}
		/*
		 * Must block input while temporarily connected.
		 */
		s = splnet();
		error = in_pcbconnect(inp, addr);
		if (error) {
			splx(s);
			goto release;
		}
	} else {
		if (inp->inp_faddr.s_addr == INADDR_ANY) {
			error = ENOTCONN;
			goto release;
		}
	}
	/*
	 * Calculate data length and get a mbuf
	 * for UDP and IP headers.
	 */
	M_PREPEND(m, sizeof(struct udpiphdr), M_WAIT);

	/*
	 * Fill in mbuf with extended UDP header
	 * and addresses and length put into network format.
	 */
	ui = mtod(m, struct udpiphdr *);
	ui->ui_next = ui->ui_prev = 0;
	ui->ui_x1 = 0;
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_len = htons((u_short)len + sizeof (struct udphdr));
	ui->ui_src = inp->inp_laddr;
	ui->ui_dst = inp->inp_faddr;
	ui->ui_sport = inp->inp_lport;
	ui->ui_dport = inp->inp_fport;
	ui->ui_ulen = ui->ui_len;

	/*
	 * Stuff checksum and output datagram.
	 */
	ui->ui_sum = 0;
	if (udpcksum) {
	    if ((ui->ui_sum = in_cksum(m, sizeof (struct udpiphdr) + len)) == 0)
		ui->ui_sum = 0xffff;
	}
	((struct ip *)ui)->ip_len = sizeof (struct udpiphdr) + len;
	((struct ip *)ui)->ip_ttl = inp->inp_ip.ip_ttl;	/* XXX */
	((struct ip *)ui)->ip_tos = inp->inp_ip.ip_tos;	/* XXX */
	udpstat.udps_opackets++;
	error = ip_output(m, inp->inp_options, &inp->inp_route,
	    inp->inp_socket->so_options & (SO_DONTROUTE | SO_BROADCAST));

	if (addr) {
		in_pcbdisconnect(inp);
		inp->inp_laddr = laddr;
		splx(s);
	}
	return (error);

release:
	m_freem(m);
	return (error);
}

u_long	udp_sendspace = 9216;		/* really max datagram size */
u_long	udp_recvspace = 40 * (1024 + sizeof(struct sockaddr_in));
					/* 40 1K datagrams */

/*ARGSUSED*/
udp_usrreq(so, req, m, addr, control)
	struct socket *so;
	int req;
	struct mbuf *m, *addr, *control;
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;
	int s;

	if (req == PRU_CONTROL)
		return (in_control(so, (int)m, (caddr_t)addr,
			(struct ifnet *)control));
	if (inp == NULL && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}
	/*
	 * Note: need to block udp_input while changing
	 * the udp pcb queue and/or pcb addresses.
	 */
	switch (req) {

	case PRU_ATTACH:
		if (inp != NULL) {
			error = EINVAL;
			break;
		}
		s = splnet();
		error = in_pcballoc(so, &udb);
		splx(s);
		if (error)
			break;
		error = soreserve(so, udp_sendspace, udp_recvspace);
		if (error)
			break;
		((struct inpcb *) so->so_pcb)->inp_ip.ip_ttl = udp_ttl;
		break;

	case PRU_DETACH:
		udp_detach(inp);
		break;

	case PRU_BIND:
		s = splnet();
		error = in_pcbbind(inp, addr);
		splx(s);
		break;

	case PRU_LISTEN:
		error = EOPNOTSUPP;
		break;

	case PRU_CONNECT:
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
			error = EISCONN;
			break;
		}
		s = splnet();
		error = in_pcbconnect(inp, addr);
		splx(s);
		if (error == 0)
			soisconnected(so);
		break;

	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	case PRU_ACCEPT:
		error = EOPNOTSUPP;
		break;

	case PRU_DISCONNECT:
		if (inp->inp_faddr.s_addr == INADDR_ANY) {
			error = ENOTCONN;
			break;
		}
		s = splnet();
		in_pcbdisconnect(inp);
		inp->inp_laddr.s_addr = INADDR_ANY;
		splx(s);
		so->so_state &= ~SS_ISCONNECTED;		/* XXX */
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	case PRU_SEND:
		return (udp_output(inp, m, addr, control));

	case PRU_ABORT:
		soisdisconnected(so);
		udp_detach(inp);
		break;

	case PRU_SOCKADDR:
		in_setsockaddr(inp, addr);
		break;

	case PRU_PEERADDR:
		in_setpeeraddr(inp, addr);
		break;

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		return (0);

	case PRU_SENDOOB:
	case PRU_FASTTIMO:
	case PRU_SLOWTIMO:
	case PRU_PROTORCV:
	case PRU_PROTOSEND:
		error =  EOPNOTSUPP;
		break;

	case PRU_RCVD:
	case PRU_RCVOOB:
		return (EOPNOTSUPP);	/* do not free mbuf's */

	default:
		panic("udp_usrreq");
	}

release:
	if (control) {
		printf("udp control data unexpectedly retained\n");
		m_freem(control);
	}
	if (m)
		m_freem(m);
	return (error);
}

udp_detach(inp)
	struct inpcb *inp;
{
	int s = splnet();

	if (inp == udp_last_inpcb)
		udp_last_inpcb = &udb;
	in_pcbdetach(inp);
	splx(s);
}
