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
 *	$Id: if_zp.c,v 1.6.4.5 1997/05/21 18:43:37 nate Exp $
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

#include "zp.h"
#if	NZP > 0

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/clock.h>

#if defined(ZP_DEBUG)
#include <i386/i386/cons.h>
#endif

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>
#include <i386/isa/if_zpreg.h>
#include <i386/isa/pcic.h>

#include "apm.h"
#if NAPM > 0
#include <machine/apm_bios.h>
#endif	/* NAPM > 0 */

#define ETHER_MIN_LEN	64
#define ETHER_MAX_LEN	1518
#define ETHER_ADDR_LEN	6



/*****************************************************************************
 *                       Driver for Ethernet Adapter                         *
 *****************************************************************************/
/*
 * zp_softc: per line info and status
 */
struct zp_softc {
    struct arpcom   arpcom;	/* Ethernet common part		 */
#define MAX_MBS  8	/* # of mbufs we keep around	 */
    struct mbuf    *mb[MAX_MBS];	/* spare mbuf storage.		 */
    int             next_mb;	/* Which mbuf to use next. 	 */
    int             last_mb;	/* Last mbuf.			 */
    caddr_t         bpf;	/* BPF  "magic cookie"		 */
    short           ep_io_addr;	/* i/o bus address		 */
    char            ep_connectors;	/* Connectors on this card.	 */
    int             tx_start_thresh;	/* Current TX_start_thresh.	 */
    char            bus32bit;	/* 32bit access possible	 */
    u_short         if_port;
    u_char          last_alive;	/* information for reconfiguration */
    u_char          last_up;	/* information for reconfiguration */
    int             slot;	/* PCMCIA slot */
#if NAPM > 		0
    struct apmhook  s_hook;	/* reconfiguration support */
    struct apmhook  r_hook;	/* reconfiguration support */
#endif	/* NAPM > 0 */
}               zp_softc[NZP];


int zpprobe     __P((struct isa_device *));
int zpattach    __P((struct isa_device *));
static int zpioctl __P((struct ifnet * ifp, int, caddr_t));
static u_short read_eeprom_data __P((int, int));

void zpinit     __P((int));
void zpintr     __P((int));
void zpmbuffill __P((void *));
static void zpmbufempty __P((struct zp_softc *));
void zpread     __P((struct zp_softc *));
void zpreset    __P((int));
void zpstart    __P((struct ifnet *));
void zpstop     __P((int));
void zpwatchdog __P((int));

struct isa_driver zpdriver = {
    zpprobe,
    zpattach,
    "zp"
};

static int send_ID_sequence __P((u_short));
static u_short get_eeprom_data __P((int, int));
static int f_is_eeprom_busy __P((struct isa_device *));

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
    int             i, j, k;

    card_info[0] = '\0';
    i = 0;
    while (scratch[i] != 0xff && i < 1024) {
	unsigned char   link = scratch[i + 2];

#if 0
	printf("[%02x] %02x ", i, link);
	for (j = 4; j < 2 * link + 4 && j < 32; j += 2)
	    printf("%02x ", scratch[j + i]);
	printf("\n");
#endif
	if (scratch[i] == 0x15) {
	    /*
	     * level 1 version/product info copy to card_info, translating
	     * '\0' to '~'
	     */
	    k = 0;
	    for (j = i + 8; scratch[j] != 0xff; j += 2)
		card_info[k++] = scratch[j] == '\0' ? '~' : scratch[j];
	    card_info[k++] = '\0';
#ifdef ZP_DEBUG
	    printf("card info = %s\n", card_info);
	    printf("result = %d\n", memcmp(card_info, CARD_INFO, sizeof(CARD_INFO) - 1) == 0);
#endif
	    return (memcmp(card_info, CARD_INFO, sizeof(CARD_INFO) - 1) == 0);
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

int             prev_slot = 0;

static int
zp_find_adapter(unsigned char *scratch, int reconfig)
{
    int             slot;

    for (slot = prev_slot; slot < MAXSLOT; ++slot) {
	/*
	 * see if there's a PCMCIA controller here Intel PCMCIA controllers
	 * use 0x82 and 0x83 IBM clone chips use 0x88 and 0x89, apparently
	 */
	/*
	 * IBM ThinkPad230Cs use 0x84.
	 */
	unsigned char   idbyte = pcic_getb(slot, PCIC_ID_REV);

	if (idbyte != 0x82 && idbyte != 0x83 &&
	    idbyte != 0x84 &&	/* for IBM ThinkPad 230Cs */
	    idbyte != 0x88 && idbyte != 0x89) {
#if 0
	    printf("ibmccae: pcic slot %d: wierd id/rev code 0x%02x\n",
		   slot, idbyte);
#endif
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
	/*
	 * map the card's attribute memory and examine its card information
	 * structure tuples for something we recognize.
	 */
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
int
zpprobe(struct isa_device * isa_dev)
{
    struct zp_softc *sc = &zp_softc[isa_dev->id_unit];
    int             i, x;
    u_int           memsize;
    u_char          iptr, memwidth, sum, tmp;
    int             slot;
    u_short         k;
    int             id_port = 0x100;	/* XXX */
    int             re_init_flag;

#ifdef	ZP_DEBUG
    printf("### zpprobe ####\n");
#endif	/* ZP_DEBUG */

    if ((slot = zp_find_adapter(isa_dev->id_maddr, isa_dev->id_reconfig)) < 0)
	return NULL;

    /*
     * okay, we found a card, so set it up
     */
    /*
     * Inhibit 16 bit memory delay. POINTETH.SYS apparently does this, for
     * what reason I don't know.
     */
    pcic_putb(slot, PCIC_CDGC,
	      pcic_getb(slot, PCIC_CDGC) | PCIC_16_DL_INH);
    /*
     * things to map (1) card's EEPROM is already mapped by the find_adapter
     * routine but we still need to get the card's ethernet address. after
     * that we unmap that part of attribute memory. (2) card configuration
     * registers need to be mapped in so we can set the configuration and
     * socket # registers. (3) shared memory packet buffer (4) i/o ports (5)
     * IRQ
     */
#ifdef	notdef
    /*
     * Sigh.  Location of the ethernet address isn't documented in [1]. It
     * was derived by doing a hex dump of all of attribute memory and looking
     * for the IBM vendor prefix.
     */
    enet_addr[0] = PEEK(isa_dev->id_maddr + 0xff0);
    enet_addr[1] = PEEK(isa_dev->id_maddr + 0xff2);
    enet_addr[2] = PEEK(isa_dev->id_maddr + 0xff4);
    enet_addr[3] = PEEK(isa_dev->id_maddr + 0xff6);
    enet_addr[4] = PEEK(isa_dev->id_maddr + 0xff8);
    enet_addr[5] = PEEK(isa_dev->id_maddr + 0xffa);
#endif
#if 0
    pcic_unmap_memory(slot, 0);
#endif
    re_init_flag = 0;
re_init:
    /*
     * (2) map card configuration registers.  these are offset in card memory
     * space by 0x20000.  normally we could get this offset from the card
     * information structure, but I'm too lazy and am not quite sure if I
     * understand the CIS anyway.
     *
     * XXX IF YOU'RE TRYING TO PORT THIS DRIVER FOR A DIFFERENT PCMCIA CARD, the
     * most likely thing to change is the constant 0x20000 in the next
     * statement.  Oh yes, also change the card id string that we probe for.
     */
    pcic_map_memory(slot, 0, kvtop(isa_dev->id_maddr), 0x10000, 8L,
		    ATTRIBUTE, 1);
#if OLD_3C589B_CARDS
    POKE(isa_dev->id_maddr, 0x80);	/* reset the card (how long?) */
    DELAY(40000);
#endif
    /*
     * Set the configuration index.  According to [1], the adapter won't
     * respond to any i/o signals until we do this; it uses the Memory Only
     * interface (whatever that is; it's not documented). Also turn on
     * "level" (not pulse) interrupts.
     *
     * XXX probably should init the socket and copy register also, so that we
     * can deal with multiple instances of the same card.
     */
    POKE(isa_dev->id_maddr, 0x41);
    pcic_unmap_memory(slot, 0);

#ifdef	notdef
    /*
     * (3) now map in the shared memory buffer.  This has to be mapped as
     * words, not bytes, and on a 16k boundary.  The offset value was derived
     * by installing IBM's POINTETH.SYS under DOS and looking at the PCIC
     * registers; it's not documented in IBM's tech ref manual ([1]).
     */
    pcic_map_memory(slot, 0, kvtop(isa_dev->id_maddr), 0x4000L, 0x4000L,
		    COMMON, 2);
#endif

    /*
     * (4) map i/o ports.
     *
     * XXX is it possible that the config file leaves this unspecified, in which
     * case we have to pick one?
     *
     * At least one PCMCIA device driver I'v seen maps a block of 32 consecutive
     * i/o ports as two windows of 16 ports each. Maybe some other pcic chips
     * are restricted to 16-port windows; the 82365SL doesn't seem to have
     * that problem.  But since we have an extra window anyway...
     */
#if 1
    pcic_map_io(slot, 0, isa_dev->id_iobase, 16, 2);
#else
    pcic_map_io(slot, 0, isa_dev->id_iobase, 16, 1);
    pcic_map_io(slot, 1, isa_dev->id_iobase + 16, 16, 1);
#endif

    /*
     * (5) configure the card for the desired interrupt
     *
     * XXX is it possible that the config file leaves this unspecified?
     */
    pcic_map_irq(slot, ffs(isa_dev->id_irq) - 1);

    /* tell the PCIC that this is an I/O card (not memory) */
    pcic_putb(slot, PCIC_INT_GEN,
	      pcic_getb(slot, PCIC_INT_GEN) | PCIC_CARDTYPE);

#if 0
    /* tell the PCIC to use level-mode interrupts */
    /* XXX this register may not be present on all controllers */
    pcic_putb(slot, PCIC_GLO_CTRL,
	      pcic_getb(slot, PCIC_GLO_CTRL) | PCIC_LVL_MODE);
#endif

#ifdef ZP_DEBUG
    pcic_print_regs(slot);
#endif
#ifdef	notdef
    /* I couldn't find the following part in linux. seiji */

    /*
     * Setup i/o addresses
     */
    sc->nic_addr = isa_dev->id_iobase;
#if 0
    sc->vector = isa_dev->id_irq;
#endif
    sc->smem_start = (caddr_t) isa_dev->id_maddr;

#if 0
    sc->vendor = ZE_VENDOR_IBM;
    sc->type = xxx;
#endif

    /* reset card to force it into a known state */
    tmp = inb(isa_dev->id_iobase + ZE_RESET);
    DELAY(20000);
    outb(isa_dev->id_iobase + ZE_RESET, tmp);
    DELAY(20000);

    /*
     * query MAM bit in misc register for 10base2
     */
    tmp = inb(isa_dev->id_iobase + ZE_MISC);
    if (!tmp && !re_init_flag) {
	re_init_flag++;
	goto re_init;
    }
    sc->mau = tmp & 0x09 ? "10base2" : "10baseT";
#endif

    sc->ep_io_addr = isa_dev->id_iobase;
    GO_WINDOW(0);
#if 0
    k = get_eeprom_data(BASE, EEPROM_ADDR_CFG);	/* get addr cfg */
#endif
    k = read_eeprom_data(BASE, EEPROM_ADDR_CFG);	/* get addr cfg */
    sc->if_port = k >> 14;
#ifdef ZP_DEBUG
    printf("EEPROM data = 0x%x\n", k);
#endif
    k = (k & 0x1f) * 0x10 + 0x200;	/* decode base addr. */
    if (k != (u_short) isa_dev->id_iobase)
    {
	if (!re_init_flag) {
	    re_init_flag++;
	    goto re_init;
	}
	return (0);
    }
    k = read_eeprom_data(BASE, EEPROM_RESOURCE_CFG);

    k >>= 12;

    if (isa_dev->id_irq != (1 << ((k == 2) ? 9 : k)))
#ifdef	ZP_DEBUG
    {
	printf("Unmatched !!!!!!\n");
	return (0);
    }
#else	/* ZP_DEBUG */
	return (0);
#endif	/* ZP_DEBUG */

#if 0
    outb(id_port, ACTIVATE_ADAPTER_TO_CONFIG);
#else
    outb(BASE, ACTIVATE_ADAPTER_TO_CONFIG);
#endif

    /* information for reconfiguration */
    sc->last_alive = 0;
    sc->last_up = 0;
    sc->slot = slot;

    return (0x10);	/* 16 bytes of I/O space used. */
}

#if NAPM > 0
static int
zp_suspend(isa_dev)
    struct isa_device *isa_dev;
{
    struct zp_softc *sc = &zp_softc[isa_dev->id_unit];

    pcic_power_off(sc->slot);
    return 0;
}

static int
zp_resume(isa_dev)
    struct isa_device *isa_dev;
{
    prev_slot = 0;
    reconfig_isadev(isa_dev, &net_imask);
    return 0;
}
#endif	/* NAPM > 0 */


/*
 * Install interface into kernel networking data structures
 */

int
zpattach(isa_dev)
    struct isa_device *isa_dev;
{
    struct zp_softc *sc = &zp_softc[isa_dev->id_unit];
    struct ifnet   *ifp = &sc->arpcom.ac_if;
    u_short         i;
    struct ifaddr  *ifa;
    struct sockaddr_dl *sdl;
    int             pl;

#ifdef	ZP_DEBUG
    printf("### zpattach ####\n");
#endif	/* ZP_DEBUG */

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

    sc->ep_connectors = 0;

    i = inw(isa_dev->id_iobase + EP_W0_CONFIG_CTRL);

#ifdef	ZP_DEBUG
    {
	short           if_port;
	if_port = read_eeprom_data(BASE, 8) >> 14;
	sc->if_port = if_port;
	printf("Linux select:%x\n", if_port);
    }
    printf("SELECT connectors:%x\n", i);
#endif	/* ZP_DEBUG */

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
	short           tmp_addr[3];
	int             i;
	for (i = 0; i < 3; i++) {
	    tmp_addr[i] = htons(read_eeprom_data(BASE, i));
	}
	bcopy(tmp_addr, sc->arpcom.ac_enaddr, 6);
    }

    printf(" address %s\n", ether_sprintf(sc->arpcom.ac_enaddr));

    ifp->if_mtu = ETHERMTU;
    ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
    ifp->if_unit = isa_dev->id_unit;
    ifp->if_name = "zp";
    ifp->if_init = zpinit;
    ifp->if_output = ether_output;
    ifp->if_start = zpstart;
    ifp->if_ioctl = zpioctl;
    ifp->if_watchdog = zpwatchdog;
    /*
     * Select connector according to board setting.
     */
#if 0
    if (sc->if_port != 3) {
	ifp->if_flags |= IFF_LINK0;
    }
#else
    ifp->if_flags |= IFF_LINK0;
#endif

    if_attach(ifp);

    /*
     * Fill the hardware address into ifa_addr if we find an AF_LINK entry.
     * We need to do this so bpf's can get the hardware addr of this card.
     * netstat likes this too!
     */
    ifa = ifp->if_addrlist;
    while ((ifa != 0) && (ifa->ifa_addr != 0) &&
	   (ifa->ifa_addr->sa_family != AF_LINK))
	ifa = ifa->ifa_next;

    if ((ifa != 0) && (ifa->ifa_addr != 0)) {
	sdl = (struct sockaddr_dl *) ifa->ifa_addr;
	sdl->sdl_type = IFT_ETHER;
	sdl->sdl_alen = ETHER_ADDR_LEN;
	sdl->sdl_slen = 0;
	bcopy(sc->arpcom.ac_enaddr, LLADDR(sdl), ETHER_ADDR_LEN);
    }
#if NBPFILTER > 0
    bpfattach(&sc->bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
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
#endif	/* NAPM > 0 */
    return 1;
}

/*
 * The order in here seems important. Otherwise we may not receive
 * interrupts. ?!
 */
void
zpinit(unit)
    int             unit;
{
    register struct zp_softc *sc = &zp_softc[unit];
    register struct ifnet *ifp = &sc->arpcom.ac_if;
    int             s, i;

#ifdef	ZP_DEBUG
    printf("### zpinit ####\n");
#endif	/* ZP_DEBUG */

    if (ifp->if_addrlist == (struct ifaddr *) 0)
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

    /*
     * you can `ifconfig (link0|-link0) ep0' to get the following behaviour:
     * -link0	disable AUI/UTP. enable BNC. link0	disable BNC. enable
     * AUI. if the card has a UTP connector, that is enabled too. not sure,
     * but it seems you have to be careful to not plug things into both AUI &
     * UTP.
     */

    if (!(ifp->if_flags & IFF_LINK0) && (sc->ep_connectors & BNC)) {
#ifdef ZP_DEBUG
	printf("START TRANCEIVER");
#endif	/* ZP_DEBUG */
        GO_WINDOW(0);
        /* set the xcvr */
        outw(BASE + EP_W0_ADDRESS_CFG, 3 << 14);
        GO_WINDOW(2);
        outw(BASE + EP_COMMAND, START_TRANSCEIVER);
        GO_WINDOW(1);
    }
    if ((ifp->if_flags & IFF_LINK0) && (sc->ep_connectors & UTP)) {
#ifdef ZP_DEBUG
	printf("ENABLE UTP");
#endif	/* ZP_DEBUG */
	GO_WINDOW(4);
	outw(BASE + EP_W4_MEDIA_TYPE, ENABLE_UTP);
	GO_WINDOW(1);
    }
    outw(BASE + EP_COMMAND, RX_ENABLE);
    outw(BASE + EP_COMMAND, TX_ENABLE);

    ifp->if_flags |= IFF_RUNNING;
    ifp->if_flags &= ~IFF_OACTIVE;	/* just in case */
    sc->tx_start_thresh = 20;	/* probably a good starting point. */
    /*
     * Store up a bunch of mbuf's for use later. (MAX_MBS). First we free up
     * any that we had in case we're being called from intr or somewhere
     * else.
     */
    sc->last_mb = 0;
    sc->next_mb = 0;
    zpmbuffill(sc);
    zpstart(ifp);
    splx(s);
#ifdef	ZP_DEBUG
    printf("### zpinit done ####\n");
#endif	/* ZP_DEBUG */
}

static const char padmap[] = {0, 3, 2, 1};

void
zpstart(ifp)
    struct ifnet   *ifp;
{
    register struct zp_softc *sc = &zp_softc[ifp->if_unit];
    struct mbuf    *m, *top;

    int             s, len, pad;

#ifdef	ZP_DEBUG
    printf("### zpstart ####\n");
    printf("head1 = 0x%x\n", sc->arpcom.ac_if.if_snd.ifq_head);
    printf("BASE = 0x%x\n", BASE);
#endif

    s = splimp();

    if (sc->arpcom.ac_if.if_flags & IFF_OACTIVE) {
	splx(s);
#ifdef	ZP_DEBUG
	printf("### zpstart oactive ####\n");
#endif	/* ZP_DEBUG */
	return;
    }

startagain:
    /* Sneak a peek at the next packet */
    m = sc->arpcom.ac_if.if_snd.ifq_head;
#ifdef	ZP_DEBUG
    printf("head2 = 0x%x\n", sc->arpcom.ac_if.if_snd.ifq_head);
#endif
    if (m == 0) {
	splx(s);
#ifdef	ZP_DEBUG
	printf("### zpstart none data 2 ####\n");
#endif	/* EP_DEBUG */
	return;
    }
#if  0
    len = m->m_pkthdr.len;
#else
    for (len = 0, top = m; m; m = m->m_next)
	len += m->m_len;
#endif

    pad = padmap[len & 3];

    /*
     * The 3c509 automatically pads short packets to minimum ethernet length,
     * but we drop packets that are too large. Perhaps we should truncate
     * them instead?
     */
    if (len + pad > ETHER_MAX_LEN) {
	/* packet is obviously too large: toss it */
	++sc->arpcom.ac_if.if_oerrors;
	IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);
	m_freem(m);
	goto readcheck;
    }
#if 1
    if (inw(BASE + EP_W1_FREE_TX) < len + pad + 4) {
	/* no room in FIFO */
	outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH | (len + pad + 4));
	sc->arpcom.ac_if.if_flags |= IFF_OACTIVE;
	splx(s);

#ifdef	ZP_DEBUG
	printf("### zpstart no room ####\n");
#endif	/* EP_DEBUG */
	return;
    }
#else
    {
	int             i;
	if ((i = inw(BASE + EP_W1_FREE_TX)) < len + pad + 4) {
	    printf("BASE + EP_W1_FREE_TX = 0x%x\n", i);
	    /* no room in FIFO */
	    outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH | (len + pad + 4));
	    sc->arpcom.ac_if.if_flags |= IFF_OACTIVE;
	    splx(s);

	    printf("### zpstart no room ####\n");
	    return;
	}
	printf("BASE + EP_W1_FREE_TX = 0x%x\n", i);
    }
#endif
    IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);
    if (m == 0) {	/* not really needed */
	splx(s);
#ifdef	ZP_DEBUG
	printf("### zpstart ??? ####\n");
#endif	/* ZP_DEBUG */
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
#ifdef ZP_DEBUG
	    printf("Output len = %d\n", m->m_len);
#endif	/* ZP_DEBUG */
	    outsw(BASE + EP_W1_TX_PIO_WR_1, mtod(m, caddr_t), m->m_len / 2);
	    if (m->m_len & 1)
		outb(BASE + EP_W1_TX_PIO_WR_1,
		     *(mtod(m, caddr_t) + m->m_len - 1));
	}
    }
    while (pad--)
	outb(BASE + EP_W1_TX_PIO_WR_1, 0);	/* Padding */

#if NBPFILTER > 0
    if (sc->bpf) {
#if 0
	u_short         etype;
	int             off, datasize, resid;
	struct ether_header *eh;
	struct trailer_header {
	    u_short         ether_type;
	    u_short         ether_residual;
	}               trailer_header;
	char            ether_packet[ETHER_MAX_LEN];
	char           *ep;

	ep = ether_packet;

	/*
	 * We handle trailers below: Copy ether header first, then residual
	 * data, then data. Put all this in a temporary buffer 'ether_packet'
	 * and send off to bpf. Since the system has generated this packet,
	 * we assume that all of the offsets in the packet are correct; if
	 * they're not, the system will almost certainly crash in m_copydata.
	 * We make no assumptions about how the data is arranged in the mbuf
	 * chain (i.e. how much data is in each mbuf, if mbuf clusters are
	 * used, etc.), which is why we use m_copydata to get the ether
	 * header rather than assume that this is located in the first mbuf.
	 */
	/* copy ether header */
	m_copydata(top, 0, sizeof(struct ether_header), ep);
	eh = (struct ether_header *) ep;
	ep += sizeof(struct ether_header);
	eh->ether_type = etype = ntohs(eh->ether_type);
	if (etype >= ETHERTYPE_TRAIL &&
	    etype < ETHERTYPE_TRAIL + ETHERTYPE_NTRAILER) {
	    datasize = ((etype - ETHERTYPE_TRAIL) << 9);
	    off = datasize + sizeof(struct ether_header);

	    /* copy trailer_header into a data structure */
	    m_copydata(top, off, sizeof(struct trailer_header),
		       (caddr_t) & trailer_header.ether_type);

	    /* copy residual data */
	    resid = trailer_header.ether_residual -
		sizeof(struct trailer_header);
	    resid = ntohs(resid);
	    m_copydata(top, off + sizeof(struct trailer_header),
		       resid, ep);
	    ep += resid;

	    /* copy data */
	    m_copydata(top, sizeof(struct ether_header),
		       datasize, ep);
	    ep += datasize;

	    /* restore original ether packet type */
	    eh->ether_type = trailer_header.ether_type;

	    bpf_tap(sc->bpf, ether_packet, ep - ether_packet);
	} else
	    bpf_mtap(sc->bpf, top);
#endif

	bpf_mtap(sc->bpf, top);
    }
#endif

    m_freem(top);
    ++sc->arpcom.ac_if.if_opackets;
    /*
     * Is another packet coming in? We don't want to overflow the tiny RX
     * fifo.
     */
readcheck:
    if (inw(BASE + EP_W1_RX_STATUS) & RX_BYTES_MASK) {
	splx(s);
#ifdef	ZP_DEBUG
	printf("### zpstart done ####\n");
#endif	/* ZP_DEBUG */
	return;
    }
#ifdef	ZP_DEBUG2
    printf("### zpstart startagain ####\n");
#endif	/* ZP_DEBUG */
    goto startagain;
}

void
zpintr(unit)
    int             unit;
{
    int             status, i;
    register struct zp_softc *sc = &zp_softc[unit];

    struct ifnet   *ifp = &sc->arpcom.ac_if;
    struct mbuf    *m;

#ifdef	ZP_DEBUG
    printf("### zpintr ####\n");
#endif	/* ZP_DEBUG */

    status = 0;
checkintr:
    status = inw(BASE + EP_STATUS) &
	(S_TX_COMPLETE | S_TX_AVAIL | S_RX_COMPLETE | S_CARD_FAILURE);
checkintr2:
    if (status == 0) {
	/* No interrupts. */
	outw(BASE + EP_COMMAND, C_INTR_LATCH);
#ifdef	ZP_DEBUG
	printf("### zpintr done ####\n");
#endif	/* ZP_DEBUG */

	if (status = inw(BASE + EP_STATUS) &
	    (S_TX_COMPLETE | S_TX_AVAIL | S_RX_COMPLETE |
	     S_CARD_FAILURE)) {
	    goto checkintr2;
	}

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
#ifdef	ZP_DEBUG
	printf("### zpintr error ####\n");
#endif	/* ZP_DEBUG */
	return;
    }
    if (status & S_TX_COMPLETE) {
	status &= ~S_TX_COMPLETE;
	/*
	 * We need to read TX_STATUS until we get a 0 status in order to turn
	 * off the interrupt flag.
	 */
	while ((i = inb(BASE + EP_W1_TX_STATUS)) & TXS_COMPLETE) {
	    outw(BASE + EP_W1_TX_STATUS, 0x0);
#if ZE_DEBUG
	    printf("EP_W1_TX_STATUS = 0x%x\n", i);
#endif
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

void
zpread(sc)
    register struct zp_softc *sc;
{
    struct ether_header *eh;
    struct mbuf    *mcur, *m, *m0, *top;
    int             totlen, lenthisone;
    int             save_totlen;
    u_short         etype;
    int             off, resid;
    int             count, spinwait;
    int             i;

#ifdef	ZP_DEBUG
    printf("### zpread ####\n");
#endif	/* ZP_DEBUG */

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

    top = m0 = m;	/* We assign top so we can "goto out" */
#define EROUND  ((sizeof(struct ether_header) + 3) & ~3)
#define EOFF    (EROUND - sizeof(struct ether_header))
    m0->m_data += EOFF;
    /* Read what should be the header. */
    insw(BASE + EP_W1_RX_PIO_RD_1,
	 mtod(m0, caddr_t), sizeof(struct ether_header) / 2);
    m->m_len = sizeof(struct ether_header);
    totlen -= sizeof(struct ether_header);
    /*
     * mostly deal with trailer here.  (untested) We do this in a couple of
     * parts.  First we check for a trailer, if we have one we convert the
     * mbuf back to a regular mbuf and set the offset and subtract
     * sizeof(struct ether_header) from the pktlen. After we've read the
     * packet off the interface (all except for the trailer header, we then
     * get a header mbuf, read the trailer into it, and fix up the mbuf
     * pointer chain.
     */
    eh = mtod(m, struct ether_header *);
#if 0	/* by nor@aecl.ntt.jp */
    eh->ether_type = etype = ntohs((u_short) eh->ether_type);
    if (etype >= ETHERTYPE_TRAIL &&
	etype < ETHERTYPE_TRAIL + ETHERTYPE_NTRAILER) {
	m->m_data = m->m_dat;	/* Convert back to regular mbuf.  */
	m->m_flags = 0;	/* This sucks but non-trailers are the norm */
	off = (etype - ETHERTYPE_TRAIL) * 512;
	if (off >= ETHERMTU) {
	    m_freem(m);
	    return;	/* sanity */
	}
	totlen -= sizeof(struct ether_header);	/* We don't read the trailer */
	m->m_data += 2 * sizeof(u_short);	/* Get rid of type & len */
    }
#endif	/* by nor@aecl.ntt.jp */
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
	    } else {
		timeout(zpmbuffill, sc, 0);
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
    if (sc->bpf) {
	bpf_mtap(sc->bpf, top);

	/*
	 * Note that the interface cannot be in promiscuous mode if there are
	 * no BPF listeners.  And if we are in promiscuous mode, we have to
	 * check if this packet is really ours.
	 */
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

out:outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
    while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
    if (top)
	m_freem(top);

#ifdef	ZP_DEBUG
    printf("### zpread Error ####\n");
#endif	/* ZP_DEBUG */
}


/*
 * Look familiar?
 */
static int
zpioctl(ifp, cmd, data)
    register struct ifnet *ifp;
    int             cmd;
    caddr_t         data;
{
    register struct ifaddr *ifa = (struct ifaddr *) data;
    struct zp_softc *sc = &zp_softc[ifp->if_unit];
    struct ifreq   *ifr = (struct ifreq *) data;
    int             s, error = 0;

#ifdef	ZP_DEBUG
    printf("### zpioctl ####\n");
#endif	/* ZP_DEBUG */

    switch (cmd) {
    case SIOCSIFADDR:
	ifp->if_flags |= IFF_UP;
	switch (ifa->ifa_addr->sa_family) {
#ifdef INET
	case AF_INET:
	    zpinit(ifp->if_unit);	/* before arpwhohas */
#if 1
	    arp_ifinit((struct arpcom *) ifp, ifa);
#else
	    ((struct arpcom *) ifp)->ac_ipaddr = IA_SIN(ifa)->sin_addr;
	    arpwhohas((struct arpcom *) ifp, &IA_SIN(ifa)->sin_addr);
#endif
	    break;
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
#ifdef notdef
    case SIOCGHWADDR:
	bcopy((caddr_t) sc->sc_addr, (caddr_t) & ifr->ifr_data,
	      sizeof(sc->sc_addr));
	break;
#endif
    default:
	error = EINVAL;
    }
    return (error);
}

void
zpreset(unit)
    int             unit;
{
    int             s = splimp();

#ifdef	ZP_DEBUG
    printf("### zpreset ####\n");
#endif	/* ZP_DEBUG */

    zpstop(unit);
    zpinit(unit);
    splx(s);
}

void
zpwatchdog(unit)
    int             unit;
{
    struct zp_softc *sc = &zp_softc[unit];

#ifdef	ZP_DEBUG
    printf("### zpwatchdog ####\n");
#endif	/* ZP_DEBUG */

    log(LOG_ERR, "zp%d: watchdog\n", unit);
    ++sc->arpcom.ac_if.if_oerrors;
    zpreset(unit);
}

void
zpstop(unit)
    int             unit;
{
    struct zp_softc *sc = &zp_softc[unit];

#ifdef	ZP_DEBUG
    printf("### zpstop ####\n");
#endif	/* ZP_DEBUG */

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


/*
 * This is adapted straight from the book. There's probably a better way.
 */
static int
send_ID_sequence(port)
    u_short         port;
{
    char            cx, al;

#ifdef	ZP_DEBUG2
    printf("### send_ID_sequence ####\n");
#endif	/* ZP_DEBUG */

    cx = 0x0ff;
    al = 0x0ff;

    outb(port, 0x0);
    DELAY(1000);
    outb(port, 0x0);
    DELAY(1000);

loop1:cx--;
    outb(port, al);
    if (!(al & 0x80)) {
	al = al << 1;
	goto loop1;
    }
    al = al << 1;
    al ^= 0xcf;
    if (cx)
	goto loop1;

    return (1);
}


/*
 * We get eeprom data from the id_port given an offset into the
 * eeprom.  Basically; after the ID_sequence is sent to all of
 * the cards; they enter the ID_CMD state where they will accept
 * command requests. 0x80-0xbf loads the eeprom data.  We then
 * read the port 16 times and with every read; the cards check
 * for contention (ie: if one card writes a 0 bit and another
 * writes a 1 bit then the host sees a 0. At the end of the cycle;
 * each card compares the data on the bus; if there is a difference
 * then that card goes into ID_WAIT state again). In the meantime;
 * one bit of data is returned in the AX register which is conveniently
 * returned to us by inb().  Hence; we read 16 times getting one
 * bit of data with each read.
 */
static          u_short
get_eeprom_data(id_port, offset)
    int             id_port;
    int             offset;
{
    int             i, data = 0;

#ifdef	ZP_DEBUG2
    printf("### get_eeprom_data ####\n");
#endif	/* ZP_DEBUG */

    outb(id_port, 0x80 + offset);
    DELAY(1000);
    for (i = 0; i < 16; i++)
	data = (data << 1) | (inw(id_port) & 1);
    return (data);
}


static          u_short
read_eeprom_data(id_port, offset)
    int             id_port;
    int             offset;
{
    int             i, data = 0;

#ifdef	ZP_DEBUG
    printf("### read_eeprom_data ####\n");
#endif	/* ZP_DEBUG */

    outb(id_port + 10, 0x80 + offset);
    DELAY(1000);
    return inw(id_port + 12);
}



static int
f_is_eeprom_busy(is)
    struct isa_device *is;
{
    int             i = 0, j;
    register struct zp_softc *sc = &zp_softc[is->id_unit];

#ifdef	ZP_DEBUG
    printf("### f_is_eeprom_busy ####\n");
    printf("BASE: %x\n", BASE);
#endif	/* ZP_DEBUG */

    while (i++ < 100) {
	j = inw(BASE + EP_W0_EEPROM_COMMAND);
	if (j & EEPROM_BUSY)
	    DELAY(100);
	else
	    break;
    }
    if (i >= 100) {
	printf("\nzp%d: eeprom failed to come ready.\n", is->id_unit);
	return (1);
    }
    if (j & EEPROM_TST_MODE) {
	printf("\nzp%d: 3c589 in test mode. Erase pencil mark!\n", is->id_unit);

	return (1);
    }
    return (0);
}

void
zpmbuffill(sp)
    void           *sp;
{
    struct zp_softc *sc = (struct zp_softc *) sp;
    int             s, i;

#ifdef	ZP_DEBUG
    printf("### zpmbuffill ####\n");
#endif	/* ZP_DEBUG */

    s = splimp();
    i = sc->last_mb;
    do {
	if (sc->mb[i] == NULL)
	    MGET(sc->mb[i], M_DONTWAIT, MT_DATA);
	if (sc->mb[i] == NULL)
	    break;
	i = (i + 1) % MAX_MBS;
    } while (i != sc->next_mb);
    sc->last_mb = i;
    splx(s);
}

static void
zpmbufempty(sc)
    struct zp_softc *sc;
{
    int             s, i;

#ifdef	ZP_DEBUG
    printf("### zpmbufempty ####\n");
#endif	/* ZP_DEBUG */

    s = splimp();
    for (i = 0; i < MAX_MBS; i++) {
	if (sc->mb[i]) {
	    m_freem(sc->mb[i]);
	    sc->mb[i] = NULL;
	}
    }
    sc->last_mb = sc->next_mb = 0;
    untimeout(zpmbuffill, sc);
    splx(s);
}
#endif	/* NZP > 0 */
