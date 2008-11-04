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

#ifndef _USB2_LOOKUP_H_
#define	_USB2_LOOKUP_H_

struct usb2_attach_arg;

/*
 * The following structure is used when looking up an USB driver for
 * an USB device. It is inspired by the Linux structure called
 * "usb2_device_id".
 */
struct usb2_device_id {

	/* Hook for driver specific information */
	const void *driver_info;

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

#define	USB_VENDOR(vend)			\
  .match_flag_vendor = 1, .idVendor = (vend)

#define	USB_PRODUCT(prod)			\
  .match_flag_product = 1, .idProduct = (prod)

#define	USB_VP(vend,prod)			\
  USB_VENDOR(vend), USB_PRODUCT(prod)

#define	USB_VPI(vend,prod,info)			\
  USB_VENDOR(vend), USB_PRODUCT(prod), USB_DRIVER_INFO(info)

#define	USB_DEV_BCD_GTEQ(lo)	/* greater than or equal */ \
  .match_flag_dev_lo = 1, .bcdDevice_lo = (lo)

#define	USB_DEV_BCD_LTEQ(hi)	/* less than or equal */ \
  .match_flag_dev_hi = 1, .bcdDevice_hi = (hi)

#define	USB_DEV_CLASS(dc)			\
  .match_flag_dev_class = 1, .bDeviceClass = (dc)

#define	USB_DEV_SUBCLASS(dsc)			\
  .match_flag_dev_subclass = 1, .bDeviceSubClass = (dsc)

#define	USB_DEV_PROTOCOL(dp)			\
  .match_flag_dev_protocol = 1, .bDeviceProtocol = (dp)

#define	USB_IFACE_CLASS(ic)			\
  .match_flag_int_class = 1, .bInterfaceClass = (ic)

#define	USB_IFACE_SUBCLASS(isc)			\
  .match_flag_int_subclass = 1, .bInterfaceSubClass = (isc)

#define	USB_IFACE_PROTOCOL(ip)			\
  .match_flag_int_protocol = 1, .bInterfaceProtocol = (ip)

#define	USB_IF_CSI(class,subclass,info)			\
  USB_IFACE_CLASS(class), USB_IFACE_SUBCLASS(subclass), USB_DRIVER_INFO(info)

#define	USB_DRIVER_INFO(ptr)			\
  .driver_info = ((const void *)(ptr))

#define	USB_GET_DRIVER_INFO(did)		\
  (((const uint8_t *)((did)->driver_info)) - ((const uint8_t *)0))

const struct usb2_device_id *usb2_lookup_id_by_info(const struct usb2_device_id *id, uint32_t sizeof_id, const struct usb2_lookup_info *info);
int	usb2_lookup_id_by_uaa(const struct usb2_device_id *id, uint32_t sizeof_id, struct usb2_attach_arg *uaa);

#endif					/* _USB2_LOOKUP_H_ */
