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
#include <sys/bitstring.h>
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

static struct uuid intel_nvdimm_dsm_uuid =
    {0x4309AC30,0x0D11,0x11E4,0x91,0x91,{0x08,0x00,0x20,0x0C,0x9A,0x66}};
#define INTEL_NVDIMM_DSM_REV 1
#define INTEL_NVDIMM_DSM_GET_LABEL_SIZE 4
#define INTEL_NVDIMM_DSM_GET_LABEL_DATA 5

static devclass_t nvdimm_devclass;
MALLOC_DEFINE(M_NVDIMM, "nvdimm", "NVDIMM driver memory");

static int
read_label_area_size(struct nvdimm_dev *nv)
{
	ACPI_OBJECT *result_buffer;
	ACPI_HANDLE handle;
	ACPI_STATUS status;
	ACPI_BUFFER result;
	uint32_t *out;
	int error;

	handle = nvdimm_root_get_acpi_handle(nv->nv_dev);
	if (handle == NULL)
		return (ENODEV);
	result.Length = ACPI_ALLOCATE_BUFFER;
	result.Pointer = NULL;
	status = acpi_EvaluateDSM(handle, (uint8_t *)&intel_nvdimm_dsm_uuid,
	    INTEL_NVDIMM_DSM_REV, INTEL_NVDIMM_DSM_GET_LABEL_SIZE, NULL,
	    &result);
	error = ENXIO;
	if (ACPI_SUCCESS(status) && result.Pointer != NULL &&
	    result.Length >= sizeof(ACPI_OBJECT)) {
		result_buffer = result.Pointer;
		if (result_buffer->Type == ACPI_TYPE_BUFFER &&
		    result_buffer->Buffer.Length >= 12) {
			out = (uint32_t *)result_buffer->Buffer.Pointer;
			nv->label_area_size = out[1];
			nv->max_label_xfer = out[2];
			error = 0;
		}
	}
	if (result.Pointer != NULL)
		AcpiOsFree(result.Pointer);
	return (error);
}

static int
read_label_area(struct nvdimm_dev *nv, uint8_t *dest, off_t offset,
    off_t length)
{
	ACPI_BUFFER result;
	ACPI_HANDLE handle;
	ACPI_OBJECT params_pkg, params_buf, *result_buf;
	ACPI_STATUS status;
	uint32_t params[2];
	off_t to_read;
	int error;

	error = 0;
	handle = nvdimm_root_get_acpi_handle(nv->nv_dev);
	if (offset < 0 || length <= 0 ||
	    offset + length > nv->label_area_size ||
	    handle == NULL)
		return (ENODEV);
	params_pkg.Type = ACPI_TYPE_PACKAGE;
	params_pkg.Package.Count = 1;
	params_pkg.Package.Elements = &params_buf;
	params_buf.Type = ACPI_TYPE_BUFFER;
	params_buf.Buffer.Length = sizeof(params);
	params_buf.Buffer.Pointer = (UINT8 *)params;
	while (length > 0) {
		to_read = MIN(length, nv->max_label_xfer);
		params[0] = offset;
		params[1] = to_read;
		result.Length = ACPI_ALLOCATE_BUFFER;
		result.Pointer = NULL;
		status = acpi_EvaluateDSM(handle,
		    (uint8_t *)&intel_nvdimm_dsm_uuid, INTEL_NVDIMM_DSM_REV,
		    INTEL_NVDIMM_DSM_GET_LABEL_DATA, &params_pkg, &result);
		if (ACPI_FAILURE(status) ||
		    result.Length < sizeof(ACPI_OBJECT) ||
		    result.Pointer == NULL) {
			error = ENXIO;
			break;
		}
		result_buf = (ACPI_OBJECT *)result.Pointer;
		if (result_buf->Type != ACPI_TYPE_BUFFER ||
		    result_buf->Buffer.Pointer == NULL ||
		    result_buf->Buffer.Length != 4 + to_read ||
		    ((uint16_t *)result_buf->Buffer.Pointer)[0] != 0) {
			error = ENXIO;
			break;
		}
		bcopy(result_buf->Buffer.Pointer + 4, dest, to_read);
		dest += to_read;
		offset += to_read;
		length -= to_read;
		if (result.Pointer != NULL) {
			AcpiOsFree(result.Pointer);
			result.Pointer = NULL;
		}
	}
	if (result.Pointer != NULL)
		AcpiOsFree(result.Pointer);
	return (error);
}

static uint64_t
fletcher64(const void *data, size_t length)
{
	size_t i;
	uint32_t a, b;
	const uint32_t *d;

	a = 0;
	b = 0;
	d = (const uint32_t *)data;
	length = length / sizeof(uint32_t);
	for (i = 0; i < length; i++) {
		a += d[i];
		b += a;
	}
	return ((uint64_t)b << 32 | a);
}

static bool
label_index_is_valid(struct nvdimm_label_index *index, uint32_t max_labels,
    size_t size, size_t offset)
{
	uint64_t checksum;

	index = (struct nvdimm_label_index *)((uint8_t *)index + size * offset);
	if (strcmp(index->signature, NVDIMM_INDEX_BLOCK_SIGNATURE) != 0)
		return false;
	checksum = index->checksum;
	index->checksum = 0;
	if (checksum != fletcher64(index, size) ||
	    index->this_offset != size * offset || index->this_size != size ||
	    index->other_offset != size * (offset == 0 ? 1 : 0) ||
	    index->seq == 0 || index->seq > 3 || index->slot_cnt > max_labels ||
	    index->label_size != 1)
		return false;
	return true;
}

static int
read_label(struct nvdimm_dev *nv, int num)
{
	struct nvdimm_label_entry *entry, *i, *next;
	uint64_t checksum;
	off_t offset;
	int error;

	offset = nv->label_index->label_offset +
	    num * (128 << nv->label_index->label_size);
	entry = malloc(sizeof(*entry), M_NVDIMM, M_WAITOK);
	error = read_label_area(nv, (uint8_t *)&entry->label, offset,
	    sizeof(struct nvdimm_label));
	if (error != 0) {
		free(entry, M_NVDIMM);
		return (error);
	}
	checksum = entry->label.checksum;
	entry->label.checksum = 0;
	if (checksum != fletcher64(&entry->label, sizeof(entry->label)) ||
	    entry->label.slot != num) {
		free(entry, M_NVDIMM);
		return (ENXIO);
	}

	/* Insertion ordered by dimm_phys_addr */
	if (SLIST_EMPTY(&nv->labels) ||
	    entry->label.dimm_phys_addr <=
	    SLIST_FIRST(&nv->labels)->label.dimm_phys_addr) {
		SLIST_INSERT_HEAD(&nv->labels, entry, link);
		return (0);
	}
	SLIST_FOREACH_SAFE(i, &nv->labels, link, next) {
		if (next == NULL ||
		    entry->label.dimm_phys_addr <= next->label.dimm_phys_addr) {
			SLIST_INSERT_AFTER(i, entry, link);
			return (0);
		}
	}
	__unreachable();
}

static int
read_labels(struct nvdimm_dev *nv)
{
	struct nvdimm_label_index *indices, *index1;
	size_t bitfield_size, index_size, num_labels;
	int error, n;
	bool index_0_valid, index_1_valid;

	for (index_size = 256; ; index_size += 256) {
		num_labels = 8 * (index_size -
		    sizeof(struct nvdimm_label_index));
		if (index_size + num_labels * sizeof(struct nvdimm_label) >=
		    nv->label_area_size)
			break;
	}
	num_labels = (nv->label_area_size - index_size) /
	    sizeof(struct nvdimm_label);
	bitfield_size = roundup2(num_labels, 8) / 8;
	indices = malloc(2 * index_size, M_NVDIMM, M_WAITOK);
	index1 = (void *)((uint8_t *)indices + index_size);
	error = read_label_area(nv, (void *)indices, 0, 2 * index_size);
	if (error != 0) {
		free(indices, M_NVDIMM);
		return (error);
	}
	index_0_valid = label_index_is_valid(indices, num_labels, index_size,
	    0);
	index_1_valid = label_index_is_valid(indices, num_labels, index_size,
	    1);
	if (!index_0_valid && !index_1_valid) {
		free(indices, M_NVDIMM);
		return (ENXIO);
	}
	if (index_0_valid && index_1_valid) {
		if (((int)indices->seq - (int)index1->seq + 3) % 3 == 1) {
			/* index 0 was more recently updated */
			index_1_valid = false;
		} else {
			/*
			 * either index 1 was more recently updated,
			 * or the sequence numbers are equal, in which
			 * case the specification says the block with
			 * the higher offset is to be treated as valid
			 */
			index_0_valid = false;
		}
	}
	nv->label_index = malloc(index_size, M_NVDIMM, M_WAITOK);
	bcopy(index_0_valid ? indices : index1, nv->label_index, index_size);
	free(indices, M_NVDIMM);
	bit_ffc_at((bitstr_t *)nv->label_index->free, 0,
	    nv->label_index->slot_cnt, &n);
	while (n >= 0) {
		read_label(nv, n);
		bit_ffc_at((bitstr_t *)nv->label_index->free, n + 1,
		    nv->label_index->slot_cnt, &n);
	}
	return (0);
}

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
	int error;

	nv = device_get_softc(dev);
	handle = nvdimm_root_get_acpi_handle(dev);
	MPASS(handle != NULL);
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
	error = read_label_area_size(nv);
	if (error == 0) {
		/*
		 * Ignoring errors reading labels. Not all NVDIMMs
		 * support labels and namespaces.
		 */
		read_labels(nv);
	}
	return (0);
}

static int
nvdimm_detach(device_t dev)
{
	struct nvdimm_dev *nv;
	struct nvdimm_label_entry *label, *next;

	nv = device_get_softc(dev);
	free(nv->nv_flush_addr, M_NVDIMM);
	free(nv->label_index, M_NVDIMM);
	SLIST_FOREACH_SAFE(label, &nv->labels, link, next) {
		SLIST_REMOVE_HEAD(&nv->labels, link);
		free(label, M_NVDIMM);
	}
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

DRIVER_MODULE(nvdimm, nvdimm_acpi_root, nvdimm_driver, nvdimm_devclass, NULL,
    NULL);
MODULE_DEPEND(nvdimm, acpi, 1, 1, 1);
