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

#ifndef _BUS_USB_H_
#define	_BUS_USB_H_

struct usb_device_id {

	/* Internal fields */
	char	module_name[32];
	char	module_mode[32];
	uint8_t	is_iface;
	uint8_t	is_vp;
	uint8_t	is_dev;
	uint8_t	is_any;

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
};

void	usb_import_entries(const char *, const char *, const uint8_t *, uint32_t);
void	usb_dump_entries(void);

#endif					/* _BUS_USB_H_ */
