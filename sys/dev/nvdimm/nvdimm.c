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
	acpi_nfit_get_flush_addrs(nfitbl, nv->nv_handle, &nv->nv_flush_addr,
	    &nv->nv_flush_addr_cnt);
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
find_dimm(ACPI_HANDLE handle, UINT32 nesting_level, void *context,
    void **return_value)
{
	ACPI_DEVICE_INFO *device_info;
	ACPI_STATUS status;

	status = AcpiGetObjectInfo(handle, &device_info);
	if (ACPI_FAILURE(status))
		return_ACPI_STATUS(AE_ERROR);
	if (device_info->Address == (uintptr_t)context) {
		*(ACPI_HANDLE *)return_value = handle;
		return_ACPI_STATUS(AE_CTRL_TERMINATE);
	}
	return_ACPI_STATUS(AE_OK);
}

static ACPI_HANDLE
get_dimm_acpi_handle(ACPI_HANDLE root_handle, nfit_handle_t adr)
{
	ACPI_HANDLE res;
	ACPI_STATUS status;

	res = NULL;
	status = AcpiWalkNamespace(ACPI_TYPE_DEVICE, root_handle, 1, find_dimm,
	    NULL, (void *)(uintptr_t)adr, &res);
	if (ACPI_FAILURE(status))
		res = NULL;
	return (res);
}

static int
nvdimm_root_create_devs(device_t dev, ACPI_TABLE_NFIT *nfitbl)
{
	ACPI_HANDLE root_handle, dimm_handle;
	device_t child;
	nfit_handle_t *dimm_ids, *dimm;
	uintptr_t *ivars;
	int num_dimm_ids;

	root_handle = acpi_get_handle(dev);
	acpi_nfit_get_dimm_ids(nfitbl, &dimm_ids, &num_dimm_ids);
	for (dimm = dimm_ids; dimm < dimm_ids + num_dimm_ids; dimm++) {
		dimm_handle = get_dimm_acpi_handle(root_handle, *dimm);
		child = BUS_ADD_CHILD(dev, 100, "nvdimm", -1);
		if (child == NULL) {
			device_printf(dev, "failed to create nvdimm\n");
			return (ENXIO);
		}
		ivars = mallocarray(NVDIMM_ROOT_IVAR_MAX, sizeof(uintptr_t),
		    M_NVDIMM, M_ZERO | M_WAITOK);
		device_set_ivars(child, ivars);
		nvdimm_root_set_acpi_handle(child, dimm_handle);
		nvdimm_root_set_device_handle(child, *dimm);
	}
	free(dimm_ids, M_NVDIMM);
	return (0);
}

static int
nvdimm_root_create_spas(struct nvdimm_root_dev *dev, ACPI_TABLE_NFIT *nfitbl)
{
	ACPI_NFIT_SYSTEM_ADDRESS **spas, **spa;
	struct SPA_mapping *spa_mapping;
	enum SPA_mapping_type spa_type;
	int error, num_spas;

	error = 0;
	acpi_nfit_get_spa_ranges(nfitbl, &spas, &num_spas);
	for (spa = spas; spa < spas + num_spas; spa++) {
		spa_type = nvdimm_spa_type_from_uuid(
			(struct uuid *)(*spa)->RangeGuid);
		if (spa_type == SPA_TYPE_UNKNOWN)
			continue;
		spa_mapping = malloc(sizeof(struct SPA_mapping), M_NVDIMM,
		    M_WAITOK | M_ZERO);
		error = nvdimm_spa_init(spa_mapping, *spa, spa_type);
		if (error != 0) {
			nvdimm_spa_fini(spa_mapping);
			free(spa, M_NVDIMM);
			break;
		}
		SLIST_INSERT_HEAD(&dev->spas, spa_mapping, link);
	}
	free(spas, M_NVDIMM);
	return (error);
}

static char *nvdimm_root_id[] = {"ACPI0012", NULL};

static int
nvdimm_root_probe(device_t dev)
{
	int rv;

	if (acpi_disabled("nvdimm"))
		return (ENXIO);
	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, nvdimm_root_id, NULL);
	if (rv <= 0)
		device_set_desc(dev, "ACPI NVDIMM root device");

	return (rv);
}

static int
nvdimm_root_attach(device_t dev)
{
	struct nvdimm_root_dev *root;
	ACPI_TABLE_NFIT *nfitbl;
	ACPI_STATUS status;
	int error;

	status = AcpiGetTable(ACPI_SIG_NFIT, 1, (ACPI_TABLE_HEADER **)&nfitbl);
	if (ACPI_FAILURE(status)) {
		device_printf(dev, "cannot get NFIT\n");
		return (ENXIO);
	}
	error = nvdimm_root_create_devs(dev, nfitbl);
	if (error != 0)
		return (error);
	error = bus_generic_attach(dev);
	if (error != 0)
		return (error);
	root = device_get_softc(dev);
	error = nvdimm_root_create_spas(root, nfitbl);
	AcpiPutTable(&nfitbl->Header);
	return (error);
}

static int
nvdimm_root_detach(device_t dev)
{
	struct nvdimm_root_dev *root;
	struct SPA_mapping *spa, *next;
	device_t *children;
	int i, error, num_children;

	root = device_get_softc(dev);
	SLIST_FOREACH_SAFE(spa, &root->spas, link, next) {
		nvdimm_spa_fini(spa);
		SLIST_REMOVE_HEAD(&root->spas, link);
		free(spa, M_NVDIMM);
	}
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
	sizeof(struct nvdimm_root_dev),
};

DRIVER_MODULE(nvdimm_root, acpi, nvdimm_root_driver, nvdimm_root_devclass, NULL,
    NULL);
DRIVER_MODULE(nvdimm, nvdimm_root, nvdimm_driver, nvdimm_devclass, NULL, NULL);
MODULE_DEPEND(nvdimm, acpi, 1, 1, 1);
