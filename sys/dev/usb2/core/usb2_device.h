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

#ifndef _USB2_DEVICE_H_
#define	_USB2_DEVICE_H_

struct usb2_symlink;

#define	USB_DEFAULT_XFER_MAX 2

struct usb2_clear_stall_msg {
	struct usb2_proc_msg hdr;
	struct usb2_device *udev;
};

/*
 * The following structure defines an USB pipe which is equal to an
 * USB endpoint.
 */
struct usb2_pipe {
	struct usb2_xfer_queue pipe_q;	/* queue of USB transfers */

	struct usb2_xfer *xfer_block;	/* blocking USB transfer */
	struct usb2_endpoint_descriptor *edesc;
	struct usb2_pipe_methods *methods;	/* set by HC driver */

	uint16_t isoc_next;
	uint16_t refcount;

	uint8_t	toggle_next:1;		/* next data toggle value */
	uint8_t	is_stalled:1;		/* set if pipe is stalled */
	uint8_t	is_synced:1;		/* set if we a synchronised */
	uint8_t	unused:5;
	uint8_t	iface_index;		/* not used by "default pipe" */
};

/*
 * The following structure defines an USB interface.
 */
struct usb2_interface {
	struct usb2_perm perm;		/* interface permissions */
	struct usb2_interface_descriptor *idesc;
	device_t subdev;
	uint8_t	alt_index;
	uint8_t	parent_iface_index;
};

/*
 * The following structure defines the USB device flags.
 */
struct usb2_device_flags {
	uint8_t	usb2_mode:1;		/* USB mode (see USB_MODE_XXX) */
	uint8_t	self_powered:1;		/* set if USB device is self powered */
	uint8_t	suspended:1;		/* set if USB device is suspended */
	uint8_t	no_strings:1;		/* set if USB device does not support
					 * strings */
	uint8_t	remote_wakeup:1;	/* set if remote wakeup is enabled */
	uint8_t	uq_bus_powered:1;	/* set if BUS powered quirk is present */
	uint8_t	uq_power_claim:1;	/* set if power claim quirk is present */
};

/*
 * The following structure defines an USB device. There exists one of
 * these structures for every USB device.
 */
struct usb2_device {
	struct usb2_clear_stall_msg cs_msg[2];	/* generic clear stall
						 * messages */
	struct usb2_perm perm;
	struct sx default_sx[2];
	struct mtx default_mtx[1];
	struct cv default_cv[2];
	struct usb2_interface ifaces[USB_IFACE_MAX];
	struct usb2_pipe default_pipe;	/* Control Endpoint 0 */
	struct usb2_pipe pipes[USB_EP_MAX];

	struct usb2_bus *bus;		/* our USB BUS */
	device_t parent_dev;		/* parent device */
	struct usb2_device *parent_hub;
	struct usb2_config_descriptor *cdesc;	/* full config descr */
	struct usb2_hub *hub;		/* only if this is a hub */
	struct usb_device *linux_dev;
	struct usb2_xfer *default_xfer[USB_DEFAULT_XFER_MAX];
	struct usb2_temp_data *usb2_template_ptr;
	struct usb2_pipe *pipe_curr;	/* current clear stall pipe */
	struct usb2_fifo *fifo[USB_FIFO_MAX];
	struct usb2_symlink *ugen_symlink;	/* our generic symlink */

	uint32_t plugtime;		/* copy of "ticks" */

	uint16_t refcount;
#define	USB_DEV_REF_MAX 0xffff

	uint16_t power;			/* mA the device uses */
	uint16_t langid;		/* language for strings */

	uint8_t	address;		/* device addess */
	uint8_t	device_index;		/* device index in "bus->devices" */
	uint8_t	curr_config_index;	/* current configuration index */
	uint8_t	curr_config_no;		/* current configuration number */
	uint8_t	depth;			/* distance from root HUB */
	uint8_t	speed;			/* low/full/high speed */
	uint8_t	port_index;		/* parent HUB port index */
	uint8_t	port_no;		/* parent HUB port number */
	uint8_t	hs_hub_addr;		/* high-speed HUB address */
	uint8_t	hs_port_no;		/* high-speed HUB port number */
	uint8_t	driver_added_refcount;	/* our driver added generation count */
	uint8_t	power_mode;		/* see USB_POWER_XXX */

	/* the "flags" field is write-protected by "bus->mtx" */

	struct usb2_device_flags flags;

	struct usb2_endpoint_descriptor default_ep_desc;	/* for pipe 0 */
	struct usb2_device_descriptor ddesc;	/* device descriptor */

	char	serial[64];		/* serial number */
	char	manufacturer[64];	/* manufacturer string */
	char	product[64];		/* product string */
};

/* function prototypes */

struct usb2_device *usb2_alloc_device(device_t parent_dev, struct usb2_bus *bus, struct usb2_device *parent_hub, uint8_t depth, uint8_t port_index, uint8_t port_no, uint8_t speed, uint8_t usb2_mode);
struct usb2_pipe *usb2_get_pipe(struct usb2_device *udev, uint8_t iface_index, const struct usb2_config *setup);
struct usb2_pipe *usb2_get_pipe_by_addr(struct usb2_device *udev, uint8_t ea_val);
usb2_error_t usb2_interface_count(struct usb2_device *udev, uint8_t *count);
usb2_error_t usb2_probe_and_attach(struct usb2_device *udev, uint8_t iface_index);
usb2_error_t usb2_reset_iface_endpoints(struct usb2_device *udev, uint8_t iface_index);
usb2_error_t usb2_set_config_index(struct usb2_device *udev, uint8_t index);
usb2_error_t usb2_set_endpoint_stall(struct usb2_device *udev, struct usb2_pipe *pipe, uint8_t do_stall);
usb2_error_t usb2_suspend_resume(struct usb2_device *udev, uint8_t do_suspend);
void	usb2_detach_device(struct usb2_device *udev, uint8_t iface_index, uint8_t free_subdev);
void	usb2_devinfo(struct usb2_device *udev, char *dst_ptr, uint16_t dst_len);
void	usb2_free_device(struct usb2_device *udev);
void   *usb2_find_descriptor(struct usb2_device *udev, void *id, uint8_t iface_index, uint8_t type, uint8_t type_mask, uint8_t subtype, uint8_t subtype_mask);
void	usb_linux_free_device(struct usb_device *dev);

#endif					/* _USB2_DEVICE_H_ */
