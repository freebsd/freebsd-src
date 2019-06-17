/* $FreeBSD$ */
/*-
 * SPDX-License-Identifier: BSD-2-Clause-NetBSD
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

static UINT32 acpi_uhub_find_rh_cb(ACPI_HANDLE ah, UINT32 nl, void *ctx, void **status);
static ACPI_STATUS acpi_uhub_find_rh(device_t dev, ACPI_HANDLE * ah);
static ACPI_STATUS
acpi_usb_hub_port_probe_cb(ACPI_HANDLE ah, UINT32 lv, void *ctx, void **rv);
static ACPI_STATUS acpi_usb_hub_port_probe(device_t dev, ACPI_HANDLE ah);
static int acpi_uhub_root_probe(device_t dev);
static int acpi_uhub_probe(device_t dev);
static int acpi_uhub_root_attach(device_t dev);
static int acpi_uhub_attach(device_t dev);
static int acpi_uhub_detach(device_t dev);
static int
acpi_uhub_read_ivar(device_t dev, device_t child, int idx,
    uintptr_t *res);
static int
acpi_uhub_child_location_string(device_t parent, device_t child,
    char *buf, size_t buflen);
static int acpi_uhub_parse_upc(device_t dev, unsigned int port, ACPI_HANDLE ah);

struct acpi_uhub_softc {
	struct uhub_softc usc;
	uint8_t	nports;
	ACPI_HANDLE *porthandle;
};

UINT32
acpi_uhub_find_rh_cb(ACPI_HANDLE ah, UINT32 nl, void *ctx, void **status){
	ACPI_DEVICE_INFO *devinfo;
	UINT32 ret = AE_OK;

	*status = NULL;
	devinfo = NULL;

	ret = AcpiGetObjectInfo(ah, &devinfo);

	if (ACPI_FAILURE(ret)) {
		return ret;
	}
	if ((devinfo->Valid & ACPI_VALID_ADR) &&
	    (devinfo->Address == 0)) {
		ret = AE_CTRL_TERMINATE;
		*status = ah;
	}
	AcpiOsFree(devinfo);

	return ret;
}

static int
acpi_uhub_parse_upc(device_t dev, unsigned int port, ACPI_HANDLE ah)
{
	ACPI_BUFFER buf;

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	if (AcpiEvaluateObject(ah, "_UPC", NULL, &buf) == AE_OK) {
		UINT64 porttypenum, conn;
		const char *connectable;
		const char *typelist[] = {"TypeA", "MiniAB", "Express",
			"USB3-A", "USB3-B", "USB-MicroB",
			"USB3-MicroAB", "USB3-PowerB",
			"TypeC-USB2", "TypeC-Switch",
		"TypeC-nonSwitch"};
		const char *porttype;
		const int last = sizeof(typelist) / sizeof(typelist[0]);
		ACPI_OBJECT *obj = buf.Pointer;

		acpi_PkgInt(obj, 0, &conn);
		acpi_PkgInt(obj, 1, &porttypenum);
		connectable = conn ? "" : "non";
		if (porttypenum == 0xff)
			porttype = "Proprietary";
		else if (porttypenum < last) {
			porttype = typelist[porttypenum];
		} else {
			porttype = "Unknown";
		}
		if (usb_debug)
			device_printf(dev, "Port %u %sconnectable %s\n",
			    port, connectable, porttype);
	}
	AcpiOsFree(buf.Pointer);

	return 0;
}

static int
acpi_uhub_parse_pld(device_t dev, unsigned int port, ACPI_HANDLE ah)
{
	ACPI_BUFFER buf;

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	if (AcpiEvaluateObject(ah, "_PLD", NULL, &buf) == AE_OK) {
		ACPI_OBJECT *obj;
		unsigned char *resbuf;
		int len;

		obj = buf.Pointer;

		if (obj->Type == ACPI_TYPE_PACKAGE
		    && obj->Package.Elements[0].Type == ACPI_TYPE_BUFFER) {
			ACPI_OBJECT *obj1;

			obj1 = &obj->Package.Elements[0];
			len = obj1->Buffer.Length;
			resbuf = obj1->Buffer.Pointer;
		} else if (obj->Type == ACPI_TYPE_BUFFER) {
			len = obj->Buffer.Length;
			resbuf = obj->Buffer.Pointer;
		} else {
			goto skip;
		}
		if (usb_debug) {
			device_printf(dev, "Revision:%d\n",
			    resbuf[0] & 0x7f);
			if ((resbuf[0] & 0x80) == 0) {
				device_printf(dev,
				    "Color:#%02x%02x%02x\n",
				    resbuf[1], resbuf[2],
				    resbuf[3]);
			}
			device_printf(dev, "Width %d mm Height %d mm\n",
			    resbuf[4] | (resbuf[5] << 8),
			    resbuf[6] | (resbuf[7] << 8));
			if (resbuf[8] & 1) {
				device_printf(dev, "Visible\n");
			}
			if (resbuf[8] & 2) {
				device_printf(dev, "Dock\n");
			}
			if (resbuf[8] & 4) {
				device_printf(dev, "Lid\n");
			}
			device_printf(dev, "PanelPosition: %d\n",
			    (resbuf[8] >> 3) & 7);
			device_printf(dev, "VertPosition: %d\n",
			    (resbuf[8] >> 6) & 3);
			device_printf(dev, "HorizPosition: %d\n",
			    (resbuf[9]) & 3);
			device_printf(dev, "Shape: %d\n",
			    (resbuf[9] >> 2) & 0xf);
			device_printf(dev, "80: %02x, %02x, %02x\n",
			    resbuf[9], resbuf[10], resbuf[11]);
			device_printf(dev, "96: %02x, %02x, %02x, %02x\n",
			    resbuf[12], resbuf[13],
			    resbuf[14], resbuf[15]);

			if ((resbuf[0] & 0x7f) >= 2) {
				device_printf(dev, "VOFF%d mm HOFF %dmm",
				    resbuf[16] | (resbuf[17] << 8),
				    resbuf[18] | (resbuf[19] << 8));
			}
		}
	skip:
		AcpiOsFree(buf.Pointer);
		
	}


	return 0;
}

ACPI_STATUS
acpi_uhub_find_rh(device_t dev, ACPI_HANDLE * ah){
	device_t grand;
	ACPI_HANDLE gah;

	grand = device_get_parent(device_get_parent(dev));
	if ((gah = acpi_get_handle(grand)) == NULL) {
		*ah = NULL;
		return AE_ERROR;
	}
	return AcpiWalkNamespace(ACPI_TYPE_DEVICE, gah, 1,
	    acpi_uhub_find_rh_cb, NULL, dev, ah);
}

ACPI_STATUS
acpi_usb_hub_port_probe_cb(ACPI_HANDLE ah, UINT32 lv, void *ctx, void **rv){
	ACPI_DEVICE_INFO *devinfo;
	device_t dev = ctx;
	struct acpi_uhub_softc *sc = device_get_softc(dev);

	if (usb_debug)
		device_printf(dev, "%s\n", acpi_name(ah));

	AcpiGetObjectInfo(ah, &devinfo);
	if ((devinfo->Valid & ACPI_VALID_ADR) &&
	    (devinfo->Address > 0) &&
	    (devinfo->Address <= (uint64_t)sc->nports)) {
		sc->porthandle[devinfo->Address - 1] = ah;
		acpi_uhub_parse_upc(dev, devinfo->Address, ah);
		acpi_uhub_parse_pld(dev, devinfo->Address, ah);
	} else {
		device_printf(dev, "Skiping invalid devobj %s\n",
		    acpi_name(ah));
	}
	AcpiOsFree(devinfo);
	return AE_OK;
}

ACPI_STATUS
acpi_usb_hub_port_probe(device_t dev, ACPI_HANDLE ah){
	return AcpiWalkNamespace(ACPI_TYPE_DEVICE,
	    ah, 1,
	    acpi_usb_hub_port_probe_cb,
	    NULL, dev, NULL);
}
int
acpi_uhub_root_probe(device_t dev)
{
	ACPI_HANDLE ah;
	ACPI_STATUS status;

	status = acpi_uhub_find_rh(dev, &ah);
	if (ACPI_SUCCESS(status)
	    && ah != NULL
	    && (uhub_probe(dev) <= 0)) {
		/* success prior than non - acpi hub */
		return (BUS_PROBE_DEFAULT + 1);
	}
	return ENXIO;
}

int
acpi_uhub_probe(device_t dev)
{
	ACPI_HANDLE ah = acpi_get_handle(dev);

	if (ah && (uhub_probe(dev) <= 0)) {
		/*success prior than non - acpi hub*/
		    return (BUS_PROBE_DEFAULT + 1);
	}
	return (ENXIO);
}
int
acpi_uhub_root_attach(device_t dev)
{
	ACPI_HANDLE devhandle;
	struct usb_hub *uh;
	struct acpi_uhub_softc *sc = device_get_softc(dev);
	int ret;

	if ((ret = uhub_attach(dev)) != 0) {
		return (ret);
	}
	uh = sc->usc.sc_udev->hub;

	if (ACPI_FAILURE(acpi_uhub_find_rh(dev, &devhandle)) ||
	    (devhandle == NULL)) {
		return ENXIO;
	}

	sc->nports = uh->nports;
	sc->porthandle = malloc(sizeof(ACPI_HANDLE) * uh->nports,
	    M_USBDEV, M_WAITOK | M_ZERO);
	acpi_uhub_find_rh(dev, &devhandle);
	acpi_usb_hub_port_probe(dev, devhandle);

	return 0;
}

int
acpi_uhub_attach(device_t dev)
{
	struct usb_hub *uh;
	struct acpi_uhub_softc *sc = device_get_softc(dev);
	ACPI_HANDLE devhandle;
	int ret;

	if ((ret = uhub_attach(dev)) != 0) {
		return (ret);
	}
	uh = sc->usc.sc_udev->hub;
	devhandle = acpi_get_handle(dev);

	if (devhandle == NULL) {
		return ENXIO;
	}

	sc->nports = uh->nports;
	sc->porthandle = malloc(sizeof(ACPI_HANDLE) * uh->nports,
	    M_USBDEV, M_WAITOK | M_ZERO);
	acpi_usb_hub_port_probe(dev, acpi_get_handle(dev));
	return 0;
}

int
acpi_uhub_read_ivar(device_t dev, device_t child, int idx,
    uintptr_t *res)
{
	struct hub_result hres;
	struct acpi_uhub_softc *sc = device_get_softc(dev);
	ACPI_HANDLE ah;

	mtx_lock(&Giant);
	uhub_find_iface_index(sc->usc.sc_udev->hub, child, &hres);
	mtx_unlock(&Giant);
	if ((idx == ACPI_IVAR_HANDLE) &&
	    (hres.portno > 0) &&
	    (hres.portno <= sc->nports) &&
	    (ah = sc->porthandle[hres.portno - 1])) {
		*res = (uintptr_t)ah;
		return (0);
	}
	return (ENXIO);
}
static int
acpi_uhub_child_location_string(device_t parent, device_t child,
    char *buf, size_t buflen)
{

	ACPI_HANDLE ah;

	uhub_child_location_string(parent, child, buf, buflen);
	ah = acpi_get_handle(child);
	if (ah) {
		strlcat(buf, " handle=", buflen);
		strlcat(buf, acpi_name(ah), buflen);
	}
	return (0);
}

int
acpi_uhub_detach(device_t dev)
{
	struct acpi_uhub_softc *sc = device_get_softc(dev);

	free(sc->porthandle, M_USBDEV);
	return uhub_detach(dev);
}

static device_method_t acpi_uhub_methods[] = {
	DEVMETHOD(device_probe, acpi_uhub_probe),
	DEVMETHOD(device_attach, acpi_uhub_attach),
	DEVMETHOD(device_detach, acpi_uhub_detach),
	DEVMETHOD(bus_child_location_str, acpi_uhub_child_location_string),
	DEVMETHOD(bus_read_ivar, acpi_uhub_read_ivar),
	DEVMETHOD_END

};

static device_method_t acpi_uhub_root_methods[] = {
	DEVMETHOD(device_probe, acpi_uhub_root_probe),
	DEVMETHOD(device_attach, acpi_uhub_root_attach),
	DEVMETHOD(device_detach, acpi_uhub_detach),
	DEVMETHOD(bus_read_ivar, acpi_uhub_read_ivar),
	DEVMETHOD(bus_child_location_str, acpi_uhub_child_location_string),
	DEVMETHOD_END
};

static devclass_t uhub_devclass;
extern driver_t uhub_driver;
static kobj_class_t uhub_baseclasses[] = {&uhub_driver, NULL};
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

DRIVER_MODULE(acpi_uhub, uhub, acpi_uhub_driver, uhub_devclass, 0, 0);
MODULE_DEPEND(acpi_uhub, acpi, 1, 1, 1);
DRIVER_MODULE(acpi_uhub, usbus, acpi_uhub_root_driver, uhub_devclass, 0, 0);
