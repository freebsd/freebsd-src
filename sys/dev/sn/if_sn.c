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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h> 

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


#include <dev/sn/if_snreg.h>
#include <dev/sn/if_snvar.h>

/* Exported variables */
devclass_t sn_devclass;

static int snioctl(struct ifnet * ifp, u_long, caddr_t);

static void snresume(struct ifnet *);

void sninit(void *);
void snread(struct ifnet *);
void snreset(struct sn_softc *);
void snstart(struct ifnet *);
void snstop(struct sn_softc *);
void snwatchdog(struct ifnet *);

static void sn_setmcast(struct sn_softc *);
static int sn_getmcf(struct arpcom *ac, u_char *mcf);
static u_int smc_crc(u_char *);

/* I (GB) have been unlucky getting the hardware padding
 * to work properly.
 */
#define SW_PAD

static const char *chip_ids[15] = {
	NULL, NULL, NULL,
	 /* 3 */ "SMC91C90/91C92",
	 /* 4 */ "SMC91C94",
	 /* 5 */ "SMC91C95",
	NULL,
	 /* 7 */ "SMC91C100",
	 /* 8 */ "SMC91C100FD",
	NULL, NULL, NULL,
	NULL, NULL, NULL
};

int
sn_attach(device_t dev)
{
	struct sn_softc *sc = device_get_softc(dev);
	struct ifnet   *ifp = &sc->arpcom.ac_if;
	u_short         i;
	u_char         *p;
	struct ifaddr  *ifa;
	struct sockaddr_dl *sdl;
	int             rev;
	u_short         address;
	int		j;

	sn_activate(dev);

	snstop(sc);

	sc->dev = dev;
	sc->pages_wanted = -1;

	device_printf(dev, " ");

	SMC_SELECT_BANK(3);
	rev = inw(BASE + REVISION_REG_W);
	if (chip_ids[(rev >> 4) & 0xF])
		printf("%s ", chip_ids[(rev >> 4) & 0xF]);

	SMC_SELECT_BANK(1);
	i = inw(BASE + CONFIG_REG_W);
	printf(i & CR_AUI_SELECT ? "AUI" : "UTP");

	if (sc->pccard_enaddr)
		for (j = 0; j < 3; j++) {
			u_short	w;

			w = (u_short)sc->arpcom.ac_enaddr[j * 2] | 
				(((u_short)sc->arpcom.ac_enaddr[j * 2 + 1]) << 8);
			outw(BASE + IAR_ADDR0_REG_W + j * 2, w);
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
	ifp->if_unit = device_get_unit(dev);
	ifp->if_name = "sn";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_output = ether_output;
	ifp->if_start = snstart;
	ifp->if_ioctl = snioctl;
	ifp->if_watchdog = snwatchdog;
	ifp->if_init = sninit;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	ifp->if_timer = 0;

	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);

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

	return 0;
}


int
sn_detach(device_t dev)
{
	struct sn_softc *sc = device_get_softc(dev);

	sc->arpcom.ac_if.if_flags &= ~IFF_RUNNING; 
	ether_ifdetach(&sc->arpcom.ac_if, ETHER_BPF_SUPPORTED);
	sn_deactivate(dev);
	return 0;
}

/*
 * Reset and initialize the chip
 */
void
sninit(void *xsc)
{
	register struct sn_softc *sc = xsc;
	register struct ifnet *ifp = &sc->arpcom.ac_if;
	int             s;
	int             flags;
	int             mask;

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
	register struct sn_softc *sc = ifp->if_softc;
	register u_int  len;
	register struct mbuf *m;
	struct mbuf    *top;
	int             s, pad;
	int             mask;
	u_short         length;
	u_short         numPages;
	u_char          packet_no;
	int             time_out;
	int		junk = 0;

	s = splimp();

	if (sc->arpcom.ac_if.if_flags & IFF_OACTIVE) {
		splx(s);
		return;
	}
	if (sc->pages_wanted != -1) {
		splx(s);
		if_printf(ifp, "snstart() while memory allocation pending\n");
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
		if_printf(ifp, "large packet discarded (A)\n");
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

	if (!time_out || junk > 10) {

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
		if (junk++ > 10)
			if_printf(ifp, "Memory allocation failed\n");
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
	register struct sn_softc *sc = ifp->if_softc;
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
		if_printf(ifp, "snresume() with nothing to send\n");
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
		if_printf(ifp, "large packet discarded (B)\n");
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
		if_printf(ifp, "Memory allocation failed.  Weird.\n");
		sc->arpcom.ac_if.if_timer = 1;
		goto try_start;
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
		if_printf(ifp, "memory allocation wrong size.  Weird.\n");
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
sn_intr(void *arg)
{
	int             status, interrupts;
	register struct sn_softc *sc = (struct sn_softc *) arg;
	struct ifnet   *ifp = &sc->arpcom.ac_if;
	int             x;

	/*
	 * Chip state registers
	 */
	u_char          mask;
	u_char          packet_no;
	u_short         tx_status;
	u_short         card_stats;

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
			device_printf(sc->dev, 
			    "Successful packet caused interrupt\n");
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
		snstop(sc);
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
        struct sn_softc *sc = ifp->if_softc;
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
	struct sn_softc *sc = ifp->if_softc;
	int             s, error = 0;

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
			snstop(sc);
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
	    /* update multicast filter list. */
	    sn_setmcast(sc);
	    error = 0;
	    break;
	case SIOCDELMULTI:
	    /* update multicast filter list. */
	    sn_setmcast(sc);
	    error = 0;
	    break;
	default:
		error = EINVAL;
	}

	splx(s);

	return (error);
}

void
snreset(struct sn_softc *sc)
{
	int	s;
	
	s = splimp();
	snstop(sc);
	sninit(sc);

	splx(s);
}

void
snwatchdog(struct ifnet *ifp)
{
	int	s;
	s = splimp();
	sn_intr(ifp->if_softc);
	splx(s);
}


/* 1. zero the interrupt mask
 * 2. clear the enable receive flag
 * 3. clear the enable xmit flags
 */
void
snstop(struct sn_softc *sc)
{
	
	struct ifnet   *ifp = &sc->arpcom.ac_if;

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


int
sn_activate(device_t dev)
{
	struct sn_softc *sc = device_get_softc(dev);
	int err;

	sc->port_rid = 0;
	sc->port_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->port_rid,
	    0, ~0, SMC_IO_EXTENT, RF_ACTIVE);
	if (!sc->port_res) {
		if (bootverbose)
			device_printf(dev, "Cannot allocate ioport\n");
		return ENOMEM;
	}

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irq_rid, 
	    0, ~0, 1, RF_ACTIVE);
	if (!sc->irq_res) {
		if (bootverbose)
			device_printf(dev, "Cannot allocate irq\n");
		sn_deactivate(dev);
		return ENOMEM;
	}
	if ((err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET, sn_intr, sc,
	    &sc->intrhand)) != 0) {
		sn_deactivate(dev);
		return err;
	}
	
	sc->sn_io_addr = rman_get_start(sc->port_res);
	return (0);
}

void
sn_deactivate(device_t dev)
{
	struct sn_softc *sc = device_get_softc(dev);
	
	if (sc->intrhand)
		bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	sc->intrhand = 0;
	if (sc->port_res)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->port_rid, 
		    sc->port_res);
	sc->port_res = 0;
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, 
		    sc->irq_res);
	sc->irq_res = 0;
	return;
}

/*
 * Function: sn_probe( device_t dev, int pccard )
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
int 
sn_probe(device_t dev, int pccard)
{
	struct sn_softc *sc = device_get_softc(dev);
	u_int           bank;
	u_short         revision_register;
	u_short         base_address_register;
	u_short		ioaddr;
	int		err;

	if ((err = sn_activate(dev)) != 0)
		return err;

	ioaddr = sc->sn_io_addr;
#ifdef SN_DEBUG
	device_printf(dev, "ioaddr is 0x%x\n", ioaddr);
#endif
	/*
	 * First, see if the high byte is 0x33
	 */
	bank = inw(ioaddr + BANK_SELECT_REG_W);
	if ((bank & BSR_DETECT_MASK) != BSR_DETECT_VALUE) {
#ifdef	SN_DEBUG
		device_printf(dev, "test1 failed\n");
#endif
		goto error;
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
		device_printf(dev, "test2 failed\n");
#endif
		goto error;
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
#ifdef	SN_DEBUG
		device_printf(dev, "test3 failed ioaddr = 0x%x, "
		    "base_address_register = 0x%x\n", ioaddr,
		    base_address_register >> 3 & 0x3E0);
#endif
		goto error;
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
#ifdef	SN_DEBUG
		device_printf(dev, "test4 failed\n");
#endif
		goto error;
	}

	/*
	 * at this point I'll assume that the chip is an SMC9xxx. It might be
	 * prudent to check a listing of MAC addresses against the hardware
	 * address, or do some other tests.
	 */
	sn_deactivate(dev);
	return 0;
 error:
	sn_deactivate(dev);
	return ENXIO;
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

static int
sn_getmcf(struct arpcom *ac, u_char *mcf)
{
	int i;
	register u_int index, index2;
	register u_char *af = (u_char *) mcf;
	struct ifmultiaddr *ifma;

	bzero(mcf, MCFSZ);

	TAILQ_FOREACH(ifma, &ac->ac_if.if_multiaddrs, ifma_link) {
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
