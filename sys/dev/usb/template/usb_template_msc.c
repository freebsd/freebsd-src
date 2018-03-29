/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky <hselasky@FreeBSD.org>
 * All rights reserved.
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
 * This file contains the USB templates for an USB Mass Storage Device.
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

#include <dev/usb/template/usb_template.h>
#endif			/* USB_GLOBAL_INCLUDE_FILE */

enum {
	STRING_LANG_INDEX,
	STRING_MSC_DATA_INDEX,
	STRING_MSC_CONFIG_INDEX,
	STRING_MSC_VENDOR_INDEX,
	STRING_MSC_PRODUCT_INDEX,
	STRING_MSC_SERIAL_INDEX,
	STRING_MSC_MAX,
};

#define	STRING_MSC_DATA	\
  "U\0S\0B\0 \0M\0a\0s\0s\0 \0S\0t\0o\0r\0a\0g\0e\0 " \
  "\0I\0n\0t\0e\0r\0f\0a\0c\0e"

#define	STRING_MSC_CONFIG \
  "D\0e\0f\0a\0u\0l\0t\0 \0c\0o\0n\0f\0i\0g"

#define	STRING_MSC_VENDOR \
  "F\0r\0e\0e\0B\0S\0D\0 \0f\0o\0u\0n\0d\0a\0t\0i\0o\0n"

#define	STRING_MSC_PRODUCT \
  "U\0S\0B\0 \0M\0e\0m\0o\0r\0y\0 \0S\0t\0i\0c\0k"

#define	STRING_MSC_SERIAL \
  "M\0a\0r\0c\0h\0 \0002\0000\0000\08"

/* make the real string descriptors */

USB_MAKE_STRING_DESC(STRING_MSC_DATA, string_msc_data);
USB_MAKE_STRING_DESC(STRING_MSC_CONFIG, string_msc_config);
USB_MAKE_STRING_DESC(STRING_MSC_VENDOR, string_msc_vendor);
USB_MAKE_STRING_DESC(STRING_MSC_PRODUCT, string_msc_product);
USB_MAKE_STRING_DESC(STRING_MSC_SERIAL, string_msc_serial);

/* prototypes */

static usb_temp_get_string_desc_t msc_get_string_desc;

static const struct usb_temp_packet_size bulk_mps = {
	.mps[USB_SPEED_FULL] = 64,
	.mps[USB_SPEED_HIGH] = 512,
};

static const struct usb_temp_endpoint_desc bulk_in_ep = {
	.pPacketSize = &bulk_mps,
#ifdef USB_HIP_IN_EP_0
	.bEndpointAddress = USB_HIP_IN_EP_0,
#else
	.bEndpointAddress = UE_DIR_IN,
#endif
	.bmAttributes = UE_BULK,
};

static const struct usb_temp_endpoint_desc bulk_out_ep = {
	.pPacketSize = &bulk_mps,
#ifdef USB_HIP_OUT_EP_0
	.bEndpointAddress = USB_HIP_OUT_EP_0,
#else
	.bEndpointAddress = UE_DIR_OUT,
#endif
	.bmAttributes = UE_BULK,
};

static const struct usb_temp_endpoint_desc *msc_data_endpoints[] = {
	&bulk_in_ep,
	&bulk_out_ep,
	NULL,
};

static const struct usb_temp_interface_desc msc_data_interface = {
	.ppEndpoints = msc_data_endpoints,
	.bInterfaceClass = UICLASS_MASS,
	.bInterfaceSubClass = UISUBCLASS_SCSI,
	.bInterfaceProtocol = UIPROTO_MASS_BBB,
	.iInterface = STRING_MSC_DATA_INDEX,
};

static const struct usb_temp_interface_desc *msc_interfaces[] = {
	&msc_data_interface,
	NULL,
};

static const struct usb_temp_config_desc msc_config_desc = {
	.ppIfaceDesc = msc_interfaces,
	.bmAttributes = UC_BUS_POWERED,
	.bMaxPower = 25,		/* 50 mA */
	.iConfiguration = STRING_MSC_CONFIG_INDEX,
};

static const struct usb_temp_config_desc *msc_configs[] = {
	&msc_config_desc,
	NULL,
};

const struct usb_temp_device_desc usb_template_msc = {
	.getStringDesc = &msc_get_string_desc,
	.ppConfigDesc = msc_configs,
	.idVendor = USB_TEMPLATE_VENDOR,
	.idProduct = 0x0012,
	.bcdDevice = 0x0100,
	.bDeviceClass = UDCLASS_COMM,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.iManufacturer = STRING_MSC_VENDOR_INDEX,
	.iProduct = STRING_MSC_PRODUCT_INDEX,
	.iSerialNumber = STRING_MSC_SERIAL_INDEX,
};

/*------------------------------------------------------------------------*
 *	msc_get_string_desc
 *
 * Return values:
 * NULL: Failure. No such string.
 * Else: Success. Pointer to string descriptor is returned.
 *------------------------------------------------------------------------*/
static const void *
msc_get_string_desc(uint16_t lang_id, uint8_t string_index)
{
	static const void *ptr[STRING_MSC_MAX] = {
		[STRING_LANG_INDEX] = &usb_string_lang_en,
		[STRING_MSC_DATA_INDEX] = &string_msc_data,
		[STRING_MSC_CONFIG_INDEX] = &string_msc_config,
		[STRING_MSC_VENDOR_INDEX] = &string_msc_vendor,
		[STRING_MSC_PRODUCT_INDEX] = &string_msc_product,
		[STRING_MSC_SERIAL_INDEX] = &string_msc_serial,
	};

	if (string_index == 0) {
		return (&usb_string_lang_en);
	}
	if (lang_id != 0x0409) {
		return (NULL);
	}
	if (string_index < STRING_MSC_MAX) {
		return (ptr[string_index]);
	}
	return (NULL);
}
