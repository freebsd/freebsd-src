/*
 *   Copyright (c) 1996 Arne Helme. All rights reserved.
 *
 *   Copyright (c) 1996 Gary Jennejohn. All rights reserved. 
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
 *	isic - I4B Siemens ISDN Chipset Driver for Teles S0/16.3
 *	========================================================
 *
 * $FreeBSD$ 
 *
 *      last edit-date: [Mon Jul 26 10:59:38 1999]
 *
 *	-hm	clean up
 *	-hm	more cleanup
 *      -hm     NetBSD patches from Martin
 *	-hm	VSTR detection for older 16.3 cards
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)
#include "isic.h"
#include "opt_i4b.h"
#else
#define	NISIC 1
#endif
#if NISIC > 0 && defined(TEL_S0_16_3)

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
#elif defined(__bsdi__)
	/* XXX */
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

static u_char intr_no[] = { 1, 1, 0, 2, 4, 6, 1, 1, 1, 0, 8, 10, 12, 1, 1, 14 };

#if !defined(__FreeBSD__) && !defined(__bsdi__)
static u_int8_t tels0163_read_reg __P((struct isic_softc *sc, int what, bus_size_t offs));
static void tels0163_write_reg __P((struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data));
static void tels0163_read_fifo __P((struct isic_softc *sc, int what, void *buf, size_t size));
static void tels0163_write_fifo __P((struct isic_softc *sc, int what, const void *data, size_t size));
#endif

/*---------------------------------------------------------------------------*
 *      Teles S0/16.3 read fifo routine
 *---------------------------------------------------------------------------*/
#if defined(__FreeBSD__) || defined(__bsdi__)

static void             
tels0163_read_fifo(void *buf, const void *base, size_t len)
{
        insb((int)base + 0x3e, (u_char *)buf, (u_int)len);
}

#else

static void
tels0163_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
        bus_space_tag_t t = sc->sc_maps[what+1].t;
        bus_space_handle_t h = sc->sc_maps[what+1].h;
        bus_size_t o = sc->sc_maps[what+1].offset;
        bus_space_read_multi_1(t, h, o + 0x1e, buf, size);
}

#endif

/*---------------------------------------------------------------------------*
 *      Teles S0/16.3 write fifo routine
 *---------------------------------------------------------------------------*/
#if defined(__FreeBSD__) || defined(__bsdi__)

static void
tels0163_write_fifo(void *base, const void *buf, size_t len)
{
        outsb((int)base + 0x3e, (u_char *)buf, (u_int)len);
}

#else

static void
tels0163_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
        bus_space_tag_t t = sc->sc_maps[what+1].t;
        bus_space_handle_t h = sc->sc_maps[what+1].h;
        bus_size_t o = sc->sc_maps[what+1].offset;
        bus_space_write_multi_1(t, h, o + 0x1e, (u_int8_t*)buf, size);
}
#endif

/*---------------------------------------------------------------------------*
 *      Teles S0/16.3 ISAC put register routine
 *---------------------------------------------------------------------------*/
#if defined(__FreeBSD__) || defined(__bsdi__)

static void
tels0163_write_reg(u_char *base, u_int offset, u_int v)
{
        outb((int)base + offset, (u_char)v);
}

#else

static void
tels0163_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[what+1].t;
	bus_space_handle_t h = sc->sc_maps[what+1].h;
	bus_size_t o = sc->sc_maps[what+1].offset;
	bus_space_write_1(t, h, o + offs - 0x20, data);
}
#endif

/*---------------------------------------------------------------------------*
 *	Teles S0/16.3 ISAC get register routine
 *---------------------------------------------------------------------------*/
#if defined(__FreeBSD__) || defined(__bsdi__)

static u_char
tels0163_read_reg(u_char *base, u_int offset)
{
	return (inb((int)base + offset));
}

#else

static u_int8_t
tels0163_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[what+1].t;
	bus_space_handle_t h = sc->sc_maps[what+1].h;
	bus_size_t o = sc->sc_maps[what+1].offset;
	return bus_space_read_1(t, h, o + offs - 0x20);
}

#endif

/*---------------------------------------------------------------------------*
 *	isic_probe_s0163 - probe for Teles S0/16.3 and compatibles
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
int
isic_probe_s0163(struct isa_device *dev)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];
	u_char byte;
	
	/* check max unit range */
	
	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for Teles S0/16.3!\n",
				dev->id_unit, dev->id_unit);
		return(0);	
	}	
	sc->sc_unit = dev->id_unit;

	/* check IRQ validity */

	if((intr_no[ffs(dev->id_irq) - 1]) == 1)
	{
		printf("isic%d: Error, invalid IRQ [%d] specified for Teles S0/16.3!\n",
			dev->id_unit, ffs(dev->id_irq)-1);
		return(0);
	}
	sc->sc_irq = dev->id_irq;

	/* check if memory addr specified */

	if(dev->id_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for Teles S0/16.3!\n",
			dev->id_unit, (u_long)dev->id_maddr);
		return(0);
	}
		
	dev->id_msize = 0;
	
	/* check if we got an iobase */

	switch(dev->id_iobase)
	{
		case 0xd80:
		case 0xe80:
		case 0xf80:
			break;
			
		default:
			printf("isic%d: Error, invalid iobase 0x%x specified for Teles S0/16.3!\n",
				dev->id_unit, dev->id_iobase);
			return(0);
			break;
	}
	sc->sc_port = dev->id_iobase;
	
	if(((byte = inb(sc->sc_port)) != 0x51) && (byte != 0x10))
	{
		printf("isic%d: Error, signature 1 0x%x != 0x51 or 0x10 for Teles S0/16.3!\n",
			dev->id_unit, byte);
		return(0);
	}
	
	if((byte = inb(sc->sc_port + 1)) != 0x93)
	{
		printf("isic%d: Error, signature 2 0x%x != 0x93 for Teles S0/16.3!\n",
			dev->id_unit, byte);
		return(0);
	}

	if((byte = inb(sc->sc_port + 2)) != 0x1c)	
	{
		printf("isic%d: Error, signature 3 0x%x != 0x1c for Teles S0/16.3!\n",
			dev->id_unit, byte);
		return(0);
	}

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = tels0163_read_reg;
	sc->writereg = tels0163_write_reg;

	sc->readfifo = tels0163_read_fifo;
	sc->writefifo = tels0163_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp= CARD_TYPEP_16_3;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;	

	/* setup ISAC and HSCX base addr */
	
	switch(dev->id_iobase)
	{
		case 0xd80:
		        ISAC_BASE = (caddr_t) 0x960;
			HSCX_A_BASE = (caddr_t) 0x160;
			HSCX_B_BASE = (caddr_t) 0x560;
			break;
		
		case 0xe80:
	        	ISAC_BASE = (caddr_t) 0xa60;
			HSCX_A_BASE = (caddr_t) 0x260;
			HSCX_B_BASE = (caddr_t) 0x660;
			break;

		case 0xf80:
		        ISAC_BASE = (caddr_t) 0xb60;
			HSCX_A_BASE = (caddr_t) 0x360;
			HSCX_B_BASE = (caddr_t) 0x760;
			break;
	}

	/* 
	 * Read HSCX A/B VSTR.  Expected value for the S0/16.3 card is
	 * 0x05 or 0x04 (for older 16.3's) in the least significant bits.
	 */

	if( (((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) &&
	     ((HSCX_READ(0, H_VSTR) & 0xf) != 0x4))	||
            (((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) &&
	     ((HSCX_READ(1, H_VSTR) & 0xf) != 0x4)) )  
	{
		printf("isic%d: HSCX VSTR test failed for Teles S0/16.3\n",
			dev->id_unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(1, H_VSTR));
		return (0);
	}                   

	return (1);
}

#elif defined(__bsdi__)

static int
set_softc(struct isic_softc *sc, struct isa_attach_args *ia, int unit)
{
	sc->sc_irq = ia->ia_irq;

	/* check if we got an iobase */

	switch(ia->ia_iobase)
	{
		case 0xd80:
		case 0xe80:
		case 0xf80:
			break;
			
		default:
			printf("isic%d: Error, invalid iobase 0x%x specified for Teles S0/16.3!\n",
				unit, ia->ia_iobase);
			return(0);
			break;
	}
	sc->sc_port = ia->ia_iobase;
	
	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = tels0163_read_reg;
	sc->writereg = tels0163_write_reg;

	sc->readfifo = tels0163_read_fifo;
	sc->writefifo = tels0163_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp= CARD_TYPEP_16_3;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;	

	/* setup ISAC and HSCX base addr */
	
	switch(ia->ia_iobase)
	{
		case 0xd80:
		        ISAC_BASE = (caddr_t) 0x960;
			HSCX_A_BASE = (caddr_t) 0x160;
			HSCX_B_BASE = (caddr_t) 0x560;
			break;
		
		case 0xe80:
	        	ISAC_BASE = (caddr_t) 0xa60;
			HSCX_A_BASE = (caddr_t) 0x260;
			HSCX_B_BASE = (caddr_t) 0x660;
			break;

		case 0xf80:
		        ISAC_BASE = (caddr_t) 0xb60;
			HSCX_A_BASE = (caddr_t) 0x360;
			HSCX_B_BASE = (caddr_t) 0x760;
			break;
	}
	return 1;
}

int
isic_probe_s0163(struct device *dev, struct cfdata *cf,
		struct isa_attach_args *ia)
{
	u_char byte;
	struct isic_softc dummysc, *sc = &dummysc;
	
	if((intr_no[ffs(ia->ia_irq) - 1]) == 1)
	{
		printf("isic%d: Error, invalid IRQ [%d] specified for Teles S0/16.3!\n",
			cf->cf_unit, ffs(ia->ia_irq)-1);
		return(0);
	}

	/* check if memory addr specified */

	if(ia->ia_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for Teles S0/16.3!\n",
			cf->cf_unit, (u_long)ia->ia_maddr);
		return 0;
	}

	/* Set up a temporary softc for the probe */

	if (set_softc(sc, ia, cf->cf_unit) == 0)
		return 0;
	
	if((byte = inb(sc->sc_port)) != 0x51)
	{
		printf("isic%d: Error, signature 1 0x%x != 0x51 for Teles S0/16.3!\n",
			cf->cf_unit, byte);
		return(0);
	}
	
	if((byte = inb(sc->sc_port + 1)) != 0x93)
	{
		printf("isic%d: Error, signature 2 0x%x != 0x93 for Teles S0/16.3!\n",
			cf->cf_unit, byte);
		return(0);
	}

	if((byte = inb(sc->sc_port + 2)) != 0x1c)	
	{
		printf("isic%d: Error, signature 3 0x%x != 0x1c for Teles S0/16.3!\n",
			cf->cf_unit, byte);
		return(0);
	}

	/* 
	 * Read HSCX A/B VSTR.  Expected value for the S0/16.3 card is
	 * 0x05 or 0x04 (for older 16.3's) in the least significant bits.
	 */

	if( (((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) &&
	     ((HSCX_READ(0, H_VSTR) & 0xf) != 0x4))	||
            (((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) &&
	     ((HSCX_READ(1, H_VSTR) & 0xf) != 0x4)) )  
	{
		printf("isic%d: HSCX VSTR test failed for Teles S0/16.3\n",
			cf->cf_unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			cf->cf_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			cf->cf_unit, HSCX_READ(1, H_VSTR));
		return (0);
	}                   

	return (1);
}


#else

int
isic_probe_s0163(struct isic_attach_args *ia)
{
	bus_space_tag_t t = ia->ia_maps[0].t;
	bus_space_handle_t h = ia->ia_maps[0].h;
	u_int8_t b0, b1, b2;

	b0 = bus_space_read_1(t, h, 0);
	b1 = bus_space_read_1(t, h, 1);
	b2 = bus_space_read_1(t, h, 2);

	if (b0 == 0x51 && b1 == 0x93 && b2 == 0x1c)
		return 1;

	return 0;
}
#endif

/*---------------------------------------------------------------------------*
 *	isic_attach_s0163 - attach Teles S0/16.3 and compatibles
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
int
isic_attach_s0163(struct isa_device *dev)
{
	u_char irq;

	if((irq = intr_no[ffs(dev->id_irq) - 1]) == 1)
	{
		printf("isic%d: Attach error, invalid IRQ [%d] specified for Teles S0/16.3!\n",
			dev->id_unit, ffs(dev->id_irq)-1);
		return(0);
	}

	/* configure IRQ */
	
	DELAY(SEC_DELAY / 10);
	outb(dev->id_iobase + 4, irq);

	DELAY(SEC_DELAY / 10);
	outb(dev->id_iobase + 4, irq | 0x01);	

	return (1);
}

#elif defined(__bsdi__)

extern int
isic_attach_s0163(struct device *parent, struct device *self, struct isa_attach_args *ia)
{
	u_char irq;
	struct isic_softc *sc = (struct isic_softc *)self;
	int unit = sc->sc_dev.dv_unit;

	/* Commit the probed attachement values */

	if (set_softc(sc, ia, unit) == 0)
		panic("isic_attach_s0163: set_softc");

	if (((unsigned)sc->sc_unit) >= NISIC)
		panic("attach isic%d; NISIC=%d", sc->sc_unit, NISIC);
	isic_sc[sc->sc_unit] = sc;
	irq = intr_no[ffs(sc->sc_irq) - 1];
	/* configure IRQ */
	
	DELAY(SEC_DELAY / 10);
	outb(sc->sc_port + 4, irq);

	DELAY(SEC_DELAY / 10);
	outb(sc->sc_port + 4, irq | 0x01);	

	return 1;
}
#else

int
isic_attach_s0163(struct isic_softc *sc)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	u_int8_t irq = intr_no[sc->sc_irq];

	/* configure IRQ */
	
	DELAY(SEC_DELAY / 10);
	bus_space_write_1(t, h, 4, irq);

	DELAY(SEC_DELAY / 10);
	bus_space_write_1(t, h, 4, irq | 0x01);

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = tels0163_read_reg;
	sc->writereg = tels0163_write_reg;

	sc->readfifo = tels0163_read_fifo;
	sc->writefifo = tels0163_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp= CARD_TYPEP_16_3;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	return (1);
}
#endif

#endif /* ISIC > 0 */

