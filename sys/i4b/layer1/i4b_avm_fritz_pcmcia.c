/*
 *   Copyright (c) 1998 Matthias Apitz. All rights reserved.
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
 *
 *	Fritz!Card pcmcia specific routines for isic driver
 *	---------------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/layer1/i4b_avm_fritz_pcmcia.c,v 1.6 1999/08/28 00:45:36 peter Exp $ 
 *
 *      last edit-date: [Sun May  2 12:01:16 1999]
 *
 *	-ap	added support for AVM PCMCIA Fritz!Card
 *	-mh	split into separate file
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)
#include "isic.h"
#include "opt_i4b.h"
#else
#define NISIC 1
#endif

#if NISIC > 0 && defined(AVM_A1_PCMCIA)

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
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#endif

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

#ifndef __FreeBSD__
#include <i4b/layer1/pcmcia_isic.h>

/* PCMCIA support routines */
static u_int8_t avma1_pcmcia_read_reg __P((struct isic_softc *sc, int what, bus_size_t offs));
static void avma1_pcmcia_write_reg __P((struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data));
static void avma1_pcmcia_read_fifo __P((struct isic_softc *sc, int what, void *buf, size_t size));
static void avma1_pcmcia_write_fifo __P((struct isic_softc *sc, int what, const void *data, size_t size));
#endif

/*---------------------------------------------------------------------------*
 *	AVM PCMCIA Fritz!Card special registers
 *---------------------------------------------------------------------------*/

/*
 *	register offsets from i/o base 0x140 or 0x300
 */
#define ADDR_REG_OFFSET         0x02
#define DATA_REG_OFFSET         0x03
#define STAT0_OFFSET            0x04
#define STAT1_OFFSET            0x05
#define MODREG_OFFSET           0x06
#define VERREG_OFFSET           0x07
/*
 *	AVM PCMCIA Status Latch 0 read only bits
 */
#define ASL_IRQ_TIMER           0x10    /* Timer interrupt, active low */
#define ASL_IRQ_ISAC            0x20    /* ISAC  interrupt, active low */
#define ASL_IRQ_HSCX            0x40    /* HSX   interrupt, active low */
#define ASL_IRQ_BCHAN           ASL_IRQ_HSCX
#define ASL_IRQ_Pending         (ASL_IRQ_ISAC | ASL_IRQ_HSCX | ASL_IRQ_TIMER)
/*
 *	AVM Status Latch 0 write only bits
 */
#define ASL_RESET_ALL           0x01  /* reset siemens IC's, active 1 */
#define ASL_TIMERDISABLE        0x02  /* active high */
#define ASL_TIMERRESET          0x04  /* active high */
#define ASL_ENABLE_INT          0x08  /* active high */
/*
 *	AVM Status Latch 1 write only bits
 */
#define ASL1_LED0                0x10  /* active high */
#define ASL1_LED1                0x20  /* active high */

#define ASL1_ENABLE_S0           0xc0  /* enable active S0 I/F */

/*----- EEpromless controller -----*/
/*
 *	AVM Status Latch read/write bit
 */

#define ASL_TESTBIT     0x80


/*---------------------------------------------------------------------------*
 *	AVM read fifo routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static int PCMCIA_IO_BASE = 0;		/* ap: XXX hack */
static void		
avma1_pcmcia_read_fifo(void *buf, const void *base, size_t len)
{
	outb(PCMCIA_IO_BASE + ADDR_REG_OFFSET, (int)base - 0x20);
	insb(PCMCIA_IO_BASE + DATA_REG_OFFSET, (u_char *)buf, (u_int)len);
}
#else
/* offsets of the different 'what' arguments */
static u_int8_t what_map[] = {
	0x20-0x20,	/* ISIC_WHAT_ISAC */
	0xA0-0x20,	/* ISIC_WHAT_HSCXA */
	0xE0-0x20	/* ISIC_WHAT_HSCXB */
};
static void
avma1_pcmcia_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	bus_space_write_1(t, h, ADDR_REG_OFFSET, what_map[what]);
	bus_space_read_multi_1(t, h, DATA_REG_OFFSET, buf, size);
}
#endif

/*---------------------------------------------------------------------------*
 *	AVM write fifo routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static void
avma1_pcmcia_write_fifo(void *base, const void *buf, size_t len)
{
	outb(PCMCIA_IO_BASE + ADDR_REG_OFFSET, (int)base - 0x20);
	outsb(PCMCIA_IO_BASE + DATA_REG_OFFSET, (u_char *)buf, (u_int)len);
}
#else
static void
avma1_pcmcia_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	bus_space_write_1(t, h, ADDR_REG_OFFSET, what_map[what]);
	bus_space_write_multi_1(t, h, DATA_REG_OFFSET, (u_int8_t*)buf, size);
}
#endif

/*---------------------------------------------------------------------------*
 *	AVM write register routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static void
avma1_pcmcia_write_reg(u_char *base, u_int offset, u_int v)
{
	/* offset includes 0x20 FIFO ! */
	outb(PCMCIA_IO_BASE + ADDR_REG_OFFSET, (int)base+offset-0x20);
	outb(PCMCIA_IO_BASE + DATA_REG_OFFSET, (u_char)v);
}
#else
static void
avma1_pcmcia_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	bus_space_write_1(t, h, ADDR_REG_OFFSET, what_map[what]+offs);
	bus_space_write_1(t, h, DATA_REG_OFFSET, data);
}
#endif

/*---------------------------------------------------------------------------*
 *	AVM read register routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static u_char
avma1_pcmcia_read_reg(u_char *base, u_int offset)
{
	/* offset includes 0x20 FIFO ! */
	outb(PCMCIA_IO_BASE + ADDR_REG_OFFSET, (int)base+offset-0x20);
	return (inb(PCMCIA_IO_BASE + DATA_REG_OFFSET));
}
#else
static u_int8_t
avma1_pcmcia_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	bus_space_write_1(t, h, ADDR_REG_OFFSET, what_map[what]+offs);
	return bus_space_read_1(t, h, DATA_REG_OFFSET);
}
#endif

/*---------------------------------------------------------------------------*
 *	isic_probe_avma1_pcmcia - probe for AVM PCMCIA Fritz!Card
 *	This is in the bus attachemnt part on NetBSD (pcmcia_isic.c), no
 *	card specicfic probe is needed on direct config buses like pcmcia.
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
int
isic_probe_avma1_pcmcia(struct isa_device *dev)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];
	u_char byte;
	int i;
	u_int cardinfo;
	
	/* check max unit range */
	
	if(dev->id_unit > 1)
	{
		printf("isic%d: Error, unit %d > MAXUNIT for AVM PCMCIA Fritz!Card\n",
				dev->id_unit, dev->id_unit);
		return(0);	
	}	
	sc->sc_unit = dev->id_unit;

	/*
	 * we trust the IRQ we got from PCCARD service
	 */
	sc->sc_irq = dev->id_irq;

	/* check if we got an iobase */

	switch(dev->id_iobase)
	{
		case 0x140:
		case 0x300:
			break;
		default:
			printf("isic%d: Error, invalid iobase 0x%x specified for AVM PCMCIA Fritz!Card.\n",
				dev->id_unit, dev->id_iobase);
			return(0);
			break;
	}
	sc->sc_port = dev->id_iobase;
	
	/* ResetController */

	outb(dev->id_iobase + STAT0_OFFSET, 0x00);
	DELAY(SEC_DELAY / 20);
	outb(dev->id_iobase + STAT0_OFFSET, 0x01);
	DELAY(SEC_DELAY / 20);
	outb(dev->id_iobase + STAT0_OFFSET, 0x00);

	/*
	 * CheckController
	 * The logic to check for the PCMCIA was adapted as
	 * described by AVM.
	 */

	outb(dev->id_iobase + ADDR_REG_OFFSET, 0x21);	/* ISAC STAR */
	if ( (byte=inb(dev->id_iobase + DATA_REG_OFFSET) & 0xfd) != 0x48 )
	{
		printf("isic%d: Error, ISAC STAR for AVM PCMCIA is 0x%0x (should be 0x48)\n",
				dev->id_unit, byte);
		return(0);	
	}	

	outb(dev->id_iobase + ADDR_REG_OFFSET, 0xa1);	/* HSCX STAR */
	if ( (byte=inb(dev->id_iobase + DATA_REG_OFFSET) & 0xfd) != 0x48 )
	{
		printf("isic%d: Error, HSCX STAR for AVM PCMCIA is 0x%0x (should be 0x48)\n",
				dev->id_unit, byte);
		return(0);	
	}	

	byte = ASL_TESTBIT;
	for (i=0; i<256; i++)	{
		byte = byte ? 0 : ASL_TESTBIT;
		outb(dev->id_iobase + STAT0_OFFSET, byte);
		if ((inb(dev->id_iobase+STAT0_OFFSET)&ASL_TESTBIT)!=byte)   {
			printf("isic%d: Error during toggle of AVM PCMCIA Status Latch0\n",
				dev->id_unit);
			return(0);
		}
	}

	sc->clearirq = NULL;
	sc->readreg   = avma1_pcmcia_read_reg;
	sc->writereg  = avma1_pcmcia_write_reg;

	sc->readfifo  = avma1_pcmcia_read_fifo;
	sc->writefifo = avma1_pcmcia_write_fifo;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_PCFRITZ;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2; 			/* ap: XXX ??? */

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;
	
	/* setup ISAC and HSCX base addr */
	/*
	 * NOTE: for PCMCIA these are no real addrs; they are
	 * offsets to be written into the base+ADDR_REG_OFFSET register
	 * to pick up the values of the bytes fro base+DATA_REG_OFFSET
	 *
	 * see also the logic in the avma1_pcmcia_* routines;
	 * therefore we also must have the base addr in some static
	 * space or struct; XXX better solution?
	 */
	
	PCMCIA_IO_BASE = dev->id_iobase;
	ISAC_BASE      = (caddr_t)0x20;

	HSCX_A_BASE    = (caddr_t)0xA0;
	HSCX_B_BASE    = (caddr_t)0xE0;

	/* 
	 * Read HSCX A/B VSTR.
	 * Expected value for AVM A1 is 0x04 or 0x05 and for the
	 * AVM Fritz!Card is 0x05 in the least significant bits.
	 */

	if( (((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) &&
	     ((HSCX_READ(0, H_VSTR) & 0xf) != 0x4))	||
            (((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) &&
	     ((HSCX_READ(1, H_VSTR) & 0xf) != 0x4)) )  
	{
		printf("isic%d: HSCX VSTR test failed for AVM PCMCIA Fritz!Card\n",
			dev->id_unit);
		printf("isic%d: HSC0: VSTR: 0x%0x\n",
			dev->id_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: 0x%0x\n",
			dev->id_unit, HSCX_READ(1, H_VSTR));
		return (0);
	}                   

	/*
	 * seems we really have an AVM PCMCIA Fritz!Card controller
	 */
	cardinfo = inb(dev->id_iobase + VERREG_OFFSET)<<8 | inb(dev->id_iobase + MODREG_OFFSET);
	printf("isic%d: successfully detect AVM PCMCIA cardinfo = 0x%0x\n",
		dev->id_unit, cardinfo);
	dev->id_flags = FLAG_AVM_A1_PCMCIA;
	return (1);
}
#endif /* __FreeBSD__ */



/*---------------------------------------------------------------------------*
 *	isic_attach_fritzpcmcia - attach Fritz!Card
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
int
isic_attach_fritzpcmcia(struct isa_device *dev)
{
	/* ResetController again just to make sure... */

	outb(dev->id_iobase + STAT0_OFFSET, 0x00);
	DELAY(SEC_DELAY / 10);
	outb(dev->id_iobase + STAT0_OFFSET, 0x01);
	DELAY(SEC_DELAY / 10);
	outb(dev->id_iobase + STAT0_OFFSET, 0x00);
	DELAY(SEC_DELAY / 10);

	/* enable IRQ, disable counter IRQ */

	outb(dev->id_iobase + STAT0_OFFSET, ASL_TIMERDISABLE |
		ASL_TIMERRESET | ASL_ENABLE_INT);
	/* DELAY(SEC_DELAY / 10); */

	return(1);
}

#else

/*
 * XXX - one time only! Some of this has to go into an enable
 * function, with apropriate counterpart in disable, so a card
 * could be removed an inserted again. But never mind for now,
 * this won't work anyway for several reasons (both in NetBSD
 * and in I4B).
 */
int
isic_attach_fritzpcmcia(struct pcmcia_isic_softc *psc, struct pcmcia_config_entry *cfe, struct pcmcia_attach_args *pa)
{
	struct isic_softc *sc = &psc->sc_isic;
	bus_space_tag_t t;
	bus_space_handle_t h;

	/* Validate config info */
	if (cfe->num_memspace != 0)
		printf(": unexpected number of memory spaces %d should be 0\n",
			cfe->num_memspace);
	if (cfe->num_iospace != 1)
		printf(": unexpected number of memory spaces %d should be 1\n",
			cfe->num_iospace);

	/* Allocate pcmcia space - exactly as dictated by the card */
	if (pcmcia_io_alloc(pa->pf, cfe->iospace[0].start, cfe->iospace[0].length,
			    0, &psc->sc_pcioh))
		printf(": can't allocate i/o space\n");

	/* map them */
	if (pcmcia_io_map(pa->pf, ((cfe->flags & PCMCIA_CFE_IO16) ?
	    PCMCIA_WIDTH_IO16 : PCMCIA_WIDTH_IO8), 0,
	    cfe->iospace[0].length, &psc->sc_pcioh, &psc->sc_io_window)) {
		printf(": can't map i/o space\n");
		return 0;
	}

	/* Setup bus space maps */
	sc->sc_num_mappings = 1;
	sc->sc_cardtyp = CARD_TYPEP_PCFRITZ;
	MALLOC_MAPS(sc);

	/* Copy our handles/tags to the MI maps */
	sc->sc_maps[0].t = psc->sc_pcioh.iot;
	sc->sc_maps[0].h = psc->sc_pcioh.ioh;
	sc->sc_maps[0].offset = 0;
	sc->sc_maps[0].size = 0;	/* not our mapping */

	t = sc->sc_maps[0].t;
	h = sc->sc_maps[0].h;

	sc->clearirq = NULL;
	sc->readreg = avma1_pcmcia_read_reg;
	sc->writereg = avma1_pcmcia_write_reg;

	sc->readfifo = avma1_pcmcia_read_fifo;
	sc->writefifo = avma1_pcmcia_write_fifo;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_PCFRITZ;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;
	
	/* Reset controller again just to make sure... */

	bus_space_write_1(t, h, STAT0_OFFSET, 0x00);
	DELAY(SEC_DELAY / 10);
	bus_space_write_1(t, h, STAT0_OFFSET, 0x01);
	DELAY(SEC_DELAY / 10);
	bus_space_write_1(t, h, STAT0_OFFSET, 0x00);
	DELAY(SEC_DELAY / 10);

	/* enable IRQ, disable counter IRQ */

	bus_space_write_1(t, h, STAT0_OFFSET, ASL_TIMERDISABLE |
		ASL_TIMERRESET | ASL_ENABLE_INT);

	return 1;
}
#endif

#endif /* NISIC > 0 && defined(AVM_A1_PCMCIA) */
