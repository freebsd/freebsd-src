/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1995 John Hay.  All rights reserved.
 *
 * This file includes significant work done at Cornell University by
 * Bill Nesheim.  That work included by permission.
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
 * $FreeBSD$
 */

#ifndef lint
static const char sccsid[] = "@(#)af.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

#include "defs.h"

/*
 * Address family support routines
 */
af_hash_t	null_hash;
af_netmatch_t	null_netmatch;
af_output_t	null_output;
af_portmatch_t	null_portmatch;
af_portcheck_t	null_portcheck;
af_checkhost_t	null_checkhost;
af_ishost_t	null_ishost;
af_canon_t	null_canon;

void	ipxnet_hash(struct sockaddr_ipx *, struct afhash *);
int	ipxnet_netmatch(struct sockaddr_ipx *, struct sockaddr_ipx *);
void	ipxnet_output(int, int, struct sockaddr_ipx *, int);
int	ipxnet_portmatch(struct sockaddr_ipx *);
int	ipxnet_checkhost(struct sockaddr_ipx *);
int	ipxnet_ishost(struct sockaddr_ipx *);
void	ipxnet_canon(struct sockaddr_ipx *);

#define NIL \
	{ null_hash,		null_netmatch,		null_output, \
	  null_portmatch,	null_portcheck,		null_checkhost, \
	  null_ishost,		null_canon }
#define	IPXNET \
	{ (af_hash_t *)ipxnet_hash, \
	  (af_netmatch_t *)ipxnet_netmatch, \
	  (af_output_t *)ipxnet_output, \
	  (af_portmatch_t *)ipxnet_portmatch, \
	  (af_portcheck_t *)ipxnet_portmatch, \
	  (af_checkhost_t *)ipxnet_checkhost, \
	  (af_ishost_t *)ipxnet_ishost, \
	  (af_canon_t *)ipxnet_canon }

struct afswitch afswitch[AF_MAX] =
	{ NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,
	  NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL,
	  NIL, NIL, NIL, IPXNET, NIL, NIL };

struct sockaddr_ipx ipxnet_default = { sizeof(struct sockaddr_ipx), AF_IPX };

union ipx_net ipx_anynet;
union ipx_net ipx_zeronet;

void
ipxnet_hash(sipx, hp)
	register struct sockaddr_ipx *sipx;
	struct afhash *hp;
{
	long hash;
#if 0
	u_short *s = sipx->sipx_addr.x_host.s_host;
#endif
	u_char *c;

	c = sipx->sipx_addr.x_net.c_net;

#define IMVAL	33
	hash = 0;
	hash = hash * IMVAL + *c++;
	hash = hash * IMVAL + *c++;
	hash = hash * IMVAL + *c++;
	hash = hash * IMVAL + *c++;
#undef IMVAL

	hp->afh_nethash = hash;
	hp->afh_nethash ^= (hash >> 8);
	hp->afh_nethash ^= (hash >> 16);
	hp->afh_nethash ^= (hash >> 24);

#if 0
	hash = 0;
	hash = *s++; hash <<= 8; hash += *s++; hash <<= 8; hash += *s;
	hp->afh_hosthash = hash;
#endif
}

int
ipxnet_netmatch(sxn1, sxn2)
	struct sockaddr_ipx *sxn1, *sxn2;
{
	return (ipx_neteq(sxn1->sipx_addr, sxn2->sipx_addr));
}

/*
 * Verify the message is from the right port.
 */
int
ipxnet_portmatch(sipx)
	register struct sockaddr_ipx *sipx;
{
	
	return (ntohs(sipx->sipx_addr.x_port) == IPXPORT_RIP );
}


/*
 * ipx output routine.
 */
#ifdef DEBUG
int do_output = 0;
#endif
void
ipxnet_output(s, flags, sipx, size)
	int s;
	int flags;
	struct sockaddr_ipx *sipx;
	int size;
{
	struct sockaddr_ipx dst;

	dst = *sipx;
	sipx = &dst;
	if (sipx->sipx_addr.x_port == 0)
		sipx->sipx_addr.x_port = htons(IPXPORT_RIP);
#ifdef DEBUG
	if(do_output || ntohs(msg->rip_cmd) == RIPCMD_REQUEST)
#endif	
	/*
	 * Kludge to allow us to get routes out to machines that
	 * don't know their addresses yet; send to that address on
	 * ALL connected nets
	 */
	 if (ipx_neteqnn(sipx->sipx_addr.x_net, ipx_zeronet)) {
	 	extern  struct interface *ifnet;
	 	register struct interface *ifp;
		
		for (ifp = ifnet; ifp; ifp = ifp->int_next) {
			sipx->sipx_addr.x_net = 
				satoipx_addr(ifp->int_addr).x_net;
			(void) sendto(s, msg, size, flags,
			    (struct sockaddr *)sipx, sizeof (*sipx));
		}
		return;
	}
	
	(void) sendto(s, msg, size, flags,
	    (struct sockaddr *)sipx, sizeof (*sipx));
}

/*
 * Return 1 if we want this route.
 * We use this to disallow route net G entries for one for multiple
 * point to point links.
 */
int
ipxnet_checkhost(sipx)
	struct sockaddr_ipx *sipx;
{
	register struct interface *ifp = if_ifwithnet((struct sockaddr *)sipx);
	/*
	 * We want this route if there is no more than one 
	 * point to point interface with this network.
	 */
	if (ifp == 0 || (ifp->int_flags & IFF_POINTOPOINT)==0) return (1);
	return (ifp->int_sq.n == ifp->int_sq.p);
}

/*
 * Return 1 if the address is
 * for a host, 0 for a network.
 */
int
ipxnet_ishost(sipx)
struct sockaddr_ipx *sipx;
{
	register u_short *s = sipx->sipx_addr.x_host.s_host;

	if ((s[0]==0x0000) && (s[1]==0x0000) && (s[2]==0x0000))
		return (0);
	if ((s[0]==0xffff) && (s[1]==0xffff) && (s[2]==0xffff))
		return (0);

	return (1);
}

void
ipxnet_canon(sipx)
	struct sockaddr_ipx *sipx;
{

	sipx->sipx_addr.x_port = 0;
}

void
null_hash(addr, hp)
	struct sockaddr *addr;
	struct afhash *hp;
{

	hp->afh_nethash = hp->afh_hosthash = 0;
}

int
null_netmatch(a1, a2)
	struct sockaddr *a1, *a2;
{

	return (0);
}

void
null_output(s, f, a1, n)
	int s;
	int f;
	struct sockaddr *a1;
	int n;
{

	;
}

int
null_portmatch(a1)
	struct sockaddr *a1;
{

	return (0);
}

int
null_portcheck(a1)
	struct sockaddr *a1;
{

	return (0);
}

int
null_ishost(a1)
	struct sockaddr *a1;
{

	return (0);
}

int
null_checkhost(a1)
	struct sockaddr *a1;
{

	return (0);
}

void
null_canon(a1)
	struct sockaddr *a1;
{

	;
}

