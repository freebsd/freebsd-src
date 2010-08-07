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
 * Notes:
 * - The detour through the tty layer is ridiculously expensive wrt buffering
 *   due to the high speeds.
 *   We should consider adding a simple r/w device which allows attaching of PPP
 *   in a more efficient way.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/usb/ucomvar.h>

#if __FreeBSD_version >= 800000
#include "opt_u3g.h"
#endif
#include "usbdevs.h"

static int u3gdebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, u3g, CTLFLAG_RW, 0, "USB u3g");
SYSCTL_INT(_hw_usb_u3g, OID_AUTO, debug, CTLFLAG_RW,
	   &u3gdebug, 0, "u3g debug level");
#define DPRINTF(x...)		if (u3gdebug) device_printf(sc->sc_dev, ##x)

struct u3g_softc {
	struct ucom_softc	*sc_ucom;
	device_t		sc_dev;
	usbd_device_handle	sc_udev;
	u_int8_t		sc_speed;
	u_int8_t		sc_init;
	u_char			sc_numports;
};

static int u3g_open(void *addr, int portno);
static void u3g_close(void *addr, int portno);

struct ucom_callback u3g_callback = {
	NULL,
	NULL,
	NULL,
	NULL,
	u3g_open,
	u3g_close,
	NULL,
	NULL,
};


struct u3g_speeds_s {
	u_int32_t		ispeed;
	u_int32_t		ospeed;
};

static const struct u3g_speeds_s u3g_speeds[] = {
#define U3GSP_GPRS	0
	{64000,		64000},
#define U3GSP_EDGE	1
	{384000,	64000},
#define U3GSP_CDMA	2
	{384000,	64000},
#define U3GSP_UMTS	3
	{384000,	64000},
#define U3GSP_HSDPA	4
	{7200000,	384000},
#define U3GSP_HSUPA	5
	{7200000,	2000000},
#define U3GSP_HSPA	6
	{7200000,	2000000},
};

#define U3GIBUFSIZE	1024
#define U3GOBUFSIZE	1024

/*
 * Various supported device vendors/products.
 */
struct u3g_dev_type_s {
	struct usb_devno	devno;
	u_int8_t		speed;
	u_int8_t		init;
#define U3GINIT_NONE		0
#define U3GINIT_HUAWEI		1		// Requires init command (Huawei)
#define U3GINIT_SIERRA		2		// Requires init command (Sierra)
#define U3GINIT_EJECT		3		// Requires SCSI eject command (Novatel, Qualcomm)
#define U3GINIT_ZTESTOR		4		// Requires SCSI command (ZTE STOR)
#define U3GINIT_CMOTECH		5		// Requires init command (CMOTECH)
#define U3GINIT_WAIT		6		// Device reappears after a short delay (none)
};

// Note: The entries marked with XXX should be checked for the correct speed
// indication to set the buffer sizes.
static const struct u3g_dev_type_s u3g_devs[] = {
	/* OEM: Option */
	{{ USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GT3G },		U3GSP_UMTS,	U3GINIT_NONE },
	{{ USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GT3GQUAD },		U3GSP_UMTS,	U3GINIT_NONE },
	{{ USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GT3GPLUS },		U3GSP_UMTS,	U3GINIT_NONE },
	{{ USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GTMAX36 },		U3GSP_HSDPA,	U3GINIT_NONE },
	{{ USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GTMAXHSUPA },		U3GSP_HSDPA,	U3GINIT_NONE },
	{{ USB_VENDOR_OPTION, USB_PRODUCT_OPTION_VODAFONEMC3G },	U3GSP_UMTS,	U3GINIT_NONE },
	{{ USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GTM382 },		U3GSP_HSPA,	U3GINIT_NONE },
	/* OEM: Qualcomm, Inc. */
	{{ USB_VENDOR_QUALCOMMINC, USB_PRODUCT_QUALCOMMINC_ZTE_STOR },	U3GSP_CDMA,	U3GINIT_ZTESTOR },
	{{ USB_VENDOR_QUALCOMMINC, USB_PRODUCT_QUALCOMMINC_CDMA_MSM },	U3GSP_CDMA,	U3GINIT_EJECT },
	{{ USB_VENDOR_QUALCOMMINC, USB_PRODUCT_QUALCOMMINC_ZTE_MSM },	U3GSP_CDMA,	U3GINIT_NONE },
	{{ USB_VENDOR_QUALCOMMINC, USB_PRODUCT_QUALCOMMINC_AC8700 },	U3GSP_CDMA,	U3GINIT_NONE },
	/* OEM: Huawei */
	{{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_MOBILE },		U3GSP_HSDPA,	U3GINIT_HUAWEI },
	{{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_E220 },		U3GSP_HSPA,	U3GINIT_HUAWEI },
	/* OEM: Novatel */
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_CDMA_MODEM },	U3GSP_CDMA,	U3GINIT_EJECT },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_ES620 },		U3GSP_UMTS,	U3GINIT_EJECT },	// XXX
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_MC950D },		U3GSP_HSUPA,	U3GINIT_EJECT },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U720 },		U3GSP_UMTS,	U3GINIT_EJECT },	// XXX
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U727 },		U3GSP_UMTS,	U3GINIT_EJECT },	// XXX
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U740 },		U3GSP_HSDPA,	U3GINIT_EJECT },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U740_2 },		U3GSP_HSDPA,	U3GINIT_EJECT },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U760 },		U3GSP_CDMA,	U3GINIT_EJECT },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U870 },		U3GSP_UMTS,	U3GINIT_EJECT },	// XXX
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V620 },		U3GSP_UMTS,	U3GINIT_EJECT },	// XXX
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V640 },		U3GSP_UMTS,	U3GINIT_EJECT },	// XXX
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V720 },		U3GSP_UMTS,	U3GINIT_EJECT },	// XXX
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V740 },		U3GSP_HSDPA,	U3GINIT_EJECT },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_X950D },		U3GSP_HSUPA,	U3GINIT_EJECT },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_XU870 },		U3GSP_HSDPA,	U3GINIT_EJECT },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_ZEROCD },    	U3GSP_HSUPA,	U3GINIT_EJECT },
	{{ USB_VENDOR_DELL,    USB_PRODUCT_DELL_U740 },			U3GSP_HSDPA,	U3GINIT_EJECT },
	/* OEM: Merlin */
	{{ USB_VENDOR_MERLIN, USB_PRODUCT_MERLIN_V620 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	/* OEM: Sierra Wireless: */
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD580 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD595 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC595U },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC597E },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_C597 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880E },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880U },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881E },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881U },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_EM5625 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5720 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5720_2 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5725 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MINI5725 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD875 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755_2 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755_3 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8765 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC875U },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8775_2 },		U3GSP_HSDPA,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8780 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8781 },		U3GSP_UMTS,	U3GINIT_NONE },		// XXX
	{{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_TRUINSTALL },		U3GSP_UMTS,	U3GINIT_SIERRA },
	{{ USB_VENDOR_HP, USB_PRODUCT_HP_HS2300 },			U3GSP_HSDPA,	U3GINIT_NONE },
	/* OEM: CMOTECH */
	{{ USB_VENDOR_CMOTECH, USB_PRODUCT_CMOTECH_CGU628 },		U3GSP_HSDPA,	U3GINIT_CMOTECH },
	{{ USB_VENDOR_CMOTECH, USB_PRODUCT_CMOTECH_DISK },		U3GSP_HSDPA,	U3GINIT_NONE },
	/* OEM: Longcheer */
	{ USB_VENDOR_LONGCHEER, USB_PRODUCT_LONGCHEER_WM66 },		U3GSP_HSDPA,	U3GINIT_HUAWEI },
};
#define u3g_lookup(v, p) ((const struct u3g_dev_type_s *)usb_lookup(u3g_devs, v, p))

static int
u3g_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	const struct u3g_dev_type_s *u3g_dev_type;

	if (!uaa->iface)
		return UMATCH_NONE;

	u3g_dev_type = u3g_lookup(uaa->vendor, uaa->product);
	if (!u3g_dev_type)
		return UMATCH_NONE;

	/* If the interface class of the first interface is no longer
	 * mass storage the card has changed to modem.
	 */
	usb_interface_descriptor_t *id;
	id = usbd_get_interface_descriptor(uaa->iface);
	if (!id || id->bInterfaceClass == UICLASS_MASS)
		return UMATCH_NONE;

	return UMATCH_VENDOR_PRODUCT_CONF_IFACE;
}

static int
u3g_attach(device_t self)
{
	struct u3g_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	const struct u3g_dev_type_s *u3g_dev_type;
	usbd_device_handle dev = uaa->device;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	int i, n; 
	usb_config_descriptor_t *cd;
	char devnamefmt[32];

#if __FreeBSD_version < 700000
	char *devinfo = malloc(1024, M_USBDEV, M_WAITOK);
	usbd_devinfo(dev, 0, devinfo);
	device_printf(self, "%s\n", devinfo);
	free(devinfo, M_USBDEV);
#endif

	/* get the config descriptor */
	cd = usbd_get_config_descriptor(dev);
	if (cd == NULL) {
		device_printf(self, "failed to get configuration descriptor\n");
		return ENXIO;
	}

	sc->sc_dev = self;
	sc->sc_udev = dev;

	u3g_dev_type = u3g_lookup(uaa->vendor, uaa->product);
	sc->sc_init = u3g_dev_type->init;
	sc->sc_speed = u3g_dev_type->speed;

	int portno = 0;
	for (i = 0; i < uaa->nifaces; i++) {
		DPRINTF("Interface %d of %d, %sin use\n",
			i, uaa->nifaces,
			(uaa->ifaces[i]? "not ":""));
		if (uaa->ifaces[i] == NULL)
			continue;

		id = usbd_get_interface_descriptor(uaa->ifaces[i]);
		if (id && id->bInterfaceClass == UICLASS_MASS) {
			/* We attach to the interface instead of the device as
			 * some devices have a built-in SD card reader.
			 * Claim the first umass device (cdX) as it contains
			 * only Windows drivers anyway (CD-ROM), hiding it.
			 */
			if (!(bootverbose || u3gdebug))
				if (uaa->vendor == USB_VENDOR_HUAWEI)
					if (id->bInterfaceNumber == 2)
						uaa->ifaces[i] = NULL;
			continue;
		}

		int bulkin_no = -1, bulkout_no = -1;
		int claim_iface = 0;
		for (n = 0; n < id->bNumEndpoints; n++) {
			ed = usbd_interface2endpoint_descriptor(uaa->ifaces[i], n);
			DPRINTF(" Endpoint %d of %d%s\n",
				n, id->bNumEndpoints,
				(ed? "":"no descriptor"));
			if (ed == NULL)
				continue;
			if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN
			    && UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
				bulkin_no = ed->bEndpointAddress;
			else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT
				 && UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
				bulkout_no = ed->bEndpointAddress;

			/* If we have found a pair of bulk-in/-out endpoints
			 * create a serial port for it. Note: We assume that
			 * the bulk-in and bulk-out endpoints appear in pairs.
			 */
			if (bulkin_no != -1 && bulkout_no != -1) {
				sc->sc_ucom = realloc(sc->sc_ucom, (portno+1)*sizeof(struct ucom_softc), M_USBDEV, M_WAITOK);
				struct ucom_softc *ucom = &sc->sc_ucom[portno];

				ucom->sc_dev = self;
				ucom->sc_udev = dev;
				ucom->sc_iface = uaa->ifaces[i];
				ucom->sc_bulkin_no = bulkin_no;
				ucom->sc_bulkout_no = bulkout_no;
				ucom->sc_ibufsize = U3GIBUFSIZE;
				ucom->sc_ibufsizepad = U3GIBUFSIZE;
				ucom->sc_obufsize = U3GOBUFSIZE;
				ucom->sc_opkthdrlen = 0;

				ucom->sc_callback = &u3g_callback;
				ucom->sc_parent = sc;
				ucom->sc_portno = portno;

				DPRINTF("port=%d iface=%d in=0x%x out=0x%x\n",
					 portno, i,
					 ucom->sc_bulkin_no,
					 ucom->sc_bulkout_no);

				claim_iface = 1;
				portno++;
				bulkin_no = bulkout_no = -1;
			}
		}
		if (claim_iface)
			uaa->ifaces[i] = NULL;		// claim the interface
	}
	sc->sc_numports = portno;

	sprintf(devnamefmt,"U%d.%%d", device_get_unit(self));
	for (portno = 0; portno < sc->sc_numports; portno++) {
	    struct ucom_softc *ucom = &sc->sc_ucom[portno];

#if __FreeBSD_version < 700000
	    ucom_attach_tty(ucom, MINOR_CALLOUT, devnamefmt, portno);
#elif __FreeBSD_version < 800000
	    ucom_attach_tty(ucom, TS_CALLOUT, devnamefmt, portno);
#else
	    ucom_attach_tty(ucom, devnamefmt, portno);
#endif
	}

	device_printf(self, "configured %d serial ports (%s)\n",
		      sc->sc_numports, devnamefmt);
	return 0;
}

static int
u3g_detach(device_t self)
{
	struct u3g_softc *sc = device_get_softc(self);
	int rv = 0;
	int i;

	for (i = 0; i < sc->sc_numports; i++) {
		sc->sc_ucom[i].sc_dying = 1;
		rv = ucom_detach(&sc->sc_ucom[i]);
		if (rv != 0) {
			device_printf(self, "ucom_detach(U%d.%d\n", device_get_unit(self), i);
			return rv;
		}
	}

	if (sc->sc_ucom)
	    free(sc->sc_ucom, M_USBDEV);

	return 0;
}

static int
u3g_open(void *addr, int portno)
{
#if __FreeBSD_version < 800000
	/* Supply generous buffering for these cards to avoid disappointments
	 * when setting the speed incorrectly. Only do this for the first port
	 * assuming that the rest of the ports are used for diagnostics only
	 * anyway.
	 * Note: We abuse the fact that ucom sets the speed through
	 * ispeed/ospeed, not through ispeedwat/ospeedwat.
	 */
	if (portno == 0) {
		struct u3g_softc *sc = addr;
		struct ucom_softc *ucom = &sc->sc_ucom[portno];
		struct tty *tp = ucom->sc_tty;

		tp->t_ispeedwat = u3g_speeds[sc->sc_speed].ispeed;
		tp->t_ospeedwat = u3g_speeds[sc->sc_speed].ospeed;

		/* Avoid excessive buffer sizes.
		 * XXX The values here should be checked. Lower them and see
		 * whether 'lost chars' messages appear.
		 */
		if (tp->t_ispeedwat > 384000)
		    tp->t_ispeedwat = 384000;
		if (tp->t_ospeedwat > 384000)
		    tp->t_ospeedwat = 384000;

		ttsetwater(tp);
	}
#endif

	return 0;
}

static void
u3g_close(void *addr, int portno)
{
#if __FreeBSD_version < 800000
	if (portno == 0) {	/* see u3g_open() */
		/* Reduce the buffers allocated above again */
		struct u3g_softc *sc = addr;
		struct ucom_softc *ucom = &sc->sc_ucom[portno];
		struct tty *tp = ucom->sc_tty;

		tp->t_ispeedwat = (speed_t)-1;
		tp->t_ospeedwat = (speed_t)-1;

		ttsetwater(tp);
	}
#endif
}	

static device_method_t u3g_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, u3g_match),
	DEVMETHOD(device_attach, u3g_attach),
	DEVMETHOD(device_detach, u3g_detach),

	{ 0, 0 }
};

static driver_t u3g_driver = {
	"ucom",
	u3g_methods,
	sizeof (struct u3g_softc)
};

DRIVER_MODULE(u3g, uhub, u3g_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(u3g, usb, 1, 1, 1);
MODULE_DEPEND(u3g, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);
MODULE_VERSION(u3g, 1);

/*******************************************************************
 ****** Stub driver to hide devices that need to reinitialise ******
 *******************************************************************/

struct u3gstub_softc {
	device_t		sc_dev;
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;
	usbd_pipe_handle 	sc_pipe_out, sc_pipe_in;
	usbd_xfer_handle 	sc_xfer;
	int			sc_vendor;
	int			sc_product;

	struct usb_task		sc_task;

	u_char			sc_dying;
};

/* See definition of umass_bbb_cbw_t in sys/dev/usb/umass.c for the SCSI/ATAPI command structs below.
 */

/*
 * See struct scsi_test_unit_ready in sys/cam/scsi/scsi_all.h .
 */
static unsigned char scsi_test_unit_ready[31] = {
    0x55, 0x53, 0x42, 0x43,	/* 0..3: Command Block Wrapper (CBW) signature */
    0x01, 0x00, 0x00, 0x00,	/* 4..7: CBW Tag, unique 32-bit number */
    0x00, 0x00, 0x00, 0x00,	/* 8..11: CBW Transfer Length, no data here */
    0x00,			/* 12: CBW Flag: input */
    0x00,			/* 13: CBW Lun */
    0x0c,			/* 14: CBW Length */

    0x00,			/* 15+0: opcode: SCSI TEST UNIT READY*/
    0x00,			/* 15+1: byte2: Not immediate */
    0x00, 0x00,			/* 15+2..3: reserved */
    0x00,			/* 15+4: Load/Eject command */
    0x00,			/* 15+5: control */
    0x00, 0x00, 0x00, 0x00,	/* 15+6..15: unused */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

/*
 * See struct scsi_start_stop_unit in sys/cam/scsi/scsi_all.h .
 */
static unsigned char scsi_start_stop_unit[31] = {
    0x55, 0x53, 0x42, 0x43,	/* 0..3: Command Block Wrapper (CBW) signature */
    0x01, 0x00, 0x00, 0x00,	/* 4..7: CBW Tag, unique 32-bit number */
    0x00, 0x00, 0x00, 0x00,	/* 8..11: CBW Transfer Length, no data here */
    0x00,			/* 12: CBW Flag: input */
    0x00,			/* 13: CBW Lun */
    0x0c,			/* 14: CBW Length */

    0x1b,			/* 15+0: opcode: SCSI START/STOP */
    0x00,			/* 15+1: byte2: Not immediate */
    0x00, 0x00,			/* 15+2..3: reserved */
    0x02,			/* 15+4: Load/Eject command */
    0x00,			/* 15+5: control */
    0x00, 0x00, 0x00, 0x00,	/* 15+6..15: unused */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

static unsigned char ztestor_cmd[31] = {
    0x55, 0x53, 0x42, 0x43,	/* 0..3: Command Block Wrapper (CBW) signature */
    0x01, 0x00, 0x00, 0x00,	/* 4..7: CBW Tag, unique 32-bit number */
    0x00, 0x00, 0x00, 0x00,	/* 8..11: CBW Transfer Length, no data here */
    0x00,			/* 12: CBW Flag: input */
    0x00,			/* 13: CBW Lun */
    0x0c,			/* 14: CBW Length */

    0x85,			/* 15+0: opcode */
    0x01,			/* 15+1: byte2 */
    0x01, 0x01,			/* 15+2..3 */
    0x18,			/* 15+4: */
    0x01,			/* 15+5: */
    0x01, 0x01, 0x01, 0x01,	/* 15+6..15: */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

static unsigned char cmotech_cmd[31] = {
    0x55, 0x53, 0x42, 0x43,	/* 0..3: Command Block Wrapper (CBW) signature */
    0x01, 0x00, 0x00, 0x00,	/* 4..7: CBW Tag, unique 32-bit number */
    0x00, 0x00, 0x00, 0x00,	/* 8..11: CBW Transfer Length, no data here */
    0x80,			/* 12: CBW Flag: output, so 0 */
    0x00,			/* 13: CBW Lun */
    0x08,			/* 14: CBW Length */	/* XXX Is this correct? Not 6,10,12, or 20? */

    0xff,			/* 15+0 */
    0x52,			/* 15+1 */
    0x44,			/* 15+2 */
    0x45,			/* 15+2 */
    0x56,			/* 15+4 */
    0x43,			/* 15+5 */
    0x48,			/* 15+5 */
    0x47,			/* 15+5 */
    0x00, 0x00, 0x00, 0x00,	/* 15+8..15: unused */
    0x00, 0x00, 0x00, 0x00
};


/*!
 * \brief Execute a command over BBB protocol (see umass driver).
 * \param sc softc
 * \param cmd 31 byte CBW buffer
 * \returns 0 if transfer was cancelled, 1 otherwise.
 * \note If 0 is returned detach was probably called and no data from the
 * softc should be touched.
 */
static int
u3gstub_BBB_cmd(struct u3gstub_softc *sc, unsigned char *cmd)
{
	int err;

	DPRINTF("Sending CBW\n");
	usbd_setup_xfer(sc->sc_xfer, sc->sc_pipe_out, sc,
			cmd, 31 /* CBW len */,
			0, USBD_DEFAULT_TIMEOUT, NULL);
	err = usbd_sync_transfer(sc->sc_xfer);
	
	if (err == USBD_CANCELLED) {
		return 0;
	} else if (err == USBD_STALLED) {
		DPRINTF("Sending CBW, STALLED\n");
		err = usbd_clear_endpoint_stall(sc->sc_pipe_out);
		if (err != USBD_NORMAL_COMPLETION) {
			device_printf(sc->sc_dev,
				      "Failed to send CBW to "
				      "change to modem mode, "
				      "clear endpoint stall failed: %s\n",
				      usbd_errstr(err));
		}
	} else if (err) {
		device_printf(sc->sc_dev,
			      "Failed to send CBW to "
		              "change to modem mode: %s\n",
			      usbd_errstr(err));
	} else {
		DPRINTF("Reading CSW\n");
		usbd_setup_xfer(sc->sc_xfer, sc->sc_pipe_in, sc,
				cmd, sizeof(cmd),
				0, USBD_DEFAULT_TIMEOUT, NULL);
		err = usbd_sync_transfer(sc->sc_xfer);
		if (err == USBD_CANCELLED) {
			return 0;
		} else if (err == USBD_STALLED) {
			DPRINTF("Reading CSW, STALLED\n");
			err = usbd_clear_endpoint_stall(sc->sc_pipe_out);
			if (err != USBD_NORMAL_COMPLETION) {
				device_printf(sc->sc_dev,
					      "Failed to retrieve CSW to "
					      "change to modem mode, "
					      "clear endpoint stall failed: %s\n",
					      usbd_errstr(err));
			}
		} else if (err != USBD_NORMAL_COMPLETION) {
			if (u3gdebug)
				device_printf(sc->sc_dev,
					      "Failed to retrieve CSW to "
					      "change to modem mode: %s\n",
					      usbd_errstr(err));
		}
	}

	return 1;
}

static int
u3gstub_sierra_init(struct u3gstub_softc *sc)
{
	usb_device_request_t req;
	int err;

	req.bmRequestType = UT_VENDOR;
	req.bRequest = UR_SET_INTERFACE;
	USETW(req.wValue, UF_DEVICE_REMOTE_WAKEUP);
	USETW(req.wIndex, UHF_PORT_CONNECTION);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_udev, &req, 0);
	if (err && u3gdebug)
		device_printf(sc->sc_dev,
			      "Failed to send Sierra request: %s\n",
			      usbd_errstr(err));

	return 1;
}

static int
u3gstub_huawei_init(struct u3gstub_softc *sc)
{
	usb_device_request_t req;
	int err;

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, UF_DEVICE_REMOTE_WAKEUP);
	USETW(req.wIndex, UHF_PORT_SUSPEND);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_udev, &req, 0);
	if (err && u3gdebug)
		device_printf(sc->sc_dev,
			      "Failed to send Huawei request: %s\n",
			      usbd_errstr(err));

	return 1;
}

/*!
 * \brief Execute the requested init command for the device.
 * \param priv u3gstub_softc
 * 
 * This is implemented as a task so we can do synchronous transfers.
 */
static void
u3gstub_do_init(void *priv)
{
	struct u3gstub_softc *sc = priv;
	const struct u3g_dev_type_s *u3g_dev_type;

	u3g_dev_type = u3g_lookup(sc->sc_vendor, sc->sc_product);
	switch (u3g_dev_type->init) {
	case U3GINIT_HUAWEI:
		if (bootverbose || u3gdebug)
			device_printf(sc->sc_dev,
				      "changing Huawei modem to modem mode\n");
		u3gstub_huawei_init(sc);
		break;
	case U3GINIT_SIERRA:
		if (bootverbose || u3gdebug)
			device_printf(sc->sc_dev,
				      "changing Sierra modem to modem mode\n");
		u3gstub_sierra_init(sc);
		break;
	case U3GINIT_EJECT:
		if (bootverbose || u3gdebug)
			device_printf(sc->sc_dev,
				      "sending CD eject command to change to modem mode\n");
		while (!sc->sc_dying
		       && u3gstub_BBB_cmd(sc, scsi_test_unit_ready)
		       && u3gstub_BBB_cmd(sc, scsi_start_stop_unit))
			;	/* nop */
		break;
	case U3GINIT_ZTESTOR:
		if (bootverbose || u3gdebug)
			device_printf(sc->sc_dev,
				      "changing ZTE STOR modem to modem mode\n");
		u3gstub_BBB_cmd(sc, ztestor_cmd);
		break;
	case U3GINIT_CMOTECH:
		if (bootverbose || u3gdebug)
			device_printf(sc->sc_dev,
				      "changing CMOTECH modem to modem mode\n");
		u3gstub_BBB_cmd(sc, cmotech_cmd);
		break;
	case U3GINIT_WAIT:
	default:
		if (bootverbose || u3gdebug)
			device_printf(sc->sc_dev,
				      "waiting for modem to change to modem mode\n");
		/* nop  */
	}
}

static int
u3gstub_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	const struct u3g_dev_type_s *u3g_dev_type;
	usb_interface_descriptor_t *id;

	/* This stub handles 3G modem devices (E220, Mobile, etc.) with
	 * auto-install flash disks for Windows/MacOSX on the first interface.
	 * After some command or some delay they change appearance to a modem.
	 */

	if (!uaa->iface)
		return UMATCH_NONE;

	u3g_dev_type = u3g_lookup(uaa->vendor, uaa->product);
	if (!u3g_dev_type)
		return UMATCH_NONE;

	if (u3g_dev_type->init != U3GINIT_NONE) {
		/* We assume that if the first interface is still a mass
		 * storage device the device has not yet changed appearance.
		 */
		id = usbd_get_interface_descriptor(uaa->iface);
		if (id && id->bInterfaceNumber == 0
		    && id->bInterfaceClass == UICLASS_MASS) {
			if (u3gdebug == 0)
				device_quiet(self);

			return UMATCH_VENDOR_PRODUCT;
		}
	}

	return UMATCH_NONE;
}

static int
u3gstub_attach(device_t self)
{
	struct u3gstub_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	int i, err;

	if (u3gdebug == 0)
		device_quiet(self);

	for (i = 0; i < uaa->nifaces; i++)
		uaa->ifaces[i] = NULL;		// claim all interfaces

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;
	sc->sc_iface = uaa->iface;
	sc->sc_vendor = uaa->vendor;
	sc->sc_product = uaa->product;

	sc->sc_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_xfer == NULL) {
		DPRINTF("failed to allocate xfer\n");
		return ENOMEM;
	}

	/* Find the bulk-out endpoints */
	id = usbd_get_interface_descriptor(sc->sc_iface);
	for (i = 0 ; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed != NULL
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			if (!sc->sc_pipe_out
			    && UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT) {
				err = usbd_open_pipe(sc->sc_iface,
						     ed->bEndpointAddress,
						     USBD_EXCLUSIVE_USE,
						     &sc->sc_pipe_out);
				if (err != USBD_NORMAL_COMPLETION) {
					DPRINTF("failed to open bulk-out pipe on endpoint %d\n",
						ed->bEndpointAddress);
					return 0;
				} else {
					DPRINTF("opening bulk-in pipe on endpoint %d\n", ed->bEndpointAddress);
				}
			} else if (!sc->sc_pipe_in
				   && UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN) {
				err = usbd_open_pipe(sc->sc_iface,
						     ed->bEndpointAddress,
						     USBD_EXCLUSIVE_USE,
						     &sc->sc_pipe_in);
				if (err != USBD_NORMAL_COMPLETION) {
					DPRINTF("failed to open bulk-in pipe on endpoint %d\n",
						ed->bEndpointAddress);
					return 0;
				} else {
					DPRINTF("opening bulk-in pipe on endpoint %d\n", ed->bEndpointAddress);
				}
			}
		}
		if (sc->sc_pipe_out && sc->sc_pipe_in)
			break;
	}

	if (i == id->bNumEndpoints) {
		device_printf(sc->sc_dev, "failed to find bulk-out and/or bulk-in pipe\n");
		return ENXIO;
	}

	usb_init_task(&sc->sc_task, u3gstub_do_init, sc);
	usb_add_task(sc->sc_udev, &sc->sc_task, USB_TASKQ_DRIVER);

	return 0;
}

static int
u3gstub_detach(device_t self)
{
	struct u3gstub_softc *sc = device_get_softc(self);

	sc->sc_dying = 1;
	usb_rem_task(sc->sc_udev, &sc->sc_task);

	if (sc->sc_pipe_in) {
		usbd_abort_pipe(sc->sc_pipe_in);
		usbd_close_pipe(sc->sc_pipe_in);
	}
	if (sc->sc_pipe_out) {
		usbd_abort_pipe(sc->sc_pipe_out);
		usbd_close_pipe(sc->sc_pipe_out);
	}

	if (sc->sc_xfer)
		usbd_free_xfer(sc->sc_xfer);

	return 0;
}

static device_method_t u3gstub_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, u3gstub_match),
	DEVMETHOD(device_attach, u3gstub_attach),
	DEVMETHOD(device_detach, u3gstub_detach),

	{ 0, 0 }
};

static driver_t u3gstub_driver = {
	"u3g",
	u3gstub_methods,
	sizeof (struct u3gstub_softc)
};

DRIVER_MODULE(u3gstub, uhub, u3gstub_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(u3gstub, usb, 1, 1, 1);
