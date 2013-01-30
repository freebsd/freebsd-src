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
 * This file contains the USB template for an USB Audio Device.
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
	INDEX_AUDIO_LANG,
	INDEX_AUDIO_MIXER,
	INDEX_AUDIO_RECORD,
	INDEX_AUDIO_PLAYBACK,
	INDEX_AUDIO_PRODUCT,
	INDEX_AUDIO_MAX,
};

#define	STRING_AUDIO_PRODUCT \
  "A\0u\0d\0i\0o\0 \0T\0e\0s\0t\0 \0D\0e\0v\0i\0c\0e"

#define	STRING_AUDIO_MIXER \
  "M\0i\0x\0e\0r\0 \0i\0n\0t\0e\0r\0f\0a\0c\0e"

#define	STRING_AUDIO_RECORD \
  "R\0e\0c\0o\0r\0d\0 \0i\0n\0t\0e\0r\0f\0a\0c\0e"

#define	STRING_AUDIO_PLAYBACK \
  "P\0l\0a\0y\0b\0a\0c\0k\0 \0i\0n\0t\0e\0r\0f\0a\0c\0e"


/* make the real string descriptors */

USB_MAKE_STRING_DESC(STRING_AUDIO_MIXER, string_audio_mixer);
USB_MAKE_STRING_DESC(STRING_AUDIO_RECORD, string_audio_record);
USB_MAKE_STRING_DESC(STRING_AUDIO_PLAYBACK, string_audio_playback);
USB_MAKE_STRING_DESC(STRING_AUDIO_PRODUCT, string_audio_product);

/* prototypes */

/*
 * Audio Mixer description structures
 *
 * Some of the audio descriptors were dumped
 * from a Creative Labs USB audio device.
 */

static const uint8_t audio_raw_desc_0[] = {
	0x0a, 0x24, 0x01, 0x00, 0x01, 0xa9, 0x00, 0x02,
	0x01, 0x02
};

static const uint8_t audio_raw_desc_1[] = {
	0x0c, 0x24, 0x02, 0x01, 0x01, 0x01, 0x00, 0x02,
	0x03, 0x00, 0x00, 0x00
};

static const uint8_t audio_raw_desc_2[] = {
	0x0c, 0x24, 0x02, 0x02, 0x01, 0x02, 0x00, 0x02,
	0x03, 0x00, 0x00, 0x00
};

static const uint8_t audio_raw_desc_3[] = {
	0x0c, 0x24, 0x02, 0x03, 0x03, 0x06, 0x00, 0x02,
	0x03, 0x00, 0x00, 0x00
};

static const uint8_t audio_raw_desc_4[] = {
	0x0c, 0x24, 0x02, 0x04, 0x05, 0x06, 0x00, 0x02,
	0x03, 0x00, 0x00, 0x00
};

static const uint8_t audio_raw_desc_5[] = {
	0x09, 0x24, 0x03, 0x05, 0x05, 0x06, 0x00, 0x01,
	0x00
};

static const uint8_t audio_raw_desc_6[] = {
	0x09, 0x24, 0x03, 0x06, 0x01, 0x03, 0x00, 0x09,
	0x00
};

static const uint8_t audio_raw_desc_7[] = {
	0x09, 0x24, 0x03, 0x07, 0x01, 0x01, 0x00, 0x08,
	0x00
};

static const uint8_t audio_raw_desc_8[] = {
	0x09, 0x24, 0x05, 0x08, 0x03, 0x0a, 0x0b, 0x0c,
	0x00
};

static const uint8_t audio_raw_desc_9[] = {
	0x0a, 0x24, 0x06, 0x09, 0x0f, 0x01, 0x01, 0x02,
	0x02, 0x00
};

static const uint8_t audio_raw_desc_10[] = {
	0x0a, 0x24, 0x06, 0x0a, 0x02, 0x01, 0x43, 0x00,
	0x00, 0x00
};

static const uint8_t audio_raw_desc_11[] = {
	0x0a, 0x24, 0x06, 0x0b, 0x03, 0x01, 0x01, 0x02,
	0x02, 0x00
};

static const uint8_t audio_raw_desc_12[] = {
	0x0a, 0x24, 0x06, 0x0c, 0x04, 0x01, 0x01, 0x00,
	0x00, 0x00
};

static const uint8_t audio_raw_desc_13[] = {
	0x0a, 0x24, 0x06, 0x0d, 0x02, 0x01, 0x03, 0x00,
	0x00, 0x00
};

static const uint8_t audio_raw_desc_14[] = {
	0x0a, 0x24, 0x06, 0x0e, 0x03, 0x01, 0x01, 0x02,
	0x02, 0x00
};

static const uint8_t audio_raw_desc_15[] = {
	0x0f, 0x24, 0x04, 0x0f, 0x03, 0x01, 0x0d, 0x0e,
	0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const void *audio_raw_iface_0_desc[] = {
	audio_raw_desc_0,
	audio_raw_desc_1,
	audio_raw_desc_2,
	audio_raw_desc_3,
	audio_raw_desc_4,
	audio_raw_desc_5,
	audio_raw_desc_6,
	audio_raw_desc_7,
	audio_raw_desc_8,
	audio_raw_desc_9,
	audio_raw_desc_10,
	audio_raw_desc_11,
	audio_raw_desc_12,
	audio_raw_desc_13,
	audio_raw_desc_14,
	audio_raw_desc_15,
	NULL,
};

static const struct usb_temp_interface_desc audio_iface_0 = {
	.ppEndpoints = NULL,		/* no endpoints */
	.ppRawDesc = audio_raw_iface_0_desc,
	.bInterfaceClass = 1,
	.bInterfaceSubClass = 1,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_AUDIO_MIXER,
};

static const uint8_t audio_raw_desc_20[] = {
	0x07, 0x24, 0x01, 0x01, 0x03, 0x01, 0x00

};

static const uint8_t audio_raw_desc_21[] = {
	0x0b, 0x24, 0x02, 0x01, 0x02, 0x02, 0x10, 0x01,
	/* 48kHz */
	0x80, 0xbb, 0x00
};

static const uint8_t audio_raw_desc_22[] = {
	0x07, 0x25, 0x01, 0x00, 0x01, 0x04, 0x00
};

static const void *audio_raw_iface_1_desc[] = {
	audio_raw_desc_20,
	audio_raw_desc_21,
	NULL,
};

static const void *audio_raw_ep_1_desc[] = {
	audio_raw_desc_22,
	NULL,
};

static const struct usb_temp_packet_size audio_isoc_mps = {
  .mps[USB_SPEED_FULL] = 0xC8,
  .mps[USB_SPEED_HIGH] = 0xC8,
};

static const struct usb_temp_interval audio_isoc_interval = {
	.bInterval[USB_SPEED_FULL] = 1,	/* 1:1 */
	.bInterval[USB_SPEED_HIGH] = 4,	/* 1:8 */
};

static const struct usb_temp_endpoint_desc audio_isoc_out_ep = {
	.ppRawDesc = audio_raw_ep_1_desc,
	.pPacketSize = &audio_isoc_mps,
	.pIntervals = &audio_isoc_interval,
	.bEndpointAddress = UE_DIR_OUT,
	.bmAttributes = UE_ISOCHRONOUS | UE_ISO_ADAPT,
};

static const struct usb_temp_endpoint_desc *audio_iface_1_ep[] = {
	&audio_isoc_out_ep,
	NULL,
};

static const struct usb_temp_interface_desc audio_iface_1_alt_0 = {
	.ppEndpoints = NULL,		/* no endpoints */
	.ppRawDesc = NULL,		/* no raw descriptors */
	.bInterfaceClass = 1,
	.bInterfaceSubClass = 2,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_AUDIO_PLAYBACK,
};

static const struct usb_temp_interface_desc audio_iface_1_alt_1 = {
	.ppEndpoints = audio_iface_1_ep,
	.ppRawDesc = audio_raw_iface_1_desc,
	.bInterfaceClass = 1,
	.bInterfaceSubClass = 2,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_AUDIO_PLAYBACK,
	.isAltInterface = 1,		/* this is an alternate setting */
};

static const uint8_t audio_raw_desc_30[] = {
	0x07, 0x24, 0x01, 0x07, 0x01, 0x01, 0x00

};

static const uint8_t audio_raw_desc_31[] = {
	0x0b, 0x24, 0x02, 0x01, 0x02, 0x02, 0x10, 0x01,
	/* 48kHz */
	0x80, 0xbb, 0x00
};

static const uint8_t audio_raw_desc_32[] = {
	0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00
};

static const void *audio_raw_iface_2_desc[] = {
	audio_raw_desc_30,
	audio_raw_desc_31,
	NULL,
};

static const void *audio_raw_ep_2_desc[] = {
	audio_raw_desc_32,
	NULL,
};

static const struct usb_temp_endpoint_desc audio_isoc_in_ep = {
	.ppRawDesc = audio_raw_ep_2_desc,
	.pPacketSize = &audio_isoc_mps,
	.pIntervals = &audio_isoc_interval,
	.bEndpointAddress = UE_DIR_IN,
	.bmAttributes = UE_ISOCHRONOUS | UE_ISO_ADAPT,
};

static const struct usb_temp_endpoint_desc *audio_iface_2_ep[] = {
	&audio_isoc_in_ep,
	NULL,
};

static const struct usb_temp_interface_desc audio_iface_2_alt_0 = {
	.ppEndpoints = NULL,		/* no endpoints */
	.ppRawDesc = NULL,		/* no raw descriptors */
	.bInterfaceClass = 1,
	.bInterfaceSubClass = 2,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_AUDIO_RECORD,
};

static const struct usb_temp_interface_desc audio_iface_2_alt_1 = {
	.ppEndpoints = audio_iface_2_ep,
	.ppRawDesc = audio_raw_iface_2_desc,
	.bInterfaceClass = 1,
	.bInterfaceSubClass = 2,
	.bInterfaceProtocol = 0,
	.iInterface = INDEX_AUDIO_RECORD,
	.isAltInterface = 1,		/* this is an alternate setting */
};

static const struct usb_temp_interface_desc *audio_interfaces[] = {
	&audio_iface_0,
	&audio_iface_1_alt_0,
	&audio_iface_1_alt_1,
	&audio_iface_2_alt_0,
	&audio_iface_2_alt_1,
	NULL,
};

static const struct usb_temp_config_desc audio_config_desc = {
	.ppIfaceDesc = audio_interfaces,
	.bmAttributes = UC_BUS_POWERED,
	.bMaxPower = 25,		/* 50 mA */
	.iConfiguration = INDEX_AUDIO_PRODUCT,
};

static const struct usb_temp_config_desc *audio_configs[] = {
	&audio_config_desc,
	NULL,
};

static usb_temp_get_string_desc_t audio_get_string_desc;

const struct usb_temp_device_desc usb_template_audio = {
	.getStringDesc = &audio_get_string_desc,
	.ppConfigDesc = audio_configs,
	.idVendor = USB_TEMPLATE_VENDOR,
	.idProduct = 0x000A,
	.bcdDevice = 0x0100,
	.bDeviceClass = UDCLASS_COMM,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.iManufacturer = 0,
	.iProduct = INDEX_AUDIO_PRODUCT,
	.iSerialNumber = 0,
};

/*------------------------------------------------------------------------*
 *	audio_get_string_desc
 *
 * Return values:
 * NULL: Failure. No such string.
 * Else: Success. Pointer to string descriptor is returned.
 *------------------------------------------------------------------------*/
static const void *
audio_get_string_desc(uint16_t lang_id, uint8_t string_index)
{
	static const void *ptr[INDEX_AUDIO_MAX] = {
		[INDEX_AUDIO_LANG] = &usb_string_lang_en,
		[INDEX_AUDIO_MIXER] = &string_audio_mixer,
		[INDEX_AUDIO_RECORD] = &string_audio_record,
		[INDEX_AUDIO_PLAYBACK] = &string_audio_playback,
		[INDEX_AUDIO_PRODUCT] = &string_audio_product,
	};

	if (string_index == 0) {
		return (&usb_string_lang_en);
	}
	if (lang_id != 0x0409) {
		return (NULL);
	}
	if (string_index < INDEX_AUDIO_MAX) {
		return (ptr[string_index]);
	}
	return (NULL);
}
