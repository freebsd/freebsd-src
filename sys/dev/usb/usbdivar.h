/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson <augustss@carlstedt.se>
 *         Carlstedt Research & Technology
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

#if defined(__FreeBSD__)
/* conversiom from one type of queue to the other */
#define SIMPLEQ_REMOVE_HEAD	STAILQ_REMOVE_HEAD_QUEUE
#define SIMPLEQ_INSERT_HEAD	STAILQ_INSERT_HEAD
#define SIMPLEQ_INSERT_TAIL	STAILQ_INSERT_TAIL
#define SIMPLEQ_NEXT		STAILQ_NEXT
#define SIMPLEQ_FIRST		STAILQ_FIRST
#define SIMPLEQ_HEAD		STAILQ_HEAD
#define SIMPLEQ_INIT		STAILQ_INIT
#define SIMPLEQ_ENTRY		STAILQ_ENTRY
#endif

struct usbd_request;
struct usbd_pipe;

struct usbd_endpoint {
	usb_endpoint_descriptor_t *edesc;
	usbd_endpoint_state	state;
	int			refcnt;
	int			toggle;	/* XXX */
};

typedef void (*usbd_xfercb)__P((usbd_request_handle req));

struct usbd_methods {
	usbd_status	      (*transfer)__P((usbd_request_handle reqh));
	void		      (*abort)__P((usbd_request_handle reqh));
	void		      (*close)__P((usbd_pipe_handle pipe));	
	usbd_status	      (*isobuf)__P((usbd_pipe_handle pipe,
					    u_int32_t bufsize,u_int32_t nbuf));
};

struct usbd_port {
	usb_port_status_t	status;
	int 			power;	/* mA of current on port */
	struct usbd_device     *device;
	struct usbd_device     *parent;	/* The ports hub */
};

struct usbd_hub {
	usbd_status	      (*explore)__P((usbd_device_handle hub));
	void		       *hubdata;
	usb_hub_descriptor_t	hubdesc;
	int			nports;
	struct usbd_port        ports[1];
};

struct usb_softc;

/*****/

struct usbd_bus {
	/* Filled by HC driver */
	bdevice			bdev; /* base device, host adapter */
	usbd_status	      (*open_pipe)__P((struct usbd_pipe *pipe));
	u_int32_t		pipe_size; /* size of a pipe struct */
	void		      (*do_poll)__P((struct usbd_bus *));
	/* Filled by usb driver */
	struct usbd_device     *root_hub;
	usbd_device_handle	devices[USB_MAX_DEVICES];
	char			needs_explore;/* a hub a signalled a change */
	char			use_polling;
	struct usb_softc       *usbctl;
	struct usb_device_stats	stats;
};

struct usbd_device {
	struct usbd_bus	       *bus;
	usbd_device_state	state;
	struct usbd_pipe       *default_pipe;
	u_int8_t		address;
	u_int8_t		depth;
	u_int8_t		lowspeed;
	u_int16_t		power;
	u_int8_t		self_powered;
	int			config;
	struct usbd_port       *powersrc;
	struct usbd_endpoint	def_ep;	/* for pipe 0 */
	usb_endpoint_descriptor_t def_ep_desc; /* for pipe 0 */
	struct usbd_interface  *ifaces;
	usb_device_descriptor_t ddesc;
	usb_config_descriptor_t *cdesc;	/* full config descr */
	struct usbd_quirks     *quirks;
	struct usbd_hub	       *hub; /* only if this is a hub */
#if defined(__FreeBSD__)
	bdevice			bdev;	/* base device */
#endif
};

struct usbd_interface {
	struct usbd_device     *device;
	usbd_interface_state	state;
	usb_interface_descriptor_t *idesc;
	struct usbd_endpoint   *endpoints;
	void		       *priv;
	LIST_HEAD(, usbd_pipe)	pipes;
};

struct usbd_pipe {
	struct usbd_interface  *iface;
	struct usbd_device     *device;
	struct usbd_endpoint   *endpoint;
	usbd_pipe_state		state;
	int32_t			refcnt;
	char			running;
	SIMPLEQ_HEAD(, usbd_request) queue;
	LIST_ENTRY(usbd_pipe)	next;

	void		      (*disco) __P((void *));
	void		       *discoarg;

	usbd_request_handle     intrreqh; /* used for repeating requests */
	usbd_request_handle     curreqh; /* currently running request */

	/* Filled by HC driver. */
	struct usbd_methods    *methods;
};

struct usbd_request {
	struct usbd_pipe       *pipe;
	void		       *priv;
	void		       *buffer;
	u_int32_t		length;
	u_int32_t		actlen;
	u_int16_t		flags;
	u_int32_t		timeout;
	usbd_status		status;
	usbd_callback		callback;
	usbd_xfercb		xfercb;
	u_int32_t		retries;
	char			done;

	usb_device_request_t	request;
	u_int8_t		isreq;

	SIMPLEQ_ENTRY(usbd_request) next;

	void		       *hcpriv; /* XXX private use by the HC driver */

#if defined(__FreeBSD__)
	struct callout_handle  timo_handle;
#endif
};

void usbd_init __P((void));

/* Routines from usb_subr.c */
int		usbctlprint __P((void *, const char *));
void		usbd_delay_ms __P((usbd_bus_handle, int));
void		usbd_devinfo_vp __P((usbd_device_handle, char *, char *));
usbd_status	usbd_set_config_no __P((usbd_device_handle, int, int));
usbd_status	usbd_reset_port __P((usbd_device_handle dev,
				     int port, usb_port_status_t *ps));
usbd_status	usbd_setup_pipe __P((usbd_device_handle dev,
				     usbd_interface_handle iface,
				     struct usbd_endpoint *,
				     usbd_pipe_handle *pipe));
usbd_status	usbd_new_device __P((bdevice *parent, 
				     usbd_bus_handle bus, int depth,
				     int lowspeed, int port, 
				     struct usbd_port *));
void		usbd_remove_device __P((usbd_device_handle,
					struct usbd_port *));
int		usbd_printBCD __P((char *cp, int bcd));

/* Routines from usb.c */
int		usb_bus_count      __P((void));
usbd_status	usb_get_bus_handle __P((int, usbd_bus_handle *));
void		usb_needs_explore  __P((usbd_bus_handle));

#if defined(__FreeBSD__)
int 		usb_driver_load    __P((module_t mod, int what, void *arg));
void		usb_device_set_desc __P((device_t device, char *devinfo));
#endif

extern	int usbd_use_polling;

/* Locator stuff. */

#if defined(__NetBSD__)
/* NWH File not found anywhere in NetBSD sources... */
#include "locators.h"
#endif

#define	uhubcf_port	cf_loc[UHUBCF_PORT]
#define	UHUB_UNK_PORT	UHUBCF_PORT_DEFAULT /* wildcarded 'port' */

/* Junk. */

/* XXX */
#define splusb splbio
#define IPL_USB IPL_BIO
/* XXX */

