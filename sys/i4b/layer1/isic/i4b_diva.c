/*
 *   Copyright (c) 2001 Hellmuth Michaelis. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
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
 *	Eicon Diehl DIVA 2.0 or 2.02 (ISA PnP) support for isic driver
 *	--------------------------------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Fri Jan 26 13:57:10 2001]
 *
 *---------------------------------------------------------------------------*/

#include "opt_i4b.h"

#if defined EICON_DIVA

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/if.h>

#include <machine/i4b_ioctl.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_ipac.h>
#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_hscx.h>

/* offsets from base address */

#define DIVA_IPAC_OFF_ALE	0x00
#define DIVA_IPAC_OFF_RW	0x01

#define DIVA_ISAC_OFF_RW	0x02
#define DIVA_ISAC_OFF_ALE	0x06

#define DIVA_HSCX_OFF_RW	0x00
#define DIVA_HSCX_OFF_ALE	0x04

#define DIVA_CTRL_OFF		0x07
#define		DIVA_CTRL_RDIST	0x01
#define		DIVA_CTRL_WRRST	0x08
#define		DIVA_CTRL_WRLDA	0x20
#define		DIVA_CTRL_WRLDB	0x40
#define		DIVA_CTRL_WRICL	0x80

/* HSCX channel base offsets */

#define DIVA_HSCXA		0x00
#define DIVA_HSCXB		0x40

/*---------------------------------------------------------------------------*
 *	Eicon Diehl DIVA 2.02
 *---------------------------------------------------------------------------*/
static void 
diva_ipac_read_fifo(struct l1_softc *sc,int what,void *buf,size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch ( what )
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,IPAC_ISAC_OFF);
			bus_space_read_multi_1(t,h,DIVA_IPAC_OFF_RW,buf,size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,IPAC_HSCXA_OFF);
			bus_space_read_multi_1(t,h,DIVA_IPAC_OFF_RW,buf,size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,IPAC_HSCXB_OFF);
			bus_space_read_multi_1(t,h,DIVA_IPAC_OFF_RW,buf,size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	Eicon Diehl DIVA 2.02
 *---------------------------------------------------------------------------*/
static void 
diva_ipac_write_fifo(struct l1_softc *sc,int what,void *buf,size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch ( what )
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,IPAC_ISAC_OFF);
			bus_space_write_multi_1(t,h,DIVA_IPAC_OFF_RW,buf,size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,IPAC_HSCXA_OFF);
			bus_space_write_multi_1(t,h,DIVA_IPAC_OFF_RW,buf,size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,IPAC_HSCXB_OFF);
			bus_space_write_multi_1(t,h,DIVA_IPAC_OFF_RW,buf,size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	Eicon Diehl DIVA 2.02
 *---------------------------------------------------------------------------*/
static void
diva_ipac_write_reg(struct l1_softc *sc,int what,bus_size_t reg,u_int8_t data)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch ( what )
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,reg+IPAC_ISAC_OFF);
			bus_space_write_1(t,h,DIVA_IPAC_OFF_RW,data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,reg+IPAC_HSCXA_OFF);
			bus_space_write_1(t,h,DIVA_IPAC_OFF_RW,data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,reg+IPAC_HSCXB_OFF);
			bus_space_write_1(t,h,DIVA_IPAC_OFF_RW,data);
			break;
		case ISIC_WHAT_IPAC:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,reg+IPAC_IPAC_OFF);
			bus_space_write_1(t,h,DIVA_IPAC_OFF_RW,data);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	Eicon Diehl DIVA 2.02
 *---------------------------------------------------------------------------*/
static u_int8_t
diva_ipac_read_reg(struct l1_softc *sc,int what,bus_size_t reg)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch ( what )
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,reg+IPAC_ISAC_OFF);
			return bus_space_read_1(t,h,DIVA_IPAC_OFF_RW);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,reg+IPAC_HSCXA_OFF);
			return bus_space_read_1(t,h,DIVA_IPAC_OFF_RW);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,reg+IPAC_HSCXB_OFF);
			return bus_space_read_1(t,h,DIVA_IPAC_OFF_RW);
		case ISIC_WHAT_IPAC:
			bus_space_write_1(t,h,DIVA_IPAC_OFF_ALE,reg+IPAC_IPAC_OFF);
			return bus_space_read_1(t,h,DIVA_IPAC_OFF_RW);
		default:
			return 0;
	}
}

/*---------------------------------------------------------------------------*
 *	Eicon Diehl DIVA 2.02
 *---------------------------------------------------------------------------*/
int
isic_attach_diva_ipac(device_t dev)
{
	int unit = device_get_unit(dev);
	struct l1_softc *sc = &l1_sc[unit];	
	
	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = diva_ipac_read_reg;
	sc->writereg = diva_ipac_write_reg;

	sc->readfifo = diva_ipac_read_fifo;
	sc->writefifo = diva_ipac_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_DIVA_ISA;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* setup chip type = IPAC */
	
	sc->sc_ipac = 1;
	sc->sc_bfifolen = IPAC_BFIFO_LEN;

	/* enable hscx/isac irq's */

	IPAC_WRITE(IPAC_MASK, (IPAC_MASK_INT1 | IPAC_MASK_INT0));

	IPAC_WRITE(IPAC_ACFG, 0);	/* outputs are open drain */

	IPAC_WRITE(IPAC_AOE,		/* aux 5..2 are inputs, 7, 6 outputs */
		(IPAC_AOE_OE5 | IPAC_AOE_OE4 | IPAC_AOE_OE3 | IPAC_AOE_OE2));

	IPAC_WRITE(IPAC_ATX, 0xff);	/* set all output lines high */

	return(0);
}

/*---------------------------------------------------------------------------*
 *	Eicon Diehl DIVA 2.0
 *---------------------------------------------------------------------------*/
static void 
diva_read_fifo(struct l1_softc *sc,int what,void *buf,size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch(what)
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,DIVA_ISAC_OFF_ALE,0);
			bus_space_read_multi_1(t,h,DIVA_ISAC_OFF_RW,buf,size);
			break;

		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,DIVA_HSCX_OFF_ALE,DIVA_HSCXA);
			bus_space_read_multi_1(t,h,DIVA_HSCX_OFF_RW,buf,size);
			break;

		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,DIVA_HSCX_OFF_ALE,DIVA_HSCXB);
			bus_space_read_multi_1(t,h,DIVA_HSCX_OFF_RW,buf,size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	Eicon Diehl DIVA 2.0
 *---------------------------------------------------------------------------*/
static void 
diva_write_fifo(struct l1_softc *sc,int what,void *buf,size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch(what)
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,DIVA_ISAC_OFF_ALE,0);
			bus_space_write_multi_1(t,h,DIVA_ISAC_OFF_RW,buf,size);
			break;

		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,DIVA_HSCX_OFF_ALE,DIVA_HSCXA);
			bus_space_write_multi_1(t,h,DIVA_HSCX_OFF_RW,buf,size);
			break;

		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,DIVA_HSCX_OFF_ALE,DIVA_HSCXB);
			bus_space_write_multi_1(t,h,DIVA_HSCX_OFF_RW,buf,size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	Eicon Diehl DIVA 2.0
 *---------------------------------------------------------------------------*/
static void
diva_write_reg(struct l1_softc *sc,int what,bus_size_t reg,u_int8_t data)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch(what)
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,DIVA_ISAC_OFF_ALE,reg);
			bus_space_write_1(t,h,DIVA_ISAC_OFF_RW,data);
			break;

		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,DIVA_HSCX_OFF_ALE,reg+DIVA_HSCXA);
			bus_space_write_1(t,h,DIVA_HSCX_OFF_RW,data);
			break;

		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,DIVA_HSCX_OFF_ALE,reg+DIVA_HSCXB);
			bus_space_write_1(t,h,DIVA_HSCX_OFF_RW,data);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	Eicon Diehl DIVA 2.0
 *---------------------------------------------------------------------------*/
static u_int8_t
diva_read_reg(struct l1_softc *sc,int what,bus_size_t reg)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch(what)
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,DIVA_ISAC_OFF_ALE,reg);
			return bus_space_read_1(t,h,DIVA_ISAC_OFF_RW);

		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,DIVA_HSCX_OFF_ALE,reg+DIVA_HSCXA);
			return bus_space_read_1(t,h,DIVA_HSCX_OFF_RW);

		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,DIVA_HSCX_OFF_ALE,reg+DIVA_HSCXB);
			return bus_space_read_1(t,h,DIVA_HSCX_OFF_RW);

		default:
			return 0;
	}
}

/*---------------------------------------------------------------------------*
 *	Eicon Diehl DIVA 2.0
 *---------------------------------------------------------------------------*/
int
isic_attach_diva(device_t dev)
{
	int unit = device_get_unit(dev);
	struct l1_softc *sc = &l1_sc[unit];	
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);
	
	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = diva_read_reg;
	sc->writereg = diva_write_reg;

	sc->readfifo = diva_read_fifo;
	sc->writefifo = diva_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_DIVA_ISA;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* setup chip type = ISAC/HSCX */

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* Read HSCX A/B VSTR.  Expected value is 0x05 (V2.1). */

	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) || 
	    ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for Eicon DIVA 2.0\n",
			sc->sc_unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			sc->sc_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			sc->sc_unit, HSCX_READ(1, H_VSTR));
		return ENXIO;
	}
	/* reset on */
	bus_space_write_1(t,h,DIVA_CTRL_OFF,0);
	DELAY(100);
	/* reset off */
	bus_space_write_1(t,h,DIVA_CTRL_OFF,DIVA_CTRL_WRRST);
	return(0);
}

#endif /* defined EICON_DIVA */
