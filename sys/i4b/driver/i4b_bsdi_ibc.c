/*
 *   Copyright (c) 1998, 1999 Bert Driehuis. All rights reserved.
 *
 *   Copyright (c) 1997, 1998 Hellmuth Michaelis. All rights reserved.
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
 *	i4b_bsdi_ibc.c - isdn4bsd kernel BSD/OS point to point driver
 *	-------------------------------------------------------------
 *
 *	$Id: i4b_bsdi_ibc.c,v 1.1 1999/04/23 08:35:07 hm Exp $
 *
 * $FreeBSD: src/sys/i4b/driver/i4b_bsdi_ibc.c,v 1.3 1999/12/14 20:48:12 hm Exp $
 *
 *	last edit-date: [Tue Dec 14 21:55:37 1999]
 *
 *---------------------------------------------------------------------------*/

#include "ibc.h"

#if NIBC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/ioccom.h>
#include <sys/ttycom.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/protosw.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_p2p.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <i4b/i4b_ioctl.h>
#include <i4b/i4b_cause.h>
#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/layer4/i4b_l4.h>

#define IFP2UNIT(ifp)	(ifp)->if_unit

#define IOCTL_CMD_T u_long

void ibcattach(void *);

#define IBCACCT		1	/* enable accounting messages */
#define	IBCACCTINTVL	2	/* accounting msg interval in secs */

#define PPP_HDRLEN   		4	/* 4 octets PPP header length	*/

struct ibc_softc {
	struct p2pcom sc_p2pcom;

	int	sc_state;	/* state of the interface	*/
	call_desc_t *sc_cdp;	/* ptr to call descriptor	*/

#ifdef IBCACCT
	int sc_iinb;		/* isdn driver # of inbytes	*/
	int sc_ioutb;		/* isdn driver # of outbytes	*/
	int sc_inb;		/* # of bytes rx'd		*/
	int sc_outb;		/* # of bytes tx'd	 	*/
	int sc_linb;		/* last # of bytes rx'd		*/
	int sc_loutb;		/* last # of bytes tx'd 	*/
	int sc_fn;		/* flag, first null acct	*/
#endif
} ibc_softc[NIBC];

static void	ibc_init_linktab(int unit);

static int	ibc_start(struct ifnet *ifp);

static int	ibc_watchdog(int unit);
static int	ibc_mdmctl(struct p2pcom *pp, int flag);
static int	ibc_getmdm(struct p2pcom *pp, caddr_t arg);

/* initialized by L4 */

static drvr_link_t ibc_drvr_linktab[NIBC];
static isdn_link_t *isdn_ibc_lt[NIBC];

enum ibc_states {
	ST_IDLE,			/* initialized, ready, idle	*/
	ST_DIALING,			/* dialling out to remote	*/
	ST_CONNECTED,			/* connected to remote		*/
};

int ibcdebug = 0;	/* Use bpatch to set this for debug printf's */
#define DBG(x)	if (ibcdebug) printf x

/*===========================================================================*
 *			DEVICE DRIVER ROUTINES
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	interface attach routine at kernel boot time
 *---------------------------------------------------------------------------*/
void
ibcattach(void *dummy)
{
	struct ibc_softc *sc = ibc_softc;
	struct ifnet *ifp;
	int i;

#ifndef HACK_NO_PSEUDO_ATTACH_MSG
	printf("ibc: %d ISDN ibc device(s) attached\n",
	       NIBC);
#endif

	for(i = 0; i < NIBC; sc++, i++) {
		ibc_init_linktab(i);

		sc->sc_p2pcom.p2p_mdmctl = ibc_mdmctl;
		sc->sc_p2pcom.p2p_getmdm = ibc_getmdm;
		sc->sc_state = ST_IDLE;
		ifp = &sc->sc_p2pcom.p2p_if;
		ifp->if_name = "ibc";
		ifp->if_next = NULL;
		ifp->if_unit = i;
		ifp->if_mtu = 1500 /*XXX*/;
		ifp->if_baudrate = 64000;
		ifp->if_flags = IFF_SIMPLEX | IFF_POINTOPOINT;
		ifp->if_type = IFT_ISDNBASIC;
		ifp->if_start = ibc_start;
		ifp->if_output = 0;
		ifp->if_ioctl = p2p_ioctl;

		ifp->if_hdrlen = 0;
		ifp->if_addrlen = 0;
		ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

		ifp->if_ipackets = 0;
		ifp->if_ierrors = 0;
		ifp->if_opackets = 0;
		ifp->if_oerrors = 0;
		ifp->if_collisions = 0;
		ifp->if_ibytes = 0;
		ifp->if_obytes = 0;
		ifp->if_imcasts = 0;
		ifp->if_omcasts = 0;
		ifp->if_iqdrops = 0;
		ifp->if_noproto = 0;
#if IBCACCT
		ifp->if_timer = 0;
		ifp->if_watchdog = ibc_watchdog;
		sc->sc_iinb = 0;
		sc->sc_ioutb = 0;
		sc->sc_inb = 0;
		sc->sc_outb = 0;
		sc->sc_linb = 0;
		sc->sc_loutb = 0;
		sc->sc_fn = 1;
#endif
		if_attach(ifp);
		p2p_attach(&sc->sc_p2pcom);
	}
}

static struct mbuf *
p2p_dequeue(struct p2pcom *pp)
{
	struct ifqueue *ifq;
	struct mbuf *m;

	ifq = &pp->p2p_isnd;
	m = ifq->ifq_head;
	if (m == 0) {
		ifq = &pp->p2p_if.if_snd;
		m = ifq->ifq_head;
	}
	if (m == 0)
		return 0;
	IF_DEQUEUE(ifq, m);
	return m;
}

/*---------------------------------------------------------------------------*
 *	start output to ISDN B-channel
 *---------------------------------------------------------------------------*/
static int
ibc_start(struct ifnet *ifp)
{
	int unit = IFP2UNIT(ifp);
	struct ibc_softc *sc = (struct ibc_softc *)&ibc_softc[unit];
	struct p2pcom *pp = &sc->sc_p2pcom;
	struct mbuf *m;
	int s;

	if(sc->sc_state != ST_CONNECTED) {
		DBG(("ibc%d: ibc_start called with sc_state=%d\n",
			unit, sc->sc_state));
		return 0;
	}

	s = SPLI4B();

	if (IF_QFULL(isdn_ibc_lt[unit]->tx_queue)) {
		splx(s);
		return 0;
	}

	m = p2p_dequeue(pp);
	if (m == NULL) {
		splx(s);
		return 0;
	}

	do {
		microtime(&ifp->if_lastchange);

		IF_ENQUEUE(isdn_ibc_lt[unit]->tx_queue, m);

		ifp->if_obytes += m->m_pkthdr.len;
		sc->sc_outb += m->m_pkthdr.len;
		ifp->if_opackets++;
	} while (!IF_QFULL(isdn_ibc_lt[unit]->tx_queue) &&
					(m = p2p_dequeue(pp)) != NULL);
	isdn_ibc_lt[unit]->bch_tx_start(isdn_ibc_lt[unit]->unit,
					 isdn_ibc_lt[unit]->channel);
	splx(s);
	return 0;
}

#ifdef IBCACCT
/*---------------------------------------------------------------------------*
 *	watchdog routine
 *---------------------------------------------------------------------------*/
static int
ibc_watchdog(int unit)
{
	struct ibc_softc *sc = &ibc_softc[unit];
	struct ifnet *ifp = &sc->sc_p2pcom.p2p_if;
	bchan_statistics_t bs;

	(*isdn_ibc_lt[unit]->bch_stat)
		(isdn_ibc_lt[unit]->unit, isdn_ibc_lt[unit]->channel, &bs);

	sc->sc_ioutb += bs.outbytes;
	sc->sc_iinb += bs.inbytes;

	if((sc->sc_iinb != sc->sc_linb) || (sc->sc_ioutb != sc->sc_loutb) || sc->sc_fn)
	{
		int ri = (sc->sc_iinb - sc->sc_linb)/IBCACCTINTVL;
		int ro = (sc->sc_ioutb - sc->sc_loutb)/IBCACCTINTVL;

		if((sc->sc_iinb == sc->sc_linb) && (sc->sc_ioutb == sc->sc_loutb))
			sc->sc_fn = 0;
		else
			sc->sc_fn = 1;

		sc->sc_linb = sc->sc_iinb;
		sc->sc_loutb = sc->sc_ioutb;

		i4b_l4_accounting(BDRV_IBC, unit, ACCT_DURING,
			 sc->sc_ioutb, sc->sc_iinb, ro, ri, sc->sc_outb, sc->sc_inb);
 	}
	ifp->if_timer = IBCACCTINTVL;
	return 0;
}
#endif /* IBCACCT */

/*
 *===========================================================================*
 *			P2P layer interface routines
 *===========================================================================*
 */

#if 0
/*---------------------------------------------------------------------------*
 *	PPP interface phase change
 *---------------------------------------------------------------------------*
 */
static void
ibc_state_changed(struct sppp *sp, int new_state)
{
	struct ibc_softc *sc = (struct ibc_softc *)sp;

	i4b_l4_ifstate_changed(sc->sc_cdp, new_state);
}

/*---------------------------------------------------------------------------*
 *	PPP control protocol negotiation complete (run ip-up script now)
 *---------------------------------------------------------------------------*
 */
static void
ibc_negotiation_complete(struct sppp *sp)
{
	struct ibc_softc *sc = (struct ibc_softc *)sp;

	i4b_l4_negcomplete(sc->sc_cdp);
}
#endif

/*===========================================================================*
 *			ISDN INTERFACE ROUTINES
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	this routine is called from L4 handler at connect time
 *---------------------------------------------------------------------------*/
static void
ibc_connect(int unit, void *cdp)
{
	struct ibc_softc *sc = &ibc_softc[unit];
	struct ifnet *ifp = &sc->sc_p2pcom.p2p_if;
	int s;

	DBG(("ibc%d: ibc_connect\n", unit));

	s = splimp();

	sc->sc_cdp = (call_desc_t *)cdp;
	sc->sc_state = ST_CONNECTED;

#if IBCACCT
	sc->sc_iinb = 0;
	sc->sc_ioutb = 0;
	sc->sc_inb = 0;
	sc->sc_outb = 0;
	sc->sc_linb = 0;
	sc->sc_loutb = 0;
	ifp->if_timer = IBCACCTINTVL;
#endif

	splx(s);
	if (sc->sc_p2pcom.p2p_modem)
		(*sc->sc_p2pcom.p2p_modem)(&sc->sc_p2pcom, 1);

		/* This is a lie... PPP is just starting to negociate :-) */
	i4b_l4_negcomplete(sc->sc_cdp);
}

/*---------------------------------------------------------------------------*
 *	this routine is called from L4 handler at disconnect time
 *---------------------------------------------------------------------------*/
static void
ibc_disconnect(int unit, void *cdp)
{
	call_desc_t *cd = (call_desc_t *)cdp;
	struct ibc_softc *sc = &ibc_softc[unit];
	struct ifnet *ifp = &sc->sc_p2pcom.p2p_if;
	int s;

	DBG(("ibc%d: ibc_disconnect\n", unit));

	s = splimp();

	/* new stuff to check that the active channel is being closed */
	if (cd != sc->sc_cdp)
	{
		DBG(("ibc_disconnect: ibc%d channel%d not active\n",
			cd->driver_unit, cd->channelid));
		splx(s);
		return;
	}

#if IBCACCT
	ifp->if_timer = 0;
#endif

	i4b_l4_accounting(BDRV_IBC, unit, ACCT_FINAL,
		 sc->sc_ioutb, sc->sc_iinb, 0, 0, sc->sc_outb, sc->sc_inb);

	if (sc->sc_state == ST_CONNECTED)
	{
		sc->sc_cdp = (call_desc_t *)0;
		sc->sc_state = ST_IDLE;
		if (sc->sc_p2pcom.p2p_modem)
			(*sc->sc_p2pcom.p2p_modem)(&sc->sc_p2pcom, 0);
	}

	splx(s);
}

/*---------------------------------------------------------------------------*
 *	this routine is used to give a feedback from userland demon
 *	in case of dial problems
 *---------------------------------------------------------------------------*/
static void
ibc_dialresponse(int unit, int status)
{
	DBG(("ibc%d: ibc_dialresponse %d\n", unit, status));
/*	struct ibc_softc *sc = &ibc_softc[unit];	*/
}

/*---------------------------------------------------------------------------*
 *	interface up/down
 *---------------------------------------------------------------------------*/
static void
ibc_updown(int unit, int updown)
{
	DBG(("ibc%d: ibc_updown %d\n", unit, updown));
	/* could probably do something useful here */
}

/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	when a new frame (mbuf) has been received and was put on
 *	the rx queue.
 *---------------------------------------------------------------------------*/
static void
ibc_rx_data_rdy(int unit)
{
	struct ibc_softc *sc = &ibc_softc[unit];
	struct ifnet *ifp = &sc->sc_p2pcom.p2p_if;
	struct mbuf *m, *m0;
	char *buf;
	int s;

	if((m = *isdn_ibc_lt[unit]->rx_mbuf) == NULL)
		return;

	microtime(&ifp->if_lastchange);
	ifp->if_ipackets++;

		/* Walk the mbuf chain */
	s = splimp();
	for (m0 = m; m != 0; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		ifp->if_ibytes += m->m_len;
#if IBCACCT
		sc->sc_inb += m->m_len;
#endif
		buf = mtod(m, caddr_t);
		if ((*sc->sc_p2pcom.p2p_hdrinput)(
					&sc->sc_p2pcom, buf, m->m_len) >= 0)
			(*sc->sc_p2pcom.p2p_input)(&sc->sc_p2pcom, 0);
	}
	splx(s);
	m_freem(m0);
}

/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	when the last frame has been sent out and there is no
 *	further frame (mbuf) in the tx queue.
 *---------------------------------------------------------------------------*/
static void
ibc_tx_queue_empty(int unit)
{
	ibc_start(&ibc_softc[unit].sc_p2pcom.p2p_if);
}

/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	each time a packet is received or transmitted. It should
 *	be used to implement an activity timeout mechanism.
 *---------------------------------------------------------------------------*/
static void
ibc_activity(int unit, int rxtx)
{
	ibc_softc[unit].sc_cdp->last_active_time = SECOND;
}

/*---------------------------------------------------------------------------*
 *	return this drivers linktab address
 *---------------------------------------------------------------------------*/
drvr_link_t *
ibc_ret_linktab(int unit)
{
	return(&ibc_drvr_linktab[unit]);
}

/*---------------------------------------------------------------------------*
 *	setup the isdn_ibc_lt for this driver
 *---------------------------------------------------------------------------*/
void
ibc_set_linktab(int unit, isdn_link_t *ilt)
{
	isdn_ibc_lt[unit] = ilt;
}

/*---------------------------------------------------------------------------*
 *	initialize this drivers linktab
 *---------------------------------------------------------------------------*/
static void
ibc_init_linktab(int unit)
{
	ibc_drvr_linktab[unit].unit = unit;
	ibc_drvr_linktab[unit].bch_rx_data_ready = ibc_rx_data_rdy;
	ibc_drvr_linktab[unit].bch_tx_queue_empty = ibc_tx_queue_empty;
	ibc_drvr_linktab[unit].bch_activity = ibc_activity;
	ibc_drvr_linktab[unit].line_connected = ibc_connect;
	ibc_drvr_linktab[unit].line_disconnected = ibc_disconnect;
	ibc_drvr_linktab[unit].dial_response = ibc_dialresponse;
	ibc_drvr_linktab[unit].updown_ind = ibc_updown;
}

/*===========================================================================*/

static int
ibc_mdmctl(pp, flag)
	struct p2pcom *pp;
	int flag;
{
	register struct ifnet *ifp = &pp->p2p_if;
	struct ibc_softc *sc = (struct ibc_softc *)&ibc_softc[ifp->if_unit];

	DBG(("ibc%d: ibc_mdmctl called flags=%d\n", IFP2UNIT(ifp), flag));

	if (flag == 1 && sc->sc_state == ST_IDLE) {
		sc->sc_state = ST_DIALING;
		i4b_l4_dialout(BDRV_IBC, IFP2UNIT(ifp));
	} else if (flag == 0 && sc->sc_state != ST_IDLE) {
		sc->sc_state = ST_IDLE;
		i4b_l4_drvrdisc(BDRV_IBC, IFP2UNIT(ifp));
	}
	return 0;
}

static int
ibc_getmdm(pp, arg)
	struct p2pcom *pp;
	caddr_t arg;
{
	register struct ifnet *ifp = &pp->p2p_if;
	struct ibc_softc *sc = (struct ibc_softc *)&ibc_softc[ifp->if_unit];

	if (sc->sc_state == ST_CONNECTED)
		*(int *)arg = TIOCM_CAR;
	else
		*(int *)arg = 0;
	return 0;

	DBG(("ibc%d: ibc_getmdm called ret=%d\n", IFP2UNIT(ifp), *(int *)arg));
}
#endif
