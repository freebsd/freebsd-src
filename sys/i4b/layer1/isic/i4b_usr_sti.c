/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 *	$Id: i4b_usr_sti.c,v 1.3 2000/05/29 15:41:42 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon May 29 16:47:26 2000]
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)
#include "isic.h"
#include "opt_i4b.h"
#else
#define NISIC 1
#endif

#if (NISIC > 0) && defined(USR_STI)

#include <sys/param.h>
#include <sys/systm.h>

#ifdef __FreeBSD__
#include <machine/bus.h>
#include <sys/bus.h>
#include <sys/rman.h>
#else
#include <machine/bus.h>
#include <sys/device.h>
#endif

#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#include <machine/i4b_ioctl.h>
#else
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_hscx.h>


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

#ifdef USRTA_DEBUG_PORTACCESS
int debugcntr;
#define USRTA_DEBUG(fmt) \
		if (++debugcntr < 1000) printf fmt;
#else
#define USRTA_DEBUG(fmt)
#endif

/*---------------------------------------------------------------------------*
 *	USRobotics read fifo routine
 *---------------------------------------------------------------------------*/
static void		
usrtai_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	register int offset = 0;
	register unsigned int base = 0;

USRTA_DEBUG(("usrtai_read_fifo: what %d size %d\n", what, size))
	switch (what)
	{
		case ISIC_WHAT_ISAC:
			base = (unsigned int)ISAC_BASE;
			break;
		case ISIC_WHAT_HSCXA:
			base = (unsigned int)HSCX_A_BASE;
			break;
		case ISIC_WHAT_HSCXB:
			base = (unsigned int)HSCX_B_BASE;
			break;
		default:
			printf("usrtai_read_fifo: invalid what %d\n", what);
			return;
	}

	for(;size > 0; size--, offset++)	
	{
		*((u_char *)buf + offset) = inb(base + ADDR(offset));
	}
}

/*---------------------------------------------------------------------------*
 *	USRobotics write fifo routine
 *---------------------------------------------------------------------------*/
static void
usrtai_write_fifo(struct l1_softc *sc, int what, void *data, size_t size)
{
	register int offset = 0;
	register unsigned int base = 0;

USRTA_DEBUG(("usrtai_write_fifo: what %d size %d\n", what, size))
	switch (what)
	{
		case ISIC_WHAT_ISAC:
			base = (unsigned int)ISAC_BASE;
			break;
		case ISIC_WHAT_HSCXA:
			base = (unsigned int)HSCX_A_BASE;
			break;
		case ISIC_WHAT_HSCXB:
			base = (unsigned int)HSCX_B_BASE;
			break;
		default:
			printf("usrtai_write_fifo: invalid what %d\n", what);
			return;
	}

	
	for(;size > 0; size--, offset++)
	{
		outb(base + ADDR(offset), *((u_char *)data + offset));
	}
}

/*---------------------------------------------------------------------------*
 *	USRobotics write register routine
 *---------------------------------------------------------------------------*/
static void
usrtai_write_reg(struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	register unsigned int base = 0;

USRTA_DEBUG(("usrtai_write_reg: what %d ADDR(%d) %d data %#x\n", what, offs, ADDR(offs), data))
	switch (what)
	{
		case ISIC_WHAT_ISAC:
			base = (unsigned int)ISAC_BASE;
			break;
		case ISIC_WHAT_HSCXA:
			base = (unsigned int)HSCX_A_BASE;
			break;
		case ISIC_WHAT_HSCXB:
			base = (unsigned int)HSCX_B_BASE;
			break;
		default:
			printf("usrtai_write_reg invalid what %d\n", what);
			return;
	}

	outb(base + ADDR(offs), (u_char)data);
}

/*---------------------------------------------------------------------------*
 *	USRobotics read register routine
 *---------------------------------------------------------------------------*/
static u_int8_t
usrtai_read_reg(struct l1_softc *sc, int what, bus_size_t offs)
{
	register unsigned int base = 0;
	u_int8_t byte;

USRTA_DEBUG(("usrtai_read_reg: what %d ADDR(%d) %d..", what, offs, ADDR(offs)))
	switch (what)
	{
		case ISIC_WHAT_ISAC:
			base = (unsigned int)ISAC_BASE;
			break;
		case ISIC_WHAT_HSCXA:
			base = (unsigned int)HSCX_A_BASE;
			break;
		case ISIC_WHAT_HSCXB:
			base = (unsigned int)HSCX_B_BASE;
			break;
		default:
			printf("usrtai_read_reg: invalid what %d\n", what);
			return(0);
	}

	byte = inb(base + ADDR(offs));
USRTA_DEBUG(("usrtai_read_reg: got %#x\n", byte))
	return(byte);
}

/*---------------------------------------------------------------------------*
 *	allocate an io port - based on code in isa_isic.c
 *---------------------------------------------------------------------------*/
static int
usrtai_alloc_port(device_t dev)
{ 
	size_t unit = device_get_unit(dev);
	struct l1_softc *sc = &l1_sc[unit];
	int i, num = 0;
	bus_size_t base;

	/* 49 io mappings: 1 config and 48x8 registers */

	/* config at offset 0x8000 */
	base = sc->sc_port + 0x8000;
	if (base < 0 || base > 0x0ffff)
		return 1;
	sc->sc_resources.io_rid[num] = num;

	bus_set_resource(dev, SYS_RES_IOPORT, num, base, 1);

	if(!(sc->sc_resources.io_base[num] =
		bus_alloc_resource(dev, SYS_RES_IOPORT,
				   &sc->sc_resources.io_rid[num],
				   0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Error, failed to reserve io #%dport %#x!\n", unit, num, base);
		isic_detach_common(dev);
		return(ENXIO);
	}
	num++;

	/* HSCX A at offset 0 */
	base = sc->sc_port;
	for (i = 0; i < 16; i++) {
		if (base+i*1024 < 0 || base+i*1024+8 > 0x0ffff)
			return 1;
		sc->sc_resources.io_rid[num] = num;

		bus_set_resource(dev, SYS_RES_IOPORT, num, base+i*1024, 8);

		if(!(sc->sc_resources.io_base[num] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
					   &sc->sc_resources.io_rid[num],
					   0ul, ~0ul, 1, RF_ACTIVE)))
		{
			printf("isic%d: Error, failed to reserve io #%d port %#x!\n", unit, num, base+i*1024);
			isic_detach_common(dev);
			return(ENXIO);
		}
		++num;
	}

	/* HSCX B at offset 0x4000 */
	base = sc->sc_port + 0x4000;
	for (i = 0; i < 16; i++) {
		if (base+i*1024 < 0 || base+i*1024+8 > 0x0ffff)
			return 1;
		sc->sc_resources.io_rid[num] = num;

		bus_set_resource(dev, SYS_RES_IOPORT, num, base+i*1024, 8);

		if(!(sc->sc_resources.io_base[num] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
					   &sc->sc_resources.io_rid[num],
					   0ul, ~0ul, 1, RF_ACTIVE)))
		{
			printf("isic%d: Error, failed to reserve io #%d port %#x!\n", unit, num, base+i*1024);
			isic_detach_common(dev);
			return(ENXIO);
		}
		++num;
	}

	/* ISAC at offset 0xc000 */
	base = sc->sc_port + 0xc000;
	for (i = 0; i < 16; i++) {
		if (base+i*1024 < 0 || base+i*1024+8 > 0x0ffff)
			return 1;
		sc->sc_resources.io_rid[num] = num;

		bus_set_resource(dev, SYS_RES_IOPORT, num, base+i*1024, 8);

		if(!(sc->sc_resources.io_base[num] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
					   &sc->sc_resources.io_rid[num],
					   0ul, ~0ul, 1, RF_ACTIVE)))
		{
			printf("isic%d: Error, failed to reserve io #%d port %#x!\n", unit, num, base+i*1024);
			isic_detach_common(dev);
			return(ENXIO);
		}
		++num;
	}

	return(0);
}

/*---------------------------------------------------------------------------*
 *	isic_probe_usrtai - probe for USR
 *---------------------------------------------------------------------------*/
int
isic_probe_usrtai(device_t dev)
{
	size_t unit = device_get_unit(dev);	/* get unit */
	struct l1_softc *sc = 0;	/* pointer to softc */
	void *ih = 0;			/* dummy */

	/* check max unit range */

	if(unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for USR Sportster TA!\n",
				unit, unit);
		return(ENXIO);	
	}

	sc = &l1_sc[unit];			/* get pointer to softc */
	sc->sc_unit = unit;			/* set unit */
	sc->sc_flags = FLAG_USR_ISDN_TA_INT;	/* set flags */

	/* see if an io base was supplied */
	
	if(!(sc->sc_resources.io_base[0] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
	                                   &sc->sc_resources.io_rid[0],
	                                   0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get iobase for USR Sportster TA!\n",
				unit);
		return(ENXIO);
	}

	/* set io base */

	sc->sc_port = rman_get_start(sc->sc_resources.io_base[0]);
	
	/* release io base */
	
	bus_release_resource(dev, SYS_RES_IOPORT, sc->sc_resources.io_rid[0],
		sc->sc_resources.io_base[0]);


	/* check if we got an iobase */

	switch(sc->sc_port)
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
				unit, sc->sc_port);
			return(0);
			break;
	}

	/* allocate all the ports needed */

	if(usrtai_alloc_port(dev))
	{
		printf("isic%d: Could not get the ports for USR Sportster TA!\n", unit);
		isic_detach_common(dev);
		return(ENXIO);
	}

	/* get our irq */

	if(!(sc->sc_resources.irq =
		bus_alloc_resource(dev, SYS_RES_IRQ,
				   &sc->sc_resources.irq_rid,
				   0ul, ~0ul, 1, RF_ACTIVE)))
	{
		printf("isic%d: Could not get an irq for USR Sportster TA!\n",unit);
		isic_detach_common(dev);
		return ENXIO;
	}

	/* get the irq number */
	sc->sc_irq = rman_get_start(sc->sc_resources.irq);

	/* register interrupt routine */
	bus_setup_intr(dev, sc->sc_resources.irq, INTR_TYPE_NET,
			(void(*)(void *))(isicintr),
			sc, &ih);

	/* check IRQ validity */

	if(intr_no[sc->sc_irq] == 0)
	{
		printf("isic%d: Error, invalid IRQ [%d] specified for USR Sportster TA!\n",
			unit, sc->sc_irq);
		return(1);
	}

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
	
	ISAC_BASE   = (caddr_t)sc->sc_port + USR_ISAC_OFF;
	HSCX_A_BASE = (caddr_t)sc->sc_port + USR_HSCXA_OFF;
	HSCX_B_BASE = (caddr_t)sc->sc_port + USR_HSCXB_OFF;

	/* 
	 * Read HSCX A/B VSTR.  Expected value for USR Sportster TA based
	 * boards is 0x05 in the least significant bits.
	 */

	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) ||
            ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for USR Sportster TA\n",
			unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			unit, HSCX_READ(1, H_VSTR));
		return (1);
	}                   
	
	return (0);
}

/*---------------------------------------------------------------------------*
 *	isic_attach_usrtai - attach USR
 *---------------------------------------------------------------------------*/
int
isic_attach_usrtai(device_t dev)
{
	u_char irq = 0;
	size_t unit = device_get_unit(dev);	/* get unit */
	struct l1_softc *sc = 0;	/* pointer to softc */
	
	sc = &l1_sc[unit];			/* get pointer to softc */

	/* reset the HSCX and ISAC chips */
	
	outb(sc->sc_port + USR_INTL_OFF, USR_RES_BIT);
	DELAY(SEC_DELAY / 10);

	outb(sc->sc_port + USR_INTL_OFF, 0x00);
	DELAY(SEC_DELAY / 10);

	/* setup IRQ */

	if((irq = intr_no[sc->sc_irq]) == 0)
	{
		printf("isic%d: Attach error, invalid IRQ [%d] specified for USR Sportster TA!\n",
			unit, sc->sc_irq);
		return(1);
	}

	/* configure and enable irq */

	outb(sc->sc_port + USR_INTL_OFF, irq | USR_INTE_BIT);
	DELAY(SEC_DELAY / 10);

	return (0);
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
