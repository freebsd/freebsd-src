/*
 * Copyright 1998, Joerg Wunsch
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *
 * $FreeBSD$
 */

/*
 * Device driver for RealTek RTL 8002 (`REDP') based pocket-ethernet
 * adapters, hooked up to a printer port.  `rdp' is a shorthand for
 * REDP since some tools like netstat work best if the interface name
 * has no more than three letters.
 *
 * Driver configuration flags so far:
 *   flags 0x1 -- assume 74S288 EEPROM (default 94C46)
 *   flags 0x2 -- use `slow' mode (mode 3 of the packet driver, default 0)
 *
 * Maybe this driver will some day also work with the successor, RTL
 * 8012 (`AREDP'), which is unfortunately not fully register-
 * compatible with the 8002.  The 8012 offers support for faster
 * transfer modi like bidirectional SPP and EPP, 64 K x 4 buffer
 * memory as opposed to 16 K x 4 for the 8002, a multicast filter, and
 * a builtin multiplexer that allows chaining a printer behind the
 * ethernet adapter.
 *
 * About the only documentation i've been able to find about the RTL
 * 8002 was the packet driver source code at ftp.realtek.com.tw, so
 * this driver is somewhat based on the way the packet driver handles
 * the chip.  The exact author of the packet driver is unknown, the
 * only name that i could find in the source was someone called Chiu,
 * supposedly an employee of RealTek.  So credits to them for that
 * piece of code which has proven valuable to me.
 *
 * Later on, Leo kuo <leo@realtek.com.tw> has been very helpful to me
 * by sending me a readable (PDF) file documenting the RTL 8012, which
 * helped me to also understand the 8002, as well as by providing me
 * with the source code of the 8012 packet driver that i haven't been
 * able to find on the FTP site.  A big Thanks! goes here to RealTek
 * for this kind of service.
 */

#include "rdp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_mib.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <net/bpf.h>

#include <machine/md_var.h>

#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>
#include <i386/isa/if_rdpreg.h>
#include <i386/isa/intr_machdep.h>

#ifndef COMPAT_OLDISA
#error "The rdp device requires the old isa compatibility shims"
#endif

#define IOCTL_CMD_T u_long

/*
 * Debug levels (ORed together):
 *  != 0 - general (bad packets etc.)
 *  2 - debug EEPROM IO
 *  4 - debug interrupt status
 */
#undef DEBUG
#define DEBUG 0

/*
 * rdp_softc: per interface info and status
 */
struct rdp_softc {
	struct arpcom arpcom;	/*
				 * Ethernet common, always goes first so
				 * a rdp_softc * can be cast into an
				 * arpcom * or into an ifnet *.
				 */

	/*
	 * local stuff, somewhat sorted by memory alignment class
	 */
	u_short baseaddr;	/* IO port address */
	u_short txsize;		/* tx size for next (buffered) packet,
				 * there's only one additional packet
				 * we can buffer, thus a single variable
				 * ought to be enough */
	int txbusy;		/* tx is transmitting */
	int txbuffered;		/* # of packets in tx buffer */
	int slow;		/* use lpt_control to send data */
	u_char irqenbit;	/* mirror of current Ctrl_IRQEN */
	/*
	 * type of parameter EEPROM; device flags 0x1 selects 74S288
	 */
	enum {
		EEPROM_93C46, EEPROM_74S288 /* or 82S123 */
	} eeprom;
};

static struct rdp_softc rdp_softc[NRDP];

/*
 * Since there's no fixed location in the EEPROM about where to find
 * the ethernet hardware address, we drop a table of valid OUIs here,
 * and search through the EEPROM until we find a possible valid
 * Ethernet address.  Only the first 16 bits of all possible OUIs are
 * recorded in the table (as obtained from
 * http://standards.ieee.org/regauth/oui/oui.txt).
 */

static u_short allowed_ouis[] = {
	0x0000, 0x0001, 0x0002, 0x0004, 0x0005, 0x0006, 0x0007,
	0x0008, 0x0010, 0x001C, 0x0020, 0x0040, 0x0050, 0x0060,
	0x0070, 0x0080, 0x0090, 0x009D, 0x00A0, 0x00AA, 0x00BB,
	0x00C0, 0x00CF, 0x00DD, 0x00E0, 0x00E6, 0x0207, 0x021C,
	0x0260, 0x0270, 0x029D, 0x02AA, 0x02BB, 0x02C0, 0x02CF,
	0x02E6, 0x040A, 0x04E0, 0x0800, 0x08BB, 0x1000, 0x1100,
	0x8000, 0xAA00
};

/*
 * ISA bus support.
 */
static int rdp_probe		(struct isa_device *);
static int rdp_attach		(struct isa_device *);

/*
 * Required entry points.
 */
static void rdp_init(void *);
static int rdp_ioctl(struct ifnet *, IOCTL_CMD_T, caddr_t);
static void rdp_start(struct ifnet *);
static void rdp_reset(struct ifnet *);
static void rdp_watchdog(struct ifnet *);
static void rdpintr(int);

/*
 * REDP private functions.
 */

static void rdp_stop(struct rdp_softc *);
static void rdp_rint(struct rdp_softc *);
static void rdp_get_packet(struct rdp_softc *, unsigned);
static u_short rdp_write_mbufs(struct rdp_softc *, struct mbuf *);
static int rdp_gethwaddr_93c46(struct rdp_softc *, u_char *);
static void rdp_gethwaddr_74s288(struct rdp_softc *, u_char *);
static void rdp_93c46_cmd(struct rdp_softc *, u_short, unsigned);
static u_short rdp_93c46_read(struct rdp_softc *);

struct isa_driver rdpdriver = {
	INTR_TYPE_NET,
	rdp_probe,
	rdp_attach,
	"rdp",
	1			/* we wanna get a chance before lptN */
};
COMPAT_ISA_DRIVER(rdp, rdpdriver);

/*
 * REDP-specific functions.
 *
 * They are inlined, thus go first in this file.  Together with gcc's
 * usual optimization, these functions probably come close to the
 * packet driver's hand-optimized code. ;-)
 *
 * Comments are partially obtained from the packet driver as well.
 * Some of the function names contain register names which don't make
 * much sense for us, but i've kept them for easier reference in
 * comparision to the packet driver.
 *
 * Some of the functions are currently not used by the driver; it's
 * not quite clear whether we ever need them at all.  They are
 * supposedly even slower than what is currently implemented as `slow'
 * mode.  Right now, `fast' (default) mode is what the packet driver
 * calls mode 0, slow mode is mode 3 (writing through lpt_control,
 * reading twice).
 *
 * We should autoprobe the modi, as opposed to making them dependent
 * on a kernel configuration flag.
 */

/*
 * read a nibble from rreg; end-of-data cmd is not issued;
 * used for general register read.
 *
 * Unlike the packet driver's version, i'm shifting the result
 * by 3 here (as opposed to within the caller's code) for clarity.
 *  -- Joerg
 */
static __inline u_char
RdNib(struct rdp_softc *sc, u_char rreg)
{

	outb(sc->baseaddr + lpt_data, EOC + rreg);
	outb(sc->baseaddr + lpt_data, RdAddr + rreg); /* write addr */
	(void)inb(sc->baseaddr + lpt_status);
	return (inb(sc->baseaddr + lpt_status) >> 3) & 0x0f;
}

#if 0
/*
 * read a byte from MAR register through lpt_data; the low nibble is
 * read prior to the high one; end-of-read command is not issued; used
 * for remote DMA in mode 4 + 5
 */
static __inline u_char
RdByte(struct rdp_softc *sc)
{
	u_char hinib, lonib;

	outb(sc->baseaddr + lpt_data, RdAddr + MAR); /* cmd for low nibble */
	lonib = (inb(sc->baseaddr + lpt_status) >> 3) & 0x0f;
	outb(sc->baseaddr + lpt_data, RdAddr + MAR + HNib);
	hinib = (inb(sc->baseaddr + lpt_status) << 1) & 0xf0;
	return hinib + lonib;
}


/*
 * read a byte from MAR register through lpt_data; the low nibble is
 * read prior to the high one; end-of-read command is not issued; used
 * for remote DMA in mode 6 + 7
 */
static __inline u_char
RdByte1(struct rdp_softc *sc)
{
	u_char hinib, lonib;

	outb(sc->baseaddr + lpt_data, RdAddr + MAR); /* cmd for low nibble */
	(void)inb(sc->baseaddr + lpt_status);
	lonib = (inb(sc->baseaddr + lpt_status) >> 3) & 0x0f;
	outb(sc->baseaddr + lpt_data, RdAddr + MAR + HNib);
	(void)inb(sc->baseaddr + lpt_status);
	hinib = (inb(sc->baseaddr + lpt_status) << 1) & 0xf0;
	return hinib + lonib;
}
#endif


/*
 * read a byte from MAR register through lpt_control; the low nibble is
 * read prior to the high one; end-of-read command is not issued; used
 * for remote DMA in mode 0 + 1
 */
static __inline u_char
RdByteA1(struct rdp_softc *sc)
{
	u_char hinib, lonib;

	outb(sc->baseaddr + lpt_control, Ctrl_LNibRead);
	lonib = (inb(sc->baseaddr + lpt_status) >> 3) & 0x0f;
	outb(sc->baseaddr + lpt_control, Ctrl_HNibRead);
	hinib = (inb(sc->baseaddr + lpt_status) << 1) & 0xf0;
	return hinib + lonib;
}


/*
 * read a byte from MAR register through lpt_control; the low nibble is
 * read prior to the high one; end-of-read command is not issued; used
 * for remote DMA in mode 2 + 3
 */
static __inline u_char
RdByteA2(struct rdp_softc *sc)
{
	u_char hinib, lonib;

	outb(sc->baseaddr + lpt_control, Ctrl_LNibRead);
	(void)inb(sc->baseaddr + lpt_status);
	lonib = (inb(sc->baseaddr + lpt_status) >> 3) & 0x0f;
	outb(sc->baseaddr + lpt_control, Ctrl_HNibRead);
	(void)inb(sc->baseaddr + lpt_status);
	hinib = (inb(sc->baseaddr + lpt_status) << 1) & 0xf0;
	return hinib + lonib;
}

/*
 * End-of-read cmd
 */
static __inline void
RdEnd(struct rdp_softc *sc, u_char rreg)
{

	outb(sc->baseaddr + lpt_data, EOC + rreg);
}

/*
 * Write a nibble to a register; end-of-write is issued.
 * Used for general register write.
 */
static __inline void
WrNib(struct rdp_softc *sc, u_char wreg, u_char wdata)
{

	/* prepare and write address */
	outb(sc->baseaddr + lpt_data, EOC + wreg);
	outb(sc->baseaddr + lpt_data, WrAddr + wreg);
	outb(sc->baseaddr + lpt_data, WrAddr + wreg);
	/* prepare and write data */
	outb(sc->baseaddr + lpt_data, WrAddr + wdata);
	outb(sc->baseaddr + lpt_data, wdata);
	outb(sc->baseaddr + lpt_data, wdata);
	/* end-of-write */
	outb(sc->baseaddr + lpt_data, EOC + wdata);
}

/*
 * Write a byte to a register; end-of-write is issued.
 * Used for general register write.
 */
static __inline void
WrByte(struct rdp_softc *sc, u_char wreg, u_char wdata)
{

	/* prepare and write address */
	outb(sc->baseaddr + lpt_data, EOC + wreg);
	outb(sc->baseaddr + lpt_data, WrAddr + wreg);
	outb(sc->baseaddr + lpt_data, WrAddr + wreg);
	/* prepare and write low nibble */
	outb(sc->baseaddr + lpt_data, WrAddr + (wdata & 0x0F));
	outb(sc->baseaddr + lpt_data, (wdata & 0x0F));
	outb(sc->baseaddr + lpt_data, (wdata & 0x0F));
	/* prepare and write high nibble */
	wdata >>= 4;
	outb(sc->baseaddr + lpt_data, wdata);
	outb(sc->baseaddr + lpt_data, wdata + HNib);
	outb(sc->baseaddr + lpt_data, wdata + HNib);
	/* end-of-write */
	outb(sc->baseaddr + lpt_data, EOC + wdata + HNib);
}

/*
 * Write the byte to DRAM via lpt_data;
 * used for remote DMA write in mode 0 / 2 / 4
 */
static __inline void
WrByteALToDRAM(struct rdp_softc *sc, u_char val)
{

	outb(sc->baseaddr + lpt_data, val & 0x0F);
	outb(sc->baseaddr + lpt_data, MkHi(val));
}

/*
 * Write the byte to DRAM via lpt_control;
 * used for remote DMA write in mode 1 / 3 / 5
 */
static __inline void
WrByteALToDRAMA(struct rdp_softc *sc, u_char val)
{

	outb(sc->baseaddr + lpt_data, val & 0x0F);
	outb(sc->baseaddr + lpt_control, Ctrl_LNibRead | sc->irqenbit);
	outb(sc->baseaddr + lpt_data, val >> 4);
	outb(sc->baseaddr + lpt_control, Ctrl_HNibRead | sc->irqenbit);
}

#if 0 /* they could be used for the RAM test */
/*
 * Write the u_short to DRAM via lpt_data;
 * used for remote DMA write in mode 0 / 2 / 4
 */
static __inline void
WrWordbxToDRAM(struct rdp_softc *sc, u_short val)
{

	outb(sc->baseaddr + lpt_data, val & 0x0F);
	val >>= 4;
	outb(sc->baseaddr + lpt_data, (val & 0x0F) + HNib);
	val >>= 4;
	outb(sc->baseaddr + lpt_data, val & 0x0F);
	val >>= 4;
	outb(sc->baseaddr + lpt_data, val + HNib);
}


/*
 * Write the u_short to DRAM via lpt_control;
 * used for remote DMA write in mode 1 / 3 / 5
 */
static __inline void
WrWordbxToDRAMA(struct rdp_softc *sc, u_short val)
{

	outb(sc->baseaddr + lpt_data, val & 0x0F);
	outb(sc->baseaddr + lpt_control, Ctrl_LNibRead | sc->irqenbit);
	val >>= 4;
	outb(sc->baseaddr + lpt_data, (val & 0x0F) + HNib);
	outb(sc->baseaddr + lpt_control, Ctrl_HNibRead | sc->irqenbit);
	val >>= 4;
	outb(sc->baseaddr + lpt_data, val & 0x0F);
	outb(sc->baseaddr + lpt_control, Ctrl_LNibRead | sc->irqenbit);
	val >>= 4;
	outb(sc->baseaddr + lpt_data, val + HNib);
	outb(sc->baseaddr + lpt_control, Ctrl_HNibRead | sc->irqenbit);
}
#endif


/*
 * Determine if the device is present
 *
 *   on entry:
 * 	a pointer to an isa_device struct
 *   on exit:
 *	0 if device not found
 *	or # of i/o addresses used (if found)
 */
static int
rdp_probe(struct isa_device *isa_dev)
{
	int unit = isa_dev->id_unit;
	struct rdp_softc *sc = &rdp_softc[unit];
	u_char b1, b2;
	intrmask_t irqmap[3];
	u_char sval[3];

	if (unit < 0 || unit >= NRDP)
		return 0;

	sc->baseaddr = isa_dev->id_iobase;
	if (isa_dev->id_flags & 1)
		sc->eeprom = EEPROM_74S288;
	/* else defaults to 93C46 */
	if (isa_dev->id_flags & 2)
		sc->slow = 1;

	/* let R/WB = A/DB = CSB = high to be ready for next r/w cycle */
	outb(sc->baseaddr + lpt_data, 0xFF);
	/* DIR = 0 for write mode, IRQEN=0, SLCT=INIT=AUTOFEED=STB=high */
	outb(sc->baseaddr + lpt_control, Ctrl_SelData);
	/* software reset */
	WrNib(sc, CMR1 + HNib, MkHi(CMR1_RST));
	DELAY(2000);
	/* is EPLC alive? */
	b1 = RdNib(sc, CMR1);
	RdEnd(sc, CMR1);
	b2 = RdNib(sc, CMR2) & 0x0f;
	b2 |= RdNib(sc, CMR2 + HNib) << 4;
	RdEnd(sc, CMR2 + HNib);
	/*
	 * After the reset, we expect CMR1 & 7 to be 1 (rx buffer empty),
	 * and CMR2 & 0xf7 to be 0x20 (receive mode set to physical and
	 * broadcasts).
	 */
	if (bootverbose)
		printf("rdp%d: CMR1 = %#x, CMR2 = %#x\n", unit, b1, b2);

	if ((b1 & (CMR1_BUFE | CMR1_IRQ | CMR1_TRA)) != CMR1_BUFE
	    || (b2 & ~CMR2_IRQINV) != CMR2_AM_PB)
		return 0;

	/*
	 * We have found something that could be a RTL 80[01]2, now
	 * see whether we can generate an interrupt.
	 */
	disable_intr();

	/*
	 * Test whether our configured IRQ is working.
	 *
	 * Set to no acception mode + IRQout, then enable RxE + TxE,
	 * then cause RBER (by advancing the read pointer although
	 * the read buffer is empty) to generate an interrupt.
	 */
	WrByte(sc, CMR2, CMR2_IRQOUT);
	WrNib(sc, CMR1 + HNib, MkHi(CMR1_TE | CMR1_RE));
	WrNib(sc, CMR1, CMR1_RDPAC);
	DELAY(1000);

	irqmap[0] = isa_irq_pending();
	sval[0] = inb(sc->baseaddr + lpt_status);

	/* allow IRQs to pass the parallel interface */
	outb(sc->baseaddr + lpt_control, Ctrl_IRQEN + Ctrl_SelData);
	DELAY(1000);
	/* generate interrupt */
	WrNib(sc, IMR + HNib, MkHi(ISR_RBER));
	DELAY(1000);

	irqmap[1] = isa_irq_pending();
	sval[1] = inb(sc->baseaddr + lpt_status);

	/* de-assert and disable IRQ */
	WrNib(sc, IMR + HNib, MkHi(0));
	(void)inb(sc->baseaddr + lpt_status); /* might be necessary to
						 clear IRQ */
	DELAY(1000);
	irqmap[2] = isa_irq_pending();
	sval[2] = inb(sc->baseaddr + lpt_status);

	WrNib(sc, CMR1 + HNib, MkHi(0));
	outb(sc->baseaddr + lpt_control, Ctrl_SelData);
	WrNib(sc, CMR2, CMR2_IRQINV);

	enable_intr();

	if (bootverbose)
		printf("rdp%d: irq maps / lpt status "
		       "%#x/%#x - %#x/%#x - %#x/%#x (id_irq %#x)\n",
		       unit, irqmap[0], sval[0], irqmap[1], sval[1],
		       irqmap[2], sval[2], isa_dev->id_irq);

	if ((irqmap[1] & isa_dev->id_irq) == 0) {
		printf("rdp%d: configured IRQ (%d) cannot be asserted "
		       "by device",
		       unit, ffs(isa_dev->id_irq) - 1);
		if (irqmap[1])
			printf(" (probable IRQ: %d)", ffs(irqmap[1]) - 1);
		printf("\n");
		return 0;
	}

	/*
	 * XXX should do RAMtest here
	 */

	switch (sc->eeprom) {
	case EEPROM_93C46:
		if (rdp_gethwaddr_93c46(sc, sc->arpcom.ac_enaddr) == 0) {
			printf("rdp%d: failed to find a valid hardware "
			       "address in EEPROM\n",
			       unit);
			return 0;
		}
		break;

	case EEPROM_74S288:
		rdp_gethwaddr_74s288(sc, sc->arpcom.ac_enaddr);
		break;
	}

	return lpt_control + 1;
}

/*
 * Install interface into kernel networking data structures
 */
static int
rdp_attach(struct isa_device *isa_dev)
{
	int unit = isa_dev->id_unit;
	struct rdp_softc *sc = &rdp_softc[unit];
	struct ifnet *ifp = &sc->arpcom.ac_if;

	isa_dev->id_ointr = rdpintr;

	/*
	 * Reset interface
	 */
	rdp_stop(sc);

	if (!ifp->if_name) {
		/*
		 * Initialize ifnet structure
		 */
		ifp->if_softc = sc;
		ifp->if_unit = unit;
		ifp->if_name = "rdp";
		ifp->if_output = ether_output;
		ifp->if_start = rdp_start;
		ifp->if_ioctl = rdp_ioctl;
		ifp->if_watchdog = rdp_watchdog;
		ifp->if_init = rdp_init;
		ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
		ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX;

		/*
		 * Attach the interface
		 */
		ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
	}

	/*
	 * Print additional info when attached
	 */
	printf("%s%d: RealTek RTL%s pocket ethernet, EEPROM %s, %s mode\n",
	       ifp->if_name, ifp->if_unit,
	       "8002",		/* hook for 8012 */
	       sc->eeprom == EEPROM_93C46? "93C46": "74S288",
	       sc->slow? "slow": "fast");
	printf("%s%d: address %6D\n", ifp->if_name, ifp->if_unit,
	       sc->arpcom.ac_enaddr, ":");

	return 1;
}

/*
 * Reset interface.
 */
static void
rdp_reset(struct ifnet *ifp)
{
	struct rdp_softc *sc = ifp->if_softc;
	int s;

	s = splimp();

	/*
	 * Stop interface and re-initialize.
	 */
	rdp_stop(sc);
	rdp_init(sc);

	(void) splx(s);
}

/*
 * Take interface offline.
 */
static void
rdp_stop(struct rdp_softc *sc)
{

	sc->txbusy = sc->txbusy = 0;

	/* disable printer interface interrupts */
	sc->irqenbit = 0;
	outb(sc->baseaddr + lpt_control, Ctrl_SelData);
	outb(sc->baseaddr + lpt_data, 0xff);

	/* reset the RTL 8002 */
	WrNib(sc, CMR1 + HNib, MkHi(CMR1_RST));
	DELAY(100);
}

/*
 * Device timeout/watchdog routine. Entered if the device neglects to
 * generate an interrupt after a transmit has been started on it.
 */
static void
rdp_watchdog(struct ifnet *ifp)
{

	log(LOG_ERR, "rdp%d: device timeout\n", ifp->if_unit);
	ifp->if_oerrors++;

	rdp_reset(ifp);
}

/*
 * Initialize device.
 */
static void
rdp_init(void *xsc)
{
	struct rdp_softc *sc = xsc;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int i, s;
	u_char reg;

	/* address not known */
	if (TAILQ_EMPTY(&ifp->if_addrhead))
		return;

	s = splimp();

	ifp->if_timer = 0;

	/* program ethernet ID into the chip */
	for (i = 0, reg = IDR0; i < 6; i++, reg++)
		WrByte(sc, reg, sc->arpcom.ac_enaddr[i]);

	/* set accept mode */
	WrNib(sc, CMR2 + HNib,
	      MkHi((ifp->if_flags & IFF_PROMISC)? CMR2_AM_ALL: CMR2_AM_PB));

	/* enable tx and rx */
	WrNib(sc, CMR1 + HNib, MkHi(CMR1_TE | CMR1_RE));

	/* allow interrupts to happen */
	WrNib(sc, CMR2, CMR2_IRQOUT | CMR2_IRQINV);
	WrNib(sc, IMR, ISR_TOK | ISR_TER | ISR_ROK | ISR_RER);
	WrNib(sc, IMR + HNib, MkHi(ISR_RBER));

	/* allow IRQs to pass the parallel interface */
	sc->irqenbit = Ctrl_IRQEN;
	outb(sc->baseaddr + lpt_control, sc->irqenbit + Ctrl_SelData);

	/* clear all flags */
	sc->txbusy = sc->txbuffered = 0;

	/*
	 * Set 'running' flag, and clear output active flag.
	 */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * ...and attempt to start output
	 */
	rdp_start(ifp);

	(void) splx(s);
}

/*
 * Start output on interface.
 * We make two assumptions here:
 *  1) that the current priority is set to splimp _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) that the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 */
static void
rdp_start(struct ifnet *ifp)
{
	struct rdp_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int len;

outloop:

	/*
	 * See if there is room to put another packet in the buffer.
	 */
	if (sc->txbuffered) {
		/*
		 * No room. Indicate this to the outside world and exit.
		 */
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}
	IF_DEQUEUE(&ifp->if_snd, m);
	if (m == 0) {
		/*
		 * We are using the !OACTIVE flag to indicate to the outside
		 * world that we can accept an additional packet rather than
		 * that the transmitter is _actually_ active. Indeed, the
		 * transmitter may be active, but if we haven't filled all the
		 * buffers with data then we still want to accept more.
		 */
		ifp->if_flags &= ~IFF_OACTIVE;
		return;
	}

	/*
	 * Copy the mbuf chain into the transmit buffer
	 */

	len = rdp_write_mbufs(sc, m);
	if (len == 0)
		goto outloop;

	/* ensure minimal valid ethernet length */
	len = max(len, (ETHER_MIN_LEN-ETHER_CRC_LEN));

	/*
	 * Actually start the transceiver.  Set a timeout in case the
	 * Tx interrupt never arrives.
	 */
	if (!sc->txbusy) {
		WrNib(sc, TBCR1, len >> 8);
		WrByte(sc, TBCR0, len & 0xff);
		WrNib(sc, CMR1, CMR1_TRA);
		sc->txbusy = 1;
		ifp->if_timer = 2;
	} else {
		sc->txbuffered = 1;
		sc->txsize = len;
	}

	/*
	 * Tap off here if there is a bpf listener.
	 */
	if (ifp->if_bpf) {
		bpf_mtap(ifp, m);
	}

	m_freem(m);

	/*
	 * Loop back to the top to possibly buffer more packets
	 */
	goto outloop;
}

/*
 * Process an ioctl request.
 */
static int
rdp_ioctl(struct ifnet *ifp, IOCTL_CMD_T command, caddr_t data)
{
	struct rdp_softc *sc = ifp->if_softc;
	int s, error = 0;

	s = splimp();

	switch (command) {

	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;

	case SIOCSIFFLAGS:
		/*
		 * If the interface is marked up and stopped, then start it.
		 * If it is marked down and running, then stop it.
		 */
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_flags & IFF_RUNNING) == 0)
				rdp_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				rdp_stop(sc);
				ifp->if_flags &= ~IFF_RUNNING;
			}
		}

		/*
		 * Promiscuous flag may have changed, propagage this
		 * to the NIC.
		 */
		if (ifp->if_flags & IFF_UP)
			WrNib(sc, CMR2 + HNib,
			      MkHi((ifp->if_flags & IFF_PROMISC)?
				   CMR2_AM_ALL: CMR2_AM_PB));

		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/*
		 * Multicast list has changed; we don't support it.
		 */
		error = ENOTTY;
		break;

	default:
		error = EINVAL;
	}
	(void) splx(s);
	return (error);
}

/*
 * External interrupt service routine.
 */
void 
rdpintr(int unit)
{
	struct rdp_softc *sc = rdp_softc + unit;
	struct ifnet *ifp = (struct ifnet *)sc;
	u_char isr, tsr, rsr, colls;

	/* disable interrupts, so SD3 can be routed to the pin */
	sc->irqenbit = 0;
	outb(sc->baseaddr + lpt_control, Ctrl_SelData);
	WrNib(sc, CMR2, CMR2_IRQINV);
	/*
	 * loop until there are no more new interrupts
	 */
	for (;;) {
		isr = RdNib(sc, ISR);
		isr |= RdNib(sc, ISR + HNib) << 4;
		RdEnd(sc, ISR + HNib);

		if (isr == 0)
			break;
#if DEBUG & 4
		printf("rdp%d: ISR = %#x\n", unit, isr);
#endif

		/*
		 * Clear the pending interrupt bits.
		 */
		WrNib(sc, ISR, isr & 0x0f);
		if (isr & 0xf0)
			WrNib(sc, ISR + HNib, MkHi(isr));

		/*
		 * Handle transmitter interrupts.
		 */
		if (isr & (ISR_TOK | ISR_TER)) {
			tsr = RdNib(sc, TSR);
			RdEnd(sc, TSR);
#if DEBUG & 4
			if (isr & ISR_TER)
				printf("rdp%d: tsr %#x\n", unit, tsr);
#endif
			if (tsr & TSR_TABT)
				ifp->if_oerrors++;
			else
				/*
				 * Update total number of successfully
				 * transmitted packets.
				 */
				ifp->if_opackets++;

			if (tsr & TSR_COL) {
				colls = RdNib(sc, COLR);
				RdEnd(sc, COLR);
				ifp->if_collisions += colls;
			}

			/*
			 * reset tx busy and output active flags
			 */
			sc->txbusy = 0;
			ifp->if_flags &= ~IFF_OACTIVE;

			/*
			 * If we had already queued up another packet,
			 * start sending it now.
			 */
			if (sc->txbuffered) {
				WrNib(sc, TBCR1, sc->txsize >> 8);
				WrByte(sc, TBCR0, sc->txsize & 0xff);
				WrNib(sc, CMR1, CMR1_TRA);
				sc->txbusy = 1;
				sc->txbuffered = 0;
				ifp->if_timer = 2;
			} else {
				/*
				 * clear watchdog timer
				 */
				ifp->if_timer = 0;
			}
			
		}

		/*
		 * Handle receiver interrupts
		 */
		if (isr & (ISR_ROK | ISR_RER | ISR_RBER)) {
			rsr = RdNib(sc, RSR);
			rsr |= RdNib(sc, RSR + HNib) << 4;
			RdEnd(sc, RSR + HNib);
#if DEBUG & 4
			if (isr & (ISR_RER | ISR_RBER))
				printf("rdp%d: rsr %#x\n", unit, rsr);
#endif

			if (rsr & (RSR_PUN | RSR_POV)) {
				printf("rdp%d: rsr %#x, resetting\n",
				       unit, rsr);
				rdp_reset(ifp);
				break;
			}

			if (rsr & RSR_BUFO)
				/*
				 * CRC and FA errors are recorded in
				 * rdp_rint() on a per-packet basis
				 */
				ifp->if_ierrors++;
			if (isr & (ISR_ROK | ISR_RER))
				rdp_rint(sc);
		}

		/*
		 * If it looks like the transmitter can take more data,
		 * attempt to start output on the interface. This is done
		 * after handling the receiver to give the receiver priority.
		 */
		if ((ifp->if_flags & IFF_OACTIVE) == 0)
			rdp_start(ifp);

	}
	/* re-enable interrupts */
	WrNib(sc, CMR2, CMR2_IRQOUT | CMR2_IRQINV);
	sc->irqenbit = Ctrl_IRQEN;
	outb(sc->baseaddr + lpt_control, Ctrl_SelData + sc->irqenbit);
}

/*
 * Ethernet interface receiver interrupt.
 */
static void
rdp_rint(struct rdp_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct rdphdr rh;
	u_short len;
	size_t i;
	u_char *packet_ptr, b, status;
	int excessive_bad_pkts = 0;

	/*
	 * Fetch the packets from the NIC's buffer.
	 */
	for (;;) {
		b = RdNib(sc, CMR1);
		RdEnd(sc, CMR1);

		if (b & CMR1_BUFE)
			/* no more packets */
			break;

		/* first, obtain the buffer header */
		
		outb(sc->baseaddr + lpt_data, MAR + EOC); /* prepare addr */
		outb(sc->baseaddr + lpt_control, Ctrl_LNibRead);
		outb(sc->baseaddr + lpt_data, MAR + RdAddr + HNib);

		packet_ptr = (u_char *)&rh;
		if (sc->slow)
			for (i = 0; i < sizeof rh; i++, packet_ptr++)
				*packet_ptr = RdByteA2(sc);
		else
			for (i = 0; i < sizeof rh; i++, packet_ptr++)
				*packet_ptr = RdByteA1(sc);

		RdEnd(sc, MAR + HNib);
		outb(sc->baseaddr + lpt_control, Ctrl_SelData);

		len = rh.pktlen - ETHER_CRC_LEN;
		status = rh.status;

		if ((status & (RSR_ROK | RSR_CRC | RSR_FA)) != RSR_ROK ||
		    len > (ETHER_MAX_LEN - ETHER_CRC_LEN) ||
		    len < (ETHER_MIN_LEN - ETHER_CRC_LEN) ||
		    len > MCLBYTES) {
#if DEBUG
			printf("rdp%d: bad packet in buffer, "
			       "len %d, status %#x\n",
			       ifp->if_unit, (int)len, (int)status);
#endif
			ifp->if_ierrors++;
			/* rx jump packet */
			WrNib(sc, CMR1, CMR1_RDPAC);
			if (++excessive_bad_pkts > 5) {
				/*
				 * the chip seems to be stuck, we are
				 * probably seeing the same bad packet
				 * over and over again
				 */
#if DEBUG
				printf("rdp%d: resetting due to an "
				       "excessive number of bad packets\n",
				       ifp->if_unit);
#endif
				rdp_reset(ifp);
				return;
			}
			continue;
		}

		/*
		 * Go get packet.
		 */
		excessive_bad_pkts = 0;
		rdp_get_packet(sc, len);
		ifp->if_ipackets++;
	}
}

/*
 * Retreive packet from NIC memory and send to the next level up via
 * ether_input().
 */
static void
rdp_get_packet(struct rdp_softc *sc, unsigned len)
{
	struct ether_header *eh;
	struct mbuf *m;
	u_char *packet_ptr;
	size_t s;

	/* Allocate a header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;
	m->m_pkthdr.rcvif = &sc->arpcom.ac_if;
	m->m_pkthdr.len = m->m_len = len;

	/*
	 * We always put the received packet in a single buffer -
	 * either with just an mbuf header or in a cluster attached
	 * to the header. The +2 is to compensate for the alignment
	 * fixup below.
	 */
	if ((len + 2) > MHLEN) {
		/* Attach an mbuf cluster */
		MCLGET(m, M_DONTWAIT);

		/* Insist on getting a cluster */
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return;
		}
	}

	/*
	 * The +2 is to longword align the start of the real packet.
	 * This is important for NFS.
	 */
	m->m_data += 2;
	eh = mtod(m, struct ether_header *);

	/*
	 * Get packet, including link layer address, from interface.
	 */
	outb(sc->baseaddr + lpt_control, Ctrl_LNibRead);
	outb(sc->baseaddr + lpt_data, RdAddr + MAR);

	packet_ptr = (u_char *)eh;
	if (sc->slow)
		for (s = 0; s < len; s++, packet_ptr++)
			*packet_ptr = RdByteA2(sc);
	else
		for (s = 0; s < len; s++, packet_ptr++)
			*packet_ptr = RdByteA1(sc);

	RdEnd(sc, MAR + HNib);
	outb(sc->baseaddr + lpt_control, Ctrl_SelData);
	WrNib(sc, CMR1, CMR1_RDPAC);

	/*
	 * Remove link layer address.
	 */
	m->m_pkthdr.len = m->m_len = len - sizeof(struct ether_header);
	m->m_data += sizeof(struct ether_header);

	ether_input(&sc->arpcom.ac_if, eh, m);
}

/*
 * Write an mbuf chain to the NIC's tx buffer.
 */
static u_short
rdp_write_mbufs(struct rdp_softc *sc, struct mbuf *m)
{
	u_short total_len;
	struct mbuf *mp;
	u_char *dp, b;
	int i;

	/* First, count up the total number of bytes to copy */
	for (total_len = 0, mp = m; mp; mp = mp->m_next)
		total_len += mp->m_len;

	if (total_len == 0)
		return 0;

	outb(sc->baseaddr + lpt_data, MAR | EOC);

	/*
	 * Transfer the mbuf chain to the NIC memory.
	 */
	if (sc->slow) {
		/* writing the first byte is complicated */
		outb(sc->baseaddr + lpt_control,
		     Ctrl_LNibRead | sc->irqenbit);
		outb(sc->baseaddr + lpt_data, MAR | WrAddr);
		b = *(u_char *)m->m_data;
		outb(sc->baseaddr + lpt_data, (b & 0x0f) | 0x40);
		outb(sc->baseaddr + lpt_data, b & 0x0f);
		outb(sc->baseaddr + lpt_data, b >> 4);
		outb(sc->baseaddr + lpt_control,
		     Ctrl_HNibRead | sc->irqenbit);
		/* advance the mbuf pointer */
		mp = m;
		m->m_len--;
		m->m_data++;
		/* write the remaining bytes */
		while (m) {
			for (i = 0, dp = (u_char *)m->m_data;
			     i < m->m_len;
			     i++, dp++)
				WrByteALToDRAMA(sc, *dp);
			m = m->m_next;
		}
		/*
		 * restore old mbuf in case we have to hand it off to
		 * BPF again
		 */
		m = mp;
		m->m_len++;
		m->m_data--;

		/* the RTL 8002 requires an even byte-count remote DMA */
		if (total_len & 1)
			WrByteALToDRAMA(sc, 0);
	} else {
		outb(sc->baseaddr + lpt_data, MAR | WrAddr);
		while (m) {
			for (i = 0, dp = (u_char *)m->m_data;
			     i < m->m_len;
			     i++, dp++)
				WrByteALToDRAM(sc, *dp);
			m = m->m_next;
		}

		/* the RTL 8002 requires an even byte-count remote DMA */
		if (total_len & 1)
			WrByteALToDRAM(sc, 0);
	}

	outb(sc->baseaddr + lpt_data, 0xff);
	outb(sc->baseaddr + lpt_control,
	     Ctrl_HNibRead | Ctrl_SelData | sc->irqenbit);

	return total_len;
}

/*
 * Read the designated ethernet hardware address out of a 93C46
 * (serial) EEPROM.
 * Note that the 93C46 uses 16-bit words in big-endian notation.
 */
static int
rdp_gethwaddr_93c46(struct rdp_softc *sc, u_char *etheraddr)
{
	int i, magic;
	size_t j = 0;
	u_short w;

	WrNib(sc, CMR2, CMR2_PAGE | CMR2_IRQINV); /* select page 1 */

	/*
	 * The original RealTek packet driver had the ethernet address
	 * starting at EEPROM address 0.  Other vendors seem to have
	 * gone `creative' here -- while they didn't do anything else
	 * than changing a few strings in the entire driver, compared
	 * to the RealTek version, they also moved out the ethernet
	 * address to a different location in the EEPROM, so the
	 * original RealTek driver won't work correctly with them, and
	 * vice versa.  Sounds pretty cool, eh?  $@%&!
	 *
	 * Anyway, we walk through the EEPROM, until we find some
	 * allowable value based upon our table of IEEE OUI assignments.
	 */
	for (i = magic = 0; magic < 3 && i < 32; i++) {
		/* read cmd (+ 6 bit address) */
		rdp_93c46_cmd(sc, 0x180 + i, 10);
		w = rdp_93c46_read(sc);
		switch (magic) {
		case 0:
			for (j = 0;
			     j < sizeof allowed_ouis / sizeof(u_short);
			     j++)
				if (w == allowed_ouis[j]) {
					etheraddr[0] = (w >> 8) & 0xff;
					etheraddr[1] = w & 0xff;
					magic++;
					break;
				}
			break;

		case 1:
			/*
			 * If the first two bytes have been 00:00, we
			 * discard the match iff the next two bytes
			 * are also 00:00, so we won't get fooled by
			 * an EEPROM that has been filled with zeros.
			 * This in theory would disallow 64 K of legal
			 * addresses assigned to Xerox, but it's
			 * almost certain that those addresses haven't
			 * been used for RTL80[01]2 chips anyway.
			 */
			if ((etheraddr[0] | etheraddr[1]) == 0 && w == 0) {
				magic--;
				break;
			}

			etheraddr[2] = (w >> 8) & 0xff;
			etheraddr[3] = w & 0xff;
			magic++;
			break;

		case 2:
			etheraddr[4] = (w >> 8) & 0xff;
			etheraddr[5] = w & 0xff;
			magic++;
			break;
		}
	}

	WrNib(sc, CMR2, CMR2_IRQINV);	/* back to page 0 */

	return magic == 3;
}

/*
 * Read the designated ethernet hardware address out of a 74S288
 * EEPROM.
 *
 * This is untested, since i haven't seen any adapter actually using
 * a 74S288.  In the RTL 8012, only the serial EEPROM (94C46) is
 * supported anymore.
 */
static void
rdp_gethwaddr_74s288(struct rdp_softc *sc, u_char *etheraddr)
{
	int i;
	u_char b;

	WrNib(sc, CMR2, CMR2_PAGE | CMR2_IRQINV); /* select page 1 */

	for (i = 0; i < 6; i++) {
		WrNib(sc, PCMR, i & 0x0f); /* lower 4 bit of addr */
		WrNib(sc, PCMR + HNib, HNib + 4); /* upper 2 bit addr + /CS */
		WrNib(sc, PCMR + HNib, HNib); /* latch data now */
		b = RdNib(sc, PDR) & 0x0f;
		b |= (RdNib(sc, PDR + HNib) & 0x0f) << 4;
		etheraddr[i] = b;
	}

	RdEnd(sc, PDR + HNib);
	WrNib(sc, CMR2, CMR2_IRQINV);	/* reselect page 0 */
}

/*
 * Send nbits of data (starting with MSB) out to the 93c46 as a
 * command.  Assumes register page 1 has already been selected.
 */
static void
rdp_93c46_cmd(struct rdp_softc *sc, u_short data, unsigned nbits)
{
	u_short mask = 1 << (nbits - 1);
	unsigned i;
	u_char b;

#if DEBUG & 2
	printf("rdp_93c46_cmd(): ");
#endif
	for (i = 0; i < nbits; i++, mask >>= 1) {
		b = HNib + PCMR_SK + PCMR_CS;
		if (data & mask)
			b += PCMR_DO;
#if DEBUG & 2
		printf("%d", b & 1);
#endif
		WrNib(sc, PCMR + HNib, b);
		DELAY(1);
		WrNib(sc, PCMR + HNib, b & ~PCMR_SK);
		DELAY(1);
	}
#if DEBUG & 2
	printf("\n");
#endif
}

/*
 * Read one word of data from the 93c46.  Actually, we have to read
 * 17 bits, and discard the very first bit.  Assumes register page 1
 * to be selected as well.
 */
static u_short
rdp_93c46_read(struct rdp_softc *sc)
{
	u_short data = 0;
	u_char b;
	int i;

#if DEBUG & 2
	printf("rdp_93c46_read(): ");
#endif
	for (i = 0; i < 17; i++) {
		WrNib(sc, PCMR + HNib, PCMR_SK + PCMR_CS + HNib);
		DELAY(1);
		WrNib(sc, PCMR + HNib, PCMR_CS + HNib);
		DELAY(1);
		b = RdNib(sc, PDR);
		data <<= 1;
		if (b & 1)
			data |= 1;
#if DEBUG & 2
		printf("%d", b & 1);
#endif
		RdEnd(sc, PDR);
		DELAY(1);
	}

#if DEBUG & 2
	printf("\n");
#endif
	/* end of cycle */
	WrNib(sc, PCMR + HNib, PCMR_SK + HNib);
	DELAY(1);

	return data;
}
