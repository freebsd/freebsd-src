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
 *	isic - I4B Siemens ISDN Chipset Driver for Teles S0/8 and clones
 *	================================================================
 *
 * $FreeBSD$ 
 *
 *      last edit-date: [Sun Feb 14 10:28:53 1999]
 *
 *	-hm	clean up
 *	-hm	more cleanup
 *      -hm     NetBSD patches from Martin
 *	-hm	making it finally work (checked with board revision 1.2)
 *	-hm	converting asm -> C
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)
#include "isic.h"
#include "opt_i4b.h"
#else
#define	NISIC 1
#endif
#if NISIC > 0 && defined(TEL_S0_8)

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

#ifndef __FreeBSD__
static u_int8_t tels08_read_reg __P((struct isic_softc *sc, int what, bus_size_t offs));
static void tels08_write_reg __P((struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data));
static void tels08_write_fifo __P((struct isic_softc *sc, int what, const void *data, size_t size));
static void tels08_read_fifo __P((struct isic_softc *sc, int what, void *buf, size_t size));
#endif

/*---------------------------------------------------------------------------*
 *	Teles S0/8 write register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
tels08_write_reg(u_char *base, u_int i, u_int v)
{
	if(i & 0x01)
		i |= 0x200;
	base[i] = v;
}

#else

static const bus_size_t offset[] = { 0x100, 0x180, 0x1c0 };

static void
tels08_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;

	offs += offset[what];

	if (offs & 0x01)
		offs |= 0x200;

	bus_space_write_1(t, h, offs, data);
}
#endif

/*---------------------------------------------------------------------------*
 *	Teles S0/8 read register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static u_char
tels08_read_reg(u_char *base, u_int i)
{
	if(i & 0x1)
		i |= 0x200;
	return(base[i]);
}

#else

static u_int8_t
tels08_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;

	offs += offset[what];

	if (offs & 0x01)
		offs |= 0x200;

	return bus_space_read_1(t, h, offs);
}
#endif

/*---------------------------------------------------------------------------*
 *	Teles S0/8 fifo read/write access
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void		
tels08_memcpyb(void *to, const void *from, size_t len)
{
	for(;len > 0; len--)
		*((unsigned char *)to)++ = *((unsigned char *)from)++;
}

#else

static void
tels08_write_fifo(struct isic_softc *sc, int what, const void *data, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	bus_space_write_region_1(t, h, offset[what], data, size);
}

static void
tels08_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	bus_space_read_region_1(t, h, offset[what], buf, size);
}
#endif

/*---------------------------------------------------------------------------*
 *	isic_probe_s08 - probe for Teles S0/8 and compatibles
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

int
isic_probe_s08(struct isa_device *dev)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];

	/* check max unit range */
	
	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for Teles S0/8!\n",
				dev->id_unit, dev->id_unit);
		return(0);	
	}	
	sc->sc_unit = dev->id_unit;

	/* check IRQ validity */
	
	switch(ffs(dev->id_irq)-1)
	{
		case 2:
		case 9:		/* XXX */
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			break;
			
		default:
			printf("isic%d: Error, invalid IRQ [%d] specified for Teles S0/8!\n",
				dev->id_unit, ffs(dev->id_irq)-1);
			return(0);
			break;
	}		
	sc->sc_irq = dev->id_irq;
	
	/* check if we got an iobase */

	if(dev->id_iobase > 0)
	{
		printf("isic%d: Error, iobase specified for Teles S0/8!\n",
				dev->id_unit);
		return(0);	
	}
	
	/* check if inside memory range of 0xA0000 .. 0xDF000 */
	
	if( (kvtop(dev->id_maddr) < 0xa0000) ||
	    (kvtop(dev->id_maddr) > 0xdf000) )
	{
		printf("isic%d: Error, mem addr 0x%lx outside 0xA0000-0xDF000 for Teles S0/8!\n",
				dev->id_unit, kvtop(dev->id_maddr));
		return(0);
	}
		
	sc->sc_vmem_addr = (caddr_t) dev->id_maddr;
	dev->id_msize = 0x1000;
	
	/* setup ISAC access routines */

	sc->clearirq = NULL;
	sc->readreg = tels08_read_reg;
	sc->writereg = tels08_write_reg;

	sc->readfifo = tels08_memcpyb;
	sc->writefifo = tels08_memcpyb;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_8;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM1;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* setup ISAC base addr */
	
	ISAC_BASE = (caddr_t)((dev->id_maddr) + 0x100);

	/* setup HSCX base addr */
	
	HSCX_A_BASE = (caddr_t)((dev->id_maddr) + 0x180);
	HSCX_B_BASE = (caddr_t)((dev->id_maddr) + 0x1c0);
		
	return (1);
}

#else

int
isic_probe_s08(struct isic_attach_args *ia)
{
	/* no real sensible probe is easy - write to fifo memory
	   and read back to verify there is memory doesn't work,
	   because you talk to tx fifo and rcv fifo. So, just check
	   HSCX version, which at least fails if no card present 
	   at the given location. */
	bus_space_tag_t t = ia->ia_maps[0].t;
	bus_space_handle_t h = ia->ia_maps[0].h;
	u_int8_t v1, v2;

	/* HSCX A VSTR */
	v1 = bus_space_read_1(t, h, offset[1] + H_VSTR) & 0x0f;
	if (v1 != HSCX_VA1 && v1 != HSCX_VA2 && v1 != HSCX_VA3 && v1 != HSCX_V21)
		return 0;

	/* HSCX B VSTR */
	v2 = bus_space_read_1(t, h, offset[2] + H_VSTR) & 0x0f;
	if (v2 != HSCX_VA1 && v2 != HSCX_VA2 && v2 != HSCX_VA3 && v2 != HSCX_V21)
		return 0;

	/* both HSCX channels should have the same version... */
	if (v1 != v2)
		return 0;

	return 1;
}
#endif

/*---------------------------------------------------------------------------*
 *	isic_attach_s08 - attach Teles S0/8 and compatibles
 *---------------------------------------------------------------------------*/
int
#ifdef __FreeBSD__
isic_attach_s08(struct isa_device *dev)
#else
isic_attach_s08(struct isic_softc *sc)
#endif
{
#ifdef __FreeBSD__
	struct isic_softc *sc = &isic_sc[dev->id_unit];
#else
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
#endif

	/* set card off */

#ifdef __FreeBSD__
	sc->sc_vmem_addr[0x80] = 0;
#else
	bus_space_write_1(t, h, 0x80, 0);
#endif

	DELAY(SEC_DELAY / 5);

	/* set card on */

#ifdef __FreeBSD__
	sc->sc_vmem_addr[0x80] = 1;
#else
	bus_space_write_1(t, h, 0x80, 1);
#endif

	DELAY(SEC_DELAY / 5);

#ifndef __FreeBSD__
	
	/* setup ISAC access routines */

	sc->clearirq = NULL;
	sc->readreg = tels08_read_reg;
	sc->writereg = tels08_write_reg;
	sc->readfifo = tels08_read_fifo;
	sc->writefifo = tels08_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_8;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM1;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;
	
#endif
  
  	return (1);
}

#endif /* ISIC > 0 */

