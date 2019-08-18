/*
 * ng_ubt_intel.c
 */

/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Vladimir Kondratyev <wulf@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

/*
 * Attempt to initialize FreeBSD bluetooth stack while Intel Wireless 8260/8265
 * device is in bootloader mode locks the adapter hardly so it requires power
 * on/off cycle to restore. This driver blocks ng_ubt attachment until
 * operational firmware is loaded by iwmbtfw utility thus avoiding the lock up.
 */

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include "usbdevs.h"
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/bluetooth/include/ng_bluetooth.h>
#include <netgraph/bluetooth/include/ng_hci.h>
#include <netgraph/bluetooth/include/ng_ubt.h>
#include <netgraph/bluetooth/drivers/ubt/ng_ubt_var.h>

static device_probe_t	ubt_intel_probe;

/*
 * List of supported bluetooth devices. If you add a new device PID here ensure
 * that it is blacklisted in ng_ubt.c and is supported by iwmbtfw utility.
 */

static const STRUCT_USB_HOST_ID ubt_intel_devs[] =
{
	/* Intel Wireless 8260/8265 and successors */
	{ USB_VPI(USB_VENDOR_INTEL2, 0x0a2b, 0) },
	{ USB_VPI(USB_VENDOR_INTEL2, 0x0aaa, 0) },
	{ USB_VPI(USB_VENDOR_INTEL2, 0x0025, 0) },
	{ USB_VPI(USB_VENDOR_INTEL2, 0x0026, 0) },
	{ USB_VPI(USB_VENDOR_INTEL2, 0x0029, 0) },
};

/*
 * Find if the Intel Wireless 8260/8265 device is in bootloader mode or is
 * running operational firmware with checking of 4-th byte "Intel version"
 * HCI command response. The value 0x23 identifies the operational firmware.
 */

static bool
ubt_intel_check_firmware_state(struct usb_device *udev)
{
#define	UBT_INTEL_VER_LEN		13
#define	UBT_INTEL_HCICMD_TIMEOUT	2000	/* ms */
	struct ubt_hci_event_command_compl *evt;
	uint8_t buf[offsetof(struct ubt_hci_event, data) + UBT_INTEL_VER_LEN];
	static struct ubt_hci_cmd cmd = {
		.opcode = htole16(NG_HCI_OPCODE(NG_HCI_OGF_VENDOR, 0x05)),
		.length = 0,
	};
	usb_error_t error;

	bzero(buf, sizeof(buf));
	evt = (struct ubt_hci_event_command_compl *)buf;
	evt->header.length = UBT_INTEL_VER_LEN;

	error = ubt_do_hci_request(udev, &cmd, evt, UBT_INTEL_HCICMD_TIMEOUT);
	if (error != USB_ERR_NORMAL_COMPLETION)
		return false;

	return (evt->header.event == NG_HCI_EVENT_COMMAND_COMPL &&
		evt->header.length == UBT_INTEL_VER_LEN &&
		evt->data[4] == 0x23);
}

/*
 * Probe for a Intel Wireless Bluetooth device.
 */

static int
ubt_intel_probe(device_t dev)
{
	struct usb_attach_arg	*uaa = device_get_ivars(dev);
	int error;

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if (uaa->info.bIfaceIndex != 0)
		return (ENXIO);

	error = usbd_lookup_id_by_uaa(ubt_intel_devs, sizeof(ubt_intel_devs),
	    uaa);
	if (error != 0)
		return (error);

	if (!ubt_intel_check_firmware_state(uaa->device))
		return (ENXIO);

	return (BUS_PROBE_DEFAULT);
}

/*
 * Module interface. Attach and detach methods, netgraph node type
 * registration and PNP string are inherited from ng_ubt.c driver.
 */

static device_method_t	ubt_intel_methods[] =
{
	DEVMETHOD(device_probe,	ubt_intel_probe),
	DEVMETHOD_END
};

static kobj_class_t ubt_baseclasses[] = { &ubt_driver, NULL };
static driver_t		ubt_intel_driver =
{
	.name =	   "ubt",
	.methods = ubt_intel_methods,
	.size =	   sizeof(struct ubt_softc),
	.baseclasses = ubt_baseclasses,
};

DRIVER_MODULE(ng_ubt_intel, uhub, ubt_intel_driver, ubt_devclass, 0, 0);
MODULE_VERSION(ng_ubt_intel, NG_BLUETOOTH_VERSION);
MODULE_DEPEND(ng_ubt_intel, netgraph, NG_ABI_VERSION, NG_ABI_VERSION, NG_ABI_VERSION);
MODULE_DEPEND(ng_ubt_intel, ng_hci, NG_BLUETOOTH_VERSION, NG_BLUETOOTH_VERSION, NG_BLUETOOTH_VERSION);
MODULE_DEPEND(ng_ubt_intel, usb, 1, 1, 1);
