/*-
 * Copyright (c) 2025, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/smp.h>

#include <vm/vm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "ufshci_private.h"

static int ufshci_pci_probe(device_t);
static int ufshci_pci_attach(device_t);
static int ufshci_pci_detach(device_t);

static int ufshci_pci_setup_interrupts(struct ufshci_controller *ctrlr);

static device_method_t ufshci_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ufshci_pci_probe),
	DEVMETHOD(device_attach, ufshci_pci_attach),
	DEVMETHOD(device_detach, ufshci_pci_detach),
	/* TODO: Implement Suspend, Resume */
	{ 0, 0 }
};

static driver_t ufshci_pci_driver = {
	"ufshci",
	ufshci_pci_methods,
	sizeof(struct ufshci_controller),
};

DRIVER_MODULE(ufshci, pci, ufshci_pci_driver, 0, 0);

static struct _pcsid {
	uint32_t devid;
	const char *desc;
	uint32_t ref_clk;
	uint32_t quirks;
} pci_ids[] = { { 0x131b36, "QEMU UFS Host Controller", UFSHCI_REF_CLK_19_2MHz,
		    UFSHCI_QUIRK_IGNORE_UIC_POWER_MODE },
	{ 0x98fa8086, "Intel Lakefield UFS Host Controller",
	    UFSHCI_REF_CLK_19_2MHz,
	    UFSHCI_QUIRK_LONG_PEER_PA_TACTIVATE |
		UFSHCI_QUIRK_WAIT_AFTER_POWER_MODE_CHANGE |
		UFSHCI_QUIRK_CHANGE_LANE_AND_GEAR_SEPARATELY },
	{ 0x54ff8086, "Intel UFS Host Controller", UFSHCI_REF_CLK_19_2MHz },
	{ 0x00000000, NULL } };

static int
ufshci_pci_probe(device_t device)
{
	struct ufshci_controller *ctrlr = device_get_softc(device);
	uint32_t devid = pci_get_devid(device);
	struct _pcsid *ep = pci_ids;

	while (ep->devid && ep->devid != devid)
		++ep;

	if (ep->devid) {
		ctrlr->quirks = ep->quirks;
		ctrlr->ref_clk = ep->ref_clk;
	}

	if (ep->desc) {
		device_set_desc(device, ep->desc);
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
ufshci_pci_allocate_bar(struct ufshci_controller *ctrlr)
{
	ctrlr->resource_id = PCIR_BAR(0);

	ctrlr->resource = bus_alloc_resource_any(ctrlr->dev, SYS_RES_MEMORY,
	    &ctrlr->resource_id, RF_ACTIVE);

	if (ctrlr->resource == NULL) {
		ufshci_printf(ctrlr, "unable to allocate pci resource\n");
		return (ENOMEM);
	}

	ctrlr->bus_tag = rman_get_bustag(ctrlr->resource);
	ctrlr->bus_handle = rman_get_bushandle(ctrlr->resource);
	ctrlr->regs = (struct ufshci_registers *)ctrlr->bus_handle;

	return (0);
}

static int
ufshci_pci_attach(device_t dev)
{
	struct ufshci_controller *ctrlr = device_get_softc(dev);
	int status;

	ctrlr->dev = dev;
	status = ufshci_pci_allocate_bar(ctrlr);
	if (status != 0)
		goto bad;
	pci_enable_busmaster(dev);
	status = ufshci_pci_setup_interrupts(ctrlr);
	if (status != 0)
		goto bad;

	return (ufshci_attach(dev));
bad:
	if (ctrlr->resource != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, ctrlr->resource_id,
		    ctrlr->resource);
	}

	if (ctrlr->tag)
		bus_teardown_intr(dev, ctrlr->res, ctrlr->tag);

	if (ctrlr->res)
		bus_release_resource(dev, SYS_RES_IRQ, rman_get_rid(ctrlr->res),
		    ctrlr->res);

	if (ctrlr->msi_count > 0)
		pci_release_msi(dev);

	return (status);
}

static int
ufshci_pci_detach(device_t dev)
{
	struct ufshci_controller *ctrlr = device_get_softc(dev);
	int error;

	error = ufshci_detach(dev);
	if (ctrlr->msi_count > 0)
		pci_release_msi(dev);
	pci_disable_busmaster(dev);
	return (error);
}

static int
ufshci_pci_setup_shared(struct ufshci_controller *ctrlr, int rid)
{
	int error;

	ctrlr->num_io_queues = 1;
	ctrlr->rid = rid;
	ctrlr->res = bus_alloc_resource_any(ctrlr->dev, SYS_RES_IRQ,
	    &ctrlr->rid, RF_SHAREABLE | RF_ACTIVE);
	if (ctrlr->res == NULL) {
		ufshci_printf(ctrlr, "unable to allocate shared interrupt\n");
		return (ENOMEM);
	}

	error = bus_setup_intr(ctrlr->dev, ctrlr->res,
	    INTR_TYPE_MISC | INTR_MPSAFE, NULL, ufshci_ctrlr_shared_handler,
	    ctrlr, &ctrlr->tag);
	if (error) {
		ufshci_printf(ctrlr, "unable to setup shared interrupt\n");
		return (error);
	}

	return (0);
}

static int
ufshci_pci_setup_interrupts(struct ufshci_controller *ctrlr)
{
	device_t dev = ctrlr->dev;
	int force_intx = 0;
	int num_io_queues, per_cpu_io_queues, min_cpus_per_ioq;
	int num_vectors_requested;

	TUNABLE_INT_FETCH("hw.ufshci.force_intx", &force_intx);
	if (force_intx)
		goto intx;

	if (pci_msix_count(dev) == 0)
		goto msi;

	/*
	 * Try to allocate one MSI-X per core for I/O queues, plus one
	 * for admin queue, but accept single shared MSI-X if have to.
	 * Fall back to MSI if can't get any MSI-X.
	 */

	/*
	 * TODO: Need to implement MCQ(Multi Circular Queue)
	 * Example: num_io_queues = mp_ncpus;
	 */
	num_io_queues = 1;

	TUNABLE_INT_FETCH("hw.ufshci.num_io_queues", &num_io_queues);
	if (num_io_queues < 1 || num_io_queues > mp_ncpus)
		num_io_queues = mp_ncpus;

	per_cpu_io_queues = 1;
	TUNABLE_INT_FETCH("hw.ufshci.per_cpu_io_queues", &per_cpu_io_queues);
	if (per_cpu_io_queues == 0)
		num_io_queues = 1;

	min_cpus_per_ioq = smp_threads_per_core;
	TUNABLE_INT_FETCH("hw.ufshci.min_cpus_per_ioq", &min_cpus_per_ioq);
	if (min_cpus_per_ioq > 1) {
		num_io_queues = min(num_io_queues,
		    max(1, mp_ncpus / min_cpus_per_ioq));
	}

	num_io_queues = min(num_io_queues, max(1, pci_msix_count(dev) - 1));

again:
	if (num_io_queues > vm_ndomains)
		num_io_queues -= num_io_queues % vm_ndomains;
	num_vectors_requested = min(num_io_queues + 1, pci_msix_count(dev));
	ctrlr->msi_count = num_vectors_requested;
	if (pci_alloc_msix(dev, &ctrlr->msi_count) != 0) {
		ufshci_printf(ctrlr, "unable to allocate MSI-X\n");
		ctrlr->msi_count = 0;
		goto msi;
	}
	if (ctrlr->msi_count == 1)
		return (ufshci_pci_setup_shared(ctrlr, 1));
	if (ctrlr->msi_count != num_vectors_requested) {
		pci_release_msi(dev);
		num_io_queues = ctrlr->msi_count - 1;
		goto again;
	}

	ctrlr->num_io_queues = num_io_queues;
	return (0);

msi:
	/*
	 * Try to allocate 2 MSIs (admin and I/O queues), but accept single
	 * shared if have to.  Fall back to INTx if can't get any MSI.
	 */
	ctrlr->msi_count = min(pci_msi_count(dev), 2);
	if (ctrlr->msi_count > 0) {
		if (pci_alloc_msi(dev, &ctrlr->msi_count) != 0) {
			ufshci_printf(ctrlr, "unable to allocate MSI\n");
			ctrlr->msi_count = 0;
		} else if (ctrlr->msi_count == 2) {
			ctrlr->num_io_queues = 1;
			return (0);
		}
	}

intx:
	return (ufshci_pci_setup_shared(ctrlr, ctrlr->msi_count > 0 ? 1 : 0));
}
