/*	$NetBSD: uhid.c,v 1.46 2001/11/13 06:24:55 lukem Exp $	*/

/* Also already merged from NetBSD:
 *	$NetBSD: uhid.c,v 1.54 2002/09/23 05:51:21 simonb Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_hid.h>
#include <dev/usb2/include/usb2_ioctl.h>

#define	USB_DEBUG_VAR uhid_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_transfer.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_dynamic.h>
#include <dev/usb2/core/usb2_mbuf.h>
#include <dev/usb2/core/usb2_dev.h>
#include <dev/usb2/core/usb2_hid.h>

#include <dev/usb2/input/usb2_input.h>
#include <dev/usb2/input/usb2_rdesc.h>

#include <dev/usb2/quirk/usb2_quirk.h>

#if USB_DEBUG
static int uhid_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, uhid, CTLFLAG_RW, 0, "USB uhid");
SYSCTL_INT(_hw_usb2_uhid, OID_AUTO, debug, CTLFLAG_RW,
    &uhid_debug, 0, "Debug level");
#endif

#define	UHID_N_TRANSFER    4		/* units */
#define	UHID_BSIZE	1024		/* bytes, buffer size */
#define	UHID_FRAME_NUM 	  50		/* bytes, frame number */

struct uhid_softc {
	struct usb2_fifo_sc sc_fifo;
	struct mtx sc_mtx;

	struct usb2_xfer *sc_xfer[UHID_N_TRANSFER];
	struct usb2_device *sc_udev;
	void   *sc_repdesc_ptr;

	uint32_t sc_isize;
	uint32_t sc_osize;
	uint32_t sc_fsize;

	uint16_t sc_repdesc_size;

	uint8_t	sc_iface_no;
	uint8_t	sc_iface_index;
	uint8_t	sc_iid;
	uint8_t	sc_oid;
	uint8_t	sc_fid;
	uint8_t	sc_flags;
#define	UHID_FLAG_IMMED        0x01	/* set if read should be immediate */
#define	UHID_FLAG_INTR_STALL   0x02	/* set if interrupt transfer stalled */
#define	UHID_FLAG_STATIC_DESC  0x04	/* set if report descriptors are
					 * static */
};

static const uint8_t uhid_xb360gp_report_descr[] = {UHID_XB360GP_REPORT_DESCR()};
static const uint8_t uhid_graphire_report_descr[] = {UHID_GRAPHIRE_REPORT_DESCR()};
static const uint8_t uhid_graphire3_4x5_report_descr[] = {UHID_GRAPHIRE3_4X5_REPORT_DESCR()};

/* prototypes */

static device_probe_t uhid_probe;
static device_attach_t uhid_attach;
static device_detach_t uhid_detach;

static usb2_callback_t uhid_intr_callback;
static usb2_callback_t uhid_intr_clear_stall_callback;
static usb2_callback_t uhid_write_callback;
static usb2_callback_t uhid_read_callback;

static usb2_fifo_cmd_t uhid_start_read;
static usb2_fifo_cmd_t uhid_stop_read;
static usb2_fifo_cmd_t uhid_start_write;
static usb2_fifo_cmd_t uhid_stop_write;
static usb2_fifo_open_t uhid_open;
static usb2_fifo_close_t uhid_close;
static usb2_fifo_ioctl_t uhid_ioctl;

static struct usb2_fifo_methods uhid_fifo_methods = {
	.f_open = &uhid_open,
	.f_close = &uhid_close,
	.f_ioctl = &uhid_ioctl,
	.f_start_read = &uhid_start_read,
	.f_stop_read = &uhid_stop_read,
	.f_start_write = &uhid_start_write,
	.f_stop_write = &uhid_stop_write,
	.basename[0] = "uhid",
};

static void
uhid_intr_callback(struct usb2_xfer *xfer)
{
	struct uhid_softc *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTF("transferred!\n");

		if (xfer->actlen >= sc->sc_isize) {
			usb2_fifo_put_data(
			    sc->sc_fifo.fp[USB_FIFO_RX],
			    xfer->frbuffers,
			    0, sc->sc_isize, 1);
		} else {
			/* ignore it */
			DPRINTF("ignored short transfer, "
			    "%d bytes\n", xfer->actlen);
		}

	case USB_ST_SETUP:
		if (sc->sc_flags & UHID_FLAG_INTR_STALL) {
			usb2_transfer_start(sc->sc_xfer[1]);
		} else {
			if (usb2_fifo_put_bytes_max(
			    sc->sc_fifo.fp[USB_FIFO_RX]) != 0) {
				xfer->frlengths[0] = xfer->max_data_length;
				usb2_start_hardware(xfer);
			}
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= UHID_FLAG_INTR_STALL;
			usb2_transfer_start(sc->sc_xfer[1]);
		}
		return;
	}
}

static void
uhid_intr_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct uhid_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UHID_FLAG_INTR_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
uhid_fill_set_report(struct usb2_device_request *req, uint8_t iface_no,
    uint8_t type, uint8_t id, uint16_t size)
{
	req->bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req->bRequest = UR_SET_REPORT;
	USETW2(req->wValue, type, id);
	req->wIndex[0] = iface_no;
	req->wIndex[1] = 0;
	USETW(req->wLength, size);
	return;
}

static void
uhid_fill_get_report(struct usb2_device_request *req, uint8_t iface_no,
    uint8_t type, uint8_t id, uint16_t size)
{
	req->bmRequestType = UT_READ_CLASS_INTERFACE;
	req->bRequest = UR_GET_REPORT;
	USETW2(req->wValue, type, id);
	req->wIndex[0] = iface_no;
	req->wIndex[1] = 0;
	USETW(req->wLength, size);
	return;
}

static void
uhid_write_callback(struct usb2_xfer *xfer)
{
	struct uhid_softc *sc = xfer->priv_sc;
	struct usb2_device_request req;
	uint32_t size = sc->sc_osize;
	uint32_t actlen;
	uint8_t id;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
		/* try to extract the ID byte */
		if (sc->sc_oid) {

			if (usb2_fifo_get_data(
			    sc->sc_fifo.fp[USB_FIFO_TX],
			    xfer->frbuffers,
			    0, 1, &actlen, 0)) {
				if (actlen != 1) {
					goto tr_error;
				}
				usb2_copy_out(xfer->frbuffers, 0, &id, 1);

			} else {
				return;
			}
			if (size) {
				size--;
			}
		} else {
			id = 0;
		}

		if (usb2_fifo_get_data(
		    sc->sc_fifo.fp[USB_FIFO_TX],
		    xfer->frbuffers + 1,
		    0, UHID_BSIZE, &actlen, 1)) {
			if (actlen != size) {
				goto tr_error;
			}
			uhid_fill_set_report
			    (&req, sc->sc_iface_no,
			    UHID_OUTPUT_REPORT, id, size);

			usb2_copy_in(xfer->frbuffers, 0, &req, sizeof(req));

			xfer->frlengths[0] = sizeof(req);
			xfer->frlengths[1] = size;
			xfer->nframes = xfer->frlengths[1] ? 2 : 1;
			usb2_start_hardware(xfer);
		}
		return;

	default:
tr_error:
		/* bomb out */
		usb2_fifo_get_data_error(sc->sc_fifo.fp[USB_FIFO_TX]);
		return;
	}
}

static void
uhid_read_callback(struct usb2_xfer *xfer)
{
	struct uhid_softc *sc = xfer->priv_sc;
	struct usb2_device_request req;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usb2_fifo_put_data(sc->sc_fifo.fp[USB_FIFO_RX], xfer->frbuffers,
		    sizeof(req), sc->sc_isize, 1);
		return;

	case USB_ST_SETUP:

		if (usb2_fifo_put_bytes_max(sc->sc_fifo.fp[USB_FIFO_RX]) > 0) {

			uhid_fill_get_report
			    (&req, sc->sc_iface_no, UHID_INPUT_REPORT,
			    sc->sc_iid, sc->sc_isize);

			usb2_copy_in(xfer->frbuffers, 0, &req, sizeof(req));

			xfer->frlengths[0] = sizeof(req);
			xfer->frlengths[1] = sc->sc_isize;
			xfer->nframes = xfer->frlengths[1] ? 2 : 1;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		/* bomb out */
		usb2_fifo_put_data_error(sc->sc_fifo.fp[USB_FIFO_RX]);
		return;
	}
}

static const struct usb2_config uhid_config[UHID_N_TRANSFER] = {

	[0] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.bufsize = 0,	/* use wMaxPacketSize */
		.mh.callback = &uhid_intr_callback,
	},

	[1] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &uhid_intr_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request) + UHID_BSIZE,
		.mh.callback = &uhid_write_callback,
		.mh.timeout = 1000,	/* 1 second */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request) + UHID_BSIZE,
		.mh.callback = &uhid_read_callback,
		.mh.timeout = 1000,	/* 1 second */
	},
};

static void
uhid_start_read(struct usb2_fifo *fifo)
{
	struct uhid_softc *sc = fifo->priv_sc0;

	if (sc->sc_flags & UHID_FLAG_IMMED) {
		usb2_transfer_start(sc->sc_xfer[3]);
	} else {
		usb2_transfer_start(sc->sc_xfer[0]);
	}
	return;
}

static void
uhid_stop_read(struct usb2_fifo *fifo)
{
	struct uhid_softc *sc = fifo->priv_sc0;

	usb2_transfer_stop(sc->sc_xfer[3]);
	usb2_transfer_stop(sc->sc_xfer[0]);
	return;
}

static void
uhid_start_write(struct usb2_fifo *fifo)
{
	struct uhid_softc *sc = fifo->priv_sc0;

	usb2_transfer_start(sc->sc_xfer[2]);
	return;
}

static void
uhid_stop_write(struct usb2_fifo *fifo)
{
	struct uhid_softc *sc = fifo->priv_sc0;

	usb2_transfer_stop(sc->sc_xfer[2]);
	return;
}

static int
uhid_get_report(struct uhid_softc *sc, uint8_t type,
    uint8_t id, void *kern_data, void *user_data,
    uint16_t len)
{
	int err;
	uint8_t free_data = 0;

	if (kern_data == NULL) {
		kern_data = malloc(len, M_USBDEV, M_WAITOK);
		if (kern_data == NULL) {
			err = ENOMEM;
			goto done;
		}
		free_data = 1;
	}
	err = usb2_req_get_report(sc->sc_udev, NULL, kern_data,
	    len, sc->sc_iface_index, type, id);
	if (err) {
		err = ENXIO;
		goto done;
	}
	if (user_data) {
		/* dummy buffer */
		err = copyout(kern_data, user_data, len);
		if (err) {
			goto done;
		}
	}
done:
	if (free_data) {
		free(kern_data, M_USBDEV);
	}
	return (err);
}

static int
uhid_set_report(struct uhid_softc *sc, uint8_t type,
    uint8_t id, void *kern_data, void *user_data,
    uint16_t len)
{
	int err;
	uint8_t free_data = 0;

	if (kern_data == NULL) {
		kern_data = malloc(len, M_USBDEV, M_WAITOK);
		if (kern_data == NULL) {
			err = ENOMEM;
			goto done;
		}
		free_data = 1;
		err = copyin(user_data, kern_data, len);
		if (err) {
			goto done;
		}
	}
	err = usb2_req_set_report(sc->sc_udev, NULL, kern_data,
	    len, sc->sc_iface_index, type, id);
	if (err) {
		err = ENXIO;
		goto done;
	}
done:
	if (free_data) {
		free(kern_data, M_USBDEV);
	}
	return (err);
}

static int
uhid_open(struct usb2_fifo *fifo, int fflags, struct thread *td)
{
	struct uhid_softc *sc = fifo->priv_sc0;

	/*
	 * The buffers are one byte larger than maximum so that one
	 * can detect too large read/writes and short transfers:
	 */
	if (fflags & FREAD) {
		/* reset flags */
		sc->sc_flags &= ~UHID_FLAG_IMMED;

		if (usb2_fifo_alloc_buffer(fifo,
		    sc->sc_isize + 1, UHID_FRAME_NUM)) {
			return (ENOMEM);
		}
	}
	if (fflags & FWRITE) {
		if (usb2_fifo_alloc_buffer(fifo,
		    sc->sc_osize + 1, UHID_FRAME_NUM)) {
			return (ENOMEM);
		}
	}
	return (0);
}

static void
uhid_close(struct usb2_fifo *fifo, int fflags, struct thread *td)
{
	if (fflags & (FREAD | FWRITE)) {
		usb2_fifo_free_buffer(fifo);
	}
	return;
}

static int
uhid_ioctl(struct usb2_fifo *fifo, u_long cmd, void *addr,
    int fflags, struct thread *td)
{
	struct uhid_softc *sc = fifo->priv_sc0;
	struct usb2_gen_descriptor *ugd;
	uint32_t size;
	int error = 0;
	uint8_t id;

	switch (cmd) {
	case USB_GET_REPORT_DESC:
		ugd = addr;
		if (sc->sc_repdesc_size > ugd->ugd_maxlen) {
			size = ugd->ugd_maxlen;
		} else {
			size = sc->sc_repdesc_size;
		}
		ugd->ugd_actlen = size;
		error = copyout(sc->sc_repdesc_ptr, ugd->ugd_data, size);
		break;

	case USB_SET_IMMED:
		if (!(fflags & FREAD)) {
			error = EPERM;
			break;
		}
		if (*(int *)addr) {

			/* do a test read */

			error = uhid_get_report(sc, UHID_INPUT_REPORT,
			    sc->sc_iid, NULL, NULL, sc->sc_isize);
			if (error) {
				break;
			}
			mtx_lock(&sc->sc_mtx);
			sc->sc_flags |= UHID_FLAG_IMMED;
			mtx_unlock(&sc->sc_mtx);
		} else {
			mtx_lock(&sc->sc_mtx);
			sc->sc_flags &= ~UHID_FLAG_IMMED;
			mtx_unlock(&sc->sc_mtx);
		}
		break;

	case USB_GET_REPORT:
		if (!(fflags & FREAD)) {
			error = EPERM;
			break;
		}
		ugd = addr;
		switch (ugd->ugd_report_type) {
		case UHID_INPUT_REPORT:
			size = sc->sc_isize;
			id = sc->sc_iid;
			break;
		case UHID_OUTPUT_REPORT:
			size = sc->sc_osize;
			id = sc->sc_oid;
			break;
		case UHID_FEATURE_REPORT:
			size = sc->sc_fsize;
			id = sc->sc_fid;
			break;
		default:
			return (EINVAL);
		}
		error = uhid_get_report(sc, ugd->ugd_report_type, id,
		    NULL, ugd->ugd_data, size);
		break;

	case USB_SET_REPORT:
		if (!(fflags & FWRITE)) {
			error = EPERM;
			break;
		}
		ugd = addr;
		switch (ugd->ugd_report_type) {
		case UHID_INPUT_REPORT:
			size = sc->sc_isize;
			id = sc->sc_iid;
			break;
		case UHID_OUTPUT_REPORT:
			size = sc->sc_osize;
			id = sc->sc_oid;
			break;
		case UHID_FEATURE_REPORT:
			size = sc->sc_fsize;
			id = sc->sc_fid;
			break;
		default:
			return (EINVAL);
		}
		error = uhid_set_report(sc, ugd->ugd_report_type, id,
		    NULL, ugd->ugd_data, size);
		break;

	case USB_GET_REPORT_ID:
		*(int *)addr = 0;	/* XXX: we only support reportid 0? */
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}

static int
uhid_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	DPRINTFN(11, "\n");

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->use_generic == 0) {
		/* give Mouse and Keyboard drivers a try first */
		return (ENXIO);
	}
	if (uaa->info.bInterfaceClass != UICLASS_HID) {

		/* the Xbox 360 gamepad doesn't use the HID class */

		if ((uaa->info.bInterfaceClass != UICLASS_VENDOR) ||
		    (uaa->info.bInterfaceSubClass != UISUBCLASS_XBOX360_CONTROLLER) ||
		    (uaa->info.bInterfaceProtocol != UIPROTO_XBOX360_GAMEPAD)) {
			return (ENXIO);
		}
	}
	if (usb2_test_quirk(uaa, UQ_HID_IGNORE)) {
		return (ENXIO);
	}
	return (0);
}

static int
uhid_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct uhid_softc *sc = device_get_softc(dev);
	int unit = device_get_unit(dev);
	int error = 0;

	DPRINTFN(10, "sc=%p\n", sc);

	if (sc == NULL) {
		return (ENOMEM);
	}
	device_set_usb2_desc(dev);

	mtx_init(&sc->sc_mtx, "uhid lock", NULL, MTX_DEF | MTX_RECURSE);

	sc->sc_udev = uaa->device;

	sc->sc_iface_no = uaa->info.bIfaceNum;
	sc->sc_iface_index = uaa->info.bIfaceIndex;

	error = usb2_transfer_setup(uaa->device,
	    &uaa->info.bIfaceIndex, sc->sc_xfer, uhid_config,
	    UHID_N_TRANSFER, sc, &sc->sc_mtx);

	if (error) {
		DPRINTF("error=%s\n", usb2_errstr(error));
		goto detach;
	}
	if (uaa->info.idVendor == USB_VENDOR_WACOM) {

		/* the report descriptor for the Wacom Graphire is broken */

		if (uaa->info.idProduct == USB_PRODUCT_WACOM_GRAPHIRE) {

			sc->sc_repdesc_size = sizeof(uhid_graphire_report_descr);
			sc->sc_repdesc_ptr = USB_ADD_BYTES(uhid_graphire_report_descr, 0);
			sc->sc_flags |= UHID_FLAG_STATIC_DESC;

		} else if (uaa->info.idProduct == USB_PRODUCT_WACOM_GRAPHIRE3_4X5) {

			static uint8_t reportbuf[] = {2, 2, 2};

			/*
			 * The Graphire3 needs 0x0202 to be written to
			 * feature report ID 2 before it'll start
			 * returning digitizer data.
			 */
			error = usb2_req_set_report
			    (uaa->device, &Giant, reportbuf, sizeof(reportbuf),
			    uaa->info.bIfaceIndex, UHID_FEATURE_REPORT, 2);

			if (error) {
				DPRINTF("set report failed, error=%s (ignored)\n",
				    usb2_errstr(error));
			}
			sc->sc_repdesc_size = sizeof(uhid_graphire3_4x5_report_descr);
			sc->sc_repdesc_ptr = USB_ADD_BYTES(uhid_graphire3_4x5_report_descr, 0);
			sc->sc_flags |= UHID_FLAG_STATIC_DESC;
		}
	} else if ((uaa->info.bInterfaceClass == UICLASS_VENDOR) &&
		    (uaa->info.bInterfaceSubClass == UISUBCLASS_XBOX360_CONTROLLER) &&
	    (uaa->info.bInterfaceProtocol == UIPROTO_XBOX360_GAMEPAD)) {

		/* the Xbox 360 gamepad has no report descriptor */
		sc->sc_repdesc_size = sizeof(uhid_xb360gp_report_descr);
		sc->sc_repdesc_ptr = USB_ADD_BYTES(uhid_xb360gp_report_descr, 0);
		sc->sc_flags |= UHID_FLAG_STATIC_DESC;
	}
	if (sc->sc_repdesc_ptr == NULL) {

		error = usb2_req_get_hid_desc
		    (uaa->device, &Giant, &sc->sc_repdesc_ptr,
		    &sc->sc_repdesc_size, M_USBDEV, uaa->info.bIfaceIndex);

		if (error) {
			device_printf(dev, "no report descriptor\n");
			goto detach;
		}
	}
	error = usb2_req_set_idle(uaa->device, &Giant,
	    uaa->info.bIfaceIndex, 0, 0);

	if (error) {
		DPRINTF("set idle failed, error=%s (ignored)\n",
		    usb2_errstr(error));
	}
	sc->sc_isize = hid_report_size
	    (sc->sc_repdesc_ptr, sc->sc_repdesc_size, hid_input, &sc->sc_iid);

	sc->sc_osize = hid_report_size
	    (sc->sc_repdesc_ptr, sc->sc_repdesc_size, hid_output, &sc->sc_oid);

	sc->sc_fsize = hid_report_size
	    (sc->sc_repdesc_ptr, sc->sc_repdesc_size, hid_feature, &sc->sc_fid);

	if (sc->sc_isize > UHID_BSIZE) {
		DPRINTF("input size is too large, "
		    "%d bytes (truncating)\n",
		    sc->sc_isize);
		sc->sc_isize = UHID_BSIZE;
	}
	if (sc->sc_osize > UHID_BSIZE) {
		DPRINTF("output size is too large, "
		    "%d bytes (truncating)\n",
		    sc->sc_osize);
		sc->sc_osize = UHID_BSIZE;
	}
	if (sc->sc_fsize > UHID_BSIZE) {
		DPRINTF("feature size is too large, "
		    "%d bytes (truncating)\n",
		    sc->sc_fsize);
		sc->sc_fsize = UHID_BSIZE;
	}
	/* set interface permissions */
	usb2_set_iface_perm(uaa->device, uaa->info.bIfaceIndex,
	    UID_ROOT, GID_OPERATOR, 0644);

	error = usb2_fifo_attach(uaa->device, sc, &sc->sc_mtx,
	    &uhid_fifo_methods, &sc->sc_fifo,
	    unit, 0 - 1, uaa->info.bIfaceIndex);
	if (error) {
		goto detach;
	}
	return (0);			/* success */

detach:
	uhid_detach(dev);
	return (ENOMEM);
}

static int
uhid_detach(device_t dev)
{
	struct uhid_softc *sc = device_get_softc(dev);

	usb2_fifo_detach(&sc->sc_fifo);

	usb2_transfer_unsetup(sc->sc_xfer, UHID_N_TRANSFER);

	if (sc->sc_repdesc_ptr) {
		if (!(sc->sc_flags & UHID_FLAG_STATIC_DESC)) {
			free(sc->sc_repdesc_ptr, M_USBDEV);
		}
	}
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static devclass_t uhid_devclass;

static device_method_t uhid_methods[] = {
	DEVMETHOD(device_probe, uhid_probe),
	DEVMETHOD(device_attach, uhid_attach),
	DEVMETHOD(device_detach, uhid_detach),
	{0, 0}
};

static driver_t uhid_driver = {
	.name = "uhid",
	.methods = uhid_methods,
	.size = sizeof(struct uhid_softc),
};

DRIVER_MODULE(uhid, ushub, uhid_driver, uhid_devclass, NULL, 0);
MODULE_DEPEND(uhid, usb2_input, 1, 1, 1);
MODULE_DEPEND(uhid, usb2_core, 1, 1, 1);
