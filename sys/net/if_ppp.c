/*
 * if_ppp.c - Point-to-Point Protocol (PPP) Asynchronous driver.
 */

/*-
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Drew D. Perkins
 * Carnegie Mellon University
 * 4910 Forbes Ave.
 * Pittsburgh, PA 15213
 * (412) 268-8576
 * ddp@andrew.cmu.edu
 *
 * Based on:
 *	@(#)if_sl.c	7.6.1.2 (Berkeley) 2/15/89
 *
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Serial Line interface
 *
 * Rick Adams
 * Center for Seismic Studies
 * 1300 N 17th Street, Suite 1450
 * Arlington, Virginia 22209
 * (703)276-7900
 * rick@seismo.ARPA
 * seismo!rick
 *
 * Pounded on heavily by Chris Torek (chris@mimsy.umd.edu, umcp-cs!chris).
 * Converted to 4.3BSD Beta by Chris Torek.
 * Other changes made at Berkeley, based in part on code by Kirk Smith.
 *
 * Converted to 4.3BSD+ 386BSD by Brad Parker (brad@cayman.com)
 * Added VJ tcp header compression; more unified ioctls
 *
 * Extensively modified by Paul Mackerras (paulus@cs.anu.edu.au).
 * Cleaned up a lot of the mbuf-related code to fix bugs that
 * caused system crashes and packet corruption.  Changed pppstart
 * so that it doesn't just give up with a collision if the whole
 * packet doesn't fit in the output ring buffer.
 *
 * Added priority queueing for interactive IP packets, following
 * the model of if_sl.c, plus hooks for bpf.
 * Paul Mackerras (paulus@cs.anu.edu.au).
 */

/* $FreeBSD$ */
/* from if_sl.c,v 1.11 84/10/04 12:54:47 rick Exp */
/* from NetBSD: if_ppp.c,v 1.15.2.2 1994/07/28 05:17:58 cgd Exp */

#include "opt_inet.h"
#include "opt_ipx.h"
#include "opt_mac.h"
#include "opt_ppp.h"

#ifdef INET
#define VJC
#endif
#define PPP_COMPRESS

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mac.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/filio.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef VJC
#include <net/slcompress.h>
#endif

#include <net/if_ppp.h>
#include <net/if_pppvar.h>

/* minimise diffs */
#ifndef splsoftnet
#define splsoftnet	splnet
#endif

#ifdef PPP_COMPRESS
#define PACKETPTR	struct mbuf *
#include <net/ppp_comp.h>
#endif

#define PPPNAME		"ppp"
static MALLOC_DEFINE(M_PPP, PPPNAME, "PPP interface");

static struct mtx ppp_softc_list_mtx;
static LIST_HEAD(, ppp_softc) ppp_softc_list;

#define	PPP_LIST_LOCK_INIT()	mtx_init(&ppp_softc_list_mtx,		\
				    "ppp_softc_list_mtx", NULL, MTX_DEF)
#define	PPP_LIST_LOCK_DESTROY()	mtx_destroy(&ppp_softc_list_mtx)
#define	PPP_LIST_LOCK()		mtx_lock(&ppp_softc_list_mtx)
#define	PPP_LIST_UNLOCK()	mtx_unlock(&ppp_softc_list_mtx)

/* XXX layering violation */
extern void	pppasyncattach(void *);
extern void	pppasyncdetach(void);

static int	pppsioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static void	pppintr(void);

static void	ppp_requeue(struct ppp_softc *);
static void	ppp_ccp(struct ppp_softc *, struct mbuf *m, int rcvd);
static void	ppp_ccp_closed(struct ppp_softc *);
static void	ppp_inproc(struct ppp_softc *, struct mbuf *);
static void	pppdumpm(struct mbuf *m0);
static int	ppp_clone_create(struct if_clone *, int);
static void	ppp_clone_destroy(struct ifnet *);

IFC_SIMPLE_DECLARE(ppp, 0);

/*
 * Some useful mbuf macros not in mbuf.h.
 */
#define M_IS_CLUSTER(m)	((m)->m_flags & M_EXT)

#define M_DATASTART(m)	\
	(M_IS_CLUSTER(m) ? (m)->m_ext.ext_buf : \
	    (m)->m_flags & M_PKTHDR ? (m)->m_pktdat : (m)->m_dat)

#define M_DATASIZE(m)	\
	(M_IS_CLUSTER(m) ? (m)->m_ext.ext_size : \
	    (m)->m_flags & M_PKTHDR ? MHLEN: MLEN)

/*
 * We steal two bits in the mbuf m_flags, to mark high-priority packets
 * for output, and received packets following lost/corrupted packets.
 */
#define M_HIGHPRI	0x2000	/* output packet for sc_fastq */
#define M_ERRMARK	0x4000	/* steal a bit in mbuf m_flags */


#ifdef PPP_COMPRESS
/*
 * List of compressors we know about.
 * We leave some space so maybe we can modload compressors.
 */

extern struct compressor ppp_bsd_compress;
extern struct compressor ppp_deflate, ppp_deflate_draft;

static struct compressor *ppp_compressors[8] = {
#if defined(PPP_BSDCOMP)
    &ppp_bsd_compress,
#endif
#if defined(PPP_DEFLATE)
    &ppp_deflate,
    &ppp_deflate_draft,
#endif
    NULL
};
#endif /* PPP_COMPRESS */

static int
ppp_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet		*ifp;
	struct ppp_softc	*sc;

	sc = malloc(sizeof(struct ppp_softc), M_PPP, M_WAITOK | M_ZERO);
	ifp = sc->sc_ifp = if_alloc(IFT_PPP);
	if (ifp == NULL) {
		free(sc, M_PPP);
		return (ENOSPC);
	}

	ifp->if_softc = sc;
	if_initname(ifp, ifc->ifc_name, unit);
	ifp->if_mtu = PPP_MTU;
	ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
	ifp->if_hdrlen = PPP_HDRLEN;
	ifp->if_ioctl = pppsioctl;
	ifp->if_output = pppoutput;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	sc->sc_inq.ifq_maxlen = IFQ_MAXLEN;
	sc->sc_fastq.ifq_maxlen = IFQ_MAXLEN;
	sc->sc_rawq.ifq_maxlen = IFQ_MAXLEN;
	mtx_init(&sc->sc_inq.ifq_mtx, "ppp_inq", NULL, MTX_DEF);
	mtx_init(&sc->sc_fastq.ifq_mtx, "ppp_fastq", NULL, MTX_DEF);
	mtx_init(&sc->sc_rawq.ifq_mtx, "ppp_rawq", NULL, MTX_DEF);
	if_attach(ifp);
	bpfattach(ifp, DLT_PPP, PPP_HDRLEN);

	PPP_LIST_LOCK();
	LIST_INSERT_HEAD(&ppp_softc_list, sc, sc_list);
	PPP_LIST_UNLOCK();

	return (0);
}

static void
ppp_clone_destroy(struct ifnet *ifp)
{
	struct ppp_softc *sc;

	sc = ifp->if_softc;
	PPP_LIST_LOCK();
	LIST_REMOVE(sc, sc_list);
	PPP_LIST_UNLOCK();

	bpfdetach(ifp);
	if_detach(ifp);
	if_free(ifp);
	mtx_destroy(&sc->sc_rawq.ifq_mtx);
	mtx_destroy(&sc->sc_fastq.ifq_mtx);
	mtx_destroy(&sc->sc_inq.ifq_mtx);
	free(sc, M_PPP);
}

static int
ppp_modevent(module_t mod, int type, void *data) 
{

	switch (type) { 
	case MOD_LOAD: 
		PPP_LIST_LOCK_INIT();
		LIST_INIT(&ppp_softc_list);
		if_clone_attach(&ppp_cloner);

		netisr_register(NETISR_PPP, (netisr_t *)pppintr, NULL, 0);
		/*
		 * XXX layering violation - if_ppp can work over any lower
		 * level transport that cares to attach to it.
		 */
		pppasyncattach(NULL);
		break; 
	case MOD_UNLOAD: 
		/* XXX: layering violation */
		pppasyncdetach();

		netisr_unregister(NETISR_PPP);

		if_clone_detach(&ppp_cloner);
		PPP_LIST_LOCK_DESTROY();
		break; 
	default:
		return EOPNOTSUPP;
	} 
	return 0; 
} 

static moduledata_t ppp_mod = { 
	"if_ppp", 
	ppp_modevent, 
	0
}; 

DECLARE_MODULE(if_ppp, ppp_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

/*
 * Allocate a ppp interface unit and initialize it.
 */
struct ppp_softc *
pppalloc(pid)
    pid_t pid;
{
    int i;
    char tmpname[IFNAMSIZ];
    struct ifnet *ifp;
    struct ppp_softc *sc;

    PPP_LIST_LOCK();
    LIST_FOREACH(sc, &ppp_softc_list, sc_list) {
	if (sc->sc_xfer == pid) {
	    sc->sc_xfer = 0;
	    PPP_LIST_UNLOCK();
	    return sc;
	}
    }
    LIST_FOREACH(sc, &ppp_softc_list, sc_list) {
	if (sc->sc_devp == NULL)
	    break;
    }
    PPP_LIST_UNLOCK();
    /* Try to clone an interface if we don't have a free one */
    if (sc == NULL) {
	strcpy(tmpname, PPPNAME);
	if (if_clone_create(tmpname, sizeof(tmpname)) != 0)
	    return NULL;
	ifp = ifunit(tmpname);
	if (ifp == NULL)
	    return NULL;
	sc = ifp->if_softc;
    }
    if (sc == NULL || sc->sc_devp != NULL)
	return NULL;

    sc->sc_flags = 0;
    sc->sc_mru = PPP_MRU;
    sc->sc_relinq = NULL;
    bzero((char *)&sc->sc_stats, sizeof(sc->sc_stats));
#ifdef VJC
    MALLOC(sc->sc_comp, struct slcompress *, sizeof(struct slcompress),
	   M_DEVBUF, M_NOWAIT);
    if (sc->sc_comp)
	sl_compress_init(sc->sc_comp, -1);
#endif
#ifdef PPP_COMPRESS
    sc->sc_xc_state = NULL;
    sc->sc_rc_state = NULL;
#endif /* PPP_COMPRESS */
    for (i = 0; i < NUM_NP; ++i)
	sc->sc_npmode[i] = NPMODE_ERROR;
    sc->sc_npqueue = NULL;
    sc->sc_npqtail = &sc->sc_npqueue;
    sc->sc_last_sent = sc->sc_last_recv = time_uptime;

    return sc;
}

/*
 * Deallocate a ppp unit.  Must be called at splsoftnet or higher.
 */
void
pppdealloc(sc)
    struct ppp_softc *sc;
{
    struct mbuf *m;

    if_down(PPP2IFP(sc));
    PPP2IFP(sc)->if_flags &= ~IFF_UP;
    PPP2IFP(sc)->if_drv_flags &= ~IFF_DRV_RUNNING;
    getmicrotime(&PPP2IFP(sc)->if_lastchange);
    sc->sc_devp = NULL;
    sc->sc_xfer = 0;
    IF_DRAIN(&sc->sc_rawq);
    IF_DRAIN(&sc->sc_inq);
    IF_DRAIN(&sc->sc_fastq);
    while ((m = sc->sc_npqueue) != NULL) {
	sc->sc_npqueue = m->m_nextpkt;
	m_freem(m);
    }
#ifdef PPP_COMPRESS
    ppp_ccp_closed(sc);
    sc->sc_xc_state = NULL;
    sc->sc_rc_state = NULL;
#endif /* PPP_COMPRESS */
#ifdef PPP_FILTER
    if (sc->sc_pass_filt.bf_insns != 0) {
	free(sc->sc_pass_filt.bf_insns, M_DEVBUF);
	sc->sc_pass_filt.bf_insns = 0;
	sc->sc_pass_filt.bf_len = 0;
    }
    if (sc->sc_active_filt.bf_insns != 0) {
	free(sc->sc_active_filt.bf_insns, M_DEVBUF);
	sc->sc_active_filt.bf_insns = 0;
	sc->sc_active_filt.bf_len = 0;
    }
#endif /* PPP_FILTER */
#ifdef VJC
    if (sc->sc_comp != 0) {
	free(sc->sc_comp, M_DEVBUF);
	sc->sc_comp = 0;
    }
#endif
}

/*
 * Ioctl routine for generic ppp devices.
 */
int
pppioctl(sc, cmd, data, flag, td)
    struct ppp_softc *sc;
    u_long cmd;
    caddr_t data;
    int flag;
    struct thread *td;
{
    struct proc *p = td->td_proc;
    int s, flags, mru, npx;
    u_int nb;
    int error = 0;
    struct ppp_option_data *odp;
    struct compressor **cp;
    struct npioctl *npi;
    time_t t;
#ifdef PPP_FILTER
    struct bpf_program *bp, *nbp;
    struct bpf_insn *newcode, *oldcode;
    int newcodelen;
#endif /* PPP_FILTER */
#ifdef	PPP_COMPRESS
    u_char ccp_option[CCP_MAX_OPTION_LENGTH];
#endif

    switch (cmd) {
    case FIONREAD:
	*(int *)data = sc->sc_inq.ifq_len;
	break;

    case PPPIOCGUNIT:
	*(int *)data = PPP2IFP(sc)->if_dunit;
	break;

    case PPPIOCGFLAGS:
	*(u_int *)data = sc->sc_flags;
	break;

    case PPPIOCSFLAGS:
	if ((error = suser(td)) != 0)
	    break;
	flags = *(int *)data & SC_MASK;
	s = splsoftnet();
#ifdef PPP_COMPRESS
	if (sc->sc_flags & SC_CCP_OPEN && !(flags & SC_CCP_OPEN))
	    ppp_ccp_closed(sc);
#endif
	splimp();
	sc->sc_flags = (sc->sc_flags & ~SC_MASK) | flags;
	splx(s);
	break;

    case PPPIOCSMRU:
	if ((error = suser(td)) != 0)
	    return (error);
	mru = *(int *)data;
	if (mru >= PPP_MRU && mru <= PPP_MAXMRU)
	    sc->sc_mru = mru;
	break;

    case PPPIOCGMRU:
	*(int *)data = sc->sc_mru;
	break;

#ifdef VJC
    case PPPIOCSMAXCID:
	if ((error = suser(td)) != 0)
	    break;
	if (sc->sc_comp) {
	    s = splsoftnet();
	    sl_compress_init(sc->sc_comp, *(int *)data);
	    splx(s);
	}
	break;
#endif

    case PPPIOCXFERUNIT:
	if ((error = suser(td)) != 0)
	    break;
	sc->sc_xfer = p->p_pid;
	break;

#ifdef PPP_COMPRESS
    case PPPIOCSCOMPRESS:
	if ((error = suser(td)) != 0)
	    break;
	odp = (struct ppp_option_data *) data;
	nb = odp->length;
	if (nb > sizeof(ccp_option))
	    nb = sizeof(ccp_option);
	if ((error = copyin(odp->ptr, ccp_option, nb)) != 0)
	    break;
	if (ccp_option[1] < 2) {  /* preliminary check on the length byte */
	    error = EINVAL;
	    break;
	}
	for (cp = ppp_compressors; *cp != NULL; ++cp)
	    if ((*cp)->compress_proto == ccp_option[0]) {
		/*
		 * Found a handler for the protocol - try to allocate
		 * a compressor or decompressor.
		 */
		error = 0;
		if (odp->transmit) {
		    s = splsoftnet();
		    if (sc->sc_xc_state != NULL)
			(*sc->sc_xcomp->comp_free)(sc->sc_xc_state);
		    sc->sc_xcomp = *cp;
		    sc->sc_xc_state = (*cp)->comp_alloc(ccp_option, nb);
		    if (sc->sc_xc_state == NULL) {
			if (sc->sc_flags & SC_DEBUG)
			    if_printf(PPP2IFP(sc), "comp_alloc failed\n");
			error = ENOBUFS;
		    }
		    splimp();
		    sc->sc_flags &= ~SC_COMP_RUN;
		    splx(s);
		} else {
		    s = splsoftnet();
		    if (sc->sc_rc_state != NULL)
			(*sc->sc_rcomp->decomp_free)(sc->sc_rc_state);
		    sc->sc_rcomp = *cp;
		    sc->sc_rc_state = (*cp)->decomp_alloc(ccp_option, nb);
		    if (sc->sc_rc_state == NULL) {
			if (sc->sc_flags & SC_DEBUG)
			    if_printf(PPP2IFP(sc), "decomp_alloc failed\n");
			error = ENOBUFS;
		    }
		    splimp();
		    sc->sc_flags &= ~SC_DECOMP_RUN;
		    splx(s);
		}
		break;
	    }
	if (sc->sc_flags & SC_DEBUG)
	    if_printf(PPP2IFP(sc), "no compressor for [%x %x %x], %x\n",
		   ccp_option[0], ccp_option[1], ccp_option[2], nb);
	error = EINVAL;		/* no handler found */
        break;
#endif /* PPP_COMPRESS */

    case PPPIOCGNPMODE:
    case PPPIOCSNPMODE:
	npi = (struct npioctl *) data;
	npx = 0;	/* XXX: quiet gcc */
	switch (npi->protocol) {
	case PPP_IP:
	    npx = NP_IP;
	    break;
	default:
	    error = EINVAL;
	}
	if (error)
	    break;
	if (cmd == PPPIOCGNPMODE) {
	    npi->mode = sc->sc_npmode[npx];
	} else {
	    if ((error = suser(td)) != 0)
		break;
	    if (npi->mode != sc->sc_npmode[npx]) {
		s = splsoftnet();
		sc->sc_npmode[npx] = npi->mode;
		if (npi->mode != NPMODE_QUEUE) {
		    ppp_requeue(sc);
		    (*sc->sc_start)(sc);
		}
		splx(s);
	    }
	}
	break;

    case PPPIOCGIDLE:
	s = splsoftnet();
	t = time_uptime;
	((struct ppp_idle *)data)->xmit_idle = t - sc->sc_last_sent;
	((struct ppp_idle *)data)->recv_idle = t - sc->sc_last_recv;
	splx(s);
	break;

#ifdef PPP_FILTER
    case PPPIOCSPASS:
    case PPPIOCSACTIVE:
	nbp = (struct bpf_program *) data;
	if ((unsigned) nbp->bf_len > BPF_MAXINSNS) {
	    error = EINVAL;
	    break;
	}
	newcodelen = nbp->bf_len * sizeof(struct bpf_insn);
	if (newcodelen != 0) {
	    MALLOC(newcode, struct bpf_insn *, newcodelen, M_DEVBUF, M_WAITOK);
	    if (newcode == 0) {
		error = EINVAL;		/* or sumpin */
		break;
	    }
	    if ((error = copyin((caddr_t)nbp->bf_insns, (caddr_t)newcode,
			       newcodelen)) != 0) {
		free(newcode, M_DEVBUF);
		break;
	    }
	    if (!bpf_validate(newcode, nbp->bf_len)) {
		free(newcode, M_DEVBUF);
		error = EINVAL;
		break;
	    }
	} else
	    newcode = 0;
	bp = (cmd == PPPIOCSPASS)? &sc->sc_pass_filt: &sc->sc_active_filt;
	oldcode = bp->bf_insns;
	s = splimp();
	bp->bf_len = nbp->bf_len;
	bp->bf_insns = newcode;
	splx(s);
	if (oldcode != 0)
	    free(oldcode, M_DEVBUF);
	break;
#endif

    default:
	error = ENOIOCTL;
	break;
    }
    return (error);
}

/*
 * Process an ioctl request to the ppp network interface.
 */
static int
pppsioctl(ifp, cmd, data)
    register struct ifnet *ifp;
    u_long cmd;
    caddr_t data;
{
    struct thread *td = curthread;	/* XXX */
    register struct ppp_softc *sc = ifp->if_softc;
    register struct ifaddr *ifa = (struct ifaddr *)data;
    register struct ifreq *ifr = (struct ifreq *)data;
    struct ppp_stats *psp;
#ifdef	PPP_COMPRESS
    struct ppp_comp_stats *pcp;
#endif
    int s = splimp(), error = 0;

    switch (cmd) {
    case SIOCSIFFLAGS:
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
	    ifp->if_flags &= ~IFF_UP;
	break;

    case SIOCSIFADDR:
    case SIOCAIFADDR:
	switch(ifa->ifa_addr->sa_family) {
#ifdef INET
	case AF_INET:
	    break;
#endif
#ifdef IPX
	case AF_IPX:
	    break;
#endif
	default:
	    error = EAFNOSUPPORT;
	    break;
	}
	break;

    case SIOCSIFDSTADDR:
	switch(ifa->ifa_addr->sa_family) {
#ifdef INET
	case AF_INET:
	    break;
#endif
#ifdef IPX
	case AF_IPX:
	    break;
#endif
	default:
	    error = EAFNOSUPPORT;
	    break;
	}
	break;

    case SIOCSIFMTU:
	if ((error = suser(td)) != 0)
	    break;
	if (ifr->ifr_mtu > PPP_MAXMTU)
	    error = EINVAL;
	else {
	    PPP2IFP(sc)->if_mtu = ifr->ifr_mtu;
	    if (sc->sc_setmtu)
		    (*sc->sc_setmtu)(sc);
	}
	break;

    case SIOCGIFMTU:
	ifr->ifr_mtu = PPP2IFP(sc)->if_mtu;
	break;

    case SIOCADDMULTI:
    case SIOCDELMULTI:
	if (ifr == 0) {
	    error = EAFNOSUPPORT;
	    break;
	}
	switch(ifr->ifr_addr.sa_family) {
#ifdef INET
	case AF_INET:
	    break;
#endif
	default:
	    error = EAFNOSUPPORT;
	    break;
	}
	break;

    case SIOCGPPPSTATS:
	psp = &((struct ifpppstatsreq *) data)->stats;
	bzero(psp, sizeof(*psp));
	psp->p = sc->sc_stats;
#if defined(VJC) && !defined(SL_NO_STATS)
	if (sc->sc_comp) {
	    psp->vj.vjs_packets = sc->sc_comp->sls_packets;
	    psp->vj.vjs_compressed = sc->sc_comp->sls_compressed;
	    psp->vj.vjs_searches = sc->sc_comp->sls_searches;
	    psp->vj.vjs_misses = sc->sc_comp->sls_misses;
	    psp->vj.vjs_uncompressedin = sc->sc_comp->sls_uncompressedin;
	    psp->vj.vjs_compressedin = sc->sc_comp->sls_compressedin;
	    psp->vj.vjs_errorin = sc->sc_comp->sls_errorin;
	    psp->vj.vjs_tossed = sc->sc_comp->sls_tossed;
	}
#endif /* VJC */
	break;

#ifdef PPP_COMPRESS
    case SIOCGPPPCSTATS:
	pcp = &((struct ifpppcstatsreq *) data)->stats;
	bzero(pcp, sizeof(*pcp));
	if (sc->sc_xc_state != NULL)
	    (*sc->sc_xcomp->comp_stat)(sc->sc_xc_state, &pcp->c);
	if (sc->sc_rc_state != NULL)
	    (*sc->sc_rcomp->decomp_stat)(sc->sc_rc_state, &pcp->d);
	break;
#endif /* PPP_COMPRESS */

    default:
	error = ENOTTY;
    }
    splx(s);
    return (error);
}

/*
 * Queue a packet.  Start transmission if not active.
 * Packet is placed in Information field of PPP frame.
 * Called at splnet as the if->if_output handler.
 * Called at splnet from pppwrite().
 */
int
pppoutput(ifp, m0, dst, rtp)
    struct ifnet *ifp;
    struct mbuf *m0;
    struct sockaddr *dst;
    struct rtentry *rtp;
{
    register struct ppp_softc *sc = ifp->if_softc;
    int protocol, address, control;
    u_char *cp;
    int s, error;
    struct ip *ip;
    struct ifqueue *ifq;
    enum NPmode mode;
    int len;

#ifdef MAC
    error = mac_check_ifnet_transmit(ifp, m0);
    if (error)
	goto bad;
#endif

    if (sc->sc_devp == NULL || (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0
	|| ((ifp->if_flags & IFF_UP) == 0 && dst->sa_family != AF_UNSPEC)) {
	error = ENETDOWN;	/* sort of */
	goto bad;
    }

    /*
     * Compute PPP header.
     */
    m0->m_flags &= ~M_HIGHPRI;
    switch (dst->sa_family) {
#ifdef INET
    case AF_INET:
	address = PPP_ALLSTATIONS;
	control = PPP_UI;
	protocol = PPP_IP;
	mode = sc->sc_npmode[NP_IP];

	/*
	 * If this packet has the "low delay" bit set in the IP header,
	 * put it on the fastq instead.
	 */
	ip = mtod(m0, struct ip *);
	if (ip->ip_tos & IPTOS_LOWDELAY)
	    m0->m_flags |= M_HIGHPRI;
	break;
#endif
#ifdef IPX
    case AF_IPX:
	/*
	 * This is pretty bogus.. We dont have an ipxcp module in pppd
	 * yet to configure the link parameters.  Sigh. I guess a
	 * manual ifconfig would do....  -Peter
	 */
	address = PPP_ALLSTATIONS;
	control = PPP_UI;
	protocol = PPP_IPX;
	mode = NPMODE_PASS;
	break;
#endif
    case AF_UNSPEC:
	address = PPP_ADDRESS(dst->sa_data);
	control = PPP_CONTROL(dst->sa_data);
	protocol = PPP_PROTOCOL(dst->sa_data);
	mode = NPMODE_PASS;
	break;
    default:
	if_printf(ifp, "af%d not supported\n", dst->sa_family);
	error = EAFNOSUPPORT;
	goto bad;
    }

    /*
     * Drop this packet, or return an error, if necessary.
     */
    if (mode == NPMODE_ERROR) {
	error = ENETDOWN;
	goto bad;
    }
    if (mode == NPMODE_DROP) {
	error = 0;
	goto bad;
    }

    /*
     * Add PPP header.  If no space in first mbuf, allocate another.
     * (This assumes M_LEADINGSPACE is always 0 for a cluster mbuf.)
     */
    if (M_LEADINGSPACE(m0) < PPP_HDRLEN) {
	m0 = m_prepend(m0, PPP_HDRLEN, M_DONTWAIT);
	if (m0 == 0) {
	    error = ENOBUFS;
	    goto bad;
	}
	m0->m_len = 0;
    } else
	m0->m_data -= PPP_HDRLEN;

    cp = mtod(m0, u_char *);
    *cp++ = address;
    *cp++ = control;
    *cp++ = protocol >> 8;
    *cp++ = protocol & 0xff;
    m0->m_len += PPP_HDRLEN;

    len = m_length(m0, NULL);

    if (sc->sc_flags & SC_LOG_OUTPKT) {
	printf("%s output: ", ifp->if_xname);
	pppdumpm(m0);
    }

    if ((protocol & 0x8000) == 0) {
#ifdef PPP_FILTER
	/*
	 * Apply the pass and active filters to the packet,
	 * but only if it is a data packet.
	 */
	*mtod(m0, u_char *) = 1;	/* indicates outbound */
	if (sc->sc_pass_filt.bf_insns != 0
	    && bpf_filter(sc->sc_pass_filt.bf_insns, (u_char *) m0,
			  len, 0) == 0) {
	    error = 0;		/* drop this packet */
	    goto bad;
	}

	/*
	 * Update the time we sent the most recent packet.
	 */
	if (sc->sc_active_filt.bf_insns == 0
	    || bpf_filter(sc->sc_active_filt.bf_insns, (u_char *) m0, len, 0))
	    sc->sc_last_sent = time_uptime;

	*mtod(m0, u_char *) = address;
#else
	/*
	 * Update the time we sent the most recent data packet.
	 */
	sc->sc_last_sent = time_uptime;
#endif /* PPP_FILTER */
    }

    /*
     * See if bpf wants to look at the packet.
     */
    BPF_MTAP(ifp, m0);

    /*
     * Put the packet on the appropriate queue.
     */
    s = splsoftnet();	/* redundant */
    if (mode == NPMODE_QUEUE) {
	/* XXX we should limit the number of packets on this queue */
	*sc->sc_npqtail = m0;
	m0->m_nextpkt = NULL;
	sc->sc_npqtail = &m0->m_nextpkt;
    } else {
	/* fastq and if_snd are emptied at spl[soft]net now */
	ifq = (m0->m_flags & M_HIGHPRI)? &sc->sc_fastq:
	    (struct ifqueue *)&ifp->if_snd;
        IF_LOCK(ifq);
	if (_IF_QFULL(ifq) && dst->sa_family != AF_UNSPEC) {
	    _IF_DROP(ifq);
	    IF_UNLOCK(ifq);
	    PPP2IFP(sc)->if_oerrors++;
	    sc->sc_stats.ppp_oerrors++;
	    error = ENOBUFS;
	    goto bad;
	}
	_IF_ENQUEUE(ifq, m0);
        IF_UNLOCK(ifq);
	(*sc->sc_start)(sc);
    }
    getmicrotime(&ifp->if_lastchange);
    ifp->if_opackets++;
    ifp->if_obytes += len;

    splx(s);
    return (0);

bad:
    m_freem(m0);
    return (error);
}

/*
 * After a change in the NPmode for some NP, move packets from the
 * npqueue to the send queue or the fast queue as appropriate.
 * Should be called at spl[soft]net.
 */
static void
ppp_requeue(sc)
    struct ppp_softc *sc;
{
    struct mbuf *m, **mpp;
    struct ifqueue *ifq;
    enum NPmode mode;

    for (mpp = &sc->sc_npqueue; (m = *mpp) != NULL; ) {
	switch (PPP_PROTOCOL(mtod(m, u_char *))) {
	case PPP_IP:
	    mode = sc->sc_npmode[NP_IP];
	    break;
	default:
	    mode = NPMODE_PASS;
	}

	switch (mode) {
	case NPMODE_PASS:
	    /*
	     * This packet can now go on one of the queues to be sent.
	     */
	    *mpp = m->m_nextpkt;
	    m->m_nextpkt = NULL;
	    ifq = (m->m_flags & M_HIGHPRI)? &sc->sc_fastq:
	        (struct ifqueue *)&PPP2IFP(sc)->if_snd;
            if (! IF_HANDOFF(ifq, m, NULL)) {
		PPP2IFP(sc)->if_oerrors++;
		sc->sc_stats.ppp_oerrors++;
	    }
	    break;

	case NPMODE_DROP:
	case NPMODE_ERROR:
	    *mpp = m->m_nextpkt;
	    m_freem(m);
	    break;

	case NPMODE_QUEUE:
	    mpp = &m->m_nextpkt;
	    break;
	}
    }
    sc->sc_npqtail = mpp;
}

/*
 * Transmitter has finished outputting some stuff;
 * remember to call sc->sc_start later at splsoftnet.
 */
void
ppp_restart(sc)
    struct ppp_softc *sc;
{
    int s = splimp();

    sc->sc_flags &= ~SC_TBUSY;
    schednetisr(NETISR_PPP);
    splx(s);
}


/*
 * Get a packet to send.  This procedure is intended to be called at
 * splsoftnet, since it may involve time-consuming operations such as
 * applying VJ compression, packet compression, address/control and/or
 * protocol field compression to the packet.
 */
struct mbuf *
ppp_dequeue(sc)
    struct ppp_softc *sc;
{
    struct mbuf *m, *mp;
    u_char *cp;
    int address, control, protocol;

    /*
     * Grab a packet to send: first try the fast queue, then the
     * normal queue.
     */
    IF_DEQUEUE(&sc->sc_fastq, m);
    if (m == NULL)
	IF_DEQUEUE(&PPP2IFP(sc)->if_snd, m);
    if (m == NULL)
	return NULL;

    ++sc->sc_stats.ppp_opackets;

    /*
     * Extract the ppp header of the new packet.
     * The ppp header will be in one mbuf.
     */
    cp = mtod(m, u_char *);
    address = PPP_ADDRESS(cp);
    control = PPP_CONTROL(cp);
    protocol = PPP_PROTOCOL(cp);

    switch (protocol) {
    case PPP_IP:
#ifdef VJC
	/*
	 * If the packet is a TCP/IP packet, see if we can compress it.
	 */
	if ((sc->sc_flags & SC_COMP_TCP) && sc->sc_comp != NULL) {
	    struct ip *ip;
	    int type;

	    mp = m;
	    ip = (struct ip *) (cp + PPP_HDRLEN);
	    if (mp->m_len <= PPP_HDRLEN) {
		mp = mp->m_next;
		if (mp == NULL)
		    break;
		ip = mtod(mp, struct ip *);
	    }
	    /* this code assumes the IP/TCP header is in one non-shared mbuf */
	    if (ip->ip_p == IPPROTO_TCP) {
		type = sl_compress_tcp(mp, ip, sc->sc_comp,
				       !(sc->sc_flags & SC_NO_TCP_CCID));
		switch (type) {
		case TYPE_UNCOMPRESSED_TCP:
		    protocol = PPP_VJC_UNCOMP;
		    break;
		case TYPE_COMPRESSED_TCP:
		    protocol = PPP_VJC_COMP;
		    cp = mtod(m, u_char *);
		    cp[0] = address;	/* header has moved */
		    cp[1] = control;
		    cp[2] = 0;
		    break;
		}
		cp[3] = protocol;	/* update protocol in PPP header */
	    }
	}
#endif	/* VJC */
	break;

#ifdef PPP_COMPRESS
    case PPP_CCP:
	ppp_ccp(sc, m, 0);
	break;
#endif	/* PPP_COMPRESS */
    }

#ifdef PPP_COMPRESS
    if (protocol != PPP_LCP && protocol != PPP_CCP
	&& sc->sc_xc_state && (sc->sc_flags & SC_COMP_RUN)) {
	struct mbuf *mcomp = NULL;
	int slen, clen;

	slen = m_length(m, NULL);
	clen = (*sc->sc_xcomp->compress)
	    (sc->sc_xc_state, &mcomp, m, slen, PPP2IFP(sc)->if_mtu + PPP_HDRLEN);
	if (mcomp != NULL) {
	    if (sc->sc_flags & SC_CCP_UP) {
		/* Send the compressed packet instead of the original. */
		m_freem(m);
		m = mcomp;
		cp = mtod(m, u_char *);
		protocol = cp[3];
	    } else {
		/* Can't transmit compressed packets until CCP is up. */
		m_freem(mcomp);
	    }
	}
    }
#endif	/* PPP_COMPRESS */

    /*
     * Compress the address/control and protocol, if possible.
     */
    if (sc->sc_flags & SC_COMP_AC && address == PPP_ALLSTATIONS &&
	control == PPP_UI && protocol != PPP_ALLSTATIONS &&
	protocol != PPP_LCP) {
	/* can compress address/control */
	m->m_data += 2;
	m->m_len -= 2;
    }
    if (sc->sc_flags & SC_COMP_PROT && protocol < 0xFF) {
	/* can compress protocol */
	if (mtod(m, u_char *) == cp) {
	    cp[2] = cp[1];	/* move address/control up */
	    cp[1] = cp[0];
	}
	++m->m_data;
	--m->m_len;
    }

    return m;
}

/*
 * Software interrupt routine, called at spl[soft]net.
 */
static void
pppintr()
{
    struct ppp_softc *sc;
    int s;
    struct mbuf *m;

    GIANT_REQUIRED;

    PPP_LIST_LOCK();
    LIST_FOREACH(sc, &ppp_softc_list, sc_list) {
	s = splimp();
	if (!(sc->sc_flags & SC_TBUSY)
	    && (PPP2IFP(sc)->if_snd.ifq_head || sc->sc_fastq.ifq_head)) {
	    sc->sc_flags |= SC_TBUSY;
	    splx(s);
	    (*sc->sc_start)(sc);
	} else
	    splx(s);
	for (;;) {
	    s = splimp();
	    IF_DEQUEUE(&sc->sc_rawq, m);
	    splx(s);
	    if (m == NULL)
		break;
#ifdef MAC
	    mac_create_mbuf_from_ifnet(PPP2IFP(sc), m);
#endif
	    ppp_inproc(sc, m);
	}
    }
    PPP_LIST_UNLOCK();
}

#ifdef PPP_COMPRESS
/*
 * Handle a CCP packet.  `rcvd' is 1 if the packet was received,
 * 0 if it is about to be transmitted.
 */
static void
ppp_ccp(sc, m, rcvd)
    struct ppp_softc *sc;
    struct mbuf *m;
    int rcvd;
{
    u_char *dp, *ep;
    struct mbuf *mp;
    int slen, s;

    /*
     * Get a pointer to the data after the PPP header.
     */
    if (m->m_len <= PPP_HDRLEN) {
	mp = m->m_next;
	if (mp == NULL)
	    return;
	dp = (mp != NULL)? mtod(mp, u_char *): NULL;
    } else {
	mp = m;
	dp = mtod(mp, u_char *) + PPP_HDRLEN;
    }

    ep = mtod(mp, u_char *) + mp->m_len;
    if (dp + CCP_HDRLEN > ep)
	return;
    slen = CCP_LENGTH(dp);
    if (dp + slen > ep) {
	if (sc->sc_flags & SC_DEBUG)
	    printf("if_ppp/ccp: not enough data in mbuf (%p+%x > %p+%x)\n",
		   dp, slen, mtod(mp, u_char *), mp->m_len);
	return;
    }

    switch (CCP_CODE(dp)) {
    case CCP_CONFREQ:
    case CCP_TERMREQ:
    case CCP_TERMACK:
	/* CCP must be going down - disable compression */
	if (sc->sc_flags & SC_CCP_UP) {
	    s = splimp();
	    sc->sc_flags &= ~(SC_CCP_UP | SC_COMP_RUN | SC_DECOMP_RUN);
	    splx(s);
	}
	break;

    case CCP_CONFACK:
	if (sc->sc_flags & SC_CCP_OPEN && !(sc->sc_flags & SC_CCP_UP)
	    && slen >= CCP_HDRLEN + CCP_OPT_MINLEN
	    && slen >= CCP_OPT_LENGTH(dp + CCP_HDRLEN) + CCP_HDRLEN) {
	    if (!rcvd) {
		/* we're agreeing to send compressed packets. */
		if (sc->sc_xc_state != NULL
		    && (*sc->sc_xcomp->comp_init)
			(sc->sc_xc_state, dp + CCP_HDRLEN, slen - CCP_HDRLEN,
			 PPP2IFP(sc)->if_dunit, 0, sc->sc_flags & SC_DEBUG)) {
		    s = splimp();
		    sc->sc_flags |= SC_COMP_RUN;
		    splx(s);
		}
	    } else {
		/* peer is agreeing to send compressed packets. */
		if (sc->sc_rc_state != NULL
		    && (*sc->sc_rcomp->decomp_init)
			(sc->sc_rc_state, dp + CCP_HDRLEN, slen - CCP_HDRLEN,
			 PPP2IFP(sc)->if_dunit, 0, sc->sc_mru,
			 sc->sc_flags & SC_DEBUG)) {
		    s = splimp();
		    sc->sc_flags |= SC_DECOMP_RUN;
		    sc->sc_flags &= ~(SC_DC_ERROR | SC_DC_FERROR);
		    splx(s);
		}
	    }
	}
	break;

    case CCP_RESETACK:
	if (sc->sc_flags & SC_CCP_UP) {
	    if (!rcvd) {
		if (sc->sc_xc_state && (sc->sc_flags & SC_COMP_RUN))
		    (*sc->sc_xcomp->comp_reset)(sc->sc_xc_state);
	    } else {
		if (sc->sc_rc_state && (sc->sc_flags & SC_DECOMP_RUN)) {
		    (*sc->sc_rcomp->decomp_reset)(sc->sc_rc_state);
		    s = splimp();
		    sc->sc_flags &= ~SC_DC_ERROR;
		    splx(s);
		}
	    }
	}
	break;
    }
}

/*
 * CCP is down; free (de)compressor state if necessary.
 */
static void
ppp_ccp_closed(sc)
    struct ppp_softc *sc;
{
    if (sc->sc_xc_state) {
	(*sc->sc_xcomp->comp_free)(sc->sc_xc_state);
	sc->sc_xc_state = NULL;
    }
    if (sc->sc_rc_state) {
	(*sc->sc_rcomp->decomp_free)(sc->sc_rc_state);
	sc->sc_rc_state = NULL;
    }
}
#endif /* PPP_COMPRESS */

/*
 * PPP packet input routine.
 * The caller has checked and removed the FCS and has inserted
 * the address/control bytes and the protocol high byte if they
 * were omitted.
 */
void
ppppktin(sc, m, lost)
    struct ppp_softc *sc;
    struct mbuf *m;
    int lost;
{
    int s = splimp();

    if (lost)
	m->m_flags |= M_ERRMARK;
    IF_ENQUEUE(&sc->sc_rawq, m);
    schednetisr(NETISR_PPP);
    splx(s);
}

/*
 * Process a received PPP packet, doing decompression as necessary.
 * Should be called at splsoftnet.
 */
#define COMPTYPE(proto)	((proto) == PPP_VJC_COMP? TYPE_COMPRESSED_TCP: \
			 TYPE_UNCOMPRESSED_TCP)

static void
ppp_inproc(sc, m)
    struct ppp_softc *sc;
    struct mbuf *m;
{
    struct ifnet *ifp = PPP2IFP(sc);
    int isr;
    int s, ilen = 0, xlen, proto, rv;
    u_char *cp, adrs, ctrl;
    struct mbuf *mp, *dmp = NULL;
    u_char *iphdr;
    u_int hlen;

    sc->sc_stats.ppp_ipackets++;

    if (sc->sc_flags & SC_LOG_INPKT) {
	ilen = m_length(m, NULL);
	if_printf(ifp, "got %d bytes\n", ilen);
	pppdumpm(m);
    }

    cp = mtod(m, u_char *);
    adrs = PPP_ADDRESS(cp);
    ctrl = PPP_CONTROL(cp);
    proto = PPP_PROTOCOL(cp);

    if (m->m_flags & M_ERRMARK) {
	m->m_flags &= ~M_ERRMARK;
	s = splimp();
	sc->sc_flags |= SC_VJ_RESET;
	splx(s);
    }

#ifdef PPP_COMPRESS
    /*
     * Decompress this packet if necessary, update the receiver's
     * dictionary, or take appropriate action on a CCP packet.
     */
    if (proto == PPP_COMP && sc->sc_rc_state && (sc->sc_flags & SC_DECOMP_RUN)
	&& !(sc->sc_flags & SC_DC_ERROR) && !(sc->sc_flags & SC_DC_FERROR)) {
	/* decompress this packet */
	rv = (*sc->sc_rcomp->decompress)(sc->sc_rc_state, m, &dmp);
	if (rv == DECOMP_OK) {
	    m_freem(m);
	    if (dmp == NULL) {
		/* no error, but no decompressed packet produced */
		return;
	    }
	    m = dmp;
	    cp = mtod(m, u_char *);
	    proto = PPP_PROTOCOL(cp);

	} else {
	    /*
	     * An error has occurred in decompression.
	     * Pass the compressed packet up to pppd, which may take
	     * CCP down or issue a Reset-Req.
	     */
	    if (sc->sc_flags & SC_DEBUG)
		if_printf(ifp, "decompress failed %d\n", rv);
	    s = splimp();
	    sc->sc_flags |= SC_VJ_RESET;
	    if (rv == DECOMP_ERROR)
		sc->sc_flags |= SC_DC_ERROR;
	    else
		sc->sc_flags |= SC_DC_FERROR;
	    splx(s);
	}

    } else {
	if (sc->sc_rc_state && (sc->sc_flags & SC_DECOMP_RUN)) {
	    (*sc->sc_rcomp->incomp)(sc->sc_rc_state, m);
	}
	if (proto == PPP_CCP) {
	    ppp_ccp(sc, m, 1);
	}
    }
#endif

    ilen = m_length(m, NULL);

#ifdef VJC
    if (sc->sc_flags & SC_VJ_RESET) {
	/*
	 * If we've missed a packet, we must toss subsequent compressed
	 * packets which don't have an explicit connection ID.
	 */
	if (sc->sc_comp)
	    sl_uncompress_tcp(NULL, 0, TYPE_ERROR, sc->sc_comp);
	s = splimp();
	sc->sc_flags &= ~SC_VJ_RESET;
	splx(s);
    }

    /*
     * See if we have a VJ-compressed packet to uncompress.
     */
    if (proto == PPP_VJC_COMP) {
	if ((sc->sc_flags & SC_REJ_COMP_TCP) || sc->sc_comp == 0)
	    goto bad;

	xlen = sl_uncompress_tcp_core(cp + PPP_HDRLEN, m->m_len - PPP_HDRLEN,
				      ilen - PPP_HDRLEN, TYPE_COMPRESSED_TCP,
				      sc->sc_comp, &iphdr, &hlen);

	if (xlen <= 0) {
	    if (sc->sc_flags & SC_DEBUG)
		if_printf(ifp, "VJ uncompress failed on type comp\n");
	    goto bad;
	}

	/* Copy the PPP and IP headers into a new mbuf. */
	MGETHDR(mp, M_DONTWAIT, MT_DATA);
	if (mp == NULL)
	    goto bad;
	mp->m_len = 0;
	mp->m_next = NULL;
	if (hlen + PPP_HDRLEN > MHLEN) {
	    MCLGET(mp, M_DONTWAIT);
	    if (M_TRAILINGSPACE(mp) < hlen + PPP_HDRLEN) {
		m_freem(mp);
		goto bad;	/* lose if big headers and no clusters */
	    }
	}
#ifdef MAC
	mac_copy_mbuf(m, mp);
#endif
	cp = mtod(mp, u_char *);
	cp[0] = adrs;
	cp[1] = ctrl;
	cp[2] = 0;
	cp[3] = PPP_IP;
	proto = PPP_IP;
	bcopy(iphdr, cp + PPP_HDRLEN, hlen);
	mp->m_len = hlen + PPP_HDRLEN;

	/*
	 * Trim the PPP and VJ headers off the old mbuf
	 * and stick the new and old mbufs together.
	 */
	m->m_data += PPP_HDRLEN + xlen;
	m->m_len -= PPP_HDRLEN + xlen;
	if (m->m_len <= M_TRAILINGSPACE(mp)) {
	    bcopy(mtod(m, u_char *), mtod(mp, u_char *) + mp->m_len, m->m_len);
	    mp->m_len += m->m_len;
	    mp->m_next = m_free(m);
	} else {
	    mp->m_next = m;
	}
	m = mp;
	ilen += hlen - xlen;

    } else if (proto == PPP_VJC_UNCOMP) {
	if ((sc->sc_flags & SC_REJ_COMP_TCP) || sc->sc_comp == 0)
	    goto bad;

	xlen = sl_uncompress_tcp_core(cp + PPP_HDRLEN, m->m_len - PPP_HDRLEN,
				      ilen - PPP_HDRLEN, TYPE_UNCOMPRESSED_TCP,
				      sc->sc_comp, &iphdr, &hlen);

	if (xlen < 0) {
	    if (sc->sc_flags & SC_DEBUG)
		if_printf(ifp, "VJ uncompress failed on type uncomp\n");
	    goto bad;
	}

	proto = PPP_IP;
	cp[3] = PPP_IP;
    }
#endif /* VJC */

    /*
     * If the packet will fit in a header mbuf, don't waste a
     * whole cluster on it.
     */
    if (ilen <= MHLEN && M_IS_CLUSTER(m)) {
	MGETHDR(mp, M_DONTWAIT, MT_DATA);
	if (mp != NULL) {
#ifdef MAC
	    mac_copy_mbuf(m, mp);
#endif
	    m_copydata(m, 0, ilen, mtod(mp, caddr_t));
	    m_freem(m);
	    m = mp;
	    m->m_len = ilen;
	}
    }
    m->m_pkthdr.len = ilen;
    m->m_pkthdr.rcvif = ifp;

    if ((proto & 0x8000) == 0) {
#ifdef PPP_FILTER
	/*
	 * See whether we want to pass this packet, and
	 * if it counts as link activity.
	 */
	adrs = *mtod(m, u_char *);	/* save address field */
	*mtod(m, u_char *) = 0;		/* indicate inbound */
	if (sc->sc_pass_filt.bf_insns != 0
	    && bpf_filter(sc->sc_pass_filt.bf_insns, (u_char *) m,
			  ilen, 0) == 0) {
	    /* drop this packet */
	    m_freem(m);
	    return;
	}
	if (sc->sc_active_filt.bf_insns == 0
	    || bpf_filter(sc->sc_active_filt.bf_insns, (u_char *) m, ilen, 0))
	    sc->sc_last_recv = time_uptime;

	*mtod(m, u_char *) = adrs;
#else
	/*
	 * Record the time that we received this packet.
	 */
	sc->sc_last_recv = time_uptime;
#endif /* PPP_FILTER */
    }

    /* See if bpf wants to look at the packet. */
    BPF_MTAP(PPP2IFP(sc), m);

    isr = -1;
    switch (proto) {
#ifdef INET
    case PPP_IP:
	/*
	 * IP packet - take off the ppp header and pass it up to IP.
	 */
	if ((ifp->if_flags & IFF_UP) == 0
	    || sc->sc_npmode[NP_IP] != NPMODE_PASS) {
	    /* interface is down - drop the packet. */
	    m_freem(m);
	    return;
	}
	m->m_pkthdr.len -= PPP_HDRLEN;
	m->m_data += PPP_HDRLEN;
	m->m_len -= PPP_HDRLEN;
	if ((m = ip_fastforward(m)) == NULL)
	    return;
	isr = NETISR_IP;
	break;
#endif
#ifdef IPX
    case PPP_IPX:
	/*
	 * IPX packet - take off the ppp header and pass it up to IPX.
	 */
	if ((PPP2IFP(sc)->if_flags & IFF_UP) == 0
	    /* XXX: || sc->sc_npmode[NP_IPX] != NPMODE_PASS*/) {
	    /* interface is down - drop the packet. */
	    m_freem(m);
	    return;
	}
	m->m_pkthdr.len -= PPP_HDRLEN;
	m->m_data += PPP_HDRLEN;
	m->m_len -= PPP_HDRLEN;
	isr = NETISR_IPX;
	sc->sc_last_recv = time_uptime;	/* update time of last pkt rcvd */
	break;
#endif

    default:
	/*
	 * Some other protocol - place on input queue for read().
	 */
	break;
    }

    if (isr == -1)
      rv = IF_HANDOFF(&sc->sc_inq, m, NULL);
    else
      rv = netisr_queue(isr, m);	/* (0) on success. */
    if ((isr == -1 && !rv) || (isr != -1 && rv)) {
	if (sc->sc_flags & SC_DEBUG)
	    if_printf(ifp, "input queue full\n");
	ifp->if_iqdrops++;
	m = NULL;
	goto bad;
    }
    ifp->if_ipackets++;
    ifp->if_ibytes += ilen;
    getmicrotime(&ifp->if_lastchange);

    if (isr == -1)
	(*sc->sc_ctlp)(sc);

    return;

 bad:
    if (m)
        m_freem(m);
    PPP2IFP(sc)->if_ierrors++;
    sc->sc_stats.ppp_ierrors++;
}

#define MAX_DUMP_BYTES	128

static void
pppdumpm(m0)
    struct mbuf *m0;
{
    char buf[3*MAX_DUMP_BYTES+4];
    char *bp = buf;
    struct mbuf *m;

    for (m = m0; m; m = m->m_next) {
	int l = m->m_len;
	u_char *rptr = (u_char *)m->m_data;

	while (l--) {
	    if (bp > buf + (sizeof(buf) - 4))
		goto done;
	    *bp++ = hex2ascii(*rptr >> 4);
	    *bp++ = hex2ascii(*rptr++ & 0xf);
	}

	if (m->m_next) {
	    if (bp > buf + (sizeof(buf) - 3))
		goto done;
	    *bp++ = '|';
	} else
	    *bp++ = ' ';
    }
done:
    if (m)
	*bp++ = '>';
    *bp = 0;
    printf("%s\n", buf);
}
