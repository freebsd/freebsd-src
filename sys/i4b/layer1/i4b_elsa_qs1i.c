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
 *	isic - I4B Siemens ISDN Chipset Driver for ELSA Quickstep 1000pro ISA
 *	=====================================================================
 *
 *	$Id: i4b_elsa_qs1i.c,v 1.15 1999/03/16 14:57:53 hm Exp $
 *
 *      last edit-date: [Tue Mar 16 15:42:10 1999]
 *
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__

#include "isic.h"
#include "opt_i4b.h"
#include "pnp.h"

#else

#define	NISIC	1
#define	NPNP	1

#endif

/* 
 * this driver works for both the ELSA QuickStep 1000 PNP and the ELSA
 * PCC-16
 */
#if (NISIC > 0) && (((NPNP > 0) && defined(ELSA_QS1ISA)) || defined(ELSA_PCC16))

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#if __FreeBSD__ >= 3
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif
#include <machine/clock.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/pnp.h>
#else
#include <machine/bus.h>
#include <sys/device.h>
#endif

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

#ifdef __FreeBSD__
static void i4b_eq1i_clrirq(void* base);
#else
static void i4b_eq1i_clrirq(struct isic_softc *sc);
void isic_attach_Eqs1pi __P((struct isic_softc *sc));
#endif

/* masks for register encoded in base addr */

#define ELSA_BASE_MASK		0x0ffff
#define ELSA_OFF_MASK		0xf0000

/* register id's to be encoded in base addr */

#define ELSA_IDISAC		0x00000
#define ELSA_IDHSCXA		0x10000
#define ELSA_IDHSCXB		0x20000

/* offsets from base address */

#define ELSA_OFF_ISAC		0x00
#define ELSA_OFF_HSCX		0x02
#define ELSA_OFF_OFF		0x03
#define ELSA_OFF_CTRL		0x04
#define ELSA_OFF_CFG		0x05
#define ELSA_OFF_TIMR		0x06
#define ELSA_OFF_IRQ		0x07

/* control register (write access) */

#define ELSA_CTRL_LED_YELLOW	0x02
#define ELSA_CTRL_LED_GREEN	0x08
#define ELSA_CTRL_RESET		0x20
#define ELSA_CTRL_TIMEREN	0x80
#define ELSA_CTRL_SECRET	0x50

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/ISA clear IRQ routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static void
i4b_eq1i_clrirq(void* base)
{
	outb((u_int)base + ELSA_OFF_IRQ, 0);
}

#else
static void
i4b_eq1i_clrirq(struct isic_softc *sc)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	bus_space_write_1(t, h, ELSA_OFF_IRQ, 0);
}
#endif

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/ISA ISAC get fifo routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void             
eqs1pi_read_fifo(void *buf, const void *base, size_t len)
{
	if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXB)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, 0x40);
		insb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX), (u_char *)buf, (u_int)len);
	}
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXA)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, 0);
		insb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX), (u_char *)buf, (u_int)len);
	}		
	else /* if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDISAC) */
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, 0);
		insb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ISAC), (u_char *)buf, (u_int)len);
	}		
}

#else

static void
eqs1pi_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_read_multi_1(t, h, ELSA_OFF_ISAC, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_read_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40);
			bus_space_read_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
	}
}

#endif

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/ISA ISAC put fifo routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
eqs1pi_write_fifo(void *base, const void *buf, size_t len)
{
	if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXB)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, 0x40);
		outsb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX), (u_char *)buf, (u_int)len);
	}
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXA)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, 0);
		outsb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX), (u_char *)buf, (u_int)len);
	}		
	else /* if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDISAC) */
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, 0);
		outsb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ISAC), (u_char *)buf, (u_int)len);
	}
}

#else

static void
eqs1pi_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_write_multi_1(t, h, ELSA_OFF_ISAC, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_write_multi_1(t, h, ELSA_OFF_HSCX, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40);
			bus_space_write_multi_1(t, h, ELSA_OFF_HSCX, (u_int8_t*)buf, size);
			break;
	}
}
#endif

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/ISA ISAC put register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
eqs1pi_write_reg(u_char *base, u_int offset, u_int v)
{
	if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXB)
	{
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, (u_char)(offset+0x40));
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX, (u_char)v);
	}		
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXA)
	{
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, (u_char)offset);
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX, (u_char)v);
	}		
	else /* if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDISAC) */
	{
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, (u_char)offset);
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ISAC, (u_char)v);
	}		
}

#else

static void
eqs1pi_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			bus_space_write_1(t, h, ELSA_OFF_ISAC, data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			bus_space_write_1(t, h, ELSA_OFF_HSCX, data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40+offs);
			bus_space_write_1(t, h, ELSA_OFF_HSCX, data);
			break;
	}
}
#endif

/*---------------------------------------------------------------------------*
 *	ELSA QuickStep 1000pro/ISA ISAC get register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static u_char
eqs1pi_read_reg(u_char *base, u_int offset)
{
	if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXB)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, (u_char)(offset+0x40));
		return(inb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX));
	}
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXA)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, (u_char)offset);
		return(inb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX));
	}		
	else /* if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDISAC) */
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, (u_char)offset);
		return(inb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ISAC));
	}		
}

#else

static u_int8_t
eqs1pi_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			return bus_space_read_1(t, h, ELSA_OFF_ISAC);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			return bus_space_read_1(t, h, ELSA_OFF_HSCX);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40+offs);
			return bus_space_read_1(t, h, ELSA_OFF_HSCX);
	}
	return 0;
}

#endif

#ifdef __FreeBSD__

/*---------------------------------------------------------------------------*
 *	isic_probe_Eqs1pi - probe for ELSA QuickStep 1000pro/ISA and compatibles
 *---------------------------------------------------------------------------*/
int
isic_probe_Eqs1pi(struct isa_device *dev, unsigned int iobase2)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];
	
	/* check max unit range */
	
	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for ELSA QuickStep 1000pro/ISA!\n",
				dev->id_unit, dev->id_unit);
		return(0);	
	}	
	sc->sc_unit = dev->id_unit;

	/* check IRQ validity */

	switch(ffs(dev->id_irq) - 1)
	{
		case 3:
		case 4:		
		case 5:
		case 7:
		case 10:
		case 11:
		case 12:
		case 15:		
			break;
			
		default:
			printf("isic%d: Error, invalid IRQ [%d] specified for ELSA QuickStep 1000pro/ISA!\n",
				dev->id_unit, ffs(dev->id_irq)-1);
			return(0);
			break;
	}
	sc->sc_irq = dev->id_irq;

	/* check if memory addr specified */

	if(dev->id_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for ELSA QuickStep 1000pro/ISA!\n",
			dev->id_unit, (u_long)dev->id_maddr);
		return(0);
	}
	dev->id_msize = 0;
	
	/* check if we got an iobase */

	if(!((dev->id_iobase >= 0x160) && (dev->id_iobase <= 0x360)))
	{
		printf("isic%d: Error, invalid iobase 0x%x specified for ELSA QuickStep 1000pro/ISA!\n",
			dev->id_unit, dev->id_iobase);
		return(0);
	}
	sc->sc_port = dev->id_iobase;

	/* setup access routines */

	sc->clearirq = i4b_eq1i_clrirq;
	sc->readreg = eqs1pi_read_reg;
	sc->writereg = eqs1pi_write_reg;

	sc->readfifo = eqs1pi_read_fifo;
	sc->writefifo = eqs1pi_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_ELSAQS1ISA;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;	

	/* setup ISAC and HSCX base addr */
	
	ISAC_BASE   = (caddr_t) ((u_int)dev->id_iobase | ELSA_IDISAC);
	HSCX_A_BASE = (caddr_t) ((u_int)dev->id_iobase | ELSA_IDHSCXA);
	HSCX_B_BASE = (caddr_t) ((u_int)dev->id_iobase | ELSA_IDHSCXB);

	/* 
	 * Read HSCX A/B VSTR.  Expected value for the ELSA QuickStep 1000pro
	 * ISA card is 0x05 ( = version 2.1 ) in the least significant bits.
	 */

	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) ||
            ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for ELSA QuickStep 1000pro/ISA\n",
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
 *	isic_attach_s0163P - attach ELSA QuickStep 1000pro/ISA
 *---------------------------------------------------------------------------*/
int
isic_attach_Eqs1pi(struct isa_device *dev, unsigned int iobase2)
{
	u_char byte = ELSA_CTRL_SECRET;

	byte &= ~ELSA_CTRL_RESET;
        outb(dev->id_iobase + ELSA_OFF_CTRL, byte);
        DELAY(20);
	byte |= ELSA_CTRL_RESET;
        outb(dev->id_iobase + ELSA_OFF_CTRL, byte);

        DELAY(20);
        outb(dev->id_iobase + ELSA_OFF_IRQ, 0xff);

	return(1);
}

#else /* !__FreeBSD__ */

void
isic_attach_Eqs1pi(struct isic_softc *sc)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	u_char byte = ELSA_CTRL_SECRET;

	byte &= ~ELSA_CTRL_RESET;
        bus_space_write_1(t, h, ELSA_OFF_CTRL, byte);
        DELAY(20);
	byte |= ELSA_CTRL_RESET;
        bus_space_write_1(t, h, ELSA_OFF_CTRL, byte);

        DELAY(20);
        bus_space_write_1(t, h, ELSA_OFF_IRQ, 0xff);

	/* setup access routines */

	sc->clearirq = i4b_eq1i_clrirq;
	sc->readreg = eqs1pi_read_reg;
	sc->writereg = eqs1pi_write_reg;

	sc->readfifo = eqs1pi_read_fifo;
	sc->writefifo = eqs1pi_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_ELSAQS1ISA;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;	
}

#endif

#endif /* (NISIC > 0) && (NPNP > 0) && defined(ELSA_QS1ISA) */
