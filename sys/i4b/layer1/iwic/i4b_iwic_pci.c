/*-
 * Copyright (c) 1999, 2000 Dave Boyce. All rights reserved.
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
 */

/*---------------------------------------------------------------------------
 *
 *      i4b_iwic - isdn4bsd Winbond W6692 driver
 *      ----------------------------------------
 *      last edit-date: [Tue Jan 16 10:53:03 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_i4b.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l3l4.h>

#include <i4b/layer1/i4b_l1.h>

#include <i4b/layer1/iwic/i4b_iwic.h>
#include <i4b/layer1/iwic/i4b_w6692.h>

extern struct i4b_l1mux_func iwic_l1mux_func;

/* Winbond PCI Configuration Space */

#define BADDR0 PCIR_BAR(0)
#define BADDR1 PCIR_BAR(1)


static void iwic_pci_intr(struct iwic_softc *sc);
static int iwic_pci_probe(device_t dev);
static int iwic_pci_attach(device_t dev);

static device_method_t iwic_pci_methods[] =
{
	DEVMETHOD(device_probe,		iwic_pci_probe),
	DEVMETHOD(device_attach,	iwic_pci_attach),
	{ 0, 0 }
};

static driver_t iwic_pci_driver =
{
	"iwic",
	iwic_pci_methods,
	0
};

static devclass_t iwic_pci_devclass;

DRIVER_MODULE(iwic, pci, iwic_pci_driver, iwic_pci_devclass, 0, 0);

#define IWIC_MAXUNIT 4

struct iwic_softc iwic_sc[IWIC_MAXUNIT];

/*---------------------------------------------------------------------------*
 * PCI ID list for ASUSCOM card got from Asuscom in March 2000:
 *
 * Vendor ID: 0675 Device ID: 1702
 * Vendor ID: 0675 Device ID: 1703
 * Vendor ID: 0675 Device ID: 1707
 * Vendor ID: 10CF Device ID: 105E
 * Vendor ID: 1043 Device ID: 0675 SubVendor: 144F SubDevice ID: 2000
 * Vendor ID: 1043 Device ID: 0675 SubVendor: 144F SubDevice ID: 1702
 * Vendor ID: 1043 Device ID: 0675 SubVendor: 144F SubDevice ID: 1707
 * Vendor ID: 1043 Device ID: 0675 SubVendor: 1043 SubDevice ID: 1702
 * Vendor ID: 1043 Device ID: 0675 SubVendor: 1043 SubDevice ID: 1707
 * Vendor ID: 1050 Device ID: 6692 SubVendor: 0675 SubDevice ID: 1702
 * Vendor ID: 1043 Device ID: 0675 SubVendor: 0675 SubDevice ID: 1704
 *---------------------------------------------------------------------------*/

static struct winids {
	u_int32_t type;
	int sv;	
	int sd;	
	const char *desc;
} win_ids[] = {
 { 0x66921050, -1, -1,		"Generic Winbond W6692 ISDN PCI (0x66921050)"      },
 { 0x06751043, 0x0675, 0x1704,	"Planet PCI ISDN Adapter (IA128P-STD) ASUS-HCF675" },
 { 0x66921050, 0x144F, 0x1707,	"Planet PCI ISDN Adapter (Model IA128P-STDV)"      },
 { 0x17020675, -1, -1,		"ASUSCOM P-IN100-ST-D (Winbond W6692, 0x17020675)" },
 { 0x17030675, -1, -1,		"ASUSCOM P-IN100-ST-D (Winbond W6692, 0x17030675)" },
 { 0x17070675, -1, -1,		"ASUSCOM P-IN100-ST-D (Winbond W6692, 0x17070675)" },
 { 0x105e10cf, -1, -1,		"ASUSCOM P-IN100-ST-D (Winbond W6692, 0x105e10cf)" },
 { 0x06751043, 0x144F, 0x2000,	"ASUSCOM P-IN100-ST-D (Winbond W6692, 0x06751043)" },
 { 0x06751043, 0x144F, 0x1702,	"ASUSCOM P-IN100-ST-D (Winbond W6692, 0x06751043)" },
 { 0x06751043, 0x144F, 0x1707,	"ASUSCOM P-IN100-ST-D (Winbond W6692, 0x06751043)" },
 { 0x06751043, 0x1443, 0x1702,	"ASUSCOM P-IN100-ST-D (Winbond W6692, 0x06751043)" },
 { 0x06751043, 0x1443, 0x1707,	"ASUSCOM P-IN100-ST-D (Winbond W6692, 0x06751043)" },
 { 0x06751043, 0x144F, 0x2000,	"ASUSCOM P-IN100-ST-D (Winbond W6692, 0x06751043)" },
 { 0x06751043, 0x144F, 0x2000,	"ASUSCOM P-IN100-ST-D (Winbond W6692, 0x06751043)" },
 { 0x06751043, 0x144F, 0x2000,	"ASUSCOM P-IN100-ST-D (Winbond W6692, 0x06751043)" },
 { 0x00000000, 0, 0, NULL }
};

/*---------------------------------------------------------------------------*
 *	iwic PCI probe
 *---------------------------------------------------------------------------*/
static int
iwic_pci_probe(device_t dev)
{
	u_int32_t type = pci_get_devid(dev);
	u_int32_t sv = pci_get_subvendor(dev);
	u_int32_t sd = pci_get_subdevice(dev);
	
	struct winids *wip = win_ids;

	while(wip->type)
	{
		if(wip->type == type)
		{
			if(((wip->sv == -1) && (wip->sd == -1)) ||
			   ((wip->sv == sv) && (wip->sd == sd)))
				break;
		}
		++wip;
	}

	if(wip->desc)
	{
		if(bootverbose)
		{
			printf("iwic_pci_probe: vendor = 0x%x, device = 0x%x\n", pci_get_vendor(dev), pci_get_device(dev));
			printf("iwic_pci_probe: subvendor = 0x%x, subdevice = 0x%x\n", sv, sd);
		}
		device_set_desc(dev, wip->desc);
		return(0);
	}
	else
	{
		return(ENXIO);
	}
}

/*---------------------------------------------------------------------------*
 *	PCI attach
 *---------------------------------------------------------------------------*/
static int
iwic_pci_attach(device_t dev)
{
	unsigned short iobase;
	struct iwic_softc *sc;
	void *ih = 0;
	int unit = device_get_unit(dev);
	struct iwic_bchan *bchan;
	
	/* check max unit range */
	
	if(unit >= IWIC_MAXUNIT)
	{
		printf("iwic%d: Error, unit %d >= IWIC_MAXUNIT!\n", unit, unit);
		return(ENXIO);	
	}	

	sc = iwic_find_sc(unit);	/* get softc */	
	
	sc->sc_unit = unit;

	/* use the i/o mapped base address */
	
	sc->sc_resources.io_rid[0] = BADDR1;
	
	if(!(sc->sc_resources.io_base[0] =
			bus_alloc_resource_any(dev, SYS_RES_IOPORT,
					       &sc->sc_resources.io_rid[0],
					       RF_ACTIVE)))
	{
		printf("iwic%d: Couldn't alloc io port!\n", unit);
		return(ENXIO);                                       
	}

	iobase = rman_get_start(sc->sc_resources.io_base[0]);

	if(!(sc->sc_resources.irq =
			bus_alloc_resource_any(dev, SYS_RES_IRQ,
					       &sc->sc_resources.irq_rid,
					       RF_SHAREABLE|RF_ACTIVE)))
	{
		printf("iwic%d: Couldn't alloc irq!\n",unit);
		return(ENXIO);                                       
	}
	
	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_WINB6692;
	sc->sc_iobase = (u_int32_t) iobase;
	sc->sc_trace = TRACE_OFF;
	sc->sc_I430state = ST_F3N;	/* Deactivated */
	sc->enabled = FALSE;
	
	if(bus_setup_intr(dev, sc->sc_resources.irq, INTR_TYPE_NET,
				(void(*)(void*))iwic_pci_intr,
				sc, &ih))
	{
		printf("iwic%d: Couldn't set up irq!\n", unit);
		return(ENXIO);
	}

	/* disable interrupts */
	IWIC_WRITE(sc, IMASK, 0xff);

	iwic_dchan_init(sc);

	bchan = &sc->sc_bchan[IWIC_BCH_A];
	bchan->unit = unit;
	bchan->offset = B1_CHAN_OFFSET;
	bchan->channel = IWIC_BCH_A;
	bchan->state = ST_IDLE;
		
	iwic_bchannel_setup(unit, IWIC_BCH_A, BPROT_NONE, 0);

	bchan = &sc->sc_bchan[IWIC_BCH_B];
	bchan->unit = unit;
	bchan->offset = B2_CHAN_OFFSET;
	bchan->channel = IWIC_BCH_B;
	bchan->state = ST_IDLE;
	
	iwic_bchannel_setup(unit, IWIC_BCH_B, BPROT_NONE, 0);

	iwic_init_linktab(sc);
	
	if(bootverbose)
	{
		int ver = IWIC_READ(sc, D_RBCH);
		printf("iwic%d: W6692 chip version = %d\n", unit, D_RBCH_VN(ver));
	}

	i4b_l1_mph_status_ind(L0IWICUNIT(sc->sc_unit), STI_ATTACH, sc->sc_cardtyp, &iwic_l1mux_func);

	IWIC_READ(sc, ISTA);

	/* Enable interrupts */
	IWIC_WRITE(sc, IMASK, IMASK_XINT0 | IMASK_XINT1);

	return(0);
}

/*---------------------------------------------------------------------------*
 *	IRQ handler
 *---------------------------------------------------------------------------*/
static void
iwic_pci_intr(struct iwic_softc *sc)
{
	while (1)
	{
		int irq_stat = IWIC_READ(sc, ISTA);

		if (irq_stat == 0)
			break;

		if (irq_stat & (ISTA_D_RME | ISTA_D_RMR | ISTA_D_XFR))
		{
			iwic_dchan_xfer_irq(sc, irq_stat);
		}
		if (irq_stat & ISTA_D_EXI)
		{
			iwic_dchan_xirq(sc);
		}
		if (irq_stat & ISTA_B1_EXI)
		{
			iwic_bchan_xirq(sc, 0);
		}
		if (irq_stat & ISTA_B2_EXI)
		{
			iwic_bchan_xirq(sc, 1);
		}
	}
}
