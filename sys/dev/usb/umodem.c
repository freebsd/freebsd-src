/*	$NetBSD: umodem.c,v 1.45 2002/09/23 05:51:23 simonb Exp $	*/


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*-
 * Copyright (c) 2003, M. Warner Losh <imp@freebsd.org>.
 * All rights reserved.
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

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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
 * Comm Class spec:  http://www.usb.org/developers/devclass_docs/usbccs10.pdf
 *                   http://www.usb.org/developers/devclass_docs/usbcdc11.pdf
 */

/*
 * TODO:
 * - Add error recovery in various places; the big problem is what
 *   to do in a callback if there is an error.
 * - Implement a Call Device for modems without multiplexed commands.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/ioccom.h>
#include <sys/conf.h>
#include <sys/serial.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <sys/taskqueue.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ucomvar.h>

#include "usbdevs.h"

#ifdef USB_DEBUG
int	umodemdebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, umodem, CTLFLAG_RW, 0, "USB umodem");
SYSCTL_INT(_hw_usb_umodem, OID_AUTO, debug, CTLFLAG_RW,
	   &umodemdebug, 0, "umodem debug level");
#define DPRINTFN(n, x)	if (umodemdebug > (n)) printf x
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

static const struct umodem_product {
	u_int16_t	vendor;
	u_int16_t	product;
	u_int8_t	interface;
} umodem_products[] = {
	/* Kyocera AH-K3001V*/
	{ USB_VENDOR_KYOCERA, USB_PRODUCT_KYOCERA_AHK3001V, 0 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5720, 0 },
	{ USB_VENDOR_CURITEL, USB_PRODUCT_CURITEL_PC5740, 0 },
	{ 0, 0, 0 },
};

/*
 * These are the maximum number of bytes transferred per frame.
 * As speeds for umodem deivces increase, these numbers will need to
 * be increased.  They should be good for G3 speeds and below.
 */
#define UMODEMIBUFSIZE 1024
#define UMODEMOBUFSIZE 1024

#define UMODEM_MODVER			1	/* module version */

struct umodem_softc {
	struct ucom_softc	sc_ucom;

	device_t		sc_dev;		/* base device */

	usbd_device_handle	sc_udev;	/* USB device */

	int			sc_ctl_iface_no;
	usbd_interface_handle	sc_ctl_iface;	/* control interface */
	int			sc_data_iface_no;
	usbd_interface_handle	sc_data_iface;	/* data interface */

	int			sc_cm_cap;	/* CM capabilities */
	int			sc_acm_cap;	/* ACM capabilities */

	int			sc_cm_over_data;

	usb_cdc_line_state_t	sc_line_state;	/* current line state */
	u_char			sc_dtr;		/* current DTR state */
	u_char			sc_rts;		/* current RTS state */

	u_char			sc_opening;	/* lock during open */

	int			sc_ctl_notify;	/* Notification endpoint */
	usbd_pipe_handle	sc_notify_pipe; /* Notification pipe */
	usb_cdc_notification_t	sc_notify_buf;	/* Notification structure */
	u_char			sc_lsr;		/* Local status register */
	u_char			sc_msr;		/* Modem status register */

	struct task		sc_task;
};

static void	*umodem_get_desc(usbd_device_handle dev, int type, int subtype);
static usbd_status umodem_set_comm_feature(struct umodem_softc *sc,
					   int feature, int state);
static usbd_status umodem_set_line_coding(struct umodem_softc *sc,
					  usb_cdc_line_state_t *state);

static void	umodem_get_caps(usbd_device_handle, int *, int *);

static void	umodem_get_status(void *, int portno, u_char *lsr, u_char *msr);
static void	umodem_set(void *, int, int, int);
static void	umodem_dtr(struct umodem_softc *, int);
static void	umodem_rts(struct umodem_softc *, int);
static void	umodem_break(struct umodem_softc *, int);
static void	umodem_set_line_state(struct umodem_softc *);
static int	umodem_param(void *, int, struct termios *);
static int	umodem_ioctl(void *, int, u_long, caddr_t, struct thread *);
static int	umodem_open(void *, int portno);
static void	umodem_close(void *, int portno);
static void	umodem_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void	umodem_notify(void *, int);

static struct ucom_callback umodem_callback = {
	umodem_get_status,
	umodem_set,
	umodem_param,
	umodem_ioctl,
	umodem_open,
	umodem_close,
	NULL,
	NULL,
};

static device_probe_t umodem_match;
static device_attach_t umodem_attach;
static device_detach_t umodem_detach;

static device_method_t umodem_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, umodem_match),
	DEVMETHOD(device_attach, umodem_attach),
	DEVMETHOD(device_detach, umodem_detach),
	{ 0, 0 }
};

static driver_t umodem_driver = {
	"ucom",
	umodem_methods,
	sizeof (struct umodem_softc)
};

DRIVER_MODULE(umodem, uhub, umodem_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(umodem, usb, 1, 1, 1);
MODULE_DEPEND(umodem, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);
MODULE_VERSION(umodem, UMODEM_MODVER);

static int
umodem_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usb_interface_descriptor_t *id;
	usb_device_descriptor_t *dd;
	int cm, acm, i, ret;

	if (uaa->iface == NULL)
		return (UMATCH_NONE);

	id = usbd_get_interface_descriptor(uaa->iface);
	dd = usbd_get_device_descriptor(uaa->device);
	if (id == NULL || dd == NULL)
		return (UMATCH_NONE);

	ret = UMATCH_NONE;
	for (i = 0; umodem_products[i].vendor != 0; i++) {
		if (umodem_products[i].vendor == UGETW(dd->idVendor) &&
		    umodem_products[i].product == UGETW(dd->idProduct) &&
		    umodem_products[i].interface == id->bInterfaceNumber) {
			ret = UMATCH_VENDOR_PRODUCT;
			break;
		}
	}

	if (ret == UMATCH_NONE &&
	    id->bInterfaceClass == UICLASS_CDC &&
	    id->bInterfaceSubClass == UISUBCLASS_ABSTRACT_CONTROL_MODEL &&
	    id->bInterfaceProtocol == UIPROTO_CDC_AT)
		ret = UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO;

	if (ret == UMATCH_NONE)
		return (ret);

	umodem_get_caps(uaa->device, &cm, &acm);
	if (!(cm & USB_CDC_CM_DOES_CM) ||
	    !(cm & USB_CDC_CM_OVER_DATA) ||
	    !(acm & USB_CDC_ACM_HAS_LINE))
		return (UMATCH_NONE);

	return ret;
}

static int
umodem_attach(device_t self)
{
	struct umodem_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_device_handle dev = uaa->device;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usb_cdc_cm_descriptor_t *cmd;
	int data_ifcno;
	int i;
	struct ucom_softc *ucom;

	ucom = &sc->sc_ucom;
	ucom->sc_dev = self;
	sc->sc_dev = self;
	ucom->sc_udev = dev;
	ucom->sc_iface = uaa->iface;

	sc->sc_udev = dev;
	sc->sc_ctl_iface = uaa->iface;
	id = usbd_get_interface_descriptor(sc->sc_ctl_iface);
	sc->sc_ctl_iface_no = id->bInterfaceNumber;
	device_printf(self, "iclass %d/%d\n", id->bInterfaceClass,
	  id->bInterfaceSubClass);

	umodem_get_caps(dev, &sc->sc_cm_cap, &sc->sc_acm_cap);

	/* Get the data interface no. */
	cmd = umodem_get_desc(dev, UDESC_CS_INTERFACE, UDESCSUB_CDC_CM);
	if (cmd == NULL) {
		device_printf(sc->sc_dev, "no CM descriptor\n");
		goto bad;
	}
	sc->sc_data_iface_no = data_ifcno = cmd->bDataInterface;

	device_printf(sc->sc_dev,
	    "data interface %d, has %sCM over data, has %sbreak\n",
	    data_ifcno, sc->sc_cm_cap & USB_CDC_CM_OVER_DATA ? "" : "no ",
	    sc->sc_acm_cap & USB_CDC_ACM_HAS_BREAK ? "" : "no ");

	/* Get the data interface too. */
	for (i = 0; i < uaa->nifaces; i++) {
		if (uaa->ifaces[i] != NULL) {
			id = usbd_get_interface_descriptor(uaa->ifaces[i]);
			if (id != NULL && id->bInterfaceNumber == data_ifcno) {
				sc->sc_data_iface = uaa->ifaces[i];
				uaa->ifaces[i] = NULL;
			}
		}
	}
	if (sc->sc_data_iface == NULL) {
		device_printf(sc->sc_dev, "no data interface\n");
		goto bad;
	}
	ucom->sc_iface = sc->sc_data_iface;

	/*
	 * Find the bulk endpoints.
	 * Iterate over all endpoints in the data interface and take note.
	 */
	ucom->sc_bulkin_no = ucom->sc_bulkout_no = -1;

	id = usbd_get_interface_descriptor(sc->sc_data_iface);
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_data_iface, i);
		if (ed == NULL) {
			device_printf(sc->sc_dev,
			    "no endpoint descriptor for %d\n", i);
			goto bad;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			ucom->sc_bulkin_no = ed->bEndpointAddress;
                } else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			ucom->sc_bulkout_no = ed->bEndpointAddress;
                }
        }

	if (ucom->sc_bulkin_no == -1) {
		device_printf(sc->sc_dev, "Could not find data bulk in\n");
		goto bad;
	}
	if (ucom->sc_bulkout_no == -1) {
		device_printf(sc->sc_dev, "Could not find data bulk out\n");
		goto bad;
	}

	if (sc->sc_cm_cap & USB_CDC_CM_OVER_DATA) {
		if (sc->sc_acm_cap & USB_CDC_ACM_HAS_FEATURE)
			umodem_set_comm_feature(sc, UCDC_ABSTRACT_STATE,
			    UCDC_DATA_MULTIPLEXED);
		sc->sc_cm_over_data = 1;
	}

	/*
	 * The standard allows for notification messages (to indicate things
	 * like a modem hangup) to come in via an interrupt endpoint
	 * off of the control interface.  Iterate over the endpoints on
	 * the control interface and see if there are any interrupt
	 * endpoints; if there are, then register it.
	 */

	sc->sc_ctl_notify = -1;
	sc->sc_notify_pipe = NULL;

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_ctl_iface, i);
		if (ed == NULL)
			continue;

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT) {
			device_printf(sc->sc_dev, 
			    "status change notification available\n");
			sc->sc_ctl_notify = ed->bEndpointAddress;
		}
	}

	sc->sc_dtr = -1;

	ucom->sc_parent = sc;
	ucom->sc_portno = UCOM_UNK_PORTNO;
	/* bulkin, bulkout set above */
	ucom->sc_ibufsize = UMODEMIBUFSIZE;
	ucom->sc_obufsize = UMODEMOBUFSIZE;
	ucom->sc_ibufsizepad = UMODEMIBUFSIZE;
	ucom->sc_opkthdrlen = 0;
	ucom->sc_callback = &umodem_callback;

	TASK_INIT(&sc->sc_task, 0, umodem_notify, sc);
	ucom_attach(&sc->sc_ucom);
	return 0;

 bad:
	ucom->sc_dying = 1;
	return ENXIO;
}

static int
umodem_open(void *addr, int portno)
{
	struct umodem_softc *sc = addr;
	int err;

	DPRINTF(("umodem_open: sc=%p\n", sc));

	if (sc->sc_ctl_notify != -1 && sc->sc_notify_pipe == NULL) {
		err = usbd_open_pipe_intr(sc->sc_ctl_iface, sc->sc_ctl_notify,
		    USBD_SHORT_XFER_OK, &sc->sc_notify_pipe, sc,
		    &sc->sc_notify_buf, sizeof(sc->sc_notify_buf),
		    umodem_intr, USBD_DEFAULT_INTERVAL);

		if (err) {
			DPRINTF(("Failed to establish notify pipe: %s\n",
				usbd_errstr(err)));
			return EIO;
		}
	}

	return 0;
}

static void
umodem_close(void *addr, int portno)
{
	struct umodem_softc *sc = addr;
	int err;

	DPRINTF(("umodem_close: sc=%p\n", sc));

	if (sc->sc_notify_pipe != NULL) {
		err = usbd_abort_pipe(sc->sc_notify_pipe);
		if (err)
			device_printf(sc->sc_dev, 
			    "abort notify pipe failed: %s\n",
			    usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_notify_pipe);
		if (err)
			device_printf(sc->sc_dev,
			    "close notify pipe failed: %s\n",
			    usbd_errstr(err));
		sc->sc_notify_pipe = NULL;
	}
}

static void
umodem_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct umodem_softc *sc = priv;
	u_char mstatus;

	if (sc->sc_ucom.sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		device_printf(sc->sc_dev, "abnormal status: %s\n",
		    usbd_errstr(status));
		return;
	}

	if (sc->sc_notify_buf.bmRequestType != UCDC_NOTIFICATION) {
		DPRINTF(("%s: unknown message type (%02x) on notify pipe\n",
			 device_get_nameunit(sc->sc_dev),
			 sc->sc_notify_buf.bmRequestType));
		return;
	}

	switch (sc->sc_notify_buf.bNotification) {
	case UCDC_N_SERIAL_STATE:
		/*
		 * Set the serial state in ucom driver based on
		 * the bits from the notify message
		 */
		if (UGETW(sc->sc_notify_buf.wLength) != 2) {
			device_printf(sc->sc_dev,
			    "Invalid notification length! (%d)\n",
			    UGETW(sc->sc_notify_buf.wLength));
			break;
		}
		DPRINTF(("%s: notify bytes = %02x%02x\n",
			 device_get_nameunit(sc->sc_dev),
			 sc->sc_notify_buf.data[0],
			 sc->sc_notify_buf.data[1]));
		/* Currently, lsr is always zero. */
		sc->sc_lsr = sc->sc_msr = 0;
		mstatus = sc->sc_notify_buf.data[0];

		if (ISSET(mstatus, UCDC_N_SERIAL_RI))
			sc->sc_msr |= SER_RI;
		if (ISSET(mstatus, UCDC_N_SERIAL_DSR))
			sc->sc_msr |= SER_DSR;
		if (ISSET(mstatus, UCDC_N_SERIAL_DCD))
			sc->sc_msr |= SER_DCD;
		/* Deferred notifying to the ucom layer */
		taskqueue_enqueue(taskqueue_swi_giant, &sc->sc_task);
		break;
	default:
		DPRINTF(("%s: unknown notify message: %02x\n",
			 device_get_nameunit(sc->sc_dev),
			 sc->sc_notify_buf.bNotification));
		break;
	}
}

static void
umodem_notify(void *arg, int count)
{
	struct umodem_softc *sc;

	sc = (struct umodem_softc *)arg;
	if (sc->sc_ucom.sc_dying)
		return;
	ucom_status_change(&sc->sc_ucom);
}

void
umodem_get_caps(usbd_device_handle dev, int *cm, int *acm)
{
	usb_cdc_cm_descriptor_t *cmd;
	usb_cdc_acm_descriptor_t *cad;

	*cm = *acm = 0;

	cmd = umodem_get_desc(dev, UDESC_CS_INTERFACE, UDESCSUB_CDC_CM);
	if (cmd == NULL) {
		DPRINTF(("umodem_get_desc: no CM desc\n"));
		return;
	}
	*cm = cmd->bmCapabilities;

	cad = umodem_get_desc(dev, UDESC_CS_INTERFACE, UDESCSUB_CDC_ACM);
	if (cad == NULL) {
		DPRINTF(("umodem_get_desc: no ACM desc\n"));
		return;
	}
	*acm = cad->bmCapabilities;
}

void
umodem_get_status(void *addr, int portno, u_char *lsr, u_char *msr)
{
	struct umodem_softc *sc = addr;

	DPRINTF(("umodem_get_status:\n"));

	if (lsr != NULL)
		*lsr = sc->sc_lsr;
	if (msr != NULL)
		*msr = sc->sc_msr;
}

int
umodem_param(void *addr, int portno, struct termios *t)
{
	struct umodem_softc *sc = addr;
	usbd_status err;
	usb_cdc_line_state_t ls;

	DPRINTF(("umodem_param: sc=%p\n", sc));

	USETDW(ls.dwDTERate, t->c_ospeed);
	if (ISSET(t->c_cflag, CSTOPB))
		ls.bCharFormat = UCDC_STOP_BIT_2;
	else
		ls.bCharFormat = UCDC_STOP_BIT_1;
	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			ls.bParityType = UCDC_PARITY_ODD;
		else
			ls.bParityType = UCDC_PARITY_EVEN;
	} else
		ls.bParityType = UCDC_PARITY_NONE;
	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		ls.bDataBits = 5;
		break;
	case CS6:
		ls.bDataBits = 6;
		break;
	case CS7:
		ls.bDataBits = 7;
		break;
	case CS8:
		ls.bDataBits = 8;
		break;
	}

	err = umodem_set_line_coding(sc, &ls);
	if (err) {
		DPRINTF(("umodem_param: err=%s\n", usbd_errstr(err)));
		return (ENOTTY);
	}
	return (0);
}

int
umodem_ioctl(void *addr, int portno, u_long cmd, caddr_t data,
	     struct thread *p)
{
	struct umodem_softc *sc = addr;
	int error = 0;

	if (sc->sc_ucom.sc_dying)
		return (EIO);

	DPRINTF(("umodemioctl: cmd=0x%08lx\n", cmd));

	switch (cmd) {
	case USB_GET_CM_OVER_DATA:
		*(int *)data = sc->sc_cm_over_data;
		break;

	case USB_SET_CM_OVER_DATA:
		if (*(int *)data != sc->sc_cm_over_data) {
			/* XXX change it */
		}
		break;

	default:
		DPRINTF(("umodemioctl: unknown\n"));
		error = ENOTTY;
		break;
	}

	return (error);
}

void
umodem_dtr(struct umodem_softc *sc, int onoff)
{
	DPRINTF(("umodem_modem: onoff=%d\n", onoff));

	if (sc->sc_dtr == onoff)
		return;
	sc->sc_dtr = onoff;

	umodem_set_line_state(sc);
}

void
umodem_rts(struct umodem_softc *sc, int onoff)
{
	DPRINTF(("umodem_modem: onoff=%d\n", onoff));

	if (sc->sc_rts == onoff)
		return;
	sc->sc_rts = onoff;

	umodem_set_line_state(sc);
}

void
umodem_set_line_state(struct umodem_softc *sc)
{
	usb_device_request_t req;
	int ls;

	ls = (sc->sc_dtr ? UCDC_LINE_DTR : 0) |
	     (sc->sc_rts ? UCDC_LINE_RTS : 0);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, ls);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, 0);

	(void)usbd_do_request(sc->sc_udev, &req, 0);

}

void
umodem_break(struct umodem_softc *sc, int onoff)
{
	usb_device_request_t req;

	DPRINTF(("umodem_break: onoff=%d\n", onoff));

	if (!(sc->sc_acm_cap & USB_CDC_ACM_HAS_BREAK))
		return;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SEND_BREAK;
	USETW(req.wValue, onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, 0);

	(void)usbd_do_request(sc->sc_udev, &req, 0);
}

void
umodem_set(void *addr, int portno, int reg, int onoff)
{
	struct umodem_softc *sc = addr;

	switch (reg) {
	case UCOM_SET_DTR:
		umodem_dtr(sc, onoff);
		break;
	case UCOM_SET_RTS:
		umodem_rts(sc, onoff);
		break;
	case UCOM_SET_BREAK:
		umodem_break(sc, onoff);
		break;
	default:
		break;
	}
}

usbd_status
umodem_set_line_coding(struct umodem_softc *sc, usb_cdc_line_state_t *state)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("umodem_set_line_coding: rate=%d fmt=%d parity=%d bits=%d\n",
		 UGETDW(state->dwDTERate), state->bCharFormat,
		 state->bParityType, state->bDataBits));

	if (memcmp(state, &sc->sc_line_state, UCDC_LINE_STATE_LENGTH) == 0) {
		DPRINTF(("umodem_set_line_coding: already set\n"));
		return (USBD_NORMAL_COMPLETION);
	}

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_LINE_CODING;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, UCDC_LINE_STATE_LENGTH);

	err = usbd_do_request(sc->sc_udev, &req, state);
	if (err) {
		DPRINTF(("umodem_set_line_coding: failed, err=%s\n",
			 usbd_errstr(err)));
		return (err);
	}

	sc->sc_line_state = *state;

	return (USBD_NORMAL_COMPLETION);
}

void *
umodem_get_desc(usbd_device_handle dev, int type, int subtype)
{
	usb_descriptor_t *desc;
	usb_config_descriptor_t *cd = usbd_get_config_descriptor(dev);
        uByte *p = (uByte *)cd;
        uByte *end = p + UGETW(cd->wTotalLength);

	while (p < end) {
		desc = (usb_descriptor_t *)p;
		if (desc->bDescriptorType == type &&
		    desc->bDescriptorSubtype == subtype)
			return (desc);
		p += desc->bLength;
	}

	return (0);
}

usbd_status
umodem_set_comm_feature(struct umodem_softc *sc, int feature, int state)
{
	usb_device_request_t req;
	usbd_status err;
	usb_cdc_abstract_state_t ast;

	DPRINTF(("umodem_set_comm_feature: feature=%d state=%d\n", feature,
		 state));

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_COMM_FEATURE;
	USETW(req.wValue, feature);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, UCDC_ABSTRACT_STATE_LENGTH);
	USETW(ast.wState, state);

	err = usbd_do_request(sc->sc_udev, &req, &ast);
	if (err) {
		DPRINTF(("umodem_set_comm_feature: feature=%d, err=%s\n",
			 feature, usbd_errstr(err)));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

static int
umodem_detach(device_t self)
{
	struct umodem_softc *sc = device_get_softc(self);
	int rv = 0;

	DPRINTF(("umodem_detach: sc=%p\n", sc));

	if (sc->sc_notify_pipe != NULL) {
		usbd_abort_pipe(sc->sc_notify_pipe);
		usbd_close_pipe(sc->sc_notify_pipe);
		sc->sc_notify_pipe = NULL;
	}

	sc->sc_ucom.sc_dying = 1;
	rv = ucom_detach(&sc->sc_ucom);

	return (rv);
}
