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
 *		EXPERIMENTAL !!!!
 *		=================
 *
 * $FreeBSD$
 *
 *	last edit-date: [Sun Feb 14 10:28:31 1999]
 *
 *	-hm	adding driver to i4b
 *	-hm	adjustments for FreeBSD < 2.2.6, no PnP support yet
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)

#include "isic.h"
#include "opt_i4b.h"

#else

#define	NISIC	1

#endif
  
#if defined (SEDLBAUER) && NISIC > 0

#define SWS_RESON  0 /* reset on                 */
#define SWS_RESOFF 1 /* reset off                */
#define SWS_ISAC   2 /* ISAC                     */
#define SWS_HSCX0  3 /* HSCX0                    */
#define SWS_RW     4 /* indirect access register */
#define SWS_HSCX1  5 /* this is for fakeing that we mean hscx1, though */
                     /* access is done through hscx0                   */

#define SWS_REGS   8 /* we use an area of 8 bytes for io */

#define SWS_BASE(X)   ((unsigned int)X&~(SWS_REGS-1))
#define SWS_PART(X)   ((unsigned int)X& (SWS_REGS-1))
#define SWS_ADDR(X)   ((SWS_PART(X) == SWS_ISAC) ? (SWS_BASE(X)+SWS_ISAC) : (SWS_BASE(X)+SWS_HSCX0) )
#define SWS_REG(X,Y)  ((SWS_PART(X) != SWS_HSCX1) ? Y : (Y+0x40) )
#define SWS_IDO(X)    (SWS_BASE(X)+SWS_RW)

#include <sys/param.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#ifdef __FreeBSD__
#include <machine/clock.h>
#include <i386/isa/isa_device.h>
#else
#include <machine/bus.h>
#include <sys/device.h>
#endif

#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_mbuf.h>

#ifndef __FreeBSD__
static u_int8_t sws_read_reg   __P((struct isic_softc *sc, int what, bus_size_t offs));
static void     sws_write_reg  __P((struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data));
static void     sws_read_fifo  __P((struct isic_softc *sc, int what, void *buf, size_t size));
static void     sws_write_fifo __P((struct isic_softc *sc, int what, const void *data, size_t size));
void		isic_attach_sws __P((struct isic_softc *sc));
#endif

/*---------------------------------------------------------------------------*
 *      SWS P&P ISAC get fifo routine
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__

static void
sws_read_fifo(void *buf, const void *base, size_t len)
{
 outb(SWS_IDO(base),SWS_REG(base,0));
 insb(SWS_ADDR(base),buf,len);
}

#else

static void
sws_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, SWS_RW, 0);
			bus_space_read_multi_1(t, h, SWS_ISAC, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, SWS_RW, 0);
			bus_space_read_multi_1(t, h, SWS_HSCX0, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, SWS_RW, 0x40);
			bus_space_read_multi_1(t, h, SWS_HSCX0, buf, size);
			break;
	}
}

#endif

/*---------------------------------------------------------------------------*
 *      SWS P&P ISAC put fifo routine
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__

static void
sws_write_fifo(void *base, const void *buf, size_t len)
{
 outb (SWS_IDO(base),SWS_REG(base,0));
 outsb(SWS_ADDR(base),buf,len);
}

#else

static void
sws_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, SWS_RW, 0);
			bus_space_write_multi_1(t, h, SWS_ISAC, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, SWS_RW, 0);
			bus_space_write_multi_1(t, h, SWS_HSCX0, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, SWS_RW, 0x40);
			bus_space_write_multi_1(t, h, SWS_HSCX0, (u_int8_t*)buf, size);
			break;
	}
}

#endif

/*---------------------------------------------------------------------------*
 *      SWS P&P ISAC put register routine
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__

static void
sws_write_reg(u_char *base, u_int offset, u_int v)
{
 outb(SWS_IDO(base),SWS_REG(base,offset));
 outb(SWS_ADDR(base),v);
}

#else

static void
sws_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, SWS_RW, offs);
			bus_space_write_1(t, h, SWS_ISAC, data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, SWS_RW, offs);
			bus_space_write_1(t, h, SWS_HSCX0, data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, SWS_RW, 0x40+offs);
			bus_space_write_1(t, h, SWS_HSCX0, data);
			break;
	}
}

#endif

/*---------------------------------------------------------------------------*
 *	SWS P&P ISAC get register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static u_char
sws_read_reg(u_char *base, u_int offset)
{
 outb(SWS_IDO(base),SWS_REG(base,offset));
 return inb(SWS_ADDR(base));
}

#else

static u_int8_t
sws_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, SWS_RW, offs);
			return bus_space_read_1(t, h, SWS_ISAC);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, SWS_RW, offs);
			return bus_space_read_1(t, h, SWS_HSCX0);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, SWS_RW, 0x40+offs);
			return bus_space_read_1(t, h, SWS_HSCX0);
	}
	return 0;
}

#endif

#ifdef __FreeBSD__

/* attach callback routine */

int
isic_attach_sws(struct isa_device *dev)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];

	/* fill in isic_softc structure */

	sc->readreg     = sws_read_reg;
	sc->writereg    = sws_write_reg;
	sc->readfifo    = sws_read_fifo;
	sc->writefifo   = sws_write_fifo;
	sc->clearirq    = NULL;
	sc->sc_unit     = dev->id_unit;
	sc->sc_irq      = dev->id_irq;
	sc->sc_port     = dev->id_iobase;
	sc->sc_cardtyp  = CARD_TYPEP_SWS;
	sc->sc_bustyp   = BUS_TYPE_IOM2;
	sc->sc_ipac     = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;
	dev->id_msize   = 0;

	ISAC_BASE   = (caddr_t) (((u_int) sc->sc_port) + SWS_ISAC);
	HSCX_A_BASE = (caddr_t) (((u_int) sc->sc_port) + SWS_HSCX0);
	HSCX_B_BASE = (caddr_t) (((u_int) sc->sc_port) + SWS_HSCX1);

	/* 
	 * Read HSCX A/B VSTR.  Expected value for the SWS PnP card is
	 * 0x05 ( = version 2.1 ) in the least significant bits.
	 */

	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) ||
            ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for SWS PnP\n",
			dev->id_unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(1, H_VSTR));
		return (0);
	}                   

	/* reset card */

	outb( ((u_int) sc->sc_port) + SWS_RESON , 0x3);
	DELAY(SEC_DELAY / 5);
	outb( ((u_int) sc->sc_port) + SWS_RESOFF, 0);
	DELAY(SEC_DELAY / 5);
		
	return(1);
}

#else /* !__FreeBSD__ */

void
isic_attach_sws(struct isic_softc *sc)
{
	/* setup access routines */

	sc->readreg   = sws_read_reg;
	sc->writereg  = sws_write_reg;

	sc->readfifo  = sws_read_fifo;
	sc->writefifo = sws_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_SWS;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* 
	 * Read HSCX A/B VSTR.  Expected value for the SWS PnP card is
	 * 0x05 ( = version 2.1 ) in the least significant bits.
	 */

	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) ||
            ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("%s: HSCX VSTR test failed for SWS PnP\n",
			sc->sc_dev.dv_xname);
		printf("%s: HSC0: VSTR: %#x\n",
			sc->sc_dev.dv_xname, HSCX_READ(0, H_VSTR));
		printf("%s: HSC1: VSTR: %#x\n",
			sc->sc_dev.dv_xname, HSCX_READ(1, H_VSTR));
		return;
	}                   

	/* reset card */
        {
        	bus_space_tag_t t = sc->sc_maps[0].t;
        	bus_space_handle_t h = sc->sc_maps[0].h;
        	bus_space_write_1(t, h, SWS_RESON, 0x3);
		DELAY(SEC_DELAY / 5);
		bus_space_write_1(t, h, SWS_RESOFF, 0);
		DELAY(SEC_DELAY / 5);
	}
}

#endif /* !__FreeBSD__ */

#endif /* defined(SEDLBAUER) && NISIC > 0 */
