/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alexander Motin <mav@FreeBSD.org>
 * Copyright 2019 Cisco Systems, Inc.
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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/intr_machdep.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <sys/pciio.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pci_private.h>
#include <dev/pci/pcib_private.h>

#include <dev/vmd/vmd.h>

#include "pcib_if.h"

struct vmd_type {
	u_int16_t	vmd_vid;
	u_int16_t	vmd_did;
	char		*vmd_name;
	int		flags;
#define BUS_RESTRICT	1
#define VECTOR_OFFSET	2
#define CAN_BYPASS_MSI	4
};

#define VMD_CAP		0x40
#define VMD_BUS_RESTRICT	0x1

#define VMD_CONFIG	0x44
#define VMD_BYPASS_MSI		0x2
#define VMD_BUS_START(x)	((x >> 8) & 0x3)

#define VMD_LOCK	0x70

SYSCTL_NODE(_hw, OID_AUTO, vmd, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Intel Volume Management Device tuning parameters");

/*
 * By default all VMD devices remap children MSI/MSI-X interrupts into their
 * own.  It creates additional isolation, but also complicates things due to
 * sharing, etc.  Fortunately some VMD devices can bypass the remapping.
 */
static int vmd_bypass_msi = 1;
SYSCTL_INT(_hw_vmd, OID_AUTO, bypass_msi, CTLFLAG_RWTUN, &vmd_bypass_msi, 0,
    "Bypass MSI remapping on capable hardware");

/*
 * All MSIs within a group share address, so VMD can't distinguish them.
 * It makes no sense to use more than one per device, only if required by
 * some specific device drivers.
 */
static int vmd_max_msi = 1;
SYSCTL_INT(_hw_vmd, OID_AUTO, max_msi, CTLFLAG_RWTUN, &vmd_max_msi, 0,
    "Maximum number of MSI vectors per device");

/*
 * MSI-X can use different addresses, but we have limited number of MSI-X
 * we can route to, so use conservative default to try to avoid sharing.
 */
static int vmd_max_msix = 3;
SYSCTL_INT(_hw_vmd, OID_AUTO, max_msix, CTLFLAG_RWTUN, &vmd_max_msix, 0,
    "Maximum number of MSI-X vectors per device");

static struct vmd_type vmd_devs[] = {
        { 0x8086, 0x201d, "Intel Volume Management Device", 0 },
        { 0x8086, 0x28c0, "Intel Volume Management Device", BUS_RESTRICT | CAN_BYPASS_MSI },
        { 0x8086, 0x467f, "Intel Volume Management Device", BUS_RESTRICT | VECTOR_OFFSET },
        { 0x8086, 0x4c3d, "Intel Volume Management Device", BUS_RESTRICT | VECTOR_OFFSET },
        { 0x8086, 0x7d0b, "Intel Volume Management Device", BUS_RESTRICT | VECTOR_OFFSET },
        { 0x8086, 0x9a0b, "Intel Volume Management Device", BUS_RESTRICT | VECTOR_OFFSET },
        { 0x8086, 0xa77f, "Intel Volume Management Device", BUS_RESTRICT | VECTOR_OFFSET },
        { 0x8086, 0xad0b, "Intel Volume Management Device", BUS_RESTRICT | VECTOR_OFFSET },
        { 0, 0, NULL, 0 }
};

static int
vmd_probe(device_t dev)
{
	struct vmd_type *t;
	uint16_t vid, did;

	vid = pci_get_vendor(dev);
	did = pci_get_device(dev);
	for (t = vmd_devs; t->vmd_name != NULL; t++) {
		if (vid == t->vmd_vid && did == t->vmd_did) {
			device_set_desc(dev, t->vmd_name);
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

static void
vmd_free(struct vmd_softc *sc)
{
	struct vmd_irq *vi;
	struct vmd_irq_user *u;
	int i;

	if (sc->psc.bus.rman.rm_end != 0)
		rman_fini(&sc->psc.bus.rman);
	if (sc->psc.mem.rman.rm_end != 0)
		rman_fini(&sc->psc.mem.rman);
	while ((u = LIST_FIRST(&sc->vmd_users)) != NULL) {
		LIST_REMOVE(u, viu_link);
		free(u, M_DEVBUF);
	}
	if (sc->vmd_irq != NULL) {
		for (i = 0; i < sc->vmd_msix_count; i++) {
			vi = &sc->vmd_irq[i];
			if (vi->vi_res == NULL)
				continue;
			bus_teardown_intr(sc->psc.dev, vi->vi_res,
			    vi->vi_handle);
			bus_release_resource(sc->psc.dev, SYS_RES_IRQ,
			    vi->vi_rid, vi->vi_res);
		}
	}
	free(sc->vmd_irq, M_DEVBUF);
	sc->vmd_irq = NULL;
	pci_release_msi(sc->psc.dev);
	for (i = 0; i < VMD_MAX_BAR; i++) {
		if (sc->vmd_regs_res[i] != NULL)
			bus_release_resource(sc->psc.dev, SYS_RES_MEMORY,
			    sc->vmd_regs_rid[i], sc->vmd_regs_res[i]);
	}
}

/* Hidden PCI Roots are hidden in BAR(0). */

static uint32_t
vmd_read_config(device_t dev, u_int b, u_int s, u_int f, u_int reg, int width)
{
	struct vmd_softc *sc;
	bus_addr_t offset;

	sc = device_get_softc(dev);
	if (b < sc->vmd_bus_start || b > sc->vmd_bus_end)
		return (0xffffffff);

	offset = ((b - sc->vmd_bus_start) << 20) + (s << 15) + (f << 12) + reg;

	switch (width) {
	case 4:
		return (bus_space_read_4(sc->vmd_btag, sc->vmd_bhandle,
		    offset));
	case 2:
		return (bus_space_read_2(sc->vmd_btag, sc->vmd_bhandle,
		    offset));
	case 1:
		return (bus_space_read_1(sc->vmd_btag, sc->vmd_bhandle,
		    offset));
	default:
		__assert_unreachable();
		return (0xffffffff);
	}
}

static void
vmd_write_config(device_t dev, u_int b, u_int s, u_int f, u_int reg,
    uint32_t val, int width)
{
	struct vmd_softc *sc;
	bus_addr_t offset;

	sc = device_get_softc(dev);
	if (b < sc->vmd_bus_start || b > sc->vmd_bus_end)
		return;

	offset = ((b - sc->vmd_bus_start) << 20) + (s << 15) + (f << 12) + reg;

	switch (width) {
	case 4:
		return (bus_space_write_4(sc->vmd_btag, sc->vmd_bhandle,
		    offset, val));
	case 2:
		return (bus_space_write_2(sc->vmd_btag, sc->vmd_bhandle,
		    offset, val));
	case 1:
		return (bus_space_write_1(sc->vmd_btag, sc->vmd_bhandle,
		    offset, val));
	default:
		__assert_unreachable();
	}
}

static void
vmd_set_msi_bypass(device_t dev, bool enable)
{
	uint16_t val;

	val = pci_read_config(dev, VMD_CONFIG, 2);
	if (enable)
		val |= VMD_BYPASS_MSI;
	else
		val &= ~VMD_BYPASS_MSI;
	pci_write_config(dev, VMD_CONFIG, val, 2);
}

static int
vmd_intr(void *arg)
{
	/*
	 * We have nothing to do here, but we have to register some interrupt
	 * handler to make PCI code setup and enable the MSI-X vector.
	 */
	return (FILTER_STRAY);
}

static int
vmd_attach(device_t dev)
{
	struct vmd_softc *sc;
	struct pcib_secbus *bus;
	struct pcib_window *w;
	struct vmd_type *t;
	struct vmd_irq *vi;
	uint16_t vid, did;
	uint32_t bar;
	int i, j, error;
	char buf[64];

	sc = device_get_softc(dev);
	bzero(sc, sizeof(*sc));
	sc->psc.dev = dev;
	sc->psc.domain = PCI_DOMAINMAX - device_get_unit(dev);

	pci_enable_busmaster(dev);

	for (i = 0, j = 0; i < VMD_MAX_BAR; i++, j++) {
		sc->vmd_regs_rid[i] = PCIR_BAR(j);
		bar = pci_read_config(dev, PCIR_BAR(0), 4);
		if (PCI_BAR_MEM(bar) && (bar & PCIM_BAR_MEM_TYPE) ==
		    PCIM_BAR_MEM_64)
			j++;
		if ((sc->vmd_regs_res[i] = bus_alloc_resource_any(dev,
		    SYS_RES_MEMORY, &sc->vmd_regs_rid[i], RF_ACTIVE)) == NULL) {
			device_printf(dev, "Cannot allocate resources\n");
			goto fail;
		}
	}

	sc->vmd_btag = rman_get_bustag(sc->vmd_regs_res[0]);
	sc->vmd_bhandle = rman_get_bushandle(sc->vmd_regs_res[0]);

	vid = pci_get_vendor(dev);
	did = pci_get_device(dev);
	for (t = vmd_devs; t->vmd_name != NULL; t++) {
		if (vid == t->vmd_vid && did == t->vmd_did)
			break;
	}

	sc->vmd_bus_start = 0;
	if ((t->flags & BUS_RESTRICT) &&
	    (pci_read_config(dev, VMD_CAP, 2) & VMD_BUS_RESTRICT)) {
		switch (VMD_BUS_START(pci_read_config(dev, VMD_CONFIG, 2))) {
		case 0:
			sc->vmd_bus_start = 0;
			break;
		case 1:
			sc->vmd_bus_start = 128;
			break;
		case 2:
			sc->vmd_bus_start = 224;
			break;
		default:
			device_printf(dev, "Unknown bus offset\n");
			goto fail;
		}
	}
	sc->vmd_bus_end = MIN(PCI_BUSMAX, sc->vmd_bus_start +
	    (rman_get_size(sc->vmd_regs_res[0]) >> 20) - 1);

	bus = &sc->psc.bus;
	bus->sec = sc->vmd_bus_start;
	bus->sub = sc->vmd_bus_end;
	bus->dev = dev;
	bus->rman.rm_start = 0;
	bus->rman.rm_end = PCI_BUSMAX;
	bus->rman.rm_type = RMAN_ARRAY;
	snprintf(buf, sizeof(buf), "%s bus numbers", device_get_nameunit(dev));
	bus->rman.rm_descr = strdup(buf, M_DEVBUF);
	error = rman_init(&bus->rman);
	if (error) {
		device_printf(dev, "Failed to initialize bus rman\n");
		bus->rman.rm_end = 0;
		goto fail;
	}
	error = rman_manage_region(&bus->rman, sc->vmd_bus_start,
	    sc->vmd_bus_end);
	if (error) {
		device_printf(dev, "Failed to add resource to bus rman\n");
		goto fail;
	}

	w = &sc->psc.mem;
	w->rman.rm_type = RMAN_ARRAY;
	snprintf(buf, sizeof(buf), "%s memory window", device_get_nameunit(dev));
	w->rman.rm_descr = strdup(buf, M_DEVBUF);
	error = rman_init(&w->rman);
	if (error) {
		device_printf(dev, "Failed to initialize memory rman\n");
		w->rman.rm_end = 0;
		goto fail;
	}
	error = rman_manage_region(&w->rman,
	    rman_get_start(sc->vmd_regs_res[1]),
	    rman_get_end(sc->vmd_regs_res[1]));
	if (error) {
		device_printf(dev, "Failed to add resource to memory rman\n");
		goto fail;
	}
	error = rman_manage_region(&w->rman,
	    rman_get_start(sc->vmd_regs_res[2]) + 0x2000,
	    rman_get_end(sc->vmd_regs_res[2]));
	if (error) {
		device_printf(dev, "Failed to add resource to memory rman\n");
		goto fail;
	}

	LIST_INIT(&sc->vmd_users);
	sc->vmd_fist_vector = (t->flags & VECTOR_OFFSET) ? 1 : 0;
	sc->vmd_msix_count = pci_msix_count(dev);
	if (vmd_bypass_msi && (t->flags & CAN_BYPASS_MSI)) {
		sc->vmd_msix_count = 0;
		vmd_set_msi_bypass(dev, true);
	} else if (pci_alloc_msix(dev, &sc->vmd_msix_count) == 0) {
		sc->vmd_irq = malloc(sizeof(struct vmd_irq) *
		    sc->vmd_msix_count, M_DEVBUF, M_WAITOK | M_ZERO);
		for (i = 0; i < sc->vmd_msix_count; i++) {
			vi = &sc->vmd_irq[i];
			vi->vi_rid = i + 1;
			vi->vi_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
			    &vi->vi_rid, RF_ACTIVE | RF_SHAREABLE);
			if (vi->vi_res == NULL) {
				device_printf(dev, "Failed to allocate irq\n");
				goto fail;
			}
			vi->vi_irq = rman_get_start(vi->vi_res);
			if (bus_setup_intr(dev, vi->vi_res, INTR_TYPE_MISC |
			    INTR_MPSAFE, vmd_intr, NULL, vi, &vi->vi_handle)) {
				device_printf(dev, "Can't set up interrupt\n");
				bus_release_resource(dev, SYS_RES_IRQ,
				    vi->vi_rid, vi->vi_res);
				vi->vi_res = NULL;
				goto fail;
			}
		}
		vmd_set_msi_bypass(dev, false);
	}

	sc->vmd_dma_tag = bus_get_dma_tag(dev);

	sc->psc.child = device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));

fail:
	vmd_free(sc);
	return (ENXIO);
}

static int
vmd_detach(device_t dev)
{
	struct vmd_softc *sc = device_get_softc(dev);
	int error;

	error = bus_generic_detach(dev);
	if (error)
		return (error);
	error = device_delete_children(dev);
	if (error)
		return (error);
	if (sc->vmd_msix_count == 0)
		vmd_set_msi_bypass(dev, false);
	vmd_free(sc);
	return (0);
}

static bus_dma_tag_t
vmd_get_dma_tag(device_t dev, device_t child)
{
	struct vmd_softc *sc = device_get_softc(dev);

	return (sc->vmd_dma_tag);
}

static struct resource *
vmd_alloc_resource(device_t dev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct vmd_softc *sc = device_get_softc(dev);
	struct resource *res;

	switch (type) {
	case SYS_RES_IRQ:
		/* VMD hardware does not support legacy interrupts. */
		if (*rid == 0)
			return (NULL);
		return (bus_generic_alloc_resource(dev, child, type, rid,
		    start, end, count, flags | RF_SHAREABLE));
	case SYS_RES_MEMORY:
		res = rman_reserve_resource(&sc->psc.mem.rman, start, end,
		    count, flags, child);
		if (res == NULL)
			return (NULL);
		if (bootverbose)
			device_printf(dev,
			    "allocated memory range (%#jx-%#jx) for rid %d of %s\n",
			    rman_get_start(res), rman_get_end(res), *rid,
			    pcib_child_name(child));
		break;
	case PCI_RES_BUS:
		res = rman_reserve_resource(&sc->psc.bus.rman, start, end,
		    count, flags, child);
		if (res == NULL)
			return (NULL);
		if (bootverbose)
			device_printf(dev,
			    "allocated bus range (%ju-%ju) for rid %d of %s\n",
			    rman_get_start(res), rman_get_end(res), *rid,
			    pcib_child_name(child));
		break;
	default:
		/* VMD hardware does not support I/O ports. */
		return (NULL);
	}
	rman_set_rid(res, *rid);
	return (res);
}

static int
vmd_adjust_resource(device_t dev, device_t child, int type,
    struct resource *r, rman_res_t start, rman_res_t end)
{

	if (type == SYS_RES_IRQ) {
		return (bus_generic_adjust_resource(dev, child, type, r,
		    start, end));
	}
	return (rman_adjust_resource(r, start, end));
}

static int
vmd_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{

	if (type == SYS_RES_IRQ) {
		return (bus_generic_release_resource(dev, child, type, rid,
		    r));
	}
	return (rman_release_resource(r));
}

static int
vmd_route_interrupt(device_t dev, device_t child, int pin)
{

	/* VMD hardware does not support legacy interrupts. */
	return (PCI_INVALID_IRQ);
}

static int
vmd_alloc_msi(device_t dev, device_t child, int count, int maxcount,
    int *irqs)
{
	struct vmd_softc *sc = device_get_softc(dev);
	struct vmd_irq_user *u;
	int i, ibest = 0, best = INT_MAX;

	if (sc->vmd_msix_count == 0) {
		return (PCIB_ALLOC_MSI(device_get_parent(device_get_parent(dev)),
		    child, count, maxcount, irqs));
	}

	if (count > vmd_max_msi)
		return (ENOSPC);
	LIST_FOREACH(u, &sc->vmd_users, viu_link) {
		if (u->viu_child == child)
			return (EBUSY);
	}

	for (i = sc->vmd_fist_vector; i < sc->vmd_msix_count; i++) {
		if (best > sc->vmd_irq[i].vi_nusers) {
			best = sc->vmd_irq[i].vi_nusers;
			ibest = i;
		}
	}

	u = malloc(sizeof(*u), M_DEVBUF, M_WAITOK | M_ZERO);
	u->viu_child = child;
	u->viu_vector = ibest;
	LIST_INSERT_HEAD(&sc->vmd_users, u, viu_link);
	sc->vmd_irq[ibest].vi_nusers += count;

	for (i = 0; i < count; i++)
		irqs[i] = sc->vmd_irq[ibest].vi_irq;
	return (0);
}

static int
vmd_release_msi(device_t dev, device_t child, int count, int *irqs)
{
	struct vmd_softc *sc = device_get_softc(dev);
	struct vmd_irq_user *u;

	if (sc->vmd_msix_count == 0) {
		return (PCIB_RELEASE_MSI(device_get_parent(device_get_parent(dev)),
		    child, count, irqs));
	}

	LIST_FOREACH(u, &sc->vmd_users, viu_link) {
		if (u->viu_child == child) {
			sc->vmd_irq[u->viu_vector].vi_nusers -= count;
			LIST_REMOVE(u, viu_link);
			free(u, M_DEVBUF);
			return (0);
		}
	}
	return (EINVAL);
}

static int
vmd_alloc_msix(device_t dev, device_t child, int *irq)
{
	struct vmd_softc *sc = device_get_softc(dev);
	struct vmd_irq_user *u;
	int i, ibest = 0, best = INT_MAX;

	if (sc->vmd_msix_count == 0) {
		return (PCIB_ALLOC_MSIX(device_get_parent(device_get_parent(dev)),
		    child, irq));
	}

	i = 0;
	LIST_FOREACH(u, &sc->vmd_users, viu_link) {
		if (u->viu_child == child)
			i++;
	}
	if (i >= vmd_max_msix)
		return (ENOSPC);

	for (i = sc->vmd_fist_vector; i < sc->vmd_msix_count; i++) {
		if (best > sc->vmd_irq[i].vi_nusers) {
			best = sc->vmd_irq[i].vi_nusers;
			ibest = i;
		}
	}

	u = malloc(sizeof(*u), M_DEVBUF, M_WAITOK | M_ZERO);
	u->viu_child = child;
	u->viu_vector = ibest;
	LIST_INSERT_HEAD(&sc->vmd_users, u, viu_link);
	sc->vmd_irq[ibest].vi_nusers++;

	*irq = sc->vmd_irq[ibest].vi_irq;
	return (0);
}

static int
vmd_release_msix(device_t dev, device_t child, int irq)
{
	struct vmd_softc *sc = device_get_softc(dev);
	struct vmd_irq_user *u;

	if (sc->vmd_msix_count == 0) {
		return (PCIB_RELEASE_MSIX(device_get_parent(device_get_parent(dev)),
		    child, irq));
	}

	LIST_FOREACH(u, &sc->vmd_users, viu_link) {
		if (u->viu_child == child &&
		    sc->vmd_irq[u->viu_vector].vi_irq == irq) {
			sc->vmd_irq[u->viu_vector].vi_nusers--;
			LIST_REMOVE(u, viu_link);
			free(u, M_DEVBUF);
			return (0);
		}
	}
	return (EINVAL);
}

static int
vmd_map_msi(device_t dev, device_t child, int irq, uint64_t *addr, uint32_t *data)
{
	struct vmd_softc *sc = device_get_softc(dev);
	int i;

	if (sc->vmd_msix_count == 0) {
		return (PCIB_MAP_MSI(device_get_parent(device_get_parent(dev)),
		    child, irq, addr, data));
	}

	for (i = sc->vmd_fist_vector; i < sc->vmd_msix_count; i++) {
		if (sc->vmd_irq[i].vi_irq == irq)
			break;
	}
	if (i >= sc->vmd_msix_count)
		return (EINVAL);
	*addr = MSI_INTEL_ADDR_BASE | (i << 12);
	*data = 0;
	return (0);
}

static device_method_t vmd_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			vmd_probe),
	DEVMETHOD(device_attach,		vmd_attach),
	DEVMETHOD(device_detach,		vmd_detach),
	DEVMETHOD(device_suspend,		bus_generic_suspend),
	DEVMETHOD(device_resume,		bus_generic_resume),
	DEVMETHOD(device_shutdown,		bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_get_dma_tag,		vmd_get_dma_tag),
	DEVMETHOD(bus_read_ivar,		pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,		pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource,		vmd_alloc_resource),
	DEVMETHOD(bus_adjust_resource,		vmd_adjust_resource),
	DEVMETHOD(bus_release_resource,		vmd_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,		pcib_maxslots),
	DEVMETHOD(pcib_read_config,		vmd_read_config),
	DEVMETHOD(pcib_write_config,		vmd_write_config),
	DEVMETHOD(pcib_route_interrupt,		vmd_route_interrupt),
	DEVMETHOD(pcib_alloc_msi,		vmd_alloc_msi),
	DEVMETHOD(pcib_release_msi,		vmd_release_msi),
	DEVMETHOD(pcib_alloc_msix,		vmd_alloc_msix),
	DEVMETHOD(pcib_release_msix,		vmd_release_msix),
	DEVMETHOD(pcib_map_msi,			vmd_map_msi),
	DEVMETHOD(pcib_request_feature,		pcib_request_feature_allow),

	DEVMETHOD_END
};

static devclass_t pcib_devclass;

DEFINE_CLASS_0(pcib, vmd_pci_driver, vmd_pci_methods, sizeof(struct vmd_softc));
DRIVER_MODULE(vmd, pci, vmd_pci_driver, pcib_devclass, NULL, NULL);
MODULE_PNP_INFO("U16:vendor;U16:device;D:#", pci, vmd,
    vmd_devs, nitems(vmd_devs) - 1);
