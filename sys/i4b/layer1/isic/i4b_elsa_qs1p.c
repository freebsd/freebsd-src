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
 *	isic - I4B Siemens ISDN Chipset Driver for ELSA MicroLink ISDN/PCI
 *	==================================================================
 *
 *	$Id: i4b_elsa_qs1p.c,v 1.4 2000/06/02 11:58:56 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sun Dec  3 17:18:59 2000]
 *
 *	Note: ELSA Quickstep 1000pro PCI = ELSA MicroLink ISDN/PCI
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"
#include "opt_i4b.h"
#include "pci.h"

#if (NISIC > 0) && (NPCI > 0) && defined(ELSA_QS1PCI)

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>


#include <machine/bus.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <machine/i4b_ioctl.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_ipac.h>

#define MEM0_MAPOFF	0
#define PORT0_MAPOFF	4
#define PORT1_MAPOFF	12

#define ELSA_PORT0_MAPOFF	(PCIR_MAPS+PORT0_MAPOFF)
#define ELSA_PORT1_MAPOFF	(PCIR_MAPS+PORT1_MAPOFF)

#define PCI_QS1000_DID	0x1000
#define PCI_QS1000_VID	0x1048

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


static int eqs1p_pci_probe(device_t dev);
static int eqs1p_pci_attach(device_t dev);

static device_method_t eqs1p_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		eqs1p_pci_probe),
	DEVMETHOD(device_attach,	eqs1p_pci_attach),
	{ 0, 0 }
};

static driver_t eqs1p_pci_driver = {
	"isic",
	eqs1p_pci_methods,
	0
};

static devclass_t eqs1p_pci_devclass;

DRIVER_MODULE(eqs1p, pci, eqs1p_pci_driver, eqs1p_pci_devclass, 0, 0);

/*---------------------------------------------------------------------------*
 *      ELSA MicroLink ISDN/PCI fifo read routine
 *---------------------------------------------------------------------------*/
static void
eqs1pp_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[1]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[1]);

	switch(what)
	{
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

/*---------------------------------------------------------------------------*
 *      ELSA MicroLink ISDN/PCI fifo write routine
 *---------------------------------------------------------------------------*/
static void
eqs1pp_write_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[1]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[1]);

	switch(what)
	{
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

/*---------------------------------------------------------------------------*
 *      ELSA MicroLink ISDN/PCI register write routine
 *---------------------------------------------------------------------------*/
static void
eqs1pp_write_reg(struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[1]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[1]);

	switch(what)
	{
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

/*---------------------------------------------------------------------------*
 *	ELSA MicroLink ISDN/PCI register read routine
 *---------------------------------------------------------------------------*/
static u_int8_t
eqs1pp_read_reg(struct l1_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t    t = rman_get_bustag(sc->sc_resources.io_base[1]);
	bus_space_handle_t h = rman_get_bushandle(sc->sc_resources.io_base[1]);

	switch(what)
	{
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
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_IPAC_OFF+offs);
			return bus_space_read_1(t, h, ELSA_OFF_RW);
	}
	return 0;
}

/*---------------------------------------------------------------------------*
 *	avma1pp_probe - probe for a card
 *---------------------------------------------------------------------------*/
static int
eqs1p_pci_probe(device_t dev)
{
	if((pci_get_vendor(dev) == PCI_QS1000_VID) &&
	   (pci_get_device(dev) == PCI_QS1000_DID))
	{
		device_set_desc(dev, "ELSA MicroLink ISDN/PCI");
		return(0);
	}
	return(ENXIO);
}

/*---------------------------------------------------------------------------*
 *	isic_attach_Eqs1pp - attach for ELSA MicroLink ISDN/PCI
 *---------------------------------------------------------------------------*/
static int
eqs1p_pci_attach(device_t dev)
{
	bus_space_tag_t t;
	bus_space_handle_t h;
	struct l1_softc *sc;
	void *ih = 0;
	int unit = device_get_unit(dev);
	
	/* check max unit range */
	
	if(unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for ELSA MicroLink ISDN/PCI!\n",
				unit, unit);
		return(ENXIO);	
	}	

	sc = &l1_sc[unit];		/* get softc */	
	
	sc->sc_unit = unit;

	/* get io_base */

	sc->sc_resources.io_rid[0] = ELSA_PORT0_MAPOFF;
	
	if(!(sc->sc_resources.io_base[0] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
						&sc->sc_resources.io_rid[0],
						0UL, ~0UL, 1, RF_ACTIVE)))
	{
		printf("isic%d: Couldn't get first iobase for ELSA MicroLink ISDN/PCI!\n", unit);
		return(ENXIO);                                       
	}

	sc->sc_resources.io_rid[1] = ELSA_PORT1_MAPOFF;
	
	if(!(sc->sc_resources.io_base[1] =
			bus_alloc_resource(dev, SYS_RES_IOPORT,
						&sc->sc_resources.io_rid[1],
						0UL, ~0UL, 1, RF_ACTIVE)))
	{
		printf("isic%d: Couldn't get second iobase for ELSA MicroLink ISDN/PCI!\n", unit);
		isic_detach_common(dev);
		return(ENXIO);                                       
	}

	sc->sc_port = rman_get_start(sc->sc_resources.io_base[1]);

	if(!(sc->sc_resources.irq =
			bus_alloc_resource(dev, SYS_RES_IRQ,
					   &sc->sc_resources.irq_rid,
					   0UL, ~0UL, 1, RF_ACTIVE | RF_SHAREABLE)))
	{
		printf("isic%d: Could not get irq for ELSA MicroLink ISDN/PCI!\n",unit);
		isic_detach_common(dev);
		return(ENXIO);                                       
	}
	
	sc->sc_irq = rman_get_start(sc->sc_resources.irq);

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

	if(isic_attach_common(dev))
	{
		isic_detach_common(dev);
		return(ENXIO);
	}

	if(bus_setup_intr(dev, sc->sc_resources.irq, INTR_TYPE_NET,
				(void(*)(void*))isicintr,
				sc, &ih))
	{
		printf("isic%d: Couldn't set up irq for ELSA MicroLink ISDN/PCI!\n", unit);
		isic_detach_common(dev);
		return(ENXIO);
	}

	/* enable hscx/isac irq's */

	IPAC_WRITE(IPAC_MASK, (IPAC_MASK_INT1 | IPAC_MASK_INT0));

	IPAC_WRITE(IPAC_ACFG, 0);	/* outputs are open drain */
	IPAC_WRITE(IPAC_AOE,		/* aux 5..2 are inputs, 7, 6 outputs */
		(IPAC_AOE_OE5 | IPAC_AOE_OE4 | IPAC_AOE_OE3 | IPAC_AOE_OE2));
	IPAC_WRITE(IPAC_ATX, 0xff);	/* set all output lines high */

	t = rman_get_bustag(sc->sc_resources.io_base[0]);
	h = rman_get_bushandle(sc->sc_resources.io_base[0]);
	
        bus_space_write_1(t, h, 0x4c, 0x41);	/* enable card interrupt */
        
	return(0);
}

#endif /* (NISIC > 0) && (NPCI > 0) && defined(ELSA_QS1PCI) */
