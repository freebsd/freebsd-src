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
 *   the SMC Elite Ultra (8216), the 3Com 3c503, the NE1000 and NE2000,
 *   and a variety of similar clones.
 *
 * $Id: if_ed.c,v 1.80 1995/10/26 20:29:27 julian Exp $
 */

#include "ed.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/devconf.h>

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
#include <net/bpfdesc.h>
#endif

#include <machine/clock.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>
#include <i386/isa/if_edreg.h>

/* For backwards compatibility */
#ifndef IFF_ALTPHYS
#define IFF_ALTPHYS IFF_LINK0
#endif

/*
 * ed_softc: per line info and status
 */
struct ed_softc {
	struct arpcom arpcom;	/* ethernet common */

	char   *type_str;	/* pointer to type string */
	u_char  vendor;		/* interface vendor */
	u_char  type;		/* interface type code */
	u_char	gone;		/* HW missing, presumed having a good time */

	u_short asic_addr;	/* ASIC I/O bus address */
	u_short nic_addr;	/* NIC (DS8390) I/O bus address */

/*
 * The following 'proto' variable is part of a work-around for 8013EBT asics
 *	being write-only. It's sort of a prototype/shadow of the real thing.
 */
	u_char  wd_laar_proto;
	u_char	cr_proto;
	u_char  isa16bit;	/* width of access to card 0=8 or 1=16 */
	int     is790;		/* set by the probe code if the card is 790
				 * based */

	caddr_t bpf;		/* BPF "magic cookie" */
	caddr_t mem_start;	/* NIC memory start address */
	caddr_t mem_end;	/* NIC memory end address */
	u_long  mem_size;	/* total NIC memory size */
	caddr_t mem_ring;	/* start of RX ring-buffer (in NIC mem) */

	u_char  mem_shared;	/* NIC memory is shared with host */
	u_char  xmit_busy;	/* transmitter is busy */
	u_char  txb_cnt;	/* number of transmit buffers */
	u_char  txb_inuse;	/* number of TX buffers currently in-use */

	u_char  txb_new;	/* pointer to where new buffer will be added */
	u_char  txb_next_tx;	/* pointer to next buffer ready to xmit */
	u_short txb_len[8];	/* buffered xmit buffer lengths */
	u_char  tx_page_start;	/* first page of TX buffer area */
	u_char  rec_page_start;	/* first page of RX ring-buffer */
	u_char  rec_page_stop;	/* last page of RX ring-buffer */
	u_char  next_packet;	/* pointer to next unread RX packet */
	struct	kern_devconf kdc; /* kernel configuration database info */
}       ed_softc[NED];

int     ed_attach(struct isa_device *);
void    ed_init(int);
void    edintr(int);
int     ed_ioctl(struct ifnet *, int, caddr_t);
int     ed_probe(struct isa_device *);
void    ed_start(struct ifnet *);
void    ed_reset(int);
void    ed_watchdog(int);
int	ed_probe_generic8390(struct ed_softc *);
int	ed_probe_WD80x3(struct isa_device *);
int	ed_probe_3Com(struct isa_device *);
int	ed_probe_Novell(struct isa_device *);
int	ed_probe_pccard(struct isa_device *, u_char *);

void    ds_getmcaf();

static void ed_get_packet(struct ed_softc *, char *, int /* u_short */ , int);
static void ed_stop(int);

static inline void ed_rint();
static inline void ed_xmit();
static inline char *ed_ring_copy();

void    ed_pio_readmem(), ed_pio_writemem();
u_short ed_pio_write_mbufs();

void    ed_setrcr(struct ifnet *, struct ed_softc *);

#include "crd.h"
#if NCRD > 0
#include <sys/select.h>
#include <pccard/card.h>
#include <pccard/slot.h>
/*
 *	PC-Card (PCMCIA) specific code.
 */
static int card_intr(struct pccard_dev *);	/* Interrupt handler */
void edunload(struct pccard_dev *);	/* Disable driver */
void edsuspend(struct pccard_dev *);	/* Suspend driver */
static int edinit(struct pccard_dev *, int);	/* init device */

static struct pccard_drv ed_info =
	{
	"ed",
	card_intr,
	edunload,
	edsuspend,
	edinit,
	0,			/* Attributes - presently unused */
	&net_imask		/* Interrupt mask for device */
				/* This should also include net_imask?? */
	};
/*
 * Called when a power down is wanted. Shuts down the
 * device and configures the device as unavailable (but
 * still loaded...). A resume is done by calling
 * edinit with first=0. This is called when the user suspends
 * the system, or the APM code suspends the system.
 */
void
edsuspend(struct pccard_dev *dp)
{
	printf("ed%d: suspending\n", dp->isahd.id_unit);
}
/*
 *	Initialize the device - called from Slot manager.
 *	if first is set, then initially check for
 *	the device's existence before initialising it.
 *	Once initialised, the device table may be set up.
 */
int
edinit(struct pccard_dev *dp, int first)
{
	int s;
	struct ed_softc *sc = &ed_softc[dp->isahd.id_unit];
/*
 *	validate unit number.
 */
	if (first) {
		if (dp->isahd.id_unit >= NED)
			return(ENODEV);
/*
 *	Probe the device. If a value is returned, the
 *	device was found at the location.
 */
		
		sc->gone = 0;
		if (ed_probe_pccard(&dp->isahd,dp->misc)==0) {
			return(ENXIO);
		}
		if (ed_attach(&dp->isahd)==0) {
			return(ENXIO);
		}
	}
/*
 *	XXX TODO:
 *	If it was already inited before, the device structure
 *	should be already initialised. Here we should
 *	reset (and possibly restart) the hardware, but
 *	I am not sure of the best way to do this...
 */
	return(0);
}
/*
 *	edunload - unload the driver and clear the table.
 *	XXX TODO:
 *	This is called usually when the card is ejected, but
 *	can be caused by the modunload of a controller driver.
 *	The idea is reset the driver's view of the device
 *	and ensure that any driver entry points such as
 *	read and write do not hang.
 */
void
edunload(struct pccard_dev *dp)
{
	struct ed_softc *sc = &ed_softc[dp->isahd.id_unit];
	if (sc->kdc.kdc_state == DC_UNCONFIGURED) {
		printf("ed%d: already unloaded\n", dp->isahd.id_unit);
		return;
	}
	sc->kdc.kdc_state = DC_UNCONFIGURED;
	sc->arpcom.ac_if.if_flags &= ~IFF_RUNNING;
	if_down(&sc->arpcom.ac_if);
	sc->gone = 1;
	printf("ed%d: unload\n", dp->isahd.id_unit);
}

/*
 *	card_intr - Shared interrupt called from
 *	front end of PC-Card handler.
 */
static int
card_intr(struct pccard_dev *dp)
{
	edintr(dp->isahd.id_unit);
	return(1);
}
#endif /* NCRD > 0 */

struct isa_driver eddriver = {
	ed_probe,
	ed_attach,
	"ed",
	1		/* We are ultra sensitive */
};

/*
 * Interrupt conversion table for WD/SMC ASIC/83C584
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
 * Interrupt conversion table for 83C790
 */
static unsigned short ed_790_intr_mask[] = {
	0,
	IRQ9,
	IRQ3,
	IRQ5,
	IRQ7,
	IRQ10,
	IRQ11,
	IRQ15
};

#define	ETHER_MIN_LEN	60
#define ETHER_MAX_LEN	1514
#define	ETHER_ADDR_LEN	6
#define	ETHER_HDR_SIZE	14

static struct kern_devconf kdc_ed_template = {
	0, 0, 0,		/* filled in by dev_attach */
	"ed", 0, { MDDT_ISA, 0, "net" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parentdata */
	DC_UNCONFIGURED,	/* state */
	"",			/* description */
	DC_CLS_NETIF		/* class */
};

static inline void
ed_registerdev(struct isa_device *id, const char *descr)
{
	struct kern_devconf *kdc = &ed_softc[id->id_unit].kdc;
	*kdc = kdc_ed_template;
	kdc->kdc_unit = id->id_unit;
	kdc->kdc_parentdata = id;
	kdc->kdc_description = descr;
	dev_attach(kdc);
}

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
	int     nports;
#if NCRD > 0
/*
 *	If PC-Card probe required, then register driver with
 *	slot manager.
 */
		pccard_add_driver(&ed_info);
#endif /* NCRD > 0 */

#ifndef DEV_LKM
	ed_registerdev(isa_dev, "Ethernet adapter");
#endif /* not DEV_LKM */

	nports = ed_probe_WD80x3(isa_dev);
	if (nports)
		return (nports);

	nports = ed_probe_3Com(isa_dev);
	if (nports)
		return (nports);

	nports = ed_probe_Novell(isa_dev);
	if (nports)
		return (nports);

	return (0);
}

/*
 * Generic probe routine for testing for the existance of a DS8390.
 *	Must be called after the NIC has just been reset. This routine
 *	works by looking at certain register values that are guaranteed
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
	     (ED_CR_RD2 | ED_CR_TXP | ED_CR_STA | ED_CR_STP)) !=
	    (ED_CR_RD2 | ED_CR_STP))
		return (0);
	if ((inb(sc->nic_addr + ED_P0_ISR) & ED_ISR_RST) != ED_ISR_RST)
		return (0);

	return (1);
}

/*
 * Probe and vendor-specific initialization routine for SMC/WD80x3 boards
 */
int
ed_probe_WD80x3(isa_dev)
	struct isa_device *isa_dev;
{
	struct ed_softc *sc = &ed_softc[isa_dev->id_unit];
	int     i;
	u_int   memsize;
	u_char  iptr, isa16bit, sum;

	sc->asic_addr = isa_dev->id_iobase;
	sc->nic_addr = sc->asic_addr + ED_WD_NIC_OFFSET;
	sc->is790 = 0;

#ifdef TOSH_ETHER
	outb(sc->asic_addr + ED_WD_MSR, ED_WD_MSR_POW);
	DELAY(10000);
#endif

	/*
	 * Attempt to do a checksum over the station address PROM. If it
	 * fails, it's probably not a SMC/WD board. There is a problem with
	 * this, though: some clone WD boards don't pass the checksum test.
	 * Danpex boards for one.
	 */
	for (sum = 0, i = 0; i < 8; ++i)
		sum += inb(sc->asic_addr + ED_WD_PROM + i);

	if (sum != ED_WD_ROM_CHECKSUM_TOTAL) {

		/*
		 * Checksum is invalid. This often happens with cheap WD8003E
		 * clones.  In this case, the checksum byte (the eighth byte)
		 * seems to always be zero.
		 */
		if (inb(sc->asic_addr + ED_WD_CARD_ID) != ED_TYPE_WD8003E ||
		    inb(sc->asic_addr + ED_WD_PROM + 7) != 0)
			return (0);
	}
	/* reset card to force it into a known state. */
#ifdef TOSH_ETHER
	outb(sc->asic_addr + ED_WD_MSR, ED_WD_MSR_RST | ED_WD_MSR_POW);
#else
	outb(sc->asic_addr + ED_WD_MSR, ED_WD_MSR_RST);
#endif
	DELAY(100);
	outb(sc->asic_addr + ED_WD_MSR, inb(sc->asic_addr + ED_WD_MSR) & ~ED_WD_MSR_RST);
	/* wait in the case this card is reading it's EEROM */
	DELAY(5000);

	sc->vendor = ED_VENDOR_WD_SMC;
	sc->type = inb(sc->asic_addr + ED_WD_CARD_ID);

	/*
	 * Set initial values for width/size.
	 */
	memsize = 8192;
	isa16bit = 0;
	switch (sc->type) {
	case ED_TYPE_WD8003S:
		sc->type_str = "WD8003S";
		sc->kdc.kdc_description = "Ethernet adapter: WD 8003S";
		break;
	case ED_TYPE_WD8003E:
		sc->type_str = "WD8003E";
		sc->kdc.kdc_description = "Ethernet adapter: WD 8003E";
		break;
	case ED_TYPE_WD8003EB:
		sc->type_str = "WD8003EB";
		sc->kdc.kdc_description = "Ethernet adapter: WD 8003EB";
		break;
	case ED_TYPE_WD8003W:
		sc->type_str = "WD8003W";
		sc->kdc.kdc_description = "Ethernet adapter: WD 8003W";
		break;
	case ED_TYPE_WD8013EBT:
		sc->type_str = "WD8013EBT";
		sc->kdc.kdc_description = "Ethernet adapter: WD 8013EBT";
		memsize = 16384;
		isa16bit = 1;
		break;
	case ED_TYPE_WD8013W:
		sc->type_str = "WD8013W";
		sc->kdc.kdc_description = "Ethernet adapter: WD 8013W";
		memsize = 16384;
		isa16bit = 1;
		break;
	case ED_TYPE_WD8013EP:	/* also WD8003EP */
		if (inb(sc->asic_addr + ED_WD_ICR)
		    & ED_WD_ICR_16BIT) {
			isa16bit = 1;
			memsize = 16384;
			sc->type_str = "WD8013EP";
			sc->kdc.kdc_description =
				"Ethernet adapter: WD 8013EP";
		} else {
			sc->type_str = "WD8003EP";
			sc->kdc.kdc_description =
				"Ethernet adapter: WD 8003EP";
		}
		break;
	case ED_TYPE_WD8013WC:
		sc->type_str = "WD8013WC";
		sc->kdc.kdc_description = "Ethernet adapter: WD 8013WC";
		memsize = 16384;
		isa16bit = 1;
		break;
	case ED_TYPE_WD8013EBP:
		sc->type_str = "WD8013EBP";
		sc->kdc.kdc_description = "Ethernet adapter: WD 8013EBP";
		memsize = 16384;
		isa16bit = 1;
		break;
	case ED_TYPE_WD8013EPC:
		sc->type_str = "WD8013EPC";
		sc->kdc.kdc_description = "Ethernet adapter: WD 8013EPC";
		memsize = 16384;
		isa16bit = 1;
		break;
	case ED_TYPE_SMC8216C: /* 8216 has 16K shared mem -- 8416 has 8K */
		(unsigned int) *(isa_dev->id_maddr+8192) = (unsigned int)0;
		if ((unsigned int) *(isa_dev->id_maddr+8192)) {
			sc->type_str = "SMC8416C/SMC8416BT";
			sc->kdc.kdc_description =
				"Ethernet adapter: SMC 8416C or 8416BT";
			memsize = 8192;
		} else {
			sc->type_str = "SMC8216/SMC8216C";
			sc->kdc.kdc_description =
				"Ethernet adapter: SMC 8216 or 8216C";
			memsize = 16384;
		}
		isa16bit = 1;
		sc->is790 = 1;
		break;
	case ED_TYPE_SMC8216T:
		sc->type_str = "SMC8216T";
		sc->kdc.kdc_description =
			"Ethernet adapter: SMC 8216T";

		outb(sc->asic_addr + ED_WD790_HWR,
		    inb(sc->asic_addr + ED_WD790_HWR) | ED_WD790_HWR_SWH);
		switch (inb(sc->asic_addr + ED_WD790_RAR) & ED_WD790_RAR_SZ64) {
		case ED_WD790_RAR_SZ64:
			memsize = 65536;
			break;
		case ED_WD790_RAR_SZ32:
			memsize = 32768;
			break;
		case ED_WD790_RAR_SZ16:
			memsize = 16384;
			break;
		case ED_WD790_RAR_SZ8:
			sc->type_str = "SMC8416T";
			sc->kdc.kdc_description =
				"Ethernet adapter: SMC 8416T";
			memsize = 8192;
			break;
		}
		outb(sc->asic_addr + ED_WD790_HWR,
		    inb(sc->asic_addr + ED_WD790_HWR) & ~ED_WD790_HWR_SWH);

		isa16bit = 1;
		sc->is790 = 1;
		break;
#ifdef TOSH_ETHER
	case ED_TYPE_TOSHIBA1:
		sc->type_str = "Toshiba1";
		sc->kdc.kdc_description = "Ethernet adapter: Toshiba1";
		memsize = 32768;
		isa16bit = 1;
		break;
	case ED_TYPE_TOSHIBA4:
		sc->type_str = "Toshiba4";
		sc->kdc.kdc_description = "Ethernet adapter: Toshiba4";
		memsize = 32768;
		isa16bit = 1;
		break;
#endif
	default:
		sc->type_str = "";
		break;
	}

	/*
	 * Make some adjustments to initial values depending on what is found
	 * in the ICR.
	 */
	if (isa16bit && (sc->type != ED_TYPE_WD8013EBT)
#ifdef TOSH_ETHER
	  && (sc->type != ED_TYPE_TOSHIBA1) && (sc->type != ED_TYPE_TOSHIBA4)
#endif
	    && ((inb(sc->asic_addr + ED_WD_ICR) & ED_WD_ICR_16BIT) == 0)) {
		isa16bit = 0;
		memsize = 8192;
	}

#if ED_DEBUG
	printf("type = %x type_str=%s isa16bit=%d memsize=%d id_msize=%d\n",
	       sc->type, sc->type_str, isa16bit, memsize, isa_dev->id_msize);
	for (i = 0; i < 8; i++)
		printf("%x -> %x\n", i, inb(sc->asic_addr + i));
#endif

	/*
	 * Allow the user to override the autoconfiguration
	 */
	if (isa_dev->id_msize)
		memsize = isa_dev->id_msize;

	/*
	 * (note that if the user specifies both of the following flags that
	 * '8bit' mode intentionally has precedence)
	 */
	if (isa_dev->id_flags & ED_FLAGS_FORCE_16BIT_MODE)
		isa16bit = 1;
	if (isa_dev->id_flags & ED_FLAGS_FORCE_8BIT_MODE)
		isa16bit = 0;

	/*
	 * If possible, get the assigned interrupt number from the card and
	 * use it.
	 */
	if ((sc->type & ED_WD_SOFTCONFIG) && (!sc->is790)) {

		/*
		 * Assemble together the encoded interrupt number.
		 */
		iptr = (inb(isa_dev->id_iobase + ED_WD_ICR) & ED_WD_ICR_IR2) |
		    ((inb(isa_dev->id_iobase + ED_WD_IRR) &
		      (ED_WD_IRR_IR0 | ED_WD_IRR_IR1)) >> 5);

		/*
		 * If no interrupt specified (or "?"), use what the board tells us.
		 */
		if (isa_dev->id_irq <= 0)
			isa_dev->id_irq = ed_intr_mask[iptr];

		/*
		 * Enable the interrupt.
		 */
		outb(isa_dev->id_iobase + ED_WD_IRR,
		     inb(isa_dev->id_iobase + ED_WD_IRR) | ED_WD_IRR_IEN);
	}
	if (sc->is790) {
		outb(isa_dev->id_iobase + ED_WD790_HWR,
		  inb(isa_dev->id_iobase + ED_WD790_HWR) | ED_WD790_HWR_SWH);
		iptr = (((inb(isa_dev->id_iobase + ED_WD790_GCR) & ED_WD790_GCR_IR2) >> 4) |
			(inb(isa_dev->id_iobase + ED_WD790_GCR) &
			 (ED_WD790_GCR_IR1 | ED_WD790_GCR_IR0)) >> 2);
		outb(isa_dev->id_iobase + ED_WD790_HWR,
		 inb(isa_dev->id_iobase + ED_WD790_HWR) & ~ED_WD790_HWR_SWH);

		/*
		 * If no interrupt specified (or "?"), use what the board tells us.
		 */
		if (isa_dev->id_irq <= 0)
			isa_dev->id_irq = ed_790_intr_mask[iptr];

		/*
		 * Enable interrupts.
		 */
		outb(isa_dev->id_iobase + ED_WD790_ICR,
		  inb(isa_dev->id_iobase + ED_WD790_ICR) | ED_WD790_ICR_EIL);
	}
	if (isa_dev->id_irq <= 0) {
		printf("ed%d: %s cards don't support auto-detected/assigned interrupts.\n",
		    isa_dev->id_unit, sc->type_str);
		return (0);
	}
	sc->isa16bit = isa16bit;
	sc->mem_shared = 1;
	isa_dev->id_msize = memsize;
	sc->mem_start = (caddr_t) isa_dev->id_maddr;

	/*
	 * allocate one xmit buffer if < 16k, two buffers otherwise
	 */
	if ((memsize < 16384) ||
            (isa_dev->id_flags & ED_FLAGS_NO_MULTI_BUFFERING)) {
		sc->txb_cnt = 1;
	} else {
		sc->txb_cnt = 2;
	}
	sc->tx_page_start = ED_WD_PAGE_OFFSET;
	sc->rec_page_start = ED_WD_PAGE_OFFSET + ED_TXBUF_SIZE * sc->txb_cnt;
	sc->rec_page_stop = ED_WD_PAGE_OFFSET + memsize / ED_PAGE_SIZE;
	sc->mem_ring = sc->mem_start + (ED_PAGE_SIZE * sc->rec_page_start);
	sc->mem_size = memsize;
	sc->mem_end = sc->mem_start + memsize;

	/*
	 * Get station address from on-board ROM
	 */
	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		sc->arpcom.ac_enaddr[i] = inb(sc->asic_addr + ED_WD_PROM + i);

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
	} else {
		if (((sc->type & ED_WD_SOFTCONFIG) ||
#ifdef TOSH_ETHER
		    (sc->type == ED_TYPE_TOSHIBA1) || (sc->type == ED_TYPE_TOSHIBA4) ||
#endif
		    (sc->type == ED_TYPE_WD8013EBT)) && (!sc->is790)) {
			outb(sc->asic_addr + ED_WD_LAAR, (sc->wd_laar_proto =
			    ((kvtop(sc->mem_start) >> 19) & ED_WD_LAAR_ADDRHI)));
		}
	}

	/*
	 * Set address and enable interface shared memory.
	 */
	if (!sc->is790) {
#ifdef TOSH_ETHER
		outb(sc->asic_addr + ED_WD_MSR + 1, ((kvtop(sc->mem_start) >> 8) & 0xe0) | 4);
		outb(sc->asic_addr + ED_WD_MSR + 2, ((kvtop(sc->mem_start) >> 16) & 0x0f));
		outb(sc->asic_addr + ED_WD_MSR, ED_WD_MSR_MENB | ED_WD_MSR_POW);

#else
		outb(sc->asic_addr + ED_WD_MSR, ((kvtop(sc->mem_start) >> 13) &
		    ED_WD_MSR_ADDR) | ED_WD_MSR_MENB);
#endif
		sc->cr_proto = ED_CR_RD2;
	} else {
		outb(sc->asic_addr + ED_WD_MSR, ED_WD_MSR_MENB);
		outb(sc->asic_addr + ED_WD790_HWR, (inb(sc->asic_addr + ED_WD790_HWR) | ED_WD790_HWR_SWH));
		outb(sc->asic_addr + ED_WD790_RAR, ((kvtop(sc->mem_start) >> 13) & 0x0f) |
		     ((kvtop(sc->mem_start) >> 11) & 0x40) |
		     (inb(sc->asic_addr + ED_WD790_RAR) & 0xb0));
		outb(sc->asic_addr + ED_WD790_HWR, (inb(sc->asic_addr + ED_WD790_HWR) & ~ED_WD790_HWR_SWH));
		sc->cr_proto = 0;
	}

#if 0
	printf("starting memory performance test at 0x%x, size %d...\n",
		sc->mem_start, memsize*16384);
	for (i = 0; i < 16384; i++)
		bzero(sc->mem_start, memsize);
	printf("***DONE***\n");
#endif

	/*
	 * Now zero memory and verify that it is clear
	 */
	bzero(sc->mem_start, memsize);

	for (i = 0; i < memsize; ++i) {
		if (sc->mem_start[i]) {
			printf("ed%d: failed to clear shared memory at %lx - check configuration\n",
			    isa_dev->id_unit, kvtop(sc->mem_start + i));

			/*
			 * Disable 16 bit access to shared memory
			 */
			if (isa16bit) {
				if (sc->is790) {
					outb(sc->asic_addr + ED_WD_MSR, 0x00);
				}
				outb(sc->asic_addr + ED_WD_LAAR, (sc->wd_laar_proto &=
				    ~ED_WD_LAAR_M16EN));
			}
			return (0);
		}
	}

	/*
	 * Disable 16bit access to shared memory - we leave it
	 * disabled so that 1) machines reboot properly when the board
	 * is set 16 bit mode and there are conflicting 8bit
	 * devices/ROMS in the same 128k address space as this boards
	 * shared memory. and 2) so that other 8 bit devices with
	 * shared memory can be used in this 128k region, too.
	 */
	if (isa16bit) {
		if (sc->is790) {
			outb(sc->asic_addr + ED_WD_MSR, 0x00);
		}
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
	int     i;
	u_int   memsize;
	u_char  isa16bit;

	sc->asic_addr = isa_dev->id_iobase + ED_3COM_ASIC_OFFSET;
	sc->nic_addr = isa_dev->id_iobase + ED_3COM_NIC_OFFSET;

	/*
	 * Verify that the kernel configured I/O address matches the board
	 * configured address
	 */
	switch (inb(sc->asic_addr + ED_3COM_BCFR)) {
	case ED_3COM_BCFR_300:
		if (isa_dev->id_iobase != 0x300)
			return (0);
		break;
	case ED_3COM_BCFR_310:
		if (isa_dev->id_iobase != 0x310)
			return (0);
		break;
	case ED_3COM_BCFR_330:
		if (isa_dev->id_iobase != 0x330)
			return (0);
		break;
	case ED_3COM_BCFR_350:
		if (isa_dev->id_iobase != 0x350)
			return (0);
		break;
	case ED_3COM_BCFR_250:
		if (isa_dev->id_iobase != 0x250)
			return (0);
		break;
	case ED_3COM_BCFR_280:
		if (isa_dev->id_iobase != 0x280)
			return (0);
		break;
	case ED_3COM_BCFR_2A0:
		if (isa_dev->id_iobase != 0x2a0)
			return (0);
		break;
	case ED_3COM_BCFR_2E0:
		if (isa_dev->id_iobase != 0x2e0)
			return (0);
		break;
	default:
		return (0);
	}

	/*
	 * Verify that the kernel shared memory address matches the board
	 * configured address.
	 */
	switch (inb(sc->asic_addr + ED_3COM_PCFR)) {
	case ED_3COM_PCFR_DC000:
		if (kvtop(isa_dev->id_maddr) != 0xdc000)
			return (0);
		break;
	case ED_3COM_PCFR_D8000:
		if (kvtop(isa_dev->id_maddr) != 0xd8000)
			return (0);
		break;
	case ED_3COM_PCFR_CC000:
		if (kvtop(isa_dev->id_maddr) != 0xcc000)
			return (0);
		break;
	case ED_3COM_PCFR_C8000:
		if (kvtop(isa_dev->id_maddr) != 0xc8000)
			return (0);
		break;
	default:
		return (0);
	}


	/*
	 * Reset NIC and ASIC. Enable on-board transceiver throughout reset
	 * sequence because it'll lock up if the cable isn't connected if we
	 * don't.
	 */
	outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_RST | ED_3COM_CR_XSEL);

	/*
	 * Wait for a while, then un-reset it
	 */
	DELAY(50);

	/*
	 * The 3Com ASIC defaults to rather strange settings for the CR after
	 * a reset - it's important to set it again after the following outb
	 * (this is done when we map the PROM below).
	 */
	outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_XSEL);

	/*
	 * Wait a bit for the NIC to recover from the reset
	 */
	DELAY(5000);

	sc->vendor = ED_VENDOR_3COM;
	sc->type_str = "3c503";
	sc->kdc.kdc_description = "Ethernet adapter: 3c503";
	sc->mem_shared = 1;
	sc->cr_proto = ED_CR_RD2;

	/*
	 * Hmmm...a 16bit 3Com board has 16k of memory, but only an 8k window
	 * to it.
	 */
	memsize = 8192;

	/*
	 * Get station address from on-board ROM
	 */

	/*
	 * First, map ethernet address PROM over the top of where the NIC
	 * registers normally appear.
	 */
	outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_EALO | ED_3COM_CR_XSEL);

	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		sc->arpcom.ac_enaddr[i] = inb(sc->nic_addr + i);

	/*
	 * Unmap PROM - select NIC registers. The proper setting of the
	 * tranceiver is set in ed_init so that the attach code is given a
	 * chance to set the default based on a compile-time config option
	 */
	outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_XSEL);

	/*
	 * Determine if this is an 8bit or 16bit board
	 */

	/*
	 * select page 0 registers
	 */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2 | ED_CR_STP);

	/*
	 * Attempt to clear WTS bit. If it doesn't clear, then this is a 16bit
	 * board.
	 */
	outb(sc->nic_addr + ED_P0_DCR, 0);

	/*
	 * select page 2 registers
	 */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_PAGE_2 | ED_CR_RD2 | ED_CR_STP);

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
	outb(sc->nic_addr + ED_P2_CR, ED_CR_RD2 | ED_CR_STP);

	sc->mem_start = (caddr_t) isa_dev->id_maddr;
	sc->mem_size = memsize;
	sc->mem_end = sc->mem_start + memsize;

	/*
	 * We have an entire 8k window to put the transmit buffers on the
	 * 16bit boards. But since the 16bit 3c503's shared memory is only
	 * fast enough to overlap the loading of one full-size packet, trying
	 * to load more than 2 buffers can actually leave the transmitter idle
	 * during the load. So 2 seems the best value. (Although a mix of
	 * variable-sized packets might change this assumption. Nonetheless,
	 * we optimize for linear transfers of same-size packets.)
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
	 * Initialize GA page start/stop registers. Probably only needed if
	 * doing DMA, but what the hell.
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
		return (0);
	}

	/*
	 * Initialize GA configuration register. Set bank and enable shared
	 * mem.
	 */
	outb(sc->asic_addr + ED_3COM_GACFR, ED_3COM_GACFR_RSEL |
	     ED_3COM_GACFR_MBS0);

	/*
	 * Initialize "Vector Pointer" registers. These gawd-awful things are
	 * compared to 20 bits of the address on ISA, and if they match, the
	 * shared memory is disabled. We set them to 0xffff0...allegedly the
	 * reset vector.
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
			printf("ed%d: failed to clear shared memory at %lx - check configuration\n",
			       isa_dev->id_unit, kvtop(sc->mem_start + i));
			return (0);
		}
	isa_dev->id_msize = memsize;
	return (ED_3COM_IO_PORTS);
}

/*
 * Probe and vendor-specific initialization routine for NE1000/2000 boards
 */
int
ed_probe_Novell(isa_dev)
	struct isa_device *isa_dev;
{
	struct ed_softc *sc = &ed_softc[isa_dev->id_unit];
	u_int   memsize, n;
	u_char  romdata[16], tmp;
	static char test_pattern[32] = "THIS is A memory TEST pattern";
	char    test_buffer[32];

	sc->asic_addr = isa_dev->id_iobase + ED_NOVELL_ASIC_OFFSET;
	sc->nic_addr = isa_dev->id_iobase + ED_NOVELL_NIC_OFFSET;

	/* XXX - do Novell-specific probe here */

	/* Reset the board */
#ifdef GWETHER
	outb(sc->asic_addr + ED_NOVELL_RESET, 0);
	DELAY(200);
#endif	/* GWETHER */
	tmp = inb(sc->asic_addr + ED_NOVELL_RESET);

	/*
	 * I don't know if this is necessary; probably cruft leftover from
	 * Clarkson packet driver code. Doesn't do a thing on the boards I've
	 * tested. -DG [note that a outb(0x84, 0) seems to work here, and is
	 * non-invasive...but some boards don't seem to reset and I don't have
	 * complete documentation on what the 'right' thing to do is...so we
	 * do the invasive thing for now. Yuck.]
	 */
	outb(sc->asic_addr + ED_NOVELL_RESET, tmp);
	DELAY(5000);

	/*
	 * This is needed because some NE clones apparently don't reset the
	 * NIC properly (or the NIC chip doesn't reset fully on power-up) XXX
	 * - this makes the probe invasive! ...Done against my better
	 * judgement. -DLG
	 */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2 | ED_CR_STP);

	DELAY(5000);

	/* Make sure that we really have an 8390 based board */
	if (!ed_probe_generic8390(sc))
		return (0);

	sc->vendor = ED_VENDOR_NOVELL;
	sc->mem_shared = 0;
	sc->cr_proto = ED_CR_RD2;
	isa_dev->id_maddr = 0;

	/*
	 * Test the ability to read and write to the NIC memory. This has the
	 * side affect of determining if this is an NE1000 or an NE2000.
	 */

	/*
	 * This prevents packets from being stored in the NIC memory when the
	 * readmem routine turns on the start bit in the CR.
	 */
	outb(sc->nic_addr + ED_P0_RCR, ED_RCR_MON);

	/* Temporarily initialize DCR for byte operations */
	outb(sc->nic_addr + ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);

	outb(sc->nic_addr + ED_P0_PSTART, 8192 / ED_PAGE_SIZE);
	outb(sc->nic_addr + ED_P0_PSTOP, 16384 / ED_PAGE_SIZE);

	sc->isa16bit = 0;

	/*
	 * Write a test pattern in byte mode. If this fails, then there
	 * probably isn't any memory at 8k - which likely means that the board
	 * is an NE2000.
	 */
	ed_pio_writemem(sc, test_pattern, 8192, sizeof(test_pattern));
	ed_pio_readmem(sc, 8192, test_buffer, sizeof(test_pattern));

	if (bcmp(test_pattern, test_buffer, sizeof(test_pattern))) {
		/* not an NE1000 - try NE2000 */

		outb(sc->nic_addr + ED_P0_DCR, ED_DCR_WTS | ED_DCR_FT1 | ED_DCR_LS);
		outb(sc->nic_addr + ED_P0_PSTART, 16384 / ED_PAGE_SIZE);
		outb(sc->nic_addr + ED_P0_PSTOP, 32768 / ED_PAGE_SIZE);

		sc->isa16bit = 1;

		/*
		 * Write a test pattern in word mode. If this also fails, then
		 * we don't know what this board is.
		 */
		ed_pio_writemem(sc, test_pattern, 16384, sizeof(test_pattern));
		ed_pio_readmem(sc, 16384, test_buffer, sizeof(test_pattern));

		if (bcmp(test_pattern, test_buffer, sizeof(test_pattern)))
			return (0);	/* not an NE2000 either */

		sc->type = ED_TYPE_NE2000;
		sc->type_str = "NE2000";
		sc->kdc.kdc_description = "Ethernet adapter: NE2000";
	} else {
		sc->type = ED_TYPE_NE1000;
		sc->type_str = "NE1000";
		sc->kdc.kdc_description = "Ethernet adapter: NE1000";
	}

	/* 8k of memory plus an additional 8k if 16bit */
	memsize = 8192 + sc->isa16bit * 8192;

#if 0	/* probably not useful - NE boards only come two ways */
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

#ifdef GWETHER
	{
		int     x, i, mstart = 0, msize = 0;
		char    pbuf0[ED_PAGE_SIZE], pbuf[ED_PAGE_SIZE], tbuf[ED_PAGE_SIZE];

		for (i = 0; i < ED_PAGE_SIZE; i++)
			pbuf0[i] = 0;

		/* Clear all the memory. */
		for (x = 1; x < 256; x++)
			ed_pio_writemem(sc, pbuf0, x * 256, ED_PAGE_SIZE);

		/* Search for the start of RAM. */
		for (x = 1; x < 256; x++) {
			ed_pio_readmem(sc, x * 256, tbuf, ED_PAGE_SIZE);
			if (bcmp(pbuf0, tbuf, ED_PAGE_SIZE) == 0) {
				for (i = 0; i < ED_PAGE_SIZE; i++)
					pbuf[i] = 255 - x;
				ed_pio_writemem(sc, pbuf, x * 256, ED_PAGE_SIZE);
				ed_pio_readmem(sc, x * 256, tbuf, ED_PAGE_SIZE);
				if (bcmp(pbuf, tbuf, ED_PAGE_SIZE) == 0) {
					mstart = x * ED_PAGE_SIZE;
					msize = ED_PAGE_SIZE;
					break;
				}
			}
		}

		if (mstart == 0) {
			printf("ed%d: Cannot find start of RAM.\n", isa_dev->id_unit);
			return 0;
		}
		/* Search for the start of RAM. */
		for (x = (mstart / ED_PAGE_SIZE) + 1; x < 256; x++) {
			ed_pio_readmem(sc, x * 256, tbuf, ED_PAGE_SIZE);
			if (bcmp(pbuf0, tbuf, ED_PAGE_SIZE) == 0) {
				for (i = 0; i < ED_PAGE_SIZE; i++)
					pbuf[i] = 255 - x;
				ed_pio_writemem(sc, pbuf, x * 256, ED_PAGE_SIZE);
				ed_pio_readmem(sc, x * 256, tbuf, ED_PAGE_SIZE);
				if (bcmp(pbuf, tbuf, ED_PAGE_SIZE) == 0)
					msize += ED_PAGE_SIZE;
				else {
					break;
				}
			} else {
				break;
			}
		}

		if (msize == 0) {
			printf("ed%d: Cannot find any RAM, start : %d, x = %d.\n", isa_dev->id_unit, mstart, x);
			return 0;
		}
		printf("ed%d: RAM start at %d, size : %d.\n", isa_dev->id_unit, mstart, msize);

		sc->mem_size = msize;
		sc->mem_start = (char *) mstart;
		sc->mem_end = (char *) (msize + mstart);
		sc->tx_page_start = mstart / ED_PAGE_SIZE;
	}
#endif	/* GWETHER */

	/*
	 * Use one xmit buffer if < 16k, two buffers otherwise (if not told
	 * otherwise).
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
		sc->arpcom.ac_enaddr[n] = romdata[n * (sc->isa16bit + 1)];

#ifdef GWETHER
	if (sc->arpcom.ac_enaddr[2] == 0x86) {
		sc->type_str = "Gateway AT";
		sc->kdc.kdc_description = "Ethernet adapter: Gateway AT";
	}
#endif	/* GWETHER */

	/* clear any pending interrupts that might have occurred above */
	outb(sc->nic_addr + ED_P0_ISR, 0xff);

	return (ED_NOVELL_IO_PORTS);
}


/*
 * Probe and vendor-specific initialization routine for PCCARDs
 */
int
ed_probe_pccard(isa_dev, ether)
	struct isa_device *isa_dev;
	u_char *ether;
{
	struct ed_softc *sc = &ed_softc[isa_dev->id_unit];
	int     i;
	u_int   memsize;
	u_char  isa16bit;

	sc->nic_addr = isa_dev->id_iobase; 
	sc->gone = 0;
	sc->is790 = 0;
	sc->cr_proto = ED_CR_RD2;
	sc->vendor = ED_VENDOR_PCCARD;
	sc->type = 0;
	sc->type_str = "PCCARD";
	sc->kdc.kdc_description = "PCCARD Ethernet";
	sc->mem_size = isa_dev->id_msize = memsize = 16384;
	sc->isa16bit = isa16bit = 1;

	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		sc->arpcom.ac_enaddr[i] = ether[i];

#if ED_DEBUG
	printf("type = %x type_str=%s isa16bit=%d memsize=%d id_msize=%d\n",
	       sc->type, sc->type_str, isa16bit, memsize, isa_dev->id_msize);
#endif

	i = inb(sc->nic_addr + ED_PC_RESET);
	DELAY(100000);
	outb(sc->nic_addr + ED_PC_RESET,i);
	DELAY(100000);
	i = inb(sc->nic_addr + ED_PC_MISC);
	if (!i) {
		int j;
		printf("ed_probe_pccard: possible failure\n");
		for (j=0;j<20 && !i;j++) {
			printf(".");
			DELAY(100000);
			i = inb(sc->nic_addr + ED_PC_MISC);
		}
		if (!i) {
			printf("dead :-(\n");
			return 0;
		}
		printf("\n");
	}
	/*
	 * Set initial values for width/size.
	 */

	/* Make sure that we really have an 8390 based board */
	if (!ed_probe_generic8390(sc)) {
		printf("ed_probe_generic8390 failed\n");
		return (0);
	}
	sc->txb_cnt = 2;
	sc->tx_page_start = ED_PC_PAGE_OFFSET;
	sc->rec_page_start = sc->tx_page_start + ED_TXBUF_SIZE * sc->txb_cnt;
	sc->rec_page_stop = sc->tx_page_start + memsize / ED_PAGE_SIZE;

	sc->mem_shared = 1;
	sc->mem_start = (caddr_t) isa_dev->id_maddr;
	sc->mem_size = memsize;
	sc->mem_end = sc->mem_start + memsize;

	sc->mem_ring = sc->mem_start + 
		sc->txb_cnt * ED_PAGE_SIZE * ED_TXBUF_SIZE;

	/*
	 * Now zero memory and verify that it is clear
	 */
	bzero(sc->mem_start, memsize);

	for (i = 0; i < memsize; ++i) {
		if (sc->mem_start[i]) {
			printf("ed%d: failed to clear shared memory at %lx - check configuration\n",
			    isa_dev->id_unit, kvtop(sc->mem_start + i));

			return (0);
		}
		sc->mem_start[i] = (i - 5) & 0xff;
	}
	for (i = 0; i < memsize; ++i) {
		if ((sc->mem_start[i] & 0xff) != ((i - 5) & 0xff)) {
			printf("ed%d: shared memory failed at %lx (%x != %x) - check configuration\n",
			    isa_dev->id_unit, kvtop(sc->mem_start + i),
			    sc->mem_start[i], (i-5) & 0xff);
			return (0);

		}
	}

	i = inb(sc->nic_addr + ED_PC_MISC);
	if (!i) {
		printf("ed_probe_pccard: possible failure(2)\n");
	}

	/* clear any pending interupts that we may have caused */
	outb(sc->nic_addr + ED_P0_ISR, 0xff);

	return (ED_PC_IO_PORTS);
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

	/*
	 * Set interface to stopped condition (reset)
	 */
	ed_stop(isa_dev->id_unit);

	if (!ifp->if_name) {
		/*
		 * Initialize ifnet structure
		 */
		ifp->if_unit = isa_dev->id_unit;
		ifp->if_name = "ed";
		ifp->if_init = ed_init;
		ifp->if_output = ether_output;
		ifp->if_start = ed_start;
		ifp->if_ioctl = ed_ioctl;
		ifp->if_reset = ed_reset;
		ifp->if_watchdog = ed_watchdog;
		ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

		/*
		 * Set default state for ALTPHYS flag (used to disable the 
		 * tranceiver for AUI operation), based on compile-time 
		 * config option.
		 */
		if (isa_dev->id_flags & ED_FLAGS_DISABLE_TRANCEIVER)
			ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX | 
			    IFF_MULTICAST | IFF_ALTPHYS);
		else
			ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX |
			    IFF_MULTICAST);

		/*
		 * Attach the interface
		 */
		if_attach(ifp);
	}
	/* device attach does transition from UNCONFIGURED to IDLE state */
	sc->kdc.kdc_state = DC_IDLE;

	/*
	 * Print additional info when attached
	 */
	printf("ed%d: address %s, ", isa_dev->id_unit,
	       ether_sprintf(sc->arpcom.ac_enaddr));

	if (sc->type_str && (*sc->type_str != 0))
		printf("type %s ", sc->type_str);
	else
		printf("type unknown (0x%x) ", sc->type);

	printf("%s ", sc->isa16bit ? "(16 bit)" : "(8 bit)");

	printf("%s\n", ((sc->vendor == ED_VENDOR_3COM) &&
	       (ifp->if_flags & IFF_ALTPHYS)) ? " tranceiver disabled" : "");

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
ed_reset(unit)
	int     unit;
{
	int     s;

	if (ed_softc[unit].gone)
		return;
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
	int     unit;
{
	struct ed_softc *sc = &ed_softc[unit];
	int     n = 5000;

	if (sc->gone)
		return;
	/*
	 * Stop everything on the interface, and select page 0 registers.
	 */
	outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_STP);

	/*
	 * Wait for interface to enter stopped state, but limit # of checks to
	 * 'n' (about 5ms). It shouldn't even take 5us on modern DS8390's, but
	 * just in case it's an old one.
	 */
	while (((inb(sc->nic_addr + ED_P0_ISR) & ED_ISR_RST) == 0) && --n);
}

/*
 * Device timeout/watchdog routine. Entered if the device neglects to
 *	generate an interrupt after a transmit has been started on it.
 */
void
ed_watchdog(unit)
	int     unit;
{
	struct ed_softc *sc = &ed_softc[unit];

	if (sc->gone)
		return;
	log(LOG_ERR, "ed%d: device timeout\n", unit);
	++sc->arpcom.ac_if.if_oerrors;

	ed_reset(unit);
}

/*
 * Initialize device.
 */
void
ed_init(unit)
	int     unit;
{
	struct ed_softc *sc = &ed_softc[unit];
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int     i, s;

	if (sc->gone)
		return;

	/* address not known */
	if (ifp->if_addrlist == (struct ifaddr *) 0)
		return;

	/*
	 * Initialize the NIC in the exact order outlined in the NS manual.
	 * This init procedure is "mandatory"...don't change what or when
	 * things happen.
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
	outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_STP);

	if (sc->isa16bit) {

		/*
		 * Set FIFO threshold to 8, No auto-init Remote DMA, byte
		 * order=80x86, word-wide DMA xfers,
		 */
		outb(sc->nic_addr + ED_P0_DCR, ED_DCR_FT1 | ED_DCR_WTS | ED_DCR_LS);
	} else {

		/*
		 * Same as above, but byte-wide DMA xfers
		 */
		outb(sc->nic_addr + ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);
	}

	/*
	 * Clear Remote Byte Count Registers
	 */
	outb(sc->nic_addr + ED_P0_RBCR0, 0);
	outb(sc->nic_addr + ED_P0_RBCR1, 0);

	/*
	 * For the moment, don't store incoming packets in memory.
	 */
	outb(sc->nic_addr + ED_P0_RCR, ED_RCR_MON);

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
	 * corresponding flag.
	 */
	outb(sc->nic_addr + ED_P0_ISR, 0xff);

	/*
	 * Enable the following interrupts: receive/transmit complete,
	 * receive/transmit error, and Receiver OverWrite.
	 *
	 * Counter overflow and Remote DMA complete are *not* enabled.
	 */
	outb(sc->nic_addr + ED_P0_IMR,
	ED_IMR_PRXE | ED_IMR_PTXE | ED_IMR_RXEE | ED_IMR_TXEE | ED_IMR_OVWE);

	/*
	 * Program Command Register for page 1
	 */
	outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STP);

	/*
	 * Copy out our station address
	 */
	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		outb(sc->nic_addr + ED_P1_PAR0 + i, sc->arpcom.ac_enaddr[i]);

	/*
	 * Set Current Page pointer to next_packet (initialized above)
	 */
	outb(sc->nic_addr + ED_P1_CURR, sc->next_packet);

	/*
	 * Program Receiver Configuration Register and multicast filter. CR is
	 * set to page 0 on return.
	 */
	ed_setrcr(ifp, sc);

	/*
	 * Take interface out of loopback
	 */
	outb(sc->nic_addr + ED_P0_TCR, 0);

	/*
	 * If this is a 3Com board, the tranceiver must be software enabled
	 * (there is no settable hardware default).
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
static inline void
ed_xmit(ifp)
	struct ifnet *ifp;
{
	struct ed_softc *sc = &ed_softc[ifp->if_unit];
	unsigned short len;

	if (sc->gone)
		return;
	len = sc->txb_len[sc->txb_next_tx];

	/*
	 * Set NIC for page 0 register access
	 */
	outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_STA);

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
	outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_TXP | ED_CR_STA);
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
	int     len;

	if (sc->gone) {
		printf("ed_start(%p) GONE\n",ifp);
		return;
	}
outloop:

	/*
	 * First, see if there are buffered packets and an idle transmitter -
	 * should never happen at this point.
	 */
	if (sc->txb_inuse && (sc->xmit_busy == 0)) {
		printf("ed: packets buffered, but transmitter idle\n");
		ed_xmit(ifp);
	}

	/*
	 * See if there is room to put another packet in the buffer.
	 */
	if (sc->txb_inuse == sc->txb_cnt) {

		/*
		 * No room. Indicate this to the outside world and exit.
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
		 * transmitter may be active, but if we haven't filled all the
		 * buffers with data then we still want to accept more.
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
				 * For 16bit 3Com boards (which have 16k of
				 * memory), we have the xmit buffers in a
				 * different page of memory ('page 0') - so
				 * change pages.
				 */
			case ED_VENDOR_3COM:
				outb(sc->asic_addr + ED_3COM_GACFR,
				     ED_3COM_GACFR_RSEL);
				break;

				/*
				 * Enable 16bit access to shared memory on
				 * WD/SMC boards.
				 */
			case ED_VENDOR_WD_SMC:{
					outb(sc->asic_addr + ED_WD_LAAR,
					     (sc->wd_laar_proto | ED_WD_LAAR_M16EN));
					if (sc->is790) {
						outb(sc->asic_addr + ED_WD_MSR, ED_WD_MSR_MENB);
					}
					break;
				}
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
			case ED_VENDOR_WD_SMC:{
					if (sc->is790) {
						outb(sc->asic_addr + ED_WD_MSR, 0x00);
					}
					outb(sc->asic_addr + ED_WD_LAAR, sc->wd_laar_proto);
					break;
				}
			}
		}
	} else {
		len = ed_pio_write_mbufs(sc, m, buffer);
		if (len == 0)
			goto outloop;
	}

	sc->txb_len[sc->txb_new] = max(len, ETHER_MIN_LEN);

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
	 * Tap off here if there is a bpf listener.
	 */
#if NBPFILTER > 0
	if (sc->bpf) {
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
	int     unit;
{
	register struct ed_softc *sc = &ed_softc[unit];
	u_char  boundry;
	u_short len;
	struct ed_ring packet_hdr;
	char   *packet_ptr;

	if (sc->gone)
		return;

	/*
	 * Set NIC to page 1 registers to get 'current' pointer
	 */
	outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STA);

	/*
	 * 'sc->next_packet' is the logical beginning of the ring-buffer -
	 * i.e. it points to where new data has been buffered. The 'CURR'
	 * (current) register points to the logical end of the ring-buffer -
	 * i.e. it points to where additional new data will be added. We loop
	 * here until the logical beginning equals the logical end (or in
	 * other words, until the ring-buffer is empty).
	 */
	while (sc->next_packet != inb(sc->nic_addr + ED_P1_CURR)) {

		/* get pointer to this buffer's header structure */
		packet_ptr = sc->mem_ring +
		    (sc->next_packet - sc->rec_page_start) * ED_PAGE_SIZE;

		/*
		 * The byte count includes a 4 byte header that was added by
		 * the NIC.
		 */
		if (sc->mem_shared)
			packet_hdr = *(struct ed_ring *) packet_ptr;
		else
			ed_pio_readmem(sc, packet_ptr, (char *) &packet_hdr,
				       sizeof(packet_hdr));
		len = packet_hdr.count;
		if (len > (ETHER_MAX_LEN + sizeof(struct ed_ring)) ||
		    len < (ETHER_HDR_SIZE + sizeof(struct ed_ring))) {
			/*
			 * Length is a wild value. There's a good chance that
			 * this was caused by the NIC being old and buggy.
			 * The bug is that the length low byte is duplicated in
			 * the high byte. Try to recalculate the length based on
			 * the pointer to the next packet.
			 */
			/*
			 * NOTE: sc->next_packet is pointing at the current packet.
			 */
			len &= ED_PAGE_SIZE - 1;	/* preserve offset into page */
			if (packet_hdr.next_packet >= sc->next_packet) {
				len += (packet_hdr.next_packet - sc->next_packet) * ED_PAGE_SIZE;
			} else {
				len += ((packet_hdr.next_packet - sc->rec_page_start) +
					(sc->rec_page_stop - sc->next_packet)) * ED_PAGE_SIZE;
			}
		}
		/*
		 * Be fairly liberal about what we allow as a "reasonable" length
		 * so that a [crufty] packet will make it to BPF (and can thus
		 * be analyzed). Note that all that is really important is that
		 * we have a length that will fit into one mbuf cluster or less;
		 * the upper layer protocols can then figure out the length from
		 * their own length field(s).
		 */
		if ((len > sizeof(struct ed_ring)) &&
		    (len <= MCLBYTES) &&
		    (packet_hdr.next_packet >= sc->rec_page_start) &&
		    (packet_hdr.next_packet < sc->rec_page_stop)) {
			/*
			 * Go get packet.
			 */
			ed_get_packet(sc, packet_ptr + sizeof(struct ed_ring),
				      len - sizeof(struct ed_ring), packet_hdr.rsr & ED_RSR_PHY);
			++sc->arpcom.ac_if.if_ipackets;
		} else {
			/*
			 * Really BAD. The ring pointers are corrupted.
			 */
			log(LOG_ERR,
			    "ed%d: NIC memory corrupt - invalid packet length %d\n",
			    unit, len);
			++sc->arpcom.ac_if.if_ierrors;
			ed_reset(unit);
			return;
		}

		/*
		 * Update next packet pointer
		 */
		sc->next_packet = packet_hdr.next_packet;

		/*
		 * Update NIC boundry pointer - being careful to keep it one
		 * buffer behind. (as recommended by NS databook)
		 */
		boundry = sc->next_packet - 1;
		if (boundry < sc->rec_page_start)
			boundry = sc->rec_page_stop - 1;

		/*
		 * Set NIC to page 0 registers to update boundry register
		 */
		outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_STA);

		outb(sc->nic_addr + ED_P0_BNRY, boundry);

		/*
		 * Set NIC to page 1 registers before looping to top (prepare
		 * to get 'CURR' current pointer)
		 */
		outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STA);
	}
}

/*
 * Ethernet interface interrupt processor
 */
void
edintr(unit)
	int     unit;
{
	struct ed_softc *sc = &ed_softc[unit];
	u_char  isr;

	if (sc->gone)
		return;
	/*
	 * Set NIC to page 0 registers
	 */
	outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_STA);

	/*
	 * loop until there are no more new interrupts
	 */
	while ((isr = inb(sc->nic_addr + ED_P0_ISR)) != 0) {

		/*
		 * reset all the bits that we are 'acknowledging' by writing a
		 * '1' to each bit position that was set (writing a '1'
		 * *clears* the bit)
		 */
		outb(sc->nic_addr + ED_P0_ISR, isr);

		/*
		 * Handle transmitter interrupts. Handle these first because
		 * the receiver will reset the board under some conditions.
		 */
		if (isr & (ED_ISR_PTX | ED_ISR_TXE)) {
			u_char  collisions = inb(sc->nic_addr + ED_P0_NCR) & 0x0f;

			/*
			 * Check for transmit error. If a TX completed with an
			 * error, we end up throwing the packet away. Really
			 * the only error that is possible is excessive
			 * collisions, and in this case it is best to allow
			 * the automatic mechanisms of TCP to backoff the
			 * flow. Of course, with UDP we're screwed, but this
			 * is expected when a network is heavily loaded.
			 */
			(void) inb(sc->nic_addr + ED_P0_TSR);
			if (isr & ED_ISR_TXE) {

				/*
				 * Excessive collisions (16)
				 */
				if ((inb(sc->nic_addr + ED_P0_TSR) & ED_TSR_ABT)
				    && (collisions == 0)) {

					/*
					 * When collisions total 16, the
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
				 * transmitted packets.
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
			 * transmission.
			 */
			sc->arpcom.ac_if.if_collisions += collisions;

			/*
			 * Decrement buffer in-use count if not zero (can only
			 * be zero if a transmitter interrupt occured while
			 * not actually transmitting). If data is ready to
			 * transmit, start it transmitting, otherwise defer
			 * until after handling receiver
			 */
			if (sc->txb_inuse && --sc->txb_inuse)
				ed_xmit(&sc->arpcom.ac_if);
		}

		/*
		 * Handle receiver interrupts
		 */
		if (isr & (ED_ISR_PRX | ED_ISR_RXE | ED_ISR_OVW)) {

			/*
			 * Overwrite warning. In order to make sure that a
			 * lockup of the local DMA hasn't occurred, we reset
			 * and re-init the NIC. The NSC manual suggests only a
			 * partial reset/re-init is necessary - but some chips
			 * seem to want more. The DMA lockup has been seen
			 * only with early rev chips - Methinks this bug was
			 * fixed in later revs. -DG
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
				ed_reset(unit);
			} else {

				/*
				 * Receiver Error. One or more of: CRC error,
				 * frame alignment error FIFO overrun, or
				 * missed packet.
				 */
				if (isr & ED_ISR_RXE) {
					++sc->arpcom.ac_if.if_ierrors;
#ifdef ED_DEBUG
					printf("ed%d: receive error %x\n", unit,
					       inb(sc->nic_addr + ED_P0_RSR));
#endif
				}

				/*
				 * Go get the packet(s) XXX - Doing this on an
				 * error is dubious because there shouldn't be
				 * any data to get (we've configured the
				 * interface to not accept packets with
				 * errors).
				 */

				/*
				 * Enable 16bit access to shared memory first
				 * on WD/SMC boards.
				 */
				if (sc->isa16bit &&
				    (sc->vendor == ED_VENDOR_WD_SMC)) {

					outb(sc->asic_addr + ED_WD_LAAR,
					     (sc->wd_laar_proto |=
					      ED_WD_LAAR_M16EN));
					if (sc->is790) {
						outb(sc->asic_addr + ED_WD_MSR,
						     ED_WD_MSR_MENB);
					}
				}
				ed_rint(unit);

				/* disable 16bit access */
				if (sc->isa16bit &&
				    (sc->vendor == ED_VENDOR_WD_SMC)) {

					if (sc->is790) {
						outb(sc->asic_addr + ED_WD_MSR, 0x00);
					}
					outb(sc->asic_addr + ED_WD_LAAR,
					     (sc->wd_laar_proto &=
					      ~ED_WD_LAAR_M16EN));
				}
			}
		}

		/*
		 * If it looks like the transmitter can take more data,
		 * attempt to start output on the interface. This is done
		 * after handling the receiver to give the receiver priority.
		 */
		if ((sc->arpcom.ac_if.if_flags & IFF_OACTIVE) == 0)
			ed_start(&sc->arpcom.ac_if);

		/*
		 * return NIC CR to standard state: page 0, remote DMA
		 * complete, start (toggling the TXP bit off, even if was just
		 * set in the transmit routine, is *okay* - it is 'edge'
		 * triggered from low to high)
		 */
		outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_STA);

		/*
		 * If the Network Talley Counters overflow, read them to reset
		 * them. It appears that old 8390's won't clear the ISR flag
		 * otherwise - resulting in an infinite loop.
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
	int     command;
	caddr_t data;
{
	register struct ifaddr *ifa = (struct ifaddr *) data;
	struct ed_softc *sc = &ed_softc[ifp->if_unit];
	struct ifreq *ifr = (struct ifreq *) data;
	int     s, error = 0;

	if (sc->gone) {
		ifp->if_flags &= ~IFF_RUNNING;
		return ENXIO;
	}
	s = splimp();

	switch (command) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* netifs are BUSY when UP */
		sc->kdc.kdc_state = DC_BUSY;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			ed_init(ifp->if_unit);	/* before arpwhohas */
			arp_ifinit((struct arpcom *)ifp, ifa);
			break;
#endif
#ifdef IPX
		/*
		 * XXX - This code is probably wrong
		 */
		case AF_IPX:
			{
				register struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);

				if (ipx_nullhost(*ina))
					ina->x_host =
					    *(union ipx_host *) (sc->arpcom.ac_enaddr);
				else {
					bcopy((caddr_t) ina->x_host.c_host,
					      (caddr_t) sc->arpcom.ac_enaddr,
					      sizeof(sc->arpcom.ac_enaddr));
				}

				/*
				 * Set new address
				 */
				ed_init(ifp->if_unit);
				break;
			}
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
					    *(union ns_host *) (sc->arpcom.ac_enaddr);
				else {
					bcopy((caddr_t) ina->x_host.c_host,
					      (caddr_t) sc->arpcom.ac_enaddr,
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

	case SIOCGIFADDR:
		{
			struct sockaddr *sa;

			sa = (struct sockaddr *) & ifr->ifr_data;
			bcopy((caddr_t) sc->arpcom.ac_enaddr,
			      (caddr_t) sa->sa_data, ETHER_ADDR_LEN);
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
			 * If interface is marked up and it is stopped, then
			 * start it
			 */
			if ((ifp->if_flags & IFF_UP) &&
			    ((ifp->if_flags & IFF_RUNNING) == 0))
				ed_init(ifp->if_unit);
		}
		/* UP controls BUSY/IDLE */
		sc->kdc.kdc_state = ((ifp->if_flags & IFF_UP)
				     ? DC_BUSY
				     : DC_IDLE);

#if NBPFILTER > 0

		/*
		 * Promiscuous flag may have changed, so reprogram the RCR.
		 */
		ed_setrcr(ifp, sc);
#endif

		/*
		 * An unfortunate hack to provide the (required) software
		 * control of the tranceiver for 3Com boards. The ALTPHYS flag
		 * disables the tranceiver if set.
		 */
		if (sc->vendor == ED_VENDOR_3COM) {
			if (ifp->if_flags & IFF_ALTPHYS) {
				outb(sc->asic_addr + ED_3COM_CR, 0);
			} else {
				outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_XSEL);
			}
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/*
		 * Update out multicast list.
		 */
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->arpcom) :
		    ether_delmulti(ifr, &sc->arpcom);

		if (error == ENETRESET) {

			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			ed_setrcr(ifp, sc);
			error = 0;
		}
		break;

	case SIOCSIFMTU:
		/*
		 * Set the interface MTU.
		 */
		if (ifr->ifr_mtu > ETHERMTU) {
			error = EINVAL;
		} else {
			ifp->if_mtu = ifr->ifr_mtu;
		}
		break;

	default:
		error = EINVAL;
	}
	(void) splx(s);
	return (error);
}

/*
 * Retreive packet from shared memory and send to the next level up via
 *	ether_input(). If there is a BPF listener, give a copy to BPF, too.
 */
static void
ed_get_packet(sc, buf, len, multicast)
	struct ed_softc *sc;
	char   *buf;
	u_short len;
	int     multicast;
{
	struct ether_header *eh;
	struct mbuf *m;

	/* Allocate a header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;
	m->m_pkthdr.rcvif = &sc->arpcom.ac_if;
	m->m_pkthdr.len = m->m_len = len;

	/* Attach an mbuf cluster */
	MCLGET(m, M_DONTWAIT);

	/* Insist on getting a cluster */
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return;
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
	ed_ring_copy(sc, buf, (char *)eh, len);

#if NBPFILTER > 0

	/*
	 * Check if there's a BPF listener on this interface. If so, hand off
	 * the raw packet to bpf.
	 */
	if (sc->bpf) {
		bpf_mtap(sc->bpf, m);

		/*
		 * Note that the interface cannot be in promiscuous mode if
		 * there are no BPF listeners.  And if we are in promiscuous
		 * mode, we have to check if this packet is really ours.
		 */
		if ((sc->arpcom.ac_if.if_flags & IFF_PROMISC) &&
		    bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr,
		      sizeof(eh->ether_dhost)) != 0 && multicast == 0) {
			m_freem(m);
			return;
		}
	}
#endif

	/*
	 * Remove link layer address.
	 */
	m->m_pkthdr.len = m->m_len = len - sizeof(struct ether_header);
	m->m_data += sizeof(struct ether_header);

	ether_input(&sc->arpcom.ac_if, eh, m);
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
ed_pio_readmem(sc, src, dst, amount)
	struct ed_softc *sc;
	unsigned short src;
	unsigned char *dst;
	unsigned short amount;
{
	/* select page 0 registers */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2 | ED_CR_STA);

	/* round up to a word */
	if (amount & 1)
		++amount;

	/* set up DMA byte count */
	outb(sc->nic_addr + ED_P0_RBCR0, amount);
	outb(sc->nic_addr + ED_P0_RBCR1, amount >> 8);

	/* set up source address in NIC mem */
	outb(sc->nic_addr + ED_P0_RSAR0, src);
	outb(sc->nic_addr + ED_P0_RSAR1, src >> 8);

	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD0 | ED_CR_STA);

	if (sc->isa16bit) {
		insw(sc->asic_addr + ED_NOVELL_DATA, dst, amount / 2);
	} else
		insb(sc->asic_addr + ED_NOVELL_DATA, dst, amount);

}

/*
 * Stripped down routine for writing a linear buffer to NIC memory.
 *	Only used in the probe routine to test the memory. 'len' must
 *	be even.
 */
void
ed_pio_writemem(sc, src, dst, len)
	struct ed_softc *sc;
	char   *src;
	unsigned short dst;
	unsigned short len;
{
	int     maxwait = 200;	/* about 240us */

	/* select page 0 registers */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2 | ED_CR_STA);

	/* reset remote DMA complete flag */
	outb(sc->nic_addr + ED_P0_ISR, ED_ISR_RDC);

	/* set up DMA byte count */
	outb(sc->nic_addr + ED_P0_RBCR0, len);
	outb(sc->nic_addr + ED_P0_RBCR1, len >> 8);

	/* set up destination address in NIC mem */
	outb(sc->nic_addr + ED_P0_RSAR0, dst);
	outb(sc->nic_addr + ED_P0_RSAR1, dst >> 8);

	/* set remote DMA write */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD1 | ED_CR_STA);

	if (sc->isa16bit)
		outsw(sc->asic_addr + ED_NOVELL_DATA, src, len / 2);
	else
		outsb(sc->asic_addr + ED_NOVELL_DATA, src, len);

	/*
	 * Wait for remote DMA complete. This is necessary because on the
	 * transmit side, data is handled internally by the NIC in bursts and
	 * we can't start another remote DMA until this one completes. Not
	 * waiting causes really bad things to happen - like the NIC
	 * irrecoverably jamming the ISA bus.
	 */
	while (((inb(sc->nic_addr + ED_P0_ISR) & ED_ISR_RDC) != ED_ISR_RDC) && --maxwait);
}

/*
 * Write an mbuf chain to the destination NIC memory address using
 *	programmed I/O.
 */
u_short
ed_pio_write_mbufs(sc, m, dst)
	struct ed_softc *sc;
	struct mbuf *m;
	unsigned short dst;
{
	unsigned short total_len, dma_len;
	struct mbuf *mp;
	int     maxwait = 200;	/* about 240us */

	/* First, count up the total number of bytes to copy */
	for (total_len = 0, mp = m; mp; mp = mp->m_next)
		total_len += mp->m_len;

	dma_len = total_len;
	if (sc->isa16bit && (dma_len & 1))
		dma_len++;

	/* select page 0 registers */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2 | ED_CR_STA);

	/* reset remote DMA complete flag */
	outb(sc->nic_addr + ED_P0_ISR, ED_ISR_RDC);

	/* set up DMA byte count */
	outb(sc->nic_addr + ED_P0_RBCR0, dma_len);
	outb(sc->nic_addr + ED_P0_RBCR1, dma_len >> 8);

	/* set up destination address in NIC mem */
	outb(sc->nic_addr + ED_P0_RSAR0, dst);
	outb(sc->nic_addr + ED_P0_RSAR1, dst >> 8);

	/* set remote DMA write */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD1 | ED_CR_STA);

  /*
   * Transfer the mbuf chain to the NIC memory.
   * 16-bit cards require that data be transferred as words, and only words.
   * So that case requires some extra code to patch over odd-length mbufs.
   */

	if (!sc->isa16bit) {
		/* NE1000s are easy */
		while (m) {
			if (m->m_len) {
				outsb(sc->asic_addr + ED_NOVELL_DATA,
				      m->m_data, m->m_len);
			}
			m = m->m_next;
		}
	} else {
		/* NE2000s are a pain */
		unsigned char *data;
		int len, wantbyte;
		unsigned char savebyte[2];

		wantbyte = 0;

		while (m) {
			len = m->m_len;
			if (len) {
				data = mtod(m, caddr_t);
				/* finish the last word */
				if (wantbyte) {
					savebyte[1] = *data;
					outw(sc->asic_addr + ED_NOVELL_DATA, *(u_short *)savebyte);
					data++;
					len--;
					wantbyte = 0;
				}
				/* output contiguous words */
				if (len > 1) {
					outsw(sc->asic_addr + ED_NOVELL_DATA,
					      data, len >> 1);
					data += len & ~1;
					len &= 1;
				}
				/* save last byte, if necessary */
				if (len == 1) {
					savebyte[0] = *data;
					wantbyte = 1;
				}
			}
			m = m->m_next;
		}
		/* spit last byte */
		if (wantbyte) {
			outw(sc->asic_addr + ED_NOVELL_DATA, *(u_short *)savebyte);
		}
	}

	/*
	 * Wait for remote DMA complete. This is necessary because on the
	 * transmit side, data is handled internally by the NIC in bursts and
	 * we can't start another remote DMA until this one completes. Not
	 * waiting causes really bad things to happen - like the NIC
	 * irrecoverably jamming the ISA bus.
	 */
	while (((inb(sc->nic_addr + ED_P0_ISR) & ED_ISR_RDC) != ED_ISR_RDC) && --maxwait);

	if (!maxwait) {
		log(LOG_WARNING, "ed%d: remote transmit DMA failed to complete\n",
		    sc->arpcom.ac_if.if_unit);
		ed_reset(sc->arpcom.ac_if.if_unit);
		return(0);
	}
	return (total_len);
}

/*
 * Given a source and destination address, copy 'amount' of a packet from
 *	the ring buffer into a linear destination buffer. Takes into account
 *	ring-wrap.
 */
static inline char *
ed_ring_copy(sc, src, dst, amount)
	struct ed_softc *sc;
	char   *src;
	char   *dst;
	u_short amount;
{
	u_short tmp_amount;

	/* does copy wrap to lower addr in ring buffer? */
	if (src + amount > sc->mem_end) {
		tmp_amount = sc->mem_end - src;

		/* copy amount up to end of NIC memory */
		if (sc->mem_shared)
			bcopy(src, dst, tmp_amount);
		else
			ed_pio_readmem(sc, src, dst, tmp_amount);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst += tmp_amount;
	}
	if (sc->mem_shared)
		bcopy(src, dst, amount);
	else
		ed_pio_readmem(sc, src, dst, amount);

	return (src + amount);
}

void
ed_setrcr(ifp, sc)
	struct ifnet *ifp;
	struct ed_softc *sc;
{
	int     i;

	/* set page 1 registers */
	outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STP);

	if (ifp->if_flags & IFF_PROMISC) {

		/*
		 * Reconfigure the multicast filter.
		 */
		for (i = 0; i < 8; i++)
			outb(sc->nic_addr + ED_P1_MAR0 + i, 0xff);

		/*
		 * And turn on promiscuous mode. Also enable reception of
		 * runts and packets with CRC & alignment errors.
		 */
		/* Set page 0 registers */
		outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_STP);

		outb(sc->nic_addr + ED_P0_RCR, ED_RCR_PRO | ED_RCR_AM |
		     ED_RCR_AB | ED_RCR_AR | ED_RCR_SEP);
	} else {
		/* set up multicast addresses and filter modes */
		if (ifp->if_flags & IFF_MULTICAST) {
			u_long  mcaf[2];

			if (ifp->if_flags & IFF_ALLMULTI) {
				mcaf[0] = 0xffffffff;
				mcaf[1] = 0xffffffff;
			} else
				ds_getmcaf(sc, mcaf);

			/*
			 * Set multicast filter on chip.
			 */
			for (i = 0; i < 8; i++)
				outb(sc->nic_addr + ED_P1_MAR0 + i, ((u_char *) mcaf)[i]);

			/* Set page 0 registers */
			outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_STP);

			outb(sc->nic_addr + ED_P0_RCR, ED_RCR_AM | ED_RCR_AB);
		} else {

			/*
			 * Initialize multicast address hashing registers to
			 * not accept multicasts.
			 */
			for (i = 0; i < 8; ++i)
				outb(sc->nic_addr + ED_P1_MAR0 + i, 0x00);

			/* Set page 0 registers */
			outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_STP);

			outb(sc->nic_addr + ED_P0_RCR, ED_RCR_AB);
		}
	}

	/*
	 * Start interface.
	 */
	outb(sc->nic_addr + ED_P0_CR, sc->cr_proto | ED_CR_STA);
}

/*
 * Compute crc for ethernet address
 */
u_long
ds_crc(ep)
	u_char *ep;
{
#define POLYNOMIAL 0x04c11db6
	register u_long crc = 0xffffffffL;
	register int carry, i, j;
	register u_char b;

	for (i = 6; --i >= 0;) {
		b = *ep++;
		for (j = 8; --j >= 0;) {
			carry = ((crc & 0x80000000L) ? 1 : 0) ^ (b & 0x01);
			crc <<= 1;
			b >>= 1;
			if (carry)
				crc = ((crc ^ POLYNOMIAL) | carry);
		}
	}
	return crc;
#undef POLYNOMIAL
}

/*
 * Compute the multicast address filter from the
 * list of multicast addresses we need to listen to.
 */
void
ds_getmcaf(sc, mcaf)
	struct ed_softc *sc;
	u_long *mcaf;
{
	register u_int index;
	register u_char *af = (u_char *) mcaf;
	register struct ether_multi *enm;
	register struct ether_multistep step;

	mcaf[0] = 0;
	mcaf[1] = 0;

	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, 6) != 0) {
			mcaf[0] = 0xffffffff;
			mcaf[1] = 0xffffffff;
			return;
		}
		index = ds_crc(enm->enm_addrlo) >> 26;
		af[index >> 3] |= 1 << (index & 7);

		ETHER_NEXT_MULTI(step, enm);
	}
}
