/* $FreeBSD$ */
/*-
 * Copyright (c) 2008,2011 Hans Petter Selasky. All rights reserved.
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

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
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

#define	USB_DEBUG_VAR usb_debug

#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_transfer.h>
#include <dev/usb/usb_msctest.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_request.h>
#include <dev/usb/usb_util.h>
#include <dev/usb/quirk/usb_quirk.h>

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

#define	SCSI_MAX_LEN	0x100
#define	SCSI_INQ_LEN	0x24
#define	SCSI_SENSE_LEN	0xFF

static uint8_t scsi_test_unit_ready[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static uint8_t scsi_inquiry[] = { 0x12, 0x00, 0x00, 0x00, SCSI_INQ_LEN, 0x00 };
static uint8_t scsi_rezero_init[] =     { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };
static uint8_t scsi_start_stop_unit[] = { 0x1b, 0x00, 0x00, 0x00, 0x02, 0x00 };
static uint8_t scsi_ztestor_eject[] =   { 0x85, 0x01, 0x01, 0x01, 0x18, 0x01,
					  0x01, 0x01, 0x01, 0x01, 0x00, 0x00 };
static uint8_t scsi_cmotech_eject[] =   { 0xff, 0x52, 0x44, 0x45, 0x56, 0x43,
					  0x48, 0x47 };
static uint8_t scsi_huawei_eject[] =	{ 0x11, 0x06, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00 };
static uint8_t scsi_tct_eject[] =	{ 0x06, 0xf5, 0x04, 0x02, 0x52, 0x70 };
static uint8_t scsi_sync_cache[] =	{ 0x35, 0x00, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00 };
static uint8_t scsi_request_sense[] =	{ 0x03, 0x00, 0x00, 0x00, 0x12, 0x00,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#define	BULK_SIZE		64	/* dummy */
#define	ERR_CSW_FAILED		-1

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

	struct usb_xfer *xfer[ST_MAX];

	uint8_t *data_ptr;

	usb_size_t data_len;		/* bytes */
	usb_size_t data_rem;		/* bytes */
	usb_timeout_t data_timeout;	/* ms */
	usb_frlength_t actlen;		/* bytes */

	uint8_t	cmd_len;		/* bytes */
	uint8_t	dir;
	uint8_t	lun;
	uint8_t	state;
	uint8_t	status_try;
	int	error;

	uint8_t	buffer[SCSI_MAX_LEN] __aligned(4);
};

static usb_callback_t bbb_command_callback;
static usb_callback_t bbb_data_read_callback;
static usb_callback_t bbb_data_rd_cs_callback;
static usb_callback_t bbb_data_write_callback;
static usb_callback_t bbb_data_wr_cs_callback;
static usb_callback_t bbb_status_callback;

static void	bbb_done(struct bbb_transfer *, int);
static void	bbb_transfer_start(struct bbb_transfer *, uint8_t);
static void	bbb_data_clear_stall_callback(struct usb_xfer *, uint8_t,
		    uint8_t);
static int	bbb_command_start(struct bbb_transfer *, uint8_t, uint8_t,
		    void *, size_t, void *, size_t, usb_timeout_t);
static struct bbb_transfer *bbb_attach(struct usb_device *, uint8_t);
static void	bbb_detach(struct bbb_transfer *);

static const struct usb_config bbb_config[ST_MAX] = {

	[ST_COMMAND] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = sizeof(struct bbb_cbw),
		.flags = {.ext_buffer = 1,},
		.callback = &bbb_command_callback,
		.timeout = 4 * USB_MS_HZ,	/* 4 seconds */
	},

	[ST_DATA_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = BULK_SIZE,
		.flags = {.ext_buffer = 1,.proxy_buffer = 1,.short_xfer_ok = 1,},
		.callback = &bbb_data_read_callback,
		.timeout = 4 * USB_MS_HZ,	/* 4 seconds */
	},

	[ST_DATA_RD_CS] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.bufsize = sizeof(struct usb_device_request),
		.callback = &bbb_data_rd_cs_callback,
		.timeout = 1 * USB_MS_HZ,	/* 1 second  */
	},

	[ST_DATA_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = BULK_SIZE,
		.flags = {.ext_buffer = 1,.proxy_buffer = 1,},
		.callback = &bbb_data_write_callback,
		.timeout = 4 * USB_MS_HZ,	/* 4 seconds */
	},

	[ST_DATA_WR_CS] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.bufsize = sizeof(struct usb_device_request),
		.callback = &bbb_data_wr_cs_callback,
		.timeout = 1 * USB_MS_HZ,	/* 1 second  */
	},

	[ST_STATUS] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = sizeof(struct bbb_csw),
		.flags = {.ext_buffer = 1,.short_xfer_ok = 1,},
		.callback = &bbb_status_callback,
		.timeout = 1 * USB_MS_HZ,	/* 1 second  */
	},
};

static void
bbb_done(struct bbb_transfer *sc, int error)
{

	sc->error = error;
	sc->state = ST_COMMAND;
	sc->status_try = 1;
	cv_signal(&sc->cv);
}

static void
bbb_transfer_start(struct bbb_transfer *sc, uint8_t xfer_index)
{
	sc->state = xfer_index;
	usbd_transfer_start(sc->xfer[xfer_index]);
}

static void
bbb_data_clear_stall_callback(struct usb_xfer *xfer,
    uint8_t next_xfer, uint8_t stall_xfer)
{
	struct bbb_transfer *sc = usbd_xfer_softc(xfer);

	if (usbd_clear_stall_callback(xfer, sc->xfer[stall_xfer])) {
		switch (USB_GET_STATE(xfer)) {
		case USB_ST_SETUP:
		case USB_ST_TRANSFERRED:
			bbb_transfer_start(sc, next_xfer);
			break;
		default:
			bbb_done(sc, USB_ERR_STALLED);
			break;
		}
	}
}

static void
bbb_command_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct bbb_transfer *sc = usbd_xfer_softc(xfer);
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
		USETDW(sc->cbw.dCBWDataTransferLength, (uint32_t)sc->data_len);
		sc->cbw.bCBWFlags = ((sc->dir == DIR_IN) ? CBWFLAGS_IN : CBWFLAGS_OUT);
		sc->cbw.bCBWLUN = sc->lun;
		sc->cbw.bCDBLength = sc->cmd_len;
		if (sc->cbw.bCDBLength > sizeof(sc->cbw.CBWCDB)) {
			sc->cbw.bCDBLength = sizeof(sc->cbw.CBWCDB);
			DPRINTFN(0, "Truncating long command\n");
		}
		usbd_xfer_set_frame_data(xfer, 0, &sc->cbw, sizeof(sc->cbw));
		usbd_transfer_submit(xfer);
		break;

	default:			/* Error */
		bbb_done(sc, error);
		break;
	}
}

static void
bbb_data_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct bbb_transfer *sc = usbd_xfer_softc(xfer);
	usb_frlength_t max_bulk = usbd_xfer_max_len(xfer);
	int actlen, sumlen;

	usbd_xfer_status(xfer, &actlen, &sumlen, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		sc->data_rem -= actlen;
		sc->data_ptr += actlen;
		sc->actlen += actlen;

		if (actlen < sumlen) {
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
		usbd_xfer_set_timeout(xfer, sc->data_timeout);
		usbd_xfer_set_frame_data(xfer, 0, sc->data_ptr, max_bulk);
		usbd_transfer_submit(xfer);
		break;

	default:			/* Error */
		if (error == USB_ERR_CANCELLED) {
			bbb_done(sc, error);
		} else {
			bbb_transfer_start(sc, ST_DATA_RD_CS);
		}
		break;
	}
}

static void
bbb_data_rd_cs_callback(struct usb_xfer *xfer, usb_error_t error)
{
	bbb_data_clear_stall_callback(xfer, ST_STATUS,
	    ST_DATA_RD);
}

static void
bbb_data_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct bbb_transfer *sc = usbd_xfer_softc(xfer);
	usb_frlength_t max_bulk = usbd_xfer_max_len(xfer);
	int actlen, sumlen;

	usbd_xfer_status(xfer, &actlen, &sumlen, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		sc->data_rem -= actlen;
		sc->data_ptr += actlen;
		sc->actlen += actlen;

		if (actlen < sumlen) {
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
		usbd_xfer_set_timeout(xfer, sc->data_timeout);
		usbd_xfer_set_frame_data(xfer, 0, sc->data_ptr, max_bulk);
		usbd_transfer_submit(xfer);
		return;

	default:			/* Error */
		if (error == USB_ERR_CANCELLED) {
			bbb_done(sc, error);
		} else {
			bbb_transfer_start(sc, ST_DATA_WR_CS);
		}
		return;

	}
}

static void
bbb_data_wr_cs_callback(struct usb_xfer *xfer, usb_error_t error)
{
	bbb_data_clear_stall_callback(xfer, ST_STATUS,
	    ST_DATA_WR);
}

static void
bbb_status_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct bbb_transfer *sc = usbd_xfer_softc(xfer);
	int actlen, sumlen;

	usbd_xfer_status(xfer, &actlen, &sumlen, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		/* very simple status check */

		if (actlen < sizeof(sc->csw)) {
			bbb_done(sc, USB_ERR_SHORT_XFER);
		} else if (sc->csw.bCSWStatus == CSWSTATUS_GOOD) {
			bbb_done(sc, 0);	/* success */
		} else {
			bbb_done(sc, ERR_CSW_FAILED);	/* error */
		}
		break;

	case USB_ST_SETUP:
		usbd_xfer_set_frame_data(xfer, 0, &sc->csw, sizeof(sc->csw));
		usbd_transfer_submit(xfer);
		break;

	default:
		DPRINTF("Failed to read CSW: %s, try %d\n",
		    usbd_errstr(error), sc->status_try);

		if (error == USB_ERR_CANCELLED || sc->status_try) {
			bbb_done(sc, error);
		} else {
			sc->status_try = 1;
			bbb_transfer_start(sc, ST_DATA_RD_CS);
		}
		break;
	}
}

/*------------------------------------------------------------------------*
 *	bbb_command_start - execute a SCSI command synchronously
 *
 * Return values
 * 0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static int
bbb_command_start(struct bbb_transfer *sc, uint8_t dir, uint8_t lun,
    void *data_ptr, size_t data_len, void *cmd_ptr, size_t cmd_len,
    usb_timeout_t data_timeout)
{
	sc->lun = lun;
	sc->dir = data_len ? dir : DIR_NONE;
	sc->data_ptr = data_ptr;
	sc->data_len = data_len;
	sc->data_rem = data_len;
	sc->data_timeout = (data_timeout + USB_MS_HZ);
	sc->actlen = 0;
	sc->cmd_len = cmd_len;
	bzero(&sc->cbw.CBWCDB, sizeof(sc->cbw.CBWCDB));
	bcopy(cmd_ptr, &sc->cbw.CBWCDB, cmd_len);
	DPRINTFN(1, "SCSI cmd = %*D\n", (int)cmd_len, &sc->cbw.CBWCDB, ":");

	mtx_lock(&sc->mtx);
	usbd_transfer_start(sc->xfer[sc->state]);

	while (usbd_transfer_pending(sc->xfer[sc->state])) {
		cv_wait(&sc->cv, &sc->mtx);
	}
	mtx_unlock(&sc->mtx);
	return (sc->error);
}

static struct bbb_transfer *
bbb_attach(struct usb_device *udev, uint8_t iface_index)
{
	struct usb_interface *iface;
	struct usb_interface_descriptor *id;
	struct bbb_transfer *sc;
	usb_error_t err;
	uint8_t do_unlock;

	/* automatic locking */
	if (usbd_enum_is_locked(udev)) {
		do_unlock = 0;
	} else {
		do_unlock = 1;
		usbd_enum_lock(udev);
	}

	/*
	 * Make sure any driver which is hooked up to this interface,
	 * like umass is gone:
	 */
	usb_detach_device(udev, iface_index, 0);

	if (do_unlock)
		usbd_enum_unlock(udev);

	iface = usbd_get_iface(udev, iface_index);
	if (iface == NULL)
		return (NULL);

	id = iface->idesc;
	if (id == NULL || id->bInterfaceClass != UICLASS_MASS)
		return (NULL);

	switch (id->bInterfaceSubClass) {
	case UISUBCLASS_SCSI:
	case UISUBCLASS_UFI:
	case UISUBCLASS_SFF8020I:
	case UISUBCLASS_SFF8070I:
		break;
	default:
		return (NULL);
	}

	switch (id->bInterfaceProtocol) {
	case UIPROTO_MASS_BBB_OLD:
	case UIPROTO_MASS_BBB:
		break;
	default:
		return (NULL);
	}

	sc = malloc(sizeof(*sc), M_USB, M_WAITOK | M_ZERO);
	mtx_init(&sc->mtx, "USB autoinstall", NULL, MTX_DEF);
	cv_init(&sc->cv, "WBBB");

	err = usbd_transfer_setup(udev, &iface_index, sc->xfer, bbb_config,
	    ST_MAX, sc, &sc->mtx);
	if (err) {
		bbb_detach(sc);
		return (NULL);
	}
	return (sc);
}

static void
bbb_detach(struct bbb_transfer *sc)
{
	usbd_transfer_unsetup(sc->xfer, ST_MAX);
	mtx_destroy(&sc->mtx);
	cv_destroy(&sc->cv);
	free(sc, M_USB);
}

/*------------------------------------------------------------------------*
 *	usb_iface_is_cdrom
 *
 * Return values:
 * 1: This interface is an auto install disk (CD-ROM)
 * 0: Not an auto install disk.
 *------------------------------------------------------------------------*/
int
usb_iface_is_cdrom(struct usb_device *udev, uint8_t iface_index)
{
	struct bbb_transfer *sc;
	uint8_t timeout;
	uint8_t is_cdrom;
	uint8_t sid_type;
	int err;

	sc = bbb_attach(udev, iface_index);
	if (sc == NULL)
		return (0);

	is_cdrom = 0;
	timeout = 4;	/* tries */
	while (--timeout) {
		err = bbb_command_start(sc, DIR_IN, 0, sc->buffer,
		    SCSI_INQ_LEN, &scsi_inquiry, sizeof(scsi_inquiry),
		    USB_MS_HZ);

		if (err == 0 && sc->actlen > 0) {
			sid_type = sc->buffer[0] & 0x1F;
			if (sid_type == 0x05)
				is_cdrom = 1;
			break;
		} else if (err != ERR_CSW_FAILED)
			break;	/* non retryable error */
		usb_pause_mtx(NULL, hz);
	}
	bbb_detach(sc);
	return (is_cdrom);
}

usb_error_t
usb_msc_auto_quirk(struct usb_device *udev, uint8_t iface_index)
{
	struct bbb_transfer *sc;
	uint8_t timeout;
	uint8_t is_no_direct;
	uint8_t sid_type;
	int err;

	sc = bbb_attach(udev, iface_index);
	if (sc == NULL)
		return (0);

	/*
	 * Some devices need a delay after that the configuration
	 * value is set to function properly:
	 */
	usb_pause_mtx(NULL, hz);

	is_no_direct = 1;
	for (timeout = 4; timeout; timeout--) {
		err = bbb_command_start(sc, DIR_IN, 0, sc->buffer,
		    SCSI_INQ_LEN, &scsi_inquiry, sizeof(scsi_inquiry),
		    USB_MS_HZ);

		if (err == 0 && sc->actlen > 0) {
			sid_type = sc->buffer[0] & 0x1F;
			if (sid_type == 0x00)
				is_no_direct = 0;
			break;
		} else if (err != ERR_CSW_FAILED)
			break;	/* non retryable error */
		usb_pause_mtx(NULL, hz);
	}

	if (is_no_direct) {
		DPRINTF("Device is not direct access.\n");
		goto done;
	}

	err = bbb_command_start(sc, DIR_IN, 0, NULL, 0,
	    &scsi_test_unit_ready, sizeof(scsi_test_unit_ready),
	    USB_MS_HZ);

	if (err != 0) {

		if (err != ERR_CSW_FAILED)
			goto error;
	}

	err = bbb_command_start(sc, DIR_IN, 0, NULL, 0,
	    &scsi_sync_cache, sizeof(scsi_sync_cache),
	    USB_MS_HZ);

	if (err != 0) {

		if (err != ERR_CSW_FAILED)
			goto error;

		DPRINTF("Device doesn't handle synchronize cache\n");

		usbd_add_dynamic_quirk(udev, UQ_MSC_NO_SYNC_CACHE);
	}

	/* clear sense status of any failed commands on the device */

	err = bbb_command_start(sc, DIR_IN, 0, sc->buffer,
	    SCSI_INQ_LEN, &scsi_inquiry, sizeof(scsi_inquiry),
	    USB_MS_HZ);

	DPRINTF("Inquiry = %d\n", err);

	if (err != 0) {

		if (err != ERR_CSW_FAILED)
			goto error;
	}

	err = bbb_command_start(sc, DIR_IN, 0, sc->buffer,
	    SCSI_SENSE_LEN, &scsi_request_sense,
	    sizeof(scsi_request_sense), USB_MS_HZ);

	DPRINTF("Request sense = %d\n", err);

	if (err != 0) {

		if (err != ERR_CSW_FAILED)
			goto error;
	}

done:
	bbb_detach(sc);
	return (0);

error:
 	bbb_detach(sc);

	DPRINTF("Device did not respond, enabling all quirks\n");

	usbd_add_dynamic_quirk(udev, UQ_MSC_NO_SYNC_CACHE);
	usbd_add_dynamic_quirk(udev, UQ_MSC_NO_TEST_UNIT_READY);

	/* Need to re-enumerate the device */
	usbd_req_re_enumerate(udev, NULL);

	return (USB_ERR_STALLED);
}

usb_error_t
usb_msc_eject(struct usb_device *udev, uint8_t iface_index, int method)
{
	struct bbb_transfer *sc;
	usb_error_t err;

	sc = bbb_attach(udev, iface_index);
	if (sc == NULL)
		return (USB_ERR_INVAL);

	err = 0;
	switch (method) {
	case MSC_EJECT_STOPUNIT:
		err = bbb_command_start(sc, DIR_IN, 0, NULL, 0,
		    &scsi_test_unit_ready, sizeof(scsi_test_unit_ready),
		    USB_MS_HZ);
		DPRINTF("Test unit ready status: %s\n", usbd_errstr(err));
		err = bbb_command_start(sc, DIR_IN, 0, NULL, 0,
		    &scsi_start_stop_unit, sizeof(scsi_start_stop_unit),
		    USB_MS_HZ);
		break;
	case MSC_EJECT_REZERO:
		err = bbb_command_start(sc, DIR_IN, 0, NULL, 0,
		    &scsi_rezero_init, sizeof(scsi_rezero_init),
		    USB_MS_HZ);
		break;
	case MSC_EJECT_ZTESTOR:
		err = bbb_command_start(sc, DIR_IN, 0, NULL, 0,
		    &scsi_ztestor_eject, sizeof(scsi_ztestor_eject),
		    USB_MS_HZ);
		break;
	case MSC_EJECT_CMOTECH:
		err = bbb_command_start(sc, DIR_IN, 0, NULL, 0,
		    &scsi_cmotech_eject, sizeof(scsi_cmotech_eject),
		    USB_MS_HZ);
		break;
	case MSC_EJECT_HUAWEI:
		err = bbb_command_start(sc, DIR_IN, 0, NULL, 0,
		    &scsi_huawei_eject, sizeof(scsi_huawei_eject),
		    USB_MS_HZ);
		break;
	case MSC_EJECT_TCT:
		/*
		 * TCTMobile needs DIR_IN flag. To get it, we
		 * supply a dummy data with the command.
		 */
		err = bbb_command_start(sc, DIR_IN, 0, &sc->buffer,
		    sizeof(sc->buffer), &scsi_tct_eject,
		    sizeof(scsi_tct_eject), USB_MS_HZ);
		break;
	default:
		printf("usb_msc_eject: unknown eject method (%d)\n", method);
		break;
	}
	DPRINTF("Eject CD command status: %s\n", usbd_errstr(err));

	bbb_detach(sc);
	return (0);
}
