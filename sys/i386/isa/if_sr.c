/*
 * Copyright (c) 1996 John Hay.
 * Copyright (c) 1996 SDL Communications, Inc.
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * $Id: if_sr.c,v 1.13 1998/06/07 17:10:37 dfr Exp $
 */

/*
 * Programming assumptions and other issues.
 *
 * Only a 16K window will be used.
 *
 * The descriptors of a DMA channel will fit in a 16K memory window.
 *
 * The buffers of a transmit DMA channel will fit in a 16K memory window.
 *
 * When interface is going up, handshaking is set and it is only cleared
 * when the interface is down'ed.
 *
 * There should be a way to set/reset Raw HDLC/PPP, Loopback, DCE/DTE,
 * internal/external clock, etc.....
 *
 */

#include "sr.h"
#ifdef notyet
#include "fr.h"
#else
#define NFR	0
#endif
#include "bpfilter.h"

#include "sppp.h"
#if NSPPP <= 0
#error Device 'sr' requires sppp.
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/sockio.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_sppp.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/md_var.h>

#include <i386/isa/if_srregs.h>
#include <i386/isa/ic/hd64570.h>
#include <i386/isa/isa_device.h>

#include "ioconf.h"

/* #define USE_MODEMCK */

#ifndef BUGGY
#define BUGGY		0
#endif

#define PPP_HEADER_LEN	4

/*
 * These macros are used to hide the difference between the way the
 * ISA N2 cards and the PCI N2 cards access the Hitachi 64570 SCA.
 */
#define SRC_GET8(base,off)	(*hc->src_get8)(base,(u_int)&off)
#define SRC_GET16(base,off)	(*hc->src_get16)(base,(u_int)&off)
#define SRC_PUT8(base,off,d)	(*hc->src_put8)(base,(u_int)&off,d)
#define SRC_PUT16(base,off,d)	(*hc->src_put16)(base,(u_int)&off,d)

/*
 * These macros enable/disable the DPRAM and select the correct
 * DPRAM page.
 */
#define SRC_GET_WIN(addr)	((addr >> SRC_WIN_SHFT) & SR_PG_MSK)

#define SRC_SET_ON(iobase)	outb(iobase+SR_PCR,			     \
					SR_PCR_MEM_WIN | inb(iobase+SR_PCR))
#define SRC_SET_MEM(iobase,win)	outb(iobase+SR_PSR, SRC_GET_WIN(win) |	     \
					(inb(iobase+SR_PSR) & ~SR_PG_MSK))
#define SRC_SET_OFF(iobase)	outb(iobase+SR_PCR,			     \
					~SR_PCR_MEM_WIN & inb(iobase+SR_PCR))

/*
 * Define the hardware (card information) structure needed to keep
 * track of the device itself... There is only one per card.
 */
struct sr_hardc {
	struct	sr_hardc *next;		/* PCI card linkage */
	struct	sr_softc *sc;		/* software channels */
	int	cunit;			/* card w/in system */

	u_short	iobase;			/* I/O Base Address */
	int	cardtype;
	int	numports;		/* # of ports on cd */
	int	mempages;
	u_int	memsize;		/* DPRAM size: bytes */
	u_int	winmsk;
	vm_offset_t	sca_base;
	vm_offset_t	mem_pstart;	/* start of buffer */
	caddr_t	mem_start;		/* start of DP RAM */
	caddr_t	mem_end;		/* end of DP RAM */
	caddr_t	plx_base;

	sca_regs	*sca;		/* register array */

	/*
	 * We vectorize the following functions to allow re-use between the
	 * ISA card's needs and those of the PCI card.
	 */
	void    (*src_put8)(u_int base, u_int off, u_int val);
	void    (*src_put16)(u_int base, u_int off, u_int val);
	u_int	(*src_get8)(u_int base, u_int off);
	u_int	(*src_get16)(u_int base, u_int off);
};

static int	next_sc_unit = 0;
static int	sr_watcher = 0;
static struct	sr_hardc sr_hardc[NSR];
static struct	sr_hardc *sr_hardc_pci;

/*
 * Define the software interface for the card... There is one for
 * every channel (port).
 */
struct sr_softc {
	struct	sppp ifsppp;	/* PPP service w/in system */
	struct	sr_hardc *hc;	/* card-level information */

	int	unit;		/* With regard to all sr devices */
	int	subunit;	/* With regard to this card */

	int	attached;	/* attached to FR or PPP */
	int	protocol;	/* FR or PPP */
#define	N2_USE_FRP	2	/* Frame Relay Protocol */
#define	N2_USE_PPP	1	/* Point-to-Point Protocol */

	struct	buf_block {
		u_int	txdesc;	/* DPRAM offset */
		u_int	txstart;/* DPRAM offset */
		u_int	txend;	/* DPRAM offset */
		u_int	txtail;	/* # of 1st free gran */
		u_int	txmax;	/* # of free grans */
		u_int	txeda;	/* err descr addr */
	} block[SR_TX_BLOCKS];

	char	xmit_busy;	/* Transmitter is busy */
	char	txb_inuse;	/* # of tx grans in use */
	u_int	txb_new;	/* ndx to new buffer */
	u_int	txb_next_tx;	/* ndx to next gran rdy tx */

	u_int	rxdesc;		/* DPRAM offset */
	u_int	rxstart;	/* DPRAM offset */
	u_int	rxend;		/* DPRAM offset */
	u_int	rxhind;		/* ndx to the hd of rx bufrs */
	u_int	rxmax;		/* # of avail grans */

	u_int	clk_cfg;	/* Clock configuration */

	int	scachan;	/* channel # on card */
};

/*
 * List of valid interrupt numbers for the N2 ISA card.
 */
static int sr_irqtable[16] = {
	0,	/*  0 */
	0,	/*  1 */
	0,	/*  2 */
	1,	/*  3 */
	1,	/*  4 */
	1,	/*  5 */
	0,	/*  6 */
	1,	/*  7 */
	0,	/*  8 */
	0,	/*  9 */
	1,	/* 10 */
	1,	/* 11 */
	1,	/* 12 */
	0,	/* 13 */
	0,	/* 14 */
	1	/* 15 */
};

static int	srprobe(struct isa_device *id);
static int	srattach_isa(struct isa_device *id);

struct	isa_driver srdriver = {srprobe, srattach_isa, "src"};

/*
 * Baud Rate table for Sync Mode.
 * Each entry consists of 3 elements:
 * Baud Rate (x100) , TMC, BR
 *
 * Baud Rate = FCLK / TMC / 2^BR
 * Baud table for Crystal freq. of 9.8304 Mhz
 */
#ifdef N2_TEST_SPEED
struct rate_line {
	int	target;		/* target rate/100 */
	int	tmc_reg;	/* TMC register value */
	int	br_reg;		/* BR (BaudRateClk) selector */
} n2_rates[] = {
	/* Baudx100	TMC		BR */
	{ 3,		128,		8 },
	{ 6,		128,		7 },
	{ 12,		128,		6 },
	{ 24,		128,		5 },
	{ 48,		128,		4 },
	{ 96,		128,		3 },
	{ 192,		128,		2 },
	{ 384,		128,		1 },
	{ 560,		88,		1 },
	{ 640,		77,		1 },
	{ 1280,		38,		1 },
	{ 2560,		19,		1 },
	{ 5120,		10,		1 },
	{ 10000,	5,		1 },
	{ 15000,	3,		1 },
	{ 25000,	2,		1 },
	{ 50000,	1,		1 },
	{ 0,		0,		0 }
};

int	sr_test_speed[] = {
	N2_TEST_SPEED,
	N2_TEST_SPEED
};

int	etc0vals[] = {
	SR_MCR_ETC0,		/* ISA channel 0 */
	SR_MCR_ETC1,		/* ISA channel 1 */
	SR_FECR_ETC0,		/* PCI channel 0 */
	SR_FECR_ETC1		/* PCI channel 1 */
};
#endif

struct	sr_hardc *srattach_pci(int unit, vm_offset_t plx_vaddr,
				vm_offset_t sca_vaddr);
void	srintr_hc(struct sr_hardc *hc);

static int	srattach(struct sr_hardc *hc);
static void	sr_xmit(struct sr_softc *sc);
static void	srstart(struct ifnet *ifp);
static int	srioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static void	srwatchdog(struct ifnet *ifp);
static int	sr_packet_avail(struct sr_softc *sc, int *len, u_char *rxstat);
static void	sr_copy_rxbuf(struct mbuf *m, struct sr_softc *sc, int len);
static void	sr_eat_packet(struct sr_softc *sc, int single);
static void	sr_get_packets(struct sr_softc *sc);

static void	sr_up(struct sr_softc *sc);
static void	sr_down(struct sr_softc *sc);
static void	src_init(struct sr_hardc *hc);
static void	sr_init_sca(struct sr_hardc *hc);
static void	sr_init_msci(struct sr_softc *sc);
static void	sr_init_rx_dmac(struct sr_softc *sc);
static void	sr_init_tx_dmac(struct sr_softc *sc);
static void	sr_dmac_intr(struct sr_hardc *hc, u_char isr);
static void	sr_msci_intr(struct sr_hardc *hc, u_char isr);
static void	sr_timer_intr(struct sr_hardc *hc, u_char isr);
static void	sr_modemck(void *x);

static u_int	src_get8_io(u_int base, u_int off);
static u_int	src_get16_io(u_int base, u_int off);
static void	src_put8_io(u_int base, u_int off, u_int val);
static void	src_put16_io(u_int base, u_int off, u_int val);
static u_int	src_get8_mem(u_int base, u_int off);
static u_int	src_get16_mem(u_int base, u_int off);
static void	src_put8_mem(u_int base, u_int off, u_int val);
static void	src_put16_mem(u_int base, u_int off, u_int val);

#if NFR > 0
extern void	fr_detach(struct ifnet *);
extern int	fr_attach(struct ifnet *);
extern int	fr_ioctl(struct ifnet *, int, caddr_t);
extern void	fr_flush(struct ifnet *);
extern int	fr_input(struct ifnet *, struct mbuf *);
extern struct	mbuf *fr_dequeue(struct ifnet *);
#endif

/*
 * I/O for ISA N2 card(s)
 */
#define SRC_REG(iobase,y)	((((y) & 0xf) + (((y) & 0xf0) << 6) +       \
				(iobase)) | 0x8000)

static u_int
src_get8_io(u_int base, u_int off)
{
	return inb(SRC_REG(base, off));
}

static u_int
src_get16_io(u_int base, u_int off)
{
	return inw(SRC_REG(base, off));
}

static void
src_put8_io(u_int base, u_int off, u_int val)
{
	outb(SRC_REG(base, off), val);
}

static void
src_put16_io(u_int base, u_int off, u_int val)
{
	outw(SRC_REG(base, off), val);
}

/*
 * I/O for PCI N2 card(s)
 */
#define SRC_PCI_SCA_REG(y)	((y & 2) ? ((y & 0xfd) + 0x100) : y)

static u_int
src_get8_mem(u_int base, u_int off)
{
	return *((u_char *)(base + SRC_PCI_SCA_REG(off)));
}

static u_int
src_get16_mem(u_int base, u_int off)
{
	return *((u_short *)(base + SRC_PCI_SCA_REG(off)));
}

static void
src_put8_mem(u_int base, u_int off, u_int val)
{
	*((u_char *)(base + SRC_PCI_SCA_REG(off))) = (u_char)val;
}

static void
src_put16_mem(u_int base, u_int off, u_int val)
{
	*((u_short *)(base + SRC_PCI_SCA_REG(off))) = (u_short)val;
}

/*
 * Probe for an ISA card. If it is there, size its memory. Then get the
 * rest of its information and fill it in.
 */
static int
srprobe(struct isa_device *id)
{
	struct sr_hardc *hc = &sr_hardc[id->id_unit];
	u_int pgs, i, tmp;
	u_short port;
	u_short *smem;
	u_char mar;
	sca_regs *sca = 0;

	/*
	 * Now see if the card is realy there.
	 */
	hc->cardtype = SR_CRD_N2;

	/*
	 * We have to fill these in early because the SRC_PUT* and SRC_GET*
	 * macros use them.
	 */
	hc->src_get8 = src_get8_io;
	hc->src_get16 = src_get16_io;
	hc->src_put8 = src_put8_io;
	hc->src_put16 = src_put16_io;

	hc->sca = 0;
	port = id->id_iobase;
	hc->numports = NCHAN;	/* assumed # of channels on the card */

	if (id->id_flags & SR_FLAGS_NCHAN_MSK)
		hc->numports = id->id_flags & SR_FLAGS_NCHAN_MSK;

	outb(port + SR_PCR, 0);	/* turn off the card */

	/*
	 * Next, we'll test the Base Address Register to retension of
	 * data... ... seeing if we're *really* talking to an N2.
	 */
	for (i = 0; i < 0x100; i++) {
		outb(port + SR_BAR, i);
		inb(port + SR_PCR);
		tmp = inb(port + SR_BAR);
		if (tmp != i) {
			printf("sr%d: probe failed BAR %x, %x.\n",
			       id->id_unit, i, tmp);
			return 0;
		}
	}

	/*
	 * Now see if we can see the SCA.
	 */
	outb(port + SR_PCR, SR_PCR_SCARUN | inb(port + SR_PCR));
	SRC_PUT8(port, sca->wcrl, 0);
	SRC_PUT8(port, sca->wcrm, 0);
	SRC_PUT8(port, sca->wcrh, 0);
	SRC_PUT8(port, sca->pcr, 0);
	SRC_PUT8(port, sca->msci[0].tmc, 0);
	inb(port);

	tmp = SRC_GET8(port, sca->msci[0].tmc);
	if (tmp != 0) {
		printf("sr%d: Error reading SCA 0, %x\n", id->id_unit, tmp);
		return 0;
	}
	SRC_PUT8(port, sca->msci[0].tmc, 0x5A);
	inb(port);

	tmp = SRC_GET8(port, sca->msci[0].tmc);
	if (tmp != 0x5A) {
		printf("sr%d: Error reading SCA 0x5A, %x\n", id->id_unit, tmp);
		return 0;
	}
	SRC_PUT16(port, sca->dmac[0].cda, 0);
	inb(port);

	tmp = SRC_GET16(port, sca->dmac[0].cda);
	if (tmp != 0) {
		printf("sr%d: Error reading SCA 0, %x\n", id->id_unit, tmp);
		return 0;
	}
	SRC_PUT16(port, sca->dmac[0].cda, 0x55AA);
	inb(port);

	tmp = SRC_GET16(port, sca->dmac[0].cda);
	if (tmp != 0x55AA) {
		printf("sr%d: Error reading SCA 0x55AA, %x\n",
		       id->id_unit, tmp);
		return 0;
	}
	/*
	 * OK, the board's interface registers seem to work. Now we'll see
	 * if the Dual-Ported RAM is fully accessible...
	 */
	outb(port + SR_PCR, SR_PCR_EN_VPM | SR_PCR_ISA16);
	outb(port + SR_PSR, SR_PSR_WIN_16K);

	/*
	 * Take the kernel "virtual" address supplied to us and convert
	 * it to a "real" address. Then program the card to use that.
	 */
	mar = (kvtop(id->id_maddr) >> 16) & SR_PCR_16M_SEL;
	outb(port + SR_PCR, mar | inb(port + SR_PCR));
	mar = kvtop(id->id_maddr) >> 12;
	outb(port + SR_BAR, mar);
	outb(port + SR_PCR, inb(port + SR_PCR) | SR_PCR_MEM_WIN);
	smem = (u_short *)id->id_maddr;	/* DP RAM Address */

	/*
	 * Here we will perform the memory scan to size the device.
	 *
	 * This is done by marking each potential page with a magic number.
	 * We then loop through the pages looking for that magic number. As
	 * soon as we no longer see that magic number, we'll quit the scan,
	 * knowing that no more memory is present. This provides the number
	 * of pages present on the card.
	 *
	 * Note: We're sizing 16K memory granules.
	 */
	for (i = 0; i <= SR_PSR_PG_SEL; i++) {
		outb(port + SR_PSR,
		     (inb(port + SR_PSR) & ~SR_PSR_PG_SEL) | i);

		*smem = 0xAA55;
	}

	for (i = 0; i <= SR_PSR_PG_SEL; i++) {
		outb(port + SR_PSR,
		     (inb(port + SR_PSR) & ~SR_PSR_PG_SEL) | i);

		if (*smem != 0xAA55) {
			/*
			 * If we have less than 64k of memory, give up. That
			 * is 4 x 16k pages.
			 */
			if (i < 4) {
				printf("sr%d: Bad mem page %d, mem %x, %x.\n",
				       id->id_unit, i, 0xAA55, *smem);
				return 0;
			}
			break;
		}
		*smem = i;
	}

	hc->mempages = i;
	hc->memsize = i * SRC_WIN_SIZ;
	hc->winmsk = SRC_WIN_MSK;
	pgs = i;		/* final count of 16K pages */

	/*
	 * This next loop erases the contents of that page in DPRAM
	 */
	for (i = 0; i <= pgs; i++) {
		outb(port + SR_PSR,
		     (inb(port + SR_PSR) & ~SR_PSR_PG_SEL) | i);
		bzero(smem, SRC_WIN_SIZ);
	}

	SRC_SET_OFF(port);

	/*
	 * We have a card here, fill in what we can.
	 */
	id->id_msize = SRC_WIN_SIZ;
	hc->iobase = id->id_iobase;
	hc->sca_base = id->id_iobase;
	hc->mem_start = id->id_maddr;
	hc->mem_end = (id->id_maddr + id->id_msize) - 1;
	hc->mem_pstart = 0;
	hc->cunit = id->id_unit;

	/*
	 * Do a little sanity check.
	 */
	if (sr_irqtable[ffs(id->id_irq) - 1] == 0)
		printf("sr%d: Warning: illegal interrupt %d chosen.\n",
		       id->id_unit, ffs(id->id_irq) - 1);

	/*
	 * Bogus card configuration
	 */
	if ((hc->numports > NCHAN)	/* only 2 ports/card */
	    ||(hc->memsize > (512 * 1024)))	/* no more than 256K */
		return 0;

	return SRC_IO_SIZ;	/* return the amount of IO addresses used. */
}

/*
 * srattach_isa and srattach_pci allocate memory for hardc, softc and
 * data buffers. It also does any initialization that is bus specific.
 * At the end they call the common srattach() function.
 */
static int
srattach_isa(struct isa_device *id)
{
	u_char mar;
	struct sr_hardc *hc = &sr_hardc[id->id_unit];

	outb(hc->iobase + SR_PCR, inb(hc->iobase + SR_PCR) | SR_PCR_SCARUN);
	outb(hc->iobase + SR_PSR, inb(hc->iobase + SR_PSR) | SR_PSR_EN_SCA_DMA);
	outb(hc->iobase + SR_MCR,
	     SR_MCR_DTR0 | SR_MCR_DTR1 | SR_MCR_TE0 | SR_MCR_TE1);

	SRC_SET_ON(hc->iobase);

	/*
	 * Configure the card. Mem address, irq,
	 */
	mar = (kvtop(id->id_maddr) >> 16) & SR_PCR_16M_SEL;
	outb(hc->iobase + SR_PCR,
	     mar | (inb(hc->iobase + SR_PCR) & ~SR_PCR_16M_SEL));
	mar = kvtop(id->id_maddr) >> 12;
	outb(hc->iobase + SR_BAR, mar);

	/*
	 * Allocate the software interface table(s)
	 */
	hc->sc = malloc(hc->numports * sizeof(struct sr_softc),
			M_DEVBUF, M_WAITOK);
	bzero(hc->sc, hc->numports * sizeof(struct sr_softc));

	/*
	 * Get the TX clock direction and configuration. The default is a
	 * single external clock which is used by RX and TX.
	 */
#ifdef N2_TEST_SPEED
	if (sr_test_speed[0] > 0)
		hc->sc[0].clk_cfg = SR_FLAGS_INT_CLK;
	else if (id->id_flags & SR_FLAGS_0_CLK_MSK)
		hc->sc[0].clk_cfg =
		    (id->id_flags & SR_FLAGS_0_CLK_MSK)
		    >> SR_FLAGS_CLK_SHFT;
#else
	if (id->id_flags & SR_FLAGS_0_CLK_MSK)
		hc->sc[0].clk_cfg =
		    (id->id_flags & SR_FLAGS_0_CLK_MSK)
		    >> SR_FLAGS_CLK_SHFT;
#endif

	if (hc->numports == 2)
#ifdef N2_TEST_SPEED
		if (sr_test_speed[1] > 0)
			hc->sc[0].clk_cfg = SR_FLAGS_INT_CLK;
		else
#endif
		if (id->id_flags & SR_FLAGS_1_CLK_MSK)
			hc->sc[1].clk_cfg = (id->id_flags & SR_FLAGS_1_CLK_MSK)
			    >> (SR_FLAGS_CLK_SHFT + SR_FLAGS_CLK_CHAN_SHFT);

	return srattach(hc);
}

struct sr_hardc *
srattach_pci(int unit, vm_offset_t plx_vaddr, vm_offset_t sca_vaddr)
{
	int numports, pndx;
	u_int fecr, *fecrp = (u_int *)(sca_vaddr + SR_FECR);
	struct sr_hardc *hc, **hcp;

	/*
	 * Configure the PLX. This is magic. I'm doing it just like I'm told
	 * to. :-)
	 * 
	 * offset
	 *  0x00 - Map Range    - Mem-mapped to locate anywhere
	 *  0x04 - Re-Map       - PCI address decode enable
	 *  0x18 - Bus Region   - 32-bit bus, ready enable
	 *  0x1c - Master Range - include all 16 MB
	 *  0x20 - Master RAM   - Map SCA Base at 0
	 *  0x28 - Master Remap - direct master memory enable
	 *  0x68 - Interrupt    - Enable interrupt (0 to disable)
	 * 
	 * Note: This is "cargo cult" stuff.  - jrc
	 */
	*((u_int *)(plx_vaddr + 0x00)) = 0xfffff000;
	*((u_int *)(plx_vaddr + 0x04)) = 1;
	*((u_int *)(plx_vaddr + 0x18)) = 0x40030043;
	*((u_int *)(plx_vaddr + 0x1c)) = 0xff000000;
	*((u_int *)(plx_vaddr + 0x20)) = 0;
	*((u_int *)(plx_vaddr + 0x28)) = 0xe9;
	*((u_int *)(plx_vaddr + 0x68)) = 0x10900;

	/*
	 * Get info from card.
	 *
	 * Only look for the second port if the first exists. Too many things
	 * will break if we have only a second port.
	 */
	fecr = *fecrp;
	numports = 0;

	if (((fecr & SR_FECR_ID0) >> SR_FE_ID0_SHFT) != SR_FE_ID_NONE) {
		numports++;
		if (((fecr & SR_FECR_ID1) >> SR_FE_ID1_SHFT) != SR_FE_ID_NONE)
			numports++;
	}
	if (numports == 0)
		return NULL;

	hc = sr_hardc_pci;
	hcp = &sr_hardc_pci;

	while (hc) {
		hcp = &hc->next;
		hc = hc->next;
	}

	hc = malloc(sizeof(struct sr_hardc), M_DEVBUF, M_WAITOK);
	*hcp = hc;
	bzero(hc, sizeof(struct sr_hardc));

	hc->sc = malloc(numports * sizeof(struct sr_softc),
			M_DEVBUF, M_WAITOK);
	bzero(hc->sc, numports * sizeof(struct sr_softc));

	hc->numports = numports;
	hc->cunit = unit;
	hc->cardtype = SR_CRD_N2PCI;
	hc->plx_base = (caddr_t)plx_vaddr;
	hc->sca_base = sca_vaddr;

	hc->src_put8 = src_put8_mem;
	hc->src_put16 = src_put16_mem;
	hc->src_get8 = src_get8_mem;
	hc->src_get16 = src_get16_mem;

	/*
	 * Malloc area for tx and rx buffers. For now allocate SRC_WIN_SIZ
	 * (16k) for each buffer.
	 *
	 * Allocate the block below 16M because the N2pci card can only access
	 * 16M memory at a time.
	 *
	 * (We could actually allocate a contiguous block above the 16MB limit,
	 * but this would complicate card programming more than we want to
	 * right now -jrc)
	 */
	hc->memsize = 2 * hc->numports * SRC_WIN_SIZ;
	hc->mem_start = contigmalloc(hc->memsize,
				     M_DEVBUF,
				     M_NOWAIT,
				     0ul,
				     0xfffffful,
				     0x10000,
				     0x1000000);

	if (hc->mem_start == NULL) {
		printf("src%d: pci: failed to allocate buffer space.\n", unit);
		return NULL;
	}
	hc->winmsk = 0xffffffff;
	hc->mem_end = (caddr_t)((u_int)hc->mem_start + hc->memsize);
	hc->mem_pstart = kvtop(hc->mem_start);
	bzero(hc->mem_start, hc->memsize);

	for (pndx = 0; pndx < numports; pndx++) {
		int intf_sw;
		struct sr_softc *sc;

		sc = &hc->sc[pndx];

		switch (pndx) {
		case 1:
			intf_sw = fecr & SR_FECR_ID1 >> SR_FE_ID1_SHFT;
			break;
		case 0:
		default:
			intf_sw = fecr & SR_FECR_ID0 >> SR_FE_ID0_SHFT;
		}

#ifdef N2_TEST_SPEED
		if (sr_test_speed[pndx] > 0)
			sc->clk_cfg = SR_FLAGS_INT_CLK;
		else
#endif
			switch (intf_sw) {
			default:
			case SR_FE_ID_RS232:
			case SR_FE_ID_HSSI:
			case SR_FE_ID_RS422:
			case SR_FE_ID_TEST:
				break;

			case SR_FE_ID_V35:
				sc->clk_cfg = SR_FLAGS_EXT_SEP_CLK;
				break;

			case SR_FE_ID_X21:
				sc->clk_cfg = SR_FLAGS_EXT_CLK;
				break;
			}
	}

	*fecrp = SR_FECR_DTR0
	    | SR_FECR_DTR1
	    | SR_FECR_TE0
	    | SR_FECR_TE1;

	srattach(hc);

	return hc;
}

/*
 * Register the ports on the adapter.
 * Fill in the info for each port.
 * Attach each port to sppp and bpf.
 */
static int
srattach(struct sr_hardc *hc)
{
	struct sr_softc *sc = hc->sc;
	struct ifnet *ifp;
	int unit;		/* index: channel w/in card */

	/*
	 * Report Card configuration information before we start configuring
	 * each channel on the card...
	 */
	printf("src%d: %uK RAM (%d mempages) @ %08x-%08x, %u ports.\n",
	       hc->cunit, hc->memsize / 1024, hc->mempages,
	       (u_int)hc->mem_start, (u_int)hc->mem_end, hc->numports);

	src_init(hc);
	sr_init_sca(hc);

	/*
	 * Now configure each port on the card.
	 */
	for (unit = 0; unit < hc->numports; sc++, unit++) {
		sc->hc = hc;
		sc->subunit = unit;
		sc->unit = next_sc_unit;
		next_sc_unit++;
		sc->scachan = unit % NCHAN;

		sr_init_rx_dmac(sc);
		sr_init_tx_dmac(sc);
		sr_init_msci(sc);

		ifp = &sc->ifsppp.pp_if;
		ifp->if_softc = sc;
		ifp->if_unit = sc->unit;
		ifp->if_name = "sr";
		ifp->if_mtu = PP_MTU;
		ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
		ifp->if_ioctl = srioctl;
		ifp->if_start = srstart;
		ifp->if_watchdog = srwatchdog;

		printf("sr%d: Adapter %d, port %d.\n",
		       sc->unit, hc->cunit, sc->subunit);

		/*
		 * Despite the fact that we want to allow both PPP *and*
		 * Frame Relay access to a channel, due to the architecture
		 * of the system, we'll have to do the attach here.
		 *
		 * At some point I'll defer the attach to the "up" call and
		 * have the attach/detach performed when the interface is
		 * up/downed...
		 */
		sc->attached = 0;
		sc->protocol = N2_USE_PPP;	/* default protocol */

#if	0
		sc->ifsppp.pp_flags = PP_KEEPALIVE;
		sppp_attach((struct ifnet *)&sc->ifsppp);
#endif

		if_attach(ifp);

#if NBPFILTER > 0
		bpfattach(ifp, DLT_PPP, PPP_HEADER_LEN);
#endif
	}

	if (hc->mempages)
		SRC_SET_OFF(hc->iobase);

	return 1;
}

/*
 * N2 Interrupt Service Routine
 *
 * First figure out which SCA gave the interrupt.
 * Process it.
 * See if there is other interrupts pending.
 * Repeat until there no interrupts remain.
 */
void
srintr(int unit)
{
	struct sr_hardc *hc;

	hc = &sr_hardc[unit];
	srintr_hc(hc);

	return;
}

void
srintr_hc(struct sr_hardc *hc)
{
	sca_regs *sca = hc->sca;	/* MSCI register tree */
	u_char  isr0, isr1, isr2;	/* interrupt statii captured */

#if BUGGY > 1
	printf("sr: srintr_hc(hc=%08x)\n", hc);
#endif

	/*
	 * Since multiple interfaces may share this interrupt, we must loop
	 * until no interrupts are still pending service.
	 */
	while (1) {
		/*
		 * Read all three interrupt status registers from the N2
		 * card...
		 */
		isr0 = SRC_GET8(hc->sca_base, sca->isr0);
		isr1 = SRC_GET8(hc->sca_base, sca->isr1);
		isr2 = SRC_GET8(hc->sca_base, sca->isr2);

		/*
		 * If all three registers returned 0, we've finished
		 * processing interrupts from this device, so we can quit
		 * this loop...
		 */
		if ((isr0 | isr1 | isr2) == 0)
			break;

#if BUGGY > 2
		printf("src%d: srintr_hc isr0 %x, isr1 %x, isr2 %x\n",
			unit, isr0, isr1, isr2);
#endif

		/*
		 * Now we can dispatch the interrupts. Since we don't expect
		 * either MSCI or timer interrupts, we'll test for DMA
		 * interrupts first...
		 */
		if (isr1)	/* DMA-initiated interrupt */
			sr_dmac_intr(hc, isr1);

		if (isr0)	/* serial part IRQ? */
			sr_msci_intr(hc, isr0);

		if (isr2)	/* timer-initiated interrupt */
			sr_timer_intr(hc, isr2);
	}
}

/*
 * This will only start the transmitter. It is assumed that the data
 * is already there.
 * It is normally called from srstart() or sr_dmac_intr().
 */
static void
sr_xmit(struct sr_softc *sc)
{
	u_short cda_value;	/* starting descriptor */
	u_short eda_value;	/* ending descriptor */
	struct sr_hardc *hc;
	struct ifnet *ifp;	/* O/S Network Services */
	dmac_channel *dmac;	/* DMA channel registers */

#if BUGGY > 0
	printf("sr: sr_xmit( sc=%08x)\n", sc);
#endif

	hc = sc->hc;
	ifp = &sc->ifsppp.pp_if;
	dmac = &hc->sca->dmac[DMAC_TXCH(sc->scachan)];

	/*
	 * Get the starting and ending addresses of the chain to be
	 * transmitted and pass these on to the DMA engine on-chip.
	 */
	cda_value = sc->block[sc->txb_next_tx].txdesc + hc->mem_pstart;
	cda_value &= 0x00ffff;
	eda_value = sc->block[sc->txb_next_tx].txeda + hc->mem_pstart;
	eda_value &= 0x00ffff;

	SRC_PUT16(hc->sca_base, dmac->cda, cda_value);
	SRC_PUT16(hc->sca_base, dmac->eda, eda_value);

	/*
	 * Now we'll let the DMA status register know about this change
	 */
	SRC_PUT8(hc->sca_base, dmac->dsr, SCA_DSR_DE);

	sc->xmit_busy = 1;	/* mark transmitter busy */

#if BUGGY > 2
	printf("sr%d: XMIT  cda=%04x, eda=%4x, rcda=%08lx\n",
	       sc->unit, cda_value, eda_value,
	       sc->block[sc->txb_next_tx].txdesc + hc->mem_pstart);
#endif

	sc->txb_next_tx++;	/* update next transmit seq# */

	if (sc->txb_next_tx == SR_TX_BLOCKS)	/* handle wrap... */
		sc->txb_next_tx = 0;

	/*
	 * Finally, we'll set a timout (which will start srwatchdog())
	 * within the O/S network services layer...
	 */
	ifp->if_timer = 2;	/* Value in seconds. */
}

/*
 * This function will be called from the upper level when a user add a
 * packet to be send, and from the interrupt handler after a finished
 * transmit.
 *
 * NOTE: it should run at spl_imp().
 *
 * This function only place the data in the oncard buffers. It does not
 * start the transmition. sr_xmit() does that.
 *
 * Transmitter idle state is indicated by the IFF_OACTIVE flag.
 * The function that clears that should ensure that the transmitter
 * and its DMA is in a "good" idle state.
 */
static void
srstart(struct ifnet *ifp)
{
	struct sr_softc *sc;	/* channel control structure */
	struct sr_hardc *hc;	/* card control/config block */
	int len;		/* total length of a packet */
	int pkts;		/* packets placed in DPRAM */
	int tlen;		/* working length of pkt */
	u_int i;
	struct mbuf *mtx;	/* message buffer from O/S */
	u_char *txdata;		/* buffer address in DPRAM */
	sca_descriptor *txdesc;	/* working descriptor pointr */
	struct buf_block *blkp;

#if BUGGY > 0
	printf("sr: srstart( ifp=%08x)\n", ifp);
#endif

	sc = ifp->if_softc;
	hc = sc->hc;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	/*
	 * It is OK to set the memory window outside the loop because all tx
	 * buffers and descriptors are assumed to be in the same 16K window.
	 */
	if (hc->mempages) {
		SRC_SET_ON(hc->iobase);
		SRC_SET_MEM(hc->iobase, sc->block[0].txdesc);
	}

	/*
	 * Loop to place packets into DPRAM.
	 *
	 * We stay in this loop until there is nothing in
	 * the TX queue left or the tx buffers are full.
	 */
top_srstart:

	/*
	 * See if we have space for more packets.
	 */
	if (sc->txb_inuse == SR_TX_BLOCKS) {	/* out of space? */
		ifp->if_flags |= IFF_OACTIVE;	/* yes, mark active */

		if (hc->mempages)
			SRC_SET_OFF(hc->iobase);

#if BUGGY > 9
		printf("sr%d.srstart: sc->txb_inuse=%d; DPRAM full...\n",
		       sc->unit, sc->txb_inuse);
#endif
		return;
	}
	/*
	 * OK, the card can take more traffic.  Let's see if there's any
	 * pending from the system...
	 *
	 * NOTE:
	 * The architecture of the networking interface doesn't
	 * actually call us like 'write()', providing an address.  We get
	 * started, a lot like a disk strategy routine, and we actually call
	 * back out to the system to get traffic to send...
	 *
	 * NOTE:
	 * If we were gonna run through another layer, we would use a
	 * dispatch table to select the service we're getting a packet
	 * from...
	 */
	switch (sc->protocol) {
#if NFR > 0
	case N2_USE_FRP:
		mtx = fr_dequeue(ifp);
		break;
#endif
	case N2_USE_PPP:
	default:
		mtx = sppp_dequeue(ifp);
	}

	if (!mtx) {
		if (hc->mempages)
			SRC_SET_OFF(hc->iobase);
		return;
	}
	/*
	 * OK, we got a packet from the network services of the O/S. Now we
	 * can move it into the DPRAM (under control of the descriptors) and
	 * fire it off...
	 */
	pkts = 0;
	i = 0;			/* counts # of granules used */

	blkp = &sc->block[sc->txb_new];	/* address of free granule */
	txdesc = (sca_descriptor *)
	    (hc->mem_start + (blkp->txdesc & hc->winmsk));

	txdata = (u_char *)(hc->mem_start
			    + (blkp->txstart & hc->winmsk));

	/*
	 * Now we'll try to install as many packets as possible into the
	 * card's DP RAM buffers.
	 */
	for (;;) {		/* perform actual copy of packet */
		len = mtx->m_pkthdr.len;	/* length of message */

#if BUGGY > 1
		printf("sr%d.srstart: mbuf @ %08lx, %d bytes\n",
			   sc->unit, mtx, len);
#endif

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp, mtx);
#endif

		/*
		 * We can perform a straight copy because the tranmit
		 * buffers won't wrap.
		 */
		m_copydata(mtx, 0, len, txdata);

		/*
		 * Now we know how big the message is gonna be.  We must now
		 * construct the descriptors to drive this message out...
		 */
		tlen = len;
		while (tlen > SR_BUF_SIZ) {	/* loop for full granules */
			txdesc->stat = 0;	/* reset bits */
			txdesc->len = SR_BUF_SIZ;	/* size of granule */
			tlen -= SR_BUF_SIZ;

			txdesc++;	/* move to next dscr */
			txdata += SR_BUF_SIZ;	/* adjust data addr */
			i++;
		}

		/*
		 * This section handles the setting of the final piece of a
		 * message.
		 */
		txdesc->stat = SCA_DESC_EOM;
		txdesc->len = tlen;
		pkts++;

		/*
		 * prepare for subsequent packets (if any)
		 */
		txdesc++;
		txdata += SR_BUF_SIZ;	/* next mem granule */
		i++;		/* count of granules */

		/*
		 * OK, we've now placed the message into the DPRAM where it
		 * can be transmitted.  We'll now release the message memory
		 * and update the statistics...
		 */
		m_freem(mtx);
		++sc->ifsppp.pp_if.if_opackets;

		/*
		 * Check if we have space for another packet. XXX This is
		 * hardcoded.  A packet can't be larger than 3 buffers (3 x
		 * 512).
		 */
		if ((i + 3) >= blkp->txmax) {	/* enough remains? */
#if BUGGY > 9
			printf("sr%d.srstart: i=%d (%d pkts); card full.\n",
			       sc->unit, i, pkts);
#endif
			break;
		}
		/*
		 * We'll pull the next message to be sent (if any)
		 */
		switch (sc->protocol) {
#if NFR > 0
		case N2_USE_FRP:
			mtx = fr_dequeue(ifp);
			break;
#endif
		case N2_USE_PPP:
		default:
			mtx = sppp_dequeue(ifp);
		}

		if (!mtx) {	/* no message?  We're done! */
#if BUGGY > 9
			printf("sr%d.srstart: pending=0, pkts=%d\n",
			       sc->unit, pkts);
#endif
			break;
		}
	}

	blkp->txtail = i;	/* record next free granule */

	/*
	 * Mark the last descriptor, so that the SCA know where to stop.
	 */
	txdesc--;		/* back up to last descriptor in list */
	txdesc->stat |= SCA_DESC_EOT;	/* mark as end of list */

	/*
	 * Now we'll reset the transmit granule's descriptor address so we
	 * can record this in the structure and fire it off w/ the DMA
	 * processor of the serial chip...
	 */
	txdesc = (sca_descriptor *)blkp->txdesc;
	blkp->txeda = (u_short)((u_int)&txdesc[i]);

	sc->txb_inuse++;	/* update inuse status */
	sc->txb_new++;		/* new traffic wuz added */

	if (sc->txb_new == SR_TX_BLOCKS)
		sc->txb_new = 0;

	/*
	 * If the tranmitter wasn't marked as "busy" we will force it to be
	 * started...
	 */
	if (sc->xmit_busy == 0) {
		sr_xmit(sc);
#if BUGGY > 9
		printf("sr%d.srstart: called sr_xmit()\n", sc->unit);
#endif
	}
	goto top_srstart;
}

/*
 * Handle ioctl's at the device level, though we *will* call up
 * a layer...
 */
#if BUGGY > 2
static int bug_splats[] = {0, 0, 0, 0, 0, 0, 0, 0};
#endif

static int
srioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	int s, error, was_up, should_be_up;
	struct sppp *sp = (struct sppp *)ifp;
	struct sr_softc *sc = ifp->if_softc;

#if BUGGY > 0
	printf("sr%d: srioctl(ifp=%08x, cmd=%08x, data=%08x)\n",
	       ifp->if_unit, ifp, cmd, data);
#endif

	was_up = ifp->if_flags & IFF_RUNNING;

	if (cmd == SIOCSIFFLAGS) {
		/*
		 * First, handle an apparent protocol switch
		 */
#if NFR > 0
		if (was_up == 0)/* can only happen if DOWN */
			if (ifp->if_flags & IFF_LINK1)
				sc->protocol = N2_USE_FRP;
			else
				sc->protocol = N2_USE_PPP;
#else
		sc->protocol = N2_USE_PPP;
		ifp->if_flags &= ~IFF_LINK1;
#endif

		/*
		 * Next we can handle minor protocol point(s)
		 */
		if (ifp->if_flags & IFF_LINK2)
			sp->pp_flags |= PP_CISCO;
		else
			sp->pp_flags &= ~PP_CISCO;
	}
	/*
	 * Next, we'll allow the network service layer we've called process
	 * the ioctl...
	 */
	if ((sc->attached != 0)
	    && (sc->attached != sc->protocol)) {
		switch (sc->attached) {
#if NFR > 0
		case N2_USE_FRP:
			fr_detach(ifp);
			break;
#endif
		case N2_USE_PPP:
		default:
			sppp_detach(ifp);
			sc->ifsppp.pp_flags &= ~PP_KEEPALIVE;
		}

		sc->attached = 0;
	}
	if (sc->attached == 0) {
		switch (sc->protocol) {
#if NFR > 0
		case N2_USE_FRP:
			fr_attach(&sc->ifsppp.pp_if);
			break;
#endif
		case N2_USE_PPP:
		default:
			sc->ifsppp.pp_flags |= PP_KEEPALIVE;
			sppp_attach(&sc->ifsppp.pp_if);

			/*
			 * Shortcut the sppp tls/tlf actions to
			 * up/down events since our lower layer is
			 * always ready.
			 */
			sc->ifsppp.pp_tls = sc->ifsppp.pp_up;
			sc->ifsppp.pp_tlf = sc->ifsppp.pp_down;
		}

		sc->attached = sc->protocol;
	}
	switch (sc->protocol) {
#if NFR > 0
	case N2_USE_FRP:
		error = fr_ioctl(ifp, cmd, data);
		break;
#endif
	case N2_USE_PPP:
	default:
		error = sppp_ioctl(ifp, cmd, data);
	}

#if BUGGY > 1
	printf("sr%d: ioctl: ifsppp.pp_flags = %08x, if_flags %08x.\n",
	      ifp->if_unit, ((struct sppp *)ifp)->pp_flags, ifp->if_flags);
#endif

	if (error)
		return error;

	if ((cmd != SIOCSIFFLAGS) && (cmd != SIOCSIFADDR)) {
#if BUGGY > 2
		if (bug_splats[sc->unit]++ < 2) {
			printf("sr(%d).if_addrlist = %08x\n",
			       sc->unit, ifp->if_addrlist);
			printf("sr(%d).if_bpf = %08x\n",
			       sc->unit, ifp->if_bpf);
			printf("sr(%d).if_init = %08x\n",
			       sc->unit, ifp->if_init);
			printf("sr(%d).if_output = %08x\n",
			       sc->unit, ifp->if_output);
			printf("sr(%d).if_start = %08x\n",
			       sc->unit, ifp->if_start);
			printf("sr(%d).if_done = %08x\n",
			       sc->unit, ifp->if_done);
			printf("sr(%d).if_ioctl = %08x\n",
			       sc->unit, ifp->if_ioctl);
			printf("sr(%d).if_reset = %08x\n",
			       sc->unit, ifp->if_reset);
			printf("sr(%d).if_watchdog = %08x\n",
			       sc->unit, ifp->if_watchdog);
		}
#endif
		return 0;
	}

	s = splimp();
	should_be_up = ifp->if_flags & IFF_RUNNING;

	if (!was_up && should_be_up) {
		/*
		 * Interface should be up -- start it.
		 */
		sr_up(sc);
		srstart(ifp);

		/*
		 * XXX Clear the IFF_UP flag so that the link will only go
		 * up after sppp lcp and ipcp negotiation.
		 */
		ifp->if_flags &= ~IFF_UP;
	} else if (was_up && !should_be_up) {
		/*
		 * Interface should be down -- stop it.
		 */
		sr_down(sc);
		switch (sc->protocol) {
#if NFR > 0
		case N2_USE_FRP:
			fr_flush(ifp);
			break;
#endif
		case N2_USE_PPP:
		default:
			sppp_flush(ifp);
		}
	}
	splx(s);

#if BUGGY > 2
	if (bug_splats[sc->unit]++ < 2) {
		printf("sr(%d).if_addrlist = %08x\n",
		       sc->unit, ifp->if_addrlist);
		printf("sr(%d).if_bpf = %08x\n",
		       sc->unit, ifp->if_bpf);
		printf("sr(%d).if_init = %08x\n",
		       sc->unit, ifp->if_init);
		printf("sr(%d).if_output = %08x\n",
		       sc->unit, ifp->if_output);
		printf("sr(%d).if_start = %08x\n",
		       sc->unit, ifp->if_start);
		printf("sr(%d).if_done = %08x\n",
		       sc->unit, ifp->if_done);
		printf("sr(%d).if_ioctl = %08x\n",
		       sc->unit, ifp->if_ioctl);
		printf("sr(%d).if_reset = %08x\n",
		       sc->unit, ifp->if_reset);
		printf("sr(%d).if_watchdog = %08x\n",
		       sc->unit, ifp->if_watchdog);
	}
#endif

	return 0;
}

/*
 * This is to catch lost tx interrupts.
 */
static void
srwatchdog(struct ifnet *ifp)
{
	int     got_st0, got_st1, got_st3, got_dsr;
	struct sr_softc *sc = ifp->if_softc;
	struct sr_hardc *hc = sc->hc;
	msci_channel *msci = &hc->sca->msci[sc->scachan];
	dmac_channel *dmac = &sc->hc->sca->dmac[sc->scachan];

#if BUGGY > 0
	printf("srwatchdog(unit=%d)\n", unit);
#endif

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	ifp->if_oerrors++;	/* update output error count */

	got_st0 = SRC_GET8(hc->sca_base, msci->st0);
	got_st1 = SRC_GET8(hc->sca_base, msci->st1);
	got_st3 = SRC_GET8(hc->sca_base, msci->st3);
	got_dsr = SRC_GET8(hc->sca_base, dmac->dsr);

#if	0
	if (ifp->if_flags & IFF_DEBUG)
#endif
		printf("sr%d: transmit failed, "
		       "ST0 %02x, ST1 %02x, ST3 %02x, DSR %02x.\n",
		       sc->unit,
		       got_st0, got_st1, got_st3, got_dsr);

	if (SRC_GET8(hc->sca_base, msci->st1) & SCA_ST1_UDRN) {
		SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_TXABORT);
		SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_TXENABLE);
		SRC_PUT8(hc->sca_base, msci->st1, SCA_ST1_UDRN);
	}
	sc->xmit_busy = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (sc->txb_inuse && --sc->txb_inuse)
		sr_xmit(sc);

	srstart(ifp);	/* restart transmitter */
}

static void
sr_up(struct sr_softc *sc)
{
	u_int *fecrp;
	struct sr_hardc *hc = sc->hc;
	sca_regs *sca = hc->sca;
	msci_channel *msci = &sca->msci[sc->scachan];

#if BUGGY > 0
	printf("sr_up(sc=%08x)\n", sc);
#endif

	/*
	 * This section should really do the attach to the appropriate
	 * system service, be it frame relay or PPP...
	 */
	if (sc->attached == 0) {
		switch (sc->protocol) {
#if NFR > 0
		case N2_USE_FRP:
			fr_attach(&sc->ifsppp.pp_if);
			break;
#endif
		case N2_USE_PPP:
		default:
			sc->ifsppp.pp_flags |= PP_KEEPALIVE;
			sppp_attach(&sc->ifsppp.pp_if);
	
			/*
			 * Shortcut the sppp tls/tlf actions to
			 * up/down events since our lower layer is
			 * always ready.
			 */
			sc->ifsppp.pp_tls = sc->ifsppp.pp_up;
			sc->ifsppp.pp_tlf = sc->ifsppp.pp_down;
	}

		sc->attached = sc->protocol;
	}

	/*
	 * Enable transmitter and receiver. Raise DTR and RTS. Enable
	 * interrupts.
	 *
	 * XXX What about using AUTO mode in msci->md0 ???
	 */
	SRC_PUT8(hc->sca_base, msci->ctl,
		 SRC_GET8(hc->sca_base, msci->ctl) & ~SCA_CTL_RTS);

	if (sc->scachan == 0)
		switch (hc->cardtype) {
		case SR_CRD_N2:
			outb(hc->iobase + SR_MCR,
			     (inb(hc->iobase + SR_MCR) & ~SR_MCR_DTR0));
			break;
		case SR_CRD_N2PCI:
			fecrp = (u_int *)(hc->sca_base + SR_FECR);
			*fecrp &= ~SR_FECR_DTR0;
			break;
		}
	else
		switch (hc->cardtype) {
		case SR_CRD_N2:
			outb(hc->iobase + SR_MCR,
			     (inb(hc->iobase + SR_MCR) & ~SR_MCR_DTR1));
			break;
		case SR_CRD_N2PCI:
			fecrp = (u_int *)(hc->sca_base + SR_FECR);
			*fecrp &= ~SR_FECR_DTR1;
			break;
		}

	if (sc->scachan == 0) {
		SRC_PUT8(hc->sca_base, sca->ier0,
			 SRC_GET8(hc->sca_base, sca->ier0) | 0x000F);
		SRC_PUT8(hc->sca_base, sca->ier1,
			 SRC_GET8(hc->sca_base, sca->ier1) | 0x000F);
	} else {
		SRC_PUT8(hc->sca_base, sca->ier0,
			 SRC_GET8(hc->sca_base, sca->ier0) | 0x00F0);
		SRC_PUT8(hc->sca_base, sca->ier1,
			 SRC_GET8(hc->sca_base, sca->ier1) | 0x00F0);
	}

	SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_RXENABLE);
	inb(hc->iobase);	/* XXX slow it down a bit. */
	SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_TXENABLE);

#ifdef USE_MODEMCK
	if (sr_watcher == 0)
		sr_modemck(NULL);
#endif
}

static void
sr_down(struct sr_softc *sc)
{
	u_int *fecrp;
	struct sr_hardc *hc = sc->hc;
	sca_regs *sca = hc->sca;
	msci_channel *msci = &sca->msci[sc->scachan];

#if BUGGY > 0
	printf("sr_down(sc=%08x)\n", sc);
#endif

	/*
	 * Disable transmitter and receiver. Lower DTR and RTS. Disable
	 * interrupts.
	 */
	SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_RXDISABLE);
	inb(hc->iobase);	/* XXX slow it down a bit. */
	SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_TXDISABLE);

	SRC_PUT8(hc->sca_base, msci->ctl,
		 SRC_GET8(hc->sca_base, msci->ctl) | SCA_CTL_RTS);

	if (sc->scachan == 0)
		switch (hc->cardtype) {
		case SR_CRD_N2:
			outb(hc->iobase + SR_MCR,
			     (inb(hc->iobase + SR_MCR) | SR_MCR_DTR0));
			break;
		case SR_CRD_N2PCI:
			fecrp = (u_int *)(hc->sca_base + SR_FECR);
			*fecrp |= SR_FECR_DTR0;
			break;
		}
	else
		switch (hc->cardtype) {
		case SR_CRD_N2:
			outb(hc->iobase + SR_MCR,
			     (inb(hc->iobase + SR_MCR) | SR_MCR_DTR1));
			break;
		case SR_CRD_N2PCI:
			fecrp = (u_int *)(hc->sca_base + SR_FECR);
			*fecrp |= SR_FECR_DTR1;
			break;
		}

	if (sc->scachan == 0) {
		SRC_PUT8(hc->sca_base, sca->ier0,
			 SRC_GET8(hc->sca_base, sca->ier0) & ~0x0F);
		SRC_PUT8(hc->sca_base, sca->ier1,
			 SRC_GET8(hc->sca_base, sca->ier1) & ~0x0F);
	} else {
		SRC_PUT8(hc->sca_base, sca->ier0,
			 SRC_GET8(hc->sca_base, sca->ier0) & ~0xF0);
		SRC_PUT8(hc->sca_base, sca->ier1,
			 SRC_GET8(hc->sca_base, sca->ier1) & ~0xF0);
	}

	/*
	 * This section does the detach from the currently configured net
	 * service, be it frame relay or PPP...
	 */
	switch (sc->protocol) {
#if NFR > 0
	case N2_USE_FRP:
		fr_detach(&sc->ifsppp.pp_if);
		break;
#endif
	case N2_USE_PPP:
	default:
		sppp_detach(&sc->ifsppp.pp_if);
	}

	sc->attached = 0;
}

/*
 * Initialize the card, allocate memory for the sr_softc structures
 * and fill in the pointers.
 */
static void
src_init(struct sr_hardc *hc)
{
	struct sr_softc *sc = hc->sc;
	int x;
	u_int chanmem;
	u_int bufmem;
	u_int next;
	u_int descneeded;

#if BUGGY > 0
	printf("src_init(hc=%08x)\n", hc);
#endif

	chanmem = hc->memsize / hc->numports;
	next = 0;

	for (x = 0; x < hc->numports; x++, sc++) {
		int blk;

		for (blk = 0; blk < SR_TX_BLOCKS; blk++) {
			sc->block[blk].txdesc = next;
			bufmem = (16 * 1024) / SR_TX_BLOCKS;
			descneeded = bufmem / SR_BUF_SIZ;

			sc->block[blk].txstart = sc->block[blk].txdesc
			    + ((((descneeded * sizeof(sca_descriptor))
				 / SR_BUF_SIZ) + 1)
			       * SR_BUF_SIZ);

			sc->block[blk].txend = next + bufmem;
			sc->block[blk].txmax =
			    (sc->block[blk].txend - sc->block[blk].txstart)
			    / SR_BUF_SIZ;
			next += bufmem;

#if BUGGY > 2
			printf("sr%d: blk %d: txdesc %08x, txstart %08x\n",
			       sc->unit, blk,
			       sc->block[blk].txdesc, sc->block[blk].txstart);
#endif
		}

		sc->rxdesc = next;
		bufmem = chanmem - (bufmem * SR_TX_BLOCKS);
		descneeded = bufmem / SR_BUF_SIZ;
		sc->rxstart = sc->rxdesc +
		    ((((descneeded * sizeof(sca_descriptor)) /
		       SR_BUF_SIZ) + 1) * SR_BUF_SIZ);
		sc->rxend = next + bufmem;
		sc->rxmax = (sc->rxend - sc->rxstart) / SR_BUF_SIZ;
		next += bufmem;
	}
}

/*
 * The things done here are channel independent.
 *
 * Configure the sca waitstates.
 * Configure the global interrupt registers.
 * Enable master dma enable.
 */
static void
sr_init_sca(struct sr_hardc *hc)
{
	sca_regs *sca = hc->sca;

#if BUGGY > 0
	printf("sr_init_sca(hc=%08x)\n", hc);
#endif

	/*
	 * Do the wait registers. Set everything to 0 wait states.
	 */
	SRC_PUT8(hc->sca_base, sca->pabr0, 0);
	SRC_PUT8(hc->sca_base, sca->pabr1, 0);
	SRC_PUT8(hc->sca_base, sca->wcrl, 0);
	SRC_PUT8(hc->sca_base, sca->wcrm, 0);
	SRC_PUT8(hc->sca_base, sca->wcrh, 0);

	/*
	 * Configure the interrupt registers. Most are cleared until the
	 * interface is configured.
	 */
	SRC_PUT8(hc->sca_base, sca->ier0, 0x00);	/* MSCI interrupts. */
	SRC_PUT8(hc->sca_base, sca->ier1, 0x00);	/* DMAC interrupts */
	SRC_PUT8(hc->sca_base, sca->ier2, 0x00);	/* TIMER interrupts. */
	SRC_PUT8(hc->sca_base, sca->itcr, 0x00);	/* Use ivr and no intr
							 * ack */
	SRC_PUT8(hc->sca_base, sca->ivr, 0x40);	/* Interrupt vector. */
	SRC_PUT8(hc->sca_base, sca->imvr, 0x40);

	/*
	 * Configure the timers. XXX Later
	 */

	/*
	 * Set the DMA channel priority to rotate between all four channels.
	 *
	 * Enable all dma channels.
	 */
	SRC_PUT8(hc->sca_base, sca->pcr, SCA_PCR_PR2);
	SRC_PUT8(hc->sca_base, sca->dmer, SCA_DMER_EN);
}

/*
 * Configure the msci
 *
 * NOTE: The serial port configuration is hardcoded at the moment.
 */
static void
sr_init_msci(struct sr_softc *sc)
{
	int portndx;		/* on-board port number */
	u_int mcr_v;		/* contents of modem control */
	u_int *fecrp;		/* pointer for PCI's MCR i/o */
	struct sr_hardc *hc = sc->hc;
	msci_channel *msci = &hc->sca->msci[sc->scachan];
#ifdef N2_TEST_SPEED
	int br_v;		/* contents for BR divisor */
	int etcndx;		/* index into ETC table */
	int fifo_v, gotspeed;	/* final tabled speed found */
	int tmc_v;		/* timer control register */
	int wanted;		/* speed (bitrate) wanted... */
	struct rate_line *rtp;
#endif

	portndx = sc->scachan;

#if BUGGY > 0
	printf("sr: sr_init_msci( sc=%08x)\n", sc);
#endif

	SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_RESET);
	SRC_PUT8(hc->sca_base, msci->md0, SCA_MD0_CRC_1 |
		 SCA_MD0_CRC_CCITT |
		 SCA_MD0_CRC_ENABLE |
		 SCA_MD0_MODE_HDLC);
	SRC_PUT8(hc->sca_base, msci->md1, SCA_MD1_NOADDRCHK);
	SRC_PUT8(hc->sca_base, msci->md2, SCA_MD2_DUPLEX | SCA_MD2_NRZ);

	/*
	 * According to the manual I should give a reset after changing the
	 * mode registers.
	 */
	SRC_PUT8(hc->sca_base, msci->cmd, SCA_CMD_RXRESET);
	SRC_PUT8(hc->sca_base, msci->ctl, SCA_CTL_IDLPAT |
		 SCA_CTL_UDRNC |
		 SCA_CTL_RTS);

	/*
	 * XXX Later we will have to support different clock settings.
	 */
	switch (sc->clk_cfg) {
	default:
#if BUGGY > 0
		printf("sr%: clk_cfg=%08x, selected default clock.\n",
		       portndx, sc->clk_cfg);
#endif
		/* FALLTHROUGH */
	case SR_FLAGS_EXT_CLK:
		/*
		 * For now all interfaces are programmed to use the RX clock
		 * for the TX clock.
		 */

#if BUGGY > 0
		printf("sr%d: External Clock Selected.\n", portndx);
#endif

		SRC_PUT8(hc->sca_base, msci->rxs, 0);
		SRC_PUT8(hc->sca_base, msci->txs, 0);
		break;

	case SR_FLAGS_EXT_SEP_CLK:
#if BUGGY > 0
		printf("sr%d: Split Clocking Selected.\n", portndx);
#endif

#if	1
		SRC_PUT8(hc->sca_base, msci->rxs, 0);
		SRC_PUT8(hc->sca_base, msci->txs, 0);
#else
		SRC_PUT8(hc->sca_base, msci->rxs,
			 SCA_RXS_CLK_RXC0 | SCA_RXS_DIV1);

		/*
		 * We need to configure the internal bit clock for the
		 * transmitter's channel...
		 */
		SRC_PUT8(hc->sca_base, msci->txs,
			 SCA_TXS_CLK_RX | SCA_TXS_DIV1);
#endif
		break;

	case SR_FLAGS_INT_CLK:
#if BUGGY > 0
		printf("sr%d: Internal Clocking selected.\n", portndx);
#endif

		/*
		 * XXX I do need some code to set the baud rate here!
		 */
#ifdef N2_TEST_SPEED
		switch (hc->cardtype) {
		case SR_CRD_N2PCI:
			fecrp = (u_int *)(hc->sca_base + SR_FECR);
			mcr_v = *fecrp;
			etcndx = 2;
			break;
		case SR_CRD_N2:
		default:
			mcr_v = inb(hc->iobase + SR_MCR);
			etcndx = 0;
		}

		fifo_v = 0x10;	/* stolen from Linux version */

		/*
		 * search for appropriate speed in table, don't calc it:
		 */
		wanted = sr_test_speed[portndx];
		rtp = &n2_rates[0];	/* point to first table item */

		while ((rtp->target > 0)	/* search table for speed */
		       &&(rtp->target != wanted))
			rtp++;

		/*
		 * We've searched the table for a matching speed.  If we've
		 * found the correct rate line, we'll get the pre-calc'd
		 * values for the TMC and baud rate divisor for subsequent
		 * use...
		 */
		if (rtp->target > 0) {	/* use table-provided values */
			gotspeed = wanted;
			tmc_v = rtp->tmc_reg;
			br_v = rtp->br_reg;
		} else {	/* otherwise assume 1MBit comm rate */
			gotspeed = 10000;
			tmc_v = 5;
			br_v = 1;
		}

		/*
		 * Now we mask in the enable clock output for the MCR:
		 */
		mcr_v |= etc0vals[etcndx + portndx];

		/*
		 * Now we'll program the registers with these speed- related
		 * contents...
		 */
		SRC_PUT8(hc->sca_base, msci->tmc, tmc_v);
		SRC_PUT8(hc->sca_base, msci->trc0, fifo_v);
		SRC_PUT8(hc->sca_base, msci->rxs, SCA_RXS_CLK_INT + br_v);
		SRC_PUT8(hc->sca_base, msci->txs, SCA_TXS_CLK_INT + br_v);

		switch (hc->cardtype) {
		case SR_CRD_N2PCI:
			*fecrp = mcr_v;
			break;
		case SR_CRD_N2:
		default:
			outb(hc->iobase + SR_MCR, mcr_v);
		}

#if BUGGY > 0
		if (wanted != gotspeed)
			printf("sr%d: Speed wanted=%d, found=%d\n",
			       wanted, gotspeed);

		printf("sr%d: Internal Clock %dx100 BPS, tmc=%d, div=%d\n",
		       portndx, gotspeed, tmc_v, br_v);
#endif
#else
		SRC_PUT8(hc->sca_base, msci->rxs,
			 SCA_RXS_CLK_INT | SCA_RXS_DIV1);
		SRC_PUT8(hc->sca_base, msci->txs,
			 SCA_TXS_CLK_INT | SCA_TXS_DIV1);

		SRC_PUT8(hc->sca_base, msci->tmc, 5);

		if (portndx == 0)
			switch (hc->cardtype) {
			case SR_CRD_N2PCI:
				fecrp = (u_int *)(hc->sca_base + SR_FECR);
				*fecrp |= SR_FECR_ETC0;
				break;
			case SR_CRD_N2:
			default:
				mcr_v = inb(hc->iobase + SR_MCR);
				mcr_v |= SR_MCR_ETC0;
				outb(hc->iobase + SR_MCR, mcr_v);
			}
		else
			switch (hc->cardtype) {
			case SR_CRD_N2:
				mcr_v = inb(hc->iobase + SR_MCR);
				mcr_v |= SR_MCR_ETC1;
				outb(hc->iobase + SR_MCR, mcr_v);
				break;
			case SR_CRD_N2PCI:
				fecrp = (u_int *)(hc->sca_base + SR_FECR);
				*fecrp |= SR_FECR_ETC1;
				break;
			}
#endif
	}

	/*
	 * XXX Disable all interrupts for now. I think if you are using the
	 * dmac you don't use these interrupts.
	 */
	SRC_PUT8(hc->sca_base, msci->ie0, 0);
	SRC_PUT8(hc->sca_base, msci->ie1, 0x0C);
	SRC_PUT8(hc->sca_base, msci->ie2, 0);
	SRC_PUT8(hc->sca_base, msci->fie, 0);

	SRC_PUT8(hc->sca_base, msci->sa0, 0);
	SRC_PUT8(hc->sca_base, msci->sa1, 0);

	SRC_PUT8(hc->sca_base, msci->idl, 0x7E);	/* set flags value */

	SRC_PUT8(hc->sca_base, msci->rrc, 0x0E);
	SRC_PUT8(hc->sca_base, msci->trc0, 0x10);
	SRC_PUT8(hc->sca_base, msci->trc1, 0x1F);
}

/*
 * Configure the rx dma controller.
 */
static void
sr_init_rx_dmac(struct sr_softc *sc)
{
	struct sr_hardc *hc;
	dmac_channel *dmac;
	sca_descriptor *rxd;
	u_int cda_v, sarb_v, rxbuf, rxda, rxda_d;

#if BUGGY > 0
	printf("sr_init_rx_dmac(sc=%08x)\n", sc);
#endif

	hc = sc->hc;
	dmac = &hc->sca->dmac[DMAC_RXCH(sc->scachan)];

	if (hc->mempages)
		SRC_SET_MEM(hc->iobase, sc->rxdesc);

	/*
	 * This phase initializes the contents of the descriptor table
	 * needed to construct a circular buffer...
	 */
	rxd = (sca_descriptor *)(hc->mem_start + (sc->rxdesc & hc->winmsk));
	rxda_d = (u_int) hc->mem_start - (sc->rxdesc & ~hc->winmsk);

	for (rxbuf = sc->rxstart;
	     rxbuf < sc->rxend;
	     rxbuf += SR_BUF_SIZ, rxd++) {
		/*
		 * construct the circular chain...
		 */
		rxda = (u_int) & rxd[1] - rxda_d + hc->mem_pstart;
		rxd->cp = (u_short)(rxda & 0xffff);

		/*
		 * set the on-card buffer address...
		 */
		rxd->bp = (u_short)((rxbuf + hc->mem_pstart) & 0xffff);
		rxd->bpb = (u_char)(((rxbuf + hc->mem_pstart) >> 16) & 0xff);

		rxd->len = 0;	/* bytes resident w/in granule */
		rxd->stat = 0xff;	/* The sca write here when finished */
	}

	/*
	 * heal the chain so that the last entry points to the first...
	 */
	rxd--;
	rxd->cp = (u_short)((sc->rxdesc + hc->mem_pstart) & 0xffff);

	/*
	 * reset the reception handler's index...
	 */
	sc->rxhind = 0;

	/*
	 * We'll now configure the receiver's DMA logic...
	 */
	SRC_PUT8(hc->sca_base, dmac->dsr, 0);	/* Disable DMA transfer */
	SRC_PUT8(hc->sca_base, dmac->dcr, SCA_DCR_ABRT);

	/* XXX maybe also SCA_DMR_CNTE */
	SRC_PUT8(hc->sca_base, dmac->dmr, SCA_DMR_TMOD | SCA_DMR_NF);
	SRC_PUT16(hc->sca_base, dmac->bfl, SR_BUF_SIZ);

	cda_v = (u_short)((sc->rxdesc + hc->mem_pstart) & 0xffff);
	sarb_v = (u_char)(((sc->rxdesc + hc->mem_pstart) >> 16) & 0xff);

	SRC_PUT16(hc->sca_base, dmac->cda, cda_v);
	SRC_PUT8(hc->sca_base, dmac->sarb, sarb_v);

	rxd = (sca_descriptor *)sc->rxstart;

	SRC_PUT16(hc->sca_base, dmac->eda,
		  (u_short)((u_int) & rxd[sc->rxmax - 1] & 0xffff));

	SRC_PUT8(hc->sca_base, dmac->dir, 0xF0);


	SRC_PUT8(hc->sca_base, dmac->dsr, SCA_DSR_DE);	/* Enable DMA */
}

/*
 * Configure the TX DMA descriptors.
 * Initialize the needed values and chain the descriptors.
 */
static void
sr_init_tx_dmac(struct sr_softc *sc)
{
	int blk;
	u_int txbuf, txda, txda_d;
	struct sr_hardc *hc;
	sca_descriptor *txd;
	dmac_channel *dmac;
	struct buf_block *blkp;
	u_int x;
	u_int sarb_v;

#if BUGGY > 0
	printf("sr_init_tx_dmac(sc=%08x)\n", sc);
#endif

	hc = sc->hc;
	dmac = &hc->sca->dmac[DMAC_TXCH(sc->scachan)];

	if (hc->mempages)
		SRC_SET_MEM(hc->iobase, sc->block[0].txdesc);

	/*
	 * Initialize the array of descriptors for transmission
	 */
	for (blk = 0; blk < SR_TX_BLOCKS; blk++) {
		blkp = &sc->block[blk];
		txd = (sca_descriptor *)(hc->mem_start
					 + (blkp->txdesc & hc->winmsk));
		txda_d = (u_int) hc->mem_start
		    - (blkp->txdesc & ~hc->winmsk);

		x = 0;
		txbuf = blkp->txstart;
		for (; txbuf < blkp->txend; txbuf += SR_BUF_SIZ, txd++) {
			txda = (u_int) & txd[1] - txda_d + hc->mem_pstart;
			txd->cp = (u_short)(txda & 0xffff);

			txd->bp = (u_short)((txbuf + hc->mem_pstart)
					    & 0xffff);
			txd->bpb = (u_char)(((txbuf + hc->mem_pstart) >> 16)
					    & 0xff);
			txd->len = 0;
			txd->stat = 0;
			x++;
		}

		txd--;
		txd->cp = (u_short)((blkp->txdesc + hc->mem_pstart)
				    & 0xffff);

		blkp->txtail = (u_int)txd - (u_int)hc->mem_start;
	}

	SRC_PUT8(hc->sca_base, dmac->dsr, 0);	/* Disable DMA */
	SRC_PUT8(hc->sca_base, dmac->dcr, SCA_DCR_ABRT);
	SRC_PUT8(hc->sca_base, dmac->dmr, SCA_DMR_TMOD | SCA_DMR_NF);
	SRC_PUT8(hc->sca_base, dmac->dir,
		 SCA_DIR_EOT | SCA_DIR_BOF | SCA_DIR_COF);

	sarb_v = (sc->block[0].txdesc + hc->mem_pstart) >> 16;
	sarb_v &= 0x00ff;

	SRC_PUT8(hc->sca_base, dmac->sarb, (u_char) sarb_v);
}

/*
 * Look through the descriptors to see if there is a complete packet
 * available. Stop if we get to where the sca is busy.
 *
 * Return the length and status of the packet.
 * Return nonzero if there is a packet available.
 *
 * NOTE:
 * It seems that we get the interrupt a bit early. The updateing of
 * descriptor values is not always completed when this is called.
 */
static int
sr_packet_avail(struct sr_softc *sc, int *len, u_char *rxstat)
{
	int granules;	/* count of granules in pkt */
	int wki, wko;
	struct sr_hardc *hc;
	sca_descriptor *rxdesc;	/* current descriptor */
	sca_descriptor *endp;	/* ending descriptor */
	sca_descriptor *cda;	/* starting descriptor */

	hc = sc->hc;		/* get card's information */

	/*
	 * set up starting descriptor by pulling that info from the DMA half
	 * of the HD chip...
	 */
	wki = DMAC_RXCH(sc->scachan);
	wko = SRC_GET16(hc->sca_base, hc->sca->dmac[wki].cda);

	cda = (sca_descriptor *)(hc->mem_start + (wko & hc->winmsk));

#if BUGGY > 1
	printf("sr_packet_avail(): wki=%d, wko=%04x, cda=%08x\n",
	       wki, wko, cda);
#endif

	/*
	 * open the appropriate memory window and set our expectations...
	 */
	if (hc->mempages) {
		SRC_SET_MEM(hc->iobase, sc->rxdesc);
		SRC_SET_ON(hc->iobase);
	}
	rxdesc = (sca_descriptor *)
	    (hc->mem_start + (sc->rxdesc & hc->winmsk));
	endp = rxdesc;
	rxdesc = &rxdesc[sc->rxhind];
	endp = &endp[sc->rxmax];

	*len = 0;		/* reset result total length */
	granules = 0;		/* reset count of granules */

	/*
	 * This loop will scan descriptors, but it *will* puke up if we wrap
	 * around to our starting point...
	 */
	while (rxdesc != cda) {
		*len += rxdesc->len;	/* increment result length */
		granules++;

		/*
		 * If we hit a valid packet's completion we'll know we've
		 * got a live one, and that we can deliver the packet.
		 * Since we're only allowed to report a packet available,
		 * somebody else does that...
		 */
		if (rxdesc->stat & SCA_DESC_EOM) {	/* End Of Message */
			*rxstat = rxdesc->stat;	/* return closing */
#if BUGGY > 0
			printf("sr%d: PKT AVAIL len %d, %x, bufs %u.\n",
			       sc->unit, *len, *rxstat, granules);
#endif
			return 1;	/* indicate success */
		}
		/*
		 * OK, this packet take up multiple granules.  Move on to
		 * the next descriptor so we can consider it...
		 */
		rxdesc++;

		if (rxdesc == endp)	/* recognize & act on wrap point */
			rxdesc = (sca_descriptor *)
			    (hc->mem_start + (sc->rxdesc & hc->winmsk));
	}

	/*
	 * Nothing found in the DPRAM.  Let the caller know...
	 */
	*len = 0;
	*rxstat = 0;

	return 0;
}

/*
 * Copy a packet from the on card memory into a provided mbuf.
 * Take into account that buffers wrap and that a packet may
 * be larger than a buffer.
 */
static void
sr_copy_rxbuf(struct mbuf *m, struct sr_softc *sc, int len)
{
	struct sr_hardc *hc;
	sca_descriptor *rxdesc;
	u_int rxdata;
	u_int rxmax;
	u_int off = 0;
	u_int tlen;

#if BUGGY > 0
	printf("sr_copy_rxbuf(m=%08x,sc=%08x,len=%d)\n",
	       m, sc, len);
#endif

	hc = sc->hc;

	rxdata = sc->rxstart + (sc->rxhind * SR_BUF_SIZ);
	rxmax = sc->rxstart + (sc->rxmax * SR_BUF_SIZ);

	rxdesc = (sca_descriptor *)
	    (hc->mem_start + (sc->rxdesc & hc->winmsk));
	rxdesc = &rxdesc[sc->rxhind];

	/*
	 * Using the count of bytes in the received packet, we decrement it
	 * for each granule (controller by an SCA descriptor) to control the
	 * looping...
	 */
	while (len) {
		/*
		 * tlen gets the length of *this* granule... ...which is
		 * then copied to the target buffer.
		 */
		tlen = (len < SR_BUF_SIZ) ? len : SR_BUF_SIZ;

		if (hc->mempages)
			SRC_SET_MEM(hc->iobase, rxdata);

		bcopy(hc->mem_start + (rxdata & hc->winmsk),
		      mtod(m, caddr_t) +off,
		      tlen);

		off += tlen;
		len -= tlen;

		/*
		 * now, return to the descriptor's window in DPRAM and reset
		 * the descriptor we've just suctioned...
		 */
		if (hc->mempages)
			SRC_SET_MEM(hc->iobase, sc->rxdesc);

		rxdesc->len = 0;
		rxdesc->stat = 0xff;

		/*
		 * Move on to the next granule.  If we've any remaining
		 * bytes to process we'll just continue in our loop...
		 */
		rxdata += SR_BUF_SIZ;
		rxdesc++;

		if (rxdata == rxmax) {	/* handle the wrap point */
			rxdata = sc->rxstart;
			rxdesc = (sca_descriptor *)
			    (hc->mem_start + (sc->rxdesc & hc->winmsk));
		}
	}
}

/*
 * If single is set, just eat a packet. Otherwise eat everything up to
 * where cda points. Update pointers to point to the next packet.
 *
 * This handles "flushing" of a packet as received...
 *
 * If the "single" parameter is zero, all pending reeceive traffic will
 * be flushed out of existence.  A non-zero value will only drop the
 * *next* (currently) pending packet...
 */
static void
sr_eat_packet(struct sr_softc *sc, int single)
{
	struct sr_hardc *hc;
	sca_descriptor *rxdesc;	/* current descriptor being eval'd */
	sca_descriptor *endp;	/* last descriptor in chain */
	sca_descriptor *cda;	/* current start point */
	u_int loopcnt = 0;	/* count of packets flushed ??? */
	u_char stat;		/* captured status byte from descr */

	hc = sc->hc;
	cda = (sca_descriptor *)(hc->mem_start +
				 (SRC_GET16(hc->sca_base,
				  hc->sca->dmac[DMAC_RXCH(sc->scachan)].cda) &
				  hc->winmsk));

	/*
	 * loop until desc->stat == (0xff || EOM) Clear the status and
	 * length in the descriptor. Increment the descriptor.
	 */
	if (hc->mempages)
		SRC_SET_MEM(hc->iobase, sc->rxdesc);

	rxdesc = (sca_descriptor *)
	    (hc->mem_start + (sc->rxdesc & hc->winmsk));
	endp = rxdesc;
	rxdesc = &rxdesc[sc->rxhind];
	endp = &endp[sc->rxmax];

	/*
	 * allow loop, but abort it if we wrap completely...
	 */
	while (rxdesc != cda) {
		loopcnt++;

		if (loopcnt > sc->rxmax) {
			printf("sr%d: eat pkt %d loop, cda %x, "
			       "rxdesc %x, stat %x.\n",
			       sc->unit, loopcnt, (u_int) cda, (u_int) rxdesc,
			       rxdesc->stat);
			break;
		}
		stat = rxdesc->stat;

		rxdesc->len = 0;
		rxdesc->stat = 0xff;

		rxdesc++;
		sc->rxhind++;

		if (rxdesc == endp) {
			rxdesc = (sca_descriptor *)
			    (hc->mem_start + (sc->rxdesc & hc->winmsk));
			sc->rxhind = 0;
		}
		if (single && (stat == SCA_DESC_EOM))
			break;
	}

	/*
	 * Update the eda to the previous descriptor.
	 */
	rxdesc = (sca_descriptor *)sc->rxdesc;
	rxdesc = &rxdesc[(sc->rxhind + sc->rxmax - 2) % sc->rxmax];

	SRC_PUT16(hc->sca_base,
		  hc->sca->dmac[DMAC_RXCH(sc->scachan)].eda,
		  (u_short)((u_int)(rxdesc + hc->mem_pstart) & 0xffff));
}

/*
 * While there is packets available in the rx buffer, read them out
 * into mbufs and ship them off.
 */
static void
sr_get_packets(struct sr_softc *sc)
{
	u_char rxstat;		/* acquired status byte */
	int i;
	int pkts;		/* count of packets found */
	int rxndx;		/* rcv buffer index */
	int tries;		/* settling time counter */
	u_int len;		/* length of pending packet */
	struct sr_hardc *hc;	/* card-level information */
	sca_descriptor *rxdesc;	/* descriptor in memory */
	struct ifnet *ifp;	/* network intf ctl table */
	struct mbuf *m = NULL;	/* message buffer */

#if BUGGY > 0
	printf("sr_get_packets(sc=%08x)\n", sc);
#endif

	hc = sc->hc;
	ifp = &sc->ifsppp.pp_if;

	if (hc->mempages) {
		SRC_SET_MEM(hc->iobase, sc->rxdesc);
		SRC_SET_ON(hc->iobase);	/* enable shared memory */
	}
	pkts = 0;		/* reset count of found packets */

	/*
	 * for each complete packet in the receiving pool, process each
	 * packet...
	 */
	while (sr_packet_avail(sc, &len, &rxstat)) {	/* packet pending? */
		/*
		 * I have seen situations where we got the interrupt but the
		 * status value wasn't deposited.  This code should allow
		 * the status byte's value to settle...
		 */

		tries = 5;

		while ((rxstat == 0x00ff)
		       && --tries)
			sr_packet_avail(sc, &len, &rxstat);

#if BUGGY > 1
		printf("sr_packet_avail() returned len=%d, rxstat=%02ux\n",
		       len, rxstat);
#endif

		pkts++;

		/*
		 * OK, we've settled the incoming message status. We can now
		 * process it...
		 */
		if (((rxstat & SCA_DESC_ERRORS) == 0) && (len < MCLBYTES)) {
#if BUGGY > 1
			printf("sr%d: sr_get_packet() rxstat=%02x, len=%d\n",
			       sc->unit, rxstat, len);
#endif

			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				/*
				 * eat (flush) packet if get mbuf fail!!
				 */
				sr_eat_packet(sc, 1);
				continue;
			}
			/*
			 * construct control information for pass-off
			 */
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len = len;
			if (len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					/*
					 * We couldn't get a big enough
					 * message packet, so we'll send the
					 * packet to /dev/null...
					 */
					m_freem(m);
					sr_eat_packet(sc, 1);
					continue;
				}
			}
			/*
			 * OK, we've got a good message buffer.  Now we can
			 * copy the received message into it
			 */
			sr_copy_rxbuf(m, sc, len);	/* copy from DPRAM */

#if NBPFILTER > 0
			if (ifp->if_bpf)
				bpf_mtap(ifp, m);
#endif

#if BUGGY > 3
			{
				u_char *bp;

				bp = (u_char *)m;
				printf("sr%d: rcvd=%02x%02x%02x%02x%02x%02x\n",
				       sc->unit,
				       bp[0], bp[1], bp[2],
				       bp[4], bp[5], bp[6]);
			}
#endif

			/*
			 * Pass off the message to PPP, connecting it it to
			 * the system...
			 */
			switch (sc->protocol) {
#if NFR > 0
			case N2_USE_FRP:
				fr_input(ifp, m);
				break;
#endif
			case N2_USE_PPP:
			default:
				sppp_input(ifp, m);
			}

			ifp->if_ipackets++;

			/*
			 * Update the eda to the previous descriptor.
			 */
			i = (len + SR_BUF_SIZ - 1) / SR_BUF_SIZ;
			sc->rxhind = (sc->rxhind + i) % sc->rxmax;

			rxdesc = (sca_descriptor *)sc->rxdesc;
			rxndx = (sc->rxhind + sc->rxmax - 2) % sc->rxmax;
			rxdesc = &rxdesc[rxndx];

			SRC_PUT16(hc->sca_base,
				  hc->sca->dmac[DMAC_RXCH(sc->scachan)].eda,
				  (u_short)((u_int)(rxdesc + hc->mem_pstart)
					     & 0xffff));

		} else {
			int got_st3, got_cda, got_eda;
			int tries = 5;

			while((rxstat == 0xff) && --tries)
				sr_packet_avail(sc, &len, &rxstat);

			/*
			 * It look like we get an interrupt early
			 * sometimes and then the status is not
			 * filled in yet.
			 */
			if(tries && (tries != 5))
				continue;

			/*
			 * This chunk of code handles the error packets.
			 * We'll log them for posterity...
			 */
			sr_eat_packet(sc, 1);

			ifp->if_ierrors++;

			got_st3 = SRC_GET8(hc->sca_base,
				  hc->sca->msci[sc->scachan].st3);
			got_cda = SRC_GET16(hc->sca_base,
				  hc->sca->dmac[DMAC_RXCH(sc->scachan)].cda);
			got_eda = SRC_GET16(hc->sca_base,
				  hc->sca->dmac[DMAC_RXCH(sc->scachan)].eda);

#if BUGGY > 0
			printf("sr%d: Receive error chan %d, "
			       "stat %02x, msci st3 %02x,"
			       "rxhind %d, cda %04x, eda %04x.\n",
			       sc->unit, sc->scachan, rxstat,
			       got_st3, sc->rxhind, got_cda, got_eda);
#endif
		}
	}

#if BUGGY > 0
	printf("sr%d: sr_get_packets() found %d packet(s)\n",
	       sc->unit, pkts);
#endif

	if (hc->mempages)
		SRC_SET_OFF(hc->iobase);
}

/*
 * All DMA interrupts come here.
 *
 * Each channel has two interrupts.
 * Interrupt A for errors and Interrupt B for normal stuff like end
 * of transmit or receive dmas.
 */
static void
sr_dmac_intr(struct sr_hardc *hc, u_char isr1)
{
	u_char dsr;		/* contents of DMA Stat Reg */
	u_char dotxstart;	/* enables for tranmit part */
	int mch;		/* channel being processed */
	struct sr_softc *sc;	/* channel's softc structure */
	sca_regs *sca = hc->sca;
	dmac_channel *dmac;	/* dma structure of chip */

#if BUGGY > 0
	printf("sr_dmac_intr(hc=%08x,isr1=%04x)\n", hc, isr1);
#endif

	mch = 0;		/* assume chan0 on card */
	dotxstart = isr1;	/* copy for xmitter starts */

	/*
	 * Shortcut if there is no interrupts for dma channel 0 or 1.
	 * Skip processing for channel 0 if no incoming hit
	 */
	if ((isr1 & 0x0F) == 0) {
		mch = 1;
		isr1 >>= 4;
	}
	do {
		sc = &hc->sc[mch];

		/*
		 * Transmit channel - DMA Status Register Evaluation
		 */
		if (isr1 & 0x0C) {
			dmac = &sca->dmac[DMAC_TXCH(mch)];

			/*
			 * get the DMA Status Register contents and write
			 * back to reset interrupt...
			 */
			dsr = SRC_GET8(hc->sca_base, dmac->dsr);
			SRC_PUT8(hc->sca_base, dmac->dsr, dsr);

			/*
			 * Check for (& process) a Counter overflow
			 */
			if (dsr & SCA_DSR_COF) {
				printf("sr%d: TX DMA Counter overflow, "
				       "txpacket no %lu.\n",
				       sc->unit, sc->ifsppp.pp_if.if_opackets);
				sc->ifsppp.pp_if.if_oerrors++;
			}
			/*
			 * Check for (& process) a Buffer overflow
			 */
			if (dsr & SCA_DSR_BOF) {
				printf("sr%d: TX DMA Buffer overflow, "
				       "txpacket no %lu, dsr %02x, "
				       "cda %04x, eda %04x.\n",
				       sc->unit, sc->ifsppp.pp_if.if_opackets,
				       dsr,
				       SRC_GET16(hc->sca_base, dmac->cda),
				       SRC_GET16(hc->sca_base, dmac->eda));
				sc->ifsppp.pp_if.if_oerrors++;
			}
			/*
			 * Check for (& process) an End of Transfer (OK)
			 */
			if (dsr & SCA_DSR_EOT) {
				/*
				 * This should be the most common case.
				 *
				 * Clear the IFF_OACTIVE flag.
				 *
				 * Call srstart to start a new transmit if
				 * there is data to transmit.
				 */
#if BUGGY > 0
				printf("sr%d: TX Completed OK\n", sc->unit);
#endif
				sc->xmit_busy = 0;
				sc->ifsppp.pp_if.if_flags &= ~IFF_OACTIVE;
				sc->ifsppp.pp_if.if_timer = 0;

				if (sc->txb_inuse && --sc->txb_inuse)
					sr_xmit(sc);
			}
		}
		/*
		 * Receive channel processing of DMA Status Register
		 */
		if (isr1 & 0x03) {
			dmac = &sca->dmac[DMAC_RXCH(mch)];

			dsr = SRC_GET8(hc->sca_base, dmac->dsr);
			SRC_PUT8(hc->sca_base, dmac->dsr, dsr);

			/*
			 * End of frame processing (MSG OK?)
			 */
			if (dsr & SCA_DSR_EOM) {
#if BUGGY > 0
				int tt, ind;

				tt = sc->ifsppp.pp_if.if_ipackets;
				ind = sc->rxhind;
#endif

				sr_get_packets(sc);

#if BUGGY > 0
				if (tt == sc->ifsppp.pp_if.if_ipackets) {
					sca_descriptor *rxdesc;
					int i;

					printf("SR: RXINTR isr1 %x, dsr %x, "
					       "no data %d pkts, orxind %d.\n",
					       dotxstart, dsr, tt, ind);
					printf("SR: rxdesc %x, rxstart %x, "
					       "rxend %x, rxhind %d, "
					       "rxmax %d.\n",
					       sc->rxdesc, sc->rxstart,
					       sc->rxend, sc->rxhind,
					       sc->rxmax);
					printf("SR: cda %x, eda %x.\n",
					    SRC_GET16(hc->sca_base, dmac->cda),
					    SRC_GET16(hc->sca_base, dmac->eda));

					if (hc->mempages) {
						SRC_SET_ON(hc->iobase);
						SRC_SET_MEM(hc->iobase, sc->rxdesc);
					}
					rxdesc = (sca_descriptor *)
					         (hc->mem_start +
					          (sc->rxdesc & hc->winmsk));
					rxdesc = &rxdesc[sc->rxhind];

					for (i = 0; i < 3; i++, rxdesc++)
						printf("SR: rxdesc->stat %x, "
						       "len %d.\n",
						       rxdesc->stat,
						       rxdesc->len);

					if (hc->mempages)
						SRC_SET_OFF(hc->iobase);
				}
#endif
			}
			/*
			 * Check for Counter overflow
			 */
			if (dsr & SCA_DSR_COF) {
				printf("sr%d: RX DMA Counter overflow, "
				       "rxpkts %lu.\n",
				       sc->unit, sc->ifsppp.pp_if.if_ipackets);
				sc->ifsppp.pp_if.if_ierrors++;
			}
			/*
			 * Check for Buffer overflow
			 */
			if (dsr & SCA_DSR_BOF) {
				printf("sr%d: RX DMA Buffer overflow, "
				       "rxpkts %lu, rxind %d, "
				       "cda %x, eda %x, dsr %x.\n",
				       sc->unit, sc->ifsppp.pp_if.if_ipackets,
				       sc->rxhind,
				       SRC_GET16(hc->sca_base, dmac->cda),
				       SRC_GET16(hc->sca_base, dmac->eda),
				       dsr);

				/*
				 * Make sure we eat as many as possible.
				 * Then get the system running again.
				 */
				if (hc->mempages)
					SRC_SET_ON(hc->iobase);

				sr_eat_packet(sc, 0);
				sc->ifsppp.pp_if.if_ierrors++;

				SRC_PUT8(hc->sca_base,
					 sca->msci[mch].cmd,
					 SCA_CMD_RXMSGREJ);

				SRC_PUT8(hc->sca_base, dmac->dsr, SCA_DSR_DE);

#if BUGGY > 0
				printf("sr%d: RX DMA Buffer overflow, "
				       "rxpkts %lu, rxind %d, "
				       "cda %x, eda %x, dsr %x. After\n",
				       sc->unit,
				       sc->ifsppp.pp_if.if_ipackets,
				       sc->rxhind,
				       SRC_GET16(hc->sca_base, dmac->cda),
				       SRC_GET16(hc->sca_base, dmac->eda),
				       SRC_GET8(hc->sca_base, dmac->dsr));
#endif

				if (hc->mempages)
					SRC_SET_OFF(hc->iobase);
			}
			/*
			 * End of Transfer
			 */
			if (dsr & SCA_DSR_EOT) {
				/*
				 * If this happen, it means that we are
				 * receiving faster than what the processor
				 * can handle.
				 * 
				 * XXX We should enable the dma again.
				 */
				printf("sr%d: RX End of xfer, rxpkts %lu.\n",
				       sc->unit,
				       sc->ifsppp.pp_if.if_ipackets);
				sc->ifsppp.pp_if.if_ierrors++;
			}
		}
		isr1 >>= 4;	/* process next half of ISR */
		mch++;		/* and move to next channel */
	} while ((mch < NCHAN) && isr1);	/* loop for each chn */

	/*
	 * Now that we have done all the urgent things, see if we can fill
	 * the transmit buffers.
	 */
	for (mch = 0; mch < NCHAN; mch++) {
		if (dotxstart & 0x0C) {	/* TX initiation enabled? */
			sc = &hc->sc[mch];
			srstart(&sc->ifsppp.pp_if);
		}
		dotxstart >>= 4;/* shift for next channel */
	}
}

/*
 * Perform timeout on an FR channel 
 *
 * Establish a periodic check of open N2 ports;  If
 * a port is open/active, its DCD state is checked
 * and a loss of DCD is recognized (and eventually
 * processed).
 */
static void
sr_modemck(void *arg)
{
	u_int s;
	int card;		/* card index in table */
	int cards;		/* card list index */
	int mch;		/* channel on card */
	u_char dcd_v;		/* Data Carrier Detect */
	u_char got_st0;		/* contents of ST0 */
	u_char got_st1;		/* contents of ST1 */
	u_char got_st2;		/* contents of ST2 */
	u_char got_st3;		/* contents of ST3 */
	struct sr_hardc *hc;	/* card's configuration */
	struct sr_hardc *Card[16];/* up to 16 cards in system */
	struct sr_softc *sc;	/* channel's softc structure */
	struct ifnet *ifp;	/* interface control table */
	msci_channel *msci;	/* regs specific to channel */

	s = splimp();

#if	0
	if (sr_opens == 0) {	/* count of "up" channels */
		sr_watcher = 0;	/* indicate no watcher */
		splx(s);
		return;
	}
#endif

	sr_watcher = 1;		/* mark that we're online */

	/*
	 * Now we'll need a list of cards to process.  Since we can handle
	 * both ISA and PCI cards (and I didn't think of making this logic
	 * global YET) we'll generate a single table of card table
	 * addresses.
	 */
	cards = 0;

	for (card = 0; card < NSR; card++) {
		hc = &sr_hardc[card];

		if (hc->sc == (void *)0)
			continue;

		Card[cards++] = hc;
	}

	hc = sr_hardc_pci;

	while (hc) {
		Card[cards++] = hc;
		hc = hc->next;
	}

	/*
	 * OK, we've got work we can do.  Let's do it... (Please note that
	 * this code _only_ deals w/ ISA cards)
	 */
	for (card = 0; card < cards; card++) {
		hc = Card[card];/* get card table */

		for (mch = 0; mch < hc->numports; mch++) {
			sc = &hc->sc[mch];

			if (sc->attached == 0)
				continue;

			ifp = &sc->ifsppp.pp_if;

			/*
			 * if this channel isn't "up", skip it
			 */
			if ((ifp->if_flags & IFF_UP) == 0)
				continue;

			/*
			 * OK, now we can go looking at this channel's
			 * actual register contents...
			 */
			msci = &hc->sca->msci[sc->scachan];

			/*
			 * OK, now we'll look into the actual status of this
			 * channel...
			 * 
			 * I suck in more registers than strictly needed
			 */
			got_st0 = SRC_GET8(hc->sca_base, msci->st0);
			got_st1 = SRC_GET8(hc->sca_base, msci->st1);
			got_st2 = SRC_GET8(hc->sca_base, msci->st2);
			got_st3 = SRC_GET8(hc->sca_base, msci->st3);

			/*
			 * We want to see if the DCD signal is up (DCD is
			 * true if zero)
			 */
			dcd_v = (got_st3 & SCA_ST3_DCD) == 0;

			if (dcd_v == 0)
				printf("sr%d: DCD lost\n", sc->unit);
		}
	}

	/*
	 * OK, now set up for the next modem signal checking pass...
	 */
	timeout(sr_modemck, NULL, hz);

	splx(s);
}

static void
sr_msci_intr(struct sr_hardc *hc, u_char isr0)
{
	printf("src%d: SRINTR: MSCI\n", hc->cunit);
}

static void
sr_timer_intr(struct sr_hardc *hc, u_char isr2)
{
	printf("src%d: SRINTR: TIMER\n", hc->cunit);
}

/*
 ********************************* END ************************************
 */
