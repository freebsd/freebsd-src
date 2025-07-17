/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <contrib/dev/acpica/include/acpi.h>
#pragma GCC diagnostic pop

struct vmctx;

struct acpi_device;

/**
 * Device specific information and emulation.
 *
 * @param name        Used as device name in the DSDT.
 * @param hid         Used as _HID in the DSDT.
 * @param build_table Called to build a device specific ACPI table like the TPM2
 *                    table.
 * @param write_dsdt  Called to append the DSDT with device specific
 *                    information.
 */
struct acpi_device_emul {
	const char *name;
	const char *hid;

	int (*build_table)(const struct acpi_device *dev);
	int (*write_dsdt)(const struct acpi_device *dev);
};

/**
 * Creates an ACPI device.
 *
 * @param[out] new_dev Returns the newly create ACPI device.
 * @param[in]  softc   Pointer to the software context of the ACPI device.
 * @param[in]  vm_ctx  VM context the ACPI device is created in.
 * @param[in]  emul    Device emulation struct. It contains some information
 *                     like the name of the ACPI device and some device specific
 *                     functions.
 */
int acpi_device_create(struct acpi_device **new_dev, void *softc,
    struct vmctx *vm_ctx, const struct acpi_device_emul *emul);
void acpi_device_destroy(struct acpi_device *dev);

int acpi_device_add_res_fixed_ioport(struct acpi_device *dev, UINT16 port,
    UINT8 length);
int acpi_device_add_res_fixed_memory32(struct acpi_device *dev,
    UINT8 write_protected, UINT32 address, UINT32 length);

void *acpi_device_get_softc(const struct acpi_device *dev);

int acpi_device_build_table(const struct acpi_device *dev);
int acpi_device_write_dsdt(const struct acpi_device *dev);
