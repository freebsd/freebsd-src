/*
 *   Copyright (c) 1996 Arne Helme. All rights reserved.
 *
 *   Copyright (c) 1996 Gary Jennejohn. All rights reserved. 
 *
 *   Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
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
 *	isic - I4B Siemens ISDN Chipset Driver for Teles S0/16.3
 *	========================================================
 *
 * $FreeBSD$
 *
 *      last edit-date: [Wed Jan 24 09:27:40 2001]
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"
#include "opt_i4b.h"

#if NISIC > 0 && defined(TEL_S0_16_3)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>

#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_hscx.h>

static u_char intr_no[] = { 1, 1, 0, 2, 4, 6, 1, 1, 1, 0, 8, 10, 12, 1, 1, 14 };

#define ISAC_OFFS	0x400
#define	HSCXA_OFFS	0xc00
#define HSCXB_OFFS	0x800

/*---------------------------------------------------------------------------*
 *      Teles S0/16.3 read fifo routine
 *---------------------------------------------------------------------------*/
static void
tels0163_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[what+1]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[what+1]);
	bus_space_read_multi_1(t,h,0x1e,buf,size);
}

/*---------------------------------------------------------------------------*
 *      Teles S0/16.3 write fifo routine
 *---------------------------------------------------------------------------*/
static void
tels0163_write_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[what+1]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[what+1]);
	bus_space_write_multi_1(t,h,0x1e,buf,size);
}

/*---------------------------------------------------------------------------*
 *      Teles S0/16.3 ISAC put register routine
 *---------------------------------------------------------------------------*/
static void
tels0163_write_reg(struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[what+1]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[what+1]);
	bus_space_write_1(t,h,offs - 0x20,data);
}

/*---------------------------------------------------------------------------*
 *	Teles S0/16.3 ISAC get register routine
 *---------------------------------------------------------------------------*/
static u_int8_t
tels0163_read_reg(struct l1_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[what+1]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[what+1]);
	return bus_space_read_1(t,h,offs - 0x20);
}

/*---------------------------------------------------------------------------*
 *	isic_probe_s0163 - probe routine for Teles S0/16.3
 *---------------------------------------------------------------------------*/
int
isic_probe_s0163(device_t dev)
{
	size_t unit = device_get_unit(dev);	/* get unit */
	struct l1_softc *sc = 0;	/* pointer to softc */
	void *ih = 0;			/* dummy */
	bus_space_tag_t    t;		/* bus things */
	bus_space_handle_t h;
	u_int8_t b0,b1,b2;		/* signature */

	/* check max unit range */

	if(unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for Teles 16.3!\n",
				unit, unit);
		return(ENXIO);	
	}

	sc = &l1_sc[unit];			/* get pointer to softc */
	sc->sc_unit = unit;			/* set unit */

	/* see if an io base was supplied */
	
	if(!(sc->sc_resources.io_base[0] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
	                                   &sc->sc_resources.io_rid[0],
	                                   0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get iobase for Teles S0/16.3.\n",
				unit);
		return(ENXIO);
	}

	/* set io base */

	sc->sc_port = rman_get_start(sc->sc_resources.io_base[0]);
	/* Release the resource -  re-allocate later with correct size	*/
        bus_release_resource(dev, SYS_RES_IOPORT, sc->sc_resources.io_rid[0],
			sc->sc_resources.io_base[0]);
	
	switch(sc->sc_port)
	{
		case 0xd80:
		case 0xe80:
		case 0xf80:
			break;
			
		case 0x180:
		case 0x280:
		case 0x380:
			printf("isic%d: Error, instead of using iobase 0x%x for your Teles S0/16.3,\n",
				unit, sc->sc_port);
			printf("isic%d:        please use 0x%x in the kernel configuration file!\n",
				unit, sc->sc_port+0xc00);			
			isic_detach_common(dev);
			return(ENXIO);
			break;
	
		default:
			printf("isic%d: Error, invalid iobase 0x%x specified for Teles S0/16.3!\n",
				unit, sc->sc_port);
			isic_detach_common(dev);
			return(ENXIO);
			break;
	}
	
	/* set io port resources */

	sc->sc_resources.io_rid[0] = 0;	
	bus_set_resource(dev, SYS_RES_IOPORT, 0, sc->sc_port, 0x20);
	sc->sc_resources.io_base[0] =
		bus_alloc_resource(dev, SYS_RES_IOPORT,
				   &sc->sc_resources.io_rid[0],
				   0ul, ~0ul, 1, RF_ACTIVE);
	if(!sc->sc_resources.io_base[0])
	{
		printf("isic%d: Error allocating io at 0x%x for Teles S0/16.3!\n",
			unit, sc->sc_port);
		isic_detach_common(dev);
		return ENXIO;
	}
	sc->sc_resources.io_rid[1] = 1;	
	bus_set_resource(dev, SYS_RES_IOPORT, 1,
		sc->sc_port-ISAC_OFFS, 0x20);
	sc->sc_resources.io_base[1] =
		bus_alloc_resource(dev, SYS_RES_IOPORT,
				   &sc->sc_resources.io_rid[1],
				   0ul, ~0ul, 1, RF_ACTIVE);
	if(!sc->sc_resources.io_base[1])
	{
		printf("isic%d: Error allocating io at 0x%x for Teles S0/16.3!\n",
			unit, sc->sc_port-ISAC_OFFS);
		isic_detach_common(dev);
		return ENXIO;
	}
	
	sc->sc_resources.io_rid[2] = 2;
	bus_set_resource(dev, SYS_RES_IOPORT, 2,
		sc->sc_port-HSCXA_OFFS, 0x20);
	sc->sc_resources.io_base[2] =
		bus_alloc_resource(dev, SYS_RES_IOPORT,
				   &sc->sc_resources.io_rid[2],
				   0ul, ~0ul, 1, RF_ACTIVE);
	if(!sc->sc_resources.io_base[2])
	{
		printf("isic%d: Error allocating io at 0x%x for Teles S0/16.3!\n",
			unit, sc->sc_port-HSCXA_OFFS);
		isic_detach_common(dev);
		return ENXIO;
	}

	sc->sc_resources.io_rid[3] = 3;
	bus_set_resource(dev, SYS_RES_IOPORT, 3,
		sc->sc_port-HSCXB_OFFS, 0x20);
	sc->sc_resources.io_base[3] =
		bus_alloc_resource(dev, SYS_RES_IOPORT,
				   &sc->sc_resources.io_rid[3],
				   0ul, ~0ul, 1, RF_ACTIVE);
	if(!sc->sc_resources.io_base[3])
	{
		printf("isic%d: Error allocating io at 0x%x for Teles S0/16.3!\n",
			unit, sc->sc_port-HSCXB_OFFS);
		isic_detach_common(dev);
		return ENXIO;
	}

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = tels0163_read_reg;
	sc->writereg = tels0163_write_reg;

	sc->readfifo = tels0163_read_fifo;
	sc->writefifo = tels0163_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp= CARD_TYPEP_16_3;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	t = rman_get_bustag(sc->sc_resources.io_base[0]);
	h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	b0 = bus_space_read_1(t, h, 0);
	b1 = bus_space_read_1(t, h, 1);
	b2 = bus_space_read_1(t, h, 2);
	
	if ( b0 != 0x51 && b0 != 0x10 ) {
		printf("isic%d: Error, signature 1 0x%x != 0x51 or 0x10 for Teles S0/16.3!\n",
			unit, b0);
		isic_detach_common(dev);
		return ENXIO;
	}
	
	if ( b1 != 0x93 ) {
		printf("isic%d: Error, signature 2 0x%x != 0x93 for Teles S0/16.3!\n",
			unit, b1);
		isic_detach_common(dev);
		return ENXIO;
	}
	if (( b2 != 0x1c ) && ( b2 != 0x1f )) {
		printf("isic%d: Error, signature 3 0x%x != (0x1c || 0x1f) for Teles S0/16.3!\n",
			unit, b2);
		isic_detach_common(dev);
		return ENXIO;	
	}
	
	/* 
	 * Read HSCX A/B VSTR.  Expected value for the S0/16.3 card is
	 * 0x05 or 0x04 (for older 16.3's) in the least significant bits.
	 */
	 
	if( (((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) &&
	     ((HSCX_READ(0, H_VSTR) & 0xf) != 0x4))	||
            (((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) &&
	     ((HSCX_READ(1, H_VSTR) & 0xf) != 0x4)) )  
	{
		printf("isic%d: HSCX VSTR test failed for Teles S0/16.3\n",
			unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			unit, HSCX_READ(1, H_VSTR));
		isic_detach_common(dev);
		return (ENXIO);
	}                   

	/* get our irq */

	if(!(sc->sc_resources.irq =
		bus_alloc_resource(dev, SYS_RES_IRQ,
		&sc->sc_resources.irq_rid,
		0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get IRQ for Teles S0/16.3.\n",unit);
		isic_detach_common(dev);
		return ENXIO;
	}

	/* get the irq number */
	sc->sc_irq = rman_get_start(sc->sc_resources.irq);

	switch(sc->sc_irq)
	{
		case 2:
		case 9:
		case 5:
		case 10:
		case 12:
		case 15:
			break;

		default:
			printf("isic%d: Error, invalid IRQ [%d] specified for Teles S0/16.3!\n",
				unit, sc->sc_irq);
			isic_detach_common(dev);
			return(ENXIO);
			break;
	}

	/* register interupt routine */
	bus_setup_intr(dev, sc->sc_resources.irq,
			INTR_TYPE_NET,
			(void(*)(void *))(isicintr),
			sc, &ih);

	return (0);
}

/*---------------------------------------------------------------------------*
 *	isic_attach_s0163 - attach Teles S0/16.3 and compatibles
 *---------------------------------------------------------------------------*/
int
isic_attach_s0163(device_t dev)
{
	unsigned int unit = device_get_unit(dev);	/* get unit */
	struct l1_softc *sc = &l1_sc[unit];
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	/* configure IRQ */
	
	DELAY(SEC_DELAY / 10);
	bus_space_write_1(t, h, 4, intr_no[sc->sc_irq]);

	DELAY(SEC_DELAY / 10);
	bus_space_write_1(t, h, 4, intr_no[sc->sc_irq] | 0x01);

	return (0);
}

#endif /* ISIC > 0 */

