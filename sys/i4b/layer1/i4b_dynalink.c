/*
 *   Copyright (c) 1998 Martijn Plak. All rights reserved.
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
 *	isdn4bsd layer1 driver for Dynalink IS64PH isdn TA
 *	==================================================
 *
 * $FreeBSD: src/sys/i4b/layer1/i4b_dynalink.c,v 1.7 1999/12/07 12:06:31 peter Exp $
 *
 *      last edit-date: [Sun Feb 14 10:26:21 1999]
 *
 *	written by Martijn Plak <tigrfhur@xs4all.nl>
 *
 *	-mp 11 jun 1998 first try, code borrowed from Creatix driver
 *	-mp 18 jun 1998 cleaned up code
 *	-hm FreeBSD PnP
 *	-mp 17 dec 1998 made it compile again
 *
 *---------------------------------------------------------------------------*/

/*	NOTES:
	
	This driver was written for the Dynalink IS64PH ISDN TA, based on two 
	Siemens chips (HSCX 21525 and ISAC 2186). It is sold in the Netherlands.
	
	model numbers found on (my) card:
		IS64PH, TAS100H-N, P/N:89590555, TA200S100045521
	
	chips: 	
		Siemens PSB 21525N, HSCX TE V2.1
		Siemens PSB 2186N, ISAC-S TE V1.1
		95MS14, PNP
	
	plug-and-play info: 
		device id 	"ASU1688" 
		vendor id 	0x88167506 
		serial 		0x00000044
		i/o port	4 byte alignment, 4 bytes requested, 
				10 bit i/o decoding, 0x100-0x3f8 (?)
		irq		3,4,5,9,10,11,12,15, high true, edge sensitive
			
	At the moment I'm writing this Dynalink is replacing this card with 
	one based on a single Siemens chip (IPAC). It will apparently be sold 
	under the same model name.

	This driver might also work for Asuscom cards.
*/

#ifdef __FreeBSD__

#include "isic.h"
#include "opt_i4b.h"

#else

#define NISIC   1

#endif

#define NPNP    1

#if (NISIC > 0) && (NPNP > 0) && defined(DYNALINK)

/* HEADERS
*/

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
/* #include <i386/isa/pnp.h> */
#elif defined(__bsdi__)
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

#if defined(__FreeBSD__) || defined(__bsdi__)
static void dynalink_read_fifo(void *buf, const void *base, size_t len);
static void dynalink_write_fifo(void *base, const void *buf, size_t len);
static void dynalink_write_reg(u_char *base, u_int offset, u_int v);
static u_char dynalink_read_reg(u_char *base, u_int offset);
#endif

#ifdef __FreeBSD__
extern struct isa_driver isicdriver;
#endif
#ifdef __bsdi__
extern struct cfdriver isiccd;
#endif

#if !defined(__FreeBSD__) && !defined(__bsdi__)
static void dynalink_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size);
static void dynalink_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size);
static void dynalink_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data);
static u_int8_t dynalink_read_reg(struct isic_softc *sc, int what, bus_size_t offs);
void isic_attach_Dyn(struct isic_softc *sc);
#endif

/* io address mapping */
#define ISAC		0
#define HSCX		1
#define	ADDR		2

/* ADDR bits */
#define ADDRMASK	0x7F
#define RESET		0x80

/* HSCX register offsets */
#define HSCXA		0x00
#define HSCXB		0x40

#if defined(__FreeBSD__) || defined(__bsdi__)
/* base address juggling */
#define HSCXB_HACK		0x400
#define IOBASE(addr)		(((int)addr)&0x3FC)
#define IOADDR(addr)		(((int)addr)&0x3FF)
#define IS_HSCXB_HACK(addr)	((((int)addr)&HSCXB_HACK)?HSCXB:HSCXA)
#endif

#ifdef __FreeBSD__
/* ISIC probe and attach
*/

int
isic_probe_Dyn(struct isa_device *dev, unsigned int iobase2) 
{

	struct isic_softc *sc = &isic_sc[dev->id_unit];

	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for Dynalink IS64PH.\n",
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
		case 9:
		case 10:
		case 11:
		case 12:
		case 15:
			break;
			
		default:
			printf("isic%d: Error, invalid IRQ [%d] specified for Dynalink IS64PH.\n",
				dev->id_unit, ffs(dev->id_irq)-1);
			return(0);
			break;
	}
	sc->sc_irq = dev->id_irq;

	/* check if memory addr specified */

	if(dev->id_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for Dynalink IS64PH.\n",
			dev->id_unit, (u_long)dev->id_maddr);
		return (0);
	}
	dev->id_msize = 0;
	
	/* check if we got an iobase */
	if ( (dev->id_iobase < 0x100) || 
	     (dev->id_iobase > 0x3f8) || 
	     (dev->id_iobase & 3) ) 
	{
			printf("isic%d: Error, invalid iobase 0x%x specified for Dynalink!\n", dev->id_unit, dev->id_iobase);
			return(0);
	}
	sc->sc_port = dev->id_iobase;

	/* setup access routines */
	sc->clearirq = NULL;
	sc->readreg = dynalink_read_reg;
	sc->writereg = dynalink_write_reg;
	sc->readfifo = dynalink_read_fifo;
	sc->writefifo = dynalink_write_fifo;

	/* setup card type */	
	sc->sc_cardtyp = CARD_TYPEP_DYNALINK;

	/* setup IOM bus type */
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* setup ISAC and HSCX base addr */
	ISAC_BASE = (caddr_t) sc->sc_port;
	HSCX_A_BASE = (caddr_t) sc->sc_port + 1;
	HSCX_B_BASE = (caddr_t) sc->sc_port + 1 + HSCXB_HACK;

	/* Read HSCX A/B VSTR.  Expected value is 0x05 (V2.1). */
	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) || 
	    ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for Dynalink\n",
			dev->id_unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(1, H_VSTR));
		return (0);
	}

	return (1);                
}

int
isic_attach_Dyn(struct isa_device *dev, unsigned int iobase2)
{
	outb((dev->id_iobase)+ADDR, RESET);
	DELAY(SEC_DELAY / 10);
	outb((dev->id_iobase)+ADDR, 0);
	DELAY(SEC_DELAY / 10);
	return(1);
}

#elif defined(__bsdi__)

/* ISIC probe and attach
*/

static int
set_softc(struct isic_softc *sc, struct isa_attach_args *ia, int unit)
{
	if (unit >= NISIC)
		return 0;
	sc->sc_unit = unit;
	switch(ffs(ia->ia_irq) - 1)
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
			printf("isic%d: Error, invalid IRQ [%d] specified for Dynalink IS64PH.\n",
				unit, ffs(ia->ia_irq)-1);
			return(0);
			break;
	}
	sc->sc_irq = ia->ia_irq;

	/* check if memory addr specified */

	if(ia->ia_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for Dynalink IS64PH.\n",
			unit, (u_long)ia->ia_maddr);
		return (0);
	}
	
	/* check if we got an iobase */
	if ( (ia->ia_iobase < 0x100) || 
	     (ia->ia_iobase > 0x3f8) || 
	     (ia->ia_iobase & 3) ) 
	{
			printf("isic%d: Error, invalid iobase 0x%x specified for Dynalink!\n", unit, ia->ia_iobase);
			return(0);
	}
	sc->sc_port = ia->ia_iobase;

	/* setup access routines */
	sc->clearirq = NULL;
	sc->readreg = dynalink_read_reg;
	sc->writereg = dynalink_write_reg;
	sc->readfifo = dynalink_read_fifo;
	sc->writefifo = dynalink_write_fifo;

	/* setup card type */	
	sc->sc_cardtyp = CARD_TYPEP_DYNALINK;

	/* setup IOM bus type */
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* setup ISAC and HSCX base addr */
	ISAC_BASE = (caddr_t) sc->sc_port;
	HSCX_A_BASE = (caddr_t) sc->sc_port + 1;
	HSCX_B_BASE = (caddr_t) sc->sc_port + 1 + HSCXB_HACK;
	return 1;
}

int
isapnp_match_dynalink(struct device *parent, struct cfdata *cf,
		struct isa_attach_args *ia)
{
	struct isic_softc dummysc, *sc = &dummysc;
	pnp_resource_t res;
	char *ids[] = {"ASU1688", NULL};
	bzero(&res, sizeof res);
	res.res_irq[0].irq_level = ia->ia_irq;
	res.res_port[0].prt_base = ia->ia_iobase;
	res.res_port[0].prt_length = 4;

	if (!pnp_assigndev(ids, isiccd.cd_name, &res))
		return (0);

	ia->ia_irq = res.res_irq[0].irq_level;
	ia->ia_iobase = res.res_port[0].prt_base;
	ia->ia_iosize = res.res_port[0].prt_length;

	if (set_softc(sc, ia, cf->cf_unit) == 0)
		return 0;

	/* Read HSCX A/B VSTR.  Expected value is 0x05 (V2.1). */
	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) || 
	    ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for Dynalink\n",
			cf->cf_unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			cf->cf_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			cf->cf_unit, HSCX_READ(1, H_VSTR));
		return (0);
	}

	cf->cf_flags = FLAG_DYNALINK;
	return (1);
}

int
isic_attach_Dyn(struct device *parent, struct device *self,
		struct isa_attach_args *ia)
{
	struct isic_softc *sc = (struct isic_softc *)self;
	int unit = sc->sc_dev.dv_unit;

	/* Commit the probed attachment values */
	if (set_softc(sc, ia, unit) == 0)
		panic("isic_attach_Dyn: set_softc");

	outb((ia->ia_iobase)+ADDR, RESET);
	DELAY(SEC_DELAY / 10);
	outb((ia->ia_iobase)+ADDR, 0);
	DELAY(SEC_DELAY / 10);
	return(1);
}

#else

void isic_attach_Dyn(struct isic_softc *sc)
{
	/* setup access routines */
	sc->clearirq = NULL;
	sc->readreg = dynalink_read_reg;
	sc->writereg = dynalink_write_reg;
	sc->readfifo = dynalink_read_fifo;
	sc->writefifo = dynalink_write_fifo;

	/* setup card type */	
	sc->sc_cardtyp = CARD_TYPEP_DYNALINK;

	/* setup IOM bus type */
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* Read HSCX A/B VSTR.  Expected value is 0x05 (V2.1). */
	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) || ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("%s: HSCX VSTR test failed for Dynalink PnP\n",
			sc->sc_dev.dv_xname);
		printf("%s: HSC0: VSTR: %#x\n",
			sc->sc_dev.dv_xname, HSCX_READ(0, H_VSTR));
		printf("%s: HSC1: VSTR: %#x\n",
			sc->sc_dev.dv_xname, HSCX_READ(1, H_VSTR));
		return;
	}                   

	bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, ADDR, RESET);
	DELAY(SEC_DELAY / 10);
	bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, ADDR, 0);
	DELAY(SEC_DELAY / 10);
}

#endif /* ISIC>0 && NPNP>0 && defined(DYNALINK) */

/*	LOW-LEVEL DEVICE ACCESS

	NOTE:	The isdn4bsd code expects the two HSCX channels at different 
	base addresses. I'm faking this, and remap them to the same address 
	in the low-level routines. Search for HSCXB_HACK and IS_HSCXB_HACK.

	REM: this is only true for the FreeBSD version of I4B!
*/

#if defined(__FreeBSD__) || defined(__bsdi__)
static void             
dynalink_read_fifo(void *buf, const void *base, size_t len)
{
	outb(IOBASE(base)+ADDR, 0+IS_HSCXB_HACK(base)); 
	insb(IOADDR(base), (u_char *)buf, (u_int)len);
}
#else
static void
dynalink_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ADDR, 0);
			bus_space_read_multi_1(t, h, ISAC, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ADDR, HSCXA);
			bus_space_read_multi_1(t, h, HSCX, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ADDR, HSCXB);
			bus_space_read_multi_1(t, h, HSCX, buf, size);
			break;
	}
}
#endif

#if defined(__FreeBSD__) || defined(__bsdi__)
static void
dynalink_write_fifo(void *base, const void *buf, size_t len)
{
	outb(IOBASE(base)+ADDR, 0+IS_HSCXB_HACK(base));
	outsb(IOADDR(base), (u_char *)buf, (u_int)len);
}
#else
static void dynalink_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ADDR, 0);
			bus_space_write_multi_1(t, h, ISAC, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ADDR, HSCXA);
			bus_space_write_multi_1(t, h, HSCX, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ADDR, HSCXB);
			bus_space_write_multi_1(t, h, HSCX, (u_int8_t*)buf, size);
			break;
	}
}
#endif

#if defined(__FreeBSD__) || defined(__bsdi__)
static void
dynalink_write_reg(u_char *base, u_int offset, u_int v)
{
	outb(IOBASE(base)+ADDR, (offset+IS_HSCXB_HACK(base))&ADDRMASK);
	outb(IOADDR(base), (u_char)v);
}
#else
static void dynalink_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ADDR, offs);
			bus_space_write_1(t, h, ISAC, data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ADDR, HSCXA+offs);
			bus_space_write_1(t, h, HSCX, data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ADDR, HSCXB+offs);
			bus_space_write_1(t, h, HSCX, data);
			break;
	}
}
#endif

#if defined(__FreeBSD__) || defined(__bsdi__)
static u_char
dynalink_read_reg(u_char *base, u_int offset)
{
	outb(IOBASE(base)+ADDR, (offset+IS_HSCXB_HACK(base))&ADDRMASK);
	return (inb(IOADDR(base)));
}
#else
static u_int8_t dynalink_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ADDR, offs);
			return bus_space_read_1(t, h, ISAC);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ADDR, HSCXA+offs);
			return bus_space_read_1(t, h, HSCX);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ADDR, HSCXB+offs);
			return bus_space_read_1(t, h, HSCX);
	}
	return 0;
}
#endif

#endif /* (NISIC > 0) && (NPNP > 0) && defined(DYNALINK) */
