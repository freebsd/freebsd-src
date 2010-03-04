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

/* This file contains various factored out debug macros. */

#ifndef _USB_DEBUG_H_
#define	_USB_DEBUG_H_

/* Declare global USB debug variable. */
extern int usb_debug;

/* Check if USB debugging is enabled. */
#ifdef USB_DEBUG_VAR
#if (USB_DEBUG != 0)
#define	DPRINTFN(n,fmt,...) do {		\
  if ((USB_DEBUG_VAR) >= (n)) {			\
    printf("%s: " fmt,				\
	   __FUNCTION__,## __VA_ARGS__);	\
  }						\
} while (0)
#define	DPRINTF(...)	DPRINTFN(1, __VA_ARGS__)
#else
#define	DPRINTF(...) do { } while (0)
#define	DPRINTFN(...) do { } while (0)
#endif
#endif

struct usb_interface;
struct usb_device;
struct usb_endpoint;
struct usb_xfer;

void	usb_dump_iface(struct usb_interface *iface);
void	usb_dump_device(struct usb_device *udev);
void	usb_dump_queue(struct usb_endpoint *ep);
void	usb_dump_endpoint(struct usb_endpoint *ep);
void	usb_dump_xfer(struct usb_xfer *xfer);

#endif					/* _USB_DEBUG_H_ */
