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
 *	isic - I4B Siemens ISDN Chipset Driver for Teles S0/8 and clones
 *	================================================================
 *
 * $FreeBSD$
 *
 *      last edit-date: [Wed Jan 24 09:27:58 2001]
 *
 *---------------------------------------------------------------------------*/

#include "opt_i4b.h"

#if defined(TEL_S0_8)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>

#include <machine/md_var.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_hscx.h>

#define TELES_S08_MEMSIZE 0x1000

static const bus_size_t offset[] = { 0x100, 0x180, 0x1c0 };

/*---------------------------------------------------------------------------*
 *	Teles S0/8 write register routine
 *---------------------------------------------------------------------------*/
static void
tels08_write_reg(struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = rman_get_bustag(sc->sc_resources.mem);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.mem);

	offs += offset[what];

	if (offs & 0x01)
		offs |= 0x200;

	bus_space_write_1(t, h, offs, data);
}

/*---------------------------------------------------------------------------*
 *	Teles S0/8 read register routine
 *---------------------------------------------------------------------------*/
static u_int8_t
tels08_read_reg(struct l1_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = rman_get_bustag(sc->sc_resources.mem);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.mem);

	offs += offset[what];

	if (offs & 0x01)
		offs |= 0x200;

	return bus_space_read_1(t, h, offs);
}

/*---------------------------------------------------------------------------*
 *	Teles S0/8 fifo write access
 *---------------------------------------------------------------------------*/
static void
tels08_write_fifo(struct l1_softc *sc, int what, void *data, size_t size)
{
	bus_space_tag_t t = rman_get_bustag(sc->sc_resources.mem);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.mem);
	bus_space_write_region_1(t, h, offset[what], data, size);
}

/*---------------------------------------------------------------------------*
 *	Teles S0/8 fifo read access
 *---------------------------------------------------------------------------*/
static void
tels08_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = rman_get_bustag(sc->sc_resources.mem);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.mem);
	bus_space_read_region_1(t, h, offset[what], buf, size);
}

/*---------------------------------------------------------------------------*
 *	isic_probe_s08 - probe for Teles S0/8 and compatibles
 *---------------------------------------------------------------------------*/
int
isic_probe_s08(device_t dev)
{
	size_t unit = device_get_unit(dev);	/* get unit */
	struct l1_softc *sc = 0;		/* pointer to softc */
	void *ih = 0;				/* dummy */

	/* check max unit range */

	if(unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for Teles S0/8!\n",
				unit, unit);
		return(ENXIO);	
	}

	sc = &l1_sc[unit];		/* get pointer to softc */
	sc->sc_unit = unit;		/* set unit */

	/* see if an io base was supplied */

	if((sc->sc_resources.io_base[0] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
	                                   &sc->sc_resources.io_rid[0],
	                                   0ul, ~0ul, 1, RF_ACTIVE)))
	{
		/* the S0/8 is completely memory mapped ! */
		
	 	bus_release_resource(dev,SYS_RES_IOPORT,
				     sc->sc_resources.io_rid[0],
				     sc->sc_resources.io_base[0]);
		printf("isic%d: Error, iobase specified for Teles S0/8!\n", unit);
		return(ENXIO);
	}

	/* allocate memory */

	if(!(sc->sc_resources.mem =
		bus_alloc_resource(dev, SYS_RES_MEMORY,
				&sc->sc_resources.mem_rid,
				0ul, ~0ul, TELES_S08_MEMSIZE, RF_ACTIVE)))
	{
		printf("isic%d: Could not allocate memory for Teles S0/8!\n", unit);
		return(ENXIO);
	}

	/* 
	 * get virtual addr. it's just needed to see if it is in
	 * the valid range
	 */

	sc->sc_vmem_addr = rman_get_virtual(sc->sc_resources.mem);
		
	/* check if inside memory range of 0xA0000 .. 0xDF000 */

	if((kvtop(sc->sc_vmem_addr) < 0xa0000) ||
	   (kvtop(sc->sc_vmem_addr) > 0xdf000))
	{
		printf("isic%d: Error, mem addr 0x%lx outside 0xA0000-0xDF000 for Teles S0/8!\n",
				unit, kvtop(sc->sc_vmem_addr));
		bus_release_resource(dev,SYS_RES_MEMORY,
				     sc->sc_resources.mem_rid,
				     sc->sc_resources.mem);
		sc->sc_resources.mem = 0;
		return(ENXIO);
	}
	
	/* setup ISAC access routines */

	sc->clearirq = NULL;

	sc->readreg = tels08_read_reg;
	sc->writereg = tels08_write_reg;

	sc->readfifo = tels08_read_fifo;
	sc->writefifo = tels08_write_fifo;

	sc->sc_cardtyp = CARD_TYPEP_8;		/* setup card type */
	
	sc->sc_bustyp = BUS_TYPE_IOM1;		/* setup IOM bus type */

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* setup ISAC base addr, though we don't really need it */
	
	ISAC_BASE = (caddr_t)((sc->sc_vmem_addr) + 0x100);

	/* setup HSCX base addr */
	
	HSCX_A_BASE = (caddr_t)((sc->sc_vmem_addr) + 0x180);
	HSCX_B_BASE = (caddr_t)((sc->sc_vmem_addr) + 0x1c0);

	/* allocate our irq */

	if(!(sc->sc_resources.irq =
			bus_alloc_resource(dev, SYS_RES_IRQ,
						&sc->sc_resources.irq_rid,
						0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not allocate irq for Teles S0/8!\n",unit);

	 	bus_release_resource(dev,SYS_RES_MEMORY,
				     sc->sc_resources.mem_rid,
				     sc->sc_resources.mem);

		sc->sc_resources.mem = 0;
		return ENXIO;
	}

	/* get the irq number */

	sc->sc_irq = rman_get_start(sc->sc_resources.irq);
	
	/* check IRQ validity */

	switch(sc->sc_irq)
	{
		case 2:
		case 9:		/* XXX */
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			break;
			
		default:
			printf("isic%d: Error, invalid IRQ [%d] specified for Teles S0/8!\n",
				unit, sc->sc_irq);
			bus_release_resource(dev,SYS_RES_IRQ,
			                     sc->sc_resources.irq_rid,
			                     sc->sc_resources.irq);
			sc->sc_resources.irq = 0;
		 	bus_release_resource(dev,SYS_RES_MEMORY,
					     sc->sc_resources.mem_rid,
					     sc->sc_resources.mem);
			sc->sc_resources.mem = 0;
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
 *	isic_attach_s08 - attach Teles S0/8 and compatibles
 *---------------------------------------------------------------------------*/
int
isic_attach_s08(device_t dev)
{
	struct l1_softc *sc = &l1_sc[device_get_unit(dev)];
	bus_space_tag_t t = rman_get_bustag(sc->sc_resources.mem);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.mem);

	/* set card off */

	bus_space_write_1(t, h, 0x80, 0);

	DELAY(SEC_DELAY / 5);

	/* set card on */

	bus_space_write_1(t, h, 0x80, 1);

	DELAY(SEC_DELAY / 5);

	return 0;
}

#endif /* defined(TEL_S0_8) */
