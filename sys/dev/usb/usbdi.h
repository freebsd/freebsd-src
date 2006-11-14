/*	$NetBSD: usbdi.h,v 1.64 2004/10/23 13:26:34 augustss Exp $	*/
/*	$FreeBSD$	*/

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

#ifndef _USBDI_H_
#define _USBDI_H_

typedef struct usbd_bus		*usbd_bus_handle;
typedef struct usbd_device	*usbd_device_handle;
typedef struct usbd_interface	*usbd_interface_handle;
typedef struct usbd_pipe	*usbd_pipe_handle;
typedef struct usbd_xfer	*usbd_xfer_handle;
typedef void			*usbd_private_handle;

typedef enum {		/* keep in sync with usbd_status_msgs */
	USBD_NORMAL_COMPLETION = 0, /* must be 0 */
	USBD_IN_PROGRESS,	/* 1 */
	/* errors */
	USBD_PENDING_REQUESTS,	/* 2 */
	USBD_NOT_STARTED,	/* 3 */
	USBD_INVAL,		/* 4 */
	USBD_NOMEM,		/* 5 */
	USBD_CANCELLED,		/* 6 */
	USBD_BAD_ADDRESS,	/* 7 */
	USBD_IN_USE,		/* 8 */
	USBD_NO_ADDR,		/* 9 */
	USBD_SET_ADDR_FAILED,	/* 10 */
	USBD_NO_POWER,		/* 11 */
	USBD_TOO_DEEP,		/* 12 */
	USBD_IOERROR,		/* 13 */
	USBD_NOT_CONFIGURED,	/* 14 */
	USBD_TIMEOUT,		/* 15 */
	USBD_SHORT_XFER,	/* 16 */
	USBD_STALLED,		/* 17 */
	USBD_INTERRUPTED,	/* 18 */

	USBD_ERROR_MAX		/* must be last */
} usbd_status;

typedef void (*usbd_callback)(usbd_xfer_handle, usbd_private_handle,
			      usbd_status);

/* Open flags */
#define USBD_EXCLUSIVE_USE	0x01

/* Use default (specified by ep. desc.) interval on interrupt pipe */
#define USBD_DEFAULT_INTERVAL	(-1)

/* Request flags */
#define USBD_NO_COPY		0x01	/* do not copy data to DMA buffer */
#define USBD_SYNCHRONOUS	0x02	/* wait for completion */
/* in usb.h #define USBD_SHORT_XFER_OK	0x04*/	/* allow short reads */
#define USBD_FORCE_SHORT_XFER	0x08	/* force last short packet on write */

#define USBD_NO_TIMEOUT 0
#define USBD_DEFAULT_TIMEOUT 5000 /* ms = 5 s */

usbd_status usbd_open_pipe(usbd_interface_handle, u_int8_t,
			   u_int8_t, usbd_pipe_handle *);
usbd_status usbd_close_pipe(usbd_pipe_handle);
usbd_status usbd_transfer(usbd_xfer_handle);
usbd_xfer_handle usbd_alloc_xfer(usbd_device_handle);
usbd_status usbd_free_xfer(usbd_xfer_handle);
void usbd_setup_xfer(usbd_xfer_handle, usbd_pipe_handle,
		     usbd_private_handle, void *,
		     u_int32_t, u_int16_t, u_int32_t,
		     usbd_callback);
void usbd_setup_default_xfer(usbd_xfer_handle, usbd_device_handle,
			     usbd_private_handle, u_int32_t,
			     usb_device_request_t *, void *,
			     u_int32_t, u_int16_t, usbd_callback);
void usbd_setup_isoc_xfer(usbd_xfer_handle, usbd_pipe_handle,
			  usbd_private_handle, u_int16_t *,
			  u_int32_t, u_int16_t, usbd_callback);
void usbd_get_xfer_status(usbd_xfer_handle, usbd_private_handle *,
			  void **, u_int32_t *, usbd_status *);
usb_endpoint_descriptor_t *usbd_interface2endpoint_descriptor
			(usbd_interface_handle, u_int8_t);
usbd_status usbd_abort_pipe(usbd_pipe_handle);
usbd_status usbd_abort_default_pipe(usbd_device_handle);
usbd_status usbd_clear_endpoint_stall(usbd_pipe_handle);
usbd_status usbd_clear_endpoint_stall_async(usbd_pipe_handle);
void usbd_clear_endpoint_toggle(usbd_pipe_handle);
usbd_status usbd_endpoint_count(usbd_interface_handle, u_int8_t *);
usbd_status usbd_interface_count(usbd_device_handle, u_int8_t *);
void usbd_interface2device_handle(usbd_interface_handle,
					 usbd_device_handle *);
usbd_status usbd_device2interface_handle(usbd_device_handle,
			      u_int8_t, usbd_interface_handle *);

usbd_device_handle usbd_pipe2device_handle(usbd_pipe_handle);
void *usbd_alloc_buffer(usbd_xfer_handle, u_int32_t);
void usbd_free_buffer(usbd_xfer_handle);
void *usbd_get_buffer(usbd_xfer_handle);
usbd_status usbd_sync_transfer(usbd_xfer_handle);
usbd_status usbd_open_pipe_intr(usbd_interface_handle, u_int8_t,
				u_int8_t, usbd_pipe_handle *,
				usbd_private_handle, void *,
				u_int32_t, usbd_callback, int);
usbd_status usbd_do_request(usbd_device_handle, usb_device_request_t *, void *);
usbd_status usbd_do_request_async(usbd_device_handle,
				  usb_device_request_t *, void *);
usbd_status usbd_do_request_flags(usbd_device_handle, usb_device_request_t *,
				  void *, u_int16_t, int*, u_int32_t);
usbd_status usbd_do_request_flags_pipe(usbd_device_handle, usbd_pipe_handle,
	usb_device_request_t *, void *, u_int16_t, int *, u_int32_t);
usb_interface_descriptor_t *usbd_get_interface_descriptor
				(usbd_interface_handle);
usb_config_descriptor_t *usbd_get_config_descriptor(usbd_device_handle);
usb_device_descriptor_t *usbd_get_device_descriptor(usbd_device_handle);
int usbd_get_speed(usbd_device_handle);
usbd_status usbd_set_interface(usbd_interface_handle, int);
int usbd_get_no_alts(usb_config_descriptor_t *, int);
usbd_status  usbd_get_interface(usbd_interface_handle, u_int8_t *);
void usbd_fill_deviceinfo(usbd_device_handle, struct usb_device_info *, int);
int usbd_get_interface_altindex(usbd_interface_handle);

usb_interface_descriptor_t *usbd_find_idesc(usb_config_descriptor_t *,
					    int, int);
usb_endpoint_descriptor_t *usbd_find_edesc(usb_config_descriptor_t *,
					   int, int, int);

void usbd_dopoll(usbd_interface_handle);
void usbd_set_polling(usbd_device_handle, int);

const char *usbd_errstr(usbd_status);

void usbd_add_dev_event(int, usbd_device_handle);
void usbd_add_drv_event(int, usbd_device_handle, device_ptr_t);

void usbd_devinfo(usbd_device_handle, int, char *);
const struct usbd_quirks *usbd_get_quirks(usbd_device_handle);
usb_endpoint_descriptor_t *usbd_get_endpoint_descriptor
			(usbd_interface_handle, u_int8_t);

usbd_status usbd_reload_device_desc(usbd_device_handle);

int usbd_ratecheck(struct timeval *last);

usbd_status usbd_get_string(usbd_device_handle dev, int si, char *buf);

/* An iterator for descriptors. */
typedef struct {
	const uByte *cur;
	const uByte *end;
} usbd_desc_iter_t;
void usb_desc_iter_init(usbd_device_handle dev, usbd_desc_iter_t *iter);
const usb_descriptor_t *usb_desc_iter_next(usbd_desc_iter_t *iter);

/*
 * The usb_task structs form a queue of things to run in the USB event
 * thread.  Normally this is just device discovery when a connect/disconnect
 * has been detected.  But it may also be used by drivers that need to
 * perform (short) tasks that must have a process context.
 */
struct usb_task {
	TAILQ_ENTRY(usb_task) next;
	void (*fun)(void *);
	void *arg;
	char onqueue;
};

void usb_add_task(usbd_device_handle, struct usb_task *);
void usb_rem_task(usbd_device_handle, struct usb_task *);
#define usb_init_task(t, f, a) ((t)->fun = (f), (t)->arg = (a), (t)->onqueue = 0)

struct usb_devno {
	u_int16_t ud_vendor;
	u_int16_t ud_product;
};
const struct usb_devno *usb_match_device(const struct usb_devno *,
	u_int, u_int, u_int16_t, u_int16_t);
#define usb_lookup(tbl, vendor, product) \
	usb_match_device((const struct usb_devno *)(tbl), sizeof (tbl) / sizeof ((tbl)[0]), sizeof ((tbl)[0]), (vendor), (product))
#define	USB_PRODUCT_ANY		0xffff

/* NetBSD attachment information */

/* Attach data */
struct usb_attach_arg {
	int			port;
	int			configno;
	int			ifaceno;
	int			vendor;
	int			product;
	int			release;
	int			matchlvl;
	usbd_device_handle	device;	/* current device */
	usbd_interface_handle	iface; /* current interface */
	int			usegeneric;
	usbd_interface_handle  *ifaces;	/* all interfaces */
	int			nifaces; /* number of interfaces */
};

#if defined(__NetBSD__) || defined(__OpenBSD__)
/* Match codes. */
/* First five codes is for a whole device. */
#define UMATCH_VENDOR_PRODUCT_REV			14
#define UMATCH_VENDOR_PRODUCT				13
#define UMATCH_VENDOR_DEVCLASS_DEVPROTO			12
#define UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO		11
#define UMATCH_DEVCLASS_DEVSUBCLASS			10
/* Next six codes are for interfaces. */
#define UMATCH_VENDOR_PRODUCT_REV_CONF_IFACE		 9
#define UMATCH_VENDOR_PRODUCT_CONF_IFACE		 8
#define UMATCH_VENDOR_IFACESUBCLASS_IFACEPROTO		 7
#define UMATCH_VENDOR_IFACESUBCLASS			 6
#define UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO	 5
#define UMATCH_IFACECLASS_IFACESUBCLASS			 4
#define UMATCH_IFACECLASS				 3
#define UMATCH_IFACECLASS_GENERIC			 2
/* Generic driver */
#define UMATCH_GENERIC					 1
/* No match */
#define UMATCH_NONE					 0

#elif defined(__FreeBSD__)
/* FreeBSD needs values less than zero */
#define UMATCH_VENDOR_PRODUCT_REV			(-10)
#define UMATCH_VENDOR_PRODUCT				(-20)
#define UMATCH_VENDOR_DEVCLASS_DEVPROTO			(-30)
#define UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO		(-40)
#define UMATCH_DEVCLASS_DEVSUBCLASS			(-50)
#define UMATCH_VENDOR_PRODUCT_REV_CONF_IFACE		(-60)
#define UMATCH_VENDOR_PRODUCT_CONF_IFACE		(-70)
#define UMATCH_VENDOR_IFACESUBCLASS_IFACEPROTO		(-80)
#define UMATCH_VENDOR_IFACESUBCLASS			(-90)
#define UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO	(-100)
#define UMATCH_IFACECLASS_IFACESUBCLASS			(-110)
#define UMATCH_IFACECLASS				(-120)
#define UMATCH_IFACECLASS_GENERIC			(-130)
#define UMATCH_GENERIC					(-140)
#define UMATCH_NONE					(ENXIO)

#endif

#define USBD_SHOW_DEVICE_CLASS		0x1
#define USBD_SHOW_INTERFACE_CLASS	0x2

#if defined(__FreeBSD__)
int usbd_driver_load(module_t mod, int what, void *arg);

static inline int
usb_get_port(device_t dev)
{
	struct usb_attach_arg *uap = device_get_ivars(dev);
	return (uap->port);
}

static inline struct usbd_interface *
usb_get_iface(device_t dev)
{
	struct usb_attach_arg *uap = device_get_ivars(dev);
	return (uap->iface);
}

#endif

/* XXX Perhaps USB should have its own levels? */
#ifdef USB_USE_SOFTINTR
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
#define splusb splsoftnet
#else
#define	splusb splsoftclock
#endif /* __HAVE_GENERIC_SOFT_INTERRUPTS */
#else
#define splusb splbio
#endif /* USB_USE_SOFTINTR */
#define splhardusb splbio
#define IPL_USB IPL_BIO

#endif /* _USBDI_H_ */
