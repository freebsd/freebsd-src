/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
 * Creates an ACPI device.
 *
 * @param[out] new_dev Returns the newly create ACPI device.
 * @param[in]  vm_ctx  VM context the ACPI device is created in.
 * @param[in]  name    Name of the ACPI device. Should always be a NULL
 *                     terminated string.
 * @param[in]  hid     Hardware ID of the ACPI device. Should always be a NULL
 *                     terminated string.
 */
int acpi_device_create(struct acpi_device **new_dev, struct vmctx *vm_ctx,
    const char *name, const char *hid);
void acpi_device_destroy(struct acpi_device *dev);

int acpi_device_add_res_fixed_ioport(struct acpi_device *dev, UINT16 port,
    UINT8 length);
int acpi_device_add_res_fixed_memory32(struct acpi_device *dev,
    UINT8 write_protected, UINT32 address, UINT32 length);

void acpi_device_write_dsdt(const struct acpi_device *dev);
