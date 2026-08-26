/*-
 * Copyright (c) 2026, Samsung Electronics Co., Ltd.
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

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>

#include "ufshci_private.h"

static int ufshci_acpi_probe(device_t);
static int ufshci_acpi_attach(device_t);
static int ufshci_acpi_detach(device_t);
static int ufshci_acpi_suspend(device_t);
static int ufshci_acpi_resume(device_t);

static device_method_t ufshci_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ufshci_acpi_probe),
	DEVMETHOD(device_attach, ufshci_acpi_attach),
	DEVMETHOD(device_detach, ufshci_acpi_detach),
	DEVMETHOD(device_suspend, ufshci_acpi_suspend),
	DEVMETHOD(device_resume, ufshci_acpi_resume), { 0, 0 }
};

static driver_t ufshci_acpi_driver = {
	"ufshci",
	ufshci_acpi_methods,
	sizeof(struct ufshci_controller),
};

DRIVER_MODULE(ufshci, acpi, ufshci_acpi_driver, 0, 0);
MODULE_DEPEND(ufshci, acpi, 1, 1, 1);

static struct ufshci_acpi_device {
	const char *hid;
	const char *desc;
	uint32_t ref_clk;
	uint32_t quirks;
} ufshci_acpi_devices[] = {
	{ "QCOM24A5", "Qualcomm Snapdragon X Elite UFS Host Controller",
	    UFSHCI_REF_CLK_19_2MHz,
	    UFSHCI_QUIRK_REINIT_AFTER_MAX_GEAR_SWITCH |
			UFSHCI_QUIRK_BROKEN_LSDBS_MCQS_CAP },
	{ 0x00000000, NULL, 0, 0 }
};

static char *ufshci_acpi_ids[] = { "QCOM24A5", NULL };

static const struct ufshci_acpi_device *
ufshci_acpi_find_device(device_t dev)
{
	char *hid;
	int i;
	int rv;

	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, ufshci_acpi_ids, &hid);
	if (rv > 0)
		return (NULL);

	for (i = 0; ufshci_acpi_devices[i].hid != NULL; i++) {
		if (strcmp(ufshci_acpi_devices[i].hid, hid) != 0)
			continue;
		return (&ufshci_acpi_devices[i]);
	}

	return (NULL);
}

static int
ufshci_acpi_probe(device_t dev)
{
	struct ufshci_controller *ctrlr = device_get_softc(dev);
	const struct ufshci_acpi_device *acpi_dev;

	acpi_dev = ufshci_acpi_find_device(dev);
	if (acpi_dev == NULL)
		return (ENXIO);

	if (acpi_dev->hid) {
		ctrlr->quirks = acpi_dev->quirks;
		ctrlr->ref_clk = acpi_dev->ref_clk;
	}

	if (acpi_dev->desc) {
		device_set_desc(dev, acpi_dev->desc);
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
ufshci_acpi_allocate_memory(struct ufshci_controller *ctrlr)
{
	ctrlr->resource_id = 0;
	ctrlr->resource = bus_alloc_resource_any(ctrlr->dev, SYS_RES_MEMORY,
	    &ctrlr->resource_id, RF_ACTIVE);

	if (ctrlr->resource == NULL) {
		ufshci_printf(ctrlr, "unable to allocate acpi resource\n");
		return (ENOMEM);
	}

	ctrlr->bus_tag = rman_get_bustag(ctrlr->resource);
	ctrlr->bus_handle = rman_get_bushandle(ctrlr->resource);
	ctrlr->regs = (struct ufshci_registers *)ctrlr->bus_handle;

	return (0);
}

static int
ufshci_acpi_setup_shared(struct ufshci_controller *ctrlr)
{
	int error;

	ctrlr->num_io_queues = 1;
	ctrlr->rid = 0;
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
ufshci_acpi_setup_interrupts(struct ufshci_controller *ctrlr)
{
	int num_io_queues, per_cpu_io_queues, min_cpus_per_ioq;

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

	if (num_io_queues > vm_ndomains)
		num_io_queues -= num_io_queues % vm_ndomains;

	ctrlr->num_io_queues = num_io_queues;
	return (ufshci_acpi_setup_shared(ctrlr));
}

static int
ufshci_acpi_attach(device_t dev)
{
	struct ufshci_controller *ctrlr = device_get_softc(dev);
	int status;

	ctrlr->dev = dev;
	status = ufshci_acpi_allocate_memory(ctrlr);
	if (status != 0)
		goto bad;

	status = ufshci_acpi_setup_interrupts(ctrlr);
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

	return (status);
}

static int
ufshci_acpi_detach(device_t dev)
{
	return (ufshci_detach(dev));
}

static int
ufshci_acpi_suspend(device_t dev)
{
	struct ufshci_controller *ctrlr = device_get_softc(dev);
	int error;

	error = bus_generic_suspend(dev);
	if (error)
		return (error);

	/* Currently, PCI-based ufshci only supports POWER_STYPE_STANDBY */
	error = ufshci_ctrlr_suspend(ctrlr, POWER_STYPE_STANDBY);
	return (error);
}

static int
ufshci_acpi_resume(device_t dev)
{
	struct ufshci_controller *ctrlr = device_get_softc(dev);
	int error;

	error = ufshci_ctrlr_resume(ctrlr, POWER_STYPE_AWAKE);
	if (error)
		return (error);

	error = bus_generic_resume(dev);
	return (error);
}
