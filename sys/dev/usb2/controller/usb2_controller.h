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

#ifndef _USB2_CONTROLLER_H_
#define	_USB2_CONTROLLER_H_

/* defines */

#define	USB_BUS_DMA_TAG_MAX 8

/* structure prototypes */

struct usb2_bus;
struct usb2_page;
struct usb2_pipe;
struct usb2_page_cache;
struct usb2_setup_params;
struct usb2_hw_ep_profile;
struct usb2_fs_isoc_schedule;
struct usb2_config_descriptor;
struct usb2_endpoint_descriptor;

/* typedefs */

typedef void (usb2_bus_mem_sub_cb_t)(struct usb2_bus *bus, struct usb2_page_cache *pc, struct usb2_page *pg, uint32_t size, uint32_t align);
typedef void (usb2_bus_mem_cb_t)(struct usb2_bus *bus, usb2_bus_mem_sub_cb_t *scb);

/*
 * The following structure is used to define all the USB BUS
 * callbacks.
 */
struct usb2_bus_methods {

	/* USB Device and Host mode - Mandatory */

	void    (*pipe_init) (struct usb2_device *udev, struct usb2_endpoint_descriptor *edesc, struct usb2_pipe *pipe);
	void    (*do_poll) (struct usb2_bus *);
	void    (*xfer_setup) (struct usb2_setup_params *parm);
	void    (*xfer_unsetup) (struct usb2_xfer *xfer);
	void    (*get_dma_delay) (struct usb2_bus *, uint32_t *pdelay);

	/* USB Device mode only - Mandatory */

	void    (*get_hw_ep_profile) (struct usb2_device *udev, const struct usb2_hw_ep_profile **ppf, uint8_t ep_addr);
	void    (*set_stall) (struct usb2_device *udev, struct usb2_xfer *xfer, struct usb2_pipe *pipe);
	void    (*clear_stall) (struct usb2_device *udev, struct usb2_pipe *pipe);
	void    (*rem_wakeup_set) (struct usb2_device *udev, uint8_t is_on);

	/* USB Device mode only - Optional */

	void    (*vbus_interrupt) (struct usb2_bus *, uint8_t is_on);
};

/*
 * The following structure is used to define all the USB pipe
 * callbacks.
 */
struct usb2_pipe_methods {

	/* Mandatory USB Device and Host mode callbacks: */

	void    (*open) (struct usb2_xfer *xfer);
	void    (*close) (struct usb2_xfer *xfer);

	void    (*enter) (struct usb2_xfer *xfer);
	void    (*start) (struct usb2_xfer *xfer);

	/* Optional */

	uint8_t (*isdone) (struct usb2_xfer *xfer);
	void   *info;

	/* Flags */

	uint8_t	enter_is_cancelable:1;
	uint8_t	start_is_cancelable:1;
};

/*
 * The following structure keeps information about what a hardware USB
 * endpoint supports.
 */
struct usb2_hw_ep_profile {
	uint16_t max_in_frame_size;	/* IN-token direction */
	uint16_t max_out_frame_size;	/* OUT-token direction */
	uint8_t	is_simplex:1;
	uint8_t	support_multi_buffer:1;
	uint8_t	support_bulk:1;
	uint8_t	support_control:1;
	uint8_t	support_interrupt:1;
	uint8_t	support_isochronous:1;
	uint8_t	support_in:1;		/* IN-token is supported */
	uint8_t	support_out:1;		/* OUT-token is supported */
};

/*
 * The following structure is used when trying to allocate hardware
 * endpoints for an USB configuration in USB device side mode.
 */
struct usb2_hw_ep_scratch_sub {
	const struct usb2_hw_ep_profile *pf;
	uint16_t max_frame_size;
	uint8_t	hw_endpoint_out;
	uint8_t	hw_endpoint_in;
	uint8_t	needs_ep_type;
	uint8_t	needs_in:1;
	uint8_t	needs_out:1;
};

/*
 * The following structure is used when trying to allocate hardware
 * endpoints for an USB configuration in USB device side mode.
 */
struct usb2_hw_ep_scratch {
	struct usb2_hw_ep_scratch_sub ep[USB_EP_MAX];
	struct usb2_hw_ep_scratch_sub *ep_max;
	struct usb2_config_descriptor *cd;
	struct usb2_device *udev;
	struct usb2_bus_methods *methods;
	uint8_t	bmOutAlloc[(USB_EP_MAX + 15) / 16];
	uint8_t	bmInAlloc[(USB_EP_MAX + 15) / 16];
};

/*
 * The following structure is used when generating USB descriptors
 * from USB templates.
 */
struct usb2_temp_setup {
	void   *buf;
	uint32_t size;
	uint8_t	usb2_speed;
	uint8_t	self_powered;
	uint8_t	bNumEndpoints;
	uint8_t	bInterfaceNumber;
	uint8_t	bAlternateSetting;
	uint8_t	bConfigurationValue;
	usb2_error_t err;
};

/* prototypes */

void	usb2_bus_mem_flush_all(struct usb2_bus *bus, usb2_bus_mem_cb_t *cb);
uint8_t	usb2_bus_mem_alloc_all(struct usb2_bus *bus, bus_dma_tag_t dmat, usb2_bus_mem_cb_t *cb);
void	usb2_bus_mem_free_all(struct usb2_bus *bus, usb2_bus_mem_cb_t *cb);
uint16_t usb2_isoc_time_expand(struct usb2_bus *bus, uint16_t isoc_time_curr);
uint16_t usb2_fs_isoc_schedule_isoc_time_expand(struct usb2_device *udev, struct usb2_fs_isoc_schedule **pp_start, struct usb2_fs_isoc_schedule **pp_end, uint16_t isoc_time);
uint8_t	usb2_fs_isoc_schedule_alloc(struct usb2_fs_isoc_schedule *fss, uint8_t *pstart, uint16_t len);

#endif					/* _USB2_CONTROLLER_H_ */
