/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <corvink@FreeBSD.org>
 */

#include <sys/types.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <vmmapi.h>

#include "acpi.h"
#include "acpi_device.h"
#include "config.h"
#include "tpm_device.h"
#include "tpm_emul.h"
#include "tpm_intf.h"
#include "tpm_ppi.h"

#define TPM_ACPI_DEVICE_NAME "TPM"
#define TPM_ACPI_HARDWARE_ID "MSFT0101"

SET_DECLARE(tpm_emul_set, struct tpm_emul);
SET_DECLARE(tpm_intf_set, struct tpm_intf);
SET_DECLARE(tpm_ppi_set, struct tpm_ppi);

struct tpm_device {
	struct vmctx *vm_ctx;
	struct acpi_device *acpi_dev;
	struct tpm_emul *emul;
	void *emul_sc;
	struct tpm_intf *intf;
	void *intf_sc;
	struct tpm_ppi *ppi;
	void *ppi_sc;
};

static int
tpm_build_acpi_table(const struct acpi_device *const dev)
{
	const struct tpm_device *const tpm = acpi_device_get_softc(dev);

	if (tpm->intf->build_acpi_table == NULL) {
		return (0);
	}

	return (tpm->intf->build_acpi_table(tpm->intf_sc, tpm->vm_ctx));
}

static int
tpm_write_dsdt(const struct acpi_device *const dev)
{
	int error;

	const struct tpm_device *const tpm = acpi_device_get_softc(dev);
	const struct tpm_ppi *const ppi = tpm->ppi;

	/*
	 * packages for returns
	 */
	dsdt_line("Name(TPM2, Package(2) {0, 0})");
	dsdt_line("Name(TPM3, Package(3) {0, 0, 0})");

	if (ppi->write_dsdt_regions) {
		error = ppi->write_dsdt_regions(tpm->ppi_sc);
		if (error) {
			warnx("%s: failed to write ppi dsdt regions\n",
			    __func__);
			return (error);
		}
	}

	/*
	 * Device Specific Method
	 * Arg0: UUID
	 * Arg1: Revision ID
	 * Arg2: Function Index
	 * Arg3: Arguments
	 */
	dsdt_line("Method(_DSM, 4, Serialized)");
	dsdt_line("{");
	dsdt_indent(1);
	if (ppi->write_dsdt_dsm) {
		error = ppi->write_dsdt_dsm(tpm->ppi_sc);
		if (error) {
			warnx("%s: failed to write ppi dsdt dsm\n", __func__);
			return (error);
		}
	}
	dsdt_unindent(1);
	dsdt_line("}");

	return (0);
}

static const struct acpi_device_emul tpm_acpi_device_emul = {
	.name = TPM_ACPI_DEVICE_NAME,
	.hid = TPM_ACPI_HARDWARE_ID,
	.build_table = tpm_build_acpi_table,
	.write_dsdt = tpm_write_dsdt,
};

void
tpm_device_destroy(struct tpm_device *const dev)
{
	if (dev == NULL)
		return;

	if (dev->ppi != NULL && dev->ppi->deinit != NULL)
		dev->ppi->deinit(dev->ppi_sc);
	if (dev->intf != NULL && dev->intf->deinit != NULL)
		dev->intf->deinit(dev->intf_sc);
	if (dev->emul != NULL && dev->emul->deinit != NULL)
		dev->emul->deinit(dev->emul_sc);

	acpi_device_destroy(dev->acpi_dev);
	free(dev);
}

int
tpm_device_create(struct tpm_device **const new_dev, struct vmctx *const vm_ctx,
    nvlist_t *const nvl)
{
	struct tpm_device *dev = NULL;
	struct tpm_emul **ppemul;
	struct tpm_intf **ppintf;
	struct tpm_ppi **pp_ppi;
	const char *value;
	int error;

	if (new_dev == NULL || vm_ctx == NULL) {
		error = EINVAL;
		goto err_out;
	}

	set_config_value_node_if_unset(nvl, "intf", "crb");
	set_config_value_node_if_unset(nvl, "ppi", "qemu");

	value = get_config_value_node(nvl, "version");
	assert(value != NULL);
	if (strcmp(value, "2.0")) {
		warnx("%s: unsupported tpm version %s", __func__, value);
		error = EINVAL;
		goto err_out;
	}

	dev = calloc(1, sizeof(*dev));
	if (dev == NULL) {
		error = ENOMEM;
		goto err_out;
	}

	dev->vm_ctx = vm_ctx;

	error = acpi_device_create(&dev->acpi_dev, dev, dev->vm_ctx,
	    &tpm_acpi_device_emul);
	if (error)
		goto err_out;

	value = get_config_value_node(nvl, "type");
	assert(value != NULL);
	SET_FOREACH(ppemul, tpm_emul_set) {
		if (strcmp(value, (*ppemul)->name))
			continue;
		dev->emul = *ppemul;
		break;
	}
	if (dev->emul == NULL) {
		warnx("TPM emulation \"%s\" not found", value);
		error = EINVAL;
		goto err_out;
	}

	if (dev->emul->init) {
		error = dev->emul->init(&dev->emul_sc, nvl);
		if (error)
			goto err_out;
	}

	value = get_config_value_node(nvl, "intf");
	SET_FOREACH(ppintf, tpm_intf_set) {
		if (strcmp(value, (*ppintf)->name)) {
			continue;
		}
		dev->intf = *ppintf;
		break;
	}
	if (dev->intf == NULL) {
		warnx("TPM interface \"%s\" not found", value);
		error = EINVAL;
		goto err_out;
	}

	if (dev->intf->init) {
		error = dev->intf->init(&dev->intf_sc, dev->emul, dev->emul_sc,
		    dev->acpi_dev);
		if (error)
			goto err_out;
	}

	value = get_config_value_node(nvl, "ppi");
	SET_FOREACH(pp_ppi, tpm_ppi_set) {
		if (strcmp(value, (*pp_ppi)->name)) {
			continue;
		}
		dev->ppi = *pp_ppi;
		break;
	}
	if (dev->ppi == NULL) {
		warnx("TPM PPI \"%s\" not found\n", value);
		error = EINVAL;
		goto err_out;
	}

	if (dev->ppi->init) {
		error = dev->ppi->init(&dev->ppi_sc);
		if (error)
			goto err_out;
	}

	*new_dev = dev;

	return (0);

err_out:
	tpm_device_destroy(dev);

	return (error);
}

static struct tpm_device *lpc_tpm;

int
init_tpm(struct vmctx *ctx)
{
	nvlist_t *nvl;
	int error;

	nvl = find_config_node("tpm");
	if (nvl == NULL)
		return (0);

	error = tpm_device_create(&lpc_tpm, ctx, nvl);
	if (error) {
		warnx("%s: unable to create a TPM device (%d)",
		    __func__, error);
		return (error);
	}

	return (0);
}
