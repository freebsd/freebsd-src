/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>

#include <sys/pciio.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pci_private.h>
#include <dev/pci/pcib_private.h>

#define TASK_QUEUE_INTR 1
#include <dev/vmd/vmd.h>

#include "pcib_if.h"
#include "pci_if.h"

struct vmd_type {
	u_int16_t	vmd_vid;
	u_int16_t	vmd_did;
	char		*vmd_name;
};

#define INTEL_VENDOR_ID		0x8086
#define INTEL_DEVICE_ID_VMD	0x201d
#define INTEL_DEVICE_ID_VMD2	0x28c0

static struct vmd_type vmd_devs[] = {
        { INTEL_VENDOR_ID, INTEL_DEVICE_ID_VMD,  "Intel Volume Management Device" },
        { INTEL_VENDOR_ID, INTEL_DEVICE_ID_VMD2, "Intel Volume Management Device" },
        { 0, 0, NULL }
};

static int
vmd_probe(device_t dev)
{
	struct vmd_type *t;
	uint16_t vid, did;

	t = vmd_devs;
	vid = pci_get_vendor(dev);
	did = pci_get_device(dev);

	while (t->vmd_name != NULL) {
		if (vid == t->vmd_vid &&
			did == t->vmd_did) {
			device_set_desc(dev, t->vmd_name);
			return (BUS_PROBE_DEFAULT);
		}
		t++;
	}

return (ENXIO);
}

static void
vmd_free(struct vmd_softc *sc)
{
	int i;
	struct vmd_irq_handler *elm, *tmp;

	if (sc->vmd_bus.rman.rm_end != 0)
		rman_fini(&sc->vmd_bus.rman);

#ifdef TASK_QUEUE_INTR
	if (sc->vmd_irq_tq != NULL) {
		taskqueue_drain(sc->vmd_irq_tq, &sc->vmd_irq_task);
		taskqueue_free(sc->vmd_irq_tq);
		sc->vmd_irq_tq = NULL;
	}
#endif
	if (sc->vmd_irq != NULL) {
		for (i = 0; i < sc->vmd_msix_count; i++) {
			if (sc->vmd_irq[i].vmd_res != NULL) {
				bus_teardown_intr(sc->vmd_dev,
				    sc->vmd_irq[i].vmd_res,
				    sc->vmd_irq[i].vmd_handle);
				bus_release_resource(sc->vmd_dev, SYS_RES_IRQ,
				    sc->vmd_irq[i].vmd_rid,
				    sc->vmd_irq[i].vmd_res);
			}
		}
		TAILQ_FOREACH_SAFE(elm, &sc->vmd_irq[0].vmd_list ,vmd_link,
		    tmp) {
			TAILQ_REMOVE(&sc->vmd_irq[0].vmd_list, elm, vmd_link);
			free(elm, M_DEVBUF);
		}
	}
	free(sc->vmd_irq, M_DEVBUF);
	sc->vmd_irq = NULL;
	pci_release_msi(sc->vmd_dev);
	for (i = 0; i < VMD_MAX_BAR; i++) {
		if (sc->vmd_regs_resource[i] != NULL)
			bus_release_resource(sc->vmd_dev, SYS_RES_MEMORY,
			    sc->vmd_regs_rid[i],
			    sc->vmd_regs_resource[i]);
	}
	if (sc->vmd_io_resource)
		bus_release_resource(device_get_parent(sc->vmd_dev),
		    SYS_RES_IOPORT, sc->vmd_io_rid, sc->vmd_io_resource);

#ifndef TASK_QUEUE_INTR
	if (mtx_initialized(&sc->vmd_irq_lock)) {
		mtx_destroy(&sc->vmd_irq_lock);
	}
#endif
}

/* Hidden PCI Roots are hidden in BAR(0). */

static uint32_t
vmd_read_config(device_t dev, u_int b, u_int s, u_int f, u_int reg, int width)
{

	struct vmd_softc *sc;
	bus_addr_t offset;

	offset = (b << 20) + (s << 15) + (f << 12) + reg;
	sc = device_get_softc(dev);
	switch(width) {
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
		KASSERT(1, ("Invalid width requested"));
		return (0xffffffff);
	}
}

static void
vmd_write_config(device_t dev, u_int b, u_int s, u_int f, u_int reg,
    uint32_t val, int width)
{

	struct vmd_softc *sc;
	bus_addr_t offset;

	offset = (b << 20) + (s << 15) + (f << 12) + reg;
	sc = device_get_softc(dev);

	switch(width) {
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
		panic("Failed to specific width");
	}
}

static uint32_t
vmd_pci_read_config(device_t dev, device_t child, int reg, int width)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;

	return vmd_read_config(dev, cfg->bus, cfg->slot, cfg->func, reg, width);
}

static void
vmd_pci_write_config(device_t dev, device_t child, int reg, uint32_t val,
    int width)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;

	vmd_write_config(dev, cfg->bus, cfg->slot, cfg->func, reg, val, width);
}

static struct pci_devinfo *
vmd_alloc_devinfo(device_t dev)
{
	struct pci_devinfo *dinfo;

	dinfo = malloc(sizeof(*dinfo), M_DEVBUF, M_WAITOK | M_ZERO);
	return (dinfo);
}

static void
vmd_intr(void *arg)
{
	struct vmd_irq  *irq;
	struct vmd_softc *sc;
#ifndef TASK_QUEUE_INTR
	struct vmd_irq_handler *elm, *tmp_elm;
#endif

	irq = (struct vmd_irq *)arg;
	sc = irq->vmd_sc;
#ifdef TASK_QUEUE_INTR
	taskqueue_enqueue(sc->vmd_irq_tq, &sc->vmd_irq_task);
#else
	mtx_lock(&sc->vmd_irq_lock);
	TAILQ_FOREACH_SAFE(elm, &sc->vmd_irq[0].vmd_list, vmd_link, tmp_elm) {
		(elm->vmd_intr)(elm->vmd_arg);
	}
	mtx_unlock(&sc->vmd_irq_lock);
#endif
}

#ifdef TASK_QUEUE_INTR
static void
vmd_handle_irq(void *context, int pending)
{
	struct vmd_irq_handler *elm, *tmp_elm;
	struct vmd_softc *sc;

	sc = context;

	TAILQ_FOREACH_SAFE(elm, &sc->vmd_irq[0].vmd_list, vmd_link, tmp_elm) {
		(elm->vmd_intr)(elm->vmd_arg);
	}
}
#endif

static int
vmd_attach(device_t dev)
{
	struct vmd_softc *sc;
	struct pcib_secbus *bus;
	uint32_t bar;
	int i, j, error;
	int rid, sec_reg;
	static int b;
	static int s;
	static int f;
	int min_count = 1;
	char buf[64];

	sc = device_get_softc(dev);
	bzero(sc, sizeof(*sc));
	sc->vmd_dev = dev;
	b = s = f = 0;

	pci_enable_busmaster(dev);

#ifdef TASK_QUEUE_INTR
	sc->vmd_irq_tq = taskqueue_create_fast("vmd_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &sc->vmd_irq_tq);
	taskqueue_start_threads(&sc->vmd_irq_tq, 1, PI_DISK, "%s taskq",
            device_get_nameunit(sc->vmd_dev));
	TASK_INIT(&sc->vmd_irq_task, 0, vmd_handle_irq, sc);
#else
	mtx_init(&sc->vmd_irq_lock, "VMD IRQ lock", NULL, MTX_DEF);
#endif
	for (i = 0, j = 0; i < VMD_MAX_BAR; i++, j++ ) {
		sc->vmd_regs_rid[i] = PCIR_BAR(j);
		bar = pci_read_config(dev, PCIR_BAR(0), 4);
		if (PCI_BAR_MEM(bar) && (bar & PCIM_BAR_MEM_TYPE) ==
		    PCIM_BAR_MEM_64)
			j++;
		if ((sc->vmd_regs_resource[i] = bus_alloc_resource_any(
		    sc->vmd_dev, SYS_RES_MEMORY, &sc->vmd_regs_rid[i],
		    RF_ACTIVE)) == NULL) {
			device_printf(dev, "Cannot allocate resources\n");
			goto fail;
		}
	}

	sc->vmd_io_rid = PCIR_IOBASEL_1;
	sc->vmd_io_resource = bus_alloc_resource_any(
	    device_get_parent(sc->vmd_dev), SYS_RES_IOPORT, &sc->vmd_io_rid,
	    RF_ACTIVE);
	if (sc->vmd_io_resource == NULL) {
		device_printf(dev, "Cannot allocate IO\n");
		goto fail;
	}

	sc->vmd_btag = rman_get_bustag(sc->vmd_regs_resource[0]);
	sc->vmd_bhandle = rman_get_bushandle(sc->vmd_regs_resource[0]);

	pci_write_config(dev, PCIR_PRIBUS_2,
	    pcib_get_bus(device_get_parent(dev)), 1);

	sec_reg = PCIR_SECBUS_1;
	bus = &sc->vmd_bus;
	bus->sub_reg = PCIR_SUBBUS_1;
	bus->sec = vmd_read_config(dev, b, s, f, sec_reg, 1);
	bus->sub = vmd_read_config(dev, b, s, f, bus->sub_reg, 1);
	bus->dev = dev;
	bus->rman.rm_start = 0;
	bus->rman.rm_end = PCI_BUSMAX;
	bus->rman.rm_type = RMAN_ARRAY;
	snprintf(buf, sizeof(buf), "%s bus numbers", device_get_nameunit(dev));
	bus->rman.rm_descr = strdup(buf, M_DEVBUF);
	error = rman_init(&bus->rman);
	if (error) {
		device_printf(dev, "Failed to initialize %s bus number rman\n",
		    device_get_nameunit(dev));
		bus->rman.rm_end = 0;
		goto fail;
	}

	/*
	 * Allocate a bus range.  This will return an existing bus range
	 * if one exists, or a new bus range if one does not.
	 */
	rid = 0;
	bus->res = bus_alloc_resource_anywhere(dev, PCI_RES_BUS, &rid,
	    min_count, 0);
	if (bus->res == NULL) {
		/*
		 * Fall back to just allocating a range of a single bus
		 * number.
		 */
		bus->res = bus_alloc_resource_anywhere(dev, PCI_RES_BUS, &rid,
		    1, 0);
	} else if (rman_get_size(bus->res) < min_count) {
		/*
		 * Attempt to grow the existing range to satisfy the
		 * minimum desired count.
		 */
		(void)bus_adjust_resource(dev, PCI_RES_BUS, bus->res,
		    rman_get_start(bus->res), rman_get_start(bus->res) +
		    min_count - 1);
	}

	/*
	 * Add the initial resource to the rman.
	 */
	if (bus->res != NULL) {
		error = rman_manage_region(&bus->rman, rman_get_start(bus->res),
		    rman_get_end(bus->res));
		if (error) {
			device_printf(dev, "Failed to add resource to rman\n");
			goto fail;
		}
		bus->sec = rman_get_start(bus->res);
		bus->sub = rman_get_end(bus->res);
	}

	sc->vmd_msix_count = pci_msix_count(dev);
	if (pci_alloc_msix(dev, &sc->vmd_msix_count) == 0) {
		sc->vmd_irq = malloc(sizeof(struct vmd_irq) *
		    sc->vmd_msix_count,
		    M_DEVBUF, M_WAITOK | M_ZERO);

		for (i = 0; i < sc->vmd_msix_count; i++) {
			sc->vmd_irq[i].vmd_rid = i + 1;
			sc->vmd_irq[i].vmd_sc = sc;
			sc->vmd_irq[i].vmd_instance = i;
			sc->vmd_irq[i].vmd_res = bus_alloc_resource_any(dev,
			    SYS_RES_IRQ, &sc->vmd_irq[i].vmd_rid,
			    RF_ACTIVE);
			if (sc->vmd_irq[i].vmd_res == NULL) {
				device_printf(dev,"Failed to alloc irq\n");
				goto fail;
			}

			TAILQ_INIT(&sc->vmd_irq[i].vmd_list);
			if (bus_setup_intr(dev, sc->vmd_irq[i].vmd_res,
			    INTR_TYPE_MISC | INTR_MPSAFE, NULL, vmd_intr,
			    &sc->vmd_irq[i], &sc->vmd_irq[i].vmd_handle)) {
				device_printf(sc->vmd_dev,
				    "Cannot set up interrupt\n");
				sc->vmd_irq[i].vmd_res = NULL;
				goto fail;
			}
		}
	}

	sc->vmd_child = device_add_child(dev, NULL, -1);
	if (sc->vmd_child == NULL) {
		device_printf(dev, "Failed to attach child\n");
		goto fail;
	}

	error = device_probe_and_attach(sc->vmd_child);
	if (error) {
		device_printf(dev, "Failed to add probe child: %d\n", error);
		(void)device_delete_child(dev, sc->vmd_child);
		goto fail;
	}

	return (0);

fail:
	vmd_free(sc);
	return (ENXIO);
}

static int
vmd_detach(device_t dev)
{
	struct vmd_softc *sc;
	int err;

	sc = device_get_softc(dev);
	if (sc->vmd_child != NULL) {
		err = bus_generic_detach(sc->vmd_child);
		if (err)
			return (err);
		err = device_delete_child(dev, sc->vmd_child);
		if (err)
			return (err);
	}
	vmd_free(sc);
	return (0);
}

/* Pass request to alloc an MSI-X message up to the parent bridge. */
static int
vmd_alloc_msix(device_t pcib, device_t dev, int *irq)
{
	struct vmd_softc *sc = device_get_softc(pcib);
	device_t bus;
	int ret;

	if (sc->vmd_flags & PCIB_DISABLE_MSIX)
		return (ENXIO);
	bus = device_get_parent(pcib);
	ret = PCIB_ALLOC_MSIX(device_get_parent(bus), dev, irq);
        return (ret);
}

static struct resource *
vmd_alloc_resource(device_t dev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	/* Start at max PCI vmd_domain and work down */
	if (type == PCI_RES_BUS) {
		return (pci_domain_alloc_bus(PCI_DOMAINMAX -
		    device_get_unit(dev), child, rid, start, end,
		    count, flags));
	}

	return (pcib_alloc_resource(dev, child, type, rid, start, end,
				    count, flags));
}

static int
vmd_adjust_resource(device_t dev, device_t child, int type,
    struct resource *r, rman_res_t start, rman_res_t end)
{
	struct resource *res = r;

	if (type == PCI_RES_BUS)
		return (pci_domain_adjust_bus(PCI_DOMAINMAX -
			device_get_unit(dev), child, res, start, end));
	return (pcib_adjust_resource(dev, child, type, res, start, end));
}

static int
vmd_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	if (type == PCI_RES_BUS)
		return (pci_domain_release_bus(PCI_DOMAINMAX -
		    device_get_unit(dev), child, rid, r));
	return (pcib_release_resource(dev, child, type, rid, r));
}

static int
vmd_shutdown(device_t dev)
{
	return (0);
}

static int
vmd_pcib_route_interrupt(device_t pcib, device_t dev, int pin)
{
	return (pcib_route_interrupt(pcib, dev, pin));
}

static int
vmd_pcib_alloc_msi(device_t pcib, device_t dev, int count, int maxcount,
    int *irqs)
{
	return (pcib_alloc_msi(pcib, dev, count, maxcount, irqs));
}

static int
vmd_pcib_release_msi(device_t pcib, device_t dev, int count, int *irqs)
{

	return (pcib_release_msi(pcib, dev, count, irqs));
}

static int
vmd_pcib_release_msix(device_t pcib, device_t dev, int irq) {
	return	pcib_release_msix(pcib, dev, irq);
}

static int
vmd_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_filter_t *filter, driver_intr_t *intr, void *arg,
    void **cookiep)
{
	struct vmd_irq_handler *elm;
	struct vmd_softc *sc;
	int i;

	sc = device_get_softc(dev);

	/*
	 * There appears to be no steering of VMD interrupts from device
	 * to VMD interrupt
	 */

	i = 0;
	elm = malloc(sizeof(*elm), M_DEVBUF, M_NOWAIT|M_ZERO);
	elm->vmd_child = child;
	elm->vmd_intr = intr;
	elm->vmd_rid = rman_get_rid(irq);
	elm->vmd_arg = arg;
	TAILQ_INSERT_TAIL(&sc->vmd_irq[i].vmd_list, elm, vmd_link);

	return (bus_generic_setup_intr(dev, child, irq, flags, filter, intr,
	    arg, cookiep));
}

static int
vmd_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	struct vmd_irq_handler *elm, *tmp;;
	struct vmd_softc *sc;

	sc = device_get_softc(dev);
	TAILQ_FOREACH_SAFE(elm, &sc->vmd_irq[0].vmd_list, vmd_link, tmp) {
		if (elm->vmd_child == child &&
		    elm->vmd_rid == rman_get_rid(irq)) {
			TAILQ_REMOVE(&sc->vmd_irq[0].vmd_list, elm, vmd_link);
			free(elm, M_DEVBUF);
		}
	}

	return (bus_generic_teardown_intr(dev, child, irq, cookie));
}

static device_method_t vmd_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			vmd_probe),
	DEVMETHOD(device_attach,		vmd_attach),
	DEVMETHOD(device_detach,		vmd_detach),
	DEVMETHOD(device_shutdown,		vmd_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,		pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,		pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource,		vmd_alloc_resource),
	DEVMETHOD(bus_adjust_resource,		vmd_adjust_resource),
	DEVMETHOD(bus_release_resource,		vmd_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		vmd_setup_intr),
	DEVMETHOD(bus_teardown_intr,		vmd_teardown_intr),

	/* pci interface */
	DEVMETHOD(pci_read_config,		vmd_pci_read_config),
	DEVMETHOD(pci_write_config,		vmd_pci_write_config),
	DEVMETHOD(pci_alloc_devinfo,		vmd_alloc_devinfo),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,		pcib_maxslots),
	DEVMETHOD(pcib_read_config,		vmd_read_config),
	DEVMETHOD(pcib_write_config,		vmd_write_config),
	DEVMETHOD(pcib_route_interrupt,		vmd_pcib_route_interrupt),
	DEVMETHOD(pcib_alloc_msi,		vmd_pcib_alloc_msi),
	DEVMETHOD(pcib_release_msi,		vmd_pcib_release_msi),
	DEVMETHOD(pcib_alloc_msix,		vmd_alloc_msix),
	DEVMETHOD(pcib_release_msix,		vmd_pcib_release_msix),
	DEVMETHOD(pcib_map_msi,			pcib_map_msi),

	DEVMETHOD_END
};

static devclass_t vmd_devclass;

DEFINE_CLASS_0(vmd, vmd_pci_driver, vmd_pci_methods, sizeof(struct vmd_softc));
DRIVER_MODULE(vmd, pci, vmd_pci_driver, vmd_devclass, NULL, NULL);
MODULE_PNP_INFO("U16:vendor;U16:device;D:#", pci, vmd,
    vmd_devs, nitems(vmd_devs) - 1);
MODULE_DEPEND(vmd, vmd_bus, 1, 1, 1);
