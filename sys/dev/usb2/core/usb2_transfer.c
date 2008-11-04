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

#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_defs.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_transfer.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/controller/usb2_controller.h>
#include <dev/usb2/controller/usb2_bus.h>

struct usb2_std_packet_size {
	struct {
		uint16_t min;		/* inclusive */
		uint16_t max;		/* inclusive */
	}	range;

	uint16_t fixed[4];
};

/*
 * This table stores the all the allowed packet sizes based on
 * endpoint type and USB speed:
 */
static const struct usb2_std_packet_size
	usb2_std_packet_size[4][USB_SPEED_MAX] = {

	[UE_INTERRUPT] = {
		[USB_SPEED_LOW] = {.range = {0, 8}},
		[USB_SPEED_FULL] = {.range = {0, 64}},
		[USB_SPEED_HIGH] = {.range = {0, 1024}},
		[USB_SPEED_VARIABLE] = {.range = {0, 1024}},
	},

	[UE_CONTROL] = {
		[USB_SPEED_LOW] = {.fixed = {8, 8, 8, 8}},
		[USB_SPEED_FULL] = {.fixed = {8, 16, 32, 64}},
		[USB_SPEED_HIGH] = {.fixed = {64, 64, 64, 64}},
		[USB_SPEED_VARIABLE] = {.fixed = {512, 512, 512, 512}},
	},

	[UE_BULK] = {
		[USB_SPEED_LOW] = {.fixed = {0, 0, 0, 0}},	/* invalid */
		[USB_SPEED_FULL] = {.fixed = {8, 16, 32, 64}},
		[USB_SPEED_HIGH] = {.fixed = {512, 512, 512, 512}},
		[USB_SPEED_VARIABLE] = {.fixed = {512, 512, 1024, 1536}},
	},

	[UE_ISOCHRONOUS] = {
		[USB_SPEED_LOW] = {.fixed = {0, 0, 0, 0}},	/* invalid */
		[USB_SPEED_FULL] = {.range = {0, 1023}},
		[USB_SPEED_HIGH] = {.range = {0, 1024}},
		[USB_SPEED_VARIABLE] = {.range = {0, 3584}},
	},
};

static const struct usb2_config usb2_control_ep_cfg[USB_DEFAULT_XFER_MAX] = {

	/* This transfer is used for generic control endpoint transfers */

	[0] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control endpoint */
		.direction = UE_DIR_ANY,
		.mh.bufsize = 1024,	/* bytes */
		.mh.flags = {.proxy_buffer = 1,.short_xfer_ok = 1,},
		.mh.callback = &usb2_do_request_callback,
		.md.bufsize = 1024,	/* bytes */
		.md.flags = {.proxy_buffer = 1,.short_xfer_ok = 0,},
		.md.callback = &usb2_handle_request_callback,
	},

	/* This transfer is used for generic clear stall only */

	[1] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &usb2_do_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

/* function prototypes */

static void usb2_update_max_frame_size(struct usb2_xfer *xfer);
static uint32_t usb2_get_dma_delay(struct usb2_bus *bus);
static void usb2_transfer_unsetup_sub(struct usb2_xfer_root *info, uint8_t needs_delay);
static void usb2_control_transfer_init(struct usb2_xfer *xfer);
static uint8_t usb2_start_hardware_sub(struct usb2_xfer *xfer);
static void usb2_callback_proc(struct usb2_proc_msg *_pm);
static void usb2_callback_ss_done_defer(struct usb2_xfer *xfer);
static void usb2_callback_wrapper(struct usb2_xfer_queue *pq);
static void usb2_dma_delay_done_cb(void *arg);
static void usb2_transfer_start_cb(void *arg);
static uint8_t usb2_callback_wrapper_sub(struct usb2_xfer *xfer);

/*------------------------------------------------------------------------*
 *	usb2_update_max_frame_size
 *
 * This function updates the maximum frame size, hence high speed USB
 * can transfer multiple consecutive packets.
 *------------------------------------------------------------------------*/
static void
usb2_update_max_frame_size(struct usb2_xfer *xfer)
{
	/* compute maximum frame size */

	if (xfer->max_packet_count == 2) {
		xfer->max_frame_size = 2 * xfer->max_packet_size;
	} else if (xfer->max_packet_count == 3) {
		xfer->max_frame_size = 3 * xfer->max_packet_size;
	} else {
		xfer->max_frame_size = xfer->max_packet_size;
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_get_dma_delay
 *
 * The following function is called when we need to
 * synchronize with DMA hardware.
 *
 * Returns:
 *    0: no DMA delay required
 * Else: milliseconds of DMA delay
 *------------------------------------------------------------------------*/
static uint32_t
usb2_get_dma_delay(struct usb2_bus *bus)
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
 *	usb2_transfer_setup_sub_malloc
 *
 * This function will allocate one or more DMA'able memory chunks
 * according to "size", "align" and "count" arguments. "ppc" is
 * pointed to a linear array of USB page caches afterwards.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
uint8_t
usb2_transfer_setup_sub_malloc(struct usb2_setup_params *parm,
    struct usb2_page_cache **ppc, uint32_t size, uint32_t align,
    uint32_t count)
{
	struct usb2_page_cache *pc;
	struct usb2_page *pg;
	void *buf;
	uint32_t n_dma_pc;
	uint32_t n_obj;
	uint32_t x;
	uint32_t y;
	uint32_t r;
	uint32_t z;

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
		    &parm->curr_xfer->usb2_root->dma_parent_tag;
	}
	for (x = 0; x != count; x++) {
		/* need to initialize the page cache */
		parm->xfer_page_cache_ptr[x].tag_parent =
		    &parm->curr_xfer->usb2_root->dma_parent_tag;
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
		if (usb2_pc_alloc_mem(parm->dma_page_cache_ptr,
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
			if (usb2_pc_dmamap_create(pc, size)) {
				return (1);	/* failure */
			}
			pc->buffer = USB_ADD_BYTES(buf, y * size);
			pc->page_start = pg;

			mtx_lock(pc->tag_parent->mtx);
			if (usb2_pc_load_mem(pc, size, 1 /* synchronous */ )) {
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

/*------------------------------------------------------------------------*
 *	usb2_transfer_setup_sub - transfer setup subroutine
 *
 * This function must be called from the "xfer_setup" callback of the
 * USB Host or Device controller driver when setting up an USB
 * transfer. This function will setup correct packet sizes, buffer
 * sizes, flags and more, that are stored in the "usb2_xfer"
 * structure.
 *------------------------------------------------------------------------*/
void
usb2_transfer_setup_sub(struct usb2_setup_params *parm)
{
	enum {
		REQ_SIZE = 8,
		MIN_PKT = 8,
	};
	struct usb2_xfer *xfer = parm->curr_xfer;
	const struct usb2_config_sub *setup_sub = parm->curr_setup_sub;
	struct usb2_endpoint_descriptor *edesc;
	struct usb2_std_packet_size std_size;
	uint32_t n_frlengths;
	uint32_t n_frbuffers;
	uint32_t x;
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
	edesc = xfer->pipe->edesc;

	type = (edesc->bmAttributes & UE_XFERTYPE);

	xfer->flags = setup_sub->flags;
	xfer->nframes = setup_sub->frames;
	xfer->timeout = setup_sub->timeout;
	xfer->callback = setup_sub->callback;
	xfer->interval = setup_sub->interval;
	xfer->endpoint = edesc->bEndpointAddress;
	xfer->max_packet_size = UGETW(edesc->wMaxPacketSize);
	xfer->max_packet_count = 1;
	/* make a shadow copy: */
	xfer->flags_int.usb2_mode = parm->udev->flags.usb2_mode;

	parm->bufsize = setup_sub->bufsize;

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

	std_size = usb2_std_packet_size[type][parm->speed];

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

	usb2_update_max_frame_size(xfer);

	/* check interrupt interval and transfer pre-delay */

	if (type == UE_ISOCHRONOUS) {

		uint32_t frame_limit;

		xfer->interval = 0;	/* not used, must be zero */
		xfer->flags_int.isochronous_xfr = 1;	/* set flag */

		if (xfer->timeout == 0) {
			/*
			 * set a default timeout in
			 * case something goes wrong!
			 */
			xfer->timeout = 1000 / 4;
		}
		if (parm->speed == USB_SPEED_HIGH) {
			frame_limit = USB_MAX_HS_ISOC_FRAMES_PER_XFER;
		} else {
			frame_limit = USB_MAX_FS_ISOC_FRAMES_PER_XFER;
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

				if (parm->speed == USB_SPEED_HIGH) {
					xfer->interval /= 8;	/* 125us -> 1ms */
				}
				if (xfer->interval == 0) {
					/*
					 * one millisecond is the smallest
					 * interval
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
			usb2_update_max_frame_size(xfer);

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

			usb2_set_frame_offset(xfer, 0, 0);

			if ((type == UE_CONTROL) && (n_frbuffers > 1)) {
				usb2_set_frame_offset(xfer, REQ_SIZE, 1);
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
	if (xfer->flags_int.bdma_enable) {
		/*
		 * Setup "dma_page_ptr".
		 *
		 * Proof for formula below:
		 *
		 * Assume there are three USB frames having length "a", "b" and
		 * "c". These USB frames will at maximum need "z"
		 * "usb2_page" structures. "z" is given by:
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
	if (zmps) {
		/* correct maximum data length */
		xfer->max_data_length = 0;
	}
	/* subtract USB frame remainder from "hc_max_frame_size" */

	xfer->max_usb2_frame_size =
	    (parm->hc_max_frame_size -
	    (parm->hc_max_frame_size % xfer->max_frame_size));

	if (xfer->max_usb2_frame_size == 0) {
		parm->err = USB_ERR_INVAL;
		goto done;
	}
	/* initialize max frame count */

	xfer->max_frame_count = xfer->nframes;

	/* initialize frame buffers */

	if (parm->buf) {
		for (x = 0; x != n_frbuffers; x++) {
			xfer->frbuffers[x].tag_parent =
			    &xfer->usb2_root->dma_parent_tag;

			if (xfer->flags_int.bdma_enable &&
			    (parm->bufsize_max > 0)) {

				if (usb2_pc_dmamap_create(
				    xfer->frbuffers + x,
				    parm->bufsize_max)) {
					parm->err = USB_ERR_NOMEM;
					goto done;
				}
			}
		}
	}
done:
	if (parm->err) {
		/*
		 * Set some dummy values so that we avoid division by zero:
		 */
		xfer->max_usb2_frame_size = 1;
		xfer->max_frame_size = 1;
		xfer->max_packet_size = 1;
		xfer->max_data_length = 0;
		xfer->nframes = 0;
		xfer->max_frame_count = 0;
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_setup - setup an array of USB transfers
 *
 * NOTE: You must always call "usb2_transfer_unsetup" after calling
 * "usb2_transfer_setup" if success was returned.
 *
 * The idea is that the USB device driver should pre-allocate all its
 * transfers by one call to this function.
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_transfer_setup(struct usb2_device *udev,
    const uint8_t *ifaces, struct usb2_xfer **ppxfer,
    const struct usb2_config *setup_start, uint16_t n_setup,
    void *priv_sc, struct mtx *priv_mtx)
{
	struct usb2_xfer dummy;
	struct usb2_setup_params parm;
	const struct usb2_config *setup_end = setup_start + n_setup;
	const struct usb2_config *setup;
	struct usb2_pipe *pipe;
	struct usb2_xfer_root *info;
	struct usb2_xfer *xfer;
	void *buf = NULL;
	uint16_t n;
	uint16_t refcount;

	parm.err = 0;
	refcount = 0;
	info = NULL;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "usb2_transfer_setup can sleep!");

	/* do some checking first */

	if (n_setup == 0) {
		DPRINTFN(6, "setup array has zero length!\n");
		return (USB_ERR_INVAL);
	}
	if (ifaces == 0) {
		DPRINTFN(6, "ifaces array is NULL!\n");
		return (USB_ERR_INVAL);
	}
	if (priv_mtx == NULL) {
		DPRINTFN(6, "using global lock\n");
		priv_mtx = &Giant;
	}
	/* sanity checks */
	for (setup = setup_start, n = 0;
	    setup != setup_end; setup++, n++) {
		if ((setup->mh.bufsize == 0xffffffff) ||
		    (setup->md.bufsize == 0xffffffff)) {
			parm.err = USB_ERR_BAD_BUFSIZE;
			DPRINTF("invalid bufsize\n");
		}
		if ((setup->mh.callback == NULL) &&
		    (setup->md.callback == NULL)) {
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
	parm.speed = usb2_get_speed(udev);
	parm.hc_max_packet_count = 1;

	if (parm.speed >= USB_SPEED_MAX) {
		parm.err = USB_ERR_INVAL;
		goto done;
	}
	/* setup all transfers */

	while (1) {

		if (buf) {
			/*
			 * Initialize the "usb2_xfer_root" structure,
			 * which is common for all our USB transfers.
			 */
			info = USB_ADD_BYTES(buf, 0);

			info->memory_base = buf;
			info->memory_size = parm.size[0];

			info->dma_page_cache_start = USB_ADD_BYTES(buf, parm.size[4]);
			info->dma_page_cache_end = USB_ADD_BYTES(buf, parm.size[5]);
			info->xfer_page_cache_start = USB_ADD_BYTES(buf, parm.size[5]);
			info->xfer_page_cache_end = USB_ADD_BYTES(buf, parm.size[2]);

			usb2_cv_init(&info->cv_drain, "WDRAIN");

			info->usb2_mtx = &udev->bus->mtx;
			info->priv_mtx = priv_mtx;

			usb2_dma_tag_setup(&info->dma_parent_tag,
			    parm.dma_tag_p, udev->bus->dma_parent_tag[0].tag,
			    priv_mtx, &usb2_bdma_done_event, info, 32, parm.dma_tag_max);

			info->bus = udev->bus;

			TAILQ_INIT(&info->done_q.head);
			info->done_q.command = &usb2_callback_wrapper;

			TAILQ_INIT(&info->dma_q.head);
			info->dma_q.command = &usb2_bdma_work_loop;

			info->done_m[0].hdr.pm_callback = &usb2_callback_proc;
			info->done_m[0].usb2_root = info;
			info->done_m[1].hdr.pm_callback = &usb2_callback_proc;
			info->done_m[1].usb2_root = info;

			/* create a callback thread */

			if (usb2_proc_setup(&info->done_p,
			    &udev->bus->mtx, USB_PRI_HIGH)) {
				parm.err = USB_ERR_NO_INTR_THREAD;
				goto done;
			}
		}
		/* reset sizes */

		parm.size[0] = 0;
		parm.buf = buf;
		parm.size[0] += sizeof(info[0]);

		for (setup = setup_start, n = 0;
		    setup != setup_end; setup++, n++) {

			/* select mode specific structure */
			if (udev->flags.usb2_mode == USB_MODE_HOST) {
				parm.curr_setup_sub = &setup->mh;
			} else {
				parm.curr_setup_sub = &setup->md;
			}
			/* skip USB transfers without callbacks: */
			if (parm.curr_setup_sub->callback == NULL) {
				continue;
			}
			/* see if there is a matching endpoint */
			pipe = usb2_get_pipe(udev,
			    ifaces[setup->if_index], setup);

			if (!pipe) {
				if (parm.curr_setup_sub->flags.no_pipe_ok) {
					continue;
				}
				parm.err = USB_ERR_NO_PIPE;
				goto done;
			}
			/* store current setup pointer */
			parm.curr_setup = setup;

			/* align data properly */
			parm.size[0] += ((-parm.size[0]) & (USB_HOST_ALIGN - 1));

			if (buf) {

				/*
				 * Common initialization of the
				 * "usb2_xfer" structure.
				 */
				xfer = USB_ADD_BYTES(buf, parm.size[0]);

				ppxfer[n] = xfer;
				xfer->udev = udev;
				xfer->address = udev->address;
				xfer->priv_sc = priv_sc;
				xfer->priv_mtx = priv_mtx;
				xfer->usb2_mtx = &udev->bus->mtx;
				xfer->usb2_root = info;
				info->setup_refcount++;

				usb2_callout_init_mtx(&xfer->timeout_handle, xfer->usb2_mtx,
				    CALLOUT_RETURNUNLOCKED);
			} else {
				/*
				 * Setup a dummy xfer, hence we are
				 * writing to the "usb2_xfer"
				 * structure pointed to by "xfer"
				 * before we have allocated any
				 * memory:
				 */
				xfer = &dummy;
				bzero(&dummy, sizeof(dummy));
				refcount++;
			}

			parm.size[0] += sizeof(xfer[0]);

			xfer->pipe = pipe;

			if (buf) {
				/*
				 * Increment the pipe refcount. This
				 * basically prevents setting a new
				 * configuration and alternate setting
				 * when USB transfers are in use on
				 * the given interface. Search the USB
				 * code for "pipe->refcount" if you
				 * want more information.
				 */
				xfer->pipe->refcount++;
			}
			parm.methods = xfer->pipe->methods;
			parm.curr_xfer = xfer;

			/*
			 * Call the Host or Device controller transfer setup
			 * routine:
			 */
			(udev->bus->methods->xfer_setup) (&parm);

			if (parm.err) {
				goto done;
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
			 * "usb2_transfer_unsetup_sub" will unlock
			 * "usb2_mtx" before returning !
			 */
			mtx_lock(info->usb2_mtx);

			/* something went wrong */
			usb2_transfer_unsetup_sub(info, 0);
		}
	}
	if (parm.err) {
		usb2_transfer_unsetup(ppxfer, n_setup);
	}
	return (parm.err);
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_unsetup_sub - factored out code
 *------------------------------------------------------------------------*/
static void
usb2_transfer_unsetup_sub(struct usb2_xfer_root *info, uint8_t needs_delay)
{
	struct usb2_page_cache *pc;
	uint32_t temp;

	mtx_assert(info->usb2_mtx, MA_OWNED);

	/* wait for any outstanding DMA operations */

	if (needs_delay) {
		temp = usb2_get_dma_delay(info->bus);
		usb2_pause_mtx(info->usb2_mtx, temp);
	}
	mtx_unlock(info->usb2_mtx);

	/* wait for interrupt thread to exit */
	usb2_proc_unsetup(&info->done_p);

	/* free DMA'able memory, if any */
	pc = info->dma_page_cache_start;
	while (pc != info->dma_page_cache_end) {
		usb2_pc_free_mem(pc);
		pc++;
	}

	/* free DMA maps in all "xfer->frbuffers" */
	pc = info->xfer_page_cache_start;
	while (pc != info->xfer_page_cache_end) {
		usb2_pc_dmamap_destroy(pc);
		pc++;
	}

	/* free all DMA tags */
	usb2_dma_tag_unsetup(&info->dma_parent_tag);

	usb2_cv_destroy(&info->cv_drain);

	/*
	 * free the "memory_base" last, hence the "info" structure is
	 * contained within the "memory_base"!
	 */
	free(info->memory_base, M_USB);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_unsetup - unsetup/free an array of USB transfers
 *
 * NOTE: All USB transfers in progress will get called back passing
 * the error code "USB_ERR_CANCELLED" before this function
 * returns.
 *------------------------------------------------------------------------*/
void
usb2_transfer_unsetup(struct usb2_xfer **pxfer, uint16_t n_setup)
{
	struct usb2_xfer *xfer;
	struct usb2_xfer_root *info;
	uint8_t needs_delay = 0;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "usb2_transfer_unsetup can sleep!");

	while (n_setup--) {
		xfer = pxfer[n_setup];

		if (xfer) {
			if (xfer->pipe) {
				mtx_lock(xfer->priv_mtx);
				mtx_lock(xfer->usb2_mtx);

				/*
				 * HINT: when you start/stop a transfer, it
				 * might be a good idea to directly use the
				 * "pxfer[]" structure:
				 *
				 * usb2_transfer_start(sc->pxfer[0]);
				 * usb2_transfer_stop(sc->pxfer[0]);
				 *
				 * That way, if your code has many parts that
				 * will not stop running under the same
				 * lock, in other words "priv_mtx", the
				 * usb2_transfer_start and
				 * usb2_transfer_stop functions will simply
				 * return when they detect a NULL pointer
				 * argument.
				 *
				 * To avoid any races we clear the "pxfer[]"
				 * pointer while holding the private mutex
				 * of the driver:
				 */
				pxfer[n_setup] = NULL;

				mtx_unlock(xfer->usb2_mtx);
				mtx_unlock(xfer->priv_mtx);

				usb2_transfer_drain(xfer);

				if (xfer->flags_int.bdma_enable) {
					needs_delay = 1;
				}
				/*
				 * NOTE: default pipe does not have an
				 * interface, even if pipe->iface_index == 0
				 */
				xfer->pipe->refcount--;

			} else {
				/* clear the transfer pointer */
				pxfer[n_setup] = NULL;
			}

			usb2_callout_drain(&xfer->timeout_handle);

			if (xfer->usb2_root) {
				info = xfer->usb2_root;

				mtx_lock(info->usb2_mtx);

				USB_ASSERT(info->setup_refcount != 0,
				    ("Invalid setup "
				    "reference count!\n"));

				info->setup_refcount--;

				if (info->setup_refcount == 0) {
					usb2_transfer_unsetup_sub(info,
					    needs_delay);
				} else {
					mtx_unlock(info->usb2_mtx);
				}
			}
		}
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_control_transfer_init - factored out code
 *
 * In USB Device Mode we have to wait for the SETUP packet which
 * containst the "struct usb2_device_request" structure, before we can
 * transfer any data. In USB Host Mode we already have the SETUP
 * packet at the moment the USB transfer is started. This leads us to
 * having to setup the USB transfer at two different places in
 * time. This function just contains factored out control transfer
 * initialisation code, so that we don't duplicate the code.
 *------------------------------------------------------------------------*/
static void
usb2_control_transfer_init(struct usb2_xfer *xfer)
{
	struct usb2_device_request req;

	/* copy out the USB request header */

	usb2_copy_out(xfer->frbuffers, 0, &req, sizeof(req));

	/* setup remainder */

	xfer->flags_int.control_rem = UGETW(req.wLength);

	/* copy direction to endpoint variable */

	xfer->endpoint &= ~(UE_DIR_IN | UE_DIR_OUT);
	xfer->endpoint |=
	    (req.bmRequestType & UT_READ) ? UE_DIR_IN : UE_DIR_OUT;

	return;
}

/*------------------------------------------------------------------------*
 *	usb2_start_hardware_sub
 *
 * This function handles initialisation of control transfers. Control
 * transfers are special in that regard that they can both transmit
 * and receive data.
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
usb2_start_hardware_sub(struct usb2_xfer *xfer)
{
	uint32_t len;

	/* Check for control endpoint stall */
	if (xfer->flags.stall_pipe) {
		/* no longer active */
		xfer->flags_int.control_act = 0;
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
			if (xfer->flags_int.usb2_mode == USB_MODE_DEVICE) {
				usb2_control_transfer_init(xfer);
			}
		}
		/* get data length */

		len = xfer->sumlen;

	} else {

		/* the size of the SETUP structure is hardcoded ! */

		if (xfer->frlengths[0] != sizeof(struct usb2_device_request)) {
			DPRINTFN(0, "Wrong framelength %u != %zu\n",
			    xfer->frlengths[0], sizeof(struct
			    usb2_device_request));
			goto error;
		}
		/* check USB mode */
		if (xfer->flags_int.usb2_mode == USB_MODE_DEVICE) {

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
			 * call to "usb2_control_transfer_init()" !
			 */
			xfer->flags_int.control_rem = 0xFFFF;
		} else {

			/* setup "endpoint" and "control_rem" */

			usb2_control_transfer_init(xfer);
		}

		/* set transfer-header flag */

		xfer->flags_int.control_hdr = 1;

		/* get data length */

		len = (xfer->sumlen - sizeof(struct usb2_device_request));
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
 *	usb2_start_hardware - start USB hardware for the given transfer
 *
 * This function should only be called from the USB callback.
 *------------------------------------------------------------------------*/
void
usb2_start_hardware(struct usb2_xfer *xfer)
{
	uint32_t x;

	DPRINTF("xfer=%p, pipe=%p, nframes=%d, dir=%s\n",
	    xfer, xfer->pipe, xfer->nframes, USB_GET_DATA_ISREAD(xfer) ?
	    "read" : "write");

#if USB_DEBUG
	if (USB_DEBUG_VAR > 0) {
		mtx_lock(xfer->usb2_mtx);

		usb2_dump_pipe(xfer->pipe);

		mtx_unlock(xfer->usb2_mtx);
	}
#endif

	mtx_assert(xfer->priv_mtx, MA_OWNED);
	mtx_assert(xfer->usb2_mtx, MA_NOTOWNED);

	/* Only open the USB transfer once! */
	if (!xfer->flags_int.open) {
		xfer->flags_int.open = 1;

		DPRINTF("open\n");

		mtx_lock(xfer->usb2_mtx);
		(xfer->pipe->methods->open) (xfer);
		mtx_unlock(xfer->usb2_mtx);
	}
	/* set "transferring" flag */
	xfer->flags_int.transferring = 1;

	/*
	 * Check if the transfer is waiting on a queue, most
	 * frequently the "done_q":
	 */
	if (xfer->wait_queue) {
		mtx_lock(xfer->usb2_mtx);
		usb2_transfer_dequeue(xfer);
		mtx_unlock(xfer->usb2_mtx);
	}
	/* clear "did_dma_delay" flag */
	xfer->flags_int.did_dma_delay = 0;

	/* clear "did_close" flag */
	xfer->flags_int.did_close = 0;

	/* clear "bdma_setup" flag */
	xfer->flags_int.bdma_setup = 0;

	/* by default we cannot cancel any USB transfer immediately */
	xfer->flags_int.can_cancel_immed = 0;

	/* clear lengths and frame counts by default */
	xfer->sumlen = 0;
	xfer->actlen = 0;
	xfer->aframes = 0;

	/* clear any previous errors */
	xfer->error = 0;

	/* sanity check */

	if (xfer->nframes == 0) {
		if (xfer->flags.stall_pipe) {
			/*
			 * Special case - want to stall without transferring
			 * any data:
			 */
			DPRINTF("xfer=%p nframes=0: stall "
			    "or clear stall!\n", xfer);
			mtx_lock(xfer->usb2_mtx);
			xfer->flags_int.can_cancel_immed = 1;
			/* start the transfer */
			usb2_command_wrapper(&xfer->pipe->pipe_q, xfer);
			mtx_unlock(xfer->usb2_mtx);
			return;
		}
		mtx_lock(xfer->usb2_mtx);
		usb2_transfer_done(xfer, USB_ERR_INVAL);
		mtx_unlock(xfer->usb2_mtx);
		return;
	}
	/* compute total transfer length */

	for (x = 0; x != xfer->nframes; x++) {
		xfer->sumlen += xfer->frlengths[x];
		if (xfer->sumlen < xfer->frlengths[x]) {
			/* length wrapped around */
			mtx_lock(xfer->usb2_mtx);
			usb2_transfer_done(xfer, USB_ERR_INVAL);
			mtx_unlock(xfer->usb2_mtx);
			return;
		}
	}

	/* clear some internal flags */

	xfer->flags_int.short_xfer_ok = 0;
	xfer->flags_int.short_frames_ok = 0;

	/* check if this is a control transfer */

	if (xfer->flags_int.control_xfr) {

		if (usb2_start_hardware_sub(xfer)) {
			mtx_lock(xfer->usb2_mtx);
			usb2_transfer_done(xfer, USB_ERR_STALLED);
			mtx_unlock(xfer->usb2_mtx);
			return;
		}
	}
	/*
	 * Setup filtered version of some transfer flags,
	 * in case of data read direction
	 */
	if (USB_GET_DATA_ISREAD(xfer)) {

		if (xfer->flags_int.control_xfr) {

			/*
			 * Control transfers do not support reception
			 * of multiple short USB frames !
			 */

			if (xfer->flags.short_xfer_ok) {
				xfer->flags_int.short_xfer_ok = 1;
			}
		} else {

			if (xfer->flags.short_frames_ok) {
				xfer->flags_int.short_xfer_ok = 1;
				xfer->flags_int.short_frames_ok = 1;
			} else if (xfer->flags.short_xfer_ok) {
				xfer->flags_int.short_xfer_ok = 1;
			}
		}
	}
	/*
	 * Check if BUS-DMA support is enabled and try to load virtual
	 * buffers into DMA, if any:
	 */
	if (xfer->flags_int.bdma_enable) {
		/* insert the USB transfer last in the BUS-DMA queue */
		usb2_command_wrapper(&xfer->usb2_root->dma_q, xfer);
		return;
	}
	/*
	 * Enter the USB transfer into the Host Controller or
	 * Device Controller schedule:
	 */
	usb2_pipe_enter(xfer);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_pipe_enter - factored out code
 *------------------------------------------------------------------------*/
void
usb2_pipe_enter(struct usb2_xfer *xfer)
{
	struct usb2_pipe *pipe;

	mtx_assert(xfer->priv_mtx, MA_OWNED);

	mtx_lock(xfer->usb2_mtx);

	pipe = xfer->pipe;

	DPRINTF("enter\n");

	/* enter the transfer */
	(pipe->methods->enter) (xfer);

	/* check cancelability */
	if (pipe->methods->enter_is_cancelable) {
		xfer->flags_int.can_cancel_immed = 1;
		/* check for transfer error */
		if (xfer->error) {
			/* some error has happened */
			usb2_transfer_done(xfer, 0);
			mtx_unlock(xfer->usb2_mtx);
			return;
		}
	} else {
		xfer->flags_int.can_cancel_immed = 0;
	}

	/* start the transfer */
	usb2_command_wrapper(&pipe->pipe_q, xfer);
	mtx_unlock(xfer->usb2_mtx);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_start - start an USB transfer
 *
 * NOTE: Calling this function more than one time will only
 *       result in a single transfer start, until the USB transfer
 *       completes.
 *------------------------------------------------------------------------*/
void
usb2_transfer_start(struct usb2_xfer *xfer)
{
	if (xfer == NULL) {
		/* transfer is gone */
		return;
	}
	mtx_assert(xfer->priv_mtx, MA_OWNED);

	/* mark the USB transfer started */

	if (!xfer->flags_int.started) {
		xfer->flags_int.started = 1;
	}
	/* check if the USB transfer callback is already transferring */

	if (xfer->flags_int.transferring) {
		return;
	}
	mtx_lock(xfer->usb2_mtx);
	/* call the USB transfer callback */
	usb2_callback_ss_done_defer(xfer);
	mtx_unlock(xfer->usb2_mtx);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_stop - stop an USB transfer
 *
 * NOTE: Calling this function more than one time will only
 *       result in a single transfer stop.
 * NOTE: When this function returns it is not safe to free nor
 *       reuse any DMA buffers. See "usb2_transfer_drain()".
 *------------------------------------------------------------------------*/
void
usb2_transfer_stop(struct usb2_xfer *xfer)
{
	struct usb2_pipe *pipe;

	if (xfer == NULL) {
		/* transfer is gone */
		return;
	}
	mtx_assert(xfer->priv_mtx, MA_OWNED);

	/* check if the USB transfer was ever opened */

	if (!xfer->flags_int.open) {
		/* nothing to do except clearing the "started" flag */
		xfer->flags_int.started = 0;
		return;
	}
	/* try to stop the current USB transfer */

	mtx_lock(xfer->usb2_mtx);
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
			(xfer->pipe->methods->close) (xfer);
			/* only close once */
			xfer->flags_int.did_close = 1;
		} else {
			/* need to wait for the next done callback */
		}
	} else {
		DPRINTF("close\n");

		/* close here and now */
		(xfer->pipe->methods->close) (xfer);

		/*
		 * Any additional DMA delay is done by
		 * "usb2_transfer_unsetup()".
		 */

		/*
		 * Special case. Check if we need to restart a blocked
		 * pipe.
		 */
		pipe = xfer->pipe;

		/*
		 * If the current USB transfer is completing we need
		 * to start the next one:
		 */
		if (pipe->pipe_q.curr == xfer) {
			usb2_command_wrapper(&pipe->pipe_q, NULL);
		}
	}

	mtx_unlock(xfer->usb2_mtx);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_pending
 *
 * This function will check if an USB transfer is pending which is a
 * little bit complicated!
 * Return values:
 * 0: Not pending
 * 1: Pending: The USB transfer will receive a callback in the future.
 *------------------------------------------------------------------------*/
uint8_t
usb2_transfer_pending(struct usb2_xfer *xfer)
{
	struct usb2_xfer_root *info;
	struct usb2_xfer_queue *pq;

	mtx_assert(xfer->priv_mtx, MA_OWNED);

	if (xfer->flags_int.transferring) {
		/* trivial case */
		return (1);
	}
	mtx_lock(xfer->usb2_mtx);
	if (xfer->wait_queue) {
		/* we are waiting on a queue somewhere */
		mtx_unlock(xfer->usb2_mtx);
		return (1);
	}
	info = xfer->usb2_root;
	pq = &info->done_q;

	if (pq->curr == xfer) {
		/* we are currently scheduled for callback */
		mtx_unlock(xfer->usb2_mtx);
		return (1);
	}
	/* we are not pending */
	mtx_unlock(xfer->usb2_mtx);
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_drain
 *
 * This function will stop the USB transfer and wait for any
 * additional BUS-DMA and HW-DMA operations to complete. Buffers that
 * are loaded into DMA can safely be freed or reused after that this
 * function has returned.
 *------------------------------------------------------------------------*/
void
usb2_transfer_drain(struct usb2_xfer *xfer)
{
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "usb2_transfer_drain can sleep!");

	if (xfer == NULL) {
		/* transfer is gone */
		return;
	}
	if (xfer->priv_mtx != &Giant) {
		mtx_assert(xfer->priv_mtx, MA_NOTOWNED);
	}
	mtx_lock(xfer->priv_mtx);

	usb2_transfer_stop(xfer);

	while (usb2_transfer_pending(xfer)) {
		xfer->flags_int.draining = 1;
		/*
		 * Wait until the current outstanding USB
		 * transfer is complete !
		 */
		usb2_cv_wait(&xfer->usb2_root->cv_drain, xfer->priv_mtx);
	}
	mtx_unlock(xfer->priv_mtx);

	return;
}

/*------------------------------------------------------------------------*
 *	usb2_set_frame_data
 *
 * This function sets the pointer of the buffer that should
 * loaded directly into DMA for the given USB frame. Passing "ptr"
 * equal to NULL while the corresponding "frlength" is greater
 * than zero gives undefined results!
 *------------------------------------------------------------------------*/
void
usb2_set_frame_data(struct usb2_xfer *xfer, void *ptr, uint32_t frindex)
{
	/* set virtual address to load and length */
	xfer->frbuffers[frindex].buffer = ptr;
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_set_frame_offset
 *
 * This function sets the frame data buffer offset relative to the beginning
 * of the USB DMA buffer allocated for this USB transfer.
 *------------------------------------------------------------------------*/
void
usb2_set_frame_offset(struct usb2_xfer *xfer, uint32_t offset,
    uint32_t frindex)
{
	USB_ASSERT(!xfer->flags.ext_buffer, ("Cannot offset data frame "
	    "when the USB buffer is external!\n"));

	/* set virtual address to load */
	xfer->frbuffers[frindex].buffer =
	    USB_ADD_BYTES(xfer->local_buffer, offset);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_callback_proc - factored out code
 *
 * This function performs USB callbacks.
 *------------------------------------------------------------------------*/
static void
usb2_callback_proc(struct usb2_proc_msg *_pm)
{
	struct usb2_done_msg *pm = (void *)_pm;
	struct usb2_xfer_root *info = pm->usb2_root;

	/* Change locking order */
	mtx_unlock(info->usb2_mtx);

	/*
	 * We exploit the fact that the mutex is the same for all
	 * callbacks that will be called from this thread:
	 */
	mtx_lock(info->priv_mtx);
	mtx_lock(info->usb2_mtx);

	/* Continue where we lost track */
	usb2_command_wrapper(&info->done_q,
	    info->done_q.curr);

	mtx_unlock(info->priv_mtx);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_callback_ss_done_defer
 *
 * This function will defer the start, stop and done callback to the
 * correct thread.
 *------------------------------------------------------------------------*/
static void
usb2_callback_ss_done_defer(struct usb2_xfer *xfer)
{
	struct usb2_xfer_root *info = xfer->usb2_root;
	struct usb2_xfer_queue *pq = &info->done_q;

	if (!mtx_owned(xfer->usb2_mtx)) {
		panic("%s: called unlocked!\n", __FUNCTION__);
	}
	if (pq->curr != xfer) {
		usb2_transfer_enqueue(pq, xfer);
	}
	if (!pq->recurse_1) {

		/*
	         * We have to postpone the callback due to the fact we
	         * will have a Lock Order Reversal, LOR, if we try to
	         * proceed !
	         */
		if (usb2_proc_msignal(&info->done_p,
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
 *	usb2_callback_wrapper
 *
 * This is a wrapper for USB callbacks. This wrapper does some
 * auto-magic things like figuring out if we can call the callback
 * directly from the current context or if we need to wakeup the
 * interrupt process.
 *------------------------------------------------------------------------*/
static void
usb2_callback_wrapper(struct usb2_xfer_queue *pq)
{
	struct usb2_xfer *xfer = pq->curr;
	struct usb2_xfer_root *info = xfer->usb2_root;

	if (!mtx_owned(xfer->usb2_mtx)) {
		panic("%s: called unlocked!\n", __FUNCTION__);
	}
	if (!mtx_owned(xfer->priv_mtx)) {
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
		if (usb2_proc_msignal(&info->done_p,
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

	mtx_unlock(xfer->usb2_mtx);
	mtx_assert(xfer->usb2_mtx, MA_NOTOWNED);

	/* set correct USB state for callback */
	if (!xfer->flags_int.transferring) {
		xfer->usb2_state = USB_ST_SETUP;
		if (!xfer->flags_int.started) {
			/* we got stopped before we even got started */
			mtx_lock(xfer->usb2_mtx);
			goto done;
		}
	} else {

		if (usb2_callback_wrapper_sub(xfer)) {
			/* the callback has been deferred */
			mtx_lock(xfer->usb2_mtx);
			goto done;
		}
		xfer->flags_int.transferring = 0;

		if (xfer->error) {
			xfer->usb2_state = USB_ST_ERROR;
		} else {
			/* set transferred state */
			xfer->usb2_state = USB_ST_TRANSFERRED;

			/* sync DMA memory, if any */
			if (xfer->flags_int.bdma_enable &&
			    (!xfer->flags_int.bdma_no_post_sync)) {
				usb2_bdma_post_sync(xfer);
			}
		}
	}

	/* call processing routine */
	(xfer->callback) (xfer);

	/* pickup the USB mutex again */
	mtx_lock(xfer->usb2_mtx);

	/*
	 * Check if we got started after that we got cancelled, but
	 * before we managed to do the callback. Check if we are
	 * draining.
	 */
	if ((!xfer->flags_int.open) &&
	    (xfer->flags_int.started) &&
	    (xfer->usb2_state == USB_ST_ERROR)) {
		/* try to loop, but not recursivly */
		usb2_command_wrapper(&info->done_q, xfer);
		return;
	} else if (xfer->flags_int.draining &&
	    (!xfer->flags_int.transferring)) {
		/* "usb2_transfer_drain()" is waiting for end of transfer */
		xfer->flags_int.draining = 0;
		usb2_cv_broadcast(&xfer->usb2_root->cv_drain);
	}
done:
	/* do the next callback, if any */
	usb2_command_wrapper(&info->done_q,
	    info->done_q.curr);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_dma_delay_done_cb
 *
 * This function is called when the DMA delay has been exectuded, and
 * will make sure that the callback is called to complete the USB
 * transfer. This code path is ususally only used when there is an USB
 * error like USB_ERR_CANCELLED.
 *------------------------------------------------------------------------*/
static void
usb2_dma_delay_done_cb(void *arg)
{
	struct usb2_xfer *xfer = arg;

	mtx_assert(xfer->usb2_mtx, MA_OWNED);

	DPRINTFN(3, "Completed %p\n", xfer);

	/* queue callback for execution, again */
	usb2_transfer_done(xfer, 0);

	mtx_unlock(xfer->usb2_mtx);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_dequeue
 *
 *  - This function is used to remove an USB transfer from a USB
 *  transfer queue.
 *
 *  - This function can be called multiple times in a row.
 *------------------------------------------------------------------------*/
void
usb2_transfer_dequeue(struct usb2_xfer *xfer)
{
	struct usb2_xfer_queue *pq;

	pq = xfer->wait_queue;
	if (pq) {
		TAILQ_REMOVE(&pq->head, xfer, wait_entry);
		xfer->wait_queue = NULL;
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_enqueue
 *
 *  - This function is used to insert an USB transfer into a USB *
 *  transfer queue.
 *
 *  - This function can be called multiple times in a row.
 *------------------------------------------------------------------------*/
void
usb2_transfer_enqueue(struct usb2_xfer_queue *pq, struct usb2_xfer *xfer)
{
	/*
	 * Insert the USB transfer into the queue, if it is not
	 * already on a USB transfer queue:
	 */
	if (xfer->wait_queue == NULL) {
		xfer->wait_queue = pq;
		TAILQ_INSERT_TAIL(&pq->head, xfer, wait_entry);
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_done
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
usb2_transfer_done(struct usb2_xfer *xfer, usb2_error_t error)
{
	struct usb2_xfer_queue *pq;

	mtx_assert(xfer->usb2_mtx, MA_OWNED);

	DPRINTF("err=%s\n", usb2_errstr(error));

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
	usb2_callout_stop(&xfer->timeout_handle);

	/*
	 * If we are waiting on a queue, just remove the USB transfer
	 * from the queue, if any. We should have the required locks
	 * locked to do the remove when this function is called.
	 */
	usb2_transfer_dequeue(xfer);

	if (mtx_owned(xfer->priv_mtx)) {
		/*
		 * If the private USB lock is not locked, then we assume
		 * that the BUS-DMA load stage has been passed:
		 */
		pq = &xfer->usb2_root->dma_q;

		if (pq->curr == xfer) {
			/* start the next BUS-DMA load, if any */
			usb2_command_wrapper(pq, NULL);
		}
	}
	/* keep some statistics */
	if (xfer->error) {
		xfer->udev->bus->stats_err.uds_requests
		    [xfer->pipe->edesc->bmAttributes & UE_XFERTYPE]++;
	} else {
		xfer->udev->bus->stats_ok.uds_requests
		    [xfer->pipe->edesc->bmAttributes & UE_XFERTYPE]++;
	}

	/* call the USB transfer callback */
	usb2_callback_ss_done_defer(xfer);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_start_cb
 *
 * This function is called to start the USB transfer when
 * "xfer->interval" is greater than zero, and and the endpoint type is
 * BULK or CONTROL.
 *------------------------------------------------------------------------*/
static void
usb2_transfer_start_cb(void *arg)
{
	struct usb2_xfer *xfer = arg;
	struct usb2_pipe *pipe = xfer->pipe;

	mtx_assert(xfer->usb2_mtx, MA_OWNED);

	DPRINTF("start\n");

	/* start the transfer */
	(pipe->methods->start) (xfer);

	/* check cancelability */
	if (pipe->methods->start_is_cancelable) {
		xfer->flags_int.can_cancel_immed = 1;
		if (xfer->error) {
			/* some error has happened */
			usb2_transfer_done(xfer, 0);
		}
	} else {
		xfer->flags_int.can_cancel_immed = 0;
	}
	mtx_unlock(xfer->usb2_mtx);

	return;
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_set_stall
 *
 * This function is used to set the stall flag outside the
 * callback. This function is NULL safe.
 *------------------------------------------------------------------------*/
void
usb2_transfer_set_stall(struct usb2_xfer *xfer)
{
	if (xfer == NULL) {
		/* tearing down */
		return;
	}
	mtx_assert(xfer->priv_mtx, MA_OWNED);

	/* avoid any races by locking the USB mutex */
	mtx_lock(xfer->usb2_mtx);

	xfer->flags.stall_pipe = 1;

	mtx_unlock(xfer->usb2_mtx);

	return;
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_clear_stall
 *
 * This function is used to clear the stall flag outside the
 * callback. This function is NULL safe.
 *------------------------------------------------------------------------*/
void
usb2_transfer_clear_stall(struct usb2_xfer *xfer)
{
	if (xfer == NULL) {
		/* tearing down */
		return;
	}
	mtx_assert(xfer->priv_mtx, MA_OWNED);

	/* avoid any races by locking the USB mutex */
	mtx_lock(xfer->usb2_mtx);

	xfer->flags.stall_pipe = 0;

	mtx_unlock(xfer->usb2_mtx);

	return;
}

/*------------------------------------------------------------------------*
 *	usb2_pipe_start
 *
 * This function is used to add an USB transfer to the pipe transfer list.
 *------------------------------------------------------------------------*/
void
usb2_pipe_start(struct usb2_xfer_queue *pq)
{
	struct usb2_pipe *pipe;
	struct usb2_xfer *xfer;
	uint8_t type;

	xfer = pq->curr;
	pipe = xfer->pipe;

	mtx_assert(xfer->usb2_mtx, MA_OWNED);

	/*
	 * If the pipe is already stalled we do nothing !
	 */
	if (pipe->is_stalled) {
		return;
	}
	/*
	 * Check if we are supposed to stall the pipe:
	 */
	if (xfer->flags.stall_pipe) {
		/* clear stall command */
		xfer->flags.stall_pipe = 0;

		/*
		 * Only stall BULK and INTERRUPT endpoints.
		 */
		type = (pipe->edesc->bmAttributes & UE_XFERTYPE);
		if ((type == UE_BULK) ||
		    (type == UE_INTERRUPT)) {
			struct usb2_device *udev;
			struct usb2_xfer_root *info;

			udev = xfer->udev;
			pipe->is_stalled = 1;

			if (udev->flags.usb2_mode == USB_MODE_DEVICE) {
				(udev->bus->methods->set_stall) (
				    udev, NULL, pipe);
			} else if (udev->default_xfer[1]) {
				info = udev->default_xfer[1]->usb2_root;
				if (usb2_proc_msignal(&info->done_p,
				    &udev->cs_msg[0], &udev->cs_msg[1])) {
					/* ignore */
				}
			} else {
				/* should not happen */
				DPRINTFN(0, "No stall handler!\n");
			}
			/*
			 * We get started again when the stall is cleared!
			 */
			return;
		}
	}
	/* Set or clear stall complete - special case */
	if (xfer->nframes == 0) {
		/* we are complete */
		xfer->aframes = 0;
		usb2_transfer_done(xfer, 0);
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
		type = (pipe->edesc->bmAttributes & UE_XFERTYPE);
		if ((type == UE_BULK) ||
		    (type == UE_CONTROL)) {
			usb2_transfer_timeout_ms(xfer,
			    &usb2_transfer_start_cb,
			    xfer->interval);
			return;
		}
	}
	DPRINTF("start\n");

	/* start USB transfer */
	(pipe->methods->start) (xfer);

	/* check cancelability */
	if (pipe->methods->start_is_cancelable) {
		xfer->flags_int.can_cancel_immed = 1;
		if (xfer->error) {
			/* some error has happened */
			usb2_transfer_done(xfer, 0);
		}
	} else {
		xfer->flags_int.can_cancel_immed = 0;
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_transfer_timeout_ms
 *
 * This function is used to setup a timeout on the given USB
 * transfer. If the timeout has been deferred the callback given by
 * "cb" will get called after "ms" milliseconds.
 *------------------------------------------------------------------------*/
void
usb2_transfer_timeout_ms(struct usb2_xfer *xfer,
    void (*cb) (void *arg), uint32_t ms)
{
	mtx_assert(xfer->usb2_mtx, MA_OWNED);

	/* defer delay */
	usb2_callout_reset(&xfer->timeout_handle,
	    USB_MS_TO_TICKS(ms), cb, xfer);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_callback_wrapper_sub
 *
 *  - This function will update variables in an USB transfer after
 *  that the USB transfer is complete.
 *
 *  - This function is used to start the next USB transfer on the
 *  pipe transfer queue, if any.
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
usb2_callback_wrapper_sub(struct usb2_xfer *xfer)
{
	struct usb2_pipe *pipe;
	uint32_t x;

	if ((!xfer->flags_int.open) &&
	    (!xfer->flags_int.did_close)) {
		DPRINTF("close\n");
		mtx_lock(xfer->usb2_mtx);
		(xfer->pipe->methods->close) (xfer);
		mtx_unlock(xfer->usb2_mtx);
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

		uint32_t temp;

		/* only delay once */
		xfer->flags_int.did_dma_delay = 1;

		/* we can not cancel this delay */
		xfer->flags_int.can_cancel_immed = 0;

		temp = usb2_get_dma_delay(xfer->udev->bus);

		DPRINTFN(3, "DMA delay, %u ms, "
		    "on %p\n", temp, xfer);

		if (temp != 0) {
			mtx_lock(xfer->usb2_mtx);
			usb2_transfer_timeout_ms(xfer,
			    &usb2_dma_delay_done_cb, temp);
			mtx_unlock(xfer->usb2_mtx);
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
		xfer->frlengths[x] = 0;
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
	DPRINTFN(6, "xfer=%p pipe=%p sts=%d alen=%d, slen=%d, afrm=%d, nfrm=%d\n",
	    xfer, xfer->pipe, xfer->error, xfer->actlen, xfer->sumlen,
	    xfer->aframes, xfer->nframes);

	if (xfer->error) {
		/* end of control transfer, if any */
		xfer->flags_int.control_act = 0;

		/* check if we should block the execution queue */
		if ((xfer->error != USB_ERR_CANCELLED) &&
		    (xfer->flags.pipe_bof)) {
			DPRINTFN(2, "xfer=%p: Block On Failure "
			    "on pipe=%p\n", xfer, xfer->pipe);
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
					    "Short Transfer on pipe %p.\n",
					    xfer, xfer->pipe);
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
				    "active on pipe=%p\n", xfer, xfer->pipe);
				goto done;
			}
		}
	}

	pipe = xfer->pipe;

	/*
	 * If the current USB transfer is completing we need to start the
	 * next one:
	 */
	mtx_lock(xfer->usb2_mtx);
	if (pipe->pipe_q.curr == xfer) {
		usb2_command_wrapper(&pipe->pipe_q, NULL);

		if (pipe->pipe_q.curr || TAILQ_FIRST(&pipe->pipe_q.head)) {
			/* there is another USB transfer waiting */
		} else {
			/* this is the last USB transfer */
			/* clear isochronous sync flag */
			xfer->pipe->is_synced = 0;
		}
	}
	mtx_unlock(xfer->usb2_mtx);
done:
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_command_wrapper
 *
 * This function is used to execute commands non-recursivly on an USB
 * transfer.
 *------------------------------------------------------------------------*/
void
usb2_command_wrapper(struct usb2_xfer_queue *pq, struct usb2_xfer *xfer)
{
	if (xfer) {
		/*
		 * If the transfer is not already processing,
		 * queue it!
		 */
		if (pq->curr != xfer) {
			usb2_transfer_enqueue(pq, xfer);
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
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_default_transfer_setup
 *
 * This function is used to setup the default USB control endpoint
 * transfer.
 *------------------------------------------------------------------------*/
void
usb2_default_transfer_setup(struct usb2_device *udev)
{
	struct usb2_xfer *xfer;
	uint8_t no_resetup;
	uint8_t iface_index;

repeat:

	xfer = udev->default_xfer[0];
	if (xfer) {
		mtx_lock(xfer->priv_mtx);
		no_resetup =
		    ((xfer->address == udev->address) &&
		    (udev->default_ep_desc.wMaxPacketSize[0] ==
		    udev->ddesc.bMaxPacketSize));
		if (udev->flags.usb2_mode == USB_MODE_DEVICE) {
			if (no_resetup) {
				/*
				 * NOTE: checking "xfer->address" and
				 * starting the USB transfer must be
				 * atomic!
				 */
				usb2_transfer_start(xfer);
			}
		}
		mtx_unlock(xfer->priv_mtx);
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
	usb2_transfer_unsetup(udev->default_xfer, USB_DEFAULT_XFER_MAX);

	/*
	 * Try to setup a new USB transfer for the
	 * default control endpoint:
	 */
	iface_index = 0;
	if (usb2_transfer_setup(udev, &iface_index,
	    udev->default_xfer, usb2_control_ep_cfg, USB_DEFAULT_XFER_MAX, NULL,
	    udev->default_mtx)) {
		DPRINTFN(0, "could not setup default "
		    "USB transfer!\n");
	} else {
		goto repeat;
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_clear_data_toggle - factored out code
 *
 * NOTE: the intention of this function is not to reset the hardware
 * data toggle.
 *------------------------------------------------------------------------*/
void
usb2_clear_data_toggle(struct usb2_device *udev, struct usb2_pipe *pipe)
{
	DPRINTFN(5, "udev=%p pipe=%p\n", udev, pipe);

	mtx_lock(&udev->bus->mtx);
	pipe->toggle_next = 0;
	mtx_unlock(&udev->bus->mtx);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_clear_stall_callback - factored out clear stall callback
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
 * static const struct usb2_config my_clearstall =  {
 *	.type = UE_CONTROL,
 *	.endpoint = 0,
 *	.direction = UE_DIR_ANY,
 *	.interval = 50, //50 milliseconds
 *	.bufsize = sizeof(struct usb2_device_request),
 *	.mh.timeout = 1000, //1.000 seconds
 *	.mh.flags = { },
 *	.mh.callback = &my_clear_stall_callback, // **
 * };
 *
 * ** "my_clear_stall_callback" calls "usb2_clear_stall_callback"
 * passing the correct parameters.
 *------------------------------------------------------------------------*/
uint8_t
usb2_clear_stall_callback(struct usb2_xfer *xfer1,
    struct usb2_xfer *xfer2)
{
	struct usb2_device_request req;

	if (xfer2 == NULL) {
		/* looks like we are tearing down */
		DPRINTF("NULL input parameter\n");
		return (0);
	}
	mtx_assert(xfer1->priv_mtx, MA_OWNED);
	mtx_assert(xfer2->priv_mtx, MA_OWNED);

	switch (USB_GET_STATE(xfer1)) {
	case USB_ST_SETUP:

		/*
		 * pre-clear the data toggle to DATA0 ("umass.c" and
		 * "ata-usb.c" depends on this)
		 */

		usb2_clear_data_toggle(xfer2->udev, xfer2->pipe);

		/* setup a clear-stall packet */

		req.bmRequestType = UT_WRITE_ENDPOINT;
		req.bRequest = UR_CLEAR_FEATURE;
		USETW(req.wValue, UF_ENDPOINT_HALT);
		req.wIndex[0] = xfer2->pipe->edesc->bEndpointAddress;
		req.wIndex[1] = 0;
		USETW(req.wLength, 0);

		/*
		 * "usb2_transfer_setup_sub()" will ensure that
		 * we have sufficient room in the buffer for
		 * the request structure!
		 */

		/* copy in the transfer */

		usb2_copy_in(xfer1->frbuffers, 0, &req, sizeof(req));

		/* set length */
		xfer1->frlengths[0] = sizeof(req);
		xfer1->nframes = 1;

		usb2_start_hardware(xfer1);
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

#if (USB_NO_POLL == 0)

/*------------------------------------------------------------------------*
 *	usb2_callout_poll
 *------------------------------------------------------------------------*/
static void
usb2_callout_poll(struct usb2_xfer *xfer)
{
	struct usb2_callout *co;
	void (*cb) (void *);
	void *arg;
	struct mtx *mtx;
	uint32_t delta;

	if (xfer == NULL) {
		return;
	}
	co = &xfer->timeout_handle;

#if __FreeBSD_version >= 800000
	mtx = (void *)(co->co.c_lock);
#else
	mtx = co->co.c_mtx;
#endif
	mtx_lock(mtx);

	if (usb2_callout_pending(co)) {
		delta = ticks - co->co.c_time;
		if (!(delta & 0x80000000)) {

			cb = co->co.c_func;
			arg = co->co.c_arg;

			/* timed out */
			usb2_callout_stop(co);

			(cb) (arg);

			/* the callback should drop the mutex */
		} else {
			mtx_unlock(mtx);
		}
	} else {
		mtx_unlock(mtx);
	}
	return;
}


/*------------------------------------------------------------------------*
 *	usb2_do_poll
 *
 * This function is called from keyboard driver when in polling
 * mode.
 *------------------------------------------------------------------------*/
void
usb2_do_poll(struct usb2_xfer **ppxfer, uint16_t max)
{
	struct usb2_xfer *xfer;
	struct usb2_xfer_root *usb2_root;
	struct usb2_device *udev;
	struct usb2_proc_msg *pm;
	uint32_t to;
	uint16_t n;

	/* compute system tick delay */
	to = ((uint32_t)(1000000)) / ((uint32_t)(hz));
	DELAY(to);
	atomic_add_int((volatile int *)&ticks, 1);

	for (n = 0; n != max; n++) {
		xfer = ppxfer[n];
		if (xfer) {
			usb2_root = xfer->usb2_root;
			udev = xfer->udev;

			/*
			 * Poll hardware - signal that we are polling by
			 * locking the private mutex:
			 */
			mtx_lock(xfer->priv_mtx);
			(udev->bus->methods->do_poll) (udev->bus);
			mtx_unlock(xfer->priv_mtx);

			/* poll clear stall start */
			mtx_lock(xfer->usb2_mtx);
			pm = &udev->cs_msg[0].hdr;
			(pm->pm_callback) (pm);
			mtx_unlock(xfer->usb2_mtx);

			if (udev->default_xfer[1]) {

				/* poll timeout */
				usb2_callout_poll(udev->default_xfer[1]);

				/* poll clear stall done thread */
				mtx_lock(xfer->usb2_mtx);
				pm = &udev->default_xfer[1]->
				    usb2_root->done_m[0].hdr;
				(pm->pm_callback) (pm);
				mtx_unlock(xfer->usb2_mtx);
			}
			/* poll timeout */
			usb2_callout_poll(xfer);

			/* poll done thread */
			mtx_lock(xfer->usb2_mtx);
			pm = &usb2_root->done_m[0].hdr;
			(pm->pm_callback) (pm);
			mtx_unlock(xfer->usb2_mtx);
		}
	}
	return;
}

#else

void
usb2_do_poll(struct usb2_xfer **ppxfer, uint16_t max)
{
	/* polling not supported */
	return;
}

#endif
