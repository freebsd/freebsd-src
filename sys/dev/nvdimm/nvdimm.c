/*-
 * Copyright (c) 2017 The FreeBSD Foundation
 * All rights reserved.
 * Copyright (c) 2018, 2019 Intel Corporation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include "opt_acpi.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/uuid.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acuuid.h>
#include <dev/acpica/acpivar.h>
#include <dev/nvdimm/nvdimm_var.h>

#define _COMPONENT	ACPI_OEM
ACPI_MODULE_NAME("NVDIMM")

static devclass_t nvdimm_devclass;
static devclass_t nvdimm_root_devclass;
MALLOC_DEFINE(M_NVDIMM, "nvdimm", "NVDIMM driver memory");

struct nvdimm_dev *
nvdimm_find_by_handle(nfit_handle_t nv_handle)
{
	struct nvdimm_dev *res;
	device_t *dimms;
	int i, error, num_dimms;

	res = NULL;
	error = devclass_get_devices(nvdimm_devclass, &dimms, &num_dimms);
	if (error != 0)
		return (NULL);
	for (i = 0; i < num_dimms; i++) {
		if (nvdimm_root_get_device_handle(dimms[i]) == nv_handle) {
			res = device_get_softc(dimms[i]);
			break;
		}
	}
	free(dimms, M_TEMP);
	return (res);
}

static int
nvdimm_parse_flush_addr(void *nfitsubtbl, void *arg)
{
	ACPI_NFIT_FLUSH_ADDRESS *nfitflshaddr;
	struct nvdimm_dev *nv;
	int i;

	nfitflshaddr = nfitsubtbl;
	nv = arg;
	if (nfitflshaddr->DeviceHandle != nv->nv_handle)
		return (0);

	MPASS(nv->nv_flush_addr == NULL && nv->nv_flush_addr_cnt == 0);
	nv->nv_flush_addr = mallocarray(nfitflshaddr->HintCount,
	    sizeof(uint64_t *), M_NVDIMM, M_WAITOK);
	for (i = 0; i < nfitflshaddr->HintCount; i++)
		nv->nv_flush_addr[i] = (uint64_t *)nfitflshaddr->HintAddress[i];
	nv->nv_flush_addr_cnt = nfitflshaddr->HintCount;
	return (0);
}

int
nvdimm_iterate_nfit(ACPI_TABLE_NFIT *nfitbl, enum AcpiNfitType type,
    int (*cb)(void *, void *), void *arg)
{
	ACPI_NFIT_HEADER *nfithdr;
	ACPI_NFIT_SYSTEM_ADDRESS *nfitaddr;
	ACPI_NFIT_MEMORY_MAP *nfitmap;
	ACPI_NFIT_INTERLEAVE *nfitintrl;
	ACPI_NFIT_SMBIOS *nfitsmbios;
	ACPI_NFIT_CONTROL_REGION *nfitctlreg;
	ACPI_NFIT_DATA_REGION *nfitdtreg;
	ACPI_NFIT_FLUSH_ADDRESS *nfitflshaddr;
	char *ptr;
	int error;

	error = 0;
	for (ptr = (char *)(nfitbl + 1);
	    ptr < (char *)nfitbl + nfitbl->Header.Length;
	    ptr += nfithdr->Length) {
		nfithdr = (ACPI_NFIT_HEADER *)ptr;
		if (nfithdr->Type != type)
			continue;
		switch (nfithdr->Type) {
		case ACPI_NFIT_TYPE_SYSTEM_ADDRESS:
			nfitaddr = __containerof(nfithdr,
			    ACPI_NFIT_SYSTEM_ADDRESS, Header);
			error = cb(nfitaddr, arg);
			break;
		case ACPI_NFIT_TYPE_MEMORY_MAP:
			nfitmap = __containerof(nfithdr,
			    ACPI_NFIT_MEMORY_MAP, Header);
			error = cb(nfitmap, arg);
			break;
		case ACPI_NFIT_TYPE_INTERLEAVE:
			nfitintrl = __containerof(nfithdr,
			    ACPI_NFIT_INTERLEAVE, Header);
			error = cb(nfitintrl, arg);
			break;
		case ACPI_NFIT_TYPE_SMBIOS:
			nfitsmbios = __containerof(nfithdr,
			    ACPI_NFIT_SMBIOS, Header);
			error = cb(nfitsmbios, arg);
			break;
		case ACPI_NFIT_TYPE_CONTROL_REGION:
			nfitctlreg = __containerof(nfithdr,
			    ACPI_NFIT_CONTROL_REGION, Header);
			error = cb(nfitctlreg, arg);
			break;
		case ACPI_NFIT_TYPE_DATA_REGION:
			nfitdtreg = __containerof(nfithdr,
			    ACPI_NFIT_DATA_REGION, Header);
			error = cb(nfitdtreg, arg);
			break;
		case ACPI_NFIT_TYPE_FLUSH_ADDRESS:
			nfitflshaddr = __containerof(nfithdr,
			    ACPI_NFIT_FLUSH_ADDRESS, Header);
			error = cb(nfitflshaddr, arg);
			break;
		case ACPI_NFIT_TYPE_RESERVED:
		default:
			if (bootverbose)
				printf("NFIT subtype %d unknown\n",
				    nfithdr->Type);
			error = 0;
			break;
		}
		if (error != 0)
			break;
	}
	return (error);
}

static int
nvdimm_probe(device_t dev)
{

	return (BUS_PROBE_NOWILDCARD);
}

static int
nvdimm_attach(device_t dev)
{
	struct nvdimm_dev *nv;
	ACPI_TABLE_NFIT *nfitbl;
	ACPI_HANDLE handle;
	ACPI_STATUS status;

	nv = device_get_softc(dev);
	handle = nvdimm_root_get_acpi_handle(dev);
	if (handle == NULL)
		return (EINVAL);
	nv->nv_dev = dev;
	nv->nv_handle = nvdimm_root_get_device_handle(dev);

	status = AcpiGetTable(ACPI_SIG_NFIT, 1, (ACPI_TABLE_HEADER **)&nfitbl);
	if (ACPI_FAILURE(status)) {
		if (bootverbose)
			device_printf(dev, "cannot get NFIT\n");
		return (ENXIO);
	}
	nvdimm_iterate_nfit(nfitbl, ACPI_NFIT_TYPE_FLUSH_ADDRESS,
	    nvdimm_parse_flush_addr, nv);
	AcpiPutTable(&nfitbl->Header);
	return (0);
}

static int
nvdimm_detach(device_t dev)
{
	struct nvdimm_dev *nv;

	nv = device_get_softc(dev);
	free(nv->nv_flush_addr, M_NVDIMM);
	return (0);
}

static int
nvdimm_suspend(device_t dev)
{

	return (0);
}

static int
nvdimm_resume(device_t dev)
{

	return (0);
}

static ACPI_STATUS
nvdimm_root_create_dev(ACPI_HANDLE handle, UINT32 nesting_level, void *context,
    void **return_value)
{
	ACPI_STATUS status;
	ACPI_DEVICE_INFO *device_info;
	device_t parent, child;
	uintptr_t *ivars;

	parent = context;
	child = BUS_ADD_CHILD(parent, 100, "nvdimm", -1);
	if (child == NULL) {
		device_printf(parent, "failed to create nvdimm\n");
		return_ACPI_STATUS(AE_ERROR);
	}
	status = AcpiGetObjectInfo(handle, &device_info);
	if (ACPI_FAILURE(status)) {
		device_printf(parent, "failed to get nvdimm device info\n");
		return_ACPI_STATUS(AE_ERROR);
	}
	ivars = mallocarray(NVDIMM_ROOT_IVAR_MAX - 1, sizeof(uintptr_t),
	    M_NVDIMM, M_ZERO | M_WAITOK);
	device_set_ivars(child, ivars);
	nvdimm_root_set_acpi_handle(child, handle);
	nvdimm_root_set_device_handle(child, device_info->Address);
	return_ACPI_STATUS(AE_OK);
}

static char *nvdimm_root_id[] = {"ACPI0012", NULL};

static int
nvdimm_root_probe(device_t dev)
{

	if (acpi_disabled("nvdimm"))
		return (ENXIO);
	if (ACPI_ID_PROBE(device_get_parent(dev), dev, nvdimm_root_id)
	    != NULL) {
		device_set_desc(dev, "ACPI NVDIMM root device");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
nvdimm_root_attach(device_t dev)
{
	ACPI_HANDLE handle;
	ACPI_STATUS status;
	int error;

	handle = acpi_get_handle(dev);
	status = AcpiWalkNamespace(ACPI_TYPE_DEVICE, handle, 1,
	    nvdimm_root_create_dev, NULL, dev, NULL);
	if (ACPI_FAILURE(status))
		device_printf(dev, "failed adding children\n");
	error = bus_generic_attach(dev);
	return (error);
}

static int
nvdimm_root_detach(device_t dev)
{
	device_t *children;
	int i, error, num_children;

	error = bus_generic_detach(dev);
	if (error != 0)
		return (error);
	error = device_get_children(dev, &children, &num_children);
	if (error != 0)
		return (error);
	for (i = 0; i < num_children; i++)
		free(device_get_ivars(children[i]), M_NVDIMM);
	free(children, M_TEMP);
	error = device_delete_children(dev);
	return (error);
}

static int
nvdimm_root_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result)
{

	if (index < 0 || index >= NVDIMM_ROOT_IVAR_MAX)
		return (ENOENT);
	*result = ((uintptr_t *)device_get_ivars(child))[index];
	return (0);
}

static int
nvdimm_root_write_ivar(device_t dev, device_t child, int index,
    uintptr_t value)
{

	if (index < 0 || index >= NVDIMM_ROOT_IVAR_MAX)
		return (ENOENT);
	((uintptr_t *)device_get_ivars(child))[index] = value;
	return (0);
}

static device_method_t nvdimm_methods[] = {
	DEVMETHOD(device_probe, nvdimm_probe),
	DEVMETHOD(device_attach, nvdimm_attach),
	DEVMETHOD(device_detach, nvdimm_detach),
	DEVMETHOD(device_suspend, nvdimm_suspend),
	DEVMETHOD(device_resume, nvdimm_resume),
	DEVMETHOD_END
};

static driver_t	nvdimm_driver = {
	"nvdimm",
	nvdimm_methods,
	sizeof(struct nvdimm_dev),
};

static device_method_t nvdimm_root_methods[] = {
	DEVMETHOD(device_probe, nvdimm_root_probe),
	DEVMETHOD(device_attach, nvdimm_root_attach),
	DEVMETHOD(device_detach, nvdimm_root_detach),
	DEVMETHOD(bus_add_child, bus_generic_add_child),
	DEVMETHOD(bus_read_ivar, nvdimm_root_read_ivar),
	DEVMETHOD(bus_write_ivar, nvdimm_root_write_ivar),
	DEVMETHOD_END
};

static driver_t	nvdimm_root_driver = {
	"nvdimm_root",
	nvdimm_root_methods,
};

DRIVER_MODULE(nvdimm_root, acpi, nvdimm_root_driver, nvdimm_root_devclass, NULL,
    NULL);
DRIVER_MODULE(nvdimm, nvdimm_root, nvdimm_driver, nvdimm_devclass, NULL, NULL);
MODULE_DEPEND(nvdimm, acpi, 1, 1, 1);
