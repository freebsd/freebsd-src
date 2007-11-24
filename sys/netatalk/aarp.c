/*-
 * Copyright (c) 2004-2005 Robert N. M. Watson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1990,1991,1994 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
 *
 * $FreeBSD$
 */

#include "opt_atalk.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mac.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>

#include <netinet/in.h>
#undef s_net
#include <netinet/if_ether.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/aarp.h>
#include <netatalk/phase2.h>
#include <netatalk/at_extern.h>

static void aarptfree(struct aarptab *aat);
static void at_aarpinput(struct ifnet *ifp, struct mbuf *m);

#define AARPTAB_BSIZ	9
#define AARPTAB_NB	19
#define AARPTAB_SIZE	(AARPTAB_BSIZ * AARPTAB_NB)
static struct aarptab	aarptab[AARPTAB_SIZE];

struct mtx aarptab_mtx;
MTX_SYSINIT(aarptab_mtx, &aarptab_mtx, "aarptab_mtx", MTX_DEF);

#define AARPTAB_HASH(a) \
    ((((a).s_net << 8) + (a).s_node) % AARPTAB_NB)

#define AARPTAB_LOOK(aat, addr) { \
    int		n; \
    AARPTAB_LOCK_ASSERT(); \
    aat = &aarptab[ AARPTAB_HASH(addr) * AARPTAB_BSIZ ]; \
    for (n = 0; n < AARPTAB_BSIZ; n++, aat++) \
	if (aat->aat_ataddr.s_net == (addr).s_net && \
	     aat->aat_ataddr.s_node == (addr).s_node) \
	    break; \
	if (n >= AARPTAB_BSIZ) \
	    aat = NULL; \
}

#define AARPT_AGE	(60 * 1)
#define AARPT_KILLC	20
#define AARPT_KILLI	3

# if !defined(__FreeBSD__)
extern u_char			etherbroadcastaddr[6];
# endif /* __FreeBSD__ */

static const u_char atmulticastaddr[ 6 ] = {
    0x09, 0x00, 0x07, 0xff, 0xff, 0xff,
};

u_char	at_org_code[ 3 ] = {
    0x08, 0x00, 0x07,
};
const u_char	aarp_org_code[ 3 ] = {
    0x00, 0x00, 0x00,
};

static struct callout_handle aarptimer_ch =
    CALLOUT_HANDLE_INITIALIZER(&aarptimer_ch);

static void
aarptimer(void *ignored)
{
    struct aarptab	*aat;
    int			i;

    aarptimer_ch = timeout(aarptimer, (caddr_t)0, AARPT_AGE * hz);
    aat = aarptab;
    AARPTAB_LOCK();
    for (i = 0; i < AARPTAB_SIZE; i++, aat++) {
	if (aat->aat_flags == 0 || (aat->aat_flags & ATF_PERM))
	    continue;
	if (++aat->aat_timer < ((aat->aat_flags & ATF_COM) ?
		AARPT_KILLC : AARPT_KILLI))
	    continue;
	aarptfree(aat);
    }
    AARPTAB_UNLOCK();
}

/* 
 * search through the network addresses to find one that includes
 * the given network.. remember to take netranges into
 * consideration.
 */
struct at_ifaddr *
at_ifawithnet(struct sockaddr_at  *sat)
{
    struct at_ifaddr	*aa;
    struct sockaddr_at	*sat2;

	for (aa = at_ifaddr_list; aa != NULL; aa = aa->aa_next) {
		sat2 = &(aa->aa_addr);
		if (sat2->sat_addr.s_net == sat->sat_addr.s_net) {
	    		break;
		}
		if((aa->aa_flags & AFA_PHASE2)
	 	&& (ntohs(aa->aa_firstnet) <= ntohs(sat->sat_addr.s_net))
	 	&& (ntohs(aa->aa_lastnet) >= ntohs(sat->sat_addr.s_net))) {
			break;
		}
	}
	return (aa);
}

static void
aarpwhohas(struct ifnet *ifp, struct sockaddr_at *sat)
{
    struct mbuf		*m;
    struct ether_header	*eh;
    struct ether_aarp	*ea;
    struct at_ifaddr	*aa;
    struct llc		*llc;
    struct sockaddr	sa;

    AARPTAB_UNLOCK_ASSERT();
    if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL) {
	return;
    }
#ifdef MAC
    mac_create_mbuf_linklayer(ifp, m);
#endif
    m->m_len = sizeof(*ea);
    m->m_pkthdr.len = sizeof(*ea);
    MH_ALIGN(m, sizeof(*ea));

    ea = mtod(m, struct ether_aarp *);
    bzero((caddr_t)ea, sizeof(*ea));

    ea->aarp_hrd = htons(AARPHRD_ETHER);
    ea->aarp_pro = htons(ETHERTYPE_AT);
    ea->aarp_hln = sizeof(ea->aarp_sha);
    ea->aarp_pln = sizeof(ea->aarp_spu);
    ea->aarp_op = htons(AARPOP_REQUEST);
    bcopy(IFP2ENADDR(ifp), (caddr_t)ea->aarp_sha,
	    sizeof(ea->aarp_sha));

    /*
     * We need to check whether the output ethernet type should
     * be phase 1 or 2. We have the interface that we'll be sending
     * the aarp out. We need to find an AppleTalk network on that
     * interface with the same address as we're looking for. If the
     * net is phase 2, generate an 802.2 and SNAP header.
     */
    if ((aa = at_ifawithnet(sat)) == NULL) {
	m_freem(m);
	return;
    }

    eh = (struct ether_header *)sa.sa_data;

    if (aa->aa_flags & AFA_PHASE2) {
	bcopy(atmulticastaddr, eh->ether_dhost, sizeof(eh->ether_dhost));
	eh->ether_type = htons(sizeof(struct llc) + sizeof(struct ether_aarp));
	M_PREPEND(m, sizeof(struct llc), M_DONTWAIT);
	if (m == NULL) {
	    return;
	}
	llc = mtod(m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	bcopy(aarp_org_code, llc->llc_org_code, sizeof(aarp_org_code));
	llc->llc_ether_type = htons(ETHERTYPE_AARP);

	bcopy(&AA_SAT(aa)->sat_addr.s_net, ea->aarp_spnet,
	       sizeof(ea->aarp_spnet));
	bcopy(&sat->sat_addr.s_net, ea->aarp_tpnet,
	       sizeof(ea->aarp_tpnet));
	ea->aarp_spnode = AA_SAT(aa)->sat_addr.s_node;
	ea->aarp_tpnode = sat->sat_addr.s_node;
    } else {
	bcopy(ifp->if_broadcastaddr, (caddr_t)eh->ether_dhost,
		sizeof(eh->ether_dhost));
	eh->ether_type = htons(ETHERTYPE_AARP);

	ea->aarp_spa = AA_SAT(aa)->sat_addr.s_node;
	ea->aarp_tpa = sat->sat_addr.s_node;
    }

#ifdef NETATALKDEBUG
    printf("aarp: sending request for %u.%u\n",
	   ntohs(AA_SAT(aa)->sat_addr.s_net),
	   AA_SAT(aa)->sat_addr.s_node);
#endif /* NETATALKDEBUG */

    sa.sa_len = sizeof(struct sockaddr);
    sa.sa_family = AF_UNSPEC;
    ifp->if_output(ifp, m, &sa, NULL /* route */);
}

int
aarpresolve(ifp, m, destsat, desten)
    struct ifnet	*ifp;
    struct mbuf		*m;
    struct sockaddr_at	*destsat;
    u_char		*desten;
{
    struct at_ifaddr	*aa;
    struct aarptab	*aat;

    if (at_broadcast(destsat)) {
	m->m_flags |= M_BCAST;
	if ((aa = at_ifawithnet(destsat)) == NULL)  {
	    m_freem(m);
	    return (0);
	}
	if (aa->aa_flags & AFA_PHASE2) {
	    bcopy(atmulticastaddr, (caddr_t)desten, sizeof(atmulticastaddr));
	} else {
	    bcopy(ifp->if_broadcastaddr, (caddr_t)desten,
		    sizeof(ifp->if_addrlen));
	}
	return (1);
    }

    AARPTAB_LOCK();
    AARPTAB_LOOK(aat, destsat->sat_addr);
    if (aat == NULL) {			/* No entry */
	aat = aarptnew(&destsat->sat_addr);
	if (aat == NULL) { /* we should fail more gracefully! */
	    panic("aarpresolve: no free entry");
	}
	goto done;
    }
    /* found an entry */
    aat->aat_timer = 0;
    if (aat->aat_flags & ATF_COM) {	/* entry is COMplete */
	bcopy((caddr_t)aat->aat_enaddr, (caddr_t)desten,
		sizeof(aat->aat_enaddr));
	AARPTAB_UNLOCK();
	return (1);
    }
    /* entry has not completed */
    if (aat->aat_hold) {
	m_freem(aat->aat_hold);
    }
done:
    aat->aat_hold = m;
    AARPTAB_UNLOCK();
    aarpwhohas(ifp, destsat);
    return (0);
}

void
aarpintr(m)
    struct mbuf		*m;
{
    struct arphdr	*ar;
    struct ifnet	*ifp;

    ifp = m->m_pkthdr.rcvif;
    if (ifp->if_flags & IFF_NOARP)
	goto out;

    if (m->m_len < sizeof(struct arphdr)) {
	goto out;
    }

    ar = mtod(m, struct arphdr *);
    if (ntohs(ar->ar_hrd) != AARPHRD_ETHER) {
	goto out;
    }
    
    if (m->m_len < sizeof(struct arphdr) + 2 * ar->ar_hln +
	    2 * ar->ar_pln) {
	goto out;
    }
    
    switch(ntohs(ar->ar_pro)) {
    case ETHERTYPE_AT :
	at_aarpinput(ifp, m);
	return;

    default:
	break;
    }

out:
    m_freem(m);
}

static void
at_aarpinput(struct ifnet *ifp, struct mbuf *m)
{
    struct ether_aarp	*ea;
    struct at_ifaddr	*aa;
    struct aarptab	*aat;
    struct ether_header	*eh;
    struct llc		*llc;
    struct sockaddr_at	sat;
    struct sockaddr	sa;
    struct at_addr	spa, tpa, ma;
    int			op;
    u_short		net;

    ea = mtod(m, struct ether_aarp *);

    /* Check to see if from my hardware address */
    if (!bcmp((caddr_t)ea->aarp_sha, IFP2ENADDR(ifp),
	    sizeof(IFP2ENADDR(ifp)))) {
	m_freem(m);
	return;
    }

    op = ntohs(ea->aarp_op);
    bcopy(ea->aarp_tpnet, &net, sizeof(net));

    if (net != 0) { /* should be ATADDR_ANYNET? */
	sat.sat_len = sizeof(struct sockaddr_at);
	sat.sat_family = AF_APPLETALK;
	sat.sat_addr.s_net = net;
	if ((aa = at_ifawithnet(&sat)) == NULL) {
	    m_freem(m);
	    return;
	}
	bcopy(ea->aarp_spnet, &spa.s_net, sizeof(spa.s_net));
	bcopy(ea->aarp_tpnet, &tpa.s_net, sizeof(tpa.s_net));
    } else {
	/*
	 * Since we don't know the net, we just look for the first
	 * phase 1 address on the interface.
	 */
	for (aa = (struct at_ifaddr *)TAILQ_FIRST(&ifp->if_addrhead); aa;
		aa = (struct at_ifaddr *)aa->aa_ifa.ifa_link.tqe_next) {
	    if (AA_SAT(aa)->sat_family == AF_APPLETALK &&
		    (aa->aa_flags & AFA_PHASE2) == 0) {
		break;
	    }
	}
	if (aa == NULL) {
	    m_freem(m);
	    return;
	}
	tpa.s_net = spa.s_net = AA_SAT(aa)->sat_addr.s_net;
    }

    spa.s_node = ea->aarp_spnode;
    tpa.s_node = ea->aarp_tpnode;
    ma.s_net = AA_SAT(aa)->sat_addr.s_net;
    ma.s_node = AA_SAT(aa)->sat_addr.s_node;

    /*
     * This looks like it's from us.
     */
    if (spa.s_net == ma.s_net && spa.s_node == ma.s_node) {
	if (aa->aa_flags & AFA_PROBING) {
	    /*
	     * We're probing, someone either responded to our probe, or
	     * probed for the same address we'd like to use. Change the
	     * address we're probing for.
	     */
	    callout_stop(&aa->aa_callout);
	    wakeup(aa);
	    m_freem(m);
	    return;
	} else if (op != AARPOP_PROBE) {
	    /*
	     * This is not a probe, and we're not probing. This means
	     * that someone's saying they have the same source address
	     * as the one we're using. Get upset...
	     */
	    log(LOG_ERR,
		    "aarp: duplicate AT address!! %x:%x:%x:%x:%x:%x\n",
		    ea->aarp_sha[ 0 ], ea->aarp_sha[ 1 ], ea->aarp_sha[ 2 ],
		    ea->aarp_sha[ 3 ], ea->aarp_sha[ 4 ], ea->aarp_sha[ 5 ]);
	    m_freem(m);
	    return;
	}
    }

    AARPTAB_LOCK();
    AARPTAB_LOOK(aat, spa);
    if (aat != NULL) {
	if (op == AARPOP_PROBE) {
	    /*
	     * Someone's probing for spa, dealocate the one we've got,
	     * so that if the prober keeps the address, we'll be able
	     * to arp for him.
	     */
	    aarptfree(aat);
	    AARPTAB_UNLOCK();
	    m_freem(m);
	    return;
	}

	bcopy((caddr_t)ea->aarp_sha, (caddr_t)aat->aat_enaddr,
		sizeof(ea->aarp_sha));
	aat->aat_flags |= ATF_COM;
	if (aat->aat_hold) {
	    struct mbuf *mhold = aat->aat_hold;
	    aat->aat_hold = NULL;
	    AARPTAB_UNLOCK();
	    sat.sat_len = sizeof(struct sockaddr_at);
	    sat.sat_family = AF_APPLETALK;
	    sat.sat_addr = spa;
	    (*ifp->if_output)(ifp, mhold,
		    (struct sockaddr *)&sat, NULL); /* XXX */
	} else
	    AARPTAB_UNLOCK();
    } else if ((tpa.s_net == ma.s_net)
	   && (tpa.s_node == ma.s_node)
	   && (op != AARPOP_PROBE)
	   && ((aat = aarptnew(&spa)) != NULL)) {
		bcopy((caddr_t)ea->aarp_sha, (caddr_t)aat->aat_enaddr,
		    sizeof(ea->aarp_sha));
		aat->aat_flags |= ATF_COM;
	        AARPTAB_UNLOCK();
    } else
	AARPTAB_UNLOCK();

    /*
     * Don't respond to responses, and never respond if we're
     * still probing.
     */
    if (tpa.s_net != ma.s_net || tpa.s_node != ma.s_node ||
	    op == AARPOP_RESPONSE || (aa->aa_flags & AFA_PROBING)) {
	m_freem(m);
	return;
    }

    bcopy((caddr_t)ea->aarp_sha, (caddr_t)ea->aarp_tha,
	    sizeof(ea->aarp_sha));
    bcopy(IFP2ENADDR(ifp), (caddr_t)ea->aarp_sha,
	    sizeof(ea->aarp_sha));

    /* XXX */
    eh = (struct ether_header *)sa.sa_data;
    bcopy((caddr_t)ea->aarp_tha, (caddr_t)eh->ether_dhost,
	    sizeof(eh->ether_dhost));

    if (aa->aa_flags & AFA_PHASE2) {
	eh->ether_type = htons(sizeof(struct llc) +
		sizeof(struct ether_aarp));
	M_PREPEND(m, sizeof(struct llc), M_DONTWAIT);
	if (m == NULL) {
	    return;
	}
	llc = mtod(m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	bcopy(aarp_org_code, llc->llc_org_code, sizeof(aarp_org_code));
	llc->llc_ether_type = htons(ETHERTYPE_AARP);

	bcopy(ea->aarp_spnet, ea->aarp_tpnet, sizeof(ea->aarp_tpnet));
	bcopy(&ma.s_net, ea->aarp_spnet, sizeof(ea->aarp_spnet));
    } else {
	eh->ether_type = htons(ETHERTYPE_AARP);
    }

    ea->aarp_tpnode = ea->aarp_spnode;
    ea->aarp_spnode = ma.s_node;
    ea->aarp_op = htons(AARPOP_RESPONSE);

    sa.sa_len = sizeof(struct sockaddr);
    sa.sa_family = AF_UNSPEC;
    (*ifp->if_output)(ifp, m, &sa, NULL); /* XXX */
    return;
}

static void
aarptfree(struct aarptab *aat)
{

    AARPTAB_LOCK_ASSERT();
    if (aat->aat_hold)
	m_freem(aat->aat_hold);
    aat->aat_hold = NULL;
    aat->aat_timer = aat->aat_flags = 0;
    aat->aat_ataddr.s_net = 0;
    aat->aat_ataddr.s_node = 0;
}

struct aarptab *
aarptnew(addr)
    struct at_addr	*addr;
{
    int			n;
    int			oldest = -1;
    struct aarptab	*aat, *aato = NULL;
    static int		first = 1;

    AARPTAB_LOCK_ASSERT();
    if (first) {
	first = 0;
	aarptimer_ch = timeout(aarptimer, (caddr_t)0, hz);
    }
    aat = &aarptab[ AARPTAB_HASH(*addr) * AARPTAB_BSIZ ];
    for (n = 0; n < AARPTAB_BSIZ; n++, aat++) {
	if (aat->aat_flags == 0)
	    goto out;
	if (aat->aat_flags & ATF_PERM)
	    continue;
	if ((int) aat->aat_timer > oldest) {
	    oldest = aat->aat_timer;
	    aato = aat;
	}
    }
    if (aato == NULL)
	return (NULL);
    aat = aato;
    aarptfree(aat);
out:
    aat->aat_ataddr = *addr;
    aat->aat_flags = ATF_INUSE;
    return (aat);
}


void
aarpprobe(void *arg)
{
    struct ifnet	*ifp = arg;
    struct mbuf		*m;
    struct ether_header	*eh;
    struct ether_aarp	*ea;
    struct at_ifaddr	*aa;
    struct llc		*llc;
    struct sockaddr	sa;

    /*
     * We need to check whether the output ethernet type should
     * be phase 1 or 2. We have the interface that we'll be sending
     * the aarp out. We need to find an AppleTalk network on that
     * interface with the same address as we're looking for. If the
     * net is phase 2, generate an 802.2 and SNAP header.
     */
    AARPTAB_LOCK();
    for (aa = (struct at_ifaddr *)TAILQ_FIRST(&ifp->if_addrhead); aa;
	    aa = (struct at_ifaddr *)aa->aa_ifa.ifa_link.tqe_next) {
	if (AA_SAT(aa)->sat_family == AF_APPLETALK &&
		(aa->aa_flags & AFA_PROBING)) {
	    break;
	}
    }
    if (aa == NULL) {		/* serious error XXX */
	AARPTAB_UNLOCK();
	printf("aarpprobe why did this happen?!\n");
	return;
    }

    if (aa->aa_probcnt <= 0) {
	aa->aa_flags &= ~AFA_PROBING;
	wakeup(aa);
	AARPTAB_UNLOCK();
	return;
    } else {
	callout_reset(&aa->aa_callout, hz / 5, aarpprobe, ifp);
    }
    AARPTAB_UNLOCK();

    if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL) {
	return;
    }
#ifdef MAC
    mac_create_mbuf_linklayer(ifp, m);
#endif
    m->m_len = sizeof(*ea);
    m->m_pkthdr.len = sizeof(*ea);
    MH_ALIGN(m, sizeof(*ea));

    ea = mtod(m, struct ether_aarp *);
    bzero((caddr_t)ea, sizeof(*ea));

    ea->aarp_hrd = htons(AARPHRD_ETHER);
    ea->aarp_pro = htons(ETHERTYPE_AT);
    ea->aarp_hln = sizeof(ea->aarp_sha);
    ea->aarp_pln = sizeof(ea->aarp_spu);
    ea->aarp_op = htons(AARPOP_PROBE);
    bcopy(IFP2ENADDR(ifp), (caddr_t)ea->aarp_sha,
	    sizeof(ea->aarp_sha));

    eh = (struct ether_header *)sa.sa_data;

    if (aa->aa_flags & AFA_PHASE2) {
	bcopy(atmulticastaddr, eh->ether_dhost, sizeof(eh->ether_dhost));
	eh->ether_type = htons(sizeof(struct llc) +
		sizeof(struct ether_aarp));
	M_PREPEND(m, sizeof(struct llc), M_TRYWAIT);
	if (m == NULL) {
	    return;
	}
	llc = mtod(m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	bcopy(aarp_org_code, llc->llc_org_code, sizeof(aarp_org_code));
	llc->llc_ether_type = htons(ETHERTYPE_AARP);

	bcopy(&AA_SAT(aa)->sat_addr.s_net, ea->aarp_spnet,
		sizeof(ea->aarp_spnet));
	bcopy(&AA_SAT(aa)->sat_addr.s_net, ea->aarp_tpnet,
		sizeof(ea->aarp_tpnet));
	ea->aarp_spnode = ea->aarp_tpnode = AA_SAT(aa)->sat_addr.s_node;
    } else {
	bcopy(ifp->if_broadcastaddr, (caddr_t)eh->ether_dhost,
		sizeof(eh->ether_dhost));
	eh->ether_type = htons(ETHERTYPE_AARP);
	ea->aarp_spa = ea->aarp_tpa = AA_SAT(aa)->sat_addr.s_node;
    }

#ifdef NETATALKDEBUG
    printf("aarp: sending probe for %u.%u\n",
	   ntohs(AA_SAT(aa)->sat_addr.s_net),
	   AA_SAT(aa)->sat_addr.s_node);
#endif /* NETATALKDEBUG */

    sa.sa_len = sizeof(struct sockaddr);
    sa.sa_family = AF_UNSPEC;
    (*ifp->if_output)(ifp, m, &sa, NULL); /* XXX */
    aa->aa_probcnt--;
}

void
aarp_clean(void)
{
    struct aarptab	*aat;
    int			i;

    untimeout(aarptimer, 0, aarptimer_ch);
    AARPTAB_LOCK();
    for (i = 0, aat = aarptab; i < AARPTAB_SIZE; i++, aat++) {
	if (aat->aat_hold) {
	    m_freem(aat->aat_hold);
	    aat->aat_hold = NULL;
	}
    }
    AARPTAB_UNLOCK();
}
