/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <i386/isa/pcibus.h>
#include <machine/pci_cfgreg.h>

#include "pci_if.h"

static devclass_t	pcib_devclass;

static const char *
nexus_pcib_is_host_bridge(pcicfgregs *cfg,
			  u_int32_t id, u_int8_t class, u_int8_t subclass,
			  u_int8_t *busnum)
{
	const char *s = NULL;
	static u_int8_t pxb[4];	/* hack for 450nx */

	*busnum = 0;

	switch (id) {
	case 0x12258086:
		s = "Intel 824?? host to PCI bridge";
		/* XXX This is a guess */
		/* *busnum = pci_cfgread(cfg, 0x41, 1); */
		*busnum = cfg->bus;
		break;
	case 0x71208086:
		s = "Intel 82810 (i810 GMCH) Host To Hub bridge";
		break;
	case 0x71228086:
		s = "Intel 82810-DC100 (i810-DC100 GMCH) Host To Hub bridge";
		break;
	case 0x71248086:
		s = "Intel 82810E (i810E GMCH) Host To Hub bridge";
		break;
	case 0x71808086:
		s = "Intel 82443LX (440 LX) host to PCI bridge";
		break;
	case 0x71908086:
		s = "Intel 82443BX (440 BX) host to PCI bridge";
		break;
	case 0x71928086:
		s = "Intel 82443BX host to PCI bridge (AGP disabled)";
		break;
	case 0x71948086:
		s = "Intel 82443MX host to PCI bridge";
		break;
	case 0x71a08086:
		s = "Intel 82443GX host to PCI bridge";
		break;
	case 0x71a18086:
		s = "Intel 82443GX host to AGP bridge";
		break;
	case 0x71a28086:
		s = "Intel 82443GX host to PCI bridge (AGP disabled)";
		break;
	case 0x84c48086:
		s = "Intel 82454KX/GX (Orion) host to PCI bridge";
		*busnum = pci_cfgread(cfg, 0x4a, 1);
		break;
	case 0x84ca8086:
		/*
		 * For the 450nx chipset, there is a whole bundle of
		 * things pretending to be host bridges. The MIOC will 
		 * be seen first and isn't really a pci bridge (the
		 * actual busses are attached to the PXB's). We need to 
		 * read the registers of the MIOC to figure out the
		 * bus numbers for the PXB channels.
		 *
		 * Since the MIOC doesn't have a pci bus attached, we
		 * pretend it wasn't there.
		 */
		pxb[0] = pci_cfgread(cfg, 0xd0, 1); /* BUSNO[0] */
		pxb[1] = pci_cfgread(cfg, 0xd1, 1) + 1;	/* SUBA[0]+1 */
		pxb[2] = pci_cfgread(cfg, 0xd3, 1); /* BUSNO[1] */
		pxb[3] = pci_cfgread(cfg, 0xd4, 1) + 1;	/* SUBA[1]+1 */
		return NULL;
	case 0x84cb8086:
		switch (cfg->slot) {
		case 0x12:
			s = "Intel 82454NX PXB#0, Bus#A";
			*busnum = pxb[0];
			break;
		case 0x13:
			s = "Intel 82454NX PXB#0, Bus#B";
			*busnum = pxb[1];
			break;
		case 0x14:
			s = "Intel 82454NX PXB#1, Bus#A";
			*busnum = pxb[2];
			break;
		case 0x15:
			s = "Intel 82454NX PXB#1, Bus#B";
			*busnum = pxb[3];
			break;
		}
		break;

		/* AMD -- vendor 0x1022 */
	case 0x70061022:
		s = "AMD-751 host to PCI bridge";
		break;
	case 0x700e1022:
		s = "AMD-761 host to PCI bridge";
		break;

		/* SiS -- vendor 0x1039 */
	case 0x04961039:
		s = "SiS 85c496";
		break;
	case 0x04061039:
		s = "SiS 85c501";
		break;
	case 0x06011039:
		s = "SiS 85c601";
		break;
	case 0x55911039:
		s = "SiS 5591 host to PCI bridge";
		break;
	case 0x00011039:
		s = "SiS 5591 host to AGP bridge";
		break;

		/* VLSI -- vendor 0x1004 */
	case 0x00051004:
		s = "VLSI 82C592 Host to PCI bridge";
		break;

		/* XXX Here is MVP3, I got the datasheet but NO M/B to test it  */
		/* totally. Please let me know if anything wrong.            -F */
		/* XXX need info on the MVP3 -- any takers? */
	case 0x05981106:
		s = "VIA 82C598MVP (Apollo MVP3) host bridge";
		break;

		/* AcerLabs -- vendor 0x10b9 */
		/* Funny : The datasheet told me vendor id is "10b8",sub-vendor */
		/* id is '10b9" but the register always shows "10b9". -Foxfair  */
	case 0x154110b9:
		s = "AcerLabs M1541 (Aladdin-V) PCI host bridge";
		break;

		/* OPTi -- vendor 0x1045 */
	case 0xc8221045:
		s = "OPTi 82C822 host to PCI Bridge";
		break;

		/* ServerWorks -- vendor 0x1166 */
	case 0x00051166:
		s = "ServerWorks NB6536 2.0HE host to PCI bridge";
		*busnum = pci_cfgread(cfg, 0x44, 1);
		break;
	
	case 0x00061166:
		/* FALLTHROUGH */
	case 0x00081166:
		s = "ServerWorks host to PCI bridge";
		*busnum = pci_cfgread(cfg, 0x44, 1);
		break;

	case 0x00091166:
		s = "ServerWorks NB6635 3.0LE host to PCI bridge";
		*busnum = pci_cfgread(cfg, 0x44, 1);
		break;

		/* Integrated Micro Solutions -- vendor 0x10e0 */
	case 0x884910e0:
		s = "Integrated Micro Solutions VL Bridge";
		break;

	default:
		if (class == PCIC_BRIDGE && subclass == PCIS_BRIDGE_HOST)
			s = "Host to PCI bridge";
		break;
	}

	return s;
}

/*
 * Scan the first pci bus for host-pci bridges and add pcib instances
 * to the nexus for each bridge.
 */
static void
nexus_pcib_identify(driver_t *driver, device_t parent)
{
	pcicfgregs probe;
	u_int8_t  hdrtype;
	int found = 0;
	int pcifunchigh;
	int found824xx = 0;
	int found_orion = 0;

	if (pci_cfgregopen() == 0)
		return;
	probe.hose = 0;
	probe.bus = 0;
 retry:
	for (probe.slot = 0; probe.slot <= PCI_SLOTMAX; probe.slot++) {
		probe.func = 0;
		hdrtype = pci_cfgread(&probe, PCIR_HEADERTYPE, 1);
		if (hdrtype & PCIM_MFDEV && (!found_orion || hdrtype != 0xff) )
			pcifunchigh = 7;
		else
			pcifunchigh = 0;
		for (probe.func = 0;
		     probe.func <= pcifunchigh;
		     probe.func++) {
			/*
			 * Read the IDs and class from the device.
			 */
			u_int32_t id;
			u_int8_t class, subclass, busnum;
			device_t child;
			const char *s;

			id = pci_cfgread(&probe, PCIR_DEVVENDOR, 4);
			if (id == -1)
				continue;
			class = pci_cfgread(&probe, PCIR_CLASS, 1);
			subclass = pci_cfgread(&probe, PCIR_SUBCLASS, 1);

			s = nexus_pcib_is_host_bridge(&probe, id,
						      class, subclass,
						      &busnum);
			if (s) {
				/*
				 * Add at priority 100 to make sure we
				 * go after any motherboard resources
				 */
				child = BUS_ADD_CHILD(parent, 100,
						      "pcib", busnum);
				device_set_desc(child, s);
				found = 1;
				if (id == 0x12258086)
					found824xx = 1;
				if (id == 0x84c48086)
					found_orion = 1;
			}
		}
	}
	if (found824xx && probe.bus == 0) {
		probe.bus++;
		goto retry;
	}

	/*
	 * Make sure we add at least one bridge since some old
	 * hardware doesn't actually have a host-pci bridge device.
	 * Note that pci_cfgregopen() thinks we have PCI devices..
	 */
	if (!found) {
		if (bootverbose)
			printf(
	"nexus_pcib_identify: no bridge found, adding pcib0 anyway\n");
		BUS_ADD_CHILD(parent, 100, "pcib", 0);
	}
}

static int
nexus_pcib_probe(device_t dev)
{
	if (pci_cfgregopen() != 0) {
		device_add_child(dev, "pci", device_get_unit(dev));
		return 0;
	}
	return ENXIO;
}

/* route interrupt */

static int
nexus_pcib_route_interrupt(device_t pcib, device_t dev, int pin)
{
	return(pci_cfgintr(pci_get_bus(dev), pci_get_slot(dev), pin));
}

static device_method_t nexus_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	nexus_pcib_identify),
	DEVMETHOD(device_probe,		nexus_pcib_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* pci interface */
	DEVMETHOD(pci_route_interrupt,	nexus_pcib_route_interrupt),
	{ 0, 0 }
};

static driver_t nexus_pcib_driver = {
	"pcib",
	nexus_pcib_methods,
	1,
};

DRIVER_MODULE(pcib, nexus, nexus_pcib_driver, pcib_devclass, 0, 0);
