/* $FreeBSD$ */

/*-
 * Copyright (c) 2011 Hans Petter Selasky. All rights reserved.
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

#ifndef _BUS_AUTOCONF_H_
#define	_BUS_AUTOCONF_H_

/* Make sure we get the have compat linux definition. */
#include <dev/usb/usb.h>

struct usb_device_id {

	/* Hook for driver specific information */
	unsigned long driver_info;

	/* Used for product specific matches; the BCD range is inclusive */
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice_lo;
	uint16_t bcdDevice_hi;

	/* Used for device class matches */
	uint8_t	bDeviceClass;
	uint8_t	bDeviceSubClass;
	uint8_t	bDeviceProtocol;

	/* Used for interface class matches */
	uint8_t	bInterfaceClass;
	uint8_t	bInterfaceSubClass;
	uint8_t	bInterfaceProtocol;

	/* Select which fields to match against */
	uint8_t	match_flag_vendor:1;
	uint8_t	match_flag_product:1;
	uint8_t	match_flag_dev_lo:1;
	uint8_t	match_flag_dev_hi:1;
	uint8_t	match_flag_dev_class:1;
	uint8_t	match_flag_dev_subclass:1;
	uint8_t	match_flag_dev_protocol:1;
	uint8_t	match_flag_int_class:1;
	uint8_t	match_flag_int_subclass:1;
	uint8_t	match_flag_int_protocol:1;

#if USB_HAVE_COMPAT_LINUX
	/* which fields to match against */
	uint16_t match_flags;
#define	USB_DEVICE_ID_MATCH_VENDOR              0x0001
#define	USB_DEVICE_ID_MATCH_PRODUCT             0x0002
#define	USB_DEVICE_ID_MATCH_DEV_LO              0x0004
#define	USB_DEVICE_ID_MATCH_DEV_HI              0x0008
#define	USB_DEVICE_ID_MATCH_DEV_CLASS           0x0010
#define	USB_DEVICE_ID_MATCH_DEV_SUBCLASS        0x0020
#define	USB_DEVICE_ID_MATCH_DEV_PROTOCOL        0x0040
#define	USB_DEVICE_ID_MATCH_INT_CLASS           0x0080
#define	USB_DEVICE_ID_MATCH_INT_SUBCLASS        0x0100
#define	USB_DEVICE_ID_MATCH_INT_PROTOCOL        0x0200
#endif
};

#endif					/* _BUS_AUTOCONF_H_ */
