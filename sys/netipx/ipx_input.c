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
 *	@(#)ipx_input.c
 *
 * $Id: ipx_input.c,v 1.4 1995/11/04 09:02:54 julian Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/raw_cb.h>

#include <netipx/ipx.h>
#include <netipx/spx.h>
#include <netipx/ipx_if.h>
#include <netipx/ipx_pcb.h>
#include <netipx/ipx_var.h>
#include <netipx/ipx_error.h>

#ifndef	IPXFORWARDING
#ifdef GATEWAY
#define	IPXFORWARDING	1	/* forward IPX packets not for us */
#else
#define	IPXFORWARDING	0	/* don't forward IPX packets not for us */
#endif
#endif
#ifndef IPXPRINTFS
#define IPXPRINTFS	1	/* printing forwarding information */
#endif

#ifndef IPXCKSUM
#define IPXCKSUM	0	/* perform IPX checksum */
#endif

#ifndef IXDONOSOCKS
#define IPXDONOSOCKS	0	/* return no socket errors */
#endif

int ipxcksum = IPXCKSUM;
int ipxprintfs = IPXPRINTFS;
int ipxdonosocks = IPXDONOSOCKS;
int ipxforwarding = IPXFORWARDING;

union ipx_host	ipx_thishost;
union ipx_net	ipx_zeronet;
union ipx_host	ipx_zerohost;

union ipx_net	ipx_broadnet;
union ipx_host	ipx_broadhost;

struct ipxstat ipxstat;
struct sockaddr_ipx ipx_netmask, ipx_hostmask;

int ipxintr_getpck = 0;
int ipxintr_swtch = 0;

static u_short allones[] = {-1, -1, -1};

struct ipxpcb ipxpcb;
struct ipxpcb ipxrawpcb;

struct ifqueue	ipxintrq;
int	ipxqmaxlen = IFQ_MAXLEN;

long	ipx_pexseq;

NETISR_SET(NETISR_IPX, ipxintr);

/*
 * IPX initialization.
 */

void
ipx_init()
{
	ipx_broadnet = * (union ipx_net *) allones;
	ipx_broadhost = * (union ipx_host *) allones;

	ipx_pexseq = time.tv_usec;
	ipxintrq.ifq_maxlen = ipxqmaxlen;
	ipxpcb.ipxp_next = ipxpcb.ipxp_prev = &ipxpcb;
	ipxrawpcb.ipxp_next = ipxrawpcb.ipxp_prev = &ipxrawpcb;

	ipx_netmask.sipx_len = 6;
	ipx_netmask.sipx_addr.x_net = ipx_broadnet;

	ipx_hostmask.sipx_len = 12;
	ipx_hostmask.sipx_addr.x_net = ipx_broadnet;
	ipx_hostmask.sipx_addr.x_host = ipx_broadhost;
}

/*
 * IPX input routine.  Pass to next level.
 */
void
ipxintr()
{
	register struct ipx *ipx;
	register struct mbuf *m;
	register struct ipxpcb *ipxp;
	register int i;
	int len, s, error;
	char oddpacketp;

next:
	/*
	 * Get next datagram off input queue and get IPX header
	 * in first mbuf.
	 */
	s = splimp();
	IF_DEQUEUE(&ipxintrq, m);
	splx(s);
	ipxintr_getpck++;
	if (m == 0)
		return;
	if ((m->m_flags & M_EXT || m->m_len < sizeof (struct ipx)) &&
	    (m = m_pullup(m, sizeof (struct ipx))) == 0) {
		ipxstat.ipxs_toosmall++;
		goto next;
	}

	/*
	 * Give any raw listeners a crack at the packet
	 */
	for (ipxp = ipxrawpcb.ipxp_next; ipxp != &ipxrawpcb; ipxp = ipxp->ipxp_next) {
		struct mbuf *m1 = m_copy(m, 0, (int)M_COPYALL);
		if (m1) ipx_input(m1, ipxp);
	}

	ipx = mtod(m, struct ipx *);
	len = ntohs(ipx->ipx_len);
	if (oddpacketp = len & 1) {
		len++;		/* If this packet is of odd length,
				   preserve garbage byte for checksum */
	}

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IPX header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len < len) {
		ipxstat.ipxs_tooshort++;
		goto bad;
	}
	if (m->m_pkthdr.len > len) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = len;
			m->m_pkthdr.len = len;
		} else
			m_adj(m, len - m->m_pkthdr.len);
	}
	if (ipxcksum && ((i = ipx->ipx_sum)!=0xffff)) {
		ipx->ipx_sum = 0;
		if (i != (ipx->ipx_sum = ipx_cksum(m, len))) {
			ipxstat.ipxs_badsum++;
			ipx->ipx_sum = i;
			if (ipx_hosteqnh(ipx_thishost, ipx->ipx_dna.x_host))
				error = IPX_ERR_BADSUM;
			else
				error = IPX_ERR_BADSUM_T;
			ipx_error(m, error, 0);
			goto next;
		}
	}
	/*
	 * Is this a directed broadcast?
	 */
	if (ipx_hosteqnh(ipx_broadhost,ipx->ipx_dna.x_host)) {
		if ((!ipx_neteq(ipx->ipx_dna, ipx->ipx_sna)) &&
		    (!ipx_neteqnn(ipx->ipx_dna.x_net, ipx_broadnet)) &&
		    (!ipx_neteqnn(ipx->ipx_sna.x_net, ipx_zeronet)) &&
		    (!ipx_neteqnn(ipx->ipx_dna.x_net, ipx_zeronet)) ) {
			/*
			 * Look to see if I need to eat this packet.
			 * Algorithm is to forward all young packets
			 * and prematurely age any packets which will
			 * by physically broadcasted.
			 * Any very old packets eaten without forwarding
			 * would die anyway.
			 *
			 * Suggestion of Bill Nesheim, Cornell U.
			 */
			if (ipx->ipx_tc < IPX_MAXHOPS) {
				ipx_forward(m);
				goto next;
			}
		}
	/*
	 * Is this our packet? If not, forward.
	 */
	} else if (!ipx_hosteqnh(ipx_thishost,ipx->ipx_dna.x_host)) {
		ipx_forward(m);
		goto next;
	}
	/*
	 * Locate pcb for datagram.
	 */
	ipxp = ipx_pcblookup(&ipx->ipx_sna, ipx->ipx_dna.x_port, IPX_WILDCARD);
	/*
	 * Switch out to protocol's input routine.
	 */
	ipxintr_swtch++;
	if (ipxp) {
		if (oddpacketp) {
			m_adj(m, -1);
		}
		if ((ipxp->ipxp_flags & IPXP_ALL_PACKETS)==0)
			switch (ipx->ipx_pt) {

			    case IPXPROTO_SPX:
				    spx_input(m, ipxp);
				    goto next;

			    case IPXPROTO_ERROR:
				    ipx_err_input(m);
				    goto next;
			}
		ipx_input(m, ipxp);
	} else {
		ipx_error(m, IPX_ERR_NOSOCK, 0);
	}
	goto next;

bad:
	m_freem(m);
	goto next;
}

u_char ipxctlerrmap[PRC_NCMDS] = {
	ECONNABORTED,	ECONNABORTED,	0,		0,
	0,		0,		EHOSTDOWN,	EHOSTUNREACH,
	ENETUNREACH,	EHOSTUNREACH,	ECONNREFUSED,	ECONNREFUSED,
	EMSGSIZE,	0,		0,		0,
	0,		0,		0,		0
};

void
ipx_ctlinput(cmd, arg)
	int cmd;
	caddr_t arg;
{
	struct ipx_addr *ipx;
	struct ipxpcb *ipxp;
	struct ipx_errp *errp;
	int type;

	if (cmd < 0 || cmd > PRC_NCMDS)
		return;
	if (ipxctlerrmap[cmd] == 0)
		return;		/* XXX */
	type = IPX_ERR_UNREACH_HOST;
	errp = (struct ipx_errp *)arg;
	switch (cmd) {
		struct sockaddr_ipx *sipx;

	case PRC_IFDOWN:
	case PRC_HOSTDEAD:
	case PRC_HOSTUNREACH:
		sipx = (struct sockaddr_ipx *)arg;
		if (sipx->sipx_family != AF_IPX)
			return;
		ipx = &sipx->sipx_addr;
		break;

	default:
		ipx = &errp->ipx_err_ipx.ipx_dna;
		type = errp->ipx_err_num;
		type = ntohs((u_short)type);
		break;
	}
	switch (type) {

	case IPX_ERR_UNREACH_HOST:
		ipx_pcbnotify(ipx, (int)ipxctlerrmap[cmd], ipx_abort, (long)0);
		break;

	case IPX_ERR_NOSOCK:
		ipxp = ipx_pcblookup(ipx, errp->ipx_err_ipx.ipx_sna.x_port,
			IPX_WILDCARD);
		if(ipxp && ipxdonosocks && ! ipx_nullhost(ipxp->ipxp_faddr))
			(void) ipx_drop(ipxp, (int)ipxctlerrmap[cmd]);
	}
}

/*
 * Forward a packet.  If some error occurs return the sender
 * an error packet.  Note we can't always generate a meaningful
 * error message because the IPX errors don't have a large enough repetoire
 * of codes and types.
 */
struct route ipx_droute;
struct route ipx_sroute;

void
ipx_forward(m)
struct mbuf *m;
{
	register struct ipx *ipx = mtod(m, struct ipx *);
	register int error, type, code;
	struct mbuf *mcopy = NULL;
	int agedelta = 1;
	int flags = IPX_FORWARDING;
	int ok_there = 0;
	int ok_back = 0;

	if (ipxforwarding == 0) {
		/* can't tell difference between net and host */
		type = IPX_ERR_UNREACH_HOST, code = 0;
		goto senderror;
	}
	ipx->ipx_tc++;
	if (ipx->ipx_tc > IPX_MAXHOPS) {
		type = IPX_ERR_TOO_OLD, code = 0;
		goto senderror;
	}
	/*
	 * Save at most 42 bytes of the packet in case
	 * we need to generate an IPX error message to the src.
	 */
	mcopy = m_copy(m, 0, imin((int)ntohs(ipx->ipx_len), 42));

	if ((ok_there = ipx_do_route(&ipx->ipx_dna,&ipx_droute))==0) {
		type = IPX_ERR_UNREACH_HOST, code = 0;
		goto senderror;
	}
	/*
	 * Here we think about  forwarding  broadcast packets,
	 * so we try to insure that it doesn't go back out
	 * on the interface it came in on.  Also, if we
	 * are going to physically broadcast this, let us
	 * age the packet so we can eat it safely the second time around.
	 */
	if (ipx->ipx_dna.x_host.c_host[0] & 0x1) {
		struct ipx_ifaddr *ia = ipx_iaonnetof(&ipx->ipx_dna);
		struct ifnet *ifp;
		if (ia) {
			/* I'm gonna hafta eat this packet */
			agedelta += IPX_MAXHOPS - ipx->ipx_tc;
			ipx->ipx_tc = IPX_MAXHOPS;
		}
		if ((ok_back = ipx_do_route(&ipx->ipx_sna,&ipx_sroute))==0) {
			/* error = ENETUNREACH; He'll never get it! */
			m_freem(m);
			goto cleanup;
		}
		if (ipx_droute.ro_rt &&
		    (ifp=ipx_droute.ro_rt->rt_ifp) &&
		    ipx_sroute.ro_rt &&
		    (ifp!=ipx_sroute.ro_rt->rt_ifp)) {
			flags |= IPX_ALLOWBROADCAST;
		} else {
			type = IPX_ERR_UNREACH_HOST, code = 0;
			goto senderror;
		}
	}
	/* need to adjust checksum */
	if (ipxcksum && ipx->ipx_sum != 0xffff) {
		union bytes {
			u_char c[4];
			u_short s[2];
			long l;
		} x;
		register int shift;
		x.l = 0; x.c[0] = agedelta;
		shift = (((((int)ntohs(ipx->ipx_len))+1)>>1)-2) & 0xf;
		x.l = ipx->ipx_sum + (x.s[0] << shift);
		x.l = x.s[0] + x.s[1];
		x.l = x.s[0] + x.s[1];
		if (x.l==0xffff) ipx->ipx_sum = 0; else ipx->ipx_sum = x.l;
	} else 
		ipx->ipx_sum = 0xffff;

	error = ipx_outputfl(m, &ipx_droute, flags);

	if (ipxprintfs && !error) {
		printf("forward: ");
		ipx_printhost(&ipx->ipx_sna);
		printf(" to ");
		ipx_printhost(&ipx->ipx_dna);
		printf(" hops %d\n", ipx->ipx_tc);
	}

	if (error && mcopy != NULL) {
		ipx = mtod(mcopy, struct ipx *);
		type = IPX_ERR_UNSPEC_T, code = 0;
		switch (error) {

		case ENETUNREACH:
		case EHOSTDOWN:
		case EHOSTUNREACH:
		case ENETDOWN:
		case EPERM:
			type = IPX_ERR_UNREACH_HOST;
			break;

		case EMSGSIZE:
			type = IPX_ERR_TOO_BIG;
			code = 576; /* too hard to figure out mtu here */
			break;

		case ENOBUFS:
			type = IPX_ERR_UNSPEC_T;
			break;
		}
		mcopy = NULL;
	senderror:
		ipx_error(m, type, code);
	}
cleanup:
	if (ok_there)
		ipx_undo_route(&ipx_droute);
	if (ok_back)
		ipx_undo_route(&ipx_sroute);
	if (mcopy != NULL)
		m_freem(mcopy);
}

int
ipx_do_route(src, ro)
struct ipx_addr *src;
struct route *ro;
{
	struct sockaddr_ipx *dst;

	bzero((caddr_t)ro, sizeof (*ro));
	dst = (struct sockaddr_ipx *)&ro->ro_dst;

	dst->sipx_len = sizeof(*dst);
	dst->sipx_family = AF_IPX;
	dst->sipx_addr = *src;
	dst->sipx_addr.x_port = 0;
	rtalloc(ro);
	if (ro->ro_rt == 0 || ro->ro_rt->rt_ifp == 0) {
		return (0);
	}
	ro->ro_rt->rt_use++;
	return (1);
}

void
ipx_undo_route(ro)
register struct route *ro;
{
	if (ro->ro_rt) {RTFREE(ro->ro_rt);}
}

void
ipx_watch_output(m, ifp)
struct mbuf *m;
struct ifnet *ifp;
{
	register struct ipxpcb *ipxp;
	register struct ifaddr *ifa;
	/*
	 * Give any raw listeners a crack at the packet
	 */
	for (ipxp = ipxrawpcb.ipxp_next; ipxp != &ipxrawpcb; ipxp = ipxp->ipxp_next) {
		struct mbuf *m0 = m_copy(m, 0, (int)M_COPYALL);
		if (m0) {
			register struct ipx *ipx;

			M_PREPEND(m0, sizeof (*ipx), M_DONTWAIT);
			if (m0 == NULL)
				continue;
			ipx = mtod(m0, struct ipx *);
			ipx->ipx_sna.x_net = ipx_zeronet;
			ipx->ipx_sna.x_host = ipx_thishost;
			if (ifp && (ifp->if_flags & IFF_POINTOPOINT))
			    for(ifa = ifp->if_addrlist; ifa;
						ifa = ifa->ifa_next) {
				if (ifa->ifa_addr->sa_family==AF_IPX) {
				    ipx->ipx_sna = IA_SIPX(ifa)->sipx_addr;
				    break;
				}
			    }
			ipx->ipx_len = ntohl(m0->m_pkthdr.len);
			ipx_input(m0, ipxp);
		}
	}
}
