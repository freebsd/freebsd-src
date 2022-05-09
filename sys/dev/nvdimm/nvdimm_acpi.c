/*-
 * Copyright (c) 2017 The FreeBSD Foundation
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
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sbuf.h>
#include <sys/uuid.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acuuid.h>
#include <dev/acpica/acpivar.h>

#include <dev/nvdimm/nvdimm_var.h>

#define _COMPONENT	ACPI_OEM
ACPI_MODULE_NAME("NVDIMM_ACPI")

struct nvdimm_root_dev {
	SLIST_HEAD(, SPA_mapping) spas;
};

static MALLOC_DEFINE(M_NVDIMM_ACPI, "nvdimm_acpi", "NVDIMM ACPI bus memory");

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
		if (dimm_handle == NULL)
			continue;

		child = BUS_ADD_CHILD(dev, 100, "nvdimm", -1);
		if (child == NULL) {
			device_printf(dev, "failed to create nvdimm\n");
			return (ENXIO);
		}
		ivars = mallocarray(NVDIMM_ROOT_IVAR_MAX, sizeof(uintptr_t),
		    M_NVDIMM_ACPI, M_ZERO | M_WAITOK);
		device_set_ivars(child, ivars);
		nvdimm_root_set_acpi_handle(child, dimm_handle);
		nvdimm_root_set_device_handle(child, *dimm);
	}
	free(dimm_ids, M_NVDIMM_ACPI);
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
		spa_mapping = malloc(sizeof(struct SPA_mapping), M_NVDIMM_ACPI,
		    M_WAITOK | M_ZERO);
		error = nvdimm_spa_init(spa_mapping, *spa, spa_type);
		if (error != 0) {
			nvdimm_spa_fini(spa_mapping);
			free(spa_mapping, M_NVDIMM_ACPI);
			break;
		}
		if (nvdimm_spa_type_user_accessible(spa_type) &&
		    spa_type != SPA_TYPE_CONTROL_REGION)
			nvdimm_create_namespaces(spa_mapping, nfitbl);
		SLIST_INSERT_HEAD(&dev->spas, spa_mapping, link);
	}
	free(spas, M_NVDIMM_ACPI);
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
		nvdimm_destroy_namespaces(spa);
		nvdimm_spa_fini(spa);
		SLIST_REMOVE_HEAD(&root->spas, link);
		free(spa, M_NVDIMM_ACPI);
	}
	error = bus_generic_detach(dev);
	if (error != 0)
		return (error);
	error = device_get_children(dev, &children, &num_children);
	if (error != 0)
		return (error);
	for (i = 0; i < num_children; i++)
		free(device_get_ivars(children[i]), M_NVDIMM_ACPI);
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

static int
nvdimm_root_child_location(device_t dev, device_t child, struct sbuf *sb)
{
	ACPI_HANDLE handle;

	handle = nvdimm_root_get_acpi_handle(child);
	if (handle != NULL)
		sbuf_printf(sb, "handle=%s", acpi_name(handle));

	return (0);
}

static device_method_t nvdimm_acpi_methods[] = {
	DEVMETHOD(device_probe, nvdimm_root_probe),
	DEVMETHOD(device_attach, nvdimm_root_attach),
	DEVMETHOD(device_detach, nvdimm_root_detach),
	DEVMETHOD(bus_add_child, bus_generic_add_child),
	DEVMETHOD(bus_read_ivar, nvdimm_root_read_ivar),
	DEVMETHOD(bus_write_ivar, nvdimm_root_write_ivar),
	DEVMETHOD(bus_child_location, nvdimm_root_child_location),
	DEVMETHOD(bus_get_device_path, acpi_get_acpi_device_path),
	DEVMETHOD_END
};

static driver_t	nvdimm_acpi_driver = {
	"nvdimm_acpi_root",
	nvdimm_acpi_methods,
	sizeof(struct nvdimm_root_dev),
};

DRIVER_MODULE(nvdimm_acpi_root, acpi, nvdimm_acpi_driver, NULL, NULL);
MODULE_DEPEND(nvdimm_acpi_root, acpi, 1, 1, 1);
