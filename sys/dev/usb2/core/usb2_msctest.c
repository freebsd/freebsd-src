/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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

/*
 * The following file contains code that will detect USB autoinstall
 * disks.
 *
 * TODO: Potentially we could add code to automatically detect USB
 * mass storage quirks for not supported SCSI commands!
 */

#include <dev/usb2/include/usb2_defs.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_standard.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_transfer.h>
#include <dev/usb2/core/usb2_msctest.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_standard.h>

enum {
	ST_COMMAND,
	ST_DATA_RD,
	ST_DATA_RD_CS,
	ST_DATA_WR,
	ST_DATA_WR_CS,
	ST_STATUS,
	ST_MAX,
};

enum {
	DIR_IN,
	DIR_OUT,
	DIR_NONE,
};

#define	BULK_SIZE 64			/* dummy */


/* Command Block Wrapper */
struct bbb_cbw {
	uDWord	dCBWSignature;
#define	CBWSIGNATURE	0x43425355
	uDWord	dCBWTag;
	uDWord	dCBWDataTransferLength;
	uByte	bCBWFlags;
#define	CBWFLAGS_OUT	0x00
#define	CBWFLAGS_IN	0x80
	uByte	bCBWLUN;
	uByte	bCDBLength;
#define	CBWCDBLENGTH	16
	uByte	CBWCDB[CBWCDBLENGTH];
} __packed;

/* Command Status Wrapper */
struct bbb_csw {
	uDWord	dCSWSignature;
#define	CSWSIGNATURE	0x53425355
	uDWord	dCSWTag;
	uDWord	dCSWDataResidue;
	uByte	bCSWStatus;
#define	CSWSTATUS_GOOD	0x0
#define	CSWSTATUS_FAILED	0x1
#define	CSWSTATUS_PHASE	0x2
} __packed;

struct bbb_transfer {
	struct mtx mtx;
	struct cv cv;
	struct bbb_cbw cbw;
	struct bbb_csw csw;

	struct usb2_xfer *xfer[ST_MAX];

	uint8_t *data_ptr;

	uint32_t data_len;		/* bytes */
	uint32_t data_rem;		/* bytes */
	uint32_t data_timeout;		/* ms */
	uint32_t actlen;		/* bytes */

	uint8_t	cmd_len;		/* bytes */
	uint8_t	dir;
	uint8_t	lun;
	uint8_t	state;
	uint8_t	error;
	uint8_t	status_try;

	uint8_t	buffer[256];
};

static usb2_callback_t bbb_command_callback;
static usb2_callback_t bbb_data_read_callback;
static usb2_callback_t bbb_data_rd_cs_callback;
static usb2_callback_t bbb_data_write_callback;
static usb2_callback_t bbb_data_wr_cs_callback;
static usb2_callback_t bbb_status_callback;

static const struct usb2_config bbb_config[ST_MAX] = {

	[ST_COMMAND] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = sizeof(struct bbb_cbw),
		.mh.flags = {},
		.mh.callback = &bbb_command_callback,
		.mh.timeout = 4 * USB_MS_HZ,	/* 4 seconds */
	},

	[ST_DATA_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = BULK_SIZE,
		.mh.flags = {.proxy_buffer = 1,.short_xfer_ok = 1,},
		.mh.callback = &bbb_data_read_callback,
		.mh.timeout = 4 * USB_MS_HZ,	/* 4 seconds */
	},

	[ST_DATA_RD_CS] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &bbb_data_rd_cs_callback,
		.mh.timeout = 1 * USB_MS_HZ,	/* 1 second  */
	},

	[ST_DATA_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = BULK_SIZE,
		.mh.flags = {.proxy_buffer = 1,.short_xfer_ok = 1,},
		.mh.callback = &bbb_data_write_callback,
		.mh.timeout = 4 * USB_MS_HZ,	/* 4 seconds */
	},

	[ST_DATA_WR_CS] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &bbb_data_wr_cs_callback,
		.mh.timeout = 1 * USB_MS_HZ,	/* 1 second  */
	},

	[ST_STATUS] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = sizeof(struct bbb_csw),
		.mh.flags = {.short_xfer_ok = 1,},
		.mh.callback = &bbb_status_callback,
		.mh.timeout = 1 * USB_MS_HZ,	/* 1 second  */
	},
};

static void
bbb_done(struct bbb_transfer *sc, uint8_t error)
{
	struct usb2_xfer *xfer;

	xfer = sc->xfer[sc->state];

	/* verify the error code */

	if (error) {
		switch (USB_GET_STATE(xfer)) {
		case USB_ST_SETUP:
		case USB_ST_TRANSFERRED:
			error = 1;
			break;
		default:
			error = 2;
			break;
		}
	}
	sc->error = error;
	sc->state = ST_COMMAND;
	sc->status_try = 1;
	usb2_cv_signal(&sc->cv);
	return;
}

static void
bbb_transfer_start(struct bbb_transfer *sc, uint8_t xfer_index)
{
	sc->state = xfer_index;
	usb2_transfer_start(sc->xfer[xfer_index]);
	return;
}

static void
bbb_data_clear_stall_callback(struct usb2_xfer *xfer,
    uint8_t next_xfer, uint8_t stall_xfer)
{
	struct bbb_transfer *sc = xfer->priv_sc;

	if (usb2_clear_stall_callback(xfer, sc->xfer[stall_xfer])) {
		switch (USB_GET_STATE(xfer)) {
		case USB_ST_SETUP:
		case USB_ST_TRANSFERRED:
			bbb_transfer_start(sc, next_xfer);
			break;
		default:
			bbb_done(sc, 1);
			break;
		}
	}
	return;
}

static void
bbb_command_callback(struct usb2_xfer *xfer)
{
	struct bbb_transfer *sc = xfer->priv_sc;
	uint32_t tag;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		bbb_transfer_start
		    (sc, ((sc->dir == DIR_IN) ? ST_DATA_RD :
		    (sc->dir == DIR_OUT) ? ST_DATA_WR :
		    ST_STATUS));
		break;

	case USB_ST_SETUP:
		sc->status_try = 0;
		tag = UGETDW(sc->cbw.dCBWTag) + 1;
		USETDW(sc->cbw.dCBWSignature, CBWSIGNATURE);
		USETDW(sc->cbw.dCBWTag, tag);
		USETDW(sc->cbw.dCBWDataTransferLength, sc->data_len);
		sc->cbw.bCBWFlags = ((sc->dir == DIR_IN) ? CBWFLAGS_IN : CBWFLAGS_OUT);
		sc->cbw.bCBWLUN = sc->lun;
		if (sc->cbw.bCDBLength > sizeof(sc->cbw.CBWCDB)) {
			sc->cbw.bCDBLength = sizeof(sc->cbw.CBWCDB);
			DPRINTFN(0, "Truncating long command!\n");
		}
		xfer->frlengths[0] = sizeof(sc->cbw);

		usb2_set_frame_data(xfer, &sc->cbw, 0);
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		bbb_done(sc, 1);
		break;
	}
	return;
}

static void
bbb_data_read_callback(struct usb2_xfer *xfer)
{
	struct bbb_transfer *sc = xfer->priv_sc;
	uint32_t max_bulk = xfer->max_data_length;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		sc->data_rem -= xfer->actlen;
		sc->data_ptr += xfer->actlen;
		sc->actlen += xfer->actlen;

		if (xfer->actlen < xfer->sumlen) {
			/* short transfer */
			sc->data_rem = 0;
		}
	case USB_ST_SETUP:
		DPRINTF("max_bulk=%d, data_rem=%d\n",
		    max_bulk, sc->data_rem);

		if (sc->data_rem == 0) {
			bbb_transfer_start(sc, ST_STATUS);
			break;
		}
		if (max_bulk > sc->data_rem) {
			max_bulk = sc->data_rem;
		}
		xfer->timeout = sc->data_timeout;
		xfer->frlengths[0] = max_bulk;

		usb2_set_frame_data(xfer, sc->data_ptr, 0);
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			bbb_done(sc, 1);
		} else {
			bbb_transfer_start(sc, ST_DATA_RD_CS);
		}
		break;
	}
	return;
}

static void
bbb_data_rd_cs_callback(struct usb2_xfer *xfer)
{
	bbb_data_clear_stall_callback(xfer, ST_STATUS,
	    ST_DATA_RD);
	return;
}

static void
bbb_data_write_callback(struct usb2_xfer *xfer)
{
	struct bbb_transfer *sc = xfer->priv_sc;
	uint32_t max_bulk = xfer->max_data_length;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		sc->data_rem -= xfer->actlen;
		sc->data_ptr += xfer->actlen;
		sc->actlen += xfer->actlen;

		if (xfer->actlen < xfer->sumlen) {
			/* short transfer */
			sc->data_rem = 0;
		}
	case USB_ST_SETUP:
		DPRINTF("max_bulk=%d, data_rem=%d\n",
		    max_bulk, sc->data_rem);

		if (sc->data_rem == 0) {
			bbb_transfer_start(sc, ST_STATUS);
			return;
		}
		if (max_bulk > sc->data_rem) {
			max_bulk = sc->data_rem;
		}
		xfer->timeout = sc->data_timeout;
		xfer->frlengths[0] = max_bulk;

		usb2_set_frame_data(xfer, sc->data_ptr, 0);
		usb2_start_hardware(xfer);
		return;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			bbb_done(sc, 1);
		} else {
			bbb_transfer_start(sc, ST_DATA_WR_CS);
		}
		return;

	}
}

static void
bbb_data_wr_cs_callback(struct usb2_xfer *xfer)
{
	bbb_data_clear_stall_callback(xfer, ST_STATUS,
	    ST_DATA_WR);
	return;
}

static void
bbb_status_callback(struct usb2_xfer *xfer)
{
	struct bbb_transfer *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		/* very simple status check */

		if (xfer->actlen < sizeof(sc->csw)) {
			bbb_done(sc, 1);/* error */
		} else if (sc->csw.bCSWStatus == CSWSTATUS_GOOD) {
			bbb_done(sc, 0);/* success */
		} else {
			bbb_done(sc, 1);/* error */
		}
		break;

	case USB_ST_SETUP:
		xfer->frlengths[0] = sizeof(sc->csw);

		usb2_set_frame_data(xfer, &sc->csw, 0);
		usb2_start_hardware(xfer);
		break;

	default:
		DPRINTFN(0, "Failed to read CSW: %s, try %d\n",
		    usb2_errstr(xfer->error), sc->status_try);

		if ((xfer->error == USB_ERR_CANCELLED) ||
		    (sc->status_try)) {
			bbb_done(sc, 1);
		} else {
			sc->status_try = 1;
			bbb_transfer_start(sc, ST_DATA_RD_CS);
		}
		break;
	}
	return;
}

/*------------------------------------------------------------------------*
 *	bbb_command_start - execute a SCSI command synchronously
 *
 * Return values
 * 0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
bbb_command_start(struct bbb_transfer *sc, uint8_t dir, uint8_t lun,
    void *data_ptr, uint32_t data_len, uint8_t cmd_len,
    uint32_t data_timeout)
{
	sc->lun = lun;
	sc->dir = data_len ? dir : DIR_NONE;
	sc->data_ptr = data_ptr;
	sc->data_len = data_len;
	sc->data_rem = data_len;
	sc->data_timeout = (data_timeout + USB_MS_HZ);
	sc->actlen = 0;
	sc->cmd_len = cmd_len;

	usb2_transfer_start(sc->xfer[sc->state]);

	while (usb2_transfer_pending(sc->xfer[sc->state])) {
		usb2_cv_wait(&sc->cv, &sc->mtx);
	}
	return (sc->error);
}

/*------------------------------------------------------------------------*
 *	usb2_test_autoinstall
 *
 * Return values:
 * 0: This interface is an auto install disk (CD-ROM)
 * Else: Not an auto install disk.
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_test_autoinstall(struct usb2_device *udev, uint8_t iface_index)
{
	struct usb2_interface *iface;
	struct usb2_interface_descriptor *id;
	usb2_error_t err;
	uint8_t timeout;
	uint8_t sid_type;
	struct bbb_transfer *sc;

	if (udev == NULL) {
		return (USB_ERR_INVAL);
	}
	iface = usb2_get_iface(udev, iface_index);
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
	switch (id->bInterfaceSubClass) {
	case UISUBCLASS_SCSI:
	case UISUBCLASS_UFI:
		break;
	default:
		return (USB_ERR_INVAL);
	}

	switch (id->bInterfaceProtocol) {
	case UIPROTO_MASS_BBB_OLD:
	case UIPROTO_MASS_BBB:
		break;
	default:
		return (USB_ERR_INVAL);
	}

	sc = malloc(sizeof(*sc), M_USB, M_WAITOK | M_ZERO);
	if (sc == NULL) {
		return (USB_ERR_NOMEM);
	}
	mtx_init(&sc->mtx, "USB autoinstall", NULL, MTX_DEF);
	usb2_cv_init(&sc->cv, "WBBB");

	err = usb2_transfer_setup(udev,
	    &iface_index, sc->xfer, bbb_config,
	    ST_MAX, sc, &sc->mtx);

	if (err) {
		goto done;
	}
	mtx_lock(&sc->mtx);

	timeout = 4;			/* tries */

repeat_inquiry:

	sc->cbw.CBWCDB[0] = 0x12;	/* INQUIRY */

	err = bbb_command_start(sc, DIR_IN, 0, sc->buffer, 256, 6, USB_MS_HZ);
	if (err) {
		err = bbb_command_start(sc, DIR_IN, 0, sc->buffer, 256, 12, USB_MS_HZ);
		if (err) {
			err = bbb_command_start(sc, DIR_IN, 0, sc->buffer, 256, 16, USB_MS_HZ);
		}
	}
	if ((sc->actlen != 0) && (err == 0)) {
		sid_type = sc->buffer[0] & 0x1F;
		if (sid_type == 0x05) {
			/* CD-ROM */
			/* XXX could investigate more */
			return (0);
		}
	} else if (--timeout) {
		usb2_pause_mtx(&sc->mtx, USB_MS_HZ);
		goto repeat_inquiry;
	}
	err = USB_ERR_INVAL;
	goto done;

done:
	mtx_unlock(&sc->mtx);
	usb2_transfer_unsetup(sc->xfer, ST_MAX);
	mtx_destroy(&sc->mtx);
	usb2_cv_destroy(&sc->cv);
	free(sc, M_USB);
	return (err);
}

/*
 * Huawei Exxx radio devices have a built in flash disk which is their
 * default power up configuration. This allows the device to carry its
 * own installation software.
 *
 * Instead of following the USB spec, and create multiple
 * configuration descriptors for this, the devices expects the driver
 * to send UF_DEVICE_REMOTE_WAKEUP to endpoint 2 to reset the device,
 * so it reprobes, now with the radio exposed.
 */

usb2_error_t
usb2_test_huawei(struct usb2_device *udev, uint8_t iface_index)
{
	struct usb2_device_request req;
	struct usb2_interface *iface;
	struct usb2_interface_descriptor *id;
	usb2_error_t err;

	if (udev == NULL) {
		return (USB_ERR_INVAL);
	}
	iface = usb2_get_iface(udev, iface_index);
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
	/* Bend it like Beckham */
	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, UF_DEVICE_REMOTE_WAKEUP);
	USETW(req.wIndex, 2);
	USETW(req.wLength, 0);

	/* We get error at return, but it works */
	err = usb2_do_request_flags(udev, NULL, &req, NULL, 0, NULL, 1 * USB_MS_HZ);

	return (0);			/* success */
}
