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

#include <machine/cache.h>
#include <machine/iommureg.h>
#include <machine/ofw_bus.h>
#include <machine/ver.h>

#include "pcib_if.h"

u_int8_t pci_bus_cnt;
phandle_t *pci_bus_map;
int pci_bus_map_sz;

#define	OPQ_NO_SWIZZLE	1
static struct ofw_pci_quirk {
	char	*opq_model;
	int	opq_quirks;
} ofw_pci_quirks[] = {
	{ "SUNW,Ultra-4", OPQ_NO_SWIZZLE },
	{ "SUNW,Ultra-1-Engine", OPQ_NO_SWIZZLE },
};
#define	OPQ_NENT	(sizeof(ofw_pci_quirks) / sizeof(ofw_pci_quirks[0]))

static int pci_quirks;

#define	OFW_PCI_PCIBUS	"pci"
#define	PCI_BUS_MAP_INC	10

int
ofw_pci_orb_callback(phandle_t node, u_int8_t *pintptr, int pintsz,
    u_int8_t *pregptr, int pregsz, u_int8_t **rintr, int *terminate)
{
	struct ofw_pci_register preg;
	u_int32_t pintr, intr;
	char type[32];

	if (pintsz != sizeof(u_int32_t))
		return (-1);
	bcopy(pintptr, &pintr, sizeof(pintr));
	if ((pci_quirks & OPQ_NO_SWIZZLE) == 0 && pregsz >= sizeof(preg) &&
	    OF_getprop(node, "device_type", type, sizeof(type)) != -1 &&
	    strcmp(type, OFW_PCI_PCIBUS) == 0 && pintr >= 1 && pintr <= 4) {
		/*
		 * Handle a quirk found on some Netra t1 models: there exist
		 * PCI bridges without interrupt maps, where we apparently must
		 * do the PCI swizzle and continue to map on at the parent.
		 */
		bcopy(pregptr, &preg, sizeof(preg));
		intr = (OFW_PCI_PHYS_HI_DEVICE(preg.phys_hi) + pintr + 3) %
		    4 + 1;
		*rintr = malloc(sizeof(intr), M_OFWPROP, 0);
		bcopy(&intr, *rintr, sizeof(intr));
		*terminate = 0;
		return (sizeof(intr));
	}
	return (-1);
}

u_int32_t
ofw_pci_route_intr(phandle_t node, u_int32_t ign)
{
	u_int32_t rv;

	rv = ofw_bus_route_intr(node, ORIP_NOINT, ofw_pci_orb_callback);
	if (rv == ORIR_NOTFOUND)
		return (255);
	/*
	 * Some machines (notably the SPARCengine Ultra AX) have no mappings
	 * at all, but use complete interrupt vector number including the IGN.
	 * Catch this case and remove the IGN.
	 */
	if (rv > ign)
		rv -= ign;
	return (rv);
}

u_int8_t
ofw_pci_alloc_busno(phandle_t node)
{
	phandle_t *om;
	int osz;
	u_int8_t n;

	n = pci_bus_cnt++;
	/* Establish a mapping between bus numbers and device nodes. */
	if (n >= pci_bus_map_sz) {
		osz = pci_bus_map_sz;
		om = pci_bus_map;
		pci_bus_map_sz = n + PCI_BUS_MAP_INC;
		pci_bus_map = malloc(sizeof(*pci_bus_map) * pci_bus_map_sz,
		    M_DEVBUF, M_ZERO);
		if (om != NULL) {
			bcopy(om, pci_bus_map, sizeof(*om) * osz);
			free(om, M_DEVBUF);
		}
	}
	pci_bus_map[n] = node;
	return (n);
}

/*
 * Initialize bridge bus numbers for bridges that implement the primary,
 * secondary and subordinate bus number registers.
 */
void
ofw_pci_binit(device_t busdev, struct ofw_pci_bdesc *obd)
{

#ifdef OFW_PCI_DEBUG
	printf("PCI-PCI bridge at %u/%u/%u: setting bus #s to %u/%u/%u\n",
	    obd->obd_bus, obd->obd_slot, obd->obd_func, obd->obd_bus,
	    obd->obd_secbus, obd->obd_subbus);
#endif /* OFW_PCI_DEBUG */
	PCIB_WRITE_CONFIG(busdev, obd->obd_bus, obd->obd_slot, obd->obd_func,
	    PCIR_PRIBUS_1, obd->obd_bus, 1);
	PCIB_WRITE_CONFIG(busdev, obd->obd_bus, obd->obd_slot, obd->obd_func,
	    PCIR_SECBUS_1, obd->obd_secbus, 1);
	PCIB_WRITE_CONFIG(busdev, obd->obd_bus, obd->obd_slot, obd->obd_func,
	    PCIR_SUBBUS_1, obd->obd_subbus, 1);
}

/*
 * Walk the PCI bus hierarchy, starting with the root PCI bus and descending
 * through bridges, and initialize the interrupt line and latency timer
 * configuration registers of attached devices using firmware information,
 * as well as the the bus numbers and ranges of the bridges.
 */
void
ofw_pci_init(device_t dev, phandle_t bushdl, u_int32_t ign,
    struct ofw_pci_bdesc *obd)
{
	struct ofw_pci_register pcir;
	struct ofw_pci_bdesc subobd, *tobd;
	phandle_t node;
	char type[32];
	int i, intr, freemap;
	u_int slot, busno, func, sub, lat;
	u_int8_t clnsz;

	/* Initialize the quirk list. */
	for (i = 0; i < OPQ_NENT; i++) {
		if (strcmp(sparc64_model, ofw_pci_quirks[i].opq_model) == 0) {
			pci_quirks = ofw_pci_quirks[i].opq_quirks;
			break;
		}
	}

	if ((node = OF_child(bushdl)) == 0)
		return;
	freemap = 0;
	busno = obd->obd_secbus;
	/*
	 * Compute the value to write into the cache line size register.
	 * The role of the streaming cache is unclear in write invalidate
	 * transfers, so it is made sure that it's line size is always reached.
	 */
	clnsz = imax(cache.ec_linesize, STRBUF_LINESZ);
	KASSERT((clnsz / STRBUF_LINESZ) * STRBUF_LINESZ == clnsz &&
	    (clnsz / cache.ec_linesize) * cache.ec_linesize == clnsz &&
	    (clnsz / 4) * 4 == clnsz, ("bogus cache line size %d", clnsz));

	do {
		if (node == -1)
			panic("ofw_pci_init_intr: OF_child failed");
		if (OF_getprop(node, "device_type", type, sizeof(type)) == -1)
			type[0] = '\0';
		else
			type[sizeof(type) - 1] = '\0';
		if (OF_getprop(node, "reg", &pcir, sizeof(pcir)) == -1)
			panic("ofw_pci_init: OF_getprop failed");
		slot = OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi);
		func = OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi);
		if (strcmp(type, OFW_PCI_PCIBUS) == 0) {
			/*
			 * This is a pci-pci bridge, initalize the bus number and
			 * recurse to initialize the child bus. The hierarchy is
			 * usually at most 2 levels deep, so recursion is
			 * feasible.
			 */
			subobd.obd_bus = busno;
			subobd.obd_slot = slot;
			subobd.obd_func = func;
			sub = ofw_pci_alloc_busno(node);
			subobd.obd_secbus = subobd.obd_subbus = sub;
			/* Assume this bridge is mostly standard conforming. */
			subobd.obd_init = ofw_pci_binit;
			subobd.obd_super = obd;
			/*
			 * Need to change all subordinate bus registers of the
			 * bridges above this one now so that configuration
			 * transactions will get through.
			 */
			for (tobd = obd; tobd != NULL; tobd = tobd->obd_super) {
				tobd->obd_subbus = sub;
				tobd->obd_init(dev, tobd);
			}
			subobd.obd_init(dev, &subobd);
#ifdef OFW_PCI_DEBUG
			device_printf(dev, "%s: descending to "
			    "subordinate PCI bus\n", __func__);
#endif /* OFW_PCI_DEBUG */
			ofw_pci_init(dev, node, ign, &subobd);
		} else {
			/*
			 * Initialize the latency timer register for
			 * busmaster devices to work properly. This is another
			 * task which the firmware does not always perform.
			 * The Min_Gnt register can be used to compute it's
			 * recommended value: it contains the desired latency
			 * in units of 1/4 us. To calculate the correct latency
			 * timer value, a bus clock of 33 and no wait states
			 * should be assumed.
			 */
			lat = PCIB_READ_CONFIG(dev, busno, slot, func,
			    PCIR_MINGNT, 1) * 33 / 4;
			if (lat != 0) {
#ifdef OFW_PCI_DEBUG
				printf("device %d/%d/%d: latency timer %d -> "
				    "%d\n", busno, slot, func,
				    PCIB_READ_CONFIG(dev, busno, slot, func,
					PCIR_LATTIMER, 1), lat);
#endif /* OFW_PCI_DEBUG */
				PCIB_WRITE_CONFIG(dev, busno, slot, func,
				    PCIR_LATTIMER, imin(lat, 255), 1);
			}
			PCIB_WRITE_CONFIG(dev, busno, slot, func,
			    PCIR_CACHELNSZ, clnsz / 4, 1);

			/* Initialize the intline registers. */
			if ((intr = ofw_pci_route_intr(node, ign)) != 255) {
#ifdef OFW_PCI_DEBUG
				device_printf(dev, "%s: mapping intr for "
				    "%d/%d/%d to %d (preset was %d)\n",
				    __func__, busno, slot, func, intr,
				    (int)PCIB_READ_CONFIG(dev, busno, slot,
					func, PCIR_INTLINE, 1));
#endif /* OFW_PCI_DEBUG */
				PCIB_WRITE_CONFIG(dev, busno, slot, func,
				    PCIR_INTLINE, intr, 1);
			} else {
#ifdef OFW_PCI_DEBUG
				device_printf(dev, "%s: no interrupt "
				    "mapping found for %d/%d/%d (preset %d)\n",
				    __func__, busno, slot, func,
				    (int)PCIB_READ_CONFIG(dev, busno, slot,
					func, PCIR_INTLINE, 1));
#endif /* OFW_PCI_DEBUG */
				/*
				 * The firmware initializes to 0 instead of
				 * 255.
				 */
				PCIB_WRITE_CONFIG(dev, busno, slot, func,
				    PCIR_INTLINE, 255, 1);
			}
		}
	} while ((node = OF_peer(node)) != 0);
}

phandle_t
ofw_pci_find_node(int bus, int slot, int func)
{
	phandle_t node, bnode;
	struct ofw_pci_register pcir;

	/*
	 * Retrieve the bus node from the mapping that was created on
	 * initialization. The bus numbers the firmware uses cannot be trusted,
	 * so they might have needed to be changed and this is necessary.
	 */
	if (bus >= pci_bus_map_sz)
		return (0);
	bnode = pci_bus_map[bus];
	if (bnode == 0)
		return (0);
	for (node = OF_child(bnode); node != 0 && node != -1;
	     node = OF_peer(node)) {
		if (OF_getprop(node, "reg", &pcir, sizeof(pcir)) == -1)
			continue;
		if (OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi) == slot &&
		    OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi) == func)
			return (node);
	}
	return (0);
}

phandle_t
ofw_pci_node(device_t dev)
{

	return (ofw_pci_find_node(pci_get_bus(dev), pci_get_slot(dev),
	    pci_get_function(dev)));
}
