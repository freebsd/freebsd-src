/*
 * Copyright (c) 1999, 2000 Matthew R. Green
 * All rights reserved.
 * Copyright 2001 by Thomas Moestl <tmm@FreeBSD.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: psycho.c,v 1.35 2001/09/10 16:17:06 eeh Exp
 *
 * $FreeBSD$
 */

#include "opt_ofw_pci.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/openfirm.h>

#include <sparc64/pci/ofw_pci.h>

#include <machine/ofw_bus.h>

#include "pcib_if.h"

u_int32_t
ofw_pci_route_intr(phandle_t node)
{
	u_int32_t rv;

	rv = ofw_bus_route_intr(node, ORIP_NOINT);
	if (rv == ORIR_NOTFOUND)
		return (255);
	return (rv);
}

#define	OFW_PCI_PCIBUS	"pci"
/*
 * Walk the PCI bus hierarchy, starting with the root PCI bus and descending
 * through bridges, and initialize the interrupt line configuration registers
 * of attached devices using firmware information.
 */
void
ofw_pci_init_intr(device_t dev, phandle_t bus)
{
	struct ofw_pci_register pcir;
	phandle_t node;
	char type[32];
	int intr;
	int freemap;

	if ((node = OF_child(bus)) == 0)
		return;
	freemap = 0;
	do {
		if (node == -1)
			panic("ofw_pci_init_intr: OF_child failed");
		if (OF_getprop(node, "device_type", type, sizeof(type)) == -1)
			type[0] = '\0';
		else
			type[sizeof(type) - 1] = '\0';
		if (strcmp(type, OFW_PCI_PCIBUS) == 0) {
			/*
			 * This is a pci-pci bridge, recurse to initialize the
			 * child bus. The hierarchy is usually at most 2 levels
			 * deep, so recursion is feasible.
			 */
#ifdef OFW_PCI_DEBUG
			device_printf(dev, "%s: descending to "
			    "subordinate PCI bus\n", __func__);
#endif
			ofw_pci_init_intr(dev, node);
		} else {
			if (OF_getprop(node, "reg", &pcir, sizeof(pcir)) == -1)
				panic("ofw_pci_route_intr: OF_getprop failed");

			if ((intr = ofw_pci_route_intr(node)) != 255) {
#ifdef OFW_PCI_DEBUG
				device_printf(dev, "%s: mapping intr for "
				    "%d/%d/%d to %d (preset was %d)\n",
				    __func__,
				    OFW_PCI_PHYS_HI_BUS(pcir.phys_hi),
				    OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi),
				    OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi),
				    intr,
				    (int)PCIB_READ_CONFIG(dev,
					OFW_PCI_PHYS_HI_BUS(pcir.phys_hi),
					OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi),
					OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi),
					PCIR_INTLINE, 1));
#endif /* OFW_PCI_DEBUG */
				PCIB_WRITE_CONFIG(dev,
				    OFW_PCI_PHYS_HI_BUS(pcir.phys_hi),
				    OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi),
				    OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi),
				    PCIR_INTLINE, intr, 1);
			} else {
#ifdef OFW_PCI_DEBUG
				device_printf(dev, "%s: no interrupt "
				    "mapping found for %d/%d/%d (preset %d)\n",
				    __func__,
				    OFW_PCI_PHYS_HI_BUS(pcir.phys_hi),
				    OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi),
				    OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi),
				    (int)PCIB_READ_CONFIG(dev,
					OFW_PCI_PHYS_HI_BUS(pcir.phys_hi),
					OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi),
					OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi),
					PCIR_INTLINE, 1));
#endif /* OFW_PCI_DEBUG */
				/* The firmware initializes to 0 instead 255 */
				PCIB_WRITE_CONFIG(dev,
				    OFW_PCI_PHYS_HI_BUS(pcir.phys_hi),
				    OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi),
				    OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi),
				    PCIR_INTLINE, 255, 1);
			}
		}
	} while ((node = OF_peer(node)) != 0);
}

phandle_t
ofw_pci_find_node(int bus, int slot, int func)
{
	phandle_t node, bnode, parent;
	struct ofw_pci_register pcir;
	int br[2];
	char name[16];

	/* 1. Try to find the bus in question. */
	bnode = 0;
	name[sizeof(name) - 1] = '\0';
	parent = OF_peer(0);
	node = OF_child(parent);
	while (node != 0 && node != -1) {
		if (OF_getprop(node, "name", name, sizeof(name) - 1) != -1 &&
		    strcmp(name, "pci") == 0 &&
		    OF_getprop(node, "bus-range", br, sizeof(br)) != -1) {
			/* Found the bus? */
			if (bus == br[0]) {
				bnode = node;
				break;
			}
			/* Need to descend? */
			if (bus > br[0] && bus <= br[1]) {
				parent = node;
				node = OF_child(node);
				continue;
			}
		}
		node = OF_peer(node);
	}
	if (bnode == 0)
		return (0);
	for (node = OF_child(bnode); node != 0 && node != -1;
	     node = OF_peer(node)) {
		if (OF_getprop(node, "reg", &pcir, sizeof(pcir)) == -1)
			continue;
		if (OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi) == slot &&
		    OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi) == func) {
			if (OFW_PCI_PHYS_HI_BUS(pcir.phys_hi) != bus)
				panic("ofw_pci_find_node: bus number mismatch");
			return (node);
		}
	}
	return (0);
}

phandle_t
ofw_pci_node(device_t dev)
{

	return (ofw_pci_find_node(pci_get_bus(dev), pci_get_slot(dev),
	    pci_get_function(dev)));
}
