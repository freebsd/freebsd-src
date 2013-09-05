/*      $NetBSD: if_atm.c,v 1.6 1996/10/13 02:03:01 christos Exp $       */

/*-
 *
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * IP <=> ATM address resolution.
 */
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_natm.h"

#if defined(INET) || defined(INET6)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/if_atm.h>

#include <netinet/in.h>
#include <netinet/if_atm.h>

#ifdef NATM
#include <netnatm/natm.h>
#endif

#define SDL(s) ((struct sockaddr_dl *)s)

#define	GET3BYTE(V, A, L)	do {				\
	(V) = ((A)[0] << 16) | ((A)[1] << 8) | (A)[2];		\
	(A) += 3;						\
	(L) -= 3;						\
    } while (0)

#define GET2BYTE(V, A, L)	do {				\
	(V) = ((A)[0] << 8) | (A)[1];				\
	(A) += 2;						\
	(L) -= 2;						\
    } while (0)

#define GET1BYTE(V, A, L)	do {				\
	(V) = *(A)++;						\
	(L)--;							\
    } while (0)


/*
 * atm_rtrequest: handle ATM rt request (in support of generic code)
 *   inputs: "req" = request code
 *           "rt" = route entry
 *           "info" = rt_addrinfo
 */
void
atm_rtrequest(int req, struct rtentry *rt, struct rt_addrinfo *info)
{
	struct sockaddr *gate = rt->rt_gateway;
	struct atmio_openvcc op;
	struct atmio_closevcc cl;
	u_char *addr;
	u_int alen;
#ifdef NATM
	struct sockaddr_in *sin;
	struct natmpcb *npcb = NULL;
#endif
	static struct sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK};

	if (rt->rt_flags & RTF_GATEWAY)   /* link level requests only */
		return;

	switch (req) {

	case RTM_RESOLVE: /* resolve: only happens when cloning */
		printf("atm_rtrequest: RTM_RESOLVE request detected?\n");
		break;

	case RTM_ADD:
		/*
		 * route added by a command (e.g. ifconfig, route, arp...).
		 *
		 * first check to see if this is not a host route, in which
		 * case we are being called via "ifconfig" to set the address.
		 */
		if ((rt->rt_flags & RTF_HOST) == 0) {
			rt_setgate(rt,rt_key(rt),(struct sockaddr *)&null_sdl);
			gate = rt->rt_gateway;
			SDL(gate)->sdl_type = rt->rt_ifp->if_type;
			SDL(gate)->sdl_index = rt->rt_ifp->if_index;
			break;
		}

		if (gate->sa_family != AF_LINK ||
		    gate->sa_len < sizeof(null_sdl)) {
			log(LOG_DEBUG, "atm_rtrequest: bad gateway value");
			break;
		}

		KASSERT(rt->rt_ifp->if_ioctl != NULL,
		    ("atm_rtrequest: null ioctl"));

		/*
		 * Parse and verify the link level address as
		 * an open request
		 */
#ifdef NATM
		NATM_LOCK();
#endif
		bzero(&op, sizeof(op));
		addr = LLADDR(SDL(gate));
		alen = SDL(gate)->sdl_alen;
		if (alen < 4) {
			printf("%s: bad link-level address\n", __func__);
			goto failed;
		}

		if (alen == 4) {
			/* old type address */
			GET1BYTE(op.param.flags, addr, alen);
			GET1BYTE(op.param.vpi, addr, alen);
			GET2BYTE(op.param.vci, addr, alen);
			op.param.traffic = ATMIO_TRAFFIC_UBR;
			op.param.aal = (op.param.flags & ATM_PH_AAL5) ?
			    ATMIO_AAL_5 : ATMIO_AAL_0;
		} else {
			/* new address */
			op.param.aal = ATMIO_AAL_5;

			GET1BYTE(op.param.flags, addr, alen);
			op.param.flags &= ATM_PH_LLCSNAP;

			GET1BYTE(op.param.vpi, addr, alen);
			GET2BYTE(op.param.vci, addr, alen);

			GET1BYTE(op.param.traffic, addr, alen);

			switch (op.param.traffic) {

			  case ATMIO_TRAFFIC_UBR:
				if (alen >= 3)
					GET3BYTE(op.param.tparam.pcr,
					    addr, alen);
				break;

			  case ATMIO_TRAFFIC_CBR:
				if (alen < 3)
					goto bad_param;
				GET3BYTE(op.param.tparam.pcr, addr, alen);
				break;

			  case ATMIO_TRAFFIC_VBR:
				if (alen < 3 * 3)
					goto bad_param;
				GET3BYTE(op.param.tparam.pcr, addr, alen);
				GET3BYTE(op.param.tparam.scr, addr, alen);
				GET3BYTE(op.param.tparam.mbs, addr, alen);
				break;

			  case ATMIO_TRAFFIC_ABR:
				if (alen < 4 * 3 + 2 + 1 * 2 + 3)
					goto bad_param;
				GET3BYTE(op.param.tparam.pcr, addr, alen);
				GET3BYTE(op.param.tparam.mcr, addr, alen);
				GET3BYTE(op.param.tparam.icr, addr, alen);
				GET3BYTE(op.param.tparam.tbe, addr, alen);
				GET1BYTE(op.param.tparam.nrm, addr, alen);
				GET1BYTE(op.param.tparam.trm, addr, alen);
				GET2BYTE(op.param.tparam.adtf, addr, alen);
				GET1BYTE(op.param.tparam.rif, addr, alen);
				GET1BYTE(op.param.tparam.rdf, addr, alen);
				GET1BYTE(op.param.tparam.cdf, addr, alen);
				break;

			  default:
			  bad_param:
				printf("%s: bad traffic params\n", __func__);
				goto failed;
			}
		}
		op.param.rmtu = op.param.tmtu = rt->rt_ifp->if_mtu;
#ifdef NATM
		/*
		 * let native ATM know we are using this VCI/VPI
		 * (i.e. reserve it)
		 */
		sin = (struct sockaddr_in *) rt_key(rt);
		if (sin->sin_family != AF_INET)
			goto failed;
		npcb = npcb_add(NULL, rt->rt_ifp, op.param.vci,  op.param.vpi);
		if (npcb == NULL)
			goto failed;
		npcb->npcb_flags |= NPCB_IP;
		npcb->ipaddr.s_addr = sin->sin_addr.s_addr;
		/* XXX: move npcb to llinfo when ATM ARP is ready */
#ifdef __notyet_restored__
		rt->rt_llinfo = (caddr_t) npcb;
#endif
		rt->rt_flags |= RTF_LLINFO;
#endif
		/*
		 * let the lower level know this circuit is active
		 */
		op.rxhand = NULL;
		op.param.flags |= ATMIO_FLAG_ASYNC;
		if (rt->rt_ifp->if_ioctl(rt->rt_ifp, SIOCATMOPENVCC,
		    (caddr_t)&op) != 0) {
			printf("atm: couldn't add VC\n");
			goto failed;
		}

		SDL(gate)->sdl_type = rt->rt_ifp->if_type;
		SDL(gate)->sdl_index = rt->rt_ifp->if_index;

#ifdef NATM
		NATM_UNLOCK();
#endif
		break;

failed:
#ifdef NATM
		if (npcb) {
			npcb_free(npcb, NPCB_DESTROY);
#ifdef __notyet_restored__
			rt->rt_llinfo = NULL;
#endif
			rt->rt_flags &= ~RTF_LLINFO;
		}
		NATM_UNLOCK();
#endif
		/* mark as invalid. We cannot RTM_DELETE the route from
		 * here, because the recursive call to rtrequest1 does
		 * not really work. */
		rt->rt_flags |= RTF_REJECT;
		break;

	case RTM_DELETE:
#ifdef NATM
		/*
		 * tell native ATM we are done with this VC
		 */
		if (rt->rt_flags & RTF_LLINFO) {
			NATM_LOCK();
#ifdef __notyet_restored__
			npcb_free((struct natmpcb *)rt->rt_llinfo,
			    NPCB_DESTROY);
			rt->rt_llinfo = NULL;
#endif
			rt->rt_flags &= ~RTF_LLINFO;
			NATM_UNLOCK();
		}
#endif
		/*
		 * tell the lower layer to disable this circuit
		 */
		bzero(&op, sizeof(op));
		addr = LLADDR(SDL(gate));
		addr++;
		cl.vpi = *addr++;
		cl.vci = *addr++ << 8;
		cl.vci |= *addr++;
		(void)rt->rt_ifp->if_ioctl(rt->rt_ifp, SIOCATMCLOSEVCC,
		    (caddr_t)&cl);
		break;
	}
}

/*
 * atmresolve:
 *   inputs:
 *     [1] "rt" = the link level route to use (or null if need to look one up)
 *     [2] "m" = mbuf containing the data to be sent
 *     [3] "dst" = sockaddr_in (IP) address of dest.
 *   output:
 *     [4] "desten" = ATM pseudo header which we will fill in VPI/VCI info
 *   return:
 *     0 == resolve FAILED; note that "m" gets m_freem'd in this case
 *     1 == resolve OK; desten contains result
 *
 *   XXX: will need more work if we wish to support ATMARP in the kernel,
 *   but this is enough for PVCs entered via the "route" command.
 */
int
atmresolve(struct rtentry *rt, struct mbuf *m, const struct sockaddr *dst,
    struct atm_pseudohdr *desten)
{
	struct sockaddr_dl *sdl;

	if (m->m_flags & (M_BCAST | M_MCAST)) {
		log(LOG_INFO,
		    "atmresolve: BCAST/MCAST packet detected/dumped\n");
		goto bad;
	}

	if (rt == NULL) {
		/* link level on table 0 XXX MRT */
		rt = RTALLOC1(__DECONST(struct sockaddr *, dst), 0);
		if (rt == NULL)
			goto bad;	/* failed */
		RT_REMREF(rt);		/* don't keep LL references */
		if ((rt->rt_flags & RTF_GATEWAY) != 0 ||
		    rt->rt_gateway->sa_family != AF_LINK) {
			RT_UNLOCK(rt);
			goto bad;
		}
		RT_UNLOCK(rt);
	}

	/*
	 * note that rt_gateway is a sockaddr_dl which contains the
	 * atm_pseudohdr data structure for this route.   we currently
	 * don't need any rt_llinfo info (but will if we want to support
	 * ATM ARP [c.f. if_ether.c]).
	 */
	sdl = SDL(rt->rt_gateway);

	/*
	 * Check the address family and length is valid, the address
	 * is resolved; otherwise, try to resolve.
	 */
	if (sdl->sdl_family == AF_LINK && sdl->sdl_alen >= sizeof(*desten)) {
		bcopy(LLADDR(sdl), desten, sizeof(*desten));
		return (1);	/* ok, go for it! */
	}

	/*
	 * we got an entry, but it doesn't have valid link address
	 * info in it (it is prob. the interface route, which has
	 * sdl_alen == 0).    dump packet.  (fall through to "bad").
	 */
bad:
	m_freem(m);
	return (0);
}
#endif /* INET */
