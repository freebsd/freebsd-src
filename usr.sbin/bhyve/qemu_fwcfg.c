/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#include <sys/param.h>

#include <machine/vmm.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "acpi_device.h"
#include "inout.h"
#include "qemu_fwcfg.h"

#define QEMU_FWCFG_ACPI_DEVICE_NAME "FWCF"
#define QEMU_FWCFG_ACPI_HARDWARE_ID "QEMU0002"

#define QEMU_FWCFG_SELECTOR_PORT_NUMBER 0x510
#define QEMU_FWCFG_SELECTOR_PORT_SIZE 1
#define QEMU_FWCFG_SELECTOR_PORT_FLAGS IOPORT_F_INOUT
#define QEMU_FWCFG_DATA_PORT_NUMBER 0x511
#define QEMU_FWCFG_DATA_PORT_SIZE 1
#define QEMU_FWCFG_DATA_PORT_FLAGS \
	IOPORT_F_INOUT /* QEMU v2.4+ ignores writes */

struct qemu_fwcfg_softc {
	struct acpi_device *acpi_dev;
};

static struct qemu_fwcfg_softc fwcfg_sc;

static int
qemu_fwcfg_selector_port_handler(struct vmctx *const ctx __unused, const int in,
    const int port __unused, const int bytes, uint32_t *const eax,
    void *const arg __unused)
{
	return (0);
}

static int
qemu_fwcfg_data_port_handler(struct vmctx *const ctx __unused, const int in,
    const int port __unused, const int bytes, uint32_t *const eax,
    void *const arg __unused)
{
	return (0);
}

static int
qemu_fwcfg_register_port(const char *const name, const int port, const int size,
    const int flags, const inout_func_t handler)
{
	struct inout_port iop;

	bzero(&iop, sizeof(iop));
	iop.name = name;
	iop.port = port;
	iop.size = size;
	iop.flags = flags;
	iop.handler = handler;

	return (register_inout(&iop));
}

int
qemu_fwcfg_init(struct vmctx *const ctx)
{
	int error;

	error = acpi_device_create(&fwcfg_sc.acpi_dev, ctx,
	    QEMU_FWCFG_ACPI_DEVICE_NAME, QEMU_FWCFG_ACPI_HARDWARE_ID);
	if (error) {
		warnx("%s: failed to create ACPI device for QEMU FwCfg",
		    __func__);
		goto done;
	}

	error = acpi_device_add_res_fixed_ioport(fwcfg_sc.acpi_dev,
	    QEMU_FWCFG_SELECTOR_PORT_NUMBER, 2);
	if (error) {
		warnx("%s: failed to add fixed IO port for QEMU FwCfg",
		    __func__);
		goto done;
	}

	/* add handlers for fwcfg ports */
	if ((error = qemu_fwcfg_register_port("qemu_fwcfg_selector",
	    QEMU_FWCFG_SELECTOR_PORT_NUMBER, QEMU_FWCFG_SELECTOR_PORT_SIZE,
	    QEMU_FWCFG_SELECTOR_PORT_FLAGS,
	    qemu_fwcfg_selector_port_handler)) != 0) {
		warnx("%s: Unable to register qemu fwcfg selector port 0x%x",
		    __func__, QEMU_FWCFG_SELECTOR_PORT_NUMBER);
		goto done;
	}
	if ((error = qemu_fwcfg_register_port("qemu_fwcfg_data",
	    QEMU_FWCFG_DATA_PORT_NUMBER, QEMU_FWCFG_DATA_PORT_SIZE,
	    QEMU_FWCFG_DATA_PORT_FLAGS, qemu_fwcfg_data_port_handler)) != 0) {
		warnx("%s: Unable to register qemu fwcfg data port 0x%x",
		    __func__, QEMU_FWCFG_DATA_PORT_NUMBER);
		goto done;
	}

done:
	if (error) {
		acpi_device_destroy(fwcfg_sc.acpi_dev);
	}

	return (error);
}
