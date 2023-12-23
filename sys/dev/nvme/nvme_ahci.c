/*-
 * Copyright (C) 2017 Olivier Houchard
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/smp.h>

#include "nvme_private.h"

static int    nvme_ahci_probe(device_t dev);
static int    nvme_ahci_attach(device_t dev);
static int    nvme_ahci_detach(device_t dev);

static device_method_t nvme_ahci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,     nvme_ahci_probe),
	DEVMETHOD(device_attach,    nvme_ahci_attach),
	DEVMETHOD(device_detach,    nvme_ahci_detach),
	DEVMETHOD(device_shutdown,  nvme_shutdown),
	{ 0, 0 }
};

static driver_t nvme_ahci_driver = {
	"nvme",
	nvme_ahci_methods,
	sizeof(struct nvme_controller),
};

DRIVER_MODULE(nvme, ahci, nvme_ahci_driver, NULL, NULL);

static int
nvme_ahci_probe (device_t device)
{
	return (0);
}

static int
nvme_ahci_attach(device_t dev)
{
	struct nvme_controller*ctrlr = DEVICE2SOFTC(dev);
	int ret;

	/* Map MMIO registers */
	ctrlr->resource_id = 0;

	ctrlr->resource = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &ctrlr->resource_id, RF_ACTIVE);

	if(ctrlr->resource == NULL) {
		nvme_printf(ctrlr, "unable to allocate mem resource\n");
		ret = ENOMEM;
		goto bad;
	}
	ctrlr->bus_tag = rman_get_bustag(ctrlr->resource);
	ctrlr->bus_handle = rman_get_bushandle(ctrlr->resource);
	ctrlr->regs = (struct nvme_registers *)ctrlr->bus_handle;

	/* Allocate and setup IRQ */
	ctrlr->rid = 0;
	ctrlr->res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &ctrlr->rid, RF_SHAREABLE | RF_ACTIVE);
	if (ctrlr->res == NULL) {
		nvme_printf(ctrlr, "unable to allocate shared interrupt\n");
		ret = ENOMEM;
		goto bad;
	}

	ctrlr->msi_count = 0;
	ctrlr->num_io_queues = 1;
	if (bus_setup_intr(dev, ctrlr->res,
	    INTR_TYPE_MISC | INTR_MPSAFE, NULL, nvme_ctrlr_shared_handler,
	    ctrlr, &ctrlr->tag) != 0) {
		nvme_printf(ctrlr, "unable to setup shared interrupt\n");
		ret = ENOMEM;
		goto bad;
	}
	ctrlr->tag = (void *)0x1;

	/*
	 * We're attached via this funky mechanism. Flag the controller so that
	 * it avoids things that can't work when we do that, like asking for
	 * PCI config space entries.
	 */
	ctrlr->quirks |= QUIRK_AHCI;

	return (nvme_attach(dev));	/* Note: failure frees resources */
bad:
	if (ctrlr->resource != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    ctrlr->resource_id, ctrlr->resource);
	}
	if (ctrlr->res)
		bus_release_resource(ctrlr->dev, SYS_RES_IRQ,
		    rman_get_rid(ctrlr->res), ctrlr->res);
	return (ret);
}

static int
nvme_ahci_detach(device_t dev)
{

	return (nvme_detach(dev));
}
