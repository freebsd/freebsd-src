/*      $NetBSD: if_atmsubr.c,v 1.10 1997/03/11 23:19:51 chuck Exp $       */

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
 *	Washington University.
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
 *
 * if_atmsubr.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_natm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_atm.h>

#include <netinet/in.h>
#include <netinet/if_atm.h>
#include <netinet/if_ether.h> /* XXX: for ETHERTYPE_* */
#if defined(INET) || defined(INET6)
#include <netinet/in_var.h>
#endif
#ifdef NATM
#include <netnatm/natm.h>
#endif

#include <security/mac/mac_framework.h>

/*
 * Netgraph interface functions.
 * These need not be protected by a lock, because ng_atm nodes are persitent.
 * The ng_atm module can be unloaded only if all ATM interfaces have been
 * unloaded, so nobody should be in the code paths accessing these function
 * pointers.
 */
void	(*ng_atm_attach_p)(struct ifnet *);
void	(*ng_atm_detach_p)(struct ifnet *);
int	(*ng_atm_output_p)(struct ifnet *, struct mbuf **);
void	(*ng_atm_input_p)(struct ifnet *, struct mbuf **,
	    struct atm_pseudohdr *, void *);
void	(*ng_atm_input_orphan_p)(struct ifnet *, struct mbuf *,
	    struct atm_pseudohdr *, void *);
void	(*ng_atm_event_p)(struct ifnet *, uint32_t, void *);

/*
 * Harp pseudo interface hooks
 */
void	(*atm_harp_input_p)(struct ifnet *ifp, struct mbuf **m,
	    struct atm_pseudohdr *ah, void *rxhand);
void	(*atm_harp_attach_p)(struct ifnet *);
void	(*atm_harp_detach_p)(struct ifnet *);
void	(*atm_harp_event_p)(struct ifnet *, uint32_t, void *);

SYSCTL_NODE(_hw, OID_AUTO, atm, CTLFLAG_RW, 0, "ATM hardware");

static MALLOC_DEFINE(M_IFATM, "ifatm", "atm interface internals");

#ifndef ETHERTYPE_IPV6
#define	ETHERTYPE_IPV6	0x86dd
#endif

#define	senderr(e) do { error = (e); goto bad; } while (0)

/*
 * atm_output: ATM output routine
 *   inputs:
 *     "ifp" = ATM interface to output to
 *     "m0" = the packet to output
 *     "dst" = the sockaddr to send to (either IP addr, or raw VPI/VCI)
 *     "ro" = the route to use
 *   returns: error code   [0 == ok]
 *
 *   note: special semantic: if (dst == NULL) then we assume "m" already
 *		has an atm_pseudohdr on it and just send it directly.
 *		[for native mode ATM output]   if dst is null, then
 *		ro->ro_rt must also be NULL.
 */
int
atm_output(struct ifnet *ifp, struct mbuf *m0, const struct sockaddr *dst,
    struct route *ro)
{
	u_int16_t etype = 0;			/* if using LLC/SNAP */
	int error = 0, sz;
	struct atm_pseudohdr atmdst, *ad;
	struct mbuf *m = m0;
	struct atmllc *atmllc;
	const struct atmllc *llc_hdr = NULL;
	u_int32_t atm_flags;

#ifdef MAC
	error = mac_ifnet_check_transmit(ifp, m);
	if (error)
		senderr(error);
#endif

	if (!((ifp->if_flags & IFF_UP) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING)))
		senderr(ENETDOWN);

	/*
	 * check for non-native ATM traffic   (dst != NULL)
	 */
	if (dst) {
		switch (dst->sa_family) {

#if defined(INET) || defined(INET6)
		case AF_INET:
		case AF_INET6:
		{
			if (dst->sa_family == AF_INET6)
			        etype = ETHERTYPE_IPV6;
			else
			        etype = ETHERTYPE_IP;
			if (!atmresolve(ro->ro_rt, m, dst, &atmdst)) {
				m = NULL; 
				/* XXX: atmresolve already free'd it */
				senderr(EHOSTUNREACH);
				/* XXX: put ATMARP stuff here */
				/* XXX: watch who frees m on failure */
			}
		}
			break;
#endif /* INET || INET6 */

		case AF_UNSPEC:
			/*
			 * XXX: bpfwrite. assuming dst contains 12 bytes
			 * (atm pseudo header (4) + LLC/SNAP (8))
			 */
			bcopy(dst->sa_data, &atmdst, sizeof(atmdst));
			llc_hdr = (const struct atmllc *)(dst->sa_data +
			    sizeof(atmdst));
			break;
			
		default:
			printf("%s: can't handle af%d\n", ifp->if_xname, 
			    dst->sa_family);
			senderr(EAFNOSUPPORT);
		}

		/*
		 * must add atm_pseudohdr to data
		 */
		sz = sizeof(atmdst);
		atm_flags = ATM_PH_FLAGS(&atmdst);
		if (atm_flags & ATM_PH_LLCSNAP)
			sz += 8;	/* sizeof snap == 8 */
		M_PREPEND(m, sz, M_NOWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		ad = mtod(m, struct atm_pseudohdr *);
		*ad = atmdst;
		if (atm_flags & ATM_PH_LLCSNAP) {
			atmllc = (struct atmllc *)(ad + 1);
			if (llc_hdr == NULL) {
			        bcopy(ATMLLC_HDR, atmllc->llchdr, 
				      sizeof(atmllc->llchdr));
				/* note: in host order */
				ATM_LLC_SETTYPE(atmllc, etype); 
			}
			else
			        bcopy(llc_hdr, atmllc, sizeof(struct atmllc));
		}
	}

	if (ng_atm_output_p != NULL) {
		if ((error = (*ng_atm_output_p)(ifp, &m)) != 0) {
			if (m != NULL)
				m_freem(m);
			return (error);
		}
		if (m == NULL)
			return (0);
	}

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	if (!IF_HANDOFF_ADJ(&ifp->if_snd, m, ifp,
	    -(int)sizeof(struct atm_pseudohdr)))
		return (ENOBUFS);
	return (error);

bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Process a received ATM packet;
 * the packet is in the mbuf chain m.
 */
void
atm_input(struct ifnet *ifp, struct atm_pseudohdr *ah, struct mbuf *m,
    void *rxhand)
{
	int isr;
	u_int16_t etype = ETHERTYPE_IP;		/* default */

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
#ifdef MAC
	mac_ifnet_create_mbuf(ifp, m);
#endif
	if_inc_counter(ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);

	if (ng_atm_input_p != NULL) {
		(*ng_atm_input_p)(ifp, &m, ah, rxhand);
		if (m == NULL)
			return;
	}

	/* not eaten by ng_atm. Maybe it's a pseudo-harp PDU? */
	if (atm_harp_input_p != NULL) {
		(*atm_harp_input_p)(ifp, &m, ah, rxhand);
		if (m == NULL)
			return;
	}

	if (rxhand) {
#ifdef NATM
		struct natmpcb *npcb;

		/*
		 * XXXRW: this use of 'rxhand' is not a very good idea, and
		 * was subject to races even before SMPng due to the release
		 * of spl here.
		 */
		NATM_LOCK();
		npcb = rxhand;
		npcb->npcb_inq++;	/* count # in queue */
		isr = NETISR_NATM;
		m->m_pkthdr.rcvif = rxhand; /* XXX: overload */
		NATM_UNLOCK();
#else
		printf("atm_input: NATM detected but not "
		    "configured in kernel\n");
		goto dropit;
#endif
	} else {
		/*
		 * handle LLC/SNAP header, if present
		 */
		if (ATM_PH_FLAGS(ah) & ATM_PH_LLCSNAP) {
			struct atmllc *alc;

			if (m->m_len < sizeof(*alc) &&
			    (m = m_pullup(m, sizeof(*alc))) == 0)
				return; /* failed */
			alc = mtod(m, struct atmllc *);
			if (bcmp(alc, ATMLLC_HDR, 6)) {
				printf("%s: recv'd invalid LLC/SNAP frame "
				    "[vp=%d,vc=%d]\n", ifp->if_xname,
				    ATM_PH_VPI(ah), ATM_PH_VCI(ah));
				m_freem(m);
				return;
			}
			etype = ATM_LLC_TYPE(alc);
			m_adj(m, sizeof(*alc));
		}

		switch (etype) {

#ifdef INET
		case ETHERTYPE_IP:
			isr = NETISR_IP;
			break;
#endif

#ifdef INET6
		case ETHERTYPE_IPV6:
			isr = NETISR_IPV6;
			break;
#endif
		default:
#ifndef NATM
  dropit:
#endif
			if (ng_atm_input_orphan_p != NULL)
				(*ng_atm_input_orphan_p)(ifp, m, ah, rxhand);
			else
				m_freem(m);
			return;
		}
	}
	M_SETFIB(m, ifp->if_fib);
	netisr_dispatch(isr, m);
}

/*
 * Perform common duties while attaching to interface list.
 */
void
atm_ifattach(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	struct ifatm *ifatm = ifp->if_l2com;

	ifp->if_addrlen = 0;
	ifp->if_hdrlen = 0;
	if_attach(ifp);
	ifp->if_mtu = ATMMTU;
	ifp->if_output = atm_output;
#if 0
	ifp->if_input = atm_input;
#endif
	ifp->if_snd.ifq_maxlen = 50;	/* dummy */

	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
		if (ifa->ifa_addr->sa_family == AF_LINK) {
			sdl = (struct sockaddr_dl *)ifa->ifa_addr;
			sdl->sdl_type = IFT_ATM;
			sdl->sdl_alen = ifp->if_addrlen;
#ifdef notyet /* if using ATMARP, store hardware address using the next line */
			bcopy(ifp->hw_addr, LLADDR(sdl), ifp->if_addrlen);
#endif
			break;
		}

	ifp->if_linkmib = &ifatm->mib;
	ifp->if_linkmiblen = sizeof(ifatm->mib);

	if(ng_atm_attach_p)
		(*ng_atm_attach_p)(ifp);
	if (atm_harp_attach_p)
		(*atm_harp_attach_p)(ifp);
}

/*
 * Common stuff for detaching an ATM interface
 */
void
atm_ifdetach(struct ifnet *ifp)
{
	if (atm_harp_detach_p)
		(*atm_harp_detach_p)(ifp);
	if(ng_atm_detach_p)
		(*ng_atm_detach_p)(ifp);
	if_detach(ifp);
}

/*
 * Support routine for the SIOCATMGVCCS ioctl().
 *
 * This routine assumes, that the private VCC structures used by the driver
 * begin with a struct atmio_vcc.
 *
 * Return a table of VCCs in a freshly allocated memory area.
 * Here we have a problem: we first count, how many vccs we need
 * to return. The we allocate the memory and finally fill it in.
 * Because we cannot lock while calling malloc, the number of active
 * vccs may change while we're in malloc. So we allocate a couple of
 * vccs more and if space anyway is not enough re-iterate.
 *
 * We could use an sx lock for the vcc tables.
 */
struct atmio_vcctable *
atm_getvccs(struct atmio_vcc **table, u_int size, u_int start,
    struct mtx *lock, int waitok)
{
	u_int cid, alloc;
	size_t len;
	struct atmio_vcctable *vccs;
	struct atmio_vcc *v;

	alloc = start + 10;
	vccs = NULL;

	for (;;) {
		len = sizeof(*vccs) + alloc * sizeof(vccs->vccs[0]);
		vccs = reallocf(vccs, len, M_TEMP,
		    waitok ? M_WAITOK : M_NOWAIT);
		if (vccs == NULL)
			return (NULL);
		bzero(vccs, len);

		vccs->count = 0;
		v = vccs->vccs;

		mtx_lock(lock);
		for (cid = 0; cid < size; cid++)
			if (table[cid] != NULL) {
				if (++vccs->count == alloc)
					/* too many - try again */
					break;
				*v++ = *table[cid];
			}
		mtx_unlock(lock);

		if (cid == size)
			break;

		alloc *= 2;
	}
	return (vccs);
}

/*
 * Driver or channel state has changed. Inform whoever is interested
 * in these events.
 */
void
atm_event(struct ifnet *ifp, u_int event, void *arg)
{
	if (ng_atm_event_p != NULL)
		(*ng_atm_event_p)(ifp, event, arg);
	if (atm_harp_event_p != NULL)
		(*atm_harp_event_p)(ifp, event, arg);
}

static void *
atm_alloc(u_char type, struct ifnet *ifp)
{
	struct ifatm	*ifatm;

	ifatm = malloc(sizeof(struct ifatm), M_IFATM, M_WAITOK | M_ZERO);
	ifatm->ifp = ifp;

	return (ifatm);
}

static void
atm_free(void *com, u_char type)
{

	free(com, M_IFATM);
}

static int
atm_modevent(module_t mod, int type, void *data)
{
	switch (type) {
	case MOD_LOAD:
		if_register_com_alloc(IFT_ATM, atm_alloc, atm_free);
		break;
	case MOD_UNLOAD:
		if_deregister_com_alloc(IFT_ATM);
		break;
	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

static moduledata_t atm_mod = {
        "atm",
        atm_modevent,
        0
};
                
DECLARE_MODULE(atm, atm_mod, SI_SUB_INIT_IF, SI_ORDER_ANY);
MODULE_VERSION(atm, 1);
