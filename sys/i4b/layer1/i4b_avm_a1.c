/*
 *   Copyright (c) 1996 Andrew Gordon. All rights reserved.
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
 *	i4b_avm_a1.c - AVM A1/Fritz passive card driver for isdn4bsd
 *	------------------------------------------------------------
 *
 * $FreeBSD$ 
 *
 *      last edit-date: [Sun Feb 14 10:25:11 1999]
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)
#include "isic.h"
#include "opt_i4b.h"
#else
#define	NISIC	1
#endif
#if NISIC > 0 && defined(AVM_A1)

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

#ifndef __FreeBSD__
static u_int8_t avma1_read_reg __P((struct isic_softc *sc, int what, bus_size_t offs));
static void avma1_write_reg __P((struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data));
static void avma1_read_fifo __P((struct isic_softc *sc, int what, void *buf, size_t size));
static void avma1_write_fifo __P((struct isic_softc *sc, int what, const void *data, size_t size));
#endif

/*---------------------------------------------------------------------------*
 *	AVM A1 and AVM Fritz! Card special registers
 *---------------------------------------------------------------------------*/

#define	AVM_CONF_REG	0x1800		/* base offset for config register */
#define	AVM_CONF_IRQ	0x1801		/* base offset for IRQ register    */
					/* config register write           */
#define	 AVM_CONF_WR_RESET	0x01	/* 1 = RESET ISAC and HSCX         */
#define	 AVM_CONF_WR_CCL	0x02	/* 1 = clear counter low nibble    */
#define	 AVM_CONF_WR_CCH	0x04	/* 1 = clear counter high nibble   */
#define	 AVM_CONF_WR_IRQEN	0x08	/* 1 = enable IRQ                  */
#define	 AVM_CONF_WR_TEST	0x10	/* test bit                        */
					/* config register read            */
#define	 AVM_CONF_RD_IIRQ	0x01	/* 0 = ISAC IRQ active             */
#define	 AVM_CONF_RD_HIRQ	0x02	/* 0 = HSCX IRQ active             */
#define	 AVM_CONF_RD_CIRQ	0x04    /* 0 = counter IRQ active          */
#define	 AVM_CONF_RD_ZER1	0x08	/* unused, always read 0           */
#define	 AVM_CONF_RD_TEST	0x10	/* test bit read back              */
#define	 AVM_CONF_RD_ZER2	0x20	/* unused, always read 0           */

/*---------------------------------------------------------------------------*
 *	AVM read fifo routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static void		
avma1_read_fifo(void *buf, const void *base, size_t len)
{
	insb((int)base - 0x3e0, (u_char *)buf, (u_int)len);
}
#else
static void
avma1_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[what+4].t;
	bus_space_handle_t h = sc->sc_maps[what+4].h;
	bus_space_read_multi_1(t, h, 0, buf, size);
}
#endif

/*---------------------------------------------------------------------------*
 *	AVM write fifo routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static void
avma1_write_fifo(void *base, const void *buf, size_t len)
{
	outsb((int)base - 0x3e0, (u_char *)buf, (u_int)len);
}
#else
static void
avma1_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[what+4].t;
	bus_space_handle_t h = sc->sc_maps[what+4].h;
	bus_space_write_multi_1(t, h, 0, (u_int8_t*)buf, size);
}
#endif

/*---------------------------------------------------------------------------*
 *	AVM write register routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static void
avma1_write_reg(u_char *base, u_int offset, u_int v)
{
	outb((int)base + offset, (u_char)v);
}
#else
static void
avma1_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[what+1].t;
	bus_space_handle_t h = sc->sc_maps[what+1].h;
	bus_space_write_1(t, h, offs, data);
}
#endif

/*---------------------------------------------------------------------------*
 *	AVM read register routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static u_char
avma1_read_reg(u_char *base, u_int offset)
{
	return (inb((int)base + offset));
}
#else
static u_int8_t
avma1_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[what+1].t;
	bus_space_handle_t h = sc->sc_maps[what+1].h;
	return bus_space_read_1(t, h, offs);
}
#endif

/*---------------------------------------------------------------------------*
 *	isic_probe_avma1 - probe for AVM A1 and compatibles
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
int
isic_probe_avma1(struct isa_device *dev)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];
	u_char savebyte;
	u_char byte;
	
	/* check max unit range */
	
	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for AVM A1/Fritz!\n",
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
		case 6:
		case 7:
		case 8:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			break;
			
		default:
			printf("isic%d: Error, invalid IRQ [%d] specified for AVM A1/Fritz!\n",
				dev->id_unit, ffs(dev->id_irq)-1);
			return(0);
			break;
	}		
	sc->sc_irq = dev->id_irq;

	/* check if memory addr specified */

	if(dev->id_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for AVM A1/Fritz!\n",
			dev->id_unit, (u_long)dev->id_maddr);
		return(0);
	}
		
	dev->id_msize = 0;
	
	/* check if we got an iobase */

	switch(dev->id_iobase)
	{
		case 0x200:
		case 0x240:
		case 0x300:
		case 0x340:		
			break;
			
		default:
			printf("isic%d: Error, invalid iobase 0x%x specified for AVM A1/Fritz!\n",
				dev->id_unit, dev->id_iobase);
			return(0);
			break;
	}
	sc->sc_port = dev->id_iobase;

	sc->clearirq = NULL;
	sc->readreg = avma1_read_reg;
	sc->writereg = avma1_write_reg;

	sc->readfifo = avma1_read_fifo;
	sc->writefifo = avma1_write_fifo;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_AVMA1;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* setup ISAC and HSCX base addr */
	
	ISAC_BASE = (caddr_t)dev->id_iobase + 0x1400 - 0x20;

	HSCX_A_BASE = (caddr_t)dev->id_iobase + 0x400 - 0x20;
	HSCX_B_BASE = (caddr_t)dev->id_iobase + 0xc00 - 0x20;

	/* 
	 * Read HSCX A/B VSTR.
	 * Expected value for AVM A1 is 0x04 or 0x05 and for the
	 * AVM Fritz!Card is 0x05 in the least significant bits.
	 */

	if( (((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) &&
	     ((HSCX_READ(0, H_VSTR) & 0xf) != 0x4))	||
            (((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) &&
	     ((HSCX_READ(1, H_VSTR) & 0xf) != 0x4)) )  
	{
		printf("isic%d: HSCX VSTR test failed for AVM A1/Fritz\n",
			dev->id_unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(1, H_VSTR));
		return (0);
	}                   

	/* AVM A1 or Fritz! control register bits:	*/
	/*        read                write		*/
	/* 0x01  hscx irq*           RESET		*/
	/* 0x02  isac irq*           clear counter1	*/
	/* 0x04  counter irq*        clear counter2	*/
	/* 0x08  always 0            irq enable		*/
	/* 0x10  read test bit       set test bit	*/
	/* 0x20  always 0            unused		*/

	/*
	 * XXX the following test may be destructive, to prevent the
	 * worst case, we save the byte first, and in case the test
	 * fails, we write back the saved byte .....
	 */

	savebyte = inb(dev->id_iobase + AVM_CONF_REG);
	
	/* write low to test bit */

	outb(dev->id_iobase + AVM_CONF_REG, 0x00);
	
	/* test bit and next higher and lower bit must be 0 */

	if((byte = inb(dev->id_iobase + AVM_CONF_REG) & 0x38) != 0x00)
	{
		printf("isic%d: Error, probe-1 failed, 0x%02x should be 0x00 for AVM A1/Fritz!\n",
				dev->id_unit, byte);
		outb(dev->id_iobase + AVM_CONF_REG, savebyte);
		return (0);
	}

	/* write high to test bit */

	outb(dev->id_iobase + AVM_CONF_REG, 0x10);
	
	/* test bit must be high, next higher and lower bit must be 0 */

	if((byte = inb(dev->id_iobase + AVM_CONF_REG) & 0x38) != 0x10)
	{
		printf("isic%d: Error, probe-2 failed, 0x%02x should be 0x10 for AVM A1/Fritz!\n",
				dev->id_unit, byte);
		outb(dev->id_iobase + AVM_CONF_REG, savebyte);
		return (0);
	}

	return (1);
}

#else

int
isic_probe_avma1(struct isic_attach_args *ia)
{
	u_int8_t savebyte, v1, v2;

	/* 
	 * Read HSCX A/B VSTR.
	 * Expected value for AVM A1 is 0x04 or 0x05 and for the
	 * AVM Fritz!Card is 0x05 in the least significant bits.
	 */

	v1 = bus_space_read_1(ia->ia_maps[ISIC_WHAT_HSCXA+1].t, ia->ia_maps[ISIC_WHAT_HSCXA+1].h, H_VSTR) & 0x0f;
	v2 = bus_space_read_1(ia->ia_maps[ISIC_WHAT_HSCXB+1].t, ia->ia_maps[ISIC_WHAT_HSCXB+1].h, H_VSTR) & 0x0f;
	if (v1 != v2 || (v1 != 0x05 && v1 != 0x04))
	    	return 0;

	/* AVM A1 or Fritz! control register bits:	*/
	/*        read                write		*/
	/* 0x01  hscx irq*           RESET		*/
	/* 0x02  isac irq*           clear counter1	*/
	/* 0x04  counter irq*        clear counter2	*/
	/* 0x08  always 0            irq enable		*/
	/* 0x10  read test bit       set test bit	*/
	/* 0x20  always 0            unused		*/

	/*
	 * XXX the following test may be destructive, to prevent the
	 * worst case, we save the byte first, and in case the test
	 * fails, we write back the saved byte .....
	 */

	savebyte = bus_space_read_1(ia->ia_maps[0].t, ia->ia_maps[0].h, 0);
	
	/* write low to test bit */

	bus_space_write_1(ia->ia_maps[0].t, ia->ia_maps[0].h, 0, 0);
	
	/* test bit and next higher and lower bit must be 0 */

	if((bus_space_read_1(ia->ia_maps[0].t, ia->ia_maps[0].h, 0) & 0x38) != 0x00)
	{
		bus_space_write_1(ia->ia_maps[0].t, ia->ia_maps[0].h, 0, savebyte);
		return 0;
	}

	/* write high to test bit */

	bus_space_write_1(ia->ia_maps[0].t, ia->ia_maps[0].h, 0, 0x10);
	
	/* test bit must be high, next higher and lower bit must be 0 */

	if((bus_space_read_1(ia->ia_maps[0].t, ia->ia_maps[0].h, 0) & 0x38) != 0x10)
	{
		bus_space_write_1(ia->ia_maps[0].t, ia->ia_maps[0].h, 0, savebyte);
		return 0;
	}

	return (1);
}
#endif

/*---------------------------------------------------------------------------*
 *	isic_attach_avma1 - attach AVM A1 and compatibles
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
int
isic_attach_avma1(struct isa_device *dev)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];

	/* reset the HSCX and ISAC chips */
	
	outb(dev->id_iobase + AVM_CONF_REG, 0x00);
	DELAY(SEC_DELAY / 10);

	outb(dev->id_iobase + AVM_CONF_REG, AVM_CONF_WR_RESET);
	DELAY(SEC_DELAY / 10);

	outb(dev->id_iobase + AVM_CONF_REG, 0x00);
	DELAY(SEC_DELAY / 10);

	/* setup IRQ */

	outb(dev->id_iobase + AVM_CONF_IRQ, (ffs(sc->sc_irq)) - 1);
	DELAY(SEC_DELAY / 10);

	/* enable IRQ, disable counter IRQ */

	outb(dev->id_iobase + AVM_CONF_REG, AVM_CONF_WR_IRQEN |
		AVM_CONF_WR_CCH | AVM_CONF_WR_CCL);
	DELAY(SEC_DELAY / 10);

	return (1);
}

#else

int
isic_attach_avma1(struct isic_softc *sc)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;

	sc->clearirq = NULL;
	sc->readreg = avma1_read_reg;
	sc->writereg = avma1_write_reg;

	sc->readfifo = avma1_read_fifo;
	sc->writefifo = avma1_write_fifo;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_AVMA1;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;
	
	/* reset the HSCX and ISAC chips */
	
	bus_space_write_1(t, h, 0, 0x00);
	DELAY(SEC_DELAY / 10);

	bus_space_write_1(t, h, 0, AVM_CONF_WR_RESET);
	DELAY(SEC_DELAY / 10);

	bus_space_write_1(t, h, 0, 0x00);
	DELAY(SEC_DELAY / 10);

	/* setup IRQ */

	bus_space_write_1(t, h, 1, sc->sc_irq);
	DELAY(SEC_DELAY / 10);

	/* enable IRQ, disable counter IRQ */

	bus_space_write_1(t, h, 0, AVM_CONF_WR_IRQEN |
		AVM_CONF_WR_CCH | AVM_CONF_WR_CCL);
	DELAY(SEC_DELAY / 10);

	return (1);
}
#endif

#endif /* ISIC > 0 */
