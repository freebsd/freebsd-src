/*
 * Copyright (c) 1996 Gardner Buchanan <gbuchanan@shl.com>
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
 *      This product includes software developed by Gardner Buchanan.
 * 4. The name of Gardner Buchanan may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 *   $FreeBSD$
 */

/*
 * This is a driver for SMC's 9000 series of Ethernet adapters.
 *
 * This FreeBSD driver is derived from the smc9194 Linux driver by
 * Erik Stahlman and is Copyright (C) 1996 by Erik Stahlman.
 * This driver also shamelessly borrows from the FreeBSD ep driver
 * which is Copyright (C) 1994 Herb Peyerl <hpeyerl@novatel.ca>
 * All rights reserved.
 *
 * It is set up for my SMC91C92 equipped Ampro LittleBoard embedded
 * PC.  It is adapted from Erik Stahlman's Linux driver which worked
 * with his EFA Info*Express SVC VLB adaptor.  According to SMC's databook,
 * it will work for the entire SMC 9xxx series. (Ha Ha)
 *
 * "Features" of the SMC chip:
 *   4608 byte packet memory. (for the 91C92.  Others have more)
 *   EEPROM for configuration
 *   AUI/TP selection
 *
 * Authors:
 *      Erik Stahlman                   erik@vt.edu
 *      Herb Peyerl                     hpeyerl@novatel.ca
 *      Andres Vega Garcia              avega@sophia.inria.fr
 *      Serge Babkin                    babkin@hq.icb.chel.su
 *      Gardner Buchanan                gbuchanan@shl.com
 *
 * Sources:
 *    o   SMC databook
 *    o   "smc9194.c:v0.10(FIXED) 02/15/96 by Erik Stahlman (erik@vt.edu)"
 *    o   "if_ep.c,v 1.19 1995/01/24 20:53:45 davidg Exp"
 *
 * Known Bugs:
 *    o   The hardware multicast filter isn't used yet.
 *    o   Setting of the hardware address isn't supported.
 *    o   Hardware padding isn't used.
 */

/*
 * Modifications for Megahertz X-Jack Ethernet Card (XJ-10BT)
 * 
 * Copyright (c) 1996 by Tatsumi Hosokawa <hosokawa@jp.FreeBSD.org>
 *                       BSD-nomads, Tokyo, Japan.
 */
/*
 * Multicast support by Kei TANAKA <kei@pal.xerox.com>
 * Special thanks to itojun@itojun.org
 */
#include "sn.h"
#if NSN > 0

#undef	SN_DEBUG	/* (by hosokawa) */

#include "pnp.h"

#include <sys/param.h>
#if defined(__FreeBSD__)
#include <sys/systm.h>
#include <sys/kernel.h>
#endif
#include <sys/errno.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#if defined(__NetBSD__)
#include <sys/select.h>
#endif

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_mib.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#if defined(__FreeBSD__)
#include <machine/clock.h>
#endif

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>

/* PCCARD suport */
#define NCARD 0
#if NCARD > 0
#include <sys/select.h>
#include <sys/module.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>
#include <pccard/driver.h>
#endif /* NCARD > 0 */

#include <dev/sn/if_snreg.h>

/* for PCMCIA Ethernet */
static int	sn_pccard[NSN];	/* set to 1 if it's PCMCIA card */
static u_char	sn_pccard_macaddr[NSN][6];
static int	sn_import_macaddr[NSN];

static int snprobe __P((struct isa_device *));
static int snattach __P((struct isa_device *));
static int snioctl __P((struct ifnet * ifp, u_long, caddr_t));

static int smc_probe __P((int ioaddr, int pccard));
static void snresume __P((struct ifnet *));

void sninit     __P((void *));
static ointhand2_t snintr;
void snread     __P((struct ifnet *));
void snreset    __P((int));
void snstart    __P((struct ifnet *));
void snstop     __P((int));
void snwatchdog __P((struct ifnet *));

static void sn_setmcast(struct sn_softc *);
static int sn_getmcf(struct arpcom *ac, u_char *mcf);
static u_int smc_crc(u_char *);

/* I (GB) have been unlucky getting the hardware padding
 * to work properly.
 */
#define SW_PAD

struct sn_softc sn_softc[NSN];

struct isa_driver sndriver = {
	snprobe,
	snattach,
	"sn",
	0
};


/* PCCARD Support */
#if NCARD > 0
/*
 *	PC-Card (PCMCIA) specific code.
 */
static int sn_card_intr(struct pccard_devinfo *);	/* Interrupt handler */
static void snunload(struct pccard_devinfo *);		/* Disable driver */
static int sn_card_init(struct pccard_devinfo *);	/* init device */

PCCARD_MODULE(sn,sn_card_init,snunload,sn_card_intr,0,net_imask);

static int
sn_card_init(struct pccard_devinfo *devi)
{
	int		unit = devi->pd_unit;
	struct sn_softc *sc = &sn_softc[devi->pd_unit];

	sn_pccard[unit] = 1;
	sn_import_macaddr[unit] = 0;
	if (devi->misc[0] | devi->misc[1] | devi->misc[2]) {
		int	i;
		for (i = 0; i < 6; i++) {
			sn_pccard_macaddr[unit][i] = devi->misc[i];
		}
		sn_import_macaddr[unit] = 1;
	}
	sc->gone = 0;
	/*
	 *      validate unit number.
	 */
	if (unit >= NSN)
		return ENODEV;
	/*
	 *	Probe the device. If a value is returned, the
	 *	device was found at the location.
	 */
#ifdef	SN_DEBUG
printf("snprobe()\n");
#endif
	if (snprobe(&devi->isahd)==0)
		return ENXIO;
#ifdef	SN_DEBUG
printf("snattach()\n");
#endif
	if (snattach(&devi->isahd)==0)
		return ENXIO;
	/* initialize interface dynamically */
	sc->arpcom.ac_if.if_snd.ifq_maxlen = ifqmaxlen;

	return 0;
}

static void
snunload(struct pccard_devinfo *devi)
{
	int		unit = devi->pd_unit;
	struct sn_softc *sc = &sn_softc[devi->pd_unit];
	struct ifnet   *ifp = &sc->arpcom.ac_if;

	if (sc->gone) {
		printf("sn%d: already unloaded.\n", unit);
		return;
	}

	snstop(unit);
	sc->gone = 1;
	ifp->if_flags &= ~IFF_RUNNING;
	if_down(ifp);
	printf("sn%d: unload.\n", unit);
}

static int
sn_card_intr(struct pccard_devinfo *devi)
{
	int	unit = devi->pd_unit;
	snintr(unit);
	return(1);
}

#endif /* NCARD > 0 */


int
snprobe(struct isa_device *is)
{
	/*
	 * Device was configured with 'port ?' In this case we complain
	 */
	if (is->id_iobase == -1) {	/* port? */
		printf("sn%d: SMC91Cxx cannot determine ioaddr\n", is->id_unit);
		return 0;
	}
	/*
	 * Device was configured with 'irq ?' In this case we complain
	 */
	if (is->id_irq == 0) {
		printf("sn%d: SMC91Cxx cannot determine irq\n", is->id_unit);
		return (0);
	}
	/*
	 * Device was configured with 'port xxx', 'irq xx' In this case we
	 * search for the card with that address
	 */
	if (smc_probe(is->id_iobase, sn_pccard[is->id_unit]) != 0)
		return (0);

	return (SMC_IO_EXTENT);
}

static int
snattach(struct isa_device *is)
{
	struct sn_softc *sc = &sn_softc[is->id_unit];
	struct ifnet   *ifp = &sc->arpcom.ac_if;
	u_short         i;
	int		j;
	u_char         *p;
	struct ifaddr  *ifa;
	struct sockaddr_dl *sdl;
	int             rev;
	u_short         address;
#if NCARD > 0
	static int      alredy_ifatch[NSN];
#endif
	snstop(is->id_unit);

	is->id_ointr = snintr;

	/*
	 * This is the value used for BASE
	 */
	sc->sn_io_addr = is->id_iobase;

	sc->pages_wanted = -1;

	printf("sn%d: ", is->id_unit);

	SMC_SELECT_BANK(3);
	rev = inw(BASE + REVISION_REG_W);
	if (chip_ids[(rev >> 4) & 0xF])
		printf("%s ", chip_ids[(rev >> 4) & 0xF]);

	SMC_SELECT_BANK(1);
	i = inw(BASE + CONFIG_REG_W);
	printf(i & CR_AUI_SELECT ? "AUI" : "UTP");

	if (sn_import_macaddr[is->id_unit]) {
		for (j = 0; j < 3; j++) {
			u_short	w;

			w = (u_short)sn_pccard_macaddr[is->id_unit][j * 2] | 
				(((u_short)sn_pccard_macaddr[is->id_unit][j * 2 + 1]) << 8);
			outw(BASE + IAR_ADDR0_REG_W + j * 2, w);
		}
	}

	/*
	 * Read the station address from the chip. The MAC address is bank 1,
	 * regs 4 - 9
	 */
	SMC_SELECT_BANK(1);
	p = (u_char *) & sc->arpcom.ac_enaddr;
	for (i = 0; i < 6; i += 2) {
		address = inw(BASE + IAR_ADDR0_REG_W + i);
		p[i + 1] = address >> 8;
		p[i] = address & 0xFF;
	}
	printf(" MAC address %6D\n", sc->arpcom.ac_enaddr, ":");
	ifp->if_softc = sc;
	ifp->if_unit = is->id_unit;
	ifp->if_name = "sn";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_output = ether_output;
	ifp->if_start = snstart;
	ifp->if_ioctl = snioctl;
	ifp->if_watchdog = snwatchdog;
	ifp->if_init = sninit;

	ifp->if_timer = 0;

#if NCARD > 0
	if (alredy_ifatch[is->id_unit] != 1) {
		if_attach( ifp );
		alredy_ifatch[is->id_unit] = 1;
	}
#else
	if_attach(ifp);
#endif
	ether_ifattach(ifp);
	/*
	 * Fill the hardware address into ifa_addr if we find an AF_LINK
	 * entry. We need to do this so bpf's can get the hardware addr of
	 * this card. netstat likes this too!
	 */
	ifa = TAILQ_FIRST(&ifp->if_addrhead);
	while ((ifa != 0) && (ifa->ifa_addr != 0) &&
	       (ifa->ifa_addr->sa_family != AF_LINK))
		ifa = TAILQ_NEXT(ifa, ifa_link);

	if ((ifa != 0) && (ifa->ifa_addr != 0)) {
		sdl = (struct sockaddr_dl *) ifa->ifa_addr;
		sdl->sdl_type = IFT_ETHER;
		sdl->sdl_alen = ETHER_ADDR_LEN;
		sdl->sdl_slen = 0;
		bcopy(sc->arpcom.ac_enaddr, LLADDR(sdl), ETHER_ADDR_LEN);
	}

	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));

	return 1;
}


/*
 * Reset and initialize the chip
 */
void
sninit(void *xsc)
{
	/* register struct sn_softc *sc = &sn_softc[unit]; */
	register struct sn_softc *sc = xsc;
	register struct ifnet *ifp = &sc->arpcom.ac_if;
	int             s;
	int             flags;
	int             mask;

#if	NCARD > 0
	if (sc->gone)
		return;
#endif
	s = splimp();

	/*
	 * This resets the registers mostly to defaults, but doesn't affect
	 * EEPROM.  After the reset cycle, we pause briefly for the chip to
	 * be happy.
	 */
	SMC_SELECT_BANK(0);
	outw(BASE + RECV_CONTROL_REG_W, RCR_SOFTRESET);
	SMC_DELAY();
	outw(BASE + RECV_CONTROL_REG_W, 0x0000);
	SMC_DELAY();
	SMC_DELAY();

	outw(BASE + TXMIT_CONTROL_REG_W, 0x0000);

	/*
	 * Set the control register to automatically release succesfully
	 * transmitted packets (making the best use out of our limited
	 * memory) and to enable the EPH interrupt on certain TX errors.
	 */
	SMC_SELECT_BANK(1);
	outw(BASE + CONTROL_REG_W, (CTR_AUTO_RELEASE | CTR_TE_ENABLE |
				    CTR_CR_ENABLE | CTR_LE_ENABLE));

	/* Set squelch level to 240mV (default 480mV) */
	flags = inw(BASE + CONFIG_REG_W);
	flags |= CR_SET_SQLCH;
	outw(BASE + CONFIG_REG_W, flags);

	/*
	 * Reset the MMU and wait for it to be un-busy.
	 */
	SMC_SELECT_BANK(2);
	outw(BASE + MMU_CMD_REG_W, MMUCR_RESET);
	while (inw(BASE + MMU_CMD_REG_W) & MMUCR_BUSY)	/* NOTHING */
		;

	/*
	 * Disable all interrupts
	 */
	outb(BASE + INTR_MASK_REG_B, 0x00);

	sn_setmcast(sc);

	/*
	 * Set the transmitter control.  We want it enabled.
	 */
	flags = TCR_ENABLE;

#ifndef SW_PAD
	/*
	 * I (GB) have been unlucky getting this to work.
	 */
	flags |= TCR_PAD_ENABLE;
#endif	/* SW_PAD */

	outw(BASE + TXMIT_CONTROL_REG_W, flags);


	/*
	 * Now, enable interrupts
	 */
	SMC_SELECT_BANK(2);

	mask = IM_EPH_INT |
		IM_RX_OVRN_INT |
		IM_RCV_INT |
		IM_TX_INT;

	outb(BASE + INTR_MASK_REG_B, mask);
	sc->intr_mask = mask;
	sc->pages_wanted = -1;


	/*
	 * Mark the interface running but not active.
	 */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Attempt to push out any waiting packets.
	 */
	snstart(ifp);

	splx(s);
}


void
snstart(struct ifnet *ifp)
{
	register struct sn_softc *sc = &sn_softc[ifp->if_unit];
	register u_int  len;
	register struct mbuf *m;
	struct mbuf    *top;
	int             s, pad;
	int             mask;
	u_short         length;
	u_short         numPages;
	u_char          packet_no;
	int             time_out;

#if	NCARD > 0
	if (sc->gone)
		return;
#endif

	s = splimp();

	if (sc->arpcom.ac_if.if_flags & IFF_OACTIVE) {
		splx(s);
		return;
	}
	if (sc->pages_wanted != -1) {
		splx(s);
		printf("sn%d: snstart() while memory allocation pending\n",
		       ifp->if_unit);
		return;
	}
startagain:

	/*
	 * Sneak a peek at the next packet
	 */
	m = sc->arpcom.ac_if.if_snd.ifq_head;
	if (m == 0) {
		splx(s);
		return;
	}
	/*
	 * Compute the frame length and set pad to give an overall even
	 * number of bytes.  Below we assume that the packet length is even.
	 */
	for (len = 0, top = m; m; m = m->m_next)
		len += m->m_len;

	pad = (len & 1);

	/*
	 * We drop packets that are too large. Perhaps we should truncate
	 * them instead?
	 */
	if (len + pad > ETHER_MAX_LEN - ETHER_CRC_LEN) {

		printf("sn%d: large packet discarded (A)\n", ifp->if_unit);

		++sc->arpcom.ac_if.if_oerrors;
		IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);
		m_freem(m);
		goto readcheck;
	}
#ifdef SW_PAD

	/*
	 * If HW padding is not turned on, then pad to ETHER_MIN_LEN.
	 */
	if (len < ETHER_MIN_LEN - ETHER_CRC_LEN)
		pad = ETHER_MIN_LEN - ETHER_CRC_LEN - len;

#endif	/* SW_PAD */

	length = pad + len;

	/*
	 * The MMU wants the number of pages to be the number of 256 byte
	 * 'pages', minus 1 (A packet can't ever have 0 pages. We also
	 * include space for the status word, byte count and control bytes in
	 * the allocation request.
	 */
	numPages = (length + 6) >> 8;


	/*
	 * Now, try to allocate the memory
	 */
	SMC_SELECT_BANK(2);
	outw(BASE + MMU_CMD_REG_W, MMUCR_ALLOC | numPages);

	/*
	 * Wait a short amount of time to see if the allocation request
	 * completes.  Otherwise, I enable the interrupt and wait for
	 * completion asyncronously.
	 */

	time_out = MEMORY_WAIT_TIME;
	do {
		if (inb(BASE + INTR_STAT_REG_B) & IM_ALLOC_INT)
			break;
	} while (--time_out);

	if (!time_out) {

		/*
		 * No memory now.  Oh well, wait until the chip finds memory
		 * later.   Remember how many pages we were asking for and
		 * enable the allocation completion interrupt. Also set a
		 * watchdog in case  we miss the interrupt. We mark the
		 * interface active since there is no point in attempting an
		 * snstart() until after the memory is available.
		 */
		mask = inb(BASE + INTR_MASK_REG_B) | IM_ALLOC_INT;
		outb(BASE + INTR_MASK_REG_B, mask);
		sc->intr_mask = mask;

		sc->arpcom.ac_if.if_timer = 1;
		sc->arpcom.ac_if.if_flags |= IFF_OACTIVE;
		sc->pages_wanted = numPages;

		splx(s);
		return;
	}
	/*
	 * The memory allocation completed.  Check the results.
	 */
	packet_no = inb(BASE + ALLOC_RESULT_REG_B);
	if (packet_no & ARR_FAILED) {
		printf("sn%d: Memory allocation failed\n", ifp->if_unit);
		goto startagain;
	}
	/*
	 * We have a packet number, so tell the card to use it.
	 */
	outb(BASE + PACKET_NUM_REG_B, packet_no);

	/*
	 * Point to the beginning of the packet
	 */
	outw(BASE + POINTER_REG_W, PTR_AUTOINC | 0x0000);

	/*
	 * Send the packet length (+6 for status, length and control byte)
	 * and the status word (set to zeros)
	 */
	outw(BASE + DATA_REG_W, 0);
	outb(BASE + DATA_REG_B, (length + 6) & 0xFF);
	outb(BASE + DATA_REG_B, (length + 6) >> 8);

	/*
	 * Get the packet from the kernel.  This will include the Ethernet
	 * frame header, MAC Addresses etc.
	 */
	IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);

	/*
	 * Push out the data to the card.
	 */
	for (top = m; m != 0; m = m->m_next) {

		/*
		 * Push out words.
		 */
		outsw(BASE + DATA_REG_W, mtod(m, caddr_t), m->m_len / 2);

		/*
		 * Push out remaining byte.
		 */
		if (m->m_len & 1)
			outb(BASE + DATA_REG_B, *(mtod(m, caddr_t) + m->m_len - 1));
	}

	/*
	 * Push out padding.
	 */
	while (pad > 1) {
		outw(BASE + DATA_REG_W, 0);
		pad -= 2;
	}
	if (pad)
		outb(BASE + DATA_REG_B, 0);

	/*
	 * Push out control byte and unused packet byte The control byte is 0
	 * meaning the packet is even lengthed and no special CRC handling is
	 * desired.
	 */
	outw(BASE + DATA_REG_W, 0);

	/*
	 * Enable the interrupts and let the chipset deal with it Also set a
	 * watchdog in case we miss the interrupt.
	 */
	mask = inb(BASE + INTR_MASK_REG_B) | (IM_TX_INT | IM_TX_EMPTY_INT);
	outb(BASE + INTR_MASK_REG_B, mask);
	sc->intr_mask = mask;

	outw(BASE + MMU_CMD_REG_W, MMUCR_ENQUEUE);

	sc->arpcom.ac_if.if_flags |= IFF_OACTIVE;
	sc->arpcom.ac_if.if_timer = 1;

	if (ifp->if_bpf) {
		bpf_mtap(ifp, top);
	}

	sc->arpcom.ac_if.if_opackets++;
	m_freem(top);


readcheck:

	/*
	 * Is another packet coming in?  We don't want to overflow the tiny
	 * RX FIFO.  If nothing has arrived then attempt to queue another
	 * transmit packet.
	 */
	if (inw(BASE + FIFO_PORTS_REG_W) & FIFO_REMPTY)
		goto startagain;

	splx(s);
	return;
}



/* Resume a packet transmit operation after a memory allocation
 * has completed.
 *
 * This is basically a hacked up copy of snstart() which handles
 * a completed memory allocation the same way snstart() does.
 * It then passes control to snstart to handle any other queued
 * packets.
 */
static void
snresume(struct ifnet *ifp)
{
	register struct sn_softc *sc = &sn_softc[ifp->if_unit];
	register u_int  len;
	register struct mbuf *m;
	struct mbuf    *top;
	int             pad;
	int             mask;
	u_short         length;
	u_short         numPages;
	u_short         pages_wanted;
	u_char          packet_no;

	if (sc->pages_wanted < 0)
		return;

	pages_wanted = sc->pages_wanted;
	sc->pages_wanted = -1;

	/*
	 * Sneak a peek at the next packet
	 */
	m = sc->arpcom.ac_if.if_snd.ifq_head;
	if (m == 0) {
		printf("sn%d: snresume() with nothing to send\n", ifp->if_unit);
		return;
	}
	/*
	 * Compute the frame length and set pad to give an overall even
	 * number of bytes.  Below we assume that the packet length is even.
	 */
	for (len = 0, top = m; m; m = m->m_next)
		len += m->m_len;

	pad = (len & 1);

	/*
	 * We drop packets that are too large. Perhaps we should truncate
	 * them instead?
	 */
	if (len + pad > ETHER_MAX_LEN - ETHER_CRC_LEN) {

		printf("sn%d: large packet discarded (B)\n", ifp->if_unit);

		++sc->arpcom.ac_if.if_oerrors;
		IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);
		m_freem(m);
		return;
	}
#ifdef SW_PAD

	/*
	 * If HW padding is not turned on, then pad to ETHER_MIN_LEN.
	 */
	if (len < ETHER_MIN_LEN - ETHER_CRC_LEN)
		pad = ETHER_MIN_LEN - ETHER_CRC_LEN - len;

#endif	/* SW_PAD */

	length = pad + len;


	/*
	 * The MMU wants the number of pages to be the number of 256 byte
	 * 'pages', minus 1 (A packet can't ever have 0 pages. We also
	 * include space for the status word, byte count and control bytes in
	 * the allocation request.
	 */
	numPages = (length + 6) >> 8;


	SMC_SELECT_BANK(2);

	/*
	 * The memory allocation completed.  Check the results. If it failed,
	 * we simply set a watchdog timer and hope for the best.
	 */
	packet_no = inb(BASE + ALLOC_RESULT_REG_B);
	if (packet_no & ARR_FAILED) {

		printf("sn%d: Memory allocation failed.  Weird.\n", ifp->if_unit);

		sc->arpcom.ac_if.if_timer = 1;

		goto try_start;
		return;
	}
	/*
	 * We have a packet number, so tell the card to use it.
	 */
	outb(BASE + PACKET_NUM_REG_B, packet_no);

	/*
	 * Now, numPages should match the pages_wanted recorded when the
	 * memory allocation was initiated.
	 */
	if (pages_wanted != numPages) {

		printf("sn%d: memory allocation wrong size.  Weird.\n", ifp->if_unit);

		/*
		 * If the allocation was the wrong size we simply release the
		 * memory once it is granted. Wait for the MMU to be un-busy.
		 */
		while (inw(BASE + MMU_CMD_REG_W) & MMUCR_BUSY)	/* NOTHING */
			;
		outw(BASE + MMU_CMD_REG_W, MMUCR_FREEPKT);

		return;
	}
	/*
	 * Point to the beginning of the packet
	 */
	outw(BASE + POINTER_REG_W, PTR_AUTOINC | 0x0000);

	/*
	 * Send the packet length (+6 for status, length and control byte)
	 * and the status word (set to zeros)
	 */
	outw(BASE + DATA_REG_W, 0);
	outb(BASE + DATA_REG_B, (length + 6) & 0xFF);
	outb(BASE + DATA_REG_B, (length + 6) >> 8);

	/*
	 * Get the packet from the kernel.  This will include the Ethernet
	 * frame header, MAC Addresses etc.
	 */
	IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);

	/*
	 * Push out the data to the card.
	 */
	for (top = m; m != 0; m = m->m_next) {

		/*
		 * Push out words.
		 */
		outsw(BASE + DATA_REG_W, mtod(m, caddr_t), m->m_len / 2);

		/*
		 * Push out remaining byte.
		 */
		if (m->m_len & 1)
			outb(BASE + DATA_REG_B, *(mtod(m, caddr_t) + m->m_len - 1));
	}

	/*
	 * Push out padding.
	 */
	while (pad > 1) {
		outw(BASE + DATA_REG_W, 0);
		pad -= 2;
	}
	if (pad)
		outb(BASE + DATA_REG_B, 0);

	/*
	 * Push out control byte and unused packet byte The control byte is 0
	 * meaning the packet is even lengthed and no special CRC handling is
	 * desired.
	 */
	outw(BASE + DATA_REG_W, 0);

	/*
	 * Enable the interrupts and let the chipset deal with it Also set a
	 * watchdog in case we miss the interrupt.
	 */
	mask = inb(BASE + INTR_MASK_REG_B) | (IM_TX_INT | IM_TX_EMPTY_INT);
	outb(BASE + INTR_MASK_REG_B, mask);
	sc->intr_mask = mask;
	outw(BASE + MMU_CMD_REG_W, MMUCR_ENQUEUE);

	if (ifp->if_bpf) {
		bpf_mtap(ifp, top);
	}

	sc->arpcom.ac_if.if_opackets++;
	m_freem(top);

try_start:

	/*
	 * Now pass control to snstart() to queue any additional packets
	 */
	sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
	snstart(ifp);

	/*
	 * We've sent something, so we're active.  Set a watchdog in case the
	 * TX_EMPTY interrupt is lost.
	 */
	sc->arpcom.ac_if.if_flags |= IFF_OACTIVE;
	sc->arpcom.ac_if.if_timer = 1;

	return;
}


void
snintr(int unit)
{
	int             status, interrupts;
	register struct sn_softc *sc = &sn_softc[unit];
	struct ifnet   *ifp = &sc->arpcom.ac_if;
	int             x;

	/*
	 * Chip state registers
	 */
	u_char          mask;
	u_char          packet_no;
	u_short         tx_status;
	u_short         card_stats;

#if	NCARD > 0
	if (sc->gone)
		return;
#endif

	/*
	 * if_ep.c did this, so I do too.  Yet if_ed.c doesn't. I wonder...
	 */
	x = splbio();

	/*
	 * Clear the watchdog.
	 */
	ifp->if_timer = 0;

	SMC_SELECT_BANK(2);

	/*
	 * Obtain the current interrupt mask and clear the hardware mask
	 * while servicing interrupts.
	 */
	mask = inb(BASE + INTR_MASK_REG_B);
	outb(BASE + INTR_MASK_REG_B, 0x00);

	/*
	 * Get the set of interrupts which occurred and eliminate any which
	 * are masked.
	 */
	interrupts = inb(BASE + INTR_STAT_REG_B);
	status = interrupts & mask;

	/*
	 * Now, process each of the interrupt types.
	 */

	/*
	 * Receive Overrun.
	 */
	if (status & IM_RX_OVRN_INT) {

		/*
		 * Acknowlege Interrupt
		 */
		SMC_SELECT_BANK(2);
		outb(BASE + INTR_ACK_REG_B, IM_RX_OVRN_INT);

		++sc->arpcom.ac_if.if_ierrors;
	}
	/*
	 * Got a packet.
	 */
	if (status & IM_RCV_INT) {
#if 1
		int             packet_number;

		SMC_SELECT_BANK(2);
		packet_number = inw(BASE + FIFO_PORTS_REG_W);

		if (packet_number & FIFO_REMPTY) {

			/*
			 * we got called , but nothing was on the FIFO
			 */
			printf("sn: Receive interrupt with nothing on FIFO\n");

			goto out;
		}
#endif
		snread(ifp);
	}
	/*
	 * An on-card memory allocation came through.
	 */
	if (status & IM_ALLOC_INT) {

		/*
		 * Disable this interrupt.
		 */
		mask &= ~IM_ALLOC_INT;
		sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
		snresume(&sc->arpcom.ac_if);
	}
	/*
	 * TX Completion.  Handle a transmit error message. This will only be
	 * called when there is an error, because of the AUTO_RELEASE mode.
	 */
	if (status & IM_TX_INT) {

		/*
		 * Acknowlege Interrupt
		 */
		SMC_SELECT_BANK(2);
		outb(BASE + INTR_ACK_REG_B, IM_TX_INT);

		packet_no = inw(BASE + FIFO_PORTS_REG_W);
		packet_no &= FIFO_TX_MASK;

		/*
		 * select this as the packet to read from
		 */
		outb(BASE + PACKET_NUM_REG_B, packet_no);

		/*
		 * Position the pointer to the first word from this packet
		 */
		outw(BASE + POINTER_REG_W, PTR_AUTOINC | PTR_READ | 0x0000);

		/*
		 * Fetch the TX status word.  The value found here will be a
		 * copy of the EPH_STATUS_REG_W at the time the transmit
		 * failed.
		 */
		tx_status = inw(BASE + DATA_REG_W);

		if (tx_status & EPHSR_TX_SUC) {
			printf("sn%d: Successful packet caused interrupt\n", unit);
		} else {
			++sc->arpcom.ac_if.if_oerrors;
		}

		if (tx_status & EPHSR_LATCOL)
			++sc->arpcom.ac_if.if_collisions;

		/*
		 * Some of these errors will have disabled transmit.
		 * Re-enable transmit now.
		 */
		SMC_SELECT_BANK(0);

#ifdef SW_PAD
		outw(BASE + TXMIT_CONTROL_REG_W, TCR_ENABLE);
#else
		outw(BASE + TXMIT_CONTROL_REG_W, TCR_ENABLE | TCR_PAD_ENABLE);
#endif	/* SW_PAD */

		/*
		 * kill the failed packet. Wait for the MMU to be un-busy.
		 */
		SMC_SELECT_BANK(2);
		while (inw(BASE + MMU_CMD_REG_W) & MMUCR_BUSY)	/* NOTHING */
			;
		outw(BASE + MMU_CMD_REG_W, MMUCR_FREEPKT);

		/*
		 * Attempt to queue more transmits.
		 */
		sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
		snstart(&sc->arpcom.ac_if);
	}
	/*
	 * Transmit underrun.  We use this opportunity to update transmit
	 * statistics from the card.
	 */
	if (status & IM_TX_EMPTY_INT) {

		/*
		 * Acknowlege Interrupt
		 */
		SMC_SELECT_BANK(2);
		outb(BASE + INTR_ACK_REG_B, IM_TX_EMPTY_INT);

		/*
		 * Disable this interrupt.
		 */
		mask &= ~IM_TX_EMPTY_INT;

		SMC_SELECT_BANK(0);
		card_stats = inw(BASE + COUNTER_REG_W);

		/*
		 * Single collisions
		 */
		sc->arpcom.ac_if.if_collisions += card_stats & ECR_COLN_MASK;

		/*
		 * Multiple collisions
		 */
		sc->arpcom.ac_if.if_collisions += (card_stats & ECR_MCOLN_MASK) >> 4;

		SMC_SELECT_BANK(2);

		/*
		 * Attempt to enqueue some more stuff.
		 */
		sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
		snstart(&sc->arpcom.ac_if);
	}
	/*
	 * Some other error.  Try to fix it by resetting the adapter.
	 */
	if (status & IM_EPH_INT) {
		snstop(unit);
		sninit(sc);
	}

out:
	/*
	 * Handled all interrupt sources.
	 */

	SMC_SELECT_BANK(2);

	/*
	 * Reestablish interrupts from mask which have not been deselected
	 * during this interrupt.  Note that the hardware mask, which was set
	 * to 0x00 at the start of this service routine, may have been
	 * updated by one or more of the interrupt handers and we must let
	 * those new interrupts stay enabled here.
	 */
	mask |= inb(BASE + INTR_MASK_REG_B);
	outb(BASE + INTR_MASK_REG_B, mask);
	sc->intr_mask = mask;

	splx(x);
}

void
snread(register struct ifnet *ifp)
{
        struct sn_softc *sc = &sn_softc[ifp->if_unit];
	struct ether_header *eh;
	struct mbuf    *m;
	short           status;
	int             packet_number;
	u_short         packet_length;
	u_char         *data;

	SMC_SELECT_BANK(2);
#if 0
	packet_number = inw(BASE + FIFO_PORTS_REG_W);

	if (packet_number & FIFO_REMPTY) {

		/*
		 * we got called , but nothing was on the FIFO
		 */
		printf("sn: Receive interrupt with nothing on FIFO\n");
		return;
	}
#endif
read_another:

	/*
	 * Start reading from the start of the packet. Since PTR_RCV is set,
	 * packet number is found in FIFO_PORTS_REG_W, FIFO_RX_MASK.
	 */
	outw(BASE + POINTER_REG_W, PTR_READ | PTR_RCV | PTR_AUTOINC | 0x0000);

	/*
	 * First two words are status and packet_length
	 */
	status = inw(BASE + DATA_REG_W);
	packet_length = inw(BASE + DATA_REG_W) & RLEN_MASK;

	/*
	 * The packet length contains 3 extra words: status, length, and a
	 * extra word with the control byte.
	 */
	packet_length -= 6;

	/*
	 * Account for receive errors and discard.
	 */
	if (status & RS_ERRORS) {
		++sc->arpcom.ac_if.if_ierrors;
		goto out;
	}
	/*
	 * A packet is received.
	 */

	/*
	 * Adjust for odd-length packet.
	 */
	if (status & RS_ODDFRAME)
		packet_length++;

	/*
	 * Allocate a header mbuf from the kernel.
	 */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		goto out;

	m->m_pkthdr.rcvif = &sc->arpcom.ac_if;
	m->m_pkthdr.len = m->m_len = packet_length;

	/*
	 * Attach an mbuf cluster
	 */
	MCLGET(m, M_DONTWAIT);

	/*
	 * Insist on getting a cluster
	 */
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		++sc->arpcom.ac_if.if_ierrors;
		printf("sn: snread() kernel memory allocation problem\n");
		goto out;
	}
	eh = mtod(m, struct ether_header *);

	/*
	 * Get packet, including link layer address, from interface.
	 */

	data = (u_char *) eh;
	insw(BASE + DATA_REG_W, data, packet_length >> 1);
	if (packet_length & 1) {
		data += packet_length & ~1;
		*data = inb(BASE + DATA_REG_B);
	}
	++sc->arpcom.ac_if.if_ipackets;

	if (sc->arpcom.ac_if.if_bpf)
	{
		bpf_mtap(&sc->arpcom.ac_if, m);

		/*
		 * Note that the interface cannot be in promiscuous mode if
		 * there are no BPF listeners.  And if we are in promiscuous
		 * mode, we have to check if this packet is really ours.
		 */
		if ((sc->arpcom.ac_if.if_flags & IFF_PROMISC) &&
		    (eh->ether_dhost[0] & 1) == 0 &&
		    bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr,
			 sizeof(eh->ether_dhost)) != 0 &&
		    bcmp(eh->ether_dhost, etherbroadcastaddr,
			 sizeof(eh->ether_dhost)) != 0) {
			m_freem(m);
			goto out;
		}
	}

	/*
	 * Remove link layer addresses and whatnot.
	 */
	m->m_pkthdr.len = m->m_len = packet_length - sizeof(struct ether_header);
	m->m_data += sizeof(struct ether_header);

	ether_input(&sc->arpcom.ac_if, eh, m);

out:

	/*
	 * Error or good, tell the card to get rid of this packet Wait for
	 * the MMU to be un-busy.
	 */
	SMC_SELECT_BANK(2);
	while (inw(BASE + MMU_CMD_REG_W) & MMUCR_BUSY)	/* NOTHING */
		;
	outw(BASE + MMU_CMD_REG_W, MMUCR_RELEASE);

	/*
	 * Check whether another packet is ready
	 */
	packet_number = inw(BASE + FIFO_PORTS_REG_W);
	if (packet_number & FIFO_REMPTY) {
		return;
	}
	goto read_another;
}


/*
 * Handle IOCTLS.  This function is completely stolen from if_ep.c
 * As with its progenitor, it does not handle hardware address
 * changes.
 */
static int
snioctl(register struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct sn_softc *sc = &sn_softc[ifp->if_unit];
	int             s, error = 0;
#if !defined(__FreeBSD__) || __FreeBSD_version < 300000
	struct ifreq   *ifr = (struct ifreq *) data;
#endif

#if	NCARD > 0
	if (sc->gone) {
		ifp->if_flags &= ~IFF_RUNNING;
		return ENXIO;
	}
#endif

	s = splimp();

	switch (cmd) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, cmd, data);
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 && ifp->if_flags & IFF_RUNNING) {
			ifp->if_flags &= ~IFF_RUNNING;
			snstop(ifp->if_unit);
			break;
		} else {
			/* reinitialize card on any parameter change */
			sninit(sc);
			break;
		}
		break;

#ifdef notdef
	case SIOCGHWADDR:
		bcopy((caddr_t) sc->sc_addr, (caddr_t) & ifr->ifr_data,
		      sizeof(sc->sc_addr));
		break;
#endif

	case SIOCADDMULTI:
#if defined(__FreeBSD__) && __FreeBSD_version >= 300000
	    /* update multicast filter list. */
	    sn_setmcast(sc);
	    error = 0;
#else
	    error = ether_addmulti(ifr, &sc->arpcom);
	    if (error == ENETRESET) {
		/* update multicast filter list. */
	        sn_setmcast(sc);
		error = 0;
	    }
#endif
	    break;
	case SIOCDELMULTI:
#if defined(__FreeBSD__) && __FreeBSD_version >= 300000
	    /* update multicast filter list. */
	    sn_setmcast(sc);
	    error = 0;
#else
	    error = ether_delmulti(ifr, &sc->arpcom);
	    if (error == ENETRESET) {
		/* update multicast filter list. */
	        sn_setmcast(sc);
		error = 0;
	    }
#endif
	    break;
	default:
		error = EINVAL;
	}

	splx(s);

	return (error);
}

void
snreset(int unit)
{
	int	s;
	struct sn_softc *sc = &sn_softc[unit];

#if NCARD > 0
	if (sc->gone) 
		return;
#endif
	s = splimp();
	snstop(unit);
	sninit(sc);

	splx(s);
}

void
snwatchdog(struct ifnet *ifp)
{
	int	s;
#if NCARD > 0
	struct sn_softc *sc = &sn_softc[ifp->if_unit];

	if (sc->gone)
		return;
#endif
	s = splimp();
	snintr(ifp->if_unit);
	splx(s);
}


/* 1. zero the interrupt mask
 * 2. clear the enable receive flag
 * 3. clear the enable xmit flags
 */
void
snstop(int unit)
{
	struct sn_softc *sc = &sn_softc[unit];
	struct ifnet   *ifp = &sc->arpcom.ac_if;

#if NCARD > 0
	if (sc->gone)
		return;
#endif
	/*
	 * Clear interrupt mask; disable all interrupts.
	 */
	SMC_SELECT_BANK(2);
	outb(BASE + INTR_MASK_REG_B, 0x00);

	/*
	 * Disable transmitter and Receiver
	 */
	SMC_SELECT_BANK(0);
	outw(BASE + RECV_CONTROL_REG_W, 0x0000);
	outw(BASE + TXMIT_CONTROL_REG_W, 0x0000);

	/*
	 * Cancel watchdog.
	 */
	ifp->if_timer = 0;
}



/*
 * Function: smc_probe( int ioaddr, int pccard )
 *
 * Purpose:
 *      Tests to see if a given ioaddr points to an SMC9xxx chip.
 *      Tries to cause as little damage as possible if it's not a SMC chip.
 *      Returns a 0 on success
 *
 * Algorithm:
 *      (1) see if the high byte of BANK_SELECT is 0x33
 *      (2) compare the ioaddr with the base register's address
 *      (3) see if I recognize the chip ID in the appropriate register
 *
 *
 */
static int 
smc_probe(int ioaddr, int pccard)
{
	u_int           bank;
	u_short         revision_register;
	u_short         base_address_register;

	/*
	 * First, see if the high byte is 0x33
	 */
	bank = inw(ioaddr + BANK_SELECT_REG_W);
	if ((bank & BSR_DETECT_MASK) != BSR_DETECT_VALUE) {
#ifdef	SN_DEBUG
printf("test1 failed\n");
#endif
		return -ENODEV;
	}
	/*
	 * The above MIGHT indicate a device, but I need to write to further
	 * test this.  Go to bank 0, then test that the register still
	 * reports the high byte is 0x33.
	 */
	outw(ioaddr + BANK_SELECT_REG_W, 0x0000);
	bank = inw(ioaddr + BANK_SELECT_REG_W);
	if ((bank & BSR_DETECT_MASK) != BSR_DETECT_VALUE) {
#ifdef	SN_DEBUG
printf("test2 failed\n");
#endif
		return -ENODEV;
	}
	/*
	 * well, we've already written once, so hopefully another time won't
	 * hurt.  This time, I need to switch the bank register to bank 1, so
	 * I can access the base address register.  The contents of the
	 * BASE_ADDR_REG_W register, after some jiggery pokery, is expected
	 * to match the I/O port address where the adapter is being probed.
	 */
	outw(ioaddr + BANK_SELECT_REG_W, 0x0001);
	base_address_register = inw(ioaddr + BASE_ADDR_REG_W);

	/*
	 * This test is nonsence on PC-card architecture, so if 
	 * pccard == 1, skip this test. (hosokawa)
	 */
	if (!pccard && (ioaddr != (base_address_register >> 3 & 0x3E0))) {

		/*
		 * Well, the base address register didn't match.  Must not
		 * have been a SMC chip after all.
		 */
		/*
		 * printf("sn: ioaddr %x doesn't match card configuration
		 * (%x)\n", ioaddr, base_address_register >> 3 & 0x3E0 );
		 */

#ifdef	SN_DEBUG
printf("test3 failed ioaddr = 0x%x, base_address_register = 0x%x\n",
	ioaddr, base_address_register >> 3 & 0x3E0);
#endif
		return -ENODEV;
	}
	/*
	 * Check if the revision register is something that I recognize.
	 * These might need to be added to later, as future revisions could
	 * be added.
	 */
	outw(ioaddr + BANK_SELECT_REG_W, 0x3);
	revision_register = inw(ioaddr + REVISION_REG_W);
	if (!chip_ids[(revision_register >> 4) & 0xF]) {

		/*
		 * I don't regonize this chip, so...
		 */
		/*
		 * printf("sn: ioaddr %x unrecognized revision register:
		 * %x\n", ioaddr, revision_register );
		 */

#ifdef	SN_DEBUG
printf("test4 failed\n");
#endif
		return -ENODEV;
	}
	/*
	 * at this point I'll assume that the chip is an SMC9xxx. It might be
	 * prudent to check a listing of MAC addresses against the hardware
	 * address, or do some other tests.
	 */
	return 0;
}

#define MCFSZ 8

static void
sn_setmcast(struct sn_softc *sc)
{
	struct ifnet *ifp = (struct ifnet *)sc;
	int flags;

	/*
	 * Set the receiver filter.  We want receive enabled and auto strip
	 * of CRC from received packet.  If we are promiscuous then set that
	 * bit too.
	 */
	flags = RCR_ENABLE | RCR_STRIP_CRC;
  
	if (ifp->if_flags & IFF_PROMISC) {
		flags |= RCR_PROMISC | RCR_ALMUL;
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		flags |= RCR_ALMUL;
	} else {
		u_char mcf[MCFSZ];
		if (sn_getmcf(&sc->arpcom, mcf)) {
			/* set filter */
			SMC_SELECT_BANK(3);
			outw(BASE + MULTICAST1_REG_W,
			    ((u_short)mcf[1] << 8) |  mcf[0]);
			outw(BASE + MULTICAST2_REG_W,
			    ((u_short)mcf[3] << 8) |  mcf[2]);
			outw(BASE + MULTICAST3_REG_W,
			    ((u_short)mcf[5] << 8) |  mcf[4]);
			outw(BASE + MULTICAST4_REG_W,
			    ((u_short)mcf[7] << 8) |  mcf[6]);
		} else {
			flags |= RCR_ALMUL;
		}
	}
	SMC_SELECT_BANK(0);
	outw(BASE + RECV_CONTROL_REG_W, flags);
}

#if defined(__FreeBSD__) && __FreeBSD_version >= 300000
static int
sn_getmcf(struct arpcom *ac, u_char *mcf)
{
	int i;
	register u_int index, index2;
	register u_char *af = (u_char *) mcf;
	struct ifmultiaddr *ifma;

	bzero(mcf, MCFSZ);

	for (ifma = ac->ac_if.if_multiaddrs.lh_first; ifma;
	     ifma = ifma->ifma_link.le_next) {
	    if (ifma->ifma_addr->sa_family != AF_LINK)
		return 0;
	    index = smc_crc(LLADDR((struct sockaddr_dl *)ifma->ifma_addr)) & 0x3f;
	    index2 = 0;
	    for (i = 0; i < 6; i++) {
		index2 <<= 1;
		index2 |= (index & 0x01);
		index >>= 1;
	    }
	    af[index2 >> 3] |= 1 << (index2 & 7);
	}
	return 1;  /* use multicast filter */
}
#else
static int
sn_getmcf(struct arpcom *ac, u_char *mcf)
{
	int i;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int a, b;

	bzero(mcf, MCFSZ);
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, 6) != 0) {
			/* impossible to hash */
			bzero(mcf, MCFSZ);
			return 0;
		}
		a = smc_crc(enm->enm_addrlo) & 0x3f;
		b = 0;
		for (i=0; i < 6; i++) {
			b <<= 1;
			b |= (a & 0x01);
			a >>= 1;
		}
		mcf[b >> 3] |= 1 << (b & 7);
		ETHER_NEXT_MULTI(step, enm);
	}
	return 1;  /* use multicast filter */
}
#endif

static u_int
smc_crc(u_char *s)
{
	int perByte;
	int perBit;
	const u_int poly = 0xedb88320;
	u_int v = 0xffffffff;
	u_char c;
  
	for (perByte = 0; perByte < ETHER_ADDR_LEN; perByte++) {
		c = s[perByte];
		for (perBit = 0; perBit < 8; perBit++) {
			v = (v >> 1)^(((v ^ c) & 0x01) ? poly : 0);
			c >>= 1;
		}
	}
	return v;
}
#endif
