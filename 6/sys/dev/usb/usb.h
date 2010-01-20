/*	$NetBSD: usb.h,v 1.69 2002/09/22 23:20:50 augustss Exp $	*/
/*	$FreeBSD$    */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _USB_H_
#define _USB_H_

#include <sys/types.h>
#include <sys/time.h>

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/ioctl.h>
#endif

#if defined(_KERNEL)
#include <dev/usb/usb_port.h>
#endif /* _KERNEL */

/* These two defines are used by usbd to autoload the usb kld */
#define USB_KLD		"usb"		/* name of usb module */
#define USB_UHUB	"usb/uhub"	/* root hub */

#define USB_STACK_VERSION 2

#define USB_MAX_DEVICES 128
#define USB_START_ADDR 0

#define USB_CONTROL_ENDPOINT 0
#define USB_MAX_ENDPOINTS 16

#define USB_FRAMES_PER_SECOND 1000

/*
 * The USB records contain some unaligned little-endian word
 * components.  The U[SG]ETW macros take care of both the alignment
 * and endian problem and should always be used to access non-byte
 * values.
 */
typedef u_int8_t uByte;
typedef u_int8_t uWord[2];
typedef u_int8_t uDWord[4];

#define USETW2(w,h,l) ((w)[0] = (u_int8_t)(l), (w)[1] = (u_int8_t)(h))

#if 1
#define UGETW(w) ((w)[0] | ((w)[1] << 8))
#define USETW(w,v) ((w)[0] = (u_int8_t)(v), (w)[1] = (u_int8_t)((v) >> 8))
#define UGETDW(w) ((w)[0] | ((w)[1] << 8) | ((w)[2] << 16) | ((w)[3] << 24))
#define USETDW(w,v) ((w)[0] = (u_int8_t)(v), \
		     (w)[1] = (u_int8_t)((v) >> 8), \
		     (w)[2] = (u_int8_t)((v) >> 16), \
		     (w)[3] = (u_int8_t)((v) >> 24))
#else
/*
 * On little-endian machines that can handle unanliged accesses
 * (e.g. i386) these macros can be replaced by the following.
 */
#define UGETW(w) (*(u_int16_t *)(w))
#define USETW(w,v) (*(u_int16_t *)(w) = (v))
#define UGETDW(w) (*(u_int32_t *)(w))
#define USETDW(w,v) (*(u_int32_t *)(w) = (v))
#endif

#define UPACKED __packed

typedef struct {
	uByte		bmRequestType;
	uByte		bRequest;
	uWord		wValue;
	uWord		wIndex;
	uWord		wLength;
} UPACKED usb_device_request_t;

#define UT_WRITE		0x00
#define UT_READ			0x80
#define UT_STANDARD		0x00
#define UT_CLASS		0x20
#define UT_VENDOR		0x40
#define UT_DEVICE		0x00
#define UT_INTERFACE		0x01
#define UT_ENDPOINT		0x02
#define UT_OTHER		0x03

#define UT_READ_DEVICE		(UT_READ  | UT_STANDARD | UT_DEVICE)
#define UT_READ_INTERFACE	(UT_READ  | UT_STANDARD | UT_INTERFACE)
#define UT_READ_ENDPOINT	(UT_READ  | UT_STANDARD | UT_ENDPOINT)
#define UT_WRITE_DEVICE		(UT_WRITE | UT_STANDARD | UT_DEVICE)
#define UT_WRITE_INTERFACE	(UT_WRITE | UT_STANDARD | UT_INTERFACE)
#define UT_WRITE_ENDPOINT	(UT_WRITE | UT_STANDARD | UT_ENDPOINT)
#define UT_READ_CLASS_DEVICE	(UT_READ  | UT_CLASS | UT_DEVICE)
#define UT_READ_CLASS_INTERFACE	(UT_READ  | UT_CLASS | UT_INTERFACE)
#define UT_READ_CLASS_OTHER	(UT_READ  | UT_CLASS | UT_OTHER)
#define UT_READ_CLASS_ENDPOINT	(UT_READ  | UT_CLASS | UT_ENDPOINT)
#define UT_WRITE_CLASS_DEVICE	(UT_WRITE | UT_CLASS | UT_DEVICE)
#define UT_WRITE_CLASS_INTERFACE (UT_WRITE | UT_CLASS | UT_INTERFACE)
#define UT_WRITE_CLASS_OTHER	(UT_WRITE | UT_CLASS | UT_OTHER)
#define UT_WRITE_CLASS_ENDPOINT	(UT_WRITE | UT_CLASS | UT_ENDPOINT)
#define UT_READ_VENDOR_DEVICE	(UT_READ  | UT_VENDOR | UT_DEVICE)
#define UT_READ_VENDOR_INTERFACE (UT_READ  | UT_VENDOR | UT_INTERFACE)
#define UT_READ_VENDOR_OTHER	(UT_READ  | UT_VENDOR | UT_OTHER)
#define UT_READ_VENDOR_ENDPOINT	(UT_READ  | UT_VENDOR | UT_ENDPOINT)
#define UT_WRITE_VENDOR_DEVICE	(UT_WRITE | UT_VENDOR | UT_DEVICE)
#define UT_WRITE_VENDOR_INTERFACE (UT_WRITE | UT_VENDOR | UT_INTERFACE)
#define UT_WRITE_VENDOR_OTHER	(UT_WRITE | UT_VENDOR | UT_OTHER)
#define UT_WRITE_VENDOR_ENDPOINT (UT_WRITE | UT_VENDOR | UT_ENDPOINT)

/* Requests */
#define UR_GET_STATUS		0x00
#define UR_CLEAR_FEATURE	0x01
#define UR_SET_FEATURE		0x03
#define UR_SET_ADDRESS		0x05
#define UR_GET_DESCRIPTOR	0x06
#define  UDESC_DEVICE		0x01
#define  UDESC_CONFIG		0x02
#define  UDESC_STRING		0x03
#define  UDESC_INTERFACE	0x04
#define  UDESC_ENDPOINT		0x05
#define  UDESC_DEVICE_QUALIFIER	0x06
#define  UDESC_OTHER_SPEED_CONFIGURATION 0x07
#define  UDESC_INTERFACE_POWER	0x08
#define  UDESC_OTG		0x09
#define  UDESC_CS_DEVICE	0x21	/* class specific */
#define  UDESC_CS_CONFIG	0x22
#define  UDESC_CS_STRING	0x23
#define  UDESC_CS_INTERFACE	0x24
#define  UDESC_CS_ENDPOINT	0x25
#define  UDESC_HUB		0x29
#define UR_SET_DESCRIPTOR	0x07
#define UR_GET_CONFIG		0x08
#define UR_SET_CONFIG		0x09
#define UR_GET_INTERFACE	0x0a
#define UR_SET_INTERFACE	0x0b
#define UR_SYNCH_FRAME		0x0c

/* Feature numbers */
#define UF_ENDPOINT_HALT	0
#define UF_DEVICE_REMOTE_WAKEUP	1
#define UF_TEST_MODE		2

#define USB_MAX_IPACKET		8 /* maximum size of the initial packet */

#define USB_2_MAX_CTRL_PACKET	64
#define USB_2_MAX_BULK_PACKET	512

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
} UPACKED usb_descriptor_t;

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uWord		bcdUSB;
#define UD_USB_2_0		0x0200
#define UD_IS_USB2(d) (UGETW((d)->bcdUSB) >= UD_USB_2_0)
	uByte		bDeviceClass;
	uByte		bDeviceSubClass;
	uByte		bDeviceProtocol;
	uByte		bMaxPacketSize;
	/* The fields below are not part of the initial descriptor. */
	uWord		idVendor;
	uWord		idProduct;
	uWord		bcdDevice;
	uByte		iManufacturer;
	uByte		iProduct;
	uByte		iSerialNumber;
	uByte		bNumConfigurations;
} UPACKED usb_device_descriptor_t;
#define USB_DEVICE_DESCRIPTOR_SIZE 18

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uWord		wTotalLength;
	uByte		bNumInterface;
	uByte		bConfigurationValue;
	uByte		iConfiguration;
	uByte		bmAttributes;
#define UC_BUS_POWERED		0x80
#define UC_SELF_POWERED		0x40
#define UC_REMOTE_WAKEUP	0x20
	uByte		bMaxPower; /* max current in 2 mA units */
#define UC_POWER_FACTOR 2
} UPACKED usb_config_descriptor_t;
#define USB_CONFIG_DESCRIPTOR_SIZE 9

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bInterfaceNumber;
	uByte		bAlternateSetting;
	uByte		bNumEndpoints;
	uByte		bInterfaceClass;
	uByte		bInterfaceSubClass;
	uByte		bInterfaceProtocol;
	uByte		iInterface;
} UPACKED usb_interface_descriptor_t;
#define USB_INTERFACE_DESCRIPTOR_SIZE 9

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bEndpointAddress;
#define UE_GET_DIR(a)	((a) & 0x80)
#define UE_SET_DIR(a,d)	((a) | (((d)&1) << 7))
#define UE_DIR_IN	0x80
#define UE_DIR_OUT	0x00
#define UE_ADDR		0x0f
#define UE_GET_ADDR(a)	((a) & UE_ADDR)
	uByte		bmAttributes;
#define UE_XFERTYPE	0x03
#define  UE_CONTROL	0x00
#define  UE_ISOCHRONOUS	0x01
#define  UE_BULK	0x02
#define  UE_INTERRUPT	0x03
#define UE_GET_XFERTYPE(a)	((a) & UE_XFERTYPE)
#define UE_ISO_TYPE	0x0c
#define  UE_ISO_ASYNC	0x04
#define  UE_ISO_ADAPT	0x08
#define  UE_ISO_SYNC	0x0c
#define UE_GET_ISO_TYPE(a)	((a) & UE_ISO_TYPE)
	uWord		wMaxPacketSize;
	uByte		bInterval;
} UPACKED usb_endpoint_descriptor_t;
#define USB_ENDPOINT_DESCRIPTOR_SIZE 7

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uWord		bString[127];
} UPACKED usb_string_descriptor_t;
#define USB_MAX_STRING_LEN 128
#define USB_LANGUAGE_TABLE 0	/* # of the string language id table */

/* Hub specific request */
#define UR_GET_BUS_STATE	0x02
#define UR_CLEAR_TT_BUFFER	0x08
#define UR_RESET_TT		0x09
#define UR_GET_TT_STATE		0x0a
#define UR_STOP_TT		0x0b

/* Hub features */
#define UHF_C_HUB_LOCAL_POWER	0
#define UHF_C_HUB_OVER_CURRENT	1
#define UHF_PORT_CONNECTION	0
#define UHF_PORT_ENABLE		1
#define UHF_PORT_SUSPEND	2
#define UHF_PORT_OVER_CURRENT	3
#define UHF_PORT_RESET		4
#define UHF_PORT_POWER		8
#define UHF_PORT_LOW_SPEED	9
#define UHF_C_PORT_CONNECTION	16
#define UHF_C_PORT_ENABLE	17
#define UHF_C_PORT_SUSPEND	18
#define UHF_C_PORT_OVER_CURRENT	19
#define UHF_C_PORT_RESET	20
#define UHF_PORT_TEST		21
#define UHF_PORT_INDICATOR	22

typedef struct {
	uByte		bDescLength;
	uByte		bDescriptorType;
	uByte		bNbrPorts;
	uWord		wHubCharacteristics;
#define UHD_PWR			0x0003
#define  UHD_PWR_GANGED		0x0000
#define  UHD_PWR_INDIVIDUAL	0x0001
#define  UHD_PWR_NO_SWITCH	0x0002
#define UHD_COMPOUND		0x0004
#define UHD_OC			0x0018
#define  UHD_OC_GLOBAL		0x0000
#define  UHD_OC_INDIVIDUAL	0x0008
#define  UHD_OC_NONE		0x0010
#define UHD_TT_THINK		0x0060
#define  UHD_TT_THINK_8		0x0000
#define  UHD_TT_THINK_16	0x0020
#define  UHD_TT_THINK_24	0x0040
#define  UHD_TT_THINK_32	0x0060
#define UHD_PORT_IND		0x0080
	uByte		bPwrOn2PwrGood;	/* delay in 2 ms units */
#define UHD_PWRON_FACTOR 2
	uByte		bHubContrCurrent;
	uByte		DeviceRemovable[32]; /* max 255 ports */
#define UHD_NOT_REMOV(desc, i) \
    (((desc)->DeviceRemovable[(i)/8] >> ((i) % 8)) & 1)
	/* deprecated */ uByte		PortPowerCtrlMask[1];
} UPACKED usb_hub_descriptor_t;
#define USB_HUB_DESCRIPTOR_SIZE 9 /* includes deprecated PortPowerCtrlMask */

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uWord		bcdUSB;
	uByte		bDeviceClass;
	uByte		bDeviceSubClass;
	uByte		bDeviceProtocol;
	uByte		bMaxPacketSize0;
	uByte		bNumConfigurations;
	uByte		bReserved;
} UPACKED usb_device_qualifier_t;
#define USB_DEVICE_QUALIFIER_SIZE 10

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bmAttributes;
#define UOTG_SRP	0x01
#define UOTG_HNP	0x02
} UPACKED usb_otg_descriptor_t;

/* OTG feature selectors */
#define UOTG_B_HNP_ENABLE	3
#define UOTG_A_HNP_SUPPORT	4
#define UOTG_A_ALT_HNP_SUPPORT	5

typedef struct {
	uWord		wStatus;
/* Device status flags */
#define UDS_SELF_POWERED		0x0001
#define UDS_REMOTE_WAKEUP		0x0002
/* Endpoint status flags */
#define UES_HALT			0x0001
} UPACKED usb_status_t;

typedef struct {
	uWord		wHubStatus;
#define UHS_LOCAL_POWER			0x0001
#define UHS_OVER_CURRENT		0x0002
	uWord		wHubChange;
} UPACKED usb_hub_status_t;

typedef struct {
	uWord		wPortStatus;
#define UPS_CURRENT_CONNECT_STATUS	0x0001
#define UPS_PORT_ENABLED		0x0002
#define UPS_SUSPEND			0x0004
#define UPS_OVERCURRENT_INDICATOR	0x0008
#define UPS_RESET			0x0010
#define UPS_PORT_POWER			0x0100
#define UPS_LOW_SPEED			0x0200
#define UPS_HIGH_SPEED			0x0400
#define UPS_PORT_TEST			0x0800
#define UPS_PORT_INDICATOR		0x1000
	uWord		wPortChange;
#define UPS_C_CONNECT_STATUS		0x0001
#define UPS_C_PORT_ENABLED		0x0002
#define UPS_C_SUSPEND			0x0004
#define UPS_C_OVERCURRENT_INDICATOR	0x0008
#define UPS_C_PORT_RESET		0x0010
} UPACKED usb_port_status_t;

/* Device class codes */
#define UDCLASS_IN_INTERFACE	0x00
#define UDCLASS_COMM		0x02
#define UDCLASS_HUB		0x09
#define  UDSUBCLASS_HUB		0x00
#define  UDPROTO_FSHUB		0x00
#define  UDPROTO_HSHUBSTT	0x01
#define  UDPROTO_HSHUBMTT	0x02
#define UDCLASS_DIAGNOSTIC	0xdc
#define UDCLASS_WIRELESS	0xe0
#define  UDSUBCLASS_RF		0x01
#define   UDPROTO_BLUETOOTH	0x01
#define UDCLASS_VENDOR		0xff

/* Interface class codes */
#define UICLASS_UNSPEC		0x00

#define UICLASS_AUDIO		0x01
#define  UISUBCLASS_AUDIOCONTROL	1
#define  UISUBCLASS_AUDIOSTREAM		2
#define  UISUBCLASS_MIDISTREAM		3

#define UICLASS_CDC		0x02 /* communication */
#define	 UISUBCLASS_DIRECT_LINE_CONTROL_MODEL	1
#define  UISUBCLASS_ABSTRACT_CONTROL_MODEL	2
#define	 UISUBCLASS_TELEPHONE_CONTROL_MODEL	3
#define	 UISUBCLASS_MULTICHANNEL_CONTROL_MODEL	4
#define	 UISUBCLASS_CAPI_CONTROLMODEL		5
#define	 UISUBCLASS_ETHERNET_NETWORKING_CONTROL_MODEL 6
#define	 UISUBCLASS_ATM_NETWORKING_CONTROL_MODEL 7
#define   UIPROTO_CDC_AT			1

#define UICLASS_HID		0x03
#define  UISUBCLASS_BOOT	1
#define  UIPROTO_BOOT_KEYBOARD	1

#define UICLASS_PHYSICAL	0x05

#define UICLASS_IMAGE		0x06

#define UICLASS_PRINTER		0x07
#define  UISUBCLASS_PRINTER	1
#define  UIPROTO_PRINTER_UNI	1
#define  UIPROTO_PRINTER_BI	2
#define  UIPROTO_PRINTER_1284	3

#define UICLASS_MASS		0x08
#define  UISUBCLASS_RBC		1
#define  UISUBCLASS_SFF8020I	2
#define  UISUBCLASS_QIC157	3
#define  UISUBCLASS_UFI		4
#define  UISUBCLASS_SFF8070I	5
#define  UISUBCLASS_SCSI	6
#define  UIPROTO_MASS_CBI_I	0
#define  UIPROTO_MASS_CBI	1
#define  UIPROTO_MASS_BBB_OLD	2	/* Not in the spec anymore */
#define  UIPROTO_MASS_BBB	80	/* 'P' for the Iomega Zip drive */

#define UICLASS_HUB		0x09
#define  UISUBCLASS_HUB		0
#define  UIPROTO_FSHUB		0
#define  UIPROTO_HSHUBSTT	0 /* Yes, same as previous */
#define  UIPROTO_HSHUBMTT	1

#define UICLASS_CDC_DATA	0x0a
#define  UISUBCLASS_DATA		0
#define   UIPROTO_DATA_ISDNBRI		0x30    /* Physical iface */
#define   UIPROTO_DATA_HDLC		0x31    /* HDLC */
#define   UIPROTO_DATA_TRANSPARENT	0x32    /* Transparent */
#define   UIPROTO_DATA_Q921M		0x50    /* Management for Q921 */
#define   UIPROTO_DATA_Q921		0x51    /* Data for Q921 */
#define   UIPROTO_DATA_Q921TM		0x52    /* TEI multiplexer for Q921 */
#define   UIPROTO_DATA_V42BIS		0x90    /* Data compression */
#define   UIPROTO_DATA_Q931		0x91    /* Euro-ISDN */
#define   UIPROTO_DATA_V120		0x92    /* V.24 rate adaption */
#define   UIPROTO_DATA_CAPI		0x93    /* CAPI 2.0 commands */
#define   UIPROTO_DATA_HOST_BASED	0xfd    /* Host based driver */
#define   UIPROTO_DATA_PUF		0xfe    /* see Prot. Unit Func. Desc.*/
#define   UIPROTO_DATA_VENDOR		0xff    /* Vendor specific */

#define UICLASS_SMARTCARD	0x0b

/*#define UICLASS_FIRM_UPD	0x0c*/

#define UICLASS_SECURITY	0x0d

#define UICLASS_DIAGNOSTIC	0xdc

#define UICLASS_WIRELESS	0xe0
#define  UISUBCLASS_RF			0x01
#define   UIPROTO_BLUETOOTH		0x01

#define UICLASS_APPL_SPEC	0xfe
#define  UISUBCLASS_FIRMWARE_DOWNLOAD	1
#define  UISUBCLASS_IRDA		2
#define  UIPROTO_IRDA			0

#define UICLASS_VENDOR		0xff
#define  UISUBCLASS_XBOX360_CONTROLLER	0x5d
#define  UIPROTO_XBOX360_GAMEPAD	0x01


#define USB_HUB_MAX_DEPTH 5

/*
 * Minimum time a device needs to be powered down to go through
 * a power cycle.  XXX Are these time in the spec?
 */
#define USB_POWER_DOWN_TIME	200 /* ms */
#define USB_PORT_POWER_DOWN_TIME	100 /* ms */

#if 0
/* These are the values from the spec. */
#define USB_PORT_RESET_DELAY	10  /* ms */
#define USB_PORT_ROOT_RESET_DELAY 50  /* ms */
#define USB_PORT_RESET_RECOVERY	10  /* ms */
#define USB_PORT_POWERUP_DELAY	100 /* ms */
#define USB_SET_ADDRESS_SETTLE	2   /* ms */
#define USB_RESUME_DELAY	(20*5)  /* ms */
#define USB_RESUME_WAIT		10  /* ms */
#define USB_RESUME_RECOVERY	10  /* ms */
#define USB_EXTRA_POWER_UP_TIME	0   /* ms */
#else
/* Allow for marginal (i.e. non-conforming) devices. */
#define USB_PORT_RESET_DELAY	50  /* ms */
#define USB_PORT_ROOT_RESET_DELAY 250  /* ms */
#define USB_PORT_RESET_RECOVERY	250  /* ms */
#define USB_PORT_POWERUP_DELAY	300 /* ms */
#define USB_SET_ADDRESS_SETTLE	10  /* ms */
#define USB_RESUME_DELAY	(50*5)  /* ms */
#define USB_RESUME_WAIT		50  /* ms */
#define USB_RESUME_RECOVERY	50  /* ms */
#define USB_EXTRA_POWER_UP_TIME	20  /* ms */
#endif

#define USB_MIN_POWER		100 /* mA */
#define USB_MAX_POWER		500 /* mA */

#define USB_BUS_RESET_DELAY	100 /* ms XXX?*/


#define USB_UNCONFIG_NO 0
#define USB_UNCONFIG_INDEX (-1)

/*** ioctl() related stuff ***/

struct usb_ctl_request {
	int	ucr_addr;
	usb_device_request_t ucr_request;
	void	*ucr_data;
	int	ucr_flags;
#define USBD_SHORT_XFER_OK	0x04	/* allow short reads */
	int	ucr_actlen;		/* actual length transferred */
};

struct usb_alt_interface {
	int	uai_config_index;
	int	uai_interface_index;
	int	uai_alt_no;
};

#define USB_CURRENT_CONFIG_INDEX (-1)
#define USB_CURRENT_ALT_INDEX (-1)

struct usb_config_desc {
	int	ucd_config_index;
	usb_config_descriptor_t ucd_desc;
};

struct usb_interface_desc {
	int	uid_config_index;
	int	uid_interface_index;
	int	uid_alt_index;
	usb_interface_descriptor_t uid_desc;
};

struct usb_endpoint_desc {
	int	ued_config_index;
	int	ued_interface_index;
	int	ued_alt_index;
	int	ued_endpoint_index;
	usb_endpoint_descriptor_t ued_desc;
};

struct usb_full_desc {
	int	ufd_config_index;
	u_int	ufd_size;
	u_char	*ufd_data;
};

struct usb_string_desc {
	int	usd_string_index;
	int	usd_language_id;
	usb_string_descriptor_t usd_desc;
};

struct usb_ctl_report_desc {
	int	ucrd_size;
	u_char	ucrd_data[1024];	/* filled data size will vary */
};

typedef struct { u_int32_t cookie; } usb_event_cookie_t;

#define USB_MAX_DEVNAMES 4
#define USB_MAX_DEVNAMELEN 16
struct usb_device_info {
	u_int8_t	udi_bus;
	u_int8_t	udi_addr;	/* device address */
	usb_event_cookie_t udi_cookie;
	char		udi_product[USB_MAX_STRING_LEN];
	char		udi_vendor[USB_MAX_STRING_LEN];
	char		udi_release[8];
	u_int16_t	udi_productNo;
	u_int16_t	udi_vendorNo;
	u_int16_t	udi_releaseNo;
	u_int8_t	udi_class;
	u_int8_t	udi_subclass;
	u_int8_t	udi_protocol;
	u_int8_t	udi_config;
	u_int8_t	udi_speed;
#define USB_SPEED_LOW  1
#define USB_SPEED_FULL 2
#define USB_SPEED_HIGH 3
	int		udi_power;	/* power consumption in mA, 0 if selfpowered */
	int		udi_nports;
	char		udi_devnames[USB_MAX_DEVNAMES][USB_MAX_DEVNAMELEN];
	u_int8_t	udi_ports[16];/* hub only: addresses of devices on ports */
#define USB_PORT_ENABLED 0xff
#define USB_PORT_SUSPENDED 0xfe
#define USB_PORT_POWERED 0xfd
#define USB_PORT_DISABLED 0xfc
};

struct usb_ctl_report {
	int	ucr_report;
	u_char	ucr_data[1024];	/* filled data size will vary */
};

struct usb_device_stats {
	u_long	uds_requests[4];	/* indexed by transfer type UE_* */
};

/* Events that can be read from /dev/usb */
struct usb_event {
	int			ue_type;
#define USB_EVENT_CTRLR_ATTACH 1
#define USB_EVENT_CTRLR_DETACH 2
#define USB_EVENT_DEVICE_ATTACH 3
#define USB_EVENT_DEVICE_DETACH 4
#define USB_EVENT_DRIVER_ATTACH 5
#define USB_EVENT_DRIVER_DETACH 6
#define USB_EVENT_IS_ATTACH(n) ((n) == USB_EVENT_CTRLR_ATTACH || (n) == USB_EVENT_DEVICE_ATTACH || (n) == USB_EVENT_DRIVER_ATTACH)
#define USB_EVENT_IS_DETACH(n) ((n) == USB_EVENT_CTRLR_DETACH || (n) == USB_EVENT_DEVICE_DETACH || (n) == USB_EVENT_DRIVER_DETACH)
	struct timespec		ue_time;
	union {
		struct {
			int			ue_bus;
		} ue_ctrlr;
		struct usb_device_info		ue_device;
		struct {
			usb_event_cookie_t	ue_cookie;
			char			ue_devname[16];
		} ue_driver;
	} u;
};

/* USB controller */
#define USB_REQUEST		_IOWR('U', 1, struct usb_ctl_request)
#define USB_SETDEBUG		_IOW ('U', 2, int)
#define USB_DISCOVER		_IO  ('U', 3)
#define USB_DEVICEINFO		_IOWR('U', 4, struct usb_device_info)
#define USB_DEVICESTATS		_IOR ('U', 5, struct usb_device_stats)

/* Generic HID device */
#define USB_GET_REPORT_DESC	_IOR ('U', 21, struct usb_ctl_report_desc)
#define USB_SET_IMMED		_IOW ('U', 22, int)
#define USB_GET_REPORT		_IOWR('U', 23, struct usb_ctl_report)
#define USB_SET_REPORT		_IOW ('U', 24, struct usb_ctl_report)
#define USB_GET_REPORT_ID	_IOR ('U', 25, int)

/* Generic USB device */
#define USB_GET_CONFIG		_IOR ('U', 100, int)
#define USB_SET_CONFIG		_IOW ('U', 101, int)
#define USB_GET_ALTINTERFACE	_IOWR('U', 102, struct usb_alt_interface)
#define USB_SET_ALTINTERFACE	_IOWR('U', 103, struct usb_alt_interface)
#define USB_GET_NO_ALT		_IOWR('U', 104, struct usb_alt_interface)
#define USB_GET_DEVICE_DESC	_IOR ('U', 105, usb_device_descriptor_t)
#define USB_GET_CONFIG_DESC	_IOWR('U', 106, struct usb_config_desc)
#define USB_GET_INTERFACE_DESC	_IOWR('U', 107, struct usb_interface_desc)
#define USB_GET_ENDPOINT_DESC	_IOWR('U', 108, struct usb_endpoint_desc)
#define USB_GET_FULL_DESC	_IOWR('U', 109, struct usb_full_desc)
#define USB_GET_STRING_DESC	_IOWR('U', 110, struct usb_string_desc)
#define USB_DO_REQUEST		_IOWR('U', 111, struct usb_ctl_request)
#define USB_GET_DEVICEINFO	_IOR ('U', 112, struct usb_device_info)
#define USB_SET_SHORT_XFER	_IOW ('U', 113, int)
#define USB_SET_TIMEOUT		_IOW ('U', 114, int)
#define USB_RESET_DEVICE	_IO  ('U', 115)

/* Modem device */
#define USB_GET_CM_OVER_DATA	_IOR ('U', 130, int)
#define USB_SET_CM_OVER_DATA	_IOW ('U', 131, int)

#endif /* _USB_H_ */
