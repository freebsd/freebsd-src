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
 *	i4b_usr_sti.c - USRobotics Sportster ISDN TA intern (Tina-pp)
 *	-------------------------------------------------------------
 *
 * $FreeBSD$ 
 *
 *      last edit-date: [Sun Feb 14 10:29:05 1999]
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)
#include "isic.h"
#include "opt_i4b.h"
#else
#define	NISIC 1
#endif
#if NISIC > 0 && defined(USR_STI)

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

/*---------------------------------------------------------------------------*
 *	USR Sportster TA intern special registers
 *---------------------------------------------------------------------------*/
#define USR_HSCXA_OFF	0x0000
#define USR_HSCXB_OFF	0x4000
#define USR_INTL_OFF	0x8000
#define USR_ISAC_OFF	0xc000

#define USR_RES_BIT	0x80	/* 0 = normal, 1 = reset ISAC/HSCX	*/
#define USR_INTE_BIT	0x40	/* 0 = IRQ disabled, 1 = IRQ's enabled	*/
#define USR_IL_MASK	0x07	/* IRQ level config			*/

static u_char intr_no[] = { 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 3, 4, 5, 0, 6, 7 };

#ifdef __FreeBSD__

#define ADDR(reg)	\
	(((reg/4) * 1024) + ((reg%4) * 2))

/*---------------------------------------------------------------------------*
 *	USRobotics read fifo routine
 *---------------------------------------------------------------------------*/
static void		
usrtai_read_fifo(void *buf, const void *base, size_t len)
{
	register int offset = 0;

	for(;len > 0; len--, offset++)	
		*((u_char *)buf + offset) = inb((int)base + ADDR(offset));
}

/*---------------------------------------------------------------------------*
 *	USRobotics write fifo routine
 *---------------------------------------------------------------------------*/
static void
usrtai_write_fifo(void *base, const void *buf, size_t len)
{
	register int offset = 0;
	
	for(;len > 0; len--, offset++)
		outb((int)base + ADDR(offset), *((u_char *)buf + offset));
}

/*---------------------------------------------------------------------------*
 *	USRobotics write register routine
 *---------------------------------------------------------------------------*/
static void
usrtai_write_reg(u_char *base, u_int offset, u_int v)
{
	outb((int)base + ADDR(offset), (u_char)v);
}

/*---------------------------------------------------------------------------*
 *	USRobotics read register routine
 *---------------------------------------------------------------------------*/
static u_char
usrtai_read_reg(u_char *base, u_int offset)
{
	return(inb((int)base + ADDR(offset)));
}

/*---------------------------------------------------------------------------*
 *	isic_probe_usrtai - probe for USR
 *---------------------------------------------------------------------------*/
int
isic_probe_usrtai(struct isa_device *dev)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];
	
	/* check max unit range */
	
	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= MAXUNIT for USR Sportster TA!\n",
				dev->id_unit, dev->id_unit);
		return(0);	
	}	
	sc->sc_unit = dev->id_unit;

	/* check IRQ validity */

	if((intr_no[ffs(dev->id_irq) - 1]) == 0)
	{
		printf("isic%d: Error, invalid IRQ [%d] specified for USR Sportster TA!\n",
			dev->id_unit, (ffs(dev->id_irq))-1);
		return(0);
	}
	sc->sc_irq = dev->id_irq;

	/* check if memory addr specified */

	if(dev->id_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for USR Sportster TA!\n",
			dev->id_unit, (u_long)dev->id_maddr);
		return(0);
	}
	dev->id_msize = 0;
	
	/* check if we got an iobase */

	switch(dev->id_iobase)
	{
		case 0x200:
		case 0x208:
		case 0x210:
		case 0x218:
		case 0x220:
		case 0x228:
		case 0x230:
		case 0x238:
		case 0x240:
		case 0x248:
		case 0x250:
		case 0x258:
		case 0x260:
		case 0x268:
		case 0x270:
		case 0x278:
			break;
			
		default:
			printf("isic%d: Error, invalid iobase 0x%x specified for USR Sportster TA!\n",
				dev->id_unit, dev->id_iobase);
			return(0);
			break;
	}
	sc->sc_port = dev->id_iobase;
	
	/* setup ISAC access routines */

	sc->clearirq = NULL;
	sc->readreg = usrtai_read_reg;
	sc->writereg = usrtai_write_reg;

	sc->readfifo = usrtai_read_fifo;
	sc->writefifo = usrtai_write_fifo;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_USRTA;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;
	
	/* setup ISAC and HSCX base addr */
	
	ISAC_BASE   = (caddr_t)dev->id_iobase + USR_ISAC_OFF;
	HSCX_A_BASE = (caddr_t)dev->id_iobase + USR_HSCXA_OFF;
	HSCX_B_BASE = (caddr_t)dev->id_iobase + USR_HSCXB_OFF;

	/* 
	 * Read HSCX A/B VSTR.  Expected value for USR Sportster TA based
	 * boards is 0x05 in the least significant bits.
	 */

	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) ||
            ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for USR Sportster TA\n",
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
 *	isic_attach_usrtai - attach USR
 *---------------------------------------------------------------------------*/
int
isic_attach_usrtai(struct isa_device *dev)
{
	u_char irq = 0;
	
	/* reset the HSCX and ISAC chips */
	
	outb(dev->id_iobase + USR_INTL_OFF, USR_RES_BIT);
	DELAY(SEC_DELAY / 10);

	outb(dev->id_iobase + USR_INTL_OFF, 0x00);
	DELAY(SEC_DELAY / 10);

	/* setup IRQ */

	if((irq = intr_no[ffs(dev->id_irq) - 1]) == 0)
	{
		printf("isic%d: Attach error, invalid IRQ [%d] specified for USR Sportster TA!\n",
			dev->id_unit, ffs(dev->id_irq)-1);
		return(0);
	}

	/* configure and enable irq */

	outb(dev->id_iobase + USR_INTL_OFF, irq | USR_INTE_BIT);
	DELAY(SEC_DELAY / 10);

	return (1);
}

#else /* end of FreeBSD, start NetBSD */

/*
 * Use of sc->sc_maps:
 *       0 : config register
 *  1 - 16 : HSCX A registers
 * 17 - 32 : HSCX B registers
 * 33 - 48 : ISAC registers
 */

#define USR_REG_OFFS(reg)	((reg % 4) * 2)
#define USR_HSCXA_MAP(reg)	((reg / 4) + 1)
#define USR_HSCXB_MAP(reg)	((reg / 4) + 17)
#define USR_ISAC_MAP(reg)	((reg / 4) + 33)

static int map_base[] = { 33, 1, 17, 0 };	/* ISAC, HSCX A, HSCX B */

/*---------------------------------------------------------------------------*
 *	USRobotics read fifo routine
 *---------------------------------------------------------------------------*/
static void
usrtai_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	int map, off, offset;
	u_char * p = buf;
	bus_space_tag_t t;
	bus_space_handle_t h;

	for (offset = 0; size > 0; size--, offset++) {
		map = map_base[what] + (offset / 4);
		t = sc->sc_maps[map].t;
		h = sc->sc_maps[map].h;
		off = USR_REG_OFFS(offset);

		*p++ = bus_space_read_1(t, h, off);
	}
}

/*---------------------------------------------------------------------------*
 *	USRobotics write fifo routine
 *---------------------------------------------------------------------------*/
static void
usrtai_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	int map, off, offset;
	const u_char * p = buf;
	bus_space_tag_t t;
	bus_space_handle_t h;
	u_char v;

	for (offset = 0; size > 0; size--, offset++) {
		map = map_base[what] + (offset / 4);
		t = sc->sc_maps[map].t;
		h = sc->sc_maps[map].h;
		off = USR_REG_OFFS(offset);

		v = *p++;
		bus_space_write_1(t, h, off, v);
	}
}

/*---------------------------------------------------------------------------*
 *	USRobotics write register routine
 *---------------------------------------------------------------------------*/
static void
usrtai_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	int map = map_base[what] + (offs / 4), 
	    off = USR_REG_OFFS(offs);
	bus_space_tag_t t = sc->sc_maps[map].t;
	bus_space_handle_t h = sc->sc_maps[map].h;

	bus_space_write_1(t, h, off, data);
}

/*---------------------------------------------------------------------------*
 *	USRobotics read register routine
 *---------------------------------------------------------------------------*/
static u_char
usrtai_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	int map = map_base[what] + (offs / 4), 
	    off = USR_REG_OFFS(offs);
	bus_space_tag_t t = sc->sc_maps[map].t;
	bus_space_handle_t h = sc->sc_maps[map].h;

	return bus_space_read_1(t, h, off);
}

/*---------------------------------------------------------------------------*
 *	isic_probe_usrtai - probe for USR
 *---------------------------------------------------------------------------*/
int
isic_probe_usrtai(struct isic_attach_args *ia)
{
	/* 
	 * Read HSCX A/B VSTR.  Expected value for IOM2 based
	 * boards is 0x05 in the least significant bits.
	 */

	if(((bus_space_read_1(ia->ia_maps[USR_HSCXA_MAP(H_VSTR)].t, ia->ia_maps[USR_HSCXA_MAP(H_VSTR)].h, USR_REG_OFFS(H_VSTR)) & 0x0f) != 0x05) ||
	   ((bus_space_read_1(ia->ia_maps[USR_HSCXB_MAP(H_VSTR)].t, ia->ia_maps[USR_HSCXB_MAP(H_VSTR)].h, USR_REG_OFFS(H_VSTR)) & 0x0f) != 0x05))
	    	return 0;
	
	return (1);
}

/*---------------------------------------------------------------------------*
 *	isic_attach_usrtai - attach USR
 *---------------------------------------------------------------------------*/
int
isic_attach_usrtai(struct isic_softc *sc)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	u_char irq = intr_no[sc->sc_irq];	

	sc->clearirq = NULL;
	sc->readreg = usrtai_read_reg;
	sc->writereg = usrtai_write_reg;

	sc->readfifo = usrtai_read_fifo;
	sc->writefifo = usrtai_write_fifo;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_USRTA;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;	

	/* reset the HSCX and ISAC chips */
	
	bus_space_write_1(t, h, 0, USR_RES_BIT);
	DELAY(SEC_DELAY / 10);

	bus_space_write_1(t, h, 0, 0x00);
	DELAY(SEC_DELAY / 10);

	/* setup IRQ */

	bus_space_write_1(t, h, 0, irq | USR_INTE_BIT);
	DELAY(SEC_DELAY / 10);

	return (1);
}

#endif /* __FreeBSD__ */

#endif /* ISIC > 0 */
