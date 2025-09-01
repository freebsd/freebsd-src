/*
 * ng_ubt_rtl.c
 */

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Vladimir Kondratyev <wulf@FreeBSD.org>
 * Copyright (c) 2023 Future Crew LLC.
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
 * Attempt to initialize FreeBSD bluetooth stack while Realtek 87XX/88XX USB
 * device is in bootloader mode locks the adapter hardly so it requires power
 * on/off cycle to restore. This driver blocks ng_ubt attachment until
 * operational firmware is loaded by rtlbtfw utility thus avoiding the lock up.
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
#define	USB_DEBUG_VAR usb_debug
#include <dev/usb/usb_debug.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/bluetooth/include/ng_bluetooth.h>
#include <netgraph/bluetooth/include/ng_hci.h>
#include <netgraph/bluetooth/include/ng_ubt.h>
#include <netgraph/bluetooth/drivers/ubt/ng_ubt_var.h>

static device_probe_t	ubt_rtl_probe;

/*
 * List of supported bluetooth devices. If you add a new device PID here ensure
 * that it is blacklisted in ng_ubt.c and is supported by rtlbtfw utility.
 */

const STRUCT_USB_HOST_ID ubt_rtl_devs[] =
{
	/* Generic Realtek Bluetooth class devices */
	{ USB_VENDOR(USB_VENDOR_REALTEK),
	  USB_IFACE_CLASS(UDCLASS_WIRELESS),
	  USB_IFACE_SUBCLASS(UDSUBCLASS_RF),
	  USB_IFACE_PROTOCOL(UDPROTO_BLUETOOTH) },

	/* Realtek 8821CE Bluetooth devices */
	{ USB_VPI(0x13d3, 0x3529, 0) },

	/* Realtek 8822CE Bluetooth devices */
	{ USB_VPI(0x0bda, 0xb00c, 0) },
	{ USB_VPI(0x0bda, 0xc822, 0) },

	/* Realtek 8851BE Bluetooth devices */
	{ USB_VPI(0x13d3, 0x3600, 0) },

	/* Realtek 8852AE Bluetooth devices */
	{ USB_VPI(0x0bda, 0x2852, 0) },
	{ USB_VPI(0x0bda, 0xc852, 0) },
	{ USB_VPI(0x0bda, 0x385a, 0) },
	{ USB_VPI(0x0bda, 0x4852, 0) },
	{ USB_VPI(0x04c5, 0x165c, 0) },
	{ USB_VPI(0x04ca, 0x4006, 0) },
	{ USB_VPI(0x0cb8, 0xc549, 0) },

	/* Realtek 8852CE Bluetooth devices */
	{ USB_VPI(0x04ca, 0x4007, 0) },
	{ USB_VPI(0x04c5, 0x1675, 0) },
	{ USB_VPI(0x0cb8, 0xc558, 0) },
	{ USB_VPI(0x13d3, 0x3587, 0) },
	{ USB_VPI(0x13d3, 0x3586, 0) },
	{ USB_VPI(0x13d3, 0x3592, 0) },
	{ USB_VPI(0x0489, 0xe122, 0) },

	/* Realtek 8852BE Bluetooth devices */
	{ USB_VPI(0x0cb8, 0xc559, 0) },
	{ USB_VPI(0x0bda, 0x4853, 0) },
	{ USB_VPI(0x0bda, 0x887b, 0) },
	{ USB_VPI(0x0bda, 0xb85b, 0) },
	{ USB_VPI(0x13d3, 0x3570, 0) },
	{ USB_VPI(0x13d3, 0x3571, 0) },
	{ USB_VPI(0x13d3, 0x3572, 0) },
	{ USB_VPI(0x13d3, 0x3591, 0) },
	{ USB_VPI(0x0489, 0xe123, 0) },
	{ USB_VPI(0x0489, 0xe125, 0) },

	/* Realtek 8852BT/8852BE-VT Bluetooth devices */
	{ USB_VPI(0x0bda, 0x8520, 0) },

	/* Realtek 8922AE Bluetooth devices */
	{ USB_VPI(0x0bda, 0x8922, 0) },
	{ USB_VPI(0x13d3, 0x3617, 0) },
	{ USB_VPI(0x13d3, 0x3616, 0) },
	{ USB_VPI(0x0489, 0xe130, 0) },

	/* Realtek 8723AE Bluetooth devices */
	{ USB_VPI(0x0930, 0x021d, 0) },
	{ USB_VPI(0x13d3, 0x3394, 0) },

	/* Realtek 8723BE Bluetooth devices */
	{ USB_VPI(0x0489, 0xe085, 0) },
	{ USB_VPI(0x0489, 0xe08b, 0) },
	{ USB_VPI(0x04f2, 0xb49f, 0) },
	{ USB_VPI(0x13d3, 0x3410, 0) },
	{ USB_VPI(0x13d3, 0x3416, 0) },
	{ USB_VPI(0x13d3, 0x3459, 0) },
	{ USB_VPI(0x13d3, 0x3494, 0) },

	/* Realtek 8723BU Bluetooth devices */
	{ USB_VPI(0x7392, 0xa611, 0) },

	/* Realtek 8723DE Bluetooth devices */
	{ USB_VPI(0x0bda, 0xb009, 0) },
	{ USB_VPI(0x2ff8, 0xb011, 0) },

	/* Realtek 8761BUV Bluetooth devices */
	{ USB_VPI(0x2c4e, 0x0115, 0) },
	{ USB_VPI(0x2357, 0x0604, 0) },
	{ USB_VPI(0x0b05, 0x190e, 0) },
	{ USB_VPI(0x2550, 0x8761, 0) },
	{ USB_VPI(0x0bda, 0x8771, 0) },
	{ USB_VPI(0x6655, 0x8771, 0) },
	{ USB_VPI(0x7392, 0xc611, 0) },
	{ USB_VPI(0x2b89, 0x8761, 0) },

	/* Realtek 8821AE Bluetooth devices */
	{ USB_VPI(0x0b05, 0x17dc, 0) },
	{ USB_VPI(0x13d3, 0x3414, 0) },
	{ USB_VPI(0x13d3, 0x3458, 0) },
	{ USB_VPI(0x13d3, 0x3461, 0) },
	{ USB_VPI(0x13d3, 0x3462, 0) },

	/* Realtek 8822BE Bluetooth devices */
	{ USB_VPI(0x13d3, 0x3526, 0) },
	{ USB_VPI(0x0b05, 0x185c, 0) },

	/* Realtek 8822CE Bluetooth devices */
	{ USB_VPI(0x04ca, 0x4005, 0) },
	{ USB_VPI(0x04c5, 0x161f, 0) },
	{ USB_VPI(0x0b05, 0x18ef, 0) },
	{ USB_VPI(0x13d3, 0x3548, 0) },
	{ USB_VPI(0x13d3, 0x3549, 0) },
	{ USB_VPI(0x13d3, 0x3553, 0) },
	{ USB_VPI(0x13d3, 0x3555, 0) },
	{ USB_VPI(0x2ff8, 0x3051, 0) },
	{ USB_VPI(0x1358, 0xc123, 0) },
	{ USB_VPI(0x0bda, 0xc123, 0) },
	{ USB_VPI(0x0cb5, 0xc547, 0) },
};
const size_t ubt_rtl_devs_sizeof = sizeof(ubt_rtl_devs);

/*
 * List of lmp_subversion values that correspond to Realtek firmwares
 * incompatible with ng_ubt driver. Alternative firmware for these devices
 * has to be loaded with rtlbtfw utility. That will trigger lmp_subversion
 * change to different value.
 */
static const uint16_t ubt_rtl_lmp_subvers[] = {
	0x8703, 0x1200, 0x8723, 0x8821,
	0x8761, 0x8822, 0x8852, 0x8851,
};

/*
 * Execute generic HCI command and return response in provided buffer.
 */

static usb_error_t
ubt_rtl_do_hci_request(struct usb_device *udev, uint16_t opcode,
    void *resp, uint8_t resp_len)
{
#define	UBT_RTL_HCICMD_TIMEOUT	2000	/* ms */
	struct ubt_hci_event_command_compl *evt;
	struct ubt_hci_cmd cmd;
	usb_error_t error;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = htole16(opcode);
	evt = malloc(offsetof(struct ubt_hci_event_command_compl, data) +
	    resp_len, M_TEMP, M_ZERO | M_WAITOK);
	evt->header.event = NG_HCI_EVENT_COMMAND_COMPL;
	evt->header.length = resp_len + UBT_HCI_EVENT_COMPL_HEAD_SIZE;

	error = ubt_do_hci_request(udev, &cmd, evt, UBT_RTL_HCICMD_TIMEOUT);
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
 * Probe for a Realtek 87XX/88XX USB Bluetooth device.
 */

static int
ubt_rtl_probe(device_t dev)
{
	struct usb_attach_arg	*uaa = device_get_ivars(dev);
	ng_hci_read_local_ver_rp ver;
	unsigned int i;
	int error;

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if (uaa->info.bIfaceIndex != 0)
		return (ENXIO);

	error = usbd_lookup_id_by_uaa(ubt_rtl_devs, sizeof(ubt_rtl_devs), uaa);
	if (error != 0)
		return (error);

	if (ubt_rtl_do_hci_request(uaa->device,
	    NG_HCI_OPCODE(NG_HCI_OGF_INFO, NG_HCI_OCF_READ_LOCAL_VER),
	    &ver, sizeof(ver)) != USB_ERR_NORMAL_COMPLETION)
		return (ENXIO);

	DPRINTFN(2, "hci_version    0x%02x\n", ver.hci_version);
	DPRINTFN(2, "hci_revision   0x%04x\n", le16toh(ver.hci_revision));
	DPRINTFN(2, "lmp_version    0x%02x\n", ver.lmp_version);
	DPRINTFN(2, "lmp_subversion 0x%04x\n", le16toh(ver.lmp_subversion));

	for (i = 0; i < nitems(ubt_rtl_lmp_subvers); i++)
		if (le16toh(ver.lmp_subversion) == ubt_rtl_lmp_subvers[i])
			return (ENXIO);

	return (BUS_PROBE_DEFAULT);
}

/*
 * Module interface. Attach and detach methods, netgraph node type
 * registration and PNP string are inherited from ng_ubt.c driver.
 */

static device_method_t	ubt_rtl_methods[] =
{
	DEVMETHOD(device_probe,	ubt_rtl_probe),
	DEVMETHOD_END
};

DEFINE_CLASS_1(ubt, ubt_rtl_driver, ubt_rtl_methods,
    sizeof(struct ubt_softc), ubt_driver);
DRIVER_MODULE(ng_ubt_rtl, uhub, ubt_rtl_driver, 0, 0);
MODULE_VERSION(ng_ubt_rtl, NG_BLUETOOTH_VERSION);
MODULE_DEPEND(ng_ubt_rtl, netgraph, NG_ABI_VERSION, NG_ABI_VERSION, NG_ABI_VERSION);
MODULE_DEPEND(ng_ubt_rtl, ng_hci, NG_BLUETOOTH_VERSION, NG_BLUETOOTH_VERSION, NG_BLUETOOTH_VERSION);
MODULE_DEPEND(ng_ubt_rtl, usb, 1, 1, 1);
