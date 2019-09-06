/* $FreeBSD$ */
/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2008 Hans Petter Selasky <hselasky@FreeBSD.org>
 * Copyright (c) 2018 The FreeBSD Foundation
 * Copyright (c) 2019 Edward Tomasz Napierala <trasz@FreeBSD.org>
 * All rights reserved.
 *
 * Portions of this software were developed by Edward Tomasz Napierala
 * under sponsorship from the FreeBSD Foundation.
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
#include <dev/usb/usb_ioctl.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/template/usb_template.h>
#endif			/* USB_GLOBAL_INCLUDE_FILE */

enum {
	CDCEEM_LANG_INDEX,
	CDCEEM_INTERFACE_INDEX,
	CDCEEM_CONFIGURATION_INDEX,
	CDCEEM_MANUFACTURER_INDEX,
	CDCEEM_PRODUCT_INDEX,
	CDCEEM_SERIAL_NUMBER_INDEX,
	CDCEEM_MAX_INDEX,
};

#define	CDCEEM_DEFAULT_VENDOR_ID	USB_TEMPLATE_VENDOR
#define	CDCEEM_DEFAULT_PRODUCT_ID	0x27df
#define	CDCEEM_DEFAULT_INTERFACE	"USB CDC EEM Interface"
#define	CDCEEM_DEFAULT_CONFIGURATION	"Default Config"
#define	CDCEEM_DEFAULT_MANUFACTURER	USB_TEMPLATE_MANUFACTURER
#define	CDCEEM_DEFAULT_PRODUCT		"CDC EEM"
#define	CDCEEM_DEFAULT_SERIAL_NUMBER	"March 2008"

static struct usb_string_descriptor	cdceem_interface;
static struct usb_string_descriptor	cdceem_configuration;
static struct usb_string_descriptor	cdceem_manufacturer;
static struct usb_string_descriptor	cdceem_product;
static struct usb_string_descriptor	cdceem_serial_number;

static struct sysctl_ctx_list		cdceem_ctx_list;

/* prototypes */

static usb_temp_get_string_desc_t cdceem_get_string_desc;

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

static const struct usb_temp_endpoint_desc *cdceem_data_endpoints[] = {
	&bulk_in_ep,
	&bulk_out_ep,
	NULL,
};

static const struct usb_temp_interface_desc cdceem_data_interface = {
	.ppEndpoints = cdceem_data_endpoints,
	.bInterfaceClass = UICLASS_CDC,
	.bInterfaceSubClass = UISUBCLASS_ETHERNET_EMULATION_MODEL,
	.bInterfaceProtocol = UIPROTO_CDC_EEM,
	.iInterface = CDCEEM_INTERFACE_INDEX,
};

static const struct usb_temp_interface_desc *cdceem_interfaces[] = {
	&cdceem_data_interface,
	NULL,
};

static const struct usb_temp_config_desc cdceem_config_desc = {
	.ppIfaceDesc = cdceem_interfaces,
	.bmAttributes = 0,
	.bMaxPower = 0,
	.iConfiguration = CDCEEM_CONFIGURATION_INDEX,
};

static const struct usb_temp_config_desc *cdceem_configs[] = {
	&cdceem_config_desc,
	NULL,
};

struct usb_temp_device_desc usb_template_cdceem = {
	.getStringDesc = &cdceem_get_string_desc,
	.ppConfigDesc = cdceem_configs,
	.idVendor = CDCEEM_DEFAULT_VENDOR_ID,
	.idProduct = CDCEEM_DEFAULT_PRODUCT_ID,
	.bcdDevice = 0x0100,
	.bDeviceClass = UDCLASS_COMM,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.iManufacturer = CDCEEM_MANUFACTURER_INDEX,
	.iProduct = CDCEEM_PRODUCT_INDEX,
	.iSerialNumber = CDCEEM_SERIAL_NUMBER_INDEX,
};

/*------------------------------------------------------------------------*
 *	cdceem_get_string_desc
 *
 * Return values:
 * NULL: Failure. No such string.
 * Else: Success. Pointer to string descriptor is returned.
 *------------------------------------------------------------------------*/
static const void *
cdceem_get_string_desc(uint16_t lang_id, uint8_t string_index)
{
	static const void *ptr[CDCEEM_MAX_INDEX] = {
		[CDCEEM_LANG_INDEX] = &usb_string_lang_en,
		[CDCEEM_INTERFACE_INDEX] = &cdceem_interface,
		[CDCEEM_CONFIGURATION_INDEX] = &cdceem_configuration,
		[CDCEEM_MANUFACTURER_INDEX] = &cdceem_manufacturer,
		[CDCEEM_PRODUCT_INDEX] = &cdceem_product,
		[CDCEEM_SERIAL_NUMBER_INDEX] = &cdceem_serial_number,
	};

	if (string_index == 0) {
		return (&usb_string_lang_en);
	}
	if (lang_id != 0x0409) {
		return (NULL);
	}
	if (string_index < CDCEEM_MAX_INDEX) {
		return (ptr[string_index]);
	}
	return (NULL);
}

static void
cdceem_init(void *arg __unused)
{
	struct sysctl_oid *parent;
	char parent_name[3];

	usb_make_str_desc(&cdceem_interface, sizeof(cdceem_interface),
	    CDCEEM_DEFAULT_INTERFACE);
	usb_make_str_desc(&cdceem_configuration, sizeof(cdceem_configuration),
	    CDCEEM_DEFAULT_CONFIGURATION);
	usb_make_str_desc(&cdceem_manufacturer, sizeof(cdceem_manufacturer),
	    CDCEEM_DEFAULT_MANUFACTURER);
	usb_make_str_desc(&cdceem_product, sizeof(cdceem_product),
	    CDCEEM_DEFAULT_PRODUCT);
	usb_make_str_desc(&cdceem_serial_number, sizeof(cdceem_serial_number),
	    CDCEEM_DEFAULT_SERIAL_NUMBER);

	snprintf(parent_name, sizeof(parent_name), "%d", USB_TEMP_CDCEEM);
	sysctl_ctx_init(&cdceem_ctx_list);

	parent = SYSCTL_ADD_NODE(&cdceem_ctx_list,
	    SYSCTL_STATIC_CHILDREN(_hw_usb_templates), OID_AUTO,
	    parent_name, CTLFLAG_RW,
	    0, "USB CDC EEM device side template");
	SYSCTL_ADD_U16(&cdceem_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "vendor_id", CTLFLAG_RWTUN,
	    &usb_template_cdceem.idVendor, 1, "Vendor identifier");
	SYSCTL_ADD_U16(&cdceem_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "product_id", CTLFLAG_RWTUN,
	    &usb_template_cdceem.idProduct, 1, "Product identifier");
#if 0
	SYSCTL_ADD_PROC(&cdceem_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "interface", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &cdceem_interface, sizeof(cdceem_interface), usb_temp_sysctl,
	    "A", "Interface string");
	SYSCTL_ADD_PROC(&cdceem_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "configuration", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &cdceem_configuration, sizeof(cdceem_configuration), usb_temp_sysctl,
	    "A", "Configuration string");
#endif
	SYSCTL_ADD_PROC(&cdceem_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "manufacturer", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &cdceem_manufacturer, sizeof(cdceem_manufacturer), usb_temp_sysctl,
	    "A", "Manufacturer string");
	SYSCTL_ADD_PROC(&cdceem_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "product", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &cdceem_product, sizeof(cdceem_product), usb_temp_sysctl,
	    "A", "Product string");
	SYSCTL_ADD_PROC(&cdceem_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "serial_number", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &cdceem_serial_number, sizeof(cdceem_serial_number), usb_temp_sysctl,
	    "A", "Serial number string");
}

static void
cdceem_uninit(void *arg __unused)
{

	sysctl_ctx_free(&cdceem_ctx_list);
}

SYSINIT(cdceem_init, SI_SUB_LOCK, SI_ORDER_FIRST, cdceem_init, NULL);
SYSUNINIT(cdceem_uninit, SI_SUB_LOCK, SI_ORDER_FIRST, cdceem_uninit, NULL);
