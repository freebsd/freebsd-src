/*-
 * Copyright (c) 2001 by Thomas Moestl <tmm@FreeBSD.org>.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Some OpenFirmware helper functions that are likely machine dependent.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <net/ethernet.h>

#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/idprom.h>
#include <machine/ofw_machdep.h>
#include <machine/ofw_upa.h>
#include <machine/resource.h>

#include <sparc64/pci/ofw_pci.h>
#include <sparc64/isa/ofw_isa.h>
#include <sparc64/sbus/ofw_sbus.h>

void
OF_getetheraddr(device_t dev, u_char *addr)
{
	phandle_t node;
	struct idprom idp;

	node = OF_peer(0);
	if (node <= 0 || OF_getprop(node, "idprom", &idp, sizeof(idp)) == -1)
		panic("Could not determine the machine ethernet address");
	bcopy(&idp.id_ether, addr, ETHER_ADDR_LEN);
}

int
OF_decode_addr(phandle_t node, int *space, bus_addr_t *addr)
{
	char name[32];
	union {
		struct isa_ranges isa[4];
		struct sbus_ranges sbus[8];
		struct upa_ranges upa[4];
	} range;
	union {
		struct isa_regs isa;
		struct sbus_regs sbus;
	} reg;
	phandle_t bus, pbus;
	u_long child, dummy, phys;
	int cs, i, rsz, type;

	bus = OF_parent(node);
	if (bus == NULL)
		return (ENXIO);
	if (OF_getprop(bus, "name", name, sizeof(name)) == -1)
		return (ENXIO);
	name[sizeof(name) - 1] = '\0';
	if (strcmp(name, "ebus") == 0) {
		if (OF_getprop(node, "reg", &reg.isa, sizeof(reg.isa)) == -1)
			return (ENXIO);
		rsz = OF_getprop(bus, "ranges", range.isa, sizeof(range.isa));
		if (rsz == -1)
			return (ENXIO);
		phys = ISA_REG_PHYS(&reg.isa);
		dummy = phys + 1;
		type = ofw_isa_map_iorange(range.isa, rsz / sizeof(*range.isa),
		    &phys, &dummy);
		if (type == SYS_RES_MEMORY) {
			cs = PCI_CS_MEM32;
			*space = PCI_MEMORY_BUS_SPACE;
		} else {
			cs = PCI_CS_IO;
			*space = PCI_IO_BUS_SPACE;
		}

		/* Find the topmost PCI node (the host bridge) */
		while (1) {
			pbus = OF_parent(bus);
			if (pbus == NULL)
				return (ENXIO);
			if (OF_getprop(pbus, "name", name, sizeof(name)) == -1)
				return (ENXIO);
			name[sizeof(name) - 1] = '\0';
			if (strcmp(name, "pci") != 0)
				break;
			bus = pbus;
		}

		/* There wasn't a PCI bridge. */
		if (bus == OF_parent(node))
			return (ENXIO);

		/* Make sure we reached the UPA/PCI node. */
		if (OF_getprop(pbus, "device_type", name, sizeof(name)) == -1)
			return (ENXIO);
		name[sizeof(name) - 1] = '\0';
		if (strcmp(name, "upa") != 0)
			return (ENXIO);

		rsz = OF_getprop(bus, "ranges", range.upa, sizeof(range.upa));
		if (rsz == -1)
			return (ENXIO);
		for (i = 0; i < (rsz / sizeof(range.upa[0])); i++) {
			child = UPA_RANGE_CHILD(&range.upa[i]);
			if (UPA_RANGE_CS(&range.upa[i]) == cs &&
			    phys >= child &&
			    phys - child < UPA_RANGE_SIZE(&range.upa[i])) {
				*addr = UPA_RANGE_PHYS(&range.upa[i]) + phys;
				return (0);
			}
		}
	} else if (strcmp(name, "sbus") == 0) {
		if (OF_getprop(node, "reg", &reg.sbus, sizeof(reg.sbus)) == -1)
			return (ENXIO);
		rsz = OF_getprop(bus, "ranges", range.sbus,
		    sizeof(range.sbus));
		if (rsz == -1)
			return (ENXIO);
		for (i = 0; i < (rsz / sizeof(range.sbus[0])); i++) {
			if (reg.sbus.sbr_slot != range.sbus[i].cspace)
				continue;
			if (reg.sbus.sbr_offset < range.sbus[i].coffset ||
			    reg.sbus.sbr_offset >= range.sbus[i].coffset +
			    range.sbus[i].size)
				continue;
			/* Found it... */
			phys = range.sbus[i].poffset |
			    ((bus_addr_t)range.sbus[i].pspace << 32);
			phys += reg.sbus.sbr_offset - range.sbus[i].coffset;
			*addr = phys;
			*space = SBUS_BUS_SPACE;
			return (0);
		}
	}
	return (ENXIO);
}
