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
 *	@(#)ipx_outputfl.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/route.h>

#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#include <netipx/ipx_var.h>

#ifdef vax
#include <machine/mtpr.h>
#endif

int ipx_hold_output = 0;
int ipx_copy_output = 0;
int ipx_outputfl_cnt = 0;
struct mbuf *ipx_lastout;

int
ipx_outputfl(m0, ro, flags)
	struct mbuf *m0;
	struct route *ro;
	int flags;
{
	register struct ipx *ipx = mtod(m0, struct ipx *);
	register struct ifnet *ifp = 0;
	int error = 0;
	struct sockaddr_ipx *dst;
	struct route ipxroute;

	if (ipx_hold_output) {
		if (ipx_lastout) {
			(void)m_free(ipx_lastout);
		}
		ipx_lastout = m_copy(m0, 0, (int)M_COPYALL);
	}
	/*
	 * Route packet.
	 */
	if (ro == 0) {
		ro = &ipxroute;
		bzero((caddr_t)ro, sizeof (*ro));
	}
	dst = (struct sockaddr_ipx *)&ro->ro_dst;
	if (ro->ro_rt == 0) {
		dst->sipx_family = AF_IPX;
		dst->sipx_len = sizeof (*dst);
		dst->sipx_addr = ipx->ipx_dna;
		dst->sipx_addr.x_port = 0;
		/*
		 * If routing to interface only,
		 * short circuit routing lookup.
		 */
		if (flags & IPX_ROUTETOIF) {
			struct ipx_ifaddr *ia = ipx_iaonnetof(&ipx->ipx_dna);

			if (ia == 0) {
				error = ENETUNREACH;
				goto bad;
			}
			ifp = ia->ia_ifp;
			goto gotif;
		}
		rtalloc(ro);
	} else if ((ro->ro_rt->rt_flags & RTF_UP) == 0) {
		/*
		 * The old route has gone away; try for a new one.
		 */
		rtfree(ro->ro_rt);
		ro->ro_rt = NULL;
		rtalloc(ro);
	}
	if (ro->ro_rt == 0 || (ifp = ro->ro_rt->rt_ifp) == 0) {
		error = ENETUNREACH;
		goto bad;
	}
	ro->ro_rt->rt_use++;
	if (ro->ro_rt->rt_flags & (RTF_GATEWAY|RTF_HOST))
		dst = (struct sockaddr_ipx *)ro->ro_rt->rt_gateway;
gotif:
	/*
	 * Look for multicast addresses and
	 * and verify user is allowed to send
	 * such a packet.
	 */
	if (dst->sipx_addr.x_host.c_host[0]&1) {
		if ((ifp->if_flags & IFF_BROADCAST) == 0) {
			error = EADDRNOTAVAIL;
			goto bad;
		}
		if ((flags & IPX_ALLOWBROADCAST) == 0) {
			error = EACCES;
			goto bad;
		}
	}

	if (htons(ipx->ipx_len) <= ifp->if_mtu) {
		ipx_outputfl_cnt++;
		if (ipx_copy_output) {
			ipx_watch_output(m0, ifp);
		}
		error = (*ifp->if_output)(ifp, m0,
					(struct sockaddr *)dst, ro->ro_rt);
		goto done;
	} else error = EMSGSIZE;
bad:
	if (ipx_copy_output) {
		ipx_watch_output(m0, ifp);
	}
	m_freem(m0);
done:
	if (ro == &ipxroute && (flags & IPX_ROUTETOIF) == 0 && ro->ro_rt) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = 0;
	}
	return (error);
}
