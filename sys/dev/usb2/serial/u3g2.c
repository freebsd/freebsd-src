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
 * NOTE:
 *
 * - The device ID's are stored in "core/usb2_msctest.c"
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_defs.h>

#define	USB_DEBUG_VAR u3g_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_msctest.h>
#include <dev/usb2/core/usb2_dynamic.h>
#include <dev/usb2/core/usb2_device.h>

#include <dev/usb2/serial/usb2_serial.h>

#if USB_DEBUG
static int u3g_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, u3g, CTLFLAG_RW, 0, "USB u3g");
SYSCTL_INT(_hw_usb2_u3g, OID_AUTO, debug, CTLFLAG_RW,
    &u3g_debug, 0, "u3g debug level");
#endif

#define	U3G_N_TRANSFER		2
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

#define	U3GFL_NONE		0x00	/* No flags */
#define	U3GFL_HUAWEI_INIT	0x01	/* Init command required */
#define	U3GFL_SCSI_EJECT	0x02	/* SCSI eject command required */
#define	U3GFL_SIERRA_INIT	0x04	/* Init command required */

struct u3g_speeds_s {
	uint32_t ispeed;
	uint32_t ospeed;
};

struct u3g_softc {
	struct usb2_com_super_softc sc_super_ucom;
	struct usb2_com_softc sc_ucom;

	struct usb2_xfer *sc_xfer[U3G_N_TRANSFER];
	struct usb2_device *sc_udev;

	uint8_t	sc_iface_no;		/* interface number */
	uint8_t	sc_iface_index;		/* interface index */
	uint8_t	sc_lsr;			/* local status register */
	uint8_t	sc_msr;			/* U3G status register */
	struct u3g_speeds_s sc_speed;
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

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = U3G_BSIZE,/* bytes */
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &u3g_write_callback,
	},

	[1] = {
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

static const struct u3g_speeds_s u3g_speeds[U3GSP_MAX] = {
	[U3GSP_GPRS] = {64000, 64000},
	[U3GSP_EDGE] = {384000, 64000},
	[U3GSP_CDMA] = {384000, 64000},
	[U3GSP_UMTS] = {384000, 64000},
	[U3GSP_HSDPA] = {1200000, 384000},
	[U3GSP_HSUPA] = {1200000, 384000},
	[U3GSP_HSPA] = {7200000, 384000},
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
MODULE_DEPEND(u3g, usb2_serial, 1, 1, 1);
MODULE_DEPEND(u3g, usb2_core, 1, 1, 1);

/* Huawei specific defines */

#define	U3GINFO(flag,speed) ((flag)|((speed) * 256))
#define	U3G_GET_SPEED(uaa) (USB_GET_DRIVER_INFO(uaa) / 256)

/*
 * NOTE: The entries marked with XXX should be checked for the correct
 * speed indication to set the buffer sizes.
 */
static const struct usb2_device_id u3g_devs[] = {
	/* OEM: Option */
	{USB_VPI(USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GT3G, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},
	{USB_VPI(USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GT3GQUAD, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},
	{USB_VPI(USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GT3GPLUS, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},
	{USB_VPI(USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GTMAX36, U3GINFO(U3GSP_HSDPA, U3GFL_NONE))},
	{USB_VPI(USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GTMAXHSUPA, U3GINFO(U3GSP_HSDPA, U3GFL_NONE))},
	{USB_VPI(USB_VENDOR_OPTION, USB_PRODUCT_OPTION_VODAFONEMC3G, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},
	/* OEM: Qualcomm, Inc. */
	{USB_VPI(USB_VENDOR_QUALCOMMINC, USB_PRODUCT_QUALCOMMINC_ZTE_STOR, U3GINFO(U3GSP_CDMA, U3GFL_SCSI_EJECT))},
	{USB_VPI(USB_VENDOR_QUALCOMMINC, USB_PRODUCT_QUALCOMMINC_CDMA_MSM, U3GINFO(U3GSP_CDMA, U3GFL_SCSI_EJECT))},
	/* OEM: Huawei */
	{USB_VPI(USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_MOBILE, U3GINFO(U3GSP_HSDPA, U3GFL_HUAWEI_INIT))},
	{USB_VPI(USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_E220, U3GINFO(U3GSP_HSPA, U3GFL_HUAWEI_INIT))},
	/* OEM: Novatel */
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_CDMA_MODEM, U3GINFO(U3GSP_CDMA, U3GFL_SCSI_EJECT))},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_ES620, U3GINFO(U3GSP_UMTS, U3GFL_SCSI_EJECT))},	/* XXX */
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_MC950D, U3GINFO(U3GSP_HSUPA, U3GFL_SCSI_EJECT))},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U720, U3GINFO(U3GSP_UMTS, U3GFL_SCSI_EJECT))},	/* XXX */
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U727, U3GINFO(U3GSP_UMTS, U3GFL_SCSI_EJECT))},	/* XXX */
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U740, U3GINFO(U3GSP_HSDPA, U3GFL_SCSI_EJECT))},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U740_2, U3GINFO(U3GSP_HSDPA, U3GFL_SCSI_EJECT))},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U870, U3GINFO(U3GSP_UMTS, U3GFL_SCSI_EJECT))},	/* XXX */
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V620, U3GINFO(U3GSP_UMTS, U3GFL_SCSI_EJECT))},	/* XXX */
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V640, U3GINFO(U3GSP_UMTS, U3GFL_SCSI_EJECT))},	/* XXX */
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V720, U3GINFO(U3GSP_UMTS, U3GFL_SCSI_EJECT))},	/* XXX */
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V740, U3GINFO(U3GSP_HSDPA, U3GFL_SCSI_EJECT))},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_X950D, U3GINFO(U3GSP_HSUPA, U3GFL_SCSI_EJECT))},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_XU870, U3GINFO(U3GSP_HSDPA, U3GFL_SCSI_EJECT))},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_ZEROCD, U3GINFO(U3GSP_HSUPA, U3GFL_SCSI_EJECT))},
	{USB_VPI(USB_VENDOR_DELL, USB_PRODUCT_DELL_U740, U3GINFO(U3GSP_HSDPA, U3GFL_SCSI_EJECT))},
	/* OEM: Merlin */
	{USB_VPI(USB_VENDOR_MERLIN, USB_PRODUCT_MERLIN_V620, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	/* OEM: Sierra Wireless: */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD580, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD595, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC595U, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC597E, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_C597, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880E, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880U, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881E, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881U, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_EM5625, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5720, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5720_2, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5725, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MINI5725, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD875, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755_2, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755_3, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8765, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC875U, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8775_2, U3GINFO(U3GSP_HSDPA, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_HS2300, U3GINFO(U3GSP_HSDPA, U3GFL_NONE))},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8780, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8781, U3GINFO(U3GSP_UMTS, U3GFL_NONE))},	/* XXX */
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_HS2300, U3GINFO(U3GSP_HSPA, U3GFL_NONE))},	/* XXX */
	/* Sierra TruInstaller device ID */
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_TRUINSTALL, U3GINFO(U3GSP_UMTS, U3GFL_SIERRA_INIT))},
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
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct u3g_softc *sc = device_get_softc(dev);
	int error;

	DPRINTF("sc=%p\n", sc);

	if (sc == NULL) {
		return (ENOMEM);
	}
	device_set_usb2_desc(dev);

	sc->sc_udev = uaa->device;
	sc->sc_iface_no = uaa->info.bIfaceNum;
	sc->sc_iface_index = uaa->info.bIfaceIndex;
	sc->sc_speed = u3g_speeds[U3G_GET_SPEED(uaa)];

	error = usb2_transfer_setup(uaa->device, &sc->sc_iface_index,
	    sc->sc_xfer, u3g_config, U3G_N_TRANSFER, sc, &Giant);

	if (error) {
		DPRINTF("could not allocate all pipes\n");
		goto detach;
	}
	/* set stall by default */
	usb2_transfer_set_stall(sc->sc_xfer[0]);
	usb2_transfer_set_stall(sc->sc_xfer[1]);

	error = usb2_com_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &u3g_callback, &Giant);
	if (error) {
		DPRINTF("usb2_com_attach failed\n");
		goto detach;
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

	DPRINTF("sc=%p\n", sc);

	usb2_com_detach(&sc->sc_super_ucom, &sc->sc_ucom, 1);

	usb2_transfer_unsetup(sc->sc_xfer, U3G_N_TRANSFER);

	return (0);
}

static void
u3g_start_read(struct usb2_com_softc *ucom)
{
	struct u3g_softc *sc = ucom->sc_parent;

	/* start read endpoint */
	usb2_transfer_start(sc->sc_xfer[1]);
	return;
}

static void
u3g_stop_read(struct usb2_com_softc *ucom)
{
	struct u3g_softc *sc = ucom->sc_parent;

	/* stop read endpoint */
	usb2_transfer_stop(sc->sc_xfer[1]);
	return;
}

static void
u3g_start_write(struct usb2_com_softc *ucom)
{
	struct u3g_softc *sc = ucom->sc_parent;

	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
u3g_stop_write(struct usb2_com_softc *ucom)
{
	struct u3g_softc *sc = ucom->sc_parent;

	usb2_transfer_stop(sc->sc_xfer[0]);
	return;
}

static void
u3g_write_callback(struct usb2_xfer *xfer)
{
	struct u3g_softc *sc = xfer->priv_sc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
tr_setup:
		if (usb2_com_get_data(&sc->sc_ucom, xfer->frbuffers, 0,
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
	struct u3g_softc *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usb2_com_put_data(&sc->sc_ucom, xfer->frbuffers, 0, xfer->actlen);

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
