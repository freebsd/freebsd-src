/*
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1988, 1993
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
 *	@(#)$Id$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>

#include <netipx/ipx.h>
#include <netipx/spx.h>
#include <netipx/ipx_pcb.h>
#include <netipx/ipx_error.h>

/*
 * IPX_ERR routines: error generation, receive packet processing, and
 * routines to turnaround packets back to the originator.
 */
#ifndef IPX_ERRPRINTFS
#define IPX_ERRPRINTFS 0
#endif
int	ipx_errprintfs = IPX_ERRPRINTFS;

struct ipx_errstat ipx_errstat;

int
ipx_err_x(c)
int c;
{
	register u_short *w, *lim, *base = ipx_errstat.ipx_es_codes;
	u_short x = c;

	/*
	 * zero is a legit error code, handle specially
	 */
	if (x == 0)
		return (0);
	lim = base + IPX_ERR_MAX - 1;
	for (w = base + 1; w < lim; w++) {
		if (*w == 0)
			*w = x;
		if (*w == x)
			break;
	}
	return (w - base);
}

/*
 * Generate an error packet of type error
 * in response to bad packet.
 */

void
ipx_error(om, type, param)
	struct mbuf *om;
	int type, param;
{
	register struct ipx_epipx *ep;
	struct mbuf *m;
	struct ipx *nip;
	register struct ipx *oip = mtod(om, struct ipx *);

	/*
	 * If this packet was sent to the echo port,
	 * and nobody was there, just echo it.
	 * (Yes, this is a wart!)
	 */
	if (type == IPX_ERR_NOSOCK &&
	    oip->ipx_dna.x_port == htons(2) &&
	    (type = ipx_echo(om))==0)
		return;

	if (ipx_errprintfs)
		printf("ipx_error(%x, %u, %d)\n", oip, type, param);

	/*
	 * Don't Generate error packets in response to multicasts.
	 */
	if (oip->ipx_dna.x_host.c_host[0] & 1)
		goto freeit;

	ipx_errstat.ipx_es_error++;
	/*
	 * Make sure that the old IPX packet had 30 bytes of data to return;
	 * if not, don't bother.  Also don't EVER error if the old
	 * packet protocol was IPX_ERR.
	 */
	if (oip->ipx_len < sizeof(struct ipx)) {
		ipx_errstat.ipx_es_oldshort++;
		goto freeit;
	}
	if (oip->ipx_pt == IPXPROTO_ERROR) {
		ipx_errstat.ipx_es_oldipx_err++;
		goto freeit;
	}

	/*
	 * First, formulate ipx_err message
	 */
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		goto freeit;
	m->m_len = sizeof(*ep);
	MH_ALIGN(m, m->m_len);
	ep = mtod(m, struct ipx_epipx *);
	if ((u_int)type > IPX_ERR_TOO_BIG)
		panic("ipx_err_error");
	ipx_errstat.ipx_es_outhist[ipx_err_x(type)]++;
	ep->ipx_ep_errp.ipx_err_num = htons((u_short)type);
	ep->ipx_ep_errp.ipx_err_param = htons((u_short)param);
	bcopy((caddr_t)oip, (caddr_t)&ep->ipx_ep_errp.ipx_err_ipx, 42);
	nip = &ep->ipx_ep_ipx;
	nip->ipx_len = sizeof(*ep);
	nip->ipx_len = htons((u_short)nip->ipx_len);
	nip->ipx_pt = IPXPROTO_ERROR;
	nip->ipx_tc = 0;
	nip->ipx_dna = oip->ipx_sna;
	nip->ipx_sna = oip->ipx_dna;
	if (ipxcksum) {
		nip->ipx_sum = 0;
		nip->ipx_sum = ipx_cksum(m, sizeof(*ep));
	} else 
		nip->ipx_sum = 0xffff;
	(void) ipx_outputfl(m, (struct route *)0, 0);

freeit:
	m_freem(om);
}

void
ipx_printhost(addr)
register struct ipx_addr *addr;
{
	u_short port;
	struct ipx_addr work = *addr;
	register char *p; register u_char *q;
	register char *net = "", *host = "";
	char cport[10], chost[15], cnet[15];

	port = ntohs(work.x_port);

	if (ipx_nullnet(work) && ipx_nullhost(work)) {

		if (port)
			printf("*.%x", port);
		else
			printf("*.*");

		return;
	}

	if (ipx_wildnet(work))
		net = "any";
	else if (ipx_nullnet(work))
		net = "*";
	else {
		q = work.x_net.c_net;
		sprintf(cnet, "%x%x%x%x",
			q[0], q[1], q[2], q[3]);
		for (p = cnet; *p == '0' && p < cnet + 8; p++)
			continue;
		net = p;
	}

	if (ipx_wildhost(work))
		host = "any";
	else if (ipx_nullhost(work))
		host = "*";
	else {
		q = work.x_host.c_host;
		sprintf(chost, "%x%x%x%x%x%x",
			q[0], q[1], q[2], q[3], q[4], q[5]);
		for (p = chost; *p == '0' && p < chost + 12; p++)
			continue;
		host = p;
	}

	if (port) {
		if (strcmp(host, "*") == 0) {
			host = "";
			sprintf(cport, "%x", port);
		} else
			sprintf(cport, ".%x", port);
	} else
		*cport = 0;

	printf("%s.%s%s", net, host, cport);
}

/*
 * Process a received IPX_ERR message.
 */
void
ipx_err_input(m)
	struct mbuf *m;
{
	register struct ipx_errp *ep;
	register struct ipx_epipx *epipx = mtod(m, struct ipx_epipx *);
	register int i;
	int type, code, param;

	/*
	 * Locate ipx_err structure in mbuf, and check
	 * that not corrupted and of at least minimum length.
	 */

	if (ipx_errprintfs) {
		printf("ipx_err_input ");
		ipx_printhost(&epipx->ipx_ep_ipx.ipx_sna);
		printf("%d\n", ntohs(epipx->ipx_ep_ipx.ipx_len));
	}

	i = sizeof (struct ipx_epipx);
 	if (((m->m_flags & M_EXT) || m->m_len < i) &&
 		(m = m_pullup(m, i)) == 0)  {
		ipx_errstat.ipx_es_tooshort++;
		return;
	}
	ep = &(mtod(m, struct ipx_epipx *)->ipx_ep_errp);
	type = ntohs(ep->ipx_err_num);
	param = ntohs(ep->ipx_err_param);
	ipx_errstat.ipx_es_inhist[ipx_err_x(type)]++;

	/*
	 * Message type specific processing.
	 */
	if (ipx_errprintfs)
		printf("ipx_err_input, type %d param %d\n", type, param);

	if (type >= IPX_ERR_TOO_BIG) {
		goto badcode;
	}
	ipx_errstat.ipx_es_outhist[ipx_err_x(type)]++;
	switch (type) {

	case IPX_ERR_UNREACH_HOST:
		code = PRC_UNREACH_NET;
		goto deliver;

	case IPX_ERR_TOO_OLD:
		code = PRC_TIMXCEED_INTRANS;
		goto deliver;

	case IPX_ERR_TOO_BIG:
		code = PRC_MSGSIZE;
		goto deliver;

	case IPX_ERR_FULLUP:
		code = PRC_QUENCH;
		goto deliver;

	case IPX_ERR_NOSOCK:
		code = PRC_UNREACH_PORT;
		goto deliver;

	case IPX_ERR_UNSPEC_T:
	case IPX_ERR_BADSUM_T:
	case IPX_ERR_BADSUM:
	case IPX_ERR_UNSPEC:
		code = PRC_PARAMPROB;
		goto deliver;

	deliver:
		/*
		 * Problem with datagram; advise higher level routines.
		 */

		if (ipx_errprintfs)
			printf("deliver to protocol %d\n",
				       ep->ipx_err_ipx.ipx_pt);

		switch(ep->ipx_err_ipx.ipx_pt) {
		case IPXPROTO_SPX:
			spx_ctlinput(code, (caddr_t)ep);
			break;

		default:
			ipx_ctlinput(code, (caddr_t)ep);
		}
		
		goto freeit;

	default:
	badcode:
		ipx_errstat.ipx_es_badcode++;
		goto freeit;

	}
freeit:
	m_freem(m);
}

#ifdef notdef
u_long
ipxtime()
{
	int s = splclock();
	u_long t;

	t = (time.tv_sec % (24*60*60)) * 1000 + time.tv_usec / 1000;
	splx(s);
	return (htonl(t));
}
#endif

int
ipx_echo(m)
struct mbuf *m;
{
	register struct ipx *ipx = mtod(m, struct ipx *);
	register struct echo {
	    struct ipx	ec_ipx;
	    u_short ec_op; /* Operation, 1 = request, 2 = reply */
	} *ec = (struct echo *)ipx;
	struct ipx_addr temp;

	if (ipx->ipx_pt!=IPXPROTO_ECHO)
		return(IPX_ERR_NOSOCK);
	if (ec->ec_op!=htons(1))
		return(IPX_ERR_UNSPEC);

	ec->ec_op = htons(2);

	temp = ipx->ipx_dna;
	ipx->ipx_dna = ipx->ipx_sna;
	ipx->ipx_sna = temp;

	if (ipxcksum && ipx->ipx_sum != 0xffff) {
		ipx->ipx_sum = 0;
		ipx->ipx_sum = ipx_cksum(m,
		    (int)(((ntohs(ipx->ipx_len) - 1)|1)+1));
	}
	else
		ipx->ipx_sum = 0xffff;

	(void) ipx_outputfl(m, (struct route *)0, IPX_FORWARDING);

	return(0);
}
