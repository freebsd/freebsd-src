/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 *	isic - I4B Siemens ISDN Chipset Driver for Creatix/Teles PnP
 *	============================================================
 *
 *	$Id: i4b_ctx_s0P.c,v 1.4 2000/08/22 11:30:04 hm Exp $ 
 *
 * $FreeBSD$
 *
 *	last edit-date: [Fri Oct 13 15:59:45 2000]
 *
 *	Note: this driver works for the Creatix ISDN S0-16 P+P and
 *	      for the Teles S0/16.3 PnP card. Although they are not
 *            the same hardware and don't share the same PnP config
 *            information, once the base addresses are set, the
 *            offsets are same and therefore they can use the same
 *            driver.
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"
#include "opt_i4b.h"

#if (NISIC > 0) && (defined(CRTX_S0_P) || defined(TEL_S0_16_3_P))

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>


#include <net/if.h>

#include <machine/i4b_ioctl.h>


#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_hscx.h>

/*---------------------------------------------------------------------------*
 *      Creatix / Teles PnP ISAC get fifo routine
 *---------------------------------------------------------------------------*/
static void
ctxs0P_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[what+2]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[what+2]);
	bus_space_read_multi_1(t,h,0x3e,buf,size);
}

/*---------------------------------------------------------------------------*
 *      Creatix / Teles PnP ISAC put fifo routine
 *---------------------------------------------------------------------------*/
static void
ctxs0P_write_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[what+2]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[what+2]);
	bus_space_write_multi_1(t,h,0x3e,buf,size);
}

/*---------------------------------------------------------------------------*
 *      Creatix / Teles PnP ISAC put register routine
 *---------------------------------------------------------------------------*/
static void
ctxs0P_write_reg(struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[what+2]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[what+2]);
	bus_space_write_1(t,h,offs,data);
}

/*---------------------------------------------------------------------------*
 *	Creatix / Teles PnP ISAC get register routine
 *---------------------------------------------------------------------------*/
static u_int8_t
ctxs0P_read_reg(struct l1_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[what+2]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[what+2]);
	return bus_space_read_1(t,h,offs);
}

/*---------------------------------------------------------------------------*
 *	isic_attach_Cs0P - attach Creatix / Teles PnP
 *---------------------------------------------------------------------------*/
int
isic_attach_Cs0P(device_t dev)
{
	u_int32_t iobase1;
	u_int32_t iobase2;
	int unit = device_get_unit(dev);
	struct l1_softc *sc = &l1_sc[unit];	
	bus_space_tag_t t;
	bus_space_handle_t h;

	/*
	 * this card needs a second io_base,
	 * free resources if we don't get it
	 */

	sc->sc_resources.io_rid[1] = 1;
	
	if(!(sc->sc_resources.io_base[1] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
					&sc->sc_resources.io_rid[1],
					0UL, ~0UL, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get io area 1 for Creatix / Teles PnP!\n", unit);
		isic_detach_common(dev);
		return ENXIO;
	}

	/* remember the io base addresses */
	
	iobase1 = rman_get_start(sc->sc_resources.io_base[0]);
	iobase2 = rman_get_start(sc->sc_resources.io_base[1]);
	
	/*
	 * because overlapping resources are invalid,
	 * release the first io port resource
	 */
	
        bus_release_resource(dev, SYS_RES_IOPORT, sc->sc_resources.io_rid[0],
			sc->sc_resources.io_base[0]);
	
	/* set and allocate a base io address for the ISAC chip */
	
	sc->sc_resources.io_rid[2] = 2;
	
	bus_set_resource(dev, SYS_RES_IOPORT, 2, iobase1-0x20, 0x40);
	
	if(!(sc->sc_resources.io_base[2] =
		bus_alloc_resource(dev, SYS_RES_IOPORT,
				   &sc->sc_resources.io_rid[2],
				   0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get io area 2 for Creatix / Teles PnP!\n", unit);
		isic_detach_common(dev);
		return ENXIO;
	}

	/*
	 * because overlapping resources are invalid,
	 * release the second io port resource
	 */

        bus_release_resource(dev, SYS_RES_IOPORT, sc->sc_resources.io_rid[1],
			sc->sc_resources.io_base[1]);

	/* set and allocate a resource for the HSCX channel A */
	
	sc->sc_resources.io_rid[3] = 3;

/*XXX*/	/* FIXME !!!!
	 * the width of the resource is too small, there are accesses
	 * to it with an offset of 0x3e into the next resource. anyway,
         * it seems to work and i have no idea how to do 2 resources
	 * overlapping each other.
	 */

#if 0
	bus_set_resource(dev, SYS_RES_IOPORT, 3, iobase2-0x20, 0x20);
#else
	bus_set_resource(dev, SYS_RES_IOPORT, 3, iobase2-0x20, 0x10);
#endif

	if(!(sc->sc_resources.io_base[3] =
		bus_alloc_resource(dev,SYS_RES_IOPORT,
				   &sc->sc_resources.io_rid[3],
				   0ul,~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get io area 3 for Creatix / Teles PnP!\n", unit);
		isic_detach_common(dev);
		return ENXIO;
	}

	/* set and allocate a resources for the HSCX channel B */
	
	sc->sc_resources.io_rid[4] = 4;
	
	bus_set_resource(dev, SYS_RES_IOPORT, 4, iobase2, 0x40);
					
	if(!(sc->sc_resources.io_base[4] =
		bus_alloc_resource(dev,SYS_RES_IOPORT,
				   &sc->sc_resources.io_rid[4],
				   0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get io area 4 for Creatix / Teles PnP!\n", unit);
		isic_detach_common(dev);
		return ENXIO;
	}
	
	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = ctxs0P_read_reg;
	sc->writereg = ctxs0P_write_reg;

	sc->readfifo = ctxs0P_read_fifo;
	sc->writefifo = ctxs0P_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_CS0P;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* enable the card */
	
	t = rman_get_bustag(sc->sc_resources.io_base[2]);
	h = rman_get_bushandle(sc->sc_resources.io_base[2]);
	
	bus_space_write_1(t, h, 0x3c, 0);
	DELAY(SEC_DELAY / 10);

	bus_space_write_1(t, h, 0x3c, 1);
	DELAY(SEC_DELAY / 10);

	return 0;
}

#endif /* (NISIC > 0) && (defined(CRTX_S0_P) || defined(TEL_S0_16_3_P)) */

