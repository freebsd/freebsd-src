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
 *	i4b_drn_ngo.c - Dr. Neuhaus Niccy GO@ and SAGEM Cybermod
 *	--------------------------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sun Feb 14 10:25:39 1999]
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

#if (NISIC > 0) && (NPNP > 0) && defined(DRN_NGO)

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

/*---------------------------------------------------------------------------*
 *	Niccy GO@ definitions
 *
 *	the card uses 2 i/o addressranges each using 2 bytes
 *
 *	addressrange 0:
 *		offset 0 - ISAC dataregister
 *		offset 1 - HSCX dataregister
 *	addressrange 1:
 *		offset 0 - ISAC addressregister
 *		offset 1 - HSCX addressregister
 *
 *	to access an ISAC/HSCX register, you have to write the register
 *	number into the ISAC or HSCX addressregister and then read/write
 *	data for the ISAC/HSCX register into/from the corresponding
 *	dataregister.
 *
 *	Thanks to Klaus Muehle of Dr. Neuhaus Telekommunikation for giving
 *	out this information!
 *                                                     
 *---------------------------------------------------------------------------*/
#define NICCY_PORT_MIN	0x200
#define NICCY_PORT_MAX	0x3e0

#define HSCX_ABIT	0x1000		/* flag, HSCX A is meant */
#define HSCX_BBIT	0x2000		/* flag, HSCX B is meant */

#define HSCX_BOFF	0x40

#define ADDR_OFF	2		/* address register range offset XXX */

#define ISAC_DATA	0
#define HSCX_DATA	1

#define ISAC_ADDR	0
#define HSCX_ADDR	1

#ifdef __FreeBSD__

#if 0
#define HSCXADEBUG
#define HSCXBDEBUG
#define ISACDEBUG
#else
#undef HSCXADEBUG
#undef HSCXBDEBUG
#undef ISACDEBUG
#endif

/*---------------------------------------------------------------------------*
 *	Dr. Neuhaus Niccy GO@ read fifo routine
 *---------------------------------------------------------------------------*/
static void		
drnngo_read_fifo(void *buf, const void *base, size_t len)
{
	register int offset;
	register u_int data;

	int x = SPLI4B();

	if((u_int)base & HSCX_ABIT)
	{
		(u_int)base &= ~HSCX_ABIT;
		(u_int)data = ((u_int)base + HSCX_DATA);
		(u_int)base +=  (ADDR_OFF + HSCX_ADDR);
		offset = 0;
#ifdef HSCXADEBUG
printf("GO/A/frd: base=0x%x, data=0x%x, len=%d\n", base, data, len);
#endif
	}
	else if((u_int)base & HSCX_BBIT)
	{
		(u_int)base &= ~HSCX_BBIT;
		(u_int)data = ((u_int)base + HSCX_DATA);
		(u_int)base +=  (ADDR_OFF + HSCX_ADDR);
		offset = HSCX_BOFF;
#ifdef HSCXBDEBUG
printf("GO/B/frd: base=0x%x, data=0x%x, len=%d\n", base, data, len);
#endif
	}
	else
	{
		(u_int)data = ((u_int)base + ISAC_DATA);
		(u_int)base +=  (ADDR_OFF + ISAC_ADDR);
		offset = 0;		
#ifdef ISACDEBUG
printf("GO/I/frd: base=0x%x, data=0x%x, len=%d\n", base, data, len);
#endif
	}

	for(;len > 0; len--, offset++)
	{
		outb((int)base, (u_char)offset);
		*((u_char *)buf + offset) = inb((int)data);
	}

	splx(x);
}

/*---------------------------------------------------------------------------*
 *	Dr. Neuhaus Niccy GO@ write fifo routine
 *---------------------------------------------------------------------------*/
static void
drnngo_write_fifo(void *base, const void *buf, size_t len)
{
	register int offset;
	register u_int data;

	int x = SPLI4B();
	
	if((u_int)base & HSCX_ABIT)
	{
		(u_int)base &= ~HSCX_ABIT;
		(u_int)data = ((u_int)base + HSCX_DATA);
		(u_int)base +=  (ADDR_OFF + HSCX_ADDR);
		offset = 0;
#ifdef HSCXADEBUG
printf("GO/A/fwr: base=0x%x, data=0x%x, len=%d\n", base, data, len);
#endif
	}
	else if((u_int)base & HSCX_BBIT)
	{
		(u_int)base &= ~HSCX_BBIT;
		(u_int)data = ((u_int)base + HSCX_DATA);
		(u_int)base +=  (ADDR_OFF + HSCX_ADDR);
		offset = HSCX_BOFF;
#ifdef HSCXBDEBUG
printf("GO/B/fwr: base=0x%x, data=0x%x, len=%d\n", base, data, len);
#endif
	}
	else
	{
		(u_int)data = ((u_int)base + ISAC_DATA);		
		(u_int)base +=  (ADDR_OFF + ISAC_ADDR);
		offset = 0;
#ifdef ISACDEBUG
printf("GO/I/fwr: base=0x%x, data=0x%x, len=%d\n", base, data, len);
#endif
	}		

	for(;len > 0; len--, offset++)
	{
		outb((int)base, (u_char)offset);
		outb((int)data, *((u_char *)buf + offset));
	}

	splx(x);
}

/*---------------------------------------------------------------------------*
 *	Dr. Neuhaus Niccy GO@ write register routine
 *---------------------------------------------------------------------------*/
static void
drnngo_write_reg(u_char *base, u_int offset, u_int v)
{
	int x = SPLI4B();
	if((u_int)base & HSCX_ABIT)
	{
		(u_int)base &= ~HSCX_ABIT;
		outb((int)base + ADDR_OFF + HSCX_ADDR, (u_char)offset);
		outb((int)base + HSCX_DATA, (u_char)v);
#ifdef HSCXADEBUG
printf("GO/A/rwr: base=0x%x, addr=0x%x, offset=0x%x, val=0x%x\n",
	(int)base + ADDR_OFF + HSCX_ADDR, (int)base + HSCX_DATA,
	(u_char)offset, (u_char)v);
#endif
	}		
	else if((u_int)base & HSCX_BBIT)
	{
		(u_int)base &= ~HSCX_BBIT;
		outb((int)base + ADDR_OFF + HSCX_ADDR, (u_char)(offset + HSCX_BOFF));
		outb((int)base + HSCX_DATA, (u_char)v);
#ifdef HSCXBDEBUG
printf("GO/B/rwr: base=0x%x, addr=0x%x, offset=0x%x, val=0x%x\n",
	(int)base + ADDR_OFF + HSCX_ADDR, (int)base + HSCX_DATA,
	(u_char)(offset + HSCX_BOFF), (u_char)v);
#endif
	}
	else
	{
		outb((int)base + ADDR_OFF + ISAC_ADDR, (u_char)offset);
		outb((int)base + ISAC_DATA, (u_char)v);	
#ifdef ISACDEBUG
printf("GO/I/rwr: base=0x%x, addr=0x%x, offset=0x%x, val=0x%x\n",
	(int)base + ADDR_OFF + ISAC_ADDR, (int)base + ISAC_DATA,
	(u_char)offset, (u_char)v);
#endif	
	}
	splx(x);
}

/*---------------------------------------------------------------------------*
 *	Dr. Neuhaus Niccy GO@ read register routine
 *---------------------------------------------------------------------------*/
static u_char
drnngo_read_reg(u_char *base, u_int offset)
{
	u_char val;
	int x = SPLI4B();
	
	if((u_int)base & HSCX_ABIT)
	{
		(u_int)base &= ~HSCX_ABIT;
		outb((int)base + ADDR_OFF + HSCX_ADDR, (u_char)offset);
		val = inb((int)base + HSCX_DATA);
#ifdef HSCXADEBUG		
printf("GO/A/rrd: base=0x%x, addr=0x%x, offset=0x%x, val=0x%x\n",
	(int)base + ADDR_OFF + HSCX_ADDR, (int)base + HSCX_DATA,
	(u_char)offset, (u_char)val);
#endif
	}
	else if((u_int)base & HSCX_BBIT)
	{
		(u_int)base &= ~HSCX_BBIT;
		outb((int)base + ADDR_OFF + HSCX_ADDR, (u_char)(offset + HSCX_BOFF));
		val = inb((int)base + HSCX_DATA);
#ifdef HSCXBDEBUG		
printf("GO/B/rrd: base=0x%x, addr=0x%x, offset=0x%x, val=0x%x\n",
	(int)base + ADDR_OFF + HSCX_ADDR, (int)base + HSCX_DATA,
	(u_char)(offset + HSCX_BOFF), (u_char)val);
#endif
	}
	else
	{
		outb((int)base + ADDR_OFF + ISAC_ADDR, (u_char)offset);
		val = inb((int)base + ISAC_DATA);
#ifdef ISACDEBUG		
printf("GO/I/rrd: base=0x%x, addr=0x%x, offset=0x%x, val=0x%x\n",
	(int)base + ADDR_OFF + ISAC_ADDR, (int)base + ISAC_DATA,
	(u_char)offset, (u_char)val);
#endif
	}		
	splx(x);
	return(val);
}

/*---------------------------------------------------------------------------*
 *	isic_probe_drnngo - probe for Dr. Neuhaus Niccy GO@
 *---------------------------------------------------------------------------*/
int
isic_probe_drnngo(struct isa_device *dev, unsigned int iobase2)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];
	
	/* check max unit range */
	
	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for Dr. Neuhaus Niccy GO@!\n",
				dev->id_unit, dev->id_unit);
		return(0);	
	}	
	sc->sc_unit = dev->id_unit;

	/* check IRQ validity */

	switch(ffs(dev->id_irq)-1)
	{
		case 3:
		case 4:
		case 5:
		case 9:
		case 10:
		case 11:
		case 12:
		case 15:
			break;
			
		default:
			printf("isic%d: Error, invalid IRQ [%d] specified for Dr. Neuhaus Niccy GO@!\n",
				dev->id_unit, ffs(dev->id_irq)-1);
			return(0);
			break;
	}		
	sc->sc_irq = dev->id_irq;

	/* check if memory addr specified */

	if(dev->id_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for Dr. Neuhaus Niccy GO@!\n",
			dev->id_unit, (u_long)dev->id_maddr);
		return(0);
	}
	dev->id_msize = 0;
	
	/* check if we got an iobase */

	if(dev->id_iobase < NICCY_PORT_MIN || dev->id_iobase > NICCY_PORT_MAX)
	{
		printf("isic%d: Error, invalid iobase 0x%x specified for Dr. Neuhaus Niccy GO@!\n",
			dev->id_unit, dev->id_iobase);
		return(0);
	}
	sc->sc_port = dev->id_iobase;

	if(iobase2 == 0)
	{
		printf("isic%d: Error, iobase2 is 0 for Dr. Neuhaus Niccy GO@!\n",
			dev->id_unit);
		return(0);
	}

	if(iobase2 < NICCY_PORT_MIN || iobase2 > NICCY_PORT_MAX)
	{
		printf("isic%d: Error, invalid port1 0x%x specified for Dr. Neuhaus Niccy GO@!\n",
			dev->id_unit, iobase2);
		return(0);
	}

/*XXX*/	if((dev->id_iobase + 2) != iobase2)
	{
		printf("isic%d: Error, port1 must be (port0+2) for Dr.Neuhaus Niccy GO@!\n",
			dev->id_unit);
		return(0);
	}
	
	/* setup ISAC access routines */

	sc->clearirq = NULL;
	sc->readreg = drnngo_read_reg;
	sc->writereg = drnngo_write_reg;

	sc->readfifo = drnngo_read_fifo;
	sc->writefifo = drnngo_write_fifo;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_DRNNGO;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;
	
	/* setup ISAC and HSCX base addr */
	
	ISAC_BASE   = (caddr_t)dev->id_iobase;
	HSCX_A_BASE = (caddr_t)(((u_int)dev->id_iobase) | HSCX_ABIT);
	HSCX_B_BASE = (caddr_t)(((u_int)dev->id_iobase) | HSCX_BBIT);

	/* 
	 * Read HSCX A/B VSTR.  Expected value for Dr. Neuhaus Niccy GO@ based
	 * boards is 0x05 in the least significant bits.
	 */

	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) ||
	    ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for Dr. Neuhaus Niccy GO@\n",
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
 *	isic_attach_drnngo - attach Dr. Neuhaus Niccy GO@
 *---------------------------------------------------------------------------*/
int
isic_attach_drnngo(struct isa_device *dev, unsigned int iobase2)
{
	return (1);
}

#else

static u_int8_t drnngo_read_reg __P((struct isic_softc *sc, int what, bus_size_t offs));
static void drnngo_write_reg __P((struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data));
static void drnngo_read_fifo __P((struct isic_softc *sc, int what, void *buf, size_t size));
static void drnngo_write_fifo __P((struct isic_softc *sc, int what, const void *data, size_t size));
void isic_attach_drnngo __P((struct isic_softc *sc));

/*
 * Mapping from "what" parameter to offsets into the io map
 */
static struct {
	bus_size_t oa,	/* address register offset */
		   od, 	/* data register offset */
		   or;	/* additional chip register offset */
} offset[] =
{
	{ ISAC_ADDR, ISAC_DATA, 0 },		/* ISAC access */
	{ HSCX_ADDR, HSCX_DATA, 0 },		/* HSCX A access */
	{ HSCX_ADDR, HSCX_DATA, HSCX_BOFF }	/* HSCX B access */
};

static void
drnngo_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t hd = sc->sc_maps[0].h, ha = sc->sc_maps[1].h;
	bus_space_write_1(t, ha, offset[what].oa, offset[what].or);
	bus_space_read_multi_1(t, hd, offset[what].od, buf, size);
}

static void
drnngo_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t hd = sc->sc_maps[0].h, ha = sc->sc_maps[1].h;
	bus_space_write_1(t, ha, offset[what].oa, offset[what].or);
	bus_space_write_multi_1(t, hd, offset[what].od, (u_int8_t*)buf, size);
}

static void
drnngo_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t hd = sc->sc_maps[0].h, ha = sc->sc_maps[1].h;
	bus_space_write_1(t, ha, offset[what].oa, offs+offset[what].or);
	bus_space_write_1(t, hd, offset[what].od, data);
}

static u_int8_t
drnngo_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t hd = sc->sc_maps[0].h, ha = sc->sc_maps[1].h;
	bus_space_write_1(t, ha, offset[what].oa, offs+offset[what].or);
	return bus_space_read_1(t, hd, offset[what].od);
}

void
isic_attach_drnngo(struct isic_softc *sc)
{
	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = drnngo_read_reg;
	sc->writereg = drnngo_write_reg;

	sc->readfifo = drnngo_read_fifo;
	sc->writefifo = drnngo_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_DRNNGO;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;	
}

#endif

#endif /* (NISIC > 0) && (NPNP > 0) && defined(DRN_NGO) */
