/*
 *   Copyright (c) 1997 Joerg Wunsch. All rights reserved.
 *
 *   Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_isppp.c - isdn4bsd kernel SyncPPP driver
 *	--------------------------------------------
 *
 * 	Uses Serge Vakulenko's sppp backend (originally contributed with
 *	the "cx" driver for Cronyx's HDLC-in-hardware device).  This driver
 *	is only the glue between sppp and i4b.
 *
 *	$Id: i4b_isppp.c,v 1.44 2000/08/31 07:07:26 hm Exp $
 *
 * $FreeBSD$
 *
 *	last edit-date: [Thu Aug 31 09:02:27 2000]
 *
 *---------------------------------------------------------------------------*/


#ifndef __NetBSD__
#define USE_ISPPP
#endif
#include "i4bisppp.h"

#ifndef USE_ISPPP

#ifdef __FreeBSD__
#include "sppp.h"
#endif

#ifndef __NetBSD__
#if NI4BISPPP == 0
# error "You need to define `pseudo-device sppp <N>' with options ISPPP"
#endif
#endif

#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/ioccom.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/protosw.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <net/slcompress.h>

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#ifndef USE_ISPPP
#include <net/if_sppp.h>
#else
#include <machine/i4b_isppp.h>
#endif /* USE_ISPPP */
#else
#include <net/if_sppp.h>
#endif


#if defined(__FreeBSD_version) &&  __FreeBSD_version >= 400008                
#include "bpf.h"     
#else
#include "bpfilter.h"
#endif
#if NBPFILTER > 0 || NBPF > 0
#include <sys/time.h>
#include <net/bpf.h>
#endif

#ifdef __FreeBSD__
#include <machine/i4b_ioctl.h>
#include <machine/i4b_cause.h>
#include <machine/i4b_debug.h>
#else
#include <i4b/i4b_ioctl.h>
#include <i4b/i4b_cause.h>
#include <i4b/i4b_debug.h>
#endif

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_l3l4.h>

#include <i4b/layer4/i4b_l4.h>

#ifdef __FreeBSD__
#define ISPPP_FMT	"isp%d: "
#define	ISPPP_ARG(sc)	((sc)->sc_if.if_unit)
#define	PDEVSTATIC	static
#define IFP2UNIT(ifp)	(ifp)->if_unit
		
# if __FreeBSD_version >= 300001
#  define CALLOUT_INIT(chan)		callout_handle_init(chan)
#  define TIMEOUT(fun, arg, chan, tick)	chan = timeout(fun, arg, tick)
#  define UNTIMEOUT(fun, arg, chan)	untimeout(fun, arg, chan)
#  define IOCTL_CMD_T u_long
# else
#  define CALLOUT_INIT(chan)		do {} while(0)
#  define TIMEOUT(fun, arg, chan, tick)	timeout(fun, arg, tick)
#  define UNTIMEOUT(fun, arg, chan)	untimeout(fun, arg)
#  define IOCTL_CMD_T int
# endif

#elif defined __NetBSD__ || defined __OpenBSD__
#define	ISPPP_FMT	"%s: "
#define	ISPPP_ARG(sc)	((sc)->sc_if.if_xname)
#define	PDEVSTATIC	/* not static */
#define IOCTL_CMD_T	u_long
#define IFP2UNIT(ifp)	((struct i4bisppp_softc *)ifp->if_softc)->sc_unit
#else
# error "What system are you using?"
#endif

#ifdef __FreeBSD__
PDEVSTATIC void i4bispppattach(void *);
PSEUDO_SET(i4bispppattach, i4b_isppp);
#else
PDEVSTATIC void i4bispppattach(void);
#endif

#define I4BISPPPACCT		1	/* enable accounting messages */
#define	I4BISPPPACCTINTVL	2	/* accounting msg interval in secs */
#define I4BISPPPDISCDEBUG	1	

#define PPP_HDRLEN   		4	/* 4 octetts PPP header length	*/

struct i4bisppp_softc {
	/*
	 * struct sppp starts with a struct ifnet, but we gotta allocate
	 * more space for it.  NB: do not relocate this union, it must
	 * be first in isppp_softc.  The tls and tlf hooks below want to
	 * convert a ``struct sppp *'' into a ``struct isppp_softc *''.
	 */
	union {
		struct ifnet scu_if;
		struct sppp scu_sp;
	} sc_if_un;
#define sc_if sc_if_un.scu_if

	int	sc_state;	/* state of the interface	*/

#ifndef __FreeBSD__
	int	sc_unit;	/* unit number for Net/OpenBSD	*/
#endif

	call_desc_t *sc_cdp;	/* ptr to call descriptor	*/

#ifdef I4BISPPPACCT
	int sc_iinb;		/* isdn driver # of inbytes	*/
	int sc_ioutb;		/* isdn driver # of outbytes	*/
	int sc_inb;		/* # of bytes rx'd		*/
	int sc_outb;		/* # of bytes tx'd	 	*/
	int sc_linb;		/* last # of bytes rx'd		*/
	int sc_loutb;		/* last # of bytes tx'd 	*/
	int sc_fn;		/* flag, first null acct	*/
#endif

#if defined(__FreeBSD_version) && __FreeBSD_version >= 300001
	struct callout_handle sc_ch;
#endif

} i4bisppp_softc[NI4BISPPP];

static void	i4bisppp_init_linktab(int unit);
static int	i4bisppp_ioctl(struct ifnet *ifp, IOCTL_CMD_T cmd, caddr_t data);

#if 0
static void	i4bisppp_send(struct ifnet *ifp);
#endif

static void	i4bisppp_start(struct ifnet *ifp);

#if 0 /* never used ??? */
static void	i4bisppp_timeout(void *cookie);
#endif

static void	i4bisppp_tls(struct sppp *sp);
static void	i4bisppp_tlf(struct sppp *sp);
static void	i4bisppp_state_changed(struct sppp *sp, int new_state);
static void	i4bisppp_negotiation_complete(struct sppp *sp);
static void	i4bisppp_watchdog(struct ifnet *ifp);
time_t   	i4bisppp_idletime(int unit);

/* initialized by L4 */

static drvr_link_t i4bisppp_drvr_linktab[NI4BISPPP];
static isdn_link_t *isdn_linktab[NI4BISPPP];

enum i4bisppp_states {
	ST_IDLE,			/* initialized, ready, idle	*/
	ST_DIALING,			/* dialling out to remote	*/
	ST_CONNECTED,			/* connected to remote		*/
};

/*===========================================================================*
 *			DEVICE DRIVER ROUTINES
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	interface attach routine at kernel boot time
 *---------------------------------------------------------------------------*/
PDEVSTATIC void
#ifdef __FreeBSD__
i4bispppattach(void *dummy)
#else
i4bispppattach()
#endif
{
	struct i4bisppp_softc *sc = i4bisppp_softc;
	int i;

#ifndef HACK_NO_PSEUDO_ATTACH_MSG
#ifdef SPPP_VJ
	printf("i4bisppp: %d ISDN SyncPPP device(s) attached (VJ header compression)\n", NI4BISPPP);
#else
	printf("i4bisppp: %d ISDN SyncPPP device(s) attached\n", NI4BISPPP);
#endif
#endif

	for(i = 0; i < NI4BISPPP; sc++, i++) {
		i4bisppp_init_linktab(i);
		
		sc->sc_if.if_softc = sc;

#ifdef __FreeBSD__		
		sc->sc_if.if_name = "isp";
#if defined(__FreeBSD_version) && __FreeBSD_version < 300001
		sc->sc_if.if_next = NULL;
#endif
		sc->sc_if.if_unit = i;
#else
		sprintf(sc->sc_if.if_xname, "isp%d", i);
		sc->sc_unit = i;
#endif

		sc->sc_if.if_mtu = PP_MTU;

#ifdef __NetBSD__
		sc->sc_if.if_flags = IFF_SIMPLEX | IFF_POINTOPOINT |
					IFF_MULTICAST;
#else
		sc->sc_if.if_flags = IFF_SIMPLEX | IFF_POINTOPOINT;
#endif

		sc->sc_if.if_type = IFT_ISDNBASIC;
		sc->sc_state = ST_IDLE;

		sc->sc_if.if_ioctl = i4bisppp_ioctl;

		/* actually initialized by sppp_attach() */
		/* sc->sc_if.if_output = sppp_output; */

		sc->sc_if.if_start = i4bisppp_start;

		sc->sc_if.if_hdrlen = 0;
		sc->sc_if.if_addrlen = 0;
		sc->sc_if.if_snd.ifq_maxlen = IFQ_MAXLEN;

		sc->sc_if.if_ipackets = 0;
		sc->sc_if.if_ierrors = 0;
		sc->sc_if.if_opackets = 0;
		sc->sc_if.if_oerrors = 0;
		sc->sc_if.if_collisions = 0;
		sc->sc_if.if_ibytes = 0;
		sc->sc_if.if_obytes = 0;
		sc->sc_if.if_imcasts = 0;
		sc->sc_if.if_omcasts = 0;
		sc->sc_if.if_iqdrops = 0;
		sc->sc_if.if_noproto = 0;

#if I4BISPPPACCT
		sc->sc_if.if_timer = 0;	
		sc->sc_if.if_watchdog = i4bisppp_watchdog;	
		sc->sc_iinb = 0;
		sc->sc_ioutb = 0;
		sc->sc_inb = 0;
		sc->sc_outb = 0;
		sc->sc_linb = 0;
		sc->sc_loutb = 0;
		sc->sc_fn = 1;
#endif

		sc->sc_if_un.scu_sp.pp_tls = i4bisppp_tls;
		sc->sc_if_un.scu_sp.pp_tlf = i4bisppp_tlf;
		sc->sc_if_un.scu_sp.pp_con = i4bisppp_negotiation_complete;
		sc->sc_if_un.scu_sp.pp_chg = i4bisppp_state_changed;

#ifndef USE_ISPPP
		sppp_attach(&sc->sc_if);
#else
		isppp_attach(&sc->sc_if);
#endif
#if defined(__FreeBSD_version) && ((__FreeBSD_version >= 500009) || (410000 <= __FreeBSD_version && __FreeBSD_version < 500000))
		/* do not call bpfattach in ether_ifattach */
		ether_ifattach(&sc->sc_if, 0);
#else
		if_attach(&sc->sc_if);
#endif

#if NBPFILTER > 0 || NBPF > 0
#ifdef __FreeBSD__
		bpfattach(&sc->sc_if, DLT_PPP, PPP_HDRLEN);
		CALLOUT_INIT(&sc->sc_ch);
#endif /* __FreeBSD__ */
#ifdef __NetBSD__
		bpfattach(&sc->sc_if.if_bpf, &sc->sc_if, DLT_PPP, sizeof(u_int));
#endif
#endif		
	}
}

/*---------------------------------------------------------------------------*
 *	process ioctl
 *---------------------------------------------------------------------------*/
static int
i4bisppp_ioctl(struct ifnet *ifp, IOCTL_CMD_T cmd, caddr_t data)
{
	struct i4bisppp_softc *sc = ifp->if_softc;
#if 0
	struct sppp *sp = (struct sppp *)sc;
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct ifreq *ifr = (struct ifreq *) data;
#endif

	int error;

#ifndef USE_ISPPP		
	error = sppp_ioctl(&sc->sc_if, cmd, data);
#else
	error = isppp_ioctl(&sc->sc_if, cmd, data);
#endif
	if (error)
		return error;

	switch(cmd) {
	case SIOCSIFFLAGS:
#if 0 /* never used ??? */
		x = splimp();
		if ((ifp->if_flags & IFF_UP) == 0)
			UNTIMEOUT(i4bisppp_timeout, (void *)sp, sc->sc_ch);
		splx(x);
#endif
		break;
	}

	return 0;
}

/*---------------------------------------------------------------------------*
 *	start output to ISDN B-channel
 *---------------------------------------------------------------------------*/
static void
i4bisppp_start(struct ifnet *ifp)
{
	struct i4bisppp_softc *sc = ifp->if_softc;
	struct mbuf *m;
	/* int s; */
	int unit = IFP2UNIT(ifp);

#ifndef USE_ISPPP
	if (sppp_isempty(ifp))
#else
	if (isppp_isempty(ifp))
#endif
		return;

	if(sc->sc_state != ST_CONNECTED)
		return;

	/*
	 * s = splimp();
	 * ifp->if_flags |= IFF_OACTIVE; // - need to clear this somewhere
	 * splx(s);
	 */

#ifndef USE_ISPPP
	while ((m = sppp_dequeue(&sc->sc_if)) != NULL)
#else
	while ((m = isppp_dequeue(&sc->sc_if)) != NULL)
#endif
	{

#if NBPFILTER > 0 || NBPF > 0
#ifdef __FreeBSD__
		if (ifp->if_bpf)
			bpf_mtap(ifp, m);
#endif /* __FreeBSD__ */

#ifdef __NetBSD__
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif
#endif /* NBPFILTER > 0 || NBPF > 0 */

		microtime(&ifp->if_lastchange);

		if(IF_QFULL(isdn_linktab[unit]->tx_queue))
		{
			NDBGL4(L4_ISPDBG, "isp%d, tx queue full!", unit);
			m_freem(m);
		}
		else
		{
			IF_ENQUEUE(isdn_linktab[unit]->tx_queue, m);
#if 0
			sc->sc_if.if_obytes += m->m_pkthdr.len;
#endif
			sc->sc_outb += m->m_pkthdr.len;
			sc->sc_if.if_opackets++;
		}
	}
	isdn_linktab[unit]->bch_tx_start(isdn_linktab[unit]->unit,
					 isdn_linktab[unit]->channel);
}

#ifdef I4BISPPPACCT
/*---------------------------------------------------------------------------*
 *	watchdog routine
 *---------------------------------------------------------------------------*/
static void
i4bisppp_watchdog(struct ifnet *ifp)
{
	struct i4bisppp_softc *sc = ifp->if_softc;
	int unit = IFP2UNIT(ifp);
	bchan_statistics_t bs;
	
	(*isdn_linktab[unit]->bch_stat)
		(isdn_linktab[unit]->unit, isdn_linktab[unit]->channel, &bs);

	sc->sc_ioutb += bs.outbytes;
	sc->sc_iinb += bs.inbytes;
	
	if((sc->sc_iinb != sc->sc_linb) || (sc->sc_ioutb != sc->sc_loutb) || sc->sc_fn)
	{
		int ri = (sc->sc_iinb - sc->sc_linb)/I4BISPPPACCTINTVL;
		int ro = (sc->sc_ioutb - sc->sc_loutb)/I4BISPPPACCTINTVL;

		if((sc->sc_iinb == sc->sc_linb) && (sc->sc_ioutb == sc->sc_loutb))
			sc->sc_fn = 0;
		else
			sc->sc_fn = 1;
			
		sc->sc_linb = sc->sc_iinb;
		sc->sc_loutb = sc->sc_ioutb;

		i4b_l4_accounting(BDRV_ISPPP, unit, ACCT_DURING,
			 sc->sc_ioutb, sc->sc_iinb, ro, ri, sc->sc_outb, sc->sc_inb);
 	}
	sc->sc_if.if_timer = I4BISPPPACCTINTVL; 	

#if 0 /* old stuff, keep it around */
	printf(ISPPP_FMT "transmit timeout\n", ISPPP_ARG(sc));
	i4bisppp_start(ifp);
#endif
}
#endif /* I4BISPPPACCT */

/*
 *===========================================================================*
 *			SyncPPP layer interface routines
 *===========================================================================*
 */

#if 0 /* never used ??? */
/*---------------------------------------------------------------------------*
 *	just an alias for i4bisppp_tls, but of type timeout_t
 *---------------------------------------------------------------------------*/
static void
i4bisppp_timeout(void *cookie)
{
	i4bisppp_tls((struct sppp *)cookie);
}
#endif

/*---------------------------------------------------------------------------*
 *	PPP this-layer-started action
 *---------------------------------------------------------------------------*
 */
static void
i4bisppp_tls(struct sppp *sp)
{
	struct i4bisppp_softc *sc = (struct i4bisppp_softc *)sp;
	struct ifnet *ifp = (struct ifnet *)sp;

	if(sc->sc_state == ST_CONNECTED)
		return;

	i4b_l4_dialout(BDRV_ISPPP, IFP2UNIT(ifp));
}

/*---------------------------------------------------------------------------*
 *	PPP this-layer-finished action
 *---------------------------------------------------------------------------*
 */
static void
i4bisppp_tlf(struct sppp *sp)
{
	struct i4bisppp_softc *sc = (struct i4bisppp_softc *)sp;
/*	call_desc_t *cd = sc->sc_cdp;	*/
        struct ifnet *ifp = (struct ifnet *)sp;	
	
	if(sc->sc_state != ST_CONNECTED)
		return;

#if 0 /* never used ??? */
	UNTIMEOUT(i4bisppp_timeout, (void *)sp, sc->sc_ch);
#endif

	i4b_l4_drvrdisc(BDRV_ISPPP, IFP2UNIT(ifp));
}
/*---------------------------------------------------------------------------*
 *	PPP interface phase change
 *---------------------------------------------------------------------------*
 */
static void
i4bisppp_state_changed(struct sppp *sp, int new_state)
{
	struct i4bisppp_softc *sc = (struct i4bisppp_softc *)sp;
	
	i4b_l4_ifstate_changed(sc->sc_cdp, new_state);
}

/*---------------------------------------------------------------------------*
 *	PPP control protocol negotiation complete (run ip-up script now)
 *---------------------------------------------------------------------------*
 */
static void
i4bisppp_negotiation_complete(struct sppp *sp)
{
	struct i4bisppp_softc *sc = (struct i4bisppp_softc *)sp;
	
	i4b_l4_negcomplete(sc->sc_cdp);
}

/*===========================================================================*
 *			ISDN INTERFACE ROUTINES
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	this routine is called from L4 handler at connect time
 *---------------------------------------------------------------------------*/
static void
i4bisppp_connect(int unit, void *cdp)
{
	struct i4bisppp_softc *sc = &i4bisppp_softc[unit];
	struct sppp *sp = &sc->sc_if_un.scu_sp;
	int s = splimp();

	sc->sc_cdp = (call_desc_t *)cdp;
	sc->sc_state = ST_CONNECTED;

#if I4BISPPPACCT
	sc->sc_iinb = 0;
	sc->sc_ioutb = 0;
	sc->sc_inb = 0;
	sc->sc_outb = 0;
	sc->sc_linb = 0;
	sc->sc_loutb = 0;
	sc->sc_if.if_timer = I4BISPPPACCTINTVL;
#endif
	
#if 0 /* never used ??? */
	UNTIMEOUT(i4bisppp_timeout, (void *)sp, sc->sc_ch);
#endif

	sp->pp_up(sp);		/* tell PPP we are ready */
#ifndef __NetBSD__
	sp->pp_last_sent = sp->pp_last_recv = SECOND;
#endif
	splx(s);
}

/*---------------------------------------------------------------------------*
 *	this routine is called from L4 handler at disconnect time
 *---------------------------------------------------------------------------*/
static void
i4bisppp_disconnect(int unit, void *cdp)
{
	call_desc_t *cd = (call_desc_t *)cdp;
	struct i4bisppp_softc *sc = &i4bisppp_softc[unit];
	struct sppp *sp = &sc->sc_if_un.scu_sp;

	int s = splimp();

	/* new stuff to check that the active channel is being closed */
	if (cd != sc->sc_cdp)
	{
		NDBGL4(L4_ISPDBG, "isp%d, channel%d not active!", unit, cd->channelid);
		splx(s);
		return;
	}

#if I4BISPPPACCT
	sc->sc_if.if_timer = 0;
#endif

	i4b_l4_accounting(BDRV_ISPPP, unit, ACCT_FINAL,
		 sc->sc_ioutb, sc->sc_iinb, 0, 0, sc->sc_outb, sc->sc_inb);
	
	if (sc->sc_state == ST_CONNECTED)
	{
#if 0 /* never used ??? */
		UNTIMEOUT(i4bisppp_timeout, (void *)sp, sc->sc_ch);
#endif
		sc->sc_cdp = (call_desc_t *)0;	
		/* do thhis here because pp_down calls i4bisppp_tlf */
		sc->sc_state = ST_IDLE;
		sp->pp_down(sp);	/* tell PPP we have hung up */
	}

	splx(s);
}

/*---------------------------------------------------------------------------*
 *	this routine is used to give a feedback from userland demon
 *	in case of dial problems
 *---------------------------------------------------------------------------*/
static void
i4bisppp_dialresponse(int unit, int status, cause_t cause)
{
	struct i4bisppp_softc *sc = &i4bisppp_softc[unit];

	NDBGL4(L4_ISPDBG, "isp%d: status=%d, cause=%d", unit, status, cause);

	if(status != DSTAT_NONE)
	{
		struct mbuf *m;
		
		NDBGL4(L4_ISPDBG, "isp%d: clearing queues", unit);

#ifndef USE_ISPPP
		if(!(sppp_isempty(&sc->sc_if)))
#else
		if(!(isppp_isempty(&sc->sc_if)))
#endif		
		{
#ifndef USE_ISPPP
			while((m = sppp_dequeue(&sc->sc_if)) != NULL)
#else
			while((m = isppp_dequeue(&sc->sc_if)) != NULL)
#endif
				m_freem(m);
		}
	}
}
	
/*---------------------------------------------------------------------------*
 *	interface up/down
 *---------------------------------------------------------------------------*/
static void
i4bisppp_updown(int unit, int updown)
{
	/* could probably do something useful here */
}
	
/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	when a new frame (mbuf) has been received and was put on
 *	the rx queue.
 *---------------------------------------------------------------------------*/
static void
i4bisppp_rx_data_rdy(int unit)
{
	struct i4bisppp_softc *sc = &i4bisppp_softc[unit];
	struct mbuf *m;
	int s;
	
	if((m = *isdn_linktab[unit]->rx_mbuf) == NULL)
		return;

	m->m_pkthdr.rcvif = &sc->sc_if;
	m->m_pkthdr.len = m->m_len;

	microtime(&sc->sc_if.if_lastchange);

	sc->sc_if.if_ipackets++;
#if 0
	sc->sc_if.if_ibytes += m->m_pkthdr.len;
#endif

#if I4BISPPPACCT
	sc->sc_inb += m->m_pkthdr.len;
#endif
	
#ifdef I4BISPPPDEBUG
	printf("i4bisppp_rx_data_ready: received packet!\n");
#endif

#if NBPFILTER > 0 || NBPF > 0

#ifdef __FreeBSD__	
	if(sc->sc_if.if_bpf)
		bpf_mtap(&sc->sc_if, m);
#endif /* __FreeBSD__ */

#ifdef __NetBSD__
	if(sc->sc_if.if_bpf)
		bpf_mtap(sc->sc_if.if_bpf, m);
#endif

#endif /* NBPFILTER > 0  || NBPF > 0 */

	s = splimp();

#ifndef USE_ISPPP
	sppp_input(&sc->sc_if, m);
#else
	isppp_input(&sc->sc_if, m);
#endif

	splx(s);
}

/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	when the last frame has been sent out and there is no
 *	further frame (mbuf) in the tx queue.
 *---------------------------------------------------------------------------*/
static void
i4bisppp_tx_queue_empty(int unit)
{
	i4bisppp_start(&i4bisppp_softc[unit].sc_if);	
}

/*---------------------------------------------------------------------------*
 *	THIS should be used instead of last_active_time to implement
 *	an activity timeout mechanism.
 *
 *	Sending back the time difference unneccessarily complicates the
 *	idletime checks in i4b_l4.c. Return the largest time instead.
 *	That way the code in i4b_l4.c needs only minimal changes.
 *---------------------------------------------------------------------------*/
time_t
i4bisppp_idletime(int unit)
{
#ifdef __NetBSD__
       return(i4bisppp_softc[unit].sc_cdp->last_active_time);
#else
	struct sppp *sp;
	sp = (struct sppp *) &i4bisppp_softc[unit];

	return((sp->pp_last_recv < sp->pp_last_sent) ?
			sp->pp_last_sent : sp->pp_last_recv);
#endif
}

/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	each time a packet is received or transmitted. It should
 *	be used to implement an activity timeout mechanism.
 *---------------------------------------------------------------------------*/
static void
i4bisppp_activity(int unit, int rxtx)
{
	i4bisppp_softc[unit].sc_cdp->last_active_time = SECOND;
}

/*---------------------------------------------------------------------------*
 *	return this drivers linktab address
 *---------------------------------------------------------------------------*/
drvr_link_t *
i4bisppp_ret_linktab(int unit)
{
	return(&i4bisppp_drvr_linktab[unit]);
}

/*---------------------------------------------------------------------------*
 *	setup the isdn_linktab for this driver
 *---------------------------------------------------------------------------*/
void
i4bisppp_set_linktab(int unit, isdn_link_t *ilt)
{
	isdn_linktab[unit] = ilt;
}

/*---------------------------------------------------------------------------*
 *	initialize this drivers linktab
 *---------------------------------------------------------------------------*/
static void
i4bisppp_init_linktab(int unit)
{
	i4bisppp_drvr_linktab[unit].unit = unit;
	i4bisppp_drvr_linktab[unit].bch_rx_data_ready = i4bisppp_rx_data_rdy;
	i4bisppp_drvr_linktab[unit].bch_tx_queue_empty = i4bisppp_tx_queue_empty;
	i4bisppp_drvr_linktab[unit].bch_activity = i4bisppp_activity;
	i4bisppp_drvr_linktab[unit].line_connected = i4bisppp_connect;
	i4bisppp_drvr_linktab[unit].line_disconnected = i4bisppp_disconnect;
	i4bisppp_drvr_linktab[unit].dial_response = i4bisppp_dialresponse;	
	i4bisppp_drvr_linktab[unit].updown_ind = i4bisppp_updown;	
}

/*===========================================================================*/
