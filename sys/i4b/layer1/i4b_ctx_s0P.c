/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *	isic - I4B Siemens ISDN Chipset Driver for Creatix PnP cards
 *	============================================================
 *
 * $FreeBSD$ 
 *
 *      last edit-date: [Sun Feb 14 10:25:33 1999]
 *
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__

#include "isic.h"
#include "opt_i4b.h"
#include "pnp.h"

#else

#define	NISIC	1
#define NPNP	1

#endif

#if (NISIC > 0) && (NPNP > 0) && defined(CRTX_S0_P)

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

#include <i4b/include/i4b_global.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_mbuf.h>

#ifndef __FreeBSD__
static u_int8_t ctxs0P_read_reg __P((struct isic_softc *sc, int what, bus_size_t offs));
static void ctxs0P_write_reg __P((struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data));
static void ctxs0P_read_fifo __P((struct isic_softc *sc, int what, void *buf, size_t size));
static void ctxs0P_write_fifo __P((struct isic_softc *sc, int what, const void *data, size_t size));
void isic_attach_Cs0P(struct isic_softc *sc);
#endif

#ifdef __FreeBSD__
#include <i386/isa/pnp.h>
extern void isicintr ( int unit );
#endif

/*---------------------------------------------------------------------------*
 *      Creatix ISDN-S0 P&P ISAC get fifo routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void             
ctxs0P_read_fifo(void *buf, const void *base, size_t len)
{
        insb((int)base + 0x3e, (u_char *)buf, (u_int)len);
}

#else

static void
ctxs0P_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
        bus_space_tag_t t = sc->sc_maps[what+1].t;
        bus_space_handle_t h = sc->sc_maps[what+1].h;
        bus_size_t o = sc->sc_maps[what+1].offset;
        bus_space_read_multi_1(t, h, o + 0x3e, buf, size);
}

#endif

/*---------------------------------------------------------------------------*
 *      Creatix ISDN-S0 P&P ISAC put fifo routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
ctxs0P_write_fifo(void *base, const void *buf, size_t len)
{
        outsb((int)base + 0x3e, (u_char *)buf, (u_int)len);
}

#else

static void
ctxs0P_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
        bus_space_tag_t t = sc->sc_maps[what+1].t;
        bus_space_handle_t h = sc->sc_maps[what+1].h;
        bus_size_t o = sc->sc_maps[what+1].offset;
        bus_space_write_multi_1(t, h, o + 0x3e, (u_int8_t*)buf, size);
}
#endif

/*---------------------------------------------------------------------------*
 *      Creatix ISDN-S0 P&P ISAC put register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
ctxs0P_write_reg(u_char *base, u_int offset, u_int v)
{
        outb((int)base + offset, (u_char)v);
}

#else

static void
ctxs0P_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[what+1].t;
	bus_space_handle_t h = sc->sc_maps[what+1].h;
	bus_size_t o = sc->sc_maps[what+1].offset;
	bus_space_write_1(t, h, o + offs, data);
}
#endif

/*---------------------------------------------------------------------------*
 *	Creatix ISDN-S0 P&P ISAC get register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static u_char
ctxs0P_read_reg(u_char *base, u_int offset)
{
	return (inb((int)base + offset));
}

#else

static u_int8_t
ctxs0P_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[what+1].t;
	bus_space_handle_t h = sc->sc_maps[what+1].h;
	bus_size_t o = sc->sc_maps[what+1].offset;
	return bus_space_read_1(t, h, o + offs);
}

#endif

#ifdef __FreeBSD__

/*---------------------------------------------------------------------------*
 *	isic_probe_Cs0P - probe for Creatix ISDN-S0 P&P and compatibles
 *---------------------------------------------------------------------------*/
int
isic_probe_Cs0P(struct isa_device *dev, unsigned int iobase2)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];
	
	/* check max unit range */
	
	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for Creatix ISDN-S0 P&P!\n",
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
			printf("isic%d: Error, invalid IRQ [%d] specified for Creatix ISDN-S0 P&P!\n",
				dev->id_unit, ffs(dev->id_irq)-1);
			return(0);
			break;
	}
	sc->sc_irq = dev->id_irq;

	/* check if memory addr specified */

	if(dev->id_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for Creatix ISDN-S0 P&P!\n",
			dev->id_unit, (u_long)dev->id_maddr);
		return(0);
	}
	dev->id_msize = 0;
	
	if(iobase2 == 0)
	{
		printf("isic%d: Error, iobase2 is 0 for Creatix ISDN-S0 P&P!\n",
			dev->id_unit);
		return(0);
	}

	/* check if we got an iobase */

	switch(dev->id_iobase)
	{
		case 0x120:
		case 0x180:
/*XXX*/			break;
			
		default:
			printf("isic%d: Error, invalid iobase 0x%x specified for Creatix ISDN-S0 P&P!\n",
				dev->id_unit, dev->id_iobase);
			return(0);
			break;
	}
	sc->sc_port = dev->id_iobase;

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
	
	/* setup ISAC and HSCX base addr */
	
	ISAC_BASE   = (caddr_t) dev->id_iobase - 0x20;
	HSCX_A_BASE = (caddr_t) iobase2 - 0x20;
	HSCX_B_BASE = (caddr_t) iobase2;

	/* 
	 * Read HSCX A/B VSTR.  Expected value for the Creatix PnP card is
	 * 0x05 ( = version 2.1 ) in the least significant bits.
	 */

	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) ||
            ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for Creatix PnP\n",
			dev->id_unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(1, H_VSTR));
		return (0);
	}                   

	return (1);
}

/*---------------------------------------------------------------------------*
 *	isic_attach_s0163P - attach Creatix ISDN-S0 P&P
 *---------------------------------------------------------------------------*/
int
isic_attach_Cs0P(struct isa_device *dev, unsigned int iobase2)
{
	outb((dev->id_iobase) + 0x1c, 0);
	DELAY(SEC_DELAY / 10);
	outb((dev->id_iobase) + 0x1c, 1);
	DELAY(SEC_DELAY / 10);
	return(1);
}

#else /* !__FreeBSD__ */

void
isic_attach_Cs0P(struct isic_softc *sc)
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
}
#endif

#endif /* (NISIC > 0) && (NPNP > 0) && defined(CRTX_S0_P) */

