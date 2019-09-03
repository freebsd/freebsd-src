/*-
 * Copyright (C) 2012-2016 Intel Corporation
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/smp.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "nvme_private.h"

static int    nvme_pci_probe(device_t);
static int    nvme_pci_attach(device_t);
static int    nvme_pci_detach(device_t);
static int    nvme_pci_suspend(device_t);
static int    nvme_pci_resume(device_t);

static void nvme_ctrlr_setup_interrupts(struct nvme_controller *ctrlr);

static device_method_t nvme_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,     nvme_pci_probe),
	DEVMETHOD(device_attach,    nvme_pci_attach),
	DEVMETHOD(device_detach,    nvme_pci_detach),
	DEVMETHOD(device_suspend,   nvme_pci_suspend),
	DEVMETHOD(device_resume,    nvme_pci_resume),
	DEVMETHOD(device_shutdown,  nvme_shutdown),
	{ 0, 0 }
};

static driver_t nvme_pci_driver = {
	"nvme",
	nvme_pci_methods,
	sizeof(struct nvme_controller),
};

DRIVER_MODULE(nvme, pci, nvme_pci_driver, nvme_devclass, NULL, 0);

static struct _pcsid
{
	uint32_t	devid;
	int		match_subdevice;
	uint16_t	subdevice;
	const char	*desc;
	uint32_t	quirks;
} pci_ids[] = {
	{ 0x01118086,		0, 0, "NVMe Controller"  },
	{ IDT32_PCI_ID,		0, 0, "IDT NVMe Controller (32 channel)"  },
	{ IDT8_PCI_ID,		0, 0, "IDT NVMe Controller (8 channel)" },
	{ 0x09538086,		1, 0x3702, "DC P3700 SSD" },
	{ 0x09538086,		1, 0x3703, "DC P3700 SSD [2.5\" SFF]" },
	{ 0x09538086,		1, 0x3704, "DC P3500 SSD [Add-in Card]" },
	{ 0x09538086,		1, 0x3705, "DC P3500 SSD [2.5\" SFF]" },
	{ 0x09538086,		1, 0x3709, "DC P3600 SSD [Add-in Card]" },
	{ 0x09538086,		1, 0x370a, "DC P3600 SSD [2.5\" SFF]" },
	{ 0x00031c58,		0, 0, "HGST SN100",	QUIRK_DELAY_B4_CHK_RDY },
	{ 0x00231c58,		0, 0, "WDC SN200",	QUIRK_DELAY_B4_CHK_RDY },
	{ 0x05401c5f,		0, 0, "Memblaze Pblaze4", QUIRK_DELAY_B4_CHK_RDY },
	{ 0xa821144d,		0, 0, "Samsung PM1725", QUIRK_DELAY_B4_CHK_RDY },
	{ 0xa822144d,		0, 0, "Samsung PM1725a", QUIRK_DELAY_B4_CHK_RDY },
	{ 0x00000000,		0, 0, NULL  }
};


static int
nvme_match(uint32_t devid, uint16_t subdevice, struct _pcsid *ep)
{
	if (devid != ep->devid)
		return 0;

	if (!ep->match_subdevice)
		return 1;

	if (subdevice == ep->subdevice)
		return 1;
	else
		return 0;
}

static int
nvme_pci_probe (device_t device)
{
	struct nvme_controller *ctrlr = DEVICE2SOFTC(device);
	struct _pcsid	*ep;
	uint32_t	devid;
	uint16_t	subdevice;

	devid = pci_get_devid(device);
	subdevice = pci_get_subdevice(device);
	ep = pci_ids;

	while (ep->devid) {
		if (nvme_match(devid, subdevice, ep))
			break;
		++ep;
	}
	if (ep->devid)
		ctrlr->quirks = ep->quirks;

	if (ep->desc) {
		device_set_desc(device, ep->desc);
		return (BUS_PROBE_DEFAULT);
	}

#if defined(PCIS_STORAGE_NVM)
	if (pci_get_class(device)    == PCIC_STORAGE &&
	    pci_get_subclass(device) == PCIS_STORAGE_NVM &&
	    pci_get_progif(device)   == PCIP_STORAGE_NVM_ENTERPRISE_NVMHCI_1_0) {
		device_set_desc(device, "Generic NVMe Device");
		return (BUS_PROBE_GENERIC);
	}
#endif

	return (ENXIO);
}

static int
nvme_ctrlr_allocate_bar(struct nvme_controller *ctrlr)
{

	ctrlr->resource_id = PCIR_BAR(0);

	ctrlr->resource = bus_alloc_resource_any(ctrlr->dev, SYS_RES_MEMORY,
	    &ctrlr->resource_id, RF_ACTIVE);

	if(ctrlr->resource == NULL) {
		nvme_printf(ctrlr, "unable to allocate pci resource\n");
		return (ENOMEM);
	}

	ctrlr->bus_tag = rman_get_bustag(ctrlr->resource);
	ctrlr->bus_handle = rman_get_bushandle(ctrlr->resource);
	ctrlr->regs = (struct nvme_registers *)ctrlr->bus_handle;

	/*
	 * The NVMe spec allows for the MSI-X table to be placed behind
	 *  BAR 4/5, separate from the control/doorbell registers.  Always
	 *  try to map this bar, because it must be mapped prior to calling
	 *  pci_alloc_msix().  If the table isn't behind BAR 4/5,
	 *  bus_alloc_resource() will just return NULL which is OK.
	 */
	ctrlr->bar4_resource_id = PCIR_BAR(4);
	ctrlr->bar4_resource = bus_alloc_resource_any(ctrlr->dev, SYS_RES_MEMORY,
	    &ctrlr->bar4_resource_id, RF_ACTIVE);

	return (0);
}

static int
nvme_pci_attach(device_t dev)
{
	struct nvme_controller*ctrlr = DEVICE2SOFTC(dev);
	int status;

	ctrlr->dev = dev;
	status = nvme_ctrlr_allocate_bar(ctrlr);
	if (status != 0)
		goto bad;
	pci_enable_busmaster(dev);
	nvme_ctrlr_setup_interrupts(ctrlr);
	return nvme_attach(dev);
bad:
	if (ctrlr->resource != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    ctrlr->resource_id, ctrlr->resource);
	}

	if (ctrlr->bar4_resource != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    ctrlr->bar4_resource_id, ctrlr->bar4_resource);
	}

	if (ctrlr->tag)
		bus_teardown_intr(dev, ctrlr->res, ctrlr->tag);

	if (ctrlr->res)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(ctrlr->res), ctrlr->res);

	if (ctrlr->msix_enabled)
		pci_release_msi(dev);

	return status;
}

static int
nvme_pci_detach(device_t dev)
{
	struct nvme_controller*ctrlr = DEVICE2SOFTC(dev);
	int rv;

	rv = nvme_detach(dev);
	if (ctrlr->msix_enabled)
		pci_release_msi(dev);
	pci_disable_busmaster(dev);
	return (rv);
}

static int
nvme_ctrlr_configure_intx(struct nvme_controller *ctrlr)
{

	ctrlr->msix_enabled = 0;
	ctrlr->num_io_queues = 1;
	ctrlr->num_cpus_per_ioq = mp_ncpus;
	ctrlr->rid = 0;
	ctrlr->res = bus_alloc_resource_any(ctrlr->dev, SYS_RES_IRQ,
	    &ctrlr->rid, RF_SHAREABLE | RF_ACTIVE);

	if (ctrlr->res == NULL) {
		nvme_printf(ctrlr, "unable to allocate shared IRQ\n");
		return (ENOMEM);
	}

	bus_setup_intr(ctrlr->dev, ctrlr->res,
	    INTR_TYPE_MISC | INTR_MPSAFE, NULL, nvme_ctrlr_intx_handler,
	    ctrlr, &ctrlr->tag);

	if (ctrlr->tag == NULL) {
		nvme_printf(ctrlr, "unable to setup intx handler\n");
		return (ENOMEM);
	}

	return (0);
}

static void
nvme_ctrlr_setup_interrupts(struct nvme_controller *ctrlr)
{
	device_t	dev;
	int		per_cpu_io_queues;
	int		min_cpus_per_ioq;
	int		num_vectors_requested, num_vectors_allocated;
	int		num_vectors_available;

	dev = ctrlr->dev;
	min_cpus_per_ioq = 1;
	TUNABLE_INT_FETCH("hw.nvme.min_cpus_per_ioq", &min_cpus_per_ioq);

	if (min_cpus_per_ioq < 1) {
		min_cpus_per_ioq = 1;
	} else if (min_cpus_per_ioq > mp_ncpus) {
		min_cpus_per_ioq = mp_ncpus;
	}

	per_cpu_io_queues = 1;
	TUNABLE_INT_FETCH("hw.nvme.per_cpu_io_queues", &per_cpu_io_queues);

	if (per_cpu_io_queues == 0) {
		min_cpus_per_ioq = mp_ncpus;
	}

	ctrlr->force_intx = 0;
	TUNABLE_INT_FETCH("hw.nvme.force_intx", &ctrlr->force_intx);

	/*
	 * FreeBSD currently cannot allocate more than about 190 vectors at
	 *  boot, meaning that systems with high core count and many devices
	 *  requesting per-CPU interrupt vectors will not get their full
	 *  allotment.  So first, try to allocate as many as we may need to
	 *  understand what is available, then immediately release them.
	 *  Then figure out how many of those we will actually use, based on
	 *  assigning an equal number of cores to each I/O queue.
	 */

	/* One vector for per core I/O queue, plus one vector for admin queue. */
	num_vectors_available = min(pci_msix_count(dev), mp_ncpus + 1);
	if (pci_alloc_msix(dev, &num_vectors_available) != 0) {
		num_vectors_available = 0;
	}
	pci_release_msi(dev);

	if (ctrlr->force_intx || num_vectors_available < 2) {
		nvme_ctrlr_configure_intx(ctrlr);
		return;
	}

	/*
	 * Do not use all vectors for I/O queues - one must be saved for the
	 *  admin queue.
	 */
	ctrlr->num_cpus_per_ioq = max(min_cpus_per_ioq,
	    howmany(mp_ncpus, num_vectors_available - 1));

	ctrlr->num_io_queues = howmany(mp_ncpus, ctrlr->num_cpus_per_ioq);
	num_vectors_requested = ctrlr->num_io_queues + 1;
	num_vectors_allocated = num_vectors_requested;

	/*
	 * Now just allocate the number of vectors we need.  This should
	 *  succeed, since we previously called pci_alloc_msix()
	 *  successfully returning at least this many vectors, but just to
	 *  be safe, if something goes wrong just revert to INTx.
	 */
	if (pci_alloc_msix(dev, &num_vectors_allocated) != 0) {
		nvme_ctrlr_configure_intx(ctrlr);
		return;
	}

	if (num_vectors_allocated < num_vectors_requested) {
		pci_release_msi(dev);
		nvme_ctrlr_configure_intx(ctrlr);
		return;
	}

	ctrlr->msix_enabled = 1;
}

static int
nvme_pci_suspend(device_t dev)
{
	struct nvme_controller	*ctrlr;

	ctrlr = DEVICE2SOFTC(dev);
	return (nvme_ctrlr_suspend(ctrlr));
}

static int
nvme_pci_resume(device_t dev)
{
	struct nvme_controller	*ctrlr;

	ctrlr = DEVICE2SOFTC(dev);
	return (nvme_ctrlr_resume(ctrlr));
}
