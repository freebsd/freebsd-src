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
 *	isic - I4B Siemens ISDN Chipset Driver for ELSA Quickstep 1000pro PCI
 *	=====================================================================
 *
 * $FreeBSD$
 *
 *      last edit-date: [Wed Mar 10 07:24:32 1999]
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)
#include "isic.h"
#include "opt_i4b.h"
#include "pci.h"
#else
#define	NISIC	1
#endif

#if (NISIC > 0) && /* (NPCI > 0) && */ defined(ELSA_QS1PCI)

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
#else
#include <machine/bus.h>
#include <sys/device.h>
#endif

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>
#include <i4b/layer1/i4b_ipac.h>

#ifndef __FreeBSD__
#include <i4b/layer1/pci_isic.h>
#endif

/* masks for register encoded in base addr */

#define ELSA_BASE_MASK		0x0ffff
#define ELSA_OFF_MASK		0xf0000

/* register id's to be encoded in base addr */

#define ELSA_IDISAC		0x00000
#define ELSA_IDHSCXA		0x10000
#define ELSA_IDHSCXB		0x20000
#define ELSA_IDIPAC		0x40000

/* offsets from base address */

#define ELSA_OFF_ALE		0x00
#define ELSA_OFF_RW		0x01

#define ELSA_PORT0_MAPOFF	PCI_MAPREG_START+4
#define ELSA_PORT1_MAPOFF	PCI_MAPREG_START+12

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/PCI ISAC get fifo routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void             
eqs1pp_read_fifo(void *buf, const void *base, size_t len)
{
	if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXB)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, IPAC_HSCXB_OFF);
		insb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW), (u_char *)buf, (u_int)len);
	}
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXA)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, IPAC_HSCXA_OFF);
		insb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW), (u_char *)buf, (u_int)len);
	}		
	else /* if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDISAC) */
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, IPAC_ISAC_OFF);
		insb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW), (u_char *)buf, (u_int)len);
	}		
}

#else

static void
eqs1pp_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_ISAC_OFF);
			bus_space_read_multi_1(t, h, ELSA_OFF_RW, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXA_OFF);
			bus_space_read_multi_1(t, h, ELSA_OFF_RW, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXB_OFF);
			bus_space_read_multi_1(t, h, ELSA_OFF_RW, buf, size);
			break;
	}
}

#endif

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/PCI ISAC put fifo routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
eqs1pp_write_fifo(void *base, const void *buf, size_t len)
{
	if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXB)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, IPAC_HSCXB_OFF);
		outsb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW), (u_char *)buf, (u_int)len);
	}
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXA)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, IPAC_HSCXA_OFF);
		outsb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW), (u_char *)buf, (u_int)len);
	}		
	else /* if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDISAC) */
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, IPAC_ISAC_OFF);
		outsb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW), (u_char *)buf, (u_int)len);
	}
}

#else

static void
eqs1pp_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_ISAC_OFF);
			bus_space_write_multi_1(t, h, ELSA_OFF_RW, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXA_OFF);
			bus_space_write_multi_1(t, h, ELSA_OFF_RW, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXB_OFF);
			bus_space_write_multi_1(t, h, ELSA_OFF_RW, (u_int8_t*)buf, size);
			break;
	}
}
#endif

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/PCI ISAC put register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
eqs1pp_write_reg(u_char *base, u_int offset, u_int v)
{
	if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXB)
	{
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, (u_char)(offset+IPAC_HSCXB_OFF));
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW, (u_char)v);
	}		
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXA)
	{
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, (u_char)(offset+IPAC_HSCXA_OFF));
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW, (u_char)v);
	}		
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDISAC)
	{
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, (u_char)(offset+IPAC_ISAC_OFF));
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW, (u_char)v);
	}		
	else /* IPAC */
	{
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, (u_char)(offset+IPAC_IPAC_OFF));
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW, (u_char)v);
	}		
}

#else

static void
eqs1pp_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_ISAC_OFF+offs);
			bus_space_write_1(t, h, ELSA_OFF_RW, data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXA_OFF+offs);
			bus_space_write_1(t, h, ELSA_OFF_RW, data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXB_OFF+offs);
			bus_space_write_1(t, h, ELSA_OFF_RW, data);
			break;
		case ISIC_WHAT_IPAC:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_IPAC_OFF+offs);
			bus_space_write_1(t, h, ELSA_OFF_RW, data);
			break;
	}
}
#endif

/*---------------------------------------------------------------------------*
 *	ELSA QuickStep 1000pro/PCI ISAC get register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static u_char
eqs1pp_read_reg(u_char *base, u_int offset)
{
	if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXB)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, (u_char)(offset+IPAC_HSCXB_OFF));
		return(inb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW));
	}
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXA)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, (u_char)(offset+IPAC_HSCXA_OFF));
		return(inb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW));
	}		
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDISAC)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, (u_char)(offset+IPAC_ISAC_OFF));
		return(inb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW));
	}		
	else /* IPAC */
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ALE, (u_char)(offset+IPAC_IPAC_OFF));
		return(inb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_RW));
	}		
}

#else

static u_int8_t
eqs1pp_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_ISAC_OFF+offs);
			return bus_space_read_1(t, h, ELSA_OFF_RW);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXA_OFF+offs);
			return bus_space_read_1(t, h, ELSA_OFF_RW);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXB_OFF+offs);
			return bus_space_read_1(t, h, ELSA_OFF_RW);
		case ISIC_WHAT_IPAC:
		{
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_IPAC_OFF+offs);
			return bus_space_read_1(t, h, ELSA_OFF_RW);
		}
	}

	return 0;
}

#endif

/*---------------------------------------------------------------------------*
 *	isic_attach_Eqs1pp - attach for ELSA QuickStep 1000pro/PCI
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
int
isic_attach_Eqs1pp(int unit, unsigned int iobase1, unsigned int iobase2)
{
	struct isic_softc *sc = &isic_sc[unit];
	
	/* check max unit range */
	
	if(unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for ELSA QuickStep 1000pro/PCI!\n",
				unit, unit);
		return(0);	
	}	
	sc->sc_unit = unit;

	/* setup iobase */

	if((iobase2 <= 0) || (iobase2 > 0xffff))
	{
		printf("isic%d: Error, invalid iobase 0x%x specified for ELSA QuickStep 1000pro/PCI!\n",
			unit, iobase2);
		return(0);
	}
	sc->sc_port = iobase2;

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = eqs1pp_read_reg;
	sc->writereg = eqs1pp_write_reg;

	sc->readfifo = eqs1pp_read_fifo;
	sc->writefifo = eqs1pp_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_ELSAQS1PCI;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* setup chip type = IPAC ! */
	
	sc->sc_ipac = 1;
	sc->sc_bfifolen = IPAC_BFIFO_LEN;
	
	/* setup ISAC and HSCX base addr */
	
	ISAC_BASE   = (caddr_t) ((u_int)iobase2 | ELSA_IDISAC);
	HSCX_A_BASE = (caddr_t) ((u_int)iobase2 | ELSA_IDHSCXA);
	HSCX_B_BASE = (caddr_t) ((u_int)iobase2 | ELSA_IDHSCXB);
	IPAC_BASE   = (caddr_t) ((u_int)iobase2 | ELSA_IDIPAC);

	/* enable hscx/isac irq's */
	IPAC_WRITE(IPAC_MASK, (IPAC_MASK_INT1 | IPAC_MASK_INT0));

	IPAC_WRITE(IPAC_ACFG, 0);	/* outputs are open drain */
	IPAC_WRITE(IPAC_AOE,		/* aux 5..2 are inputs, 7, 6 outputs */
		(IPAC_AOE_OE5 | IPAC_AOE_OE4 | IPAC_AOE_OE3 | IPAC_AOE_OE2));
	IPAC_WRITE(IPAC_ATX, 0xff);	/* set all output lines high */

        outb(iobase1 + 0x4c, 0x41);	/* enable card interrupt */
        
	return (1);
}

#else	/* !FreeBSD */

void
isic_attach_Eqs1pp(psc, pa)
	struct pci_isic_softc *psc;
	struct pci_attach_args *pa;
{
	struct isic_softc *sc = &psc->sc_isic;

	/* setup io mappings */
	sc->sc_num_mappings = 2;
	MALLOC_MAPS(sc);
	sc->sc_maps[0].size = 0;
	if (pci_mapreg_map(pa, ELSA_PORT0_MAPOFF, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_maps[0].t, &sc->sc_maps[0].h, NULL, NULL)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}
	sc->sc_maps[1].size = 0;
	if (pci_mapreg_map(pa, ELSA_PORT1_MAPOFF, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_maps[1].t, &sc->sc_maps[1].h, NULL, NULL)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = eqs1pp_read_reg;
	sc->writereg = eqs1pp_write_reg;

	sc->readfifo = eqs1pp_read_fifo;
	sc->writefifo = eqs1pp_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_ELSAQS1PCI;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* setup chip type = IPAC ! */
	
	sc->sc_ipac = 1;
	sc->sc_bfifolen = IPAC_BFIFO_LEN;
	
	/* enable hscx/isac irq's */
	IPAC_WRITE(IPAC_MASK, (IPAC_MASK_INT1 | IPAC_MASK_INT0));

	IPAC_WRITE(IPAC_ACFG, 0);	/* outputs are open drain */
	IPAC_WRITE(IPAC_AOE,		/* aux 5..2 are inputs, 7, 6 outputs */
		(IPAC_AOE_OE5 | IPAC_AOE_OE4 | IPAC_AOE_OE3 | IPAC_AOE_OE2));
	IPAC_WRITE(IPAC_ATX, 0xff);	/* set all output lines high */

        bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, 0x4c, 0x41);	/* enable card interrupt */
}

#endif

#endif /* (NISIC > 0) && defined(ELSA_QS1PCI) */
