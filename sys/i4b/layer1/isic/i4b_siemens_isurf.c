/*
 *   Copyright (c) 1999, 2000 Udo Schweigert. All rights reserved.
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
 *	Siemens I-Surf 2.0 PnP specific routines for isic driver
 *	--------------------------------------------------------
 *
 *	Based on ELSA Quickstep 1000pro PCI driver (i4b_elsa_qs1p.c)
 *	In case of trouble please contact Udo Schweigert <ust@cert.siemens.de>
 
 * $FreeBSD$
 *
 *      last edit-date: [Wed Jan 24 09:13:25 2001]
 *
 *---------------------------------------------------------------------------*/

#include "opt_i4b.h"

#if defined(SIEMENS_ISURF2)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/if.h>

#include <machine/i4b_ioctl.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_ipac.h>

/* masks for register encoded in base addr */

#define SIE_ISURF_BASE_MASK		0x0ffff
#define SIE_ISURF_OFF_MASK		0xf0000

/* register id's to be encoded in base addr */

#define SIE_ISURF_IDISAC		0x00000
#define SIE_ISURF_IDHSCXA		0x10000
#define SIE_ISURF_IDHSCXB		0x20000
#define SIE_ISURF_IDIPAC		0x40000

/* offsets from base address */

#define SIE_ISURF_OFF_ALE		0x00
#define SIE_ISURF_OFF_RW		0x01

/*---------------------------------------------------------------------------*
 *      Siemens I-Surf 2.0 PnP ISAC get fifo routine
 *---------------------------------------------------------------------------*/
static void 
siemens_isurf_read_fifo(struct l1_softc *sc,int what,void *buf,size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch ( what )
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,IPAC_ISAC_OFF);
			bus_space_read_multi_1(t,h,SIE_ISURF_OFF_RW,buf,size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,IPAC_HSCXA_OFF);
			bus_space_read_multi_1(t,h,SIE_ISURF_OFF_RW,buf,size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,IPAC_HSCXB_OFF);
			bus_space_read_multi_1(t,h,SIE_ISURF_OFF_RW,buf,size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *      Siemens I-Surf 2.0 PnP ISAC put fifo routine
 *---------------------------------------------------------------------------*/
static void 
siemens_isurf_write_fifo(struct l1_softc *sc,int what,void *buf,size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch ( what )
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,IPAC_ISAC_OFF);
			bus_space_write_multi_1(t,h,SIE_ISURF_OFF_RW,buf,size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,IPAC_HSCXA_OFF);
			bus_space_write_multi_1(t,h,SIE_ISURF_OFF_RW,buf,size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,IPAC_HSCXB_OFF);
			bus_space_write_multi_1(t,h,SIE_ISURF_OFF_RW,buf,size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *      Siemens I-Surf 2.0 PnP ISAC put register routine
 *---------------------------------------------------------------------------*/
static void
siemens_isurf_write_reg(struct l1_softc *sc,int what,bus_size_t reg,u_int8_t data)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch ( what )
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,reg+IPAC_ISAC_OFF);
			bus_space_write_1(t,h,SIE_ISURF_OFF_RW,data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,reg+IPAC_HSCXA_OFF);
			bus_space_write_1(t,h,SIE_ISURF_OFF_RW,data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,reg+IPAC_HSCXB_OFF);
			bus_space_write_1(t,h,SIE_ISURF_OFF_RW,data);
			break;
		case ISIC_WHAT_IPAC:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,reg+IPAC_IPAC_OFF);
			bus_space_write_1(t,h,SIE_ISURF_OFF_RW,data);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	Siemens I-Surf 2.0 PnP ISAC get register routine
 *---------------------------------------------------------------------------*/
static u_int8_t
siemens_isurf_read_reg(struct l1_softc *sc,int what,bus_size_t reg)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch ( what )
	{
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,reg+IPAC_ISAC_OFF);
			return bus_space_read_1(t,h,SIE_ISURF_OFF_RW);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,reg+IPAC_HSCXA_OFF);
			return bus_space_read_1(t,h,SIE_ISURF_OFF_RW);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,reg+IPAC_HSCXB_OFF);
			return bus_space_read_1(t,h,SIE_ISURF_OFF_RW);
		case ISIC_WHAT_IPAC:
			bus_space_write_1(t,h,SIE_ISURF_OFF_ALE,reg+IPAC_IPAC_OFF);
			return bus_space_read_1(t,h,SIE_ISURF_OFF_RW);
		default:
			return 0;
	}
}

/*---------------------------------------------------------------------------*
 *	isic_attach_siemens_isurf - attach for Siemens I-Surf 2.0 PnP
 *---------------------------------------------------------------------------*/
int
isic_attach_siemens_isurf(device_t dev)
{
	int unit = device_get_unit(dev);
	struct l1_softc *sc = &l1_sc[unit];	
	
	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = siemens_isurf_read_reg;
	sc->writereg = siemens_isurf_write_reg;

	sc->readfifo = siemens_isurf_read_fifo;
	sc->writefifo = siemens_isurf_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_SIE_ISURF2;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* setup chip type = IPAC ! */
	
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
#endif /* defined(SIEMENS_ISURF2) */
