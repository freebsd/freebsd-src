/*	$NetBSD: ulpt.c,v 1.60 2003/10/04 21:19:50 augustss Exp $	*/

/*-
 * Copyright (c) 1998, 2003 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Printer Class spec: http://www.usb.org/developers/data/devclass/usbprint109.PDF
 */

/* XXXimp: need to migrate from devclass_get_softc */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"
#include <dev/usb/usb_quirks.h>

#define	TIMEOUT		hz*16	/* wait up to 16 seconds for a ready */
#define	STEP		hz/4

#define	LPTPRI		(PZERO+8)
#define	ULPT_BSIZE	PAGE_SIZE

#define ULPT_READS_PER_SEC 5
#define ULPT_READ_TIMO 10

#ifdef USB_DEBUG
#define DPRINTF(x)	if (ulptdebug) printf x
#define DPRINTFN(n,x)	if (ulptdebug>(n)) printf x
int	ulptdebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, ulpt, CTLFLAG_RW, 0, "USB ulpt");
SYSCTL_INT(_hw_usb_ulpt, OID_AUTO, debug, CTLFLAG_RW,
	   &ulptdebug, 0, "ulpt debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UR_GET_DEVICE_ID 0
#define UR_GET_PORT_STATUS 1
#define UR_SOFT_RESET 2

#define	LPS_NERR		0x08	/* printer no error */
#define	LPS_SELECT		0x10	/* printer selected */
#define	LPS_NOPAPER		0x20	/* printer out of paper */
#define LPS_INVERT      (LPS_SELECT|LPS_NERR)
#define LPS_MASK        (LPS_SELECT|LPS_NERR|LPS_NOPAPER)

struct ulpt_softc {
	device_t sc_dev;
	usbd_device_handle sc_udev;	/* device */
	usbd_interface_handle sc_iface;	/* interface */
	int sc_ifaceno;

	int sc_out;
	usbd_pipe_handle sc_out_pipe;	/* bulk out pipe */
	usbd_xfer_handle sc_out_xfer;
	void *sc_out_buf;

	int sc_in;
	usbd_pipe_handle sc_in_pipe;	/* bulk in pipe */
	usbd_xfer_handle sc_in_xfer;
	void *sc_in_buf;

	struct callout sc_read_callout;
	int sc_has_callout;

	u_char sc_state;
#define	ULPT_OPEN	0x01	/* device is open */
#define	ULPT_OBUSY	0x02	/* printer is busy doing output */
#define	ULPT_INIT	0x04	/* waiting to initialize for open */
	u_char sc_flags;
#define	ULPT_NOPRIME	0x40	/* don't prime on open */
	u_char sc_laststatus;

	int sc_refcnt;
	u_char sc_dying;

	struct cdev *dev;
	struct cdev *dev_noprime;
};

static d_open_t ulptopen;
static d_close_t ulptclose;
static d_write_t ulptwrite;
static d_read_t ulptread;
static d_ioctl_t ulptioctl;


static struct cdevsw ulpt_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	ulptopen,
	.d_close =	ulptclose,
	.d_write =	ulptwrite,
	.d_read =	ulptread,
	.d_ioctl =	ulptioctl,
	.d_name =	"ulpt",
};

void ulpt_disco(void *);

int ulpt_do_write(struct ulpt_softc *, struct uio *uio, int);
int ulpt_do_read(struct ulpt_softc *, struct uio *uio, int);
int ulpt_status(struct ulpt_softc *);
void ulpt_reset(struct ulpt_softc *);
int ulpt_statusmsg(u_char, struct ulpt_softc *);
void ulpt_read_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
		  usbd_status status);
void ulpt_tick(void *xsc);

#if 0
void ieee1284_print_id(char *);
#endif

#define	ULPTUNIT(s)	(minor(s) & 0x1f)
#define	ULPTFLAGS(s)	(minor(s) & 0xe0)

static device_probe_t ulpt_match;
static device_attach_t ulpt_attach;
static device_detach_t ulpt_detach;

static device_method_t ulpt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ulpt_match),
	DEVMETHOD(device_attach,	ulpt_attach),
	DEVMETHOD(device_detach,	ulpt_detach),

	{ 0, 0 }
};

static driver_t ulpt_driver = {
	"ulpt",
	ulpt_methods,
	sizeof(struct ulpt_softc)
};

static devclass_t ulpt_devclass;

MODULE_DEPEND(umass, usb, 1, 1, 1);
DRIVER_MODULE(ulpt, uhub, ulpt_driver, ulpt_devclass, usbd_driver_load, 0);

static int
ulpt_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usb_interface_descriptor_t *id;

	DPRINTFN(10,("ulpt_match\n"));
	if (uaa->iface == NULL)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id != NULL &&
	    id->bInterfaceClass == UICLASS_PRINTER &&
	    id->bInterfaceSubClass == UISUBCLASS_PRINTER &&
	    (id->bInterfaceProtocol == UIPROTO_PRINTER_UNI ||
	     id->bInterfaceProtocol == UIPROTO_PRINTER_BI ||
	     id->bInterfaceProtocol == UIPROTO_PRINTER_1284))
		return (UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO);
	return (UMATCH_NONE);
}

static int
ulpt_attach(device_t self)
{
	struct ulpt_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *ifcd = usbd_get_interface_descriptor(iface);
	usb_interface_descriptor_t *id, *iend;
	usb_config_descriptor_t *cdesc;
	usbd_status err;
	usb_endpoint_descriptor_t *ed;
	u_int8_t epcount;
	int i, altno;

	DPRINTFN(10,("ulpt_attach: sc=%p\n", sc));
	sc->sc_dev = self;

	/* XXX
	 * Stepping through the alternate settings needs to be abstracted out.
	 */
	cdesc = usbd_get_config_descriptor(dev);
	if (cdesc == NULL) {
		printf("%s: failed to get configuration descriptor\n",
		       device_get_nameunit(sc->sc_dev));
		return ENXIO;
	}
	iend = (usb_interface_descriptor_t *)
		   ((char *)cdesc + UGETW(cdesc->wTotalLength));
#ifdef DIAGNOSTIC
	if (ifcd < (usb_interface_descriptor_t *)cdesc ||
	    ifcd >= iend)
		panic("ulpt: iface desc out of range");
#endif
	/* Step through all the descriptors looking for bidir mode */
	for (id = ifcd, altno = 0;
	     id < iend;
	     id = (void *)((char *)id + id->bLength)) {
		if (id->bDescriptorType == UDESC_INTERFACE &&
		    id->bInterfaceNumber == ifcd->bInterfaceNumber) {
			if (id->bInterfaceClass == UICLASS_PRINTER &&
			    id->bInterfaceSubClass == UISUBCLASS_PRINTER &&
			    (id->bInterfaceProtocol == UIPROTO_PRINTER_BI /* ||
			     id->bInterfaceProtocol == UIPROTO_PRINTER_1284 */))
				goto found;
			altno++;
		}
	}
	id = ifcd;		/* not found, use original */
 found:
	if (id != ifcd) {
		/* Found a new bidir setting */
		DPRINTF(("ulpt_attach: set altno = %d\n", altno));
		err = usbd_set_interface(iface, altno);
		if (err) {
			printf("%s: setting alternate interface failed\n",
			       device_get_nameunit(sc->sc_dev));
			sc->sc_dying = 1;
			return ENXIO;
		}
	}

	epcount = 0;
	(void)usbd_endpoint_count(iface, &epcount);

	sc->sc_in = -1;
	sc->sc_out = -1;
	for (i = 0; i < epcount; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("%s: couldn't get ep %d\n",
			    device_get_nameunit(sc->sc_dev), i);
			return ENXIO;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_in = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_out = ed->bEndpointAddress;
		}
	}
	if (sc->sc_out == -1) {
		printf("%s: could not find bulk out endpoint\n",
		    device_get_nameunit(sc->sc_dev));
		sc->sc_dying = 1;
		return ENXIO;
	}

	if (usbd_get_quirks(dev)->uq_flags & UQ_BROKEN_BIDIR) {
		/* This device doesn't handle reading properly. */
		sc->sc_in = -1;
	}

	printf("%s: using %s-directional mode\n", device_get_nameunit(sc->sc_dev),
	       sc->sc_in >= 0 ? "bi" : "uni");

	DPRINTFN(10, ("ulpt_attach: bulk=%d\n", sc->sc_out));

	sc->sc_iface = iface;
	sc->sc_ifaceno = id->bInterfaceNumber;
	sc->sc_udev = dev;

#if 0
/*
 * This code is disabled because for some mysterious reason it causes
 * printing not to work.  But only sometimes, and mostly with
 * UHCI and less often with OHCI.  *sigh*
 */
	{
	usb_config_descriptor_t *cd = usbd_get_config_descriptor(dev);
	usb_device_request_t req;
	int len, alen;

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_DEVICE_ID;
	USETW(req.wValue, cd->bConfigurationValue);
	USETW2(req.wIndex, id->bInterfaceNumber, id->bAlternateSetting);
	USETW(req.wLength, sizeof devinfo - 1);
	err = usbd_do_request_flags(dev, &req, devinfo, USBD_SHORT_XFER_OK,
		  &alen, USBD_DEFAULT_TIMEOUT);
	if (err) {
		printf("%s: cannot get device id\n", device_get_nameunit(sc->sc_dev));
	} else if (alen <= 2) {
		printf("%s: empty device id, no printer connected?\n",
		       device_get_nameunit(sc->sc_dev));
	} else {
		/* devinfo now contains an IEEE-1284 device ID */
		len = ((devinfo[0] & 0xff) << 8) | (devinfo[1] & 0xff);
		if (len > sizeof devinfo - 3)
			len = sizeof devinfo - 3;
		devinfo[len] = 0;
		printf("%s: device id <", device_get_nameunit(sc->sc_dev));
		ieee1284_print_id(devinfo+2);
		printf(">\n");
	}
	}
#endif

	sc->dev = make_dev(&ulpt_cdevsw, device_get_unit(self),
		UID_ROOT, GID_OPERATOR, 0644, "ulpt%d", device_get_unit(self));
	sc->dev_noprime = make_dev(&ulpt_cdevsw,
		device_get_unit(self)|ULPT_NOPRIME,
		UID_ROOT, GID_OPERATOR, 0644, "unlpt%d", device_get_unit(self));

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);

	return 0;
}


static int
ulpt_detach(device_t self)
{
	struct ulpt_softc *sc = device_get_softc(self);
	int s;

	DPRINTF(("ulpt_detach: sc=%p\n", sc));

	sc->sc_dying = 1;
	if (sc->sc_out_pipe != NULL)
		usbd_abort_pipe(sc->sc_out_pipe);
	if (sc->sc_in_pipe != NULL)
		usbd_abort_pipe(sc->sc_in_pipe);

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* There is noone to wake, aborting the pipe is enough */
		/* Wait for processes to go away. */
		usb_detach_wait(sc->sc_dev);
	}
	splx(s);

	destroy_dev(sc->dev);
	destroy_dev(sc->dev_noprime);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);

	return (0);
}

int
ulpt_status(struct ulpt_softc *sc)
{
	usb_device_request_t req;
	usbd_status err;
	u_char status;

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_PORT_STATUS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, 1);
	err = usbd_do_request(sc->sc_udev, &req, &status);
	DPRINTFN(1, ("ulpt_status: status=0x%02x err=%d\n", status, err));
	if (!err)
		return (status);
	else
		return (0);
}

void
ulpt_reset(struct ulpt_softc *sc)
{
	usb_device_request_t req;

	DPRINTFN(1, ("ulpt_reset\n"));
	req.bRequest = UR_SOFT_RESET;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, 0);

	/*
	 * There was a mistake in the USB printer 1.0 spec that gave the
	 * request type as UT_WRITE_CLASS_OTHER; it should have been
	 * UT_WRITE_CLASS_INTERFACE.  Many printers use the old one,
	 * so we try both.
	 */
	req.bmRequestType = UT_WRITE_CLASS_OTHER;
	if (usbd_do_request(sc->sc_udev, &req, 0)) {	/* 1.0 */
		req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
		(void)usbd_do_request(sc->sc_udev, &req, 0); /* 1.1 */
	}
}
#if 0
static void
ulpt_input(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ulpt_softc *sc = priv;
	u_int32_t count;

	/* Don't loop on errors or 0-length input. */
	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	if (status != USBD_NORMAL_COMPLETION || count == 0)
		return;

	DPRINTFN(2,("ulpt_input: got some data\n"));
	/* Do it again. */
	if (xfer == sc->sc_in_xfer1)
		usbd_transfer(sc->sc_in_xfer2);
	else
		usbd_transfer(sc->sc_in_xfer1);
}
#endif

int ulptusein = 1;

/*
 * Reset the printer, then wait until it's selected and not busy.
 */
int
ulptopen(struct cdev *dev, int flag, int mode, struct thread *p)
{
	u_char flags = ULPTFLAGS(dev);
	struct ulpt_softc *sc;
	usbd_status err;
	int error;

	sc = devclass_get_softc(ulpt_devclass, ULPTUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	if (sc == NULL || sc->sc_iface == NULL || sc->sc_dying)
		return (ENXIO);

	if (sc->sc_state)
		return (EBUSY);

	sc->sc_state = ULPT_INIT;
	sc->sc_flags = flags;
	DPRINTF(("ulptopen: flags=0x%x\n", (unsigned)flags));

#if defined(USB_DEBUG)
	/* Ignoring these flags might not be a good idea */
	if ((flags & ~ULPT_NOPRIME) != 0)
		printf("ulptopen: flags ignored: %b\n", flags,
			"\20\3POS_INIT\4POS_ACK\6PRIME_OPEN\7AUTOLF\10BYPASS");
#endif


	error = 0;
	sc->sc_refcnt++;

	if ((flags & ULPT_NOPRIME) == 0) {
		ulpt_reset(sc);
		if (sc->sc_dying) {
			error = ENXIO;
			sc->sc_state = 0;
			goto done;
		}
	}

	err = usbd_open_pipe(sc->sc_iface, sc->sc_out, 0, &sc->sc_out_pipe);
	if (err) {
		error = EIO;
		goto err0;
	}
	sc->sc_out_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_out_xfer == NULL) {
		error = ENOMEM;
		goto err1;
	}
	sc->sc_out_buf = usbd_alloc_buffer(sc->sc_out_xfer, ULPT_BSIZE);
	if (sc->sc_out_buf == NULL) {
		error = ENOMEM;
		goto err2;
	}

	if (ulptusein && sc->sc_in != -1) {
		DPRINTF(("ulpt_open: open input pipe\n"));
		err = usbd_open_pipe(sc->sc_iface, sc->sc_in,0,&sc->sc_in_pipe);
		if (err) {
			error = EIO;
			goto err2;
		}
		sc->sc_in_xfer = usbd_alloc_xfer(sc->sc_udev);
		if (sc->sc_in_xfer == NULL) {
			error = ENOMEM;
			goto err3;
		}
		sc->sc_in_buf = usbd_alloc_buffer(sc->sc_in_xfer, ULPT_BSIZE);
		if (sc->sc_in_buf == NULL) {
			error = ENOMEM;
			goto err4;
		}

		/* If it's not opened for read the set up a reader. */
		if (!(flag & FREAD)) {
			DPRINTF(("ulpt_open: start read callout\n"));
			callout_init(&sc->sc_read_callout, 0);
			callout_reset(&sc->sc_read_callout, hz/5, ulpt_tick,
			    sc);
			sc->sc_has_callout = 1;
		}
	}

	sc->sc_state = ULPT_OPEN;
	goto done;

 err4:
	usbd_free_xfer(sc->sc_in_xfer);
	sc->sc_in_xfer = NULL;
 err3:
	usbd_close_pipe(sc->sc_in_pipe);
	sc->sc_in_pipe = NULL;
 err2:
	usbd_free_xfer(sc->sc_out_xfer);
	sc->sc_out_xfer = NULL;
 err1:
	usbd_close_pipe(sc->sc_out_pipe);
	sc->sc_out_pipe = NULL;
 err0:
	sc->sc_state = 0;

 done:
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);

	DPRINTF(("ulptopen: done, error=%d\n", error));
	return (error);
}

int
ulpt_statusmsg(u_char status, struct ulpt_softc *sc)
{
	u_char new;

	status = (status ^ LPS_INVERT) & LPS_MASK;
	new = status & ~sc->sc_laststatus;
	sc->sc_laststatus = status;

	if (new & LPS_SELECT)
		log(LOG_NOTICE, "%s: offline\n", device_get_nameunit(sc->sc_dev));
	else if (new & LPS_NOPAPER)
		log(LOG_NOTICE, "%s: out of paper\n", device_get_nameunit(sc->sc_dev));
	else if (new & LPS_NERR)
		log(LOG_NOTICE, "%s: output error\n", device_get_nameunit(sc->sc_dev));

	return (status);
}

int
ulptclose(struct cdev *dev, int flag, int mode, struct thread *p)
{
	struct ulpt_softc *sc;

	sc = devclass_get_softc(ulpt_devclass, ULPTUNIT(dev));

	if (sc->sc_state != ULPT_OPEN)
		/* We are being forced to close before the open completed. */
		return (0);

	if (sc->sc_has_callout) {
		callout_stop(&sc->sc_read_callout);
		sc->sc_has_callout = 0;
	}

	if (sc->sc_out_pipe != NULL) {
		usbd_abort_pipe(sc->sc_out_pipe);
		usbd_close_pipe(sc->sc_out_pipe);
		sc->sc_out_pipe = NULL;
	}
	if (sc->sc_out_xfer != NULL) {
		usbd_free_xfer(sc->sc_out_xfer);
		sc->sc_out_xfer = NULL;
	}

	if (sc->sc_in_pipe != NULL) {
		usbd_abort_pipe(sc->sc_in_pipe);
		usbd_close_pipe(sc->sc_in_pipe);
		sc->sc_in_pipe = NULL;
	}
	if (sc->sc_in_xfer != NULL) {
		usbd_free_xfer(sc->sc_in_xfer);
		sc->sc_in_xfer = NULL;
	}

	sc->sc_state = 0;

	DPRINTF(("ulptclose: closed\n"));
	return (0);
}

int
ulpt_do_write(struct ulpt_softc *sc, struct uio *uio, int flags)
{
	u_int32_t n;
	int error = 0;
	void *bufp;
	usbd_xfer_handle xfer;
	usbd_status err;

	DPRINTF(("ulptwrite\n"));
	xfer = sc->sc_out_xfer;
	bufp = sc->sc_out_buf;
	while ((n = min(ULPT_BSIZE, uio->uio_resid)) != 0) {
		ulpt_statusmsg(ulpt_status(sc), sc);
		error = uiomove(bufp, n, uio);
		if (error)
			break;
		DPRINTFN(1, ("ulptwrite: transfer %d bytes\n", n));
		err = usbd_bulk_transfer(xfer, sc->sc_out_pipe, USBD_NO_COPY,
			  USBD_NO_TIMEOUT, bufp, &n, "ulptwr");
		if (err) {
			DPRINTF(("ulptwrite: error=%d\n", err));
			error = EIO;
			break;
		}
	}

	return (error);
}

int
ulptwrite(struct cdev *dev, struct uio *uio, int flags)
{
	struct ulpt_softc *sc;
	int error;

	sc = devclass_get_softc(ulpt_devclass, ULPTUNIT(dev));

	if (sc->sc_dying)
		return (EIO);

	sc->sc_refcnt++;
	error = ulpt_do_write(sc, uio, flags);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
	return (error);
}

int
ulpt_do_read(struct ulpt_softc *sc, struct uio *uio, int flags)
{
	u_int32_t n, on;
	int error = 0;
	void *bufp;
	usbd_xfer_handle xfer;
	usbd_status err;

	DPRINTF(("ulptread\n"));

	if (sc->sc_in_pipe == NULL)
		return 0;

	xfer = sc->sc_in_xfer;
	bufp = sc->sc_in_buf;
	while ((n = min(ULPT_BSIZE, uio->uio_resid)) != 0) {
		DPRINTFN(1, ("ulptread: transfer %d bytes\n", n));
		on = n;
		err = usbd_bulk_transfer(xfer, sc->sc_in_pipe,
			  USBD_NO_COPY | USBD_SHORT_XFER_OK,
			  USBD_NO_TIMEOUT, bufp, &n, "ulptrd");
		if (err) {
			DPRINTF(("ulptread: error=%d\n", err));
			error = EIO;
			break;
		}
		error = uiomove(bufp, n, uio);
		if (error)
			break;
		if (on != n)
			break;
	}

	return (error);
}

int
ulptread(struct cdev *dev, struct uio *uio, int flags)
{
	struct ulpt_softc *sc;
	int error;

	sc = devclass_get_softc(ulpt_devclass, ULPTUNIT(dev));

	if (sc->sc_dying)
		return (EIO);

	sc->sc_refcnt++;
	error = ulpt_do_read(sc, uio, flags);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
	return (error);
}

void
ulpt_read_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
	     usbd_status status)
{
	usbd_status err;
	u_int32_t n;
	usbd_private_handle xsc;
	struct ulpt_softc *sc;

	usbd_get_xfer_status(xfer, &xsc, NULL, &n, &err);
	sc = xsc;

	DPRINTFN(1,("ulpt_read_cb: start sc=%p, err=%d n=%d\n", sc, err, n));

#ifdef ULPT_DEBUG
	if (!err && n > 0)
		DPRINTF(("ulpt_tick: discarding %d bytes\n", n));
#endif
	if (!err || err == USBD_TIMEOUT)
		callout_reset(&sc->sc_read_callout, hz / ULPT_READS_PER_SEC,
		    ulpt_tick, sc);
}

void
ulpt_tick(void *xsc)
{
	struct ulpt_softc *sc = xsc;
	usbd_status err;

	if (sc == NULL || sc->sc_dying)
		return;

	DPRINTFN(1,("ulpt_tick: start sc=%p\n", sc));

	usbd_setup_xfer(sc->sc_in_xfer, sc->sc_in_pipe, sc, sc->sc_in_buf,
			ULPT_BSIZE, USBD_NO_COPY | USBD_SHORT_XFER_OK,
			ULPT_READ_TIMO, ulpt_read_cb);
	err = usbd_transfer(sc->sc_in_xfer);
	DPRINTFN(1,("ulpt_tick: err=%d\n", err));
}

int
ulptioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *p)
{
	int error = 0;

	switch (cmd) {
	default:
		error = ENODEV;
	}

	return (error);
}

#if 0
/* XXX This does not belong here. */
/*
 * Print select parts of an IEEE 1284 device ID.
 */
void
ieee1284_print_id(char *str)
{
	char *p, *q;

	for (p = str-1; p; p = strchr(p, ';')) {
		p++;		/* skip ';' */
		if (strncmp(p, "MFG:", 4) == 0 ||
		    strncmp(p, "MANUFACTURER:", 14) == 0 ||
		    strncmp(p, "MDL:", 4) == 0 ||
		    strncmp(p, "MODEL:", 6) == 0) {
			q = strchr(p, ';');
			if (q)
				printf("%.*s", (int)(q - p + 1), p);
		}
	}
}
#endif
