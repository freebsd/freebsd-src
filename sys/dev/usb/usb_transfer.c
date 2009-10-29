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

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/linker_set.h>
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

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_transfer.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>

struct usb_std_packet_size {
	struct {
		uint16_t min;		/* inclusive */
		uint16_t max;		/* inclusive */
	}	range;

	uint16_t fixed[4];
};

static usb_callback_t usb_request_callback;

static const struct usb_config usb_control_ep_cfg[USB_DEFAULT_XFER_MAX] = {

	/* This transfer is used for generic control endpoint transfers */

	[0] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control endpoint */
		.direction = UE_DIR_ANY,
		.bufsize = USB_EP0_BUFSIZE,	/* bytes */
		.flags = {.proxy_buffer = 1,},
		.callback = &usb_request_callback,
		.usb_mode = USB_MODE_DUAL,	/* both modes */
	},

	/* This transfer is used for generic clear stall only */

	[1] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.bufsize = sizeof(struct usb_device_request),
		.callback = &usb_do_clear_stall_callback,
		.timeout = 1000,	/* 1 second */
		.interval = 50,	/* 50ms */
		.usb_mode = USB_MODE_HOST,
	},
};

/* function prototypes */

static void	usbd_update_max_frame_size(struct usb_xfer *);
static void	usbd_transfer_unsetup_sub(struct usb_xfer_root *, uint8_t);
static void	usbd_control_transfer_init(struct usb_xfer *);
static int	usbd_setup_ctrl_transfer(struct usb_xfer *);
static void	usb_callback_proc(struct usb_proc_msg *);
static void	usbd_callback_ss_done_defer(struct usb_xfer *);
static void	usbd_callback_wrapper(struct usb_xfer_queue *);
static void	usb_dma_delay_done_cb(void *);
static void	usbd_transfer_start_cb(void *);
static uint8_t	usbd_callback_wrapper_sub(struct usb_xfer *);
static void	usbd_get_std_packet_size(struct usb_std_packet_size *ptr, 
		    uint8_t type, enum usb_dev_speed speed);

/*------------------------------------------------------------------------*
 *	usb_request_callback
 *------------------------------------------------------------------------*/
static void
usb_request_callback(struct usb_xfer *xfer, usb_error_t error)
{
	if (xfer->flags_int.usb_mode == USB_MODE_DEVICE)
		usb_handle_request_callback(xfer, error);
	else
		usbd_do_request_callback(xfer, error);
}

/*------------------------------------------------------------------------*
 *	usbd_update_max_frame_size
 *
 * This function updates the maximum frame size, hence high speed USB
 * can transfer multiple consecutive packets.
 *------------------------------------------------------------------------*/
static void
usbd_update_max_frame_size(struct usb_xfer *xfer)
{
	/* compute maximum frame size */

	if (xfer->max_packet_count == 2) {
		xfer->max_frame_size = 2 * xfer->max_packet_size;
	} else if (xfer->max_packet_count == 3) {
		xfer->max_frame_size = 3 * xfer->max_packet_size;
	} else {
		xfer->max_frame_size = xfer->max_packet_size;
	}
}

/*------------------------------------------------------------------------*
 *	usbd_get_dma_delay
 *
 * The following function is called when we need to
 * synchronize with DMA hardware.
 *
 * Returns:
 *    0: no DMA delay required
 * Else: milliseconds of DMA delay
 *------------------------------------------------------------------------*/
usb_timeout_t
usbd_get_dma_delay(struct usb_bus *bus)
{
	uint32_t temp = 0;

	if (bus->methods->get_dma_delay) {
		(bus->methods->get_dma_delay) (bus, &temp);
		/*
		 * Round up and convert to milliseconds. Note that we use
		 * 1024 milliseconds per second. to save a division.
		 */
		temp += 0x3FF;
		temp /= 0x400;
	}
	return (temp);
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_setup_sub_malloc
 *
 * This function will allocate one or more DMA'able memory chunks
 * according to "size", "align" and "count" arguments. "ppc" is
 * pointed to a linear array of USB page caches afterwards.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
#if USB_HAVE_BUSDMA
uint8_t
usbd_transfer_setup_sub_malloc(struct usb_setup_params *parm,
    struct usb_page_cache **ppc, usb_size_t size, usb_size_t align,
    usb_size_t count)
{
	struct usb_page_cache *pc;
	struct usb_page *pg;
	void *buf;
	usb_size_t n_dma_pc;
	usb_size_t n_obj;
	usb_size_t x;
	usb_size_t y;
	usb_size_t r;
	usb_size_t z;

	USB_ASSERT(align > 1, ("Invalid alignment, 0x%08x!\n",
	    align));
	USB_ASSERT(size > 0, ("Invalid size = 0!\n"));

	if (count == 0) {
		return (0);		/* nothing to allocate */
	}
	/*
	 * Make sure that the size is aligned properly.
	 */
	size = -((-size) & (-align));

	/*
	 * Try multi-allocation chunks to reduce the number of DMA
	 * allocations, hence DMA allocations are slow.
	 */
	if (size >= PAGE_SIZE) {
		n_dma_pc = count;
		n_obj = 1;
	} else {
		/* compute number of objects per page */
		n_obj = (PAGE_SIZE / size);
		/*
		 * Compute number of DMA chunks, rounded up
		 * to nearest one:
		 */
		n_dma_pc = ((count + n_obj - 1) / n_obj);
	}

	if (parm->buf == NULL) {
		/* for the future */
		parm->dma_page_ptr += n_dma_pc;
		parm->dma_page_cache_ptr += n_dma_pc;
		parm->dma_page_ptr += count;
		parm->xfer_page_cache_ptr += count;
		return (0);
	}
	for (x = 0; x != n_dma_pc; x++) {
		/* need to initialize the page cache */
		parm->dma_page_cache_ptr[x].tag_parent =
		    &parm->curr_xfer->xroot->dma_parent_tag;
	}
	for (x = 0; x != count; x++) {
		/* need to initialize the page cache */
		parm->xfer_page_cache_ptr[x].tag_parent =
		    &parm->curr_xfer->xroot->dma_parent_tag;
	}

	if (ppc) {
		*ppc = parm->xfer_page_cache_ptr;
	}
	r = count;			/* set remainder count */
	z = n_obj * size;		/* set allocation size */
	pc = parm->xfer_page_cache_ptr;
	pg = parm->dma_page_ptr;

	for (x = 0; x != n_dma_pc; x++) {

		if (r < n_obj) {
			/* compute last remainder */
			z = r * size;
			n_obj = r;
		}
		if (usb_pc_alloc_mem(parm->dma_page_cache_ptr,
		    pg, z, align)) {
			return (1);	/* failure */
		}
		/* Set beginning of current buffer */
		buf = parm->dma_page_cache_ptr->buffer;
		/* Make room for one DMA page cache and one page */
		parm->dma_page_cache_ptr++;
		pg++;

		for (y = 0; (y != n_obj); y++, r--, pc++, pg++) {

			/* Load sub-chunk into DMA */
			if (usb_pc_dmamap_create(pc, size)) {
				return (1);	/* failure */
			}
			pc->buffer = USB_ADD_BYTES(buf, y * size);
			pc->page_start = pg;

			mtx_lock(pc->tag_parent->mtx);
			if (usb_pc_load_mem(pc, size, 1 /* synchronous */ )) {
				mtx_unlock(pc->tag_parent->mtx);
				return (1);	/* failure */
			}
			mtx_unlock(pc->tag_parent->mtx);
		}
	}

	parm->xfer_page_cache_ptr = pc;
	parm->dma_page_ptr = pg;
	return (0);
}
#endif

/*------------------------------------------------------------------------*
 *	usbd_transfer_setup_sub - transfer setup subroutine
 *
 * This function must be called from the "xfer_setup" callback of the
 * USB Host or Device controller driver when setting up an USB
 * transfer. This function will setup correct packet sizes, buffer
 * sizes, flags and more, that are stored in the "usb_xfer"
 * structure.
 *------------------------------------------------------------------------*/
void
usbd_transfer_setup_sub(struct usb_setup_params *parm)
{
	enum {
		REQ_SIZE = 8,
		MIN_PKT = 8,
	};
	struct usb_xfer *xfer = parm->curr_xfer;
	const struct usb_config *setup = parm->curr_setup;
	struct usb_endpoint_descriptor *edesc;
	struct usb_std_packet_size std_size;
	usb_frcount_t n_frlengths;
	usb_frcount_t n_frbuffers;
	usb_frcount_t x;
	uint8_t type;
	uint8_t zmps;

	/*
	 * Sanity check. The following parameters must be initialized before
	 * calling this function.
	 */
	if ((parm->hc_max_packet_size == 0) ||
	    (parm->hc_max_packet_count == 0) ||
	    (parm->hc_max_frame_size == 0)) {
		parm->err = USB_ERR_INVAL;
		goto done;
	}
	edesc = xfer->endpoint->edesc;

	type = (edesc->bmAttributes & UE_XFERTYPE);

	xfer->flags = setup->flags;
	xfer->nframes = setup->frames;
	xfer->timeout = setup->timeout;
	xfer->callback = setup->callback;
	xfer->interval = setup->interval;
	xfer->endpointno = edesc->bEndpointAddress;
	xfer->max_packet_size = UGETW(edesc->wMaxPacketSize);
	xfer->max_packet_count = 1;
	/* make a shadow copy: */
	xfer->flags_int.usb_mode = parm->udev->flags.usb_mode;

	parm->bufsize = setup->bufsize;

	if (parm->speed == USB_SPEED_HIGH) {
		xfer->max_packet_count += (xfer->max_packet_size >> 11) & 3;
		xfer->max_packet_size &= 0x7FF;
	}
	/* range check "max_packet_count" */

	if (xfer->max_packet_count > parm->hc_max_packet_count) {
		xfer->max_packet_count = parm->hc_max_packet_count;
	}
	/* filter "wMaxPacketSize" according to HC capabilities */

	if ((xfer->max_packet_size > parm->hc_max_packet_size) ||
	    (xfer->max_packet_size == 0)) {
		xfer->max_packet_size = parm->hc_max_packet_size;
	}
	/* filter "wMaxPacketSize" according to standard sizes */

	usbd_get_std_packet_size(&std_size, type, parm->speed);

	if (std_size.range.min || std_size.range.max) {

		if (xfer->max_packet_size < std_size.range.min) {
			xfer->max_packet_size = std_size.range.min;
		}
		if (xfer->max_packet_size > std_size.range.max) {
			xfer->max_packet_size = std_size.range.max;
		}
	} else {

		if (xfer->max_packet_size >= std_size.fixed[3]) {
			xfer->max_packet_size = std_size.fixed[3];
		} else if (xfer->max_packet_size >= std_size.fixed[2]) {
			xfer->max_packet_size = std_size.fixed[2];
		} else if (xfer->max_packet_size >= std_size.fixed[1]) {
			xfer->max_packet_size = std_size.fixed[1];
		} else {
			/* only one possibility left */
			xfer->max_packet_size = std_size.fixed[0];
		}
	}

	/* compute "max_frame_size" */

	usbd_update_max_frame_size(xfer);

	/* check interrupt interval and transfer pre-delay */

	if (type == UE_ISOCHRONOUS) {

		uint16_t frame_limit;

		xfer->interval = 0;	/* not used, must be zero */
		xfer->flags_int.isochronous_xfr = 1;	/* set flag */

		if (xfer->timeout == 0) {
			/*
			 * set a default timeout in
			 * case something goes wrong!
			 */
			xfer->timeout = 1000 / 4;
		}
		switch (parm->speed) {
		case USB_SPEED_LOW:
		case USB_SPEED_FULL:
			frame_limit = USB_MAX_FS_ISOC_FRAMES_PER_XFER;
			break;
		default:
			frame_limit = USB_MAX_HS_ISOC_FRAMES_PER_XFER;
			break;
		}

		if (xfer->nframes > frame_limit) {
			/*
			 * this is not going to work
			 * cross hardware
			 */
			parm->err = USB_ERR_INVAL;
			goto done;
		}
		if (xfer->nframes == 0) {
			/*
			 * this is not a valid value
			 */
			parm->err = USB_ERR_ZERO_NFRAMES;
			goto done;
		}
	} else {

		/*
		 * if a value is specified use that else check the endpoint
		 * descriptor
		 */
		if (xfer->interval == 0) {

			if (type == UE_INTERRUPT) {

				xfer->interval = edesc->bInterval;

				switch (parm->speed) {
				case USB_SPEED_SUPER:
				case USB_SPEED_VARIABLE:
					/* 125us -> 1ms */
					if (xfer->interval < 4)
						xfer->interval = 1;
					else if (xfer->interval > 16)
						xfer->interval = (1<<(16-4));
					else
						xfer->interval = 
						    (1 << (xfer->interval-4));
					break;
				case USB_SPEED_HIGH:
					/* 125us -> 1ms */
					xfer->interval /= 8;
					break;
				default:
					break;
				}
				if (xfer->interval == 0) {
					/*
					 * One millisecond is the smallest
					 * interval we support:
					 */
					xfer->interval = 1;
				}
			}
		}
	}

	/*
	 * NOTE: we do not allow "max_packet_size" or "max_frame_size"
	 * to be equal to zero when setting up USB transfers, hence
	 * this leads to alot of extra code in the USB kernel.
	 */

	if ((xfer->max_frame_size == 0) ||
	    (xfer->max_packet_size == 0)) {

		zmps = 1;

		if ((parm->bufsize <= MIN_PKT) &&
		    (type != UE_CONTROL) &&
		    (type != UE_BULK)) {

			/* workaround */
			xfer->max_packet_size = MIN_PKT;
			xfer->max_packet_count = 1;
			parm->bufsize = 0;	/* automatic setup length */
			usbd_update_max_frame_size(xfer);

		} else {
			parm->err = USB_ERR_ZERO_MAXP;
			goto done;
		}

	} else {
		zmps = 0;
	}

	/*
	 * check if we should setup a default
	 * length:
	 */

	if (parm->bufsize == 0) {

		parm->bufsize = xfer->max_frame_size;

		if (type == UE_ISOCHRONOUS) {
			parm->bufsize *= xfer->nframes;
		}
	}
	/*
	 * check if we are about to setup a proxy
	 * type of buffer:
	 */

	if (xfer->flags.proxy_buffer) {

		/* round bufsize up */

		parm->bufsize += (xfer->max_frame_size - 1);

		if (parm->bufsize < xfer->max_frame_size) {
			/* length wrapped around */
			parm->err = USB_ERR_INVAL;
			goto done;
		}
		/* subtract remainder */

		parm->bufsize -= (parm->bufsize % xfer->max_frame_size);

		/* add length of USB device request structure, if any */

		if (type == UE_CONTROL) {
			parm->bufsize += REQ_SIZE;	/* SETUP message */
		}
	}
	xfer->max_data_length = parm->bufsize;

	/* Setup "n_frlengths" and "n_frbuffers" */

	if (type == UE_ISOCHRONOUS) {
		n_frlengths = xfer->nframes;
		n_frbuffers = 1;
	} else {

		if (type == UE_CONTROL) {
			xfer->flags_int.control_xfr = 1;
			if (xfer->nframes == 0) {
				if (parm->bufsize <= REQ_SIZE) {
					/*
					 * there will never be any data
					 * stage
					 */
					xfer->nframes = 1;
				} else {
					xfer->nframes = 2;
				}
			}
		} else {
			if (xfer->nframes == 0) {
				xfer->nframes = 1;
			}
		}

		n_frlengths = xfer->nframes;
		n_frbuffers = xfer->nframes;
	}

	/*
	 * check if we have room for the
	 * USB device request structure:
	 */

	if (type == UE_CONTROL) {

		if (xfer->max_data_length < REQ_SIZE) {
			/* length wrapped around or too small bufsize */
			parm->err = USB_ERR_INVAL;
			goto done;
		}
		xfer->max_data_length -= REQ_SIZE;
	}
	/* setup "frlengths" */
	xfer->frlengths = parm->xfer_length_ptr;
	parm->xfer_length_ptr += n_frlengths;

	/* setup "frbuffers" */
	xfer->frbuffers = parm->xfer_page_cache_ptr;
	parm->xfer_page_cache_ptr += n_frbuffers;

	/* initialize max frame count */
	xfer->max_frame_count = xfer->nframes;

	/*
	 * check if we need to setup
	 * a local buffer:
	 */

	if (!xfer->flags.ext_buffer) {

		/* align data */
		parm->size[0] += ((-parm->size[0]) & (USB_HOST_ALIGN - 1));

		if (parm->buf) {

			xfer->local_buffer =
			    USB_ADD_BYTES(parm->buf, parm->size[0]);

			usbd_xfer_set_frame_offset(xfer, 0, 0);

			if ((type == UE_CONTROL) && (n_frbuffers > 1)) {
				usbd_xfer_set_frame_offset(xfer, REQ_SIZE, 1);
			}
		}
		parm->size[0] += parm->bufsize;

		/* align data again */
		parm->size[0] += ((-parm->size[0]) & (USB_HOST_ALIGN - 1));
	}
	/*
	 * Compute maximum buffer size
	 */

	if (parm->bufsize_max < parm->bufsize) {
		parm->bufsize_max = parm->bufsize;
	}
#if USB_HAVE_BUSDMA
	if (xfer->flags_int.bdma_enable) {
		/*
		 * Setup "dma_page_ptr".
		 *
		 * Proof for formula below:
		 *
		 * Assume there are three USB frames having length "a", "b" and
		 * "c". These USB frames will at maximum need "z"
		 * "usb_page" structures. "z" is given by:
		 *
		 * z = ((a / USB_PAGE_SIZE) + 2) + ((b / USB_PAGE_SIZE) + 2) +
		 * ((c / USB_PAGE_SIZE) + 2);
		 *
		 * Constraining "a", "b" and "c" like this:
		 *
		 * (a + b + c) <= parm->bufsize
		 *
		 * We know that:
		 *
		 * z <= ((parm->bufsize / USB_PAGE_SIZE) + (3*2));
		 *
		 * Here is the general formula:
		 */
		xfer->dma_page_ptr = parm->dma_page_ptr;
		parm->dma_page_ptr += (2 * n_frbuffers);
		parm->dma_page_ptr += (parm->bufsize / USB_PAGE_SIZE);
	}
#endif
	if (zmps) {
		/* correct maximum data length */
		xfer->max_data_length = 0;
	}
	/* subtract USB frame remainder from "hc_max_frame_size" */

	xfer->max_hc_frame_size =
	    (parm->hc_max_frame_size -
	    (parm->hc_max_frame_size % xfer->max_frame_size));

	if (xfer->max_hc_frame_size == 0) {
		parm->err = USB_ERR_INVAL;
		goto done;
	}

	/* initialize frame buffers */

	if (parm->buf) {
		for (x = 0; x != n_frbuffers; x++) {
			xfer->frbuffers[x].tag_parent =
			    &xfer->xroot->dma_parent_tag;
#if USB_HAVE_BUSDMA
			if (xfer->flags_int.bdma_enable &&
			    (parm->bufsize_max > 0)) {

				if (usb_pc_dmamap_create(
				    xfer->frbuffers + x,
				    parm->bufsize_max)) {
					parm->err = USB_ERR_NOMEM;
					goto done;
				}
			}
#endif
		}
	}
done:
	if (parm->err) {
		/*
		 * Set some dummy values so that we avoid division by zero:
		 */
		xfer->max_hc_frame_size = 1;
		xfer->max_frame_size = 1;
		xfer->max_packet_size = 1;
		xfer->max_data_length = 0;
		xfer->nframes = 0;
		xfer->max_frame_count = 0;
	}
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_setup - setup an array of USB transfers
 *
 * NOTE: You must always call "usbd_transfer_unsetup" after calling
 * "usbd_transfer_setup" if success was returned.
 *
 * The idea is that the USB device driver should pre-allocate all its
 * transfers by one call to this function.
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb_error_t
usbd_transfer_setup(struct usb_device *udev,
    const uint8_t *ifaces, struct usb_xfer **ppxfer,
    const struct usb_config *setup_start, uint16_t n_setup,
    void *priv_sc, struct mtx *xfer_mtx)
{
	struct usb_xfer dummy;
	struct usb_setup_params parm;
	const struct usb_config *setup_end = setup_start + n_setup;
	const struct usb_config *setup;
	struct usb_endpoint *ep;
	struct usb_xfer_root *info;
	struct usb_xfer *xfer;
	void *buf = NULL;
	uint16_t n;
	uint16_t refcount;

	parm.err = 0;
	refcount = 0;
	info = NULL;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "usbd_transfer_setup can sleep!");

	/* do some checking first */

	if (n_setup == 0) {
		DPRINTFN(6, "setup array has zero length!\n");
		return (USB_ERR_INVAL);
	}
	if (ifaces == 0) {
		DPRINTFN(6, "ifaces array is NULL!\n");
		return (USB_ERR_INVAL);
	}
	if (xfer_mtx == NULL) {
		DPRINTFN(6, "using global lock\n");
		xfer_mtx = &Giant;
	}
	/* sanity checks */
	for (setup = setup_start, n = 0;
	    setup != setup_end; setup++, n++) {
		if (setup->bufsize == (usb_frlength_t)-1) {
			parm.err = USB_ERR_BAD_BUFSIZE;
			DPRINTF("invalid bufsize\n");
		}
		if (setup->callback == NULL) {
			parm.err = USB_ERR_NO_CALLBACK;
			DPRINTF("no callback\n");
		}
		ppxfer[n] = NULL;
	}

	if (parm.err) {
		goto done;
	}
	bzero(&parm, sizeof(parm));

	parm.udev = udev;
	parm.speed = usbd_get_speed(udev);
	parm.hc_max_packet_count = 1;

	if (parm.speed >= USB_SPEED_MAX) {
		parm.err = USB_ERR_INVAL;
		goto done;
	}
	/* setup all transfers */

	while (1) {

		if (buf) {
			/*
			 * Initialize the "usb_xfer_root" structure,
			 * which is common for all our USB transfers.
			 */
			info = USB_ADD_BYTES(buf, 0);

			info->memory_base = buf;
			info->memory_size = parm.size[0];

#if USB_HAVE_BUSDMA
			info->dma_page_cache_start = USB_ADD_BYTES(buf, parm.size[4]);
			info->dma_page_cache_end = USB_ADD_BYTES(buf, parm.size[5]);
#endif
			info->xfer_page_cache_start = USB_ADD_BYTES(buf, parm.size[5]);
			info->xfer_page_cache_end = USB_ADD_BYTES(buf, parm.size[2]);

			cv_init(&info->cv_drain, "WDRAIN");

			info->xfer_mtx = xfer_mtx;
#if USB_HAVE_BUSDMA
			usb_dma_tag_setup(&info->dma_parent_tag,
			    parm.dma_tag_p, udev->bus->dma_parent_tag[0].tag,
			    xfer_mtx, &usb_bdma_done_event, 32, parm.dma_tag_max);
#endif

			info->bus = udev->bus;
			info->udev = udev;

			TAILQ_INIT(&info->done_q.head);
			info->done_q.command = &usbd_callback_wrapper;
#if USB_HAVE_BUSDMA
			TAILQ_INIT(&info->dma_q.head);
			info->dma_q.command = &usb_bdma_work_loop;
#endif
			info->done_m[0].hdr.pm_callback = &usb_callback_proc;
			info->done_m[0].xroot = info;
			info->done_m[1].hdr.pm_callback = &usb_callback_proc;
			info->done_m[1].xroot = info;

			/* 
			 * In device side mode control endpoint
			 * requests need to run from a separate
			 * context, else there is a chance of
			 * deadlock!
			 */
			if (setup_start == usb_control_ep_cfg)
				info->done_p = 
				    &udev->bus->control_xfer_proc;
			else if (xfer_mtx == &Giant)
				info->done_p = 
				    &udev->bus->giant_callback_proc;
			else
				info->done_p = 
				    &udev->bus->non_giant_callback_proc;
		}
		/* reset sizes */

		parm.size[0] = 0;
		parm.buf = buf;
		parm.size[0] += sizeof(info[0]);

		for (setup = setup_start, n = 0;
		    setup != setup_end; setup++, n++) {

			/* skip USB transfers without callbacks: */
			if (setup->callback == NULL) {
				continue;
			}
			/* see if there is a matching endpoint */
			ep = usbd_get_endpoint(udev,
			    ifaces[setup->if_index], setup);

			if ((ep == NULL) || (ep->methods == NULL)) {
				if (setup->flags.no_pipe_ok)
					continue;
				if ((setup->usb_mode != USB_MODE_DUAL) &&
				    (setup->usb_mode != udev->flags.usb_mode))
					continue;
				parm.err = USB_ERR_NO_PIPE;
				goto done;
			}

			/* align data properly */
			parm.size[0] += ((-parm.size[0]) & (USB_HOST_ALIGN - 1));

			/* store current setup pointer */
			parm.curr_setup = setup;

			if (buf) {
				/*
				 * Common initialization of the
				 * "usb_xfer" structure.
				 */
				xfer = USB_ADD_BYTES(buf, parm.size[0]);
				xfer->address = udev->address;
				xfer->priv_sc = priv_sc;
				xfer->xroot = info;

				usb_callout_init_mtx(&xfer->timeout_handle,
				    &udev->bus->bus_mtx, 0);
			} else {
				/*
				 * Setup a dummy xfer, hence we are
				 * writing to the "usb_xfer"
				 * structure pointed to by "xfer"
				 * before we have allocated any
				 * memory:
				 */
				xfer = &dummy;
				bzero(&dummy, sizeof(dummy));
				refcount++;
			}

			/* set transfer endpoint pointer */
			xfer->endpoint = ep;

			parm.size[0] += sizeof(xfer[0]);
			parm.methods = xfer->endpoint->methods;
			parm.curr_xfer = xfer;

			/*
			 * Call the Host or Device controller transfer
			 * setup routine:
			 */
			(udev->bus->methods->xfer_setup) (&parm);

			/* check for error */
			if (parm.err)
				goto done;

			if (buf) {
				/*
				 * Increment the endpoint refcount. This
				 * basically prevents setting a new
				 * configuration and alternate setting
				 * when USB transfers are in use on
				 * the given interface. Search the USB
				 * code for "endpoint->refcount" if you
				 * want more information.
				 */
				xfer->endpoint->refcount++;

				/*
				 * Whenever we set ppxfer[] then we
				 * also need to increment the
				 * "setup_refcount":
				 */
				info->setup_refcount++;

				/*
				 * Transfer is successfully setup and
				 * can be used:
				 */
				ppxfer[n] = xfer;
			}
		}

		if (buf || parm.err) {
			goto done;
		}
		if (refcount == 0) {
			/* no transfers - nothing to do ! */
			goto done;
		}
		/* align data properly */
		parm.size[0] += ((-parm.size[0]) & (USB_HOST_ALIGN - 1));

		/* store offset temporarily */
		parm.size[1] = parm.size[0];

		/*
		 * The number of DMA tags required depends on
		 * the number of endpoints. The current estimate
		 * for maximum number of DMA tags per endpoint
		 * is two.
		 */
		parm.dma_tag_max += 2 * MIN(n_setup, USB_EP_MAX);

		/*
		 * DMA tags for QH, TD, Data and more.
		 */
		parm.dma_tag_max += 8;

		parm.dma_tag_p += parm.dma_tag_max;

		parm.size[0] += ((uint8_t *)parm.dma_tag_p) -
		    ((uint8_t *)0);

		/* align data properly */
		parm.size[0] += ((-parm.size[0]) & (USB_HOST_ALIGN - 1));

		/* store offset temporarily */
		parm.size[3] = parm.size[0];

		parm.size[0] += ((uint8_t *)parm.dma_page_ptr) -
		    ((uint8_t *)0);

		/* align data properly */
		parm.size[0] += ((-parm.size[0]) & (USB_HOST_ALIGN - 1));

		/* store offset temporarily */
		parm.size[4] = parm.size[0];

		parm.size[0] += ((uint8_t *)parm.dma_page_cache_ptr) -
		    ((uint8_t *)0);

		/* store end offset temporarily */
		parm.size[5] = parm.size[0];

		parm.size[0] += ((uint8_t *)parm.xfer_page_cache_ptr) -
		    ((uint8_t *)0);

		/* store end offset temporarily */

		parm.size[2] = parm.size[0];

		/* align data properly */
		parm.size[0] += ((-parm.size[0]) & (USB_HOST_ALIGN - 1));

		parm.size[6] = parm.size[0];

		parm.size[0] += ((uint8_t *)parm.xfer_length_ptr) -
		    ((uint8_t *)0);

		/* align data properly */
		parm.size[0] += ((-parm.size[0]) & (USB_HOST_ALIGN - 1));

		/* allocate zeroed memory */
		buf = malloc(parm.size[0], M_USB, M_WAITOK | M_ZERO);

		if (buf == NULL) {
			parm.err = USB_ERR_NOMEM;
			DPRINTFN(0, "cannot allocate memory block for "
			    "configuration (%d bytes)\n",
			    parm.size[0]);
			goto done;
		}
		parm.dma_tag_p = USB_ADD_BYTES(buf, parm.size[1]);
		parm.dma_page_ptr = USB_ADD_BYTES(buf, parm.size[3]);
		parm.dma_page_cache_ptr = USB_ADD_BYTES(buf, parm.size[4]);
		parm.xfer_page_cache_ptr = USB_ADD_BYTES(buf, parm.size[5]);
		parm.xfer_length_ptr = USB_ADD_BYTES(buf, parm.size[6]);
	}

done:
	if (buf) {
		if (info->setup_refcount == 0) {
			/*
			 * "usbd_transfer_unsetup_sub" will unlock
			 * the bus mutex before returning !
			 */
			USB_BUS_LOCK(info->bus);

			/* something went wrong */
			usbd_transfer_unsetup_sub(info, 0);
		}
	}
	if (parm.err) {
		usbd_transfer_unsetup(ppxfer, n_setup);
	}
	return (parm.err);
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_unsetup_sub - factored out code
 *------------------------------------------------------------------------*/
static void
usbd_transfer_unsetup_sub(struct usb_xfer_root *info, uint8_t needs_delay)
{
	struct usb_page_cache *pc;

	USB_BUS_LOCK_ASSERT(info->bus, MA_OWNED);

	/* wait for any outstanding DMA operations */

	if (needs_delay) {
		usb_timeout_t temp;
		temp = usbd_get_dma_delay(info->bus);
		usb_pause_mtx(&info->bus->bus_mtx,
		    USB_MS_TO_TICKS(temp));
	}

	/* make sure that our done messages are not queued anywhere */
	usb_proc_mwait(info->done_p, &info->done_m[0], &info->done_m[1]);

	USB_BUS_UNLOCK(info->bus);

#if USB_HAVE_BUSDMA
	/* free DMA'able memory, if any */
	pc = info->dma_page_cache_start;
	while (pc != info->dma_page_cache_end) {
		usb_pc_free_mem(pc);
		pc++;
	}

	/* free DMA maps in all "xfer->frbuffers" */
	pc = info->xfer_page_cache_start;
	while (pc != info->xfer_page_cache_end) {
		usb_pc_dmamap_destroy(pc);
		pc++;
	}

	/* free all DMA tags */
	usb_dma_tag_unsetup(&info->dma_parent_tag);
#endif

	cv_destroy(&info->cv_drain);

	/*
	 * free the "memory_base" last, hence the "info" structure is
	 * contained within the "memory_base"!
	 */
	free(info->memory_base, M_USB);
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_unsetup - unsetup/free an array of USB transfers
 *
 * NOTE: All USB transfers in progress will get called back passing
 * the error code "USB_ERR_CANCELLED" before this function
 * returns.
 *------------------------------------------------------------------------*/
void
usbd_transfer_unsetup(struct usb_xfer **pxfer, uint16_t n_setup)
{
	struct usb_xfer *xfer;
	struct usb_xfer_root *info;
	uint8_t needs_delay = 0;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "usbd_transfer_unsetup can sleep!");

	while (n_setup--) {
		xfer = pxfer[n_setup];

		if (xfer == NULL)
			continue;

		info = xfer->xroot;

		USB_XFER_LOCK(xfer);
		USB_BUS_LOCK(info->bus);

		/*
		 * HINT: when you start/stop a transfer, it might be a
		 * good idea to directly use the "pxfer[]" structure:
		 *
		 * usbd_transfer_start(sc->pxfer[0]);
		 * usbd_transfer_stop(sc->pxfer[0]);
		 *
		 * That way, if your code has many parts that will not
		 * stop running under the same lock, in other words
		 * "xfer_mtx", the usbd_transfer_start and
		 * usbd_transfer_stop functions will simply return
		 * when they detect a NULL pointer argument.
		 *
		 * To avoid any races we clear the "pxfer[]" pointer
		 * while holding the private mutex of the driver:
		 */
		pxfer[n_setup] = NULL;

		USB_BUS_UNLOCK(info->bus);
		USB_XFER_UNLOCK(xfer);

		usbd_transfer_drain(xfer);

#if USB_HAVE_BUSDMA
		if (xfer->flags_int.bdma_enable)
			needs_delay = 1;
#endif
		/*
		 * NOTE: default endpoint does not have an
		 * interface, even if endpoint->iface_index == 0
		 */
		xfer->endpoint->refcount--;

		usb_callout_drain(&xfer->timeout_handle);

		USB_BUS_LOCK(info->bus);

		USB_ASSERT(info->setup_refcount != 0, ("Invalid setup "
		    "reference count!\n"));

		info->setup_refcount--;

		if (info->setup_refcount == 0) {
			usbd_transfer_unsetup_sub(info,
			    needs_delay);
		} else {
			USB_BUS_UNLOCK(info->bus);
		}
	}
}

/*------------------------------------------------------------------------*
 *	usbd_control_transfer_init - factored out code
 *
 * In USB Device Mode we have to wait for the SETUP packet which
 * containst the "struct usb_device_request" structure, before we can
 * transfer any data. In USB Host Mode we already have the SETUP
 * packet at the moment the USB transfer is started. This leads us to
 * having to setup the USB transfer at two different places in
 * time. This function just contains factored out control transfer
 * initialisation code, so that we don't duplicate the code.
 *------------------------------------------------------------------------*/
static void
usbd_control_transfer_init(struct usb_xfer *xfer)
{
	struct usb_device_request req;

	/* copy out the USB request header */

	usbd_copy_out(xfer->frbuffers, 0, &req, sizeof(req));

	/* setup remainder */

	xfer->flags_int.control_rem = UGETW(req.wLength);

	/* copy direction to endpoint variable */

	xfer->endpointno &= ~(UE_DIR_IN | UE_DIR_OUT);
	xfer->endpointno |=
	    (req.bmRequestType & UT_READ) ? UE_DIR_IN : UE_DIR_OUT;
}

/*------------------------------------------------------------------------*
 *	usbd_setup_ctrl_transfer
 *
 * This function handles initialisation of control transfers. Control
 * transfers are special in that regard that they can both transmit
 * and receive data.
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static int
usbd_setup_ctrl_transfer(struct usb_xfer *xfer)
{
	usb_frlength_t len;

	/* Check for control endpoint stall */
	if (xfer->flags.stall_pipe && xfer->flags_int.control_act) {
		/* the control transfer is no longer active */
		xfer->flags_int.control_stall = 1;
		xfer->flags_int.control_act = 0;
	} else {
		/* don't stall control transfer by default */
		xfer->flags_int.control_stall = 0;
	}

	/* Check for invalid number of frames */
	if (xfer->nframes > 2) {
		/*
		 * If you need to split a control transfer, you
		 * have to do one part at a time. Only with
		 * non-control transfers you can do multiple
		 * parts a time.
		 */
		DPRINTFN(0, "Too many frames: %u\n",
		    (unsigned int)xfer->nframes);
		goto error;
	}

	/*
         * Check if there is a control
         * transfer in progress:
         */
	if (xfer->flags_int.control_act) {

		if (xfer->flags_int.control_hdr) {

			/* clear send header flag */

			xfer->flags_int.control_hdr = 0;

			/* setup control transfer */
			if (xfer->flags_int.usb_mode == USB_MODE_DEVICE) {
				usbd_control_transfer_init(xfer);
			}
		}
		/* get data length */

		len = xfer->sumlen;

	} else {

		/* the size of the SETUP structure is hardcoded ! */

		if (xfer->frlengths[0] != sizeof(struct usb_device_request)) {
			DPRINTFN(0, "Wrong framelength %u != %zu\n",
			    xfer->frlengths[0], sizeof(struct
			    usb_device_request));
			goto error;
		}
		/* check USB mode */
		if (xfer->flags_int.usb_mode == USB_MODE_DEVICE) {

			/* check number of frames */
			if (xfer->nframes != 1) {
				/*
			         * We need to receive the setup
			         * message first so that we know the
			         * data direction!
			         */
				DPRINTF("Misconfigured transfer\n");
				goto error;
			}
			/*
			 * Set a dummy "control_rem" value.  This
			 * variable will be overwritten later by a
			 * call to "usbd_control_transfer_init()" !
			 */
			xfer->flags_int.control_rem = 0xFFFF;
		} else {

			/* setup "endpoint" and "control_rem" */

			usbd_control_transfer_init(xfer);
		}

		/* set transfer-header flag */

		xfer->flags_int.control_hdr = 1;

		/* get data length */

		len = (xfer->sumlen - sizeof(struct usb_device_request));
	}

	/* check if there is a length mismatch */

	if (len > xfer->flags_int.control_rem) {
		DPRINTFN(0, "Length greater than remaining length!\n");
		goto error;
	}
	/* check if we are doing a short transfer */

	if (xfer->flags.force_short_xfer) {
		xfer->flags_int.control_rem = 0;
	} else {
		if ((len != xfer->max_data_length) &&
		    (len != xfer->flags_int.control_rem) &&
		    (xfer->nframes != 1)) {
			DPRINTFN(0, "Short control transfer without "
			    "force_short_xfer set!\n");
			goto error;
		}
		xfer->flags_int.control_rem -= len;
	}

	/* the status part is executed when "control_act" is 0 */

	if ((xfer->flags_int.control_rem > 0) ||
	    (xfer->flags.manual_status)) {
		/* don't execute the STATUS stage yet */
		xfer->flags_int.control_act = 1;

		/* sanity check */
		if ((!xfer->flags_int.control_hdr) &&
		    (xfer->nframes == 1)) {
			/*
		         * This is not a valid operation!
		         */
			DPRINTFN(0, "Invalid parameter "
			    "combination\n");
			goto error;
		}
	} else {
		/* time to execute the STATUS stage */
		xfer->flags_int.control_act = 0;
	}
	return (0);			/* success */

error:
	return (1);			/* failure */
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_submit - start USB hardware for the given transfer
 *
 * This function should only be called from the USB callback.
 *------------------------------------------------------------------------*/
void
usbd_transfer_submit(struct usb_xfer *xfer)
{
	struct usb_xfer_root *info;
	struct usb_bus *bus;
	usb_frcount_t x;

	info = xfer->xroot;
	bus = info->bus;

	DPRINTF("xfer=%p, endpoint=%p, nframes=%d, dir=%s\n",
	    xfer, xfer->endpoint, xfer->nframes, USB_GET_DATA_ISREAD(xfer) ?
	    "read" : "write");

#if USB_DEBUG
	if (USB_DEBUG_VAR > 0) {
		USB_BUS_LOCK(bus);

		usb_dump_endpoint(xfer->endpoint);

		USB_BUS_UNLOCK(bus);
	}
#endif

	USB_XFER_LOCK_ASSERT(xfer, MA_OWNED);
	USB_BUS_LOCK_ASSERT(bus, MA_NOTOWNED);

	/* Only open the USB transfer once! */
	if (!xfer->flags_int.open) {
		xfer->flags_int.open = 1;

		DPRINTF("open\n");

		USB_BUS_LOCK(bus);
		(xfer->endpoint->methods->open) (xfer);
		USB_BUS_UNLOCK(bus);
	}
	/* set "transferring" flag */
	xfer->flags_int.transferring = 1;

#if USB_HAVE_POWERD
	/* increment power reference */
	usbd_transfer_power_ref(xfer, 1);
#endif
	/*
	 * Check if the transfer is waiting on a queue, most
	 * frequently the "done_q":
	 */
	if (xfer->wait_queue) {
		USB_BUS_LOCK(bus);
		usbd_transfer_dequeue(xfer);
		USB_BUS_UNLOCK(bus);
	}
	/* clear "did_dma_delay" flag */
	xfer->flags_int.did_dma_delay = 0;

	/* clear "did_close" flag */
	xfer->flags_int.did_close = 0;

#if USB_HAVE_BUSDMA
	/* clear "bdma_setup" flag */
	xfer->flags_int.bdma_setup = 0;
#endif
	/* by default we cannot cancel any USB transfer immediately */
	xfer->flags_int.can_cancel_immed = 0;

	/* clear lengths and frame counts by default */
	xfer->sumlen = 0;
	xfer->actlen = 0;
	xfer->aframes = 0;

	/* clear any previous errors */
	xfer->error = 0;

	/* Check if the device is still alive */
	if (info->udev->state < USB_STATE_POWERED) {
		USB_BUS_LOCK(bus);
		/*
		 * Must return cancelled error code else
		 * device drivers can hang.
		 */
		usbd_transfer_done(xfer, USB_ERR_CANCELLED);
		USB_BUS_UNLOCK(bus);
		return;
	}

	/* sanity check */
	if (xfer->nframes == 0) {
		if (xfer->flags.stall_pipe) {
			/*
			 * Special case - want to stall without transferring
			 * any data:
			 */
			DPRINTF("xfer=%p nframes=0: stall "
			    "or clear stall!\n", xfer);
			USB_BUS_LOCK(bus);
			xfer->flags_int.can_cancel_immed = 1;
			/* start the transfer */
			usb_command_wrapper(&xfer->endpoint->endpoint_q, xfer);
			USB_BUS_UNLOCK(bus);
			return;
		}
		USB_BUS_LOCK(bus);
		usbd_transfer_done(xfer, USB_ERR_INVAL);
		USB_BUS_UNLOCK(bus);
		return;
	}
	/* compute total transfer length */

	for (x = 0; x != xfer->nframes; x++) {
		xfer->sumlen += xfer->frlengths[x];
		if (xfer->sumlen < xfer->frlengths[x]) {
			/* length wrapped around */
			USB_BUS_LOCK(bus);
			usbd_transfer_done(xfer, USB_ERR_INVAL);
			USB_BUS_UNLOCK(bus);
			return;
		}
	}

	/* clear some internal flags */

	xfer->flags_int.short_xfer_ok = 0;
	xfer->flags_int.short_frames_ok = 0;

	/* check if this is a control transfer */

	if (xfer->flags_int.control_xfr) {

		if (usbd_setup_ctrl_transfer(xfer)) {
			USB_BUS_LOCK(bus);
			usbd_transfer_done(xfer, USB_ERR_STALLED);
			USB_BUS_UNLOCK(bus);
			return;
		}
	}
	/*
	 * Setup filtered version of some transfer flags,
	 * in case of data read direction
	 */
	if (USB_GET_DATA_ISREAD(xfer)) {

		if (xfer->flags.short_frames_ok) {
			xfer->flags_int.short_xfer_ok = 1;
			xfer->flags_int.short_frames_ok = 1;
		} else if (xfer->flags.short_xfer_ok) {
			xfer->flags_int.short_xfer_ok = 1;

			/* check for control transfer */
			if (xfer->flags_int.control_xfr) {
				/*
				 * 1) Control transfers do not support
				 * reception of multiple short USB
				 * frames in host mode and device side
				 * mode, with exception of:
				 *
				 * 2) Due to sometimes buggy device
				 * side firmware we need to do a
				 * STATUS stage in case of short
				 * control transfers in USB host mode.
				 * The STATUS stage then becomes the
				 * "alt_next" to the DATA stage.
				 */
				xfer->flags_int.short_frames_ok = 1;
			}
		}
	}
	/*
	 * Check if BUS-DMA support is enabled and try to load virtual
	 * buffers into DMA, if any:
	 */
#if USB_HAVE_BUSDMA
	if (xfer->flags_int.bdma_enable) {
		/* insert the USB transfer last in the BUS-DMA queue */
		usb_command_wrapper(&xfer->xroot->dma_q, xfer);
		return;
	}
#endif
	/*
	 * Enter the USB transfer into the Host Controller or
	 * Device Controller schedule:
	 */
	usbd_pipe_enter(xfer);
}

/*------------------------------------------------------------------------*
 *	usbd_pipe_enter - factored out code
 *------------------------------------------------------------------------*/
void
usbd_pipe_enter(struct usb_xfer *xfer)
{
	struct usb_endpoint *ep;

	USB_XFER_LOCK_ASSERT(xfer, MA_OWNED);

	USB_BUS_LOCK(xfer->xroot->bus);

	ep = xfer->endpoint;

	DPRINTF("enter\n");

	/* enter the transfer */
	(ep->methods->enter) (xfer);

	xfer->flags_int.can_cancel_immed = 1;

	/* check for transfer error */
	if (xfer->error) {
		/* some error has happened */
		usbd_transfer_done(xfer, 0);
		USB_BUS_UNLOCK(xfer->xroot->bus);
		return;
	}

	/* start the transfer */
	usb_command_wrapper(&ep->endpoint_q, xfer);
	USB_BUS_UNLOCK(xfer->xroot->bus);
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_start - start an USB transfer
 *
 * NOTE: Calling this function more than one time will only
 *       result in a single transfer start, until the USB transfer
 *       completes.
 *------------------------------------------------------------------------*/
void
usbd_transfer_start(struct usb_xfer *xfer)
{
	if (xfer == NULL) {
		/* transfer is gone */
		return;
	}
	USB_XFER_LOCK_ASSERT(xfer, MA_OWNED);

	/* mark the USB transfer started */

	if (!xfer->flags_int.started) {
		xfer->flags_int.started = 1;
	}
	/* check if the USB transfer callback is already transferring */

	if (xfer->flags_int.transferring) {
		return;
	}
	USB_BUS_LOCK(xfer->xroot->bus);
	/* call the USB transfer callback */
	usbd_callback_ss_done_defer(xfer);
	USB_BUS_UNLOCK(xfer->xroot->bus);
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_stop - stop an USB transfer
 *
 * NOTE: Calling this function more than one time will only
 *       result in a single transfer stop.
 * NOTE: When this function returns it is not safe to free nor
 *       reuse any DMA buffers. See "usbd_transfer_drain()".
 *------------------------------------------------------------------------*/
void
usbd_transfer_stop(struct usb_xfer *xfer)
{
	struct usb_endpoint *ep;

	if (xfer == NULL) {
		/* transfer is gone */
		return;
	}
	USB_XFER_LOCK_ASSERT(xfer, MA_OWNED);

	/* check if the USB transfer was ever opened */

	if (!xfer->flags_int.open) {
		/* nothing to do except clearing the "started" flag */
		xfer->flags_int.started = 0;
		return;
	}
	/* try to stop the current USB transfer */

	USB_BUS_LOCK(xfer->xroot->bus);
	xfer->error = USB_ERR_CANCELLED;/* override any previous error */
	/*
	 * Clear "open" and "started" when both private and USB lock
	 * is locked so that we don't get a race updating "flags_int"
	 */
	xfer->flags_int.open = 0;
	xfer->flags_int.started = 0;

	/*
	 * Check if we can cancel the USB transfer immediately.
	 */
	if (xfer->flags_int.transferring) {
		if (xfer->flags_int.can_cancel_immed &&
		    (!xfer->flags_int.did_close)) {
			DPRINTF("close\n");
			/*
			 * The following will lead to an USB_ERR_CANCELLED
			 * error code being passed to the USB callback.
			 */
			(xfer->endpoint->methods->close) (xfer);
			/* only close once */
			xfer->flags_int.did_close = 1;
		} else {
			/* need to wait for the next done callback */
		}
	} else {
		DPRINTF("close\n");

		/* close here and now */
		(xfer->endpoint->methods->close) (xfer);

		/*
		 * Any additional DMA delay is done by
		 * "usbd_transfer_unsetup()".
		 */

		/*
		 * Special case. Check if we need to restart a blocked
		 * endpoint.
		 */
		ep = xfer->endpoint;

		/*
		 * If the current USB transfer is completing we need
		 * to start the next one:
		 */
		if (ep->endpoint_q.curr == xfer) {
			usb_command_wrapper(&ep->endpoint_q, NULL);
		}
	}

	USB_BUS_UNLOCK(xfer->xroot->bus);
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_pending
 *
 * This function will check if an USB transfer is pending which is a
 * little bit complicated!
 * Return values:
 * 0: Not pending
 * 1: Pending: The USB transfer will receive a callback in the future.
 *------------------------------------------------------------------------*/
uint8_t
usbd_transfer_pending(struct usb_xfer *xfer)
{
	struct usb_xfer_root *info;
	struct usb_xfer_queue *pq;

	if (xfer == NULL) {
		/* transfer is gone */
		return (0);
	}
	USB_XFER_LOCK_ASSERT(xfer, MA_OWNED);

	if (xfer->flags_int.transferring) {
		/* trivial case */
		return (1);
	}
	USB_BUS_LOCK(xfer->xroot->bus);
	if (xfer->wait_queue) {
		/* we are waiting on a queue somewhere */
		USB_BUS_UNLOCK(xfer->xroot->bus);
		return (1);
	}
	info = xfer->xroot;
	pq = &info->done_q;

	if (pq->curr == xfer) {
		/* we are currently scheduled for callback */
		USB_BUS_UNLOCK(xfer->xroot->bus);
		return (1);
	}
	/* we are not pending */
	USB_BUS_UNLOCK(xfer->xroot->bus);
	return (0);
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_drain
 *
 * This function will stop the USB transfer and wait for any
 * additional BUS-DMA and HW-DMA operations to complete. Buffers that
 * are loaded into DMA can safely be freed or reused after that this
 * function has returned.
 *------------------------------------------------------------------------*/
void
usbd_transfer_drain(struct usb_xfer *xfer)
{
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "usbd_transfer_drain can sleep!");

	if (xfer == NULL) {
		/* transfer is gone */
		return;
	}
	if (xfer->xroot->xfer_mtx != &Giant) {
		USB_XFER_LOCK_ASSERT(xfer, MA_NOTOWNED);
	}
	USB_XFER_LOCK(xfer);

	usbd_transfer_stop(xfer);

	while (usbd_transfer_pending(xfer)) {
		xfer->flags_int.draining = 1;
		/*
		 * Wait until the current outstanding USB
		 * transfer is complete !
		 */
		cv_wait(&xfer->xroot->cv_drain, xfer->xroot->xfer_mtx);
	}
	USB_XFER_UNLOCK(xfer);
}

struct usb_page_cache *
usbd_xfer_get_frame(struct usb_xfer *xfer, usb_frcount_t frindex)
{
	KASSERT(frindex < xfer->max_frame_count, ("frame index overflow"));

	return (&xfer->frbuffers[frindex]);
}

usb_frlength_t
usbd_xfer_frame_len(struct usb_xfer *xfer, usb_frcount_t frindex)
{
	KASSERT(frindex < xfer->max_frame_count, ("frame index overflow"));

	return (xfer->frlengths[frindex]);
}

/*------------------------------------------------------------------------*
 *	usbd_xfer_set_frame_data
 *
 * This function sets the pointer of the buffer that should
 * loaded directly into DMA for the given USB frame. Passing "ptr"
 * equal to NULL while the corresponding "frlength" is greater
 * than zero gives undefined results!
 *------------------------------------------------------------------------*/
void
usbd_xfer_set_frame_data(struct usb_xfer *xfer, usb_frcount_t frindex,
    void *ptr, usb_frlength_t len)
{
	KASSERT(frindex < xfer->max_frame_count, ("frame index overflow"));

	/* set virtual address to load and length */
	xfer->frbuffers[frindex].buffer = ptr;
	usbd_xfer_set_frame_len(xfer, frindex, len);
}

void
usbd_xfer_frame_data(struct usb_xfer *xfer, usb_frcount_t frindex,
    void **ptr, int *len)
{
	KASSERT(frindex < xfer->max_frame_count, ("frame index overflow"));

	if (ptr != NULL)
		*ptr = xfer->frbuffers[frindex].buffer;
	if (len != NULL)
		*len = xfer->frlengths[frindex];
}

void
usbd_xfer_status(struct usb_xfer *xfer, int *actlen, int *sumlen, int *aframes,
    int *nframes)
{
	if (actlen != NULL)
		*actlen = xfer->actlen;
	if (sumlen != NULL)
		*sumlen = xfer->sumlen;
	if (aframes != NULL)
		*aframes = xfer->aframes;
	if (nframes != NULL)
		*nframes = xfer->nframes;
}

/*------------------------------------------------------------------------*
 *	usbd_xfer_set_frame_offset
 *
 * This function sets the frame data buffer offset relative to the beginning
 * of the USB DMA buffer allocated for this USB transfer.
 *------------------------------------------------------------------------*/
void
usbd_xfer_set_frame_offset(struct usb_xfer *xfer, usb_frlength_t offset,
    usb_frcount_t frindex)
{
	KASSERT(!xfer->flags.ext_buffer, ("Cannot offset data frame "
	    "when the USB buffer is external!\n"));
	KASSERT(frindex < xfer->max_frame_count, ("frame index overflow"));

	/* set virtual address to load */
	xfer->frbuffers[frindex].buffer =
	    USB_ADD_BYTES(xfer->local_buffer, offset);
}

void
usbd_xfer_set_interval(struct usb_xfer *xfer, int i)
{
	xfer->interval = i;
}

void
usbd_xfer_set_timeout(struct usb_xfer *xfer, int t)
{
	xfer->timeout = t;
}

void
usbd_xfer_set_frames(struct usb_xfer *xfer, usb_frcount_t n)
{
	xfer->nframes = n;
}

usb_frcount_t
usbd_xfer_max_frames(struct usb_xfer *xfer)
{
	return (xfer->max_frame_count);
}

usb_frlength_t
usbd_xfer_max_len(struct usb_xfer *xfer)
{
	return (xfer->max_data_length);
}

usb_frlength_t
usbd_xfer_max_framelen(struct usb_xfer *xfer)
{
	return (xfer->max_frame_size);
}

void
usbd_xfer_set_frame_len(struct usb_xfer *xfer, usb_frcount_t frindex,
    usb_frlength_t len)
{
	KASSERT(frindex < xfer->max_frame_count, ("frame index overflow"));

	xfer->frlengths[frindex] = len;
}

/*------------------------------------------------------------------------*
 *	usb_callback_proc - factored out code
 *
 * This function performs USB callbacks.
 *------------------------------------------------------------------------*/
static void
usb_callback_proc(struct usb_proc_msg *_pm)
{
	struct usb_done_msg *pm = (void *)_pm;
	struct usb_xfer_root *info = pm->xroot;

	/* Change locking order */
	USB_BUS_UNLOCK(info->bus);

	/*
	 * We exploit the fact that the mutex is the same for all
	 * callbacks that will be called from this thread:
	 */
	mtx_lock(info->xfer_mtx);
	USB_BUS_LOCK(info->bus);

	/* Continue where we lost track */
	usb_command_wrapper(&info->done_q,
	    info->done_q.curr);

	mtx_unlock(info->xfer_mtx);
}

/*------------------------------------------------------------------------*
 *	usbd_callback_ss_done_defer
 *
 * This function will defer the start, stop and done callback to the
 * correct thread.
 *------------------------------------------------------------------------*/
static void
usbd_callback_ss_done_defer(struct usb_xfer *xfer)
{
	struct usb_xfer_root *info = xfer->xroot;
	struct usb_xfer_queue *pq = &info->done_q;

	USB_BUS_LOCK_ASSERT(xfer->xroot->bus, MA_OWNED);

	if (pq->curr != xfer) {
		usbd_transfer_enqueue(pq, xfer);
	}
	if (!pq->recurse_1) {

		/*
	         * We have to postpone the callback due to the fact we
	         * will have a Lock Order Reversal, LOR, if we try to
	         * proceed !
	         */
		if (usb_proc_msignal(info->done_p,
		    &info->done_m[0], &info->done_m[1])) {
			/* ignore */
		}
	} else {
		/* clear second recurse flag */
		pq->recurse_2 = 0;
	}
	return;

}

/*------------------------------------------------------------------------*
 *	usbd_callback_wrapper
 *
 * This is a wrapper for USB callbacks. This wrapper does some
 * auto-magic things like figuring out if we can call the callback
 * directly from the current context or if we need to wakeup the
 * interrupt process.
 *------------------------------------------------------------------------*/
static void
usbd_callback_wrapper(struct usb_xfer_queue *pq)
{
	struct usb_xfer *xfer = pq->curr;
	struct usb_xfer_root *info = xfer->xroot;

	USB_BUS_LOCK_ASSERT(info->bus, MA_OWNED);
	if (!mtx_owned(info->xfer_mtx)) {
		/*
	       	 * Cases that end up here:
		 *
		 * 5) HW interrupt done callback or other source.
		 */
		DPRINTFN(3, "case 5\n");

		/*
	         * We have to postpone the callback due to the fact we
	         * will have a Lock Order Reversal, LOR, if we try to
	         * proceed !
	         */
		if (usb_proc_msignal(info->done_p,
		    &info->done_m[0], &info->done_m[1])) {
			/* ignore */
		}
		return;
	}
	/*
	 * Cases that end up here:
	 *
	 * 1) We are starting a transfer
	 * 2) We are prematurely calling back a transfer
	 * 3) We are stopping a transfer
	 * 4) We are doing an ordinary callback
	 */
	DPRINTFN(3, "case 1-4\n");
	/* get next USB transfer in the queue */
	info->done_q.curr = NULL;

	USB_BUS_UNLOCK(info->bus);
	USB_BUS_LOCK_ASSERT(info->bus, MA_NOTOWNED);

	/* set correct USB state for callback */
	if (!xfer->flags_int.transferring) {
		xfer->usb_state = USB_ST_SETUP;
		if (!xfer->flags_int.started) {
			/* we got stopped before we even got started */
			USB_BUS_LOCK(info->bus);
			goto done;
		}
	} else {

		if (usbd_callback_wrapper_sub(xfer)) {
			/* the callback has been deferred */
			USB_BUS_LOCK(info->bus);
			goto done;
		}
#if USB_HAVE_POWERD
		/* decrement power reference */
		usbd_transfer_power_ref(xfer, -1);
#endif
		xfer->flags_int.transferring = 0;

		if (xfer->error) {
			xfer->usb_state = USB_ST_ERROR;
		} else {
			/* set transferred state */
			xfer->usb_state = USB_ST_TRANSFERRED;
#if USB_HAVE_BUSDMA
			/* sync DMA memory, if any */
			if (xfer->flags_int.bdma_enable &&
			    (!xfer->flags_int.bdma_no_post_sync)) {
				usb_bdma_post_sync(xfer);
			}
#endif
		}
	}

	/* call processing routine */
	(xfer->callback) (xfer, xfer->error);

	/* pickup the USB mutex again */
	USB_BUS_LOCK(info->bus);

	/*
	 * Check if we got started after that we got cancelled, but
	 * before we managed to do the callback.
	 */
	if ((!xfer->flags_int.open) &&
	    (xfer->flags_int.started) &&
	    (xfer->usb_state == USB_ST_ERROR)) {
		/* try to loop, but not recursivly */
		usb_command_wrapper(&info->done_q, xfer);
		return;
	}

done:
	/*
	 * Check if we are draining.
	 */
	if (xfer->flags_int.draining &&
	    (!xfer->flags_int.transferring)) {
		/* "usbd_transfer_drain()" is waiting for end of transfer */
		xfer->flags_int.draining = 0;
		cv_broadcast(&info->cv_drain);
	}

	/* do the next callback, if any */
	usb_command_wrapper(&info->done_q,
	    info->done_q.curr);
}

/*------------------------------------------------------------------------*
 *	usb_dma_delay_done_cb
 *
 * This function is called when the DMA delay has been exectuded, and
 * will make sure that the callback is called to complete the USB
 * transfer. This code path is ususally only used when there is an USB
 * error like USB_ERR_CANCELLED.
 *------------------------------------------------------------------------*/
static void
usb_dma_delay_done_cb(void *arg)
{
	struct usb_xfer *xfer = arg;

	USB_BUS_LOCK_ASSERT(xfer->xroot->bus, MA_OWNED);

	DPRINTFN(3, "Completed %p\n", xfer);

	/* only delay once */
	xfer->flags_int.did_dma_delay = 1;

	/* queue callback for execution, again */
	usbd_transfer_done(xfer, 0);
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_dequeue
 *
 *  - This function is used to remove an USB transfer from a USB
 *  transfer queue.
 *
 *  - This function can be called multiple times in a row.
 *------------------------------------------------------------------------*/
void
usbd_transfer_dequeue(struct usb_xfer *xfer)
{
	struct usb_xfer_queue *pq;

	pq = xfer->wait_queue;
	if (pq) {
		TAILQ_REMOVE(&pq->head, xfer, wait_entry);
		xfer->wait_queue = NULL;
	}
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_enqueue
 *
 *  - This function is used to insert an USB transfer into a USB *
 *  transfer queue.
 *
 *  - This function can be called multiple times in a row.
 *------------------------------------------------------------------------*/
void
usbd_transfer_enqueue(struct usb_xfer_queue *pq, struct usb_xfer *xfer)
{
	/*
	 * Insert the USB transfer into the queue, if it is not
	 * already on a USB transfer queue:
	 */
	if (xfer->wait_queue == NULL) {
		xfer->wait_queue = pq;
		TAILQ_INSERT_TAIL(&pq->head, xfer, wait_entry);
	}
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_done
 *
 *  - This function is used to remove an USB transfer from the busdma,
 *  pipe or interrupt queue.
 *
 *  - This function is used to queue the USB transfer on the done
 *  queue.
 *
 *  - This function is used to stop any USB transfer timeouts.
 *------------------------------------------------------------------------*/
void
usbd_transfer_done(struct usb_xfer *xfer, usb_error_t error)
{
	USB_BUS_LOCK_ASSERT(xfer->xroot->bus, MA_OWNED);

	DPRINTF("err=%s\n", usbd_errstr(error));

	/*
	 * If we are not transferring then just return.
	 * This can happen during transfer cancel.
	 */
	if (!xfer->flags_int.transferring) {
		DPRINTF("not transferring\n");
		return;
	}
	/* only set transfer error if not already set */
	if (!xfer->error) {
		xfer->error = error;
	}
	/* stop any callouts */
	usb_callout_stop(&xfer->timeout_handle);

	/*
	 * If we are waiting on a queue, just remove the USB transfer
	 * from the queue, if any. We should have the required locks
	 * locked to do the remove when this function is called.
	 */
	usbd_transfer_dequeue(xfer);

#if USB_HAVE_BUSDMA
	if (mtx_owned(xfer->xroot->xfer_mtx)) {
		struct usb_xfer_queue *pq;

		/*
		 * If the private USB lock is not locked, then we assume
		 * that the BUS-DMA load stage has been passed:
		 */
		pq = &xfer->xroot->dma_q;

		if (pq->curr == xfer) {
			/* start the next BUS-DMA load, if any */
			usb_command_wrapper(pq, NULL);
		}
	}
#endif
	/* keep some statistics */
	if (xfer->error) {
		xfer->xroot->bus->stats_err.uds_requests
		    [xfer->endpoint->edesc->bmAttributes & UE_XFERTYPE]++;
	} else {
		xfer->xroot->bus->stats_ok.uds_requests
		    [xfer->endpoint->edesc->bmAttributes & UE_XFERTYPE]++;
	}

	/* call the USB transfer callback */
	usbd_callback_ss_done_defer(xfer);
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_start_cb
 *
 * This function is called to start the USB transfer when
 * "xfer->interval" is greater than zero, and and the endpoint type is
 * BULK or CONTROL.
 *------------------------------------------------------------------------*/
static void
usbd_transfer_start_cb(void *arg)
{
	struct usb_xfer *xfer = arg;
	struct usb_endpoint *ep = xfer->endpoint;

	USB_BUS_LOCK_ASSERT(xfer->xroot->bus, MA_OWNED);

	DPRINTF("start\n");

	/* start the transfer */
	(ep->methods->start) (xfer);

	xfer->flags_int.can_cancel_immed = 1;

	/* check for error */
	if (xfer->error) {
		/* some error has happened */
		usbd_transfer_done(xfer, 0);
	}
}

/*------------------------------------------------------------------------*
 *	usbd_xfer_set_stall
 *
 * This function is used to set the stall flag outside the
 * callback. This function is NULL safe.
 *------------------------------------------------------------------------*/
void
usbd_xfer_set_stall(struct usb_xfer *xfer)
{
	if (xfer == NULL) {
		/* tearing down */
		return;
	}
	USB_XFER_LOCK_ASSERT(xfer, MA_OWNED);

	/* avoid any races by locking the USB mutex */
	USB_BUS_LOCK(xfer->xroot->bus);
	xfer->flags.stall_pipe = 1;
	USB_BUS_UNLOCK(xfer->xroot->bus);
}

int
usbd_xfer_is_stalled(struct usb_xfer *xfer)
{
	return (xfer->endpoint->is_stalled);
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_clear_stall
 *
 * This function is used to clear the stall flag outside the
 * callback. This function is NULL safe.
 *------------------------------------------------------------------------*/
void
usbd_transfer_clear_stall(struct usb_xfer *xfer)
{
	if (xfer == NULL) {
		/* tearing down */
		return;
	}
	USB_XFER_LOCK_ASSERT(xfer, MA_OWNED);

	/* avoid any races by locking the USB mutex */
	USB_BUS_LOCK(xfer->xroot->bus);

	xfer->flags.stall_pipe = 0;

	USB_BUS_UNLOCK(xfer->xroot->bus);
}

/*------------------------------------------------------------------------*
 *	usbd_pipe_start
 *
 * This function is used to add an USB transfer to the pipe transfer list.
 *------------------------------------------------------------------------*/
void
usbd_pipe_start(struct usb_xfer_queue *pq)
{
	struct usb_endpoint *ep;
	struct usb_xfer *xfer;
	uint8_t type;

	xfer = pq->curr;
	ep = xfer->endpoint;

	USB_BUS_LOCK_ASSERT(xfer->xroot->bus, MA_OWNED);

	/*
	 * If the endpoint is already stalled we do nothing !
	 */
	if (ep->is_stalled) {
		return;
	}
	/*
	 * Check if we are supposed to stall the endpoint:
	 */
	if (xfer->flags.stall_pipe) {
		/* clear stall command */
		xfer->flags.stall_pipe = 0;

		/*
		 * Only stall BULK and INTERRUPT endpoints.
		 */
		type = (ep->edesc->bmAttributes & UE_XFERTYPE);
		if ((type == UE_BULK) ||
		    (type == UE_INTERRUPT)) {
			struct usb_device *udev;
			struct usb_xfer_root *info;
			uint8_t did_stall;

			info = xfer->xroot;
			udev = info->udev;
			did_stall = 1;

			if (udev->flags.usb_mode == USB_MODE_DEVICE) {
				(udev->bus->methods->set_stall) (
				    udev, NULL, ep, &did_stall);
			} else if (udev->default_xfer[1]) {
				info = udev->default_xfer[1]->xroot;
				usb_proc_msignal(
				    &info->bus->non_giant_callback_proc,
				    &udev->cs_msg[0], &udev->cs_msg[1]);
			} else {
				/* should not happen */
				DPRINTFN(0, "No stall handler!\n");
			}
			/*
			 * Check if we should stall. Some USB hardware
			 * handles set- and clear-stall in hardware.
			 */
			if (did_stall) {
				/*
				 * The transfer will be continued when
				 * the clear-stall control endpoint
				 * message is received.
				 */
				ep->is_stalled = 1;
				return;
			}
		}
	}
	/* Set or clear stall complete - special case */
	if (xfer->nframes == 0) {
		/* we are complete */
		xfer->aframes = 0;
		usbd_transfer_done(xfer, 0);
		return;
	}
	/*
	 * Handled cases:
	 *
	 * 1) Start the first transfer queued.
	 *
	 * 2) Re-start the current USB transfer.
	 */
	/*
	 * Check if there should be any
	 * pre transfer start delay:
	 */
	if (xfer->interval > 0) {
		type = (ep->edesc->bmAttributes & UE_XFERTYPE);
		if ((type == UE_BULK) ||
		    (type == UE_CONTROL)) {
			usbd_transfer_timeout_ms(xfer,
			    &usbd_transfer_start_cb,
			    xfer->interval);
			return;
		}
	}
	DPRINTF("start\n");

	/* start USB transfer */
	(ep->methods->start) (xfer);

	xfer->flags_int.can_cancel_immed = 1;

	/* check for error */
	if (xfer->error) {
		/* some error has happened */
		usbd_transfer_done(xfer, 0);
	}
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_timeout_ms
 *
 * This function is used to setup a timeout on the given USB
 * transfer. If the timeout has been deferred the callback given by
 * "cb" will get called after "ms" milliseconds.
 *------------------------------------------------------------------------*/
void
usbd_transfer_timeout_ms(struct usb_xfer *xfer,
    void (*cb) (void *arg), usb_timeout_t ms)
{
	USB_BUS_LOCK_ASSERT(xfer->xroot->bus, MA_OWNED);

	/* defer delay */
	usb_callout_reset(&xfer->timeout_handle,
	    USB_MS_TO_TICKS(ms), cb, xfer);
}

/*------------------------------------------------------------------------*
 *	usbd_callback_wrapper_sub
 *
 *  - This function will update variables in an USB transfer after
 *  that the USB transfer is complete.
 *
 *  - This function is used to start the next USB transfer on the
 *  ep transfer queue, if any.
 *
 * NOTE: In some special cases the USB transfer will not be removed from
 * the pipe queue, but remain first. To enforce USB transfer removal call
 * this function passing the error code "USB_ERR_CANCELLED".
 *
 * Return values:
 * 0: Success.
 * Else: The callback has been deferred.
 *------------------------------------------------------------------------*/
static uint8_t
usbd_callback_wrapper_sub(struct usb_xfer *xfer)
{
	struct usb_endpoint *ep;
	usb_frcount_t x;

	if ((!xfer->flags_int.open) &&
	    (!xfer->flags_int.did_close)) {
		DPRINTF("close\n");
		USB_BUS_LOCK(xfer->xroot->bus);
		(xfer->endpoint->methods->close) (xfer);
		USB_BUS_UNLOCK(xfer->xroot->bus);
		/* only close once */
		xfer->flags_int.did_close = 1;
		return (1);		/* wait for new callback */
	}
	/*
	 * If we have a non-hardware induced error we
	 * need to do the DMA delay!
	 */
	if (((xfer->error == USB_ERR_CANCELLED) ||
	    (xfer->error == USB_ERR_TIMEOUT)) &&
	    (!xfer->flags_int.did_dma_delay)) {

		usb_timeout_t temp;

		/* we can not cancel this delay */
		xfer->flags_int.can_cancel_immed = 0;

		temp = usbd_get_dma_delay(xfer->xroot->bus);

		DPRINTFN(3, "DMA delay, %u ms, "
		    "on %p\n", temp, xfer);

		if (temp != 0) {
			USB_BUS_LOCK(xfer->xroot->bus);
			usbd_transfer_timeout_ms(xfer,
			    &usb_dma_delay_done_cb, temp);
			USB_BUS_UNLOCK(xfer->xroot->bus);
			return (1);	/* wait for new callback */
		}
	}
	/* check actual number of frames */
	if (xfer->aframes > xfer->nframes) {
		if (xfer->error == 0) {
			panic("%s: actual number of frames, %d, is "
			    "greater than initial number of frames, %d!\n",
			    __FUNCTION__, xfer->aframes, xfer->nframes);
		} else {
			/* just set some valid value */
			xfer->aframes = xfer->nframes;
		}
	}
	/* compute actual length */
	xfer->actlen = 0;

	for (x = 0; x != xfer->aframes; x++) {
		xfer->actlen += xfer->frlengths[x];
	}

	/*
	 * Frames that were not transferred get zero actual length in
	 * case the USB device driver does not check the actual number
	 * of frames transferred, "xfer->aframes":
	 */
	for (; x < xfer->nframes; x++) {
		usbd_xfer_set_frame_len(xfer, x, 0);
	}

	/* check actual length */
	if (xfer->actlen > xfer->sumlen) {
		if (xfer->error == 0) {
			panic("%s: actual length, %d, is greater than "
			    "initial length, %d!\n",
			    __FUNCTION__, xfer->actlen, xfer->sumlen);
		} else {
			/* just set some valid value */
			xfer->actlen = xfer->sumlen;
		}
	}
	DPRINTFN(1, "xfer=%p endpoint=%p sts=%d alen=%d, slen=%d, afrm=%d, nfrm=%d\n",
	    xfer, xfer->endpoint, xfer->error, xfer->actlen, xfer->sumlen,
	    xfer->aframes, xfer->nframes);

	if (xfer->error) {
		/* end of control transfer, if any */
		xfer->flags_int.control_act = 0;

		/* check if we should block the execution queue */
		if ((xfer->error != USB_ERR_CANCELLED) &&
		    (xfer->flags.pipe_bof)) {
			DPRINTFN(2, "xfer=%p: Block On Failure "
			    "on endpoint=%p\n", xfer, xfer->endpoint);
			goto done;
		}
	} else {
		/* check for short transfers */
		if (xfer->actlen < xfer->sumlen) {

			/* end of control transfer, if any */
			xfer->flags_int.control_act = 0;

			if (!xfer->flags_int.short_xfer_ok) {
				xfer->error = USB_ERR_SHORT_XFER;
				if (xfer->flags.pipe_bof) {
					DPRINTFN(2, "xfer=%p: Block On Failure on "
					    "Short Transfer on endpoint %p.\n",
					    xfer, xfer->endpoint);
					goto done;
				}
			}
		} else {
			/*
			 * Check if we are in the middle of a
			 * control transfer:
			 */
			if (xfer->flags_int.control_act) {
				DPRINTFN(5, "xfer=%p: Control transfer "
				    "active on endpoint=%p\n", xfer, xfer->endpoint);
				goto done;
			}
		}
	}

	ep = xfer->endpoint;

	/*
	 * If the current USB transfer is completing we need to start the
	 * next one:
	 */
	USB_BUS_LOCK(xfer->xroot->bus);
	if (ep->endpoint_q.curr == xfer) {
		usb_command_wrapper(&ep->endpoint_q, NULL);

		if (ep->endpoint_q.curr || TAILQ_FIRST(&ep->endpoint_q.head)) {
			/* there is another USB transfer waiting */
		} else {
			/* this is the last USB transfer */
			/* clear isochronous sync flag */
			xfer->endpoint->is_synced = 0;
		}
	}
	USB_BUS_UNLOCK(xfer->xroot->bus);
done:
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb_command_wrapper
 *
 * This function is used to execute commands non-recursivly on an USB
 * transfer.
 *------------------------------------------------------------------------*/
void
usb_command_wrapper(struct usb_xfer_queue *pq, struct usb_xfer *xfer)
{
	if (xfer) {
		/*
		 * If the transfer is not already processing,
		 * queue it!
		 */
		if (pq->curr != xfer) {
			usbd_transfer_enqueue(pq, xfer);
			if (pq->curr != NULL) {
				/* something is already processing */
				DPRINTFN(6, "busy %p\n", pq->curr);
				return;
			}
		}
	} else {
		/* Get next element in queue */
		pq->curr = NULL;
	}

	if (!pq->recurse_1) {

		do {

			/* set both recurse flags */
			pq->recurse_1 = 1;
			pq->recurse_2 = 1;

			if (pq->curr == NULL) {
				xfer = TAILQ_FIRST(&pq->head);
				if (xfer) {
					TAILQ_REMOVE(&pq->head, xfer,
					    wait_entry);
					xfer->wait_queue = NULL;
					pq->curr = xfer;
				} else {
					break;
				}
			}
			DPRINTFN(6, "cb %p (enter)\n", pq->curr);
			(pq->command) (pq);
			DPRINTFN(6, "cb %p (leave)\n", pq->curr);

		} while (!pq->recurse_2);

		/* clear first recurse flag */
		pq->recurse_1 = 0;

	} else {
		/* clear second recurse flag */
		pq->recurse_2 = 0;
	}
}

/*------------------------------------------------------------------------*
 *	usbd_default_transfer_setup
 *
 * This function is used to setup the default USB control endpoint
 * transfer.
 *------------------------------------------------------------------------*/
void
usbd_default_transfer_setup(struct usb_device *udev)
{
	struct usb_xfer *xfer;
	uint8_t no_resetup;
	uint8_t iface_index;

	/* check for root HUB */
	if (udev->parent_hub == NULL)
		return;
repeat:

	xfer = udev->default_xfer[0];
	if (xfer) {
		USB_XFER_LOCK(xfer);
		no_resetup =
		    ((xfer->address == udev->address) &&
		    (udev->default_ep_desc.wMaxPacketSize[0] ==
		    udev->ddesc.bMaxPacketSize));
		if (udev->flags.usb_mode == USB_MODE_DEVICE) {
			if (no_resetup) {
				/*
				 * NOTE: checking "xfer->address" and
				 * starting the USB transfer must be
				 * atomic!
				 */
				usbd_transfer_start(xfer);
			}
		}
		USB_XFER_UNLOCK(xfer);
	} else {
		no_resetup = 0;
	}

	if (no_resetup) {
		/*
	         * All parameters are exactly the same like before.
	         * Just return.
	         */
		return;
	}
	/*
	 * Update wMaxPacketSize for the default control endpoint:
	 */
	udev->default_ep_desc.wMaxPacketSize[0] =
	    udev->ddesc.bMaxPacketSize;

	/*
	 * Unsetup any existing USB transfer:
	 */
	usbd_transfer_unsetup(udev->default_xfer, USB_DEFAULT_XFER_MAX);

	/*
	 * Try to setup a new USB transfer for the
	 * default control endpoint:
	 */
	iface_index = 0;
	if (usbd_transfer_setup(udev, &iface_index,
	    udev->default_xfer, usb_control_ep_cfg, USB_DEFAULT_XFER_MAX, NULL,
	    udev->default_mtx)) {
		DPRINTFN(0, "could not setup default "
		    "USB transfer!\n");
	} else {
		goto repeat;
	}
}

/*------------------------------------------------------------------------*
 *	usbd_clear_data_toggle - factored out code
 *
 * NOTE: the intention of this function is not to reset the hardware
 * data toggle.
 *------------------------------------------------------------------------*/
void
usbd_clear_data_toggle(struct usb_device *udev, struct usb_endpoint *ep)
{
	DPRINTFN(5, "udev=%p endpoint=%p\n", udev, ep);

	USB_BUS_LOCK(udev->bus);
	ep->toggle_next = 0;
	USB_BUS_UNLOCK(udev->bus);
}

/*------------------------------------------------------------------------*
 *	usbd_clear_stall_callback - factored out clear stall callback
 *
 * Input parameters:
 *  xfer1: Clear Stall Control Transfer
 *  xfer2: Stalled USB Transfer
 *
 * This function is NULL safe.
 *
 * Return values:
 *   0: In progress
 *   Else: Finished
 *
 * Clear stall config example:
 *
 * static const struct usb_config my_clearstall =  {
 *	.type = UE_CONTROL,
 *	.endpoint = 0,
 *	.direction = UE_DIR_ANY,
 *	.interval = 50, //50 milliseconds
 *	.bufsize = sizeof(struct usb_device_request),
 *	.timeout = 1000, //1.000 seconds
 *	.callback = &my_clear_stall_callback, // **
 *	.usb_mode = USB_MODE_HOST,
 * };
 *
 * ** "my_clear_stall_callback" calls "usbd_clear_stall_callback"
 * passing the correct parameters.
 *------------------------------------------------------------------------*/
uint8_t
usbd_clear_stall_callback(struct usb_xfer *xfer1,
    struct usb_xfer *xfer2)
{
	struct usb_device_request req;

	if (xfer2 == NULL) {
		/* looks like we are tearing down */
		DPRINTF("NULL input parameter\n");
		return (0);
	}
	USB_XFER_LOCK_ASSERT(xfer1, MA_OWNED);
	USB_XFER_LOCK_ASSERT(xfer2, MA_OWNED);

	switch (USB_GET_STATE(xfer1)) {
	case USB_ST_SETUP:

		/*
		 * pre-clear the data toggle to DATA0 ("umass.c" and
		 * "ata-usb.c" depends on this)
		 */

		usbd_clear_data_toggle(xfer2->xroot->udev, xfer2->endpoint);

		/* setup a clear-stall packet */

		req.bmRequestType = UT_WRITE_ENDPOINT;
		req.bRequest = UR_CLEAR_FEATURE;
		USETW(req.wValue, UF_ENDPOINT_HALT);
		req.wIndex[0] = xfer2->endpoint->edesc->bEndpointAddress;
		req.wIndex[1] = 0;
		USETW(req.wLength, 0);

		/*
		 * "usbd_transfer_setup_sub()" will ensure that
		 * we have sufficient room in the buffer for
		 * the request structure!
		 */

		/* copy in the transfer */

		usbd_copy_in(xfer1->frbuffers, 0, &req, sizeof(req));

		/* set length */
		xfer1->frlengths[0] = sizeof(req);
		xfer1->nframes = 1;

		usbd_transfer_submit(xfer1);
		return (0);

	case USB_ST_TRANSFERRED:
		break;

	default:			/* Error */
		if (xfer1->error == USB_ERR_CANCELLED) {
			return (0);
		}
		break;
	}
	return (1);			/* Clear Stall Finished */
}

/*------------------------------------------------------------------------*
 *	usbd_transfer_poll
 *
 * The following function gets called from the USB keyboard driver and
 * UMASS when the system has paniced.
 *
 * NOTE: It is currently not possible to resume normal operation on
 * the USB controller which has been polled, due to clearing of the
 * "up_dsleep" and "up_msleep" flags.
 *------------------------------------------------------------------------*/
void
usbd_transfer_poll(struct usb_xfer **ppxfer, uint16_t max)
{
	struct usb_xfer *xfer;
	struct usb_xfer_root *xroot;
	struct usb_device *udev;
	struct usb_proc_msg *pm;
	uint16_t n;
	uint16_t drop_bus;
	uint16_t drop_xfer;

	for (n = 0; n != max; n++) {
		/* Extra checks to avoid panic */
		xfer = ppxfer[n];
		if (xfer == NULL)
			continue;	/* no USB transfer */
		xroot = xfer->xroot;
		if (xroot == NULL)
			continue;	/* no USB root */
		udev = xroot->udev;
		if (udev == NULL)
			continue;	/* no USB device */
		if (udev->bus == NULL)
			continue;	/* no BUS structure */
		if (udev->bus->methods == NULL)
			continue;	/* no BUS methods */
		if (udev->bus->methods->xfer_poll == NULL)
			continue;	/* no poll method */

		/* make sure that the BUS mutex is not locked */
		drop_bus = 0;
		while (mtx_owned(&xroot->udev->bus->bus_mtx)) {
			mtx_unlock(&xroot->udev->bus->bus_mtx);
			drop_bus++;
		}

		/* make sure that the transfer mutex is not locked */
		drop_xfer = 0;
		while (mtx_owned(xroot->xfer_mtx)) {
			mtx_unlock(xroot->xfer_mtx);
			drop_xfer++;
		}

		/* Make sure cv_signal() and cv_broadcast() is not called */
		udev->bus->control_xfer_proc.up_msleep = 0;
		udev->bus->explore_proc.up_msleep = 0;
		udev->bus->giant_callback_proc.up_msleep = 0;
		udev->bus->non_giant_callback_proc.up_msleep = 0;

		/* poll USB hardware */
		(udev->bus->methods->xfer_poll) (udev->bus);

		USB_BUS_LOCK(xroot->bus);

		/* check for clear stall */
		if (udev->default_xfer[1] != NULL) {

			/* poll clear stall start */
			pm = &udev->cs_msg[0].hdr;
			(pm->pm_callback) (pm);
			/* poll clear stall done thread */
			pm = &udev->default_xfer[1]->
			    xroot->done_m[0].hdr;
			(pm->pm_callback) (pm);
		}

		/* poll done thread */
		pm = &xroot->done_m[0].hdr;
		(pm->pm_callback) (pm);

		USB_BUS_UNLOCK(xroot->bus);

		/* restore transfer mutex */
		while (drop_xfer--)
			mtx_lock(xroot->xfer_mtx);

		/* restore BUS mutex */
		while (drop_bus--)
			mtx_lock(&xroot->udev->bus->bus_mtx);
	}
}

static void
usbd_get_std_packet_size(struct usb_std_packet_size *ptr,
    uint8_t type, enum usb_dev_speed speed)
{
	static const uint16_t intr_range_max[USB_SPEED_MAX] = {
		[USB_SPEED_LOW] = 8,
		[USB_SPEED_FULL] = 64,
		[USB_SPEED_HIGH] = 1024,
		[USB_SPEED_VARIABLE] = 1024,
		[USB_SPEED_SUPER] = 1024,
	};

	static const uint16_t isoc_range_max[USB_SPEED_MAX] = {
		[USB_SPEED_LOW] = 0,	/* invalid */
		[USB_SPEED_FULL] = 1023,
		[USB_SPEED_HIGH] = 1024,
		[USB_SPEED_VARIABLE] = 3584,
		[USB_SPEED_SUPER] = 1024,
	};

	static const uint16_t control_min[USB_SPEED_MAX] = {
		[USB_SPEED_LOW] = 8,
		[USB_SPEED_FULL] = 8,
		[USB_SPEED_HIGH] = 64,
		[USB_SPEED_VARIABLE] = 512,
		[USB_SPEED_SUPER] = 512,
	};

	static const uint16_t bulk_min[USB_SPEED_MAX] = {
		[USB_SPEED_LOW] = 0,	/* not supported */
		[USB_SPEED_FULL] = 8,
		[USB_SPEED_HIGH] = 512,
		[USB_SPEED_VARIABLE] = 512,
		[USB_SPEED_SUPER] = 1024,
	};

	uint16_t temp;

	memset(ptr, 0, sizeof(*ptr));

	switch (type) {
	case UE_INTERRUPT:
		ptr->range.max = intr_range_max[speed];
		break;
	case UE_ISOCHRONOUS:
		ptr->range.max = isoc_range_max[speed];
		break;
	default:
		if (type == UE_BULK)
			temp = bulk_min[speed];
		else /* UE_CONTROL */
			temp = control_min[speed];

		/* default is fixed */
		ptr->fixed[0] = temp;
		ptr->fixed[1] = temp;
		ptr->fixed[2] = temp;
		ptr->fixed[3] = temp;

		if (speed == USB_SPEED_FULL) {
			/* multiple sizes */
			ptr->fixed[1] = 16;
			ptr->fixed[2] = 32;
			ptr->fixed[3] = 64;
		}
		if ((speed == USB_SPEED_VARIABLE) &&
		    (type == UE_BULK)) {
			/* multiple sizes */
			ptr->fixed[2] = 1024;
			ptr->fixed[3] = 1536;
		}
		break;
	}
}

void	*
usbd_xfer_softc(struct usb_xfer *xfer)
{
	return (xfer->priv_sc);
}

void *
usbd_xfer_get_priv(struct usb_xfer *xfer)
{
	return (xfer->priv_fifo);
}

void
usbd_xfer_set_priv(struct usb_xfer *xfer, void *ptr)
{
	xfer->priv_fifo = ptr;
}

uint8_t
usbd_xfer_state(struct usb_xfer *xfer)
{
	return (xfer->usb_state);
}

void
usbd_xfer_set_flag(struct usb_xfer *xfer, int flag)
{
	switch (flag) {
		case USB_FORCE_SHORT_XFER:
			xfer->flags.force_short_xfer = 1;
			break;
		case USB_SHORT_XFER_OK:
			xfer->flags.short_xfer_ok = 1;
			break;
		case USB_MULTI_SHORT_OK:
			xfer->flags.short_frames_ok = 1;
			break;
		case USB_MANUAL_STATUS:
			xfer->flags.manual_status = 1;
			break;
	}
}

void
usbd_xfer_clr_flag(struct usb_xfer *xfer, int flag)
{
	switch (flag) {
		case USB_FORCE_SHORT_XFER:
			xfer->flags.force_short_xfer = 0;
			break;
		case USB_SHORT_XFER_OK:
			xfer->flags.short_xfer_ok = 0;
			break;
		case USB_MULTI_SHORT_OK:
			xfer->flags.short_frames_ok = 0;
			break;
		case USB_MANUAL_STATUS:
			xfer->flags.manual_status = 0;
			break;
	}
}

/*
 * The following function returns in milliseconds when the isochronous
 * transfer was completed by the hardware. The returned value wraps
 * around 65536 milliseconds.
 */
uint16_t
usbd_xfer_get_timestamp(struct usb_xfer *xfer)
{
	return (xfer->isoc_time_complete);
}
