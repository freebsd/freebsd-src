/*
 * Copyright (c) 2008 AnyWi Technologies
 * Author: Andrea Guzzo <aguzzo@anywi.com>
 * * based on uark.c 1.1 2006/08/14 08:30:22 jsg *
 * * parts from ubsa.c 183348 2008-09-25 12:00:56Z phk *
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */

/*
 * NOTE:
 *
 * - The detour through the tty layer is ridiculously expensive wrt
 *   buffering due to the high speeds.
 *
 *   We should consider adding a simple r/w device which allows
 *   attaching of PPP in a more efficient way.
 *
 */

#include "usbdevs.h"
#include <dev/usb/usb.h>
#include <dev/usb/usb_mfunc.h>
#include <dev/usb/usb_error.h>
#include <dev/usb/usb_defs.h>

#define	USB_DEBUG_VAR u3g_debug

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_request.h>
#include <dev/usb/usb_lookup.h>
#include <dev/usb/usb_util.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_msctest.h>
#include <dev/usb/usb_dynamic.h>
#include <dev/usb/usb_device.h>

#include <dev/usb/serial/usb_serial.h>

#if USB_DEBUG
static int u3g_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, u3g, CTLFLAG_RW, 0, "USB 3g");
SYSCTL_INT(_hw_usb2_u3g, OID_AUTO, debug, CTLFLAG_RW,
    &u3g_debug, 0, "Debug level");
#endif

#define	U3G_MAXPORTS		4
#define	U3G_CONFIG_INDEX	0
#define	U3G_BSIZE		2048

#define	U3GSP_GPRS		0
#define	U3GSP_EDGE		1
#define	U3GSP_CDMA		2
#define	U3GSP_UMTS		3
#define	U3GSP_HSDPA		4
#define	U3GSP_HSUPA		5
#define	U3GSP_HSPA		6
#define	U3GSP_MAX		7

#define	U3GFL_NONE		0x0000	/* No flags */
#define	U3GFL_HUAWEI_INIT	0x0001	/* Init command required */
#define	U3GFL_SCSI_EJECT	0x0002	/* SCSI eject command required */
#define	U3GFL_SIERRA_INIT	0x0004	/* Init command required */
#define	U3GFL_SAEL_M460_INIT	0x0008	/* Init device */

enum {
	U3G_BULK_WR,
	U3G_BULK_RD,
	U3G_N_TRANSFER,
};

struct u3g_softc {
	struct usb2_com_super_softc sc_super_ucom;
	struct usb2_com_softc sc_ucom[U3G_MAXPORTS];

	struct usb2_xfer *sc_xfer[U3G_MAXPORTS][U3G_N_TRANSFER];
	struct usb2_device *sc_udev;
	struct mtx sc_mtx;

	uint8_t	sc_lsr;			/* local status register */
	uint8_t	sc_msr;			/* U3G status register */
	uint8_t	sc_numports;
};

static device_probe_t u3g_probe;
static device_attach_t u3g_attach;
static device_detach_t u3g_detach;

static usb2_callback_t u3g_write_callback;
static usb2_callback_t u3g_read_callback;

static void u3g_start_read(struct usb2_com_softc *ucom);
static void u3g_stop_read(struct usb2_com_softc *ucom);
static void u3g_start_write(struct usb2_com_softc *ucom);
static void u3g_stop_write(struct usb2_com_softc *ucom);

static int u3g_driver_loaded(struct module *mod, int what, void *arg);

static const struct usb2_config u3g_config[U3G_N_TRANSFER] = {

	[U3G_BULK_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = U3G_BSIZE,/* bytes */
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &u3g_write_callback,
	},

	[U3G_BULK_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = U3G_BSIZE,/* bytes */
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &u3g_read_callback,
	},
};

static const struct usb2_com_callback u3g_callback = {
	.usb2_com_start_read = &u3g_start_read,
	.usb2_com_stop_read = &u3g_stop_read,
	.usb2_com_start_write = &u3g_start_write,
	.usb2_com_stop_write = &u3g_stop_write,
};

static device_method_t u3g_methods[] = {
	DEVMETHOD(device_probe, u3g_probe),
	DEVMETHOD(device_attach, u3g_attach),
	DEVMETHOD(device_detach, u3g_detach),
	{0, 0}
};

static devclass_t u3g_devclass;

static driver_t u3g_driver = {
	.name = "u3g",
	.methods = u3g_methods,
	.size = sizeof(struct u3g_softc),
};

DRIVER_MODULE(u3g, ushub, u3g_driver, u3g_devclass, u3g_driver_loaded, 0);
MODULE_DEPEND(u3g, ucom, 1, 1, 1);
MODULE_DEPEND(u3g, usb, 1, 1, 1);

static const struct usb2_device_id u3g_devs[] = {
	/* OEM: Option */
	{USB_VPI(USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GT3G, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GT3GQUAD, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GT3GPLUS, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GTMAX36, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GTMAXHSUPA, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_OPTION, USB_PRODUCT_OPTION_VODAFONEMC3G, U3GFL_NONE)},
	/* OEM: Qualcomm, Inc. */
	{USB_VPI(USB_VENDOR_QUALCOMMINC, USB_PRODUCT_QUALCOMMINC_ZTE_STOR, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_QUALCOMMINC, USB_PRODUCT_QUALCOMMINC_CDMA_MSM, U3GFL_SCSI_EJECT)},
	/* OEM: Huawei */
	{USB_VPI(USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_MOBILE, U3GFL_HUAWEI_INIT)},
	{USB_VPI(USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_E220, U3GFL_HUAWEI_INIT)},
	/* OEM: Novatel */
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_CDMA_MODEM, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_ES620, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_MC950D, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U720, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U727, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U740, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U740_2, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U870, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V620, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V640, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V720, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V740, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_X950D, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_XU870, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_ZEROCD, U3GFL_SCSI_EJECT)},
	{USB_VPI(USB_VENDOR_DELL, USB_PRODUCT_DELL_U740, U3GFL_SCSI_EJECT)},
	/* OEM: Merlin */
	{USB_VPI(USB_VENDOR_MERLIN, USB_PRODUCT_MERLIN_V620, U3GFL_NONE)},
	/* OEM: Sierra Wireless: */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD580, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD595, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC595U, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC597E, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_C597, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880E, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880U, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881E, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881U, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_EM5625, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5720, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5720_2, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5725, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MINI5725, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD875, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755_2, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755_3, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8765, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC875U, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8775_2, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_HS2300, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8780, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8781, U3GFL_NONE)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_HS2300, U3GFL_NONE)},
	/* Sierra TruInstaller device ID */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_TRUINSTALL, U3GFL_SIERRA_INIT)},
	/* PRUEBA SILABS */
	{USB_VPI(USB_VENDOR_SILABS, USB_PRODUCT_SILABS_SAEL, U3GFL_SAEL_M460_INIT)},
};

static void
u3g_sierra_init(struct usb2_device *udev)
{
	struct usb2_device_request req;

	DPRINTFN(0, "\n");

	req.bmRequestType = UT_VENDOR;
	req.bRequest = UR_SET_INTERFACE;
	USETW(req.wValue, UF_DEVICE_REMOTE_WAKEUP);
	USETW(req.wIndex, UHF_PORT_CONNECTION);
	USETW(req.wLength, 0);

	if (usb2_do_request_flags(udev, NULL, &req,
	    NULL, 0, NULL, USB_MS_HZ)) {
		/* ignore any errors */
	}
	return;
}

static void
u3g_huawei_init(struct usb2_device *udev)
{
	struct usb2_device_request req;

	DPRINTFN(0, "\n");

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, UF_DEVICE_REMOTE_WAKEUP);
	USETW(req.wIndex, UHF_PORT_SUSPEND);
	USETW(req.wLength, 0);

	if (usb2_do_request_flags(udev, NULL, &req,
	    NULL, 0, NULL, USB_MS_HZ)) {
		/* ignore any errors */
	}
	return;
}

static void
u3g_sael_m460_init(struct usb2_device *udev)
{
	static const uint8_t setup[][24] = {
	     { 0x41, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	     { 0x41, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 },
	     { 0x41, 0x13, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 
	       0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	     { 0xc1, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x40, 0x02 },
	     { 0xc1, 0x08, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 },
	     { 0x41, 0x07, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00 },
	     { 0xc1, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 },
	     { 0x41, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 },
	     { 0x41, 0x07, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00 },
	     { 0x41, 0x03, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00 },
	     { 0x41, 0x19, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x11, 0x13 },
	     { 0x41, 0x13, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 
	       0x09, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
	       0x0a, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00 },
	     { 0x41, 0x12, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },
	     { 0x41, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 },
	     { 0x41, 0x07, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00 },
	     { 0x41, 0x03, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00 },
	     { 0x41, 0x19, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x11, 0x13 },
	     { 0x41, 0x13, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
	       0x09, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 
	       0x0a, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00 },
	     { 0x41, 0x07, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00 },
	};

	struct usb2_device_request req;
	uint16_t len;
	uint8_t buf[0x300];
	uint8_t n;

	DPRINTFN(1, "\n");

	if (usb2_req_set_alt_interface_no(udev, NULL, 0, 0)) {
		DPRINTFN(0, "Alt setting 0 failed\n");
		return;
	}

	for (n = 0; n != (sizeof(setup)/sizeof(setup[0])); n++) {

		memcpy(&req, setup[n], sizeof(req));

		len = UGETW(req.wLength);
		if (req.bmRequestType & UE_DIR_IN) {
			if (len > sizeof(buf)) {
				DPRINTFN(0, "too small buffer\n");
				continue;
			}
			if (usb2_do_request(udev, NULL, &req, buf)) {
				DPRINTFN(0, "request %u failed\n",
				    (unsigned int)n);
				break;
			}
		} else {
			if (len > (sizeof(setup[0]) - 8)) {
				DPRINTFN(0, "too small buffer\n");
				continue;
			}
			if (usb2_do_request(udev, NULL, &req, 
			    __DECONST(uint8_t *, &setup[n][8]))) {
				DPRINTFN(0, "request %u failed\n",
				    (unsigned int)n);
				break;
			}
		}
	}
	return;
}

static int
u3g_lookup_huawei(struct usb2_attach_arg *uaa)
{
	/* Calling the lookup function will also set the driver info! */
	return (usb2_lookup_id_by_uaa(u3g_devs, sizeof(u3g_devs), uaa));
}

/*
 * The following function handles 3G modem devices (E220, Mobile,
 * etc.) with auto-install flash disks for Windows/MacOSX on the first
 * interface.  After some command or some delay they change appearance
 * to a modem.
 */
static usb2_error_t
u3g_test_huawei_autoinst(struct usb2_device *udev,
    struct usb2_attach_arg *uaa)
{
	struct usb2_interface *iface;
	struct usb2_interface_descriptor *id;
	uint32_t flags;

	if (udev == NULL) {
		return (USB_ERR_INVAL);
	}
	iface = usb2_get_iface(udev, 0);
	if (iface == NULL) {
		return (USB_ERR_INVAL);
	}
	id = iface->idesc;
	if (id == NULL) {
		return (USB_ERR_INVAL);
	}
	if (id->bInterfaceClass != UICLASS_MASS) {
		return (USB_ERR_INVAL);
	}
	if (u3g_lookup_huawei(uaa)) {
		/* no device match */
		return (USB_ERR_INVAL);
	}
	flags = USB_GET_DRIVER_INFO(uaa);

	if (flags & U3GFL_HUAWEI_INIT) {
		u3g_huawei_init(udev);
	} else if (flags & U3GFL_SCSI_EJECT) {
		return (usb2_test_autoinstall(udev, 0, 1));
	} else if (flags & U3GFL_SIERRA_INIT) {
		u3g_sierra_init(udev);
	} else {
		/* no quirks */
		return (USB_ERR_INVAL);
	}
	return (0);			/* success */
}

static int
u3g_driver_loaded(struct module *mod, int what, void *arg)
{
	switch (what) {
	case MOD_LOAD:
		/* register our autoinstall handler */
		usb2_test_huawei_autoinst_p = &u3g_test_huawei_autoinst;
		break;
	case MOD_UNLOAD:
		usb2_test_huawei_unload(NULL);
		break;
	default:
		return (EOPNOTSUPP);
	}
 	return (0);
}

static int
u3g_probe(device_t self)
{
	struct usb2_attach_arg *uaa = device_get_ivars(self);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != U3G_CONFIG_INDEX) {
		return (ENXIO);
	}
	if (uaa->info.bInterfaceClass != UICLASS_VENDOR) {
		return (ENXIO);
	}
	return (u3g_lookup_huawei(uaa));
}

static int
u3g_attach(device_t dev)
{
	struct usb2_config u3g_config_tmp[U3G_N_TRANSFER];
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct u3g_softc *sc = device_get_softc(dev);
	struct usb2_interface *iface;
	struct usb2_interface_descriptor *id;
	uint32_t flags;
	uint8_t m;
	uint8_t n;
	uint8_t i;
	uint8_t x;
	int error;

	DPRINTF("sc=%p\n", sc);

	flags = USB_GET_DRIVER_INFO(uaa);

	if (flags & U3GFL_SAEL_M460_INIT)
		u3g_sael_m460_init(uaa->device);

	/* copy in USB config */
	for (n = 0; n != U3G_N_TRANSFER; n++) 
		u3g_config_tmp[n] = u3g_config[n];

	device_set_usb2_desc(dev);
	mtx_init(&sc->sc_mtx, "u3g", NULL, MTX_DEF);

	sc->sc_udev = uaa->device;

	x = 0;		/* interface index */
	i = 0;		/* endpoint index */
	m = 0;		/* number of ports */

	while (m != U3G_MAXPORTS) {

		/* update BULK endpoint index */
		for (n = 0; n != U3G_N_TRANSFER; n++) 
			u3g_config_tmp[n].ep_index = i;

		iface = usb2_get_iface(uaa->device, x);
		if (iface == NULL) {
			if (m != 0)
				break;	/* end of interfaces */
			DPRINTF("did not find any modem endpoints\n");
			goto detach;
		}

		id = usb2_get_interface_descriptor(iface);
		if ((id == NULL) || 
		    (id->bInterfaceClass != UICLASS_VENDOR)) {
			/* next interface */
			x++;
			i = 0;
			continue;
		}

		/* try to allocate a set of BULK endpoints */
		error = usb2_transfer_setup(uaa->device, &x,
		    sc->sc_xfer[m], u3g_config_tmp, U3G_N_TRANSFER, 
		    &sc->sc_ucom[m], &sc->sc_mtx);
		if (error) {
			/* next interface */
			x++;
			i = 0;
			continue;
		}

		/* grab other interface, if any */
                if (x != uaa->info.bIfaceIndex)
                        usb2_set_parent_iface(uaa->device, x,
                            uaa->info.bIfaceIndex);

		/* set stall by default */
		mtx_lock(&sc->sc_mtx);
		usb2_transfer_set_stall(sc->sc_xfer[m][U3G_BULK_WR]);
		usb2_transfer_set_stall(sc->sc_xfer[m][U3G_BULK_RD]);
		mtx_unlock(&sc->sc_mtx);

		m++;	/* found one port */
		i++;	/* next endpoint index */
	}

	sc->sc_numports = m;

	error = usb2_com_attach(&sc->sc_super_ucom, sc->sc_ucom, 
	    sc->sc_numports, sc, &u3g_callback, &sc->sc_mtx);
	if (error) {
		DPRINTF("usb2_com_attach failed\n");
		goto detach;
	}
	if (sc->sc_numports != 1) {
		/* be verbose */
		device_printf(dev, "Found %u ports.\n",
		    (unsigned int)sc->sc_numports);
	}
	return (0);

detach:
	u3g_detach(dev);
	return (ENXIO);
}

static int
u3g_detach(device_t dev)
{
	struct u3g_softc *sc = device_get_softc(dev);
	uint8_t m;

	DPRINTF("sc=%p\n", sc);

	/* NOTE: It is not dangerous to detach more ports than attached! */
	usb2_com_detach(&sc->sc_super_ucom, sc->sc_ucom, U3G_MAXPORTS);

	for (m = 0; m != U3G_MAXPORTS; m++)
		usb2_transfer_unsetup(sc->sc_xfer[m], U3G_N_TRANSFER);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
u3g_start_read(struct usb2_com_softc *ucom)
{
	struct u3g_softc *sc = ucom->sc_parent;

	/* start read endpoint */
	usb2_transfer_start(sc->sc_xfer[ucom->sc_local_unit][U3G_BULK_RD]);
	return;
}

static void
u3g_stop_read(struct usb2_com_softc *ucom)
{
	struct u3g_softc *sc = ucom->sc_parent;

	/* stop read endpoint */
	usb2_transfer_stop(sc->sc_xfer[ucom->sc_local_unit][U3G_BULK_RD]);
	return;
}

static void
u3g_start_write(struct usb2_com_softc *ucom)
{
	struct u3g_softc *sc = ucom->sc_parent;

	usb2_transfer_start(sc->sc_xfer[ucom->sc_local_unit][U3G_BULK_WR]);
	return;
}

static void
u3g_stop_write(struct usb2_com_softc *ucom)
{
	struct u3g_softc *sc = ucom->sc_parent;

	usb2_transfer_stop(sc->sc_xfer[ucom->sc_local_unit][U3G_BULK_WR]);
	return;
}

static void
u3g_write_callback(struct usb2_xfer *xfer)
{
	struct usb2_com_softc *ucom = xfer->priv_sc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
tr_setup:
		if (usb2_com_get_data(ucom, xfer->frbuffers, 0,
		    U3G_BSIZE, &actlen)) {
			xfer->frlengths[0] = actlen;
			usb2_start_hardware(xfer);
		}
		break;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* do a builtin clear-stall */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		break;
	}
	return;
}

static void
u3g_read_callback(struct usb2_xfer *xfer)
{
	struct usb2_com_softc *ucom = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usb2_com_put_data(ucom, xfer->frbuffers, 0, xfer->actlen);

	case USB_ST_SETUP:
tr_setup:
		xfer->frlengths[0] = xfer->max_data_length;
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* do a builtin clear-stall */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		break;
	}
	return;
}
