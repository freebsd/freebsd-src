/*	$NetBSD: usbdi.h,v 1.28 1999/09/11 08:19:27 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
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

typedef struct usbd_bus		*usbd_bus_handle;
typedef struct usbd_device	*usbd_device_handle;
typedef struct usbd_interface	*usbd_interface_handle;
typedef struct usbd_pipe	*usbd_pipe_handle;
typedef struct usbd_request	*usbd_request_handle;
typedef void			*usbd_private_handle;

typedef enum { 				/* keep in sync with usbd_status_msgs */
	USBD_NORMAL_COMPLETION = 0,	/* must be 0 */
	USBD_IN_PROGRESS,
	/* errors */
	USBD_PENDING_REQUESTS,
	USBD_NOT_STARTED,
	USBD_INVAL,
	USBD_NOMEM,
	USBD_CANCELLED,
	USBD_BAD_ADDRESS,
	USBD_IN_USE,
	USBD_NO_ADDR,
	USBD_SET_ADDR_FAILED,
	USBD_NO_POWER,
	USBD_TOO_DEEP,
	USBD_IOERROR,
	USBD_NOT_CONFIGURED,
	USBD_TIMEOUT,
	USBD_SHORT_XFER,
	USBD_STALLED,
	USBD_INTERRUPTED,

	USBD_ERROR_MAX,		/* must be last */
} usbd_status;

typedef int usbd_lock_token;

typedef void (*usbd_callback) __P((usbd_request_handle, usbd_private_handle,
				   usbd_status));

/* Open flags */
#define USBD_EXCLUSIVE_USE	0x01

/* Request flags */
/* in usb.h #define USBD_SHORT_XFER_OK	0x04*/	/* allow short reads */
#define USBD_SYNCHRONOUS	0x08	/* wait for completion */

#define USBD_NO_TIMEOUT 0
#define USBD_DEFAULT_TIMEOUT 5000 /* ms = 5 s */

#if defined(__FreeBSD__)
#define USB_CDEV_MAJOR 108
#endif

usbd_status usbd_open_pipe
	__P((usbd_interface_handle iface, u_int8_t address,
	     u_int8_t flags, usbd_pipe_handle *pipe));
usbd_status usbd_close_pipe	__P((usbd_pipe_handle pipe));
usbd_status usbd_transfer	__P((usbd_request_handle req));
usbd_request_handle usbd_alloc_request	__P((usbd_device_handle));
usbd_status usbd_free_request	__P((usbd_request_handle reqh));
void usbd_setup_request	
	__P((usbd_request_handle reqh, usbd_pipe_handle pipe,
	     usbd_private_handle priv, void *buffer,
	     u_int32_t length, u_int16_t flags, u_int32_t timeout,
	     usbd_callback));
void usbd_setup_default_request
	__P((usbd_request_handle reqh, usbd_device_handle dev,
	     usbd_private_handle priv, u_int32_t timeout,
	     usb_device_request_t *req,  void *buffer,
	     u_int32_t length, u_int16_t flags, usbd_callback));
void usbd_setup_isoc_request	
	__P((usbd_request_handle reqh, usbd_pipe_handle pipe,
	     usbd_private_handle priv, u_int16_t *frlengths,
	     u_int32_t nframes, usbd_callback));
void usbd_get_request_status
	__P((usbd_request_handle reqh, usbd_private_handle *priv,
	     void **buffer, u_int32_t *count, usbd_status *status));
usb_endpoint_descriptor_t *usbd_interface2endpoint_descriptor
	__P((usbd_interface_handle iface, u_int8_t address));
usbd_status usbd_abort_pipe __P((usbd_pipe_handle pipe));
usbd_status usbd_clear_endpoint_stall __P((usbd_pipe_handle pipe));
usbd_status usbd_clear_endpoint_stall_async __P((usbd_pipe_handle pipe));
usbd_status usbd_endpoint_count
	__P((usbd_interface_handle dev, u_int8_t *count));
usbd_status usbd_interface_count
	__P((usbd_device_handle dev, u_int8_t *count));
usbd_status usbd_interface2device_handle
	__P((usbd_interface_handle iface, usbd_device_handle *dev));
usbd_status usbd_device2interface_handle
	__P((usbd_device_handle dev, u_int8_t ifaceno, usbd_interface_handle *iface));

usbd_device_handle usbd_pipe2device_handle __P((usbd_pipe_handle));
void *usbd_alloc_buffer __P((usbd_request_handle req, u_int32_t size));
void usbd_free_buffer __P((usbd_request_handle req));
usbd_status usbd_sync_transfer	__P((usbd_request_handle req));
usbd_status usbd_open_pipe_intr
	__P((usbd_interface_handle iface, u_int8_t address,
	     u_int8_t flags, usbd_pipe_handle *pipe,
	     usbd_private_handle priv, void *buffer,
	     u_int32_t length, usbd_callback));
usbd_status usbd_do_request 
	__P((usbd_device_handle pipe, usb_device_request_t *req, void *data));
usbd_status usbd_do_request_async
	__P((usbd_device_handle pipe, usb_device_request_t *req, void *data));
usbd_status usbd_do_request_flags
	__P((usbd_device_handle pipe, usb_device_request_t *req, 
	     void *data, u_int16_t flags, int *));
usb_interface_descriptor_t *usbd_get_interface_descriptor
	__P((usbd_interface_handle iface));
usb_config_descriptor_t *usbd_get_config_descriptor
	__P((usbd_device_handle dev));
usb_device_descriptor_t *usbd_get_device_descriptor
	__P((usbd_device_handle dev));
usbd_status usbd_set_interface __P((usbd_interface_handle, int));
int usbd_get_no_alts __P((usb_config_descriptor_t *, int));
usbd_status	usbd_get_interface
	__P((usbd_interface_handle iface, u_int8_t *aiface));
void usbd_fill_deviceinfo 
	__P((usbd_device_handle dev, struct usb_device_info *di));
int usbd_get_interface_altindex __P((usbd_interface_handle iface));

usb_interface_descriptor_t *usbd_find_idesc
	__P((usb_config_descriptor_t *cd, int iindex, int ano));
usb_endpoint_descriptor_t *usbd_find_edesc
	__P((usb_config_descriptor_t *cd, int ifaceidx, int altidx, 
	     int endptidx));

void usbd_dopoll __P((usbd_interface_handle));
void usbd_set_polling __P((usbd_interface_handle iface, int on));

/* NetBSD attachment information */

/* Attach data */
struct usb_attach_arg {
	int			port;
	int			configno;
	int			ifaceno;
	int			vendor;
	int			product;
	int			release;
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
/* for the moment disabled
#define UMATCH_VENDOR_PRODUCT_REV			-14
#define UMATCH_VENDOR_PRODUCT				-13
#define UMATCH_VENDOR_DEVCLASS_DEVPROTO			-12
#define UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO		-11
#define UMATCH_DEVCLASS_DEVSUBCLASS			-10
#define UMATCH_VENDOR_PRODUCT_REV_CONF_IFACE		 -9
#define UMATCH_VENDOR_PRODUCT_CONF_IFACE		 -8
#define UMATCH_VENDOR_IFACESUBCLASS_IFACEPROTO		 -7
#define UMATCH_VENDOR_IFACESUBCLASS			 -6
#define UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO	 -5
#define UMATCH_IFACECLASS_IFACESUBCLASS			 -4
#define UMATCH_IFACECLASS				 -3
#define UMATCH_IFACECLASS_GENERIC			 -2
#define UMATCH_GENERIC					 -1
#define UMATCH_NONE				      ENXIO

* For the moment we use Yes/No answers with appropriate
* sorting in the config file
*/
#define UMATCH_VENDOR_PRODUCT_REV			0
#define UMATCH_VENDOR_PRODUCT				0
#define UMATCH_VENDOR_DEVCLASS_DEVPROTO			0
#define UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO		0
#define UMATCH_DEVCLASS_DEVSUBCLASS			0
#define UMATCH_VENDOR_PRODUCT_REV_CONF_IFACE		0
#define UMATCH_VENDOR_PRODUCT_CONF_IFACE		0
#define UMATCH_VENDOR_IFACESUBCLASS_IFACEPROTO		0
#define UMATCH_VENDOR_IFACESUBCLASS			0
#define UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO	0
#define UMATCH_IFACECLASS_IFACESUBCLASS			0
#define UMATCH_IFACECLASS				0
#define UMATCH_IFACECLASS_GENERIC			0
#define UMATCH_GENERIC					0
#define UMATCH_NONE				      ENXIO


#endif

void usbd_devinfo __P((usbd_device_handle, int, char *));
struct usbd_quirks *usbd_get_quirks __P((usbd_device_handle));
usb_endpoint_descriptor_t *usbd_get_endpoint_descriptor
	__P((usbd_interface_handle iface, u_int8_t address));
const char *usbd_errstr __P((usbd_status));


#if defined(__FreeBSD__)
int usbd_driver_load    __P((module_t mod, int what, void *arg));
#endif

/* XXX */
#define splusb splbio
#define IPL_USB IPL_BIO
/* XXX */

