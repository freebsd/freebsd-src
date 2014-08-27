/* $FreeBSD$ */
/*-
 * Copyright (c) 2014 Hans Petter Selasky. All rights reserved.
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

/*
 * This file contains the USB template for an USB phone device.
 */

#ifdef USB_GLOBAL_INCLUDE_FILE
#include USB_GLOBAL_INCLUDE_FILE
#else
#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_core.h>
#include <dev/usb/usb_cdc.h>

#include <dev/usb/template/usb_template.h>
#endif			/* USB_GLOBAL_INCLUDE_FILE */

enum {
	INDEX_PHONE_LANG,
	INDEX_PHONE_MIXER,
	INDEX_PHONE_RECORD,
	INDEX_PHONE_PLAYBACK,
	INDEX_PHONE_PRODUCT,
	INDEX_PHONE_HID,
	INDEX_PHONE_MAX,
};

#define	STRING_PHONE_PRODUCT \
  "U\0S\0B\0 \0P\0h\0o\0n\0e\0 \0D\0e\0v\0i\0c\0e"

#define	STRING_PHONE_MIXER \
  "M\0i\0x\0e\0r\0 \0i\0n\0t\0e\0r\0f\0a\0c\0e"

#define	STRING_PHONE_RECORD \
  "R\0e\0c\0o\0r\0d\0 \0i\0n\0t\0e\0r\0f\0a\0c\0e"

#define	STRING_PHONE_PLAYBACK \
  "P\0l\0a\0y\0b\0a\0c\0k\0 \0i\0n\0t\0e\0r\0f\0a\0c\0e"

#define	STRING_PHONE_HID \
  "H\0I\0D\0 \0i\0n\0t\0e\0r\0f\0a\0c\0e"

/* make the real string descriptors */

USB_MAKE_STRING_DESC(STRING_PHONE_MIXER, string_phone_mixer);
USB_MAKE_STRING_DESC(STRING_PHONE_RECORD, string_phone_record);
USB_MAKE_STRING_DESC(STRING_PHONE_PLAYBACK, string_phone_playback);
USB_MAKE_STRING_DESC(STRING_PHONE_PRODUCT, string_phone_product);
USB_MAKE_STRING_DESC(STRING_PHONE_HID, string_phone_hid);

/* prototypes */

/*
 * Phone Mixer description structures
 *
 * Some of the phone descriptors were dumped from no longer in
 * production Yealink VOIP USB phone adapter:
 */
static uint8_t phone_hid_descriptor[] = {
	0x05, 0x0b, 0x09, 0x01, 0xa1, 0x01, 0x05, 0x09,
	0x19, 0x01, 0x29, 0x3f, 0x15, 0x00, 0x25, 0x01,
	0x75, 0x01, 0x95, 0x80, 0x81, 0x00, 0x05, 0x08,
	0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01,
	0x75, 0x01, 0x95, 0x80, 0x91, 0x00, 0xc0
};

static const uint8_t phone_raw_desc_0[] = {
	0x0a, 0x24, 0x01, 0x00, 0x01, 0x4a, 0x00, 0x02,
	0x01, 0x02
};

static const uint8_t phone_raw_desc_1[] = {
	0x0c, 0x24, 0x02, 0x01, 0x01, 0x02, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00
};

static const uint8_t phone_raw_desc_2[] = {
	0x0c, 0x24, 0x02, 0x02, 0x01, 0x01, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00
};

static const uint8_t phone_raw_desc_3[] = {
	0x09, 0x24, 0x03, 0x03, 0x01, 0x03, 0x00, 0x06,
	0x00
};

static const uint8_t phone_raw_desc_4[] = {
	0x09, 0x24, 0x03, 0x04, 0x01, 0x01, 0x00, 0x05,
	0x00
};

static const uint8_t phone_raw_desc_5[] = {
	0x0b, 0x24, 0x06, 0x05, 0x01, 0x02, 0x03, 0x00,
	0x03, 0x00, 0x00
};

static const uint8_t phone_raw_desc_6[] = {
	0x0b, 0x24, 0x06, 0x06, 0x02, 0x02, 0x03, 0x00,
	0x03, 0x00, 0x00
};

static const void *phone_raw_iface_0_desc[] = {
	phone_raw_desc_0,
	phone_raw_desc_1,
	phone_raw_desc_2,
	phone_raw_desc_3,
	phone_raw_desc_4,
	phone_raw_desc_5,
	phone_raw_desc_6,
	NULL,
};

static const struct usb_temp_interface_desc phone_iface_0 = {
	.ppEndpoints = NULL,		/* no endpoints */
	.ppRawDesc = phone_raw_iface_0_desc,
	.bInterfaceClass = 1,
	.bInterfaceSubClass = 1,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_PHONE_MIXER,
};

static const uint8_t phone_raw_desc_20[] = {
	0x07, 0x24, 0x01, 0x04, 0x01, 0x01, 0x00
};

static const uint8_t phone_raw_desc_21[] = {
	0x0b, 0x24, 0x02, 0x01, 0x01, 0x02, 0x10, 0x01,
	/* 8kHz */
	0x40, 0x1f, 0x00
};

static const uint8_t phone_raw_desc_22[] = {
	0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00
};

static const void *phone_raw_iface_1_desc[] = {
	phone_raw_desc_20,
	phone_raw_desc_21,
	NULL,
};

static const void *phone_raw_ep_1_desc[] = {
	phone_raw_desc_22,
	NULL,
};

static const struct usb_temp_packet_size phone_isoc_mps = {
	.mps[USB_SPEED_FULL] = 0x10,
	.mps[USB_SPEED_HIGH] = 0x10,
};

static const struct usb_temp_interval phone_isoc_interval = {
	.bInterval[USB_SPEED_FULL] = 1,	/* 1:1 */
	.bInterval[USB_SPEED_HIGH] = 4,	/* 1:8 */
};

static const struct usb_temp_endpoint_desc phone_isoc_in_ep = {
	.ppRawDesc = phone_raw_ep_1_desc,
	.pPacketSize = &phone_isoc_mps,
	.pIntervals = &phone_isoc_interval,
	.bEndpointAddress = UE_DIR_IN,
	.bmAttributes = UE_ISOCHRONOUS,
};

static const struct usb_temp_endpoint_desc *phone_iface_1_ep[] = {
	&phone_isoc_in_ep,
	NULL,
};

static const struct usb_temp_interface_desc phone_iface_1_alt_0 = {
	.ppEndpoints = NULL,		/* no endpoints */
	.ppRawDesc = NULL,		/* no raw descriptors */
	.bInterfaceClass = 1,
	.bInterfaceSubClass = 2,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_PHONE_PLAYBACK,
};

static const struct usb_temp_interface_desc phone_iface_1_alt_1 = {
	.ppEndpoints = phone_iface_1_ep,
	.ppRawDesc = phone_raw_iface_1_desc,
	.bInterfaceClass = 1,
	.bInterfaceSubClass = 2,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_PHONE_PLAYBACK,
	.isAltInterface = 1,		/* this is an alternate setting */
};

static const uint8_t phone_raw_desc_30[] = {
	0x07, 0x24, 0x01, 0x02, 0x01, 0x01, 0x00
};

static const uint8_t phone_raw_desc_31[] = {
	0x0b, 0x24, 0x02, 0x01, 0x01, 0x02, 0x10, 0x01,
	/* 8kHz */
	0x40, 0x1f, 0x00
};

static const uint8_t phone_raw_desc_32[] = {
	0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00
};

static const void *phone_raw_iface_2_desc[] = {
	phone_raw_desc_30,
	phone_raw_desc_31,
	NULL,
};

static const void *phone_raw_ep_2_desc[] = {
	phone_raw_desc_32,
	NULL,
};

static const struct usb_temp_endpoint_desc phone_isoc_out_ep = {
	.ppRawDesc = phone_raw_ep_2_desc,
	.pPacketSize = &phone_isoc_mps,
	.pIntervals = &phone_isoc_interval,
	.bEndpointAddress = UE_DIR_OUT,
	.bmAttributes = UE_ISOCHRONOUS,
};

static const struct usb_temp_endpoint_desc *phone_iface_2_ep[] = {
	&phone_isoc_out_ep,
	NULL,
};

static const struct usb_temp_interface_desc phone_iface_2_alt_0 = {
	.ppEndpoints = NULL,		/* no endpoints */
	.ppRawDesc = NULL,		/* no raw descriptors */
	.bInterfaceClass = 1,
	.bInterfaceSubClass = 2,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_PHONE_RECORD,
};

static const struct usb_temp_interface_desc phone_iface_2_alt_1 = {
	.ppEndpoints = phone_iface_2_ep,
	.ppRawDesc = phone_raw_iface_2_desc,
	.bInterfaceClass = 1,
	.bInterfaceSubClass = 2,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_PHONE_RECORD,
	.isAltInterface = 1,		/* this is an alternate setting */
};

static const uint8_t phone_hid_raw_desc_0[] = {
	0x09, 0x21, 0x00, 0x01, 0x00, 0x01, 0x22, sizeof(phone_hid_descriptor),
	0x00
};

static const void *phone_hid_desc_0[] = {
	phone_hid_raw_desc_0,
	NULL,
};

static const struct usb_temp_packet_size phone_hid_mps = {
	.mps[USB_SPEED_FULL] = 0x10,
	.mps[USB_SPEED_HIGH] = 0x10,
};

static const struct usb_temp_interval phone_hid_interval = {
	.bInterval[USB_SPEED_FULL] = 2,		/* 2ms */
	.bInterval[USB_SPEED_HIGH] = 2,		/* 2ms */
};

static const struct usb_temp_endpoint_desc phone_hid_in_ep = {
	.pPacketSize = &phone_hid_mps,
	.pIntervals = &phone_hid_interval,
	.bEndpointAddress = UE_DIR_IN,
	.bmAttributes = UE_INTERRUPT,
};

static const struct usb_temp_endpoint_desc *phone_iface_3_ep[] = {
	&phone_hid_in_ep,
	NULL,
};

static const struct usb_temp_interface_desc phone_iface_3 = {
	.ppEndpoints = phone_iface_3_ep,
	.ppRawDesc = phone_hid_desc_0,
	.bInterfaceClass = 3,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_PHONE_HID,
};

static const struct usb_temp_interface_desc *phone_interfaces[] = {
	&phone_iface_0,
	&phone_iface_1_alt_0,
	&phone_iface_1_alt_1,
	&phone_iface_2_alt_0,
	&phone_iface_2_alt_1,
	&phone_iface_3,
	NULL,
};

static const struct usb_temp_config_desc phone_config_desc = {
	.ppIfaceDesc = phone_interfaces,
	.bmAttributes = UC_BUS_POWERED,
	.bMaxPower = 25,		/* 50 mA */
	.iConfiguration = INDEX_PHONE_PRODUCT,
};

static const struct usb_temp_config_desc *phone_configs[] = {
	&phone_config_desc,
	NULL,
};

static usb_temp_get_string_desc_t phone_get_string_desc;
static usb_temp_get_vendor_desc_t phone_get_vendor_desc;

const struct usb_temp_device_desc usb_template_phone = {
	.getStringDesc = &phone_get_string_desc,
	.getVendorDesc = &phone_get_vendor_desc,
	.ppConfigDesc = phone_configs,
	.idVendor = USB_TEMPLATE_VENDOR,
	.idProduct = 0xb001,
	.bcdDevice = 0x0100,
	.bDeviceClass = UDCLASS_IN_INTERFACE,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.iManufacturer = 0,
	.iProduct = INDEX_PHONE_PRODUCT,
	.iSerialNumber = 0,
};

/*------------------------------------------------------------------------*
 *      phone_get_vendor_desc
 *
 * Return values:
 * NULL: Failure. No such vendor descriptor.
 * Else: Success. Pointer to vendor descriptor is returned.
 *------------------------------------------------------------------------*/
static const void *
phone_get_vendor_desc(const struct usb_device_request *req, uint16_t *plen)
{
	if ((req->bmRequestType == 0x81) && (req->bRequest == 0x06) &&
	    (req->wValue[0] == 0x00) && (req->wValue[1] == 0x22) &&
	    (req->wIndex[1] == 0) && (req->wIndex[0] == 3 /* iface */)) {

		*plen = sizeof(phone_hid_descriptor);
		return (phone_hid_descriptor);
	}
	return (NULL);
}

/*------------------------------------------------------------------------*
 *	phone_get_string_desc
 *
 * Return values:
 * NULL: Failure. No such string.
 * Else: Success. Pointer to string descriptor is returned.
 *------------------------------------------------------------------------*/
static const void *
phone_get_string_desc(uint16_t lang_id, uint8_t string_index)
{
	static const void *ptr[INDEX_PHONE_MAX] = {
		[INDEX_PHONE_LANG] = &usb_string_lang_en,
		[INDEX_PHONE_MIXER] = &string_phone_mixer,
		[INDEX_PHONE_RECORD] = &string_phone_record,
		[INDEX_PHONE_PLAYBACK] = &string_phone_playback,
		[INDEX_PHONE_PRODUCT] = &string_phone_product,
		[INDEX_PHONE_HID] = &string_phone_hid,
	};

	if (string_index == 0) {
		return (&usb_string_lang_en);
	}
	if (lang_id != 0x0409) {
		return (NULL);
	}
	if (string_index < INDEX_PHONE_MAX) {
		return (ptr[string_index]);
	}
	return (NULL);
}
