/*-
 * Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
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
 */

/*---------------------------------------------------------------------------
 *
 *	i4b_drn_ngo.c - Dr. Neuhaus Niccy GO@ and SAGEM Cybermod
 *	--------------------------------------------------------
 *      last edit-date: [Wed Jan 24 09:07:44 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i4b/layer1/isic/i4b_drn_ngo.c,v 1.10 2007/07/06 07:17:20 bz Exp $");

#include "opt_i4b.h"

#if defined(DRN_NGO)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>

#include <i4b/include/i4b_ioctl.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_hscx.h>

/*---------------------------------------------------------------------------*
 *	Niccy GO@ definitions
 *
 *	the card uses 2 i/o addressranges each using 2 bytes
 *
 *	addressrange 0:
 *		offset 0 - ISAC dataregister
 *		offset 1 - HSCX dataregister
 *	addressrange 1:
 *		offset 0 - ISAC addressregister
 *		offset 1 - HSCX addressregister
 *
 *	to access an ISAC/HSCX register, you have to write the register
 *	number into the ISAC or HSCX addressregister and then read/write
 *	data for the ISAC/HSCX register into/from the corresponding
 *	dataregister.
 *
 *	Thanks to Klaus Muehle of Dr. Neuhaus Telekommunikation for giving
 *	out this information!
 *                                                     
 *---------------------------------------------------------------------------*/
#define NICCY_PORT_MIN	0x200
#define NICCY_PORT_MAX	0x3e0

#define HSCX_ABIT	0x1000		/* flag, HSCX A is meant */
#define HSCX_BBIT	0x2000		/* flag, HSCX B is meant */

#define HSCX_BOFF	0x40

#define ADDR_OFF	2		/* address register range offset */

#define ISAC_DATA	0
#define HSCX_DATA	1

#define ISAC_ADDR	0
#define HSCX_ADDR	1

/*---------------------------------------------------------------------------*
 *	Dr. Neuhaus Niccy GO@ read fifo routine
 *---------------------------------------------------------------------------*/
static void
drnngo_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t tdata, tadr;
	bus_space_handle_t hdata, hadr;

	tdata = rman_get_bustag(sc->sc_resources.io_base[0]);
	hdata = rman_get_bushandle(sc->sc_resources.io_base[0]);
	tadr = rman_get_bustag(sc->sc_resources.io_base[1]);
	hadr = rman_get_bushandle(sc->sc_resources.io_base[1]);

	switch(what)
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1     (tadr ,hadr, ISAC_ADDR,0x0);
			bus_space_read_multi_1(tdata,hdata,ISAC_DATA,buf,size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1     (tadr ,hadr ,HSCX_ADDR,0x0);
			bus_space_read_multi_1(tdata,hdata,HSCX_DATA,buf,size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1     (tadr ,hadr ,HSCX_ADDR,HSCX_BOFF);
			bus_space_read_multi_1(tdata,hdata,HSCX_DATA,buf,size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	Dr. Neuhaus Niccy GO@ write fifo routine
 *---------------------------------------------------------------------------*/
static void
drnngo_write_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t tdata, tadr;
	bus_space_handle_t hdata, hadr;

	tdata = rman_get_bustag(sc->sc_resources.io_base[0]);
	hdata = rman_get_bushandle(sc->sc_resources.io_base[0]);
	tadr = rman_get_bustag(sc->sc_resources.io_base[1]);
	hadr = rman_get_bushandle(sc->sc_resources.io_base[1]);

	switch(what)
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1      (tadr ,hadr, ISAC_ADDR,0x0);
			bus_space_write_multi_1(tdata,hdata,ISAC_DATA,buf,size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1      (tadr ,hadr ,HSCX_ADDR,0x0);
			bus_space_write_multi_1(tdata,hdata,HSCX_DATA,buf,size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1      (tadr ,hadr ,HSCX_ADDR,HSCX_BOFF);
			bus_space_write_multi_1(tdata,hdata,HSCX_DATA,buf,size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	Dr. Neuhaus Niccy GO@ write register routine
 *---------------------------------------------------------------------------*/
static void 
drnngo_write_reg(struct l1_softc *sc, int what, bus_size_t reg, u_int8_t data)
{
	bus_space_tag_t tdata, tadr;
	bus_space_handle_t hdata, hadr;

	tdata = rman_get_bustag(sc->sc_resources.io_base[0]);
	hdata = rman_get_bushandle(sc->sc_resources.io_base[0]);
	tadr = rman_get_bustag(sc->sc_resources.io_base[1]);
	hadr = rman_get_bushandle(sc->sc_resources.io_base[1]);

	switch(what)
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(tadr ,hadr, ISAC_ADDR,reg);
			bus_space_write_1(tdata,hdata,ISAC_DATA,data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(tadr ,hadr ,HSCX_ADDR,reg);
			bus_space_write_1(tdata,hdata,HSCX_DATA,data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(tadr ,hadr ,HSCX_ADDR,reg+HSCX_BOFF);
			bus_space_write_1(tdata,hdata,HSCX_DATA,data);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	Dr. Neuhaus Niccy GO@ read register routine
 *---------------------------------------------------------------------------*/
static u_int8_t
drnngo_read_reg(struct l1_softc *sc, int what, bus_size_t reg)
{
	bus_space_tag_t tdata, tadr;
	bus_space_handle_t hdata, hadr;

	tdata = rman_get_bustag(sc->sc_resources.io_base[0]);
	hdata = rman_get_bushandle(sc->sc_resources.io_base[0]);
	tadr = rman_get_bustag(sc->sc_resources.io_base[1]);
	hadr = rman_get_bushandle(sc->sc_resources.io_base[1]);

	switch(what)
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(tadr ,hadr, ISAC_ADDR,reg);
			return bus_space_read_1(tdata,hdata,ISAC_DATA);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(tadr ,hadr ,HSCX_ADDR,reg);
			return bus_space_read_1(tdata,hdata,HSCX_DATA);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(tadr ,hadr ,HSCX_ADDR,reg+HSCX_BOFF);
			return bus_space_read_1(tdata,hdata,HSCX_DATA);
		default:
			return 0;
	}
}

/*---------------------------------------------------------------------------*
 *      probe for ISA PnP cards
 *---------------------------------------------------------------------------*/
int
isic_attach_drnngo(device_t dev)
{
	int unit = device_get_unit(dev);
	struct l1_softc *sc = &l1_sc[unit];

	sc->sc_resources.io_rid[1] = 1;	

	/*
	 * this card needs a second io_base,
	 * free resources if we don't get it
	 */

	if(!(sc->sc_resources.io_base[1] =
			bus_alloc_resource_any(dev, SYS_RES_IOPORT,
					       &sc->sc_resources.io_rid[1],
					       RF_ACTIVE)))
	{
		printf("isic%d: Failed to get second io base.\n", unit);
		isic_detach_common(dev);
		return ENXIO;
	}

	/* setup ISAC access routines */

	sc->clearirq = NULL;
	sc->readreg = drnngo_read_reg;
	sc->writereg = drnngo_write_reg;

	sc->readfifo = drnngo_read_fifo;
	sc->writefifo = drnngo_write_fifo;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_DRNNGO;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;
	
	return (0);
}

#endif /* defined(DRN_NGO) */
