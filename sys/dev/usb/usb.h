/*	$NetBSD: usb.h,v 1.38 1999/10/20 21:02:39 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
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

#if defined(_KERNEL)
#include <dev/usb/usb_port.h>
#endif /* _KERNEL */

#elif defined(__FreeBSD__)
#if defined(_KERNEL)
#include <sys/malloc.h>

MALLOC_DECLARE(M_USB);
MALLOC_DECLARE(M_USBDEV);
MALLOC_DECLARE(M_USBHC);

#include <dev/usb/usb_port.h>
#endif /* _KERNEL */
#endif /* __FreeBSD__ */

/* these three defines are used by usbd to autoload the usb kld */
#define USB_KLD		"usb"
#define USB_OHCI	"ohci/usb"
#define USB_UHCI	"uhci/usb"

#define USB_MAX_DEVICES 128
#define USB_START_ADDR 0

#define USB_CONTROL_ENDPOINT 0
#define USB_MAX_ENDPOINTS 16

#define USB_FRAMES_PER_SECOND 1000

/*
 * The USB records contain some unaligned little-endian word
 * components.  The U[SG]ETW macros take care of both the alignment
 * and endian problem and should always be used to access 16 bit
 * values.
 */
typedef u_int8_t uByte;
typedef u_int8_t uWord[2];
#define UGETW(w) ((w)[0] | ((w)[1] << 8))
#define USETW(w,v) ((w)[0] = (u_int8_t)(v), (w)[1] = (u_int8_t)((v) >> 8))
#define USETW2(w,h,l) ((w)[0] = (u_int8_t)(l), (w)[1] = (u_int8_t)(h))
typedef u_int8_t uDWord[4];
#define UGETDW(w) ((w)[0] | ((w)[1] << 8) | ((w)[2] << 16) | ((w)[3] << 24))
#define USETDW(w,v) ((w)[0] = (u_int8_t)(v), \
		     (w)[1] = (u_int8_t)((v) >> 8), \
		     (w)[2] = (u_int8_t)((v) >> 16), \
		     (w)[3] = (u_int8_t)((v) >> 24))
/* 
 * On little-endian machines that can handle unanliged accesses
 * (e.g. i386) these macros can be replaced by the following.
 */
#if 0
#define UGETW(w) (*(u_int16_t *)(w))
#define USETW(w,v) (*(u_int16_t *)(w) = (v))
#endif

typedef struct {
	uByte		bmRequestType;
	uByte		bRequest;
	uWord		wValue;
	uWord		wIndex;
	uWord		wLength;
} usb_device_request_t;

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

#define USB_MAX_IPACKET		8 /* maximum size of the initial packet */

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
} usb_descriptor_t;

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uWord		bcdUSB;
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
} usb_device_descriptor_t;
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
} usb_config_descriptor_t;
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
} usb_interface_descriptor_t;
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
} usb_endpoint_descriptor_t;
#define USB_ENDPOINT_DESCRIPTOR_SIZE 7

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uWord		bString[127];
} usb_string_descriptor_t;
#define USB_MAX_STRING_LEN 128
#define USB_LANGUAGE_TABLE 0	/* # of the string language id table */

/* Hub specific request */
#define UR_GET_BUS_STATE	0x02

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

typedef struct {
	uByte		bDescLength;
	uByte		bDescriptorType;
	uByte		bNbrPorts;
	uWord		wHubCharacteristics;
#define UHD_PWR			0x03
#define UHD_PWR_GANGED		0x00
#define UHD_PWR_INDIVIDUAL	0x01
#define UHD_PWR_NO_SWITCH	0x02
#define UHD_COMPOUND		0x04
#define UHD_OC			0x18
#define UHD_OC_GLOBAL		0x00
#define UHD_OC_INDIVIDUAL	0x08
#define UHD_OC_NONE		0x10
	uByte		bPwrOn2PwrGood;	/* delay in 2 ms units */
#define UHD_PWRON_FACTOR 2
	uByte		bHubContrCurrent;
	uByte		DeviceRemovable[32]; /* max 255 ports */
#define UHD_NOT_REMOV(desc, i) \
    (((desc)->DeviceRemovable[(i)/8] >> ((i) % 8)) & 1)
	/* deprecated uByte		PortPowerCtrlMask[]; */
} usb_hub_descriptor_t;
#define USB_HUB_DESCRIPTOR_SIZE 8

typedef struct {
	uWord		wStatus;
/* Device status flags */
#define UDS_SELF_POWERED		0x0001
#define UDS_REMOTE_WAKEUP		0x0002
/* Endpoint status flags */
#define UES_HALT			0x0001
} usb_status_t;

typedef struct {
	uWord		wHubStatus;
#define UHS_LOCAL_POWER			0x0001
#define UHS_OVER_CURRENT		0x0002
	uWord		wHubChange;
} usb_hub_status_t;

typedef struct {
	uWord		wPortStatus;
#define UPS_CURRENT_CONNECT_STATUS	0x0001
#define UPS_PORT_ENABLED		0x0002
#define UPS_SUSPEND			0x0004
#define UPS_OVERCURRENT_INDICATOR	0x0008
#define UPS_RESET			0x0010
#define UPS_PORT_POWER			0x0100
#define UPS_LOW_SPEED			0x0200
	uWord		wPortChange;
#define UPS_C_CONNECT_STATUS		0x0001
#define UPS_C_PORT_ENABLED		0x0002
#define UPS_C_SUSPEND			0x0004
#define UPS_C_OVERCURRENT_INDICATOR	0x0008
#define UPS_C_PORT_RESET		0x0010
} usb_port_status_t;

#define UCLASS_UNSPEC		0
#define UCLASS_AUDIO		1
#define  USUBCLASS_AUDIOCONTROL	1
#define  USUBCLASS_AUDIOSTREAM	2
#define  USUBCLASS_MIDISTREAM	3
#define UCLASS_CDC		2 /* communication */
#define	 USUBCLASS_DIRECT_LINE_CONTROL_MODEL	1
#define  USUBCLASS_ABSTRACT_CONTROL_MODEL	2
#define	 USUBCLASS_TELEPHONE_CONTROL_MODEL	3
#define	 USUBCLASS_MULTICHANNEL_CONTROL_MODEL	4
#define	 USUBCLASS_CAPI_CONTROLMODEL		5
#define	 USUBCLASS_ETHERNET_NETWORKING_CONTROL_MODEL 6
#define	 USUBCLASS_ATM_NETWORKING_CONTROL_MODEL	7
#define   UPROTO_CDC_AT		1
#define UCLASS_HID		3
#define  USUBCLASS_BOOT	 	1
#define UCLASS_PRINTER		7
#define  USUBCLASS_PRINTER	1
#define  UPROTO_PRINTER_UNI	1
#define  UPROTO_PRINTER_BI	2
#define  UPROTO_PRINTER_1284	3
#define UCLASS_MASS		8
#define  USUBCLASS_RBC		1
#define  USUBCLASS_SFF8020I	2
#define  USUBCLASS_QIC157	3
#define  USUBCLASS_UFI		4
#define  USUBCLASS_SFF8070I	5
#define  USUBCLASS_SCSI		6
#define  UPROTO_MASS_CBI_I	0
#define  UPROTO_MASS_CBI	1
#define  UPROTO_MASS_BBB	2
#define  UPROTO_MASS_BBB_P	80	/* 'P' for the Iomega Zip drive */
#define UCLASS_HUB		9
#define  USUBCLASS_HUB		0
#define UCLASS_DATA		10
#define  USUBCLASS_DATA		0
#define   UPROTO_DATA_ISDNBRI		0x30    /* Physical iface */
#define   UPROTO_DATA_HDLC		0x31    /* HDLC */
#define   UPROTO_DATA_TRANSPARENT	0x32    /* Transparent */
#define   UPROTO_DATA_Q921M		0x50    /* Management for Q921 */
#define   UPROTO_DATA_Q921		0x51    /* Data for Q921 */
#define   UPROTO_DATA_Q921TM		0x52    /* TEI multiplexer for Q921 */
#define   UPROTO_DATA_V42BIS		0x90    /* Data compression */  
#define   UPROTO_DATA_Q931		0x91    /* Euro-ISDN */
#define   UPROTO_DATA_V120		0x92    /* V.24 rate adaption */
#define   UPROTO_DATA_CAPI		0x93    /* CAPI 2.0 commands */
#define   UPROTO_DATA_HOST_BASED	0xfd    /* Host based driver */
#define   UPROTO_DATA_PUF		0xfe    /* see Prot. Unit Func. Desc.*/
#define   UPROTO_DATA_VENDOR		0xff    /* Vendor specific */


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
#define USB_PORT_RESET_SETTLE	10  /* ms */
#define USB_PORT_POWERUP_DELAY	100 /* ms */
#define USB_SET_ADDRESS_SETTLE	2   /* ms */
#define USB_RESUME_TIME		(20*5)  /* ms */
#define USB_RESUME_WAIT		10  /* ms */
#define USB_RESUME_RECOVERY	10  /* ms */
#define USB_EXTRA_POWER_UP_TIME	0   /* ms */
#else
/* Allow for marginal (i.e. non-conforming) devices. */
#define USB_PORT_RESET_DELAY	50  /* ms */
#define USB_PORT_RESET_RECOVERY	50  /* ms */
#define USB_PORT_POWERUP_DELAY	200 /* ms */
#define USB_SET_ADDRESS_SETTLE	10  /* ms */
#define USB_RESUME_DELAY	(50*5)  /* ms */
#define USB_RESUME_WAIT		50  /* ms */
#define USB_RESUME_RECOVERY	50  /* ms */
#define USB_EXTRA_POWER_UP_TIME	20  /* ms */
#endif

#define USB_MIN_POWER		100 /* mA */
#define USB_MAX_POWER		500 /* mA */

#define USB_BUS_RESET_DELAY	100 /* ms XXX?*/

#define USB_UNCONFIG_NO		0
#define USB_UNCONFIG_INDEX	(-1)

/*** ioctl() related stuff ***/

struct usb_ctl_request {
	int	addr;
	usb_device_request_t request;
	void	*data;
	int	flags;
#define USBD_SHORT_XFER_OK	0x04	/* allow short reads */
	int	actlen;		/* actual length transferred */
};

struct usb_alt_interface {
	int	config_index;
	int	interface_index;
	int	alt_no;
};

#define USB_CURRENT_CONFIG_INDEX (-1)
#define USB_CURRENT_ALT_INDEX (-1)

struct usb_config_desc {
	int	config_index;
	usb_config_descriptor_t desc;
};

struct usb_interface_desc {
	int	config_index;
	int	interface_index;
	int	alt_index;
	usb_interface_descriptor_t desc;
};

struct usb_endpoint_desc {
	int	config_index;
	int	interface_index;
	int	alt_index;
	int	endpoint_index;
	usb_endpoint_descriptor_t desc;
};

struct usb_full_desc {
	int	config_index;
	u_int	size;
	u_char	*data;
};

struct usb_string_desc {
	int	string_index;
	int	language_id;
	usb_string_descriptor_t desc;
};

struct usb_ctl_report_desc {
	int	size;
	u_char	data[1024];	/* filled data size will vary */
};

struct usb_device_info {
	u_int8_t	bus;				/* bus number */
	u_int8_t	addr;				/* device address */
#	define		MAXDEVNAMELEN	10		/* number of drivers */
#	define		MAXDEVNAMES	4		/* attached drivers */
	char		devnames[MAXDEVNAMES][MAXDEVNAMELEN];
							/* device names */
	char		product[USB_MAX_STRING_LEN];	/* iProduct */
	char		vendor[USB_MAX_STRING_LEN];	/* iManufacturer */
	char		release[8];			/* string of releaseNo*/
	u_int16_t	productNo;			/* idProduct */
	u_int16_t	vendorNo;			/* idVendor */
	u_int16_t	releaseNo;			/* bcdDevice */
	u_int8_t	class;				/* bDeviceClass */
	u_int8_t	subclass;			/* bDeviceSubclass */
	u_int8_t	protocol;			/* bDeviceProtocol */
	u_int8_t	config;				/* config index */
	u_int8_t	lowspeed;			/* lowsped yes/no */
	int		power;	/* power consumption in mA, 0 if selfpowered */
	int		nports;				/* 0 if not hub */
	u_int8_t	ports[16];/* hub only: addresses of devices on ports */
#define USB_PORT_ENABLED 0xff
#define USB_PORT_SUSPENDED 0xfe
#define USB_PORT_POWERED 0xfd
#define USB_PORT_DISABLED 0xfc
};

struct usb_ctl_report {
	int	report;
	u_char	data[1024];	/* filled data size will vary */
};

struct usb_device_stats {
	u_long	requests[4];	/* indexed by transfer type UE_* */
};

typedef struct { u_int32_t cookie; } usb_event_cookie_t;
/* Events that can be read from /dev/usb */
struct usb_event {
	int			ue_type;
#define USB_EVENT_ATTACH 1
#define USB_EVENT_DETACH 2
	struct usb_device_info	ue_device;
	struct timespec		ue_time;
	usb_event_cookie_t	ue_cookie;
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

/* Modem device */
#define USB_GET_CM_OVER_DATA	_IOR ('U', 130, int)
#define USB_SET_CM_OVER_DATA	_IOW ('U', 131, int)

#endif /* _USB_H_ */
