/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
 *	The Regents of the University of California.
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
 *	@(#)udp_usrreq.c	8.6 (Berkeley) 5/23/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netinet/udp_usrreq.c,v 1.218 2007/10/07 20:44:24 silby Exp $");

#include "opt_ipfw.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/domain.h>
#include <sys/eventhandler.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <vm/uma.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#ifdef INET6
#include <netinet6/ip6_var.h>
#endif
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#endif

#include <machine/in_cksum.h>

#include <security/mac/mac_framework.h>

/*
 * UDP protocol implementation.
 * Per RFC 768, August, 1980.
 */

/*
 * BSD 4.2 defaulted the udp checksum to be off.  Turning off udp checksums
 * removes the only data integrity mechanism for packets and malformed
 * packets that would otherwise be discarded due to bad checksums, and may
 * cause problems (especially for NFS data blocks).
 */
static int	udp_cksum = 1;
SYSCTL_INT(_net_inet_udp, UDPCTL_CHECKSUM, checksum, CTLFLAG_RW, &udp_cksum,
    0, "");

int	udp_log_in_vain = 0;
SYSCTL_INT(_net_inet_udp, OID_AUTO, log_in_vain, CTLFLAG_RW,
    &udp_log_in_vain, 0, "Log all incoming UDP packets");

int	udp_blackhole = 0;
SYSCTL_INT(_net_inet_udp, OID_AUTO, blackhole, CTLFLAG_RW, &udp_blackhole, 0,
    "Do not send port unreachables for refused connects");

u_long	udp_sendspace = 9216;		/* really max datagram size */
					/* 40 1K datagrams */
SYSCTL_ULONG(_net_inet_udp, UDPCTL_MAXDGRAM, maxdgram, CTLFLAG_RW,
    &udp_sendspace, 0, "Maximum outgoing UDP datagram size");

u_long	udp_recvspace = 40 * (1024 +
#ifdef INET6
				      sizeof(struct sockaddr_in6)
#else
				      sizeof(struct sockaddr_in)
#endif
				      );

SYSCTL_ULONG(_net_inet_udp, UDPCTL_RECVSPACE, recvspace, CTLFLAG_RW,
    &udp_recvspace, 0, "Maximum space for incoming UDP datagrams");

struct inpcbhead	udb;		/* from udp_var.h */
struct inpcbinfo	udbinfo;

#ifndef UDBHASHSIZE
#define	UDBHASHSIZE	16
#endif

struct udpstat	udpstat;	/* from udp_var.h */
SYSCTL_STRUCT(_net_inet_udp, UDPCTL_STATS, stats, CTLFLAG_RW, &udpstat,
    udpstat, "UDP statistics (struct udpstat, netinet/udp_var.h)");

static void	udp_detach(struct socket *so);
static int	udp_output(struct inpcb *, struct mbuf *, struct sockaddr *,
		    struct mbuf *, struct thread *);

static void
udp_zone_change(void *tag)
{

	uma_zone_set_max(udbinfo.ipi_zone, maxsockets);
}

static int
udp_inpcb_init(void *mem, int size, int flags)
{
	struct inpcb *inp;

	inp = mem;
	INP_LOCK_INIT(inp, "inp", "udpinp");
	return (0);
}

void
udp_init(void)
{

	INP_INFO_LOCK_INIT(&udbinfo, "udp");
	LIST_INIT(&udb);
	udbinfo.ipi_listhead = &udb;
	udbinfo.ipi_hashbase = hashinit(UDBHASHSIZE, M_PCB,
	    &udbinfo.ipi_hashmask);
	udbinfo.ipi_porthashbase = hashinit(UDBHASHSIZE, M_PCB,
	    &udbinfo.ipi_porthashmask);
	udbinfo.ipi_zone = uma_zcreate("udpcb", sizeof(struct inpcb), NULL,
	    NULL, udp_inpcb_init, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uma_zone_set_max(udbinfo.ipi_zone, maxsockets);
	EVENTHANDLER_REGISTER(maxsockets_change, udp_zone_change, NULL,
	    EVENTHANDLER_PRI_ANY);
}

/*
 * Subroutine of udp_input(), which appends the provided mbuf chain to the
 * passed pcb/socket.  The caller must provide a sockaddr_in via udp_in that
 * contains the source address.  If the socket ends up being an IPv6 socket,
 * udp_append() will convert to a sockaddr_in6 before passing the address
 * into the socket code.
 */
static void
udp_append(struct inpcb *inp, struct ip *ip, struct mbuf *n, int off,
    struct sockaddr_in *udp_in)
{
	struct sockaddr *append_sa;
	struct socket *so;
	struct mbuf *opts = 0;
#ifdef INET6
	struct sockaddr_in6 udp_in6;
#endif

	INP_LOCK_ASSERT(inp);

#ifdef IPSEC
	/* Check AH/ESP integrity. */
	if (ipsec4_in_reject(n, inp)) {
		m_freem(n);
		ipsec4stat.in_polvio++;
		return;
	}
#endif /* IPSEC */
#ifdef MAC
	if (mac_check_inpcb_deliver(inp, n) != 0) {
		m_freem(n);
		return;
	}
#endif
	if (inp->inp_flags & INP_CONTROLOPTS ||
	    inp->inp_socket->so_options & (SO_TIMESTAMP | SO_BINTIME)) {
#ifdef INET6
		if (inp->inp_vflag & INP_IPV6) {
			int savedflags;

			savedflags = inp->inp_flags;
			inp->inp_flags &= ~INP_UNMAPPABLEOPTS;
			ip6_savecontrol(inp, n, &opts);
			inp->inp_flags = savedflags;
		} else
#endif
			ip_savecontrol(inp, &opts, ip, n);
	}
#ifdef INET6
	if (inp->inp_vflag & INP_IPV6) {
		bzero(&udp_in6, sizeof(udp_in6));
		udp_in6.sin6_len = sizeof(udp_in6);
		udp_in6.sin6_family = AF_INET6;
		in6_sin_2_v4mapsin6(udp_in, &udp_in6);
		append_sa = (struct sockaddr *)&udp_in6;
	} else
#endif
		append_sa = (struct sockaddr *)udp_in;
	m_adj(n, off);

	so = inp->inp_socket;
	SOCKBUF_LOCK(&so->so_rcv);
	if (sbappendaddr_locked(&so->so_rcv, append_sa, n, opts) == 0) {
		SOCKBUF_UNLOCK(&so->so_rcv);
		m_freem(n);
		if (opts)
			m_freem(opts);
		udpstat.udps_fullsock++;
	} else
		sorwakeup_locked(so);
}

void
udp_input(struct mbuf *m, int off)
{
	int iphlen = off;
	struct ip *ip;
	struct udphdr *uh;
	struct ifnet *ifp;
	struct inpcb *inp;
	int len;
	struct ip save_ip;
	struct sockaddr_in udp_in;
#ifdef IPFIREWALL_FORWARD
	struct m_tag *fwd_tag;
#endif

	ifp = m->m_pkthdr.rcvif;
	udpstat.udps_ipackets++;

	/*
	 * Strip IP options, if any; should skip this, make available to
	 * user, and use on returned packets, but we don't yet have a way to
	 * check the checksum with options still present.
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
	 * Destination port of 0 is illegal, based on RFC768.
	 */
	if (uh->uh_dport == 0)
		goto badunlocked;

	/*
	 * Construct sockaddr format source address.  Stuff source address
	 * and datagram in user buffer.
	 */
	bzero(&udp_in, sizeof(udp_in));
	udp_in.sin_len = sizeof(udp_in);
	udp_in.sin_family = AF_INET;
	udp_in.sin_port = uh->uh_sport;
	udp_in.sin_addr = ip->ip_src;

	/*
	 * Make mbuf data length reflect UDP length.  If not enough data to
	 * reflect UDP length, drop.
	 */
	len = ntohs((u_short)uh->uh_ulen);
	if (ip->ip_len != len) {
		if (len > ip->ip_len || len < sizeof(struct udphdr)) {
			udpstat.udps_badlen++;
			goto badunlocked;
		}
		m_adj(m, len - ip->ip_len);
		/* ip->ip_len = len; */
	}

	/*
	 * Save a copy of the IP header in case we want restore it for
	 * sending an ICMP error message in response.
	 */
	if (!udp_blackhole)
		save_ip = *ip;
	else
		memset(&save_ip, 0, sizeof(save_ip));

	/*
	 * Checksum extended UDP header and data.
	 */
	if (uh->uh_sum) {
		u_short uh_sum;

		if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR)
				uh_sum = m->m_pkthdr.csum_data;
			else
				uh_sum = in_pseudo(ip->ip_src.s_addr,
				    ip->ip_dst.s_addr, htonl((u_short)len +
				    m->m_pkthdr.csum_data + IPPROTO_UDP));
			uh_sum ^= 0xffff;
		} else {
			char b[9];

			bcopy(((struct ipovly *)ip)->ih_x1, b, 9);
			bzero(((struct ipovly *)ip)->ih_x1, 9);
			((struct ipovly *)ip)->ih_len = uh->uh_ulen;
			uh_sum = in_cksum(m, len + sizeof (struct ip));
			bcopy(b, ((struct ipovly *)ip)->ih_x1, 9);
		}
		if (uh_sum) {
			udpstat.udps_badsum++;
			m_freem(m);
			return;
		}
	} else
		udpstat.udps_nosum++;

#ifdef IPFIREWALL_FORWARD
	/*
	 * Grab info from PACKET_TAG_IPFORWARD tag prepended to the chain.
	 */
	fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL);
	if (fwd_tag != NULL) {
		struct sockaddr_in *next_hop;

		/*
		 * Do the hack.
		 */
		next_hop = (struct sockaddr_in *)(fwd_tag + 1);
		ip->ip_dst = next_hop->sin_addr;
		uh->uh_dport = ntohs(next_hop->sin_port);

		/*
		 * Remove the tag from the packet.  We don't need it anymore.
		 */
		m_tag_delete(m, fwd_tag);
	}
#endif

	INP_INFO_RLOCK(&udbinfo);
	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
	    in_broadcast(ip->ip_dst, ifp)) {
		struct inpcb *last;
		struct ip_moptions *imo;

		last = NULL;
		LIST_FOREACH(inp, &udb, inp_list) {
			if (inp->inp_lport != uh->uh_dport)
				continue;
#ifdef INET6
			if ((inp->inp_vflag & INP_IPV4) == 0)
				continue;
#endif
			if (inp->inp_laddr.s_addr != INADDR_ANY &&
			    inp->inp_laddr.s_addr != ip->ip_dst.s_addr)
				continue;
			if (inp->inp_faddr.s_addr != INADDR_ANY &&
			    inp->inp_faddr.s_addr != ip->ip_src.s_addr)
				continue;
			/*
			 * XXX: Do not check source port of incoming datagram
			 * unless inp_connect() has been called to bind the
			 * fport part of the 4-tuple; the source could be
			 * trying to talk to us with an ephemeral port.
			 */
			if (inp->inp_fport != 0 &&
			    inp->inp_fport != uh->uh_sport)
				continue;

			INP_LOCK(inp);

			/*
			 * Handle socket delivery policy for any-source
			 * and source-specific multicast. [RFC3678]
			 */
			imo = inp->inp_moptions;
			if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) &&
			    imo != NULL) {
				struct sockaddr_in	 sin;
				struct in_msource	*ims;
				int			 blocked, mode;
				size_t			 idx;

				bzero(&sin, sizeof(struct sockaddr_in));
				sin.sin_len = sizeof(struct sockaddr_in);
				sin.sin_family = AF_INET;
				sin.sin_addr = ip->ip_dst;

				blocked = 0;
				idx = imo_match_group(imo, ifp,
				    (struct sockaddr *)&sin);
				if (idx == -1) {
					/*
					 * No group membership for this socket.
					 * Do not bump udps_noportbcast, as
					 * this will happen further down.
					 */
					blocked++;
				} else {
					/*
					 * Check for a multicast source filter
					 * entry on this socket for this group.
					 * MCAST_EXCLUDE is the default
					 * behaviour.  It means default accept;
					 * entries, if present, denote sources
					 * to be excluded from delivery.
					 */
					ims = imo_match_source(imo, idx,
					    (struct sockaddr *)&udp_in);
					mode = imo->imo_mfilters[idx].imf_fmode;
					if ((ims != NULL &&
					     mode == MCAST_EXCLUDE) ||
					    (ims == NULL &&
					     mode == MCAST_INCLUDE)) {
#ifdef DIAGNOSTIC
						if (bootverbose) {
							printf("%s: blocked by"
							    " source filter\n",
							    __func__);
						}
#endif
						udpstat.udps_filtermcast++;
						blocked++;
					}
				}
				if (blocked != 0) {
					INP_UNLOCK(inp);
					continue;
				}
			}
			if (last != NULL) {
				struct mbuf *n;

				n = m_copy(m, 0, M_COPYALL);
				if (n != NULL)
					udp_append(last, ip, n, iphlen +
					    sizeof(struct udphdr), &udp_in);
				INP_UNLOCK(last);
			}
			last = inp;
			/*
			 * Don't look for additional matches if this one does
			 * not have either the SO_REUSEPORT or SO_REUSEADDR
			 * socket options set.  This heuristic avoids
			 * searching through all pcbs in the common case of a
			 * non-shared port.  It assumes that an application
			 * will never clear these options after setting them.
			 */
			if ((last->inp_socket->so_options &
			    (SO_REUSEPORT|SO_REUSEADDR)) == 0)
				break;
		}

		if (last == NULL) {
			/*
			 * No matching pcb found; discard datagram.  (No need
			 * to send an ICMP Port Unreachable for a broadcast
			 * or multicast datgram.)
			 */
			udpstat.udps_noportbcast++;
			goto badheadlocked;
		}
		udp_append(last, ip, m, iphlen + sizeof(struct udphdr),
		    &udp_in);
		INP_UNLOCK(last);
		INP_INFO_RUNLOCK(&udbinfo);
		return;
	}

	/*
	 * Locate pcb for datagram.
	 */
	inp = in_pcblookup_hash(&udbinfo, ip->ip_src, uh->uh_sport,
	    ip->ip_dst, uh->uh_dport, 1, ifp);
	if (inp == NULL) {
		if (udp_log_in_vain) {
			char buf[4*sizeof "123"];

			strcpy(buf, inet_ntoa(ip->ip_dst));
			log(LOG_INFO,
			    "Connection attempt to UDP %s:%d from %s:%d\n",
			    buf, ntohs(uh->uh_dport), inet_ntoa(ip->ip_src),
			    ntohs(uh->uh_sport));
		}
		udpstat.udps_noport++;
		if (m->m_flags & (M_BCAST | M_MCAST)) {
			udpstat.udps_noportbcast++;
			goto badheadlocked;
		}
		if (udp_blackhole)
			goto badheadlocked;
		if (badport_bandlim(BANDLIM_ICMP_UNREACH) < 0)
			goto badheadlocked;
		*ip = save_ip;
		ip->ip_len += iphlen;
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT, 0, 0);
		INP_INFO_RUNLOCK(&udbinfo);
		return;
	}

	/*
	 * Check the minimum TTL for socket.
	 */
	INP_LOCK(inp);
	if (inp->inp_ip_minttl && inp->inp_ip_minttl > ip->ip_ttl)
		goto badheadlocked;
	udp_append(inp, ip, m, iphlen + sizeof(struct udphdr), &udp_in);
	INP_UNLOCK(inp);
	INP_INFO_RUNLOCK(&udbinfo);
	return;

badheadlocked:
	if (inp)
		INP_UNLOCK(inp);
	INP_INFO_RUNLOCK(&udbinfo);
badunlocked:
	m_freem(m);
}

/*
 * Notify a udp user of an asynchronous error; just wake up so that they can
 * collect error status.
 */
struct inpcb *
udp_notify(struct inpcb *inp, int errno)
{

	inp->inp_socket->so_error = errno;
	sorwakeup(inp->inp_socket);
	sowwakeup(inp->inp_socket);
	return (inp);
}

void
udp_ctlinput(int cmd, struct sockaddr *sa, void *vip)
{
	struct ip *ip = vip;
	struct udphdr *uh;
	struct in_addr faddr;
	struct inpcb *inp;

	faddr = ((struct sockaddr_in *)sa)->sin_addr;
	if (sa->sa_family != AF_INET || faddr.s_addr == INADDR_ANY)
		return;

	/*
	 * Redirects don't need to be handled up here.
	 */
	if (PRC_IS_REDIRECT(cmd))
		return;

	/*
	 * Hostdead is ugly because it goes linearly through all PCBs.
	 *
	 * XXX: We never get this from ICMP, otherwise it makes an excellent
	 * DoS attack on machines with many connections.
	 */
	if (cmd == PRC_HOSTDEAD)
		ip = NULL;
	else if ((unsigned)cmd >= PRC_NCMDS || inetctlerrmap[cmd] == 0)
		return;
	if (ip != NULL) {
		uh = (struct udphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		INP_INFO_RLOCK(&udbinfo);
		inp = in_pcblookup_hash(&udbinfo, faddr, uh->uh_dport,
		    ip->ip_src, uh->uh_sport, 0, NULL);
		if (inp != NULL) {
			INP_LOCK(inp);
			if (inp->inp_socket != NULL) {
				udp_notify(inp, inetctlerrmap[cmd]);
			}
			INP_UNLOCK(inp);
		}
		INP_INFO_RUNLOCK(&udbinfo);
	} else
		in_pcbnotifyall(&udbinfo, faddr, inetctlerrmap[cmd],
		    udp_notify);
}

static int
udp_pcblist(SYSCTL_HANDLER_ARGS)
{
	int error, i, n;
	struct inpcb *inp, **inp_list;
	inp_gen_t gencnt;
	struct xinpgen xig;

	/*
	 * The process of preparing the PCB list is too time-consuming and
	 * resource-intensive to repeat twice on every request.
	 */
	if (req->oldptr == 0) {
		n = udbinfo.ipi_count;
		req->oldidx = 2 * (sizeof xig)
			+ (n + n/8) * sizeof(struct xinpcb);
		return (0);
	}

	if (req->newptr != 0)
		return (EPERM);

	/*
	 * OK, now we're committed to doing something.
	 */
	INP_INFO_RLOCK(&udbinfo);
	gencnt = udbinfo.ipi_gencnt;
	n = udbinfo.ipi_count;
	INP_INFO_RUNLOCK(&udbinfo);

	error = sysctl_wire_old_buffer(req, 2 * (sizeof xig)
		+ n * sizeof(struct xinpcb));
	if (error != 0)
		return (error);

	xig.xig_len = sizeof xig;
	xig.xig_count = n;
	xig.xig_gen = gencnt;
	xig.xig_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xig, sizeof xig);
	if (error)
		return (error);

	inp_list = malloc(n * sizeof *inp_list, M_TEMP, M_WAITOK);
	if (inp_list == 0)
		return (ENOMEM);

	INP_INFO_RLOCK(&udbinfo);
	for (inp = LIST_FIRST(udbinfo.ipi_listhead), i = 0; inp && i < n;
	     inp = LIST_NEXT(inp, inp_list)) {
		INP_LOCK(inp);
		if (inp->inp_gencnt <= gencnt &&
		    cr_canseesocket(req->td->td_ucred, inp->inp_socket) == 0)
			inp_list[i++] = inp;
		INP_UNLOCK(inp);
	}
	INP_INFO_RUNLOCK(&udbinfo);
	n = i;

	error = 0;
	for (i = 0; i < n; i++) {
		inp = inp_list[i];
		INP_LOCK(inp);
		if (inp->inp_gencnt <= gencnt) {
			struct xinpcb xi;
			bzero(&xi, sizeof(xi));
			xi.xi_len = sizeof xi;
			/* XXX should avoid extra copy */
			bcopy(inp, &xi.xi_inp, sizeof *inp);
			if (inp->inp_socket)
				sotoxsocket(inp->inp_socket, &xi.xi_socket);
			xi.xi_inp.inp_gencnt = inp->inp_gencnt;
			INP_UNLOCK(inp);
			error = SYSCTL_OUT(req, &xi, sizeof xi);
		} else
			INP_UNLOCK(inp);
	}
	if (!error) {
		/*
		 * Give the user an updated idea of our state.  If the
		 * generation differs from what we told her before, she knows
		 * that something happened while we were processing this
		 * request, and it might be necessary to retry.
		 */
		INP_INFO_RLOCK(&udbinfo);
		xig.xig_gen = udbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = udbinfo.ipi_count;
		INP_INFO_RUNLOCK(&udbinfo);
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}
	free(inp_list, M_TEMP);
	return (error);
}

SYSCTL_PROC(_net_inet_udp, UDPCTL_PCBLIST, pcblist, CTLFLAG_RD, 0, 0,
    udp_pcblist, "S,xinpcb", "List of active UDP sockets");

static int
udp_getcred(SYSCTL_HANDLER_ARGS)
{
	struct xucred xuc;
	struct sockaddr_in addrs[2];
	struct inpcb *inp;
	int error;

	error = priv_check(req->td, PRIV_NETINET_GETCRED);
	if (error)
		return (error);
	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);
	INP_INFO_RLOCK(&udbinfo);
	inp = in_pcblookup_hash(&udbinfo, addrs[1].sin_addr, addrs[1].sin_port,
				addrs[0].sin_addr, addrs[0].sin_port, 1, NULL);
	if (inp == NULL || inp->inp_socket == NULL) {
		error = ENOENT;
		goto out;
	}
	error = cr_canseesocket(req->td->td_ucred, inp->inp_socket);
	if (error)
		goto out;
	cru2x(inp->inp_socket->so_cred, &xuc);
out:
	INP_INFO_RUNLOCK(&udbinfo);
	if (error == 0)
		error = SYSCTL_OUT(req, &xuc, sizeof(struct xucred));
	return (error);
}

SYSCTL_PROC(_net_inet_udp, OID_AUTO, getcred,
    CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_PRISON, 0, 0,
    udp_getcred, "S,xucred", "Get the xucred of a UDP connection");

static int
udp_output(struct inpcb *inp, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct thread *td)
{
	struct udpiphdr *ui;
	int len = m->m_pkthdr.len;
	struct in_addr faddr, laddr;
	struct cmsghdr *cm;
	struct sockaddr_in *sin, src;
	int error = 0;
	int ipflags;
	u_short fport, lport;
	int unlock_udbinfo;

	/*
	 * udp_output() may need to temporarily bind or connect the current
	 * inpcb.  As such, we don't know up front whether we will need the
	 * pcbinfo lock or not.  Do any work to decide what is needed up
	 * front before acquiring any locks.
	 */
	if (len + sizeof(struct udpiphdr) > IP_MAXPACKET) {
		if (control)
			m_freem(control);
		m_freem(m);
		return (EMSGSIZE);
	}

	src.sin_family = 0;
	if (control != NULL) {
		/*
		 * XXX: Currently, we assume all the optional information is
		 * stored in a single mbuf.
		 */
		if (control->m_next) {
			m_freem(control);
			m_freem(m);
			return (EINVAL);
		}
		for (; control->m_len > 0;
		    control->m_data += CMSG_ALIGN(cm->cmsg_len),
		    control->m_len -= CMSG_ALIGN(cm->cmsg_len)) {
			cm = mtod(control, struct cmsghdr *);
			if (control->m_len < sizeof(*cm) || cm->cmsg_len == 0
			    || cm->cmsg_len > control->m_len) {
				error = EINVAL;
				break;
			}
			if (cm->cmsg_level != IPPROTO_IP)
				continue;

			switch (cm->cmsg_type) {
			case IP_SENDSRCADDR:
				if (cm->cmsg_len !=
				    CMSG_LEN(sizeof(struct in_addr))) {
					error = EINVAL;
					break;
				}
				bzero(&src, sizeof(src));
				src.sin_family = AF_INET;
				src.sin_len = sizeof(src);
				src.sin_port = inp->inp_lport;
				src.sin_addr =
				    *(struct in_addr *)CMSG_DATA(cm);
				break;

			default:
				error = ENOPROTOOPT;
				break;
			}
			if (error)
				break;
		}
		m_freem(control);
	}
	if (error) {
		m_freem(m);
		return (error);
	}

	if (src.sin_family == AF_INET || addr != NULL) {
		INP_INFO_WLOCK(&udbinfo);
		unlock_udbinfo = 1;
	} else
		unlock_udbinfo = 0;
	INP_LOCK(inp);

#ifdef MAC
	mac_create_mbuf_from_inpcb(inp, m);
#endif

	/*
	 * If the IP_SENDSRCADDR control message was specified, override the
	 * source address for this datagram.  Its use is invalidated if the
	 * address thus specified is incomplete or clobbers other inpcbs.
	 */
	laddr = inp->inp_laddr;
	lport = inp->inp_lport;
	if (src.sin_family == AF_INET) {
		if ((lport == 0) ||
		    (laddr.s_addr == INADDR_ANY &&
		     src.sin_addr.s_addr == INADDR_ANY)) {
			error = EINVAL;
			goto release;
		}
		error = in_pcbbind_setup(inp, (struct sockaddr *)&src,
		    &laddr.s_addr, &lport, td->td_ucred);
		if (error)
			goto release;
	}

	if (addr) {
		sin = (struct sockaddr_in *)addr;
		if (jailed(td->td_ucred))
			prison_remote_ip(td->td_ucred, 0,
			    &sin->sin_addr.s_addr);
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
			error = EISCONN;
			goto release;
		}
		error = in_pcbconnect_setup(inp, addr, &laddr.s_addr, &lport,
		    &faddr.s_addr, &fport, NULL, td->td_ucred);
		if (error)
			goto release;

		/* Commit the local port if newly assigned. */
		if (inp->inp_laddr.s_addr == INADDR_ANY &&
		    inp->inp_lport == 0) {
			/*
			 * Remember addr if jailed, to prevent rebinding.
			 */
			if (jailed(td->td_ucred))
				inp->inp_laddr = laddr;
			inp->inp_lport = lport;
			if (in_pcbinshash(inp) != 0) {
				inp->inp_lport = 0;
				error = EAGAIN;
				goto release;
			}
			inp->inp_flags |= INP_ANONPORT;
		}
	} else {
		faddr = inp->inp_faddr;
		fport = inp->inp_fport;
		if (faddr.s_addr == INADDR_ANY) {
			error = ENOTCONN;
			goto release;
		}
	}

	/*
	 * Calculate data length and get a mbuf for UDP, IP, and possible
	 * link-layer headers.  Immediate slide the data pointer back forward
	 * since we won't use that space at this layer.
	 */
	M_PREPEND(m, sizeof(struct udpiphdr) + max_linkhdr, M_DONTWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto release;
	}
	m->m_data += max_linkhdr;
	m->m_len -= max_linkhdr;
	m->m_pkthdr.len -= max_linkhdr;

	/*
	 * Fill in mbuf with extended UDP header and addresses and length put
	 * into network format.
	 */
	ui = mtod(m, struct udpiphdr *);
	bzero(ui->ui_x1, sizeof(ui->ui_x1));	/* XXX still needed? */
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_src = laddr;
	ui->ui_dst = faddr;
	ui->ui_sport = lport;
	ui->ui_dport = fport;
	ui->ui_ulen = htons((u_short)len + sizeof(struct udphdr));

	/*
	 * Set the Don't Fragment bit in the IP header.
	 */
	if (inp->inp_flags & INP_DONTFRAG) {
		struct ip *ip;

		ip = (struct ip *)&ui->ui_i;
		ip->ip_off |= IP_DF;
	}

	ipflags = 0;
	if (inp->inp_socket->so_options & SO_DONTROUTE)
		ipflags |= IP_ROUTETOIF;
	if (inp->inp_socket->so_options & SO_BROADCAST)
		ipflags |= IP_ALLOWBROADCAST;
	if (inp->inp_flags & INP_ONESBCAST)
		ipflags |= IP_SENDONES;

	/*
	 * Set up checksum and output datagram.
	 */
	if (udp_cksum) {
		if (inp->inp_flags & INP_ONESBCAST)
			faddr.s_addr = INADDR_BROADCAST;
		ui->ui_sum = in_pseudo(ui->ui_src.s_addr, faddr.s_addr,
		    htons((u_short)len + sizeof(struct udphdr) + IPPROTO_UDP));
		m->m_pkthdr.csum_flags = CSUM_UDP;
		m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
	} else
		ui->ui_sum = 0;
	((struct ip *)ui)->ip_len = sizeof (struct udpiphdr) + len;
	((struct ip *)ui)->ip_ttl = inp->inp_ip_ttl;	/* XXX */
	((struct ip *)ui)->ip_tos = inp->inp_ip_tos;	/* XXX */
	udpstat.udps_opackets++;

	if (unlock_udbinfo)
		INP_INFO_WUNLOCK(&udbinfo);
	error = ip_output(m, inp->inp_options, NULL, ipflags,
	    inp->inp_moptions, inp);
	INP_UNLOCK(inp);
	return (error);

release:
	INP_UNLOCK(inp);
	if (unlock_udbinfo)
		INP_INFO_WUNLOCK(&udbinfo);
	m_freem(m);
	return (error);
}

static void
udp_abort(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_abort: inp == NULL"));
	INP_INFO_WLOCK(&udbinfo);
	INP_LOCK(inp);
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		in_pcbdisconnect(inp);
		inp->inp_laddr.s_addr = INADDR_ANY;
		soisdisconnected(so);
	}
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&udbinfo);
}

static int
udp_attach(struct socket *so, int proto, struct thread *td)
{
	struct inpcb *inp;
	int error;

	inp = sotoinpcb(so);
	KASSERT(inp == NULL, ("udp_attach: inp != NULL"));
	error = soreserve(so, udp_sendspace, udp_recvspace);
	if (error)
		return (error);
	INP_INFO_WLOCK(&udbinfo);
	error = in_pcballoc(so, &udbinfo);
	if (error) {
		INP_INFO_WUNLOCK(&udbinfo);
		return (error);
	}

	inp = (struct inpcb *)so->so_pcb;
	INP_INFO_WUNLOCK(&udbinfo);
	inp->inp_vflag |= INP_IPV4;
	inp->inp_ip_ttl = ip_defttl;
	INP_UNLOCK(inp);
	return (0);
}

static int
udp_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct inpcb *inp;
	int error;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_bind: inp == NULL"));
	INP_INFO_WLOCK(&udbinfo);
	INP_LOCK(inp);
	error = in_pcbbind(inp, nam, td->td_ucred);
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&udbinfo);
	return (error);
}

static void
udp_close(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_close: inp == NULL"));
	INP_INFO_WLOCK(&udbinfo);
	INP_LOCK(inp);
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		in_pcbdisconnect(inp);
		inp->inp_laddr.s_addr = INADDR_ANY;
		soisdisconnected(so);
	}
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&udbinfo);
}

static int
udp_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct inpcb *inp;
	int error;
	struct sockaddr_in *sin;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_connect: inp == NULL"));
	INP_INFO_WLOCK(&udbinfo);
	INP_LOCK(inp);
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		INP_UNLOCK(inp);
		INP_INFO_WUNLOCK(&udbinfo);
		return (EISCONN);
	}
	sin = (struct sockaddr_in *)nam;
	if (jailed(td->td_ucred))
		prison_remote_ip(td->td_ucred, 0, &sin->sin_addr.s_addr);
	error = in_pcbconnect(inp, nam, td->td_ucred);
	if (error == 0)
		soisconnected(so);
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&udbinfo);
	return (error);
}

static void
udp_detach(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_detach: inp == NULL"));
	KASSERT(inp->inp_faddr.s_addr == INADDR_ANY,
	    ("udp_detach: not disconnected"));
	INP_INFO_WLOCK(&udbinfo);
	INP_LOCK(inp);
	in_pcbdetach(inp);
	in_pcbfree(inp);
	INP_INFO_WUNLOCK(&udbinfo);
}

static int
udp_disconnect(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_disconnect: inp == NULL"));
	INP_INFO_WLOCK(&udbinfo);
	INP_LOCK(inp);
	if (inp->inp_faddr.s_addr == INADDR_ANY) {
		INP_INFO_WUNLOCK(&udbinfo);
		INP_UNLOCK(inp);
		return (ENOTCONN);
	}

	in_pcbdisconnect(inp);
	inp->inp_laddr.s_addr = INADDR_ANY;
	SOCK_LOCK(so);
	so->so_state &= ~SS_ISCONNECTED;		/* XXX */
	SOCK_UNLOCK(so);
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&udbinfo);
	return (0);
}

static int
udp_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct thread *td)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_send: inp == NULL"));
	return (udp_output(inp, m, addr, control, td));
}

int
udp_shutdown(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("udp_shutdown: inp == NULL"));
	INP_LOCK(inp);
	socantsendmore(so);
	INP_UNLOCK(inp);
	return (0);
}

struct pr_usrreqs udp_usrreqs = {
	.pru_abort =		udp_abort,
	.pru_attach =		udp_attach,
	.pru_bind =		udp_bind,
	.pru_connect =		udp_connect,
	.pru_control =		in_control,
	.pru_detach =		udp_detach,
	.pru_disconnect =	udp_disconnect,
	.pru_peeraddr =		in_getpeeraddr,
	.pru_send =		udp_send,
	.pru_sosend =		sosend_dgram,
	.pru_shutdown =		udp_shutdown,
	.pru_sockaddr =		in_getsockaddr,
	.pru_sosetlabel =	in_pcbsosetlabel,
	.pru_close =		udp_close,
};
