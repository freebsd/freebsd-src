/*
 * Device driver for National Semiconductor DS8390 based ethernet
 *   adapters. By David Greenman, 29-April-1993
 *
 * Copyright (C) 1993, David Greenman. This software may be used, modified,
 *   copied, distributed, and sold, in both source and binary form provided
 *   that the above copyright and these terms are retained. Under no
 *   circumstances is the author responsible for the proper functioning
 *   of this software, nor does the author assume any responsibility
 *   for damages incurred with its use.
 *
 * Currently supports the Western Digital/SMC 8003 and 8013 series
 *   and the 3Com 3c503
 */

/*
 * Modification history
 *
 * $Log$
 * 
 */
 
#include "ed.h"
#if	NED > 0
#include "bpfilter.h"

#include "param.h"
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

 
/*
 * ed_softc: per line info and status
 */
struct	ed_softc {
	struct	arpcom arpcom;	/* ethernet common */

	char	*type_str;	/* pointer to type string */
	u_char	vendor;		/* interface vendor */
	u_char	type;		/* interface type code */

	u_short	vector;		/* interrupt vector */
	u_short	asic_addr;	/* ASIC I/O bus address */
	u_short	nic_addr;	/* NIC (DS8390) I/O bus address */

	caddr_t	smem_start;	/* shared memory start address */
	caddr_t	smem_end;	/* shared memory end address */
	u_long	smem_size;	/* total shared memory size */
	caddr_t	smem_ring;	/* start of RX ring-buffer (in smem) */

	caddr_t	bpf;		/* BPF "magic cookie" */

	u_char	memwidth;	/* width of access to card mem 8 or 16 */
	u_char	xmit_busy;	/* transmitter is busy */
	u_char	txb_cnt;	/* Number of transmit buffers */
	u_char	txb_next;	/* Pointer to next buffer ready to xmit */
	u_short	txb_next_len;	/* next xmit buffer length */
	u_char	data_buffered;	/* data has been buffered in interface memory */
	u_char	tx_page_start;	/* first page of TX buffer area */

	u_char	rec_page_start;	/* first page of RX ring-buffer */
	u_char	rec_page_stop;	/* last page of RX ring-buffer */
	u_char	next_packet;	/* pointer to next unread RX packet */
} ed_softc[NED];

int	ed_attach(), ed_init(), edintr(), ed_ioctl(), ed_probe(),
	ed_start(), ed_reset(), ed_watchdog();

static void ed_stop();

static inline void ed_rint();
static inline void ed_xmit();
static inline char *ed_ring_copy();

extern int ether_output();

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
	int i, x;
	u_int memsize;
	u_char iptr, memwidth, sum, tmp;

	/*
	 * Setup initial i/o address for ASIC and NIC
	 */
	sc->asic_addr = isa_dev->id_iobase;
	sc->vector = isa_dev->id_irq;
	sc->smem_start = (caddr_t)isa_dev->id_maddr;
 
	/*
	 * Attempt to do a checksum over the station address PROM.
	 * This is mapped differently on the WD80x3 and 3C503, so if
	 *	it fails, it might be a 3C503. There is a problem with
	 *	this, though: some clone WD boards don't pass the
	 *	checksum test. Danpex boards for one. We need to do
	 *	additional checking for this case.
	 */
	for (sum = 0, i = 0; i < 8; ++i) {
		sum += inb(sc->asic_addr + ED_WD_PROM + i);
	}
	
	if (sum == ED_WD_ROM_CHECKSUM_TOTAL) {
		goto type_WD80x3;
	} else {
		/*
		 * Do additional checking to make sure its a 3Com and
		 * not a broken WD clone
		 */
		goto type_3Com;
	}

type_WD80x3:
	/*
	 * Looks like a WD/SMC board
	 */

	sc->vendor = ED_VENDOR_WD_SMC;
	sc->type = inb(sc->asic_addr + ED_WD_CARD_ID);

	sc->nic_addr = sc->asic_addr + ED_WD_NIC_OFFSET;

	/* reset card to force it into a known state. */
	outb(sc->asic_addr + ED_WD_MSR, ED_WD_MSR_RST);
	DELAY(100);
	outb(sc->asic_addr + ED_WD_MSR, inb(sc->asic_addr + ED_WD_MSR) & ~ED_WD_MSR_RST);
	/* wait in the case this card is reading it's EEROM */
	DELAY(5000);

	/*
	 * Set initial values for width/size.
	 */
	switch (sc->type) {
	case ED_TYPE_WD8003S:
		sc->type_str = "WD8003S";
		memsize = 8192;
		memwidth = 8;
		break;
	case ED_TYPE_WD8003E:
		sc->type_str = "WD8003E";
		memsize = 8192;
		memwidth = 8;
		break;
	case ED_TYPE_WD8013EBT:
		sc->type_str = "WD8013EBT";
		memsize = 16384;
		memwidth = 16;
		break;
	case ED_TYPE_WD8013EB:		/* also WD8003EP */
		if (inb(sc->asic_addr + ED_WD_ICR)
			& ED_WD_ICR_16BIT) {
			memwidth = 16;
			memsize = 16384;
			sc->type_str = "WD8013EB";
		} else {
			sc->type_str = "WD8003EP";
			memsize = 8192;
			memwidth = 8;
		}
		break;
	case ED_TYPE_WD8013EBP:
		sc->type_str = "WD8013EBP";
		memsize = 16384;
		memwidth = 16;
		break;
	case ED_TYPE_WD8013EPC:
		sc->type_str = "WD8013EPC";
		memsize = 16384;
		memwidth = 16;
		break;
	default:
		sc->type_str = "unknown";
		memsize = 8192;
		memwidth = 8;
		break;
	}
	/*
	 * Make some adjustments to initial values depending on what is
	 *	found in the ICR.
	 */
	if ((memwidth==16)
		&& ((inb(sc->asic_addr + ED_WD_ICR) & ED_WD_ICR_16BIT) == 0)) {
		memwidth = 8;
		memsize = 8192;
	}
	if (inb(sc->asic_addr + ED_WD_ICR) & ED_WD_ICR_MSZ) {
		memsize = 32768;
	}

#if ED_DEBUG
	printf("type=%s width=%d memsize=%d\n",sc->type_str,memwidth,memsize);
	for (i=0; i<8; i++)
		printf("%x -> %x\n", i, inb(sc->asic_addr + i));
#endif

	if (sc->type & ED_WD_SOFTCONFIG) {
		iptr = inb(isa_dev->id_iobase + 1) & 4 |
			((inb(isa_dev->id_iobase+4) & 0x60) >> 5);
		if (ed_intr_mask[iptr] != isa_dev->id_irq) {
			printf("ed%d: kernel configured irq doesn't match board configured irq\n",
				isa_dev->id_unit);
			return(0);
		}
		outb(isa_dev->id_iobase+4, inb(isa_dev->id_iobase+4) | 0x80);
	}

	sc->memwidth = memwidth;
	/*
	 * allocate one xmit buffer if < 16k, two buffers otherwise
	 */
	if (memsize < 16384) {
		sc->smem_ring = sc->smem_start + (ED_PAGE_SIZE * ED_TXBUF_SIZE);
		sc->txb_cnt = 1;
		sc->rec_page_start = ED_TXBUF_SIZE;
	} else {
		sc->smem_ring = sc->smem_start + (ED_PAGE_SIZE * ED_TXBUF_SIZE * 2);
		sc->txb_cnt = 2;
		sc->rec_page_start = ED_TXBUF_SIZE * 2;
	}
	sc->smem_size = memsize;
	sc->smem_end = sc->smem_start + memsize;
	sc->rec_page_stop = memsize / ED_PAGE_SIZE;
	sc->tx_page_start = ED_WD_PAGE_OFFSET;

	/*
	 * Get station address from on-board ROM
	 */
	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		sc->arpcom.ac_enaddr[i] = inb(sc->asic_addr + ED_WD_PROM + i);

	/*
	 * Set address and enable interface shared memory.
	 */
        outb(sc->asic_addr + ED_WD_MSR, ((kvtop(sc->smem_start) >> 13) &
		ED_WD_MSR_ADDR) | ED_WD_MSR_MENB);

	/*
	 * Set upper address bits and 8/16 bit access to shared memory
	 */
	if (sc->type & ED_WD_SOFTCONFIG) {
		if (memwidth == 8) {
			outb(sc->asic_addr + ED_WD_LAAR,
			((kvtop(sc->smem_start) >> 19) & ED_WD_LAAR_ADDRHI));
		} else {
			outb(sc->asic_addr + ED_WD_LAAR,
				ED_WD_LAAR_L16EN | ED_WD_LAAR_M16EN |
				((kvtop(sc->smem_start) >> 19) & ED_WD_LAAR_ADDRHI));
		}
	}

	/*
	 * Now zero memory and verify that it is clear
	 */
	bzero(sc->smem_start, memsize);

	for (i = 0; i < memsize; ++i)
		if (sc->smem_start[i]) {
	        	printf("ed%d: failed to clear shared memory at %x - check configuration\n",
				isa_dev->id_unit, sc->smem_start + i);

			/*
			 * Disable 16 bit access to shared memory
			 */
			if (memwidth == 16)
				outb(sc->asic_addr + ED_WD_LAAR,
					inb(sc->asic_addr + ED_WD_LAAR)
					& ~ED_WD_LAAR_M16EN);

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
	if (memwidth == 16)
		outb(sc->asic_addr + ED_WD_LAAR, inb(sc->asic_addr + ED_WD_LAAR)
			& ~ED_WD_LAAR_M16EN);

	isa_dev->id_msize = memsize;
	return (ED_WD_IO_PORTS);

type_3Com:
	/*
	 * Looks like a 3Com board
	 */

	sc->vendor = ED_VENDOR_3COM;
	sc->asic_addr = isa_dev->id_iobase + ED_3COM_ASIC_OFFSET;
	sc->nic_addr = isa_dev->id_iobase + ED_3COM_NIC_OFFSET;

	sc->type_str = "3c503";

	memsize = 8192;

	/*
	 * Verify that the kernel configured I/O address matches the board
	 *	configured address
	 */
	switch (sc->asic_addr + ED_3COM_BCFR) {
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
	}

	/*
	 * Verify that the kernel shared memory address matches the
	 *	board configured address.
	 */
	switch (sc->asic_addr + ED_3COM_PCFR) {
	case ED_3COM_PCFR_DC000:
		if (kvtop(isa_dev->id_maddr) != ED_3COM_PCFR_DC000)
			return(0);
		break;
	case ED_3COM_PCFR_D8000:
		if (kvtop(isa_dev->id_maddr) != ED_3COM_PCFR_D8000)
			return(0);
		break;
	case ED_3COM_PCFR_CC000:
		if (kvtop(isa_dev->id_maddr) != ED_3COM_PCFR_CC000)
			return(0);
		break;
	case ED_3COM_PCFR_C8000:
		if (kvtop(isa_dev->id_maddr) != ED_3COM_PCFR_C8000)
			return(0);
		break;
	}

	/*
	 * Reset NIC and ASIC
	 */
	outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_RST);
	/*
	 * Verify that reset bit was set
	 */
	if ((inb(sc->asic_addr + ED_3COM_CR) & ED_3COM_CR_RST) == 0)
		return (0);
	/*
	 * Wait for a while, then un-reset it
	 */
	DELAY(5000);
	outb(sc->asic_addr + ED_3COM_CR, 0);

	/*
	 * Verify that the bit cleared
	 */
	if (inb(sc->asic_addr + ED_3COM_CR) & ED_3COM_CR_RST)
		return (0);

	/*
	 * Wait a bit for the NIC to recover from the reset
	 */
	DELAY(5000);

	/*
	 * Determine if this is an 8bit or 16bit board
	 */
	/* XXX - either this code is broken...or the hardware is... */
	/*	it always comes up 8bit */
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
		memwidth = 16;
	else
		memwidth = 8;

#if 0
printf(" (%dbit)",memwidth);
#endif
	/*
	 * select page 0 registers
	 */
        outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STP);

	sc->txb_cnt = 1;

	sc->tx_page_start = ED_3COM_PAGE_OFFSET;
	sc->rec_page_start = ED_TXBUF_SIZE + ED_3COM_PAGE_OFFSET;
	sc->rec_page_stop = memsize / ED_PAGE_SIZE + ED_3COM_PAGE_OFFSET;

	sc->smem_size = memsize;
	sc->smem_end = sc->smem_start + memsize;
	sc->smem_ring = sc->smem_start + (ED_PAGE_SIZE * ED_TXBUF_SIZE);

	sc->memwidth = memwidth;

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
		printf("ed0: Invalid irq configuration\n");
		return(0);
	}

	/*
	 * Initialize GA configuration register. Set bank and enable smem.
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
	 * Get station address from on-board ROM
	 */
	/*
	 * First, map ethernet address PROM over the top of where the NIC
	 *	registers normally appear.
	 */
	outb(sc->asic_addr + ED_3COM_CR, ED_3COM_CR_EALO);

	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		sc->arpcom.ac_enaddr[i] = inb(sc->nic_addr + i);

	/*
	 * Unmap PROM - select NIC registers
	 */
	outb(sc->asic_addr + ED_3COM_CR, 0);

	/*
	 * Zero memory and verify that it is clear
	 */
#if 0
{ char	test_buf[1024];
printf("starting write\n");
	for (i = 0; i < 8*8192; ++i)
	bcopy(test_buf, sc->smem_start, 1024);
printf("starting read\n");
	for (i = 0; i < 8*8192; ++i)
	bcopy(sc->smem_start, test_buf, 1024);
printf("done.\n");
}
#endif

	bzero(sc->smem_start, memsize);

	for (i = 0; i < memsize; ++i)
		if (sc->smem_start[i]) {
	        	printf("ed%d: failed to clear shared memory at %x - check configuration\n",
				isa_dev->id_unit, sc->smem_start + i);
			return(0);
		}

	isa_dev->id_msize = memsize;
	return(ED_3COM_IO_PORTS);
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
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS ;
	ifp->if_init = ed_init;
	ifp->if_output = ether_output;
	ifp->if_start = ed_start;
	ifp->if_ioctl = ed_ioctl;
	ifp->if_reset = ed_reset;
	ifp->if_watchdog = ed_watchdog;

	if_attach(ifp);

#if NBPFILTER > 0
	bpfattach(&sc->bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	/*
	 * Search down the ifa address list looking for the AF_LINK type entry
	 */
 	ifa = ifp->if_addrlist;
	while ((ifa != 0) && (ifa->ifa_addr != 0) &&
	    (ifa->ifa_addr->sa_family != AF_LINK))
		ifa = ifa->ifa_next;

	/*
	 * If we find an AF_LINK type entry, we well fill in the hardware addr
	 */
	if ((ifa != 0) && (ifa->ifa_addr != 0)) {
		/*
		 * Fill in the link level address for this interface
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
	printf(" enet %s type %s", ether_sprintf(sc->arpcom.ac_enaddr),
		sc->type_str);
}
 
/*
 * Reset interface.
 */
int
ed_reset(unit, uban)
	int unit;
{
	int s;

	s = splnet();

	/*
	 * Stop interface and re-initialize.
	 */
	ed_stop(unit);
	ed_init(unit);

	s = splx(s);
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
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STP);

	/*
	 * Wait for interface to enter stopped state, but limit # of checks
	 *	to 'n' (about 5ms). It shouldn't even take 5us on modern
	 *	DS8390's, but just in case it's an old one.
	 */
	while ((inb(sc->nic_addr + ED_P0_ISR) & ED_ISR_RST) == 0) {
		if (--n == 0)
			break;
	}
}

int
ed_watchdog(unit)
	int unit;
{
	log(LOG_ERR, "ed%d: device timeout\n", unit);

	ed_reset(unit);
}

/*
 * Initialize device. 
 */
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
	s = splnet();

	/* reset transmitter flags */
	sc->data_buffered = 0;
	sc->xmit_busy = 0;
	sc->arpcom.ac_if.if_timer = 0;

	sc->txb_next = 0;

	/* This variable is used below - don't move this assignment */
	sc->next_packet = sc->rec_page_start + 1;

	/*
	 * Set interface for page 0, Remote DMA complete, Stopped
	 */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STP);

	if (sc->memwidth == 16) {
		/*
		 * Set FIFO threshold to 8, No auto-init Remote DMA,
		 *	byte order=80x86, word-wide DMA xfers
		 */
		outb(sc->nic_addr + ED_P0_DCR, ED_DCR_FT1|ED_DCR_WTS);
	} else {
		/*
		 * Same as above, but byte-wide DMA xfers
		 */
		outb(sc->nic_addr + ED_P0_DCR, ED_DCR_FT1);
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
	outb(sc->nic_addr + ED_P0_CR, ED_CR_PAGE_1|ED_CR_RD2|ED_CR_STP);

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
	outb(sc->nic_addr + ED_P1_CR, ED_CR_RD2|ED_CR_STA);

	/*
	 * Take interface out of loopback
	 */
	outb(sc->nic_addr + ED_P0_TCR, 0);

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
 
static inline void ed_xmit(ifp)
	struct ifnet *ifp;
{
	struct ed_softc *sc = &ed_softc[ifp->if_unit];
	u_short len = sc->txb_next_len;

	/*
	 * Set NIC for page 0 register access
	 */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STA);

	/*
	 * Set TX buffer start page
	 */
	outb(sc->nic_addr + ED_P0_TPSR, sc->tx_page_start +
		sc->txb_next * ED_TXBUF_SIZE);

	/*
	 * Set TX length
	 */
	outb(sc->nic_addr + ED_P0_TBCR0, len & 0xff);
	outb(sc->nic_addr + ED_P0_TBCR1, len >> 8);

	/*
	 * Set page 0, Remote DMA complete, Transmit Packet, and Start
	 */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_TXP|ED_CR_STA);

	sc->xmit_busy = 1;
	sc->data_buffered = 0;
	
	if ((sc->txb_next == 0) && (sc->txb_cnt > 1)) 
		sc->txb_next = 1;
	else
		sc->txb_next = 0;

	/*
	 * Set a timer just in case we never hear from the board again
	 */
	ifp->if_timer = 2;
}

/*
 * Start output on interface.
 * We make two assumptions here:
 *  1) that the current priority is set to splnet _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) that the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 */
int
ed_start(ifp)
	struct ifnet *ifp;
{
	struct ed_softc *sc = &ed_softc[ifp->if_unit];
	struct mbuf *m0, *m;
	caddr_t buffer;
	int len;
	u_char laar_tmp;

outloop:
	if (sc->data_buffered)
		if (sc->xmit_busy) {
			ifp->if_flags |= IFF_OACTIVE;
			return;
		} else {
			/*
			 * Note that ed_xmit() resets the data_buffered flag
			 *  before returning
			 */
			ed_xmit(ifp);
		}

	IF_DEQUEUE(&sc->arpcom.ac_if.if_snd, m);
	if (m == 0) {
	/*
	 * The following isn't pretty; we are using the !OACTIVE flag to
	 * indicate to the outside world that we can accept an additional
	 * packet rather than that the transmitter is _actually_
	 * active. Indeed, the transmitter may be active, but if we haven't
	 * filled the secondary buffer with data then we still want to
	 * accept more.
	 * Note that it isn't necessary to test the data_buffered flag -
	 * we wouldn't have tried to de-queue the packet in the first place
	 * if it was set.
	 */
		ifp->if_flags &= ~IFF_OACTIVE;
		return;
	}

	/*
	 * Copy the mbuf chain into the transmit buffer
	 */
	/*
	 * Enable 16bit access to shared memory on WD/SMC boards
	 */
	if (sc->memwidth == 16)
		if (sc->vendor == ED_VENDOR_WD_SMC) {
			laar_tmp = inb(sc->asic_addr + ED_WD_LAAR);
			outb(sc->asic_addr + ED_WD_LAAR, laar_tmp | ED_WD_LAAR_M16EN);
		}

	buffer = sc->smem_start + (sc->txb_next * ED_TXBUF_SIZE * ED_PAGE_SIZE);
	len = 0;
	for (m0 = m; m != 0; m = m->m_next) {
		bcopy(mtod(m, caddr_t), buffer, m->m_len);
		buffer += m->m_len;
        	len += m->m_len;
	}

	/*
	 * Restore previous shared mem access type
	 */
	if (sc->memwidth == 16)
		if (sc->vendor == ED_VENDOR_WD_SMC) {
			outb(sc->asic_addr + ED_WD_LAAR, laar_tmp);
		}

	sc->txb_next_len = MAX(len, ETHER_MIN_LEN);

	if (sc->txb_cnt > 1)
		/*
		 * only set 'buffered' flag if doing multiple buffers
		 */
		sc->data_buffered = 1;

	if (sc->xmit_busy == 0)
		ed_xmit(ifp);
	/*
	 * If there is BPF support in the configuration, tap off here.
	 *   The following has support for converting trailer packets
	 *   back to normal.
	 */
#if NBPFILTER > 0
	if (sc->bpf) {
		u_short etype;
		int off, datasize, resid;
		struct ether_header *eh;
		struct trailer_header {
			u_short ether_type;
			u_short ether_residual;
		} trailer_header;
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

	if (sc->txb_cnt > 1)
		goto outloop;
	else {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}
}
 
/*
 * Ethernet interface receiver interrupt.
 */
static inline void /* only called from one place, so may as well integrate */
ed_rint(unit)
	int unit;
{
	register struct ed_softc *sc = &ed_softc[unit];
	u_char boundry, current;
	u_short len;
	struct ed_ring *packet_ptr;

	/*
	 * Set NIC to page 1 registers to get 'current' pointer
	 */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_PAGE_1|ED_CR_RD2|ED_CR_STA);

	/*
	 * 'sc->next_packet' is the logical beginning of the ring-buffer - i.e.
	 *	it points to where new data has been buffered. The 'CURR'
	 *	(current) register points to the logical end of the ring-buffer
	 *	- i.e. it points to where additional new data will be added.
	 *	We loop here until the logical beginning equals the logical
	 *	end (or in other words, until the ring-buffer is empty).
	 */
	while (sc->next_packet != inb(sc->nic_addr + ED_P1_CURR)) {

		/* get pointer to this buffer header structure */
		packet_ptr = (struct ed_ring *)(sc->smem_ring +
			 (sc->next_packet - sc->rec_page_start) * ED_PAGE_SIZE);

		/*
		 * The byte count includes the FCS - Frame Check Sequence (a
		 *	32 bit CRC).
		 */
		len = packet_ptr->count;
		if ((len >= ETHER_MIN_LEN) && (len <= ETHER_MAX_LEN)) {
			/*
			 * Go get packet. len - 4 removes CRC from length.
			 * (packet_ptr + 1) points to data just after the packet ring
			 *	header (+4 bytes)
			 */
			ed_get_packet(sc, (caddr_t)(packet_ptr + 1), len - 4);
			++sc->arpcom.ac_if.if_ipackets;
		} else {
			/*
			 * Really BAD...probably indicates that the ring pointers
			 *	are corrupted. Also seen on early rev chips under
			 *	high load - the byte order of the length gets switched.
			 */
			log(LOG_ERR,
				"ed%d: shared memory corrupt - invalid packet length %d\n",
				unit, len);
			ed_reset(unit);
			return;
		}

		/*
		 * Update next packet pointer
		 */
		sc->next_packet = packet_ptr->next_packet;

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
		outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STA);

		outb(sc->nic_addr + ED_P0_BNRY, boundry);

		/*
		 * Set NIC to page 1 registers before looping to top (prepare to
		 *	get 'CURR' current pointer)
		 */
		outb(sc->nic_addr + ED_P0_CR, ED_CR_PAGE_1|ED_CR_RD2|ED_CR_STA);
	}
}

/*
 * Ethernet interface interrupt processor
 */
int
edintr(unit)
	int unit;
{
	struct ed_softc *sc = &ed_softc[unit];
	u_char isr;

	/*
	 * Set NIC to page 0 registers
	 */
	outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STA);

	/*
	 * loop until there are no more new interrupts
	 */
	while (isr = inb(sc->nic_addr + ED_P0_ISR)) {

		/*
		 * reset all the bits that we are 'acknowleging'
		 *	by writing a '1' to each bit position that was set
		 * (writing a '1' *clears* the bit)
		 */
		outb(sc->nic_addr + ED_P0_ISR, isr);

		/*
		 * Transmit error. If a TX completed with an error, we end up
		 *	throwing the packet away. Really the only error that is
		 *	possible is excessive collisions, and in this case it is
		 *	best to allow the automatic mechanisms of TCP to backoff
		 *	the flow. Of course, with UDP we're screwed, but this is
		 *	expected when a network is heavily loaded.
		 */
		if (isr & ED_ISR_TXE) {
			u_char tsr = inb(sc->nic_addr + ED_P0_TSR);
			u_char ncr = inb(sc->nic_addr + ED_P0_NCR);

			/*
			 * Excessive collisions (16)
			 */
			if ((tsr & ED_TSR_ABT) && (ncr == 0)) {
				/*
				 *    When collisions total 16, the P0_NCR will
				 * indicate 0, and the TSR_ABT is set.
				 */
				sc->arpcom.ac_if.if_collisions += 16;
			} else
				sc->arpcom.ac_if.if_collisions += ncr;

			/*
			 * update output errors counter
			 */
			++sc->arpcom.ac_if.if_oerrors;

			/*
			 * reset tx busy and output active flags
			 */
			sc->xmit_busy = 0;
			sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;

			/*
			 * reset watchdog timer
			 */
			sc->arpcom.ac_if.if_timer = 0;
		}
				
			
		/*
		 * Receiver Error. One or more of: CRC error, frame alignment error
		 *	FIFO overrun, or missed packet.
		 */
		if (isr & ED_ISR_RXE) {
			++sc->arpcom.ac_if.if_ierrors;
		}

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
			log(LOG_WARNING,
				"ed%d: warning - receiver ring buffer overrun\n",
				unit);
			/*
			 * Stop/reset/re-init NIC
			 */
			ed_reset(unit);
		}

		/*
		 * Transmission completed normally.
		 */
		if (isr & ED_ISR_PTX) {

			/*
			 * reset tx busy and output active flags
			 */
			sc->xmit_busy = 0;
			sc->arpcom.ac_if.if_flags &= ~IFF_OACTIVE;

			/*
			 * reset watchdog timer
			 */
			sc->arpcom.ac_if.if_timer = 0;

			/*
			 * Update total number of successfully transmitted
			 *	packets.
			 */
			++sc->arpcom.ac_if.if_opackets;

			/*
			 * Add in total number of collisions on last
			 *	transmission.
			 */
			sc->arpcom.ac_if.if_collisions += inb(sc->nic_addr +
				ED_P0_TBCR0);
		}

		/*
		 * Receive Completion. Go and get the packet. 
		 *	XXX - Doing this on an error is dubious because there
		 *	   shouldn't be any data to get (we've configured the
		 *	   interface to not accept packets with errors).
		 */
		if (isr & (ED_ISR_PRX|ED_ISR_RXE)) {
			/*
			 * Enable access to shared memory on WD/SMC boards
			 */
			if (sc->memwidth == 16)
				if (sc->vendor == ED_VENDOR_WD_SMC) {
					outb(sc->asic_addr + ED_WD_LAAR,
						inb(sc->asic_addr + ED_WD_LAAR)
						| ED_WD_LAAR_M16EN);
				}

			ed_rint (unit);

			/*
			 * Disable access to shared memory
			 */
			if (sc->memwidth == 16)
				if (sc->vendor == ED_VENDOR_WD_SMC) {
					outb(sc->asic_addr + ED_WD_LAAR,
						inb(sc->asic_addr + ED_WD_LAAR)
						& ~ED_WD_LAAR_M16EN);
				}
		}

		/*
		 * If it looks like the transmitter can take more data,
		 *	attempt to start output on the interface. If data is
		 *	already buffered and ready to go, send it first.
		 */
		if ((sc->arpcom.ac_if.if_flags & IFF_OACTIVE) == 0) {
			if (sc->data_buffered)
				ed_xmit(&sc->arpcom.ac_if);
			ed_start(&sc->arpcom.ac_if);
		}

		/*
		 * return NIC CR to standard state before looping back
		 *	to top: page 0, remote DMA complete, start
		 * (toggling the TXP bit off, even if was just set in the
		 *	transmit routine, is *okay* - it is 'edge' triggered
		 *	from low to high)
		 */
		outb(sc->nic_addr + ED_P0_CR, ED_CR_RD2|ED_CR_STA);
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

	s = splnet();

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
		} else
			/*
			 * XXX - for multicasts to work, we would need to
			 *	rewrite the multicast hashing array with the
			 *	proper hash (would have been destroyed above).
			 */
			outb(sc->nic_addr + ED_P0_RCR, ED_RCR_AB);
#endif
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
	((type)( ((caddr_t)(start)+(off) >= (sc)->smem_end) ? \
		(((caddr_t)(start)+(off))) - (sc)->smem_end \
		+ (sc)->smem_ring: \
		((caddr_t)(start)+(off)) ))

/*
 * Retreive packet from shared memory and send to the next level up via
 *	ether_input(). If there is a BPF listener, give a copy to BPF, too.
 */
ed_get_packet(sc, buf, len)
	struct ed_softc *sc;
	char *buf;
	u_short len;
{
	struct ether_header *eh;
    	struct mbuf *m, *head, *ed_ring_to_mbuf();
	u_short off;
	int resid;
	u_short etype;
	struct trailer_header {
		u_short	trail_type;
		u_short trail_residual;
	} trailer_header;

	/* Allocate a header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		goto bad;
	m->m_pkthdr.rcvif = &sc->arpcom.ac_if;
	m->m_pkthdr.len = len;
	m->m_len = 0;
	head = m;

	eh = (struct ether_header *)buf;

	/* The following sillines is to make NFS happy */
#define EROUND	((sizeof(struct ether_header) + 3) & ~3)
#define EOFF	(EROUND - sizeof(struct ether_header))

	/*
	 * The following assumes there is room for
	 * the ether header in the header mbuf
	 */
	head->m_data += EOFF;
	bcopy(buf, mtod(head, caddr_t), sizeof(struct ether_header));
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

		eh->ether_type = *ringoffset(sc, buf, off, u_short *);
		resid = ntohs(*ringoffset(sc, buf, off+2, u_short *));

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
	if (src + amount > sc->smem_end) {
		tmp_amount = sc->smem_end - src;
		bcopy(src,dst,tmp_amount); /* copy amount up to end of smem */
		amount -= tmp_amount;
		src = sc->smem_ring;
		dst += tmp_amount;
	}

	bcopy(src, dst, amount);

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
			 * if there is enough data for an mbuf cluster, attempt
			 * to allocate one of those, otherwise, a regular mbuf
			 * will do.
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

