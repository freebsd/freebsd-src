/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2018 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * USB template for CDC ACM (serial), CDC ECM (network), and CDC MSC (storage).
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
#include <dev/usb/usb_ioctl.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/template/usb_template.h>
#endif		/* USB_GLOBAL_INCLUDE_FILE */

#define	MODEM_IFACE_0 0
#define	MODEM_IFACE_1 1

enum {
	MULTI_LANG_INDEX,
	MULTI_MODEM_INDEX,
	MULTI_ETH_MAC_INDEX,
	MULTI_ETH_CONTROL_INDEX,
	MULTI_ETH_DATA_INDEX,
	MULTI_STORAGE_INDEX,
	MULTI_CONFIGURATION_INDEX,
	MULTI_MANUFACTURER_INDEX,
	MULTI_PRODUCT_INDEX,
	MULTI_SERIAL_NUMBER_INDEX,
	MULTI_MAX_INDEX,
};

#define	MULTI_DEFAULT_VENDOR_ID		USB_TEMPLATE_VENDOR
#define	MULTI_DEFAULT_PRODUCT_ID	0x05dc
#define	MULTI_DEFAULT_MODEM		"Virtual serial port"
#define	MULTI_DEFAULT_ETH_MAC		"2A02030405060789AB"
#define	MULTI_DEFAULT_ETH_CONTROL	"Ethernet Comm Interface"
#define	MULTI_DEFAULT_ETH_DATA		"Ethernet Data Interface"
#define	MULTI_DEFAULT_STORAGE		"Mass Storage Interface"
#define	MULTI_DEFAULT_CONFIGURATION	"Default configuration"
#define	MULTI_DEFAULT_MANUFACTURER	USB_TEMPLATE_MANUFACTURER
#define	MULTI_DEFAULT_PRODUCT		"Multifunction Device"
/*
 * The reason for this being called like this is that OSX
 * derives the device node name from it, resulting in a somewhat
 * user-friendly "/dev/cu.usbmodemFreeBSD1".  And yes, the "1"
 * needs to be there, otherwise OSX will mangle it.
 */
#define MULTI_DEFAULT_SERIAL_NUMBER	"FreeBSD1"

static struct usb_string_descriptor	multi_modem;
static struct usb_string_descriptor	multi_eth_mac;
static struct usb_string_descriptor	multi_eth_control;
static struct usb_string_descriptor	multi_eth_data;
static struct usb_string_descriptor	multi_storage;
static struct usb_string_descriptor	multi_configuration;
static struct usb_string_descriptor	multi_manufacturer;
static struct usb_string_descriptor	multi_product;
static struct usb_string_descriptor	multi_serial_number;

static struct sysctl_ctx_list		multi_ctx_list;

/* prototypes */

static usb_temp_get_string_desc_t multi_get_string_desc;

static const struct usb_cdc_union_descriptor eth_union_desc = {
	.bLength = sizeof(eth_union_desc),
	.bDescriptorType = UDESC_CS_INTERFACE,
	.bDescriptorSubtype = UDESCSUB_CDC_UNION,
	.bMasterInterface = 0,		/* this is automatically updated */
	.bSlaveInterface[0] = 1,	/* this is automatically updated */
};

static const struct usb_cdc_header_descriptor eth_header_desc = {
	.bLength = sizeof(eth_header_desc),
	.bDescriptorType = UDESC_CS_INTERFACE,
	.bDescriptorSubtype = UDESCSUB_CDC_HEADER,
	.bcdCDC[0] = 0x10,
	.bcdCDC[1] = 0x01,
};

static const struct usb_cdc_ethernet_descriptor eth_enf_desc = {
	.bLength = sizeof(eth_enf_desc),
	.bDescriptorType = UDESC_CS_INTERFACE,
	.bDescriptorSubtype = UDESCSUB_CDC_ENF,
	.iMacAddress = MULTI_ETH_MAC_INDEX,
	.bmEthernetStatistics = {0, 0, 0, 0},
	.wMaxSegmentSize = {0xEA, 0x05},/* 1514 bytes */
	.wNumberMCFilters = {0, 0},
	.bNumberPowerFilters = 0,
};

static const void *eth_control_if_desc[] = {
	&eth_union_desc,
	&eth_header_desc,
	&eth_enf_desc,
	NULL,
};

static const struct usb_temp_packet_size bulk_mps = {
	.mps[USB_SPEED_FULL] = 64,
	.mps[USB_SPEED_HIGH] = 512,
};

static const struct usb_temp_packet_size intr_mps = {
	.mps[USB_SPEED_FULL] = 8,
	.mps[USB_SPEED_HIGH] = 8,
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

static const struct usb_temp_endpoint_desc intr_in_ep = {
	.pPacketSize = &intr_mps,
	.bEndpointAddress = UE_DIR_IN,
	.bmAttributes = UE_INTERRUPT,
};

static const struct usb_temp_endpoint_desc *eth_intr_endpoints[] = {
	&intr_in_ep,
	NULL,
};

static const struct usb_temp_interface_desc eth_control_interface = {
	.ppEndpoints = eth_intr_endpoints,
	.ppRawDesc = eth_control_if_desc,
	.bInterfaceClass = UICLASS_CDC,
	.bInterfaceSubClass = UISUBCLASS_ETHERNET_NETWORKING_CONTROL_MODEL,
	.bInterfaceProtocol = UIPROTO_CDC_NONE,
	.iInterface = MULTI_ETH_CONTROL_INDEX,
};

static const struct usb_temp_endpoint_desc *eth_data_endpoints[] = {
	&bulk_in_ep,
	&bulk_out_ep,
	NULL,
};

static const struct usb_temp_interface_desc eth_data_null_interface = {
	.ppEndpoints = NULL,		/* no endpoints */
	.bInterfaceClass = UICLASS_CDC_DATA,
	.bInterfaceSubClass = UISUBCLASS_DATA,
	.bInterfaceProtocol = 0,
	.iInterface = MULTI_ETH_DATA_INDEX,
};

static const struct usb_temp_interface_desc eth_data_interface = {
	.ppEndpoints = eth_data_endpoints,
	.bInterfaceClass = UICLASS_CDC_DATA,
	.bInterfaceSubClass = UISUBCLASS_DATA,
	.bInterfaceProtocol = 0,
	.iInterface = MULTI_ETH_DATA_INDEX,
	.isAltInterface = 1,		/* this is an alternate setting */
};

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
	.bInterfaceClass = UICLASS_CDC,
	.bInterfaceSubClass = UISUBCLASS_ABSTRACT_CONTROL_MODEL,
	.bInterfaceProtocol = UIPROTO_CDC_NONE,
	.iInterface = MULTI_MODEM_INDEX,
};

static const struct usb_temp_interface_desc modem_iface_1 = {
	.ppEndpoints = modem_iface_1_ep,
	.bInterfaceClass = UICLASS_CDC_DATA,
	.bInterfaceSubClass = UISUBCLASS_DATA,
	.bInterfaceProtocol = 0,
	.iInterface = MULTI_MODEM_INDEX,
};

static const struct usb_temp_packet_size msc_bulk_mps = {
	.mps[USB_SPEED_FULL] = 64,
	.mps[USB_SPEED_HIGH] = 512,
};

static const struct usb_temp_endpoint_desc msc_bulk_in_ep = {
	.pPacketSize = &msc_bulk_mps,
#ifdef USB_HIP_IN_EP_0
	.bEndpointAddress = USB_HIP_IN_EP_0,
#else
	.bEndpointAddress = UE_DIR_IN,
#endif
	.bmAttributes = UE_BULK,
};

static const struct usb_temp_endpoint_desc msc_bulk_out_ep = {
	.pPacketSize = &msc_bulk_mps,
#ifdef USB_HIP_OUT_EP_0
	.bEndpointAddress = USB_HIP_OUT_EP_0,
#else
	.bEndpointAddress = UE_DIR_OUT,
#endif
	.bmAttributes = UE_BULK,
};

static const struct usb_temp_endpoint_desc *msc_data_endpoints[] = {
	&msc_bulk_in_ep,
	&msc_bulk_out_ep,
	NULL,
};

static const struct usb_temp_interface_desc msc_data_interface = {
	.ppEndpoints = msc_data_endpoints,
	.bInterfaceClass = UICLASS_MASS,
	.bInterfaceSubClass = UISUBCLASS_SCSI,
	.bInterfaceProtocol = UIPROTO_MASS_BBB,
	.iInterface = MULTI_STORAGE_INDEX,
};

static const struct usb_temp_interface_desc *multi_interfaces[] = {
	&modem_iface_0,
	&modem_iface_1,
	&eth_control_interface,
	&eth_data_null_interface,
	&eth_data_interface,
	&msc_data_interface,
	NULL,
};

static const struct usb_temp_config_desc multi_config_desc = {
	.ppIfaceDesc = multi_interfaces,
	.bmAttributes = 0,
	.bMaxPower = 0,
	.iConfiguration = MULTI_CONFIGURATION_INDEX,
};
static const struct usb_temp_config_desc *multi_configs[] = {
	&multi_config_desc,
	NULL,
};

struct usb_temp_device_desc usb_template_multi = {
	.getStringDesc = &multi_get_string_desc,
	.ppConfigDesc = multi_configs,
	.idVendor = MULTI_DEFAULT_VENDOR_ID,
	.idProduct = MULTI_DEFAULT_PRODUCT_ID,
	.bcdDevice = 0x0100,
	.bDeviceClass = UDCLASS_IN_INTERFACE,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.iManufacturer = MULTI_MANUFACTURER_INDEX,
	.iProduct = MULTI_PRODUCT_INDEX,
	.iSerialNumber = MULTI_SERIAL_NUMBER_INDEX,
};

/*------------------------------------------------------------------------*
 *	multi_get_string_desc
 *
 * Return values:
 * NULL: Failure. No such string.
 * Else: Success. Pointer to string descriptor is returned.
 *------------------------------------------------------------------------*/
static const void *
multi_get_string_desc(uint16_t lang_id, uint8_t string_index)
{
	static const void *ptr[MULTI_MAX_INDEX] = {
		[MULTI_LANG_INDEX] = &usb_string_lang_en,
		[MULTI_MODEM_INDEX] = &multi_modem,
		[MULTI_ETH_MAC_INDEX] = &multi_eth_mac,
		[MULTI_ETH_CONTROL_INDEX] = &multi_eth_control,
		[MULTI_ETH_DATA_INDEX] = &multi_eth_data,
		[MULTI_STORAGE_INDEX] = &multi_storage,
		[MULTI_CONFIGURATION_INDEX] = &multi_configuration,
		[MULTI_MANUFACTURER_INDEX] = &multi_manufacturer,
		[MULTI_PRODUCT_INDEX] = &multi_product,
		[MULTI_SERIAL_NUMBER_INDEX] = &multi_serial_number,
	};

	if (string_index == 0) {
		return (&usb_string_lang_en);
	}
	if (lang_id != 0x0409) {
		return (NULL);
	}
	if (string_index < MULTI_MAX_INDEX) {
		return (ptr[string_index]);
	}
	return (NULL);
}

static void
multi_init(void *arg __unused)
{
	struct sysctl_oid *parent;
	char parent_name[3];

	usb_make_str_desc(&multi_modem, sizeof(multi_modem),
	    MULTI_DEFAULT_MODEM);
	usb_make_str_desc(&multi_eth_mac, sizeof(multi_eth_mac),
	    MULTI_DEFAULT_ETH_MAC);
	usb_make_str_desc(&multi_eth_control, sizeof(multi_eth_control),
	    MULTI_DEFAULT_ETH_CONTROL);
	usb_make_str_desc(&multi_eth_data, sizeof(multi_eth_data),
	    MULTI_DEFAULT_ETH_DATA);
	usb_make_str_desc(&multi_storage, sizeof(multi_storage),
	    MULTI_DEFAULT_STORAGE);
	usb_make_str_desc(&multi_configuration, sizeof(multi_configuration),
	    MULTI_DEFAULT_CONFIGURATION);
	usb_make_str_desc(&multi_manufacturer, sizeof(multi_manufacturer),
	    MULTI_DEFAULT_MANUFACTURER);
	usb_make_str_desc(&multi_product, sizeof(multi_product),
	    MULTI_DEFAULT_PRODUCT);
	usb_make_str_desc(&multi_serial_number, sizeof(multi_serial_number),
	    MULTI_DEFAULT_SERIAL_NUMBER);

	snprintf(parent_name, sizeof(parent_name), "%d", USB_TEMP_MULTI);
	sysctl_ctx_init(&multi_ctx_list);

	parent = SYSCTL_ADD_NODE(&multi_ctx_list,
	    SYSCTL_STATIC_CHILDREN(_hw_usb_templates), OID_AUTO,
	    parent_name, CTLFLAG_RW | CTLFLAG_MPSAFE,
	    0, "USB Multifunction device side template");
	SYSCTL_ADD_U16(&multi_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "vendor_id", CTLFLAG_RWTUN,
	    &usb_template_multi.idVendor, 1, "Vendor identifier");
	SYSCTL_ADD_U16(&multi_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "product_id", CTLFLAG_RWTUN,
	    &usb_template_multi.idProduct, 1, "Product identifier");
	SYSCTL_ADD_PROC(&multi_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "eth_mac", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &multi_eth_mac, sizeof(multi_eth_mac), usb_temp_sysctl,
	    "A", "Ethernet MAC address string");
#if 0
	SYSCTL_ADD_PROC(&multi_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "modem", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &multi_modem, sizeof(multi_modem), usb_temp_sysctl,
	    "A", "Modem interface string");
	SYSCTL_ADD_PROC(&multi_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "eth_control", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &multi_eth_control, sizeof(multi_eth_data), usb_temp_sysctl,
	    "A", "Ethernet control interface string");
	SYSCTL_ADD_PROC(&multi_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "eth_data", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &multi_eth_data, sizeof(multi_eth_data), usb_temp_sysctl,
	    "A", "Ethernet data interface string");
	SYSCTL_ADD_PROC(&multi_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "interface", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &multi_storage, sizeof(multi_storage), usb_temp_sysctl,
	    "A", "Storage interface string");
	SYSCTL_ADD_PROC(&multi_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "configuration", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &multi_configuration, sizeof(multi_configuration), usb_temp_sysctl,
	    "A", "Configuration string");
#endif
	SYSCTL_ADD_PROC(&multi_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "manufacturer", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &multi_manufacturer, sizeof(multi_manufacturer), usb_temp_sysctl,
	    "A", "Manufacturer string");
	SYSCTL_ADD_PROC(&multi_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "product", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &multi_product, sizeof(multi_product), usb_temp_sysctl,
	    "A", "Product string");
	SYSCTL_ADD_PROC(&multi_ctx_list, SYSCTL_CHILDREN(parent), OID_AUTO,
	    "serial_number", CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &multi_serial_number, sizeof(multi_serial_number), usb_temp_sysctl,
	    "A", "Serial number string");
}

static void
multi_uninit(void *arg __unused)
{

	sysctl_ctx_free(&multi_ctx_list);
}

SYSINIT(multi_init, SI_SUB_LOCK, SI_ORDER_FIRST, multi_init, NULL);
SYSUNINIT(multi_uninit, SI_SUB_LOCK, SI_ORDER_FIRST, multi_uninit, NULL);
