/*
 * ng_ubt_intel.c
 */

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019, 2021 Vladimir Kondratyev <wulf@FreeBSD.org>
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

enum {
	UBT_INTEL_DEVICE_7260,
	UBT_INTEL_DEVICE_8260,
};

struct ubt_intel_version_rp {
	uint8_t status;
	uint8_t hw_platform;
	uint8_t hw_variant;
	uint8_t hw_revision;
	uint8_t fw_variant;
	uint8_t fw_revision;
	uint8_t fw_build_num;
	uint8_t fw_build_ww;
	uint8_t fw_build_yy;
	uint8_t fw_patch_num;
} __attribute__ ((packed));

static device_probe_t	ubt_intel_probe;

/*
 * List of supported bluetooth devices. If you add a new device PID here ensure
 * that it is blacklisted in ng_ubt.c and is supported by iwmbtfw utility.
 */

static const STRUCT_USB_HOST_ID ubt_intel_devs[] =
{
	/* Intel Wireless 7260/7265 and successors */
	{ USB_VPI(USB_VENDOR_INTEL2, 0x07dc, UBT_INTEL_DEVICE_7260) },
	{ USB_VPI(USB_VENDOR_INTEL2, 0x0a2a, UBT_INTEL_DEVICE_7260) },
	{ USB_VPI(USB_VENDOR_INTEL2, 0x0aa7, UBT_INTEL_DEVICE_7260) },
	/* Intel Wireless 8260/8265 and successors */
	{ USB_VPI(USB_VENDOR_INTEL2, 0x0a2b, UBT_INTEL_DEVICE_8260) },
	{ USB_VPI(USB_VENDOR_INTEL2, 0x0aaa, UBT_INTEL_DEVICE_8260) },
	{ USB_VPI(USB_VENDOR_INTEL2, 0x0025, UBT_INTEL_DEVICE_8260) },
	{ USB_VPI(USB_VENDOR_INTEL2, 0x0026, UBT_INTEL_DEVICE_8260) },
	{ USB_VPI(USB_VENDOR_INTEL2, 0x0029, UBT_INTEL_DEVICE_8260) },
};

/*
 * Execute generic HCI command and return response in provided buffer.
 */

static usb_error_t
ubt_intel_do_hci_request(struct usb_device *udev, uint16_t opcode,
    void *resp, uint8_t resp_len)
{
#define	UBT_INTEL_HCICMD_TIMEOUT	2000	/* ms */
	struct ubt_hci_event_command_compl *evt;
	struct ubt_hci_cmd cmd;
	usb_error_t error;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = htole16(opcode);
	evt = malloc(offsetof(struct ubt_hci_event_command_compl, data) +
	    resp_len, M_TEMP, M_ZERO | M_WAITOK);
	evt->header.length = resp_len + UBT_HCI_EVENT_COMPL_HEAD_SIZE;

	error = ubt_do_hci_request(udev, &cmd, evt, UBT_INTEL_HCICMD_TIMEOUT);
	if (error != USB_ERR_NORMAL_COMPLETION)
		goto exit;

	if (evt->header.event == NG_HCI_EVENT_COMMAND_COMPL &&
	    evt->header.length == resp_len + UBT_HCI_EVENT_COMPL_HEAD_SIZE)
		memcpy(resp, evt->data, resp_len);
	else
		error = USB_ERR_INVAL;
exit:
	free(evt, M_TEMP);
	return (error);
}

/*
 * Probe for a Intel Wireless Bluetooth device.
 */

static int
ubt_intel_probe(device_t dev)
{
	struct usb_attach_arg	*uaa = device_get_ivars(dev);
	struct ubt_intel_version_rp version;
	ng_hci_reset_rp reset;
	int error;

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if (uaa->info.bIfaceIndex != 0)
		return (ENXIO);

	error = usbd_lookup_id_by_uaa(ubt_intel_devs, sizeof(ubt_intel_devs),
	    uaa);
	if (error != 0)
		return (error);

	switch (USB_GET_DRIVER_INFO(uaa)) {
	case UBT_INTEL_DEVICE_7260:
		/*
		 * Send HCI Reset command to workaround controller bug with the
		 * first HCI command sent to it returning number of completed
		 * commands as zero.  This will reset the number of completed
		 * commands and allow further normal command processing.
		 */
		if (ubt_intel_do_hci_request(uaa->device,
		    NG_HCI_OPCODE(NG_HCI_OGF_HC_BASEBAND, NG_HCI_OCF_RESET),
		    &reset, sizeof(reset)) != USB_ERR_NORMAL_COMPLETION)
			return (ENXIO);
		if (reset.status != 0)
			return (ENXIO);
		/*
		 * fw_patch_num indicates the version of patch the device
		 * currently have.  If there is no patch data in the device,
		 * it is always 0x00 and we need to patch the device again.
		 */
		if (ubt_intel_do_hci_request(uaa->device,
		    NG_HCI_OPCODE(NG_HCI_OGF_VENDOR, 0x05),
		    &version, sizeof(version)) != USB_ERR_NORMAL_COMPLETION)
			return (ENXIO);
		if (version.fw_patch_num == 0)
			return (ENXIO);
		break;

	case UBT_INTEL_DEVICE_8260:
		/*
		 * Find if the Intel Wireless 8260/8265 device is in bootloader
		 * mode or is running operational firmware with checking of
		 * variant byte of "Intel version" HCI command response.
		 * The value 0x23 identifies the operational firmware.
		 */
		if (ubt_intel_do_hci_request(uaa->device,
		    NG_HCI_OPCODE(NG_HCI_OGF_VENDOR, 0x05),
		    &version, sizeof(version)) != USB_ERR_NORMAL_COMPLETION)
			return (ENXIO);
		if (version.fw_variant != 0x23)
			return (ENXIO);
		break;

	default:
		KASSERT(0 == 1, ("Unknown DRIVER_INFO"));
	}

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

DEFINE_CLASS_1(ubt, ubt_intel_driver, ubt_intel_methods,
    sizeof(struct ubt_softc), ubt_driver);
DRIVER_MODULE(ng_ubt_intel, uhub, ubt_intel_driver, 0, 0);
MODULE_VERSION(ng_ubt_intel, NG_BLUETOOTH_VERSION);
MODULE_DEPEND(ng_ubt_intel, netgraph, NG_ABI_VERSION, NG_ABI_VERSION, NG_ABI_VERSION);
MODULE_DEPEND(ng_ubt_intel, ng_hci, NG_BLUETOOTH_VERSION, NG_BLUETOOTH_VERSION, NG_BLUETOOTH_VERSION);
MODULE_DEPEND(ng_ubt_intel, usb, 1, 1, 1);
