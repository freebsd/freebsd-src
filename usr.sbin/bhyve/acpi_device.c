/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#include <sys/param.h>
#include <sys/queue.h>

#include <machine/vmm.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <vmmapi.h>

#include "acpi.h"
#include "acpi_device.h"
#include "basl.h"

/**
 * List entry to enumerate all resources used by an ACPI device.
 *
 * @param chain Used to chain multiple elements together.
 * @param type  Type of the ACPI resource.
 * @param data  Data of the ACPI resource.
 */
struct acpi_resource_list_entry {
	SLIST_ENTRY(acpi_resource_list_entry) chain;
	UINT32 type;
	ACPI_RESOURCE_DATA data;
};

/**
 * Holds information about an ACPI device.
 *
 * @param vm_ctx VM context the ACPI device was created in.
 * @param softc  A pointer to the software context of the ACPI device.
 * @param emul   Device emulation struct. It contains some information like the
                 name of the ACPI device and some device specific functions.
 * @param crs    Current resources used by the ACPI device.
 */
struct acpi_device {
	struct vmctx *vm_ctx;
	void *softc;
	const struct acpi_device_emul *emul;
	SLIST_HEAD(acpi_resource_list, acpi_resource_list_entry) crs;
};

int
acpi_device_create(struct acpi_device **const new_dev, void *const softc,
    struct vmctx *const vm_ctx, const struct acpi_device_emul *const emul)
{
	assert(new_dev != NULL);
	assert(vm_ctx != NULL);
	assert(emul != NULL);

	struct acpi_device *const dev = calloc(1, sizeof(*dev));
	if (dev == NULL) {
		return (ENOMEM);
	}

	dev->vm_ctx = vm_ctx;
	dev->softc = softc;
	dev->emul = emul;
	SLIST_INIT(&dev->crs);

	const int error = acpi_tables_add_device(dev);
	if (error) {
		acpi_device_destroy(dev);
		return (error);
	}

	*new_dev = dev;

	return (0);
}

void
acpi_device_destroy(struct acpi_device *const dev)
{
	if (dev == NULL) {
		return;
	}

	struct acpi_resource_list_entry *res;
	while (!SLIST_EMPTY(&dev->crs)) {
		res = SLIST_FIRST(&dev->crs);
		SLIST_REMOVE_HEAD(&dev->crs, chain);
		free(res);
	}

	free(dev);
}

int
acpi_device_add_res_fixed_ioport(struct acpi_device *const dev,
    const UINT16 port, const UINT8 length)
{
	if (dev == NULL) {
		return (EINVAL);
	}

	struct acpi_resource_list_entry *const res = calloc(1, sizeof(*res));
	if (res == NULL) {
		return (ENOMEM);
	}

	res->type = ACPI_RESOURCE_TYPE_FIXED_IO;
	res->data.FixedIo.Address = port;
	res->data.FixedIo.AddressLength = length;

	SLIST_INSERT_HEAD(&dev->crs, res, chain);

	return (0);
}

int
acpi_device_add_res_fixed_memory32(struct acpi_device *const dev,
    const UINT8 write_protected, const UINT32 address, const UINT32 length)
{
	if (dev == NULL) {
		return (EINVAL);
	}

	struct acpi_resource_list_entry *const res = calloc(1, sizeof(*res));
	if (res == NULL) {
		return (ENOMEM);
	}

	res->type = ACPI_RESOURCE_TYPE_FIXED_MEMORY32;
	res->data.FixedMemory32.WriteProtect = write_protected;
	res->data.FixedMemory32.Address = address;
	res->data.FixedMemory32.AddressLength = length;

	SLIST_INSERT_HEAD(&dev->crs, res, chain);

	return (0);
}

void *
acpi_device_get_softc(const struct acpi_device *const dev)
{
	assert(dev != NULL);

	return (dev->softc);
}

int
acpi_device_build_table(const struct acpi_device *const dev)
{
	assert(dev != NULL);
	assert(dev->emul != NULL);

	if (dev->emul->build_table != NULL) {
		return (dev->emul->build_table(dev));
	}

	return (0);
}

static int
acpi_device_write_dsdt_crs(const struct acpi_device *const dev)
{
	const struct acpi_resource_list_entry *res;
	SLIST_FOREACH(res, &dev->crs, chain) {
		switch (res->type) {
		case ACPI_RESOURCE_TYPE_FIXED_IO:
			dsdt_fixed_ioport(res->data.FixedIo.Address,
			    res->data.FixedIo.AddressLength);
			break;
		case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
			dsdt_fixed_mem32(res->data.FixedMemory32.Address,
			    res->data.FixedMemory32.AddressLength);
			break;
		default:
			assert(0);
			break;
		}
	}

	return (0);
}

int
acpi_device_write_dsdt(const struct acpi_device *const dev)
{
	assert(dev != NULL);

	dsdt_line("");
	dsdt_line("  Scope (\\_SB)");
	dsdt_line("  {");
	dsdt_line("    Device (%s)", dev->emul->name);
	dsdt_line("    {");
	dsdt_line("      Name (_HID, \"%s\")", dev->emul->hid);
	dsdt_line("      Name (_STA, 0x0F)");
	dsdt_line("      Name (_CRS, ResourceTemplate ()");
	dsdt_line("      {");
	dsdt_indent(4);
	BASL_EXEC(acpi_device_write_dsdt_crs(dev));
	dsdt_unindent(4);
	dsdt_line("      })");
	if (dev->emul->write_dsdt != NULL) {
		dsdt_indent(3);
		BASL_EXEC(dev->emul->write_dsdt(dev));
		dsdt_unindent(3);
	}
	dsdt_line("    }");
	dsdt_line("  }");

	return (0);
}
