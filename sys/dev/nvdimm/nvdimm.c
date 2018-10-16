/*-
 * Copyright (c) 2017 The FreeBSD Foundation
 * All rights reserved.
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
static device_t *nvdimm_devs;
static int nvdimm_devcnt;
MALLOC_DEFINE(M_NVDIMM, "nvdimm", "NVDIMM driver memory");

struct nvdimm_dev *
nvdimm_find_by_handle(nfit_handle_t nv_handle)
{
	device_t dev;
	struct nvdimm_dev *res, *nv;
	int i;

	res = NULL;
	for (i = 0; i < nvdimm_devcnt; i++) {
		dev = nvdimm_devs[i];
		if (dev == NULL)
			continue;
		nv = device_get_softc(dev);
		if (nv->nv_handle == nv_handle) {
			res = nv;
			break;
		}
	}
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
	nv->nv_flush_addr = malloc(nfitflshaddr->HintCount * sizeof(uint64_t *),
	    M_NVDIMM, M_WAITOK);
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

static ACPI_STATUS
nvdimm_walk_dev(ACPI_HANDLE handle, UINT32 level, void *ctx, void **st)
{
	ACPI_STATUS status;
	struct nvdimm_ns_walk_ctx *wctx;

	wctx = ctx;
	status = wctx->func(handle, wctx->arg);
	return_ACPI_STATUS(status);
}

static ACPI_STATUS
nvdimm_walk_root(ACPI_HANDLE handle, UINT32 level, void *ctx, void **st)
{
	ACPI_STATUS status;

	if (!acpi_MatchHid(handle, "ACPI0012"))
		return_ACPI_STATUS(AE_OK);
	status = AcpiWalkNamespace(ACPI_TYPE_DEVICE, handle, 100,
	    nvdimm_walk_dev, NULL, ctx, NULL);
	if (ACPI_FAILURE(status))
		return_ACPI_STATUS(status);
	return_ACPI_STATUS(AE_CTRL_TERMINATE);
}

static ACPI_STATUS
nvdimm_foreach_acpi(ACPI_STATUS (*func)(ACPI_HANDLE, void *), void *arg)
{
	struct nvdimm_ns_walk_ctx wctx;
	ACPI_STATUS status;

	wctx.func = func;
	wctx.arg = arg;
	status = AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, 100,
	    nvdimm_walk_root, NULL, &wctx, NULL);
	return_ACPI_STATUS(status);
}

static ACPI_STATUS
nvdimm_count_devs(ACPI_HANDLE handle __unused, void *arg)
{
	int *cnt;

	cnt = arg;
	(*cnt)++;

	ACPI_BUFFER name;
	ACPI_STATUS status;
	if (bootverbose) {
		name.Length = ACPI_ALLOCATE_BUFFER;
		status = AcpiGetName(handle, ACPI_FULL_PATHNAME, &name);
		if (ACPI_FAILURE(status))
			return_ACPI_STATUS(status);
		printf("nvdimm: enumerated %s\n", name.Pointer);
		AcpiOsFree(name.Pointer);
	}

	return_ACPI_STATUS(AE_OK);
}

struct nvdimm_create_dev_arg {
	device_t acpi0;
	int *cnt;
};

static ACPI_STATUS
nvdimm_create_dev(ACPI_HANDLE handle, void *arg)
{
	struct nvdimm_create_dev_arg *narg;
	device_t child;
	int idx;

	narg = arg;
	idx = *(narg->cnt);
	child = device_find_child(narg->acpi0, "nvdimm", idx);
	if (child == NULL)
		child = BUS_ADD_CHILD(narg->acpi0, 1, "nvdimm", idx);
	if (child == NULL) {
		if (bootverbose)
			device_printf(narg->acpi0,
			    "failed to create nvdimm%d\n", idx);
		return_ACPI_STATUS(AE_ERROR);
	}
	acpi_set_handle(child, handle);
	KASSERT(nvdimm_devs[idx] == NULL, ("nvdimm_devs[%d] not NULL", idx));
	nvdimm_devs[idx] = child;

	(*(narg->cnt))++;
	return_ACPI_STATUS(AE_OK);
}

static bool
nvdimm_init(void)
{
	ACPI_STATUS status;

	if (nvdimm_devcnt != 0)
		return (true);
	if (acpi_disabled("nvdimm"))
		return (false);
	status = nvdimm_foreach_acpi(nvdimm_count_devs, &nvdimm_devcnt);
	if (ACPI_FAILURE(status)) {
		if (bootverbose)
			printf("nvdimm_init: count failed\n");
		return (false);
	}
	nvdimm_devs = malloc(nvdimm_devcnt * sizeof(device_t), M_NVDIMM,
	    M_WAITOK | M_ZERO);
	return (true);
}

static void
nvdimm_identify(driver_t *driver, device_t parent)
{
	struct nvdimm_create_dev_arg narg;
	ACPI_STATUS status;
	int i;

	if (!nvdimm_init())
		return;
	narg.acpi0 = parent;
	narg.cnt = &i;
	i = 0;
	status = nvdimm_foreach_acpi(nvdimm_create_dev, &narg);
	if (ACPI_FAILURE(status) && bootverbose)
		printf("nvdimm_identify: create failed\n");
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
	int i;

	nv = device_get_softc(dev);
	handle = acpi_get_handle(dev);
	if (handle == NULL)
		return (EINVAL);
	nv->nv_dev = dev;
	for (i = 0; i < nvdimm_devcnt; i++) {
		if (nvdimm_devs[i] == dev) {
			nv->nv_devs_idx = i;
			break;
		}
	}
	MPASS(i < nvdimm_devcnt);
	if (ACPI_FAILURE(acpi_GetInteger(handle, "_ADR", &nv->nv_handle))) {
		device_printf(dev, "cannot get handle\n");
		return (ENXIO);
	}

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
	nvdimm_devs[nv->nv_devs_idx] = NULL;
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

static device_method_t nvdimm_methods[] = {
	DEVMETHOD(device_identify, nvdimm_identify),
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

static void
nvdimm_fini(void)
{

	free(nvdimm_devs, M_NVDIMM);
	nvdimm_devs = NULL;
	nvdimm_devcnt = 0;
}

static int
nvdimm_modev(struct module *mod, int what, void *arg)
{
	int error;

	switch (what) {
	case MOD_LOAD:
		error = 0;
		break;

	case MOD_UNLOAD:
		nvdimm_fini();
		error = 0;
		break;

	case MOD_QUIESCE:
		error = 0;
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

DRIVER_MODULE(nvdimm, acpi, nvdimm_driver, nvdimm_devclass, nvdimm_modev, NULL);
MODULE_DEPEND(nvdimm, acpi, 1, 1, 1);
