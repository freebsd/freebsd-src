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

#include <dev/usb2/include/usb2_defs.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_ioctl.h>
#include <dev/usb2/include/usb2_error.h>

#define	USB_DEBUG_VAR ugen_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_mbuf.h>
#include <dev/usb2/core/usb2_dev.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_hub.h>
#include <dev/usb2/core/usb2_generic.h>
#include <dev/usb2/core/usb2_transfer.h>

#include <dev/usb2/controller/usb2_controller.h>
#include <dev/usb2/controller/usb2_bus.h>

/* defines */

#define	UGEN_BULK_FS_BUFFER_SIZE	(64*32)	/* bytes */
#define	UGEN_BULK_HS_BUFFER_SIZE	(1024*32)	/* bytes */
#define	UGEN_HW_FRAMES	50		/* number of milliseconds per transfer */

/* function prototypes */

static usb2_callback_t ugen_read_clear_stall_callback;
static usb2_callback_t ugen_write_clear_stall_callback;
static usb2_callback_t ugen_default_read_callback;
static usb2_callback_t ugen_default_write_callback;
static usb2_callback_t ugen_isoc_read_callback;
static usb2_callback_t ugen_isoc_write_callback;
static usb2_callback_t ugen_default_fs_callback;

static usb2_fifo_open_t ugen_open;
static usb2_fifo_close_t ugen_close;
static usb2_fifo_ioctl_t ugen_ioctl;
static usb2_fifo_cmd_t ugen_start_read;
static usb2_fifo_cmd_t ugen_start_write;
static usb2_fifo_cmd_t ugen_stop_io;

static int ugen_transfer_setup(struct usb2_fifo *f, const struct usb2_config *setup, uint8_t n_setup);
static int ugen_open_pipe_write(struct usb2_fifo *f);
static int ugen_open_pipe_read(struct usb2_fifo *f);
static int ugen_set_config(struct usb2_fifo *f, uint8_t index);
static int ugen_set_interface(struct usb2_fifo *f, uint8_t iface_index, uint8_t alt_index);
static int ugen_get_cdesc(struct usb2_fifo *f, struct usb2_gen_descriptor *pgd);
static int ugen_get_sdesc(struct usb2_fifo *f, struct usb2_gen_descriptor *ugd);
static int usb2_gen_fill_deviceinfo(struct usb2_fifo *f, struct usb2_device_info *di);
static int ugen_re_enumerate(struct usb2_fifo *f);
static int ugen_iface_ioctl(struct usb2_fifo *f, u_long cmd, void *addr, int fflags);
static int ugen_ctrl_ioctl(struct usb2_fifo *f, u_long cmd, void *addr, int fflags);
static int ugen_fs_uninit(struct usb2_fifo *f);
static uint8_t ugen_fs_get_complete(struct usb2_fifo *f, uint8_t *pindex);


/* structures */

struct usb2_fifo_methods usb2_ugen_methods = {
	.f_open = &ugen_open,
	.f_close = &ugen_close,
	.f_ioctl = &ugen_ioctl,
	.f_start_read = &ugen_start_read,
	.f_stop_read = &ugen_stop_io,
	.f_start_write = &ugen_start_write,
	.f_stop_write = &ugen_stop_io,
};

#if USB_DEBUG
static int ugen_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, ugen, CTLFLAG_RW, 0, "USB generic");
SYSCTL_INT(_hw_usb2_ugen, OID_AUTO, debug, CTLFLAG_RW, &ugen_debug,
    0, "Debug level");
#endif


/* prototypes */

static int
ugen_transfer_setup(struct usb2_fifo *f,
    const struct usb2_config *setup, uint8_t n_setup)
{
	struct usb2_pipe *pipe = f->priv_sc0;
	struct usb2_device *udev = f->udev;
	uint8_t iface_index = pipe->iface_index;
	int error;

	mtx_unlock(f->priv_mtx);

	/*
	 * "usb2_transfer_setup()" can sleep so one needs to make a wrapper,
	 * exiting the mutex and checking things
	 */
	error = usb2_transfer_setup(udev, &iface_index, f->xfer,
	    setup, n_setup, f, f->priv_mtx);
	if (error == 0) {

		if (f->xfer[0]->nframes == 1) {
			error = usb2_fifo_alloc_buffer(f,
			    f->xfer[0]->max_data_length, 2);
		} else {
			error = usb2_fifo_alloc_buffer(f,
			    f->xfer[0]->max_frame_size,
			    2 * f->xfer[0]->nframes);
		}
		if (error) {
			usb2_transfer_unsetup(f->xfer, n_setup);
		}
	}
	mtx_lock(f->priv_mtx);

	return (error);
}

static int
ugen_open(struct usb2_fifo *f, int fflags, struct thread *td)
{
	struct usb2_pipe *pipe = f->priv_sc0;
	struct usb2_endpoint_descriptor *ed = pipe->edesc;
	uint8_t type;

	DPRINTFN(6, "flag=0x%x\n", fflags);

	mtx_lock(f->priv_mtx);
	if (usb2_get_speed(f->udev) == USB_SPEED_HIGH) {
		f->nframes = UGEN_HW_FRAMES * 8;
		f->bufsize = UGEN_BULK_HS_BUFFER_SIZE;
	} else {
		f->nframes = UGEN_HW_FRAMES;
		f->bufsize = UGEN_BULK_FS_BUFFER_SIZE;
	}

	type = ed->bmAttributes & UE_XFERTYPE;
	if (type == UE_INTERRUPT) {
		f->bufsize = 0;		/* use "wMaxPacketSize" */
	}
	f->timeout = USB_NO_TIMEOUT;
	f->flag_short = 0;
	f->fifo_zlp = 0;
	mtx_unlock(f->priv_mtx);

	return (0);
}

static void
ugen_close(struct usb2_fifo *f, int fflags, struct thread *td)
{
	DPRINTFN(6, "flag=0x%x\n", fflags);

	/* cleanup */

	mtx_lock(f->priv_mtx);
	usb2_transfer_stop(f->xfer[0]);
	usb2_transfer_stop(f->xfer[1]);
	mtx_unlock(f->priv_mtx);

	usb2_transfer_unsetup(f->xfer, 2);
	usb2_fifo_free_buffer(f);

	if (ugen_fs_uninit(f)) {
		/* ignore any errors - we are closing */
	}
	return;
}

static int
ugen_open_pipe_write(struct usb2_fifo *f)
{
	struct usb2_config usb2_config[2];
	struct usb2_pipe *pipe = f->priv_sc0;
	struct usb2_endpoint_descriptor *ed = pipe->edesc;

	mtx_assert(f->priv_mtx, MA_OWNED);

	if (f->xfer[0] || f->xfer[1]) {
		/* transfers are already opened */
		return (0);
	}
	if (f->fs_xfer) {
		/* should not happen */
		return (EINVAL);
	}
	bzero(usb2_config, sizeof(usb2_config));

	usb2_config[1].type = UE_CONTROL;
	usb2_config[1].endpoint = 0;
	usb2_config[1].direction = UE_DIR_ANY;
	usb2_config[1].mh.timeout = 1000;	/* 1 second */
	usb2_config[1].mh.interval = 50;/* 50 milliseconds */
	usb2_config[1].mh.bufsize = sizeof(struct usb2_device_request);
	usb2_config[1].mh.callback = &ugen_write_clear_stall_callback;

	usb2_config[0].type = ed->bmAttributes & UE_XFERTYPE;
	usb2_config[0].endpoint = ed->bEndpointAddress & UE_ADDR;
	usb2_config[0].direction = ed->bEndpointAddress & (UE_DIR_OUT | UE_DIR_IN);
	usb2_config[0].mh.interval = USB_DEFAULT_INTERVAL;
	usb2_config[0].mh.flags.proxy_buffer = 1;

	switch (ed->bmAttributes & UE_XFERTYPE) {
	case UE_INTERRUPT:
	case UE_BULK:
		if (f->flag_short) {
			usb2_config[0].mh.flags.force_short_xfer = 1;
		}
		usb2_config[0].mh.callback = &ugen_default_write_callback;
		usb2_config[0].mh.timeout = f->timeout;
		usb2_config[0].mh.frames = 1;
		usb2_config[0].mh.bufsize = f->bufsize;
		usb2_config[0].md = usb2_config[0].mh;	/* symmetric config */
		if (ugen_transfer_setup(f, usb2_config, 2)) {
			return (EIO);
		}
		/* first transfer does not clear stall */
		f->flag_stall = 0;
		break;

	case UE_ISOCHRONOUS:
		usb2_config[0].mh.flags.short_xfer_ok = 1;
		usb2_config[0].mh.bufsize = 0;	/* use default */
		usb2_config[0].mh.frames = f->nframes;
		usb2_config[0].mh.callback = &ugen_isoc_write_callback;
		usb2_config[0].mh.timeout = 0;
		usb2_config[0].md = usb2_config[0].mh;	/* symmetric config */

		/* clone configuration */
		usb2_config[1] = usb2_config[0];

		if (ugen_transfer_setup(f, usb2_config, 2)) {
			return (EIO);
		}
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
ugen_open_pipe_read(struct usb2_fifo *f)
{
	struct usb2_config usb2_config[2];
	struct usb2_pipe *pipe = f->priv_sc0;
	struct usb2_endpoint_descriptor *ed = pipe->edesc;

	mtx_assert(f->priv_mtx, MA_OWNED);

	if (f->xfer[0] || f->xfer[1]) {
		/* transfers are already opened */
		return (0);
	}
	if (f->fs_xfer) {
		/* should not happen */
		return (EINVAL);
	}
	bzero(usb2_config, sizeof(usb2_config));

	usb2_config[1].type = UE_CONTROL;
	usb2_config[1].endpoint = 0;
	usb2_config[1].direction = UE_DIR_ANY;
	usb2_config[1].mh.timeout = 1000;	/* 1 second */
	usb2_config[1].mh.interval = 50;/* 50 milliseconds */
	usb2_config[1].mh.bufsize = sizeof(struct usb2_device_request);
	usb2_config[1].mh.callback = &ugen_read_clear_stall_callback;

	usb2_config[0].type = ed->bmAttributes & UE_XFERTYPE;
	usb2_config[0].endpoint = ed->bEndpointAddress & UE_ADDR;
	usb2_config[0].direction = UE_DIR_IN;
	usb2_config[0].mh.interval = USB_DEFAULT_INTERVAL;
	usb2_config[0].mh.flags.proxy_buffer = 1;

	switch (ed->bmAttributes & UE_XFERTYPE) {
	case UE_INTERRUPT:
	case UE_BULK:
		if (f->flag_short) {
			usb2_config[0].mh.flags.short_xfer_ok = 1;
		}
		usb2_config[0].mh.timeout = f->timeout;
		usb2_config[0].mh.frames = 1;
		usb2_config[0].mh.callback = &ugen_default_read_callback;
		usb2_config[0].mh.bufsize = f->bufsize;
		usb2_config[0].md = usb2_config[0].mh;	/* symmetric config */

		if (ugen_transfer_setup(f, usb2_config, 2)) {
			return (EIO);
		}
		/* first transfer does not clear stall */
		f->flag_stall = 0;
		break;

	case UE_ISOCHRONOUS:
		usb2_config[0].mh.flags.short_xfer_ok = 1;
		usb2_config[0].mh.bufsize = 0;	/* use default */
		usb2_config[0].mh.frames = f->nframes;
		usb2_config[0].mh.callback = &ugen_isoc_read_callback;
		usb2_config[0].mh.timeout = 0;
		usb2_config[0].md = usb2_config[0].mh;	/* symmetric config */

		/* clone configuration */
		usb2_config[1] = usb2_config[0];

		if (ugen_transfer_setup(f, usb2_config, 2)) {
			return (EIO);
		}
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

static void
ugen_start_read(struct usb2_fifo *f)
{
	/* check that pipes are open */
	if (ugen_open_pipe_read(f)) {
		/* signal error */
		usb2_fifo_put_data_error(f);
	}
	/* start transfers */
	usb2_transfer_start(f->xfer[0]);
	usb2_transfer_start(f->xfer[1]);
	return;
}

static void
ugen_start_write(struct usb2_fifo *f)
{
	/* check that pipes are open */
	if (ugen_open_pipe_write(f)) {
		/* signal error */
		usb2_fifo_get_data_error(f);
	}
	/* start transfers */
	usb2_transfer_start(f->xfer[0]);
	usb2_transfer_start(f->xfer[1]);
	return;
}

static void
ugen_stop_io(struct usb2_fifo *f)
{
	/* stop transfers */
	usb2_transfer_stop(f->xfer[0]);
	usb2_transfer_stop(f->xfer[1]);
	return;
}

static void
ugen_default_read_callback(struct usb2_xfer *xfer)
{
	struct usb2_fifo *f = xfer->priv_sc;
	struct usb2_mbuf *m;

	DPRINTFN(4, "actlen=%u, aframes=%u\n", xfer->actlen, xfer->aframes);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (xfer->actlen == 0) {
			if (f->fifo_zlp != 4) {
				f->fifo_zlp++;
			} else {
				/*
				 * Throttle a little bit we have multiple ZLPs
				 * in a row!
				 */
				xfer->interval = 64;	/* ms */
			}
		} else {
			/* clear throttle */
			xfer->interval = 0;
			f->fifo_zlp = 0;
		}
		usb2_fifo_put_data(f, xfer->frbuffers, 0,
		    xfer->actlen, 1);

	case USB_ST_SETUP:
		if (f->flag_stall) {
			usb2_transfer_start(f->xfer[1]);
			break;
		}
		USB_IF_POLL(&f->free_q, m);
		if (m) {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		break;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			f->flag_stall = 1;
			f->fifo_zlp = 0;
			usb2_transfer_start(f->xfer[1]);
		}
		break;
	}
	return;
}

static void
ugen_default_write_callback(struct usb2_xfer *xfer)
{
	struct usb2_fifo *f = xfer->priv_sc;
	uint32_t actlen;

	DPRINTFN(4, "actlen=%u, aframes=%u\n", xfer->actlen, xfer->aframes);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
		/*
		 * If writing is in stall, just jump to clear stall
		 * callback and solve the situation.
		 */
		if (f->flag_stall) {
			usb2_transfer_start(f->xfer[1]);
			break;
		}
		/*
		 * Write data, setup and perform hardware transfer.
		 */
		if (usb2_fifo_get_data(f, xfer->frbuffers, 0,
		    xfer->max_data_length, &actlen, 0)) {
			xfer->frlengths[0] = actlen;
			usb2_start_hardware(xfer);
		}
		break;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			f->flag_stall = 1;
			usb2_transfer_start(f->xfer[1]);
		}
		break;
	}
	return;
}

static void
ugen_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct usb2_fifo *f = xfer->priv_sc;
	struct usb2_xfer *xfer_other = f->xfer[0];

	if (f->flag_stall == 0) {
		/* nothing to do */
		return;
	}
	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTFN(5, "f=%p: stall cleared\n", f);
		f->flag_stall = 0;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ugen_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct usb2_fifo *f = xfer->priv_sc;
	struct usb2_xfer *xfer_other = f->xfer[0];

	if (f->flag_stall == 0) {
		/* nothing to do */
		return;
	}
	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTFN(5, "f=%p: stall cleared\n", f);
		f->flag_stall = 0;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ugen_isoc_read_callback(struct usb2_xfer *xfer)
{
	struct usb2_fifo *f = xfer->priv_sc;
	uint32_t offset;
	uint16_t n;

	DPRINTFN(4, "actlen=%u, aframes=%u\n", xfer->actlen, xfer->aframes);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTFN(6, "actlen=%d\n", xfer->actlen);

		offset = 0;

		for (n = 0; n != xfer->aframes; n++) {
			usb2_fifo_put_data(f, xfer->frbuffers, offset,
			    xfer->frlengths[n], 1);
			offset += xfer->max_frame_size;
		}

	case USB_ST_SETUP:
tr_setup:
		for (n = 0; n != xfer->nframes; n++) {
			/* setup size for next transfer */
			xfer->frlengths[n] = xfer->max_frame_size;
		}
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			break;
		}
		goto tr_setup;
	}
	return;
}

static void
ugen_isoc_write_callback(struct usb2_xfer *xfer)
{
	struct usb2_fifo *f = xfer->priv_sc;
	uint32_t actlen;
	uint32_t offset;
	uint16_t n;

	DPRINTFN(4, "actlen=%u, aframes=%u\n", xfer->actlen, xfer->aframes);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
tr_setup:
		offset = 0;
		for (n = 0; n != xfer->nframes; n++) {
			if (usb2_fifo_get_data(f, xfer->frbuffers, offset,
			    xfer->max_frame_size, &actlen, 1)) {
				xfer->frlengths[n] = actlen;
				offset += actlen;
			} else {
				break;
			}
		}

		for (; n != xfer->nframes; n++) {
			/* fill in zero frames */
			xfer->frlengths[n] = 0;
		}
		usb2_start_hardware(xfer);
		break;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			break;
		}
		goto tr_setup;
	}
	return;
}

static int
ugen_set_config(struct usb2_fifo *f, uint8_t index)
{
	DPRINTFN(2, "index %u\n", index);

	if (f->flag_no_uref) {
		/* not the control endpoint - just forget it */
		return (EINVAL);
	}
	if (f->udev->flags.usb2_mode != USB_MODE_HOST) {
		/* not possible in device side mode */
		return (ENOTTY);
	}
	if (f->udev->curr_config_index == index) {
		/* no change needed */
		return (0);
	}
	/* change setting - will free generic FIFOs, if any */
	if (usb2_set_config_index(f->udev, index)) {
		return (EIO);
	}
	/* probe and attach */
	if (usb2_probe_and_attach(f->udev, USB_IFACE_INDEX_ANY)) {
		return (EIO);
	}
	return (0);
}

static int
ugen_set_interface(struct usb2_fifo *f,
    uint8_t iface_index, uint8_t alt_index)
{
	DPRINTFN(2, "%u, %u\n", iface_index, alt_index);

	if (f->flag_no_uref) {
		/* not the control endpoint - just forget it */
		return (EINVAL);
	}
	if (f->udev->flags.usb2_mode != USB_MODE_HOST) {
		/* not possible in device side mode */
		return (ENOTTY);
	}
	/* change setting - will free generic FIFOs, if any */
	if (usb2_set_alt_interface_index(f->udev, iface_index, alt_index)) {
		return (EIO);
	}
	/* probe and attach */
	if (usb2_probe_and_attach(f->udev, iface_index)) {
		return (EIO);
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	ugen_get_cdesc
 *
 * This function will retrieve the complete configuration descriptor
 * at the given index.
 *------------------------------------------------------------------------*/
static int
ugen_get_cdesc(struct usb2_fifo *f, struct usb2_gen_descriptor *ugd)
{
	struct usb2_config_descriptor *cdesc;
	struct usb2_device *udev = f->udev;
	int error;
	uint16_t len;
	uint8_t free_data;

	DPRINTFN(6, "\n");

	if (f->flag_no_uref) {
		/* control endpoint only */
		return (EINVAL);
	}
	if (ugd->ugd_data == NULL) {
		/* userland pointer should not be zero */
		return (EINVAL);
	}
	if ((ugd->ugd_config_index == USB_UNCONFIG_INDEX) ||
	    (ugd->ugd_config_index == udev->curr_config_index)) {
		cdesc = usb2_get_config_descriptor(udev);
		if (cdesc == NULL) {
			return (ENXIO);
		}
		free_data = 0;

	} else {
		if (usb2_req_get_config_desc_full(udev,
		    &Giant, &cdesc, M_USBDEV,
		    ugd->ugd_config_index)) {
			return (ENXIO);
		}
		free_data = 1;
	}

	len = UGETW(cdesc->wTotalLength);
	if (len > ugd->ugd_maxlen) {
		len = ugd->ugd_maxlen;
	}
	DPRINTFN(6, "len=%u\n", len);

	ugd->ugd_actlen = len;
	ugd->ugd_offset = 0;

	error = copyout(cdesc, ugd->ugd_data, len);

	if (free_data) {
		free(cdesc, M_USBDEV);
	}
	return (error);
}

static int
ugen_get_sdesc(struct usb2_fifo *f, struct usb2_gen_descriptor *ugd)
{
	void *ptr = f->udev->bus->scratch[0].data;
	uint16_t size = sizeof(f->udev->bus->scratch[0].data);
	int error;

	if (f->flag_no_uref) {
		/* control endpoint only */
		return (EINVAL);
	}
	if (usb2_req_get_string_desc(f->udev, &Giant, ptr,
	    size, ugd->ugd_lang_id, ugd->ugd_string_index)) {
		error = EINVAL;
	} else {

		if (size > ((uint8_t *)ptr)[0]) {
			size = ((uint8_t *)ptr)[0];
		}
		if (size > ugd->ugd_maxlen) {
			size = ugd->ugd_maxlen;
		}
		ugd->ugd_actlen = size;
		ugd->ugd_offset = 0;

		error = copyout(ptr, ugd->ugd_data, size);
	}
	return (error);
}

/*------------------------------------------------------------------------*
 *	usb2_gen_fill_devicenames
 *
 * This function dumps information about an USB device names to
 * userland.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static int
usb2_gen_fill_devicenames(struct usb2_fifo *f, struct usb2_device_names *dn)
{
	struct usb2_interface *iface;
	const char *ptr;
	char *dst;
	char buf[32];
	int error = 0;
	int len;
	int max_len;
	uint8_t i;
	uint8_t first = 1;

	if (f->flag_no_uref) {
		/* control endpoint only */
		return (EINVAL);
	}
	max_len = dn->udn_devnames_len;
	dst = dn->udn_devnames_ptr;

	if (max_len == 0) {
		return (EINVAL);
	}
	/* put a zero there */
	error = copyout("", dst, 1);
	if (error) {
		return (error);
	}
	for (i = 0;; i++) {
		iface = usb2_get_iface(f->udev, i);
		if (iface == NULL) {
			break;
		}
		if ((iface->subdev != NULL) &&
		    device_is_attached(iface->subdev)) {
			ptr = device_get_nameunit(iface->subdev);
			if (!first) {
				strlcpy(buf, ", ", sizeof(buf));
			} else {
				buf[0] = 0;
			}
			strlcat(buf, ptr, sizeof(buf));
			len = strlen(buf) + 1;
			if (len > max_len) {
				break;
			}
			error = copyout(buf, dst, len);
			if (error) {
				return (error);
			}
			len--;
			dst += len;
			max_len -= len;
			first = 0;
		}
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_gen_fill_deviceinfo
 *
 * This function dumps information about an USB device to the
 * structure pointed to by the "di" argument.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static int
usb2_gen_fill_deviceinfo(struct usb2_fifo *f, struct usb2_device_info *di)
{
	struct usb2_device *udev;
	struct usb2_device *hub;

	if (f->flag_no_uref) {
		/* control endpoint only */
		return (EINVAL);
	}
	udev = f->udev;

	bzero(di, sizeof(di[0]));

	di->udi_bus = device_get_unit(udev->bus->bdev);
	di->udi_addr = udev->address;
	di->udi_index = udev->device_index;
	strlcpy(di->udi_serial, udev->serial,
	    sizeof(di->udi_serial));
	strlcpy(di->udi_vendor, udev->manufacturer,
	    sizeof(di->udi_vendor));
	strlcpy(di->udi_product, udev->product,
	    sizeof(di->udi_product));
	usb2_printBCD(di->udi_release, sizeof(di->udi_release),
	    UGETW(udev->ddesc.bcdDevice));
	di->udi_vendorNo = UGETW(udev->ddesc.idVendor);
	di->udi_productNo = UGETW(udev->ddesc.idProduct);
	di->udi_releaseNo = UGETW(udev->ddesc.bcdDevice);
	di->udi_class = udev->ddesc.bDeviceClass;
	di->udi_subclass = udev->ddesc.bDeviceSubClass;
	di->udi_protocol = udev->ddesc.bDeviceProtocol;
	di->udi_config_no = udev->curr_config_no;
	di->udi_config_index = udev->curr_config_index;
	di->udi_power = udev->flags.self_powered ? 0 : udev->power;
	di->udi_speed = udev->speed;
	di->udi_mode = udev->flags.usb2_mode;
	di->udi_power_mode = udev->power_mode;
	if (udev->flags.suspended) {
		di->udi_suspended = 1;
	} else {
		di->udi_suspended = 0;
	}

	hub = udev->parent_hub;
	if (hub) {
		di->udi_hubaddr = hub->address;
		di->udi_hubindex = hub->device_index;
		di->udi_hubport = udev->port_no;
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	ugen_check_request
 *
 * Return values:
 * 0: Access allowed
 * Else: No access
 *------------------------------------------------------------------------*/
static int
ugen_check_request(struct usb2_device *udev, struct usb2_device_request *req)
{
	struct usb2_pipe *pipe;
	int error;

	/*
	 * Avoid requests that would damage the bus integrity:
	 */
	if (((req->bmRequestType == UT_WRITE_DEVICE) &&
	    (req->bRequest == UR_SET_ADDRESS)) ||
	    ((req->bmRequestType == UT_WRITE_DEVICE) &&
	    (req->bRequest == UR_SET_CONFIG)) ||
	    ((req->bmRequestType == UT_WRITE_INTERFACE) &&
	    (req->bRequest == UR_SET_INTERFACE))) {
		/*
		 * These requests can be useful for testing USB drivers.
		 */
		error = priv_check(curthread, PRIV_DRIVER);
		if (error) {
			return (error);
		}
	}
	/*
	 * Special case - handle clearing of stall
	 */
	if (req->bmRequestType == UT_WRITE_ENDPOINT) {

		pipe = usb2_get_pipe_by_addr(udev, req->wIndex[0]);
		if (pipe == NULL) {
			return (EINVAL);
		}
		if (usb2_check_thread_perm(udev, curthread, FREAD | FWRITE,
		    pipe->iface_index, req->wIndex[0] & UE_ADDR)) {
			return (EPERM);
		}
		if ((req->bRequest == UR_CLEAR_FEATURE) &&
		    (UGETW(req->wValue) == UF_ENDPOINT_HALT)) {
			usb2_clear_data_toggle(udev, pipe);
		}
	}
	/* TODO: add more checks to verify the interface index */

	return (0);
}

int
ugen_do_request(struct usb2_fifo *f, struct usb2_ctl_request *ur)
{
	int error;
	uint16_t len;
	uint16_t actlen;

	if (f->flag_no_uref) {
		/* control endpoint only */
		return (EINVAL);
	}
	if (ugen_check_request(f->udev, &ur->ucr_request)) {
		return (EPERM);
	}
	len = UGETW(ur->ucr_request.wLength);

	/* check if "ucr_data" is valid */
	if (len != 0) {
		if (ur->ucr_data == NULL) {
			return (EFAULT);
		}
	}
	/* do the USB request */
	error = usb2_do_request_flags
	    (f->udev, NULL, &ur->ucr_request, ur->ucr_data,
	    (ur->ucr_flags & USB_SHORT_XFER_OK) |
	    USB_USER_DATA_PTR, &actlen,
	    USB_DEFAULT_TIMEOUT);

	ur->ucr_actlen = actlen;

	if (error) {
		error = EIO;
	}
	return (error);
}

/*------------------------------------------------------------------------
 *	ugen_re_enumerate
 *------------------------------------------------------------------------*/
static int
ugen_re_enumerate(struct usb2_fifo *f)
{
	struct usb2_device *udev = f->udev;
	int error;

	if (f->flag_no_uref) {
		/* control endpoint only */
		return (EINVAL);
	}
	/*
	 * This request can be useful for testing USB drivers:
	 */
	error = priv_check(curthread, PRIV_DRIVER);
	if (error) {
		return (error);
	}
	mtx_lock(f->priv_mtx);
	error = usb2_req_re_enumerate(udev, f->priv_mtx);
	mtx_unlock(f->priv_mtx);

	if (error) {
		return (ENXIO);
	}
	return (0);
}

static int
ugen_fs_uninit(struct usb2_fifo *f)
{
	if (f->fs_xfer == NULL) {
		return (EINVAL);
	}
	usb2_transfer_unsetup(f->fs_xfer, f->fs_ep_max);
	free(f->fs_xfer, M_USB);
	f->fs_xfer = NULL;
	f->fs_ep_max = 0;
	f->fs_ep_ptr = NULL;
	f->flag_iscomplete = 0;
	f->flag_no_uref = 0;		/* restore operation */
	usb2_fifo_free_buffer(f);
	return (0);
}

static uint8_t
ugen_fs_get_complete(struct usb2_fifo *f, uint8_t *pindex)
{
	struct usb2_mbuf *m;

	USB_IF_DEQUEUE(&f->used_q, m);

	if (m) {
		*pindex = *((uint8_t *)(m->cur_data_ptr));

		USB_IF_ENQUEUE(&f->free_q, m);

		return (0);		/* success */
	} else {

		*pindex = 0;		/* fix compiler warning */

		f->flag_iscomplete = 0;
	}
	return (1);			/* failure */
}

static void
ugen_fs_set_complete(struct usb2_fifo *f, uint8_t index)
{
	struct usb2_mbuf *m;

	USB_IF_DEQUEUE(&f->free_q, m);

	if (m == NULL) {
		/* can happen during close */
		DPRINTF("out of buffers\n");
		return;
	}
	USB_MBUF_RESET(m);

	*((uint8_t *)(m->cur_data_ptr)) = index;

	USB_IF_ENQUEUE(&f->used_q, m);

	f->flag_iscomplete = 1;

	usb2_fifo_wakeup(f);

	return;
}

static int
ugen_fs_copy_in(struct usb2_fifo *f, uint8_t ep_index)
{
	struct usb2_device_request *req;
	struct usb2_xfer *xfer;
	struct usb2_fs_endpoint fs_ep;
	void *uaddr;			/* userland pointer */
	void *kaddr;
	uint32_t offset;
	uint32_t length;
	uint32_t n;
	uint32_t rem;
	int error;
	uint8_t isread;

	if (ep_index >= f->fs_ep_max) {
		return (EINVAL);
	}
	xfer = f->fs_xfer[ep_index];
	if (xfer == NULL) {
		return (EINVAL);
	}
	mtx_lock(f->priv_mtx);
	if (usb2_transfer_pending(xfer)) {
		mtx_unlock(f->priv_mtx);
		return (EBUSY);		/* should not happen */
	}
	mtx_unlock(f->priv_mtx);

	error = copyin(f->fs_ep_ptr +
	    ep_index, &fs_ep, sizeof(fs_ep));
	if (error) {
		return (error);
	}
	/* security checks */

	if (fs_ep.nFrames > xfer->max_frame_count) {
		xfer->error = USB_ERR_INVAL;
		goto complete;
	}
	if (fs_ep.nFrames == 0) {
		xfer->error = USB_ERR_INVAL;
		goto complete;
	}
	error = copyin(fs_ep.ppBuffer,
	    &uaddr, sizeof(uaddr));
	if (error) {
		return (error);
	}
	/* reset first frame */
	usb2_set_frame_offset(xfer, 0, 0);

	if (xfer->flags_int.control_xfr) {

		req = xfer->frbuffers[0].buffer;

		error = copyin(fs_ep.pLength,
		    &length, sizeof(length));
		if (error) {
			return (error);
		}
		if (length >= sizeof(*req)) {
			xfer->error = USB_ERR_INVAL;
			goto complete;
		}
		if (length != 0) {
			error = copyin(uaddr, req, length);
			if (error) {
				return (error);
			}
		}
		if (ugen_check_request(f->udev, req)) {
			xfer->error = USB_ERR_INVAL;
			goto complete;
		}
		xfer->frlengths[0] = length;

		/* Host mode only ! */
		if ((req->bmRequestType &
		    (UT_READ | UT_WRITE)) == UT_READ) {
			isread = 1;
		} else {
			isread = 0;
		}
		n = 1;
		offset = sizeof(*req);

	} else {
		/* Device and Host mode */
		if (USB_GET_DATA_ISREAD(xfer)) {
			isread = 1;
		} else {
			isread = 0;
		}
		n = 0;
		offset = 0;
	}

	rem = xfer->max_data_length;
	xfer->nframes = fs_ep.nFrames;
	xfer->timeout = fs_ep.timeout;
	if (xfer->timeout > 65535) {
		xfer->timeout = 65535;
	}
	if (fs_ep.flags & USB_FS_FLAG_SINGLE_SHORT_OK)
		xfer->flags.short_xfer_ok = 1;
	else
		xfer->flags.short_xfer_ok = 0;

	if (fs_ep.flags & USB_FS_FLAG_MULTI_SHORT_OK)
		xfer->flags.short_frames_ok = 1;
	else
		xfer->flags.short_frames_ok = 0;

	if (fs_ep.flags & USB_FS_FLAG_FORCE_SHORT)
		xfer->flags.force_short_xfer = 1;
	else
		xfer->flags.force_short_xfer = 0;

	if (fs_ep.flags & USB_FS_FLAG_CLEAR_STALL)
		xfer->flags.stall_pipe = 1;
	else
		xfer->flags.stall_pipe = 0;

	for (; n != xfer->nframes; n++) {

		error = copyin(fs_ep.pLength + n,
		    &length, sizeof(length));
		if (error) {
			break;
		}
		xfer->frlengths[n] = length;

		if (length > rem) {
			xfer->error = USB_ERR_INVAL;
			goto complete;
		}
		rem -= length;

		if (!isread) {

			/* we need to know the source buffer */
			error = copyin(fs_ep.ppBuffer + n,
			    &uaddr, sizeof(uaddr));
			if (error) {
				break;
			}
			if (xfer->flags_int.isochronous_xfr) {
				/* get kernel buffer address */
				kaddr = xfer->frbuffers[0].buffer;
				kaddr = USB_ADD_BYTES(kaddr, offset);
			} else {
				/* set current frame offset */
				usb2_set_frame_offset(xfer, offset, n);

				/* get kernel buffer address */
				kaddr = xfer->frbuffers[n].buffer;
			}

			/* move data */
			error = copyin(uaddr, kaddr, length);
			if (error) {
				break;
			}
		}
		offset += length;
	}
	return (error);

complete:
	mtx_lock(f->priv_mtx);
	ugen_fs_set_complete(f, ep_index);
	mtx_unlock(f->priv_mtx);
	return (0);
}

static int
ugen_fs_copy_out(struct usb2_fifo *f, uint8_t ep_index)
{
	struct usb2_device_request *req;
	struct usb2_xfer *xfer;
	struct usb2_fs_endpoint fs_ep;
	struct usb2_fs_endpoint *fs_ep_uptr;	/* userland ptr */
	void *uaddr;			/* userland ptr */
	void *kaddr;
	uint32_t offset;
	uint32_t length;
	uint32_t temp;
	uint32_t n;
	uint32_t rem;
	int error;
	uint8_t isread;

	if (ep_index >= f->fs_ep_max) {
		return (EINVAL);
	}
	xfer = f->fs_xfer[ep_index];
	if (xfer == NULL) {
		return (EINVAL);
	}
	mtx_lock(f->priv_mtx);
	if (usb2_transfer_pending(xfer)) {
		mtx_unlock(f->priv_mtx);
		return (EBUSY);		/* should not happen */
	}
	mtx_unlock(f->priv_mtx);

	fs_ep_uptr = f->fs_ep_ptr + ep_index;
	error = copyin(fs_ep_uptr, &fs_ep, sizeof(fs_ep));
	if (error) {
		return (error);
	}
	fs_ep.status = xfer->error;
	fs_ep.aFrames = xfer->aframes;
	fs_ep.isoc_time_complete = xfer->isoc_time_complete;
	if (xfer->error) {
		goto complete;
	}
	if (xfer->flags_int.control_xfr) {
		req = xfer->frbuffers[0].buffer;

		/* Host mode only ! */
		if ((req->bmRequestType & (UT_READ | UT_WRITE)) == UT_READ) {
			isread = 1;
		} else {
			isread = 0;
		}
		if (xfer->nframes == 0)
			n = 0;		/* should never happen */
		else
			n = 1;
	} else {
		/* Device and Host mode */
		if (USB_GET_DATA_ISREAD(xfer)) {
			isread = 1;
		} else {
			isread = 0;
		}
		n = 0;
	}

	/* Update lengths and copy out data */

	rem = xfer->max_data_length;
	offset = 0;

	for (; n != xfer->nframes; n++) {

		/* get initial length into "temp" */
		error = copyin(fs_ep.pLength + n,
		    &temp, sizeof(temp));
		if (error) {
			return (error);
		}
		if (temp > rem) {
			/* the userland length has been corrupted */
			DPRINTF("corrupt userland length "
			    "%u > %u\n", temp, rem);
			fs_ep.status = USB_ERR_INVAL;
			goto complete;
		}
		rem -= temp;

		/* get actual transfer length */
		length = xfer->frlengths[n];
		if (length > temp) {
			/* data overflow */
			fs_ep.status = USB_ERR_INVAL;
			DPRINTF("data overflow %u > %u\n",
			    length, temp);
			goto complete;
		}
		if (isread) {

			/* we need to know the destination buffer */
			error = copyin(fs_ep.ppBuffer + n,
			    &uaddr, sizeof(uaddr));
			if (error) {
				return (error);
			}
			if (xfer->flags_int.isochronous_xfr) {
				/* only one frame buffer */
				kaddr = USB_ADD_BYTES(
				    xfer->frbuffers[0].buffer, offset);
			} else {
				/* multiple frame buffers */
				kaddr = xfer->frbuffers[n].buffer;
			}

			/* move data */
			error = copyout(kaddr, uaddr, length);
			if (error) {
				return (error);
			}
		}
		/*
		 * Update offset according to initial length, which is
		 * needed by isochronous transfers!
		 */
		offset += temp;

		/* update length */
		error = copyout(&length,
		    fs_ep.pLength + n, sizeof(length));
		if (error) {
			return (error);
		}
	}

complete:
	/* update "aFrames" */
	error = copyout(&fs_ep.aFrames, &fs_ep_uptr->aFrames,
	    sizeof(fs_ep.aFrames));
	if (error)
		goto done;

	/* update "isoc_time_complete" */
	error = copyout(&fs_ep.isoc_time_complete,
	    &fs_ep_uptr->isoc_time_complete,
	    sizeof(fs_ep.isoc_time_complete));
	if (error)
		goto done;
	/* update "status" */
	error = copyout(&fs_ep.status, &fs_ep_uptr->status,
	    sizeof(fs_ep.status));
done:
	return (error);
}

static uint8_t
ugen_fifo_in_use(struct usb2_fifo *f, int fflags)
{
	struct usb2_fifo *f_rx;
	struct usb2_fifo *f_tx;

	f_rx = f->udev->fifo[(f->fifo_index & ~1) + USB_FIFO_RX];
	f_tx = f->udev->fifo[(f->fifo_index & ~1) + USB_FIFO_TX];

	if ((fflags & FREAD) && f_rx &&
	    (f_rx->xfer[0] || f_rx->xfer[1])) {
		return (1);		/* RX FIFO in use */
	}
	if ((fflags & FWRITE) && f_tx &&
	    (f_tx->xfer[0] || f_tx->xfer[1])) {
		return (1);		/* TX FIFO in use */
	}
	return (0);			/* not in use */
}

static int
ugen_fs_ioctl(struct usb2_fifo *f, u_long cmd, void *addr, int fflags)
{
	struct usb2_config usb2_config[1];
	struct usb2_device_request req;
	union {
		struct usb2_fs_complete *pcomp;
		struct usb2_fs_start *pstart;
		struct usb2_fs_stop *pstop;
		struct usb2_fs_init *pinit;
		struct usb2_fs_uninit *puninit;
		struct usb2_fs_open *popen;
		struct usb2_fs_close *pclose;
		struct usb2_fs_clear_stall_sync *pstall;
		void   *addr;
	}     u;
	struct usb2_pipe *pipe;
	struct usb2_endpoint_descriptor *ed;
	int error = 0;
	uint8_t iface_index;
	uint8_t isread;
	uint8_t ep_index;

	u.addr = addr;

	switch (cmd) {
	case USB_FS_COMPLETE:
		mtx_lock(f->priv_mtx);
		error = ugen_fs_get_complete(f, &ep_index);
		mtx_unlock(f->priv_mtx);

		if (error) {
			error = EBUSY;
			break;
		}
		u.pcomp->ep_index = ep_index;
		error = ugen_fs_copy_out(f, u.pcomp->ep_index);
		break;

	case USB_FS_START:
		error = ugen_fs_copy_in(f, u.pstart->ep_index);
		if (error) {
			break;
		}
		mtx_lock(f->priv_mtx);
		usb2_transfer_start(f->fs_xfer[u.pstart->ep_index]);
		mtx_unlock(f->priv_mtx);
		break;

	case USB_FS_STOP:
		if (u.pstop->ep_index >= f->fs_ep_max) {
			error = EINVAL;
			break;
		}
		mtx_lock(f->priv_mtx);
		usb2_transfer_stop(f->fs_xfer[u.pstop->ep_index]);
		mtx_unlock(f->priv_mtx);
		break;

	case USB_FS_INIT:
		/* verify input parameters */
		if (u.pinit->pEndpoints == NULL) {
			error = EINVAL;
			break;
		}
		if (u.pinit->ep_index_max > 127) {
			error = EINVAL;
			break;
		}
		if (u.pinit->ep_index_max == 0) {
			error = EINVAL;
			break;
		}
		if (f->fs_xfer != NULL) {
			error = EBUSY;
			break;
		}
		if (f->flag_no_uref) {
			error = EINVAL;
			break;
		}
		if (f->dev_ep_index != 0) {
			error = EINVAL;
			break;
		}
		if (ugen_fifo_in_use(f, fflags)) {
			error = EBUSY;
			break;
		}
		error = usb2_fifo_alloc_buffer(f, 1, u.pinit->ep_index_max);
		if (error) {
			break;
		}
		f->fs_xfer = malloc(sizeof(f->fs_xfer[0]) *
		    u.pinit->ep_index_max, M_USB, M_WAITOK | M_ZERO);
		if (f->fs_xfer == NULL) {
			usb2_fifo_free_buffer(f);
			error = ENOMEM;
			break;
		}
		f->fs_ep_max = u.pinit->ep_index_max;
		f->fs_ep_ptr = u.pinit->pEndpoints;
		break;

	case USB_FS_UNINIT:
		if (u.puninit->dummy != 0) {
			error = EINVAL;
			break;
		}
		error = ugen_fs_uninit(f);
		break;

	case USB_FS_OPEN:
		if (u.popen->ep_index >= f->fs_ep_max) {
			error = EINVAL;
			break;
		}
		if (f->fs_xfer[u.popen->ep_index] != NULL) {
			error = EBUSY;
			break;
		}
		if (u.popen->max_bufsize > USB_FS_MAX_BUFSIZE) {
			u.popen->max_bufsize = USB_FS_MAX_BUFSIZE;
		}
		if (u.popen->max_frames > USB_FS_MAX_FRAMES) {
			u.popen->max_frames = USB_FS_MAX_FRAMES;
			break;
		}
		if (u.popen->max_frames == 0) {
			error = EINVAL;
			break;
		}
		pipe = usb2_get_pipe_by_addr(f->udev, u.popen->ep_no);
		if (pipe == NULL) {
			error = EINVAL;
			break;
		}
		ed = pipe->edesc;
		if (ed == NULL) {
			error = ENXIO;
			break;
		}
		iface_index = pipe->iface_index;

		error = usb2_check_thread_perm(f->udev, curthread, fflags,
		    iface_index, u.popen->ep_no);
		if (error) {
			break;
		}
		bzero(usb2_config, sizeof(usb2_config));

		usb2_config[0].type = ed->bmAttributes & UE_XFERTYPE;
		usb2_config[0].endpoint = ed->bEndpointAddress & UE_ADDR;
		usb2_config[0].direction = ed->bEndpointAddress & (UE_DIR_OUT | UE_DIR_IN);
		usb2_config[0].mh.interval = USB_DEFAULT_INTERVAL;
		usb2_config[0].mh.flags.proxy_buffer = 1;
		usb2_config[0].mh.callback = &ugen_default_fs_callback;
		usb2_config[0].mh.timeout = 0;	/* no timeout */
		usb2_config[0].mh.frames = u.popen->max_frames;
		usb2_config[0].mh.bufsize = u.popen->max_bufsize;
		usb2_config[0].md = usb2_config[0].mh;	/* symmetric config */

		if (usb2_config[0].type == UE_CONTROL) {
			if (f->udev->flags.usb2_mode != USB_MODE_HOST) {
				error = EINVAL;
				break;
			}
		} else {

			isread = ((usb2_config[0].endpoint &
			    (UE_DIR_IN | UE_DIR_OUT)) == UE_DIR_IN);

			if (f->udev->flags.usb2_mode != USB_MODE_HOST) {
				isread = !isread;
			}
			/* check permissions */
			if (isread) {
				if (!(fflags & FREAD)) {
					error = EPERM;
					break;
				}
			} else {
				if (!(fflags & FWRITE)) {
					error = EPERM;
					break;
				}
			}
		}
		error = usb2_transfer_setup(f->udev, &iface_index,
		    f->fs_xfer + u.popen->ep_index, usb2_config, 1,
		    f, f->priv_mtx);
		if (error == 0) {
			/* update maximums */
			u.popen->max_packet_length =
			    f->fs_xfer[u.popen->ep_index]->max_frame_size;
			u.popen->max_bufsize =
			    f->fs_xfer[u.popen->ep_index]->max_data_length;
			f->fs_xfer[u.popen->ep_index]->priv_fifo =
			    ((uint8_t *)0) + u.popen->ep_index;
			/*
			 * Increase performance by dropping locks we
			 * don't need:
			 */
			f->flag_no_uref = 1;
		} else {
			error = ENOMEM;
		}
		break;

	case USB_FS_CLOSE:
		if (u.pclose->ep_index >= f->fs_ep_max) {
			error = EINVAL;
			break;
		}
		if (f->fs_xfer[u.pclose->ep_index] == NULL) {
			error = EINVAL;
			break;
		}
		usb2_transfer_unsetup(f->fs_xfer + u.pclose->ep_index, 1);
		break;

	case USB_FS_CLEAR_STALL_SYNC:
		if (u.pstall->ep_index >= f->fs_ep_max) {
			error = EINVAL;
			break;
		}
		if (f->fs_xfer[u.pstall->ep_index] == NULL) {
			error = EINVAL;
			break;
		}
		if (f->udev->flags.usb2_mode != USB_MODE_HOST) {
			error = EINVAL;
			break;
		}
		mtx_lock(f->priv_mtx);
		error = usb2_transfer_pending(f->fs_xfer[u.pstall->ep_index]);
		mtx_unlock(f->priv_mtx);

		if (error) {
			return (EBUSY);
		}
		pipe = f->fs_xfer[u.pstall->ep_index]->pipe;

		/* setup a clear-stall packet */
		req.bmRequestType = UT_WRITE_ENDPOINT;
		req.bRequest = UR_CLEAR_FEATURE;
		USETW(req.wValue, UF_ENDPOINT_HALT);
		req.wIndex[0] = pipe->edesc->bEndpointAddress;
		req.wIndex[1] = 0;
		USETW(req.wLength, 0);

		error = usb2_do_request(f->udev, NULL, &req, NULL);
		if (error == 0) {
			usb2_clear_data_toggle(f->udev, pipe);
		} else {
			error = ENXIO;
		}
		break;

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static int
ugen_set_short_xfer(struct usb2_fifo *f, void *addr)
{
	uint8_t t;

	if (*(int *)addr)
		t = 1;
	else
		t = 0;

	if (f->flag_short == t) {
		/* same value like before - accept */
		return (0);
	}
	if (f->xfer[0] || f->xfer[1]) {
		/* cannot change this during transfer */
		return (EBUSY);
	}
	f->flag_short = t;
	return (0);
}

static int
ugen_set_timeout(struct usb2_fifo *f, void *addr)
{
	f->timeout = *(int *)addr;
	if (f->timeout > 65535) {
		/* limit user input */
		f->timeout = 65535;
	}
	return (0);
}

static int
ugen_get_frame_size(struct usb2_fifo *f, void *addr)
{
	if (f->xfer[0]) {
		*(int *)addr = f->xfer[0]->max_frame_size;
	} else {
		return (EINVAL);
	}
	return (0);
}

static int
ugen_set_buffer_size(struct usb2_fifo *f, void *addr)
{
	uint32_t t;

	if (*(int *)addr < 1024)
		t = 1024;
	else if (*(int *)addr < (256 * 1024))
		t = *(int *)addr;
	else
		t = 256 * 1024;

	if (f->bufsize == t) {
		/* same value like before - accept */
		return (0);
	}
	if (f->xfer[0] || f->xfer[1]) {
		/* cannot change this during transfer */
		return (EBUSY);
	}
	f->bufsize = t;
	return (0);
}

static int
ugen_get_buffer_size(struct usb2_fifo *f, void *addr)
{
	*(int *)addr = f->bufsize;
	return (0);
}

static int
ugen_get_iface_desc(struct usb2_fifo *f,
    struct usb2_interface_descriptor *idesc)
{
	struct usb2_interface *iface;

	iface = usb2_get_iface(f->udev, f->iface_index);
	if (iface && iface->idesc) {
		*idesc = *(iface->idesc);
	} else {
		return (EIO);
	}
	return (0);
}

static int
ugen_get_endpoint_desc(struct usb2_fifo *f,
    struct usb2_endpoint_descriptor *ed)
{
	struct usb2_pipe *pipe;

	pipe = f->priv_sc0;

	if (pipe && pipe->edesc) {
		*ed = *pipe->edesc;
	} else {
		return (EINVAL);
	}
	return (0);
}

static int
ugen_set_power_mode(struct usb2_fifo *f, int mode)
{
	struct usb2_device *udev = f->udev;
	int err;

	if ((udev == NULL) ||
	    (udev->parent_hub == NULL)) {
		return (EINVAL);
	}
	err = priv_check(curthread, PRIV_ROOT);
	if (err) {
		return (err);
	}
	switch (mode) {
	case USB_POWER_MODE_OFF:
		/* clear suspend */
		err = usb2_req_clear_port_feature(udev->parent_hub,
		    NULL, udev->port_no, UHF_PORT_SUSPEND);
		if (err)
			break;

		/* clear port enable */
		err = usb2_req_clear_port_feature(udev->parent_hub,
		    NULL, udev->port_no, UHF_PORT_ENABLE);
		break;

	case USB_POWER_MODE_ON:
		/* enable port */
		err = usb2_req_set_port_feature(udev->parent_hub,
		    NULL, udev->port_no, UHF_PORT_ENABLE);

		/* FALLTHROUGH */

	case USB_POWER_MODE_SAVE:
	case USB_POWER_MODE_RESUME:
		/* TODO: implement USB power save */
		err = usb2_req_clear_port_feature(udev->parent_hub,
		    NULL, udev->port_no, UHF_PORT_SUSPEND);
		break;

	case USB_POWER_MODE_SUSPEND:
		/* TODO: implement USB power save */
		err = usb2_req_set_port_feature(udev->parent_hub,
		    NULL, udev->port_no, UHF_PORT_SUSPEND);
		break;
	default:
		return (EINVAL);
	}

	if (err)
		return (ENXIO);		/* I/O failure */

	udev->power_mode = mode;	/* update copy of power mode */

	return (0);			/* success */
}

static int
ugen_get_power_mode(struct usb2_fifo *f)
{
	struct usb2_device *udev = f->udev;

	if ((udev == NULL) ||
	    (udev->parent_hub == NULL)) {
		return (USB_POWER_MODE_ON);
	}
	return (udev->power_mode);
}

static int
ugen_do_port_feature(struct usb2_fifo *f, uint8_t port_no,
    uint8_t set, uint16_t feature)
{
	struct usb2_device *udev = f->udev;
	struct usb2_hub *hub;
	int err;

	err = priv_check(curthread, PRIV_ROOT);
	if (err) {
		return (err);
	}
	if (port_no == 0) {
		return (EINVAL);
	}
	if ((udev == NULL) ||
	    (udev->hub == NULL)) {
		return (EINVAL);
	}
	hub = udev->hub;

	if (port_no > hub->nports) {
		return (EINVAL);
	}
	if (set)
		err = usb2_req_set_port_feature(udev,
		    NULL, port_no, feature);
	else
		err = usb2_req_clear_port_feature(udev,
		    NULL, port_no, feature);

	if (err)
		return (ENXIO);		/* failure */

	return (0);			/* success */
}

static int
ugen_iface_ioctl(struct usb2_fifo *f, u_long cmd, void *addr, int fflags)
{
	struct usb2_fifo *f_rx;
	struct usb2_fifo *f_tx;
	int error = 0;

	f_rx = f->udev->fifo[(f->fifo_index & ~1) + USB_FIFO_RX];
	f_tx = f->udev->fifo[(f->fifo_index & ~1) + USB_FIFO_TX];

	switch (cmd) {
	case USB_SET_RX_SHORT_XFER:
		if (fflags & FREAD) {
			error = ugen_set_short_xfer(f_rx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_SET_TX_FORCE_SHORT:
		if (fflags & FWRITE) {
			error = ugen_set_short_xfer(f_tx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_SET_RX_TIMEOUT:
		if (fflags & FREAD) {
			error = ugen_set_timeout(f_rx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_SET_TX_TIMEOUT:
		if (fflags & FWRITE) {
			error = ugen_set_timeout(f_tx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_GET_RX_FRAME_SIZE:
		if (fflags & FREAD) {
			error = ugen_get_frame_size(f_rx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_GET_TX_FRAME_SIZE:
		if (fflags & FWRITE) {
			error = ugen_get_frame_size(f_tx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_SET_RX_BUFFER_SIZE:
		if (fflags & FREAD) {
			error = ugen_set_buffer_size(f_rx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_SET_TX_BUFFER_SIZE:
		if (fflags & FWRITE) {
			error = ugen_set_buffer_size(f_tx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_GET_RX_BUFFER_SIZE:
		if (fflags & FREAD) {
			error = ugen_get_buffer_size(f_rx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_GET_TX_BUFFER_SIZE:
		if (fflags & FWRITE) {
			error = ugen_get_buffer_size(f_tx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_GET_RX_INTERFACE_DESC:
		if (fflags & FREAD) {
			error = ugen_get_iface_desc(f_rx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_GET_TX_INTERFACE_DESC:
		if (fflags & FWRITE) {
			error = ugen_get_iface_desc(f_tx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_GET_RX_ENDPOINT_DESC:
		if (fflags & FREAD) {
			error = ugen_get_endpoint_desc(f_rx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_GET_TX_ENDPOINT_DESC:
		if (fflags & FWRITE) {
			error = ugen_get_endpoint_desc(f_tx, addr);
		} else {
			error = EINVAL;
		}
		break;

	case USB_SET_RX_STALL_FLAG:
		if ((fflags & FREAD) && (*(int *)addr)) {
			f_rx->flag_stall = 1;
		}
		break;

	case USB_SET_TX_STALL_FLAG:
		if ((fflags & FWRITE) && (*(int *)addr)) {
			f_tx->flag_stall = 1;
		}
		break;

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static int
ugen_ctrl_ioctl(struct usb2_fifo *f, u_long cmd, void *addr, int fflags)
{
	union {
		struct usb2_interface_descriptor *idesc;
		struct usb2_alt_interface *ai;
		struct usb2_device_descriptor *ddesc;
		struct usb2_config_descriptor *cdesc;
		struct usb2_device_stats *stat;
		uint32_t *ptime;
		void   *addr;
		int    *pint;
	}     u;
	struct usb2_device_descriptor *dtemp;
	struct usb2_config_descriptor *ctemp;
	struct usb2_interface *iface;
	int error = 0;
	uint8_t n;

	u.addr = addr;

	switch (cmd) {
	case USB_DISCOVER:
		usb2_needs_explore_all();
		break;

	case USB_SETDEBUG:
		if (!(fflags & FWRITE)) {
			error = EPERM;
			break;
		}
		usb2_debug = *(int *)addr;
		break;

	case USB_GET_CONFIG:
		*(int *)addr = f->udev->curr_config_index;
		break;

	case USB_SET_CONFIG:
		if (!(fflags & FWRITE)) {
			error = EPERM;
			break;
		}
		error = ugen_set_config(f, *(int *)addr);
		break;

	case USB_GET_ALTINTERFACE:
		iface = usb2_get_iface(f->udev,
		    u.ai->uai_interface_index);
		if (iface && iface->idesc) {
			u.ai->uai_alt_index = iface->alt_index;
		} else {
			error = EINVAL;
		}
		break;

	case USB_SET_ALTINTERFACE:
		if (!(fflags & FWRITE)) {
			error = EPERM;
			break;
		}
		error = ugen_set_interface(f,
		    u.ai->uai_interface_index, u.ai->uai_alt_index);
		break;

	case USB_GET_DEVICE_DESC:
		dtemp = usb2_get_device_descriptor(f->udev);
		if (!dtemp) {
			error = EIO;
			break;
		}
		*u.ddesc = *dtemp;
		break;

	case USB_GET_CONFIG_DESC:
		ctemp = usb2_get_config_descriptor(f->udev);
		if (!ctemp) {
			error = EIO;
			break;
		}
		*u.cdesc = *ctemp;
		break;

	case USB_GET_FULL_DESC:
		error = ugen_get_cdesc(f, addr);
		break;

	case USB_GET_STRING_DESC:
		error = ugen_get_sdesc(f, addr);
		break;

	case USB_REQUEST:
	case USB_DO_REQUEST:
		if (!(fflags & FWRITE)) {
			error = EPERM;
			break;
		}
		error = ugen_do_request(f, addr);
		break;

	case USB_DEVICEINFO:
	case USB_GET_DEVICEINFO:
		error = usb2_gen_fill_deviceinfo(f, addr);
		break;

	case USB_GET_DEVICENAMES:
		error = usb2_gen_fill_devicenames(f, addr);
		break;

	case USB_DEVICESTATS:
		for (n = 0; n != 4; n++) {

			u.stat->uds_requests_fail[n] =
			    f->udev->bus->stats_err.uds_requests[n];

			u.stat->uds_requests_ok[n] =
			    f->udev->bus->stats_ok.uds_requests[n];
		}
		break;

	case USB_DEVICEENUMERATE:
		error = ugen_re_enumerate(f);
		break;

	case USB_GET_PLUGTIME:
		*u.ptime = f->udev->plugtime;
		break;

	case USB_CLAIM_INTERFACE:
	case USB_RELEASE_INTERFACE:
		/* TODO */
		break;

	case USB_IFACE_DRIVER_ACTIVE:
		/* TODO */
		*u.pint = 0;
		break;

	case USB_IFACE_DRIVER_DETACH:
		/* TODO */
		error = priv_check(curthread, PRIV_DRIVER);
		if (error) {
			break;
		}
		error = EINVAL;
		break;

	case USB_SET_POWER_MODE:
		error = ugen_set_power_mode(f, *u.pint);
		break;

	case USB_GET_POWER_MODE:
		*u.pint = ugen_get_power_mode(f);
		break;

	case USB_SET_PORT_ENABLE:
		error = ugen_do_port_feature(f,
		    *u.pint, 1, UHF_PORT_ENABLE);
		break;

	case USB_SET_PORT_DISABLE:
		error = ugen_do_port_feature(f,
		    *u.pint, 0, UHF_PORT_ENABLE);
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}

static int
ugen_ioctl(struct usb2_fifo *f, u_long cmd, void *addr, int fflags,
    struct thread *td)
{
	int error;

	DPRINTFN(6, "cmd=%08lx\n", cmd);
	error = ugen_fs_ioctl(f, cmd, addr, fflags);
	if (error == ENOTTY) {
		if (f->flag_no_uref) {
			mtx_lock(f->priv_mtx);
			error = ugen_iface_ioctl(f, cmd, addr, fflags);
			mtx_unlock(f->priv_mtx);
		} else {
			error = ugen_ctrl_ioctl(f, cmd, addr, fflags);
		}
	}
	DPRINTFN(6, "error=%d\n", error);
	return (error);
}

static void
ugen_default_fs_callback(struct usb2_xfer *xfer)
{
	;				/* workaround for a bug in "indent" */

	DPRINTF("st=%u alen=%u aframes=%u\n",
	    USB_GET_STATE(xfer), xfer->actlen, xfer->aframes);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
		usb2_start_hardware(xfer);
		break;
	default:
		ugen_fs_set_complete(xfer->priv_sc, USB_P2U(xfer->priv_fifo));
		break;
	}
	return;
}
