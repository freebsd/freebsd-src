/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019-2020 Vladimir Kondratyev <wulf@FreeBSD.org>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sbuf.h>

#include <machine/resource.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <dev/acpica/acpivar.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#define	ACPI_IICBUS_LOCAL_BUFSIZE	32	/* Fits max SMBUS block size */

/*
 * Make a copy of ACPI_RESOURCE_I2C_SERIALBUS type and replace "pointer to ACPI
 * object name string" field with pointer to ACPI object itself.
 * This saves us extra strdup()/free() pair on acpi_iicbus_get_i2cres call.
 */
typedef	ACPI_RESOURCE_I2C_SERIALBUS	ACPI_IICBUS_RESOURCE_I2C_SERIALBUS;
#define	ResourceSource_Handle	ResourceSource.StringPtr

/* Hooks for the ACPI CA debugging infrastructure. */
#define	_COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("IIC")

struct gsb_buffer {
	UINT8 status;
	UINT8 len;
	UINT8 data[];
} __packed;

struct acpi_iicbus_softc {
	struct iicbus_softc	super_sc;
	ACPI_CONNECTION_INFO	space_handler_info;
	bool			space_handler_installed;
};

struct acpi_iicbus_ivars {
	struct iicbus_ivar	super_ivar;
	ACPI_HANDLE		handle;
};

static int install_space_handler = 0;
TUNABLE_INT("hw.iicbus.enable_acpi_space_handler", &install_space_handler);

static inline bool
acpi_resource_is_i2c_serialbus(ACPI_RESOURCE *res)
{

	return (res->Type == ACPI_RESOURCE_TYPE_SERIAL_BUS &&
	    res->Data.CommonSerialBus.Type == ACPI_RESOURCE_SERIAL_TYPE_I2C);
}

/*
 * IICBUS Address space handler
 */
static int
acpi_iicbus_sendb(device_t dev, u_char slave, char byte)
{
	struct iic_msg msgs[] = {
	    { slave, IIC_M_WR, 1, &byte },
	};

	return (iicbus_transfer(dev, msgs, nitems(msgs)));
}

static int
acpi_iicbus_recvb(device_t dev, u_char slave, char *byte)
{
	char buf;
	struct iic_msg msgs[] = {
	    { slave, IIC_M_RD, 1, &buf },
	};
	int error;

	error = iicbus_transfer(dev, msgs, nitems(msgs));
	if (error == 0)
		*byte = buf;

	return (error);
}

static int
acpi_iicbus_write(device_t dev, u_char slave, char cmd, void *buf,
    uint16_t buflen)
{
	struct iic_msg msgs[] = {
	    { slave, IIC_M_WR | IIC_M_NOSTOP, 1, &cmd },
	    { slave, IIC_M_WR | IIC_M_NOSTART, buflen, buf },
	};

	return (iicbus_transfer(dev, msgs, nitems(msgs)));
}

static int
acpi_iicbus_read(device_t dev, u_char slave, char cmd, void *buf,
    uint16_t buflen)
{
	uint8_t local_buffer[ACPI_IICBUS_LOCAL_BUFSIZE];
	struct iic_msg msgs[] = {
	    { slave, IIC_M_WR | IIC_M_NOSTOP, 1, &cmd },
	    { slave, IIC_M_RD, buflen, NULL },
	};
	int error;

	if (buflen <= sizeof(local_buffer))
		msgs[1].buf = local_buffer;
	else
		msgs[1].buf = malloc(buflen, M_DEVBUF, M_WAITOK);
	error = iicbus_transfer(dev, msgs, nitems(msgs));
	if (error == 0)
		memcpy(buf, msgs[1].buf, buflen);
	if (msgs[1].buf != local_buffer)
		free(msgs[1].buf, M_DEVBUF);

	return (error);
}

static int
acpi_iicbus_bwrite(device_t dev, u_char slave, char cmd, u_char count, char *buf)
{
	uint8_t bytes[2] = { cmd, count };
	struct iic_msg msgs[] = {
	    { slave, IIC_M_WR | IIC_M_NOSTOP, nitems(bytes), bytes },
	    { slave, IIC_M_WR | IIC_M_NOSTART, count, buf },
	};

	if (count == 0)
		return (errno2iic(EINVAL));

	return (iicbus_transfer(dev, msgs, nitems(msgs)));
}

static int
acpi_iicbus_bread(device_t dev, u_char slave, char cmd, u_char *count, char *buf)
{
	uint8_t local_buffer[ACPI_IICBUS_LOCAL_BUFSIZE];
	u_char len;
	struct iic_msg msgs[] = {
	    { slave, IIC_M_WR | IIC_M_NOSTOP, 1, &cmd },
	    { slave, IIC_M_RD | IIC_M_NOSTOP, 1, &len },
	};
	struct iic_msg block_msg[] = {
	    { slave, IIC_M_RD | IIC_M_NOSTART, 0, NULL },
	};
	device_t parent = device_get_parent(dev);
	int error;

	/* Have to do this because the command is split in two transfers. */
	error = iicbus_request_bus(parent, dev, IIC_WAIT);
	if (error == 0)
		error = iicbus_transfer(dev, msgs, nitems(msgs));
	if (error == 0) {
		/*
		 * If the slave offers an empty reply,
		 * read one byte to generate the stop or abort.
		 */
		if (len == 0)
			block_msg[0].len = 1;
		else
			block_msg[0].len = len;
		if (len <= sizeof(local_buffer))
			block_msg[0].buf = local_buffer;
		else
			block_msg[0].buf = malloc(len, M_DEVBUF, M_WAITOK);
		error = iicbus_transfer(dev, block_msg, nitems(block_msg));
		if (len == 0)
			error = errno2iic(EBADMSG);
		if (error == 0) {
			*count = len;
			memcpy(buf, block_msg[0].buf, len);
		}
		if (block_msg[0].buf != local_buffer)
			free(block_msg[0].buf, M_DEVBUF);
	}
	(void)iicbus_release_bus(parent, dev);
	return (error);
}

static ACPI_STATUS
acpi_iicbus_space_handler(UINT32 Function, ACPI_PHYSICAL_ADDRESS Address,
    UINT32 BitWidth, UINT64 *Value, void *HandlerContext, void *RegionContext)
{
	struct gsb_buffer *gsb;
	struct acpi_iicbus_softc *sc;
	device_t dev;
	ACPI_CONNECTION_INFO *info;
	ACPI_RESOURCE_I2C_SERIALBUS *sb;
	ACPI_RESOURCE *res;
	ACPI_STATUS s;
	int val;

	gsb = (struct gsb_buffer *)Value;
	if (gsb == NULL)
		return (AE_BAD_PARAMETER);

	info = HandlerContext;
	s = AcpiBufferToResource(info->Connection, info->Length, &res);
	if (ACPI_FAILURE(s))
		return (s);

	if (!acpi_resource_is_i2c_serialbus(res)) {
		s = AE_BAD_PARAMETER;
		goto err;
	}

	sb = &res->Data.I2cSerialBus;

	/* XXX Ignore 10bit addressing for now */
	if (sb->AccessMode == ACPI_I2C_10BIT_MODE) {
		s = AE_BAD_PARAMETER;
		goto err;
	}

#define	AML_FIELD_ATTRIB_MASK		0x0F
#define	AML_FIELD_ATTRIO(attr, io)	(((attr) << 16) | (io))

	Function &= AML_FIELD_ATTRIO(AML_FIELD_ATTRIB_MASK, ACPI_IO_MASK);
	sc = __containerof(info, struct acpi_iicbus_softc, space_handler_info);
	dev = sc->super_sc.dev;

	/* the address is expected to need shifting */
	sb->SlaveAddress <<= 1;

	switch (Function) {
	case AML_FIELD_ATTRIO(AML_FIELD_ATTRIB_SEND_RECEIVE, ACPI_READ):
		val = acpi_iicbus_recvb(dev, sb->SlaveAddress, gsb->data);
		break;

	case AML_FIELD_ATTRIO(AML_FIELD_ATTRIB_SEND_RECEIVE, ACPI_WRITE):
		val = acpi_iicbus_sendb(dev, sb->SlaveAddress, gsb->data[0]);
		break;

	case AML_FIELD_ATTRIO(AML_FIELD_ATTRIB_BYTE, ACPI_READ):
		val = acpi_iicbus_read(dev, sb->SlaveAddress, Address,
		    gsb->data, 1);
		break;

	case AML_FIELD_ATTRIO(AML_FIELD_ATTRIB_BYTE, ACPI_WRITE):
		val = acpi_iicbus_write(dev, sb->SlaveAddress, Address,
		    gsb->data, 1);
		break;

	case AML_FIELD_ATTRIO(AML_FIELD_ATTRIB_WORD, ACPI_READ):
		val = acpi_iicbus_read(dev, sb->SlaveAddress, Address,
		    gsb->data, 2);
		break;

	case AML_FIELD_ATTRIO(AML_FIELD_ATTRIB_WORD, ACPI_WRITE):
		val = acpi_iicbus_write(dev, sb->SlaveAddress, Address,
		    gsb->data, 2);
		break;

	case AML_FIELD_ATTRIO(AML_FIELD_ATTRIB_BLOCK, ACPI_READ):
		val = acpi_iicbus_bread(dev, sb->SlaveAddress, Address,
		    &gsb->len, gsb->data);
		break;

	case AML_FIELD_ATTRIO(AML_FIELD_ATTRIB_BLOCK, ACPI_WRITE):
		val = acpi_iicbus_bwrite(dev, sb->SlaveAddress, Address,
		    gsb->len, gsb->data);
		break;

	case AML_FIELD_ATTRIO(AML_FIELD_ATTRIB_BYTES, ACPI_READ):
		val = acpi_iicbus_read(dev, sb->SlaveAddress, Address,
		    gsb->data, info->AccessLength);
		break;

	case AML_FIELD_ATTRIO(AML_FIELD_ATTRIB_BYTES, ACPI_WRITE):
		val = acpi_iicbus_write(dev, sb->SlaveAddress, Address,
		    gsb->data, info->AccessLength);
		break;

	default:
		device_printf(dev, "protocol(0x%04x) is not supported.\n",
		    Function);
		s = AE_BAD_PARAMETER;
		goto err;
	}

	gsb->status = val;

err:
	ACPI_FREE(res);

	return (s);
}

static int
acpi_iicbus_install_address_space_handler(struct acpi_iicbus_softc *sc)
{
	ACPI_HANDLE handle;
	ACPI_STATUS s;

	handle = acpi_get_handle(device_get_parent(sc->super_sc.dev));
	s = AcpiInstallAddressSpaceHandler(handle, ACPI_ADR_SPACE_GSBUS,
	    &acpi_iicbus_space_handler, NULL, &sc->space_handler_info);
	if (ACPI_FAILURE(s)) {
		device_printf(sc->super_sc.dev,
		    "Failed to install GSBUS Address Space Handler in ACPI\n");
		return (ENXIO);
	}

	return (0);
}

static int
acpi_iicbus_remove_address_space_handler(struct acpi_iicbus_softc *sc)
{
	ACPI_HANDLE handle;
	ACPI_STATUS s;

	handle = acpi_get_handle(device_get_parent(sc->super_sc.dev));
	s = AcpiRemoveAddressSpaceHandler(handle, ACPI_ADR_SPACE_GSBUS,
	    &acpi_iicbus_space_handler);
	if (ACPI_FAILURE(s)) {
		device_printf(sc->super_sc.dev,
		    "Failed to remove GSBUS Address Space Handler from ACPI\n");
		return (ENXIO);
	}

	return (0);
}

static ACPI_STATUS
acpi_iicbus_get_i2cres_cb(ACPI_RESOURCE *res, void *context)
{
	ACPI_IICBUS_RESOURCE_I2C_SERIALBUS *sb = context;
	ACPI_STATUS status;
	ACPI_HANDLE handle;

	if (acpi_resource_is_i2c_serialbus(res)) {
		status = AcpiGetHandle(ACPI_ROOT_OBJECT,
		    res->Data.I2cSerialBus.ResourceSource.StringPtr, &handle);
		if (ACPI_FAILURE(status))
			return (status);
		memcpy(sb, &res->Data.I2cSerialBus,
		    sizeof(ACPI_IICBUS_RESOURCE_I2C_SERIALBUS));
		/*
		 * replace "pointer to ACPI object name string" field
		 * with pointer to ACPI object itself.
		 */
		sb->ResourceSource_Handle = handle;
		return (AE_CTRL_TERMINATE);
	} else if (res->Type == ACPI_RESOURCE_TYPE_END_TAG)
		return (AE_NOT_FOUND);

	return (AE_OK);
}

static ACPI_STATUS
acpi_iicbus_get_i2cres(ACPI_HANDLE handle, ACPI_RESOURCE_I2C_SERIALBUS *sb)
{

	return (AcpiWalkResources(handle, "_CRS",
	    acpi_iicbus_get_i2cres_cb, sb));
}

static ACPI_STATUS
acpi_iicbus_parse_resources_cb(ACPI_RESOURCE *res, void *context)
{
	device_t dev = context;
	struct iicbus_ivar *super_devi = device_get_ivars(dev);
	struct resource_list *rl = &super_devi->rl;
	int irq, gpio_pin;

	switch(res->Type) {
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		if (res->Data.ExtendedIrq.InterruptCount > 0) {
			irq = res->Data.ExtendedIrq.Interrupts[0];
			if (bootverbose)
				printf("  IRQ:               %d\n", irq);
			resource_list_add_next(rl, SYS_RES_IRQ, irq, irq, 1);
			return (AE_CTRL_TERMINATE);
		}
		break;
	case ACPI_RESOURCE_TYPE_GPIO:
		if (res->Data.Gpio.ConnectionType ==
		    ACPI_RESOURCE_GPIO_TYPE_INT) {
			/* Not supported by FreeBSD yet */
			gpio_pin = res->Data.Gpio.PinTable[0];
			if (bootverbose)
				printf("  GPIO IRQ pin:      %d\n", gpio_pin);
			return (AE_CTRL_TERMINATE);
		}
		break;
	default:
		break;
	}

	return (AE_OK);
}

static ACPI_STATUS
acpi_iicbus_parse_resources(ACPI_HANDLE handle, device_t dev)
{

	return (AcpiWalkResources(handle, "_CRS",
	    acpi_iicbus_parse_resources_cb, dev));
}

static void
acpi_iicbus_dump_res(device_t dev, ACPI_IICBUS_RESOURCE_I2C_SERIALBUS *sb)
{
	device_printf(dev, "found ACPI child\n");
	printf("  SlaveAddress:      0x%04hx\n", sb->SlaveAddress);
	printf("  ConnectionSpeed:   %uHz\n", sb->ConnectionSpeed);
	printf("  SlaveMode:         %s\n",
	    sb->SlaveMode == ACPI_CONTROLLER_INITIATED ?
	    "ControllerInitiated" : "DeviceInitiated");
	printf("  AddressingMode:    %uBit\n", sb->AccessMode == 0 ? 7 : 10);
	printf("  ConnectionSharing: %s\n", sb->ConnectionSharing == 0 ?
	    "Exclusive" : "Shared");
}

static device_t
acpi_iicbus_add_child(device_t dev, u_int order, const char *name, int unit)
{

	return (iicbus_add_child_common(
	    dev, order, name, unit, sizeof(struct acpi_iicbus_ivars)));
}

static ACPI_STATUS
acpi_iicbus_enumerate_child(ACPI_HANDLE handle, UINT32 level,
    void *context, void **result)
{
	device_t iicbus, child, acpi_child, acpi0;
	struct iicbus_softc *super_sc;
	ACPI_IICBUS_RESOURCE_I2C_SERIALBUS sb;
	ACPI_STATUS status;
	UINT32 sta;

	iicbus = context;
	super_sc = device_get_softc(iicbus);

	/*
	 * If no _STA method or if it failed, then assume that
	 * the device is present.
	 */
	if (!ACPI_FAILURE(acpi_GetInteger(handle, "_STA", &sta)) &&
	    !ACPI_DEVICE_PRESENT(sta))
		return (AE_OK);

	if (!acpi_has_hid(handle))
		return (AE_OK);

	/*
	 * Read "I2C Serial Bus Connection Resource Descriptor"
	 * described in p.19.6.57 of ACPI specification.
	 */
	bzero(&sb, sizeof(ACPI_IICBUS_RESOURCE_I2C_SERIALBUS));
	if (ACPI_FAILURE(acpi_iicbus_get_i2cres(handle, &sb)) ||
	    sb.SlaveAddress == 0)
		return (AE_OK);
	if (sb.ResourceSource_Handle !=
	    acpi_get_handle(device_get_parent(iicbus)))
		return (AE_OK);
	if (bootverbose)
		acpi_iicbus_dump_res(iicbus, &sb);

	/* Find out speed of the slowest slave */
	if (super_sc->bus_freq == 0 || super_sc->bus_freq > sb.ConnectionSpeed)
		super_sc->bus_freq = sb.ConnectionSpeed;

	/* Delete existing child of acpi bus */
	acpi_child = acpi_get_device(handle);
	if (acpi_child != NULL) {
		acpi0 = devclass_get_device(devclass_find("acpi"), 0);
		if (device_get_parent(acpi_child) != acpi0)
			return (AE_OK);

		if (device_is_attached(acpi_child))
			return (AE_OK);

		if (device_delete_child(acpi0, acpi_child) != 0)
			return (AE_OK);
	}

	child = BUS_ADD_CHILD(iicbus, 0, NULL, -1);
	if (child == NULL) {
		device_printf(iicbus, "add child failed\n");
		return (AE_OK);
	}

	iicbus_set_addr(child, sb.SlaveAddress);
	acpi_set_handle(child, handle);
	(void)acpi_iicbus_parse_resources(handle, child);

	/*
	 * Update ACPI-CA to use the IIC enumerated device_t for this handle.
	 */
	status = AcpiAttachData(handle, acpi_fake_objhandler, child);
	if (ACPI_FAILURE(status))
		printf("WARNING: Unable to attach object data to %s - %s\n",
		    acpi_name(handle), AcpiFormatException(status));

	return (AE_OK);
}

static ACPI_STATUS
acpi_iicbus_enumerate_children(device_t dev)
{

	return (AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
	    ACPI_UINT32_MAX, acpi_iicbus_enumerate_child, NULL, dev, NULL));
}

static void
acpi_iicbus_set_power_children(device_t dev, int state, bool all_children)
{
	device_t *devlist;
	int i, numdevs;

	if (device_get_children(dev, &devlist, &numdevs) != 0)
		return;

	for (i = 0; i < numdevs; i++)
		if (all_children || device_is_attached(devlist[i]) != 0)
			acpi_set_powerstate(devlist[i], state);

	free(devlist, M_TEMP);
}

static int
acpi_iicbus_probe(device_t dev)
{
	ACPI_HANDLE handle;
	device_t controller;

	if (acpi_disabled("iicbus"))
		return (ENXIO);

	controller = device_get_parent(dev);
	if (controller == NULL)
		return (ENXIO);

	handle = acpi_get_handle(controller);
	if (handle == NULL)
		return (ENXIO);

	device_set_desc(dev, "Philips I2C bus (ACPI-hinted)");
	return (BUS_PROBE_DEFAULT);
}

static int
acpi_iicbus_attach(device_t dev)
{
	struct acpi_iicbus_softc *sc = device_get_softc(dev);
	int error;

	if (ACPI_FAILURE(acpi_iicbus_enumerate_children(dev)))
		device_printf(dev, "children enumeration failed\n");

	acpi_iicbus_set_power_children(dev, ACPI_STATE_D0, true);
	error = iicbus_attach_common(dev, sc->super_sc.bus_freq);
	if (error == 0 && install_space_handler != 0 &&
	    acpi_iicbus_install_address_space_handler(sc) == 0)
		sc->space_handler_installed = true;

	return (error);
}

static int
acpi_iicbus_detach(device_t dev)
{
	struct acpi_iicbus_softc *sc = device_get_softc(dev);

	if (sc->space_handler_installed)
		acpi_iicbus_remove_address_space_handler(sc);
	acpi_iicbus_set_power_children(dev, ACPI_STATE_D3, false);

	return (iicbus_detach(dev));
}

static int
acpi_iicbus_suspend(device_t dev)
{
	int error;

	error = bus_generic_suspend(dev);
	if (error == 0)
		acpi_iicbus_set_power_children(dev, ACPI_STATE_D3, false);

	return (error);
}

static int
acpi_iicbus_resume(device_t dev)
{

	acpi_iicbus_set_power_children(dev, ACPI_STATE_D0, false);

	return (bus_generic_resume(dev));
}

/*
 * If this device is an ACPI child but no one claimed it, attempt
 * to power it off.  We'll power it back up when a driver is added.
 */
static void
acpi_iicbus_probe_nomatch(device_t bus, device_t child)
{

	iicbus_probe_nomatch(bus, child);
	acpi_set_powerstate(child, ACPI_STATE_D3);
}

/*
 * If a new driver has a chance to probe a child, first power it up.
 */
static void
acpi_iicbus_driver_added(device_t dev, driver_t *driver)
{
	device_t child, *devlist;
	int i, numdevs;

	DEVICE_IDENTIFY(driver, dev);
	if (device_get_children(dev, &devlist, &numdevs) != 0)
		return;

	for (i = 0; i < numdevs; i++) {
		child = devlist[i];
		if (device_get_state(child) == DS_NOTPRESENT) {
			acpi_set_powerstate(child, ACPI_STATE_D0);
			if (device_probe_and_attach(child) != 0)
				acpi_set_powerstate(child, ACPI_STATE_D3);
		}
	}
	free(devlist, M_TEMP);
}

static void
acpi_iicbus_child_deleted(device_t bus, device_t child)
{
	struct acpi_iicbus_ivars *devi = device_get_ivars(child);

	if (acpi_get_device(devi->handle) == child)
		AcpiDetachData(devi->handle, acpi_fake_objhandler);
}

static int
acpi_iicbus_read_ivar(device_t bus, device_t child, int which, uintptr_t *res)
{
	struct acpi_iicbus_ivars *devi = device_get_ivars(child);

	switch (which) {
	case ACPI_IVAR_HANDLE:
		*res = (uintptr_t)devi->handle;
		break;
	default:
		return (iicbus_read_ivar(bus, child, which, res));
	}

	return (0);
}

static int
acpi_iicbus_write_ivar(device_t bus, device_t child, int which, uintptr_t val)
{
	struct acpi_iicbus_ivars *devi = device_get_ivars(child);

	switch (which) {
	case ACPI_IVAR_HANDLE:
		if (devi->handle != NULL)
			return (EINVAL);
		devi->handle = (ACPI_HANDLE)val;
		break;
	default:
		return (iicbus_write_ivar(bus, child, which, val));
	}

	return (0);
}

/* Location hint for devctl(8). Concatenate IIC and ACPI hints. */
static int
acpi_iicbus_child_location(device_t bus, device_t child, struct sbuf *sb)
{
	struct acpi_iicbus_ivars *devi = device_get_ivars(child);
	int error;

	/* read IIC location hint string into the buffer. */
	error = iicbus_child_location(bus, child, sb);
	if (error != 0)
		return (error);

	/* Place ACPI string right after IIC one's terminating NUL. */
	if (devi->handle != NULL)
		sbuf_printf(sb, " handle=%s", acpi_name(devi->handle));

	return (0);
}

/* PnP information for devctl(8). Concatenate IIC and ACPI info strings. */
static int
acpi_iicbus_child_pnpinfo(device_t bus, device_t child, struct sbuf *sb)
{
	struct acpi_iicbus_ivars *devi = device_get_ivars(child);
	int error;

	/* read IIC PnP string into the buffer. */
	error = iicbus_child_pnpinfo(bus, child, sb);
	if (error != 0)
		return (error);

	if (devi->handle == NULL)
		return (0);

	error = acpi_pnpinfo(devi->handle, sb);

	return (error);
}

static device_method_t acpi_iicbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		acpi_iicbus_probe),
	DEVMETHOD(device_attach,	acpi_iicbus_attach),
	DEVMETHOD(device_detach,	acpi_iicbus_detach),
	DEVMETHOD(device_suspend,	acpi_iicbus_suspend),
	DEVMETHOD(device_resume,	acpi_iicbus_resume),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	acpi_iicbus_add_child),
	DEVMETHOD(bus_probe_nomatch,	acpi_iicbus_probe_nomatch),
	DEVMETHOD(bus_driver_added,	acpi_iicbus_driver_added),
	DEVMETHOD(bus_child_deleted,	acpi_iicbus_child_deleted),
	DEVMETHOD(bus_read_ivar,	acpi_iicbus_read_ivar),
	DEVMETHOD(bus_write_ivar,	acpi_iicbus_write_ivar),
	DEVMETHOD(bus_child_location,	acpi_iicbus_child_location),
	DEVMETHOD(bus_child_pnpinfo,	acpi_iicbus_child_pnpinfo),
	DEVMETHOD(bus_get_device_path,	acpi_get_acpi_device_path),

	DEVMETHOD_END,
};

DEFINE_CLASS_1(iicbus, acpi_iicbus_driver, acpi_iicbus_methods,
    sizeof(struct acpi_iicbus_softc), iicbus_driver);
MODULE_VERSION(acpi_iicbus, 1);
MODULE_DEPEND(acpi_iicbus, acpi, 1, 1, 1);
