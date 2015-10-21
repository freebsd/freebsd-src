/* $FreeBSD$ */
/*-
 * Copyright (c) 2015 Hans Petter Selasky. All rights reserved.
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
 * This file contains the USB template for an USB MIDI Device.
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
#endif					/* USB_GLOBAL_INCLUDE_FILE */

enum {
	INDEX_MIDI_LANG,
	INDEX_MIDI_IF,
	INDEX_MIDI_PRODUCT,
	INDEX_MIDI_MAX,
};

#define	STRING_MIDI_PRODUCT \
  "M\0I\0D\0I\0 \0T\0e\0s\0t\0 \0D\0e\0v\0i\0c\0e"

#define	STRING_MIDI_IF \
  "M\0I\0D\0I\0 \0i\0n\0t\0e\0r\0f\0a\0c\0e"

/* make the real string descriptors */

USB_MAKE_STRING_DESC(STRING_MIDI_IF, string_midi_if);
USB_MAKE_STRING_DESC(STRING_MIDI_PRODUCT, string_midi_product);

/* prototypes */

static const uint8_t midi_desc_raw_0[9] = {
	0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01
};

static const void *midi_descs_0[] = {
	&midi_desc_raw_0,
	NULL
};

static const struct usb_temp_interface_desc midi_iface_0 = {
	.ppEndpoints = NULL,		/* no endpoints */
	.ppRawDesc = midi_descs_0,
	.bInterfaceClass = 1,
	.bInterfaceSubClass = 1,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_MIDI_IF,
};

static const struct usb_temp_packet_size midi_mps = {
	.mps[USB_SPEED_LOW] = 8,
	.mps[USB_SPEED_FULL] = 64,
	.mps[USB_SPEED_HIGH] = 512,
};

static const uint8_t midi_desc_raw_7[5] = {
	0x05, 0x25, 0x01, 0x01, 0x01
};

static const void *midi_descs_2[] = {
	&midi_desc_raw_7,
	NULL
};

static const struct usb_temp_endpoint_desc midi_bulk_out_ep = {
	.ppRawDesc = midi_descs_2,
	.pPacketSize = &midi_mps,
	.bEndpointAddress = UE_DIR_OUT,
	.bmAttributes = UE_BULK,
};

static const uint8_t midi_desc_raw_6[5] = {
	0x05, 0x25, 0x01, 0x01, 0x03,
};

static const void *midi_descs_3[] = {
	&midi_desc_raw_6,
	NULL
};

static const struct usb_temp_endpoint_desc midi_bulk_in_ep = {
	.ppRawDesc = midi_descs_3,
	.pPacketSize = &midi_mps,
	.bEndpointAddress = UE_DIR_IN,
	.bmAttributes = UE_BULK,
};

static const struct usb_temp_endpoint_desc *midi_iface_1_ep[] = {
	&midi_bulk_out_ep,
	&midi_bulk_in_ep,
	NULL,
};

static const uint8_t midi_desc_raw_1[7] = {
	0x07, 0x24, 0x01, 0x00, 0x01, /* wTotalLength: */ 0x41, 0x00
};

static const uint8_t midi_desc_raw_2[6] = {
	0x06, 0x24, 0x02, 0x01, 0x01, 0x00
};

static const uint8_t midi_desc_raw_3[6] = {
	0x06, 0x24, 0x02, 0x02, 0x02, 0x00
};

static const uint8_t midi_desc_raw_4[9] = {
	0x09, 0x24, 0x03, 0x01, 0x03, 0x01, 0x02, 0x01, 0x00
};

static const uint8_t midi_desc_raw_5[9] = {
	0x09, 0x24, 0x03, 0x02, 0x04, 0x01, 0x01, 0x01, 0x00
};

static const void *midi_descs_1[] = {
	&midi_desc_raw_1,
	&midi_desc_raw_2,
	&midi_desc_raw_3,
	&midi_desc_raw_4,
	&midi_desc_raw_5,
	NULL
};

static const struct usb_temp_interface_desc midi_iface_1 = {
	.ppRawDesc = midi_descs_1,
	.ppEndpoints = midi_iface_1_ep,
	.bInterfaceClass = 0x01,	/* MIDI */
	.bInterfaceSubClass = 3,	/* MIDI streaming */
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_MIDI_IF,
};

static const struct usb_temp_interface_desc *midi_interfaces[] = {
	&midi_iface_0,
	&midi_iface_1,
	NULL,
};

static const struct usb_temp_config_desc midi_config_desc = {
	.ppIfaceDesc = midi_interfaces,
	.bmAttributes = UC_BUS_POWERED,
	.bMaxPower = 25,		/* 50 mA */
	.iConfiguration = INDEX_MIDI_PRODUCT,
};

static const struct usb_temp_config_desc *midi_configs[] = {
	&midi_config_desc,
	NULL,
};

static usb_temp_get_string_desc_t midi_get_string_desc;

const struct usb_temp_device_desc usb_template_midi = {
	.getStringDesc = &midi_get_string_desc,
	.ppConfigDesc = midi_configs,
	.idVendor = USB_TEMPLATE_VENDOR,
	.idProduct = 0x00BB,
	.bcdDevice = 0x0100,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.iManufacturer = 0,
	.iProduct = INDEX_MIDI_PRODUCT,
	.iSerialNumber = 0,
};

/*------------------------------------------------------------------------*
 *	midi_get_string_desc
 *
 * Return values:
 * NULL: Failure. No such string.
 * Else: Success. Pointer to string descriptor is returned.
 *------------------------------------------------------------------------*/
static const void *
midi_get_string_desc(uint16_t lang_id, uint8_t string_index)
{
	static const void *ptr[INDEX_MIDI_MAX] = {
		[INDEX_MIDI_LANG] = &usb_string_lang_en,
		[INDEX_MIDI_IF] = &string_midi_if,
		[INDEX_MIDI_PRODUCT] = &string_midi_product,
	};

	if (string_index == 0) {
		return (&usb_string_lang_en);
	}
	if (lang_id != 0x0409) {
		return (NULL);
	}
	if (string_index < INDEX_MIDI_MAX) {
		return (ptr[string_index]);
	}
	return (NULL);
}
