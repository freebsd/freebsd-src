/*
 *   Copyright (c) 1999 Gary Jennejohn. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *   a lot of code was borrowed from i4b_bchan.c and i4b_hscx.c
 *---------------------------------------------------------------------------
 *
 *	Fritz!Card PCI specific routines for isic driver
 *	------------------------------------------------
 *
 *	$Id: i4b_avm_fritz_pci.c,v 1.5 1999/05/05 11:50:21 hm Exp $
 *
 *      last edit-date: [Tue Mar 16 16:18:35 1999]
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)
#include "isic.h"
#include "opt_i4b.h"
#else
#define NISIC 1
#endif

#if NISIC > 0 && defined(AVM_A1_PCI)

#include <sys/param.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#ifdef __FreeBSD__
#include <machine/clock.h>
#include <i386/isa/isa_device.h>
#include <pci/pcivar.h>              /* for pcici_t */
#if __FreeBSD__ < 3
#include <pci/pcireg.h>
#include <pci/pcibus.h>
#endif /* __FreeBSD__ < 3 */
#else
#include <machine/bus.h>
#include <sys/device.h>
#endif

#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

#ifndef __FreeBSD__

#include <i4b/layer1/pci_isic.h>

/* PCI config map to use (only one in this driver) */
#define FRITZPCI_PORT0_MAPOFF	PCI_MAPREG_START+4

#endif

/* prototypes */
static void avma1pp_disable(struct isic_softc *);

#ifdef __FreeBSD__

static void avma1pp_intr(struct isic_softc *);
static void avma1pp_disable(struct isic_softc *);
void avma1pp_map_int(pcici_t , void *, unsigned *);
static void hscx_write_reg(int, u_int, u_int, struct isic_softc *);
static u_char hscx_read_reg(int, u_int, struct isic_softc *);
static u_int hscx_read_reg_int(int, u_int, struct isic_softc *);
static void hscx_read_fifo(int, void *, size_t, struct isic_softc *);
static void hscx_write_fifo(int, const void *, size_t, struct isic_softc *);
static void avma1pp_hscx_int_handler(struct isic_softc *);
static void avma1pp_hscx_intr(int, u_int, struct isic_softc *);
static void avma1pp_init_linktab(struct isic_softc *);
static void avma1pp_bchannel_setup(int, int, int, int);
static void avma1pp_bchannel_start(int, int);
static void avma1pp_hscx_init(struct isic_softc *, int, int);
static void avma1pp_bchannel_stat(int, int, bchan_statistics_t *);
static void avma1pp_set_linktab(int, int, drvr_link_t *);
static isdn_link_t * avma1pp_ret_linktab(int, int);
int isic_attach_avma1pp(int, u_int, u_int);
extern void isicintr_sc(struct isic_softc *);

#else

static int avma1pp_intr(void*);
static void avma1pp_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size);
static void avma1pp_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size);
static void avma1pp_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data);
static u_int8_t avma1pp_read_reg(struct isic_softc *sc, int what, bus_size_t offs);
static void hscx_write_fifo(int chan, const void *buf, size_t len, struct isic_softc *sc);
static void hscx_read_fifo(int chan, void *buf, size_t len, struct isic_softc *sc);
static void hscx_write_reg(int chan, u_int off, u_int val, struct isic_softc *sc);
static u_char hscx_read_reg(int chan, u_int off, struct isic_softc *sc);
static u_int hscx_read_reg_int(int chan, u_int off, struct isic_softc *sc);
static void avma1pp_fifo(isic_Bchan_t *chan, struct isic_softc *sc);
static void avma1pp_bchannel_stat(int unit, int h_chan, bchan_statistics_t *bsp);
static void avma1pp_map_int(struct pci_isic_softc *sc, struct pci_attach_args *pa);
static void avma1pp_bchannel_setup(int unit, int h_chan, int bprot, int activate);
static void avma1pp_init_linktab(struct isic_softc *);
#endif

/*---------------------------------------------------------------------------*
 *	AVM PCI Fritz!Card special registers
 *---------------------------------------------------------------------------*/

/*
 *	register offsets from i/o base
 */
#define STAT0_OFFSET            0x02
#define STAT1_OFFSET            0x03
#define ADDR_REG_OFFSET         0x04
/*#define MODREG_OFFSET		0x06
#define VERREG_OFFSET           0x07*/

/* these 2 are used to select an ISAC register set */
#define ISAC_LO_REG_OFFSET	0x04
#define ISAC_HI_REG_OFFSET	0x06

/* offset higher than this goes to the HI register set */
#define MAX_LO_REG_OFFSET	0x2f

/* mask for the offset */
#define ISAC_REGSET_MASK	0x0f

/* the offset from the base to the ISAC registers */
#define ISAC_REG_OFFSET		0x10

/* the offset from the base to the ISAC FIFO */
#define ISAC_FIFO		0x02

/* not really the HSCX, but sort of */
#define HSCX_FIFO		0x00
#define HSCX_STAT		0x04

/*
 *	AVM PCI Status Latch 0 read only bits
 */
#define ASL_IRQ_ISAC            0x01    /* ISAC  interrupt, active low */
#define ASL_IRQ_HSCX            0x02    /* HSX   interrupt, active low */
#define ASL_IRQ_TIMER           0x04    /* Timer interrupt, active low */
#define ASL_IRQ_BCHAN           ASL_IRQ_HSCX
/* actually active LOW */
#define ASL_IRQ_Pending         (ASL_IRQ_ISAC | ASL_IRQ_HSCX | ASL_IRQ_TIMER)

/*
 *	AVM Status Latch 0 write only bits
 */
#define ASL_RESET_ALL           0x01  /* reset siemens IC's, active 1 */
#define ASL_TIMERDISABLE        0x02  /* active high */
#define ASL_TIMERRESET          0x04  /* active high */
#define ASL_ENABLE_INT          0x08  /* active high */
#define ASL_TESTBIT	        0x10  /* active high */

/*
 *	AVM Status Latch 1 write only bits
 */
#define ASL1_INTSEL              0x0f  /* active high */
#define ASL1_ENABLE_IOM          0x80  /* active high */

/*
 * "HSCX" mode bits
 */
#define  HSCX_MODE_ITF_FLG	0x01
#define  HSCX_MODE_TRANS	0x02
#define  HSCX_MODE_CCR_7	0x04
#define  HSCX_MODE_CCR_16	0x08
#define  HSCX_MODE_TESTLOOP	0x80

/*
 * "HSCX" status bits
 */
#define  HSCX_STAT_RME		0x01
#define  HSCX_STAT_RDO		0x10
#define  HSCX_STAT_CRCVFRRAB	0x0E
#define  HSCX_STAT_CRCVFR	0x06
#define  HSCX_STAT_RML_MASK	0x3f00

/*
 * "HSCX" interrupt bits
 */
#define  HSCX_INT_XPR		0x80
#define  HSCX_INT_XDU		0x40
#define  HSCX_INT_RPR		0x20
#define  HSCX_INT_MASK		0xE0

/*
 * "HSCX" command bits
 */
#define  HSCX_CMD_XRS		0x80
#define  HSCX_CMD_XME		0x01
#define  HSCX_CMD_RRS		0x20
#define  HSCX_CMD_XML_MASK	0x3f00

/*
 * Commands and parameters are sent to the "HSCX" as a long, but the
 * fields are handled as bytes.
 *
 * The long contains:
 *	(prot << 16)|(txl << 8)|cmd
 *
 * where:
 *	prot = protocol to use
 *	txl = transmit length
 *	cmd = the command to be executed
 *
 * The fields are defined as u_char in struct isic_softc.
 *
 * Macro to coalesce the byte fields into a u_int
 */
#define AVMA1PPSETCMDLONG(f) (f) = ((sc->avma1pp_cmd) | (sc->avma1pp_txl << 8) \
 					| (sc->avma1pp_prot << 16))

#ifdef __FreeBSD__

/* "fake" addresses for the non-existent HSCX */
/* note: the unit number is in the lower byte for both the ISAC and "HSCX" */
#define HSCX0FAKE	0xfa000 /* read: fake0 */
#define HSCX1FAKE	0xfa100 /* read: fake1 */
#define IS_HSCX_MASK	0xfff00

#endif /* __FreeBSD__ */

/*
 * to prevent deactivating the "HSCX" when both channels are active we
 * define an HSCX_ACTIVE flag which is or'd into the channel's state
 * flag in avma1pp_bchannel_setup upon active and cleared upon deactivation.
 * It is set high to allow room for new flags.
 */
#define HSCX_AVMA1PP_ACTIVE	0x1000 

/*---------------------------------------------------------------------------*
 *	AVM read fifo routines
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__
static void		
avma1pp_read_fifo(void *buf, const void *base, size_t len)
{
	int unit;
	struct isic_softc *sc;

	unit = (int)base & 0xff;
	sc = &isic_sc[unit];

	/* check whether the target is an HSCX */
	if (((int)base & IS_HSCX_MASK) == HSCX0FAKE)
	{
		hscx_read_fifo(0, buf, len, sc);
		return;
	}
	if (((int)base & IS_HSCX_MASK)  == HSCX1FAKE)
	{
		hscx_read_fifo(1, buf, len, sc);
		return;
	}
	/* tell the board to access the ISAC fifo */
	outb(sc->sc_port + ADDR_REG_OFFSET, ISAC_FIFO);
	insb(sc->sc_port + ISAC_REG_OFFSET, (u_char *)buf, len);
}

static void
hscx_read_fifo(int chan, void *buf, size_t len, struct isic_softc *sc)
{
	u_int *ip;
	size_t cnt;

	outl(sc->sc_port + ADDR_REG_OFFSET, chan);
	ip = (u_int *)buf;
	cnt = 0;
	/* what if len isn't a multiple of sizeof(int) and buf is */
	/* too small ???? */
	while (cnt < len)
	{
		*ip++ = inl(sc->sc_port + ISAC_REG_OFFSET);
		cnt += 4;
	}
}

#else

static void
avma1pp_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h,  ADDR_REG_OFFSET, ISAC_FIFO);
			bus_space_read_multi_1(sc->sc_maps[0].t, sc->sc_maps[0].h,  ISAC_REG_OFFSET, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			hscx_read_fifo(0, buf, size, sc);
			break;
		case ISIC_WHAT_HSCXB:
			hscx_read_fifo(1, buf, size, sc);
			break;
	}
}

static void
hscx_read_fifo(int chan, void *buf, size_t len, struct isic_softc *sc)
{
	u_int32_t *ip;
	size_t cnt;

	bus_space_write_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ADDR_REG_OFFSET, chan);
	ip = (u_int32_t *)buf;
	cnt = 0;
	/* what if len isn't a multiple of sizeof(int) and buf is */
	/* too small ???? */
	while (cnt < len)
	{
		*ip++ = bus_space_read_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ISAC_REG_OFFSET);
		cnt += 4;
	}
}

#endif

/*---------------------------------------------------------------------------*
 *	AVM write fifo routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static void
avma1pp_write_fifo(void *base, const void *buf, size_t len)
{
	int unit;
	struct isic_softc *sc;

	unit = (int)base & 0xff;
	sc = &isic_sc[unit];

	/* check whether the target is an HSCX */
	if (((int)base & IS_HSCX_MASK) == HSCX0FAKE)
	{
		hscx_write_fifo(0, buf, len, sc);
		return;
	}
	if (((int)base & IS_HSCX_MASK)  == HSCX1FAKE)
	{
		hscx_write_fifo(1, buf, len, sc);
		return;
	}
	/* tell the board to use the ISAC fifo */
	outb(sc->sc_port + ADDR_REG_OFFSET, ISAC_FIFO);
	outsb(sc->sc_port + ISAC_REG_OFFSET, (u_char *)buf, len);
}

static void
hscx_write_fifo(int chan, const void *buf, size_t len, struct isic_softc *sc)
{
	register u_int *ip;
	register size_t cnt;
	isic_Bchan_t *Bchan = &sc->sc_chan[chan];

	sc->avma1pp_cmd &= ~HSCX_CMD_XME;
	sc->avma1pp_txl = 0;
	if (len != sc->sc_bfifolen)
	{
	  if (Bchan->bprot != BPROT_NONE)
		 sc->avma1pp_cmd |= HSCX_CMD_XME;
	  sc->avma1pp_txl = len;
	}
	
	cnt = 0; /* borrow cnt */
	AVMA1PPSETCMDLONG(cnt);
	hscx_write_reg(chan, HSCX_STAT, cnt, sc);

	ip = (u_int *)buf;
	cnt = 0;
	while (cnt < len)
	{
		outl(sc->sc_port + ISAC_REG_OFFSET, *ip++);
		cnt += 4;
	}
}

#else

static void
avma1pp_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h,  ADDR_REG_OFFSET, ISAC_FIFO);
			bus_space_write_multi_1(sc->sc_maps[0].t, sc->sc_maps[0].h,  ISAC_REG_OFFSET, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			hscx_write_fifo(0, buf, size, sc);
			break;
		case ISIC_WHAT_HSCXB:
			hscx_write_fifo(1, buf, size, sc);
			break;
	}
}

static void
hscx_write_fifo(int chan, const void *buf, size_t len, struct isic_softc *sc)
{
	u_int32_t *ip;
	size_t cnt;
	isic_Bchan_t *Bchan = &sc->sc_chan[chan];

	sc->avma1pp_cmd &= ~HSCX_CMD_XME;
	sc->avma1pp_txl = 0;
	if (len != sc->sc_bfifolen)
	{
	  if (Bchan->bprot != BPROT_NONE)
		 sc->avma1pp_cmd |= HSCX_CMD_XME;
	  sc->avma1pp_txl = len;
	}
	
	cnt = 0; /* borrow cnt */
	AVMA1PPSETCMDLONG(cnt);
	hscx_write_reg(chan, HSCX_STAT, cnt, sc);

	ip = (u_int32_t *)buf;
	cnt = 0;
	while (cnt < len)
	{
		bus_space_write_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ISAC_REG_OFFSET, *ip);
		ip++;
		cnt += 4;
	}
}
#endif

/*---------------------------------------------------------------------------*
 *	AVM write register routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static void
avma1pp_write_reg(u_char *base, u_int offset, u_int v)
{
	int unit;
	struct isic_softc *sc;
	u_char reg_bank;

	unit = (int)base & 0xff;
	sc = &isic_sc[unit];

	/* check whether the target is an HSCX */
	if (((int)base & IS_HSCX_MASK) == HSCX0FAKE)
	{
		hscx_write_reg(0, offset, v, sc);
		return;
	}
	if (((int)base & IS_HSCX_MASK) == HSCX0FAKE)
	{
		hscx_write_reg(1, offset, v, sc);
		return;
	}
	/* must be the ISAC */
	reg_bank = (offset > MAX_LO_REG_OFFSET) ? ISAC_HI_REG_OFFSET:ISAC_LO_REG_OFFSET;
#ifdef AVMA1PCI_DEBUG
	printf("write_reg bank %d  off %d.. ", reg_bank, offset);
#endif
	/* set the register bank */
	outb(sc->sc_port + ADDR_REG_OFFSET, reg_bank);
	outb(sc->sc_port + ISAC_REG_OFFSET + (offset & ISAC_REGSET_MASK), v);
}

static void
hscx_write_reg(int chan, u_int off, u_int val, struct isic_softc *sc)
{
	/* HACK */
	if (off == H_MASK)
		return;
	/* point at the correct channel */
	outl(sc->sc_port + ADDR_REG_OFFSET, chan);
	outl(sc->sc_port + ISAC_REG_OFFSET + off, val);
}

#else

static void
avma1pp_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	u_char reg_bank;
	switch (what) {
		case ISIC_WHAT_ISAC:
			reg_bank = (offs > MAX_LO_REG_OFFSET) ? ISAC_HI_REG_OFFSET:ISAC_LO_REG_OFFSET;
#ifdef AVMA1PCI_DEBUG
			printf("write_reg bank %d  off %ld.. ", (int)reg_bank, (long)offs);
#endif
			/* set the register bank */
			bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, ADDR_REG_OFFSET, reg_bank);
			bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, ISAC_REG_OFFSET + (offs & ISAC_REGSET_MASK), data);
			break;
		case ISIC_WHAT_HSCXA:
			hscx_write_reg(0, offs, data, sc);
			break;
		case ISIC_WHAT_HSCXB:
			hscx_write_reg(1, offs, data, sc);
			break;
	}
}

static void
hscx_write_reg(int chan, u_int off, u_int val, struct isic_softc *sc)
{
	/* HACK */
	if (off == H_MASK)
		return;
	/* point at the correct channel */
	bus_space_write_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ADDR_REG_OFFSET, chan);
	bus_space_write_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ISAC_REG_OFFSET + off, val);
}

#endif

/*---------------------------------------------------------------------------*
 *	AVM read register routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static u_char
avma1pp_read_reg(u_char *base, u_int offset)
{
	int unit;
	struct isic_softc *sc;
	u_char reg_bank;

	unit = (int)base & 0xff;
	sc = &isic_sc[unit];

	/* check whether the target is an HSCX */
	if (((int)base & IS_HSCX_MASK) == HSCX0FAKE)
		return(hscx_read_reg(0, offset, sc));
	if (((int)base & IS_HSCX_MASK)  == HSCX1FAKE)
		return(hscx_read_reg(1, offset, sc));
	/* must be the ISAC */
	reg_bank = (offset > MAX_LO_REG_OFFSET) ? ISAC_HI_REG_OFFSET:ISAC_LO_REG_OFFSET;
#ifdef AVMA1PCI_DEBUG
	printf("read_reg bank %d  off %d.. ", reg_bank, offset);
#endif
	/* set the register bank */
	outb(sc->sc_port + ADDR_REG_OFFSET, reg_bank);
	return(inb(sc->sc_port + ISAC_REG_OFFSET +
		(offset & ISAC_REGSET_MASK)));
}
#else
static u_int8_t
avma1pp_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	u_char reg_bank;
	switch (what) {
		case ISIC_WHAT_ISAC:
			reg_bank = (offs > MAX_LO_REG_OFFSET) ? ISAC_HI_REG_OFFSET:ISAC_LO_REG_OFFSET;
#ifdef AVMA1PCI_DEBUG
			printf("read_reg bank %d  off %ld.. ", (int)reg_bank, (long)offs);
#endif
			/* set the register bank */
			bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, ADDR_REG_OFFSET, reg_bank);
			return(bus_space_read_1(sc->sc_maps[0].t, sc->sc_maps[0].h, ISAC_REG_OFFSET +
				(offs & ISAC_REGSET_MASK)));
		case ISIC_WHAT_HSCXA:
			return hscx_read_reg(0, offs, sc);
		case ISIC_WHAT_HSCXB:
			return hscx_read_reg(1, offs, sc);
	}
	return 0;
}
#endif

static u_char
hscx_read_reg(int chan, u_int off, struct isic_softc *sc)
{
	return(hscx_read_reg_int(chan, off, sc) & 0xff);
}

/*
 * need to be able to return an int because the RBCH is in the 2nd
 * byte.
 */
static u_int
hscx_read_reg_int(int chan, u_int off, struct isic_softc *sc)
{
	/* HACK */
	if (off == H_ISTA)
		return(0);
	/* point at the correct channel */
#ifdef __FreeBSD__
	outl(sc->sc_port + ADDR_REG_OFFSET, chan);
	return(inl(sc->sc_port + ISAC_REG_OFFSET + off));
#else
	bus_space_write_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ADDR_REG_OFFSET, chan);
	return(bus_space_read_4(sc->sc_maps[0].t, sc->sc_maps[0].h, ISAC_REG_OFFSET + off));
#endif
}

/*---------------------------------------------------------------------------*
 *	isic_attach_avma1pp - attach Fritz!Card PCI
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
int
isic_attach_avma1pp(int unit, u_int iobase1, u_int iobase2)
{
	struct isic_softc *sc = &isic_sc[unit];
	u_int v;

	/* check max unit range */
	
	if(unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for AVM FRITZ/PCI!\n",
				unit, unit);
		return(0);	
	}	
	sc->sc_unit = unit;

	/* setup iobase */

	if((iobase1 <= 0) || (iobase1 > 0xffff))
	{
		printf("isic%d: Error, invalid iobase 0x%x specified for AVM FRITZ/PCI!\n",
			unit, iobase1);
		return(0);
	}
	sc->sc_port = iobase1;

	/* the ISAC lives at offset 0x10, but we can't use that. */
	/* instead, put the unit number into the lower byte - HACK */
	sc->sc_isac = (caddr_t)((int)(iobase1 & ~0xff) + unit);

	/* this thing doesn't have an HSCX, so fake the base addresses */
	/* put the unit number into the lower byte - HACK */
	HSCX_A_BASE = (caddr_t)(HSCX0FAKE + unit);
	HSCX_B_BASE = (caddr_t)(HSCX1FAKE + unit);

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = avma1pp_read_reg;
	sc->writereg = avma1pp_write_reg;

	sc->readfifo = avma1pp_read_fifo;
	sc->writefifo = avma1pp_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_AVMA1PCI;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* set up some other miscellaneous things */
	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* reset the card */
	/* the Linux driver does this to clear any pending ISAC interrupts */
	v = 0;
	v = ISAC_READ(I_STAR);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_STAR %x...", v);
#endif
	v = ISAC_READ(I_MODE);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_MODE %x...", v);
#endif
	v = ISAC_READ(I_ADF2);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_ADF2 %x...", v);
#endif
	v = ISAC_READ(I_ISTA);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_ISTA %x...", v);
#endif
	if (v & ISAC_ISTA_EXI)
	{
		 v = ISAC_READ(I_EXIR);
#ifdef AVMA1PCI_DEBUG
		 printf("avma1pp_attach: I_EXIR %x...", v);
#endif
	}
	v = ISAC_READ(I_CIRR);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_CIRR %x...", v);
#endif
	ISAC_WRITE(I_MASK, 0xff);
	/* the Linux driver does this to clear any pending HSCX interrupts */
	v = hscx_read_reg_int(0, HSCX_STAT, sc);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: 0 HSCX_STAT %x...", v);
#endif
	v = hscx_read_reg_int(1, HSCX_STAT, sc);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: 1 HSCX_STAT %x\n", v);
#endif

	outb(sc->sc_port + STAT0_OFFSET, ASL_RESET_ALL|ASL_TIMERDISABLE);
	DELAY(SEC_DELAY/100); /* 10 ms */
	outb(sc->sc_port + STAT0_OFFSET, ASL_TIMERRESET|ASL_ENABLE_INT|ASL_TIMERDISABLE);
	DELAY(SEC_DELAY/100); /* 10 ms */
#ifdef AVMA1PCI_DEBUG
	outb(sc->sc_port + STAT1_OFFSET, ASL1_ENABLE_IOM|sc->sc_irq);
	DELAY(SEC_DELAY/100); /* 10 ms */
	printf("after reset: S1 %#x\n", inb(sc->sc_port + STAT1_OFFSET));

	v = inl(sc->sc_port);
	printf("isic_attach_avma1pp: v %#x\n", v);
#endif

   /* from here to the end would normally be done in isic_pciattach */

	 printf("isic%d: ISAC %s (IOM-%c)\n", unit,
  		"2085 Version A1/A2 or 2086/2186 Version 1.1",
		 sc->sc_bustyp == BUS_TYPE_IOM1 ? '1' : '2');

	/* init the ISAC */
	isic_isac_init(sc);

	/* init the "HSCX" */
	avma1pp_bchannel_setup(sc->sc_unit, HSCX_CH_A, BPROT_NONE, 0);
	
	avma1pp_bchannel_setup(sc->sc_unit, HSCX_CH_B, BPROT_NONE, 0);

	/* can't use the normal B-Channel stuff */
	avma1pp_init_linktab(sc);

	/* set trace level */

	sc->sc_trace = TRACE_OFF;

	sc->sc_state = ISAC_IDLE;

	sc->sc_ibuf = NULL;
	sc->sc_ib = NULL;
	sc->sc_ilen = 0;

	sc->sc_obuf = NULL;
	sc->sc_op = NULL;
	sc->sc_ol = 0;
	sc->sc_freeflag = 0;

	sc->sc_obuf2 = NULL;
	sc->sc_freeflag2 = 0;

#if defined(__FreeBSD__) && __FreeBSD__ >=3
	callout_handle_init(&sc->sc_T3_callout);
	callout_handle_init(&sc->sc_T4_callout);	
#endif
	
	/* init higher protocol layers */
	
	MPH_Status_Ind(sc->sc_unit, STI_ATTACH, sc->sc_cardtyp);

	return(1);
}

#else

void
isic_attach_fritzPci(struct pci_isic_softc *psc, struct pci_attach_args *pa)
{
	struct isic_softc *sc = &psc->sc_isic;
	u_int v;

	isic_sc[sc->sc_unit] = sc;	/* XXX - hack! */

	/* setup io mappings */
	sc->sc_num_mappings = 1;
	MALLOC_MAPS(sc);
	sc->sc_maps[0].size = 0;
	if (pci_mapreg_map(pa, FRITZPCI_PORT0_MAPOFF, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_maps[0].t, &sc->sc_maps[0].h, NULL, NULL)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = avma1pp_read_reg;
	sc->writereg = avma1pp_write_reg;

	sc->readfifo = avma1pp_read_fifo;
	sc->writefifo = avma1pp_write_fifo;


	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_AVMA1PCI;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* this is no IPAC based card */
	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;
	
	/* init the card */
	/* the Linux driver does this to clear any pending ISAC interrupts */
	/* see if it helps any - XXXX */
	v = 0;
	v = ISAC_READ(I_STAR);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_STAR %x...", v);
#endif
	v = ISAC_READ(I_MODE);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_MODE %x...", v);
#endif
	v = ISAC_READ(I_ADF2);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_ADF2 %x...", v);
#endif
	v = ISAC_READ(I_ISTA);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_ISTA %x...", v);
#endif
	if (v & ISAC_ISTA_EXI)
	{
		 v = ISAC_READ(I_EXIR);
#ifdef AVMA1PCI_DEBUG
		 printf("avma1pp_attach: I_EXIR %x...", v);
#endif
	}
	v = ISAC_READ(I_CIRR);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_CIRR %x...", v);
#endif
	ISAC_WRITE(I_MASK, 0xff);
	/* the Linux driver does this to clear any pending HSCX interrupts */
	v = hscx_read_reg_int(0, HSCX_STAT, sc);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: 0 HSCX_STAT %x...", v);
#endif
	v = hscx_read_reg_int(1, HSCX_STAT, sc);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: 1 HSCX_STAT %x\n", v);
#endif

	bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, STAT0_OFFSET, ASL_RESET_ALL|ASL_TIMERDISABLE);
	DELAY(SEC_DELAY/100); /* 10 ms */
	bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, STAT0_OFFSET, ASL_TIMERRESET|ASL_ENABLE_INT|ASL_TIMERDISABLE);
	DELAY(SEC_DELAY/100); /* 10 ms */
#ifdef AVMA1PCI_DEBUG
	bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, STAT1_OFFSET, ASL1_ENABLE_IOM|sc->sc_irq);
	DELAY(SEC_DELAY/100); /* 10 ms */
	v = bus_space_read_1(sc->sc_maps[0].t, sc->sc_maps[0].h, STAT1_OFFSET);
	printf("after reset: S1 %#x\n", v);

	v = bus_space_read_4(sc->sc_maps[0].t, sc->sc_maps[0].h, 0);
	printf("isic_attach_avma1pp: v %#x\n", v);
#endif

	/* setup i4b infrastructure (have to roll our own here) */

	/* sc->sc_isac_version = ((ISAC_READ(I_RBCH)) >> 5) & 0x03; */
	 printf("%s: ISAC %s (IOM-%c)\n", sc->sc_dev.dv_xname,
  		"2085 Version A1/A2 or 2086/2186 Version 1.1",
		 sc->sc_bustyp == BUS_TYPE_IOM1 ? '1' : '2');

	/* init the ISAC */
	isic_isac_init(sc);

	/* init the "HSCX" */
	avma1pp_bchannel_setup(sc->sc_unit, HSCX_CH_A, BPROT_NONE, 0);
	
	avma1pp_bchannel_setup(sc->sc_unit, HSCX_CH_B, BPROT_NONE, 0);

	/* can't use the normal B-Channel stuff */
	avma1pp_init_linktab(sc);

	/* set trace level */

	sc->sc_trace = TRACE_OFF;

	sc->sc_state = ISAC_IDLE;

	sc->sc_ibuf = NULL;
	sc->sc_ib = NULL;
	sc->sc_ilen = 0;

	sc->sc_obuf = NULL;
	sc->sc_op = NULL;
	sc->sc_ol = 0;
	sc->sc_freeflag = 0;

	sc->sc_obuf2 = NULL;
	sc->sc_freeflag2 = 0;

	/* init higher protocol layers */
	
	MPH_Status_Ind(sc->sc_unit, STI_ATTACH, sc->sc_cardtyp);

	/* setup interrupt mapping */
	avma1pp_map_int(psc, pa);
}

#endif

/*
 * this is the real interrupt routine
 */
static void
avma1pp_hscx_intr(int h_chan, u_int stat, struct isic_softc *sc)
{
	register isic_Bchan_t *chan = &sc->sc_chan[h_chan];
	int activity = -1;
	u_int param = 0;
	
	DBGL1(L1_H_IRQ, "avma1pp_hscx_intr", ("%#x\n", stat));

	if((stat & HSCX_INT_XDU) && (chan->bprot != BPROT_NONE))/* xmit data underrun */
	{
		chan->stat_XDU++;			
		DBGL1(L1_H_XFRERR, "avma1pp_hscx_intr", ("xmit data underrun\n"));
		/* abort the transmission */
		sc->avma1pp_txl = 0;
		sc->avma1pp_cmd |= HSCX_CMD_XRS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd &= ~HSCX_CMD_XRS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);

		if (chan->out_mbuf_head != NULL)  /* don't continue to transmit this buffer */
		{
			i4b_Bfreembuf(chan->out_mbuf_head);
			chan->out_mbuf_cur = chan->out_mbuf_head = NULL;
		}
	}

	/*
	 * The following is based on examination of the Linux driver.
	 *
	 * The logic here is different than with a "real" HSCX; all kinds
	 * of information (interrupt/status bits) are in stat.
	 *		HSCX_INT_RPR indicates a receive interrupt
	 *			HSCX_STAT_RDO indicates an overrun condition, abort -
	 *			otherwise read the bytes ((stat & HSCX_STZT_RML_MASK) >> 8)
	 *			HSCX_STAT_RME indicates end-of-frame and apparently any
	 *			CRC/framing errors are only reported in this state.
	 *				if ((stat & HSCX_STAT_CRCVFRRAB) != HSCX_STAT_CRCVFR)
	 *					CRC/framing error
	 */
	
	if(stat & HSCX_INT_RPR)
	{
		register int fifo_data_len;
		int error = 0;
		/* always have to read the FIFO, so use a scratch buffer */
		u_char scrbuf[HSCX_FIFO_LEN];

		if(stat & HSCX_STAT_RDO)
		{
			chan->stat_RDO++;
			DBGL1(L1_H_XFRERR, "avma1pp_hscx_intr", ("receive data overflow\n"));
			error++;				
		}
	
		fifo_data_len = ((stat & HSCX_STAT_RML_MASK) >> 8);
		
		if(fifo_data_len == 0)
			fifo_data_len = sc->sc_bfifolen;

		/* ALWAYS read data from HSCX fifo */
	
		HSCX_RDFIFO(h_chan, scrbuf, fifo_data_len);
		chan->rxcount += fifo_data_len;

		/* all error conditions checked, now decide and take action */
		
		if(error == 0)
		{
			if(chan->in_mbuf == NULL)
			{
				if((chan->in_mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
					panic("L1 avma1pp_hscx_intr: RME, cannot allocate mbuf!\n");
				chan->in_cbptr = chan->in_mbuf->m_data;
				chan->in_len = 0;
			}

			if((chan->in_len + fifo_data_len) <= BCH_MAX_DATALEN)
			{
			   	/* OK to copy the data */
				bcopy(scrbuf, chan->in_cbptr, fifo_data_len);
				chan->in_cbptr += fifo_data_len;
				chan->in_len += fifo_data_len;

				/* setup mbuf data length */
					
				chan->in_mbuf->m_len = chan->in_len;
				chan->in_mbuf->m_pkthdr.len = chan->in_len;

				if(sc->sc_trace & TRACE_B_RX)
				{
					i4b_trace_hdr_t hdr;
					hdr.unit = sc->sc_unit;
					hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_NT;
					hdr.count = ++sc->sc_trace_bcount;
					MICROTIME(hdr.time);
					MPH_Trace_Ind(&hdr, chan->in_mbuf->m_len, chan->in_mbuf->m_data);
				}

				if (stat & HSCX_STAT_RME)
				{
				  if((stat & HSCX_STAT_CRCVFRRAB) == HSCX_STAT_CRCVFR)
				  {
					 (*chan->drvr_linktab->bch_rx_data_ready)(chan->drvr_linktab->unit);
					 activity = ACT_RX;
				
					 /* mark buffer ptr as unused */
					
					 chan->in_mbuf = NULL;
					 chan->in_cbptr = NULL;
					 chan->in_len = 0;
				  }
				  else
				  {
						chan->stat_CRC++;
						DBGL1(L1_H_XFRERR, "avma1pp_hscx_intr", ("CRC/RAB\n"));
					  if (chan->in_mbuf != NULL)
					  {
						  i4b_Bfreembuf(chan->in_mbuf);
						  chan->in_mbuf = NULL;
						  chan->in_cbptr = NULL;
						  chan->in_len = 0;
					  }
				  }
				}
			} /* END enough space in mbuf */
			else
			{
				 if(chan->bprot == BPROT_NONE)
				 {
					  /* setup mbuf data length */
				
					  chan->in_mbuf->m_len = chan->in_len;
					  chan->in_mbuf->m_pkthdr.len = chan->in_len;

					  if(sc->sc_trace & TRACE_B_RX)
					  {
							i4b_trace_hdr_t hdr;
							hdr.unit = sc->sc_unit;
							hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
							hdr.dir = FROM_NT;
							hdr.count = ++sc->sc_trace_bcount;
							MICROTIME(hdr.time);
							MPH_Trace_Ind(&hdr, chan->in_mbuf->m_len, chan->in_mbuf->m_data);
						}

					  /* move rx'd data to rx queue */

					  IF_ENQUEUE(&chan->rx_queue, chan->in_mbuf);
				
					  (*chan->drvr_linktab->bch_rx_data_ready)(chan->drvr_linktab->unit);

					  if(!(isic_hscx_silence(chan->in_mbuf->m_data, chan->in_mbuf->m_len)))
						 activity = ACT_RX;
				
					  /* alloc new buffer */
				
					  if((chan->in_mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
						 panic("L1 avma1pp_hscx_intr: RPF, cannot allocate new mbuf!\n");
	
					  /* setup new data ptr */
				
					  chan->in_cbptr = chan->in_mbuf->m_data;
	
					  /* OK to copy the data */
					  bcopy(scrbuf, chan->in_cbptr, fifo_data_len);

					  chan->in_cbptr += fifo_data_len;
					  chan->in_len = fifo_data_len;

					  chan->rxcount += fifo_data_len;
					}
				 else
					{
					  DBGL1(L1_H_XFRERR, "avma1pp_hscx_intr", ("RAWHDLC rx buffer overflow in RPF, in_len=%d\n", chan->in_len));
					  chan->in_cbptr = chan->in_mbuf->m_data;
					  chan->in_len = 0;
					}
			  }
		} /* if(error == 0) */
		else
		{
		  	/* land here for RDO */
			if (chan->in_mbuf != NULL)
			{
				i4b_Bfreembuf(chan->in_mbuf);
				chan->in_mbuf = NULL;
				chan->in_cbptr = NULL;
				chan->in_len = 0;
			}
			sc->avma1pp_txl = 0;
			sc->avma1pp_cmd |= HSCX_CMD_RRS;
			AVMA1PPSETCMDLONG(param);
			hscx_write_reg(h_chan, HSCX_STAT, param, sc);
			sc->avma1pp_cmd &= ~HSCX_CMD_RRS;
			AVMA1PPSETCMDLONG(param);
			hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		}
	}


	/* transmit fifo empty, new data can be written to fifo */
	
	if(stat & HSCX_INT_XPR)
	{
		/*
		 * for a description what is going on here, please have
		 * a look at isic_bchannel_start() in i4b_bchan.c !
		 */

		DBGL1(L1_H_IRQ, "avma1pp_hscx_intr", ("unit %d, chan %d - XPR, Tx Fifo Empty!\n", sc->sc_unit, h_chan));

		if(chan->out_mbuf_cur == NULL) 	/* last frame is transmitted */
		{
			IF_DEQUEUE(&chan->tx_queue, chan->out_mbuf_head);

			if(chan->out_mbuf_head == NULL)
			{
				chan->state &= ~HSCX_TX_ACTIVE;
				(*chan->drvr_linktab->bch_tx_queue_empty)(chan->drvr_linktab->unit);
			}
			else
			{
				chan->state |= HSCX_TX_ACTIVE;
				chan->out_mbuf_cur = chan->out_mbuf_head;
				chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;
				chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;

				if(sc->sc_trace & TRACE_B_TX)
				{
					i4b_trace_hdr_t hdr;
					hdr.unit = sc->sc_unit;
					hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_TE;
					hdr.count = ++sc->sc_trace_bcount;
					MICROTIME(hdr.time);
					MPH_Trace_Ind(&hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
				}
				
				if(chan->bprot == BPROT_NONE)
				{
					if(!(isic_hscx_silence(chan->out_mbuf_cur->m_data, chan->out_mbuf_cur->m_len)))
						activity = ACT_TX;
				}
				else
				{
					activity = ACT_TX;
				}
			}
		}
			
		isic_hscx_fifo(chan, sc);
	}

	/* call timeout handling routine */
	
	if(activity == ACT_RX || activity == ACT_TX)
		(*chan->drvr_linktab->bch_activity)(chan->drvr_linktab->unit, activity);
}

/*
 * this is the main routine which checks each channel and then calls
 * the real interrupt routine as appropriate
 */
static void
avma1pp_hscx_int_handler(struct isic_softc *sc)
{
	u_int stat;

	/* has to be a u_int because the byte count is in the 2nd byte */
	stat = hscx_read_reg_int(0, HSCX_STAT, sc);
	if (stat & HSCX_INT_MASK)
	  avma1pp_hscx_intr(0, stat, sc);
	stat = hscx_read_reg_int(1, HSCX_STAT, sc);
	if (stat & HSCX_INT_MASK)
	  avma1pp_hscx_intr(1, stat, sc);
}

static void
avma1pp_disable(struct isic_softc *sc)
{
#ifdef __FreeBSD__
	outb(sc->sc_port + STAT0_OFFSET, ASL_RESET_ALL|ASL_TIMERDISABLE);
#else
	bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, STAT0_OFFSET, ASL_RESET_ALL|ASL_TIMERDISABLE);
#endif
}

#ifdef __FreeBSD__
static void
avma1pp_intr(struct isic_softc *sc)
{
#define OURS	/* no return value accumulated */
#define	ISICINTR(sc)	isicintr_sc(sc)
#else
static int
avma1pp_intr(void * parm)
{
	struct isic_softc *sc = parm;
	int ret = 0;
#define OURS	ret = 1
#define	ISICINTR(sc)	isicintr(sc)
#endif
	u_char stat;

#ifdef __FreeBSD__
	stat = inb(sc->sc_port + STAT0_OFFSET);
#else
	stat = bus_space_read_1(sc->sc_maps[0].t, sc->sc_maps[0].h, STAT0_OFFSET);
#endif
	DBGL1(L1_H_IRQ, "avma1pp_intr", ("stat %x\n", stat));
	/* was there an interrupt from this card ? */
	if ((stat & ASL_IRQ_Pending) == ASL_IRQ_Pending)
#ifdef __FreeBSD__
		return; /* no */
#else
		return 0; /* no */
#endif
	/* interrupts are low active */
	if (!(stat & ASL_IRQ_TIMER))
	  DBGL1(L1_H_IRQ, "avma1pp_intr", ("timer interrupt ???\n"));
	if (!(stat & ASL_IRQ_HSCX))
	{
	  DBGL1(L1_H_IRQ, "avma1pp_intr", ("HSCX\n"));
		avma1pp_hscx_int_handler(sc);
		OURS;
	}
	if (!(stat & ASL_IRQ_ISAC))
	{
	  DBGL1(L1_H_IRQ, "avma1pp_intr", ("ISAC\n"));
		ISICINTR(sc);
		OURS;
	}
#ifndef __FreeBSD__
	return ret;
#endif
}

#ifdef __FreeBSD__
void
avma1pp_map_int(pcici_t config_id, void *pisc, unsigned *net_imask)
{
	struct isic_softc *sc = (struct isic_softc *)pisc;

#ifdef AVMA1PCI_DEBUG
	/* may need the irq later */
#if __FreeBSD__ < 3
	/* I'd like to call getirq here, but it is static */
	sc->sc_irq = PCI_INTERRUPT_LINE_EXTRACT(
			pci_conf_read (config_id, PCI_INTERRUPT_REG));
	
	if (sc->sc_irq == 0 || sc->sc_irq == 0xff)
		printf ("avma1pp_map_int:int line register not set by bios\n");
	
	if (sc->sc_irq >= PCI_MAX_IRQ)
		printf ("avma1pp_map_int:irq %d out of bounds (must be < %d)\n",
				sc->sc_irq, PCI_MAX_IRQ);
#else
	sc->sc_irq = config_id->intline;
#endif
#endif /* AVMA1PCI_DEBUG */

	if(!(pci_map_int(config_id, (void *)avma1pp_intr, sc, net_imask)))
	{
		printf("Failed to map interrupt for AVM Fritz!Card PCI\n");
		/* disable the card */
		avma1pp_disable(sc);
	}
}
#else
static void
avma1pp_map_int(struct pci_isic_softc *psc, struct pci_attach_args *pa)
{
	struct isic_softc *sc = &psc->sc_isic;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr;

	/* Map and establish the interrupt. */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		avma1pp_disable(sc);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	psc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, avma1pp_intr, sc);
	if (psc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		avma1pp_disable(sc);
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);
}
#endif

static void
avma1pp_hscx_init(struct isic_softc *sc, int h_chan, int activate)
{
	isic_Bchan_t *chan = &sc->sc_chan[h_chan];
	u_int param = 0;

	DBGL1(L1_BCHAN, "avma1pp_hscx_init", ("unit=%d, channel=%d, %s\n",
		sc->sc_unit, h_chan, activate ? "activate" : "deactivate"));

	if (activate == 0)
	{
		/* only deactivate if both channels are idle */
		if (sc->sc_chan[HSCX_CH_A].state != HSCX_IDLE ||
			sc->sc_chan[HSCX_CH_B].state != HSCX_IDLE)
		{
			return;
		}
		sc->avma1pp_cmd = HSCX_CMD_XRS|HSCX_CMD_RRS;
		sc->avma1pp_prot = HSCX_MODE_TRANS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		return;
	}
	if(chan->bprot == BPROT_RHDLC)
	{
		  DBGL1(L1_BCHAN, "avma1pp_hscx_init", ("BPROT_RHDLC\n"));

		/* HDLC Frames, transparent mode 0 */
		sc->avma1pp_cmd = HSCX_CMD_XRS|HSCX_CMD_RRS;
		sc->avma1pp_prot = HSCX_MODE_ITF_FLG;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd = HSCX_CMD_XRS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd = 0;
	}
	else
	{
		  DBGL1(L1_BCHAN, "avma1pp_hscx_init", ("BPROT_NONE??\n"));

		/* Raw Telephony, extended transparent mode 1 */
		sc->avma1pp_cmd = HSCX_CMD_XRS|HSCX_CMD_RRS;
		sc->avma1pp_prot = HSCX_MODE_TRANS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd = HSCX_CMD_XRS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd = 0;
	}
}

static void
avma1pp_bchannel_setup(int unit, int h_chan, int bprot, int activate)
{
#ifdef __FreeBSD__
	struct isic_softc *sc = &isic_sc[unit];
#else
	struct isic_softc *sc = isic_find_sc(unit);
#endif
	isic_Bchan_t *chan = &sc->sc_chan[h_chan];

	int s = SPLI4B();
	
	if(activate == 0)
	{
		/* deactivation */
		chan->state &= ~HSCX_AVMA1PP_ACTIVE;
		avma1pp_hscx_init(sc, h_chan, activate);
	}
		
	DBGL1(L1_BCHAN, "avma1pp_bchannel_setup", ("unit=%d, channel=%d, %s\n",
		sc->sc_unit, h_chan, activate ? "activate" : "deactivate"));

	/* general part */

	chan->unit = sc->sc_unit;	/* unit number */
	chan->channel = h_chan;		/* B channel */
	chan->bprot = bprot;		/* B channel protocol */
	chan->state = HSCX_IDLE;	/* B channel state */

	/* receiver part */

	i4b_Bcleanifq(&chan->rx_queue);	/* clean rx queue */

	chan->rx_queue.ifq_maxlen = IFQ_MAXLEN;

	chan->rxcount = 0;		/* reset rx counter */
	
	i4b_Bfreembuf(chan->in_mbuf);	/* clean rx mbuf */

	chan->in_mbuf = NULL;		/* reset mbuf ptr */
	chan->in_cbptr = NULL;		/* reset mbuf curr ptr */
	chan->in_len = 0;		/* reset mbuf data len */
	
	/* transmitter part */

	i4b_Bcleanifq(&chan->tx_queue);	/* clean tx queue */

	chan->tx_queue.ifq_maxlen = IFQ_MAXLEN;
	
	chan->txcount = 0;		/* reset tx counter */
	
	i4b_Bfreembuf(chan->out_mbuf_head);	/* clean tx mbuf */

	chan->out_mbuf_head = NULL;	/* reset head mbuf ptr */
	chan->out_mbuf_cur = NULL;	/* reset current mbuf ptr */	
	chan->out_mbuf_cur_ptr = NULL;	/* reset current mbuf data ptr */
	chan->out_mbuf_cur_len = 0;	/* reset current mbuf data cnt */
	
	if(activate != 0)
	{
		/* activation */
		avma1pp_hscx_init(sc, h_chan, activate);
		chan->state |= HSCX_AVMA1PP_ACTIVE;
	}

	splx(s);
}

static void
avma1pp_bchannel_start(int unit, int h_chan)
{
#ifdef __FreeBSD__
	struct isic_softc *sc = &isic_sc[unit];
#else
	struct isic_softc *sc = isic_find_sc(unit);
#endif
	register isic_Bchan_t *chan = &sc->sc_chan[h_chan];
	int s;
	int activity = -1;

	s = SPLI4B();				/* enter critical section */
	if(chan->state & HSCX_TX_ACTIVE)	/* already running ? */
	{
		splx(s);
		return;				/* yes, leave */
	}

	/* get next mbuf from queue */
	
	IF_DEQUEUE(&chan->tx_queue, chan->out_mbuf_head);
	
	if(chan->out_mbuf_head == NULL)		/* queue empty ? */
	{
		splx(s);			/* leave critical section */
		return;				/* yes, exit */
	}

	/* init current mbuf values */
	
	chan->out_mbuf_cur = chan->out_mbuf_head;
	chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;
	chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;	
	
	/* activity indicator for timeout handling */

	if(chan->bprot == BPROT_NONE)
	{
		if(!(isic_hscx_silence(chan->out_mbuf_cur->m_data, chan->out_mbuf_cur->m_len)))
			activity = ACT_TX;
	}
	else
	{
		activity = ACT_TX;
	}

	chan->state |= HSCX_TX_ACTIVE;		/* we start transmitting */
	
	if(sc->sc_trace & TRACE_B_TX)	/* if trace, send mbuf to trace dev */
	{
		i4b_trace_hdr_t hdr;
		hdr.unit = unit;
		hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
		hdr.dir = FROM_TE;
		hdr.count = ++sc->sc_trace_bcount;
		MICROTIME(hdr.time);
		MPH_Trace_Ind(&hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
	}			

	isic_hscx_fifo(chan, sc);

	/* call timeout handling routine */
	
	if(activity == ACT_RX || activity == ACT_TX)
		(*chan->drvr_linktab->bch_activity)(chan->drvr_linktab->unit, activity);

	splx(s);	
}

/*---------------------------------------------------------------------------*
 *	return the address of isic drivers linktab	
 *---------------------------------------------------------------------------*/
static isdn_link_t *
avma1pp_ret_linktab(int unit, int channel)
{
#ifdef __FreeBSD__
	struct isic_softc *sc = &isic_sc[unit];
#else
	struct isic_softc *sc = isic_find_sc(unit);
#endif
	isic_Bchan_t *chan = &sc->sc_chan[channel];

	return(&chan->isdn_linktab);
}
 
/*---------------------------------------------------------------------------*
 *	set the driver linktab in the b channel softc
 *---------------------------------------------------------------------------*/
static void
avma1pp_set_linktab(int unit, int channel, drvr_link_t *dlt)
{
#ifdef __FreeBSD__
	struct isic_softc *sc = &isic_sc[unit];
#else
	struct isic_softc *sc = isic_find_sc(unit);
#endif
	isic_Bchan_t *chan = &sc->sc_chan[channel];

	chan->drvr_linktab = dlt;
}


/*---------------------------------------------------------------------------*
 *	initialize our local linktab
 *---------------------------------------------------------------------------*/
static void
avma1pp_init_linktab(struct isic_softc *sc)
{
	isic_Bchan_t *chan = &sc->sc_chan[HSCX_CH_A];
	isdn_link_t *lt = &chan->isdn_linktab;

	/* make sure the hardware driver is known to layer 4 */
	/* avoid overwriting if already set */
	if (ctrl_types[CTRL_PASSIVE].set_linktab == NULL)
	{
		ctrl_types[CTRL_PASSIVE].set_linktab = avma1pp_set_linktab;
		ctrl_types[CTRL_PASSIVE].get_linktab = avma1pp_ret_linktab;
	}

	/* local setup */
	lt->unit = sc->sc_unit;
	lt->channel = HSCX_CH_A;
	lt->bch_config = avma1pp_bchannel_setup;
	lt->bch_tx_start = avma1pp_bchannel_start;
	lt->bch_stat = avma1pp_bchannel_stat;
	lt->tx_queue = &chan->tx_queue;

	/* used by non-HDLC data transfers, i.e. telephony drivers */
	lt->rx_queue = &chan->rx_queue;

	/* used by HDLC data transfers, i.e. ipr and isp drivers */	
	lt->rx_mbuf = &chan->in_mbuf;	
                                                
	chan = &sc->sc_chan[HSCX_CH_B];
	lt = &chan->isdn_linktab;

	lt->unit = sc->sc_unit;
	lt->channel = HSCX_CH_B;
	lt->bch_config = avma1pp_bchannel_setup;
	lt->bch_tx_start = avma1pp_bchannel_start;
	lt->bch_stat = avma1pp_bchannel_stat;
	lt->tx_queue = &chan->tx_queue;

	/* used by non-HDLC data transfers, i.e. telephony drivers */
	lt->rx_queue = &chan->rx_queue;

	/* used by HDLC data transfers, i.e. ipr and isp drivers */	
	lt->rx_mbuf = &chan->in_mbuf;	
}

/*
 * use this instead of isic_bchannel_stat in i4b_bchan.c because it's static
 */
static void
avma1pp_bchannel_stat(int unit, int h_chan, bchan_statistics_t *bsp)
{
#ifdef __FreeBSD__
	struct isic_softc *sc = &isic_sc[unit];
#else
	struct isic_softc *sc = isic_find_sc(unit);
#endif
	isic_Bchan_t *chan = &sc->sc_chan[h_chan];
	int s;

	s = SPLI4B();
	
	bsp->outbytes = chan->txcount;
	bsp->inbytes = chan->rxcount;

	chan->txcount = 0;
	chan->rxcount = 0;

	splx(s);
}

/*---------------------------------------------------------------------------*
 *	fill HSCX fifo with data from the current mbuf
 *	Put this here until it can go into i4b_hscx.c
 *---------------------------------------------------------------------------*/
int
isic_hscx_fifo(isic_Bchan_t *chan, struct isic_softc *sc)
{
	int len;
	int nextlen;
	int i;
	int cmd;
	/* using a scratch buffer simplifies writing to the FIFO */
	u_char scrbuf[HSCX_FIFO_LEN];

	len = 0;

	/*
	 * fill the HSCX tx fifo with data from the current mbuf. if
	 * current mbuf holds less data than HSCX fifo length, try to
	 * get the next mbuf from (a possible) mbuf chain. if there is
	 * not enough data in a single mbuf or in a chain, then this
	 * is the last mbuf and we tell the HSCX that it has to send
	 * CRC and closing flag
	 */
	 
	while(chan->out_mbuf_cur && len != sc->sc_bfifolen)
	{
		nextlen = min(chan->out_mbuf_cur_len, sc->sc_bfifolen - len);

#ifdef NOTDEF
		printf("i:mh=%p, mc=%p, mcp=%p, mcl=%d l=%d nl=%d # ",
			chan->out_mbuf_head,
			chan->out_mbuf_cur,			
			chan->out_mbuf_cur_ptr,
			chan->out_mbuf_cur_len,
			len,
			nextlen);
#endif

		cmd |= HSCX_CMDR_XTF;
		/* collect the data in the scratch buffer */
		for (i = 0; i < nextlen; i++)
			scrbuf[i + len] = chan->out_mbuf_cur_ptr[i];

		len += nextlen;
		chan->txcount += nextlen;
	
		chan->out_mbuf_cur_ptr += nextlen;
		chan->out_mbuf_cur_len -= nextlen;
			
		if(chan->out_mbuf_cur_len == 0) 
		{
			if((chan->out_mbuf_cur = chan->out_mbuf_cur->m_next) != NULL)
			{
				chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;
				chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;
	
				if(sc->sc_trace & TRACE_B_TX)
				{
					i4b_trace_hdr_t hdr;
					hdr.unit = sc->sc_unit;
					hdr.type = (chan->channel == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_TE;
					hdr.count = ++sc->sc_trace_bcount;
					MICROTIME(hdr.time);
					MPH_Trace_Ind(&hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
				}
			}
			else
			{
				if (chan->bprot != BPROT_NONE)
					cmd |= HSCX_CMDR_XME;
				i4b_Bfreembuf(chan->out_mbuf_head);
				chan->out_mbuf_head = NULL;
			}
		}
	}
	/* write what we have from the scratch buf to the HSCX fifo */
	if (len != 0)
		HSCX_WRFIFO(chan->channel, scrbuf, len);
	return(cmd);
}

#endif /* NISIC > 0 && defined(AVM_A1_PCI) */
