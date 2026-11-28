/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1998 The NetBSD Foundation, Inc. All rights reserved.
 * Copyright (c) 1998 Lennart Augustsson. All rights reserved.
 * Copyright (c) 2008-2010 Hans Petter Selasky. All rights reserved.
 * Copyright (c) 2019 Takanori Watanabe.
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

/*
 * USB spec: http://www.usb.org/developers/docs/usbspec.zip
 */

#ifdef USB_GLOBAL_INCLUDE_FILE
#include USB_GLOBAL_INCLUDE_FILE
#else
#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#define	USB_DEBUG_VAR uhub_debug

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_request.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_hub.h>
#include <dev/usb/usb_util.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_transfer.h>
#include <dev/usb/usb_dynamic.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#endif					/* USB_GLOBAL_INCLUDE_FILE */
#include <dev/usb/usb_hub_private.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>
#include <sys/sbuf.h>

struct acpi_uhub_port {
	ACPI_HANDLE handle;
#define    ACPI_UPC_CONNECTABLE 0x80000000
#define    ACPI_UPC_PORTTYPE(x) ((x)&0xff)
	uint32_t upc;
	uint8_t	pld[ACPI_PLD_BUFFER_SIZE];
	size_t pld_len;
};

struct acpi_uhub_softc {
	struct uhub_softc usc;
	uint8_t	nports;
	ACPI_HANDLE ah;
	struct acpi_uhub_port *port;
	struct sysctl_ctx_list port_sysctl_ctx;
};

static const char *
acpi_uhub_upc_type(uint8_t type)
{
	const char *typelist[] = {"TypeA", "MiniAB", "Express",
				  "USB3-A", "USB3-B", "USB-MicroB",
				  "USB3-MicroAB", "USB3-PowerB",
				  "TypeC-USB2", "TypeC-Switch",
				  "TypeC-nonSwitch"};
	const int last = sizeof(typelist) / sizeof(typelist[0]);

	if (type == 0xff) {
		return "Proprietary";
	}

	return (type < last) ? typelist[type] : "Unknown";
}

static int
acpi_uhub_parse_upc(device_t dev, unsigned p, ACPI_HANDLE ah, struct sysctl_oid_list *poid)
{
	ACPI_BUFFER buf;
	struct acpi_uhub_softc *sc = device_get_softc(dev);
	struct acpi_uhub_port *port = &sc->port[p - 1];

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;

	if (AcpiEvaluateObject(ah, "_UPC", NULL, &buf) == AE_OK) {
		ACPI_OBJECT *obj = buf.Pointer;
		UINT64 porttypenum, conn;
		uint8_t *connectable;

		acpi_PkgInt(obj, 0, &conn);
		acpi_PkgInt(obj, 1, &porttypenum);
		connectable = conn ? "" : "non";

		port->upc = porttypenum;
		port->upc |= (conn) ? (ACPI_UPC_CONNECTABLE) : 0;

		if (usb_debug)
			device_printf(dev, "Port %u %sconnectable %s\n",
			    p, connectable,
			    acpi_uhub_upc_type(porttypenum));

		SYSCTL_ADD_U32(
		    &sc->port_sysctl_ctx,
		    poid, OID_AUTO,
		    "upc",
		    CTLFLAG_RD | CTLFLAG_MPSAFE,
		    SYSCTL_NULL_U32_PTR, port->upc,
		    "UPC value. MSB is visible flag");
	}
	AcpiOsFree(buf.Pointer);

	return (0);
}
static int
acpi_uhub_port_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct acpi_uhub_port *port = oidp->oid_arg1;
	struct sbuf sb;
	int error;

	sbuf_new_for_sysctl(&sb, NULL, 256, req);
	sbuf_printf(&sb, "Handle %s\n", acpi_name(port->handle));
	if (port->upc == 0xffffffff) {
		sbuf_printf(&sb, "\tNo information\n");
		goto end;
	}
	sbuf_printf(&sb, "\t");
	if (port->upc & ACPI_UPC_CONNECTABLE) {
		sbuf_printf(&sb, "Connectable ");
	}
	sbuf_printf(&sb, "%s port\n", acpi_uhub_upc_type(port->upc & 0xff));
	if (port->pld_len >= ACPI_PLD_REV1_BUFFER_SIZE)
		acpi_get_pld_info(&sb, port->pld, port->pld_len);
end:
	error = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return (error);
}

static void
acpi_uhub_parse_pld(device_t dev, unsigned p,
		    ACPI_HANDLE ah, struct sysctl_oid_list *tree)
{

	struct acpi_uhub_softc *sc = device_get_softc(dev);
	struct acpi_uhub_port *port = &sc->port[p - 1];
	uint8_t *port_pld = port->pld;

	port->pld_len = acpi_add_pld_sysctl(ah, &sc->port_sysctl_ctx,
					    tree, port_pld);
}

static ACPI_STATUS
acpi_uhub_find_rh(device_t dev, ACPI_HANDLE *ah)
{
	device_t grand;
	ACPI_HANDLE gah;

	*ah = NULL;
	grand = device_get_parent(device_get_parent(dev));

	if ((gah = acpi_get_handle(grand)) == NULL)
		return (AE_ERROR);

	return (acpi_find_child_by_adr(gah, ah, 0));
}

static ACPI_STATUS
acpi_usb_hub_port_probe_cb(ACPI_HANDLE ah, UINT32 lv, void *ctx, void **rv)
{
	ACPI_DEVICE_INFO *devinfo;
	device_t dev = ctx;
	struct acpi_uhub_softc *sc = device_get_softc(dev);
	UINT32 ret;

	ret = AcpiGetObjectInfo(ah, &devinfo);
	if (ACPI_SUCCESS(ret)) {
		if ((devinfo->Valid & ACPI_VALID_ADR) &&
		    (devinfo->Address > 0) &&
		    (devinfo->Address <= (uint64_t)sc->nports)) {
			char buf[] = "portXXX";
			struct sysctl_ctx_list *ctx = &sc->port_sysctl_ctx;
			struct sysctl_oid *oid;
			struct sysctl_oid_list *tree;
			
			snprintf(buf, sizeof(buf), "port%ju",
			    (uintmax_t)devinfo->Address);
			oid = SYSCTL_ADD_NODE(ctx,
			    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			        OID_AUTO, buf, CTLFLAG_RD | CTLFLAG_MPSAFE,
				NULL, "port nodes");
			tree = SYSCTL_CHILDREN(oid);
			sc->port[devinfo->Address - 1].handle = ah;
			sc->port[devinfo->Address - 1].upc = 0xffffffff;
			acpi_uhub_parse_upc(dev, devinfo->Address, ah, tree);
			acpi_uhub_parse_pld(dev, devinfo->Address, ah, tree);
			SYSCTL_ADD_PROC(ctx, tree,
			    OID_AUTO, "info",
			    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
			    &sc->port[devinfo->Address - 1], 0,
			    acpi_uhub_port_sysctl, "A", "Port information");
		}
		AcpiOsFree(devinfo);
	}
	return (AE_OK);
}

static ACPI_STATUS
acpi_usb_hub_port_probe(device_t dev, ACPI_HANDLE ah)
{
	return (AcpiWalkNamespace(ACPI_TYPE_DEVICE,
	    ah, 1,
	    acpi_usb_hub_port_probe_cb,
	    NULL, dev, NULL));
}

static int
acpi_uhub_root_probe(device_t dev)
{
	ACPI_STATUS status;
	ACPI_HANDLE ah;

	if (acpi_disabled("usb"))
		return (ENXIO);

	status = acpi_uhub_find_rh(dev, &ah);
	if (ACPI_SUCCESS(status) && ah != NULL &&
	    uhub_probe(dev) <= 0) {
		/* success prior than non-ACPI USB HUB */
		return (BUS_PROBE_DEFAULT + 1);
	}
	return (ENXIO);
}

static int
acpi_uhub_probe(device_t dev)
{
	ACPI_HANDLE ah;

	if (acpi_disabled("usb"))
		return (ENXIO);

	ah = acpi_get_handle(dev);
	if (ah == NULL)
		return (ENXIO);

	if (uhub_probe(dev) <= 0) {
		/* success prior than non-ACPI USB HUB */
		return (BUS_PROBE_DEFAULT + 1);
	}
	return (ENXIO);
}
static void
acpi_uhub_attach_common(device_t dev)
{
	struct usb_hub *uh;
	struct acpi_uhub_softc *sc = device_get_softc(dev);
	ACPI_STATUS status;

	uh = sc->usc.sc_udev->hub;
	sc->nports = uh->nports;
	sc->port = malloc(sizeof(struct acpi_uhub_port) * uh->nports,
	    M_USBDEV, M_WAITOK | M_ZERO);
	sysctl_ctx_init(&sc->port_sysctl_ctx);
	status = acpi_usb_hub_port_probe(dev, sc->ah);

	if (ACPI_FAILURE(status))
		device_printf(dev, "Port probe failed %s\n", AcpiFormatException(status));
}

static int
acpi_uhub_detach(device_t dev)
{
	struct acpi_uhub_softc *sc = device_get_softc(dev);
	int error;

	error = sysctl_ctx_free(&sc->port_sysctl_ctx);
	if (error)
		return (error);

	error = uhub_detach(dev);
	KASSERT(error == 0, ("uhub_detach returned error %d.", error));

	free(sc->port, M_USBDEV);
	sc->port = NULL;
	sc->nports = 0;

	return (0);
}

static int
acpi_uhub_root_attach(device_t dev)
{
	int ret;
	struct acpi_uhub_softc *sc = device_get_softc(dev);

	if (ACPI_FAILURE(acpi_uhub_find_rh(dev, &sc->ah)) ||
	    (sc->ah == NULL)) {
		return (ENXIO);
	}
	if ((ret = uhub_attach(dev)) != 0) {
		return (ret);
	}
	acpi_uhub_attach_common(dev);

	return (0);
}

static int
acpi_uhub_attach(device_t dev)
{
	int ret;
	struct acpi_uhub_softc *sc = device_get_softc(dev);

	sc->ah = acpi_get_handle(dev);

	if (sc->ah == NULL) {
		return (ENXIO);
	}
	if ((ret = uhub_attach(dev)) != 0) {
		return (ret);
	}
	acpi_uhub_attach_common(dev);

	return (0);
}

static int
acpi_uhub_read_ivar(device_t dev, device_t child, int idx, uintptr_t *res)
{
	struct hub_result hres;
	struct acpi_uhub_softc *sc = device_get_softc(dev);
	ACPI_HANDLE ah;

	bus_topo_lock();
	uhub_find_iface_index(sc->usc.sc_udev->hub, child, &hres);
	bus_topo_unlock();

	if ((idx == ACPI_IVAR_HANDLE) &&
	    (hres.portno > 0) &&
	    (hres.portno <= sc->nports) &&
	    (ah = sc->port[hres.portno - 1].handle)) {
		*res = (uintptr_t)ah;
		return (0);
	}
	return (ENXIO);
}

static int
acpi_uhub_child_location(device_t parent, device_t child, struct sbuf *sb)
{
	ACPI_HANDLE ah;

	uhub_child_location(parent, child, sb);

	ah = acpi_get_handle(child);
	if (ah != NULL)
		sbuf_printf(sb, " handle=%s", acpi_name(ah));
	return (0);
}

static int
acpi_uhub_get_device_path(device_t bus, device_t child, const char *locator, struct sbuf *sb)
{
	if (strcmp(locator, BUS_LOCATOR_ACPI) == 0)
		return (acpi_get_acpi_device_path(bus, child, locator, sb));

	/* Otherwise call the parent class' method. */
	return (uhub_get_device_path(bus, child, locator, sb));
}

static device_method_t acpi_uhub_methods[] = {
	DEVMETHOD(device_probe, acpi_uhub_probe),
	DEVMETHOD(device_attach, acpi_uhub_attach),
	DEVMETHOD(device_detach, acpi_uhub_detach),
	DEVMETHOD(bus_child_location, acpi_uhub_child_location),
	DEVMETHOD(bus_get_device_path, acpi_uhub_get_device_path),
	DEVMETHOD(bus_read_ivar, acpi_uhub_read_ivar),
	DEVMETHOD_END

};

static device_method_t acpi_uhub_root_methods[] = {
	DEVMETHOD(device_probe, acpi_uhub_root_probe),
	DEVMETHOD(device_attach, acpi_uhub_root_attach),
	DEVMETHOD(device_detach, acpi_uhub_detach),
	DEVMETHOD(bus_read_ivar, acpi_uhub_read_ivar),
	DEVMETHOD(bus_child_location, acpi_uhub_child_location),
	DEVMETHOD(bus_get_device_path, acpi_uhub_get_device_path),
	DEVMETHOD_END
};

extern driver_t uhub_driver;
static const kobj_class_t uhub_baseclasses[] = {&uhub_driver, NULL};

static driver_t acpi_uhub_driver = {
	.name = "uhub",
	.methods = acpi_uhub_methods,
	.size = sizeof(struct acpi_uhub_softc),
	.baseclasses = uhub_baseclasses,
};

static driver_t acpi_uhub_root_driver = {
	.name = "uhub",
	.methods = acpi_uhub_root_methods,
	.size = sizeof(struct acpi_uhub_softc),
	.baseclasses = uhub_baseclasses,
};

DRIVER_MODULE(uacpi, uhub, acpi_uhub_driver, 0, 0);
DRIVER_MODULE(uacpi, usbus, acpi_uhub_root_driver, 0, 0);

MODULE_DEPEND(uacpi, acpi, 1, 1, 1);
MODULE_DEPEND(uacpi, usb, 1, 1, 1);

MODULE_VERSION(uacpi, 1);
