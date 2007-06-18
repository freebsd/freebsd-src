/*	$NetBSD: uvisor.c,v 1.9 2001/01/23 14:04:14 augustss Exp $	*/
/*      $FreeBSD$	*/

/* Also already merged from NetBSD:
 *	$NetBSD: uvisor.c,v 1.12 2001/11/13 06:24:57 lukem Exp $
 *	$NetBSD: uvisor.c,v 1.13 2002/02/11 15:11:49 augustss Exp $
 *	$NetBSD: uvisor.c,v 1.14 2002/02/27 23:00:03 augustss Exp $
 *	$NetBSD: uvisor.c,v 1.15 2002/06/16 15:01:31 augustss Exp $
 *	$NetBSD: uvisor.c,v 1.16 2002/07/11 21:14:36 augustss Exp $
 *	$NetBSD: uvisor.c,v 1.17 2002/08/13 11:38:15 augustss Exp $
 *	$NetBSD: uvisor.c,v 1.18 2003/02/05 00:50:14 augustss Exp $
 *	$NetBSD: uvisor.c,v 1.19 2003/02/07 18:12:37 augustss Exp $
 *	$NetBSD: uvisor.c,v 1.20 2003/04/11 01:30:10 simonb Exp $
 */


/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Handspring Visor (Palmpilot compatible PDA) driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#include <dev/usb/ucomvar.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (uvisordebug) printf x
#define DPRINTFN(n,x)	if (uvisordebug>(n)) printf x
int uvisordebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, uvisor, CTLFLAG_RW, 0, "USB uvisor");
SYSCTL_INT(_hw_usb_uvisor, OID_AUTO, debug, CTLFLAG_RW,
	   &uvisordebug, 0, "uvisor debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UVISOR_CONFIG_INDEX	0
#define UVISOR_IFACE_INDEX	0
#define UVISOR_MODVER		1

/* From the Linux driver */
/*
 * UVISOR_REQUEST_BYTES_AVAILABLE asks the visor for the number of bytes that
 * are available to be transfered to the host for the specified endpoint.
 * Currently this is not used, and always returns 0x0001
 */
#define UVISOR_REQUEST_BYTES_AVAILABLE		0x01

/*
 * UVISOR_CLOSE_NOTIFICATION is set to the device to notify it that the host
 * is now closing the pipe. An empty packet is sent in response.
 */
#define UVISOR_CLOSE_NOTIFICATION		0x02

/*
 * UVISOR_GET_CONNECTION_INFORMATION is sent by the host during enumeration to
 * get the endpoints used by the connection.
 */
#define UVISOR_GET_CONNECTION_INFORMATION	0x03


/*
 * UVISOR_GET_CONNECTION_INFORMATION returns data in the following format
 */
#define UVISOR_MAX_CONN 8
struct uvisor_connection_info {
	uWord	num_ports;
	struct {
		uByte	port_function_id;
		uByte	port;
	} connections[UVISOR_MAX_CONN];
};
#define UVISOR_CONNECTION_INFO_SIZE 18

/* struct uvisor_connection_info.connection[x].port defines: */
#define UVISOR_ENDPOINT_1		0x01
#define UVISOR_ENDPOINT_2		0x02

/* struct uvisor_connection_info.connection[x].port_function_id defines: */
#define UVISOR_FUNCTION_GENERIC		0x00
#define UVISOR_FUNCTION_DEBUGGER	0x01
#define UVISOR_FUNCTION_HOTSYNC		0x02
#define UVISOR_FUNCTION_CONSOLE		0x03
#define UVISOR_FUNCTION_REMOTE_FILE_SYS	0x04

/*
 * Unknown PalmOS stuff.
 */
#define UVISOR_GET_PALM_INFORMATION		0x04
#define UVISOR_GET_PALM_INFORMATION_LEN		0x44

struct uvisor_palm_connection_info {
        uByte   num_ports;
        uByte   endpoint_numbers_different;
        uWord   reserved1;
  struct {
                uDWord  port_function_id;
                uByte   port;
                uByte   end_point_info;
                uWord   reserved;
  } connections[UVISOR_MAX_CONN];
};


/*
 * Crank down UVISORBUFSIZE from 1024 to 64 to avoid a problem where
 * the Palm device and the USB host controller deadlock. The USB host
 * controller is expecting an early-end-of-transmission packet with 0
 * data, and the Palm doesn't send one because it's already
 * communicated the amount of data it's going to send in a header
 * (which ucom/uvisor are oblivious to). This is the problem that has
 * been known on the pilot-link lists as the "[Free]BSD USB problem",
 * but not understood.
 */
#define UVISORIBUFSIZE 64
#define UVISOROBUFSIZE 1024

struct uvisor_softc {
	struct ucom_softc	sc_ucom;
	u_int16_t		sc_flags;
};

static usbd_status uvisor_init(struct uvisor_softc *);

/*static usbd_status clie_3_5_init(struct uvisor_softc *);*/

static void uvisor_close(void *, int);

struct ucom_callback uvisor_callback = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	uvisor_close,
	NULL,
	NULL,
};

static device_probe_t uvisor_match;
static device_attach_t uvisor_attach;
static device_detach_t uvisor_detach;
static device_method_t uvisor_methods[] = {
       /* Device interface */
       DEVMETHOD(device_probe, uvisor_match),
       DEVMETHOD(device_attach, uvisor_attach),
       DEVMETHOD(device_detach, uvisor_detach),
       { 0, 0 }
 };


static driver_t uvisor_driver = {
       "ucom",
       uvisor_methods,
       sizeof (struct uvisor_softc)
};

DRIVER_MODULE(uvisor, uhub, uvisor_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(uvisor, usb, 1, 1, 1);
MODULE_DEPEND(uvisor, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);
MODULE_VERSION(uvisor, UVISOR_MODVER);

struct uvisor_type {
	struct usb_devno	uv_dev;
	u_int16_t		uv_flags;
#define PALM4	0x0001
#define VISOR   0x0002
#define PALM35  0x0004
};
static const struct uvisor_type uvisor_devs[] = {
	{{ USB_VENDOR_ACEECA, USB_PRODUCT_ACEECA_MEZ1000 }, PALM4 },
	{{ USB_VENDOR_HANDSPRING, USB_PRODUCT_HANDSPRING_VISOR }, VISOR },
	{{ USB_VENDOR_HANDSPRING, USB_PRODUCT_HANDSPRING_TREO }, PALM4 },
	{{ USB_VENDOR_HANDSPRING, USB_PRODUCT_HANDSPRING_TREO600 }, PALM4 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_M500 }, PALM4 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_M505 }, PALM4 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_M515 }, PALM4 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_I705 }, PALM4 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_M125 }, PALM4 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_M130 }, PALM4 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_TUNGSTEN_Z }, PALM4 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_TUNGSTEN_T }, PALM4 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_ZIRE }, PALM4 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_ZIRE31 }, PALM4 },
	{{ USB_VENDOR_SONY, USB_PRODUCT_SONY_CLIE_40 }, 0 },
	{{ USB_VENDOR_SONY, USB_PRODUCT_SONY_CLIE_41 }, PALM4 },
	{{ USB_VENDOR_SONY, USB_PRODUCT_SONY_CLIE_S360 }, PALM4 },
	{{ USB_VENDOR_SONY, USB_PRODUCT_SONY_CLIE_NX60 }, PALM4 },
	{{ USB_VENDOR_SONY, USB_PRODUCT_SONY_CLIE_35 }, PALM35 },
/*	{{ USB_VENDOR_SONY, USB_PRODUCT_SONY_CLIE_25 }, PALM4 },*/
	{{ USB_VENDOR_SONY, USB_PRODUCT_SONY_CLIE_TJ37 }, PALM4 },
};
#define uvisor_lookup(v, p) ((const struct uvisor_type *)usb_lookup(uvisor_devs, v, p))


static int
uvisor_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	DPRINTFN(20,("uvisor: vendor=0x%x, product=0x%x\n",
		     uaa->vendor, uaa->product));

	return (uvisor_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

static int
uvisor_attach(device_t self)
{
	struct uvisor_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	const char *devname;
	int i;
	usbd_status err;
	struct ucom_softc *ucom;

	ucom = &sc->sc_ucom;
	ucom->sc_dev = self;
	ucom->sc_udev = dev;
	ucom->sc_iface = uaa->iface;

	devname = device_get_nameunit(ucom->sc_dev);

	DPRINTFN(10,("\nuvisor_attach: sc=%p\n", sc));

	/* Move the device into the configured state. */
	err = usbd_set_config_index(dev, UVISOR_CONFIG_INDEX, 1);
	if (err) {
		printf("\n%s: failed to set configuration, err=%s\n",
		       devname, usbd_errstr(err));
		goto bad;
	}

	err = usbd_device2interface_handle(dev, UVISOR_IFACE_INDEX, &iface);
	if (err) {
		printf("\n%s: failed to get interface, err=%s\n",
		       devname, usbd_errstr(err));
		goto bad;
	}

	sc->sc_flags = uvisor_lookup(uaa->vendor, uaa->product)->uv_flags;

	id = usbd_get_interface_descriptor(iface);

	ucom->sc_udev = dev;
	ucom->sc_iface = iface;

	ucom->sc_bulkin_no = ucom->sc_bulkout_no = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		int addr, dir, attr;
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("%s: could not read endpoint descriptor"
			       ": %s\n", devname, usbd_errstr(err));
			goto bad;
		}

		addr = ed->bEndpointAddress;
		dir = UE_GET_DIR(ed->bEndpointAddress);
		attr = ed->bmAttributes & UE_XFERTYPE;
		if (dir == UE_DIR_IN && attr == UE_BULK)
			ucom->sc_bulkin_no = addr;
		else if (dir == UE_DIR_OUT && attr == UE_BULK)
			ucom->sc_bulkout_no = addr;
		else {
			printf("%s: unexpected endpoint\n", devname);
			goto bad;
		}
	}
	if (ucom->sc_bulkin_no == -1) {
		printf("%s: Could not find data bulk in\n",
		       device_get_nameunit(ucom->sc_dev));
		goto bad;
	}
	if (ucom->sc_bulkout_no == -1) {
		printf("%s: Could not find data bulk out\n",
		       device_get_nameunit(ucom->sc_dev));
		goto bad;
	}

	ucom->sc_parent = sc;
	ucom->sc_portno = UCOM_UNK_PORTNO;
	/* bulkin, bulkout set above */
	ucom->sc_ibufsize = UVISORIBUFSIZE;
	ucom->sc_obufsize = UVISOROBUFSIZE;
	ucom->sc_ibufsizepad = UVISORIBUFSIZE;
	ucom->sc_opkthdrlen = 0;
	ucom->sc_callback = &uvisor_callback;

#if 0
	if (uaa->vendor == USB_VENDOR_SONY &&
	    uaa->product == USB_PRODUCT_SONY_CLIE_35)
		err = clie_3_5_init(sc);
	else
#endif
		err = uvisor_init(sc);

	if (err) {
		printf("%s: init failed, %s\n", device_get_nameunit(ucom->sc_dev),
		       usbd_errstr(err));
		goto bad;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, ucom->sc_udev,
	  ucom->sc_dev);

	DPRINTF(("uvisor: in=0x%x out=0x%x\n", ucom->sc_bulkin_no, ucom->sc_bulkout_no));
	ucom_attach(&sc->sc_ucom);

	return 0;

bad:
	DPRINTF(("uvisor_attach: ATTACH ERROR\n"));
	ucom->sc_dying = 1;
	return ENXIO;
}

#if 0

int
uvisor_activate(device_t self, enum devact act)
{
	struct uvisor_softc *sc = (struct uvisor_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_subdev != NULL)
			rv = config_deactivate(sc->sc_subdev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}

#endif

static int
uvisor_detach(device_t self)
{
	struct uvisor_softc *sc = device_get_softc(self);
	int rv = 0;

	DPRINTF(("uvisor_detach: sc=%p\n", sc));
	sc->sc_ucom.sc_dying = 1;
	rv = ucom_detach(&sc->sc_ucom);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_ucom.sc_udev,
	  sc->sc_ucom.sc_dev);

	return (rv);
}

usbd_status
uvisor_init(struct uvisor_softc *sc)
{
	usbd_status err;
	usb_device_request_t req;
	struct uvisor_connection_info coninfo;
	struct uvisor_palm_connection_info pconinfo;
	int actlen;
	uWord avail;
	char buffer[256];

	if (sc->sc_flags & VISOR) {
	  DPRINTF(("uvisor_init: getting connection info\n"));
	  req.bmRequestType = UT_READ_VENDOR_ENDPOINT;
	  req.bRequest = UVISOR_GET_CONNECTION_INFORMATION;
	  USETW(req.wValue, 0);
	  USETW(req.wIndex, 0);
	  USETW(req.wLength, UVISOR_CONNECTION_INFO_SIZE);
	  err = usbd_do_request_flags(sc->sc_ucom.sc_udev, &req, &coninfo,
				      USBD_SHORT_XFER_OK, &actlen,
				      USBD_DEFAULT_TIMEOUT);
	  if (err)
	    return (err);
	}
#ifdef USB_DEBUG
	{
		int i, np;
		char *string;

		np = UGETW(coninfo.num_ports);
		printf("%s: Number of ports: %d\n", device_get_nameunit(sc->sc_ucom.sc_dev), np);
		for (i = 0; i < np; ++i) {
			switch (coninfo.connections[i].port_function_id) {
			case UVISOR_FUNCTION_GENERIC:
				string = "Generic";
				break;
			case UVISOR_FUNCTION_DEBUGGER:
				string = "Debugger";
				break;
			case UVISOR_FUNCTION_HOTSYNC:
				string = "HotSync";
				break;
			case UVISOR_FUNCTION_REMOTE_FILE_SYS:
				string = "Remote File System";
				break;
			default:
				string = "unknown";
				break;
			}
			printf("%s: port %d, is for %s\n",
			    device_get_nameunit(sc->sc_ucom.sc_dev), coninfo.connections[i].port,
			    string);
		}
	}
#endif

	if (sc->sc_flags & PALM4) {
	        int port;
		/* Palm OS 4.0 Hack */
		req.bmRequestType = UT_READ_VENDOR_ENDPOINT;
		req.bRequest = UVISOR_GET_PALM_INFORMATION;
		USETW(req.wValue, 0);
		USETW(req.wIndex, 0);
		USETW(req.wLength, UVISOR_GET_PALM_INFORMATION_LEN);
		err = usbd_do_request_flags(sc->sc_ucom.sc_udev, &req, &pconinfo,
					    USBD_SHORT_XFER_OK, &actlen,
					    USBD_DEFAULT_TIMEOUT);
		if (err)
			return (err);

		if (pconinfo.endpoint_numbers_different) {
			port = pconinfo.connections[0].end_point_info;
			sc->sc_ucom.sc_bulkin_no = (port >> 4) | UE_DIR_IN;
			sc->sc_ucom.sc_bulkout_no = (port & 0xf) | UE_DIR_OUT;
		} else {
			port = pconinfo.connections[0].port;
			sc->sc_ucom.sc_bulkin_no = port | UE_DIR_IN;
			sc->sc_ucom.sc_bulkout_no = port | UE_DIR_OUT;
		}
#if 0
		req.bmRequestType = UT_READ_VENDOR_ENDPOINT;
		req.bRequest = UVISOR_GET_PALM_INFORMATION;
		USETW(req.wValue, 0);
		USETW(req.wIndex, 0);
		USETW(req.wLength, UVISOR_GET_PALM_INFORMATION_LEN);
		err = usbd_do_request(sc->sc_ucom.sc_udev, &req, buffer);
		if (err)
			return (err);
#endif
	}

	if (sc->sc_flags & PALM35) {
	  /* get the config number */
	  DPRINTF(("clie_3_5_init: getting config info\n"));
	  req.bmRequestType = UT_READ;
	  req.bRequest = UR_GET_CONFIG;
	  USETW(req.wValue, 0);
	  USETW(req.wIndex, 0);
	  USETW(req.wLength, 1);
	  err = usbd_do_request(sc->sc_ucom.sc_udev, &req, buffer);
	  if (err)
	    return (err);
  
	  /* get the interface number */
	  DPRINTF(("clie_3_5_init: get the interface number\n"));
	  req.bmRequestType = UT_READ_DEVICE;
	  req.bRequest = UR_GET_INTERFACE;
	  USETW(req.wValue, 0);
	  USETW(req.wIndex, 0);
	  USETW(req.wLength, 1);
	  err = usbd_do_request(sc->sc_ucom.sc_udev, &req, buffer);
	  if (err)
	    return (err);
	}

	DPRINTF(("uvisor_init: getting available bytes\n"));
	req.bmRequestType = UT_READ_VENDOR_ENDPOINT;
	req.bRequest = UVISOR_REQUEST_BYTES_AVAILABLE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 5);
	USETW(req.wLength, sizeof avail);
	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, &avail);
	if (err)
		return (err);
	DPRINTF(("uvisor_init: avail=%d\n", UGETW(avail)));

	DPRINTF(("uvisor_init: done\n"));
	return (err);
}

#if 0
usbd_status
clie_3_5_init(struct uvisor_softc *sc)
{
	usbd_status err;
	usb_device_request_t req;
	char buffer[256];

	/*
	 * Note that PEG-300 series devices expect the following two calls.
	 */

	/* get the config number */
	DPRINTF(("clie_3_5_init: getting config info\n"));
	req.bmRequestType = UT_READ;
	req.bRequest = UR_GET_CONFIG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 1);
	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, buffer);
	if (err)
		return (err);

	/* get the interface number */
	DPRINTF(("clie_3_5_init: get the interface number\n"));
	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_INTERFACE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 1);
	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, buffer);
	if (err)
		return (err);

#ifdef USB_DEBUG
	{
		struct uvisor_connection_info coninfo;
		int i, np;
		char *string;

		np = UGETW(coninfo.num_ports);
		DPRINTF(("%s: Number of ports: %d\n", device_get_nameunit(sc->sc_ucom.sc_dev), np));
		for (i = 0; i < np; ++i) {
			switch (coninfo.connections[i].port_function_id) {
			case UVISOR_FUNCTION_GENERIC:
				string = "Generic";
				break;
			case UVISOR_FUNCTION_DEBUGGER:
				string = "Debugger";
				break;
			case UVISOR_FUNCTION_HOTSYNC:
				string = "HotSync";
				break;
			case UVISOR_FUNCTION_REMOTE_FILE_SYS:
				string = "Remote File System";
				break;
			default:
				string = "unknown";
				break;	
			}
			DPRINTF(("%s: port %d, is for %s\n",
			    device_get_nameunit(sc->sc_ucom.sc_dev), coninfo.connections[i].port,
			    string));
		}
	}
#endif

	DPRINTF(("clie_3_5_init: done\n"));
	return (err);
}
#endif

void
uvisor_close(void *addr, int portno)
{
	struct uvisor_softc *sc = addr;
	usb_device_request_t req;
	struct uvisor_connection_info coninfo; /* XXX ? */
	int actlen;

	if (sc->sc_ucom.sc_dying)
		return;

	req.bmRequestType = UT_READ_VENDOR_ENDPOINT; /* XXX read? */
	req.bRequest = UVISOR_CLOSE_NOTIFICATION;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, UVISOR_CONNECTION_INFO_SIZE);
	(void)usbd_do_request_flags(sc->sc_ucom.sc_udev, &req, &coninfo,
				    USBD_SHORT_XFER_OK, &actlen,
				    USBD_DEFAULT_TIMEOUT);
}
