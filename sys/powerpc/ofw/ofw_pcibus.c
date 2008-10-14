/*-
 * Copyright (c) 1994 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000, BSDi
 * Copyright (c) 2003, Thomas Moestl <tmm@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/module.h>
#include <sys/pciio.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#include "pcib_if.h"
#include "pci_if.h"

/* Helper functions */
static int  find_node_intr(phandle_t, u_int32_t *, u_int32_t *);
static int  ofw_pci_find_intline(phandle_t node, uint32_t *irqs);
static void ofw_pci_fixup_node(device_t dev, phandle_t node);

/* Methods */
static device_probe_t ofw_pcibus_probe;
static device_attach_t ofw_pcibus_attach;
static pci_assign_interrupt_t ofw_pcibus_assign_interrupt;
static ofw_bus_get_devinfo_t ofw_pcibus_get_devinfo;

static device_method_t ofw_pcibus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ofw_pcibus_probe),
	DEVMETHOD(device_attach,	ofw_pcibus_attach),

	/* PCI interface */
	DEVMETHOD(pci_assign_interrupt, ofw_pcibus_assign_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	ofw_pcibus_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	{ 0, 0 }
};

struct ofw_pcibus_devinfo {
	struct pci_devinfo	opd_dinfo;
	struct ofw_bus_devinfo	opd_obdinfo;
};

static devclass_t pci_devclass;

DEFINE_CLASS_1(pci, ofw_pcibus_driver, ofw_pcibus_methods, 1 /* no softc */,
    pci_driver);
DRIVER_MODULE(ofw_pcibus, pcib, ofw_pcibus_driver, pci_devclass, 0, 0);
MODULE_VERSION(ofw_pcibus, 1);
MODULE_DEPEND(ofw_pcibus, pci, 1, 1, 1);

static int
ofw_pcibus_probe(device_t dev)
{
	if (ofw_bus_get_node(dev) == 0)
		return (ENXIO);
	device_set_desc(dev, "OFW PCI bus");

	return (0);
}

static int
ofw_pcibus_attach(device_t dev)
{
	device_t pcib;
	struct ofw_pci_register pcir;
	struct ofw_pcibus_devinfo *dinfo;
	phandle_t node, child;
	u_int busno, domain, func, slot;

	pcib = device_get_parent(dev);
	domain = pcib_get_domain(dev);
	busno = pcib_get_bus(dev);
	if (bootverbose)
		device_printf(dev, "domain=%d, physical bus=%d\n",
		    domain, busno);
	node = ofw_bus_get_node(dev);

	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if (OF_getprop(child, "reg", &pcir, sizeof(pcir)) == -1)
			continue;
		slot = OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi);
		func = OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi);

		/* Some OFW device trees contain dupes. */
		if (pci_find_dbsf(domain, busno, slot, func) != NULL)
			continue;

		ofw_pci_fixup_node(pcib, child);

		dinfo = (struct ofw_pcibus_devinfo *)pci_read_device(pcib,
		    domain, busno, slot, func, sizeof(*dinfo));

		if (dinfo == NULL)
			continue;

		/* Set up OFW devinfo */
		if (ofw_bus_gen_setup_devinfo(&dinfo->opd_obdinfo, child) !=
		    0) {
			pci_freecfg((struct pci_devinfo *)dinfo);
			continue;
		}

		pci_add_child(dev, (struct pci_devinfo *)dinfo);

		/*
		 * Some devices don't have an intpin set, but do have
		 * interrupts. Add them to the appropriate resource list.
		 */
		if (dinfo->opd_dinfo.cfg.intpin == 0) {
			uint32_t irqs[4];

			if (ofw_pci_find_intline(child, irqs) > 0)
				resource_list_add(&dinfo->opd_dinfo.resources, 
				    SYS_RES_IRQ, 0, irqs[0], irqs[0], 1);
		}
	}

	return (bus_generic_attach(dev));
}

static int
ofw_pcibus_assign_interrupt(device_t dev, device_t child)
{
	uint32_t irqs[4];

	device_printf(child,"Assigning interrupt\n");

	if (ofw_pci_find_intline(ofw_bus_get_node(child), irqs) < 0)
		return PCI_INVALID_IRQ;

	device_printf(child,"IRQ %d\n",irqs[0]);

	return irqs[0];

//	return (PCIB_ROUTE_INTERRUPT(device_get_parent(dev), child, intr));
}

static const struct ofw_bus_devinfo *
ofw_pcibus_get_devinfo(device_t bus, device_t dev)
{
	struct ofw_pcibus_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (&dinfo->opd_obdinfo);
}

static void
ofw_pci_fixup_node(device_t dev, phandle_t node)
{
	uint32_t	csr, intr, irqs[4];
	struct		ofw_pci_register addr[8];
	int		len, i;

	len = OF_getprop(node, "assigned-addresses", addr, sizeof(addr));
	if (len < (int)sizeof(struct ofw_pci_register)) {
		return;
	}

	csr = PCIB_READ_CONFIG(dev, OFW_PCI_PHYS_HI_BUS(addr[0].phys_hi),
	    OFW_PCI_PHYS_HI_DEVICE(addr[0].phys_hi),
	    OFW_PCI_PHYS_HI_FUNCTION(addr[0].phys_hi), PCIR_COMMAND, 4);
	csr &= ~(PCIM_CMD_PORTEN | PCIM_CMD_MEMEN);

	for (i = 0; i < len / sizeof(struct ofw_pci_register); i++) {
		switch (addr[i].phys_hi & OFW_PCI_PHYS_HI_SPACEMASK) {
		case OFW_PCI_PHYS_HI_SPACE_IO:
			csr |= PCIM_CMD_PORTEN;
			break;
		case OFW_PCI_PHYS_HI_SPACE_MEM32:
			csr |= PCIM_CMD_MEMEN;
			break;
		}
	}

	PCIB_WRITE_CONFIG(dev, OFW_PCI_PHYS_HI_BUS(addr[0].phys_hi),
	    OFW_PCI_PHYS_HI_DEVICE(addr[0].phys_hi),
	    OFW_PCI_PHYS_HI_FUNCTION(addr[0].phys_hi), PCIR_COMMAND, csr, 4);

	if (ofw_pci_find_intline(node, irqs) != -1) {
		intr = PCIB_READ_CONFIG(dev,
		    OFW_PCI_PHYS_HI_BUS(addr[0].phys_hi),
		    OFW_PCI_PHYS_HI_DEVICE(addr[0].phys_hi),
		    OFW_PCI_PHYS_HI_FUNCTION(addr[0].phys_hi), PCIR_INTLINE, 2);
		intr &= ~(0xff);
		intr |= irqs[0] & 0xff;
		PCIB_WRITE_CONFIG(dev,
		    OFW_PCI_PHYS_HI_BUS(addr[0].phys_hi),
		    OFW_PCI_PHYS_HI_DEVICE(addr[0].phys_hi),
		    OFW_PCI_PHYS_HI_FUNCTION(addr[0].phys_hi), PCIR_INTLINE,
		    intr, 2);
	}
}

static int
ofw_pci_find_intline(phandle_t node, uint32_t *irqs)
{
	uint32_t	npintr, paddr[4];
	struct		ofw_pci_register addr[8];
	int		len;

	len = OF_getprop(node, "assigned-addresses", addr, sizeof(addr));
	if (len < (int)sizeof(struct ofw_pci_register)) 
		return -1;
	/*
	 * Create PCI interrupt-map array element. pci-mid/pci-lo
	 * aren't required, but the 'interrupts' property needs
	 * to be appended
	 */
	npintr = 0;
	OF_getprop(node, "interrupts", &npintr, 4);
	paddr[0] = addr[0].phys_hi;
	paddr[1] = 0;
	paddr[2] = 0;
	paddr[3] = npintr;

	return find_node_intr(node, paddr, irqs);
}

static int
find_node_intr(phandle_t node, u_int32_t *addr, u_int32_t *intr)
{
	phandle_t	parent, iparent;
	int		len, mlen, match, i;
	u_int32_t	map[160], *mp, imask[8], maskedaddr[8], icells;
	char		name[32];

	len = OF_getprop(node, "AAPL,interrupts", intr, 4);
	if (len == 4) {
		return (len);
	}

	parent = OF_parent(node);
	len = OF_getprop(parent, "interrupt-map", map, sizeof(map));
	mlen = OF_getprop(parent, "interrupt-map-mask", imask, sizeof(imask));

	if (len == -1 || mlen == -1)
		goto nomap;

	memcpy(maskedaddr, addr, mlen);
	for (i = 0; i < mlen/4; i++)
		maskedaddr[i] &= imask[i];

	mp = map;
	while (len > mlen) {
		match = bcmp(maskedaddr, mp, mlen);
		mp += mlen / 4;
		len -= mlen;

		/*
		 * We must read "#interrupt-cells" for each time because
		 * interrupt-parent may be different.
		 */
		iparent = *mp++;
		len -= 4;
		if (OF_getprop(iparent, "#interrupt-cells", &icells, 4) != 4)
			goto nomap;

		/* Found. */
		if (match == 0) {
			bcopy(mp, intr, icells * 4);
			return (icells * 4);
		}

		mp += icells;
		len -= icells * 4;
	}

nomap:
	/*
	 * Check for local properties indicating interrupts
	 */

	len = OF_getprop(node, "interrupts", intr, 16);
	if (OF_getprop(node, "interrupt-parent", &iparent, sizeof(iparent)) ==
	   sizeof(iparent)) {
		OF_getprop(iparent, "#interrupt-cells", &icells, sizeof(icells));
		for (i = 0; i < len/icells/4; i++)
			intr[i] = intr[i*icells];

		return (len);
	}
	

	/*
	 * If the node has no interrupt property and the parent is a PCI
	 * bridge, use the parent's interrupt.  This occurs on a PCI slot.
	 */
	bzero(name, sizeof(name));
	OF_getprop(parent, "name", name, sizeof(name));
	if (strcmp(name, "pci-bridge") == 0) {
		len = OF_getprop(parent, "AAPL,interrupts", intr, 4);
		if (len == 4) {
			return (len);
		}

		/*
		 * XXX I don't know what is the correct local address.
		 * XXX Use the first entry for now.
		 */
		len = OF_getprop(parent, "interrupt-map", map, sizeof(map));
		if (len >= 36) {
			addr = &map[5];
			/* XXX Use 0 for 'interrupts' for compat */
			return (find_node_intr(parent, addr, intr));
		}
	}

	return (-1);
}

