/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008-2023 Hans Petter Selasky
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

#ifdef USB_GLOBAL_INCLUDE_FILE
#include USB_GLOBAL_INCLUDE_FILE
#else
#ifdef COMPAT_FREEBSD32
#include <sys/abi_compat.h>
#endif
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
#include <sys/conf.h>
#include <sys/fcntl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#define	USB_DEBUG_VAR ugen_debug

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_dev.h>
#include <dev/usb/usb_mbuf.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_request.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_util.h>
#include <dev/usb/usb_hub.h>
#include <dev/usb/usb_generic.h>
#include <dev/usb/usb_transfer.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#endif			/* USB_GLOBAL_INCLUDE_FILE */

#if USB_HAVE_UGEN

/* defines */

#define	UGEN_BULK_FS_BUFFER_SIZE	(64*32)	/* bytes */
#define	UGEN_BULK_HS_BUFFER_SIZE	(1024*32)	/* bytes */
#define	UGEN_HW_FRAMES	50		/* number of milliseconds per transfer */

/* function prototypes */

static usb_callback_t ugen_read_clear_stall_callback;
static usb_callback_t ugen_write_clear_stall_callback;
static usb_callback_t ugen_ctrl_read_callback;
static usb_callback_t ugen_ctrl_write_callback;
static usb_callback_t ugen_isoc_read_callback;
static usb_callback_t ugen_isoc_write_callback;
static usb_callback_t ugen_ctrl_fs_callback;

static usb_fifo_open_t ugen_open;
static usb_fifo_close_t ugen_close;
static usb_fifo_ioctl_t ugen_ioctl;
static usb_fifo_ioctl_t ugen_ioctl_post;
static usb_fifo_cmd_t ugen_start_read;
static usb_fifo_cmd_t ugen_start_write;
static usb_fifo_cmd_t ugen_stop_io;

static int	ugen_transfer_setup(struct usb_fifo *,
		     const struct usb_config *, uint8_t);
static int	ugen_open_pipe_write(struct usb_fifo *);
static int	ugen_open_pipe_read(struct usb_fifo *);
static int	ugen_set_config(struct usb_fifo *, uint8_t);
static int	ugen_set_interface(struct usb_fifo *, uint8_t, uint8_t);
static int	ugen_get_cdesc(struct usb_fifo *, struct usb_gen_descriptor *);
static int	ugen_get_sdesc(struct usb_fifo *, struct usb_gen_descriptor *);
static int	ugen_get_iface_driver(struct usb_fifo *f, struct usb_gen_descriptor *ugd);
#ifdef COMPAT_FREEBSD32
static int	ugen_get32(u_long cmd, struct usb_fifo *f, struct usb_gen_descriptor32 *ugd32);
#endif
static int	ugen_re_enumerate(struct usb_fifo *);
static int	ugen_iface_ioctl(struct usb_fifo *, u_long, void *, int);
static uint8_t	ugen_fs_get_complete(struct usb_fifo *, uint8_t *);
static int	ugen_fs_uninit(struct usb_fifo *f);
static int	ugen_fs_copyin(struct usb_fifo *, uint8_t, struct usb_fs_endpoint*);
static uint8_t	ugen_fifo_in_use(struct usb_fifo *, int fflags);

/* structures */

struct usb_fifo_methods usb_ugen_methods = {
	.f_open = &ugen_open,
	.f_close = &ugen_close,
	.f_ioctl = &ugen_ioctl,
	.f_ioctl_post = &ugen_ioctl_post,
	.f_start_read = &ugen_start_read,
	.f_stop_read = &ugen_stop_io,
	.f_start_write = &ugen_start_write,
	.f_stop_write = &ugen_stop_io,
};

#ifdef USB_DEBUG
static int ugen_debug = 0;

static SYSCTL_NODE(_hw_usb, OID_AUTO, ugen, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "USB generic");
SYSCTL_INT(_hw_usb_ugen, OID_AUTO, debug, CTLFLAG_RWTUN, &ugen_debug,
    0, "Debug level");
#endif

/* prototypes */

static int
ugen_transfer_setup(struct usb_fifo *f,
    const struct usb_config *setup, uint8_t n_setup)
{
	struct usb_endpoint *ep = usb_fifo_softc(f);
	struct usb_device *udev = f->udev;
	uint8_t iface_index = ep->iface_index;
	int error;

	mtx_unlock(f->priv_mtx);

	/*
	 * "usbd_transfer_setup()" can sleep so one needs to make a wrapper,
	 * exiting the mutex and checking things
	 */
	error = usbd_transfer_setup(udev, &iface_index, f->xfer,
	    setup, n_setup, f, f->priv_mtx);
	if (error == 0) {
		if (f->xfer[0]->nframes == 1) {
			error = usb_fifo_alloc_buffer(f,
			    f->xfer[0]->max_data_length, 2);
		} else {
			error = usb_fifo_alloc_buffer(f,
			    f->xfer[0]->max_frame_size,
			    2 * f->xfer[0]->nframes);
		}
		if (error) {
			usbd_transfer_unsetup(f->xfer, n_setup);
		}
	}
	mtx_lock(f->priv_mtx);

	return (error);
}

static int
ugen_open(struct usb_fifo *f, int fflags)
{
	struct usb_endpoint *ep = usb_fifo_softc(f);
	struct usb_endpoint_descriptor *ed = ep->edesc;
	uint8_t type;

	DPRINTFN(1, "flag=0x%x pid=%d name=%s\n", fflags,
	    curthread->td_proc->p_pid, curthread->td_proc->p_comm);

	mtx_lock(f->priv_mtx);
	switch (usbd_get_speed(f->udev)) {
	case USB_SPEED_LOW:
	case USB_SPEED_FULL:
		f->nframes = UGEN_HW_FRAMES;
		f->bufsize = UGEN_BULK_FS_BUFFER_SIZE;
		break;
	default:
		f->nframes = UGEN_HW_FRAMES * 8;
		f->bufsize = UGEN_BULK_HS_BUFFER_SIZE;
		break;
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
ugen_close(struct usb_fifo *f, int fflags)
{

	DPRINTFN(1, "flag=0x%x pid=%d name=%s\n", fflags,
	    curthread->td_proc->p_pid, curthread->td_proc->p_comm);

	/* cleanup */

	mtx_lock(f->priv_mtx);
	usbd_transfer_stop(f->xfer[0]);
	usbd_transfer_stop(f->xfer[1]);
	mtx_unlock(f->priv_mtx);

	usbd_transfer_unsetup(f->xfer, 2);
	usb_fifo_free_buffer(f);

	if (ugen_fs_uninit(f)) {
		/* ignore any errors - we are closing */
		DPRINTFN(6, "no FIFOs\n");
	}
}

static int
ugen_open_pipe_write(struct usb_fifo *f)
{
	struct usb_config usb_config[2];
	struct usb_endpoint *ep = usb_fifo_softc(f);
	struct usb_endpoint_descriptor *ed = ep->edesc;

	USB_MTX_ASSERT(f->priv_mtx, MA_OWNED);

	if (f->xfer[0] || f->xfer[1]) {
		/* transfers are already opened */
		return (0);
	}
	memset(usb_config, 0, sizeof(usb_config));

	usb_config[1].type = UE_CONTROL;
	usb_config[1].endpoint = 0;
	usb_config[1].direction = UE_DIR_ANY;
	usb_config[1].timeout = 1000;	/* 1 second */
	usb_config[1].interval = 50;/* 50 milliseconds */
	usb_config[1].bufsize = sizeof(struct usb_device_request);
	usb_config[1].callback = &ugen_write_clear_stall_callback;
	usb_config[1].usb_mode = USB_MODE_HOST;

	usb_config[0].type = ed->bmAttributes & UE_XFERTYPE;
	usb_config[0].endpoint = ed->bEndpointAddress & UE_ADDR;
	usb_config[0].stream_id = 0;	/* XXX support more stream ID's */
	usb_config[0].direction = UE_DIR_TX;
	usb_config[0].interval = USB_DEFAULT_INTERVAL;
	usb_config[0].flags.proxy_buffer = 1;
	usb_config[0].usb_mode = USB_MODE_DUAL;	/* both modes */

	switch (ed->bmAttributes & UE_XFERTYPE) {
	case UE_INTERRUPT:
	case UE_BULK:
		if (f->flag_short) {
			usb_config[0].flags.force_short_xfer = 1;
		}
		usb_config[0].callback = &ugen_ctrl_write_callback;
		usb_config[0].timeout = f->timeout;
		usb_config[0].frames = 1;
		usb_config[0].bufsize = f->bufsize;
		if (ugen_transfer_setup(f, usb_config, 2)) {
			return (EIO);
		}
		/* first transfer does not clear stall */
		f->flag_stall = 0;
		break;

	case UE_ISOCHRONOUS:
		usb_config[0].flags.short_xfer_ok = 1;
		usb_config[0].bufsize = 0;	/* use default */
		usb_config[0].frames = f->nframes;
		usb_config[0].callback = &ugen_isoc_write_callback;
		usb_config[0].timeout = 0;

		/* clone configuration */
		usb_config[1] = usb_config[0];

		if (ugen_transfer_setup(f, usb_config, 2)) {
			return (EIO);
		}
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
ugen_open_pipe_read(struct usb_fifo *f)
{
	struct usb_config usb_config[2];
	struct usb_endpoint *ep = usb_fifo_softc(f);
	struct usb_endpoint_descriptor *ed = ep->edesc;

	USB_MTX_ASSERT(f->priv_mtx, MA_OWNED);

	if (f->xfer[0] || f->xfer[1]) {
		/* transfers are already opened */
		return (0);
	}
	memset(usb_config, 0, sizeof(usb_config));

	usb_config[1].type = UE_CONTROL;
	usb_config[1].endpoint = 0;
	usb_config[1].direction = UE_DIR_ANY;
	usb_config[1].timeout = 1000;	/* 1 second */
	usb_config[1].interval = 50;/* 50 milliseconds */
	usb_config[1].bufsize = sizeof(struct usb_device_request);
	usb_config[1].callback = &ugen_read_clear_stall_callback;
	usb_config[1].usb_mode = USB_MODE_HOST;

	usb_config[0].type = ed->bmAttributes & UE_XFERTYPE;
	usb_config[0].endpoint = ed->bEndpointAddress & UE_ADDR;
	usb_config[0].stream_id = 0;	/* XXX support more stream ID's */
	usb_config[0].direction = UE_DIR_RX;
	usb_config[0].interval = USB_DEFAULT_INTERVAL;
	usb_config[0].flags.proxy_buffer = 1;
	usb_config[0].usb_mode = USB_MODE_DUAL;	/* both modes */

	switch (ed->bmAttributes & UE_XFERTYPE) {
	case UE_INTERRUPT:
	case UE_BULK:
		if (f->flag_short) {
			usb_config[0].flags.short_xfer_ok = 1;
		}
		usb_config[0].timeout = f->timeout;
		usb_config[0].frames = 1;
		usb_config[0].callback = &ugen_ctrl_read_callback;
		usb_config[0].bufsize = f->bufsize;

		if (ugen_transfer_setup(f, usb_config, 2)) {
			return (EIO);
		}
		/* first transfer does not clear stall */
		f->flag_stall = 0;
		break;

	case UE_ISOCHRONOUS:
		usb_config[0].flags.short_xfer_ok = 1;
		usb_config[0].bufsize = 0;	/* use default */
		usb_config[0].frames = f->nframes;
		usb_config[0].callback = &ugen_isoc_read_callback;
		usb_config[0].timeout = 0;

		/* clone configuration */
		usb_config[1] = usb_config[0];

		if (ugen_transfer_setup(f, usb_config, 2)) {
			return (EIO);
		}
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

static void
ugen_start_read(struct usb_fifo *f)
{
	/* check that pipes are open */
	if (ugen_open_pipe_read(f)) {
		/* signal error */
		usb_fifo_put_data_error(f);
	}
	/* start transfers */
	usbd_transfer_start(f->xfer[0]);
	usbd_transfer_start(f->xfer[1]);
}

static void
ugen_start_write(struct usb_fifo *f)
{
	/* check that pipes are open */
	if (ugen_open_pipe_write(f)) {
		/* signal error */
		usb_fifo_get_data_error(f);
	}
	/* start transfers */
	usbd_transfer_start(f->xfer[0]);
	usbd_transfer_start(f->xfer[1]);
}

static void
ugen_stop_io(struct usb_fifo *f)
{
	/* stop transfers */
	usbd_transfer_stop(f->xfer[0]);
	usbd_transfer_stop(f->xfer[1]);
}

static void
ugen_ctrl_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct usb_fifo *f = usbd_xfer_softc(xfer);
	struct usb_mbuf *m;

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
		usb_fifo_put_data(f, xfer->frbuffers, 0,
		    xfer->actlen, 1);

	case USB_ST_SETUP:
		if (f->flag_stall) {
			usbd_transfer_start(f->xfer[1]);
			break;
		}
		USB_IF_POLL(&f->free_q, m);
		if (m) {
			usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
			usbd_transfer_submit(xfer);
		}
		break;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* send a zero length packet to userland */
			usb_fifo_put_data(f, xfer->frbuffers, 0, 0, 1);
			f->flag_stall = 1;
			f->fifo_zlp = 0;
			usbd_transfer_start(f->xfer[1]);
		}
		break;
	}
}

static void
ugen_ctrl_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct usb_fifo *f = usbd_xfer_softc(xfer);
	usb_frlength_t actlen;

	DPRINTFN(4, "actlen=%u, aframes=%u\n", xfer->actlen, xfer->aframes);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
		/*
		 * If writing is in stall, just jump to clear stall
		 * callback and solve the situation.
		 */
		if (f->flag_stall) {
			usbd_transfer_start(f->xfer[1]);
			break;
		}
		/*
		 * Write data, setup and perform hardware transfer.
		 */
		if (usb_fifo_get_data(f, xfer->frbuffers, 0,
		    xfer->max_data_length, &actlen, 0)) {
			usbd_xfer_set_frame_len(xfer, 0, actlen);
			usbd_transfer_submit(xfer);
		}
		break;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			f->flag_stall = 1;
			usbd_transfer_start(f->xfer[1]);
		}
		break;
	}
}

static void
ugen_read_clear_stall_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct usb_fifo *f = usbd_xfer_softc(xfer);
	struct usb_xfer *xfer_other = f->xfer[0];

	if (f->flag_stall == 0) {
		/* nothing to do */
		return;
	}
	if (usbd_clear_stall_callback(xfer, xfer_other)) {
		DPRINTFN(5, "f=%p: stall cleared\n", f);
		f->flag_stall = 0;
		usbd_transfer_start(xfer_other);
	}
}

static void
ugen_write_clear_stall_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct usb_fifo *f = usbd_xfer_softc(xfer);
	struct usb_xfer *xfer_other = f->xfer[0];

	if (f->flag_stall == 0) {
		/* nothing to do */
		return;
	}
	if (usbd_clear_stall_callback(xfer, xfer_other)) {
		DPRINTFN(5, "f=%p: stall cleared\n", f);
		f->flag_stall = 0;
		usbd_transfer_start(xfer_other);
	}
}

static void
ugen_isoc_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct usb_fifo *f = usbd_xfer_softc(xfer);
	usb_frlength_t offset;
	usb_frcount_t n;

	DPRINTFN(4, "actlen=%u, aframes=%u\n", xfer->actlen, xfer->aframes);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTFN(6, "actlen=%d\n", xfer->actlen);

		offset = 0;

		for (n = 0; n != xfer->aframes; n++) {
			usb_fifo_put_data(f, xfer->frbuffers, offset,
			    xfer->frlengths[n], 1);
			offset += xfer->max_frame_size;
		}

	case USB_ST_SETUP:
tr_setup:
		for (n = 0; n != xfer->nframes; n++) {
			/* setup size for next transfer */
			usbd_xfer_set_frame_len(xfer, n, xfer->max_frame_size);
		}
		usbd_transfer_submit(xfer);
		break;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			break;
		}
		goto tr_setup;
	}
}

static void
ugen_isoc_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct usb_fifo *f = usbd_xfer_softc(xfer);
	usb_frlength_t actlen;
	usb_frlength_t offset;
	usb_frcount_t n;

	DPRINTFN(4, "actlen=%u, aframes=%u\n", xfer->actlen, xfer->aframes);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
tr_setup:
		offset = 0;
		for (n = 0; n != xfer->nframes; n++) {
			if (usb_fifo_get_data(f, xfer->frbuffers, offset,
			    xfer->max_frame_size, &actlen, 1)) {
				usbd_xfer_set_frame_len(xfer, n, actlen);
				offset += actlen;
			} else {
				break;
			}
		}

		for (; n != xfer->nframes; n++) {
			/* fill in zero frames */
			usbd_xfer_set_frame_len(xfer, n, 0);
		}
		usbd_transfer_submit(xfer);
		break;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			break;
		}
		goto tr_setup;
	}
}

static int
ugen_set_config(struct usb_fifo *f, uint8_t index)
{
	DPRINTFN(2, "index %u\n", index);

	if (f->udev->flags.usb_mode != USB_MODE_HOST) {
		/* not possible in device side mode */
		return (ENOTTY);
	}

	/* make sure all FIFO's are gone */
	/* else there can be a deadlock */
	if (ugen_fs_uninit(f)) {
		/* ignore any errors */
		DPRINTFN(6, "no FIFOs\n");
	}

	if (usbd_start_set_config(f->udev, index) != 0)
		return (EIO);

	return (0);
}

static int
ugen_set_interface(struct usb_fifo *f,
    uint8_t iface_index, uint8_t alt_index)
{
	DPRINTFN(2, "%u, %u\n", iface_index, alt_index);

	if (f->udev->flags.usb_mode != USB_MODE_HOST) {
		/* not possible in device side mode */
		return (ENOTTY);
	}
	/* make sure all FIFO's are gone */
	/* else there can be a deadlock */
	if (ugen_fs_uninit(f)) {
		/* ignore any errors */
		DPRINTFN(6, "no FIFOs\n");
	}
	/* change setting - will free generic FIFOs, if any */
	if (usbd_set_alt_interface_index(f->udev, iface_index, alt_index)) {
		return (EIO);
	}
	/* probe and attach */
	if (usb_probe_and_attach(f->udev, iface_index)) {
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
ugen_get_cdesc(struct usb_fifo *f, struct usb_gen_descriptor *ugd)
{
	struct usb_config_descriptor *cdesc;
	struct usb_device *udev = f->udev;
	int error;
	uint16_t len;
	uint8_t free_data;

	DPRINTFN(6, "\n");

	if (ugd->ugd_data == NULL) {
		/* userland pointer should not be zero */
		return (EINVAL);
	}
	if ((ugd->ugd_config_index == USB_UNCONFIG_INDEX) ||
	    (ugd->ugd_config_index == udev->curr_config_index)) {
		cdesc = usbd_get_config_descriptor(udev);
		if (cdesc == NULL)
			return (ENXIO);
		free_data = 0;

	} else {
#if (USB_HAVE_FIXED_CONFIG == 0)
		if (usbd_req_get_config_desc_full(udev,
		    NULL, &cdesc, ugd->ugd_config_index)) {
			return (ENXIO);
		}
		free_data = 1;
#else
		/* configuration descriptor data is shared */
		return (EINVAL);
#endif
	}

	len = UGETW(cdesc->wTotalLength);
	if (len > ugd->ugd_maxlen) {
		len = ugd->ugd_maxlen;
	}
	DPRINTFN(6, "len=%u\n", len);

	ugd->ugd_actlen = len;
	ugd->ugd_offset = 0;

	error = copyout(cdesc, ugd->ugd_data, len);

	if (free_data)
		usbd_free_config_desc(udev, cdesc);

	return (error);
}

static int
ugen_get_sdesc(struct usb_fifo *f, struct usb_gen_descriptor *ugd)
{
	void *ptr;
	uint16_t size;
	int error;
	uint8_t do_unlock;

	/* Protect scratch area */
	do_unlock = usbd_ctrl_lock(f->udev);

	ptr = f->udev->scratch.data;
	size = sizeof(f->udev->scratch.data);

	if (usbd_req_get_string_desc(f->udev, NULL, ptr,
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
	if (do_unlock)
		usbd_ctrl_unlock(f->udev);

	return (error);
}

/*------------------------------------------------------------------------*
 *	ugen_get_iface_driver
 *
 * This function generates an USB interface description for userland.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static int
ugen_get_iface_driver(struct usb_fifo *f, struct usb_gen_descriptor *ugd)
{
	struct usb_device *udev = f->udev;
	struct usb_interface *iface;
	const char *ptr;
	const char *desc;
	unsigned len;
	unsigned maxlen;
	char buf[128];
	int error;

	DPRINTFN(6, "\n");

	if ((ugd->ugd_data == NULL) || (ugd->ugd_maxlen == 0)) {
		/* userland pointer should not be zero */
		return (EINVAL);
	}

	iface = usbd_get_iface(udev, ugd->ugd_iface_index);
	if ((iface == NULL) || (iface->idesc == NULL)) {
		/* invalid interface index */
		return (EINVAL);
	}

	/* read out device nameunit string, if any */
	if ((iface->subdev != NULL) &&
	    device_is_attached(iface->subdev) &&
	    (ptr = device_get_nameunit(iface->subdev)) &&
	    (desc = device_get_desc(iface->subdev))) {
		/* print description */
		snprintf(buf, sizeof(buf), "%s: <%s>", ptr, desc);

		/* range checks */
		maxlen = ugd->ugd_maxlen - 1;
		len = strlen(buf);
		if (len > maxlen)
			len = maxlen;

		/* update actual length, including terminating zero */
		ugd->ugd_actlen = len + 1;

		/* copy out interface description */
		error = copyout(buf, ugd->ugd_data, ugd->ugd_actlen);
	} else {
		/* zero length string is default */
		error = copyout("", ugd->ugd_data, 1);
	}
	return (error);
}

/*------------------------------------------------------------------------*
 *	ugen_fill_deviceinfo
 *
 * This function dumps information about an USB device to the
 * structure pointed to by the "di" argument.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
int
ugen_fill_deviceinfo(struct usb_fifo *f, struct usb_device_info *di)
{
	struct usb_device *udev;
	struct usb_device *hub;

	udev = f->udev;

	bzero(di, sizeof(di[0]));

	di->udi_bus = device_get_unit(udev->bus->bdev);
	di->udi_addr = udev->address;
	di->udi_index = udev->device_index;
	strlcpy(di->udi_serial, usb_get_serial(udev), sizeof(di->udi_serial));
	strlcpy(di->udi_vendor, usb_get_manufacturer(udev), sizeof(di->udi_vendor));
	strlcpy(di->udi_product, usb_get_product(udev), sizeof(di->udi_product));
	usb_printbcd(di->udi_release, sizeof(di->udi_release),
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
	di->udi_mode = udev->flags.usb_mode;
	di->udi_power_mode = udev->power_mode;
	di->udi_suspended = udev->flags.peer_suspended;

	hub = udev->parent_hub;
	if (hub) {
		di->udi_hubaddr = hub->address;
		di->udi_hubindex = hub->device_index;
		di->udi_hubport = udev->port_no;
	}
	return (0);
}

int
ugen_do_request(struct usb_fifo *f, struct usb_ctl_request *ur)
{
	int error;
	uint16_t len;
	uint16_t actlen;

	if (usb_check_request(f->udev, &ur->ucr_request)) {
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
	error = usbd_do_request_flags
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

#ifdef COMPAT_FREEBSD32
static int
ugen_do_request32(struct usb_fifo *f, struct usb_ctl_request32 *ur32)
{
	struct usb_ctl_request ur;
	int error;

	PTRIN_CP(*ur32, ur, ucr_data);
	CP(*ur32, ur, ucr_flags);
	CP(*ur32, ur, ucr_actlen);
	CP(*ur32, ur, ucr_addr);
	CP(*ur32, ur, ucr_request);

	error = ugen_do_request(f, &ur);

	/* Don't update ucr_data pointer */
	CP(ur, *ur32, ucr_flags);
	CP(ur, *ur32, ucr_actlen);
	CP(ur, *ur32, ucr_addr);
	CP(ur, *ur32, ucr_request);

	return (error);
}
#endif

/*------------------------------------------------------------------------
 *	ugen_re_enumerate
 *------------------------------------------------------------------------*/
static int
ugen_re_enumerate(struct usb_fifo *f)
{
	struct usb_device *udev = f->udev;
	int error;

	/*
	 * This request can be useful for testing USB drivers:
	 */
	error = priv_check(curthread, PRIV_DRIVER);
	if (error) {
		return (error);
	}
	if (udev->flags.usb_mode != USB_MODE_HOST) {
		/* not possible in device side mode */
		DPRINTFN(6, "device mode\n");
		return (ENOTTY);
	}
	/* make sure all FIFO's are gone */
	/* else there can be a deadlock */
	if (ugen_fs_uninit(f)) {
		/* ignore any errors */
		DPRINTFN(6, "no FIFOs\n");
	}
	/* start re-enumeration of device */
	usbd_start_re_enumerate(udev);
	return (0);
}

static int
ugen_fs_init(struct usb_fifo *f,
    struct usb_fs_endpoint *fs_ep_ptr, usb_size_t fs_ep_sz,
    int fflags, uint8_t ep_index_max)
{
	int error;

	/* verify input parameters */
	if (fs_ep_ptr == NULL || ep_index_max > USB_FS_XFER_MAX)
		return (EINVAL);

	if (f->fs_ep_max != 0)
		return (EBUSY);

	if (f->dev_ep_index != 0 || ep_index_max == 0)
		return (EINVAL);

	if (ugen_fifo_in_use(f, fflags))
		return (EBUSY);

	error = usb_fifo_alloc_buffer(f, 1, ep_index_max);
	if (error == 0) {
		mtx_lock(f->priv_mtx);
		f->fs_ep_max = ep_index_max;
		f->fs_ep_ptr = fs_ep_ptr;
		f->fs_ep_sz = fs_ep_sz;
		mtx_unlock(f->priv_mtx);
	}
	return (error);
}

int
ugen_fs_uninit(struct usb_fifo *f)
{
	if (f->fs_ep_max == 0)
		return (EINVAL);

	/*
	 * Prevent calls into the fast-path code, by setting fs_ep_max
	 * to zero:
	 */
	sx_xlock(&f->fs_fastpath_lock);
	mtx_lock(f->priv_mtx);
	f->fs_ep_max = 0;
	mtx_unlock(f->priv_mtx);
	sx_xunlock(&f->fs_fastpath_lock);

	usbd_transfer_unsetup(f->fs_xfer, USB_FS_XFER_MAX);

	mtx_lock(f->priv_mtx);
	f->fs_ep_ptr = NULL;
	f->flag_iscomplete = 0;
	mtx_unlock(f->priv_mtx);

	usb_fifo_free_buffer(f);
	return (0);
}

static int
usb_fs_open(struct usb_fifo *f, struct usb_fs_open *popen,
    int fflags, usb_stream_t stream_id)
{
	struct usb_config usb_config[1] = {};
	struct usb_endpoint *ep;
	struct usb_endpoint_descriptor *ed;
	uint8_t iface_index;
	uint8_t pre_scale;
	uint8_t isread;
	int error;

	if (popen->ep_index >= f->fs_ep_max)
		return (EINVAL);

	if (f->fs_xfer[popen->ep_index] != NULL)
		return (EBUSY);

	if (popen->max_bufsize > USB_FS_MAX_BUFSIZE)
		popen->max_bufsize = USB_FS_MAX_BUFSIZE;

	if (popen->max_frames & USB_FS_MAX_FRAMES_PRE_SCALE) {
		pre_scale = 1;
		popen->max_frames &= ~USB_FS_MAX_FRAMES_PRE_SCALE;
	} else {
		pre_scale = 0;
	}

	if (popen->max_frames > USB_FS_MAX_FRAMES)
		popen->max_frames = USB_FS_MAX_FRAMES;

	if (popen->max_frames == 0)
		return (EINVAL);

	ep = usbd_get_ep_by_addr(f->udev, popen->ep_no);
	if (ep == NULL)
		return (EINVAL);

	ed = ep->edesc;
	if (ed == NULL)
		return (ENXIO);

	iface_index = ep->iface_index;

	usb_config[0].type = ed->bmAttributes & UE_XFERTYPE;
	usb_config[0].endpoint = ed->bEndpointAddress & UE_ADDR;
	usb_config[0].direction = ed->bEndpointAddress & (UE_DIR_OUT | UE_DIR_IN);
	usb_config[0].interval = USB_DEFAULT_INTERVAL;
	usb_config[0].flags.proxy_buffer = 1;
	if (pre_scale != 0)
		usb_config[0].flags.pre_scale_frames = 1;

	usb_config[0].callback = &ugen_ctrl_fs_callback;
	usb_config[0].timeout = 0;	/* no timeout */
	usb_config[0].frames = popen->max_frames;
	usb_config[0].bufsize = popen->max_bufsize;
	usb_config[0].usb_mode = USB_MODE_DUAL;	/* both modes */
	usb_config[0].stream_id = stream_id;

	if (usb_config[0].type == UE_CONTROL) {
		if (f->udev->flags.usb_mode != USB_MODE_HOST)
			return (EINVAL);
	} else {
		isread = ((usb_config[0].endpoint &
		    (UE_DIR_IN | UE_DIR_OUT)) == UE_DIR_IN);

		if (f->udev->flags.usb_mode != USB_MODE_HOST)
			isread = !isread;

		/* check permissions */
		if (isread) {
			if (!(fflags & FREAD))
				return (EPERM);
		} else {
			if (!(fflags & FWRITE))
				return (EPERM);
		}
	}
	error = usbd_transfer_setup(f->udev, &iface_index,
	    f->fs_xfer + popen->ep_index, usb_config, 1,
	    f, f->priv_mtx);
	if (error == 0) {
		/* update maximums */
		popen->max_packet_length =
		    f->fs_xfer[popen->ep_index]->max_frame_size;
		popen->max_bufsize =
		    f->fs_xfer[popen->ep_index]->max_data_length;
		/* update number of frames */
		popen->max_frames =
		    f->fs_xfer[popen->ep_index]->nframes;
		/* store index of endpoint */
		f->fs_xfer[popen->ep_index]->priv_fifo =
		    ((uint8_t *)0) + popen->ep_index;
	} else {
		error = ENOMEM;
	}
	return (error);
}

static int
usb_fs_close(struct usb_fifo *f, struct usb_fs_close *pclose)
{
	struct usb_xfer *xfer;

	if (pclose->ep_index >= f->fs_ep_max)
		return (EINVAL);

	/*
	 * Prevent calls into the fast-path code, by setting the
	 * fs_xfer[] in question to NULL:
	 */
	sx_xlock(&f->fs_fastpath_lock);
	mtx_lock(f->priv_mtx);
	xfer = f->fs_xfer[pclose->ep_index];
	f->fs_xfer[pclose->ep_index] = NULL;
	mtx_unlock(f->priv_mtx);
	sx_xunlock(&f->fs_fastpath_lock);

	if (xfer == NULL)
		return (EINVAL);

	usbd_transfer_unsetup(&xfer, 1);
	return (0);
}

static int
usb_fs_clear_stall_sync(struct usb_fifo *f, struct usb_fs_clear_stall_sync *pstall)
{
	struct usb_device_request req;
	struct usb_endpoint *ep;
	int error;

	if (pstall->ep_index >= f->fs_ep_max)
		return (EINVAL);

	if (f->fs_xfer[pstall->ep_index] == NULL)
		return (EINVAL);

	if (f->udev->flags.usb_mode != USB_MODE_HOST)
		return (EINVAL);

	mtx_lock(f->priv_mtx);
	error = usbd_transfer_pending(f->fs_xfer[pstall->ep_index]);
	mtx_unlock(f->priv_mtx);

	if (error)
		return (EBUSY);

	ep = f->fs_xfer[pstall->ep_index]->endpoint;

	/* setup a clear-stall packet */
	req.bmRequestType = UT_WRITE_ENDPOINT;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, UF_ENDPOINT_HALT);
	req.wIndex[0] = ep->edesc->bEndpointAddress;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);

	error = usbd_do_request(f->udev, NULL, &req, NULL);
	if (error == 0) {
		usbd_clear_data_toggle(f->udev, ep);
	} else {
		error = ENXIO;
	}
	return (error);
}

static uint8_t
ugen_fs_get_complete(struct usb_fifo *f, uint8_t *pindex)
{
	struct usb_mbuf *m;

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
ugen_fs_set_complete(struct usb_fifo *f, uint8_t index)
{
	struct usb_mbuf *m;

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

	usb_fifo_wakeup(f);
}

static int
ugen_fs_getbuffer(void **uptrp, struct usb_fifo *f, void *buffer,
    usb_frcount_t n)
{
	union {
		void **ppBuffer;
#ifdef COMPAT_FREEBSD32
		uint32_t *ppBuffer32;
#endif
	} u;
#ifdef COMPAT_FREEBSD32
	uint32_t uptr32;
#endif

	u.ppBuffer = buffer;
	switch (f->fs_ep_sz) {
	case sizeof(struct usb_fs_endpoint):
		if (fueword(u.ppBuffer + n, (long *)uptrp) != 0)
			return (EFAULT);
		return (0);
#ifdef COMPAT_FREEBSD32
	case sizeof(struct usb_fs_endpoint32):
		if (fueword32(u.ppBuffer32 + n, &uptr32) != 0)
			return (EFAULT);
		*uptrp = PTRIN(uptr32);
		return (0);
#endif
	default:
		panic("%s: unhandled fs_ep_sz %#x", __func__, f->fs_ep_sz);
	}
}

static int
ugen_fs_copy_in(struct usb_fifo *f, uint8_t ep_index)
{
	struct usb_device_request *req;
	struct usb_xfer *xfer;
	struct usb_fs_endpoint fs_ep;
	void *uaddr;			/* userland pointer */
	void *kaddr;
	usb_frlength_t offset;
	usb_frlength_t rem;
	usb_frcount_t n;
	uint32_t length;
	int error;
	uint8_t isread;

	mtx_lock(f->priv_mtx);
	if (ep_index >= f->fs_ep_max) {
		mtx_unlock(f->priv_mtx);
		return (EINVAL);
	}
	xfer = f->fs_xfer[ep_index];
	if (xfer == NULL) {
		mtx_unlock(f->priv_mtx);
		return (EINVAL);
	}
	if (usbd_transfer_pending(xfer)) {
		mtx_unlock(f->priv_mtx);
		return (EBUSY);		/* should not happen */
	}
	mtx_unlock(f->priv_mtx);

	error = ugen_fs_copyin(f, ep_index, &fs_ep);
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
	error = ugen_fs_getbuffer(&uaddr, f, fs_ep.ppBuffer, 0);
	if (error) {
		return (error);
	}
	/* reset first frame */
	usbd_xfer_set_frame_offset(xfer, 0, 0);

	if (xfer->flags_int.control_xfr) {
		req = xfer->frbuffers[0].buffer;

		if (fueword32(fs_ep.pLength, &length) != 0) {
			return (EFAULT);
		}
		if (length != sizeof(*req)) {
			xfer->error = USB_ERR_INVAL;
			goto complete;
		}
		if (length != 0) {
			error = copyin(uaddr, req, length);
			if (error) {
				return (error);
			}
		}
		if (usb_check_request(f->udev, req)) {
			xfer->error = USB_ERR_INVAL;
			goto complete;
		}
		usbd_xfer_set_frame_len(xfer, 0, length);

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

	rem = usbd_xfer_max_len(xfer);
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
		usbd_xfer_set_stall(xfer);
	else
		xfer->flags.stall_pipe = 0;

	for (; n != xfer->nframes; n++) {
		if (fueword32(fs_ep.pLength + n, &length) != 0) {
			break;
		}
		usbd_xfer_set_frame_len(xfer, n, length);

		if (length > rem) {
			xfer->error = USB_ERR_INVAL;
			goto complete;
		}
		rem -= length;

		if (!isread) {
			/* we need to know the source buffer */
			error = ugen_fs_getbuffer(&uaddr, f, fs_ep.ppBuffer, n);
			if (error) {
				break;
			}
			if (xfer->flags_int.isochronous_xfr) {
				/* get kernel buffer address */
				kaddr = xfer->frbuffers[0].buffer;
				kaddr = USB_ADD_BYTES(kaddr, offset);
			} else {
				/* set current frame offset */
				usbd_xfer_set_frame_offset(xfer, offset, n);

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

static struct usb_fs_endpoint *
ugen_fs_ep_uptr(struct usb_fifo *f, uint8_t ep_index)
{
	return ((struct usb_fs_endpoint *)
	    ((char *)f->fs_ep_ptr + (ep_index * f->fs_ep_sz)));
}

static int
ugen_fs_copyin(struct usb_fifo *f, uint8_t ep_index,
    struct usb_fs_endpoint* fs_ep)
{
#ifdef COMPAT_FREEBSD32
	struct usb_fs_endpoint32 fs_ep32;
#endif
	int error;

	switch (f->fs_ep_sz) {
	case sizeof(struct usb_fs_endpoint):
		error = copyin(ugen_fs_ep_uptr(f, ep_index), fs_ep,
		    f->fs_ep_sz);
		if (error != 0)
			return (error);
		break;

#ifdef COMPAT_FREEBSD32
	case sizeof(struct usb_fs_endpoint32):
		error = copyin(ugen_fs_ep_uptr(f, ep_index), &fs_ep32,
		    f->fs_ep_sz);
		if (error != 0)
			return (error);
		PTRIN_CP(fs_ep32, *fs_ep, ppBuffer);
		PTRIN_CP(fs_ep32, *fs_ep, pLength);
		CP(fs_ep32, *fs_ep, nFrames);
		CP(fs_ep32, *fs_ep, aFrames);
		CP(fs_ep32, *fs_ep, flags);
		CP(fs_ep32, *fs_ep, timeout);
		CP(fs_ep32, *fs_ep, isoc_time_complete);
		CP(fs_ep32, *fs_ep, status);
		break;
#endif
	default:
		panic("%s: unhandled fs_ep_sz %#x", __func__, f->fs_ep_sz);
	}

	return (0);
}

static int
ugen_fs_update(const struct usb_fs_endpoint *fs_ep,
    struct usb_fifo *f, uint8_t ep_index)
{
	union {
		struct usb_fs_endpoint *fs_ep_uptr;
#ifdef COMPAT_FREEBSD32
		struct usb_fs_endpoint32 *fs_ep_uptr32;
#endif
	} u;
	uint32_t *aFrames_uptr;
	uint16_t *isoc_time_complete_uptr;
	int *status_uptr;

	switch (f->fs_ep_sz) {
	case sizeof(struct usb_fs_endpoint):
		u.fs_ep_uptr = ugen_fs_ep_uptr(f, ep_index);
		aFrames_uptr = &u.fs_ep_uptr->aFrames;
		isoc_time_complete_uptr = &u.fs_ep_uptr->isoc_time_complete;
		status_uptr = &u.fs_ep_uptr->status;
		break;
#ifdef COMPAT_FREEBSD32
	case sizeof(struct usb_fs_endpoint32):
		u.fs_ep_uptr32 = (struct usb_fs_endpoint32 *)
		    ugen_fs_ep_uptr(f, ep_index);
		aFrames_uptr = &u.fs_ep_uptr32->aFrames;
		isoc_time_complete_uptr = &u.fs_ep_uptr32->isoc_time_complete;
		status_uptr = &u.fs_ep_uptr32->status;
		break;
#endif
	default:
		panic("%s: unhandled fs_ep_sz %#x", __func__, f->fs_ep_sz);
	}

	/* update "aFrames" */
	if (suword32(aFrames_uptr, fs_ep->aFrames) != 0)
		return (EFAULT);

	/* update "isoc_time_complete" */
	if (suword16(isoc_time_complete_uptr, fs_ep->isoc_time_complete) != 0)
		return (EFAULT);

	/* update "status" */
	if (suword32(status_uptr, fs_ep->status) != 0)
		return (EFAULT);

	return (0);
}

static int
ugen_fs_copy_out_cancelled(struct usb_fifo *f, uint8_t ep_index)
{
	struct usb_fs_endpoint fs_ep;
	int error;

	error = ugen_fs_copyin(f, ep_index, &fs_ep);
	if (error)
		return (error);

	fs_ep.status = USB_ERR_CANCELLED;
	fs_ep.aFrames = 0;
	fs_ep.isoc_time_complete = 0;

	return (ugen_fs_update(&fs_ep, f, ep_index));
}

static int
ugen_fs_copy_out(struct usb_fifo *f, uint8_t ep_index)
{
	struct usb_device_request *req;
	struct usb_xfer *xfer;
	struct usb_fs_endpoint fs_ep;
	void *uaddr;			/* userland ptr */
	void *kaddr;
	usb_frlength_t offset;
	usb_frlength_t rem;
	usb_frcount_t n;
	uint32_t length;
	uint32_t temp;
	int error;
	uint8_t isread;

	mtx_lock(f->priv_mtx);
	if (ep_index >= f->fs_ep_max) {
		mtx_unlock(f->priv_mtx);
		return (EINVAL);
	}
	xfer = f->fs_xfer[ep_index];
	if (xfer == NULL) {
		mtx_unlock(f->priv_mtx);
		return (EINVAL);
	}
	if (!xfer->flags_int.transferring &&
	    !xfer->flags_int.started) {
		mtx_unlock(f->priv_mtx);
		DPRINTF("Returning fake cancel event\n");
		return (ugen_fs_copy_out_cancelled(f, ep_index));
	} else if (usbd_transfer_pending(xfer)) {
		mtx_unlock(f->priv_mtx);
		return (EBUSY);		/* should not happen */
	}
	mtx_unlock(f->priv_mtx);

	error = ugen_fs_copyin(f, ep_index, &fs_ep);
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

	rem = usbd_xfer_max_len(xfer);
	offset = 0;

	for (; n != xfer->nframes; n++) {
		/* get initial length into "temp" */
		if (fueword32(fs_ep.pLength + n, &temp) != 0) {
			return (EFAULT);
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
			error = ugen_fs_getbuffer(&uaddr, f, fs_ep.ppBuffer, n);
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
				goto complete;
			}
		}
		/*
		 * Update offset according to initial length, which is
		 * needed by isochronous transfers!
		 */
		offset += temp;

		/* update length */
		if (suword32(fs_ep.pLength + n, length) != 0)
			goto complete;
	}

complete:
	if (error == 0)
		error = ugen_fs_update(&fs_ep, f, ep_index);
	return (error);
}

static uint8_t
ugen_fifo_in_use(struct usb_fifo *f, int fflags)
{
	struct usb_fifo *f_rx;
	struct usb_fifo *f_tx;

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
ugen_ioctl(struct usb_fifo *f, u_long cmd, void *addr, int fflags)
{
	union {
		struct usb_fs_complete *pcomp;
		struct usb_fs_start *pstart;
		struct usb_fs_stop *pstop;
		void   *addr;
	}     u;
	struct usb_xfer *xfer;
	int error;
	uint8_t ep_index;

	u.addr = addr;

	DPRINTFN(6, "cmd=0x%08lx\n", cmd);

	switch (cmd) {
	case USB_FS_COMPLETE:
		sx_slock(&f->fs_fastpath_lock);
		mtx_lock(f->priv_mtx);
		error = ugen_fs_get_complete(f, &ep_index);
		mtx_unlock(f->priv_mtx);

		if (error != 0) {
			error = EBUSY;
		} else {
			u.pcomp->ep_index = ep_index;
			error = ugen_fs_copy_out(f, u.pcomp->ep_index);
		}
		sx_sunlock(&f->fs_fastpath_lock);
		break;

	case USB_FS_START:
		sx_slock(&f->fs_fastpath_lock);
		error = ugen_fs_copy_in(f, u.pstart->ep_index);
		if (error == 0) {
			mtx_lock(f->priv_mtx);
			xfer = f->fs_xfer[u.pstart->ep_index];
			usbd_transfer_start(xfer);
			mtx_unlock(f->priv_mtx);
		}
		sx_sunlock(&f->fs_fastpath_lock);
		break;

	case USB_FS_STOP:
		mtx_lock(f->priv_mtx);
		if (u.pstop->ep_index >= f->fs_ep_max) {
			error = EINVAL;
		} else {
			error = 0;
			xfer = f->fs_xfer[u.pstart->ep_index];
			if (usbd_transfer_pending(xfer)) {
				usbd_transfer_stop(xfer);

				/*
				 * Check if the USB transfer was
				 * stopped before it was even started
				 * and fake a cancel event.
				 */
				if (!xfer->flags_int.transferring &&
				    !xfer->flags_int.started) {
					DPRINTF("Issuing fake completion event\n");
					ugen_fs_set_complete(xfer->priv_sc,
					    USB_P2U(xfer->priv_fifo));
				}
			}
		}
		mtx_unlock(f->priv_mtx);
		break;

	default:
		error = ENOIOCTL;
		break;
	}

	DPRINTFN(6, "error=%d\n", error);

	return (error);
}

static int
ugen_set_short_xfer(struct usb_fifo *f, void *addr)
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
ugen_set_timeout(struct usb_fifo *f, void *addr)
{
	f->timeout = *(int *)addr;
	if (f->timeout > 65535) {
		/* limit user input */
		f->timeout = 65535;
	}
	return (0);
}

static int
ugen_get_frame_size(struct usb_fifo *f, void *addr)
{
	if (f->xfer[0]) {
		*(int *)addr = f->xfer[0]->max_frame_size;
	} else {
		return (EINVAL);
	}
	return (0);
}

static int
ugen_set_buffer_size(struct usb_fifo *f, void *addr)
{
	usb_frlength_t t;

	if (*(int *)addr < 0)
		t = 0;		/* use "wMaxPacketSize" */
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
ugen_get_buffer_size(struct usb_fifo *f, void *addr)
{
	*(int *)addr = f->bufsize;
	return (0);
}

static int
ugen_get_iface_desc(struct usb_fifo *f,
    struct usb_interface_descriptor *idesc)
{
	struct usb_interface *iface;

	iface = usbd_get_iface(f->udev, f->iface_index);
	if (iface && iface->idesc) {
		*idesc = *(iface->idesc);
	} else {
		return (EIO);
	}
	return (0);
}

static int
ugen_get_endpoint_desc(struct usb_fifo *f,
    struct usb_endpoint_descriptor *ed)
{
	struct usb_endpoint *ep;

	ep = usb_fifo_softc(f);

	if (ep && ep->edesc) {
		*ed = *ep->edesc;
	} else {
		return (EINVAL);
	}
	return (0);
}

static int
ugen_set_power_mode(struct usb_fifo *f, int mode)
{
	struct usb_device *udev = f->udev;
	int err;
	uint8_t old_mode;

	if ((udev == NULL) ||
	    (udev->parent_hub == NULL)) {
		return (EINVAL);
	}
	err = priv_check(curthread, PRIV_DRIVER);
	if (err)
		return (err);

	/* get old power mode */
	old_mode = udev->power_mode;

	/* if no change, then just return */
	if (old_mode == mode)
		return (0);

	switch (mode) {
	case USB_POWER_MODE_OFF:
		if (udev->flags.usb_mode == USB_MODE_HOST &&
		    udev->re_enumerate_wait == USB_RE_ENUM_DONE) {
			udev->re_enumerate_wait = USB_RE_ENUM_PWR_OFF;
		}
		/* set power mode will wake up the explore thread */
		break;

	case USB_POWER_MODE_ON:
	case USB_POWER_MODE_SAVE:
		break;

	case USB_POWER_MODE_RESUME:
#if USB_HAVE_POWERD
		/* let USB-powerd handle resume */
		USB_BUS_LOCK(udev->bus);
		udev->pwr_save.write_refs++;
		udev->pwr_save.last_xfer_time = ticks;
		USB_BUS_UNLOCK(udev->bus);

		/* set new power mode */
		usbd_set_power_mode(udev, USB_POWER_MODE_SAVE);

		/* wait for resume to complete */
		usb_pause_mtx(NULL, hz / 4);

		/* clear write reference */
		USB_BUS_LOCK(udev->bus);
		udev->pwr_save.write_refs--;
		USB_BUS_UNLOCK(udev->bus);
#endif
		mode = USB_POWER_MODE_SAVE;
		break;

	case USB_POWER_MODE_SUSPEND:
#if USB_HAVE_POWERD
		/* let USB-powerd handle suspend */
		USB_BUS_LOCK(udev->bus);
		udev->pwr_save.last_xfer_time = ticks - (256 * hz);
		USB_BUS_UNLOCK(udev->bus);
#endif
		mode = USB_POWER_MODE_SAVE;
		break;

	default:
		return (EINVAL);
	}

	if (err)
		return (ENXIO);		/* I/O failure */

	/* if we are powered off we need to re-enumerate first */
	if (old_mode == USB_POWER_MODE_OFF) {
		if (udev->flags.usb_mode == USB_MODE_HOST &&
		    udev->re_enumerate_wait == USB_RE_ENUM_DONE) {
			udev->re_enumerate_wait = USB_RE_ENUM_START;
		}
		/* set power mode will wake up the explore thread */
	}

	/* set new power mode */
	usbd_set_power_mode(udev, mode);

	return (0);			/* success */
}

static int
ugen_get_power_mode(struct usb_fifo *f)
{
	struct usb_device *udev = f->udev;

	if (udev == NULL)
		return (USB_POWER_MODE_ON);

	return (udev->power_mode);
}

static int
ugen_get_port_path(struct usb_fifo *f, struct usb_device_port_path *dpp)
{
	struct usb_device *udev = f->udev;
	struct usb_device *next;
	unsigned nlevel = 0;

	if (udev == NULL)
		goto error;

	dpp->udp_bus = device_get_unit(udev->bus->bdev);
	dpp->udp_index = udev->device_index;

	/* count port levels */
	next = udev;
	while (next->parent_hub != NULL) {
		nlevel++;
		next = next->parent_hub;
	}

	/* check if too many levels */
	if (nlevel > USB_DEVICE_PORT_PATH_MAX)
		goto error;

	/* store total level of ports */
	dpp->udp_port_level = nlevel;

	/* store port index array */
	next = udev;
	while (next->parent_hub != NULL) {
		dpp->udp_port_no[--nlevel] = next->port_no;
		next = next->parent_hub;
	}
	return (0);	/* success */

error:
	return (EINVAL);	/* failure */
}

static int
ugen_get_power_usage(struct usb_fifo *f)
{
	struct usb_device *udev = f->udev;

	if (udev == NULL)
		return (0);

	return (udev->power);
}

static int
ugen_do_port_feature(struct usb_fifo *f, uint8_t port_no,
    uint8_t set, uint16_t feature)
{
	struct usb_device *udev = f->udev;
	struct usb_hub *hub;
	int err;

	err = priv_check(curthread, PRIV_DRIVER);
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
		err = usbd_req_set_port_feature(udev,
		    NULL, port_no, feature);
	else
		err = usbd_req_clear_port_feature(udev,
		    NULL, port_no, feature);

	if (err)
		return (ENXIO);		/* failure */

	return (0);			/* success */
}

static int
ugen_iface_ioctl(struct usb_fifo *f, u_long cmd, void *addr, int fflags)
{
	struct usb_fifo *f_rx;
	struct usb_fifo *f_tx;
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
		error = ENOIOCTL;
		break;
	}
	return (error);
}

static int
ugen_ioctl_post(struct usb_fifo *f, u_long cmd, void *addr, int fflags)
{
	union {
		struct usb_interface_descriptor *idesc;
		struct usb_alt_interface *ai;
		struct usb_device_descriptor *ddesc;
		struct usb_config_descriptor *cdesc;
		struct usb_device_stats *stat;
		struct usb_fs_init *pinit;
#ifdef COMPAT_FREEBSD32
		struct usb_fs_init32 *pinit32;
#endif
		struct usb_fs_uninit *puninit;
		struct usb_fs_open *popen;
		struct usb_fs_open_stream *popen_stream;
		struct usb_fs_close *pclose;
		struct usb_fs_clear_stall_sync *pstall;
		struct usb_device_port_path *dpp;
		uint32_t *ptime;
		void   *addr;
		int    *pint;
	}     u;
	struct usb_device_descriptor *dtemp;
	struct usb_config_descriptor *ctemp;
	struct usb_interface *iface;
	int error = 0;
	uint8_t n;

	u.addr = addr;

	DPRINTFN(6, "cmd=0x%08lx\n", cmd);

	switch (cmd) {
	case USB_DISCOVER:
		usb_needs_explore_all();
		break;

	case USB_SETDEBUG:
		if (!(fflags & FWRITE)) {
			error = EPERM;
			break;
		}
		usb_debug = *(int *)addr;
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
		iface = usbd_get_iface(f->udev,
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
		dtemp = usbd_get_device_descriptor(f->udev);
		if (!dtemp) {
			error = EIO;
			break;
		}
		*u.ddesc = *dtemp;
		break;

	case USB_GET_CONFIG_DESC:
		ctemp = usbd_get_config_descriptor(f->udev);
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

	case USB_GET_IFACE_DRIVER:
		error = ugen_get_iface_driver(f, addr);
		break;

#ifdef COMPAT_FREEBSD32
	case USB_GET_FULL_DESC32:
	case USB_GET_STRING_DESC32:
	case USB_GET_IFACE_DRIVER32:
		error = ugen_get32(cmd, f, addr);
		break;
#endif

	case USB_REQUEST:
	case USB_DO_REQUEST:
		if (!(fflags & FWRITE)) {
			error = EPERM;
			break;
		}
		error = ugen_do_request(f, addr);
		break;

#ifdef COMPAT_FREEBSD32
	case USB_REQUEST32:
	case USB_DO_REQUEST32:
		if (!(fflags & FWRITE)) {
			error = EPERM;
			break;
		}
		error = ugen_do_request32(f, addr);
		break;
#endif

	case USB_DEVICEINFO:
	case USB_GET_DEVICEINFO:
		error = ugen_fill_deviceinfo(f, addr);
		break;

	case USB_DEVICESTATS:
		for (n = 0; n != 4; n++) {
			u.stat->uds_requests_fail[n] =
			    f->udev->stats_err.uds_requests[n];
			u.stat->uds_requests_ok[n] =
			    f->udev->stats_ok.uds_requests[n];
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
		n = *u.pint & 0xFF;
		iface = usbd_get_iface(f->udev, n);
		if (iface != NULL && iface->subdev != NULL &&
		    device_is_alive(iface->subdev))
			error = 0;
		else
			error = ENXIO;
		break;

	case USB_IFACE_DRIVER_DETACH:

		error = priv_check(curthread, PRIV_DRIVER);

		if (error)
			break;

		n = *u.pint & 0xFF;

		if (n == USB_IFACE_INDEX_ANY) {
			error = EINVAL;
			break;
		}

		/*
		 * Detach the currently attached driver.
		 */
		usb_detach_device(f->udev, n, 0);

		/*
		 * Set parent to self, this should keep attach away
		 * until the next set configuration event.
		 */
		usbd_set_parent_iface(f->udev, n, n);
		break;

	case USB_SET_POWER_MODE:
		error = ugen_set_power_mode(f, *u.pint);
		break;

	case USB_GET_POWER_MODE:
		*u.pint = ugen_get_power_mode(f);
		break;

	case USB_GET_DEV_PORT_PATH:
		error = ugen_get_port_path(f, u.dpp);
		break;

	case USB_GET_POWER_USAGE:
		*u.pint = ugen_get_power_usage(f);
		break;

	case USB_SET_PORT_ENABLE:
		error = ugen_do_port_feature(f,
		    *u.pint, 1, UHF_PORT_ENABLE);
		break;

	case USB_SET_PORT_DISABLE:
		error = ugen_do_port_feature(f,
		    *u.pint, 0, UHF_PORT_ENABLE);
		break;

	case USB_FS_INIT:
		error = ugen_fs_init(f, u.pinit->pEndpoints,
		    sizeof(struct usb_fs_endpoint), fflags,
		    u.pinit->ep_index_max);
		break;
#ifdef COMPAT_FREEBSD32
	case USB_FS_INIT32:
		error = ugen_fs_init(f, PTRIN(u.pinit32->pEndpoints),
		    sizeof(struct usb_fs_endpoint32), fflags,
		    u.pinit32->ep_index_max);
		break;
#endif
	case USB_FS_UNINIT:
		if (u.puninit->dummy != 0) {
			error = EINVAL;
			break;
		}
		error = ugen_fs_uninit(f);
		break;

	case USB_FS_OPEN:
	case USB_FS_OPEN_STREAM:
		error = usb_fs_open(f, u.popen, fflags,
		    (cmd == USB_FS_OPEN_STREAM) ? u.popen_stream->stream_id : 0);
		break;

	case USB_FS_CLOSE:
		error = usb_fs_close(f, u.pclose);
		break;

	case USB_FS_CLEAR_STALL_SYNC:
		error = usb_fs_clear_stall_sync(f, u.pstall);
		break;

	default:
		mtx_lock(f->priv_mtx);
		error = ugen_iface_ioctl(f, cmd, addr, fflags);
		mtx_unlock(f->priv_mtx);
		break;
	}
	DPRINTFN(6, "error=%d\n", error);
	return (error);
}

static void
ugen_ctrl_fs_callback(struct usb_xfer *xfer, usb_error_t error)
{
	;				/* workaround for a bug in "indent" */

	DPRINTF("st=%u alen=%u aframes=%u\n",
	    USB_GET_STATE(xfer), xfer->actlen, xfer->aframes);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
		usbd_transfer_submit(xfer);
		break;
	default:
		ugen_fs_set_complete(xfer->priv_sc, USB_P2U(xfer->priv_fifo));
		break;
	}
}

#ifdef COMPAT_FREEBSD32
void
usb_gen_descriptor_from32(struct usb_gen_descriptor *ugd,
    const struct usb_gen_descriptor32 *ugd32)
{
	PTRIN_CP(*ugd32, *ugd, ugd_data);
	CP(*ugd32, *ugd, ugd_lang_id);
	CP(*ugd32, *ugd, ugd_maxlen);
	CP(*ugd32, *ugd, ugd_actlen);
	CP(*ugd32, *ugd, ugd_offset);
	CP(*ugd32, *ugd, ugd_config_index);
	CP(*ugd32, *ugd, ugd_string_index);
	CP(*ugd32, *ugd, ugd_iface_index);
	CP(*ugd32, *ugd, ugd_altif_index);
	CP(*ugd32, *ugd, ugd_endpt_index);
	CP(*ugd32, *ugd, ugd_report_type);
	/* Don't copy reserved */
}

void
update_usb_gen_descriptor32(struct usb_gen_descriptor32 *ugd32,
    struct usb_gen_descriptor *ugd)
{
	/* Don't update ugd_data pointer */
	CP(*ugd32, *ugd, ugd_lang_id);
	CP(*ugd32, *ugd, ugd_maxlen);
	CP(*ugd32, *ugd, ugd_actlen);
	CP(*ugd32, *ugd, ugd_offset);
	CP(*ugd32, *ugd, ugd_config_index);
	CP(*ugd32, *ugd, ugd_string_index);
	CP(*ugd32, *ugd, ugd_iface_index);
	CP(*ugd32, *ugd, ugd_altif_index);
	CP(*ugd32, *ugd, ugd_endpt_index);
	CP(*ugd32, *ugd, ugd_report_type);
	/* Don't update reserved */
}

static int
ugen_get32(u_long cmd, struct usb_fifo *f, struct usb_gen_descriptor32 *ugd32)
{
	struct usb_gen_descriptor ugd;
	int error;

	usb_gen_descriptor_from32(&ugd, ugd32);
	switch (cmd) {
	case USB_GET_FULL_DESC32:
		error = ugen_get_cdesc(f, &ugd);
		break;

	case USB_GET_STRING_DESC32:
		error = ugen_get_sdesc(f, &ugd);
		break;

	case USB_GET_IFACE_DRIVER32:
		error = ugen_get_iface_driver(f, &ugd);
		break;
	default:
		/* Can't happen except by programmer error */
		panic("%s: called with invalid cmd %lx", __func__, cmd);
	}
	update_usb_gen_descriptor32(ugd32, &ugd);

	return (error);
}

#endif /* COMPAT_FREEBSD32 */

#endif	/* USB_HAVE_UGEN */
