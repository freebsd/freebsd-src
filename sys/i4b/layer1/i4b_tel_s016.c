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
 *	isic - I4B Siemens ISDN Chipset Driver for Teles S0/16 and clones
 *	=================================================================
 *
 * $FreeBSD$ 
 *
 *      last edit-date: [Sun Feb 14 10:28:38 1999]
 *
 *	-hm	clean up
 *	-hm	checked with a Creatix ISDN-S0 (PCB version: mp 130.1)
 *	-hm	more cleanup
 *      -hm     NetBSD patches from Martin
 *	-hm	converting asm -> C
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)
#include "isic.h"
#include "opt_i4b.h"
#else
#define	NISIC 1
#endif
#if NISIC > 0 && defined(TEL_S0_16)

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
#include <machine/md_var.h>
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

static u_char intr_no[] = { 1, 1, 0, 2, 4, 6, 1, 1, 1, 0, 8, 10, 12, 1, 1, 14 };

#ifndef __FreeBSD__
static u_int8_t tels016_read_reg __P((struct isic_softc *sc, int what, bus_size_t offs));
static void tels016_write_reg __P((struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data));
static void tels016_read_fifo __P((struct isic_softc *sc, int what, void *buf, size_t size));
static void tels016_write_fifo __P((struct isic_softc *sc, int what, const void *data, size_t size));
#endif

/*---------------------------------------------------------------------------*
 *	Teles S0/16 write register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
tels016_write_reg(u_char *base, u_int i, u_int v)
{
	if(i & 0x01)
		i |= 0x200;
	base[i] = v;
}

#else

static const bus_size_t offset[] = { 0x100, 0x180, 0x1c0 };

static void
tels016_write_reg(struct isic_softc *sc,	int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;

	offs += offset[what];
	if (offs & 0x01)
		offs |= 0x200;

	bus_space_write_1(t, h, offs, data);
}
#endif

/*---------------------------------------------------------------------------*
 *	Teles S0/16 read register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static u_char
tels016_read_reg(u_char *base, u_int i)
{
	if(i & 0x1)
		i |= 0x200;
	return(base[i]);
}

#else

static u_int8_t
tels016_read_reg(struct isic_softc *sc,	int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;

	offs += offset[what];

	if(offs & 0x01)
		offs |= 0x200;

	return bus_space_read_1(t, h, offs);
}
#endif

/*---------------------------------------------------------------------------*
 *	Teles S0/16 fifo read/write routines
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void		
tels016_memcpyb(void *to, const void *from, size_t len)
{
	for(;len > 0; len--)
		*((unsigned char *)to)++ = *((unsigned char *)from)++;
}

#else

static void
tels016_write_fifo(struct isic_softc *sc, int what, const void *data, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	bus_space_write_region_1(t, h, offset[what], data, size);
}

static void
tels016_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	bus_space_read_region_1(t, h, offset[what], buf, size);
}
#endif

/*---------------------------------------------------------------------------*
 *	isic_probe_s016 - probe for Teles S0/16 and compatibles
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
int
isic_probe_s016(struct isa_device *dev)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];
	u_char byte;

	/* check max unit range */
	
	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for Teles S0/16!\n",
				dev->id_unit, dev->id_unit);
		return(0);	
	}	
	sc->sc_unit = dev->id_unit;

	/* check IRQ validity */

	if((intr_no[ffs(dev->id_irq) - 1]) == 1)
	{
		printf("isic%d: Error, invalid IRQ [%d] specified for Teles S0/16!\n",
			dev->id_unit, ffs(dev->id_irq)-1);
		return(0);
	}
	sc->sc_irq = dev->id_irq;

	/* check if we got an iobase */

	switch(dev->id_iobase)
	{
		case 0xd80:
		case 0xe80:
		case 0xf80:
			break;

		default:
			printf("isic%d: Error, invalid iobase 0x%x specified for Teles S0/16!\n",
				dev->id_unit, dev->id_iobase);
			return(0);
			break;
	}
	sc->sc_port = dev->id_iobase;
	
	/* check if valid memory addr */

	switch((unsigned int)kvtop(dev->id_maddr))
	{
		case 0xc0000:
		case 0xc2000:
		case 0xc4000:
		case 0xc6000:
		case 0xc8000:
		case 0xca000:
		case 0xcc000:
		case 0xce000:
		case 0xd0000:
		case 0xd2000:
		case 0xd4000:
		case 0xd6000:
		case 0xd8000:
		case 0xda000:
		case 0xdc000:
		case 0xde000:
			break;

		default:
			printf("isic%d: Error, invalid mem addr 0x%lx for Teles S0/16!\n",
				dev->id_unit, kvtop(dev->id_maddr));
			return(0);
			break;
	}		
	sc->sc_vmem_addr = (caddr_t) dev->id_maddr;
	dev->id_msize = 0x1000;
	
	/* check card signature */

	if((byte = inb(sc->sc_port)) != 0x51)
	{
		printf("isic%d: Error, signature 1 0x%x != 0x51 for Teles S0/16!\n",
			dev->id_unit, byte);
		return(0);
	}
	
	if((byte = inb(sc->sc_port + 1)) != 0x93)
	{
		printf("isic%d: Error, signature 2 0x%x != 0x93 for Teles S0/16!\n",
			dev->id_unit, byte);
		return(0);
	}
	
	byte = inb(sc->sc_port + 2);

	if((byte != 0x1e) && (byte != 0x1f))
	{
		printf("isic%d: Error, signature 3 0x%x != 0x1e or 0x1f for Teles S0/16!\n",
			dev->id_unit, byte);
		return(0);
	}

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = tels016_read_reg;
	sc->writereg = tels016_write_reg;

	sc->readfifo = tels016_memcpyb;
	sc->writefifo = tels016_memcpyb;

	/* setup card type */
	
	sc->sc_cardtyp= CARD_TYPEP_16;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM1;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;
	
	/* setup ISAC base addr */
	
	ISAC_BASE = (caddr_t) ((dev->id_maddr) + 0x100);

	/* setup HSCX base addr */
	
	HSCX_A_BASE = (caddr_t) ((dev->id_maddr) + 0x180);
	HSCX_B_BASE = (caddr_t) ((dev->id_maddr) + 0x1c0);

	return (1);
}

#else

int
isic_probe_s016(struct isic_attach_args *ia)
{
	bus_space_tag_t t = ia->ia_maps[0].t;
	bus_space_handle_t h = ia->ia_maps[0].h;
	u_int8_t b0, b1, b2;

	b0 = bus_space_read_1(t, h, 0);
	b1 = bus_space_read_1(t, h, 1);
	b2 = bus_space_read_1(t, h, 2);

	if (b0 == 0x51 && b1 == 0x93 && (b2 == 0x1e || b2 == 0x1f))
		return 1;

	return 0;
}
#endif

/*---------------------------------------------------------------------------*
 *	isic_attach_s016 - attach Teles S0/16 and compatibles
 *---------------------------------------------------------------------------*/
int
#ifdef __FreeBSD__
isic_attach_s016(struct isa_device *dev)
#else
isic_attach_s016(struct isic_softc *sc)
#endif
{

#ifdef __FreeBSD__
	struct isic_softc *sc = &isic_sc[dev->id_unit];
#endif

	u_long irq;

#ifndef __FreeBSD__
	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = tels016_read_reg;
	sc->writereg = tels016_write_reg;

	sc->readfifo = tels016_read_fifo;
	sc->writefifo = tels016_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp= CARD_TYPEP_16;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM1;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

#endif

#ifdef __FreeBSD__
	if((irq = intr_no[ffs(dev->id_irq) - 1]) == 1)
	{
		printf("isic%d: Attach error, invalid IRQ [%d] specified for Teles S0/16!\n",
			dev->id_unit, ffs(dev->id_irq)-1);
		return(0);
	}
#else
	irq = intr_no[sc->sc_irq];
#endif

	/* configure IRQ */

#ifdef __FreeBSD__
	irq |= ((u_long) sc->sc_vmem_addr) >> 9;

	DELAY(SEC_DELAY / 10);
	outb(sc->sc_port + 4, irq);

	DELAY(SEC_DELAY / 10);
	outb(sc->sc_port + 4, irq | 0x01);

	DELAY(SEC_DELAY / 5);

	/* set card bit off */

	sc->sc_vmem_addr[0x80] = 0;
	DELAY(SEC_DELAY / 5);

	/* set card bit on */
	
	sc->sc_vmem_addr[0x80] = 1;
	DELAY(SEC_DELAY / 5);

#else

	irq |= ((sc->sc_maddr >> 9) & 0x000000f0);

	DELAY(SEC_DELAY / 10);
	bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, 4, irq);

	DELAY(SEC_DELAY / 10);
	bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, 4, irq | 0x01);

	DELAY(SEC_DELAY / 5);

	/* set card bit off */

	bus_space_write_1(sc->sc_maps[1].t, sc->sc_maps[1].h, 0x80, 0);
	DELAY(SEC_DELAY / 5);

	/* set card bit on */
	
	bus_space_write_1(sc->sc_maps[1].t, sc->sc_maps[1].h, 0x80, 1);
	DELAY(SEC_DELAY / 5);
#endif

	return (1);
}

#endif /* ISIC > 0 */

