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

#ifndef _USB2_DEBUG_H_
#define	_USB2_DEBUG_H_

/* Declare parent SYSCTL USB node. */
SYSCTL_DECL(_hw_usb2);

/* Declare global USB debug variable. */
extern int usb2_debug;

/* Force debugging until further */
#ifndef USB_DEBUG
#define	USB_DEBUG 1
#endif

/* Check if USB debugging is enabled. */
#ifdef USB_DEBUG_VAR
#if (USB_DEBUG != 0)
#define	DPRINTFN(n,fmt,...) do {				\
  if ((USB_DEBUG_VAR) >= (n)) {				\
    printf("%s:%u: " fmt,				\
	   __FUNCTION__, __LINE__,## __VA_ARGS__);	\
  }							\
} while (0)
#define	DPRINTF(...)	DPRINTFN(1, __VA_ARGS__)
#else
#define	DPRINTF(...) do { } while (0)
#define	DPRINTFN(...) do { } while (0)
#endif
#endif

struct usb2_interface;
struct usb2_device;
struct usb2_pipe;
struct usb2_xfer;

void	usb2_dump_iface(struct usb2_interface *iface);
void	usb2_dump_device(struct usb2_device *udev);
void	usb2_dump_queue(struct usb2_pipe *pipe);
void	usb2_dump_pipe(struct usb2_pipe *pipe);
void	usb2_dump_xfer(struct usb2_xfer *xfer);

#endif					/* _USB2_DEBUG_H_ */
