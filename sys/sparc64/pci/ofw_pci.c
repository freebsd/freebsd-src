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

/*
 * Find the interrupt-map properties for a node. This might not be a property
 * of the parent, because there may be bridges in between, so go up through the
 * tree to find it.
 * This seems to be only needed for PCI systems, so it has not been moved to
 * ofw_bus.c
 */
int
ofw_pci_find_imap(phandle_t node, struct ofw_pci_imap **imap,
    struct ofw_pci_imap_msk *imapmsk)
{
	int nimap;

	nimap = -1;
	while ((node = OF_parent(node)) != 0) {
		if ((nimap = OF_getprop_alloc(node, "interrupt-map",
		    sizeof(**imap), (void **)imap)) == -1 ||
		    OF_getprop(node, "interrupt-map-mask",
		    imapmsk, sizeof(*imapmsk)) == -1) {
			if (*imap != NULL) {
				free(*imap, M_OFWPROP);
				*imap = NULL;
			}
			nimap = -1;
		} else
			break;
	}
	return (nimap);
}

/*
 * Route an interrupt using the firmware nodes. Returns 255 for interrupts
 * that cannot be routed (suitable for the PCI code).
 */
int
ofw_pci_route_intr2(int intr, struct ofw_pci_register *pcir,
    struct ofw_pci_imap *imap, int nimap, struct ofw_pci_imap_msk *imapmsk)
{
	char regm[12];
	int cintr;

	cintr = ofw_bus_route_intr(intr, pcir, sizeof(*pcir), 12, 1, imap,
	    nimap, imapmsk, regm);
	if (cintr == -1)
		return (255);
	else
		return (cintr);
}

int
ofw_pci_route_intr(phandle_t node, struct ofw_pci_register *pcir,
    struct ofw_pci_imap *intrmap, int nintrmap,
    struct ofw_pci_imap_msk *intrmapmsk)
{
	int intr;

	if (OF_getprop(node, "interrupts", &intr, sizeof(intr)) == -1)
		return (255);

	return (ofw_pci_route_intr2(intr, pcir, intrmap, nintrmap, intrmapmsk));
}

#define	OFW_PCI_PCIBUS	"pci"
/*
 * Walk the PCI bus hierarchy, starting with the root PCI bus and descending
 * through bridges, and initialize the interrupt line configuration registers
 * of attached devices using firmware information.
 */
void
ofw_pci_init_intr(device_t dev, phandle_t bus, struct ofw_pci_imap *intrmap,
    int nintrmap, struct ofw_pci_imap_msk *intrmapmsk)
{
	struct ofw_pci_imap_msk lintrmapmsk;
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
			device_printf(dev, __func__": descending to "
			    "subordinate PCI bus\n");
#endif
			ofw_pci_init_intr(dev, node, NULL, 0, NULL);
		} else {
			if (OF_getprop(node, "reg", &pcir, sizeof(pcir)) == -1)
				panic("ofw_pci_route_intr: OF_getprop failed");
			/*
			 * If we didn't get interrupt map properties passed,
			 * try to find them now. On some systems, buses that
			 * have no non-bridge children have no such properties,
			 * so only try to find them at need.
			 */
			if (intrmap == NULL) {
				nintrmap = OF_getprop_alloc(bus,
				    "interrupt-map", sizeof(*intrmap),
				    (void **)&intrmap);
				if (nintrmap == -1 ||
				    OF_getprop(bus, "interrupt-map-mask",
				    &lintrmapmsk, sizeof(lintrmapmsk)) == -1) {
					panic("ofw_pci_init_intr: could not get "
					    "interrupt map properties");
				}
				intrmapmsk = &lintrmapmsk;
				freemap = 1;
			}
			if ((intr = ofw_pci_route_intr(node, &pcir, intrmap,
			    nintrmap, intrmapmsk)) != 255) {
#ifdef OFW_PCI_DEBUG
				device_printf(dev, __func__": mapping intr for "
				    "%d/%d/%d to %d (preset was %d)\n",
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
				device_printf(dev, __func__": no interrupt "
				    "mapping found for %d/%d/%d (preset %d)\n",
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
	if (freemap)
		free(intrmap, M_OFWPROP);
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
