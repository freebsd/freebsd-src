/* $FreeBSD$ */
/*-
 * Copyright (c) 2009 Sylvestre Gallon. All rights reserved.
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

#ifndef __LIBUSB10_H__
#define __LIBUSB10_H__

/*
 * The two following macros were taken from the original LibUSB v1.0
 * for sake of compatibility:
 */

static int get_next_timeout(libusb_context *ctx, struct timeval *tv, struct timeval *out);
static int handle_timeouts(struct libusb_context *ctx);
static int handle_events(struct libusb_context *ctx, struct timeval *tv);
extern struct libusb_context *usbi_default_context;
extern pthread_mutex_t libusb20_lock; 

/* if ctx is NULL use default context*/

#define GET_CONTEXT(ctx) \
	if (ctx == NULL) ctx = usbi_default_context;

#define MAX(a,b) (((a)>(b))?(a):(b))
#define USB_TIMED_OUT (1<<0)
#define UNEXPORTED __attribute__((__visibility__("hidden")))

#define DPRINTF(ctx, dbg, format, args...)	\
if (ctx->debug == dbg) {			\
	printf("LIBUSB_%s : ", (ctx->debug == LIBUSB_DEBUG_FUNCTION) ? "FUNCTION" : "TRANSFER");	\
	switch(ctx->debug) {			\
		case LIBUSB_DEBUG_FUNCTION:	\
			printf(format, ## args);\
			break ;			\
		case LIBUSB_DEBUG_TRANSFER:	\
			printf(format, ## args);\
			break ;			\
	}					\
	printf("\n");				\
}

UNEXPORTED int usb_add_pollfd(libusb_context *ctx, int fd, short events);
UNEXPORTED void usb_remove_pollfd(libusb_context *ctx, int fd);
UNEXPORTED void usb_handle_transfer_completion(struct usb_transfer *uxfer, 
    enum libusb_transfer_status status);
UNEXPORTED void usb_handle_disconnect(struct libusb_device_handle *devh);

#endif /*__LIBUSB10_H__*/
