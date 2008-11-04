#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

/*
 * Printer Class spec: http://www.usb.org/developers/data/devclass/usbprint109.PDF
 * Printer Class spec: http://www.usb.org/developers/devclass_docs/usbprint11.pdf
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>

#define	USB_DEBUG_VAR ulpt_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_mbuf.h>
#include <dev/usb2/core/usb2_dev.h>
#include <dev/usb2/core/usb2_parse.h>

#include <sys/syslog.h>

#if USB_DEBUG
static int ulpt_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, ulpt, CTLFLAG_RW, 0, "USB ulpt");
SYSCTL_INT(_hw_usb2_ulpt, OID_AUTO, debug, CTLFLAG_RW,
    &ulpt_debug, 0, "Debug level");
#endif

#define	ULPT_BSIZE		(1<<15)	/* bytes */
#define	ULPT_IFQ_MAXLEN         2	/* units */
#define	ULPT_N_TRANSFER         5	/* units */

#define	UR_GET_DEVICE_ID        0x00
#define	UR_GET_PORT_STATUS      0x01
#define	UR_SOFT_RESET           0x02

#define	LPS_NERR		0x08	/* printer no error */
#define	LPS_SELECT		0x10	/* printer selected */
#define	LPS_NOPAPER		0x20	/* printer out of paper */
#define	LPS_INVERT      (LPS_SELECT|LPS_NERR)
#define	LPS_MASK        (LPS_SELECT|LPS_NERR|LPS_NOPAPER)

struct ulpt_softc {
	struct usb2_fifo_sc sc_fifo;
	struct usb2_fifo_sc sc_fifo_noreset;
	struct mtx sc_mtx;
	struct usb2_callout sc_watchdog;

	device_t sc_dev;
	struct usb2_device *sc_udev;
	struct usb2_fifo *sc_fifo_open[2];
	struct usb2_xfer *sc_xfer[ULPT_N_TRANSFER];

	int	sc_fflags;		/* current open flags, FREAD and
					 * FWRITE */

	uint8_t	sc_flags;
#define	ULPT_FLAG_READ_STALL    0x04	/* read transfer stalled */
#define	ULPT_FLAG_WRITE_STALL   0x08	/* write transfer stalled */

	uint8_t	sc_iface_no;
	uint8_t	sc_last_status;
	uint8_t	sc_zlps;		/* number of consequtive zero length
					 * packets received */
};

/* prototypes */

static device_probe_t ulpt_probe;
static device_attach_t ulpt_attach;
static device_detach_t ulpt_detach;

static usb2_callback_t ulpt_write_callback;
static usb2_callback_t ulpt_write_clear_stall_callback;
static usb2_callback_t ulpt_read_callback;
static usb2_callback_t ulpt_read_clear_stall_callback;
static usb2_callback_t ulpt_status_callback;

static void ulpt_reset(struct ulpt_softc *sc);
static void ulpt_watchdog(void *arg);

static usb2_fifo_close_t ulpt_close;
static usb2_fifo_cmd_t ulpt_start_read;
static usb2_fifo_cmd_t ulpt_start_write;
static usb2_fifo_cmd_t ulpt_stop_read;
static usb2_fifo_cmd_t ulpt_stop_write;
static usb2_fifo_ioctl_t ulpt_ioctl;
static usb2_fifo_open_t ulpt_open;
static usb2_fifo_open_t unlpt_open;

static struct usb2_fifo_methods ulpt_fifo_methods = {
	.f_close = &ulpt_close,
	.f_ioctl = &ulpt_ioctl,
	.f_open = &ulpt_open,
	.f_start_read = &ulpt_start_read,
	.f_start_write = &ulpt_start_write,
	.f_stop_read = &ulpt_stop_read,
	.f_stop_write = &ulpt_stop_write,
	.basename[0] = "ulpt",
};

static struct usb2_fifo_methods unlpt_fifo_methods = {
	.f_close = &ulpt_close,
	.f_ioctl = &ulpt_ioctl,
	.f_open = &unlpt_open,
	.f_start_read = &ulpt_start_read,
	.f_start_write = &ulpt_start_write,
	.f_stop_read = &ulpt_stop_read,
	.f_stop_write = &ulpt_stop_write,
	.basename[0] = "unlpt",
};

static void
ulpt_reset(struct ulpt_softc *sc)
{
	struct usb2_device_request req;

	DPRINTFN(2, "\n");

	req.bRequest = UR_SOFT_RESET;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_iface_no);
	USETW(req.wLength, 0);

	/*
	 * There was a mistake in the USB printer 1.0 spec that gave the
	 * request type as UT_WRITE_CLASS_OTHER; it should have been
	 * UT_WRITE_CLASS_INTERFACE.  Many printers use the old one,
	 * so we try both.
	 */

	mtx_lock(&sc->sc_mtx);
	req.bmRequestType = UT_WRITE_CLASS_OTHER;
	if (usb2_do_request_flags(sc->sc_udev, &sc->sc_mtx,
	    &req, NULL, 0, NULL, 2 * USB_MS_HZ)) {	/* 1.0 */
		req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
		if (usb2_do_request_flags(sc->sc_udev, &sc->sc_mtx,
		    &req, NULL, 0, NULL, 2 * USB_MS_HZ)) {	/* 1.1 */
			/* ignore error */
		}
	}
	mtx_unlock(&sc->sc_mtx);
	return;
}

static void
ulpt_write_callback(struct usb2_xfer *xfer)
{
	struct ulpt_softc *sc = xfer->priv_sc;
	struct usb2_fifo *f = sc->sc_fifo_open[USB_FIFO_TX];
	uint32_t actlen;

	if (f == NULL) {
		/* should not happen */
		DPRINTF("no FIFO\n");
		return;
	}
	DPRINTF("state=0x%x\n", USB_GET_STATE(xfer));

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
		if (sc->sc_flags & ULPT_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[3]);
			break;
		}
		if (usb2_fifo_get_data(f, xfer->frbuffers,
		    0, xfer->max_data_length, &actlen, 0)) {

			xfer->frlengths[0] = actlen;
			usb2_start_hardware(xfer);
		}
		break;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= ULPT_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		break;
	}
	return;
}

static void
ulpt_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ulpt_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~ULPT_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ulpt_read_callback(struct usb2_xfer *xfer)
{
	struct ulpt_softc *sc = xfer->priv_sc;
	struct usb2_fifo *f = sc->sc_fifo_open[USB_FIFO_RX];

	if (f == NULL) {
		/* should not happen */
		DPRINTF("no FIFO\n");
		return;
	}
	DPRINTF("state=0x%x\n", USB_GET_STATE(xfer));

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		if (xfer->actlen == 0) {

			if (sc->sc_zlps == 4) {
				/* enable BULK throttle */
				xfer->interval = 500;	/* ms */
			} else {
				sc->sc_zlps++;
			}
		} else {
			/* disable BULK throttle */

			xfer->interval = 0;
			sc->sc_zlps = 0;
		}

		usb2_fifo_put_data(f, xfer->frbuffers,
		    0, xfer->actlen, 1);

	case USB_ST_SETUP:
		if (sc->sc_flags & ULPT_FLAG_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer[4]);
			break;
		}
		if (usb2_fifo_put_bytes_max(f) != 0) {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		break;

	default:			/* Error */
		/* disable BULK throttle */
		xfer->interval = 0;
		sc->sc_zlps = 0;

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= ULPT_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[4]);
		}
		break;
	}
	return;
}

static void
ulpt_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ulpt_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~ULPT_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ulpt_status_callback(struct usb2_xfer *xfer)
{
	struct ulpt_softc *sc = xfer->priv_sc;
	struct usb2_device_request req;
	uint8_t cur_status;
	uint8_t new_status;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		usb2_copy_out(xfer->frbuffers + 1, 0, &cur_status, 1);

		cur_status = (cur_status ^ LPS_INVERT) & LPS_MASK;
		new_status = cur_status & ~sc->sc_last_status;
		sc->sc_last_status = cur_status;

		if (new_status & LPS_SELECT)
			log(LOG_NOTICE, "%s: offline\n",
			    device_get_nameunit(sc->sc_dev));
		else if (new_status & LPS_NOPAPER)
			log(LOG_NOTICE, "%s: out of paper\n",
			    device_get_nameunit(sc->sc_dev));
		else if (new_status & LPS_NERR)
			log(LOG_NOTICE, "%s: output error\n",
			    device_get_nameunit(sc->sc_dev));
		break;

	case USB_ST_SETUP:
		req.bmRequestType = UT_READ_CLASS_INTERFACE;
		req.bRequest = UR_GET_PORT_STATUS;
		USETW(req.wValue, 0);
		req.wIndex[0] = sc->sc_iface_no;
		req.wIndex[1] = 0;
		USETW(req.wLength, 1);

		usb2_copy_in(xfer->frbuffers, 0, &req, sizeof(req));

		xfer->frlengths[0] = sizeof(req);
		xfer->frlengths[1] = 1;
		xfer->nframes = 2;
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		DPRINTF("error=%s\n", usb2_errstr(xfer->error));
		if (xfer->error != USB_ERR_CANCELLED) {
			/* wait for next watchdog timeout */
		}
		break;
	}
	return;
}

static const struct usb2_config ulpt_config[ULPT_N_TRANSFER] = {
	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = ULPT_BSIZE,
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,.proxy_buffer = 1},
		.mh.callback = &ulpt_write_callback,
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = ULPT_BSIZE,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,.proxy_buffer = 1},
		.mh.callback = &ulpt_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request) + 1,
		.mh.callback = &ulpt_status_callback,
		.mh.timeout = 1000,	/* 1 second */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &ulpt_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[4] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &ulpt_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static void
ulpt_start_read(struct usb2_fifo *fifo)
{
	struct ulpt_softc *sc = fifo->priv_sc0;

	usb2_transfer_start(sc->sc_xfer[1]);
	return;
}

static void
ulpt_stop_read(struct usb2_fifo *fifo)
{
	struct ulpt_softc *sc = fifo->priv_sc0;

	usb2_transfer_stop(sc->sc_xfer[4]);
	usb2_transfer_stop(sc->sc_xfer[1]);
	return;
}

static void
ulpt_start_write(struct usb2_fifo *fifo)
{
	struct ulpt_softc *sc = fifo->priv_sc0;

	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
ulpt_stop_write(struct usb2_fifo *fifo)
{
	struct ulpt_softc *sc = fifo->priv_sc0;

	usb2_transfer_stop(sc->sc_xfer[3]);
	usb2_transfer_stop(sc->sc_xfer[0]);
	return;
}

static int
ulpt_open(struct usb2_fifo *fifo, int fflags, struct thread *td)
{
	struct ulpt_softc *sc = fifo->priv_sc0;

	/* we assume that open is a serial process */

	if (sc->sc_fflags == 0) {
		ulpt_reset(sc);
	}
	return (unlpt_open(fifo, fflags, td));
}

static int
unlpt_open(struct usb2_fifo *fifo, int fflags, struct thread *td)
{
	struct ulpt_softc *sc = fifo->priv_sc0;

	if (sc->sc_fflags & fflags) {
		return (EBUSY);
	}
	if (fflags & FREAD) {
		/* clear stall first */
		mtx_lock(&sc->sc_mtx);
		sc->sc_flags |= ULPT_FLAG_READ_STALL;
		mtx_unlock(&sc->sc_mtx);
		if (usb2_fifo_alloc_buffer(fifo,
		    sc->sc_xfer[1]->max_data_length,
		    ULPT_IFQ_MAXLEN)) {
			return (ENOMEM);
		}
		/* set which FIFO is opened */
		sc->sc_fifo_open[USB_FIFO_RX] = fifo;
	}
	if (fflags & FWRITE) {
		/* clear stall first */
		mtx_lock(&sc->sc_mtx);
		sc->sc_flags |= ULPT_FLAG_WRITE_STALL;
		mtx_unlock(&sc->sc_mtx);
		if (usb2_fifo_alloc_buffer(fifo,
		    sc->sc_xfer[0]->max_data_length,
		    ULPT_IFQ_MAXLEN)) {
			return (ENOMEM);
		}
		/* set which FIFO is opened */
		sc->sc_fifo_open[USB_FIFO_TX] = fifo;
	}
	sc->sc_fflags |= fflags & (FREAD | FWRITE);
	return (0);
}

static void
ulpt_close(struct usb2_fifo *fifo, int fflags, struct thread *td)
{
	struct ulpt_softc *sc = fifo->priv_sc0;

	sc->sc_fflags &= ~(fflags & (FREAD | FWRITE));

	if (fflags & (FREAD | FWRITE)) {
		usb2_fifo_free_buffer(fifo);
	}
	return;
}

static int
ulpt_ioctl(struct usb2_fifo *fifo, u_long cmd, void *data,
    int fflags, struct thread *td)
{
	return (ENODEV);
}

static int
ulpt_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	DPRINTFN(11, "\n");

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if ((uaa->info.bInterfaceClass == UICLASS_PRINTER) &&
	    (uaa->info.bInterfaceSubClass == UISUBCLASS_PRINTER) &&
	    ((uaa->info.bInterfaceProtocol == UIPROTO_PRINTER_UNI) ||
	    (uaa->info.bInterfaceProtocol == UIPROTO_PRINTER_BI) ||
	    (uaa->info.bInterfaceProtocol == UIPROTO_PRINTER_1284))) {
		return (0);
	}
	return (ENXIO);
}

static int
ulpt_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct ulpt_softc *sc = device_get_softc(dev);
	struct usb2_interface_descriptor *id;
	int unit = device_get_unit(dev);
	int error;
	uint8_t iface_index = uaa->info.bIfaceIndex;
	uint8_t alt_index;

	DPRINTFN(11, "sc=%p\n", sc);

	sc->sc_dev = dev;
	sc->sc_udev = uaa->device;

	device_set_usb2_desc(dev);

	mtx_init(&sc->sc_mtx, "ulpt lock", NULL, MTX_DEF | MTX_RECURSE);

	usb2_callout_init_mtx(&sc->sc_watchdog,
	    &sc->sc_mtx, CALLOUT_RETURNUNLOCKED);

	/* search through all the descriptors looking for bidir mode */

	id = usb2_get_interface_descriptor(uaa->iface);
	alt_index = 0 - 1;
	while (1) {
		if (id == NULL) {
			break;
		}
		if ((id->bDescriptorType == UDESC_INTERFACE) &&
		    (id->bLength >= sizeof(*id))) {
			if (id->bInterfaceNumber != uaa->info.bIfaceNum) {
				break;
			} else {
				alt_index++;
				if ((id->bInterfaceClass == UICLASS_PRINTER) &&
				    (id->bInterfaceSubClass == UISUBCLASS_PRINTER) &&
				    (id->bInterfaceProtocol == UIPROTO_PRINTER_BI)) {
					goto found;
				}
			}
		}
		id = (void *)usb2_desc_foreach(
		    usb2_get_config_descriptor(uaa->device), (void *)id);
	}
	goto detach;

found:

	DPRINTF("setting alternate "
	    "config number: %d\n", alt_index);

	if (alt_index) {

		error = usb2_set_alt_interface_index
		    (uaa->device, iface_index, alt_index);

		if (error) {
			DPRINTF("could not set alternate "
			    "config, error=%s\n", usb2_errstr(error));
			goto detach;
		}
	}
	sc->sc_iface_no = id->bInterfaceNumber;

	error = usb2_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, ulpt_config, ULPT_N_TRANSFER,
	    sc, &sc->sc_mtx);
	if (error) {
		DPRINTF("error=%s\n", usb2_errstr(error));
		goto detach;
	}
	device_printf(sc->sc_dev, "using bi-directional mode\n");

#if 0
/*
 * This code is disabled because for some mysterious reason it causes
 * printing not to work.  But only sometimes, and mostly with
 * UHCI and less often with OHCI.  *sigh*
 */
	{
		struct usb2_config_descriptor *cd = usb2_get_config_descriptor(dev);
		struct usb2_device_request req;
		int len, alen;

		req.bmRequestType = UT_READ_CLASS_INTERFACE;
		req.bRequest = UR_GET_DEVICE_ID;
		USETW(req.wValue, cd->bConfigurationValue);
		USETW2(req.wIndex, id->bInterfaceNumber, id->bAlternateSetting);
		USETW(req.wLength, sizeof devinfo - 1);
		error = usb2_do_request_flags(dev, &req, devinfo, USB_SHORT_XFER_OK,
		    &alen, USB_DEFAULT_TIMEOUT);
		if (error) {
			device_printf(sc->sc_dev, "cannot get device id\n");
		} else if (alen <= 2) {
			device_printf(sc->sc_dev, "empty device id, no "
			    "printer connected?\n");
		} else {
			/* devinfo now contains an IEEE-1284 device ID */
			len = ((devinfo[0] & 0xff) << 8) | (devinfo[1] & 0xff);
			if (len > sizeof devinfo - 3)
				len = sizeof devinfo - 3;
			devinfo[len] = 0;
			printf("%s: device id <", device_get_nameunit(sc->sc_dev));
			ieee1284_print_id(devinfo + 2);
			printf(">\n");
		}
	}
#endif

	/* set interface permissions */
	usb2_set_iface_perm(uaa->device, uaa->info.bIfaceIndex,
	    UID_ROOT, GID_OPERATOR, 0644);

	error = usb2_fifo_attach(uaa->device, sc, &sc->sc_mtx,
	    &ulpt_fifo_methods, &sc->sc_fifo,
	    unit, 0 - 1, uaa->info.bIfaceIndex);
	if (error) {
		goto detach;
	}
	error = usb2_fifo_attach(uaa->device, sc, &sc->sc_mtx,
	    &unlpt_fifo_methods, &sc->sc_fifo_noreset,
	    unit, 0 - 1, uaa->info.bIfaceIndex);
	if (error) {
		goto detach;
	}
	/* start reading of status */

	mtx_lock(&sc->sc_mtx);

	ulpt_watchdog(sc);		/* will unlock mutex */

	return (0);

detach:
	ulpt_detach(dev);
	return (ENOMEM);
}

static int
ulpt_detach(device_t dev)
{
	struct ulpt_softc *sc = device_get_softc(dev);

	DPRINTF("sc=%p\n", sc);

	usb2_fifo_detach(&sc->sc_fifo);
	usb2_fifo_detach(&sc->sc_fifo_noreset);

	mtx_lock(&sc->sc_mtx);
	usb2_callout_stop(&sc->sc_watchdog);
	mtx_unlock(&sc->sc_mtx);

	usb2_transfer_unsetup(sc->sc_xfer, ULPT_N_TRANSFER);

	usb2_callout_drain(&sc->sc_watchdog);

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

#if 0
/* XXX This does not belong here. */

/*
 * Compare two strings until the second ends.
 */

static uint8_t
ieee1284_compare(const char *a, const char *b)
{
	while (1) {

		if (*b == 0) {
			break;
		}
		if (*a != *b) {
			return 1;
		}
		b++;
		a++;
	}
	return 0;
}

/*
 * Print select parts of an IEEE 1284 device ID.
 */
void
ieee1284_print_id(char *str)
{
	char *p, *q;

	for (p = str - 1; p; p = strchr(p, ';')) {
		p++;			/* skip ';' */
		if (ieee1284_compare(p, "MFG:") == 0 ||
		    ieee1284_compare(p, "MANUFACTURER:") == 0 ||
		    ieee1284_compare(p, "MDL:") == 0 ||
		    ieee1284_compare(p, "MODEL:") == 0) {
			q = strchr(p, ';');
			if (q)
				printf("%.*s", (int)(q - p + 1), p);
		}
	}
}

#endif

static void
ulpt_watchdog(void *arg)
{
	struct ulpt_softc *sc = arg;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	usb2_transfer_start(sc->sc_xfer[2]);

	usb2_callout_reset(&sc->sc_watchdog,
	    hz, &ulpt_watchdog, sc);

	mtx_unlock(&sc->sc_mtx);
	return;
}

static devclass_t ulpt_devclass;

static device_method_t ulpt_methods[] = {
	DEVMETHOD(device_probe, ulpt_probe),
	DEVMETHOD(device_attach, ulpt_attach),
	DEVMETHOD(device_detach, ulpt_detach),
	{0, 0}
};

static driver_t ulpt_driver = {
	.name = "ulpt",
	.methods = ulpt_methods,
	.size = sizeof(struct ulpt_softc),
};

DRIVER_MODULE(ulpt, ushub, ulpt_driver, ulpt_devclass, NULL, 0);
MODULE_DEPEND(ulpt, usb2_core, 1, 1, 1);
MODULE_DEPEND(ulpt, usb2_serial, 1, 1, 1);
