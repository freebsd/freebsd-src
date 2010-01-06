/*	$NetBSD: smc90cx6.c,v 1.38 2001/07/07 15:57:53 thorpej Exp $ */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 1994, 1995, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ignatios Souvatzis.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Chip core driver for the SMC90c26 / SMC90c56 (and SMC90c66 in '56
 * compatibility mode) boards
 */

/* #define CMSOFTCOPY */
#define CMRETRANSMIT /**/
/* #define CM_DEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_arc.h>

#include <dev/cm/smc90cx6reg.h>
#include <dev/cm/smc90cx6var.h>

MODULE_DEPEND(if_cm, arcnet, 1, 1, 1);

/* these should be elsewhere */

#define ARC_MIN_LEN 1
#define ARC_MIN_FORBID_LEN 254
#define ARC_MAX_FORBID_LEN 256
#define ARC_MAX_LEN 508
#define ARC_ADDR_LEN 1

/* for watchdog timer. This should be more than enough. */
#define ARCTIMEOUT (5*IFNET_SLOWHZ)

devclass_t cm_devclass;

/*
 * This currently uses 2 bufs for tx, 2 for rx
 *
 * New rx protocol:
 *
 * rx has a fillcount variable. If fillcount > (NRXBUF-1),
 * rx can be switched off from rx hard int.
 * Else rx is restarted on the other receiver.
 * rx soft int counts down. if it is == (NRXBUF-1), it restarts
 * the receiver.
 * To ensure packet ordering (we need that for 1201 later), we have a counter
 * which is incremented modulo 256 on each receive and a per buffer
 * variable, which is set to the counter on filling. The soft int can
 * compare both values to determine the older packet.
 *
 * Transmit direction:
 *
 * cm_start checks tx_fillcount
 * case 2: return
 *
 * else fill tx_act ^ 1 && inc tx_fillcount
 *
 * check tx_fillcount again.
 * case 2: set IFF_DRV_OACTIVE to stop arc_output from filling us.
 * case 1: start tx
 *
 * tint clears IFF_OACTIVE, decrements and checks tx_fillcount
 * case 1: start tx on tx_act ^ 1, softcall cm_start
 * case 0: softcall cm_start
 *
 * #define fill(i) get mbuf && copy mbuf to chip(i)
 */

void	cm_init(void *);
static void cm_init_locked(struct cm_softc *);
static void cm_reset_locked(struct cm_softc *);
void	cm_start(struct ifnet *);
void	cm_start_locked(struct ifnet *);
int	cm_ioctl(struct ifnet *, unsigned long, caddr_t);
void	cm_watchdog(void *);
void	cm_srint_locked(void *vsc);
static	void cm_tint_locked(struct cm_softc *, int);
void	cm_reconwatch_locked(void *);

/*
 * Release all resources
 */
void
cm_release_resources(dev)
	device_t dev;
{
	struct cm_softc *sc = device_get_softc(dev);

	if (sc->port_res != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT,
				     0, sc->port_res);
		sc->port_res = NULL;
	}
	if (sc->mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     0, sc->mem_res);
		sc->mem_res = NULL;
	}
	if (sc->irq_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ,
				     0, sc->irq_res);
		sc->irq_res = NULL;
	}
}

int
cm_attach(dev)
	device_t dev;
{
	struct cm_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;
	u_int8_t linkaddress;

	ifp = sc->sc_ifp = if_alloc(IFT_ARCNET);
	if (ifp == NULL)
		return (ENOSPC);

	/*
	 * read the arcnet address from the board
	 */
	GETREG(CMRESET);
	do {
		DELAY(200);
	} while (!(GETREG(CMSTAT) & CM_POR));
	linkaddress = GETMEM(CMMACOFF);

	/* clear the int mask... */
	sc->sc_intmask = 0;
	PUTREG(CMSTAT, 0);

	PUTREG(CMCMD, CM_CONF(CONF_LONG));
	PUTREG(CMCMD, CM_CLR(CLR_POR|CLR_RECONFIG));
	sc->sc_recontime = sc->sc_reconcount = 0;

	/*
	 * set interface to stopped condition (reset)
	 */
	cm_stop_locked(sc);

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_output = arc_output;
	ifp->if_start = cm_start;
	ifp->if_ioctl = cm_ioctl;
	ifp->if_init = cm_init;
	/* XXX IFQ_SET_READY(&ifp->if_snd); */
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX;

	arc_ifattach(ifp, linkaddress);

#ifdef CMSOFTCOPY
	sc->sc_rxcookie = softintr_establish(IPL_SOFTNET, cm_srint, sc);
	sc->sc_txcookie = softintr_establish(IPL_SOFTNET,
		(void (*)(void *))cm_start, ifp);
#endif

	callout_init_mtx(&sc->sc_recon_ch, &sc->sc_mtx, 0);
	callout_init_mtx(&sc->sc_watchdog_timer, &sc->sc_mtx, 0);

	if_printf(ifp, "link addr 0x%02x (%d)\n", linkaddress, linkaddress);
	return 0;
}

/*
 * Initialize device
 *
 */
void
cm_init(xsc)
	void *xsc;
{
	struct cm_softc *sc = (struct cm_softc *)xsc;

	CM_LOCK(sc);
	cm_init_locked(sc);
	CM_UNLOCK(sc);
}

static void
cm_init_locked(struct cm_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		ifp->if_drv_flags |= IFF_DRV_RUNNING;
		cm_reset_locked(sc);
	}
}

/*
 * Reset the interface...
 *
 * Assumes that it is called with sc_mtx held
 */
void
cm_reset_locked(sc)
	struct cm_softc *sc;
{
	struct ifnet *ifp;
	int linkaddress;

	ifp = sc->sc_ifp;

#ifdef CM_DEBUG
	if_printf(ifp, "reset\n");
#endif
	/* stop and restart hardware */

	GETREG(CMRESET);
	do {
		DELAY(200);
	} while (!(GETREG(CMSTAT) & CM_POR));

	linkaddress = GETMEM(CMMACOFF);

#if defined(CM_DEBUG) && (CM_DEBUG > 2)
	if_printf(ifp, "reset: card reset, link addr = 0x%02x (%d)\n",
	    linkaddress, linkaddress);
#endif

	/* tell the routing level about the (possibly changed) link address */
	arc_storelladdr(ifp, linkaddress);
	arc_frag_init(ifp);

	/* POR is NMI, but we need it below: */
	sc->sc_intmask = CM_RECON|CM_POR;
	PUTREG(CMSTAT, sc->sc_intmask);
	PUTREG(CMCMD, CM_CONF(CONF_LONG));

#ifdef CM_DEBUG
	if_printf(ifp, "reset: chip configured, status=0x%02x\n",
	    GETREG(CMSTAT));
#endif
	PUTREG(CMCMD, CM_CLR(CLR_POR|CLR_RECONFIG));

#ifdef CM_DEBUG
	if_printf(ifp, "reset: bits cleared, status=0x%02x\n",
	     GETREG(CMSTAT));
#endif

	sc->sc_reconcount_excessive = ARC_EXCESSIVE_RECONS;

	/* start receiver */

	sc->sc_intmask  |= CM_RI;
	sc->sc_rx_fillcount = 0;
	sc->sc_rx_act = 2;

	PUTREG(CMCMD, CM_RXBC(2));
	PUTREG(CMSTAT, sc->sc_intmask);

#ifdef CM_DEBUG
	if_printf(ifp, "reset: started receiver, status=0x%02x\n",
	    GETREG(CMSTAT));
#endif

	/* and init transmitter status */
	sc->sc_tx_act = 0;
	sc->sc_tx_fillcount = 0;

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_reset(&sc->sc_watchdog_timer, hz, cm_watchdog, sc);
	cm_start_locked(ifp);
}

/*
 * Take interface offline
 */
void
cm_stop_locked(sc)
	struct cm_softc *sc;
{
	/* Stop the interrupts */
	PUTREG(CMSTAT, 0);

	/* Stop the interface */
	GETREG(CMRESET);

	/* Stop watchdog timer */
	callout_stop(&sc->sc_watchdog_timer);
	sc->sc_timer = 0;
}

void
cm_start(struct ifnet *ifp)
{
	struct cm_softc *sc = ifp->if_softc;

	CM_LOCK(sc);
	cm_start_locked(ifp);
	CM_UNLOCK(sc);
}

/*
 * Start output on interface. Get another datagram to send
 * off the interface queue, and copy it to the
 * interface becore starting the output
 *
 * Assumes that sc_mtx is held
 */
void
cm_start_locked(ifp)
	struct ifnet *ifp;
{
	struct cm_softc *sc = ifp->if_softc;
	struct mbuf *m, *mp;

	int cm_ram_ptr;
	int len, tlen, offset, buffer;
#ifdef CMTIMINGS
	u_long copystart, lencopy, perbyte;
#endif

#if defined(CM_DEBUG) && (CM_DEBUG > 3)
	if_printf(ifp, "start(%p)\n", ifp);
#endif

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	if (sc->sc_tx_fillcount >= 2)
		return;

	m = arc_frag_next(ifp);
	buffer = sc->sc_tx_act ^ 1;

	if (m == 0)
		return;

#ifdef CM_DEBUG
	if (m->m_len < ARC_HDRLEN)
		m = m_pullup(m, ARC_HDRLEN);/* gcc does structure padding */
	if_printf(ifp, "start: filling %d from %d to %d type %d\n",
	    buffer, mtod(m, u_char *)[0],
	    mtod(m, u_char *)[1], mtod(m, u_char *)[2]);
#else
	if (m->m_len < 2)
		m = m_pullup(m, 2);
#endif
	cm_ram_ptr = buffer * 512;

	if (m == 0)
		return;

	/* write the addresses to RAM and throw them away */

	/*
	 * Hardware does this: Yet Another Microsecond Saved.
	 * (btw, timing code says usually 2 microseconds)
	 * PUTMEM(cm_ram_ptr + 0, mtod(m, u_char *)[0]);
	 */

	PUTMEM(cm_ram_ptr + 1, mtod(m, u_char *)[1]);
	m_adj(m, 2);

	/* get total length left at this point */
	tlen = m->m_pkthdr.len;
	if (tlen < ARC_MIN_FORBID_LEN) {
		offset = 256 - tlen;
		PUTMEM(cm_ram_ptr + 2, offset);
	} else {
		PUTMEM(cm_ram_ptr + 2, 0);
		if (tlen <= ARC_MAX_FORBID_LEN)
			offset = 255;		/* !!! */
		else {
			if (tlen > ARC_MAX_LEN)
				tlen = ARC_MAX_LEN;
			offset = 512 - tlen;
		}
		PUTMEM(cm_ram_ptr + 3, offset);

	}
	cm_ram_ptr += offset;

	/* lets loop through the mbuf chain */

	for (mp = m; mp; mp = mp->m_next) {
		if ((len = mp->m_len)) {		/* YAMS */
			bus_space_write_region_1(
			    rman_get_bustag(sc->mem_res),
			    rman_get_bushandle(sc->mem_res),
			    cm_ram_ptr, mtod(mp, caddr_t), len);

			cm_ram_ptr += len;
		}
	}

	sc->sc_broadcast[buffer] = (m->m_flags & M_BCAST) != 0;
	sc->sc_retransmits[buffer] = (m->m_flags & M_BCAST) ? 1 : 5;

	if (++sc->sc_tx_fillcount > 1) {
		/*
		 * We are filled up to the rim. No more bufs for the moment,
		 * please.
		 */
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	} else {
#ifdef CM_DEBUG
		if_printf(ifp, "start: starting transmitter on buffer %d\n",
		    buffer);
#endif
		/* Transmitter was off, start it */
		sc->sc_tx_act = buffer;

		/*
		 * We still can accept another buf, so don't:
		 * ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		 */
		sc->sc_intmask |= CM_TA;
		PUTREG(CMCMD, CM_TX(buffer));
		PUTREG(CMSTAT, sc->sc_intmask);

		sc->sc_timer = ARCTIMEOUT;
	}
	m_freem(m);

	/*
	 * After 10 times reading the docs, I realized
	 * that in the case the receiver NAKs the buffer request,
	 * the hardware retries till shutdown.
	 * This is integrated now in the code above.
	 */
}

#ifdef CMSOFTCOPY
void
cm_srint(void *vsc)
{
	struct cm_softc *sc = (struct cm_softc *)vsc;

	CM_LOCK(sc);
	cm_srint_locked(vsc);
	CM_UNLOCK(sc);
}
#endif

/*
 * Arcnet interface receiver soft interrupt:
 * get the stuff out of any filled buffer we find.
 */
void
cm_srint_locked(vsc)
	void *vsc;
{
	struct cm_softc *sc = (struct cm_softc *)vsc;
	int buffer, len, offset, type;
	int cm_ram_ptr;
	struct mbuf *m;
	struct arc_header *ah;
	struct ifnet *ifp;

	ifp = sc->sc_ifp;

	buffer = sc->sc_rx_act ^ 1;

	/* Allocate header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);

	if (m == 0) {
		/*
		 * in case s.th. goes wrong with mem, drop it
		 * to make sure the receiver can be started again
		 * count it as input error (we dont have any other
		 * detectable)
		 */
		ifp->if_ierrors++;
		goto cleanup;
	}

	m->m_pkthdr.rcvif = ifp;

	/*
	 * Align so that IP packet will be longword aligned. Here we
	 * assume that m_data of new packet is longword aligned.
	 * When implementing PHDS, we might have to change it to 2,
	 * (2*sizeof(ulong) - CM_HDRNEWLEN)), packet type dependent.
	 */

	cm_ram_ptr = buffer * 512;
	offset = GETMEM(cm_ram_ptr + 2);
	if (offset)
		len = 256 - offset;
	else {
		offset = GETMEM(cm_ram_ptr + 3);
		len = 512 - offset;
	}

	/*
	 * first +2 bytes for align fixup below
	 * second +2 bytes are for src/dst addresses
	 */
	if ((len + 2 + 2) > MHLEN) {
		/* attach an mbuf cluster */
		MCLGET(m, M_DONTWAIT);

		/* Insist on getting a cluster */
		if ((m->m_flags & M_EXT) == 0) {
			ifp->if_ierrors++;
			goto cleanup;
		}
	}

	if (m == 0) {
		ifp->if_ierrors++;
		goto cleanup;
	}

	type = GETMEM(cm_ram_ptr + offset);
	m->m_data += 1 + arc_isphds(type);
	/* mbuf filled with ARCnet addresses */
	m->m_pkthdr.len = m->m_len = len + 2;

	ah = mtod(m, struct arc_header *);
	ah->arc_shost = GETMEM(cm_ram_ptr + 0);
	ah->arc_dhost = GETMEM(cm_ram_ptr + 1);

	bus_space_read_region_1(
	    rman_get_bustag(sc->mem_res), rman_get_bushandle(sc->mem_res),
	    cm_ram_ptr + offset, mtod(m, u_char *) + 2, len);

	CM_UNLOCK(sc);
	arc_input(ifp, m);
	CM_LOCK(sc);

	m = NULL;
	ifp->if_ipackets++;

cleanup:

	if (m != NULL)
		m_freem(m);

	/* mark buffer as invalid by source id 0 */
	PUTMEM(buffer << 9, 0);
	if (--sc->sc_rx_fillcount == 2 - 1) {

		/* was off, restart it on buffer just emptied */
		sc->sc_rx_act = buffer;
		sc->sc_intmask |= CM_RI;

		/* this also clears the RI flag interrupt: */
		PUTREG(CMCMD, CM_RXBC(buffer));
		PUTREG(CMSTAT, sc->sc_intmask);

#ifdef CM_DEBUG
		if_printf(ifp, "srint: restarted rx on buf %d\n", buffer);
#endif
	}
}

__inline static void
cm_tint_locked(sc, isr)
	struct cm_softc *sc;
	int isr;
{
	struct ifnet *ifp;

	int buffer;
#ifdef CMTIMINGS
	int clknow;
#endif

	ifp = sc->sc_ifp;
	buffer = sc->sc_tx_act;

	/*
	 * retransmit code:
	 * Normal situtations first for fast path:
	 * If acknowledgement received ok or broadcast, we're ok.
	 * else if
	 */

	if (isr & CM_TMA || sc->sc_broadcast[buffer])
		ifp->if_opackets++;
#ifdef CMRETRANSMIT
	else if (ifp->if_flags & IFF_LINK2 && sc->sc_timer > 0
	    && --sc->sc_retransmits[buffer] > 0) {
		/* retransmit same buffer */
		PUTREG(CMCMD, CM_TX(buffer));
		return;
	}
#endif
	else
		ifp->if_oerrors++;


	/* We know we can accept another buffer at this point. */
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	if (--sc->sc_tx_fillcount > 0) {

		/*
		 * start tx on other buffer.
		 * This also clears the int flag
		 */
		buffer ^= 1;
		sc->sc_tx_act = buffer;

		/*
		 * already given:
		 * sc->sc_intmask |= CM_TA;
		 * PUTREG(CMSTAT, sc->sc_intmask);
		 */
		PUTREG(CMCMD, CM_TX(buffer));
		/* init watchdog timer */
		sc->sc_timer = ARCTIMEOUT;

#if defined(CM_DEBUG) && (CM_DEBUG > 1)
		if_printf(ifp,
		    "tint: starting tx on buffer %d, status 0x%02x\n",
		    buffer, GETREG(CMSTAT));
#endif
	} else {
		/* have to disable TX interrupt */
		sc->sc_intmask &= ~CM_TA;
		PUTREG(CMSTAT, sc->sc_intmask);
		/* ... and watchdog timer */
		sc->sc_timer = 0;

#ifdef CM_DEBUG
		if_printf(ifp, "tint: no more buffers to send, status 0x%02x\n",
		    GETREG(CMSTAT));
#endif
	}

	/* XXXX TODO */
#ifdef CMSOFTCOPY
	/* schedule soft int to fill a new buffer for us */
	softintr_schedule(sc->sc_txcookie);
#else
	/* call it directly */
	cm_start_locked(ifp);
#endif
}

/*
 * Our interrupt routine
 */
void
cmintr(arg)
	void *arg;
{
	struct cm_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	u_char isr, maskedisr;
	int buffer;
	u_long newsec;

	CM_LOCK(sc);

	isr = GETREG(CMSTAT);
	maskedisr = isr & sc->sc_intmask;
	if (!maskedisr || (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		CM_UNLOCK(sc);
		return;
	}

	do {

#if defined(CM_DEBUG) && (CM_DEBUG > 1)
		if_printf(ifp, "intr: status 0x%02x, intmask 0x%02x\n",
		    isr, sc->sc_intmask);
#endif

		if (maskedisr & CM_POR) {
			/*
			 * XXX We should never see this. Don't bother to store
			 * the address.
			 * sc->sc_ifp->if_l2com->ac_anaddr = GETMEM(CMMACOFF);
			 */
			PUTREG(CMCMD, CM_CLR(CLR_POR));
			log(LOG_WARNING,
			    "%s: intr: got spurious power on reset int\n",
			    ifp->if_xname);
		}

		if (maskedisr & CM_RECON) {
			/*
			 * we dont need to:
			 * PUTREG(CMCMD, CM_CONF(CONF_LONG));
			 */
			PUTREG(CMCMD, CM_CLR(CLR_RECONFIG));
			ifp->if_collisions++;

			/*
			 * If less than 2 seconds per reconfig:
			 *	If ARC_EXCESSIVE_RECONFIGS
			 *	since last burst, complain and set treshold for
			 *	warnings to ARC_EXCESSIVE_RECONS_REWARN.
			 *
			 * This allows for, e.g., new stations on the cable, or
			 * cable switching as long as it is over after
			 * (normally) 16 seconds.
			 *
			 * XXX TODO: check timeout bits in status word and
			 * double time if necessary.
			 */

			callout_stop(&sc->sc_recon_ch);
			newsec = time_second;
			if ((newsec - sc->sc_recontime <= 2) &&
			    (++sc->sc_reconcount == ARC_EXCESSIVE_RECONS)) {
				log(LOG_WARNING,
				    "%s: excessive token losses, "
				    "cable problem?\n",
				    ifp->if_xname);
			}
			sc->sc_recontime = newsec;
			callout_reset(&sc->sc_recon_ch, 15 * hz,
			    cm_reconwatch_locked, (void *)sc);
		}

		if (maskedisr & CM_RI) {
#if defined(CM_DEBUG) && (CM_DEBUG > 1)
			if_printf(ifp, "intr: hard rint, act %d\n",
			    sc->sc_rx_act);
#endif

			buffer = sc->sc_rx_act;
			/* look if buffer is marked invalid: */
			if (GETMEM(buffer * 512) == 0) {
				/*
				 * invalid marked buffer (or illegally
				 * configured sender)
				 */
				log(LOG_WARNING,
				    "%s: spurious RX interrupt or sender 0 "
				    " (ignored)\n", ifp->if_xname);
				/*
				 * restart receiver on same buffer.
				 * XXX maybe better reset interface?
				 */
				PUTREG(CMCMD, CM_RXBC(buffer));
			} else {
				if (++sc->sc_rx_fillcount > 1) {
					sc->sc_intmask &= ~CM_RI;
					PUTREG(CMSTAT, sc->sc_intmask);
				} else {
					buffer ^= 1;
					sc->sc_rx_act = buffer;

					/*
					 * Start receiver on other receive
					 * buffer. This also clears the RI
					 * interrupt flag.
					 */
					PUTREG(CMCMD, CM_RXBC(buffer));
					/* in RX intr, so mask is ok for RX */

#ifdef CM_DEBUG
					if_printf(ifp, "strt rx for buf %d, "
					    "stat 0x%02x\n",
					    sc->sc_rx_act, GETREG(CMSTAT));
#endif
				}

#ifdef CMSOFTCOPY
				/*
				 * this one starts a soft int to copy out
				 * of the hw
				 */
				softintr_schedule(sc->sc_rxcookie);
#else
				/* this one does the copy here */
				cm_srint_locked(sc);
#endif
			}
		}
		if (maskedisr & CM_TA) {
			cm_tint_locked(sc, isr);
		}
		isr = GETREG(CMSTAT);
		maskedisr = isr & sc->sc_intmask;
	} while (maskedisr);
#if defined(CM_DEBUG) && (CM_DEBUG > 1)
	if_printf(ifp, "intr (exit): status 0x%02x, intmask 0x%02x\n",
	    isr, sc->sc_intmask);
#endif
	CM_UNLOCK(sc);
}

void
cm_reconwatch_locked(arg)
	void *arg;
{
	struct cm_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	if (sc->sc_reconcount >= ARC_EXCESSIVE_RECONS) {
		sc->sc_reconcount = 0;
		log(LOG_WARNING, "%s: token valid again.\n",
		    ifp->if_xname);
	}
	sc->sc_reconcount = 0;
}


/*
 * Process an ioctl request.
 * This code needs some work - it looks pretty ugly.
 */
int
cm_ioctl(ifp, command, data)
	struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct cm_softc *sc;
	int error;

	error = 0;
	sc = ifp->if_softc;

#if defined(CM_DEBUG) && (CM_DEBUG > 2)
	if_printf(ifp, "ioctl() called, cmd = 0x%lx\n", command);
#endif

	switch (command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCSIFMTU:
		error = arc_ioctl(ifp, command, data);
		break;

	case SIOCSIFFLAGS:
		CM_LOCK(sc);
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running,
			 * then stop it.
			 */
			cm_stop_locked(sc);
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			cm_init_locked(sc);
		}
		CM_UNLOCK(sc);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * watchdog routine for transmitter.
 *
 * We need this, because else a receiver whose hardware is alive, but whose
 * software has not enabled the Receiver, would make our hardware wait forever
 * Discovered this after 20 times reading the docs.
 *
 * Only thing we do is disable transmitter. We'll get a transmit timeout,
 * and the int handler will have to decide not to retransmit (in case
 * retransmission is implemented).
 */
void
cm_watchdog(void *arg)
{
	struct cm_softc *sc;

	sc = arg;
	callout_reset(&sc->sc_watchdog_timer, hz, cm_watchdog, sc);
	if (sc->sc_timer == 0 || --sc->sc_timer > 0)
		return;
	PUTREG(CMCMD, CM_TXDIS);
}
