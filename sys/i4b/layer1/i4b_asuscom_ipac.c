/*
 * Copyright (c) 1999 Ari Suutari. All rights reserved.
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
 *	isic - I4B Siemens ISDN Chipset Driver for Asuscom ISDNlink 128K PnP
 *	=====================================================================
 *
 * 	This driver works with Asuscom ISDNlink 128K PnP ISA adapter,
 * 	which is based on Siemens IPAC chip (my card probes as ASU1690).
 *	Older Asuscom ISA cards are based on different chipset
 *	(containing two chips) - for those cards, one might want
 *	to try the Dynalink driver.
 *
 *	This driver is heavily based on ELSA Quickstep 1000pro PCI
 *	driver written by Hellmuth Michaelis. Card initialization
 *	code is modeled after Linux i4l driver written by Karsten
 *	Keil.
 *
 *	$Id: i4b_asuscom_ipac.c,v 1.1 1999/07/05 13:46:46 hm Exp $
 *
 *      last edit-date: [Mon May 31 20:53:17 EEST 1999]
 *
 *---------------------------------------------------------------------------*/

#if defined(__FreeBSD__)
#include "isic.h"
#include "opt_i4b.h"
#include "pnp.h"
#else
#define	NISIC	1
#define	NPNP	1
#endif

#if (NISIC > 0) && (NPNP > 0) && defined(ASUSCOM_IPAC)

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
#include <i4b/layer1/i4b_ipac.h>

#ifndef __FreeBSD__
#include <i4b/layer1/pci_isic.h>
#endif

/* masks for register encoded in base addr */

#define ASI_BASE_MASK		0x0ffff
#define ASI_OFF_MASK		0xf0000

/* register id's to be encoded in base addr */

#define ASI_IDISAC		0x00000
#define ASI_IDHSCXA		0x10000
#define ASI_IDHSCXB		0x20000
#define ASI_IDIPAC		0x40000

/* offsets from base address */

#define ASI_OFF_ALE		0x00
#define ASI_OFF_RW		0x01

/*---------------------------------------------------------------------------*
 *      Asuscom ISDNlink 128K PnP ISAC get fifo routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void             
asi_read_fifo(void *buf, const void *base, size_t len)
{
	u_int	asus_base;

	asus_base = ((u_int) base) & ASI_BASE_MASK;
	switch (((u_int) base) & ASI_OFF_MASK) {
	case ASI_IDHSCXB:
	        outb(asus_base + ASI_OFF_ALE, IPAC_HSCXB_OFF);
		insb(asus_base + ASI_OFF_RW, (u_char *)buf, (u_int)len);
		break;
	case ASI_IDHSCXA:
	        outb(asus_base + ASI_OFF_ALE, IPAC_HSCXA_OFF);
		insb(asus_base + ASI_OFF_RW, (u_char *)buf, (u_int)len);
		break;
	case ASI_IDISAC:
	        outb(asus_base + ASI_OFF_ALE, IPAC_ISAC_OFF);
		insb(asus_base + ASI_OFF_RW, (u_char *)buf, (u_int)len);
		break;
	}
}

#else

static void
asi_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	switch (what) {
	case ISIC_WHAT_ISAC:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_ISAC_OFF);
		bus_space_read_multi_1(t, h, ASI_OFF_RW, buf, size);
		break;
	case ISIC_WHAT_HSCXA:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_HSCXA_OFF);
		bus_space_read_multi_1(t, h, ASI_OFF_RW, buf, size);
		break;
	case ISIC_WHAT_HSCXB:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_HSCXB_OFF);
		bus_space_read_multi_1(t, h, ASI_OFF_RW, buf, size);
		break;
	}
}

#endif

/*---------------------------------------------------------------------------*
 *      Asuscom ISDNlink 128K PnP ISAC put fifo routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
asi_write_fifo(void *base, const void *buf, size_t len)
{
	u_int	asus_base;

	asus_base = ((u_int) base) & ASI_BASE_MASK;
	switch (((u_int) base) & ASI_OFF_MASK) {
	case ASI_IDHSCXB:
	        outb(asus_base + ASI_OFF_ALE, IPAC_HSCXB_OFF);
		outsb(asus_base + ASI_OFF_RW, (u_char *)buf, (u_int)len);
		break;
	case ASI_IDHSCXA:
	        outb(asus_base + ASI_OFF_ALE, IPAC_HSCXA_OFF);
		outsb(asus_base + ASI_OFF_RW, (u_char *)buf, (u_int)len);
		break;
	case ASI_IDISAC:
	        outb(asus_base + ASI_OFF_ALE, IPAC_ISAC_OFF);
		outsb(asus_base + ASI_OFF_RW, (u_char *)buf, (u_int)len);
		break;
	}
}

#else

static void
asi_write_fifo(struct isic_softc *sc,
		    int what, const void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	switch (what) {
	case ISIC_WHAT_ISAC:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_ISAC_OFF);
		bus_space_write_multi_1(t, h, ASI_OFF_RW, (u_int8_t*)buf,size);
		break;
	case ISIC_WHAT_HSCXA:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_HSCXA_OFF);
		bus_space_write_multi_1(t, h, ASI_OFF_RW, (u_int8_t*)buf,size);
		break;
	case ISIC_WHAT_HSCXB:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_HSCXB_OFF);
		bus_space_write_multi_1(t, h, ASI_OFF_RW, (u_int8_t*)buf,size);
		break;
	}
}
#endif

/*---------------------------------------------------------------------------*
 *      Asuscom ISDNlink 128K PnP ISAC put register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
asi_write_reg(u_char *base, u_int offset, u_int v)
{
	u_int	asus_base;

	asus_base = ((u_int) base) & ASI_BASE_MASK;
	switch (((u_int) base) & ASI_OFF_MASK) {
	case ASI_IDHSCXB:
	        outb(asus_base + ASI_OFF_ALE, (u_char)(offset+IPAC_HSCXB_OFF));
	        outb(asus_base + ASI_OFF_RW, (u_char)v);
		break;
	case ASI_IDHSCXA:
	        outb(asus_base + ASI_OFF_ALE, (u_char)(offset+IPAC_HSCXA_OFF));
	        outb(asus_base + ASI_OFF_RW, (u_char)v);
		break;
	case ASI_IDISAC:
	        outb(asus_base + ASI_OFF_ALE, (u_char)(offset+IPAC_ISAC_OFF));
	        outb(asus_base + ASI_OFF_RW, (u_char)v);
		break;
	case ASI_IDIPAC:
	        outb(asus_base + ASI_OFF_ALE, (u_char)(offset+IPAC_IPAC_OFF));
	        outb(asus_base + ASI_OFF_RW, (u_char)v);
		break;
	}		
}

#else

static void
asi_write_reg(struct isic_softc *sc,
		   int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	switch (what) {
	case ISIC_WHAT_ISAC:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_ISAC_OFF+offs);
		bus_space_write_1(t, h, ASI_OFF_RW, data);
		break;
	case ISIC_WHAT_HSCXA:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_HSCXA_OFF+offs);
		bus_space_write_1(t, h, ASI_OFF_RW, data);
		break;
	case ISIC_WHAT_HSCXB:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_HSCXB_OFF+offs);
		bus_space_write_1(t, h, ASI_OFF_RW, data);
		break;
	case ISIC_WHAT_IPAC:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_IPAC_OFF+offs);
		bus_space_write_1(t, h, ASI_OFF_RW, data);
		break;
	}
}
#endif

/*---------------------------------------------------------------------------*
 *	Asuscom ISDNlink 128K PnP ISAC get register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static u_char
asi_read_reg(u_char *base, u_int offset)
{
	u_int	asus_base;

	asus_base = ((u_int) base) & ASI_BASE_MASK;
	switch (((u_int) base) & ASI_OFF_MASK) {
	case ASI_IDHSCXB:
	        outb(asus_base + ASI_OFF_ALE, (u_char)(offset+IPAC_HSCXB_OFF));
		return(inb(asus_base + ASI_OFF_RW));
	case ASI_IDHSCXA:
	        outb(asus_base + ASI_OFF_ALE, (u_char)(offset+IPAC_HSCXA_OFF));
		return(inb(asus_base + ASI_OFF_RW));
	case ASI_IDISAC:
	        outb(asus_base + ASI_OFF_ALE, (u_char)(offset+IPAC_ISAC_OFF));
		return(inb(asus_base + ASI_OFF_RW));
	case ASI_IDIPAC:
	        outb(asus_base + ASI_OFF_ALE, (u_char)(offset+IPAC_IPAC_OFF));
		return(inb(asus_base + ASI_OFF_RW));
	}		

	return 0; /* NOTREACHED */
}

#else

static u_int8_t
asi_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	switch (what) {
	case ISIC_WHAT_ISAC:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_ISAC_OFF+offs);
		return bus_space_read_1(t, h, ASI_OFF_RW);
	case ISIC_WHAT_HSCXA:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_HSCXA_OFF+offs);
		return bus_space_read_1(t, h, ASI_OFF_RW);
	case ISIC_WHAT_HSCXB:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_HSCXB_OFF+offs);
		return bus_space_read_1(t, h, ASI_OFF_RW);
	case ISIC_WHAT_IPAC:
		bus_space_write_1(t, h, ASI_OFF_ALE, IPAC_IPAC_OFF+offs);
		return bus_space_read_1(t, h, ASI_OFF_RW);
	}

	return 0;
}

#endif

/*---------------------------------------------------------------------------*
 *	isic_attach_asi - attach for Asuscom ISDNlink 128K PnP
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
int
isic_probe_asi(struct isa_device *dev, unsigned int iobase2)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];
	
	/* check max unit range */
	
	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT "
		       "for Asuscom ISDNlink 128K PnP!\n",
			dev->id_unit, dev->id_unit);

		return(0);	
	}	
	sc->sc_unit = dev->id_unit;

	/* setup iobase */

	if((dev->id_iobase <= 0) || (dev->id_iobase > 0xffff))
	{
		printf("isic%d: Error, invalid iobase 0x%x specified "
		       "for Asuscom ISDNlink 128K PnP\n",
			dev->id_unit, iobase2);

		return(0);
	}

	sc->sc_port = dev->id_iobase;

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = asi_read_reg;
	sc->writereg = asi_write_reg;

	sc->readfifo = asi_read_fifo;
	sc->writefifo = asi_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_ASUSCOMIPAC;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* setup chip type = IPAC ! */
	
	sc->sc_ipac = 1;
	sc->sc_bfifolen = IPAC_BFIFO_LEN;
	
	/* setup ISAC and HSCX base addr */
	
	ISAC_BASE   = (caddr_t) ((u_int)dev->id_iobase | ASI_IDISAC);
	HSCX_A_BASE = (caddr_t) ((u_int)dev->id_iobase | ASI_IDHSCXA);
	HSCX_B_BASE = (caddr_t) ((u_int)dev->id_iobase | ASI_IDHSCXB);
	IPAC_BASE   = (caddr_t) ((u_int)dev->id_iobase | ASI_IDIPAC);

	return (1);
}

int
isic_attach_asi(struct isa_device *dev, unsigned int iobase2)
{
	struct isic_softc *sc = &isic_sc[dev->id_unit];
	/* enable hscx/isac irq's */
#if 0
/*
 * This is for ELSA driver
 */
	IPAC_WRITE(IPAC_MASK, (IPAC_MASK_INT1 | IPAC_MASK_INT0));

	IPAC_WRITE(IPAC_ACFG, 0);	/* outputs are open drain */
	IPAC_WRITE(IPAC_AOE,		/* aux 5..2 are inputs, 7, 6 outputs */
		(IPAC_AOE_OE5 | IPAC_AOE_OE4 | IPAC_AOE_OE3 | IPAC_AOE_OE2));
	IPAC_WRITE(IPAC_ATX, 0xff);	/* set all output lines high */

        outb(dev->id_iobase + 0x4c, 0x41);	/* enable card interrupt */
#endif
/*
 * This has been taken from Linux driver.
 * XXX Figure out bits to use defines as original driver did.
 */
	IPAC_WRITE (IPAC_CONF, 0x0);
	IPAC_WRITE (IPAC_ACFG, 0xff);
	IPAC_WRITE (IPAC_AOE, 0x0);
	IPAC_WRITE (IPAC_MASK, 0xc0);
	IPAC_WRITE (IPAC_PCFG, 0x12);        

	return (1);
}

#else	/* !FreeBSD */

void
isic_attach_asi(psc, pa)
	struct pci_isic_softc *psc;
	struct pci_attach_args *pa;
{
	struct isic_softc *sc = &psc->sc_isic;

	/* setup io mappings */
	sc->sc_num_mappings = 2;
	MALLOC_MAPS(sc);
	sc->sc_maps[0].size = 0;
	if (pci_mapreg_map(pa, ASI_PORT0_MAPOFF, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_maps[0].t, &sc->sc_maps[0].h, NULL, NULL)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}
	sc->sc_maps[1].size = 0;
	if (pci_mapreg_map(pa, ASI_PORT1_MAPOFF, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_maps[1].t, &sc->sc_maps[1].h, NULL, NULL)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = asi_read_reg;
	sc->writereg = asi_write_reg;

	sc->readfifo = asi_read_fifo;
	sc->writefifo = asi_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_ASUSCOMIPAC;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* setup chip type = IPAC ! */
	
	sc->sc_ipac = 1;
	sc->sc_bfifolen = IPAC_BFIFO_LEN;
	
#if 0
/* 
 * This for ELSA card in original driver.
 */
	/* enable hscx/isac irq's */
	IPAC_WRITE(IPAC_MASK, (IPAC_MASK_INT1 | IPAC_MASK_INT0));

	IPAC_WRITE(IPAC_ACFG, 0);	/* outputs are open drain */
	IPAC_WRITE(IPAC_AOE,		/* aux 5..2 are inputs, 7, 6 outputs */
		(IPAC_AOE_OE5 | IPAC_AOE_OE4 | IPAC_AOE_OE3 | IPAC_AOE_OE2));
	IPAC_WRITE(IPAC_ATX, 0xff);	/* set all output lines high */

        bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, 0x4c, 0x41);	/* enable card interrupt */
#endif
/*
 * This has been taken from Linux driver.
 * XXX Figure out bits to use defines as original driver did.
 */
	IPAC_WRITE (IPAC_CONF, 0x0);
	IPAC_WRITE (IPAC_ACFG, 0xff);
	IPAC_WRITE (IPAC_AOE, 0x0);
	IPAC_WRITE (IPAC_MASK, 0xc0);
	IPAC_WRITE (IPAC_PCFG, 0x12);        
}


#endif

#endif /* (NISIC > 0) && defined(ASUSCOM_IPAC) */
