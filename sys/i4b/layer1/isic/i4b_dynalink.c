/*
 *   Copyright (c) 1998 Martijn Plak. All rights reserved.
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
 *	isdn4bsd layer1 driver for Dynalink IS64PH isdn TA
 *	==================================================
 *
 *      $Id: i4b_dynalink.c,v 1.1 2000/09/04 09:17:26 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Sep  4 09:47:18 2000]
 *
 *---------------------------------------------------------------------------*/

/*	NOTES:
	
	This driver was written for the Dynalink IS64PH ISDN TA, based on two 
	Siemens chips (HSCX 21525 and ISAC 2186). It is sold in the Netherlands.
	
	model numbers found on (my) card:
		IS64PH, TAS100H-N, P/N:89590555, TA200S100045521
	
	chips: 	
		Siemens PSB 21525N, HSCX TE V2.1
		Siemens PSB 2186N, ISAC-S TE V1.1
		95MS14, PNP
	
	plug-and-play info: 
		device id 	"ASU1688" 
		vendor id 	0x88167506 
		serial 		0x00000044
		i/o port	4 byte alignment, 4 bytes requested, 
				10 bit i/o decoding, 0x100-0x3f8 (?)
		irq		3,4,5,9,10,11,12,15, high true, edge sensitive
			
	At the moment I'm writing this Dynalink is replacing this card with 
	one based on a single Siemens chip (IPAC). It will apparently be sold 
	under the same model name.

	This driver might also work for Asuscom cards.
*/

#include "isic.h"
#include "opt_i4b.h"

#if (NISIC > 0) && defined(DYNALINK)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <machine/clock.h>

#include <net/if.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_isac.h>
#include <i4b/layer1/isic/i4b_hscx.h>

/* io address mapping */
#define ISAC		0
#define HSCX		1
#define	ADDR		2

/* ADDR bits */
#define ADDRMASK	0x7F
#define RESET		0x80

/* HSCX register offsets */
#define HSCXA		0x00
#define HSCXB		0x40

/*	LOW-LEVEL DEVICE ACCESS
*/

static void             
dynalink_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ADDR, 0);
			bus_space_read_multi_1(t, h, ISAC, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ADDR, HSCXA);
			bus_space_read_multi_1(t, h, HSCX, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ADDR, HSCXB);
			bus_space_read_multi_1(t, h, HSCX, buf, size);
			break;
	}
}

static void
dynalink_write_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ADDR, 0);
			bus_space_write_multi_1(t, h, ISAC, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ADDR, HSCXA);
			bus_space_write_multi_1(t, h, HSCX, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ADDR, HSCXB);
			bus_space_write_multi_1(t, h, HSCX, (u_int8_t*)buf, size);
			break;
	}
}

static void
dynalink_write_reg(struct l1_softc *sc, int what, bus_size_t reg, u_int8_t data)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ADDR, reg);
			bus_space_write_1(t, h, ISAC, data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ADDR, HSCXA+reg);
			bus_space_write_1(t, h, HSCX, data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ADDR, HSCXB+reg);
			bus_space_write_1(t, h, HSCX, data);
			break;
	}
}

static u_int8_t
dynalink_read_reg(struct l1_softc *sc, int what, bus_size_t reg)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ADDR, reg);
			return bus_space_read_1(t, h, ISAC);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ADDR, HSCXA+reg);
			return bus_space_read_1(t, h, HSCX);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ADDR, HSCXB+reg);
			return bus_space_read_1(t, h, HSCX);
	}
	return 0;
}

/* attach callback routine */
int
isic_attach_Dyn(device_t dev)
{
	int unit = device_get_unit(dev);	/* get unit */
	struct l1_softc *sc = &l1_sc[unit];		/* pointer to softc */

	struct i4b_info *  info = &(sc->sc_resources);
	bus_space_tag_t    t = rman_get_bustag(info->io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(info->io_base[0]);

	/* fill in l1_softc structure */
	sc->readreg	= dynalink_read_reg;
	sc->writereg	= dynalink_write_reg;
	sc->readfifo	= dynalink_read_fifo;
	sc->writefifo	= dynalink_write_fifo;
	sc->clearirq	= NULL;
	sc->sc_cardtyp = CARD_TYPEP_DYNALINK;
	sc->sc_bustyp = BUS_TYPE_IOM2;
	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* Read HSCX A/B VSTR.  Expected value is 0x05 (V2.1). */
	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) || 
	    ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for Dynalink\n",
			sc->sc_unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			sc->sc_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			sc->sc_unit, HSCX_READ(1, H_VSTR));
		return ENXIO;
	}

	/* reset card */
	bus_space_write_1(t,h,ADDR,RESET);
	DELAY(SEC_DELAY / 10);
	bus_space_write_1(t,h,ADDR,0);
	DELAY(SEC_DELAY / 10);

	return 0;                
}

#endif /* (NISIC > 0) && defined(DYNALINK) */
