/*
 *   Copyright (c) 1998 German Tischler. All rights reserved.
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
 * Card format:
 * 
 * iobase + 0 : reset on  (0x03)
 * iobase + 1 : reset off (0x0)
 * iobase + 2 : isac read/write
 * iobase + 3 : hscx read/write ( offset 0-0x3f    hscx0 , 
 *                                offset 0x40-0x7f hscx1 )
 * iobase + 4 : offset for indirect adressing
 *
 *---------------------------------------------------------------------------
 *
 *	isic - I4B Siemens ISDN Chipset Driver for SWS cards
 *	====================================================
 *
 *	$Id: i4b_sws.c,v 1.2 1999/12/13 21:25:26 hm Exp $
 *
 * $FreeBSD: src/sys/i4b/layer1/i4b_sws.c,v 1.6 1999/12/14 20:48:23 hm Exp $
 *
 *	last edit-date: [Mon Dec 13 22:02:39 1999]
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"
#include "opt_i4b.h"

#if defined (SEDLBAUER) && NISIC > 0

#define SWS_RESON  0 /* reset on                 */
#define SWS_RESOFF 1 /* reset off                */
#define SWS_ISAC   2 /* ISAC                     */
#define SWS_HSCX0  3 /* HSCX0                    */
#define SWS_RW     4 /* indirect access register */
#define SWS_HSCX1  5 /* this is for fakeing that we mean hscx1, though */
                     /* access is done through hscx0                   */

#define SWS_REGS   8 /* we use an area of 8 bytes for io */

#include <sys/param.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <machine/clock.h>

#include <net/if.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_mbuf.h>

/*---------------------------------------------------------------------------*
 *      SWS P&P ISAC get fifo routine
 *---------------------------------------------------------------------------*/
static void
sws_read_fifo(struct l1_softc *sc,int what,void *buf,size_t size) {
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch ( what ) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,SWS_RW,0x0);
			bus_space_read_multi_1(t,h,SWS_ISAC,buf,size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,SWS_RW,0x0);
			bus_space_read_multi_1(t,h,SWS_HSCX0,buf,size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,SWS_RW,0x0+0x40);
			bus_space_read_multi_1(t,h,SWS_HSCX0,buf,size);
			break;
	}
}

static void
sws_write_fifo(struct l1_softc *sc,int what,void *buf,size_t size) {
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch ( what ) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,SWS_RW,0x0);
			bus_space_write_multi_1(t,h,SWS_ISAC,buf,size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,SWS_RW,0x0);
			bus_space_write_multi_1(t,h,SWS_HSCX0,buf,size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,SWS_RW,0x0+0x40);
			bus_space_write_multi_1(t,h,SWS_HSCX0,buf,size);
			break;
	}
}

static void
sws_write_reg(struct l1_softc *sc,int what,bus_size_t reg,u_int8_t data) {
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch ( what ) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,SWS_RW,reg);
			bus_space_write_1(t,h,SWS_ISAC,data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,SWS_RW,reg);
			bus_space_write_1(t,h,SWS_HSCX0,data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,SWS_RW,reg+0x40);
			bus_space_write_1(t,h,SWS_HSCX0,data);
			break;
	}
}

static u_char
sws_read_reg (struct l1_softc *sc,int what,bus_size_t reg) {
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[0]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[0]);

	switch ( what ) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t,h,SWS_RW,reg);
			return bus_space_read_1(t,h,SWS_ISAC);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t,h,SWS_RW,reg);
			return bus_space_read_1(t,h,SWS_HSCX0);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t,h,SWS_RW,reg+0x40);
			return bus_space_read_1(t,h,SWS_HSCX0);
		default:
			return 0;
	}
}

/* attach callback routine */
int
isic_attach_sws(device_t dev)
{
	int unit = device_get_unit(dev);
	struct l1_softc *sc = &l1_sc[unit];	
	
	struct i4b_info * info = &(sc->sc_resources);
        bus_space_tag_t    t = rman_get_bustag(info->io_base[0]);
        bus_space_handle_t h = rman_get_bushandle(info->io_base[0]);

	/* fill in l1_softc structure */
	sc->readreg     = sws_read_reg;
	sc->writereg    = sws_write_reg;
	sc->readfifo    = sws_read_fifo;
	sc->writefifo   = sws_write_fifo;
	sc->clearirq    = NULL;
	sc->sc_cardtyp  = CARD_TYPEP_SWS;
	sc->sc_bustyp   = BUS_TYPE_IOM2;
	sc->sc_ipac     = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* 
	 * Read HSCX A/B VSTR.  Expected value for the SWS PnP card is
	 * 0x05 ( = version 2.1 ) in the least significant bits.
	 */

	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) ||
            ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for SWS PnP\n",
			sc->sc_unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			sc->sc_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			sc->sc_unit, HSCX_READ(1, H_VSTR));
		return (ENXIO);
	}                   

	/* reset card */
	bus_space_write_1(t,h,SWS_RESON,0x3);
	DELAY(SEC_DELAY / 5);
	bus_space_write_1(t,h,SWS_RESOFF,0x0);
	DELAY(SEC_DELAY / 5);

	return(0);
}
#endif /* defined(SEDLBAUER) && NISIC > 0 */
