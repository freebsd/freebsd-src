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

#ifndef _USB2_SW_TRANSFER_H_
#define	_USB2_SW_TRANSFER_H_

/* Software transfer function state argument values */

enum {
	USB_SW_TR_SETUP,
	USB_SW_TR_STATUS,
	USB_SW_TR_PRE_DATA,
	USB_SW_TR_POST_DATA,
	USB_SW_TR_PRE_CALLBACK,
};

struct usb2_sw_transfer;

typedef void (usb2_sw_transfer_func_t)(struct usb2_xfer *, struct usb2_sw_transfer *);

/*
 * The following structure is used to keep the state of a standard
 * root transfer.
 */
struct usb2_sw_transfer {
	struct usb2_device_request req;
	struct usb2_xfer *xfer;
	uint8_t *ptr;
	uint16_t len;
	uint8_t	state;
	usb2_error_t err;
};

/* prototypes */

void	usb2_sw_transfer(struct usb2_sw_transfer *std, usb2_sw_transfer_func_t *func);

#endif					/* _USB2_SW_TRANSFER_H_ */
