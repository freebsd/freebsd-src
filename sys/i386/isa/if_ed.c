/*
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 *   adapters. By David Greenman, 29-April-1993
 *
 * Copyright (C) 1993, David Greenman. This software may be used, modified,
 *   copied, distributed, and sold, in both source and binary form provided
 *   that the above copyright and these terms are retained. Under no
 *   circumstances is the author responsible for the proper functioning
 *   of this software, nor does the author assume any responsibility
 *   for damages incurred with its use.
 *
 * Currently supports the Western Digital/SMC 8003 and 8013 series,
 *   the 3Com 3c503, the NE1000 and NE2000, and a variety of similar
 *   clones.
 *
 * Thanks to Charles Hannum for proving to me with example code that the
 *	NE1000/2000 support could be added with minimal impact. Without
 *	this, I wouldn't have proceeded in this direction.
 *	
 */

/*
 * $Id: if_ed.c,v 2.16 1993/11/29 16:55:56 davidg Exp davidg $
 */

/*
 * Modification history
 *
 * Revision 2.16  1993/11/29  16:55:56  davidg
 * merged in Garrett Wollman's strict prototype changes
 *
 * Revision 2.15  1993/11/29  16:32:58  davidg
 * From Thomas Sandford <t.d.g.sandford@comp.brad.ac.uk>
 * Add support for the 8013W board type
 *
 * Revision 2.14  1993/11/22  10:55:30  davidg
 * change all splnet's to splimp's
 *
 * Revision 2.13  1993/11/22  10:53:52  davidg
 * patch to add support for SMC8216 (Elite-Ultra) boards
 * from Glen H. Lowe
 *
 * Revision 2.12  1993/11/07  18:04:13  davidg
 * fix from Garrett Wollman:
 * add a return(0) at the end of ed_probe so that if the various device
 * specific probes fail that we just don't fall of the end of the function.
 *
 * Revision 2.11  1993/10/23  04:21:03  davidg
 * Novell probe changed to be invasive because of too many complaints
 * about some clone boards not being reset properly and thus not
 * found on a warmboot. Yuck.
 *
 * Revision 2.10  1993/10/23  04:07:12  davidg
 * increment output errors if the device times out (done via watchdog)
 *
 * Revision 2.9  1993/10/23  04:01:45  davidg
 * increment input error counter if a packet with a bad length is
 * detected.
 *
 * Revision 2.8  1993/10/15  10:59:56  davidg
 * increase maximum time to wait for transmit DMA to complete to 120us.
 * call ed_reset() if the time limit is reached instead of trying
 * to abort the remote DMA.
 *
 * Revision 2.7  1993/10/15  10:49:10  davidg
 * minor change to way the mbuf pointer temp variable is assigned in
 * ed_start (slightly improves code readability)
 *
 * Revision 2.6  93/10/02  01:12:20  davidg
 * use ETHER_ADDR_LEN in NE probe rather than '6'.
 * 
 * Revision 2.5  93/09/30  17:44:14  davidg
 * patch from vak@zebub.msk.su (Serge V.Vakulenko) to work around
 * a hardware bug in cheap WD clone boards where the PROM checksum
 * byte is always zero
 * 
 * Revision 2.4  93/09/29  21:24:30  davidg
 * Added software NIC reset in NE probe to work around a problem
 * with some NE boards where the 8390 doesn't reset properly on
 * power-up. Remove initialization of IMR/ISR in the NE probe
 * because this is inherent in the reset.
 * 
 * Revision 2.3  93/09/29  15:10:16  davidg
 * credit Charles Hannum
 * 
 * Revision 2.2  93/09/29  13:23:25  davidg
 * added no multi-buffer override for 3c503
 * 
 * Revision 2.1  93/09/29  12:32:12  davidg
 * changed multi-buffer count for 16bit 3c503's from 5 to 2 after
 * noticing that the transmitter becomes idle because of so many
 * packets to load.
 * 
 * Revision 2.0  93/09/29  00:00:19  davidg
 * many changes, rewrites, additions, etc. Now supports the
 * NE1000, NE2000, WD8003, WD8013, 3C503, 16bit 3C503, and
 * a variety of similar clones. 16bit 3c503 now does multi
 * transmit buffers. Nearly every part of the driver has
 * changed in some way since rev 1.30.
 * 
 * Revision 1.1  93/06/14  22:21:24  davidg
 * Beta release of device driver for SMC/WD80x3 and 3C503 ethernet boards.
 * 
 */
 
#include "ed.h"
#if	NED > 0
/* bpfilter included here in case it is needed in future net includes */
#include "bpfilter.h"

#include "param.h"
#include "systm.h"
#include "errno.h"
#include "ioctl.h"
#include "mbuf.h"
#include "socket.h"
#include "syslog.h"

#include "net/if.h"
#include "net/if_dl.h"
#include "net/if_types.h"
#include "net/netisr.h"

#ifdef INET
#include "netinet/in.h"
#include "netinet/in_systm.h"
#include "netinet/in_var.h"
#include "netinet/ip.h"
#include "netinet/if_ether.h"
#endif

#ifdef NS
#include "netns/ns.h"
#include "netns/ns_if.h"
#endif

#if NBPFILTER > 0
#include "net/bpf.h"
#include "net/bpfdesc.h"
#endif

#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/icu.h"
#include "i386/isa/if_edreg.h"

#include "i386/include/pio.h"

/* For backwards compatibility */
#ifndef IFF_ALTPHYS
#define IFF_ALTPHYS IFF_LLC0
#endif
 
/*
 * ed_softc: per line info and status
 */
struct	ed_softc {
	struct	arpcom arpcom;	/* ethernet common */

	char	*type_str;	/* pointer to type string */
	u_char	vendor;		/* interface vendor */
	u_char	type;		/* interface type code */

	u_short	asic_addr;	/* ASIC I/O bus address */
	u_short	nic_addr;	/* NIC (DS8390) I/O bus address */

/*
 * The following 'proto' variable is part of a work-around for 8013EBT asics
 *	being write-only. It's sort of a prototype/shadow of the real thing.
 */
	u_char	wd_laar_proto;
	u_char	isa16bit;	/* width of access to card 0=8 or 1=16 */
	int	is790;		/* set by the probe code if the card is 790 based */

	caddr_t	bpf;		/* BPF "magic cookie" */
	caddr_t	mem_start;	/* NIC memory start address */
	caddr_t	mem_end;	/* NIC memory end address */
	u_long	mem_size;	/* total NIC memory size */
	caddr_t	mem_ring;	/* start of RX ring-buffer (in NIC mem) */

	u_char	mem_shared;	/* NIC memory is shared with host */
	u_char	xmit_busy;	/* transmitter is busy */
	u_char	txb_cnt;	/* number of transmit buffers */
	u_char	txb_inuse;	/* number of TX buffers currently in-use*/

	u_char 	txb_new;	/* pointer to where new buffer will be added */
	u_char	txb_next_tx;	/* pointer to next buffer ready to xmit */
	u_short	txb_len[8];	/* buffered xmit buffer lengths */
	u_char	tx_page_start;	/* first page of TX buffer area */
	u_char	rec_page_start;	/* first page of RX ring-buffer */
	u_char	rec_page_stop;	/* last page of RX ring-buffer */
	u_char	next_packet;	/* pointer to next unread RX packet */
} ed_softc[NED];

int	ed_attach(struct isa_device *);
void	ed_init(int);
void	edintr(int);
int	ed_ioctl(struct ifnet *, int, caddr_t);
int	ed_probe(struct isa_device *);
void	ed_start(struct ifnet *);
void	ed_reset(int, int);
void	ed_watchdog(int);

static void ed_get_packet(struct ed_softc *, char *, int /*u_short*/);
static void ed_stop(int);

static inline void ed_rint();
static inline void ed_xmit();
static inline char *ed_ring_copy();

void ed_pio_readmem(), ed_pio_writemem();
u_short ed_pio_write_mbufs();

extern int ether_output();

struct trailer_header {
	u_short ether_type;
	u_short ether_residual;
};

struct isa_driver eddriver = {
	ed_probe,
	ed_attach,
	"ed"
};
/* 
 * Interrupt conversion table for WD/SMC ASIC
 * (IRQ* are defined in icu.h)
 */
static unsigned short ed_intr_mask[] = {
	IRQ9,
	IRQ3,
	IRQ5,
	IRQ7,
	IRQ10,
	IRQ11,
	IRQ15,
	IRQ4
};
	
/*
 * Interrupt conversion table for 585/790 Combo
 */
static unsigned short ed_790_intr_mask[] = {
	0,
	IRQ9,
	IRQ3,
	IRQ4,
	IRQ5,
	IRQ10,
	IRQ11,
	IRQ15
};
#define	ETHER_MIN_LEN	64
#define ETHER_MAX_LEN	1518
#define	ETHER_ADDR_LEN	6
#define	ETHER_HDR_SIZE	14

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
ed_probe(isa_dev)
	struct isa_device *isa_dev;
{
	struct ed_softc *sc = &ed_softc[isa_dev->id_unit];
	int nports;

	if (nports = ed_probe_WD80x3(isa_dev))
		return (nports);

	if (nports = ed_probe_3Com(isa_dev))
		return (nports);

	if (nports = ed_probe_Novell(isa_dev))
		return (nports);

	return(0);
}

/*
 * Generic probe routine for testing for the existance of a DS8390.
 *	Must be called after the NIC has just been reset. This routine
 *	works by looking at certain register values that are gauranteed
 *	to be initialized a certain way after power-up or reset. Seems
 *	not to currently work on the 83C690.
 *
 * Specifically:
 *
 *	Register			reset bits	set bits
 *	Command Register (CR)		TXP, STA	RD2, STP
 *	Interrupt Status (ISR)				RST
 *	Interrupt Mask (IMR)		All bits
 *	Data Control (DCR)				LAS
 *	Transmit Config. (TCR)		LB1, LB0
 *
 * We only look at the CR and ISR registers, however, because looking at
 *	the others would require changing register pages (which would be
 *	intrusive if this isn't an 8390).
 *
 * Return 1 if 8390 was found, 0 if not. 
 */

int
ed_probe_generic8390(sc)
	struct ed_softc *sc;
{
	if ((inb(sc->nic_addr + ED_P0_CR) &
		(ED_CR_RD2|ED_CR_TXP|ED_CR_STA|ED_CR_STP)) !=
		(ED_CR_RD2|ED_CR_STP))
			return (0);
	if ((inb(sc->nic_addr + ED_P0_ISR) & ED_ISR_RST) != ED_ISR_RST)
		return (0);

	return(1);
}
	
/*
 * Probe and vendor-specific initialization routine for SMC/WD80x3 boards
 */
int
ed_probe_WD80x3(isa_dev)
	struct isa_device *isa_dev;
{
	struct ed_softc *sc = &ed_softc[isa_dev->id_unit];
	int i;
	u_int memsize;
	u_char iptr, isa16bit, sum;

	sc->asic_addr = isa_dev->id_iobase;
	sc->nic_addr = sc->asic_addr + ED_WD_NIC_OFFSET;
	sc->is790 = 0;

	/*
	 * Attempt to do a checksum over the station address PROM.
	 *	If it fails, it's probably not a SMC/WD board. There
	 *	is a problem with this, though: some clone WD boards
	 *	don't pass the checksum test. Danpex boards for one.
	 */
	for (sum = 0, i = 0; i < 8; ++i)
		sum += inb(sc->asic_addr + ED_WD_PROM + i);

	if (sum != ED_WD_ROM_CHECKSUM_TOTAL) {
		/*
		 * Checksum is invalid. This often happens with cheap
		 *	WD8003E clones.  In this case, the checksum byte
		 *	(the eighth byte) seems to always be zero.
		 */
		if (inb(sc->asic_addr + ED_WD_CARD_ID) != ED_TYPE_WD8003E ||
			inb(sc->asic_addr + ED_WD_PROM + 7) != 0)
				return(0);
	}

	/* reset card to force it into a known state. */
	outb(sc->asic_addr + ED_WD_MSR, ED_WD_MSR_RST);
	DELAY(100);
	outb(sc->asic_addr + ED_WD_MSR, inb(sc->asic_addr + ED_WD_MSR) & ~ED_WD_MSR_RST);
	/* wait in the case this card is reading it's EEROM */
	DELAY(5000);

	sc->vendor = ED_VENDOR_WD_SMC;
	sc->type = inb(sc->asic_addr + ED_WD_CARD_ID);

	/*
	 * Set initial values for width/size.
	 */
	switch (sc->type) {
	case ED_TYPE_WD8003S:
		sc->type_str = "WD8003S";
		memsize = 8192;
		isa16bit = 0;
		break;
	case ED_TYPE_WD8003E:
		sc->type_str = "WD8003E";
		memsize = 8192;
		isa16bit = 0;
		break;
	case ED_TYPE_WD8013EBT:
		sc->type_str = "WD8013EBT";
		memsize = 16384;
		isa16bit = 1;
		break;
	case ED_TYPE_WD8013W:
		sc->type_str = "WD8013W";
		memsize = 16384;
		isa16bit = 1;
		break;
	case ED_TYPE_WD8013EP:		/* also WD8003EP */
		if (inb(sc->asic_addr + ED_WD_ICR)
			& ED_WD_ICR_16BIT) {
			isa16bit = 1;
			memsize = 16384;
			sc->type_str = "WD8013EP";
		} else {
			isa16bit = 0;
			memsize = 8192;
			sc->type_str = "WD8003EP";
		}
		break;
	case ED_TYPE_WD8013WC:
		sc->type_str = "WD8013WC";
		memsize = 16384;
		isa16bit = 1;
		break;
	case ED_TYPE_WD8013EBP:
		sc->type_str = "WD8013EBP";
		memsize = 16384;
		isa16bit = 1;
		break;
	case ED_TYPE_WD8013EPC:
		sc->type_str = "WD8013EPC";
		memsize = 16384;
		isa16bit = 1;
		break;
	case ED_TYPE_SMC8216C:
		sc->type_str = "SMC8216/SMC8216C";
		memsize = 16384;
		isa16bit = 1;
		sc->is790 = 1;
		break;
	case ED_TYPE_SMC8216T:
		sc->type_str = "SMC8216T";
		memsize = 16384;
		isa16bit = 1;
		sc->is790 = 1;
		break;
	default:
		sc->type_str = "";
		memsize = 8192;
		isa16bit = 0;
		break;
	}
	/*
	 * Make some adjustments to initial values depending on what is
	 *	found in the ICR.
	 */
	if (isa16bit && (sc->type != ED_TYPE_WD8013EBT)
		&& ((inb(sc->asic_addr + ED_WD_ICR) & ED_WD_ICR_16BIT) == 0)) {
		isa16bit = 0;
		memsize = 8192;
	}

#if ED_DEBUG
	printf("type=%s isa16bit=%d memsize=%d id_msize=%d\n",
		sc->type_str,isa16bit,memsize,isa_dev->id_msize);
	for (i=0; i<8; i++)
		printf("%x -> %x\n", i, inb(sc->asic_addr + i));
#endif
	/*
	 * Allow the user to override the autoconfiguration
	 */
	if (isa_dev->id_msize)
		memsize = isa_dev->id_msize;
	/*
	 * (note that if the user specifies both of the following flags
	 *	that '8bit' mode intentionally has precedence)
	 */
	if (isa_dev->id_flags & ED_FLAGS_FORCE_16BIT_MODE)
		isa16bit = 1;
	if (isa_dev->id_flags & ED_FLAGS_FORCE_8BIT_MODE)
		isa16bit = 0;

	/*
	 * Check 83C584 interrupt configuration register if this board has one
	 *	XXX - we could also check the IO address register. But why
	 *		bother...if we get past this, it *has* to be correct.
	 */
	if ((sc->type & ED_WD_SOFTCONFIG) && (!sc->is790)) {
		/*
		 * Assemble together the encoded interrupt number.
		 */
		iptr = (inb(isa_dev->id_iobase + ED_WD_ICR) & ED_WD_ICR_IR2) |
			((inb(isa_dev->id_iobase + ED_WD_IRR) &
				(ED_WD_IRR_IR0 | ED_WD_IRR_IR1)) >> 5);
		/*
		 * Translate it using translation table, and check for correctness.
		 */
		if (ed_intr_mask[iptr] != isa_dev->id_irq) {
			printf("ed%d: kernel configured irq %d doesn't match board configured irq %d\n",
				isa_dev->id_unit, ffs(isa_dev->id_irq) - 1, ffs(ed_intr_mask[iptr]) - 1);
			return(0);
		}
		/*
		 * Enable the interrupt.
		 */
		outb(isa_dev->id_iobase + ED_WD_IRR,
			inb(isa_dev->id_iobase + ED_WD_IRR) | ED_WD_IRR_IEN);
	}
	if (sc->is790) {
		outb(isa_dev->id_iobase + 0x04, inb(isa_dev->id_iobase + 0x04) | 0x80);
		iptr = ((inb(isa_dev->id_iobase + 0x0d) & 0x0c ) >> 2) | 
			((inb(isa_dev->id_iobase + 0x0d) & 0x40) >> 4);
		outb(isa_dev->id_iobase + 0x04, inb(isa_dev->id_iobase + 0x04) & ~0x80);

		if (ed_790_intr_mask[iptr] != isa_dev->id_irq) {
			printf("ed%d: kernel configured irq %d doesn't match board configured irq %d %d\n",
				isa_dev->id_unit, ffs(isa_dev->id_irq) - 1, ffs(ed_790_intr_mask[iptr]) -1, iptr);
			return 0;
		}
		outb(isa_dev->id_iobase + 0x06, inb(isa_dev->id_iobase + 0x06) | 0x01);
	}

	sc->isa16bit = isa16bit;

#ifdef notyet /* XXX - I'm not sure if PIO mode is even possible on WD/SMC boards */
	/*
	 * The following allows the WD/SMC boards to be used in Programmed I/O
	 *	mode - without mapping the NIC memory shared. ...Not the prefered
	 *	way, but it might be the only way.
	 */
	if (isa_dev->id_flags & ED_FLAGS_FORCE_PIO) {
		sc->mem_shared = 0;
		isa_dev->id_maddr = 0;
	} else {
		sc->mem_shared = 1;
	}
#else
	sc->mem_shared = 1;
#endif
	isa_dev->id_msize = memsize;

	sc->mem_start = (caddr_t)isa_dev->id_maddr;

	/*
	 * allocate one xmit buffer if < 16k, two buffers otherwise
	 */
	if ((memsize < 16384) || (isa_dev->id_flags & ED_FLAGS_NO_MULTI_BUFFERING)) {
		sc->mem_ring = sc->mem_start + (ED_PAGE_SIZE * ED_TXBUF_SIZE);
		sc->txb_cnt = 1;
		sc->rec_page_start = ED_TXBUF_SIZE;
	} else {
		sc->mem_ring = sc->mem_start + (ED_PAGE_SIZE * ED_TXBUF_SIZE * 2);
		sc->txb_cnt = 2;
		sc->rec_page_start = ED_TXBUF_SIZE * 2;
	}
	sc->mem_size = memsize;
	sc->mem_end = sc->mem_start + memsize;
	sc->rec_page_stop = memsize / ED_PAGE_SIZE;
	sc->tx_page_start = ED_WD_PAGE_OFFSET;

	/*
	 * Get station address from on-board ROM
	 */
	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		sc->arpcom.ac_enaddr[i] = inb(sc->asic_addr + ED_WD_PROM + i);

	if (sc->mem_shared) {
		/*
		 * Set address and enable interface shared memory.
		 */
		if(!sc->is790) {
			outb(sc->asic_addr + ED_WD_MSR, ((kvtop(sc->mem_start) >> 13) &
				ED_WD_MSR_ADDR) | ED_WD_MSR_MENB);
		} else {
			outb(sc->asic_addr + ED_WD_MSR, ED_WD_MSR_MENB);
			outb(sc->asic_addr + 0x04, (inb(sc->asic_addr + 0x04) | 0x80));
			outb(sc->asic_addr + 0x0b, ((kvtop(sc->mem_start) >> 13) & 0x0f) |
				((kvtop(sc->mem_start) >> 11) & 0x40) | 
				(inb(sc->asic_addr + 0x0b) & 0xb0));
			outb(sc->asic_addr + 0x04, (inb(sc->asic_addr + 0x04) & ~0x80));
		}

		/*
		 * Set upper address bits and 8/16 bit access to shared memory
		 */
		if (isa16bit) {
			if (sc->is790) {
				sc->wd_laar_proto = inb(sc->asic_addr + ED_WD_LAAR);
				outb(sc->asic_addr + ED_WD_LAAR, ED_WD_LAAR_M16EN);
			} else {
				outb(sc->asic_addr + ED_WD_LAAR, (sc->wd_laar_proto =
					ED_WD_LAAR_L16EN | ED_WD_LAAR_M16EN |
					((kvtop(sc->mem_start) >> 19) & ED_WD_LAAR_ADDRHI)));
			}
		} else  {
			if ((sc->type & ED_WD_SOFTCONFIG) || (sc->type == ED_TYPE_WD8013EBT) && (!sc->is790)) {
				outb(sc->asic_addr + ED_WD_LAAR, (sc->wd_laar_proto =
					((kvtop(sc->mem_start) >> 19) & ED_WD_LAAR_ADDRHI)));
			}
		}

		/*
		 * Now zero memory and verify that it is clear
		 */
		bzero(sc->mem_start, memsize);

		for (i = 0; i < memsize; ++i)
			if (sc->mem_start[i]) {
		        	printf("ed%d: failed to clear shared memory at %x - check configuration\n",
					isa_dev->id_unit, kvtop(sc->mem_start + i));

				/*
				 * Disable 16 bit access to shared memory
				 */
				if (isa16bit)
					outb(sc->asic_addr + ED_WD_LAAR, (sc->wd_laar_proto &=
						~ED_WD_LAAR_M16EN));

				return(0);
			}
	
		/*
		 * Disable 16bit access to shared memory - we leave it disabled so
		 *	that 1) machines reboot properly when the board is set
		 *	16 bit mode and there are conflicting 8bit devices/ROMS
		 *	in the same 128k address space as this boards shared
		 *	memory. and 2) so that other 8 bit devices with shared
		 *	memory can be used in this 128k region, too.
		 */
		if (isa16bit)
			outb(sc->asic_addr + ED_WD_LAAR, (sc->wd_laar_proto &=
				~ED_WD_LAAR_M16EN));

	}

	return (ED_WD_IO_PORTS);
}

/*
 * Probe and vendor-specific initialization routine for 3Com 3c503 boards
 */
int
ed_probe_3Com(isa_dev)
	struct isa_device *isa_dev;
{
	struct ed_softc *sc = &ed_softc[isa_dev->id_unit];
	int i;
	u_int memsize;
	u_char isa16bit, sum;

	sc->asic_addr = isa_dev->id_iobase + ED_3COM_ASIC_OFFSET;
	sc->nic_addr = isa_dev->id_iobase + ED_3COM_NIC_OFFSET;

	/*
	 * Verify that the kernel configured I/O address matches the board
	 *	configured address
	 */
	switch (inb(sc->asic_addr + ED_3COM_BCFR)) {
	case ED_3COM_BCFR_300:
		if (isa_dev->id_iobase != 0x300)
			return(0);
		break;
	case ED_3COM_BCFR_310:
		if (isa_dev->id_iobase != 0x310)
			return(0);
		break;
	case ED_3COM_BCFR_330:
		if (isa_dev->id_iobase != 0x330)
			return(0);
		break;
	case ED_3COM_BCFR_350:
		if (isa_dev->id_iobase != 0x350)
			return(0);
		break;
	case ED_3COM_BCFR_250:
		if (isa_dev->id_iobase != 0x250)
			return(0);
		break;
	case ED_3COM_BCFR_280:
		if (isa_dev->id_iobase != 0x280)
			return(0);
		break;
	case ED_3COM_BCFR_2A0:
		if (isa_dev->id_iobase != 0x2a0)
			return(0);
		break;
	case ED_3COM_BCFR_2E0:
		if (isa_dev->id_iobase != 0x2e0)
			return(0);
		break;
	default:
		return(0);
	}

	/*
	 * Verify that the kernel shared memory address matches the
	 *	board configured address.
	 */
	switch (inb(sc->asic_addr + ED_3COM_PCFR)) {
	case ED_3COM_PCFR_DC000:
		if (kvtop(isa_dev->id_maddr) != 0xdc000)
			return(0);
		break;
	case ED_3COM_PCFR_D8000:
		if (kvtop(isa_dev->id_maddr) != 0xd8000)
			return(0);
		break;
	case ED_3COM_PCFR_CC000:
		if (kvtop(isa_dev->id_maddr) != 0xcc000)
			return(0);
		break;
	case ED_3COM_PCFR_C8000:
		if (kvtop(isa_dev->id_maddr) != 0xc8000)
			return(0);
		break;
	default:
		return(0);
	}


	/*
	 * Reset NIC and ASIC. Enable on-board transceiver throughout reset
	 *	sequence because it'll lock up if the cable isn't connected
	 *	if we don't.
	 */
	outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_RST | ED_3COM_CR_XSEL);

	/*
	 * Wait for a while, then un-reset it
	 */
	DELAY(50);
	/*
	 * The 3Com ASIC defaults to rather strange settings for the CR after
	 *	a reset - it's important to set it again after the following
	 *	outb (this is done when we map the PROM below).
	 */
	outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_XSEL);

	/*
	 * Wait a bit for the NIC to recover from the reset
	 */
	DELAY(5000);

	sc->vendor = ED_VENDOR_3COM;
	sc->type_str = "3c503";

	sc->mem_shared = 1;

	/*
	 * Hmmm...a 16bit 3Com board has 16k of memory, but only an 8k
	 *	window to it.
	 */
	memsize = 8192;

	/*
	 * Get station address from on-board ROM
	 */
	/*
	 * First, map ethernet address PROM over the top of where the NIC
	 *	registers normally appear.
	 */
	outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_EALO | ED_3COM_CR_XSEL);

	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		sc->arpcom.ac_enaddr[i] = inb(sc->nic_addr + i);

	/*
	 * Unmap PROM - select NIC registers. The proper setting of the
	 *	tranceiver is set in ed_init so that the attach code
	 *	is given a chance to set the default based on a compile-time
	 *	config option
	 */
	outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_XSEL);

	/*
	 * Determine if this is an 8bit or 16bit board
	 */

	/*
	 * select page 0 registers
	 */
        outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STP);

	/*
	 * Attempt to clear WTS bit. If it doesn't clear, then this is a
	 *	16bit board.
	 */
	outb(sc->nic_addr + ED_P0_DCR, 0);

	/*
	 * select page 2 registers
	 */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_PAGE_2|ED_CR_RD2|ED_CR_STP);

	/*
	 * The 3c503 forces the WTS bit to a one if this is a 16bit board
	 */
	if (inb(sc->nic_addr + ED_P2_DCR) & ED_DCR_WTS)
		isa16bit = 1;
	else
		isa16bit = 0;

	/*
	 * select page 0 registers
	 */
        outb(sc->nic_addr + ED_P2_CR, ED_CR_RD2|ED_CR_STP);

	sc->mem_start = (caddr_t)isa_dev->id_maddr;
	sc->mem_size = memsize;
	sc->mem_end = sc->mem_start + memsize;

	/*
	 * We have an entire 8k window to put the transmit buffers on the
	 *	16bit boards. But since the 16bit 3c503's shared memory
	 *	is only fast enough to overlap the loading of one full-size
	 *	packet, trying to load more than 2 buffers can actually
	 *	leave the transmitter idle during the load. So 2 seems
	 *	the best value. (Although a mix of variable-sized packets
	 *	might change this assumption. Nonetheless, we optimize for
	 *	linear transfers of same-size packets.)
	 */
	if (isa16bit) {
 		if (isa_dev->id_flags & ED_FLAGS_NO_MULTI_BUFFERING)
			sc->txb_cnt = 1;
		else
			sc->txb_cnt = 2;

		sc->tx_page_start = ED_3COM_TX_PAGE_OFFSET_16BIT;
		sc->rec_page_start = ED_3COM_RX_PAGE_OFFSET_16BIT;
		sc->rec_page_stop = memsize / ED_PAGE_SIZE +
			ED_3COM_RX_PAGE_OFFSET_16BIT;
		sc->mem_ring = sc->mem_start;
	} else {
		sc->txb_cnt = 1;
		sc->tx_page_start = ED_3COM_TX_PAGE_OFFSET_8BIT;
		sc->rec_page_start = ED_TXBUF_SIZE + ED_3COM_TX_PAGE_OFFSET_8BIT;
		sc->rec_page_stop = memsize / ED_PAGE_SIZE +
			ED_3COM_TX_PAGE_OFFSET_8BIT;
		sc->mem_ring = sc->mem_start + (ED_PAGE_SIZE * ED_TXBUF_SIZE);
	}

	sc->isa16bit = isa16bit;

	/*
	 * Initialize GA page start/stop registers. Probably only needed
	 *	if doing DMA, but what the hell.
	 */
	outb(sc->asic_addr + ED_3COM_PSTR, sc->rec_page_start);
	outb(sc->asic_addr + ED_3COM_PSPR, sc->rec_page_stop);

	/*
	 * Set IRQ. 3c503 only allows a choice of irq 2-5.
	 */
	switch (isa_dev->id_irq) {
	case IRQ2:
		outb(sc->asic_addr + ED_3COM_IDCFR, ED_3COM_IDCFR_IRQ2);
		break;
	case IRQ3:
		outb(sc->asic_addr + ED_3COM_IDCFR, ED_3COM_IDCFR_IRQ3);
		break;
	case IRQ4:
		outb(sc->asic_addr + ED_3COM_IDCFR, ED_3COM_IDCFR_IRQ4);
		break;
	case IRQ5:
		outb(sc->asic_addr + ED_3COM_IDCFR, ED_3COM_IDCFR_IRQ5);
		break;
	default:
		printf("ed%d: Invalid irq configuration (%d) must be 2-5 for 3c503\n",
			isa_dev->id_unit, ffs(isa_dev->id_irq) - 1);
		return(0);
	}

	/*
	 * Initialize GA configuration register. Set bank and enable shared mem.
	 */
	outb(sc->asic_addr + ED_3COM_GACFR, ED_3COM_GACFR_RSEL |
		ED_3COM_GACFR_MBS0);

	/*
	 * Initialize "Vector Pointer" registers. These gawd-awful things
	 *	are compared to 20 bits of the address on ISA, and if they
	 *	match, the shared memory is disabled. We set them to
	 *	0xffff0...allegedly the reset vector.
	 */
	outb(sc->asic_addr + ED_3COM_VPTR2, 0xff);
	outb(sc->asic_addr + ED_3COM_VPTR1, 0xff);
	outb(sc->asic_addr + ED_3COM_VPTR0, 0x00);

	/*
	 * Zero memory and verify that it is clear
	 */
	bzero(sc->mem_start, memsize);

	for (i = 0; i < memsize; ++i)
		if (sc->mem_start[i]) {
	        	printf("ed%d: failed to clear shared memory at %x - check configuration\n",
				isa_dev->id_unit, kvtop(sc->mem_start + i));
			return(0);
		}

	isa_dev->id_msize = memsize;
	return(ED_3COM_IO_PORTS);
}

/*
 * Probe and vendor-specific initialization routine for NE1000/2000 boards
 */
int
ed_probe_Novell(isa_dev)
	struct isa_device *isa_dev;
{
	struct ed_softc *sc = &ed_softc[isa_dev->id_unit];
	u_int memsize, n;
	u_char romdata[16], isa16bit = 0, tmp;
	static char test_pattern[32] = "THIS is A memory TEST pattern";
	char test_buffer[32];

	sc->asic_addr = isa_dev->id_iobase + ED_NOVELL_ASIC_OFFSET;
	sc->nic_addr = isa_dev->id_iobase + ED_NOVELL_NIC_OFFSET;

	/* XXX - do Novell-specific probe here */

	/* Reset the board */
	tmp = inb(sc->asic_addr + ED_NOVELL_RESET);

#if 0
	/*
	 * This total and completely screwy thing is to work around braindamage
	 *	in some NE compatible boards. Why it works, I have *no* idea.
	 *	It appears that the boards watch the ISA bus for an outb, and
	 *	will lock up the ISA bus if they see an inb first. Weird.
	 */
	outb(0x84, 0);
#endif

	/*
	 * I don't know if this is necessary; probably cruft leftover from
	 *	Clarkson packet driver code. Doesn't do a thing on the boards
	 *	I've tested. -DG [note that a outb(0x84, 0) seems to work
	 *	here, and is non-invasive...but some boards don't seem to reset
	 *	and I don't have complete documentation on what the 'right'
	 *	thing to do is...so we do the invasive thing for now. Yuck.]
	 */
	outb(sc->asic_addr + ED_NOVELL_RESET, tmp);
	DELAY(5000);

	/*
	 * This is needed because some NE clones apparently don't reset the
	 *	NIC properly (or the NIC chip doesn't reset fully on power-up)
	 * XXX - this makes the probe invasive! ...Done against my better
	 *	judgement. -DLG
	 */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STP);

	DELAY(5000);

	/* Make sure that we really have an 8390 based board */
	if (!ed_probe_generic8390(sc))
		return(0);

	sc->vendor = ED_VENDOR_NOVELL;
	sc->mem_shared = 0;
	isa_dev->id_maddr = 0;

	/*
	 * Test the ability to read and write to the NIC memory. This has
	 * the side affect of determining if this is an NE1000 or an NE2000.
	 */

	/*
	 * This prevents packets from being stored in the NIC memory when
	 *	the readmem routine turns on the start bit in the CR.
	 */
	outb(sc->nic_addr + ED_P0_RCR, ED_RCR_MON);

	/* Temporarily initialize DCR for byte operations */
	outb(sc->nic_addr + ED_P0_DCR, ED_DCR_FT1|ED_DCR_LS);

	outb(sc->nic_addr + ED_P0_PSTART, 8192 / ED_PAGE_SIZE);
	outb(sc->nic_addr + ED_P0_PSTOP, 16384 / ED_PAGE_SIZE);

	sc->isa16bit = 0;

	/*
	 * Write a test pattern in byte mode. If this fails, then there
	 *	probably isn't any memory at 8k - which likely means
	 *	that the board is an NE2000.
	 */
	ed_pio_writemem(sc, test_pattern, 8192, sizeof(test_pattern));
	ed_pio_readmem(sc, 8192, test_buffer, sizeof(test_pattern));

	if (bcmp(test_pattern, test_buffer, sizeof(test_pattern))) {
		/* not an NE1000 - try NE2000 */

		outb(sc->nic_addr + ED_P0_DCR,
			ED_DCR_WTS|ED_DCR_FT1|ED_DCR_LS);
		outb(sc->nic_addr + ED_P0_PSTART, 16384 / ED_PAGE_SIZE);
		outb(sc->nic_addr + ED_P0_PSTOP, 32768 / ED_PAGE_SIZE);

		sc->isa16bit = 1;
		/*
		 * Write a test pattern in word mode. If this also fails, then
		 *	we don't know what this board is.
		 */
		ed_pio_writemem(sc, test_pattern, 16384, sizeof(test_pattern));
		ed_pio_readmem(sc, 16384, test_buffer, sizeof(test_pattern));

		if (bcmp(test_pattern, test_buffer, sizeof(test_pattern)))
			return(0); /* not an NE2000 either */

		sc->type = ED_TYPE_NE2000;
		sc->type_str = "NE2000";
	} else {
		sc->type = ED_TYPE_NE1000;
		sc->type_str = "NE1000";
	}
	
	/* 8k of memory plus an additional 8k if 16bit */
	memsize = 8192 + sc->isa16bit * 8192;

#if 0 /* probably not useful - NE boards only come two ways */
	/* allow kernel config file overrides */
	if (isa_dev->id_msize)
		memsize = isa_dev->id_msize;
#endif

	sc->mem_size = memsize;

	/* NIC memory doesn't start at zero on an NE board */
	/* The start address is tied to the bus width */
	sc->mem_start = (char *) 8192 + sc->isa16bit * 8192;
	sc->mem_end = sc->mem_start + memsize;
	sc->tx_page_start = memsize / ED_PAGE_SIZE;

	/*
	 * Use one xmit buffer if < 16k, two buffers otherwise (if not told
	 *	otherwise).
	 */
	if ((memsize < 16384) || (isa_dev->id_flags & ED_FLAGS_NO_MULTI_BUFFERING))
		sc->txb_cnt = 1;
	else
		sc->txb_cnt = 2;

	sc->rec_page_start = sc->tx_page_start + sc->txb_cnt * ED_TXBUF_SIZE;
	sc->rec_page_stop = sc->tx_page_start + memsize / ED_PAGE_SIZE;

	sc->mem_ring = sc->mem_start + sc->txb_cnt * ED_PAGE_SIZE * ED_TXBUF_SIZE;

	ed_pio_readmem(sc, 0, romdata, 16);
	for (n = 0; n < ETHER_ADDR_LEN; n++)
		sc->arpcom.ac_enaddr[n] = romdata[n*(sc->isa16bit+1)];

	/* clear any pending interrupts that might have occurred above */
	outb(sc->nic_addr + ED_P0_ISR, 0xff);

	return(ED_NOVELL_IO_PORTS);
}
 
/*
 * Install interface into kernel networking data structures
 */
int
ed_attach(isa_dev)
	struct isa_device *isa_dev;
{
	struct ed_softc *sc = &ed_softc[isa_dev->id_unit];
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
 
	/*
	 * Set interface to stopped condition (reset)
	 */
	ed_stop(isa_dev->id_unit);

	/*
	 * Initialize ifnet structure
	 */
	ifp->if_unit = isa_dev->id_unit;
	ifp->if_name = "ed" ;
	ifp->if_mtu = ETHERMTU;
	ifp->if_init = ed_init;
	ifp->if_output = ether_output;
	ifp->if_start = ed_start;
	ifp->if_ioctl = ed_ioctl;
	ifp->if_reset = ed_reset;
	ifp->if_watchdog = ed_watchdog;

	/*
	 * Set default state for ALTPHYS flag (used to disable the tranceiver
	 *	for AUI operation), based on compile-time config option.
	 */
	if (isa_dev->id_flags & ED_FLAGS_DISABLE_TRANCEIVER)
		ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS
			| IFF_ALTPHYS);
	else
		ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS);

	/*
	 * Attach the interface
	 */
	if_attach(ifp);

	/*
	 * Search down the ifa address list looking for the AF_LINK type entry
	 */
 	ifa = ifp->if_addrlist;
	while ((ifa != 0) && (ifa->ifa_addr != 0) &&
	    (ifa->ifa_addr->sa_family != AF_LINK))
		ifa = ifa->ifa_next;
	/*
	 * If we find an AF_LINK type entry we fill in the hardware address.
	 *	This is useful for netstat(1) to keep track of which interface
	 *	is which.
	 */
	if ((ifa != 0) && (ifa->ifa_addr != 0)) {
		/*
		 * Fill in the link-level address for this interface
		 */
		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		sdl->sdl_type = IFT_ETHER;
		sdl->sdl_alen = ETHER_ADDR_LEN;
		sdl->sdl_slen = 0;
		bcopy(sc->arpcom.ac_enaddr, LLADDR(sdl), ETHER_ADDR_LEN);
	}

	/*
	 * Print additional info when attached
	 */
	printf("ed%d: address %s, ", isa_dev->id_unit,
		ether_sprintf(sc->arpcom.ac_enaddr));

	if (sc->type_str && (*sc->type_str != 0))
		printf("type %s ", sc->type_str);
	else
		printf("type unknown (0x%x) ", sc->type);

	printf("%s ",sc->isa16bit ? "(16 bit)" : "(8 bit)");

	printf("%s\n", ((sc->vendor == ED_VENDOR_3COM) &&
		(ifp->if_flags & IFF_ALTPHYS)) ? "tranceiver disabled" : "");

	/*
	 * If BPF is in the kernel, call the attach for it
	 */
#if NBPFILTER > 0
	bpfattach(&sc->bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	return 1;
}
 
/*
 * Reset interface.
 */
void
ed_reset(unit, uban)
	int unit;
	int uban;		/* XXX */
{
	int s;

	s = splimp();

	/*
	 * Stop interface and re-initialize.
	 */
	ed_stop(unit);
	ed_init(unit);

	(void) splx(s);
}
 
/*
 * Take interface offline.
 */
void
ed_stop(unit)
	int unit;
{
	struct ed_softc *sc = &ed_softc[unit];
	int n = 5000;
 
	/*
	 * Stop everything on the interface, and select page 0 registers.
	 */
	if (sc->is790) {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_STP);
	} else {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STP);
	}
	/*
	 * Wait for interface to enter stopped state, but limit # of checks
	 *	to 'n' (about 5ms). It shouldn't even take 5us on modern
	 *	DS8390's, but just in case it's an old one.
	 */
	while (((inb(sc->nic_addr + ED_P0_ISR) & ED_ISR_RST) == 0) && --n);

}

/*
 * Device timeout/watchdog routine. Entered if the device neglects to
 *	generate an interrupt after a transmit has been started on it.
 */
void
ed_watchdog(unit)
	int unit;
{
	struct ed_softc *sc = &ed_softc[unit];

	log(LOG_ERR, "ed%d: device timeout\n", unit);
	++sc->arpcom.ac_if.if_oerrors;

	ed_reset(unit, 0);
}

/*
 * Initialize device. 
 */
void
ed_init(unit)
	int unit;
{
	struct ed_softc *sc = &ed_softc[unit];
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int i, s;
	u_char	command;


	/* address not known */
	if (ifp->if_addrlist == (struct ifaddr *)0) return;

	/*
	 * Initialize the NIC in the exact order outlined in the NS manual.
	 *	This init procedure is "mandatory"...don't change what or when
	 *	things happen.
	 */
	s = splimp();

	/* reset transmitter flags */
	sc->xmit_busy = 0;
	sc->arpcom.ac_if.if_timer = 0;

	sc->txb_inuse = 0;
	sc->txb_new = 0;
	sc->txb_next_tx = 0;

	/* This variable is used below - don't move this assignment */
	sc->next_packet = sc->rec_page_start + 1;

	/*
	 * Set interface for page 0, Remote DMA complete, Stopped
	 */
	if (sc->is790) {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_STP);
	} else {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STP);
	}
	if (sc->isa16bit) {
		/*
		 * Set FIFO threshold to 8, No auto-init Remote DMA,
		 *	byte order=80x86, word-wide DMA xfers,
		 */
		outb(sc->nic_addr + ED_P0_DCR, ED_DCR_FT1|ED_DCR_WTS|ED_DCR_LS);
	} else {
		/*
		 * Same as above, but byte-wide DMA xfers
		 */
		outb(sc->nic_addr + ED_P0_DCR, ED_DCR_FT1|ED_DCR_LS);
	}

	/*
	 * Clear Remote Byte Count Registers
	 */
	outb(sc->nic_addr + ED_P0_RBCR0, 0);
	outb(sc->nic_addr + ED_P0_RBCR1, 0);

	/*
	 * Enable reception of broadcast packets
	 */
	outb(sc->nic_addr + ED_P0_RCR, ED_RCR_AB);

	/*
	 * Place NIC in internal loopback mode
	 */
	outb(sc->nic_addr + ED_P0_TCR, ED_TCR_LB0);

	/*
	 * Initialize transmit/receive (ring-buffer) Page Start
	 */
	outb(sc->nic_addr + ED_P0_TPSR, sc->tx_page_start);
	outb(sc->nic_addr + ED_P0_PSTART, sc->rec_page_start);
	/* Set lower bits of byte addressable framing to 0 */
	if (sc->is790)
		outb(sc->nic_addr + 0x09, 0);

	/*
	 * Initialize Receiver (ring-buffer) Page Stop and Boundry
	 */
	outb(sc->nic_addr + ED_P0_PSTOP, sc->rec_page_stop);
	outb(sc->nic_addr + ED_P0_BNRY, sc->rec_page_start);

	/*
	 * Clear all interrupts. A '1' in each bit position clears the
	 *	corresponding flag.
	 */
	outb(sc->nic_addr + ED_P0_ISR, 0xff);

	/*
	 * Enable the following interrupts: receive/transmit complete,
	 *	receive/transmit error, and Receiver OverWrite.
	 *
	 * Counter overflow and Remote DMA complete are *not* enabled.
	 */
	outb(sc->nic_addr + ED_P0_IMR,
		ED_IMR_PRXE|ED_IMR_PTXE|ED_IMR_RXEE|ED_IMR_TXEE|ED_IMR_OVWE);

	/*
	 * Program Command Register for page 1
	 */
	if (sc->is790) {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_PAGE_1|ED_CR_STP);
	} else {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_PAGE_1|ED_CR_RD2|ED_CR_STP);
	}
	/*
	 * Copy out our station address
	 */
	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		outb(sc->nic_addr + ED_P1_PAR0 + i, sc->arpcom.ac_enaddr[i]);

#if NBPFILTER > 0
	/*
	 * Initialize multicast address hashing registers to accept
	 *	 all multicasts (only used when in promiscuous mode)
	 */
	for (i = 0; i < 8; ++i)
		outb(sc->nic_addr + ED_P1_MAR0 + i, 0xff);
#endif

	/*
	 * Set Current Page pointer to next_packet (initialized above)
	 */
	outb(sc->nic_addr + ED_P1_CURR, sc->next_packet);

	/*
	 * Set Command Register for page 0, Remote DMA complete,
	 * 	and interface Start.
	 */
	if (sc->is790) {
		outb(sc->nic_addr + ED_P1_CR, ED_CR_STA);
	} else {
		outb(sc->nic_addr + ED_P1_CR, ED_CR_RD2|ED_CR_STA);
	}
	/*
	 * Take interface out of loopback
	 */
	outb(sc->nic_addr + ED_P0_TCR, 0);

	/*
	 * If this is a 3Com board, the tranceiver must be software enabled
	 *	(there is no settable hardware default).
	 */
	if (sc->vendor == ED_VENDOR_3COM) {
		if (ifp->if_flags & IFF_ALTPHYS) {
			outb(sc->asic_addr + ED_3COM_CR, 0);
		} else {
			outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_XSEL);
		}
	}

	/*
	 * Set 'running' flag, and clear output active flag.
	 */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * ...and attempt to start output
	 */
	ed_start(ifp);

	(void) splx(s);
}
 
/*
 * This routine actually starts the transmission on the interface
 */
static inline void ed_xmit(ifp)
	struct ifnet *ifp;
{
	struct ed_softc *sc = &ed_softc[ifp->if_unit];
	unsigned short len;

	len = sc->txb_len[sc->txb_next_tx];

	/*
	 * Set NIC for page 0 register access
	 */
	if (sc->is790) {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_STA);
	} else {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STA);
	}
	/*
	 * Set TX buffer start page
	 */
	outb(sc->nic_addr + ED_P0_TPSR, sc->tx_page_start +
		sc->txb_next_tx * ED_TXBUF_SIZE);

	/*
	 * Set TX length
	 */
	outb(sc->nic_addr + ED_P0_TBCR0, len);
	outb(sc->nic_addr + ED_P0_TBCR1, len >> 8);

	/*
	 * Set page 0, Remote DMA complete, Transmit Packet, and *Start*
	 */
	if (sc->is790) {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_TXP | ED_CR_STA);
	} else {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_TXP|ED_CR_STA);
	}
	sc->xmit_busy = 1;
	
	/*
	 * Point to next transmit buffer slot and wrap if necessary.
	 */
	sc->txb_next_tx++;
	if (sc->txb_next_tx == sc->txb_cnt)
		sc->txb_next_tx = 0;

	/*
	 * Set a timer just in case we never hear from the board again
	 */
	ifp->if_timer = 2;
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
void
ed_start(ifp)
	struct ifnet *ifp;
{
	struct ed_softc *sc = &ed_softc[ifp->if_unit];
	struct mbuf *m0, *m;
	caddr_t buffer;
	int len;

outloop:
	/*
	 * First, see if there are buffered packets and an idle
	 *	transmitter - should never happen at this point.
	 */
	if (sc->txb_inuse && (sc->xmit_busy == 0)) {
		printf("ed: packets buffers, but transmitter idle\n");
		ed_xmit(ifp);
	}

	/*
	 * See if there is room to put another packet in the buffer.
	 */
	if (sc->txb_inuse == sc->txb_cnt) {
		/*
		 * No room. Indicate this to the outside world
		 *	and exit.
		 */
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);
	if (m == 0) {
	/*
	 * We are using the !OACTIVE flag to indicate to the outside
	 * world that we can accept an additional packet rather than
	 * that the transmitter is _actually_ active. Indeed, the
	 * transmitter may be active, but if we haven't filled all
	 * the buffers with data then we still want to accept more.
	 */
		ifp->if_flags &= ~IFF_OACTIVE;
		return;
	}

	/*
	 * Copy the mbuf chain into the transmit buffer
	 */

	m0 = m;

	/* txb_new points to next open buffer slot */
	buffer = sc->mem_start + (sc->txb_new * ED_TXBUF_SIZE * ED_PAGE_SIZE);

	if (sc->mem_shared) {
		/*
		 * Special case setup for 16 bit boards...
		 */
		if (sc->isa16bit) {
			switch (sc->vendor) {
			/*
			 * For 16bit 3Com boards (which have 16k of memory),
			 *	we have the xmit buffers in a different page
			 *	of memory ('page 0') - so change pages.
			 */
			case ED_VENDOR_3COM:
				outb(sc->asic_addr + ED_3COM_GACFR,
					ED_3COM_GACFR_RSEL);
				break;
			/*
			 * Enable 16bit access to shared memory on WD/SMC boards
			 *	Don't update wd_laar_proto because we want to restore the
			 *	previous state (because an arp reply in the input code
			 *	may cause a call-back to ed_start)
			 * XXX - the call-back to 'start' is a bug, IMHO.
			 */
			case ED_VENDOR_WD_SMC:
				outb(sc->asic_addr + ED_WD_LAAR,
					(sc->wd_laar_proto | ED_WD_LAAR_M16EN));
			}
		}

		for (len = 0; m != 0; m = m->m_next) {
			bcopy(mtod(m, caddr_t), buffer, m->m_len);
			buffer += m->m_len;
      	 		len += m->m_len;
		}

		/*
		 * Restore previous shared memory access
		 */
		if (sc->isa16bit) {
			switch (sc->vendor) {
			case ED_VENDOR_3COM:
				outb(sc->asic_addr + ED_3COM_GACFR,
					ED_3COM_GACFR_RSEL | ED_3COM_GACFR_MBS0);
				break;
			case ED_VENDOR_WD_SMC:
				outb(sc->asic_addr + ED_WD_LAAR, sc->wd_laar_proto);
				break;
			}
		}
	} else {
		len = ed_pio_write_mbufs(sc, m, buffer);
	}
		
	sc->txb_len[sc->txb_new] = MAX(len, ETHER_MIN_LEN);

	sc->txb_inuse++;

	/*
	 * Point to next buffer slot and wrap if necessary.
	 */
	sc->txb_new++;
	if (sc->txb_new == sc->txb_cnt)
		sc->txb_new = 0;

	if (sc->xmit_busy == 0)
		ed_xmit(ifp);
	/*
	 * If there is BPF support in the configuration, tap off here.
	 *   The following has support for converting trailer packets
	 *   back to normal.
	 * XXX - support for trailer packets in BPF should be moved into
	 *	the bpf code proper to avoid code duplication in all of
	 *	the drivers.
	 */
#if NBPFILTER > 0
	if (sc->bpf) {
		u_short etype;
		int off, datasize, resid;
		struct ether_header *eh;
		struct trailer_header trailer_header;
		char ether_packet[ETHER_MAX_LEN];
		char *ep;

		ep = ether_packet;

		/*
		 * We handle trailers below:
		 * Copy ether header first, then residual data,
		 * then data. Put all this in a temporary buffer
		 * 'ether_packet' and send off to bpf. Since the
		 * system has generated this packet, we assume
		 * that all of the offsets in the packet are
		 * correct; if they're not, the system will almost
		 * certainly crash in m_copydata.
		 * We make no assumptions about how the data is
		 * arranged in the mbuf chain (i.e. how much
		 * data is in each mbuf, if mbuf clusters are
		 * used, etc.), which is why we use m_copydata
		 * to get the ether header rather than assume
		 * that this is located in the first mbuf.
		 */
		/* copy ether header */
		m_copydata(m0, 0, sizeof(struct ether_header), ep);
		eh = (struct ether_header *) ep;
		ep += sizeof(struct ether_header);
		etype = ntohs(eh->ether_type);
		if (etype >= ETHERTYPE_TRAIL &&
		    etype < ETHERTYPE_TRAIL+ETHERTYPE_NTRAILER) {
			datasize = ((etype - ETHERTYPE_TRAIL) << 9);
			off = datasize + sizeof(struct ether_header);

			/* copy trailer_header into a data structure */
			m_copydata(m0, off, sizeof(struct trailer_header),
				&trailer_header.ether_type);

			/* copy residual data */
			m_copydata(m0, off+sizeof(struct trailer_header),
				resid = ntohs(trailer_header.ether_residual) -
				sizeof(struct trailer_header), ep);
			ep += resid;

			/* copy data */
			m_copydata(m0, sizeof(struct ether_header),
				datasize, ep);
			ep += datasize;

			/* restore original ether packet type */
			eh->ether_type = trailer_header.ether_type;

			bpf_tap(sc->bpf, ether_packet, ep - ether_packet);
		} else
			bpf_mtap(sc->bpf, m0);
	}
#endif

	m_freem(m0);

	/*
	 * Loop back to the top to possibly buffer more packets
	 */
	goto outloop;
}
 
/*
 * Ethernet interface receiver interrupt.
 */
static inline void
ed_rint(unit)
	int unit;
{
	register struct ed_softc *sc = &ed_softc[unit];
	u_char boundry, current;
	u_short len;
	struct ed_ring packet_hdr;
	char *packet_ptr;

	/*
	 * Set NIC to page 1 registers to get 'current' pointer
	 */
	if (sc->is790) {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_PAGE_1|ED_CR_STA);
	} else {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_PAGE_1|ED_CR_RD2|ED_CR_STA);
	}
	/*
	 * 'sc->next_packet' is the logical beginning of the ring-buffer - i.e.
	 *	it points to where new data has been buffered. The 'CURR'
	 *	(current) register points to the logical end of the ring-buffer
	 *	- i.e. it points to where additional new data will be added.
	 *	We loop here until the logical beginning equals the logical
	 *	end (or in other words, until the ring-buffer is empty).
	 */
	while (sc->next_packet != inb(sc->nic_addr + ED_P1_CURR)) {

		/* get pointer to this buffer's header structure */
		packet_ptr = sc->mem_ring +
			(sc->next_packet - sc->rec_page_start) * ED_PAGE_SIZE;

		/*
		 * The byte count includes the FCS - Frame Check Sequence (a
		 *	32 bit CRC).
		 */
		if (sc->mem_shared)
			packet_hdr = *(struct ed_ring *)packet_ptr;
		else
			ed_pio_readmem(sc, packet_ptr, (char *) &packet_hdr,
				sizeof(packet_hdr));
		len = packet_hdr.count;
		if ((len >= ETHER_MIN_LEN) && (len <= ETHER_MAX_LEN)) {
			/*
			 * Go get packet. len - 4 removes CRC from length.
			 */
			ed_get_packet(sc, packet_ptr + 4, len - 4);
			++sc->arpcom.ac_if.if_ipackets;
		} else {
			/*
			 * Really BAD...probably indicates that the ring pointers
			 *	are corrupted. Also seen on early rev chips under
			 *	high load - the byte order of the length gets switched.
			 */
			log(LOG_ERR,
				"ed%d: NIC memory corrupt - invalid packet length %d\n",
				unit, len);
			++sc->arpcom.ac_if.if_ierrors;
			ed_reset(unit, 0);
			return;
		}

		/*
		 * Update next packet pointer
		 */
		sc->next_packet = packet_hdr.next_packet;

		/*
		 * Update NIC boundry pointer - being careful to keep it
		 *	one buffer behind. (as recommended by NS databook)
		 */
		boundry = sc->next_packet - 1;
		if (boundry < sc->rec_page_start)
			boundry = sc->rec_page_stop - 1;

		/*
		 * Set NIC to page 0 registers to update boundry register
		 */
		if (sc->is790) {
			outb(sc->nic_addr + ED_P0_CR, ED_CR_STA);
		} else {
			outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STA);
		}
		outb(sc->nic_addr + ED_P0_BNRY, boundry);

		/*
		 * Set NIC to page 1 registers before looping to top (prepare to
		 *	get 'CURR' current pointer)
		 */
		if (sc->is790) {
			outb(sc->nic_addr + ED_P0_CR, ED_CR_PAGE_1|ED_CR_STA);
		} else {
			outb(sc->nic_addr + ED_P0_CR, ED_CR_PAGE_1|ED_CR_RD2|ED_CR_STA);
		}
	}
}

/*
 * Ethernet interface interrupt processor
 */
void
edintr(unit)
	int unit;
{
	struct ed_softc *sc = &ed_softc[unit];
	u_char isr;

	/*
	 * Set NIC to page 0 registers
	 */
	if (sc->is790) {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_STA);
	} else {
		outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STA);
	}
	/*
	 * loop until there are no more new interrupts
	 */
	while (isr = inb(sc->nic_addr + ED_P0_ISR)) {

		/*
		 * reset all the bits that we are 'acknowledging'
		 *	by writing a '1' to each bit position that was set
		 * (writing a '1' *clears* the bit)
		 */
		outb(sc->nic_addr + ED_P0_ISR, isr);

		/*
		 * Handle transmitter interrupts. Handle these first
		 *	because the receiver will reset the board under
		 *	some conditions.
		 */
		if (isr & (ED_ISR_PTX|ED_ISR_TXE)) {
			u_char collisions = inb(sc->nic_addr + ED_P0_NCR) & 0x0f;

			/*
			 * Check for transmit error. If a TX completed with an
			 * error, we end up throwing the packet away. Really
			 * the only error that is possible is excessive
			 * collisions, and in this case it is best to allow the
			 * automatic mechanisms of TCP to backoff the flow. Of
			 * course, with UDP we're screwed, but this is expected
			 * when a network is heavily loaded.
			 */
			(void) inb(sc->nic_addr + ED_P0_TSR);
			if (isr & ED_ISR_TXE) {

				/*
				 * Excessive collisions (16)
				 */
				if ((inb(sc->nic_addr + ED_P0_TSR) & ED_TSR_ABT)
					&& (collisions == 0)) {
					/*
					 *    When collisions total 16, the
					 * P0_NCR will indicate 0, and the
					 * TSR_ABT is set.
					 */
					collisions = 16;
				}

				/*
				 * update output errors counter
				 */
				++sc->arpcom.ac_if.if_oerrors;
			} else {
				/*
				 * Update total number of successfully
				 * 	transmitted packets.
				 */
				++sc->arpcom.ac_if.if_opackets;
			}

			/*
			 * reset tx busy and output active flags
			 */
			sc->xmit_busy = 0;
			sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;

			/*
			 * clear watchdog timer
			 */
			sc->arpcom.ac_if.if_timer = 0;

			/*
			 * Add in total number of collisions on last
			 *	transmission.
			 */
			sc->arpcom.ac_if.if_collisions += collisions;

			/*
			 * Decrement buffer in-use count if not zero (can only
			 *	be zero if a transmitter interrupt occured while
			 *	not actually transmitting).
			 * If data is ready to transmit, start it transmitting,
			 *	otherwise defer until after handling receiver
			 */
			if (sc->txb_inuse && --sc->txb_inuse)
				ed_xmit(&sc->arpcom.ac_if);
		}

		/*
		 * Handle receiver interrupts
		 */
		if (isr & (ED_ISR_PRX|ED_ISR_RXE|ED_ISR_OVW)) {
		    /*
		     * Overwrite warning. In order to make sure that a lockup
		     *	of the local DMA hasn't occurred, we reset and
		     *	re-init the NIC. The NSC manual suggests only a
		     *	partial reset/re-init is necessary - but some
		     *	chips seem to want more. The DMA lockup has been
		     *	seen only with early rev chips - Methinks this
		     *	bug was fixed in later revs. -DG
		     */
			if (isr & ED_ISR_OVW) {
				++sc->arpcom.ac_if.if_ierrors;
#ifdef DIAGNOSTIC
				log(LOG_WARNING,
					"ed%d: warning - receiver ring buffer overrun\n",
					unit);
#endif
				/*
				 * Stop/reset/re-init NIC
				 */
				ed_reset(unit, 0);
			} else {

			    /*
			     * Receiver Error. One or more of: CRC error, frame
			     *	alignment error FIFO overrun, or missed packet.
			     */
				if (isr & ED_ISR_RXE) {
					++sc->arpcom.ac_if.if_ierrors;
#ifdef ED_DEBUG
					printf("ed%d: receive error %x\n", unit,
						inb(sc->nic_addr + ED_P0_RSR));
#endif
				}

				/*
				 * Go get the packet(s)
				 * XXX - Doing this on an error is dubious
				 *    because there shouldn't be any data to
				 *    get (we've configured the interface to
				 *    not accept packets with errors).
				 */

				/*
				 * Enable 16bit access to shared memory first
				 *	on WD/SMC boards.
				 */
				if (sc->isa16bit &&
					(sc->vendor == ED_VENDOR_WD_SMC)) {

					outb(sc->asic_addr + ED_WD_LAAR,
						(sc->wd_laar_proto |=
						 ED_WD_LAAR_M16EN));
				}

				ed_rint (unit);

				/* disable 16bit access */
				if (sc->isa16bit &&
					(sc->vendor == ED_VENDOR_WD_SMC)) {

					outb(sc->asic_addr + ED_WD_LAAR,
						(sc->wd_laar_proto &=
						 ~ED_WD_LAAR_M16EN));
				}
			}
		}

		/*
		 * If it looks like the transmitter can take more data,
		 * 	attempt to start output on the interface.
		 *	This is done after handling the receiver to
		 *	give the receiver priority.
		 */
		if ((sc->arpcom.ac_if.if_flags & IFF_OACTIVE) == 0)
			ed_start(&sc->arpcom.ac_if);

		/*
		 * return NIC CR to standard state: page 0, remote DMA complete,
		 * 	start (toggling the TXP bit off, even if was just set
		 *	in the transmit routine, is *okay* - it is 'edge'
		 *	triggered from low to high)
		 */
		if (sc->is790) {
			outb(sc->nic_addr + ED_P0_CR, ED_CR_STA);
		} else {
			outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STA);
		}
		/*
		 * If the Network Talley Counters overflow, read them to
		 *	reset them. It appears that old 8390's won't
		 *	clear the ISR flag otherwise - resulting in an
		 *	infinite loop.
		 */
		if (isr & ED_ISR_CNT) {
			(void) inb(sc->nic_addr + ED_P0_CNTR0);
			(void) inb(sc->nic_addr + ED_P0_CNTR1);
			(void) inb(sc->nic_addr + ED_P0_CNTR2);
		}
	}
}
 
/*
 * Process an ioctl request. This code needs some work - it looks
 *	pretty ugly.
 */
int
ed_ioctl(ifp, command, data)
	register struct ifnet *ifp;
	int command;
	caddr_t data;
{
	register struct ifaddr *ifa = (struct ifaddr *)data;
	struct ed_softc *sc = &ed_softc[ifp->if_unit];
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splimp();

	switch (command) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			ed_init(ifp->if_unit);	/* before arpwhohas */
			/*
			 * See if another station has *our* IP address.
			 * i.e.: There is an address conflict! If a
			 * conflict exists, a message is sent to the
			 * console.
			 */
			((struct arpcom *)ifp)->ac_ipaddr =
				IA_SIN(ifa)->sin_addr;
			arpwhohas((struct arpcom *)ifp, &IA_SIN(ifa)->sin_addr);
			break;
#endif
#ifdef NS
		/*
		 * XXX - This code is probably wrong
		 */
		case AF_NS:
		    {
			register struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);

			if (ns_nullhost(*ina))
				ina->x_host =
					*(union ns_host *)(sc->arpcom.ac_enaddr);
			else {
				/* 
				 * 
				 */
				bcopy((caddr_t)ina->x_host.c_host,
				    (caddr_t)sc->arpcom.ac_enaddr,
					sizeof(sc->arpcom.ac_enaddr));
			}
			/*
			 * Set new address
			 */
			ed_init(ifp->if_unit);
			break;
		    }
#endif
		default:
			ed_init(ifp->if_unit);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		/*
		 * If interface is marked down and it is running, then stop it
		 */
		if (((ifp->if_flags & IFF_UP) == 0) &&
		    (ifp->if_flags & IFF_RUNNING)) {
			ed_stop(ifp->if_unit);
			ifp->if_flags &= ~IFF_RUNNING;
		} else {
		/*
		 * If interface is marked up and it is stopped, then start it
		 */
			if ((ifp->if_flags & IFF_UP) &&
		    	    ((ifp->if_flags & IFF_RUNNING) == 0))
				ed_init(ifp->if_unit);
		}
#if NBPFILTER > 0
		if (ifp->if_flags & IFF_PROMISC) {
			/*
			 * Set promiscuous mode on interface.
			 *	XXX - for multicasts to work, we would need to
			 *		write 1's in all bits of multicast
			 *		hashing array. For now we assume that
			 *		this was done in ed_init().
			 */
			outb(sc->nic_addr + ED_P0_RCR,
				ED_RCR_PRO|ED_RCR_AM|ED_RCR_AB);
		} else {
			/*
			 * XXX - for multicasts to work, we would need to
			 *	rewrite the multicast hashing array with the
			 *	proper hash (would have been destroyed above).
			 */
			outb(sc->nic_addr + ED_P0_RCR, ED_RCR_AB);
		}
#endif
		/*
		 * An unfortunate hack to provide the (required) software control
		 *	of the tranceiver for 3Com boards. The ALTPHYS flag disables
		 *	the tranceiver if set.
		 */
		if (sc->vendor == ED_VENDOR_3COM) {
			if (ifp->if_flags & IFF_ALTPHYS) {
				outb(sc->asic_addr + ED_3COM_CR, 0);
			} else {
				outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_XSEL);
			}
		}

		break;

	default:
		error = EINVAL;
	}
	(void) splx(s);
	return (error);
}
 
/*
 * Macro to calculate a new address within shared memory when given an offset
 *	from an address, taking into account ring-wrap.
 */
#define	ringoffset(sc, start, off, type) \
	((type)( ((caddr_t)(start)+(off) >= (sc)->mem_end) ? \
		(((caddr_t)(start)+(off))) - (sc)->mem_end \
		+ (sc)->mem_ring: \
		((caddr_t)(start)+(off)) ))

/*
 * Retreive packet from shared memory and send to the next level up via
 *	ether_input(). If there is a BPF listener, give a copy to BPF, too.
 */
static void
ed_get_packet(sc, buf, len)
	struct ed_softc *sc;
	char *buf;
	u_short len;
{
	struct ether_header *eh;
    	struct mbuf *m, *head = 0, *ed_ring_to_mbuf();
	u_short off;
	int resid;
	u_short etype;
	struct trailer_header trailer_header;

	/* Allocate a header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		goto bad;
	m->m_pkthdr.rcvif = &sc->arpcom.ac_if;
	m->m_pkthdr.len = len;
	m->m_len = 0;
	head = m;

	/* The following sillines is to make NFS happy */
#define EROUND	((sizeof(struct ether_header) + 3) & ~3)
#define EOFF	(EROUND - sizeof(struct ether_header))

	/*
	 * The following assumes there is room for
	 * the ether header in the header mbuf
	 */
	head->m_data += EOFF;
	eh = mtod(head, struct ether_header *);

	if (sc->mem_shared)
		bcopy(buf, mtod(head, caddr_t), sizeof(struct ether_header));
	else
		ed_pio_readmem(sc, buf, mtod(head, caddr_t),
			sizeof(struct ether_header));
	buf += sizeof(struct ether_header);
	head->m_len += sizeof(struct ether_header);
	len -= sizeof(struct ether_header);

	etype = ntohs((u_short)eh->ether_type);

	/*
	 * Deal with trailer protocol:
	 * If trailer protocol, calculate the datasize as 'off',
	 * which is also the offset to the trailer header.
	 * Set resid to the amount of packet data following the
	 * trailer header.
	 * Finally, copy residual data into mbuf chain.
	 */
	if (etype >= ETHERTYPE_TRAIL &&
	    etype < ETHERTYPE_TRAIL+ETHERTYPE_NTRAILER) {

		off = (etype - ETHERTYPE_TRAIL) << 9;
		if ((off + sizeof(struct trailer_header)) > len)
			goto bad;	/* insanity */

		/*
		 * If we have shared memory, we can get info directly from the
		 *	stored packet, otherwise we must get a local copy
		 *	of the trailer header using PIO.
		 */
		if (sc->mem_shared) {
			eh->ether_type = *ringoffset(sc, buf, off, u_short *);
			resid = ntohs(*ringoffset(sc, buf, off+2, u_short *));
		} else {
			struct trailer_header trailer_header;
			ed_pio_readmem(sc,
				ringoffset(sc, buf, off, caddr_t),
				(char *) &trailer_header,
				sizeof(trailer_header));
			eh->ether_type = trailer_header.ether_type;
			resid = trailer_header.ether_residual;
		}

		if ((off + resid) > len) goto bad;	/* insanity */

		resid -= sizeof(struct trailer_header);
		if (resid < 0) goto bad;	/* insanity */

		m = ed_ring_to_mbuf(sc, ringoffset(sc, buf, off+4, char *), head, resid);
		if (m == 0) goto bad;

		len = off;
		head->m_pkthdr.len -= 4; /* subtract trailer header */
	}

	/*
	 * Pull packet off interface. Or if this was a trailer packet,
	 * the data portion is appended.
	 */
	m = ed_ring_to_mbuf(sc, buf, m, len);
	if (m == 0) goto bad;

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to bpf. 
	 */
	if (sc->bpf) {
		bpf_mtap(sc->bpf, head);

		/*
		 * Note that the interface cannot be in promiscuous mode if
		 * there are no BPF listeners.  And if we are in promiscuous
		 * mode, we have to check if this packet is really ours.
		 *
		 * XXX This test does not support multicasts.
		 */
		if ((sc->arpcom.ac_if.if_flags & IFF_PROMISC) &&
			bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr,
				sizeof(eh->ether_dhost)) != 0 &&
			bcmp(eh->ether_dhost, etherbroadcastaddr,
				sizeof(eh->ether_dhost)) != 0) {

			m_freem(head);
			return;
		}
	}
#endif

	/*
	 * Fix up data start offset in mbuf to point past ether header
	 */
	m_adj(head, sizeof(struct ether_header));

	/*
	 * silly ether_input routine needs 'type' in host byte order
	 */
	eh->ether_type = ntohs(eh->ether_type);

	ether_input(&sc->arpcom.ac_if, eh, head);
	return;

bad:	if (head)
		m_freem(head);
	return;
}

/*
 * Supporting routines
 */

/*
 * Given a NIC memory source address and a host memory destination
 *	address, copy 'amount' from NIC to host using Programmed I/O.
 *	The 'amount' is rounded up to a word - okay as long as mbufs
 *		are word sized.
 *	This routine is currently Novell-specific.
 */
void
ed_pio_readmem(sc,src,dst,amount)
	struct	ed_softc *sc;
	unsigned short src;
	unsigned char *dst;
	unsigned short amount;
{
	unsigned short tmp_amount;

	/* select page 0 registers */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STA);

	/* round up to a word */
	tmp_amount = amount;
	if (amount & 1) ++amount;

	/* set up DMA byte count */
	outb(sc->nic_addr + ED_P0_RBCR0, amount);
	outb(sc->nic_addr + ED_P0_RBCR1, amount>>8);

	/* set up source address in NIC mem */
	outb(sc->nic_addr + ED_P0_RSAR0, src);
	outb(sc->nic_addr + ED_P0_RSAR1, src>>8);

	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD0 | ED_CR_STA);

	if (sc->isa16bit) {
		insw(sc->asic_addr + ED_NOVELL_DATA, dst, amount/2);
	} else
		insb(sc->asic_addr + ED_NOVELL_DATA, dst, amount);

}

/*
 * Stripped down routine for writing a linear buffer to NIC memory.
 *	Only used in the probe routine to test the memory. 'len' must
 *	be even.
 */
void
ed_pio_writemem(sc,src,dst,len)
	struct ed_softc *sc;
	char *src;
	unsigned short dst;
	unsigned short len;
{
	int maxwait=100; /* about 120us */

	/* select page 0 registers */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STA);

	/* reset remote DMA complete flag */
	outb(sc->nic_addr + ED_P0_ISR, ED_ISR_RDC);

	/* set up DMA byte count */
	outb(sc->nic_addr + ED_P0_RBCR0, len);
	outb(sc->nic_addr + ED_P0_RBCR1, len>>8);

	/* set up destination address in NIC mem */
	outb(sc->nic_addr + ED_P0_RSAR0, dst);
	outb(sc->nic_addr + ED_P0_RSAR1, dst>>8);

	/* set remote DMA write */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD1 | ED_CR_STA);

	if (sc->isa16bit)
		outsw(sc->asic_addr + ED_NOVELL_DATA, src, len/2);
	else
		outsb(sc->asic_addr + ED_NOVELL_DATA, src, len);
	/*
	 * Wait for remote DMA complete. This is necessary because on the
	 *	transmit side, data is handled internally by the NIC in bursts
	 *	and we can't start another remote DMA until this one completes.
	 *	Not waiting causes really bad things to happen - like the NIC
	 *	irrecoverably jamming the ISA bus.
	 */
	while (((inb(sc->nic_addr + ED_P0_ISR) & ED_ISR_RDC) != ED_ISR_RDC) && --maxwait);
}

/*
 * Write an mbuf chain to the destination NIC memory address using
 *	programmed I/O.
 */
u_short
ed_pio_write_mbufs(sc,m,dst)
	struct ed_softc *sc;
	struct mbuf *m;
	unsigned short dst;
{
	unsigned short len, mb_offset;
	struct mbuf *mp;
	unsigned char residual[2];
	int maxwait=100; /* about 120us */

	/* First, count up the total number of bytes to copy */
	for (len = 0, mp = m; mp; mp = mp->m_next)
		len += mp->m_len;
	
	/* select page 0 registers */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STA);

	/* reset remote DMA complete flag */
	outb(sc->nic_addr + ED_P0_ISR, ED_ISR_RDC);

	/* set up DMA byte count */
	outb(sc->nic_addr + ED_P0_RBCR0, len);
	outb(sc->nic_addr + ED_P0_RBCR1, len>>8);

	/* set up destination address in NIC mem */
	outb(sc->nic_addr + ED_P0_RSAR0, dst);
	outb(sc->nic_addr + ED_P0_RSAR1, dst>>8);

	/* set remote DMA write */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD1 | ED_CR_STA);

	mb_offset = 0;
	/*
	 * Transfer the mbuf chain to the NIC memory.
	 * The following code isn't too pretty. The problem is that we can only
	 *	transfer words to the board, and if an mbuf has an odd number
	 *	of bytes in it, this is a problem. It's not a simple matter of
	 *	just removing a byte from the next mbuf (adjusting data++ and
	 *	len--) because this will hose-over the mbuf chain which might
	 *	be needed later for BPF. Instead, we maintain an offset
	 *	(mb_offset) which let's us skip over the first byte in the
	 *	following mbuf.
	 */
	while (m) {
		if (m->m_len - mb_offset) {
			if (sc->isa16bit) {
				if ((m->m_len - mb_offset) > 1)
					outsw(sc->asic_addr + ED_NOVELL_DATA,
						mtod(m, caddr_t) + mb_offset,
						(m->m_len - mb_offset) / 2);

				/*
				 * if odd number of bytes, get the odd byte from
				 * the next mbuf with data
				 */
				if ((m->m_len - mb_offset) & 1) {
					/* first the last byte in current mbuf */
					residual[0] = *(mtod(m, caddr_t)
						+ m->m_len - 1);
					
					/* advance past any empty mbufs */
					while (m->m_next && (m->m_next->m_len == 0))
						m = m->m_next;

					if (m->m_next) {
						/* remove first byte in next mbuf */
						residual[1] = *(mtod(m->m_next, caddr_t));
						mb_offset = 1;
					}

					outw(sc->asic_addr + ED_NOVELL_DATA,
						*((unsigned short *) residual));
				} else
					mb_offset = 0;
			} else
				outsb(sc->asic_addr + ED_NOVELL_DATA, m->m_data, m->m_len);

		}
		m = m->m_next;
	}

	/*
	 * Wait for remote DMA complete. This is necessary because on the
	 *	transmit side, data is handled internally by the NIC in bursts
	 *	and we can't start another remote DMA until this one completes.
	 *	Not waiting causes really bad things to happen - like the NIC
	 *	irrecoverably jamming the ISA bus.
	 */
	while (((inb(sc->nic_addr + ED_P0_ISR) & ED_ISR_RDC) != ED_ISR_RDC) && --maxwait);

	if (!maxwait) {
		log(LOG_WARNING, "ed%d: remote transmit DMA failed to complete\n",
			sc->arpcom.ac_if.if_unit);
		ed_reset(sc->arpcom.ac_if.if_unit, 0);
	}

	return(len);
}
	
/*
 * Given a source and destination address, copy 'amount' of a packet from
 *	the ring buffer into a linear destination buffer. Takes into account
 *	ring-wrap.
 */
static inline char *
ed_ring_copy(sc,src,dst,amount)
	struct ed_softc *sc;
	char	*src;
	char	*dst;
	u_short	amount;
{
	u_short	tmp_amount;

	/* does copy wrap to lower addr in ring buffer? */
	if (src + amount > sc->mem_end) {
		tmp_amount = sc->mem_end - src;

		/* copy amount up to end of NIC memory */
		if (sc->mem_shared)
			bcopy(src,dst,tmp_amount);
		else
			ed_pio_readmem(sc,src,dst,tmp_amount);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst += tmp_amount;
	}

	if (sc->mem_shared)
		bcopy(src, dst, amount);
	else
		ed_pio_readmem(sc, src, dst, amount);

	return(src + amount);
}

/*
 * Copy data from receive buffer to end of mbuf chain
 * allocate additional mbufs as needed. return pointer
 * to last mbuf in chain.
 * sc = ed info (softc)
 * src = pointer in ed ring buffer
 * dst = pointer to last mbuf in mbuf chain to copy to
 * amount = amount of data to copy
 */
struct mbuf *
ed_ring_to_mbuf(sc,src,dst,total_len)
	struct ed_softc *sc;
	char *src;
	struct mbuf *dst;
	u_short total_len;
{
	register struct mbuf *m = dst;

	while (total_len) {
		register u_short amount = min(total_len, M_TRAILINGSPACE(m));

		if (amount == 0) { /* no more data in this mbuf, alloc another */
			/*
			 * If there is enough data for an mbuf cluster, attempt
			 * 	to allocate one of those, otherwise, a regular
			 *	mbuf will do.
			 * Note that a regular mbuf is always required, even if
			 *	we get a cluster - getting a cluster does not
			 *	allocate any mbufs, and one is needed to assign
			 *	the cluster to. The mbuf that has a cluster
			 *	extension can not be used to contain data - only
			 *	the cluster can contain data.
			 */ 
			dst = m;
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0)
				return (0);

			if (total_len >= MINCLSIZE)
				MCLGET(m, M_DONTWAIT);

			m->m_len = 0;
			dst->m_next = m;
			amount = min(total_len, M_TRAILINGSPACE(m));
		}

		src = ed_ring_copy(sc, src, mtod(m, caddr_t) + m->m_len, amount);

		m->m_len += amount;
		total_len -= amount;

	}
	return (m);
}
#endif
