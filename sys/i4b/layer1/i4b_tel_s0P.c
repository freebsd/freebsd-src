/*
 *   Copyright (c) 1997 Andrew Gordon. All rights reserved.
 *
 *   Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *	isic - I4B Siemens ISDN Chipset Driver for Teles S0 PnP
 *	=======================================================
 *
 *		EXPERIMENTAL !!!
 *		================
 *
 *	$Id: i4b_tel_s0P.c,v 1.14 1999/03/16 11:12:31 hm Exp $ 
 *
 *      last edit-date: [Tue Mar 16 10:39:14 1999]
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)
#include "isic.h"
#include "opt_i4b.h"
#else
#define	NISIC 1
#endif
#if NISIC > 0 && defined(TEL_S0_16_3_P)

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
static u_int8_t tels0163P_read_reg __P((struct isic_softc *sc, int what, bus_size_t offs));
static void tels0163P_write_reg __P((struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data));
static void tels0163P_read_fifo __P((struct isic_softc *sc, int what, void *buf, size_t size));
static void tels0163P_write_fifo __P((struct isic_softc *sc, int what, const void *data, size_t size));
void isic_attach_s0163P __P((struct isic_softc *sc));
#endif


/*---------------------------------------------------------------------------*
 *      Teles S0/16.3 PnP read fifo routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void             
tels0163P_read_fifo(void *buf, const void *base, size_t len)
{
        insb((int)base + 0x3e, (u_char *)buf, (u_int)len);
}

#else

static void
tels0163P_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
        bus_space_tag_t t = sc->sc_maps[what+1].t;
        bus_space_handle_t h = sc->sc_maps[what+1].h;
        bus_size_t o = sc->sc_maps[what+1].offset;
        bus_space_read_multi_1(t, h, o + 0x3e, buf, size);
}

#endif

/*---------------------------------------------------------------------------*
 *      Teles S0/16.3 PnP write fifo routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
tels0163P_write_fifo(void *base, const void *buf, size_t len)
{
        outsb((int)base + 0x3e, (u_char *)buf, (u_int)len);
}

#else

static void
tels0163P_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
        bus_space_tag_t t = sc->sc_maps[what+1].t;
        bus_space_handle_t h = sc->sc_maps[what+1].h;
        bus_size_t o = sc->sc_maps[what+1].offset;
        bus_space_write_multi_1(t, h, o + 0x3e, (u_int8_t*)buf, size);
}
#endif

/*---------------------------------------------------------------------------*
 *      Teles S0/16.3 PnP write register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
tels0163P_write_reg(u_char *base, u_int offset, u_int v)
{
        outb((int)base + offset, (u_char)v);
}

#else

static void
tels0163P_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[what+1].t;
	bus_space_handle_t h = sc->sc_maps[what+1].h;
	bus_size_t o = sc->sc_maps[what+1].offset;
	bus_space_write_1(t, h, o + offs, data);
}
#endif

/*---------------------------------------------------------------------------*
 *	Teles S0/16.3 PnP read register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static u_char
tels0163P_read_reg(u_char *base, u_int offset)
{
	return (inb((int)base + offset));
}

#else

static u_int8_t
tels0163P_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[what+1].t;
	bus_space_handle_t h = sc->sc_maps[what+1].h;
	bus_size_t o = sc->sc_maps[what+1].offset;
	return bus_space_read_1(t, h, o + offs);
}
#endif

/*---------------------------------------------------------------------------*
 *	isic_probe_s0163P - probe for Teles S0/16.3 PnP and compatibles
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
int
isic_probe_s0163P(struct isa_device *dev, unsigned int iobase2)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];
	
	/* check max unit range */
	
	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for Teles S0/16.3 PnP!\n",
				dev->id_unit, dev->id_unit);
		return(0);	
	}	
	sc->sc_unit = dev->id_unit;

	/* check IRQ validity */

	switch(ffs(dev->id_irq) - 1)
	{
		case 3:
		case 5:
		case 7:
		case 10:
		case 11:
		case 12:
			break;

		default:					
			printf("isic%d: Error, invalid IRQ [%d] specified for Teles S0/16.3 PnP!\n",
				dev->id_unit, ffs(dev->id_irq)-1);
			return(0);
			break;
	}
	sc->sc_irq = dev->id_irq;

	/* check if memory addr specified */

	if(dev->id_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for Teles S0/16.3 PnP!\n",
			dev->id_unit, (u_long)dev->id_maddr);
		return(0);
	}
	dev->id_msize = 0;
	
	/* check if we got an iobase */

	switch(dev->id_iobase)
	{
		case 0x580:
		case 0x500:
		case 0x680:
			break;
			
		default:
			printf("isic%d: Error, invalid iobase 0x%x specified for Teles S0/16.3 PnP!\n",
				dev->id_unit, dev->id_iobase);
			return(0);
			break;
	}
	sc->sc_port = dev->id_iobase;

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = tels0163P_read_reg;
	sc->writereg = tels0163P_write_reg;

	sc->readfifo = tels0163P_read_fifo;
	sc->writefifo = tels0163P_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_163P;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* setup ISAC and HSCX base addr */
	
	switch(dev->id_iobase)
	{
		case 0x580:
		        ISAC_BASE = (caddr_t) 0x580 - 0x20;
			HSCX_A_BASE = (caddr_t) 0x180 - 0x20;
			HSCX_B_BASE = (caddr_t) 0x180;
			break;

		case 0x500:
		        ISAC_BASE = (caddr_t) 0x500 - 0x20;
			HSCX_A_BASE = (caddr_t) 0x100 - 0x20;
			HSCX_B_BASE = (caddr_t) 0x100;
			break;

		case 0x680:
		        ISAC_BASE = (caddr_t) 0x680 - 0x20;
			HSCX_A_BASE = (caddr_t) 0x280 - 0x20;
			HSCX_B_BASE = (caddr_t) 0x280;
			break;
	}

	/* 
	 * Read HSCX A/B VSTR.  Expected value for the S0/16.3 PnP card is
	 * 0x05 in the least significant bits.
	 */

	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) ||
            ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for Teles S0/16.3 PnP\n",
			dev->id_unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(1, H_VSTR));
		return (0);
	}                   

	return (1);
}
#endif

/*---------------------------------------------------------------------------*
 *	isic_attach_s0163P - attach Teles S0/16.3 PnP and compatibles
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
int
isic_attach_s0163P(struct isa_device *dev, unsigned int iobase2)
{
	outb((dev->id_iobase) + 0x1c, 0);
	DELAY(SEC_DELAY / 10);
	outb((dev->id_iobase) + 0x1c, 1);
	DELAY(SEC_DELAY / 10);
	return(1);
}

#else

void
isic_attach_s0163P(struct isic_softc *sc)
{
	/* init card */
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	bus_space_write_1(t, h, 0x1c, 0);
	DELAY(SEC_DELAY / 10);
	bus_space_write_1(t, h, 0x1c, 1);
	DELAY(SEC_DELAY / 10);

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = tels0163P_read_reg;
	sc->writereg = tels0163P_write_reg;

	sc->readfifo = tels0163P_read_fifo;
	sc->writefifo = tels0163P_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_163P;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;	
}
#endif

#endif /* ISIC > 0 */

