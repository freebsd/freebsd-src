/*
 * This code is based on
 *  (1) FreeBSD implementation on ISA/EISA Ethelink III by Herb Peyerl
 *  (2) Linux implementation on PCMCIA Etherlink III by Devid Hinds
 *  (3) FreeBSD implementation on PCMCIA IBM Ethernet Card I/II
 *      by David Greenman
 *  (4) RT-Mach implementation on PCMCIA/ISA/EISA Etherlink III
 *      by Seiji Murata
 *
 *  Copyright (c) by HOSOKAWA, Tatsumi <hosokawa@mt.cs.keio.ac.jp>
 *  Copyright (c) by Seiji Murata <seiji@mt.cs.keio.ac.jp>
 */
/*
 * Copyright (c) 1993 Herb Peyerl <hpeyerl@novatel.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *	From: if_ep.c,v 1.9 1994/01/25 10:46:29 deraadt Exp $
 *	$Id: if_zp.c,v 1.40 1997/10/26 04:36:14 nate Exp $
 */
/*-
 * TODO:
 * [1] integrate into current if_ed.c
 * [2] parse tuples to find out where to map the shared memory buffer,
 *     and what to write into the configuration register
 * [3] move pcic-specific code into a separate module.
 *
 * Device driver for IBM PCMCIA Credit Card Adapter for Ethernet,
 * if_ze.c
 *
 * Based on the Device driver for National Semiconductor DS8390 ethernet
 * adapters by David Greenman.  Modifications for PCMCIA by Keith Moore.
 * Adapted for FreeBSD 1.1.5 by Jordan Hubbard.
 *
 * Currently supports only the IBM Credit Card Adapter for Ethernet, but
 * could probably work with other PCMCIA cards also, if it were modified
 * to get the locations of the PCMCIA configuration option register (COR)
 * by parsing the configuration tuples, rather than by hard-coding in
 * the value expected by IBM's card.
 *
 * Sources for data on the PCMCIA/IBM CCAE specific portions of the driver:
 *
 * [1] _Local Area Network Credit Card Adapters Technical Reference_,
 *     IBM Corp., SC30-3585-00, part # 33G9243.
 * [2] "pre-alpha" PCMCIA support code for Linux by Barry Jaspan.
 * [3] Intel 82536SL PC Card Interface Controller Data Sheet, Intel
 *     Order Number 290423-002
 * [4] National Semiconductor DP83902A ST-NIC (tm) Serial Network
 *     Interface Controller for Twisted Pair data sheet.
 *
 *
 * Copyright (C) 1993, David Greenman. This software may be used, modified,
 *   copied, distributed, and sold, in both source and binary form provided
 *   that the above copyright and these terms are retained. Under no
 *   circumstances is the author responsible for the proper functioning
 *   of this software, nor does the author assume any responsibility
 *   for damages incurred with its use.
 */
/*======================================================================

    A PCMCIA ethernet driver for the 3com 3c589 card.

    Written by David Hinds, dhinds@allegro.stanford.edu

    The network driver code is based on Donald Becker's 3c589 code:

    Written 1994 by Donald Becker.
    Copyright 1993 United States Government as represented by the
    Director, National Security Agency.  This software may be used and
    distributed according to the terms of the GNU Public License,
    incorporated herein by reference.
    Donald Becker may be reached at becker@cesdis1.gsfc.nasa.gov

======================================================================*/
/*
 * I doubled delay loops in this file because it is not enough for some
 * laptop machines' PCIC (especially, on my Chaplet ILFA 350 ^^;).
 *                        HOSOKAWA, Tatsumi <hosokawa@mt.cs.keio.ac.jp>
 */
/*
 * Very small patch for IBM Ethernet PCMCIA Card II and IBM ThinkPad230Cs.
 *			ETO, Toshihisa <eto@osl.fujitsu.co.jp>
 */

/* XXX - Don't mix different PCCARD support code */
#include "card.h"
#include "pcic.h"
#if NCARD > 0 || NPCIC > 0
#ifndef LINT_PCCARD_HACK
#error "Dedicated PCMCIA drivers and generic PCMCIA support can't be mixed"
#else
#warning "Dedicated PCMCIA drivers and generic PCMCIA support can't be mixed"
#endif
#endif

#include "zp.h"

#include "bpfilter.h"

#include <sys/param.h>
#if defined(__FreeBSD__)
#include <sys/systm.h>
#include <sys/conf.h>
#endif
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/syslog.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/clock.h>
#include <machine/md_var.h>

#include <i386/isa/isa_device.h>
#include <i386/isa/if_zpreg.h>
#include <i386/isa/pcic.h>

#include "apm.h"
#if NAPM > 0
#include <machine/apm_bios.h>
#endif				/* NAPM > 0 */


/*****************************************************************************
 *                       Driver for Ethernet Adapter                         *
 *****************************************************************************/
/*
 * zp_softc: per line info and status
 */
static struct zp_softc {
	struct arpcom arpcom;	/* Ethernet common part		 */
#define MAX_MBS  8		/* # of mbufs we keep around	 */
	struct mbuf *mb[MAX_MBS];	/* spare mbuf storage.		 */
	int     next_mb;	/* Which mbuf to use next. 	 */
	int     last_mb;	/* Last mbuf.			 */
	int     ep_io_addr;	/* i/o bus address		 */
	char    ep_connectors;	/* Connectors on this card.	 */
	int     tx_start_thresh;/* Current TX_start_thresh.	 */
	char    bus32bit;	/* 32bit access possible	 */
	u_short if_port;
	u_char  last_alive;	/* information for reconfiguration */
	u_char  last_up;	/* information for reconfiguration */
	int     slot;		/* PCMCIA slot */
	struct	callout_handle ch; /* Callout handle for timeouts  */
	int	buffill_pending;
#if NAPM > 		0
	struct apmhook s_hook;	/* reconfiguration support */
	struct apmhook r_hook;	/* reconfiguration support */
#endif				/* NAPM > 0 */
}       zp_softc[NZP];

static int zpprobe __P((struct isa_device *));
static int zpattach __P((struct isa_device *));
static int zp_suspend __P((void *visa_dev));
static int zp_resume __P((void *visa_dev));
static int zpioctl __P((struct ifnet * ifp, int, caddr_t));
static u_short read_eeprom_data __P((int, int));

static void zpinit __P((int));
static void zpmbuffill __P((void *));
static void zpmbufempty __P((struct zp_softc *));
static void zpread __P((struct zp_softc *));
static void zpreset __P((int));
static void zpstart __P((struct ifnet *));
static void zpstop __P((int));
static void zpwatchdog __P((struct ifnet *));

struct isa_driver zpdriver = {
	zpprobe,
	zpattach,
	"zp"
};
#define CARD_INFO  "3Com Corporation~3C589"

static unsigned char card_info[256];

/*
 * scan the card information structure looking for the version/product info
 * tuple.  when we find it, compare it to the string we are looking for.
 * return 1 if we find it, 0 otherwise.
 */

static int
zp_check_cis(unsigned char *scratch)
{
	int     i, j, k;

	card_info[0] = '\0';
	i = 0;
	while (scratch[i] != 0xff && i < 1024) {
		unsigned char link = scratch[i + 2];

		if (scratch[i] == 0x15) {
			/* level 1 version/product info copy to card_info,
			 * translating '\0' to '~' */
			k = 0;
			for (j = i + 8; scratch[j] != 0xff; j += 2)
				card_info[k++] = scratch[j] == '\0' ? '~' : scratch[j];
			card_info[k++] = '\0';
			return (bcmp(card_info, CARD_INFO, sizeof(CARD_INFO) - 1) == 0);
		}
		i += 4 + 2 * link;
	}
	return 0;
}
/*
 * Probe each slot looking for an IBM Credit Card Adapter for Ethernet
 * For each card that we find, map its card information structure
 * into system memory at 'scratch' and see whether it's one of ours.
 * Return the slot number if we find a card, or -1 otherwise.
 *
 * Side effects:
 * + On success, leaves CIS mapped into memory at 'scratch';
 *   caller must free it.
 * + On success, leaves ethernet address in enet_addr.
 * + Leaves product/vendor id of last card probed in 'card_info'
 */

static int     prev_slot = 0;

static int
zp_find_adapter(unsigned char *scratch, int reconfig)
{
	int     slot;

	for (slot = prev_slot; slot < MAXSLOT; ++slot) {
		/* see if there's a PCMCIA controller here Intel PCMCIA
		 * controllers use 0x82 and 0x83 IBM clone chips use 0x88 and
		 * 0x89, apparently */
		/* IBM ThinkPad230Cs use 0x84. */
		unsigned char idbyte = pcic_getb(slot, PCIC_ID_REV);

		if (idbyte != 0x82 && idbyte != 0x83 &&
		    idbyte != 0x84 &&	/* for IBM ThinkPad 230Cs */
		    idbyte != 0x88 && idbyte != 0x89) {
			continue;
		}
		if ((pcic_getb(slot, PCIC_STATUS) & PCIC_CD) != PCIC_CD) {
			if (!reconfig) {
				printf("zp: slot %d: no card in slot\n", slot);
			} else {
				log(LOG_NOTICE, "zp: slot %d: no card in slot\n", slot);
			}
			/* no card in slot */
			continue;
		}
		pcic_power_on(slot);
		pcic_reset(slot);
		DELAY(50000);
		/* map the card's attribute memory and examine its card
		 * information structure tuples for something we recognize. */
		pcic_map_memory(slot, 0, kvtop(scratch), 0L,
		    0xFFFL, ATTRIBUTE, 1);

		if ((zp_check_cis(scratch)) > 0) {
			/* found it */
			if (!reconfig) {
				printf("zp: found card in slot %d\n", slot);
			} else {
				log(LOG_NOTICE, "zp: found card in slot %d\n", slot);
			}
			prev_slot = (prev_slot == MAXSLOT - 1) ? 0 : prev_slot + 1;

			return slot;
		} else {
			if (!reconfig) {
				printf("zp: pcmcia slot %d: %s\n", slot, card_info);
			} else {
				log(LOG_NOTICE, "zp: pcmcia slot %d: %s\n", slot, card_info);
			}
		}
		pcic_unmap_memory(slot, 0);
	}
	prev_slot = 0;
	return -1;
}


/*
 * macros to handle casting unsigned long to (char *) so we can
 * read/write into physical memory space.
 */

#define PEEK(addr) (*((unsigned char *)(addr)))
#define POKE(addr,val) do { PEEK(addr) = (val); } while (0)

/*
 * Determine if the device is present
 *
 *   on entry:
 * 	a pointer to an isa_device struct
 *   on exit:
 *	NULL if device not found
 *	or # of i/o addresses used (if found)
 */
static int
zpprobe(struct isa_device * isa_dev)
{
	struct zp_softc *sc = &zp_softc[isa_dev->id_unit];
	int     slot;
	u_short k;
	int     re_init_flag;

	if ((slot = zp_find_adapter(isa_dev->id_maddr, isa_dev->id_reconfig)) < 0)
		return 0;

	/* okay, we found a card, so set it up */
	/* Inhibit 16 bit memory delay. POINTETH.SYS apparently does this, for
	 * what reason I don't know. */
	pcic_putb(slot, PCIC_CDGC,
	    pcic_getb(slot, PCIC_CDGC) | PCIC_16_DL_INH);
	/* things to map (1) card's EEPROM is already mapped by the
	 * find_adapter routine but we still need to get the card's ethernet
	 * address. after that we unmap that part of attribute memory. (2)
	 * card configuration registers need to be mapped in so we can set the
	 * configuration and socket # registers. (3) shared memory packet
	 * buffer (4) i/o ports (5) IRQ */
#ifdef	notdef
	/* Sigh.  Location of the ethernet address isn't documented in [1]. It
	 * was derived by doing a hex dump of all of attribute memory and
	 * looking for the IBM vendor prefix. */
	enet_addr[0] = PEEK(isa_dev->id_maddr + 0xff0);
	enet_addr[1] = PEEK(isa_dev->id_maddr + 0xff2);
	enet_addr[2] = PEEK(isa_dev->id_maddr + 0xff4);
	enet_addr[3] = PEEK(isa_dev->id_maddr + 0xff6);
	enet_addr[4] = PEEK(isa_dev->id_maddr + 0xff8);
	enet_addr[5] = PEEK(isa_dev->id_maddr + 0xffa);
#endif
	re_init_flag = 0;
re_init:
	/* (2) map card configuration registers.  these are offset in card
	 * memory space by 0x20000.  normally we could get this offset from
	 * the card information structure, but I'm too lazy and am not quite
	 * sure if I understand the CIS anyway.
	 * 
	 * XXX IF YOU'RE TRYING TO PORT THIS DRIVER FOR A DIFFERENT PCMCIA CARD,
	 * the most likely thing to change is the constant 0x20000 in the next
	 * statement.  Oh yes, also change the card id string that we probe
	 * for. */
	pcic_map_memory(slot, 0, kvtop(isa_dev->id_maddr), 0x10000, 8L,
	    ATTRIBUTE, 1);
#if OLD_3C589B_CARDS
	POKE(isa_dev->id_maddr, 0x80);	/* reset the card (how long?) */
	DELAY(40000);
#endif
	/* Set the configuration index.  According to [1], the adapter won't
	 * respond to any i/o signals until we do this; it uses the Memory
	 * Only interface (whatever that is; it's not documented). Also turn
	 * on "level" (not pulse) interrupts.
	 * 
	 * XXX probably should init the socket and copy register also, so that we
	 * can deal with multiple instances of the same card. */
	POKE(isa_dev->id_maddr, 0x41);
	pcic_unmap_memory(slot, 0);

	/* (4) map i/o ports.
	 * 
	 * XXX is it possible that the config file leaves this unspecified, in
	 * which case we have to pick one?
	 * 
	 * At least one PCMCIA device driver I'v seen maps a block of 32
	 * consecutive i/o ports as two windows of 16 ports each. Maybe some
	 * other pcic chips are restricted to 16-port windows; the 82365SL
	 * doesn't seem to have that problem.  But since we have an extra
	 * window anyway... */
	pcic_map_io(slot, 0, isa_dev->id_iobase, 16, 2);

	/* (5) configure the card for the desired interrupt
	 * 
	 * XXX is it possible that the config file leaves this unspecified? */
	pcic_map_irq(slot, ffs(isa_dev->id_irq) - 1);

	/* tell the PCIC that this is an I/O card (not memory) */
	pcic_putb(slot, PCIC_INT_GEN,
	    pcic_getb(slot, PCIC_INT_GEN) | PCIC_CARDTYPE);

	sc->ep_io_addr = isa_dev->id_iobase;
	GO_WINDOW(0);
	k = read_eeprom_data(BASE, EEPROM_ADDR_CFG);	/* get addr cfg */
	sc->if_port = k >> 14;
	k = (k & 0x1f) * 0x10 + 0x200;	/* decode base addr. */
	if (k != (u_short) isa_dev->id_iobase) {
		if (!re_init_flag) {
			re_init_flag++;
			goto re_init;
		}
		return (0);
	}
	k = read_eeprom_data(BASE, EEPROM_RESOURCE_CFG);

	k >>= 12;

	if (isa_dev->id_irq != (1 << ((k == 2) ? 9 : k)))
		return (0);

	outb(BASE, ACTIVATE_ADAPTER_TO_CONFIG);


	/* information for reconfiguration */
	sc->last_alive = 0;
	sc->last_up = 0;
	sc->slot = slot;

	return (0x10);		/* 16 bytes of I/O space used. */
}
#if NAPM > 0
static int
zp_suspend(visa_dev)
	void   *visa_dev;
{
#if 0
	struct isa_device *isa_dev = visa_dev;
	struct zp_softc *sc = &zp_softc[isa_dev->id_unit];

	pcic_power_off(sc->slot);
#endif
	return 0;
}

static int
zp_resume(visa_dev)
	void   *visa_dev;
{
	struct isa_device *isa_dev = visa_dev;

	prev_slot = 0;
	reconfig_isadev(isa_dev, &net_imask);
	return 0;
}
#endif				/* NAPM > 0 */


/*
 * Install interface into kernel networking data structures
 */

static int
zpattach(isa_dev)
	struct isa_device *isa_dev;
{
	struct zp_softc *sc = &zp_softc[isa_dev->id_unit];
	struct ifnet *ifp = &sc->arpcom.ac_if;
	u_short i;
	int     pl;

	/* PCMCIA card can be offlined. Reconfiguration is required */
	if (isa_dev->id_reconfig) {
		if (!isa_dev->id_alive && sc->last_alive) {
			pl = splimp();
			sc->last_up = (ifp->if_flags & IFF_UP);
			if_down(ifp);
			splx(pl);
			sc->last_alive = 0;
		}
		if (isa_dev->id_alive && !sc->last_alive) {
			zpreset(isa_dev->id_unit);
			if (sc->last_up) {
				pl = splimp();
				if_up(ifp);
				splx(pl);
			}
			sc->last_alive = 1;
		}
		return 1;
	} else {
		sc->last_alive = 1;
	}


	sc->ep_io_addr = isa_dev->id_iobase;
	printf("zp%d: ", isa_dev->id_unit);

	sc->buffill_pending = 0;
	callout_handle_init(&sc->ch);

	sc->ep_connectors = 0;

	i = inw(isa_dev->id_iobase + EP_W0_CONFIG_CTRL);

	if (i & IS_AUI) {
		printf("aui");
		sc->ep_connectors |= AUI;
	}
	if (i & IS_BNC) {
		if (sc->ep_connectors)
			printf("/");
		printf("bnc");
		sc->ep_connectors |= BNC;
	}
	if (i & IS_UTP) {
		if (sc->ep_connectors)
			printf("/");
		printf("utp");
		sc->ep_connectors |= UTP;
	}
	if (!sc->ep_connectors)
		printf("no connectors!");

	GO_WINDOW(0);
	{
		short   tmp_addr[3];
		int     j;
		for (j = 0; j < 3; j++) {
			tmp_addr[i] = htons(read_eeprom_data(BASE, j));
		}
		bcopy(tmp_addr, sc->arpcom.ac_enaddr, 6);
	}

	printf(" address %6D\n", sc->arpcom.ac_enaddr, ":");

	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX;
	ifp->if_unit = isa_dev->id_unit;
	ifp->if_name = "zp";
	ifp->if_output = ether_output;
	ifp->if_start = zpstart;
	ifp->if_ioctl = zpioctl;
	ifp->if_watchdog = zpwatchdog;
	/* Select connector according to board setting. */
	ifp->if_flags |= IFF_LINK0;

	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
#if NAPM > 0
	sc->s_hook.ah_fun = zp_suspend;
	sc->s_hook.ah_arg = (void *) isa_dev;
	sc->s_hook.ah_name = "3Com PCMCIA Etherlink III 3C589";
	sc->s_hook.ah_order = APM_MID_ORDER;
	apm_hook_establish(APM_HOOK_SUSPEND, &sc->s_hook);
	sc->r_hook.ah_fun = zp_resume;
	sc->r_hook.ah_arg = (void *) isa_dev;
	sc->r_hook.ah_name = "3Com PCMCIA Etherlink III 3C589";
	sc->r_hook.ah_order = APM_MID_ORDER;
	apm_hook_establish(APM_HOOK_RESUME, &sc->r_hook);
#endif				/* NAPM > 0 */
	return 1;
}
/*
 * The order in here seems important. Otherwise we may not receive
 * interrupts. ?!
 */
static void
zpinit(unit)
	int     unit;
{
	register struct zp_softc *sc = &zp_softc[unit];
	register struct ifnet *ifp = &sc->arpcom.ac_if;
	int     s, i;

	if (TAILQ_EMPTY(&ifp->if_addrhead)) /* XXX unlikely */
		return;

	s = splimp();
	while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);

	GO_WINDOW(0);

	/* Disable the card */
	outw(BASE + EP_W0_CONFIG_CTRL, 0);

	/* Enable the card */
	outw(BASE + EP_W0_CONFIG_CTRL, ENABLE_DRQ_IRQ);

	GO_WINDOW(2);

	/* Reload the ether_addr. */
	for (i = 0; i < 6; i++)
		outb(BASE + EP_W2_ADDR_0 + i, sc->arpcom.ac_enaddr[i]);

	outw(BASE + EP_COMMAND, RX_RESET);
	outw(BASE + EP_COMMAND, TX_RESET);

	/* Window 1 is operating window */
	GO_WINDOW(1);
	for (i = 0; i < 31; i++)
		inb(BASE + EP_W1_TX_STATUS);

	/* get rid of stray intr's */
	outw(BASE + EP_COMMAND, ACK_INTR | 0xff);

	outw(BASE + EP_COMMAND, SET_RD_0_MASK | S_CARD_FAILURE | S_RX_COMPLETE |
	    S_TX_COMPLETE | S_TX_AVAIL);
	outw(BASE + EP_COMMAND, SET_INTR_MASK | S_CARD_FAILURE | S_RX_COMPLETE |
	    S_TX_COMPLETE | S_TX_AVAIL);

#ifndef IFF_MULTICAST
#define	IFF_MULTICAST	0x10000
#endif

	outw(BASE + EP_COMMAND, SET_RX_FILTER | FIL_INDIVIDUAL |
	    ((sc->arpcom.ac_if.if_flags & IFF_MULTICAST) ? FIL_GROUP : 0) |
	    FIL_BRDCST |
	    ((sc->arpcom.ac_if.if_flags & IFF_PROMISC) ? FIL_ALL : 0));
	/* you can `ifconfig (link0|-link0) ep0' to get the following
	 * behaviour: -link0	disable AUI/UTP. enable BNC. link0 disable
	 * BNC. enable AUI. if the card has a UTP connector, that is enabled
	 * too. not sure, but it seems you have to be careful to not plug
	 * things into both AUI & UTP. */

	if (!(ifp->if_flags & IFF_LINK0) && (sc->ep_connectors & BNC)) {
		GO_WINDOW(0);
		/* set the xcvr */
		outw(BASE + EP_W0_ADDRESS_CFG, 3 << 14);
		GO_WINDOW(2);
		outw(BASE + EP_COMMAND, START_TRANSCEIVER);
		GO_WINDOW(1);
	}
#if defined(__NetBSD__) || defined(__FreeBSD__)
	if ((ifp->if_flags & IFF_LINK0) && (sc->ep_connectors & UTP)) {
#else
	if ((ifp->if_flags & IFF_ALTPHYS) && (sc->ep_connectors & UTP)) {
#endif
		GO_WINDOW(4);
		outw(BASE + EP_W4_MEDIA_TYPE, ENABLE_UTP);
		GO_WINDOW(1);
	}
	outw(BASE + EP_COMMAND, RX_ENABLE);
	outw(BASE + EP_COMMAND, TX_ENABLE);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;	/* just in case */
	sc->tx_start_thresh = 20;	/* probably a good starting point. */
	/* Store up a bunch of mbuf's for use later. (MAX_MBS). First we free
	 * up any that we had in case we're being called from intr or
	 * somewhere else. */
	sc->last_mb = 0;
	sc->next_mb = 0;
	if (sc->buffill_pending != 0) {
		untimeout(zpmbuffill, sc, sc->ch);
		sc->buffill_pending = 0;
	}
	zpmbuffill(sc);
	zpstart(ifp);
	splx(s);
}

static const char padmap[] = {0, 3, 2, 1};
static void
zpstart(ifp)
	struct ifnet *ifp;
{
	register struct zp_softc *sc = ifp->if_softc;
	struct mbuf *m, *top;

	int     s, len, pad;

	s = splimp();

	if (sc->arpcom.ac_if.if_flags & IFF_OACTIVE) {
		splx(s);
		return;
	}
startagain:

	/* Sneak a peek at the next packet */
	m = sc->arpcom.ac_if.if_snd.ifq_head;
	if (m == 0) {
		splx(s);
		return;
	}
	for (len = 0, top = m; m; m = m->m_next)
		len += m->m_len;

	pad = padmap[len & 3];

	/* The 3c509 automatically pads short packets to minimum ethernet
	 * length, but we drop packets that are too large. Perhaps we should
	 * truncate them instead? */
	if (len + pad > ETHER_MAX_LEN) {
		/* packet is obviously too large: toss it */
		++sc->arpcom.ac_if.if_oerrors;
		IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);
		m_freem(m);
		goto readcheck;
	}
	if (inw(BASE + EP_W1_FREE_TX) < len + pad + 4) {
		/* no room in FIFO */
		outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH | (len + pad + 4));
		sc->arpcom.ac_if.if_flags |= IFF_OACTIVE;
		splx(s);

		return;
	}
	IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);

	if (m == 0) {		/* not really needed */
		splx(s);
		return;
	}
	outw(BASE + EP_COMMAND, SET_TX_START_THRESH |
	    (len / 4 + sc->tx_start_thresh));

	outw(BASE + EP_W1_TX_PIO_WR_1, len);
	outw(BASE + EP_W1_TX_PIO_WR_1, 0xffff);	/* Second dword meaningless */

	for (top = m; m != 0; m = m->m_next) {
		if (sc->bus32bit) {
			outsl(BASE + EP_W1_TX_PIO_WR_1, mtod(m, caddr_t),
			    m->m_len / 4);
			if (m->m_len & 3)
				outsb(BASE + EP_W1_TX_PIO_WR_1,
				    mtod(m, caddr_t) + (m->m_len & (~3)),
				    m->m_len & 3);
		} else {
			outsw(BASE + EP_W1_TX_PIO_WR_1, mtod(m, caddr_t), m->m_len / 2);
			if (m->m_len & 1)
				outb(BASE + EP_W1_TX_PIO_WR_1,
				    *(mtod(m, caddr_t) + m->m_len - 1));
		}
	}
	while (pad--)
		outb(BASE + EP_W1_TX_PIO_WR_1, 0);	/* Padding */

#if NBPFILTER > 0
	if (sc->arpcom.ac_if.if_bpf) {
		bpf_mtap(&sc->arpcom.ac_if, top);
	}
#endif

	m_freem(top);
	++sc->arpcom.ac_if.if_opackets;
	/* Is another packet coming in? We don't want to overflow the tiny RX
	 * fifo. */
readcheck:
	if (inw(BASE + EP_W1_RX_STATUS) & RX_BYTES_MASK) {
		splx(s);
		return;
	}
	goto startagain;
}
void
zpintr(unit)
	int     unit;
{
	int     status, i;
	register struct zp_softc *sc = &zp_softc[unit];

	struct ifnet *ifp = &sc->arpcom.ac_if;


	status = 0;
checkintr:
	status = inw(BASE + EP_STATUS) &
	    (S_TX_COMPLETE | S_TX_AVAIL | S_RX_COMPLETE | S_CARD_FAILURE);
checkintr2:
	if (status == 0) {
		/* No interrupts. */
		outw(BASE + EP_COMMAND, C_INTR_LATCH);

		status = inw(BASE + EP_STATUS) &
		    (S_TX_COMPLETE | S_TX_AVAIL | S_RX_COMPLETE |
			S_CARD_FAILURE);
		if (status)
			goto checkintr2;

		return;
	}
	/* important that we do this first. */
	outw(BASE + EP_COMMAND, ACK_INTR | status);

	if (status & S_TX_AVAIL) {
		status &= ~S_TX_AVAIL;
		inw(BASE + EP_W1_FREE_TX);
		sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
		zpstart(&sc->arpcom.ac_if);

	}
	if (status & S_RX_COMPLETE) {
		status &= ~S_RX_COMPLETE;
		zpread(sc);
	}
	if (status & S_CARD_FAILURE) {
		printf("zp%d: reset (status: %x)\n", unit, status);
		outw(BASE + EP_COMMAND, C_INTR_LATCH);
		zpinit(unit);
		return;
	}
	if (status & S_TX_COMPLETE) {
		status &= ~S_TX_COMPLETE;
		/* We need to read TX_STATUS until we get a 0 status in order
		 * to turn off the interrupt flag. */
		while ((i = inb(BASE + EP_W1_TX_STATUS)) & TXS_COMPLETE) {
			outw(BASE + EP_W1_TX_STATUS, 0x0);
			if (i & (TXS_MAX_COLLISION | TXS_JABBER | TXS_UNDERRUN)) {
				if (i & TXS_MAX_COLLISION)
					++sc->arpcom.ac_if.if_collisions;
				if (i & (TXS_JABBER | TXS_UNDERRUN)) {
					outw(BASE + EP_COMMAND, TX_RESET);
					if (i & TXS_UNDERRUN) {
						if (sc->tx_start_thresh < ETHER_MAX_LEN) {
							sc->tx_start_thresh += 20;
							outw(BASE + EP_COMMAND,
							    SET_TX_START_THRESH |
							    sc->tx_start_thresh);
						}
					}
				}
				outw(BASE + EP_COMMAND, TX_ENABLE);
				++sc->arpcom.ac_if.if_oerrors;
			}
		}
		zpstart(ifp);
	}
	goto checkintr;
}

static void
zpread(sc)
	register struct zp_softc *sc;
{
	struct ether_header *eh;
	struct mbuf *mcur, *m, *m0, *top;
	int     totlen, lenthisone;
	int     save_totlen;
	int     off;


	totlen = inw(BASE + EP_W1_RX_STATUS);
	off = 0;
	top = 0;

	if (totlen & ERR_RX) {
		++sc->arpcom.ac_if.if_ierrors;
		goto out;
	}
	save_totlen = totlen &= RX_BYTES_MASK;	/* Lower 11 bits = RX bytes. */

	m = sc->mb[sc->next_mb];
	sc->mb[sc->next_mb] = 0;

	if (m == 0) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == 0)
			goto out;
	} else {
		/* Convert one of our saved mbuf's */
		sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
		m->m_data = m->m_pktdat;
		m->m_flags = M_PKTHDR;
	}

	top = m0 = m;		/* We assign top so we can "goto out" */
#define EROUND  ((sizeof(struct ether_header) + 3) & ~3)
#define EOFF    (EROUND - sizeof(struct ether_header))
	m0->m_data += EOFF;
	/* Read what should be the header. */
	insw(BASE + EP_W1_RX_PIO_RD_1,
	    mtod(m0, caddr_t), sizeof(struct ether_header) / 2);
	m->m_len = sizeof(struct ether_header);
	totlen -= sizeof(struct ether_header);
	/* mostly deal with trailer here.  (untested) We do this in a couple
	 * of parts.  First we check for a trailer, if we have one we convert
	 * the mbuf back to a regular mbuf and set the offset and subtract
	 * sizeof(struct ether_header) from the pktlen. After we've read the
	 * packet off the interface (all except for the trailer header, we
	 * then get a header mbuf, read the trailer into it, and fix up the
	 * mbuf pointer chain. */
	eh = mtod(m, struct ether_header *);
	while (totlen > 0) {
		lenthisone = min(totlen, M_TRAILINGSPACE(m));
		if (lenthisone == 0) {	/* no room in this one */
			mcur = m;
			m = sc->mb[sc->next_mb];
			sc->mb[sc->next_mb] = 0;
			if (!m) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m == 0)
					goto out;
			} else if (sc->buffill_pending == 0) {
				sc->ch = timeout(zpmbuffill, sc, 0);
				sc->buffill_pending = 1;
				sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
			}
			if (totlen >= MINCLSIZE)
				MCLGET(m, M_DONTWAIT);
			m->m_len = 0;
			mcur->m_next = m;
			lenthisone = min(totlen, M_TRAILINGSPACE(m));
		}
		if (sc->bus32bit) {
			insl(BASE + EP_W1_RX_PIO_RD_1, mtod(m, caddr_t) + m->m_len,
			    lenthisone / 4);
			m->m_len += (lenthisone & ~3);
			if (lenthisone & 3)
				insb(BASE + EP_W1_RX_PIO_RD_1,
				    mtod(m, caddr_t) + m->m_len,
				    lenthisone & 3);
			m->m_len += (lenthisone & 3);
		} else {
			insw(BASE + EP_W1_RX_PIO_RD_1, mtod(m, caddr_t) + m->m_len,
			    lenthisone / 2);
			m->m_len += lenthisone;
			if (lenthisone & 1)
				*(mtod(m, caddr_t) + m->m_len - 1) = inb(BASE + EP_W1_RX_PIO_RD_1);
		}
		totlen -= lenthisone;
	}
	if (off) {
		top = sc->mb[sc->next_mb];
		sc->mb[sc->next_mb] = 0;
		if (top == 0) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (top == 0) {
				top = m0;
				goto out;
			}
		} else {
			/* Convert one of our saved mbuf's */
			sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
			top->m_data = top->m_pktdat;
			top->m_flags = M_PKTHDR;
		}
		insw(BASE + EP_W1_RX_PIO_RD_1, mtod(top, caddr_t),
		    sizeof(struct ether_header));
		top->m_next = m0;
		top->m_len = sizeof(struct ether_header);
		/* XXX Accomodate for type and len from beginning of trailer */
		top->m_pkthdr.len = save_totlen - (2 * sizeof(u_short));
	} else {
		top = m0;
		top->m_pkthdr.len = save_totlen;
	}

	top->m_pkthdr.rcvif = &sc->arpcom.ac_if;
	outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
	++sc->arpcom.ac_if.if_ipackets;
#if NBPFILTER > 0
	if (sc->arpcom.ac_if.if_bpf) {
		bpf_mtap(&sc->arpcom.ac_if, top);

		/* Note that the interface cannot be in promiscuous mode if
		 * there are no BPF listeners.  And if we are in promiscuous
		 * mode, we have to check if this packet is really ours. */
		if ((sc->arpcom.ac_if.if_flags & IFF_PROMISC) &&
		    (eh->ether_dhost[0] & 1) == 0 &&
		    bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr,
			sizeof(eh->ether_dhost)) != 0 &&
		    bcmp(eh->ether_dhost, etherbroadcastaddr,
			sizeof(eh->ether_dhost)) != 0) {
			m_freem(top);
			return;
		}
	}
#endif
	m_adj(top, sizeof(struct ether_header));
	ether_input(&sc->arpcom.ac_if, eh, top);
	return;

out:	outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
	if (top)
		m_freem(top);

}


/*
 * Look familiar?
 */
static int
zpioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	int     cmd;
	caddr_t data;
{
	register struct ifaddr *ifa = (struct ifaddr *) data;
	struct zp_softc *sc = ifp->if_softc;
	int     error = 0;


	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			zpinit(ifp->if_unit);	/* before arpwhohas */
			arp_ifinit((struct arpcom *) ifp, ifa);
			break;
#endif
#ifdef IPX
		case AF_IPX:
			{
				register struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);

				if (ipx_nullhost(*ina))
					ina->x_host =
					    *(union ipx_host *) (sc->arpcom.ac_enaddr);
				else {
					ifp->if_flags &= ~IFF_RUNNING;
					bcopy((caddr_t) ina->x_host.c_host,
					    (caddr_t) sc->arpcom.ac_enaddr,
					    sizeof(sc->arpcom.ac_enaddr));
				}
				zpinit(ifp->if_unit);
				break;
			}
#endif
#ifdef NS
		case AF_NS:
			{
				register struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);

				if (ns_nullhost(*ina))
					ina->x_host =
					    *(union ns_host *) (sc->arpcom.ac_enaddr);
				else {
					ifp->if_flags &= ~IFF_RUNNING;
					bcopy((caddr_t) ina->x_host.c_host,
					    (caddr_t) sc->arpcom.ac_enaddr,
					    sizeof(sc->arpcom.ac_enaddr));
				}
				zpinit(ifp->if_unit);
				break;
			}
#endif
		default:
			zpinit(ifp->if_unit);
			break;
		}
		break;
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 && ifp->if_flags & IFF_RUNNING) {
			ifp->if_flags &= ~IFF_RUNNING;
			zpstop(ifp->if_unit);
			zpmbufempty(sc);
			break;
		}
		zpinit(ifp->if_unit);
		break;
	default:
		error = EINVAL;
	}
	return (error);
}

static void
zpreset(unit)
	int     unit;
{
	int     s = splimp();

	zpstop(unit);
	zpinit(unit);
	splx(s);
}

static void
zpwatchdog(ifp)
	struct ifnet *ifp;
{
	log(LOG_ERR, "zp%d: watchdog\n", ifp->if_unit);
	ifp->if_oerrors++;
	zpreset(ifp->if_unit);
}

static void
zpstop(unit)
	int     unit;
{
	struct zp_softc *sc = &zp_softc[unit];

	outw(BASE + EP_COMMAND, RX_DISABLE);
	outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
	outw(BASE + EP_COMMAND, TX_DISABLE);
	outw(BASE + EP_COMMAND, STOP_TRANSCEIVER);
	outw(BASE + EP_COMMAND, RX_RESET);
	outw(BASE + EP_COMMAND, TX_RESET);
	outw(BASE + EP_COMMAND, C_INTR_LATCH);
	outw(BASE + EP_COMMAND, SET_RD_0_MASK);
	outw(BASE + EP_COMMAND, SET_INTR_MASK);
	outw(BASE + EP_COMMAND, SET_RX_FILTER);
}



static  u_short
read_eeprom_data(id_port, offset)
	int     id_port;
	int     offset;
{

	outb(id_port + 10, 0x80 + offset);
	DELAY(1000);
	return inw(id_port + 12);
}




static void
zpmbuffill(sp)
	void   *sp;
{
	struct zp_softc *sc = (struct zp_softc *) sp;
	int     s, i;

	s = splimp();
	i = sc->last_mb;
	do {
		if (sc->mb[i] == NULL)
			MGET(sc->mb[i], M_DONTWAIT, MT_DATA);
		if (sc->mb[i] == NULL)
			break;
		i = (i + 1) % MAX_MBS;
	} while (i != sc->next_mb);
	sc->buffill_pending = 0;
	sc->last_mb = i;
	splx(s);
}

static void
zpmbufempty(sc)
	struct zp_softc *sc;
{
	int     s, i;

	s = splimp();
	for (i = 0; i < MAX_MBS; i++) {
		if (sc->mb[i]) {
			m_freem(sc->mb[i]);
			sc->mb[i] = NULL;
		}
	}
	sc->last_mb = sc->next_mb = 0;
	if (sc->buffill_pending != 0) {
		untimeout(zpmbuffill, sc, sc->ch);
		sc->buffill_pending = 0;
	}
	splx(s);
}
