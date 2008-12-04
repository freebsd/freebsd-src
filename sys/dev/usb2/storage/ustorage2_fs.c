/* $FreeBSD$ */
/*-
 * Copyright (C) 2003-2005 Alan Stern
 * Copyright (C) 2008 Hans Petter Selasky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NOTE: Much of the SCSI statemachine handling code derives from the
 * Linux USB gadget stack.
 */
#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_defs.h>

#define	USB_DEBUG_VAR ustorage_fs_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_device.h>

#if USB_DEBUG
static int ustorage_fs_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, ustorage_fs, CTLFLAG_RW, 0, "USB ustorage_fs");
SYSCTL_INT(_hw_usb2_ustorage_fs, OID_AUTO, debug, CTLFLAG_RW,
    &ustorage_fs_debug, 0, "ustorage_fs debug level");
#endif

/* Define some limits */

#define	USTORAGE_FS_BULK_SIZE (1 << 17)
#define	USTORAGE_FS_MAX_LUN 8
#define	USTORAGE_FS_RELEASE 0x0101
#define	USTORAGE_FS_RAM_SECT (1 << 13)

static uint8_t *ustorage_fs_ramdisk;

/* USB transfer definitions */

#define	USTORAGE_FS_T_BBB_COMMAND     0
#define	USTORAGE_FS_T_BBB_DATA_DUMP   1
#define	USTORAGE_FS_T_BBB_DATA_READ   2
#define	USTORAGE_FS_T_BBB_DATA_WRITE  3
#define	USTORAGE_FS_T_BBB_STATUS      4
#define	USTORAGE_FS_T_BBB_MAX         5

/* USB data stage direction */

#define	DIR_NONE	0
#define	DIR_READ	1
#define	DIR_WRITE	2

/* USB interface specific control request */

#define	UR_BBB_RESET		0xff	/* Bulk-Only reset */
#define	UR_BBB_GET_MAX_LUN	0xfe	/* Get maximum lun */

/* Command Block Wrapper */
typedef struct {
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
} __packed ustorage_fs_bbb_cbw_t;

#define	USTORAGE_FS_BBB_CBW_SIZE	31

/* Command Status Wrapper */
typedef struct {
	uDWord	dCSWSignature;
#define	CSWSIGNATURE	0x53425355
	uDWord	dCSWTag;
	uDWord	dCSWDataResidue;
	uByte	bCSWStatus;
#define	CSWSTATUS_GOOD	0x0
#define	CSWSTATUS_FAILED	0x1
#define	CSWSTATUS_PHASE	0x2
} __packed ustorage_fs_bbb_csw_t;

#define	USTORAGE_FS_BBB_CSW_SIZE	13

struct ustorage_fs_lun {

	void   *memory_image;

	uint32_t num_sectors;
	uint32_t sense_data;
	uint32_t sense_data_info;
	uint32_t unit_attention_data;

	uint8_t	read_only:1;
	uint8_t	prevent_medium_removal:1;
	uint8_t	info_valid:1;
	uint8_t	removable:1;
};

struct ustorage_fs_softc {

	ustorage_fs_bbb_cbw_t sc_cbw;	/* Command Wrapper Block */
	ustorage_fs_bbb_csw_t sc_csw;	/* Command Status Block */

	struct mtx sc_mtx;

	struct ustorage_fs_lun sc_lun[USTORAGE_FS_MAX_LUN];

	struct {
		uint8_t *data_ptr;
		struct ustorage_fs_lun *currlun;

		uint32_t data_rem;	/* bytes, as reported by the command
					 * block wrapper */
		uint32_t offset;	/* bytes */

		uint8_t	cbw_dir;
		uint8_t	cmd_dir;
		uint8_t	lun;
		uint8_t	cmd_data[CBWCDBLENGTH];
		uint8_t	cmd_len;
		uint8_t	data_short:1;
		uint8_t	data_error:1;
	}	sc_transfer;

	device_t sc_dev;
	struct usb2_device *sc_udev;
	struct usb2_xfer *sc_xfer[USTORAGE_FS_T_BBB_MAX];

	uint32_t sc_unit;

	uint8_t	sc_name[16];
	uint8_t	sc_iface_no;		/* interface number */
	uint8_t	sc_last_lun;
	uint8_t	sc_last_xfer_index;
	uint8_t	sc_qdata[1024];
};

/* prototypes */

static device_probe_t ustorage_fs_probe;
static device_attach_t ustorage_fs_attach;
static device_detach_t ustorage_fs_detach;
static device_suspend_t ustorage_fs_suspend;
static device_resume_t ustorage_fs_resume;
static device_shutdown_t ustorage_fs_shutdown;
static usb2_handle_request_t ustorage_fs_handle_request;

static usb2_callback_t ustorage_fs_t_bbb_command_callback;
static usb2_callback_t ustorage_fs_t_bbb_data_dump_callback;
static usb2_callback_t ustorage_fs_t_bbb_data_read_callback;
static usb2_callback_t ustorage_fs_t_bbb_data_write_callback;
static usb2_callback_t ustorage_fs_t_bbb_status_callback;

static void ustorage_fs_transfer_start(struct ustorage_fs_softc *sc, uint8_t xfer_index);
static void ustorage_fs_transfer_stop(struct ustorage_fs_softc *sc);

static uint8_t ustorage_fs_verify(struct ustorage_fs_softc *sc);
static uint8_t ustorage_fs_inquiry(struct ustorage_fs_softc *sc);
static uint8_t ustorage_fs_request_sense(struct ustorage_fs_softc *sc);
static uint8_t ustorage_fs_read_capacity(struct ustorage_fs_softc *sc);
static uint8_t ustorage_fs_mode_sense(struct ustorage_fs_softc *sc);
static uint8_t ustorage_fs_start_stop(struct ustorage_fs_softc *sc);
static uint8_t ustorage_fs_prevent_allow(struct ustorage_fs_softc *sc);
static uint8_t ustorage_fs_read_format_capacities(struct ustorage_fs_softc *sc);
static uint8_t ustorage_fs_mode_select(struct ustorage_fs_softc *sc);
static uint8_t ustorage_fs_min_len(struct ustorage_fs_softc *sc, uint32_t len, uint32_t mask);
static uint8_t ustorage_fs_read(struct ustorage_fs_softc *sc);
static uint8_t ustorage_fs_write(struct ustorage_fs_softc *sc);
static uint8_t ustorage_fs_check_cmd(struct ustorage_fs_softc *sc, uint8_t cmd_size, uint16_t mask, uint8_t needs_medium);
static uint8_t ustorage_fs_do_cmd(struct ustorage_fs_softc *sc);

static device_method_t ustorage_fs_methods[] = {
	/* USB interface */
	DEVMETHOD(usb2_handle_request, ustorage_fs_handle_request),

	/* Device interface */
	DEVMETHOD(device_probe, ustorage_fs_probe),
	DEVMETHOD(device_attach, ustorage_fs_attach),
	DEVMETHOD(device_detach, ustorage_fs_detach),
	DEVMETHOD(device_suspend, ustorage_fs_suspend),
	DEVMETHOD(device_resume, ustorage_fs_resume),
	DEVMETHOD(device_shutdown, ustorage_fs_shutdown),

	{0, 0}
};

static driver_t ustorage_fs_driver = {
	.name = "ustorage_fs",
	.methods = ustorage_fs_methods,
	.size = sizeof(struct ustorage_fs_softc),
};

static devclass_t ustorage_fs_devclass;

DRIVER_MODULE(ustorage_fs, ushub, ustorage_fs_driver, ustorage_fs_devclass, NULL, 0);
MODULE_VERSION(ustorage_fs, 0);
MODULE_DEPEND(ustorage_fs, usb2_storage, 1, 1, 1);
MODULE_DEPEND(ustorage_fs, usb2_core, 1, 1, 1);

struct usb2_config ustorage_fs_bbb_config[USTORAGE_FS_T_BBB_MAX] = {

	[USTORAGE_FS_T_BBB_COMMAND] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.md.bufsize = sizeof(ustorage_fs_bbb_cbw_t),
		.md.flags = {.ext_buffer = 1,},
		.md.callback = &ustorage_fs_t_bbb_command_callback,
	},

	[USTORAGE_FS_T_BBB_DATA_DUMP] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.md.bufsize = 0,	/* use wMaxPacketSize */
		.md.flags = {.proxy_buffer = 1,.short_xfer_ok = 1,},
		.md.callback = &ustorage_fs_t_bbb_data_dump_callback,
	},

	[USTORAGE_FS_T_BBB_DATA_READ] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.md.bufsize = USTORAGE_FS_BULK_SIZE,
		.md.flags = {.proxy_buffer = 1,.short_xfer_ok = 1,.ext_buffer = 1},
		.md.callback = &ustorage_fs_t_bbb_data_read_callback,
	},

	[USTORAGE_FS_T_BBB_DATA_WRITE] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.md.bufsize = USTORAGE_FS_BULK_SIZE,
		.md.flags = {.proxy_buffer = 1,.short_xfer_ok = 1,.ext_buffer = 1},
		.md.callback = &ustorage_fs_t_bbb_data_write_callback,
	},

	[USTORAGE_FS_T_BBB_STATUS] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.md.bufsize = sizeof(ustorage_fs_bbb_csw_t),
		.md.flags = {.short_xfer_ok = 1,.ext_buffer = 1,},
		.md.callback = &ustorage_fs_t_bbb_status_callback,
	},
};

/*
 * USB device probe/attach/detach
 */

static int
ustorage_fs_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct usb2_interface_descriptor *id;

	if (uaa->usb2_mode != USB_MODE_DEVICE) {
		return (ENXIO);
	}
	if (uaa->use_generic == 0) {
		/* give other drivers a try first */
		return (ENXIO);
	}
	/* Check for a standards compliant device */
	id = usb2_get_interface_descriptor(uaa->iface);
	if ((id == NULL) ||
	    (id->bInterfaceClass != UICLASS_MASS) ||
	    (id->bInterfaceSubClass != UISUBCLASS_SCSI) ||
	    (id->bInterfaceProtocol != UIPROTO_MASS_BBB)) {
		return (ENXIO);
	}
	return (0);
}

static int
ustorage_fs_attach(device_t dev)
{
	struct ustorage_fs_softc *sc = device_get_softc(dev);
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct usb2_interface_descriptor *id;
	int err;

	if (sc == NULL) {
		return (ENOMEM);
	}
	/*
	 * NOTE: the softc struct is bzero-ed in device_set_driver.
	 * We can safely call ustorage_fs_detach without specifically
	 * initializing the struct.
	 */

	sc->sc_dev = dev;
	sc->sc_udev = uaa->device;
	sc->sc_unit = device_get_unit(dev);

	if (sc->sc_unit == 0) {
		if (ustorage_fs_ramdisk == NULL) {
			/*
			 * allocate a memory image for our ramdisk until
			 * further
			 */
			ustorage_fs_ramdisk =
			    malloc(USTORAGE_FS_RAM_SECT << 9, M_USB, M_ZERO | M_WAITOK);
			if (ustorage_fs_ramdisk == NULL) {
				return (ENOMEM);
			}
		}
		sc->sc_lun[0].memory_image = ustorage_fs_ramdisk;
		sc->sc_lun[0].num_sectors = USTORAGE_FS_RAM_SECT;
		sc->sc_lun[0].removable = 1;
	}
	snprintf(sc->sc_name, sizeof(sc->sc_name),
	    "%s", device_get_nameunit(dev));

	device_set_usb2_desc(dev);

	mtx_init(&sc->sc_mtx, "USTORAGE_FS lock",
	    NULL, (MTX_DEF | MTX_RECURSE));

	/* get interface index */

	id = usb2_get_interface_descriptor(uaa->iface);
	if (id == NULL) {
		device_printf(dev, "failed to get "
		    "interface number\n");
		goto detach;
	}
	sc->sc_iface_no = id->bInterfaceNumber;

	err = usb2_transfer_setup(uaa->device,
	    &uaa->info.bIfaceIndex, sc->sc_xfer, ustorage_fs_bbb_config,
	    USTORAGE_FS_T_BBB_MAX, sc, &sc->sc_mtx);
	if (err) {
		device_printf(dev, "could not setup required "
		    "transfers, %s\n", usb2_errstr(err));
		goto detach;
	}
	/* start Mass Storage State Machine */

	mtx_lock(&sc->sc_mtx);
	ustorage_fs_transfer_start(sc, USTORAGE_FS_T_BBB_COMMAND);
	mtx_unlock(&sc->sc_mtx);

	return (0);			/* success */

detach:
	ustorage_fs_detach(dev);
	return (ENXIO);			/* failure */
}

static int
ustorage_fs_detach(device_t dev)
{
	struct ustorage_fs_softc *sc = device_get_softc(dev);

	/* teardown our statemachine */

	usb2_transfer_unsetup(sc->sc_xfer, USTORAGE_FS_T_BBB_MAX);

	mtx_destroy(&sc->sc_mtx);

	return (0);			/* success */
}

static int
ustorage_fs_suspend(device_t dev)
{
	device_printf(dev, "suspending\n");
	return (0);			/* success */
}

static int
ustorage_fs_resume(device_t dev)
{
	device_printf(dev, "resuming\n");
	return (0);			/* success */
}

static int
ustorage_fs_shutdown(device_t dev)
{
	return (0);			/* success */
}

/*
 * Generic functions to handle transfers
 */

static void
ustorage_fs_transfer_start(struct ustorage_fs_softc *sc, uint8_t xfer_index)
{
	if (sc->sc_xfer[xfer_index]) {
		sc->sc_last_xfer_index = xfer_index;
		usb2_transfer_start(sc->sc_xfer[xfer_index]);
	}
	return;
}

static void
ustorage_fs_transfer_stop(struct ustorage_fs_softc *sc)
{
	usb2_transfer_stop(sc->sc_xfer[sc->sc_last_xfer_index]);
	mtx_unlock(&sc->sc_mtx);
	usb2_transfer_drain(sc->sc_xfer[sc->sc_last_xfer_index]);
	mtx_lock(&sc->sc_mtx);
	return;
}

static int
ustorage_fs_handle_request(device_t dev,
    const void *preq, void **pptr, uint16_t *plen,
    uint16_t offset, uint8_t is_complete)
{
	struct ustorage_fs_softc *sc = device_get_softc(dev);
	const struct usb2_device_request *req = preq;

	if (!is_complete) {
		if (req->bRequest == UR_BBB_RESET) {
			*plen = 0;
			mtx_lock(&sc->sc_mtx);
			ustorage_fs_transfer_stop(sc);
			sc->sc_transfer.data_error = 1;
			ustorage_fs_transfer_start(sc,
			    USTORAGE_FS_T_BBB_COMMAND);
			mtx_unlock(&sc->sc_mtx);
			return (0);
		} else if (req->bRequest == UR_BBB_GET_MAX_LUN) {
			if (offset == 0) {
				*plen = 1;
				*pptr = &sc->sc_last_lun;
			} else {
				*plen = 0;
			}
			return (0);
		}
	}
	return (ENXIO);			/* use builtin handler */
}

static void
ustorage_fs_t_bbb_command_callback(struct usb2_xfer *xfer)
{
	struct ustorage_fs_softc *sc = xfer->priv_sc;
	uint32_t tag;
	uint8_t error = 0;

	DPRINTF("\n");

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		tag = UGETDW(sc->sc_cbw.dCBWSignature);

		if (tag != CBWSIGNATURE) {
			/* do nothing */
			DPRINTF("invalid signature 0x%08x\n", tag);
			break;
		}
		tag = UGETDW(sc->sc_cbw.dCBWTag);

		/* echo back tag */
		USETDW(sc->sc_csw.dCSWTag, tag);

		/* reset status */
		sc->sc_csw.bCSWStatus = 0;

		/* reset data offset, data length and data remainder */
		sc->sc_transfer.offset = 0;
		sc->sc_transfer.data_rem =
		    UGETDW(sc->sc_cbw.dCBWDataTransferLength);

		/* reset data flags */
		sc->sc_transfer.data_short = 0;

		/* extract LUN */
		sc->sc_transfer.lun = sc->sc_cbw.bCBWLUN;

		if (sc->sc_transfer.data_rem == 0) {
			sc->sc_transfer.cbw_dir = DIR_NONE;
		} else {
			if (sc->sc_cbw.bCBWFlags & CBWFLAGS_IN) {
				sc->sc_transfer.cbw_dir = DIR_WRITE;
			} else {
				sc->sc_transfer.cbw_dir = DIR_READ;
			}
		}

		sc->sc_transfer.cmd_len = sc->sc_cbw.bCDBLength;
		if ((sc->sc_transfer.cmd_len > sizeof(sc->sc_cbw.CBWCDB)) ||
		    (sc->sc_transfer.cmd_len == 0)) {
			/* just halt - this is invalid */
			DPRINTF("invalid command length %d bytes\n",
			    sc->sc_transfer.cmd_len);
			break;
		}
		bcopy(sc->sc_cbw.CBWCDB, sc->sc_transfer.cmd_data,
		    sc->sc_transfer.cmd_len);

		bzero(sc->sc_cbw.CBWCDB + sc->sc_transfer.cmd_len,
		    sizeof(sc->sc_cbw.CBWCDB) - sc->sc_transfer.cmd_len);

		error = ustorage_fs_do_cmd(sc);
		if (error) {
			/* got an error */
			DPRINTF("command failed\n");
			break;
		}
		if ((sc->sc_transfer.data_rem > 0) &&
		    (sc->sc_transfer.cbw_dir != sc->sc_transfer.cmd_dir)) {
			/* contradicting data transfer direction */
			error = 1;
			DPRINTF("data direction mismatch\n");
			break;
		}
		switch (sc->sc_transfer.cbw_dir) {
		case DIR_READ:
			ustorage_fs_transfer_start(sc, USTORAGE_FS_T_BBB_DATA_READ);
			break;
		case DIR_WRITE:
			ustorage_fs_transfer_start(sc, USTORAGE_FS_T_BBB_DATA_WRITE);
			break;
		default:
			ustorage_fs_transfer_start(sc,
			    USTORAGE_FS_T_BBB_STATUS);
			break;
		}
		break;

	case USB_ST_SETUP:
tr_setup:
		if (sc->sc_transfer.data_error) {
			sc->sc_transfer.data_error = 0;
			xfer->flags.stall_pipe = 1;
			DPRINTF("stall pipe\n");
		} else {
			xfer->flags.stall_pipe = 0;
		}

		xfer->frlengths[0] = sizeof(sc->sc_cbw);
		usb2_set_frame_data(xfer, &sc->sc_cbw, 0);
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		DPRINTF("error\n");
		if (xfer->error == USB_ERR_CANCELLED) {
			break;
		}
		/* If the pipe is already stalled, don't do another stall */
		if (!xfer->pipe->is_stalled) {
			sc->sc_transfer.data_error = 1;
		}
		/* try again */
		goto tr_setup;
	}
	if (error) {
		if (sc->sc_csw.bCSWStatus == 0) {
			/* set some default error code */
			sc->sc_csw.bCSWStatus = CSWSTATUS_FAILED;
		}
		if (sc->sc_transfer.cbw_dir == DIR_READ) {
			/* dump all data */
			ustorage_fs_transfer_start(sc,
			    USTORAGE_FS_T_BBB_DATA_DUMP);
			return;
		}
		if (sc->sc_transfer.cbw_dir == DIR_WRITE) {
			/* need to stall before status */
			sc->sc_transfer.data_error = 1;
		}
		ustorage_fs_transfer_start(sc, USTORAGE_FS_T_BBB_STATUS);
	}
	return;
}

static void
ustorage_fs_t_bbb_data_dump_callback(struct usb2_xfer *xfer)
{
	struct ustorage_fs_softc *sc = xfer->priv_sc;
	uint32_t max_bulk = xfer->max_data_length;

	DPRINTF("\n");

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		sc->sc_transfer.data_rem -= xfer->actlen;
		sc->sc_transfer.offset += xfer->actlen;

		if ((xfer->actlen != xfer->sumlen) ||
		    (sc->sc_transfer.data_rem == 0)) {
			/* short transfer or end of data */
			ustorage_fs_transfer_start(sc,
			    USTORAGE_FS_T_BBB_STATUS);
			break;
		}
		/* Fallthrough */

	case USB_ST_SETUP:
tr_setup:
		if (max_bulk > sc->sc_transfer.data_rem) {
			max_bulk = sc->sc_transfer.data_rem;
		}
		if (sc->sc_transfer.data_error) {
			sc->sc_transfer.data_error = 0;
			xfer->flags.stall_pipe = 1;
		} else {
			xfer->flags.stall_pipe = 0;
		}
		xfer->frlengths[0] = max_bulk;
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			break;
		}
		/*
		 * If the pipe is already stalled, don't do another stall:
		 */
		if (!xfer->pipe->is_stalled) {
			sc->sc_transfer.data_error = 1;
		}
		/* try again */
		goto tr_setup;
	}
	return;
}

static void
ustorage_fs_t_bbb_data_read_callback(struct usb2_xfer *xfer)
{
	struct ustorage_fs_softc *sc = xfer->priv_sc;
	uint32_t max_bulk = xfer->max_data_length;

	DPRINTF("\n");

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		sc->sc_transfer.data_rem -= xfer->actlen;
		sc->sc_transfer.data_ptr += xfer->actlen;
		sc->sc_transfer.offset += xfer->actlen;

		if ((xfer->actlen != xfer->sumlen) ||
		    (sc->sc_transfer.data_rem == 0)) {
			/* short transfer or end of data */
			ustorage_fs_transfer_start(sc,
			    USTORAGE_FS_T_BBB_STATUS);
			break;
		}
		/* Fallthrough */

	case USB_ST_SETUP:
tr_setup:
		if (max_bulk > sc->sc_transfer.data_rem) {
			max_bulk = sc->sc_transfer.data_rem;
		}
		if (sc->sc_transfer.data_error) {
			sc->sc_transfer.data_error = 0;
			xfer->flags.stall_pipe = 1;
		} else {
			xfer->flags.stall_pipe = 0;
		}

		xfer->frlengths[0] = max_bulk;
		usb2_set_frame_data(xfer, sc->sc_transfer.data_ptr, 0);
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			break;
		}
		/* If the pipe is already stalled, don't do another stall */
		if (!xfer->pipe->is_stalled) {
			sc->sc_transfer.data_error = 1;
		}
		/* try again */
		goto tr_setup;
	}
	return;
}

static void
ustorage_fs_t_bbb_data_write_callback(struct usb2_xfer *xfer)
{
	struct ustorage_fs_softc *sc = xfer->priv_sc;
	uint32_t max_bulk = xfer->max_data_length;

	DPRINTF("\n");

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		sc->sc_transfer.data_rem -= xfer->actlen;
		sc->sc_transfer.data_ptr += xfer->actlen;
		sc->sc_transfer.offset += xfer->actlen;

		if ((xfer->actlen != xfer->sumlen) ||
		    (sc->sc_transfer.data_rem == 0)) {
			/* short transfer or end of data */
			ustorage_fs_transfer_start(sc,
			    USTORAGE_FS_T_BBB_STATUS);
			break;
		}
	case USB_ST_SETUP:
tr_setup:
		if (max_bulk >= sc->sc_transfer.data_rem) {
			max_bulk = sc->sc_transfer.data_rem;
			if (sc->sc_transfer.data_short) {
				xfer->flags.force_short_xfer = 1;
			} else {
				xfer->flags.force_short_xfer = 0;
			}
		} else {
			xfer->flags.force_short_xfer = 0;
		}

		if (sc->sc_transfer.data_error) {
			sc->sc_transfer.data_error = 0;
			xfer->flags.stall_pipe = 1;
		} else {
			xfer->flags.stall_pipe = 0;
		}

		xfer->frlengths[0] = max_bulk;
		usb2_set_frame_data(xfer, sc->sc_transfer.data_ptr, 0);
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			break;
		}
		/*
		 * If the pipe is already stalled, don't do another
		 * stall
		 */
		if (!xfer->pipe->is_stalled) {
			sc->sc_transfer.data_error = 1;
		}
		/* try again */
		goto tr_setup;
	}
	return;
}

static void
ustorage_fs_t_bbb_status_callback(struct usb2_xfer *xfer)
{
	struct ustorage_fs_softc *sc = xfer->priv_sc;

	DPRINTF("\n");

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		ustorage_fs_transfer_start(sc, USTORAGE_FS_T_BBB_COMMAND);
		break;

	case USB_ST_SETUP:
tr_setup:
		USETDW(sc->sc_csw.dCSWSignature, CSWSIGNATURE);
		USETDW(sc->sc_csw.dCSWDataResidue, sc->sc_transfer.data_rem);

		if (sc->sc_transfer.data_error) {
			sc->sc_transfer.data_error = 0;
			xfer->flags.stall_pipe = 1;
		} else {
			xfer->flags.stall_pipe = 0;
		}

		xfer->frlengths[0] = sizeof(sc->sc_csw);
		usb2_set_frame_data(xfer, &sc->sc_csw, 0);
		usb2_start_hardware(xfer);
		break;

	default:
		if (xfer->error == USB_ERR_CANCELLED) {
			break;
		}
		/* If the pipe is already stalled, don't do another stall */
		if (!xfer->pipe->is_stalled) {
			sc->sc_transfer.data_error = 1;
		}
		/* try again */
		goto tr_setup;
	}
	return;
}

/* SCSI commands that we recognize */
#define	SC_FORMAT_UNIT			0x04
#define	SC_INQUIRY			0x12
#define	SC_MODE_SELECT_6		0x15
#define	SC_MODE_SELECT_10		0x55
#define	SC_MODE_SENSE_6			0x1a
#define	SC_MODE_SENSE_10		0x5a
#define	SC_PREVENT_ALLOW_MEDIUM_REMOVAL	0x1e
#define	SC_READ_6			0x08
#define	SC_READ_10			0x28
#define	SC_READ_12			0xa8
#define	SC_READ_CAPACITY		0x25
#define	SC_READ_FORMAT_CAPACITIES	0x23
#define	SC_RELEASE			0x17
#define	SC_REQUEST_SENSE		0x03
#define	SC_RESERVE			0x16
#define	SC_SEND_DIAGNOSTIC		0x1d
#define	SC_START_STOP_UNIT		0x1b
#define	SC_SYNCHRONIZE_CACHE		0x35
#define	SC_TEST_UNIT_READY		0x00
#define	SC_VERIFY			0x2f
#define	SC_WRITE_6			0x0a
#define	SC_WRITE_10			0x2a
#define	SC_WRITE_12			0xaa

/* SCSI Sense Key/Additional Sense Code/ASC Qualifier values */
#define	SS_NO_SENSE				0
#define	SS_COMMUNICATION_FAILURE		0x040800
#define	SS_INVALID_COMMAND			0x052000
#define	SS_INVALID_FIELD_IN_CDB			0x052400
#define	SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE	0x052100
#define	SS_LOGICAL_UNIT_NOT_SUPPORTED		0x052500
#define	SS_MEDIUM_NOT_PRESENT			0x023a00
#define	SS_MEDIUM_REMOVAL_PREVENTED		0x055302
#define	SS_NOT_READY_TO_READY_TRANSITION	0x062800
#define	SS_RESET_OCCURRED			0x062900
#define	SS_SAVING_PARAMETERS_NOT_SUPPORTED	0x053900
#define	SS_UNRECOVERED_READ_ERROR		0x031100
#define	SS_WRITE_ERROR				0x030c02
#define	SS_WRITE_PROTECTED			0x072700

#define	SK(x)		((uint8_t) ((x) >> 16))	/* Sense Key byte, etc. */
#define	ASC(x)		((uint8_t) ((x) >> 8))
#define	ASCQ(x)		((uint8_t) (x))

/* Routines for unaligned data access */

static uint16_t
get_be16(uint8_t *buf)
{
	return ((uint16_t)buf[0] << 8) | ((uint16_t)buf[1]);
}

static uint32_t
get_be32(uint8_t *buf)
{
	return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
	((uint32_t)buf[2] << 8) | ((uint32_t)buf[3]);
}

static void
put_be16(uint8_t *buf, uint16_t val)
{
	buf[0] = val >> 8;
	buf[1] = val;
}

static void
put_be32(uint8_t *buf, uint32_t val)
{
	buf[0] = val >> 24;
	buf[1] = val >> 16;
	buf[2] = val >> 8;
	buf[3] = val & 0xff;
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_verify
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_verify(struct ustorage_fs_softc *sc)
{
	struct ustorage_fs_lun *currlun = sc->sc_transfer.currlun;
	uint32_t lba;
	uint32_t vlen;
	uint64_t file_offset;
	uint64_t amount_left;

	/*
	 * Get the starting Logical Block Address
	 */
	lba = get_be32(&sc->sc_transfer.cmd_data[2]);

	/*
	 * We allow DPO (Disable Page Out = don't save data in the cache)
	 * but we don't implement it.
	 */
	if ((sc->sc_transfer.cmd_data[1] & ~0x10) != 0) {
		currlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return (1);
	}
	vlen = get_be16(&sc->sc_transfer.cmd_data[7]);
	if (vlen == 0) {
		goto done;
	}
	/* No default reply */

	/* Prepare to carry out the file verify */
	amount_left = vlen;
	amount_left <<= 9;
	file_offset = lba;
	file_offset <<= 9;

	/* Range check */
	vlen += lba;

	if ((vlen < lba) ||
	    (vlen > currlun->num_sectors) ||
	    (lba >= currlun->num_sectors)) {
		currlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return (1);
	}
	/* XXX TODO: verify that data is readable */
done:
	return (ustorage_fs_min_len(sc, 0, 0 - 1));
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_inquiry
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_inquiry(struct ustorage_fs_softc *sc)
{
	uint8_t *buf = sc->sc_transfer.data_ptr;
	static const char vendor_id[] = "FreeBSD ";
	static const char product_id[] = "File-Stor Gadget";

	struct ustorage_fs_lun *currlun = sc->sc_transfer.currlun;

	if (!sc->sc_transfer.currlun) {
		/* Unsupported LUNs are okay */
		memset(buf, 0, 36);
		buf[0] = 0x7f;
		/* Unsupported, no device - type */
		return (ustorage_fs_min_len(sc, 36, 0 - 1));
	}
	memset(buf, 0, 8);
	/* Non - removable, direct - access device */
	if (currlun->removable)
		buf[1] = 0x80;
	buf[2] = 2;
	/* ANSI SCSI level 2 */
	buf[3] = 2;
	/* SCSI - 2 INQUIRY data format */
	buf[4] = 31;
	/* Additional length */
	/* No special options */
	/*
	 * NOTE: We are writing an extra zero here, that is not
	 * transferred to the peer:
	 */
	snprintf(buf + 8, 28 + 1, "%-8s%-16s%04x", vendor_id, product_id,
	    USTORAGE_FS_RELEASE);
	return (ustorage_fs_min_len(sc, 36, 0 - 1));
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_request_sense
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_request_sense(struct ustorage_fs_softc *sc)
{
	uint8_t *buf = sc->sc_transfer.data_ptr;
	struct ustorage_fs_lun *currlun = sc->sc_transfer.currlun;
	uint32_t sd;
	uint32_t sdinfo;
	uint8_t valid;

	/*
	 * From the SCSI-2 spec., section 7.9 (Unit attention condition):
	 *
	 * If a REQUEST SENSE command is received from an initiator
	 * with a pending unit attention condition (before the target
	 * generates the contingent allegiance condition), then the
	 * target shall either:
	 *   a) report any pending sense data and preserve the unit
	 *	attention condition on the logical unit, or,
	 *   b) report the unit attention condition, may discard any
	 *	pending sense data, and clear the unit attention
	 *	condition on the logical unit for that initiator.
	 *
	 * FSG normally uses option a); enable this code to use option b).
	 */
#if 0
	if (currlun && currlun->unit_attention_data != SS_NO_SENSE) {
		currlun->sense_data = currlun->unit_attention_data;
		currlun->unit_attention_data = SS_NO_SENSE;
	}
#endif

	if (!currlun) {
		/* Unsupported LUNs are okay */
		sd = SS_LOGICAL_UNIT_NOT_SUPPORTED;
		sdinfo = 0;
		valid = 0;
	} else {
		sd = currlun->sense_data;
		sdinfo = currlun->sense_data_info;
		valid = currlun->info_valid << 7;
		currlun->sense_data = SS_NO_SENSE;
		currlun->sense_data_info = 0;
		currlun->info_valid = 0;
	}

	memset(buf, 0, 18);
	buf[0] = valid | 0x70;
	/* Valid, current error */
	buf[2] = SK(sd);
	put_be32(&buf[3], sdinfo);
	/* Sense information */
	buf[7] = 18 - 8;
	/* Additional sense length */
	buf[12] = ASC(sd);
	buf[13] = ASCQ(sd);
	return (ustorage_fs_min_len(sc, 18, 0 - 1));
}


/*------------------------------------------------------------------------*
 *	ustorage_fs_read_capacity
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_read_capacity(struct ustorage_fs_softc *sc)
{
	uint8_t *buf = sc->sc_transfer.data_ptr;
	struct ustorage_fs_lun *currlun = sc->sc_transfer.currlun;
	uint32_t lba = get_be32(&sc->sc_transfer.cmd_data[2]);
	uint8_t pmi = sc->sc_transfer.cmd_data[8];

	/* Check the PMI and LBA fields */
	if ((pmi > 1) || ((pmi == 0) && (lba != 0))) {
		currlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return (1);
	}
	put_be32(&buf[0], currlun->num_sectors - 1);
	/* Max logical block */
	put_be32(&buf[4], 512);
	/* Block length */
	return (ustorage_fs_min_len(sc, 8, 0 - 1));
}


/*------------------------------------------------------------------------*
 *	ustorage_fs_mode_sense
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_mode_sense(struct ustorage_fs_softc *sc)
{
	uint8_t *buf = sc->sc_transfer.data_ptr;
	struct ustorage_fs_lun *currlun = sc->sc_transfer.currlun;
	uint8_t *buf0;
	uint16_t len;
	uint16_t limit;
	uint8_t mscmnd = sc->sc_transfer.cmd_data[0];
	uint8_t pc;
	uint8_t page_code;
	uint8_t changeable_values;
	uint8_t all_pages;

	buf0 = buf;

	if ((sc->sc_transfer.cmd_data[1] & ~0x08) != 0) {
		/* Mask away DBD */
		currlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return (1);
	}
	pc = sc->sc_transfer.cmd_data[2] >> 6;
	page_code = sc->sc_transfer.cmd_data[2] & 0x3f;
	if (pc == 3) {
		currlun->sense_data = SS_SAVING_PARAMETERS_NOT_SUPPORTED;
		return (1);
	}
	changeable_values = (pc == 1);
	all_pages = (page_code == 0x3f);

	/*
	 * Write the mode parameter header.  Fixed values are: default
	 * medium type, no cache control (DPOFUA), and no block descriptors.
	 * The only variable value is the WriteProtect bit.  We will fill in
	 * the mode data length later.
	 */
	memset(buf, 0, 8);
	if (mscmnd == SC_MODE_SENSE_6) {
		buf[2] = (currlun->read_only ? 0x80 : 0x00);
		/* WP, DPOFUA */
		buf += 4;
		limit = 255;
	} else {
		/* SC_MODE_SENSE_10 */
		buf[3] = (currlun->read_only ? 0x80 : 0x00);
		/* WP, DPOFUA */
		buf += 8;
		limit = 65535;
		/* Should really be mod_data.buflen */
	}

	/* No block descriptors */

	/*
	 * The mode pages, in numerical order.
	 */
	if ((page_code == 0x08) || all_pages) {
		buf[0] = 0x08;
		/* Page code */
		buf[1] = 10;
		/* Page length */
		memset(buf + 2, 0, 10);
		/* None of the fields are changeable */

		if (!changeable_values) {
			buf[2] = 0x04;
			/* Write cache enable, */
			/* Read cache not disabled */
			/* No cache retention priorities */
			put_be16(&buf[4], 0xffff);
			/* Don 't disable prefetch */
			/* Minimum prefetch = 0 */
			put_be16(&buf[8], 0xffff);
			/* Maximum prefetch */
			put_be16(&buf[10], 0xffff);
			/* Maximum prefetch ceiling */
		}
		buf += 12;
	}
	/*
	 * Check that a valid page was requested and the mode data length
	 * isn't too long.
	 */
	len = buf - buf0;
	if (len > limit) {
		currlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return (1);
	}
	/* Store the mode data length */
	if (mscmnd == SC_MODE_SENSE_6)
		buf0[0] = len - 1;
	else
		put_be16(buf0, len - 2);
	return (ustorage_fs_min_len(sc, len, 0 - 1));
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_start_stop
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_start_stop(struct ustorage_fs_softc *sc)
{
	struct ustorage_fs_lun *currlun = sc->sc_transfer.currlun;
	uint8_t loej;
	uint8_t start;
	uint8_t immed;

	if (!currlun->removable) {
		currlun->sense_data = SS_INVALID_COMMAND;
		return (1);
	}
	immed = sc->sc_transfer.cmd_data[1] & 0x01;
	loej = sc->sc_transfer.cmd_data[4] & 0x02;
	start = sc->sc_transfer.cmd_data[4] & 0x01;

	if (immed || loej || start) {
		/* compile fix */
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_prevent_allow
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_prevent_allow(struct ustorage_fs_softc *sc)
{
	struct ustorage_fs_lun *currlun = sc->sc_transfer.currlun;
	uint8_t prevent;

	if (!currlun->removable) {
		currlun->sense_data = SS_INVALID_COMMAND;
		return (1);
	}
	prevent = sc->sc_transfer.cmd_data[4] & 0x01;
	if ((sc->sc_transfer.cmd_data[4] & ~0x01) != 0) {
		/* Mask away Prevent */
		currlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return (1);
	}
	if (currlun->prevent_medium_removal && !prevent) {
		//fsync_sub(currlun);
	}
	currlun->prevent_medium_removal = prevent;
	return (0);
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_read_format_capacities
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_read_format_capacities(struct ustorage_fs_softc *sc)
{
	uint8_t *buf = sc->sc_transfer.data_ptr;
	struct ustorage_fs_lun *currlun = sc->sc_transfer.currlun;

	buf[0] = buf[1] = buf[2] = 0;
	buf[3] = 8;
	/* Only the Current / Maximum Capacity Descriptor */
	buf += 4;

	put_be32(&buf[0], currlun->num_sectors);
	/* Number of blocks */
	put_be32(&buf[4], 512);
	/* Block length */
	buf[4] = 0x02;
	/* Current capacity */
	return (ustorage_fs_min_len(sc, 12, 0 - 1));
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_mode_select
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_mode_select(struct ustorage_fs_softc *sc)
{
	struct ustorage_fs_lun *currlun = sc->sc_transfer.currlun;

	/* We don't support MODE SELECT */
	currlun->sense_data = SS_INVALID_COMMAND;
	return (1);
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_synchronize_cache
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_synchronize_cache(struct ustorage_fs_softc *sc)
{
	struct ustorage_fs_lun *currlun = sc->sc_transfer.currlun;
	uint8_t rc;

	/*
	 * We ignore the requested LBA and write out all dirty data buffers.
	 */
	rc = 0;
	if (rc) {
		currlun->sense_data = SS_WRITE_ERROR;
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_read - read data from disk
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_read(struct ustorage_fs_softc *sc)
{
	struct ustorage_fs_lun *currlun = sc->sc_transfer.currlun;
	uint64_t file_offset;
	uint32_t lba;
	uint32_t len;

	/*
	 * Get the starting Logical Block Address and check that it's not
	 * too big
	 */
	if (sc->sc_transfer.cmd_data[0] == SC_READ_6) {
		lba = (sc->sc_transfer.cmd_data[1] << 16) |
		    get_be16(&sc->sc_transfer.cmd_data[2]);
	} else {
		lba = get_be32(&sc->sc_transfer.cmd_data[2]);

		/*
		 * We allow DPO (Disable Page Out = don't save data in the
		 * cache) and FUA (Force Unit Access = don't read from the
		 * cache), but we don't implement them.
		 */
		if ((sc->sc_transfer.cmd_data[1] & ~0x18) != 0) {
			currlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return (1);
		}
	}
	len = sc->sc_transfer.data_rem >> 9;
	len += lba;

	if ((len < lba) ||
	    (len > currlun->num_sectors) ||
	    (lba >= currlun->num_sectors)) {
		currlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return (1);
	}
	file_offset = lba;
	file_offset <<= 9;

	sc->sc_transfer.data_ptr =
	    USB_ADD_BYTES(currlun->memory_image, (uint32_t)file_offset);

	return (0);
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_write - write data to disk
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_write(struct ustorage_fs_softc *sc)
{
	struct ustorage_fs_lun *currlun = sc->sc_transfer.currlun;
	uint64_t file_offset;
	uint32_t lba;
	uint32_t len;

	if (currlun->read_only) {
		currlun->sense_data = SS_WRITE_PROTECTED;
		return (1);
	}
	/* XXX clear SYNC */

	/*
	 * Get the starting Logical Block Address and check that it's not
	 * too big.
	 */
	if (sc->sc_transfer.cmd_data[0] == SC_WRITE_6)
		lba = (sc->sc_transfer.cmd_data[1] << 16) |
		    get_be16(&sc->sc_transfer.cmd_data[2]);
	else {
		lba = get_be32(&sc->sc_transfer.cmd_data[2]);

		/*
		 * We allow DPO (Disable Page Out = don't save data in the
		 * cache) and FUA (Force Unit Access = write directly to the
		 * medium).  We don't implement DPO; we implement FUA by
		 * performing synchronous output.
		 */
		if ((sc->sc_transfer.cmd_data[1] & ~0x18) != 0) {
			currlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return (1);
		}
		if (sc->sc_transfer.cmd_data[1] & 0x08) {
			/* FUA */
			/* XXX set SYNC flag here */
		}
	}

	len = sc->sc_transfer.data_rem >> 9;
	len += lba;

	if ((len < lba) ||
	    (len > currlun->num_sectors) ||
	    (lba >= currlun->num_sectors)) {
		currlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return (1);
	}
	file_offset = lba;
	file_offset <<= 9;

	sc->sc_transfer.data_ptr =
	    USB_ADD_BYTES(currlun->memory_image, (uint32_t)file_offset);

	return (0);
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_min_len
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_min_len(struct ustorage_fs_softc *sc, uint32_t len, uint32_t mask)
{
	if (len != sc->sc_transfer.data_rem) {

		if (sc->sc_transfer.cbw_dir == DIR_READ) {
			/*
			 * there must be something wrong about this SCSI
			 * command
			 */
			sc->sc_csw.bCSWStatus = CSWSTATUS_PHASE;
			return (1);
		}
		/* compute the minimum length */

		if (sc->sc_transfer.data_rem > len) {
			/* data ends prematurely */
			sc->sc_transfer.data_rem = len;
			sc->sc_transfer.data_short = 1;
		}
		/* check length alignment */

		if (sc->sc_transfer.data_rem & ~mask) {
			/* data ends prematurely */
			sc->sc_transfer.data_rem &= mask;
			sc->sc_transfer.data_short = 1;
		}
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_check_cmd - check command routine
 *
 * Check whether the command is properly formed and whether its data
 * size and direction agree with the values we already have.
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_check_cmd(struct ustorage_fs_softc *sc, uint8_t min_cmd_size,
    uint16_t mask, uint8_t needs_medium)
{
	struct ustorage_fs_lun *currlun;
	uint8_t lun = (sc->sc_transfer.cmd_data[1] >> 5);
	uint8_t i;

	/* Verify the length of the command itself */
	if (min_cmd_size > sc->sc_transfer.cmd_len) {
		DPRINTF("%u > %u\n",
		    min_cmd_size, sc->sc_transfer.cmd_len);
		sc->sc_csw.bCSWStatus = CSWSTATUS_PHASE;
		return (1);
	}
	/* Mask away the LUN */
	sc->sc_transfer.cmd_data[1] &= 0x1f;

	/* Check if LUN is correct */
	if (lun != sc->sc_transfer.lun) {

	}
	/* Check the LUN */
	if (sc->sc_transfer.lun <= sc->sc_last_lun) {
		sc->sc_transfer.currlun = currlun =
		    sc->sc_lun + sc->sc_transfer.lun;
		if (sc->sc_transfer.cmd_data[0] != SC_REQUEST_SENSE) {
			currlun->sense_data = SS_NO_SENSE;
			currlun->sense_data_info = 0;
			currlun->info_valid = 0;
		}
		/*
		 * If a unit attention condition exists, only INQUIRY
		 * and REQUEST SENSE commands are allowed. Anything
		 * else must fail!
		 */
		if ((currlun->unit_attention_data != SS_NO_SENSE) &&
		    (sc->sc_transfer.cmd_data[0] != SC_INQUIRY) &&
		    (sc->sc_transfer.cmd_data[0] != SC_REQUEST_SENSE)) {
			currlun->sense_data = currlun->unit_attention_data;
			currlun->unit_attention_data = SS_NO_SENSE;
			return (1);
		}
	} else {
		sc->sc_transfer.currlun = currlun = NULL;

		/*
		 * INQUIRY and REQUEST SENSE commands are explicitly allowed
		 * to use unsupported LUNs; all others may not.
		 */
		if ((sc->sc_transfer.cmd_data[0] != SC_INQUIRY) &&
		    (sc->sc_transfer.cmd_data[0] != SC_REQUEST_SENSE)) {
			return (1);
		}
	}

	/*
	 * Check that only command bytes listed in the mask are
	 * non-zero.
	 */
	for (i = 0; i != min_cmd_size; i++) {
		if (sc->sc_transfer.cmd_data[i] && !(mask & (1 << i))) {
			if (currlun) {
				currlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			}
			return (1);
		}
	}

	/*
	 * If the medium isn't mounted and the command needs to access
	 * it, return an error.
	 */
	if (currlun && (!currlun->memory_image) && needs_medium) {
		currlun->sense_data = SS_MEDIUM_NOT_PRESENT;
		return (1);
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	ustorage_fs_do_cmd - do command
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
ustorage_fs_do_cmd(struct ustorage_fs_softc *sc)
{
	uint8_t error = 1;
	uint8_t i;

	/* set default data transfer pointer */
	sc->sc_transfer.data_ptr = sc->sc_qdata;

	DPRINTF("cmd_data[0]=0x%02x, data_rem=0x%08x\n",
	    sc->sc_transfer.cmd_data[0], sc->sc_transfer.data_rem);

	switch (sc->sc_transfer.cmd_data[0]) {
	case SC_INQUIRY:
		sc->sc_transfer.cmd_dir = DIR_WRITE;
		error = ustorage_fs_min_len(sc, sc->sc_transfer.cmd_data[4], 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 6,
		    (1 << 4) | 1, 0);
		if (error) {
			break;
		}
		error = ustorage_fs_inquiry(sc);

		break;

	case SC_MODE_SELECT_6:
		sc->sc_transfer.cmd_dir = DIR_READ;
		error = ustorage_fs_min_len(sc, sc->sc_transfer.cmd_data[4], 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 6,
		    (1 << 1) | (1 << 4) | 1, 0);
		if (error) {
			break;
		}
		error = ustorage_fs_mode_select(sc);

		break;

	case SC_MODE_SELECT_10:
		sc->sc_transfer.cmd_dir = DIR_READ;
		error = ustorage_fs_min_len(sc,
		    get_be16(&sc->sc_transfer.cmd_data[7]), 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 10,
		    (1 << 1) | (3 << 7) | 1, 0);
		if (error) {
			break;
		}
		error = ustorage_fs_mode_select(sc);

		break;

	case SC_MODE_SENSE_6:
		sc->sc_transfer.cmd_dir = DIR_WRITE;
		error = ustorage_fs_min_len(sc, sc->sc_transfer.cmd_data[4], 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 6,
		    (1 << 1) | (1 << 2) | (1 << 4) | 1, 0);
		if (error) {
			break;
		}
		error = ustorage_fs_mode_sense(sc);

		break;

	case SC_MODE_SENSE_10:
		sc->sc_transfer.cmd_dir = DIR_WRITE;
		error = ustorage_fs_min_len(sc,
		    get_be16(&sc->sc_transfer.cmd_data[7]), 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 10,
		    (1 << 1) | (1 << 2) | (3 << 7) | 1, 0);
		if (error) {
			break;
		}
		error = ustorage_fs_mode_sense(sc);

		break;

	case SC_PREVENT_ALLOW_MEDIUM_REMOVAL:
		error = ustorage_fs_min_len(sc, 0, 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 6,
		    (1 << 4) | 1, 0);
		if (error) {
			break;
		}
		error = ustorage_fs_prevent_allow(sc);

		break;

	case SC_READ_6:
		i = sc->sc_transfer.cmd_data[4];
		sc->sc_transfer.cmd_dir = DIR_WRITE;
		error = ustorage_fs_min_len(sc,
		    ((i == 0) ? 256 : i) << 9, 0 - (1 << 9));
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 6,
		    (7 << 1) | (1 << 4) | 1, 1);
		if (error) {
			break;
		}
		error = ustorage_fs_read(sc);

		break;

	case SC_READ_10:
		sc->sc_transfer.cmd_dir = DIR_WRITE;
		error = ustorage_fs_min_len(sc,
		    get_be16(&sc->sc_transfer.cmd_data[7]) << 9, 0 - (1 << 9));
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 10,
		    (1 << 1) | (0xf << 2) | (3 << 7) | 1, 1);
		if (error) {
			break;
		}
		error = ustorage_fs_read(sc);

		break;

	case SC_READ_12:
		sc->sc_transfer.cmd_dir = DIR_WRITE;
		error = ustorage_fs_min_len(sc,
		    get_be32(&sc->sc_transfer.cmd_data[6]) << 9, 0 - (1 << 9));
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 12,
		    (1 << 1) | (0xf << 2) | (0xf << 6) | 1, 1);
		if (error) {
			break;
		}
		error = ustorage_fs_read(sc);

		break;

	case SC_READ_CAPACITY:
		sc->sc_transfer.cmd_dir = DIR_WRITE;
		error = ustorage_fs_check_cmd(sc, 10,
		    (0xf << 2) | (1 << 8) | 1, 1);
		if (error) {
			break;
		}
		error = ustorage_fs_read_capacity(sc);

		break;

	case SC_READ_FORMAT_CAPACITIES:
		sc->sc_transfer.cmd_dir = DIR_WRITE;
		error = ustorage_fs_min_len(sc,
		    get_be16(&sc->sc_transfer.cmd_data[7]), 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 10,
		    (3 << 7) | 1, 1);
		if (error) {
			break;
		}
		error = ustorage_fs_read_format_capacities(sc);

		break;

	case SC_REQUEST_SENSE:
		sc->sc_transfer.cmd_dir = DIR_WRITE;
		error = ustorage_fs_min_len(sc, sc->sc_transfer.cmd_data[4], 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 6,
		    (1 << 4) | 1, 0);
		if (error) {
			break;
		}
		error = ustorage_fs_request_sense(sc);

		break;

	case SC_START_STOP_UNIT:
		error = ustorage_fs_min_len(sc, 0, 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 6,
		    (1 << 1) | (1 << 4) | 1, 0);
		if (error) {
			break;
		}
		error = ustorage_fs_start_stop(sc);

		break;

	case SC_SYNCHRONIZE_CACHE:
		error = ustorage_fs_min_len(sc, 0, 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 10,
		    (0xf << 2) | (3 << 7) | 1, 1);
		if (error) {
			break;
		}
		error = ustorage_fs_synchronize_cache(sc);

		break;

	case SC_TEST_UNIT_READY:
		error = ustorage_fs_min_len(sc, 0, 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 6,
		    0 | 1, 1);
		break;

		/*
		 * Although optional, this command is used by MS-Windows.
		 * We support a minimal version: BytChk must be 0.
		 */
	case SC_VERIFY:
		error = ustorage_fs_min_len(sc, 0, 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 10,
		    (1 << 1) | (0xf << 2) | (3 << 7) | 1, 1);
		if (error) {
			break;
		}
		error = ustorage_fs_verify(sc);

		break;

	case SC_WRITE_6:
		i = sc->sc_transfer.cmd_data[4];
		sc->sc_transfer.cmd_dir = DIR_READ;
		error = ustorage_fs_min_len(sc,
		    ((i == 0) ? 256 : i) << 9, 0 - (1 << 9));
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 6,
		    (7 << 1) | (1 << 4) | 1, 1);
		if (error) {
			break;
		}
		error = ustorage_fs_write(sc);

		break;

	case SC_WRITE_10:
		sc->sc_transfer.cmd_dir = DIR_READ;
		error = ustorage_fs_min_len(sc,
		    get_be16(&sc->sc_transfer.cmd_data[7]) << 9, 0 - (1 << 9));
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 10,
		    (1 << 1) | (0xf << 2) | (3 << 7) | 1, 1);
		if (error) {
			break;
		}
		error = ustorage_fs_write(sc);

		break;

	case SC_WRITE_12:
		sc->sc_transfer.cmd_dir = DIR_READ;
		error = ustorage_fs_min_len(sc,
		    get_be32(&sc->sc_transfer.cmd_data[6]) << 9, 0 - (1 << 9));
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, 12,
		    (1 << 1) | (0xf << 2) | (0xf << 6) | 1, 1);
		if (error) {
			break;
		}
		error = ustorage_fs_write(sc);

		break;

		/*
		 * Some mandatory commands that we recognize but don't
		 * implement.  They don't mean much in this setting.
		 * It's left as an exercise for anyone interested to
		 * implement RESERVE and RELEASE in terms of Posix
		 * locks.
		 */
	case SC_FORMAT_UNIT:
	case SC_RELEASE:
	case SC_RESERVE:
	case SC_SEND_DIAGNOSTIC:
		/* Fallthrough */

	default:
		error = ustorage_fs_min_len(sc, 0, 0 - 1);
		if (error) {
			break;
		}
		error = ustorage_fs_check_cmd(sc, sc->sc_transfer.cmd_len,
		    0xff, 0);
		if (error) {
			break;
		}
		sc->sc_transfer.currlun->sense_data =
		    SS_INVALID_COMMAND;
		error = 1;

		break;
	}
	return (error);
}
