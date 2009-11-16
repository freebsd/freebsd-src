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


#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/linker_set.h>
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
#include "usbdevs.h"

#define	USB_DEBUG_VAR u3g_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_dynamic.h>
#include <dev/usb/usb_msctest.h>
#include <dev/usb/usb_device.h>

#include <dev/usb/serial/usb_serial.h>

#if USB_DEBUG
static int u3g_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, u3g, CTLFLAG_RW, 0, "USB 3g");
SYSCTL_INT(_hw_usb_u3g, OID_AUTO, debug, CTLFLAG_RW,
    &u3g_debug, 0, "Debug level");
#endif

#define	U3G_MAXPORTS		8
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
	struct ucom_super_softc sc_super_ucom;
	struct ucom_softc sc_ucom[U3G_MAXPORTS];

	struct usb_xfer *sc_xfer[U3G_MAXPORTS][U3G_N_TRANSFER];
	struct usb_device *sc_udev;
	struct mtx sc_mtx;

	uint8_t	sc_lsr;			/* local status register */
	uint8_t	sc_msr;			/* U3G status register */
	uint8_t	sc_numports;
};

static device_probe_t u3g_probe;
static device_attach_t u3g_attach;
static device_detach_t u3g_detach;

static usb_callback_t u3g_write_callback;
static usb_callback_t u3g_read_callback;

static void u3g_start_read(struct ucom_softc *ucom);
static void u3g_stop_read(struct ucom_softc *ucom);
static void u3g_start_write(struct ucom_softc *ucom);
static void u3g_stop_write(struct ucom_softc *ucom);

static int u3g_driver_loaded(struct module *mod, int what, void *arg);

static const struct usb_config u3g_config[U3G_N_TRANSFER] = {

	[U3G_BULK_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = U3G_BSIZE,/* bytes */
		.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.callback = &u3g_write_callback,
	},

	[U3G_BULK_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = U3G_BSIZE,/* bytes */
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = &u3g_read_callback,
	},
};

static const struct ucom_callback u3g_callback = {
	.ucom_start_read = &u3g_start_read,
	.ucom_stop_read = &u3g_stop_read,
	.ucom_start_write = &u3g_start_write,
	.ucom_stop_write = &u3g_stop_write,
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

DRIVER_MODULE(u3g, uhub, u3g_driver, u3g_devclass, u3g_driver_loaded, 0);
MODULE_DEPEND(u3g, ucom, 1, 1, 1);
MODULE_DEPEND(u3g, usb, 1, 1, 1);

static const struct usb_device_id u3g_devs[] = {
#define	U3G_DEV(v,p,i) { USB_VPI(USB_VENDOR_##v, USB_PRODUCT_##v##_##p, i) }
	U3G_DEV(CURITEL, UM175, 0),
	/* OEM: Huawei */
	U3G_DEV(HUAWEI, MOBILE, U3GFL_HUAWEI_INIT),
	U3G_DEV(HUAWEI, E180V, U3GFL_HUAWEI_INIT),
	U3G_DEV(HUAWEI, E220, U3GFL_HUAWEI_INIT),
	/* OEM: Option */
	U3G_DEV(OPTION, GT3G, 0),
	U3G_DEV(OPTION, GT3GQUAD, 0),
	U3G_DEV(OPTION, GT3GPLUS, 0),
	U3G_DEV(OPTION, GTMAX36, 0),
	U3G_DEV(OPTION, GTHSDPA, 0),
	U3G_DEV(OPTION, GTMAXHSUPA, 0),
	U3G_DEV(OPTION, GTMAXHSUPAE, 0),
	U3G_DEV(OPTION, GTMAX380HSUPAE, 0),
	U3G_DEV(OPTION, VODAFONEMC3G, 0),
	/* OEM: Qualcomm, Inc. */
	U3G_DEV(QUALCOMMINC, ZTE_STOR, U3GFL_SCSI_EJECT),
	U3G_DEV(QUALCOMMINC, CDMA_MSM, U3GFL_SCSI_EJECT),
	/* OEM: Merlin */
	U3G_DEV(MERLIN, V620, 0),
	/* OEM: Novatel */
	U3G_DEV(NOVATEL, CDMA_MODEM, 0),
	U3G_DEV(NOVATEL, ES620, 0),
	U3G_DEV(NOVATEL, MC950D, 0),
	U3G_DEV(NOVATEL, U720, 0),
	U3G_DEV(NOVATEL, U727, 0),
	U3G_DEV(NOVATEL, U740, 0),
	U3G_DEV(NOVATEL, U740_2, 0),
	U3G_DEV(NOVATEL, U870, 0),
	U3G_DEV(NOVATEL, V620, 0),
	U3G_DEV(NOVATEL, V640, 0),
	U3G_DEV(NOVATEL, V720, 0),
	U3G_DEV(NOVATEL, V740, 0),
	U3G_DEV(NOVATEL, X950D, 0),
	U3G_DEV(NOVATEL, XU870, 0),
	U3G_DEV(NOVATEL, ZEROCD, U3GFL_SCSI_EJECT),
	U3G_DEV(NOVATEL, U760, U3GFL_SCSI_EJECT),
	U3G_DEV(DELL, U740, 0),
	/* OEM: Sierra Wireless: */
	U3G_DEV(SIERRA, AIRCARD580, 0),
	U3G_DEV(SIERRA, AIRCARD595, 0),
	U3G_DEV(SIERRA, AC595U, 0),
	U3G_DEV(SIERRA, AC597E, 0),
	U3G_DEV(SIERRA, C597, 0),
	U3G_DEV(SIERRA, AC880, 0),
	U3G_DEV(SIERRA, AC880E, 0),
	U3G_DEV(SIERRA, AC880U, 0),
	U3G_DEV(SIERRA, AC881, 0),
	U3G_DEV(SIERRA, AC881E, 0),
	U3G_DEV(SIERRA, AC881U, 0),
	U3G_DEV(SIERRA, AC885U, 0),
	U3G_DEV(SIERRA, EM5625, 0),
	U3G_DEV(SIERRA, MC5720, 0),
	U3G_DEV(SIERRA, MC5720_2, 0),
	U3G_DEV(SIERRA, MC5725, 0),
	U3G_DEV(SIERRA, MINI5725, 0),
	U3G_DEV(SIERRA, AIRCARD875, 0),
	U3G_DEV(SIERRA, MC8755, 0),
	U3G_DEV(SIERRA, MC8755_2, 0),
	U3G_DEV(SIERRA, MC8755_3, 0),
	U3G_DEV(SIERRA, MC8765, 0),
	U3G_DEV(SIERRA, AC875U, 0),
	U3G_DEV(SIERRA, MC8775_2, 0),
	U3G_DEV(SIERRA, MC8780, 0),
	U3G_DEV(SIERRA, MC8781, 0),
	U3G_DEV(HP, HS2300, 0),
	/* Sierra TruInstaller device ID */
	U3G_DEV(SIERRA, TRUINSTALL, U3GFL_SIERRA_INIT),
	/* PRUEBA SILABS */
	U3G_DEV(SILABS, SAEL, U3GFL_SAEL_M460_INIT),
};

static void
u3g_sierra_init(struct usb_device *udev)
{
	struct usb_device_request req;

	DPRINTFN(0, "\n");

	req.bmRequestType = UT_VENDOR;
	req.bRequest = UR_SET_INTERFACE;
	USETW(req.wValue, UF_DEVICE_REMOTE_WAKEUP);
	USETW(req.wIndex, UHF_PORT_CONNECTION);
	USETW(req.wLength, 0);

	if (usbd_do_request_flags(udev, NULL, &req,
	    NULL, 0, NULL, USB_MS_HZ)) {
		/* ignore any errors */
	}
	return;
}

static void
u3g_huawei_init(struct usb_device *udev)
{
	struct usb_device_request req;

	DPRINTFN(0, "\n");

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, UF_DEVICE_REMOTE_WAKEUP);
	USETW(req.wIndex, UHF_PORT_SUSPEND);
	USETW(req.wLength, 0);

	if (usbd_do_request_flags(udev, NULL, &req,
	    NULL, 0, NULL, USB_MS_HZ)) {
		/* ignore any errors */
	}
	return;
}

static void
u3g_sael_m460_init(struct usb_device *udev)
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

	struct usb_device_request req;
	usb_error_t err;
	uint16_t len;
	uint8_t buf[0x300];
	uint8_t n;

	DPRINTFN(1, "\n");

	if (usbd_req_set_alt_interface_no(udev, NULL, 0, 0)) {
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
			err = usbd_do_request(udev, NULL, &req, buf);
		} else {
			if (len > (sizeof(setup[0]) - 8)) {
				DPRINTFN(0, "too small buffer\n");
				continue;
			}
			err = usbd_do_request(udev, NULL, &req, 
			    __DECONST(uint8_t *, &setup[n][8]));
		}
		if (err) {
			DPRINTFN(1, "request %u failed\n",
			    (unsigned int)n);
			/*
			 * Some of the requests will fail. Stop doing
			 * requests when we are getting timeouts so
			 * that we don't block the explore/attach
			 * thread forever.
			 */
			if (err == USB_ERR_TIMEOUT)
				break;
		}
	}
}

static int
u3g_lookup_huawei(struct usb_attach_arg *uaa)
{
	/* Calling the lookup function will also set the driver info! */
	return (usbd_lookup_id_by_uaa(u3g_devs, sizeof(u3g_devs), uaa));
}

/*
 * The following function handles 3G modem devices (E220, Mobile,
 * etc.) with auto-install flash disks for Windows/MacOSX on the first
 * interface.  After some command or some delay they change appearance
 * to a modem.
 */
static usb_error_t
u3g_test_huawei_autoinst(struct usb_device *udev,
    struct usb_attach_arg *uaa)
{
	struct usb_interface *iface;
	struct usb_interface_descriptor *id;
	uint32_t flags;

	if (udev == NULL) {
		return (USB_ERR_INVAL);
	}
	iface = usbd_get_iface(udev, 0);
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
		return (usb_test_autoinstall(udev, 0, 1));
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
		usb_test_huawei_autoinst_p = &u3g_test_huawei_autoinst;
		break;
	case MOD_UNLOAD:
		usb_test_huawei_unload(NULL);
		break;
	default:
		return (EOPNOTSUPP);
	}
 	return (0);
}

static int
u3g_probe(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->usb_mode != USB_MODE_HOST) {
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
	struct usb_config u3g_config_tmp[U3G_N_TRANSFER];
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct u3g_softc *sc = device_get_softc(dev);
	struct usb_interface *iface;
	struct usb_interface_descriptor *id;
	uint32_t iface_valid;
	int error, flags, nports;
	int ep, n;
	uint8_t i;

	DPRINTF("sc=%p\n", sc);

	flags = USB_GET_DRIVER_INFO(uaa);

	if (flags & U3GFL_SAEL_M460_INIT)
		u3g_sael_m460_init(uaa->device);

	/* copy in USB config */
	for (n = 0; n != U3G_N_TRANSFER; n++) 
		u3g_config_tmp[n] = u3g_config[n];

	device_set_usb_desc(dev);
	mtx_init(&sc->sc_mtx, "u3g", NULL, MTX_DEF);

	sc->sc_udev = uaa->device;

	/* Claim all interfaces on the device */
	iface_valid = 0;
	for (i = uaa->info.bIfaceIndex; i < USB_IFACE_MAX; i++) {
		iface = usbd_get_iface(uaa->device, i);
		if (iface == NULL)
			break;
		id = usbd_get_interface_descriptor(iface);
		if (id == NULL || id->bInterfaceClass != UICLASS_VENDOR)
			continue;
		usbd_set_parent_iface(uaa->device, i, uaa->info.bIfaceIndex);
		iface_valid |= (1<<i);
	}

	i = 0;		/* interface index */
	ep = 0;		/* endpoint index */
	nports = 0;	/* number of ports */
	while (i < USB_IFACE_MAX) {
		if ((iface_valid & (1<<i)) == 0) {
			i++;
			continue;
		}

		/* update BULK endpoint index */
		for (n = 0; n < U3G_N_TRANSFER; n++)
			u3g_config_tmp[n].ep_index = ep;

		/* try to allocate a set of BULK endpoints */
		error = usbd_transfer_setup(uaa->device, &i,
		    sc->sc_xfer[nports], u3g_config_tmp, U3G_N_TRANSFER,
		    &sc->sc_ucom[nports], &sc->sc_mtx);
		if (error) {
			/* next interface */
			i++;
			ep = 0;
			continue;
		}

		/* set stall by default */
		mtx_lock(&sc->sc_mtx);
		usbd_xfer_set_stall(sc->sc_xfer[nports][U3G_BULK_WR]);
		usbd_xfer_set_stall(sc->sc_xfer[nports][U3G_BULK_RD]);
		mtx_unlock(&sc->sc_mtx);

		nports++;	/* found one port */
		ep++;
		if (nports == U3G_MAXPORTS)
			break;
	}
	if (nports == 0) {
		device_printf(dev, "no ports found\n");
		goto detach;
	}
	sc->sc_numports = nports;

	error = ucom_attach(&sc->sc_super_ucom, sc->sc_ucom,
	    sc->sc_numports, sc, &u3g_callback, &sc->sc_mtx);
	if (error) {
		DPRINTF("ucom_attach failed\n");
		goto detach;
	}
	if (sc->sc_numports > 1)
		device_printf(dev, "Found %u ports.\n", sc->sc_numports);
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
	ucom_detach(&sc->sc_super_ucom, sc->sc_ucom, U3G_MAXPORTS);

	for (m = 0; m != U3G_MAXPORTS; m++)
		usbd_transfer_unsetup(sc->sc_xfer[m], U3G_N_TRANSFER);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
u3g_start_read(struct ucom_softc *ucom)
{
	struct u3g_softc *sc = ucom->sc_parent;

	/* start read endpoint */
	usbd_transfer_start(sc->sc_xfer[ucom->sc_local_unit][U3G_BULK_RD]);
	return;
}

static void
u3g_stop_read(struct ucom_softc *ucom)
{
	struct u3g_softc *sc = ucom->sc_parent;

	/* stop read endpoint */
	usbd_transfer_stop(sc->sc_xfer[ucom->sc_local_unit][U3G_BULK_RD]);
	return;
}

static void
u3g_start_write(struct ucom_softc *ucom)
{
	struct u3g_softc *sc = ucom->sc_parent;

	usbd_transfer_start(sc->sc_xfer[ucom->sc_local_unit][U3G_BULK_WR]);
	return;
}

static void
u3g_stop_write(struct ucom_softc *ucom)
{
	struct u3g_softc *sc = ucom->sc_parent;

	usbd_transfer_stop(sc->sc_xfer[ucom->sc_local_unit][U3G_BULK_WR]);
	return;
}

static void
u3g_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct ucom_softc *ucom = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
tr_setup:
		pc = usbd_xfer_get_frame(xfer, 0);
		if (ucom_get_data(ucom, pc, 0, U3G_BSIZE, &actlen)) {
			usbd_xfer_set_frame_len(xfer, 0, actlen);
			usbd_transfer_submit(xfer);
		}
		break;

	default:			/* Error */
		if (error != USB_ERR_CANCELLED) {
			/* do a builtin clear-stall */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
	return;
}

static void
u3g_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct ucom_softc *ucom = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		pc = usbd_xfer_get_frame(xfer, 0);
		ucom_put_data(ucom, pc, 0, actlen);

	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		break;

	default:			/* Error */
		if (error != USB_ERR_CANCELLED) {
			/* do a builtin clear-stall */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
	return;
}
