/*
 *   Copyright (c) 1996 Arne Helme. All rights reserved.
 *
 *   Copyright (c) 1996 Gary Jennejohn. All rights reserved. 
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
 *	isic - I4B Siemens ISDN Chipset Driver for Teles S0/16 and clones
 *	=================================================================
 *
 *	$Id: i4b_tel_s016.c,v 1.3 2000/05/29 15:41:41 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Fri Oct 13 16:01:20 2000]
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"
#include "opt_i4b.h"

#if NISIC > 0 && defined(TEL_S0_16)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <machine/md_var.h>

#include <net/if.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_isac.h>
#include <i4b/layer1/isic/i4b_hscx.h>

#define TELES_S016_MEMSIZE 0x1000

static u_char intr_no[] = { 1, 1, 0, 2, 4, 6, 1, 1, 1, 0, 8, 10, 12, 1, 1, 14 };
static const bus_size_t offset[] = { 0x100, 0x180, 0x1c0 };

/*---------------------------------------------------------------------------*
 *	Teles S0/16 write register routine
 *---------------------------------------------------------------------------*/
static void
tels016_write_reg(struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.mem);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.mem);

	offs += offset[what];

	if (offs & 0x01)
		offs |= 0x200;

	bus_space_write_1(t, h, offs, data);
}

/*---------------------------------------------------------------------------*
 *	Teles S0/16 read register routine
 *---------------------------------------------------------------------------*/
static u_int8_t
tels016_read_reg(struct l1_softc *sc,	int what, bus_size_t offs)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.mem);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.mem);

	offs += offset[what];

	if(offs & 0x01)
		offs |= 0x200;

	return bus_space_read_1(t, h, offs);
}

/*---------------------------------------------------------------------------*
 *	Teles S0/16 fifo write routine
 *---------------------------------------------------------------------------*/
static void
tels016_write_fifo(struct l1_softc *sc, int what, void *data, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.mem);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.mem);
	bus_space_write_region_1(t, h, offset[what], data, size);
}

/*---------------------------------------------------------------------------*
 *	Teles S0/16 fifo read routine
 *---------------------------------------------------------------------------*/
static void
tels016_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.mem);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.mem);
	bus_space_read_region_1(t, h, offset[what], buf, size);
}

/*---------------------------------------------------------------------------*
 *	isic_probe_s016 - probe for Teles S0/16 and compatibles
 *---------------------------------------------------------------------------*/
int
isic_probe_s016(device_t dev)
{
	size_t unit = device_get_unit(dev);	/* get unit */
	struct l1_softc *sc = 0;		/* softc */
	void *ih = 0;				/* dummy */
	u_int8_t b0,b1,b2;			/* for signature */
	bus_space_tag_t    t;			/* bus things */
	bus_space_handle_t h;

	/* check max unit range */

	if(unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for Teles S0/16!\n",
				unit, unit);
		return(ENXIO);	
	}

	sc = &l1_sc[unit];			/* get pointer to softc */

	sc->sc_unit = unit;			/* set unit */

	sc->sc_flags = FLAG_TELES_S0_16;	/* set flags */

	/* see if an io base was supplied */

	if(!(sc->sc_resources.io_base[0] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
	                                   &sc->sc_resources.io_rid[0],
	                                   0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not allocate i/o port for Teles S0/16.\n", unit);
		return(ENXIO);
	}

	sc->sc_port = rman_get_start(sc->sc_resources.io_base[0]);

	/*
	 * check if the provided io port is valid
	 */

	switch(sc->sc_port)
	{
		case 0xd80:
		case 0xe80:
		case 0xf80:
			break;

		default:
			printf("isic%d: Error, invalid iobase 0x%x specified for Teles S0/16!\n",
				unit, sc->sc_port);
			isic_detach_common(dev);
			return(ENXIO);
			break;
	}

	/* allocate memory resource */

	if(!(sc->sc_resources.mem =
			bus_alloc_resource(dev, SYS_RES_MEMORY,
					&sc->sc_resources.mem_rid,
					0ul, ~0ul, TELES_S016_MEMSIZE,
					RF_ACTIVE)))
	{
		printf("isic%d: Could not allocate memory for Teles S0/16.\n", unit);
		isic_detach_common(dev);
		return(ENXIO);
	}

	/* 
	 * get virtual addr.
	 */
	sc->sc_vmem_addr = rman_get_virtual(sc->sc_resources.mem);

	/*
	 * check for valid adresses
	 */
	switch(kvtop(sc->sc_vmem_addr))
	{
		case 0xc0000:
		case 0xc2000:
		case 0xc4000:
		case 0xc6000:
		case 0xc8000:
		case 0xca000:
		case 0xcc000:
		case 0xce000:
		case 0xd0000:
		case 0xd2000:
		case 0xd4000:
		case 0xd6000:
		case 0xd8000:
		case 0xda000:
		case 0xdc000:
		case 0xde000:
			break;

		default:
			printf("isic%d: Error, invalid memory address 0x%lx for Teles S0/16!\n",
				unit, kvtop(sc->sc_vmem_addr));
			isic_detach_common(dev);
			return(ENXIO);
			break;
	}		
	
	/* setup ISAC access routines */

	sc->clearirq = NULL;

	sc->readreg = tels016_read_reg;
	sc->writereg = tels016_write_reg;

	sc->readfifo = tels016_read_fifo;
	sc->writefifo = tels016_write_fifo;

	/* setup card type */
	sc->sc_cardtyp = CARD_TYPEP_16;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM1;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* setup ISAC base addr, though we don't really need it */
	
	ISAC_BASE = (caddr_t)((sc->sc_vmem_addr) + 0x100);

	/* setup HSCX base addr */
	
	HSCX_A_BASE = (caddr_t)((sc->sc_vmem_addr) + 0x180);
	HSCX_B_BASE = (caddr_t)((sc->sc_vmem_addr) + 0x1c0);

	t = rman_get_bustag(sc->sc_resources.io_base[0]);
	h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	/* get signature bytes */
	b0 = bus_space_read_1(t, h, 0);
	b1 = bus_space_read_1(t, h, 1);
	b2 = bus_space_read_1(t, h, 2);

	/* check signature bytes */
	if(b0 != 0x51)
	{
		printf("isic%d: Error, signature 1 0x%x != 0x51 for Teles S0/16!\n",
			unit, b0);
		isic_detach_common(dev);
		return(ENXIO);
	}
	
	if(b1 != 0x93)
	{
		printf("isic%d: Error, signature 2 0x%x != 0x93 for Teles S0/16!\n",
			unit, b1);
		isic_detach_common(dev);
		return(ENXIO);
	}
	
	if((b2 != 0x1e) && (b2 != 0x1f))
	{
		printf("isic%d: Error, signature 3 0x%x != 0x1e or 0x1f for Teles S0/16!\n",
			unit, b2);
		isic_detach_common(dev);
		return(ENXIO);
	}

	/* get our irq */

	if(!(sc->sc_resources.irq =
			bus_alloc_resource(dev, SYS_RES_IRQ,
						&sc->sc_resources.irq_rid,
						0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not allocate irq for Teles S0/16.\n", unit);
		isic_detach_common(dev);
		return ENXIO;
	}

	/* register interupt routine */

	bus_setup_intr(dev, sc->sc_resources.irq,
			INTR_TYPE_NET,
			(void(*)(void *))(isicintr),
			sc, &ih);

	/* get the irq number */
	
	sc->sc_irq = rman_get_start(sc->sc_resources.irq);

	/* check IRQ validity */

	if((intr_no[sc->sc_irq]) == 1)
	{
		printf("isic%d: Error, invalid IRQ [%d] specified for Teles S0/16!\n",
			unit, sc->sc_irq);
		isic_detach_common(dev);
		return(ENXIO);
	}
	
	return (0);
}

/*---------------------------------------------------------------------------*
 *	isic_attach_s016 - attach Teles S0/16 and compatibles
 *---------------------------------------------------------------------------*/
int
isic_attach_s016(device_t dev)
{
	struct l1_softc *sc = &l1_sc[device_get_unit(dev)];
	u_long irq;

	bus_space_tag_t    ta = rman_get_bustag(sc->sc_resources.mem);
	bus_space_handle_t ha = rman_get_bushandle(sc->sc_resources.mem);
	bus_space_tag_t    tb = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t hb = rman_get_bushandle(sc->sc_resources.io_base[0]);

	/* is this right for FreeBSD or off by one ? */
	irq = intr_no[sc->sc_irq];

	/* configure IRQ */

	irq |= ((u_long) sc->sc_vmem_addr) >> 9;

	DELAY(SEC_DELAY / 10);
	bus_space_write_1(tb, hb, 4, irq);

	DELAY(SEC_DELAY / 10);
	bus_space_write_1(tb, hb, 4, irq | 0x01);

	DELAY(SEC_DELAY / 5);

	/* set card bit off */

	bus_space_write_1(ta, ha, 0x80, 0);
	DELAY(SEC_DELAY / 5);

	/* set card bit on */
	
	bus_space_write_1(ta, ha, 0x80, 1);
	DELAY(SEC_DELAY / 5);

	return 0;
}
#endif /* ISIC > 0 */
