/* $FreeBSD$ */
/*-
 * Copyright (c) 2010 Hans Petter Selasky. All rights reserved.
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
 * This file contains the USB template for an USB Modem Device.
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
	INDEX_LANG,
	INDEX_MODEM,
	INDEX_PRODUCT,
	INDEX_MAX,
};

#define	STRING_PRODUCT \
  "M\0o\0d\0e\0m\0 \0T\0e\0s\0t\0 \0D\0e\0v\0i\0c\0e"

#define	STRING_MODEM \
  "M\0o\0d\0e\0m\0 \0i\0n\0t\0e\0r\0f\0a\0c\0e"

/* make the real string descriptors */

USB_MAKE_STRING_DESC(STRING_MODEM, string_modem);
USB_MAKE_STRING_DESC(STRING_PRODUCT, string_product);

#define	MODEM_IFACE_0 0
#define	MODEM_IFACE_1 1

/* prototypes */

static const struct usb_temp_packet_size modem_bulk_mps = {
	.mps[USB_SPEED_LOW] = 8,
	.mps[USB_SPEED_FULL] = 64,
	.mps[USB_SPEED_HIGH] = 512,
};

static const struct usb_temp_packet_size modem_intr_mps = {
	.mps[USB_SPEED_LOW] = 8,
	.mps[USB_SPEED_FULL] = 8,
	.mps[USB_SPEED_HIGH] = 8,
};

static const struct usb_temp_interval modem_intr_interval = {
	.bInterval[USB_SPEED_LOW] = 8,	/* 8ms */
	.bInterval[USB_SPEED_FULL] = 8,	/* 8ms */
	.bInterval[USB_SPEED_HIGH] = 7,	/* 8ms */
};

static const struct usb_temp_endpoint_desc modem_ep_0 = {
	.pPacketSize = &modem_intr_mps,
	.pIntervals = &modem_intr_interval,
	.bEndpointAddress = UE_DIR_IN,
	.bmAttributes = UE_INTERRUPT,
};

static const struct usb_temp_endpoint_desc modem_ep_1 = {
	.pPacketSize = &modem_bulk_mps,
	.bEndpointAddress = UE_DIR_OUT,
	.bmAttributes = UE_BULK,
};

static const struct usb_temp_endpoint_desc modem_ep_2 = {
	.pPacketSize = &modem_bulk_mps,
	.bEndpointAddress = UE_DIR_IN,
	.bmAttributes = UE_BULK,
};

static const struct usb_temp_endpoint_desc *modem_iface_0_ep[] = {
	&modem_ep_0,
	NULL,
};

static const struct usb_temp_endpoint_desc *modem_iface_1_ep[] = {
	&modem_ep_1,
	&modem_ep_2,
	NULL,
};

static const uint8_t modem_raw_desc_0[] = {
	0x05, 0x24, 0x00, 0x10, 0x01
};

static const uint8_t modem_raw_desc_1[] = {
	0x05, 0x24, 0x06, MODEM_IFACE_0, MODEM_IFACE_1
};

static const uint8_t modem_raw_desc_2[] = {
	0x05, 0x24, 0x01, 0x03, MODEM_IFACE_1
};

static const uint8_t modem_raw_desc_3[] = {
	0x04, 0x24, 0x02, 0x07
};

static const void *modem_iface_0_desc[] = {
	&modem_raw_desc_0,
	&modem_raw_desc_1,
	&modem_raw_desc_2,
	&modem_raw_desc_3,
	NULL,
};

static const struct usb_temp_interface_desc modem_iface_0 = {
	.ppRawDesc = modem_iface_0_desc,
	.ppEndpoints = modem_iface_0_ep,
	.bInterfaceClass = 2,
	.bInterfaceSubClass = 2,
	.bInterfaceProtocol = 1,
	.iInterface = INDEX_MODEM,
};

static const struct usb_temp_interface_desc modem_iface_1 = {
	.ppEndpoints = modem_iface_1_ep,
	.bInterfaceClass = 10,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_MODEM,
};

static const struct usb_temp_interface_desc *modem_interfaces[] = {
	&modem_iface_0,
	&modem_iface_1,
	NULL,
};

static const struct usb_temp_config_desc modem_config_desc = {
	.ppIfaceDesc = modem_interfaces,
	.bmAttributes = UC_BUS_POWERED,
	.bMaxPower = 25,		/* 50 mA */
	.iConfiguration = INDEX_PRODUCT,
};

static const struct usb_temp_config_desc *modem_configs[] = {
	&modem_config_desc,
	NULL,
};

static usb_temp_get_string_desc_t modem_get_string_desc;
static usb_temp_get_vendor_desc_t modem_get_vendor_desc;

const struct usb_temp_device_desc usb_template_modem = {
	.getStringDesc = &modem_get_string_desc,
	.getVendorDesc = &modem_get_vendor_desc,
	.ppConfigDesc = modem_configs,
	.idVendor = USB_TEMPLATE_VENDOR,
	.idProduct = 0x000E,
	.bcdDevice = 0x0100,
	.bDeviceClass = UDCLASS_COMM,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.iManufacturer = 0,
	.iProduct = INDEX_PRODUCT,
	.iSerialNumber = 0,
};

/*------------------------------------------------------------------------*
 *      modem_get_vendor_desc
 *
 * Return values:
 * NULL: Failure. No such vendor descriptor.
 * Else: Success. Pointer to vendor descriptor is returned.
 *------------------------------------------------------------------------*/
static const void *
modem_get_vendor_desc(const struct usb_device_request *req, uint16_t *plen)
{
	return (NULL);
}

/*------------------------------------------------------------------------*
 *	modem_get_string_desc
 *
 * Return values:
 * NULL: Failure. No such string.
 * Else: Success. Pointer to string descriptor is returned.
 *------------------------------------------------------------------------*/
static const void *
modem_get_string_desc(uint16_t lang_id, uint8_t string_index)
{
	static const void *ptr[INDEX_MAX] = {
		[INDEX_LANG] = &usb_string_lang_en,
		[INDEX_MODEM] = &string_modem,
		[INDEX_PRODUCT] = &string_product,
	};

	if (string_index == 0) {
		return (&usb_string_lang_en);
	}
	if (lang_id != 0x0409) {
		return (NULL);
	}
	if (string_index < INDEX_MAX) {
		return (ptr[string_index]);
	}
	return (NULL);
}
