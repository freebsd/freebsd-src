/*
 * Copyright (c) 1999, 2000 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 *---------------------------------------------------------------------------
 *
 *	isic - I4B Siemens ISDN Chipset Driver for ELSA MicroLink ISDN/PCC-16
 *	=====================================================================
 *
 *	This should now also work for an ELSA PCFpro.
 *
 *	$Id: i4b_elsa_pcc16.c,v 1.4 2000/07/19 07:51:22 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Fri Oct 13 16:00:01 2000]
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"
#include "opt_i4b.h"

#if (NISIC > 0) && defined(ELSA_PCC16)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>


#include <machine/i4b_ioctl.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_hscx.h>

static void i4b_epcc16_clrirq(struct l1_softc *sc);

/* masks for register encoded in base addr */

#define ELSA_BASE_MASK		0x0ffff
#define ELSA_OFF_MASK		0xf0000

/* register id's to be encoded in base addr */

#define ELSA_IDISAC		0x00000
#define ELSA_IDHSCXA		0x10000
#define ELSA_IDHSCXB		0x20000

/* offsets from base address */

#define ELSA_OFF_ISAC		0x00
#define ELSA_OFF_HSCX		0x02
#define ELSA_OFF_OFF		0x03
#define ELSA_OFF_CTRL		0x04
#define ELSA_OFF_CFG		0x05
#define ELSA_OFF_TIMR		0x06
#define ELSA_OFF_IRQ		0x07

/* control register (write access) */

#define ELSA_CTRL_LED_YELLOW	0x02
#define ELSA_CTRL_LED_GREEN	0x08
#define ELSA_CTRL_RESET		0x20
#define ELSA_CTRL_TIMEREN	0x80
#define ELSA_CTRL_SECRET	0x50

/*---------------------------------------------------------------------------*
 *      ELSA MicroLink ISDN/PCC-16 clear IRQ routine
 *---------------------------------------------------------------------------*/
static void
i4b_epcc16_clrirq(struct l1_softc *sc)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);
	bus_space_write_1(t, h, ELSA_OFF_IRQ, 0);
}

/*---------------------------------------------------------------------------*
 *      ELSA MicroLink ISDN/PCC-16 ISAC get fifo routine
 *---------------------------------------------------------------------------*/
static void
epcc16_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_read_multi_1(t, h, ELSA_OFF_ISAC, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_read_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40);
			bus_space_read_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *      ELSA MicroLink ISDN/PCC-16 ISAC put fifo routine
 *---------------------------------------------------------------------------*/
static void
epcc16_write_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_write_multi_1(t, h, ELSA_OFF_ISAC, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_write_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40);
			bus_space_write_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *      ELSA MicroLink ISDN/PCC-16 ISAC put register routine
 *---------------------------------------------------------------------------*/
static void
epcc16_write_reg(struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			bus_space_write_1(t, h, ELSA_OFF_ISAC, data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			bus_space_write_1(t, h, ELSA_OFF_HSCX, data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40+offs);
			bus_space_write_1(t, h, ELSA_OFF_HSCX, data);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	ELSA MicroLink ISDN/PCC-16 ISAC get register routine
 *---------------------------------------------------------------------------*/
static u_int8_t
epcc16_read_reg(struct l1_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			return bus_space_read_1(t, h, ELSA_OFF_ISAC);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			return bus_space_read_1(t, h, ELSA_OFF_HSCX);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40+offs);
			return bus_space_read_1(t, h, ELSA_OFF_HSCX);
	}
	return 0;
}

/*---------------------------------------------------------------------------*
 *	isic_detach_Epcc16 - detach for ELSA MicroLink ISDN/PCC-16
 *---------------------------------------------------------------------------*/
static void
isic_detach_Epcc16(device_t dev)
{
	struct l1_softc *sc = &l1_sc[device_get_unit(dev)];

	if ( sc->sc_resources.irq )
	{
		bus_teardown_intr(dev,sc->sc_resources.irq,
			(void(*)(void *))isicintr);
		bus_release_resource(dev,SYS_RES_IRQ,
					sc->sc_resources.irq_rid,
					sc->sc_resources.irq);
		sc->sc_resources.irq = 0;
	}
	
	if ( sc->sc_resources.io_base[0] ) {
		bus_release_resource(dev,SYS_RES_IOPORT,
					sc->sc_resources.io_rid[0],
					sc->sc_resources.io_base[0]);
		sc->sc_resources.io_base[0] = 0;
	}
}

/*---------------------------------------------------------------------------*
 *	isic_probe_Epcc16 - probe for ELSA MicroLink ISDN/PCC-16
 *---------------------------------------------------------------------------*/
int
isic_probe_Epcc16(device_t dev)
{
	size_t unit = device_get_unit(dev);	/* get unit */
	struct l1_softc *sc = 0;		/* pointer to softc */
	void *ih = 0;				/* dummy */

	/* check max unit range */
	if(unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for ELSA PCC-16!\n",
				unit, unit);
		return(ENXIO);	
	}

	sc = &l1_sc[unit];		/* get pointer to softc */

	sc->sc_unit = unit;		/* set unit */

	sc->sc_flags = FLAG_ELSA_PCC16;	/* set flags */

	/* see if an io base was supplied */
	
	if(!(sc->sc_resources.io_base[0] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
	                                   &sc->sc_resources.io_rid[0],
	                                   0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get iobase for ELSA PCC-16.\n",
				unit);
		return(ENXIO);
	}

	/* check if we got an iobase */

	sc->sc_port = rman_get_start(sc->sc_resources.io_base[0]);

	switch(sc->sc_port)
	{
		case 0x160:
		case 0x170:
		case 0x260:
		case 0x360:
			break;
		default:
			printf("isic%d: Error, invalid iobase 0x%x specified for ELSA MicroLink ISDN/PCC-16!\n",
				unit, sc->sc_port);
			isic_detach_Epcc16(dev);
			return(ENXIO);
			break;
	}

	/* setup access routines */

	sc->clearirq = i4b_epcc16_clrirq;
	sc->readreg = epcc16_read_reg;
	sc->writereg = epcc16_write_reg;

	sc->readfifo = epcc16_read_fifo;
	sc->writefifo = epcc16_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_ELSAQS1ISA;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;	

	/* 
	 * Read HSCX A/B VSTR.  Expected value for the ELSA PCC-16
	 * is 0x05 ( = version 2.1 ) in the least significant bits.
	 */

	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) ||
            ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		/* patch from "Doobee R . Tzeck" <drt@ailis.de>:
		 * I own an ELSA PCFpro. To my knowledge, the ELSA PCC16 is
		 * a stripped down Version on the PCFpro. By patching the
		 * card detection routine for the PPC16 I was able to use
		 * the PPC16 driver for the PCFpro.
		 */
		if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x85) ||
		    ((HSCX_READ(1, H_VSTR) & 0xf) != 0x85) )
		{
			printf("isic%d: HSCX VSTR test failed for ELSA MicroLink ISDN/PCC-16\n",
				unit);
			isic_detach_Epcc16(dev);
			printf("isic%d: HSC0: VSTR: %#x\n",
				unit, HSCX_READ(0, H_VSTR));
			printf("isic%d: HSC1: VSTR: %#x\n",
				unit, HSCX_READ(1, H_VSTR));
			return (ENXIO);
		}
		else
		{
			printf("isic%d: ELSA MicroLink ISDN/PCFpro found, going to tread it as PCC-16\n",
				unit);
		}
	}

	/* get our irq */

	if(!(sc->sc_resources.irq =
			bus_alloc_resource(dev, SYS_RES_IRQ,
					&sc->sc_resources.irq_rid,
					0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get an irq.\n",unit);
		isic_detach_Epcc16(dev);
		return ENXIO;
	}

	/* get the irq number */
	sc->sc_irq = rman_get_start(sc->sc_resources.irq);

	/* check IRQ validity */	
	switch(sc->sc_irq)
	{
		case 2:
		case 9:		
		case 3:		
		case 5:
		case 10:
		case 11:
		case 15:		
			break;
			
		default:
			printf("isic%d: Error, invalid IRQ [%d] specified for ELSA MicroLink ISDN/PCC-16!\n",
				unit, sc->sc_irq);
			isic_detach_Epcc16(dev);
			return(ENXIO);
			break;
	}

	/* register interupt routine */
	bus_setup_intr(dev,sc->sc_resources.irq,INTR_TYPE_NET,
			(void(*)(void *))(isicintr),
			sc,&ih);


	return (0);
}

/*---------------------------------------------------------------------------*
 * isic_attach_Epcc16 - attach for ELSA MicroLink ISDN/PCC-16
 *---------------------------------------------------------------------------*/
int
isic_attach_Epcc16(device_t dev)
{
	int unit = device_get_unit(dev);
	struct l1_softc *sc = &l1_sc[unit];	
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	u_char byte = ELSA_CTRL_SECRET;

	byte &= ~ELSA_CTRL_RESET;
        bus_space_write_1(t, h, ELSA_OFF_CTRL, byte);
        DELAY(20);
	byte |= ELSA_CTRL_RESET;
        bus_space_write_1(t, h, ELSA_OFF_CTRL, byte);

        DELAY(20);
        bus_space_write_1(t, h, ELSA_OFF_IRQ, 0xff);

	return 0;
}

#endif /* (NISIC > 0) && defined(ELSA_PCC16) */
