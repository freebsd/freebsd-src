/*
 *   Copyright (c) 1999 Udo Schweigert. All rights reserved.
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
 *   A lot of code was borrowed from i4b_bchan.c and i4b_hscx.c
 *   Based on AVM Fritz!PCI driver by Gary Jennejohn
 *---------------------------------------------------------------------------
 *   In case of trouble please contact Udo Schweigert <ust@cert.siemens.de>
 *---------------------------------------------------------------------------
 *
 *	Fritz!Card PnP specific routines for isic driver
 *	------------------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Thu 10 Jun 08:50:28 CEST 1999]
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)
#include "isic.h"
#include "opt_i4b.h"

#if NISIC > 0 && defined(AVM_PNP)

#include <sys/param.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/ioccom.h>
#if __FreeBSD__ >= 5
#include <sys/bus.h>
#endif
#else
#include <sys/ioctl.h>
#endif
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include <machine/clock.h>
#include <i386/isa/isa_device.h>

#include <sys/socket.h>
#include <net/if.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

static void hscx_write_reg(int, u_int, struct isic_softc *, int);
static void hscx_write_reg_val(int, u_int, u_char, struct isic_softc *);
static u_char hscx_read_reg(int, u_int, struct isic_softc *);
static void hscx_read_fifo(int, void *, size_t, struct isic_softc *);
static void hscx_write_fifo(int, const void *, size_t, struct isic_softc *);
static void avm_pnp_hscx_int_handler(struct isic_softc *);
static void avm_pnp_hscx_intr(int, int, int, struct isic_softc *);
static void avm_pnp_init_linktab(struct isic_softc *);
static void avm_pnp_bchannel_setup(int, int, int, int);
static void avm_pnp_bchannel_start(int, int);
static void avm_pnp_hscx_init(struct isic_softc *, int, int);
static void avm_pnp_bchannel_stat(int, int, bchan_statistics_t *);
static void avm_pnp_set_linktab(int, int, drvr_link_t *);
static void avm_pnp_intr(int);
static isdn_link_t * avm_pnp_ret_linktab(int, int);
extern void isicintr_sc(struct isic_softc *);

/*---------------------------------------------------------------------------*
 *	AVM PnP Fritz!Card special registers
 *---------------------------------------------------------------------------*/

/*
 *	register offsets from i/o base
 */
#define STAT0_OFFSET            0x02
#define STAT1_OFFSET            0x03
#define ADDR_REG_OFFSET         0x04

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
 *	AVM PnP Status Latch 0 read only bits
 */
#define ASL_IRQ_ISAC            0x01    /* ISAC  interrupt, active low */
#define ASL_IRQ_HSCX            0x02    /* HSX   interrupt, active low */
#define ASL_IRQ_TIMER           0x04    /* Timer interrupt, active low */
#define ASL_IRQ_BCHAN           ASL_IRQ_HSCX
/* actually active LOW */
#define ASL_IRQ_Pending         0x07

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

/* "fake" addresses for the non-existent HSCX */
/* note: the unit number is in the lower byte for both the ISAC and "HSCX" */
#define HSCX0FAKE	0xfa000 /* read: fake0 */
#define HSCX1FAKE	0xfa100 /* read: fake1 */
#define IS_HSCX_MASK	0xfff00

/*
 * to prevent deactivating the "HSCX" when both channels are active we
 * define an HSCX_ACTIVE flag which is or'd into the channel's state
 * flag in avm_pnp_bchannel_setup upon active and cleared upon deactivation.
 * It is set high to allow room for new flags.
 */
#define HSCX_AVMPNP_ACTIVE	0x1000 

/*---------------------------------------------------------------------------*
 *	AVM read fifo routines
 *---------------------------------------------------------------------------*/

static void		
avm_pnp_read_fifo(void *buf, const void *base, size_t len)
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
	u_char *ip;
	size_t cnt;

	outb(sc->sc_port + ADDR_REG_OFFSET, chan);
	ip = (u_char *)buf;
	cnt = 0;

	while (cnt < len)
	{
		*ip++ = inb(sc->sc_port + ISAC_REG_OFFSET);
		cnt++;
	}
}

/*---------------------------------------------------------------------------*
 *	AVM write fifo routines
 *---------------------------------------------------------------------------*/
static void
avm_pnp_write_fifo(void *base, const void *buf, size_t len)
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
	outsb(sc->sc_port + ISAC_REG_OFFSET, (const u_char *)buf, len);
}

static void
hscx_write_fifo(int chan, const void *buf, size_t len, struct isic_softc *sc)
{
	register const u_char *ip;
	register size_t cnt;
	isic_Bchan_t *Bchan = &sc->sc_chan[chan];

	sc->avma1pp_cmd &= ~HSCX_CMD_XME;
	sc->avma1pp_txl = 0;

	if (Bchan->out_mbuf_cur == NULL && Bchan->bprot != BPROT_NONE)
		sc->avma1pp_cmd |= HSCX_CMD_XME;

	if (len != sc->sc_bfifolen)
	  	sc->avma1pp_txl = len;

	hscx_write_reg(chan, HSCX_STAT, sc, 3);

	ip = (const u_char *)buf;
	cnt = 0;
	while (cnt < len)
	{
		outb(sc->sc_port + ISAC_REG_OFFSET, *ip++);
		cnt++;
	}
}

/*---------------------------------------------------------------------------*
 *	AVM write register routines
 *---------------------------------------------------------------------------*/
static void
avm_pnp_write_reg(u_char *base, u_int offset, u_int v)
{
	int unit;
	struct isic_softc *sc;
	u_char reg_bank;

	unit = (int)base & 0xff;
	sc = &isic_sc[unit];

	/* check whether the target is an HSCX */
	if (((int)base & IS_HSCX_MASK) == HSCX0FAKE)
	{
		hscx_write_reg_val(0, offset, v, sc); 
		return;
	}
	if (((int)base & IS_HSCX_MASK) == HSCX1FAKE)
	{
		hscx_write_reg_val(1, offset, v, sc); 
		return;
	}
	/* must be the ISAC */
	reg_bank = (offset > MAX_LO_REG_OFFSET) ? ISAC_HI_REG_OFFSET:ISAC_LO_REG_OFFSET;
	/* set the register bank */
	outb(sc->sc_port + ADDR_REG_OFFSET, reg_bank);
	outb(sc->sc_port + ISAC_REG_OFFSET + (offset & ISAC_REGSET_MASK), v);
}

static void
hscx_write_reg(int chan, u_int off, struct isic_softc *sc, int which)
{
	/* HACK */
	if (off == H_MASK)
		return;
	/* point at the correct channel */
	outb(sc->sc_port + ADDR_REG_OFFSET, chan);
	if (which & 4)
		outb(sc->sc_port + ISAC_REG_OFFSET + off + 2, sc->avma1pp_prot);
	if (which & 2)
		outb(sc->sc_port + ISAC_REG_OFFSET + off + 1, sc->avma1pp_txl);
	if (which & 1)
		outb(sc->sc_port + ISAC_REG_OFFSET + off, sc->avma1pp_cmd);
}

static void
hscx_write_reg_val(int chan, u_int off, u_char val, struct isic_softc *sc)
{
	/* HACK */
	if (off == H_MASK)
		return;
	/* point at the correct channel */
	outb(sc->sc_port + ADDR_REG_OFFSET, chan);
	outb(sc->sc_port + ISAC_REG_OFFSET + off, val);
}

/*---------------------------------------------------------------------------*
 *	AVM read register routines
 *---------------------------------------------------------------------------*/

static u_char
avm_pnp_read_reg(u_char *base, u_int offset)
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
	/* set the register bank */
	outb(sc->sc_port + ADDR_REG_OFFSET, reg_bank);
	return(inb(sc->sc_port + ISAC_REG_OFFSET +
		(offset & ISAC_REGSET_MASK)));
}

static u_char
hscx_read_reg(int chan, u_int off, struct isic_softc *sc)
{
	/* HACK */
	if (off == H_ISTA)
		return(0);
	/* point at the correct channel */
	outb(sc->sc_port + ADDR_REG_OFFSET, chan);
	return(inb(sc->sc_port + ISAC_REG_OFFSET + off));
}

/*---------------------------------------------------------------------------*
 *	isic_probe_avm_pnp - probe Fritz!Card PnP
 *---------------------------------------------------------------------------*/

int
isic_probe_avm_pnp(struct isa_device *dev, unsigned int iobase2)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];
	
	/* check max unit range */
	
	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for AVM Fritz! PnP\n",
				dev->id_unit, dev->id_unit);
		return(0);	
	}	
	sc->sc_unit = dev->id_unit;

	/* check IRQ validity */

	switch(ffs(dev->id_irq) - 1)
	{
		case 3:
		case 4:		
		case 5:
		case 7:
		case 10:
		case 11:
		case 12:
		case 15:		
			break;
			
		default:
			printf("isic%d: Error, invalid IRQ [%d] specified for AVM Fritz! PnP!\n",
				dev->id_unit, ffs(dev->id_irq)-1);
			return(0);
			break;
	}
	sc->sc_irq = dev->id_irq;

        dev->id_intr = (driver_intr_t *) avm_pnp_intr;

	/* check if memory addr specified */

	if(dev->id_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for AVM Fritz! PnP!\n",
			dev->id_unit, (u_long)dev->id_maddr);
		return(0);
	}
	dev->id_msize = 0;
	
	/* check if we got an iobase */

	if(!((dev->id_iobase >= 0x160) && (dev->id_iobase <= 0x360)))
	{
		printf("isic%d: Error, invalid iobase 0x%x specified for AVM Fritz! PnP!\n",
			dev->id_unit, dev->id_iobase);
		return(0);
	}
	sc->sc_port = dev->id_iobase;


	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = avm_pnp_read_reg;
	sc->writereg = avm_pnp_write_reg;

	sc->readfifo = avm_pnp_read_fifo;
	sc->writefifo = avm_pnp_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_AVM_PNP;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;	

	/* the ISAC lives at offset 0x10, but we can't use that. */
	/* instead, put the unit number into the lower byte - HACK */
	ISAC_BASE = (caddr_t)((int)(dev->id_iobase & ~0xff) + dev->id_unit);

	outb(sc->sc_port + STAT0_OFFSET, ASL_RESET_ALL|ASL_TIMERDISABLE);
	ISAC_WRITE(I_MASK, 0x0);
	outb(sc->sc_port + STAT0_OFFSET, ASL_TIMERRESET|ASL_ENABLE_INT|ASL_TIMERDISABLE);
	ISAC_WRITE(I_MASK, 0x41);
	return (1);
}

/*---------------------------------------------------------------------------*
 *	isic_attach_avm_pnp - attach Fritz!Card PnP
 *---------------------------------------------------------------------------*/
int
isic_attach_avm_pnp(struct isa_device *dev, unsigned int iobase2)
{
	struct isic_softc *sc;
	u_int v;
	int unit;

	unit = dev->id_unit;
	sc = &isic_sc[unit];

	/* this thing doesn't have an HSCX, so fake the base addresses */
	/* put the unit number into the lower byte - HACK */
	HSCX_A_BASE = (caddr_t)(HSCX0FAKE + unit);
	HSCX_B_BASE = (caddr_t)(HSCX1FAKE + unit);


	/* reset the card */

	/* the Linux driver does this to clear any pending ISAC interrupts */
	v = 0;
	v = ISAC_READ(I_STAR);
	v = ISAC_READ(I_MODE);
	v = ISAC_READ(I_ADF2);
	v = ISAC_READ(I_ISTA);
	if (v & ISAC_ISTA_EXI)
	{
		 v = ISAC_READ(I_EXIR);
	}
	v = ISAC_READ(I_CIRR);
	ISAC_WRITE(I_MASK, 0xff);

	/* the Linux driver does this to clear any pending HSCX interrupts */
	v = hscx_read_reg(0, HSCX_STAT, sc);
	v = hscx_read_reg(0, HSCX_STAT+1, sc);
	v = hscx_read_reg(0, HSCX_STAT+2, sc);
	v = hscx_read_reg(0, HSCX_STAT+3, sc);
	v = hscx_read_reg(1, HSCX_STAT, sc);
	v = hscx_read_reg(1, HSCX_STAT+1, sc);
	v = hscx_read_reg(1, HSCX_STAT+2, sc);
	v = hscx_read_reg(1, HSCX_STAT+3, sc);

	outb(sc->sc_port + STAT0_OFFSET, ASL_RESET_ALL|ASL_TIMERDISABLE);
	DELAY(SEC_DELAY/100); /* 10 ms */
	outb(sc->sc_port + STAT0_OFFSET, ASL_TIMERRESET|ASL_ENABLE_INT|ASL_TIMERDISABLE);
	DELAY(SEC_DELAY/100); /* 10 ms */
	outb(sc->sc_port + STAT1_OFFSET, ASL1_ENABLE_IOM+(ffs(sc->sc_irq)-1));
	DELAY(SEC_DELAY/100); /* 10 ms */

	printf("isic%d: ISAC %s (IOM-%c)\n", unit,
		"2085 Version A1/A2 or 2086/2186 Version 1.1",
		sc->sc_bustyp == BUS_TYPE_IOM1 ? '1' : '2');

	/* init the ISAC */
	isic_isac_init(sc);

	/* init the "HSCX" */
	avm_pnp_bchannel_setup(sc->sc_unit, HSCX_CH_A, BPROT_NONE, 0);
	
	avm_pnp_bchannel_setup(sc->sc_unit, HSCX_CH_B, BPROT_NONE, 0);

	/* can't use the normal B-Channel stuff */
	avm_pnp_init_linktab(sc);

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

	return(0);
}

/*
 * this is the real interrupt routine
 */
static void
avm_pnp_hscx_intr(int h_chan, int stat, int cnt, struct isic_softc *sc)
{
	register isic_Bchan_t *chan = &sc->sc_chan[h_chan];
	int activity = -1;
	
	DBGL1(L1_H_IRQ, "avm_pnp_hscx_intr", ("%#x\n", stat));

	if((stat & HSCX_INT_XDU) && (chan->bprot != BPROT_NONE))/* xmit data underrun */
	{
		chan->stat_XDU++;			
		DBGL1(L1_H_XFRERR, "avm_pnp_hscx_intr", ("xmit data underrun\n"));
		/* abort the transmission */
		sc->avma1pp_txl = 0;
		sc->avma1pp_cmd |= HSCX_CMD_XRS;
		hscx_write_reg(h_chan, HSCX_STAT, sc, 1);
		sc->avma1pp_cmd &= ~HSCX_CMD_XRS;
		hscx_write_reg(h_chan, HSCX_STAT, sc, 1);

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
			DBGL1(L1_H_XFRERR, "avm_pnp_hscx_intr", ("receive data overflow\n"));
			error++;				
		}
	
		fifo_data_len = cnt;
		
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
					panic("L1 avm_pnp_hscx_intr: RME, cannot allocate mbuf!\n");
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
						DBGL1(L1_H_XFRERR, "avm_pnp_hscx_intr", ("CRC/RAB\n"));
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
						 panic("L1 avm_pnp_hscx_intr: RPF, cannot allocate new mbuf!\n");
	
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
					  DBGL1(L1_H_XFRERR, "avm_pnp_hscx_intr", ("RAWHDLC rx buffer overflow in RPF, in_len=%d\n", chan->in_len));
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
			hscx_write_reg(h_chan, HSCX_STAT, sc, 1);
			sc->avma1pp_cmd &= ~HSCX_CMD_RRS;
			hscx_write_reg(h_chan, HSCX_STAT, sc, 1);
		}
	}


	/* transmit fifo empty, new data can be written to fifo */
	
	if(stat & HSCX_INT_XPR)
	{
		/*
		 * for a description what is going on here, please have
		 * a look at isic_bchannel_start() in i4b_bchan.c !
		 */

		DBGL1(L1_H_IRQ, "avm_pnp_hscx_intr", ("unit %d, chan %d - XPR, Tx Fifo Empty!\n", sc->sc_unit, h_chan));

		if(chan->out_mbuf_cur == NULL || chan->out_mbuf_head == NULL) 	/* last frame is transmitted */
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
avm_pnp_hscx_int_handler(struct isic_softc *sc)
{
	u_char stat = 0;
	u_char cnt = 0;

	stat = hscx_read_reg(0, HSCX_STAT, sc);
	if (stat & HSCX_INT_RPR) 
	  cnt = hscx_read_reg(0, HSCX_STAT+1, sc);
	if (stat & HSCX_INT_MASK)
	  avm_pnp_hscx_intr(0, stat, cnt, sc);

	cnt = 0;
	stat = hscx_read_reg(1, HSCX_STAT, sc);
	if (stat & HSCX_INT_RPR) 
	  cnt = hscx_read_reg(1, HSCX_STAT+1, sc);
	if (stat & HSCX_INT_MASK)
	  avm_pnp_hscx_intr(1, stat, cnt, sc);
}

static void
avm_pnp_hscx_init(struct isic_softc *sc, int h_chan, int activate)
{
	isic_Bchan_t *chan = &sc->sc_chan[h_chan];

	DBGL1(L1_BCHAN, "avm_pnp_hscx_init", ("unit=%d, channel=%d, %s\n",
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
		hscx_write_reg(h_chan, HSCX_STAT, sc, 5);
		return;
	}
	if(chan->bprot == BPROT_RHDLC)
	{
		  DBGL1(L1_BCHAN, "avm_pnp_hscx_init", ("BPROT_RHDLC\n"));

		/* HDLC Frames, transparent mode 0 */
		sc->avma1pp_cmd = HSCX_CMD_XRS|HSCX_CMD_RRS;
		sc->avma1pp_prot = HSCX_MODE_ITF_FLG;
		hscx_write_reg(h_chan, HSCX_STAT, sc, 5);
		sc->avma1pp_cmd = HSCX_CMD_XRS;
		hscx_write_reg(h_chan, HSCX_STAT, sc, 1);
		sc->avma1pp_cmd = 0;
	}
	else
	{
		  DBGL1(L1_BCHAN, "avm_pnp_hscx_init", ("BPROT_NONE??\n"));

		/* Raw Telephony, extended transparent mode 1 */
		sc->avma1pp_cmd = HSCX_CMD_XRS|HSCX_CMD_RRS;
		sc->avma1pp_prot = HSCX_MODE_TRANS;
		hscx_write_reg(h_chan, HSCX_STAT, sc, 5);
		sc->avma1pp_cmd = HSCX_CMD_XRS;
		hscx_write_reg(h_chan, HSCX_STAT, sc, 1);
		sc->avma1pp_cmd = 0;
	}
}

static void
avm_pnp_bchannel_setup(int unit, int h_chan, int bprot, int activate)
{
	struct isic_softc *sc = &isic_sc[unit];
	isic_Bchan_t *chan = &sc->sc_chan[h_chan];

	int s = SPLI4B();
	
	if(activate == 0)
	{
		/* deactivation */
		chan->state &= ~HSCX_AVMPNP_ACTIVE;
		avm_pnp_hscx_init(sc, h_chan, activate);
	}
		
	DBGL1(L1_BCHAN, "avm_pnp_bchannel_setup", ("unit=%d, channel=%d, %s\n",
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
		avm_pnp_hscx_init(sc, h_chan, activate);
		chan->state |= HSCX_AVMPNP_ACTIVE;
	}

	splx(s);
}

static void
avm_pnp_bchannel_start(int unit, int h_chan)
{
	struct isic_softc *sc = &isic_sc[unit];
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
avm_pnp_ret_linktab(int unit, int channel)
{
	struct isic_softc *sc = &isic_sc[unit];
	isic_Bchan_t *chan = &sc->sc_chan[channel];

	return(&chan->isdn_linktab);
}
 
/*---------------------------------------------------------------------------*
 *	set the driver linktab in the b channel softc
 *---------------------------------------------------------------------------*/
static void
avm_pnp_set_linktab(int unit, int channel, drvr_link_t *dlt)
{
	struct isic_softc *sc = &isic_sc[unit];
	isic_Bchan_t *chan = &sc->sc_chan[channel];

	chan->drvr_linktab = dlt;
}


/*---------------------------------------------------------------------------*
 *	initialize our local linktab
 *---------------------------------------------------------------------------*/
static void
avm_pnp_init_linktab(struct isic_softc *sc)
{
	isic_Bchan_t *chan = &sc->sc_chan[HSCX_CH_A];
	isdn_link_t *lt = &chan->isdn_linktab;

	/* make sure the hardware driver is known to layer 4 */
	/* avoid overwriting if already set */
	if (ctrl_types[CTRL_PASSIVE].set_linktab == NULL)
	{
		ctrl_types[CTRL_PASSIVE].set_linktab = avm_pnp_set_linktab;
		ctrl_types[CTRL_PASSIVE].get_linktab = avm_pnp_ret_linktab;
	}

	/* local setup */
	lt->unit = sc->sc_unit;
	lt->channel = HSCX_CH_A;
	lt->bch_config = avm_pnp_bchannel_setup;
	lt->bch_tx_start = avm_pnp_bchannel_start;
	lt->bch_stat = avm_pnp_bchannel_stat;
	lt->tx_queue = &chan->tx_queue;

	/* used by non-HDLC data transfers, i.e. telephony drivers */
	lt->rx_queue = &chan->rx_queue;

	/* used by HDLC data transfers, i.e. ipr and isp drivers */	
	lt->rx_mbuf = &chan->in_mbuf;	
                                                
	chan = &sc->sc_chan[HSCX_CH_B];
	lt = &chan->isdn_linktab;

	lt->unit = sc->sc_unit;
	lt->channel = HSCX_CH_B;
	lt->bch_config = avm_pnp_bchannel_setup;
	lt->bch_tx_start = avm_pnp_bchannel_start;
	lt->bch_stat = avm_pnp_bchannel_stat;
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
avm_pnp_bchannel_stat(int unit, int h_chan, bchan_statistics_t *bsp)
{
	struct isic_softc *sc = &isic_sc[unit];
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
static int
isic_hscx_fifo(isic_Bchan_t *chan, struct isic_softc *sc)
{
	int len;
	int nextlen;
	int i;
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
				i4b_Bfreembuf(chan->out_mbuf_head);
				chan->out_mbuf_head = NULL;
			}
		}
	}
	/* write what we have from the scratch buf to the HSCX fifo */
	if (len != 0)
		HSCX_WRFIFO(chan->channel, scrbuf, len);

	return(0);
}

void
avm_pnp_intr(int unit)
{
	struct isic_softc *sc;
	u_char stat;
	register u_char isac_irq_stat;
	int was_isac = 0;

	sc = &isic_sc[unit];

	for(;;) {
		stat = inb(sc->sc_port + STAT0_OFFSET);

		DBGL1(L1_H_IRQ, "avm_pnp_intr", ("stat %x\n", stat));

		/* was there an interrupt from this card ? */
		if ((stat & ASL_IRQ_Pending) == ASL_IRQ_Pending)
			break; /* no */

		/* interrupts are low active */
		if (!(stat & ASL_IRQ_TIMER))
	  		DBGL1(L1_H_IRQ, "avm_pnp_intr", ("timer interrupt ???\n"));

		if (!(stat & ASL_IRQ_ISAC))
		{
	  		DBGL1(L1_H_IRQ, "avm_pnp_intr", ("ISAC\n"));
			isac_irq_stat = ISAC_READ(I_ISTA);
			isic_isac_irq(sc, isac_irq_stat); 
			was_isac = 1;
		}

		if (!(stat & ASL_IRQ_HSCX))
		{
	  		DBGL1(L1_H_IRQ, "avm_pnp_intr", ("HSCX\n"));
			avm_pnp_hscx_int_handler(sc);
		}
	}
	if (was_isac) {
		ISAC_WRITE(0x20, 0xFF);
		ISAC_WRITE(0x20, 0x0);
        } 
}
#endif /* NISIC > 0 && defined(AVM_PNP) */
#endif /* FreeBSD */
