/*-
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles M. Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * from NetBSD: pci_machdep.c,v 1.18 2001/07/22 11:29:48 wiz Exp
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <powerpc/ofw/ofw_pci.h>

#include "pcib_if.h"

static void	fixup_node(device_t, phandle_t);
static int	find_node_intr(phandle_t, u_int32_t *, u_int32_t *);

phandle_t
ofw_pci_find_node(device_t dev)
{
	phandle_t	node, nextnode;
	struct		ofw_pci_register pcir;
	int		l, b, s, f;

	for (node = OF_peer(0); node; node = nextnode) {
		l = OF_getprop(node, "reg", &pcir, sizeof(pcir));
		if (l > 4) {
			b = OFW_PCI_PHYS_HI_BUS(pcir.phys_hi);
			s = OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi);
			f = OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi);

			if (b == pci_get_bus(dev) && s == pci_get_slot(dev) &&
			    f == pci_get_function(dev))
				return (node);
		}

		if ((nextnode = OF_child(node)))
			continue;
		while (node) {
			if ((nextnode = OF_peer(node)) != 0)
				break;
			node = OF_parent(node);
		}
	}

	return (0);
}

void
ofw_pci_fixup(device_t dev, u_int bus, phandle_t parentnode)
{
	phandle_t	node;

	for (node = OF_child(parentnode); node; node = OF_peer(node)) {
		fixup_node(dev, node);
	}
}

static void
fixup_node(device_t dev, phandle_t node)
{
	u_int32_t	csr, intr, irqs[4], npintr, paddr[4];
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

	if (find_node_intr(node, paddr, irqs) != -1) {
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
