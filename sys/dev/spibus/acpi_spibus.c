/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Vladimir Kondratyev <wulf@FreeBSD.org>
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
#include <sys/systm.h>

#include <machine/resource.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <dev/acpica/acpivar.h>

#include <dev/spibus/spibusvar.h>

/*
 * Make a copy of ACPI_RESOURCE_SPI_SERIALBUS type and replace "pointer to ACPI
 * object name string" field with pointer to ACPI object itself.
 * This saves us extra strdup()/free() pair on acpi_spibus_get_acpi_res call.
 */
typedef	ACPI_RESOURCE_SPI_SERIALBUS	ACPI_SPIBUS_RESOURCE_SPI_SERIALBUS;
#define	ResourceSource_Handle	ResourceSource.StringPtr

/* Hooks for the ACPI CA debugging infrastructure. */
#define	_COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("SPI")

#if defined (__amd64__) || defined (__i386__)
static bool is_apple;
#endif

struct acpi_spibus_ivar {
	struct spibus_ivar	super_ivar;
	ACPI_HANDLE		handle;
};

static inline bool
acpi_resource_is_spi_serialbus(ACPI_RESOURCE *res)
{
	return (res->Type == ACPI_RESOURCE_TYPE_SERIAL_BUS &&
	    res->Data.CommonSerialBus.Type == ACPI_RESOURCE_SERIAL_TYPE_SPI);
}

static ACPI_STATUS
acpi_spibus_get_acpi_res_cb(ACPI_RESOURCE *res, void *context)
{
	ACPI_SPIBUS_RESOURCE_SPI_SERIALBUS *sb = context;
	ACPI_STATUS status;
	ACPI_HANDLE handle;

	if (acpi_resource_is_spi_serialbus(res)) {
		status = AcpiGetHandle(ACPI_ROOT_OBJECT,
		    res->Data.SpiSerialBus.ResourceSource.StringPtr, &handle);
		if (ACPI_FAILURE(status))
			return (status);
		memcpy(sb, &res->Data.SpiSerialBus,
		    sizeof(ACPI_SPIBUS_RESOURCE_SPI_SERIALBUS));
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

static void
acpi_spibus_dump_res(device_t dev, ACPI_SPIBUS_RESOURCE_SPI_SERIALBUS *sb)
{
	device_printf(dev, "found ACPI child\n");
	printf("  DeviceSelection:   0x%04hx\n", sb->DeviceSelection);
	printf("  ConnectionSpeed:   %uHz\n", sb->ConnectionSpeed);
	printf("  WireMode:          %s\n",
	    sb->WireMode == ACPI_SPI_4WIRE_MODE ?
	    "FourWireMode" : "ThreeWireMode");
	printf("  DevicePolarity:    %s\n",
	     sb->DevicePolarity == ACPI_SPI_ACTIVE_LOW ?
	    "PolarityLow" : "PolarityHigh");
	printf("  DataBitLength:     %uBit\n", sb->DataBitLength);
	printf("  ClockPhase:        %s\n",
	    sb->ClockPhase == ACPI_SPI_FIRST_PHASE ?
	    "ClockPhaseFirst" : "ClockPhaseSecond");
	printf("  ClockPolarity:     %s\n",
	    sb->ClockPolarity == ACPI_SPI_START_LOW ?
	    "ClockPolarityLow" : "ClockPolarityHigh");
	printf("  SlaveMode:         %s\n",
	    sb->SlaveMode == ACPI_CONTROLLER_INITIATED ?
	    "ControllerInitiated" : "DeviceInitiated");
	printf("  ConnectionSharing: %s\n", sb->ConnectionSharing == 0 ?
	    "Exclusive" : "Shared");
}

static int
acpi_spibus_get_acpi_res(device_t spibus, ACPI_HANDLE dev,
    struct spibus_ivar *res)
{
	ACPI_SPIBUS_RESOURCE_SPI_SERIALBUS sb;

	/*
	 * Read "SPI Serial Bus Connection Resource Descriptor"
	 * described in p.19.6.126 of ACPI specification.
	 */
	bzero(&sb, sizeof(ACPI_SPIBUS_RESOURCE_SPI_SERIALBUS));
	if (ACPI_FAILURE(AcpiWalkResources(dev, "_CRS",
	    acpi_spibus_get_acpi_res_cb, &sb)))
		return (ENXIO);
	if (sb.ResourceSource_Handle !=
	    acpi_get_handle(device_get_parent(spibus)))
		return (ENXIO);
	if (bootverbose)
		acpi_spibus_dump_res(spibus, &sb);
	/*
	 * The Windows Baytrail and Braswell SPI host controller
	 * drivers uses 1 as the first (and only) value for ACPI
	 * DeviceSelection.
	 */
	if (sb.DeviceSelection != 0 &&
	    (acpi_MatchHid(sb.ResourceSource_Handle, "80860F0E") ||
	     acpi_MatchHid(sb.ResourceSource_Handle, "8086228E")))
		res->cs = sb.DeviceSelection - 1;
	else
		res->cs = sb.DeviceSelection;
	res->mode =
	    (sb.ClockPhase != ACPI_SPI_FIRST_PHASE ? SPIBUS_MODE_CPHA : 0) |
	    (sb.ClockPolarity != ACPI_SPI_START_LOW ? SPIBUS_MODE_CPOL : 0);
	res->clock =  sb.ConnectionSpeed;

	return (0);
}

#if defined (__amd64__) || defined (__i386__)
static int
acpi_spibus_get_apple_res(device_t spibus, ACPI_HANDLE dev,
    struct spibus_ivar *ivar)
{
	/* a0b5b7c6-1318-441c-b0c9-fe695eaf949b */
	static const uint8_t apple_guid[ACPI_UUID_LENGTH] = {
	    0xC6, 0xB7, 0xB5, 0xA0, 0x18, 0x13, 0x1C, 0x44,
	    0xB0, 0xC9, 0xFE, 0x69, 0x5E, 0xAF, 0x94, 0x9B,
	};
	ACPI_BUFFER buf;
	ACPI_OBJECT *pkg, *comp;
	ACPI_HANDLE parent;
	char *k;
	uint64_t val;

	/* Apple does not use _CRS but nested devices for SPI slaves */
	if (ACPI_FAILURE(AcpiGetParent(dev, &parent)))
		return (ENXIO);
	if (parent != acpi_get_handle(device_get_parent(spibus)))
		return (ENXIO);
	if (ACPI_FAILURE(acpi_EvaluateDSMTyped(dev, apple_guid,
			 1, 1, NULL, &buf, ACPI_TYPE_PACKAGE)))
		return (ENXIO);

	pkg = ((ACPI_OBJECT *)buf.Pointer);
	if (pkg->Package.Count % 2 != 0) {
		device_printf(spibus, "_DSM length %d not even\n",
		    pkg->Package.Count);
		AcpiOsFree(pkg);
		return (ENXIO);
	}

	if (bootverbose)
		device_printf(spibus, "found ACPI child\n");

	for (comp = pkg->Package.Elements;
	     comp < pkg->Package.Elements + pkg->Package.Count;
	     comp += 2) {

		if (comp[0].Type != ACPI_TYPE_STRING ||
		    comp[1].Type != ACPI_TYPE_BUFFER) {
			device_printf(spibus, "expected string+buffer, "
			    "got %d+%d\n", comp[0].Type, comp[1].Type);
			continue;
		}
		k = comp[0].String.Pointer;
		val = comp[1].Buffer.Length >= 8 ?
		    *(uint64_t *)comp[1].Buffer.Pointer : 0;

		if (bootverbose)
			printf("  %s: %ju\n", k, (intmax_t)val);

		if (strcmp(k, "spiSclkPeriod") == 0) {
			if (val != 0)
				ivar->clock = 1000000000 / val;
		} else if (strcmp(k, "spiSPO") == 0) {
			if (val != 0)
				ivar->mode |= SPIBUS_MODE_CPOL;
		} else if (strcmp(k, "spiSPH") == 0) {
			if (val != 0)
				ivar->mode |= SPIBUS_MODE_CPHA;
		} else if (strcmp(k, "spiCSDelay") == 0) {
			ivar->cs_delay = val;
		}
	}

	AcpiOsFree(pkg);

	return (0);
}
#endif

static int
acpi_spibus_delete_acpi_child(ACPI_HANDLE handle)
{
	device_t acpi_child, acpi0;

	/* Delete existing child of acpi bus */
	acpi_child = acpi_get_device(handle);
	if (acpi_child != NULL) {
		acpi0 = devclass_get_device(devclass_find("acpi"), 0);
		if (device_get_parent(acpi_child) != acpi0)
			return (ENXIO);

		if (device_is_attached(acpi_child))
			return (ENXIO);

		if (device_delete_child(acpi0, acpi_child) != 0)
			return (ENXIO);
	}

	return (0);
}

static device_t
acpi_spibus_add_child(device_t dev, u_int order, const char *name, int unit)
{
	return (spibus_add_child_common(
	    dev, order, name, unit, sizeof(struct acpi_spibus_ivar)));
}

static ACPI_STATUS
acpi_spibus_enumerate_child(ACPI_HANDLE handle, UINT32 level,
    void *context, void **result)
{
	device_t spibus, child;
	struct spibus_ivar res;
	ACPI_STATUS status;
	UINT32 sta;
	bool found = false;

	spibus = context;

	/*
	 * If no _STA method or if it failed, then assume that
	 * the device is present.
	 */
	if (!ACPI_FAILURE(acpi_GetInteger(handle, "_STA", &sta)) &&
	    !ACPI_DEVICE_PRESENT(sta))
		return (AE_OK);

	if (!acpi_has_hid(handle))
		return (AE_OK);

	bzero(&res, sizeof(res));
	if (acpi_spibus_get_acpi_res(spibus, handle, &res) == 0)
		found = true;
#if defined (__amd64__) || defined (__i386__)
	if (!found && is_apple &&
	    acpi_spibus_get_apple_res(spibus, handle, &res) == 0)
		found = true;
#endif
	if (!found || res.clock == 0)
		return (AE_OK);

	/* Delete existing child of acpi bus */
	if (acpi_spibus_delete_acpi_child(handle) != 0)
		return (AE_OK);

	child = BUS_ADD_CHILD(spibus, 0, NULL, -1);
	if (child == NULL) {
		device_printf(spibus, "add child failed\n");
		return (AE_OK);
	}

	spibus_set_cs(child, res.cs);
	spibus_set_mode(child, res.mode);
	spibus_set_clock(child, res.clock);
	spibus_set_cs_delay(child, res.cs_delay);
	acpi_set_handle(child, handle);
	acpi_parse_resources(child, handle, &acpi_res_parse_set, NULL);

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
acpi_spibus_enumerate_children(device_t dev)
{
	return (AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
	    ACPI_UINT32_MAX, acpi_spibus_enumerate_child, NULL, dev, NULL));
}

static void
acpi_spibus_set_power_children(device_t dev, int state, bool all_children)
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
acpi_spibus_probe(device_t dev)
{
	ACPI_HANDLE handle;
	device_t controller;

	if (acpi_disabled("spibus"))
		return (ENXIO);

	controller = device_get_parent(dev);
	if (controller == NULL)
		return (ENXIO);

	handle = acpi_get_handle(controller);
	if (handle == NULL)
		return (ENXIO);

	device_set_desc(dev, "SPI bus (ACPI-hinted)");
	return (BUS_PROBE_DEFAULT + 1);
}

static int
acpi_spibus_attach(device_t dev)
{

#if defined (__amd64__) || defined (__i386__)
	char *vendor = kern_getenv("smbios.bios.vendor");
	if (vendor != NULL &&
	    (strcmp(vendor, "Apple Inc.") == 0 ||
	     strcmp(vendor, "Apple Computer, Inc.") == 0))
		is_apple = true;
#endif

	if (ACPI_FAILURE(acpi_spibus_enumerate_children(dev)))
		device_printf(dev, "children enumeration failed\n");

	acpi_spibus_set_power_children(dev, ACPI_STATE_D0, true);
	return (spibus_attach(dev));
}

static int
acpi_spibus_detach(device_t dev)
{
	acpi_spibus_set_power_children(dev, ACPI_STATE_D3, false);

	return (spibus_detach(dev));
}

static int
acpi_spibus_suspend(device_t dev)
{
	acpi_spibus_set_power_children(dev, ACPI_STATE_D3, false);

	return (bus_generic_suspend(dev));
}

static int
acpi_spibus_resume(device_t dev)
{
	acpi_spibus_set_power_children(dev, ACPI_STATE_D0, false);

	return (bus_generic_resume(dev));
}

#ifndef INTRNG
/* Mostly copy of acpi_alloc_resource() */
static struct resource *
acpi_spibus_alloc_resource(device_t dev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	ACPI_RESOURCE ares;
	struct resource_list *rl;
	struct resource *res;

	if (device_get_parent(child) != dev)
		return (BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
		    type, rid, start, end, count, flags));

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (rl == NULL)
		return (NULL);

	res = resource_list_alloc(rl, dev, child, type, rid,
	    start, end, count, flags);
	if (res != NULL && type == SYS_RES_IRQ &&
	    ACPI_SUCCESS(acpi_lookup_irq_resource(child, *rid, res, &ares)))
		acpi_config_intr(child, &ares);

	return (res);
}
#endif

/*
 * If this device is an ACPI child but no one claimed it, attempt
 * to power it off.  We'll power it back up when a driver is added.
 */
static void
acpi_spibus_probe_nomatch(device_t bus, device_t child)
{
	spibus_probe_nomatch(bus, child);
	acpi_set_powerstate(child, ACPI_STATE_D3);
}

/*
 * If a new driver has a chance to probe a child, first power it up.
 */
static void
acpi_spibus_driver_added(device_t dev, driver_t *driver)
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
acpi_spibus_child_deleted(device_t bus, device_t child)
{
	struct acpi_spibus_ivar *devi = device_get_ivars(child);

	if (acpi_get_device(devi->handle) == child)
		AcpiDetachData(devi->handle, acpi_fake_objhandler);
}

static int
acpi_spibus_read_ivar(device_t bus, device_t child, int which, uintptr_t *res)
{
	struct acpi_spibus_ivar *devi = device_get_ivars(child);

	switch (which) {
	case ACPI_IVAR_HANDLE:
		*res = (uintptr_t)devi->handle;
		break;
	default:
		return (spibus_read_ivar(bus, child, which, res));
	}

	return (0);
}

static int
acpi_spibus_write_ivar(device_t bus, device_t child, int which, uintptr_t val)
{
	struct acpi_spibus_ivar *devi = device_get_ivars(child);

	switch (which) {
	case ACPI_IVAR_HANDLE:
		if (devi->handle != NULL)
			return (EINVAL);
		devi->handle = (ACPI_HANDLE)val;
		break;
	default:
		return (spibus_write_ivar(bus, child, which, val));
	}

	return (0);
}

/* Location hint for devctl(8). Concatenate IIC and ACPI hints. */
static int
acpi_spibus_child_location(device_t bus, device_t child, struct sbuf *sb)
{
	struct acpi_spibus_ivar *devi = device_get_ivars(child);
	int error;

	/* read SPI location hint string into the buffer. */
	error = spibus_child_location(bus, child, sb);
	if (error != 0)
		return (error);

	/* Place ACPI string right after IIC one's terminating NUL. */
	if (devi->handle != NULL)
		sbuf_printf(sb, " handle=%s", acpi_name(devi->handle));

	return (0);
}

/* PnP information for devctl(8). */
static int
acpi_spibus_child_pnpinfo(device_t bus, device_t child, struct sbuf *sb)
{
	struct acpi_spibus_ivar *devi = device_get_ivars(child);

	return (
	    devi->handle == NULL ? ENOTSUP : acpi_pnpinfo(devi->handle, sb));
}

static device_method_t acpi_spibus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		acpi_spibus_probe),
	DEVMETHOD(device_attach,	acpi_spibus_attach),
	DEVMETHOD(device_detach,	acpi_spibus_detach),
	DEVMETHOD(device_suspend,	acpi_spibus_suspend),
	DEVMETHOD(device_resume,	acpi_spibus_resume),

	/* Bus interface */
#ifndef INTRNG
	DEVMETHOD(bus_alloc_resource,   acpi_spibus_alloc_resource),
#endif
	DEVMETHOD(bus_add_child,	acpi_spibus_add_child),
	DEVMETHOD(bus_probe_nomatch,	acpi_spibus_probe_nomatch),
	DEVMETHOD(bus_driver_added,	acpi_spibus_driver_added),
	DEVMETHOD(bus_child_deleted,	acpi_spibus_child_deleted),
	DEVMETHOD(bus_read_ivar,	acpi_spibus_read_ivar),
	DEVMETHOD(bus_write_ivar,	acpi_spibus_write_ivar),
	DEVMETHOD(bus_child_location,	acpi_spibus_child_location),
	DEVMETHOD(bus_child_pnpinfo,	acpi_spibus_child_pnpinfo),
	DEVMETHOD(bus_get_device_path,	acpi_get_acpi_device_path),

	DEVMETHOD_END,
};

DEFINE_CLASS_1(spibus, acpi_spibus_driver, acpi_spibus_methods,
    sizeof(struct spibus_softc), spibus_driver);
DRIVER_MODULE(acpi_spibus, spi, acpi_spibus_driver, NULL, NULL);
MODULE_VERSION(acpi_spibus, 1);
MODULE_DEPEND(acpi_spibus, acpi, 1, 1, 1);
