/*
 * Copyright (c) 1995, David Greenman
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
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 *   adapters. By David Greenman, 29-April-1993
 *
 * Currently supports the Western Digital/SMC 8003 and 8013 series,
 *   the SMC Elite Ultra (8216), the 3Com 3c503, the NE1000 and NE2000,
 *   and a variety of similar clones.
 *
 */

#include "opt_ed.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <sys/bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_mib.h>
#include <net/if_media.h>

#ifndef ED_NO_MIIBUS
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#endif

#include <net/bpf.h>
#include <net/bridge.h>

#include <machine/md_var.h>

#include <dev/ed/if_edreg.h>
#include <dev/ed/if_edvar.h>

devclass_t ed_devclass;

static void	ed_init		(void *);
static int	ed_ioctl	(struct ifnet *, u_long, caddr_t);
static void	ed_start	(struct ifnet *);
static void	ed_reset	(struct ifnet *);
static void	ed_watchdog	(struct ifnet *);
#ifndef ED_NO_MIIBUS
static void	ed_tick		(void *);
#endif

static void	ds_getmcaf	(struct ed_softc *, u_int32_t *);

static void	ed_get_packet	(struct ed_softc *, char *, /* u_short */ int);

static __inline void	ed_rint	(struct ed_softc *);
static __inline void	ed_xmit	(struct ed_softc *);
static __inline char *	ed_ring_copy(struct ed_softc *, char *, char *,
					  /* u_short */ int);
static void	ed_hpp_set_physical_link(struct ed_softc *);
static void	ed_hpp_readmem	(struct ed_softc *, int, unsigned char *,
				    /* u_short */ int);
static void	ed_hpp_writemem	(struct ed_softc *, unsigned char *,
				    /* u_short */ int, /* u_short */ int);
static u_short	ed_hpp_write_mbufs(struct ed_softc *, struct mbuf *, int);

static u_short	ed_pio_write_mbufs(struct ed_softc *, struct mbuf *, int);

static void	ed_setrcr	(struct ed_softc *);

static u_int32_t ds_crc		(u_char *ep);

/*
 * Interrupt conversion table for WD/SMC ASIC/83C584
 */
static unsigned short ed_intr_val[] = {
	9,
	3,
	5,
	7,
	10,
	11,
	15,
	4
};

/*
 * Interrupt conversion table for 83C790
 */
static unsigned short ed_790_intr_val[] = {
	0,
	9,
	3,
	5,
	7,
	10,
	11,
	15
};

/*
 * Interrupt conversion table for the HP PC LAN+
 */

static unsigned short ed_hpp_intr_val[] = {
	0,		/* 0 */
	0,		/* 1 */
	0,		/* 2 */
	3,		/* 3 */
	4,		/* 4 */
	5,		/* 5 */
	6,		/* 6 */
	7,		/* 7 */
	0,		/* 8 */
	9,		/* 9 */
	10,		/* 10 */
	11,		/* 11 */
	12,		/* 12 */
	0,		/* 13 */
	0,		/* 14 */
	15		/* 15 */
};

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
	if ((ed_nic_inb(sc, ED_P0_CR) &
	     (ED_CR_RD2 | ED_CR_TXP | ED_CR_STA | ED_CR_STP)) !=
	    (ED_CR_RD2 | ED_CR_STP))
		return (0);
	if ((ed_nic_inb(sc, ED_P0_ISR) & ED_ISR_RST) != ED_ISR_RST)
		return (0);

	return (1);
}

/*
 * Probe and vendor-specific initialization routine for SMC/WD80x3 boards
 */
int
ed_probe_WD80x3_generic(dev, flags, intr_vals)
	device_t dev;
	int flags;
	unsigned short *intr_vals[];
{
	struct ed_softc *sc = device_get_softc(dev);
	int	error;
	int     i;
	u_int   memsize, maddr;
	u_char  iptr, isa16bit, sum, totalsum;
	u_long	conf_maddr, conf_msize, irq, junk;

	sc->chip_type = ED_CHIP_TYPE_DP8390;

	if (ED_FLAGS_GETTYPE(flags) == ED_FLAGS_TOSH_ETHER) {
		totalsum = ED_WD_ROM_CHECKSUM_TOTAL_TOSH_ETHER;
		ed_asic_outb(sc, ED_WD_MSR, ED_WD_MSR_POW);
		DELAY(10000);
	}
	else
		totalsum = ED_WD_ROM_CHECKSUM_TOTAL;

	/*
	 * Attempt to do a checksum over the station address PROM. If it
	 * fails, it's probably not a SMC/WD board. There is a problem with
	 * this, though: some clone WD boards don't pass the checksum test.
	 * Danpex boards for one.
	 */
	for (sum = 0, i = 0; i < 8; ++i)
		sum += ed_asic_inb(sc, ED_WD_PROM + i);

	if (sum != totalsum) {

		/*
		 * Checksum is invalid. This often happens with cheap WD8003E
		 * clones.  In this case, the checksum byte (the eighth byte)
		 * seems to always be zero.
		 */
		if (ed_asic_inb(sc, ED_WD_CARD_ID) != ED_TYPE_WD8003E ||
		    ed_asic_inb(sc, ED_WD_PROM + 7) != 0)
			return (ENXIO);
	}
	/* reset card to force it into a known state. */
	if (ED_FLAGS_GETTYPE(flags) == ED_FLAGS_TOSH_ETHER)
		ed_asic_outb(sc, ED_WD_MSR, ED_WD_MSR_RST | ED_WD_MSR_POW);
	else
		ed_asic_outb(sc, ED_WD_MSR, ED_WD_MSR_RST);

	DELAY(100);
	ed_asic_outb(sc, ED_WD_MSR, ed_asic_inb(sc, ED_WD_MSR) & ~ED_WD_MSR_RST);
	/* wait in the case this card is reading its EEROM */
	DELAY(5000);

	sc->vendor = ED_VENDOR_WD_SMC;
	sc->type = ed_asic_inb(sc, ED_WD_CARD_ID);

	/*
	 * Set initial values for width/size.
	 */
	memsize = 8192;
	isa16bit = 0;
	switch (sc->type) {
	case ED_TYPE_WD8003S:
		sc->type_str = "WD8003S";
		break;
	case ED_TYPE_WD8003E:
		sc->type_str = "WD8003E";
		break;
	case ED_TYPE_WD8003EB:
		sc->type_str = "WD8003EB";
		break;
	case ED_TYPE_WD8003W:
		sc->type_str = "WD8003W";
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
	case ED_TYPE_WD8013EP:	/* also WD8003EP */
		if (ed_asic_inb(sc, ED_WD_ICR) & ED_WD_ICR_16BIT) {
			isa16bit = 1;
			memsize = 16384;
			sc->type_str = "WD8013EP";
		} else {
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
	case ED_TYPE_SMC8216C: /* 8216 has 16K shared mem -- 8416 has 8K */
	case ED_TYPE_SMC8216T:
		if (sc->type == ED_TYPE_SMC8216C) {
			sc->type_str = "SMC8216/SMC8216C";
		} else {
			sc->type_str = "SMC8216T";
		}

		ed_asic_outb(sc, ED_WD790_HWR,
		    ed_asic_inb(sc, ED_WD790_HWR) | ED_WD790_HWR_SWH);
		switch (ed_asic_inb(sc, ED_WD790_RAR) & ED_WD790_RAR_SZ64) {
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
			/* 8216 has 16K shared mem -- 8416 has 8K */
			if (sc->type == ED_TYPE_SMC8216C) {
				sc->type_str = "SMC8416C/SMC8416BT";
			} else {
				sc->type_str = "SMC8416T";
			}
			memsize = 8192;
			break;
		}
		ed_asic_outb(sc, ED_WD790_HWR,
		    ed_asic_inb(sc, ED_WD790_HWR) & ~ED_WD790_HWR_SWH);

		isa16bit = 1;
		sc->chip_type = ED_CHIP_TYPE_WD790;
		break;
	case ED_TYPE_TOSHIBA1:
		sc->type_str = "Toshiba1";
		memsize = 32768;
		isa16bit = 1;
		break;
	case ED_TYPE_TOSHIBA4:
		sc->type_str = "Toshiba4";
		memsize = 32768;
		isa16bit = 1;
		break;
	default:
		sc->type_str = "";
		break;
	}

	/*
	 * Make some adjustments to initial values depending on what is found
	 * in the ICR.
	 */
	if (isa16bit && (sc->type != ED_TYPE_WD8013EBT)
	  && (sc->type != ED_TYPE_TOSHIBA1) && (sc->type != ED_TYPE_TOSHIBA4)
	    && ((ed_asic_inb(sc, ED_WD_ICR) & ED_WD_ICR_16BIT) == 0)) {
		isa16bit = 0;
		memsize = 8192;
	}

	error = bus_get_resource(dev, SYS_RES_MEMORY, 0,
				 &conf_maddr, &conf_msize);
	if (error)
		return (error);

#if ED_DEBUG
	printf("type = %x type_str=%s isa16bit=%d memsize=%d id_msize=%d\n",
	       sc->type, sc->type_str, isa16bit, memsize, conf_msize);
	for (i = 0; i < 8; i++)
		printf("%x -> %x\n", i, ed_asic_inb(sc, i));
#endif

	/*
	 * Allow the user to override the autoconfiguration
	 */
	if (conf_msize > 1)
		memsize = conf_msize;

	maddr = conf_maddr;
	if (maddr < 0xa0000 || maddr + memsize > 0x1000000) {
		device_printf(dev, "Invalid ISA memory address range configured: 0x%x - 0x%x\n",
			      maddr, maddr + memsize);
		return (ENXIO);
	}

	/*
	 * (note that if the user specifies both of the following flags that
	 * '8bit' mode intentionally has precedence)
	 */
	if (flags & ED_FLAGS_FORCE_16BIT_MODE)
		isa16bit = 1;
	if (flags & ED_FLAGS_FORCE_8BIT_MODE)
		isa16bit = 0;

	/*
	 * If possible, get the assigned interrupt number from the card and
	 * use it.
	 */
	if ((sc->type & ED_WD_SOFTCONFIG) &&
	    (sc->chip_type != ED_CHIP_TYPE_WD790)) {

		/*
		 * Assemble together the encoded interrupt number.
		 */
		iptr = (ed_asic_inb(sc, ED_WD_ICR) & ED_WD_ICR_IR2) |
		    ((ed_asic_inb(sc, ED_WD_IRR) &
		      (ED_WD_IRR_IR0 | ED_WD_IRR_IR1)) >> 5);

		/*
		 * If no interrupt specified (or "?"), use what the board tells us.
		 */
		error = bus_get_resource(dev, SYS_RES_IRQ, 0,
					 &irq, &junk);
		if (error && intr_vals[0] != NULL) {
			error = bus_set_resource(dev, SYS_RES_IRQ, 0,
						 intr_vals[0][iptr], 1);
		}
		if (error)
			return (error);

		/*
		 * Enable the interrupt.
		 */
		ed_asic_outb(sc, ED_WD_IRR,
		     ed_asic_inb(sc, ED_WD_IRR) | ED_WD_IRR_IEN);
	}
	if (sc->chip_type == ED_CHIP_TYPE_WD790) {
		ed_asic_outb(sc, ED_WD790_HWR,
		  ed_asic_inb(sc, ED_WD790_HWR) | ED_WD790_HWR_SWH);
		iptr = (((ed_asic_inb(sc, ED_WD790_GCR) & ED_WD790_GCR_IR2) >> 4) |
			(ed_asic_inb(sc, ED_WD790_GCR) &
			 (ED_WD790_GCR_IR1 | ED_WD790_GCR_IR0)) >> 2);
		ed_asic_outb(sc, ED_WD790_HWR,
		 ed_asic_inb(sc, ED_WD790_HWR) & ~ED_WD790_HWR_SWH);

		/*
		 * If no interrupt specified (or "?"), use what the board tells us.
		 */
		error = bus_get_resource(dev, SYS_RES_IRQ, 0,
					 &irq, &junk);
		if (error && intr_vals[1] != NULL) {
			error = bus_set_resource(dev, SYS_RES_IRQ, 0,
						 intr_vals[1][iptr], 1);
		}
		if (error)
			return (error);

		/*
		 * Enable interrupts.
		 */
		ed_asic_outb(sc, ED_WD790_ICR,
		  ed_asic_inb(sc, ED_WD790_ICR) | ED_WD790_ICR_EIL);
	}
	error = bus_get_resource(dev, SYS_RES_IRQ, 0,
				 &irq, &junk);
	if (error) {
		device_printf(dev, "%s cards don't support auto-detected/assigned interrupts.\n",
			      sc->type_str);
		return (ENXIO);
	}
	sc->isa16bit = isa16bit;
	sc->mem_shared = 1;

	error = ed_alloc_memory(dev, 0, memsize);
	if (error) {
		printf("*** ed_alloc_memory() failed! (%d)\n", error);
		return (error);
	}
	sc->mem_start = (caddr_t) rman_get_virtual(sc->mem_res);

	/*
	 * allocate one xmit buffer if < 16k, two buffers otherwise
	 */
	if ((memsize < 16384) ||
            (flags & ED_FLAGS_NO_MULTI_BUFFERING)) {
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
		sc->arpcom.ac_enaddr[i] = ed_asic_inb(sc, ED_WD_PROM + i);

	/*
	 * Set upper address bits and 8/16 bit access to shared memory.
	 */
	if (isa16bit) {
		if (sc->chip_type == ED_CHIP_TYPE_WD790) {
			sc->wd_laar_proto = ed_asic_inb(sc, ED_WD_LAAR);
		} else {
			sc->wd_laar_proto = ED_WD_LAAR_L16EN |
			    ((kvtop(sc->mem_start) >> 19) & ED_WD_LAAR_ADDRHI);
		}
		/*
		 * Enable 16bit access
		 */
		ed_asic_outb(sc, ED_WD_LAAR, sc->wd_laar_proto |
		    ED_WD_LAAR_M16EN);
	} else {
		if (((sc->type & ED_WD_SOFTCONFIG) ||
		     (sc->type == ED_TYPE_TOSHIBA1) ||
		     (sc->type == ED_TYPE_TOSHIBA4) ||
		     (sc->type == ED_TYPE_WD8013EBT)) &&
		    (sc->chip_type != ED_CHIP_TYPE_WD790)) {
			sc->wd_laar_proto = (kvtop(sc->mem_start) >> 19) &
			    ED_WD_LAAR_ADDRHI;
			ed_asic_outb(sc, ED_WD_LAAR, sc->wd_laar_proto);
		}
	}

	/*
	 * Set address and enable interface shared memory.
	 */
	if (sc->chip_type != ED_CHIP_TYPE_WD790) {
		if (ED_FLAGS_GETTYPE(flags) == ED_FLAGS_TOSH_ETHER) {
			ed_asic_outb(sc, ED_WD_MSR + 1,
				     ((kvtop(sc->mem_start) >> 8) & 0xe0) | 4);
			ed_asic_outb(sc, ED_WD_MSR + 2,
				     ((kvtop(sc->mem_start) >> 16) & 0x0f));
			ed_asic_outb(sc, ED_WD_MSR,
				     ED_WD_MSR_MENB | ED_WD_MSR_POW);
		} else {
			ed_asic_outb(sc, ED_WD_MSR,
				     ((kvtop(sc->mem_start) >> 13) &
				      ED_WD_MSR_ADDR) | ED_WD_MSR_MENB);
		}
		sc->cr_proto = ED_CR_RD2;
	} else {
		ed_asic_outb(sc, ED_WD_MSR, ED_WD_MSR_MENB);
		ed_asic_outb(sc, ED_WD790_HWR, (ed_asic_inb(sc, ED_WD790_HWR) | ED_WD790_HWR_SWH));
		ed_asic_outb(sc, ED_WD790_RAR, ((kvtop(sc->mem_start) >> 13) & 0x0f) |
		     ((kvtop(sc->mem_start) >> 11) & 0x40) |
		     (ed_asic_inb(sc, ED_WD790_RAR) & 0xb0));
		ed_asic_outb(sc, ED_WD790_HWR, (ed_asic_inb(sc, ED_WD790_HWR) & ~ED_WD790_HWR_SWH));
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
			device_printf(dev, "failed to clear shared memory at %jx - check configuration\n",
				      (uintmax_t)kvtop(sc->mem_start + i));

			/*
			 * Disable 16 bit access to shared memory
			 */
			if (isa16bit) {
				if (sc->chip_type == ED_CHIP_TYPE_WD790) {
					ed_asic_outb(sc, ED_WD_MSR, 0x00);
				}
				ed_asic_outb(sc, ED_WD_LAAR, sc->wd_laar_proto &
				    ~ED_WD_LAAR_M16EN);
			}
			return (ENXIO);
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
		if (sc->chip_type == ED_CHIP_TYPE_WD790) {
			ed_asic_outb(sc, ED_WD_MSR, 0x00);
		}
		ed_asic_outb(sc, ED_WD_LAAR, sc->wd_laar_proto &
		    ~ED_WD_LAAR_M16EN);
	}
	return (0);
}

int
ed_probe_WD80x3(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int	error;
	static unsigned short *intr_vals[] = {ed_intr_val, ed_790_intr_val};

	error = ed_alloc_port(dev, port_rid, ED_WD_IO_PORTS);
	if (error)
		return (error);

	sc->asic_offset = ED_WD_ASIC_OFFSET;
	sc->nic_offset  = ED_WD_NIC_OFFSET;

	return ed_probe_WD80x3_generic(dev, flags, intr_vals);
}

/*
 * Probe and vendor-specific initialization routine for 3Com 3c503 boards
 */
int
ed_probe_3Com(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int	error;
	int     i;
	u_int   memsize;
	u_char  isa16bit;
	u_long	conf_maddr, conf_msize, irq, junk;

	error = ed_alloc_port(dev, 0, ED_3COM_IO_PORTS);
	if (error)
		return (error);

	sc->asic_offset = ED_3COM_ASIC_OFFSET;
	sc->nic_offset  = ED_3COM_NIC_OFFSET;

	/*
	 * Verify that the kernel configured I/O address matches the board
	 * configured address
	 */
	switch (ed_asic_inb(sc, ED_3COM_BCFR)) {
	case ED_3COM_BCFR_300:
		if (rman_get_start(sc->port_res) != 0x300)
			return (ENXIO);
		break;
	case ED_3COM_BCFR_310:
		if (rman_get_start(sc->port_res) != 0x310)
			return (ENXIO);
		break;
	case ED_3COM_BCFR_330:
		if (rman_get_start(sc->port_res) != 0x330)
			return (ENXIO);
		break;
	case ED_3COM_BCFR_350:
		if (rman_get_start(sc->port_res) != 0x350)
			return (ENXIO);
		break;
	case ED_3COM_BCFR_250:
		if (rman_get_start(sc->port_res) != 0x250)
			return (ENXIO);
		break;
	case ED_3COM_BCFR_280:
		if (rman_get_start(sc->port_res) != 0x280)
			return (ENXIO);
		break;
	case ED_3COM_BCFR_2A0:
		if (rman_get_start(sc->port_res) != 0x2a0)
			return (ENXIO);
		break;
	case ED_3COM_BCFR_2E0:
		if (rman_get_start(sc->port_res) != 0x2e0)
			return (ENXIO);
		break;
	default:
		return (ENXIO);
	}

	error = bus_get_resource(dev, SYS_RES_MEMORY, 0,
				 &conf_maddr, &conf_msize);
	if (error)
		return (error);

	/*
	 * Verify that the kernel shared memory address matches the board
	 * configured address.
	 */
	switch (ed_asic_inb(sc, ED_3COM_PCFR)) {
	case ED_3COM_PCFR_DC000:
		if (conf_maddr != 0xdc000)
			return (ENXIO);
		break;
	case ED_3COM_PCFR_D8000:
		if (conf_maddr != 0xd8000)
			return (ENXIO);
		break;
	case ED_3COM_PCFR_CC000:
		if (conf_maddr != 0xcc000)
			return (ENXIO);
		break;
	case ED_3COM_PCFR_C8000:
		if (conf_maddr != 0xc8000)
			return (ENXIO);
		break;
	default:
		return (ENXIO);
	}


	/*
	 * Reset NIC and ASIC. Enable on-board transceiver throughout reset
	 * sequence because it'll lock up if the cable isn't connected if we
	 * don't.
	 */
	ed_asic_outb(sc, ED_3COM_CR, ED_3COM_CR_RST | ED_3COM_CR_XSEL);

	/*
	 * Wait for a while, then un-reset it
	 */
	DELAY(50);

	/*
	 * The 3Com ASIC defaults to rather strange settings for the CR after
	 * a reset - it's important to set it again after the following outb
	 * (this is done when we map the PROM below).
	 */
	ed_asic_outb(sc, ED_3COM_CR, ED_3COM_CR_XSEL);

	/*
	 * Wait a bit for the NIC to recover from the reset
	 */
	DELAY(5000);

	sc->vendor = ED_VENDOR_3COM;
	sc->type_str = "3c503";
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
	ed_asic_outb(sc, ED_3COM_CR, ED_3COM_CR_EALO | ED_3COM_CR_XSEL);

	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		sc->arpcom.ac_enaddr[i] = ed_nic_inb(sc, i);

	/*
	 * Unmap PROM - select NIC registers. The proper setting of the
	 * tranceiver is set in ed_init so that the attach code is given a
	 * chance to set the default based on a compile-time config option
	 */
	ed_asic_outb(sc, ED_3COM_CR, ED_3COM_CR_XSEL);

	/*
	 * Determine if this is an 8bit or 16bit board
	 */

	/*
	 * select page 0 registers
	 */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STP);

	/*
	 * Attempt to clear WTS bit. If it doesn't clear, then this is a 16bit
	 * board.
	 */
	ed_nic_outb(sc, ED_P0_DCR, 0);

	/*
	 * select page 2 registers
	 */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_PAGE_2 | ED_CR_RD2 | ED_CR_STP);

	/*
	 * The 3c503 forces the WTS bit to a one if this is a 16bit board
	 */
	if (ed_nic_inb(sc, ED_P2_DCR) & ED_DCR_WTS)
		isa16bit = 1;
	else
		isa16bit = 0;

	/*
	 * select page 0 registers
	 */
	ed_nic_outb(sc, ED_P2_CR, ED_CR_RD2 | ED_CR_STP);

	error = ed_alloc_memory(dev, 0, memsize);
	if (error)
		return (error);

	sc->mem_start = (caddr_t) rman_get_virtual(sc->mem_res);
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
		if (flags & ED_FLAGS_NO_MULTI_BUFFERING)
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
	ed_asic_outb(sc, ED_3COM_PSTR, sc->rec_page_start);
	ed_asic_outb(sc, ED_3COM_PSPR, sc->rec_page_stop);

	/*
	 * Set IRQ. 3c503 only allows a choice of irq 2-5.
	 */
	error = bus_get_resource(dev, SYS_RES_IRQ, 0, &irq, &junk);
	if (error)
		return (error);

	switch (irq) {
	case 2:
	case 9:
		ed_asic_outb(sc, ED_3COM_IDCFR, ED_3COM_IDCFR_IRQ2);
		break;
	case 3:
		ed_asic_outb(sc, ED_3COM_IDCFR, ED_3COM_IDCFR_IRQ3);
		break;
	case 4:
		ed_asic_outb(sc, ED_3COM_IDCFR, ED_3COM_IDCFR_IRQ4);
		break;
	case 5:
		ed_asic_outb(sc, ED_3COM_IDCFR, ED_3COM_IDCFR_IRQ5);
		break;
	default:
		device_printf(dev, "Invalid irq configuration (%ld) must be 3-5,9 for 3c503\n",
			      irq);
		return (ENXIO);
	}

	/*
	 * Initialize GA configuration register. Set bank and enable shared
	 * mem.
	 */
	ed_asic_outb(sc, ED_3COM_GACFR, ED_3COM_GACFR_RSEL |
	     ED_3COM_GACFR_MBS0);

	/*
	 * Initialize "Vector Pointer" registers. These gawd-awful things are
	 * compared to 20 bits of the address on ISA, and if they match, the
	 * shared memory is disabled. We set them to 0xffff0...allegedly the
	 * reset vector.
	 */
	ed_asic_outb(sc, ED_3COM_VPTR2, 0xff);
	ed_asic_outb(sc, ED_3COM_VPTR1, 0xff);
	ed_asic_outb(sc, ED_3COM_VPTR0, 0x00);

	/*
	 * Zero memory and verify that it is clear
	 */
	bzero(sc->mem_start, memsize);

	for (i = 0; i < memsize; ++i)
		if (sc->mem_start[i]) {
			device_printf(dev, "failed to clear shared memory at %jx - check configuration\n",
				      (uintmax_t)kvtop(sc->mem_start + i));
			return (ENXIO);
		}
	return (0);
}

/*
 * Probe and vendor-specific initialization routine for SIC boards
 */
int
ed_probe_SIC(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int	error;
	int	i;
	u_int	memsize;
	u_long	conf_maddr, conf_msize;
	u_char	sum;

	error = ed_alloc_port(dev, 0, ED_SIC_IO_PORTS);
	if (error)
		return (error);

	sc->asic_offset = ED_SIC_ASIC_OFFSET;
	sc->nic_offset  = ED_SIC_NIC_OFFSET;

	error = bus_get_resource(dev, SYS_RES_MEMORY, 0,
				 &conf_maddr, &conf_msize);
	if (error)
		return (error);

	memsize = 16384;
	if (conf_msize > 1)
		memsize = conf_msize;

	error = ed_alloc_memory(dev, 0, memsize);
	if (error)
		return (error);

	sc->mem_start = (caddr_t) rman_get_virtual(sc->mem_res);
	sc->mem_size  = memsize;

	/* Reset card to force it into a known state. */
	ed_asic_outb(sc, 0, 0x00);
	DELAY(100);

	/*
	 * Here we check the card ROM, if the checksum passes, and the
	 * type code and ethernet address check out, then we know we have
	 * an SIC card.
	 */
	ed_asic_outb(sc, 0, 0x81);
	DELAY(100);

	sum = sc->mem_start[6];
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		sum ^= (sc->arpcom.ac_enaddr[i] = sc->mem_start[i]);
	}
#ifdef ED_DEBUG
	device_printf(dev, "ed_probe_sic: got address %6D\n",
		      sc->arpcom.ac_enaddr, ":");
#endif
	if (sum != 0) {
		return (ENXIO);
	}
	if ((sc->arpcom.ac_enaddr[0] | sc->arpcom.ac_enaddr[1] |
	     sc->arpcom.ac_enaddr[2]) == 0) {
		return (ENXIO);
	}

	sc->vendor   = ED_VENDOR_SIC;
	sc->type_str = "SIC";
	sc->isa16bit = 0;
	sc->cr_proto = 0;

	/*
	 * SIC RAM page 0x0000-0x3fff(or 0x7fff)
	 */
	ed_asic_outb(sc, 0, 0x80);
	DELAY(100);

	/*
	 * Now zero memory and verify that it is clear
	 */
	bzero(sc->mem_start, sc->mem_size);

	for (i = 0; i < sc->mem_size; i++) {
		if (sc->mem_start[i]) {
			device_printf(dev, "failed to clear shared memory "
				"at %jx - check configuration\n",
				(uintmax_t)kvtop(sc->mem_start + i));

			return (ENXIO);
		}
	}

	sc->mem_shared = 1;
	sc->mem_end = sc->mem_start + sc->mem_size;

	/*
	 * allocate one xmit buffer if < 16k, two buffers otherwise
	 */
	if ((sc->mem_size < 16384) || (flags & ED_FLAGS_NO_MULTI_BUFFERING)) {
		sc->txb_cnt = 1;
	} else {
		sc->txb_cnt = 2;
	}
	sc->tx_page_start = 0;

	sc->rec_page_start = sc->tx_page_start + ED_TXBUF_SIZE * sc->txb_cnt;
	sc->rec_page_stop = sc->tx_page_start + sc->mem_size / ED_PAGE_SIZE;

	sc->mem_ring = sc->mem_start + sc->txb_cnt * ED_PAGE_SIZE * ED_TXBUF_SIZE;

	return (0);
}

/*
 * Probe and vendor-specific initialization routine for NE1000/2000 boards
 */
int
ed_probe_Novell_generic(dev, flags)
	device_t dev;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	u_int   memsize, n;
	u_char  romdata[16], tmp;
	static char test_pattern[32] = "THIS is A memory TEST pattern";
	char    test_buffer[32];

	/* XXX - do Novell-specific probe here */

	/* Reset the board */
	if (ED_FLAGS_GETTYPE(flags) == ED_FLAGS_GWETHER) {
		ed_asic_outb(sc, ED_NOVELL_RESET, 0);
		DELAY(200);
	}
	tmp = ed_asic_inb(sc, ED_NOVELL_RESET);

	/*
	 * I don't know if this is necessary; probably cruft leftover from
	 * Clarkson packet driver code. Doesn't do a thing on the boards I've
	 * tested. -DG [note that an outb(0x84, 0) seems to work here, and is
	 * non-invasive...but some boards don't seem to reset and I don't have
	 * complete documentation on what the 'right' thing to do is...so we
	 * do the invasive thing for now. Yuck.]
	 */
	ed_asic_outb(sc, ED_NOVELL_RESET, tmp);
	DELAY(5000);

	/*
	 * This is needed because some NE clones apparently don't reset the
	 * NIC properly (or the NIC chip doesn't reset fully on power-up) XXX
	 * - this makes the probe invasive! ...Done against my better
	 * judgement. -DLG
	 */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STP);

	DELAY(5000);

	/* Make sure that we really have an 8390 based board */
	if (!ed_probe_generic8390(sc))
		return (ENXIO);

	sc->vendor = ED_VENDOR_NOVELL;
	sc->mem_shared = 0;
	sc->cr_proto = ED_CR_RD2;

	/*
	 * Test the ability to read and write to the NIC memory. This has the
	 * side affect of determining if this is an NE1000 or an NE2000.
	 */

	/*
	 * This prevents packets from being stored in the NIC memory when the
	 * readmem routine turns on the start bit in the CR.
	 */
	ed_nic_outb(sc, ED_P0_RCR, ED_RCR_MON);

	/* Temporarily initialize DCR for byte operations */
	ed_nic_outb(sc, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);

	ed_nic_outb(sc, ED_P0_PSTART, 8192 / ED_PAGE_SIZE);
	ed_nic_outb(sc, ED_P0_PSTOP, 16384 / ED_PAGE_SIZE);

	sc->isa16bit = 0;

	/*
	 * Write a test pattern in byte mode. If this fails, then there
	 * probably isn't any memory at 8k - which likely means that the board
	 * is an NE2000.
	 */
	ed_pio_writemem(sc, test_pattern, 8192, sizeof(test_pattern));
	ed_pio_readmem(sc, 8192, test_buffer, sizeof(test_pattern));

	if (bcmp(test_pattern, test_buffer, sizeof(test_pattern)) == 0) {
		sc->type = ED_TYPE_NE1000;
		sc->type_str = "NE1000";
	} else {

		/* neither an NE1000 nor a Linksys - try NE2000 */
		ed_nic_outb(sc, ED_P0_DCR, ED_DCR_WTS | ED_DCR_FT1 | ED_DCR_LS);
		ed_nic_outb(sc, ED_P0_PSTART, 16384 / ED_PAGE_SIZE);
		ed_nic_outb(sc, ED_P0_PSTOP, 32768 / ED_PAGE_SIZE);

		sc->isa16bit = 1;

		/*
		 * Write a test pattern in word mode. If this also fails, then
		 * we don't know what this board is.
		 */
		ed_pio_writemem(sc, test_pattern, 16384, sizeof(test_pattern));
		ed_pio_readmem(sc, 16384, test_buffer, sizeof(test_pattern));
		if (bcmp(test_pattern, test_buffer, sizeof(test_pattern)) == 0) {
			sc->type = ED_TYPE_NE2000;
			sc->type_str = "NE2000";
		} else {
			return (ENXIO);
		}
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

	if (ED_FLAGS_GETTYPE(flags) == ED_FLAGS_GWETHER) {
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
			device_printf(dev, "Cannot find start of RAM.\n");
			return (ENXIO);
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
			device_printf(dev, "Cannot find any RAM, start : %d, x = %d.\n", mstart, x);
			return (ENXIO);
		}
		device_printf(dev, "RAM start at %d, size : %d.\n", mstart, msize);

		sc->mem_size = msize;
		sc->mem_start = (caddr_t) mstart;
		sc->mem_end = (caddr_t) (msize + mstart);
		sc->tx_page_start = mstart / ED_PAGE_SIZE;
	}

	/*
	 * Use one xmit buffer if < 16k, two buffers otherwise (if not told
	 * otherwise).
	 */
	if ((memsize < 16384) || (flags & ED_FLAGS_NO_MULTI_BUFFERING))
		sc->txb_cnt = 1;
	else
		sc->txb_cnt = 2;

	sc->rec_page_start = sc->tx_page_start + sc->txb_cnt * ED_TXBUF_SIZE;
	sc->rec_page_stop = sc->tx_page_start + memsize / ED_PAGE_SIZE;

	sc->mem_ring = sc->mem_start + sc->txb_cnt * ED_PAGE_SIZE * ED_TXBUF_SIZE;

	ed_pio_readmem(sc, 0, romdata, 16);
	for (n = 0; n < ETHER_ADDR_LEN; n++)
		sc->arpcom.ac_enaddr[n] = romdata[n * (sc->isa16bit + 1)];

	if ((ED_FLAGS_GETTYPE(flags) == ED_FLAGS_GWETHER) &&
	    (sc->arpcom.ac_enaddr[2] == 0x86)) {
		sc->type_str = "Gateway AT";
	}

	/* clear any pending interrupts that might have occurred above */
	ed_nic_outb(sc, ED_P0_ISR, 0xff);

	return (0);
}

int
ed_probe_Novell(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int	error;

	error = ed_alloc_port(dev, port_rid, ED_NOVELL_IO_PORTS);
	if (error)
		return (error);

	sc->asic_offset = ED_NOVELL_ASIC_OFFSET;
	sc->nic_offset  = ED_NOVELL_NIC_OFFSET;

	return ed_probe_Novell_generic(dev, flags);
}

#define	ED_HPP_TEST_SIZE	16

/*
 * Probe and vendor specific initialization for the HP PC Lan+ Cards.
 * (HP Part nos: 27247B and 27252A).
 *
 * The card has an asic wrapper around a DS8390 core.  The asic handles 
 * host accesses and offers both standard register IO and memory mapped 
 * IO.  Memory mapped I/O allows better performance at the expense of greater
 * chance of an incompatibility with existing ISA cards.
 *
 * The card has a few caveats: it isn't tolerant of byte wide accesses, only
 * short (16 bit) or word (32 bit) accesses are allowed.  Some card revisions
 * don't allow 32 bit accesses; these are indicated by a bit in the software
 * ID register (see if_edreg.h).
 * 
 * Other caveats are: we should read the MAC address only when the card
 * is inactive.
 *
 * For more information; please consult the CRYNWR packet driver.
 *
 * The AUI port is turned on using the "link2" option on the ifconfig 
 * command line.
 */
int
ed_probe_HP_pclanp(dev, port_rid, flags)
	device_t dev;
	int port_rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	int error;
	int n;				/* temp var */
	int memsize;			/* mem on board */
	u_char checksum;		/* checksum of board address */
	u_char irq;			/* board configured IRQ */
	char test_pattern[ED_HPP_TEST_SIZE];	/* read/write areas for */
	char test_buffer[ED_HPP_TEST_SIZE];	/* probing card */
	u_long conf_maddr, conf_msize, conf_irq, junk;

	error = ed_alloc_port(dev, 0, ED_HPP_IO_PORTS);
	if (error)
		return (error);

	/* Fill in basic information */
	sc->asic_offset = ED_HPP_ASIC_OFFSET;
	sc->nic_offset  = ED_HPP_NIC_OFFSET;

	sc->chip_type = ED_CHIP_TYPE_DP8390;
	sc->isa16bit = 0;	/* the 8390 core needs to be in byte mode */

	/* 
	 * Look for the HP PCLAN+ signature: "0x50,0x48,0x00,0x53" 
	 */
	
	if ((ed_asic_inb(sc, ED_HPP_ID) != 0x50) || 
	    (ed_asic_inb(sc, ED_HPP_ID + 1) != 0x48) ||
	    ((ed_asic_inb(sc, ED_HPP_ID + 2) & 0xF0) != 0) ||
	    (ed_asic_inb(sc, ED_HPP_ID + 3) != 0x53))
		return ENXIO;

	/* 
	 * Read the MAC address and verify checksum on the address.
	 */

	ed_asic_outw(sc, ED_HPP_PAGING, ED_HPP_PAGE_MAC);
	for (n  = 0, checksum = 0; n < ETHER_ADDR_LEN; n++)
		checksum += (sc->arpcom.ac_enaddr[n] = 
			ed_asic_inb(sc, ED_HPP_MAC_ADDR + n));
	
	checksum += ed_asic_inb(sc, ED_HPP_MAC_ADDR + ETHER_ADDR_LEN);

	if (checksum != 0xFF)
		return ENXIO;

	/*
	 * Verify that the software model number is 0.
	 */
	
	ed_asic_outw(sc, ED_HPP_PAGING, ED_HPP_PAGE_ID);
	if (((sc->hpp_id = ed_asic_inw(sc, ED_HPP_PAGE_4)) & 
		ED_HPP_ID_SOFT_MODEL_MASK) != 0x0000)
		return ENXIO;

	/*
	 * Read in and save the current options configured on card.
	 */

	sc->hpp_options = ed_asic_inw(sc, ED_HPP_OPTION);

	sc->hpp_options |= (ED_HPP_OPTION_NIC_RESET | 
                        	ED_HPP_OPTION_CHIP_RESET |
				ED_HPP_OPTION_ENABLE_IRQ);

	/* 
	 * Reset the chip.  This requires writing to the option register
	 * so take care to preserve the other bits.
	 */

	ed_asic_outw(sc, ED_HPP_OPTION, 
		(sc->hpp_options & ~(ED_HPP_OPTION_NIC_RESET | 
			ED_HPP_OPTION_CHIP_RESET)));

	DELAY(5000);	/* wait for chip reset to complete */

	ed_asic_outw(sc, ED_HPP_OPTION,
		(sc->hpp_options | (ED_HPP_OPTION_NIC_RESET |
			ED_HPP_OPTION_CHIP_RESET |
			ED_HPP_OPTION_ENABLE_IRQ)));

	DELAY(5000);

	if (!(ed_nic_inb(sc, ED_P0_ISR) & ED_ISR_RST))
		return ENXIO;	/* reset did not complete */

	/*
	 * Read out configuration information.
	 */

	ed_asic_outw(sc, ED_HPP_PAGING, ED_HPP_PAGE_HW);

	irq = ed_asic_inb(sc, ED_HPP_HW_IRQ);

	/*
 	 * Check for impossible IRQ.
	 */

	if (irq >= (sizeof(ed_hpp_intr_val) / sizeof(ed_hpp_intr_val[0])))
		return ENXIO;

	/* 
	 * If the kernel IRQ was specified with a '?' use the cards idea
	 * of the IRQ.  If the kernel IRQ was explicitly specified, it
 	 * should match that of the hardware.
	 */
	error = bus_get_resource(dev, SYS_RES_IRQ, 0,
				 &conf_irq, &junk);
	if (error) {
		bus_set_resource(dev, SYS_RES_IRQ, 0,
				 ed_hpp_intr_val[irq], 1);
	} else {
		if (conf_irq != ed_hpp_intr_val[irq])
			return (ENXIO);
	}

	/*
	 * Fill in softconfig info.
	 */

	sc->vendor = ED_VENDOR_HP;
	sc->type = ED_TYPE_HP_PCLANPLUS;
	sc->type_str = "HP-PCLAN+";

	sc->mem_shared = 0;	/* we DON'T have dual ported RAM */
	sc->mem_start = 0;	/* we use offsets inside the card RAM */

	sc->hpp_mem_start = NULL;/* no memory mapped I/O by default */

	/*
	 * The board has 32KB of memory.  Is there a way to determine
	 * this programmatically?
	 */
	
	memsize = 32768;

	/*
	 * Check if memory mapping of the I/O registers possible.
	 */

	if (sc->hpp_options & ED_HPP_OPTION_MEM_ENABLE)
	{
		u_long mem_addr;

		/*
		 * determine the memory address from the board.
		 */
		
		ed_asic_outw(sc, ED_HPP_PAGING, ED_HPP_PAGE_HW);
		mem_addr = (ed_asic_inw(sc, ED_HPP_HW_MEM_MAP) << 8);

		/*
		 * Check that the kernel specified start of memory and
		 * hardware's idea of it match.
		 */
		error = bus_get_resource(dev, SYS_RES_MEMORY, 0,
					 &conf_maddr, &conf_msize);
		if (error)
			return (error);
		
		if (mem_addr != conf_maddr)
			return ENXIO;

		error = ed_alloc_memory(dev, 0, memsize);
		if (error)
			return (error);

		sc->hpp_mem_start = rman_get_virtual(sc->mem_res);
	}

	/*
	 * Fill in the rest of the soft config structure.
	 */

	/*
	 * The transmit page index.
	 */

	sc->tx_page_start = ED_HPP_TX_PAGE_OFFSET;

	if (device_get_flags(dev) & ED_FLAGS_NO_MULTI_BUFFERING)
		sc->txb_cnt = 1;
	else
		sc->txb_cnt = 2;

	/*
	 * Memory description
	 */

	sc->mem_size = memsize;
	sc->mem_ring = sc->mem_start + 
		(sc->txb_cnt * ED_PAGE_SIZE * ED_TXBUF_SIZE);
	sc->mem_end = sc->mem_start + sc->mem_size;

	/*
	 * Receive area starts after the transmit area and 
	 * continues till the end of memory.
	 */

	sc->rec_page_start = sc->tx_page_start + 
				(sc->txb_cnt * ED_TXBUF_SIZE);
	sc->rec_page_stop = (sc->mem_size / ED_PAGE_SIZE);


	sc->cr_proto = 0;	/* value works */

	/*
	 * Set the wrap registers for string I/O reads.
	 */

	ed_asic_outw(sc, ED_HPP_PAGING, ED_HPP_PAGE_HW);
	ed_asic_outw(sc, ED_HPP_HW_WRAP,
		((sc->rec_page_start / ED_PAGE_SIZE) |
		 (((sc->rec_page_stop / ED_PAGE_SIZE) - 1) << 8)));

	/*
	 * Reset the register page to normal operation.
	 */

	ed_asic_outw(sc, ED_HPP_PAGING, ED_HPP_PAGE_PERF);

	/*
	 * Verify that we can read/write from adapter memory.
	 * Create test pattern.
	 */

	for (n = 0; n < ED_HPP_TEST_SIZE; n++)
	{
		test_pattern[n] = (n*n) ^ ~n;
	}

#undef	ED_HPP_TEST_SIZE

	/*
	 * Check that the memory is accessible thru the I/O ports.
	 * Write out the contents of "test_pattern", read back
	 * into "test_buffer" and compare the two for any
	 * mismatch.
	 */

	for (n = 0; n < (32768 / ED_PAGE_SIZE); n ++) {

		ed_hpp_writemem(sc, test_pattern, (n * ED_PAGE_SIZE), 
				sizeof(test_pattern));
		ed_hpp_readmem(sc, (n * ED_PAGE_SIZE), 
			test_buffer, sizeof(test_pattern));

		if (bcmp(test_pattern, test_buffer, 
			sizeof(test_pattern)))
			return ENXIO;
	}

	return (0);

}

/*
 * HP PC Lan+ : Set the physical link to use AUI or TP/TL.
 */

static void
ed_hpp_set_physical_link(struct ed_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int lan_page;

	ed_asic_outw(sc, ED_HPP_PAGING, ED_HPP_PAGE_LAN);
	lan_page = ed_asic_inw(sc, ED_HPP_PAGE_0);

	if (ifp->if_flags & IFF_ALTPHYS) {

		/*
		 * Use the AUI port.
		 */

		lan_page |= ED_HPP_LAN_AUI;

		ed_asic_outw(sc, ED_HPP_PAGING, ED_HPP_PAGE_LAN);
		ed_asic_outw(sc, ED_HPP_PAGE_0, lan_page);


	} else {

		/*
		 * Use the ThinLan interface
		 */

		lan_page &= ~ED_HPP_LAN_AUI;

		ed_asic_outw(sc, ED_HPP_PAGING, ED_HPP_PAGE_LAN);
		ed_asic_outw(sc, ED_HPP_PAGE_0, lan_page);

	}

	/*
	 * Wait for the lan card to re-initialize itself
	 */

	DELAY(150000);	/* wait 150 ms */

	/*
	 * Restore normal pages.
	 */

	ed_asic_outw(sc, ED_HPP_PAGING, ED_HPP_PAGE_PERF);

}

/*
 * Allocate a port resource with the given resource id.
 */
int
ed_alloc_port(dev, rid, size)
	device_t dev;
	int rid;
	int size;
{
	struct ed_softc *sc = device_get_softc(dev);
	struct resource *res;

	res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				 0ul, ~0ul, size, RF_ACTIVE);
	if (res) {
		sc->port_rid = rid;
		sc->port_res = res;
		sc->port_used = size;
		return (0);
	} else {
		return (ENOENT);
	}
}

/*
 * Allocate a memory resource with the given resource id.
 */
int
ed_alloc_memory(dev, rid, size)
	device_t dev;
	int rid;
	int size;
{
	struct ed_softc *sc = device_get_softc(dev);
	struct resource *res;

	res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
				 0ul, ~0ul, size, RF_ACTIVE);
	if (res) {
		sc->mem_rid = rid;
		sc->mem_res = res;
		sc->mem_used = size;
		return (0);
	} else {
		return (ENOENT);
	}
}

/*
 * Allocate an irq resource with the given resource id.
 */
int
ed_alloc_irq(dev, rid, flags)
	device_t dev;
	int rid;
	int flags;
{
	struct ed_softc *sc = device_get_softc(dev);
	struct resource *res;

	res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
				 0ul, ~0ul, 1, (RF_ACTIVE | flags));
	if (res) {
		sc->irq_rid = rid;
		sc->irq_res = res;
		return (0);
	} else {
		return (ENOENT);
	}
}

/*
 * Release all resources
 */
void
ed_release_resources(dev)
	device_t dev;
{
	struct ed_softc *sc = device_get_softc(dev);

	if (sc->port_res) {
		bus_deactivate_resource(dev, SYS_RES_IOPORT,
				     sc->port_rid, sc->port_res);
		bus_release_resource(dev, SYS_RES_IOPORT,
				     sc->port_rid, sc->port_res);
		sc->port_res = 0;
	}
	if (sc->mem_res) {
		bus_deactivate_resource(dev, SYS_RES_MEMORY,
				     sc->mem_rid, sc->mem_res);
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->mem_rid, sc->mem_res);
		sc->mem_res = 0;
	}
	if (sc->irq_res) {
		bus_deactivate_resource(dev, SYS_RES_IRQ,
				     sc->irq_rid, sc->irq_res);
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->irq_rid, sc->irq_res);
		sc->irq_res = 0;
	}
}

/*
 * Install interface into kernel networking data structures
 */
int
ed_attach(sc, unit, flags)
	struct ed_softc *sc;
	int unit;
	int flags;
{
	struct ifnet *ifp = &sc->arpcom.ac_if;

	callout_handle_init(&sc->tick_ch);
	/*
	 * Set interface to stopped condition (reset)
	 */
	ed_stop(sc);

	/*
	 * Initialize ifnet structure
	 */
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "ed";
	ifp->if_output = ether_output;
	ifp->if_start = ed_start;
	ifp->if_ioctl = ed_ioctl;
	ifp->if_watchdog = ed_watchdog;
	ifp->if_init = ed_init;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	ifp->if_linkmib = &sc->mibdata;
	ifp->if_linkmiblen = sizeof sc->mibdata;
	/*
	 * XXX - should do a better job.
	 */
	if (sc->chip_type == ED_CHIP_TYPE_WD790)
		sc->mibdata.dot3StatsEtherChipSet =
			DOT3CHIPSET(dot3VendorWesternDigital,
				    dot3ChipSetWesternDigital83C790);
	else
		sc->mibdata.dot3StatsEtherChipSet =
			DOT3CHIPSET(dot3VendorNational, 
				    dot3ChipSetNational8390);
	sc->mibdata.dot3Compliance = DOT3COMPLIANCE_COLLS;

	/*
	 * Set default state for ALTPHYS flag (used to disable the 
	 * tranceiver for AUI operation), based on compile-time 
	 * config option.
	 */
	if (flags & ED_FLAGS_DISABLE_TRANCEIVER)
		ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX | 
		    IFF_MULTICAST | IFF_ALTPHYS);
	else
		ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX |
		    IFF_MULTICAST);

	/*
	 * Attach the interface
	 */
	ether_ifattach(ifp, sc->arpcom.ac_enaddr);
	/* device attach does transition from UNCONFIGURED to IDLE state */

	/*
	 * Print additional info when attached
	 */
	if_printf(ifp, "address %6D, ", sc->arpcom.ac_enaddr, ":");

	if (sc->type_str && (*sc->type_str != 0))
		printf("type %s ", sc->type_str);
	else
		printf("type unknown (0x%x) ", sc->type);

	if (sc->vendor == ED_VENDOR_HP)
		printf("(%s %s IO)", (sc->hpp_id & ED_HPP_ID_16_BIT_ACCESS) ?
			"16-bit" : "32-bit",
			sc->hpp_mem_start ? "memory mapped" : "regular");
	else
		printf("%s ", sc->isa16bit ? "(16 bit)" : "(8 bit)");

	printf("%s\n", (((sc->vendor == ED_VENDOR_3COM) ||
			 (sc->vendor == ED_VENDOR_HP)) &&
		(ifp->if_flags & IFF_ALTPHYS)) ? " tranceiver disabled" : "");

	return (0);
}

/*
 * Reset interface.
 */
static void
ed_reset(ifp)
	struct ifnet *ifp;
{
	struct ed_softc *sc = ifp->if_softc;
	int     s;

	if (sc->gone)
		return;
	s = splimp();

	/*
	 * Stop interface and re-initialize.
	 */
	ed_stop(sc);
	ed_init(sc);

	(void) splx(s);
}

/*
 * Take interface offline.
 */
void
ed_stop(sc)
	struct ed_softc *sc;
{
	int     n = 5000;

#ifndef ED_NO_MIIBUS
	untimeout(ed_tick, sc, sc->tick_ch);
	callout_handle_init(&sc->tick_ch);
#endif
	if (sc->gone)
		return;
	/*
	 * Stop everything on the interface, and select page 0 registers.
	 */
	ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_STP);

	/*
	 * Wait for interface to enter stopped state, but limit # of checks to
	 * 'n' (about 5ms). It shouldn't even take 5us on modern DS8390's, but
	 * just in case it's an old one.
	 */
	if (sc->chip_type != ED_CHIP_TYPE_AX88190)
		while (((ed_nic_inb(sc, ED_P0_ISR) & ED_ISR_RST) == 0) && --n);
}

/*
 * Device timeout/watchdog routine. Entered if the device neglects to
 *	generate an interrupt after a transmit has been started on it.
 */
static void
ed_watchdog(ifp)
	struct ifnet *ifp;
{
	struct ed_softc *sc = ifp->if_softc;

	if (sc->gone)
		return;
	log(LOG_ERR, "ed%d: device timeout\n", ifp->if_unit);
	ifp->if_oerrors++;

	ed_reset(ifp);
}

#ifndef ED_NO_MIIBUS
static void
ed_tick(arg)
	void *arg;
{
	struct ed_softc *sc = arg;
	struct mii_data *mii;
	int s;

	if (sc->gone) {
		callout_handle_init(&sc->tick_ch);
		return;
	}
	s = splimp();
	if (sc->miibus != NULL) {
		mii = device_get_softc(sc->miibus);
		mii_tick(mii);
	}
	sc->tick_ch = timeout(ed_tick, sc, hz);
	splx(s);
}
#endif

/*
 * Initialize device.
 */
static void
ed_init(xsc)
	void *xsc;
{
	struct ed_softc *sc = xsc;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int     i, s;

	if (sc->gone)
		return;

	/* address not known */
	if (TAILQ_EMPTY(&ifp->if_addrhead)) /* unlikely? XXX */
		return;

	/*
	 * Initialize the NIC in the exact order outlined in the NS manual.
	 * This init procedure is "mandatory"...don't change what or when
	 * things happen.
	 */
	s = splimp();

	/* reset transmitter flags */
	sc->xmit_busy = 0;
	ifp->if_timer = 0;

	sc->txb_inuse = 0;
	sc->txb_new = 0;
	sc->txb_next_tx = 0;

	/* This variable is used below - don't move this assignment */
	sc->next_packet = sc->rec_page_start + 1;

	/*
	 * Set interface for page 0, Remote DMA complete, Stopped
	 */
	ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_STP);

	if (sc->isa16bit) {

		/*
		 * Set FIFO threshold to 8, No auto-init Remote DMA, byte
		 * order=80x86, word-wide DMA xfers,
		 */
		ed_nic_outb(sc, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_WTS | ED_DCR_LS);
	} else {

		/*
		 * Same as above, but byte-wide DMA xfers
		 */
		ed_nic_outb(sc, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);
	}

	/*
	 * Clear Remote Byte Count Registers
	 */
	ed_nic_outb(sc, ED_P0_RBCR0, 0);
	ed_nic_outb(sc, ED_P0_RBCR1, 0);

	/*
	 * For the moment, don't store incoming packets in memory.
	 */
	ed_nic_outb(sc, ED_P0_RCR, ED_RCR_MON);

	/*
	 * Place NIC in internal loopback mode
	 */
	ed_nic_outb(sc, ED_P0_TCR, ED_TCR_LB0);

	/*
	 * Initialize transmit/receive (ring-buffer) Page Start
	 */
	ed_nic_outb(sc, ED_P0_TPSR, sc->tx_page_start);
	ed_nic_outb(sc, ED_P0_PSTART, sc->rec_page_start);
	/* Set lower bits of byte addressable framing to 0 */
	if (sc->chip_type == ED_CHIP_TYPE_WD790)
		ed_nic_outb(sc, 0x09, 0);

	/*
	 * Initialize Receiver (ring-buffer) Page Stop and Boundry
	 */
	ed_nic_outb(sc, ED_P0_PSTOP, sc->rec_page_stop);
	ed_nic_outb(sc, ED_P0_BNRY, sc->rec_page_start);

	/*
	 * Clear all interrupts. A '1' in each bit position clears the
	 * corresponding flag.
	 */
	ed_nic_outb(sc, ED_P0_ISR, 0xff);

	/*
	 * Enable the following interrupts: receive/transmit complete,
	 * receive/transmit error, and Receiver OverWrite.
	 *
	 * Counter overflow and Remote DMA complete are *not* enabled.
	 */
	ed_nic_outb(sc, ED_P0_IMR,
	ED_IMR_PRXE | ED_IMR_PTXE | ED_IMR_RXEE | ED_IMR_TXEE | ED_IMR_OVWE);

	/*
	 * Program Command Register for page 1
	 */
	ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STP);

	/*
	 * Copy out our station address
	 */
	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		ed_nic_outb(sc, ED_P1_PAR(i), sc->arpcom.ac_enaddr[i]);

	/*
	 * Set Current Page pointer to next_packet (initialized above)
	 */
	ed_nic_outb(sc, ED_P1_CURR, sc->next_packet);

	/*
	 * Program Receiver Configuration Register and multicast filter. CR is
	 * set to page 0 on return.
	 */
	ed_setrcr(sc);

	/*
	 * Take interface out of loopback
	 */
	ed_nic_outb(sc, ED_P0_TCR, 0);

	/*
	 * If this is a 3Com board, the tranceiver must be software enabled
	 * (there is no settable hardware default).
	 */
	if (sc->vendor == ED_VENDOR_3COM) {
		if (ifp->if_flags & IFF_ALTPHYS) {
			ed_asic_outb(sc, ED_3COM_CR, 0);
		} else {
			ed_asic_outb(sc, ED_3COM_CR, ED_3COM_CR_XSEL);
		}
	}

#ifndef ED_NO_MIIBUS
	if (sc->miibus != NULL) {
		struct mii_data *mii;
		mii = device_get_softc(sc->miibus);
		mii_mediachg(mii);
	}
#endif
	/*
	 * Set 'running' flag, and clear output active flag.
	 */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * ...and attempt to start output
	 */
	ed_start(ifp);

#ifndef ED_NO_MIIBUS
	untimeout(ed_tick, sc, sc->tick_ch);
	sc->tick_ch = timeout(ed_tick, sc, hz);
#endif
	(void) splx(s);
}

/*
 * This routine actually starts the transmission on the interface
 */
static __inline void
ed_xmit(sc)
	struct ed_softc *sc;
{
	struct ifnet *ifp = (struct ifnet *)sc;
	unsigned short len;

	if (sc->gone)
		return;
	len = sc->txb_len[sc->txb_next_tx];

	/*
	 * Set NIC for page 0 register access
	 */
	ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_STA);

	/*
	 * Set TX buffer start page
	 */
	ed_nic_outb(sc, ED_P0_TPSR, sc->tx_page_start +
		    sc->txb_next_tx * ED_TXBUF_SIZE);

	/*
	 * Set TX length
	 */
	ed_nic_outb(sc, ED_P0_TBCR0, len);
	ed_nic_outb(sc, ED_P0_TBCR1, len >> 8);

	/*
	 * Set page 0, Remote DMA complete, Transmit Packet, and *Start*
	 */
	ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_TXP | ED_CR_STA);
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
static void
ed_start(ifp)
	struct ifnet *ifp;
{
	struct ed_softc *sc = ifp->if_softc;
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
		ed_xmit(sc);
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
				ed_asic_outb(sc, ED_3COM_GACFR,
					     ED_3COM_GACFR_RSEL);
				break;

				/*
				 * Enable 16bit access to shared memory on
				 * WD/SMC boards.
				 */
			case ED_VENDOR_WD_SMC:
				ed_asic_outb(sc, ED_WD_LAAR,
					     sc->wd_laar_proto | ED_WD_LAAR_M16EN);
				if (sc->chip_type == ED_CHIP_TYPE_WD790) {
					ed_asic_outb(sc, ED_WD_MSR, ED_WD_MSR_MENB);
				}
				break;
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
				ed_asic_outb(sc, ED_3COM_GACFR,
					     ED_3COM_GACFR_RSEL | ED_3COM_GACFR_MBS0);
				break;
			case ED_VENDOR_WD_SMC:
				if (sc->chip_type == ED_CHIP_TYPE_WD790) {
					ed_asic_outb(sc, ED_WD_MSR, 0x00);
				}
				ed_asic_outb(sc, ED_WD_LAAR,
					     sc->wd_laar_proto & ~ED_WD_LAAR_M16EN);
				break;
			}
		}
	} else {
		len = ed_pio_write_mbufs(sc, m, (int)buffer);
		if (len == 0) {
			m_freem(m0);
			goto outloop;
		}
	}

	sc->txb_len[sc->txb_new] = max(len, (ETHER_MIN_LEN-ETHER_CRC_LEN));

	sc->txb_inuse++;

	/*
	 * Point to next buffer slot and wrap if necessary.
	 */
	sc->txb_new++;
	if (sc->txb_new == sc->txb_cnt)
		sc->txb_new = 0;

	if (sc->xmit_busy == 0)
		ed_xmit(sc);

	/*
	 * Tap off here if there is a bpf listener.
	 */
	BPF_MTAP(ifp, m0);

	m_freem(m0);

	/*
	 * Loop back to the top to possibly buffer more packets
	 */
	goto outloop;
}

/*
 * Ethernet interface receiver interrupt.
 */
static __inline void
ed_rint(sc)
	struct ed_softc *sc;
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	u_char  boundry;
	u_short len;
	struct ed_ring packet_hdr;
	char   *packet_ptr;

	if (sc->gone)
		return;

	/*
	 * Set NIC to page 1 registers to get 'current' pointer
	 */
	ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STA);

	/*
	 * 'sc->next_packet' is the logical beginning of the ring-buffer -
	 * i.e. it points to where new data has been buffered. The 'CURR'
	 * (current) register points to the logical end of the ring-buffer -
	 * i.e. it points to where additional new data will be added. We loop
	 * here until the logical beginning equals the logical end (or in
	 * other words, until the ring-buffer is empty).
	 */
	while (sc->next_packet != ed_nic_inb(sc, ED_P1_CURR)) {

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
			ed_pio_readmem(sc, (int)packet_ptr, (char *) &packet_hdr,
				       sizeof(packet_hdr));
		len = packet_hdr.count;
		if (len > (ETHER_MAX_LEN - ETHER_CRC_LEN + sizeof(struct ed_ring)) ||
		    len < (ETHER_MIN_LEN - ETHER_CRC_LEN + sizeof(struct ed_ring))) {
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
			/*
			 * because buffers are aligned on 256-byte boundary,
			 * the length computed above is off by 256 in almost
			 * all cases. Fix it...
			 */
			if (len & 0xff)
				len -= 256 ;
			if (len > (ETHER_MAX_LEN - ETHER_CRC_LEN 
				   + sizeof(struct ed_ring)))
				sc->mibdata.dot3StatsFrameTooLongs++;
		}
		/*
		 * Be fairly liberal about what we allow as a "reasonable" length
		 * so that a [crufty] packet will make it to BPF (and can thus
		 * be analyzed). Note that all that is really important is that
		 * we have a length that will fit into one mbuf cluster or less;
		 * the upper layer protocols can then figure out the length from
		 * their own length field(s).
		 * But make sure that we have at least a full ethernet header
		 * or we would be unable to call ether_input() later.
		 */
		if ((len >= sizeof(struct ed_ring) + ETHER_HDR_LEN) &&
		    (len <= MCLBYTES) &&
		    (packet_hdr.next_packet >= sc->rec_page_start) &&
		    (packet_hdr.next_packet < sc->rec_page_stop)) {
			/*
			 * Go get packet.
			 */
			ed_get_packet(sc, packet_ptr + sizeof(struct ed_ring),
				      len - sizeof(struct ed_ring));
			ifp->if_ipackets++;
		} else {
			/*
			 * Really BAD. The ring pointers are corrupted.
			 */
			log(LOG_ERR,
			    "ed%d: NIC memory corrupt - invalid packet length %d\n",
			    ifp->if_unit, len);
			ifp->if_ierrors++;
			ed_reset(ifp);
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
		ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_STA);

		ed_nic_outb(sc, ED_P0_BNRY, boundry);

		/*
		 * Set NIC to page 1 registers before looping to top (prepare
		 * to get 'CURR' current pointer)
		 */
		ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STA);
	}
}

/*
 * Ethernet interface interrupt processor
 */
void
edintr(arg)
	void *arg;
{
	struct ed_softc *sc = (struct ed_softc*) arg;
	struct ifnet *ifp = (struct ifnet *)sc;
	u_char  isr;
	int	count;

	if (sc->gone)
		return;
	/*
	 * Set NIC to page 0 registers
	 */
	ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_STA);

	/*
	 * loop until there are no more new interrupts.  When the card
	 * goes away, the hardware will read back 0xff.  Looking at
	 * the interrupts, it would appear that 0xff is impossible,
	 * or at least extremely unlikely.
	 */
	while ((isr = ed_nic_inb(sc, ED_P0_ISR)) != 0 && isr != 0xff) {

		/*
		 * reset all the bits that we are 'acknowledging' by writing a
		 * '1' to each bit position that was set (writing a '1'
		 * *clears* the bit)
		 */
		ed_nic_outb(sc, ED_P0_ISR, isr);

		/* 
		 * XXX workaround for AX88190
		 * We limit this to 5000 iterations.  At 1us per inb/outb,
		 * this translates to about 15ms, which should be plenty
		 * of time, and also gives protection in the card eject
		 * case.
		 */
		if (sc->chip_type == ED_CHIP_TYPE_AX88190) {
			count = 5000;		/* 15ms */
			while (count-- && (ed_nic_inb(sc, ED_P0_ISR) & isr)) {
				ed_nic_outb(sc, ED_P0_ISR,0);
				ed_nic_outb(sc, ED_P0_ISR,isr);
			}
			if (count == 0)
				break;
		}

		/*
		 * Handle transmitter interrupts. Handle these first because
		 * the receiver will reset the board under some conditions.
		 */
		if (isr & (ED_ISR_PTX | ED_ISR_TXE)) {
			u_char  collisions = ed_nic_inb(sc, ED_P0_NCR) & 0x0f;

			/*
			 * Check for transmit error. If a TX completed with an
			 * error, we end up throwing the packet away. Really
			 * the only error that is possible is excessive
			 * collisions, and in this case it is best to allow
			 * the automatic mechanisms of TCP to backoff the
			 * flow. Of course, with UDP we're screwed, but this
			 * is expected when a network is heavily loaded.
			 */
			(void) ed_nic_inb(sc, ED_P0_TSR);
			if (isr & ED_ISR_TXE) {
				u_char tsr;

				/*
				 * Excessive collisions (16)
				 */
				tsr = ed_nic_inb(sc, ED_P0_TSR);
				if ((tsr & ED_TSR_ABT)	
				    && (collisions == 0)) {

					/*
					 * When collisions total 16, the
					 * P0_NCR will indicate 0, and the
					 * TSR_ABT is set.
					 */
					collisions = 16;
					sc->mibdata.dot3StatsExcessiveCollisions++;
					sc->mibdata.dot3StatsCollFrequencies[15]++;
				}
				if (tsr & ED_TSR_OWC)
					sc->mibdata.dot3StatsLateCollisions++;
				if (tsr & ED_TSR_CDH)
					sc->mibdata.dot3StatsSQETestErrors++;
				if (tsr & ED_TSR_CRS)
					sc->mibdata.dot3StatsCarrierSenseErrors++;
				if (tsr & ED_TSR_FU)
					sc->mibdata.dot3StatsInternalMacTransmitErrors++;

				/*
				 * update output errors counter
				 */
				ifp->if_oerrors++;
			} else {

				/*
				 * Update total number of successfully
				 * transmitted packets.
				 */
				ifp->if_opackets++;
			}

			/*
			 * reset tx busy and output active flags
			 */
			sc->xmit_busy = 0;
			ifp->if_flags &= ~IFF_OACTIVE;

			/*
			 * clear watchdog timer
			 */
			ifp->if_timer = 0;

			/*
			 * Add in total number of collisions on last
			 * transmission.
			 */
			ifp->if_collisions += collisions;
			switch(collisions) {
			case 0:
			case 16:
				break;
			case 1:
				sc->mibdata.dot3StatsSingleCollisionFrames++;
				sc->mibdata.dot3StatsCollFrequencies[0]++;
				break;
			default:
				sc->mibdata.dot3StatsMultipleCollisionFrames++;
				sc->mibdata.
					dot3StatsCollFrequencies[collisions-1]
						++;
				break;
			}

			/*
			 * Decrement buffer in-use count if not zero (can only
			 * be zero if a transmitter interrupt occured while
			 * not actually transmitting). If data is ready to
			 * transmit, start it transmitting, otherwise defer
			 * until after handling receiver
			 */
			if (sc->txb_inuse && --sc->txb_inuse)
				ed_xmit(sc);
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
				ifp->if_ierrors++;
#ifdef DIAGNOSTIC
				log(LOG_WARNING,
				    "ed%d: warning - receiver ring buffer overrun\n",
				    ifp->if_unit);
#endif

				/*
				 * Stop/reset/re-init NIC
				 */
				ed_reset(ifp);
			} else {

				/*
				 * Receiver Error. One or more of: CRC error,
				 * frame alignment error FIFO overrun, or
				 * missed packet.
				 */
				if (isr & ED_ISR_RXE) {
					u_char rsr;
					rsr = ed_nic_inb(sc, ED_P0_RSR);
					if (rsr & ED_RSR_CRC)
						sc->mibdata.dot3StatsFCSErrors++;
					if (rsr & ED_RSR_FAE)
						sc->mibdata.dot3StatsAlignmentErrors++;
					if (rsr & ED_RSR_FO)
						sc->mibdata.dot3StatsInternalMacReceiveErrors++;
					ifp->if_ierrors++;
#ifdef ED_DEBUG
					if_printf(ifp, "receive error %x\n",
					       ed_nic_inb(sc, ED_P0_RSR));
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

					ed_asic_outb(sc, ED_WD_LAAR,
						     sc->wd_laar_proto | ED_WD_LAAR_M16EN);
					if (sc->chip_type == ED_CHIP_TYPE_WD790) {
						ed_asic_outb(sc, ED_WD_MSR,
							     ED_WD_MSR_MENB);
					}
				}
				ed_rint(sc);

				/* disable 16bit access */
				if (sc->isa16bit &&
				    (sc->vendor == ED_VENDOR_WD_SMC)) {

					if (sc->chip_type == ED_CHIP_TYPE_WD790) {
						ed_asic_outb(sc, ED_WD_MSR, 0x00);
					}
					ed_asic_outb(sc, ED_WD_LAAR,
						     sc->wd_laar_proto & ~ED_WD_LAAR_M16EN);
				}
			}
		}

		/*
		 * If it looks like the transmitter can take more data,
		 * attempt to start output on the interface. This is done
		 * after handling the receiver to give the receiver priority.
		 */
		if ((ifp->if_flags & IFF_OACTIVE) == 0)
			ed_start(ifp);

		/*
		 * return NIC CR to standard state: page 0, remote DMA
		 * complete, start (toggling the TXP bit off, even if was just
		 * set in the transmit routine, is *okay* - it is 'edge'
		 * triggered from low to high)
		 */
		ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_STA);

		/*
		 * If the Network Talley Counters overflow, read them to reset
		 * them. It appears that old 8390's won't clear the ISR flag
		 * otherwise - resulting in an infinite loop.
		 */
		if (isr & ED_ISR_CNT) {
			(void) ed_nic_inb(sc, ED_P0_CNTR0);
			(void) ed_nic_inb(sc, ED_P0_CNTR1);
			(void) ed_nic_inb(sc, ED_P0_CNTR2);
		}
	}
}

/*
 * Process an ioctl request. This code needs some work - it looks
 *	pretty ugly.
 */
static int
ed_ioctl(ifp, command, data)
	register struct ifnet *ifp;
	u_long     command;
	caddr_t data;
{
	struct ed_softc *sc = ifp->if_softc;
#ifndef ED_NO_MIIBUS
	struct ifreq *ifr = (struct ifreq *)data;
	struct mii_data *mii;
#endif
	int     s, error = 0;

	if (sc == NULL || sc->gone) {
		ifp->if_flags &= ~IFF_RUNNING;
		return ENXIO;
	}
	s = splimp();

	switch (command) {
	case SIOCSIFFLAGS:

		/*
		 * If the interface is marked up and stopped, then start it.
		 * If it is marked down and running, then stop it.
		 */
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_flags & IFF_RUNNING) == 0)
				ed_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				ed_stop(sc);
				ifp->if_flags &= ~IFF_RUNNING;
			}
		}

		/*
		 * Promiscuous flag may have changed, so reprogram the RCR.
		 */
		ed_setrcr(sc);

		/*
		 * An unfortunate hack to provide the (required) software
		 * control of the tranceiver for 3Com boards. The ALTPHYS flag
		 * disables the tranceiver if set.
		 */
		if (sc->vendor == ED_VENDOR_3COM) {
			if (ifp->if_flags & IFF_ALTPHYS) {
				ed_asic_outb(sc, ED_3COM_CR, 0);
			} else {
				ed_asic_outb(sc, ED_3COM_CR, ED_3COM_CR_XSEL);
			}
		} else if (sc->vendor == ED_VENDOR_HP) 
			ed_hpp_set_physical_link(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/*
		 * Multicast list has changed; set the hardware filter
		 * accordingly.
		 */
		ed_setrcr(sc);
		error = 0;
		break;

#ifndef ED_NO_MIIBUS
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		if (sc->miibus == NULL) {
			error = EINVAL;
			break;
		}
		mii = device_get_softc(sc->miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
#endif

	default:
		error = ether_ioctl(ifp, command, data);
	}
	(void) splx(s);
	return (error);
}

/*
 * Given a source and destination address, copy 'amount' of a packet from
 *	the ring buffer into a linear destination buffer. Takes into account
 *	ring-wrap.
 */
static __inline char *
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
			ed_pio_readmem(sc, (int)src, dst, tmp_amount);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst += tmp_amount;
	}
	if (sc->mem_shared)
		bcopy(src, dst, amount);
	else
		ed_pio_readmem(sc, (int)src, dst, amount);

	return (src + amount);
}

/*
 * Retreive packet from shared memory and send to the next level up via
 * ether_input().
 */
static void
ed_get_packet(sc, buf, len)
	struct ed_softc *sc;
	char   *buf;
	u_short len;
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct ether_header *eh;
	struct mbuf *m;

	/* Allocate a header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;
	m->m_pkthdr.rcvif = ifp;
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
	 * Don't read in the entire packet if we know we're going to drop it
	 * and no bpf is active.
	 */
	if (!ifp->if_bpf && BDG_ACTIVE( (ifp) ) ) {
		struct ifnet *bif;

		ed_ring_copy(sc, buf, (char *)eh, ETHER_HDR_LEN);
		bif = bridge_in_ptr(ifp, eh) ;
		if (bif == BDG_DROP) {
			m_freem(m);
			return;
		}
		if (len > ETHER_HDR_LEN)
			ed_ring_copy(sc, buf + ETHER_HDR_LEN,
				(char *)(eh + 1), len - ETHER_HDR_LEN);
	} else
		/*
		 * Get packet, including link layer address, from interface.
		 */
		ed_ring_copy(sc, buf, (char *)eh, len);

	m->m_pkthdr.len = m->m_len = len;

	(*ifp->if_input)(ifp, m);
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
	int src;
	unsigned char *dst;
	unsigned short amount;
{
	/* HP PC Lan+ cards need special handling */
	if (sc->vendor == ED_VENDOR_HP && sc->type == ED_TYPE_HP_PCLANPLUS) {
		ed_hpp_readmem(sc, src, dst, amount);
		return;
	}

	/* Regular Novell cards */
	/* select page 0 registers */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STA);

	/* round up to a word */
	if (amount & 1)
		++amount;

	/* set up DMA byte count */
	ed_nic_outb(sc, ED_P0_RBCR0, amount);
	ed_nic_outb(sc, ED_P0_RBCR1, amount >> 8);

	/* set up source address in NIC mem */
	ed_nic_outb(sc, ED_P0_RSAR0, src);
	ed_nic_outb(sc, ED_P0_RSAR1, src >> 8);

	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD0 | ED_CR_STA);

	if (sc->isa16bit) {
		ed_asic_insw(sc, ED_NOVELL_DATA, dst, amount / 2);
	} else {
		ed_asic_insb(sc, ED_NOVELL_DATA, dst, amount);
	}
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
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STA);

	/* reset remote DMA complete flag */
	ed_nic_outb(sc, ED_P0_ISR, ED_ISR_RDC);

	/* set up DMA byte count */
	ed_nic_outb(sc, ED_P0_RBCR0, len);
	ed_nic_outb(sc, ED_P0_RBCR1, len >> 8);

	/* set up destination address in NIC mem */
	ed_nic_outb(sc, ED_P0_RSAR0, dst);
	ed_nic_outb(sc, ED_P0_RSAR1, dst >> 8);

	/* set remote DMA write */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD1 | ED_CR_STA);

	if (sc->isa16bit) {
		ed_asic_outsw(sc, ED_NOVELL_DATA, src, len / 2);
	} else {
		ed_asic_outsb(sc, ED_NOVELL_DATA, src, len);
	}

	/*
	 * Wait for remote DMA complete. This is necessary because on the
	 * transmit side, data is handled internally by the NIC in bursts and
	 * we can't start another remote DMA until this one completes. Not
	 * waiting causes really bad things to happen - like the NIC
	 * irrecoverably jamming the ISA bus.
	 */
	while (((ed_nic_inb(sc, ED_P0_ISR) & ED_ISR_RDC) != ED_ISR_RDC) && --maxwait);
}

/*
 * Write an mbuf chain to the destination NIC memory address using
 *	programmed I/O.
 */
static u_short
ed_pio_write_mbufs(sc, m, dst)
	struct ed_softc *sc;
	struct mbuf *m;
	int dst;
{
	struct ifnet *ifp = (struct ifnet *)sc;
	unsigned short total_len, dma_len;
	struct mbuf *mp;
	int     maxwait = 200;	/* about 240us */

	/* HP PC Lan+ cards need special handling */
	if (sc->vendor == ED_VENDOR_HP && sc->type == ED_TYPE_HP_PCLANPLUS) {
		return ed_hpp_write_mbufs(sc, m, dst);
	}

	/* Regular Novell cards */
	/* First, count up the total number of bytes to copy */
	for (total_len = 0, mp = m; mp; mp = mp->m_next)
		total_len += mp->m_len;

	dma_len = total_len;
	if (sc->isa16bit && (dma_len & 1))
		dma_len++;

	/* select page 0 registers */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STA);

	/* reset remote DMA complete flag */
	ed_nic_outb(sc, ED_P0_ISR, ED_ISR_RDC);

	/* set up DMA byte count */
	ed_nic_outb(sc, ED_P0_RBCR0, dma_len);
	ed_nic_outb(sc, ED_P0_RBCR1, dma_len >> 8);

	/* set up destination address in NIC mem */
	ed_nic_outb(sc, ED_P0_RSAR0, dst);
	ed_nic_outb(sc, ED_P0_RSAR1, dst >> 8);

	/* set remote DMA write */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD1 | ED_CR_STA);

  /*
   * Transfer the mbuf chain to the NIC memory.
   * 16-bit cards require that data be transferred as words, and only words.
   * So that case requires some extra code to patch over odd-length mbufs.
   */

	if (!sc->isa16bit) {
		/* NE1000s are easy */
		while (m) {
			if (m->m_len) {
				ed_asic_outsb(sc, ED_NOVELL_DATA,
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
					ed_asic_outw(sc, ED_NOVELL_DATA,
						     *(u_short *)savebyte);
					data++;
					len--;
					wantbyte = 0;
				}
				/* output contiguous words */
				if (len > 1) {
					ed_asic_outsw(sc, ED_NOVELL_DATA,
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
			ed_asic_outw(sc, ED_NOVELL_DATA, *(u_short *)savebyte);
		}
	}

	/*
	 * Wait for remote DMA complete. This is necessary because on the
	 * transmit side, data is handled internally by the NIC in bursts and
	 * we can't start another remote DMA until this one completes. Not
	 * waiting causes really bad things to happen - like the NIC
	 * irrecoverably jamming the ISA bus.
	 */
	while (((ed_nic_inb(sc, ED_P0_ISR) & ED_ISR_RDC) != ED_ISR_RDC) && --maxwait);

	if (!maxwait) {
		log(LOG_WARNING, "ed%d: remote transmit DMA failed to complete\n",
		    ifp->if_unit);
		ed_reset(ifp);
		return(0);
	}
	return (total_len);
}

/*
 * Support routines to handle the HP PC Lan+ card.
 */

/*
 * HP PC Lan+: Read from NIC memory, using either PIO or memory mapped
 * IO.
 */

static void
ed_hpp_readmem(sc, src, dst, amount)
	struct ed_softc *sc; 
	unsigned short src;
	unsigned char *dst;
	unsigned short amount;
{

	int use_32bit_access = !(sc->hpp_id & ED_HPP_ID_16_BIT_ACCESS);


	/* Program the source address in RAM */
	ed_asic_outw(sc, ED_HPP_PAGE_2, src);

	/*
	 * The HP PC Lan+ card supports word reads as well as
	 * a memory mapped i/o port that is aliased to every 
	 * even address on the board.
	 */

	if (sc->hpp_mem_start) {

		/* Enable memory mapped access.  */
		ed_asic_outw(sc, ED_HPP_OPTION, sc->hpp_options & 
			~(ED_HPP_OPTION_MEM_DISABLE | 
			  ED_HPP_OPTION_BOOT_ROM_ENB));

		if (use_32bit_access && (amount > 3)) {
			u_int32_t *dl = (u_int32_t *) dst;	
			volatile u_int32_t *const sl = 
				(u_int32_t *) sc->hpp_mem_start;
			u_int32_t *const fence = dl + (amount >> 2);
			
			/* Copy out NIC data.  We could probably write this
			   as a `movsl'. The currently generated code is lousy.
			   */

			while (dl < fence)
				*dl++ = *sl;
		
			dst += (amount & ~3);
			amount &= 3;

		} 

		/* Finish off any words left, as a series of short reads */
		if (amount > 1) {
			u_short *d = (u_short *) dst;	
			volatile u_short *const s = 
				(u_short *) sc->hpp_mem_start;
			u_short *const fence = d + (amount >> 1);
			
			/* Copy out NIC data.  */

			while (d < fence)
				*d++ = *s;
	
			dst += (amount & ~1);
			amount &= 1;
		}

		/*
		 * read in a byte; however we need to always read 16 bits
		 * at a time or the hardware gets into a funny state
		 */

		if (amount == 1) {
			/* need to read in a short and copy LSB */
			volatile u_short *const s = 
				(volatile u_short *) sc->hpp_mem_start;
			
			*dst = (*s) & 0xFF;	
		}

		/* Restore Boot ROM access.  */

		ed_asic_outw(sc, ED_HPP_OPTION, sc->hpp_options);


	} else { 
		/* Read in data using the I/O port */
		if (use_32bit_access && (amount > 3)) {
			ed_asic_insl(sc, ED_HPP_PAGE_4, dst, amount >> 2);
			dst += (amount & ~3);
			amount &= 3;
		}
		if (amount > 1) {
			ed_asic_insw(sc, ED_HPP_PAGE_4, dst, amount >> 1);
			dst += (amount & ~1);
			amount &= 1;
		}
		if (amount == 1) { /* read in a short and keep the LSB */
			*dst = ed_asic_inw(sc, ED_HPP_PAGE_4) & 0xFF;
		}
	}
}

/*
 * HP PC Lan+: Write to NIC memory, using either PIO or memory mapped
 * IO.
 *	Only used in the probe routine to test the memory. 'len' must
 *	be even.
 */
static void
ed_hpp_writemem(sc, src, dst, len)
	struct ed_softc *sc;
	unsigned char *src;
	unsigned short dst;
	unsigned short len;
{
	/* reset remote DMA complete flag */
	ed_nic_outb(sc, ED_P0_ISR, ED_ISR_RDC);

	/* program the write address in RAM */
	ed_asic_outw(sc, ED_HPP_PAGE_0, dst);

	if (sc->hpp_mem_start) {
		u_short *s = (u_short *) src;
		volatile u_short *d = (u_short *) sc->hpp_mem_start;
		u_short *const fence = s + (len >> 1);

		/*
		 * Enable memory mapped access.
		 */

		ed_asic_outw(sc, ED_HPP_OPTION, sc->hpp_options & 
			~(ED_HPP_OPTION_MEM_DISABLE | 
			  ED_HPP_OPTION_BOOT_ROM_ENB));

		/*
		 * Copy to NIC memory.
		 */

		while (s < fence)
			*d = *s++;

		/*
		 * Restore Boot ROM access.
		 */

		ed_asic_outw(sc, ED_HPP_OPTION, sc->hpp_options);

	} else {
		/* write data using I/O writes */
		ed_asic_outsw(sc, ED_HPP_PAGE_4, src, len / 2);
	}
}

/*
 * Write to HP PC Lan+ NIC memory.  Access to the NIC can be by using 
 * outsw() or via the memory mapped interface to the same register.
 * Writes have to be in word units; byte accesses won't work and may cause
 * the NIC to behave weirdly. Long word accesses are permitted if the ASIC
 * allows it.
 */

static u_short
ed_hpp_write_mbufs(struct ed_softc *sc, struct mbuf *m, int dst)
{
	int len, wantbyte;
	unsigned short total_len;
	unsigned char savebyte[2];
	volatile u_short * const d = 
		(volatile u_short *) sc->hpp_mem_start;
	int use_32bit_accesses = !(sc->hpp_id & ED_HPP_ID_16_BIT_ACCESS);

	/* select page 0 registers */
	ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_STA);

	/* reset remote DMA complete flag */
	ed_nic_outb(sc, ED_P0_ISR, ED_ISR_RDC);

	/* program the write address in RAM */
	ed_asic_outw(sc, ED_HPP_PAGE_0, dst);

	if (sc->hpp_mem_start) 	/* enable memory mapped I/O */
		ed_asic_outw(sc, ED_HPP_OPTION, sc->hpp_options & 
			~(ED_HPP_OPTION_MEM_DISABLE |
			ED_HPP_OPTION_BOOT_ROM_ENB));

	wantbyte = 0;
	total_len = 0;

	if (sc->hpp_mem_start) {	/* Memory mapped I/O port */
		while (m) {
			total_len += (len = m->m_len);
			if (len) {
				caddr_t data = mtod(m, caddr_t);
				/* finish the last word of the previous mbuf */
				if (wantbyte) {
					savebyte[1] = *data;
					*d = *((u_short *) savebyte);
					data++; len--; wantbyte = 0;
				}
				/* output contiguous words */
				if ((len > 3) && (use_32bit_accesses)) {
					volatile u_int32_t *const dl = 
						(volatile u_int32_t *) d;
					u_int32_t *sl = (u_int32_t *) data;
					u_int32_t *fence = sl + (len >> 2);

					while (sl < fence)
						*dl = *sl++;

					data += (len & ~3);
					len &= 3;
				}
				/* finish off remain 16 bit writes */
				if (len > 1) {
					u_short *s = (u_short *) data;
					u_short *fence = s + (len >> 1);

					while (s < fence)
						*d = *s++;

					data += (len & ~1); 
					len &= 1;
				}
				/* save last byte if needed */
				if ((wantbyte = (len == 1)) != 0)
					savebyte[0] = *data;
			}
			m = m->m_next;	/* to next mbuf */
		}
		if (wantbyte) /* write last byte */
			*d = *((u_short *) savebyte);
	} else {
		/* use programmed I/O */
		while (m) {
			total_len += (len = m->m_len);
			if (len) {
				caddr_t data = mtod(m, caddr_t);
				/* finish the last word of the previous mbuf */
				if (wantbyte) {
					savebyte[1] = *data;
					ed_asic_outw(sc, ED_HPP_PAGE_4,
						     *((u_short *)savebyte));
					data++; 
					len--; 
					wantbyte = 0;
				}
				/* output contiguous words */
				if ((len > 3) && use_32bit_accesses) {
					ed_asic_outsl(sc, ED_HPP_PAGE_4,
						      data, len >> 2);
					data += (len & ~3);
					len &= 3;
				}
				/* finish off remaining 16 bit accesses */
				if (len > 1) {
					ed_asic_outsw(sc, ED_HPP_PAGE_4,
						      data, len >> 1);
					data += (len & ~1);
					len &= 1;
				}
				if ((wantbyte = (len == 1)) != 0)
					savebyte[0] = *data;

			} /* if len != 0 */
			m = m->m_next;
		}
		if (wantbyte) /* spit last byte */
			ed_asic_outw(sc, ED_HPP_PAGE_4, *(u_short *)savebyte);

	}

	if (sc->hpp_mem_start)	/* turn off memory mapped i/o */
		ed_asic_outw(sc, ED_HPP_OPTION, sc->hpp_options);

	return (total_len);
}

#ifndef ED_NO_MIIBUS
/*
 * MII bus support routines.
 */
int
ed_miibus_readreg(dev, phy, reg)
	device_t dev;
	int phy, reg;
{
	struct ed_softc *sc;
	int failed, s, val;

	s = splimp();
	sc = device_get_softc(dev);
	if (sc->gone) {
		splx(s);
		return (0);
	}

	(*sc->mii_writebits)(sc, 0xffffffff, 32);
	(*sc->mii_writebits)(sc, ED_MII_STARTDELIM, ED_MII_STARTDELIM_BITS);
	(*sc->mii_writebits)(sc, ED_MII_READOP, ED_MII_OP_BITS);
	(*sc->mii_writebits)(sc, phy, ED_MII_PHY_BITS);
	(*sc->mii_writebits)(sc, reg, ED_MII_REG_BITS);

	failed = (*sc->mii_readbits)(sc, ED_MII_ACK_BITS);
	val = (*sc->mii_readbits)(sc, ED_MII_DATA_BITS);
	(*sc->mii_writebits)(sc, ED_MII_IDLE, ED_MII_IDLE_BITS);

	splx(s);
	return (failed ? 0 : val);
}

void
ed_miibus_writereg(dev, phy, reg, data)
	device_t dev;
	int phy, reg, data;
{
	struct ed_softc *sc;
	int s;

	s = splimp();
	sc = device_get_softc(dev);
	if (sc->gone) {
		splx(s);
		return;
	}

	(*sc->mii_writebits)(sc, 0xffffffff, 32);
	(*sc->mii_writebits)(sc, ED_MII_STARTDELIM, ED_MII_STARTDELIM_BITS);
	(*sc->mii_writebits)(sc, ED_MII_WRITEOP, ED_MII_OP_BITS);
	(*sc->mii_writebits)(sc, phy, ED_MII_PHY_BITS);
	(*sc->mii_writebits)(sc, reg, ED_MII_REG_BITS);
	(*sc->mii_writebits)(sc, ED_MII_TURNAROUND, ED_MII_TURNAROUND_BITS);
	(*sc->mii_writebits)(sc, data, ED_MII_DATA_BITS);
	(*sc->mii_writebits)(sc, ED_MII_IDLE, ED_MII_IDLE_BITS);

	splx(s);
}

int
ed_ifmedia_upd(ifp)
	struct ifnet *ifp;
{
	struct ed_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;
	if (sc->gone || sc->miibus == NULL)
		return (ENXIO);
	
	mii = device_get_softc(sc->miibus);
	return mii_mediachg(mii);
}

void
ed_ifmedia_sts(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct ed_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;
	if (sc->gone || sc->miibus == NULL)
		return;

	mii = device_get_softc(sc->miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

void
ed_child_detached(dev, child)
	device_t dev;
	device_t child;
{
	struct ed_softc *sc;

	sc = device_get_softc(dev);
	if (child == sc->miibus)
		sc->miibus = NULL;
}
#endif

static void
ed_setrcr(sc)
	struct ed_softc *sc;
{
	struct ifnet *ifp = (struct ifnet *)sc;
	int     i;
	u_char	reg1;

	/* Bit 6 in AX88190 RCR register must be set. */
	if (sc->chip_type == ED_CHIP_TYPE_AX88190)
		reg1 = ED_RCR_INTT;
	else
		reg1 = 0x00;

	/* set page 1 registers */
	ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_PAGE_1 | ED_CR_STP);

	if (ifp->if_flags & IFF_PROMISC) {

		/*
		 * Reconfigure the multicast filter.
		 */
		for (i = 0; i < 8; i++)
			ed_nic_outb(sc, ED_P1_MAR(i), 0xff);

		/*
		 * And turn on promiscuous mode. Also enable reception of
		 * runts and packets with CRC & alignment errors.
		 */
		/* Set page 0 registers */
		ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_STP);

		ed_nic_outb(sc, ED_P0_RCR, ED_RCR_PRO | ED_RCR_AM |
			    ED_RCR_AB | ED_RCR_AR | ED_RCR_SEP | reg1);
	} else {
		/* set up multicast addresses and filter modes */
		if (ifp->if_flags & IFF_MULTICAST) {
			u_int32_t  mcaf[2];

			if (ifp->if_flags & IFF_ALLMULTI) {
				mcaf[0] = 0xffffffff;
				mcaf[1] = 0xffffffff;
			} else
				ds_getmcaf(sc, mcaf);

			/*
			 * Set multicast filter on chip.
			 */
			for (i = 0; i < 8; i++)
				ed_nic_outb(sc, ED_P1_MAR(i), ((u_char *) mcaf)[i]);

			/* Set page 0 registers */
			ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_STP);

			ed_nic_outb(sc, ED_P0_RCR, ED_RCR_AM | ED_RCR_AB | reg1);
		} else {

			/*
			 * Initialize multicast address hashing registers to
			 * not accept multicasts.
			 */
			for (i = 0; i < 8; ++i)
				ed_nic_outb(sc, ED_P1_MAR(i), 0x00);

			/* Set page 0 registers */
			ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_STP);

			ed_nic_outb(sc, ED_P0_RCR, ED_RCR_AB | reg1);
		}
	}

	/*
	 * Start interface.
	 */
	ed_nic_outb(sc, ED_P0_CR, sc->cr_proto | ED_CR_STA);
}

/*
 * Compute crc for ethernet address
 */
static u_int32_t
ds_crc(ep)
	u_char *ep;
{
#define POLYNOMIAL 0x04c11db6
	register u_int32_t crc = 0xffffffff;
	register int carry, i, j;
	register u_char b;

	for (i = 6; --i >= 0;) {
		b = *ep++;
		for (j = 8; --j >= 0;) {
			carry = ((crc & 0x80000000) ? 1 : 0) ^ (b & 0x01);
			crc <<= 1;
			b >>= 1;
			if (carry)
				crc = (crc ^ POLYNOMIAL) | carry;
		}
	}
	return crc;
#undef POLYNOMIAL
}

/*
 * Compute the multicast address filter from the
 * list of multicast addresses we need to listen to.
 */
static void
ds_getmcaf(sc, mcaf)
	struct ed_softc *sc;
	u_int32_t *mcaf;
{
	register u_int32_t index;
	register u_char *af = (u_char *) mcaf;
	struct ifmultiaddr *ifma;

	mcaf[0] = 0;
	mcaf[1] = 0;

	TAILQ_FOREACH(ifma, &sc->arpcom.ac_if.if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		index = ds_crc(LLADDR((struct sockaddr_dl *)ifma->ifma_addr))
			>> 26;
		af[index >> 3] |= 1 << (index & 7);
	}
}
