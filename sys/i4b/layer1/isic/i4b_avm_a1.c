/*
 *   Copyright (c) 1996 Andrew Gordon. All rights reserved.
 *
 *   Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 *	i4b_avm_a1.c - AVM A1/Fritz passive card driver for isdn4bsd
 *	------------------------------------------------------------
 *
 *	$Id: i4b_avm_a1.c,v 1.3 2000/05/29 15:41:41 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon May 29 16:42:06 2000]
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"
#include "opt_i4b.h"

#if NISIC > 0 && defined(AVM_A1)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/if.h>

#include <machine/clock.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>


#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_isac.h>
#include <i4b/layer1/isic/i4b_hscx.h>

/*---------------------------------------------------------------------------*
 *	AVM A1 and AVM Fritz! Card special registers
 *---------------------------------------------------------------------------*/

#define	AVM_CONF_REG	0x1800		/* base offset for config register */
#define	AVM_CONF_IRQ	0x1801		/* base offset for IRQ register    */
					/* config register write           */
#define	 AVM_CONF_WR_RESET	0x01	/* 1 = RESET ISAC and HSCX         */
#define	 AVM_CONF_WR_CCL	0x02	/* 1 = clear counter low nibble    */
#define	 AVM_CONF_WR_CCH	0x04	/* 1 = clear counter high nibble   */
#define	 AVM_CONF_WR_IRQEN	0x08	/* 1 = enable IRQ                  */
#define	 AVM_CONF_WR_TEST	0x10	/* test bit                        */
					/* config register read            */
#define	 AVM_CONF_RD_IIRQ	0x01	/* 0 = ISAC IRQ active             */
#define	 AVM_CONF_RD_HIRQ	0x02	/* 0 = HSCX IRQ active             */
#define	 AVM_CONF_RD_CIRQ	0x04    /* 0 = counter IRQ active          */
#define	 AVM_CONF_RD_ZER1	0x08	/* unused, always read 0           */
#define	 AVM_CONF_RD_TEST	0x10	/* test bit read back              */
#define	 AVM_CONF_RD_ZER2	0x20	/* unused, always read 0           */

#define AVM_ISAC_R_OFFS		(0x1400-0x20)
#define AVM_HSCXA_R_OFFS	(0x400-0x20)
#define AVM_HSCXB_R_OFFS	(0xc00-0x20)
#define AVM_ISAC_F_OFFS		(0x1400-0x20-0x3e0)
#define AVM_HSCXA_F_OFFS	(0x400-0x20-0x3e0)
#define AVM_HSCXB_F_OFFS	(0xc00-0x20-0x3e0)

/*---------------------------------------------------------------------------*
 *	AVM read fifo routine
 *---------------------------------------------------------------------------*/
static void
avma1_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = rman_get_bustag(sc->sc_resources.io_base[what+4]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[what+4]);
	bus_space_read_multi_1(t, h, 0, buf, size);
}

/*---------------------------------------------------------------------------*
 *	AVM write fifo routine
 *---------------------------------------------------------------------------*/
static void
avma1_write_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = rman_get_bustag(sc->sc_resources.io_base[what+4]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[what+4]);
	bus_space_write_multi_1(t, h, 0, (u_int8_t*)buf, size);
}

/*---------------------------------------------------------------------------*
 *	AVM write register routine
 *---------------------------------------------------------------------------*/
static void
avma1_write_reg(struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = rman_get_bustag(sc->sc_resources.io_base[what+1]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[what+1]);
	bus_space_write_1(t, h, offs, data);
}

/*---------------------------------------------------------------------------*
 *	AVM read register routine
 *---------------------------------------------------------------------------*/
static u_int8_t
avma1_read_reg(struct l1_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = rman_get_bustag(sc->sc_resources.io_base[what+1]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[what+1]);
	return bus_space_read_1(t, h, offs);
}

/*---------------------------------------------------------------------------*
 *	allocate an io port
 *---------------------------------------------------------------------------*/
static int
isic_alloc_port(device_t dev, int rid, u_int base, u_int len)
{ 
	size_t unit = device_get_unit(dev);
	struct l1_softc *sc = &l1_sc[unit];

	sc->sc_resources.io_rid[rid] = rid;

	bus_set_resource(dev, SYS_RES_IOPORT, rid, base, len);

	if(!(sc->sc_resources.io_base[rid] =
		bus_alloc_resource(dev, SYS_RES_IOPORT,
				   &sc->sc_resources.io_rid[rid],
				   0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Error, failed to reserve io #%d!\n", unit, rid);
		isic_detach_common(dev);
		return(ENXIO);
	}
	return(0);
}

/*---------------------------------------------------------------------------*
 *	isic_probe_avma1 - probe for AVM A1 and compatibles
 *---------------------------------------------------------------------------*/
int
isic_probe_avma1(device_t dev)
{
	size_t unit = device_get_unit(dev);	/* get unit */
	struct l1_softc *sc = 0;	/* pointer to softc */
	void *ih = 0;			/* dummy */
	bus_space_tag_t    t;		/* bus things */
	bus_space_handle_t h;
	u_char savebyte;
	u_char byte;

	/* check max unit range */

	if(unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for AVM A1/Fritz!\n",
				unit, unit);
		return(ENXIO);	
	}

	sc = &l1_sc[unit];			/* get pointer to softc */
	sc->sc_unit = unit;			/* set unit */
	sc->sc_flags = FLAG_AVM_A1;		/* set flags */

	/* see if an io base was supplied */
	
	if(!(sc->sc_resources.io_base[0] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
	                                   &sc->sc_resources.io_rid[0],
	                                   0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get iobase for AVM A1/Fritz!\n",
				unit);
		return(ENXIO);
	}

	/* set io base */

	sc->sc_port = rman_get_start(sc->sc_resources.io_base[0]);
	
	/* release io base */
	
        bus_release_resource(dev, SYS_RES_IOPORT, sc->sc_resources.io_rid[0],
			sc->sc_resources.io_base[0]);

	switch(sc->sc_port)
	{
		case 0x200:
		case 0x240:
		case 0x300:
		case 0x340:		
			break;
			
		default:
			printf("isic%d: Error, invalid iobase 0x%x specified for AVM A1/Fritz!\n",
				unit, sc->sc_port);
			return(ENXIO);
			break;
	}

	if(isic_alloc_port(dev, 0, sc->sc_port+AVM_CONF_REG, 0x20))
		return(ENXIO);

	if(isic_alloc_port(dev, 1, sc->sc_port+AVM_ISAC_R_OFFS, 0x20))
		return(ENXIO);

	if(isic_alloc_port(dev, 2, sc->sc_port+AVM_HSCXA_R_OFFS, 0x20))
		return(ENXIO);

	if(isic_alloc_port(dev, 3, sc->sc_port+AVM_HSCXB_R_OFFS, 0x20))
		return(ENXIO);

	if(isic_alloc_port(dev, 4, sc->sc_port+AVM_ISAC_F_OFFS, 0x20))
		return(ENXIO);

	if(isic_alloc_port(dev, 5, sc->sc_port+AVM_HSCXA_F_OFFS, 0x20))
		return(ENXIO);

	if(isic_alloc_port(dev, 6, sc->sc_port+AVM_HSCXB_F_OFFS, 0x20))
		return(ENXIO);

	/* get our irq */

	if(!(sc->sc_resources.irq =
		bus_alloc_resource(dev, SYS_RES_IRQ,
				   &sc->sc_resources.irq_rid,
				   0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get an irq for AVM A1/Fritz!\n",unit);
		isic_detach_common(dev);
		return ENXIO;
	}

	/* get the irq number */
	sc->sc_irq = rman_get_start(sc->sc_resources.irq);

	/* register interupt routine */
	bus_setup_intr(dev, sc->sc_resources.irq, INTR_TYPE_NET,
			(void(*)(void *))(isicintr),
			sc, &ih);

	/* check IRQ validity */

	switch(sc->sc_irq)
	{
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			break;
			
		default:
			printf("isic%d: Error, invalid IRQ [%d] specified for AVM A1/Fritz!\n",
				unit, sc->sc_irq);
			isic_detach_common(dev);
			return(ENXIO);
			break;
	}		

	sc->clearirq = NULL;
	sc->readreg = avma1_read_reg;
	sc->writereg = avma1_write_reg;

	sc->readfifo = avma1_read_fifo;
	sc->writefifo = avma1_write_fifo;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_AVMA1;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

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
		printf("isic%d: HSCX VSTR test failed for AVM A1/Fritz\n",
			unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			unit, HSCX_READ(1, H_VSTR));
		return(ENXIO);
	}                   

	/* AVM A1 or Fritz! control register bits:	*/
	/*        read                write		*/
	/* 0x01  hscx irq*           RESET		*/
	/* 0x02  isac irq*           clear counter1	*/
	/* 0x04  counter irq*        clear counter2	*/
	/* 0x08  always 0            irq enable		*/
	/* 0x10  read test bit       set test bit	*/
	/* 0x20  always 0            unused		*/

	/*
	 * XXX the following test may be destructive, to prevent the
	 * worst case, we save the byte first, and in case the test
	 * fails, we write back the saved byte .....
	 */

	t = rman_get_bustag(sc->sc_resources.io_base[0]);
	h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	savebyte = bus_space_read_1(t, h, 0);
	
	/* write low to test bit */

	bus_space_write_1(t, h, 0, 0x00);
	
	/* test bit and next higher and lower bit must be 0 */

	if((byte = bus_space_read_1(t, h, 0) & 0x38) != 0x00)
	{
		printf("isic%d: Error, probe-1 failed, 0x%02x should be 0x00 for AVM A1/Fritz!\n",
				unit, byte);
		bus_space_write_1(t, h, 0, savebyte);
		return(ENXIO);
	}

	/* write high to test bit */

	bus_space_write_1(t, h, 0, 0x10);
	
	/* test bit must be high, next higher and lower bit must be 0 */

	if((byte = bus_space_read_1(t, h, 0) & 0x38) != 0x10)
	{
		printf("isic%d: Error, probe-2 failed, 0x%02x should be 0x10 for AVM A1/Fritz!\n",
				unit, byte);
		bus_space_write_1(t, h, 0, savebyte);
		return(ENXIO);
	}
	return(0);
}

/*---------------------------------------------------------------------------*
 *	isic_attach_avma1 - attach AVM A1 and compatibles
 *---------------------------------------------------------------------------*/
int
isic_attach_avma1(device_t dev)
{
	size_t unit = device_get_unit(dev);
	struct l1_softc *sc = &l1_sc[unit];
	bus_space_tag_t t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	/* reset ISAC/HSCX */

	bus_space_write_1(t, h, 0, 0x00);
	DELAY(SEC_DELAY / 10);

	bus_space_write_1(t, h, 0, AVM_CONF_WR_RESET);
	DELAY(SEC_DELAY / 10);

	bus_space_write_1(t, h, 0, 0x00);
	DELAY(SEC_DELAY / 10);

	/* setup IRQ */

	bus_space_write_1(t, h, 1, sc->sc_irq);
	DELAY(SEC_DELAY / 10);

	/* enable IRQ, disable counter IRQ */

	bus_space_write_1(t, h, 0, AVM_CONF_WR_IRQEN |
				AVM_CONF_WR_CCH | AVM_CONF_WR_CCL);
	DELAY(SEC_DELAY / 10);

	return(0);
}

#endif /* NISIC > 0 && defined(AVM_A1) */
