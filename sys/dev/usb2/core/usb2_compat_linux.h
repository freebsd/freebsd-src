/* $FreeBSD$ */
/*-
 * Copyright (c) 2007 Luigi Rizzo - Universita` di Pisa. All rights reserved.
 * Copyright (c) 2007 Hans Petter Selasky. All rights reserved.
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

#ifndef _USB_COMPAT_LINUX_H
#define	_USB_COMPAT_LINUX_H

struct usb_device;
struct usb_interface;
struct usb_driver;
struct urb;

typedef void *pm_message_t;
typedef void (usb_complete_t)(struct urb *);

#define	USB_MAX_FULL_SPEED_ISOC_FRAMES (60 * 1)
#define	USB_MAX_HIGH_SPEED_ISOC_FRAMES (60 * 8)

/*
 * Linux compatible USB device drivers put their device information
 * into the "usb_device_id" structure using the "USB_DEVICE()" macro.
 * The "MODULE_DEVICE_TABLE()" macro can be used to export this
 * information to userland.
 */
struct usb_device_id {
	/* which fields to match against */
	uint16_t match_flags;
#define	USB_DEVICE_ID_MATCH_VENDOR		0x0001
#define	USB_DEVICE_ID_MATCH_PRODUCT		0x0002
#define	USB_DEVICE_ID_MATCH_DEV_LO		0x0004
#define	USB_DEVICE_ID_MATCH_DEV_HI		0x0008
#define	USB_DEVICE_ID_MATCH_DEV_CLASS		0x0010
#define	USB_DEVICE_ID_MATCH_DEV_SUBCLASS	0x0020
#define	USB_DEVICE_ID_MATCH_DEV_PROTOCOL	0x0040
#define	USB_DEVICE_ID_MATCH_INT_CLASS		0x0080
#define	USB_DEVICE_ID_MATCH_INT_SUBCLASS	0x0100
#define	USB_DEVICE_ID_MATCH_INT_PROTOCOL	0x0200

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

	/* Hook for driver specific information */
	unsigned long driver_info;
};

#define	USB_DEVICE_ID_MATCH_DEVICE \
	(USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT)

#define	USB_DEVICE(vend,prod) \
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE, .idVendor = (vend), \
	.idProduct = (prod)

/* The "usb_driver" structure holds the Linux USB device driver
 * callbacks, and a pointer to device ID's which this entry should
 * match against. Usually this entry is exposed to the USB emulation
 * layer using the "USB_DRIVER_EXPORT()" macro, which is defined
 * below.
 */
struct usb_driver {
	const char *name;

	int     (*probe) (struct usb_interface *intf,
	    	const	struct usb_device_id *id);

	void    (*disconnect) (struct usb_interface *intf);

	int     (*ioctl) (struct usb_interface *intf, unsigned int code,
	    	void  *buf);

	int     (*suspend) (struct usb_interface *intf, pm_message_t message);
	int     (*resume) (struct usb_interface *intf);

	const struct usb_device_id *id_table;

	void    (*shutdown) (struct usb_interface *intf);

	LIST_ENTRY(usb_driver) linux_driver_list;
};

#define	USB_DRIVER_EXPORT(id,p_usb_drv) \
  SYSINIT(id,SI_SUB_KLD,SI_ORDER_FIRST,usb_linux_register,p_usb_drv); \
  SYSUNINIT(id,SI_SUB_KLD,SI_ORDER_ANY,usb_linux_deregister,p_usb_drv)

/*
 * The following structure is the same as "usb_device_descriptor_t"
 * except that 16-bit values are "uint16_t" and not an array of "uint8_t".
 * It is used by Linux USB device drivers.
 */
struct usb_device_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;

	uint16_t bcdUSB;
	uint8_t	bDeviceClass;
	uint8_t	bDeviceSubClass;
	uint8_t	bDeviceProtocol;
	uint8_t	bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t	iManufacturer;
	uint8_t	iProduct;
	uint8_t	iSerialNumber;
	uint8_t	bNumConfigurations;
} __packed;

/*
 * The following structure is the same as
 * "usb_interface_descriptor_t". It is used by
 * Linux USB device drivers.
 */
struct usb_interface_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;

	uint8_t	bInterfaceNumber;
	uint8_t	bAlternateSetting;
	uint8_t	bNumEndpoints;
	uint8_t	bInterfaceClass;
	uint8_t	bInterfaceSubClass;
	uint8_t	bInterfaceProtocol;
	uint8_t	iInterface;
} __packed;

/*
 * The following structure is the same as "usb_endpoint_descriptor_t"
 * except that 16-bit values are "uint16_t" and not an array of "uint8_t".
 * It is used by Linux USB device drivers.
 */
struct usb_endpoint_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;

	uint8_t	bEndpointAddress;
	uint8_t	bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t	bInterval;

	/* extension for audio endpoints only: */
	uint8_t	bRefresh;
	uint8_t	bSynchAddress;
} __packed;

#define	USB_DT_ENDPOINT_SIZE		7
#define	USB_DT_ENDPOINT_AUDIO_SIZE	9

/*
 * Endpoints
 */
#define	USB_ENDPOINT_NUMBER_MASK	0x0f	/* in bEndpointAddress */
#define	USB_ENDPOINT_DIR_MASK		0x80

#define	USB_ENDPOINT_XFERTYPE_MASK	0x03	/* in bmAttributes */
#define	USB_ENDPOINT_XFER_CONTROL	0
#define	USB_ENDPOINT_XFER_ISOC		1
#define	USB_ENDPOINT_XFER_BULK		2
#define	USB_ENDPOINT_XFER_INT		3
#define	USB_ENDPOINT_MAX_ADJUSTABLE	0x80

/* CONTROL REQUEST SUPPORT */

/*
 * Definition of direction mask for
 * "bEndpointAddress" and "bmRequestType":
 */
#define	USB_DIR_MASK			0x80
#define	USB_DIR_OUT			0x00	/* write to USB device */
#define	USB_DIR_IN			0x80	/* read from USB device */

/*
 * Definition of type mask for
 * "bmRequestType":
 */
#define	USB_TYPE_MASK			(0x03 << 5)
#define	USB_TYPE_STANDARD		(0x00 << 5)
#define	USB_TYPE_CLASS			(0x01 << 5)
#define	USB_TYPE_VENDOR			(0x02 << 5)
#define	USB_TYPE_RESERVED		(0x03 << 5)

/*
 * Definition of receiver mask for
 * "bmRequestType":
 */
#define	USB_RECIP_MASK			0x1f
#define	USB_RECIP_DEVICE		0x00
#define	USB_RECIP_INTERFACE		0x01
#define	USB_RECIP_ENDPOINT		0x02
#define	USB_RECIP_OTHER			0x03

/*
 * Definition of standard request values for
 * "bRequest":
 */
#define	USB_REQ_GET_STATUS		0x00
#define	USB_REQ_CLEAR_FEATURE		0x01
#define	USB_REQ_SET_FEATURE		0x03
#define	USB_REQ_SET_ADDRESS		0x05
#define	USB_REQ_GET_DESCRIPTOR		0x06
#define	USB_REQ_SET_DESCRIPTOR		0x07
#define	USB_REQ_GET_CONFIGURATION	0x08
#define	USB_REQ_SET_CONFIGURATION	0x09
#define	USB_REQ_GET_INTERFACE		0x0A
#define	USB_REQ_SET_INTERFACE		0x0B
#define	USB_REQ_SYNCH_FRAME		0x0C

#define	USB_REQ_SET_ENCRYPTION		0x0D	/* Wireless USB */
#define	USB_REQ_GET_ENCRYPTION		0x0E
#define	USB_REQ_SET_HANDSHAKE		0x0F
#define	USB_REQ_GET_HANDSHAKE		0x10
#define	USB_REQ_SET_CONNECTION		0x11
#define	USB_REQ_SET_SECURITY_DATA	0x12
#define	USB_REQ_GET_SECURITY_DATA	0x13
#define	USB_REQ_SET_WUSB_DATA		0x14
#define	USB_REQ_LOOPBACK_DATA_WRITE	0x15
#define	USB_REQ_LOOPBACK_DATA_READ	0x16
#define	USB_REQ_SET_INTERFACE_DS	0x17

/*
 * USB feature flags are written using USB_REQ_{CLEAR,SET}_FEATURE, and
 * are read as a bit array returned by USB_REQ_GET_STATUS.  (So there
 * are at most sixteen features of each type.)
 */
#define	USB_DEVICE_SELF_POWERED		0	/* (read only) */
#define	USB_DEVICE_REMOTE_WAKEUP	1	/* dev may initiate wakeup */
#define	USB_DEVICE_TEST_MODE		2	/* (wired high speed only) */
#define	USB_DEVICE_BATTERY		2	/* (wireless) */
#define	USB_DEVICE_B_HNP_ENABLE		3	/* (otg) dev may initiate HNP */
#define	USB_DEVICE_WUSB_DEVICE		3	/* (wireless) */
#define	USB_DEVICE_A_HNP_SUPPORT	4	/* (otg) RH port supports HNP */
#define	USB_DEVICE_A_ALT_HNP_SUPPORT	5	/* (otg) other RH port does */
#define	USB_DEVICE_DEBUG_MODE		6	/* (special devices only) */

#define	USB_ENDPOINT_HALT		0	/* IN/OUT will STALL */

#define	PIPE_ISOCHRONOUS		0x01	/* UE_ISOCHRONOUS */
#define	PIPE_INTERRUPT			0x03	/* UE_INTERRUPT */
#define	PIPE_CONTROL			0x00	/* UE_CONTROL */
#define	PIPE_BULK			0x02	/* UE_BULK */

/* Whenever Linux references an USB endpoint:
 * a) to initialize "urb->pipe"
 * b) second argument passed to "usb_control_msg()"
 *
 * Then it uses one of the following macros. The "endpoint" argument
 * is the physical endpoint value masked by 0xF. The "dev" argument
 * is a pointer to "struct usb_device".
 */
#define	usb_sndctrlpipe(dev,endpoint) \
  usb_find_host_endpoint(dev, PIPE_CONTROL, (endpoint) | USB_DIR_OUT)

#define	usb_rcvctrlpipe(dev,endpoint) \
  usb_find_host_endpoint(dev, PIPE_CONTROL, (endpoint) | USB_DIR_IN)

#define	usb_sndisocpipe(dev,endpoint) \
  usb_find_host_endpoint(dev, PIPE_ISOCHRONOUS, (endpoint) | USB_DIR_OUT)

#define	usb_rcvisocpipe(dev,endpoint) \
  usb_find_host_endpoint(dev, PIPE_ISOCHRONOUS, (endpoint) | USB_DIR_IN)

#define	usb_sndbulkpipe(dev,endpoint) \
  usb_find_host_endpoint(dev, PIPE_BULK, (endpoint) | USB_DIR_OUT)

#define	usb_rcvbulkpipe(dev,endpoint) \
  usb_find_host_endpoint(dev, PIPE_BULK, (endpoint) | USB_DIR_IN)

#define	usb_sndintpipe(dev,endpoint) \
  usb_find_host_endpoint(dev, PIPE_INTERRUPT, (endpoint) | USB_DIR_OUT)

#define	usb_rcvintpipe(dev,endpoint) \
  usb_find_host_endpoint(dev, PIPE_INTERRUPT, (endpoint) | USB_DIR_IN)

/* The following four structures makes up a tree, where we have the
 * leaf structure, "usb_host_endpoint", first, and the root structure,
 * "usb_device", last. The four structures below mirror the structure
 * of the USB descriptors belonging to an USB configuration. Please
 * refer to the USB specification for a definition of "endpoints" and
 * "interfaces".
 */
struct usb_host_endpoint {
	struct usb_endpoint_descriptor desc;

	TAILQ_HEAD(, urb) bsd_urb_list;

	struct usb2_xfer *bsd_xfer[2];

	uint8_t *extra;			/* Extra descriptors */

	uint32_t fbsd_buf_size;

	uint16_t extralen;

	uint8_t	bsd_iface_index;
} __aligned(USB_HOST_ALIGN);

struct usb_host_interface {
	struct usb_interface_descriptor desc;

	/* the following array has size "desc.bNumEndpoint" */
	struct usb_host_endpoint *endpoint;

	const char *string;		/* iInterface string, if present */
	uint8_t *extra;			/* Extra descriptors */

	uint16_t extralen;

	uint8_t	bsd_iface_index;
} __aligned(USB_HOST_ALIGN);

struct usb_interface {
	/* array of alternate settings for this interface */
	struct usb_host_interface *altsetting;
	struct usb_host_interface *cur_altsetting;
	struct usb_device *linux_udev;
	void   *bsd_priv_sc;		/* device specific information */

	uint8_t	num_altsetting;		/* number of alternate settings */
	uint8_t	bsd_iface_index;
} __aligned(USB_HOST_ALIGN);

struct usb_device {
	struct usb_device_descriptor descriptor;
	struct usb_host_endpoint ep0;

	struct usb2_device *bsd_udev;
	struct usb_interface *bsd_iface_start;
	struct usb_interface *bsd_iface_end;
	struct usb_host_endpoint *bsd_endpoint_start;
	struct usb_host_endpoint *bsd_endpoint_end;

	/* static strings from the device */
	const char *product;		/* iProduct string, if present */
	const char *manufacturer;	/* iManufacturer string, if present */
	const char *serial;		/* iSerialNumber string, if present */

	uint16_t devnum;

	uint8_t	speed;			/* USB_SPEED_XXX */
} __aligned(USB_HOST_ALIGN);

/*
 * The following structure is used to extend "struct urb" when we are
 * dealing with an isochronous endpoint. It contains information about
 * the data offset and data length of an isochronous packet.
 * The "actual_length" field is updated before the "complete"
 * callback in the "urb" structure is called.
 */
struct usb_iso_packet_descriptor {
	uint32_t offset;		/* depreciated buffer offset (the
					 * packets are usually back to back) */
	uint16_t length;		/* expected length */
	uint16_t actual_length;
	uint16_t status;
};

/*
 * The following structure holds various information about an USB
 * transfer. This structure is used for all kinds of USB transfers.
 *
 * URB is short for USB Request Block.
 */
struct urb {
	TAILQ_ENTRY(urb) bsd_urb_list;
	struct cv cv_wait;

	struct usb_device *dev;		/* (in) pointer to associated device */
	struct usb_host_endpoint *pipe;	/* (in) pipe pointer */
	uint8_t *setup_packet;		/* (in) setup packet (control only) */
	uint8_t *bsd_data_ptr;
	void   *transfer_buffer;	/* (in) associated data buffer */
	void   *context;		/* (in) context for completion */
	usb_complete_t *complete;	/* (in) completion routine */

	uint32_t transfer_buffer_length;/* (in) data buffer length */
	uint32_t actual_length;		/* (return) actual transfer length */
	uint32_t bsd_length_rem;
	uint32_t timeout;		/* FreeBSD specific */

	uint16_t transfer_flags;	/* (in) */
#define	URB_SHORT_NOT_OK	0x0001	/* report short transfers like errors */
#define	URB_ISO_ASAP		0x0002	/* ignore "start_frame" field */
#define	URB_ZERO_PACKET		0x0004	/* the USB transfer ends with a short
					 * packet */
#define	URB_NO_TRANSFER_DMA_MAP 0x0008	/* "transfer_dma" is valid on submit */
#define	URB_WAIT_WAKEUP		0x0010	/* custom flags */
#define	URB_IS_SLEEPING		0x0020	/* custom flags */

	uint16_t start_frame;		/* (modify) start frame (ISO) */
	uint16_t number_of_packets;	/* (in) number of ISO packets */
	uint16_t interval;		/* (modify) transfer interval
					 * (INT/ISO) */
	uint16_t error_count;		/* (return) number of ISO errors */
	int16_t	status;			/* (return) status */

	uint8_t	setup_dma;		/* (in) not used on FreeBSD */
	uint8_t	transfer_dma;		/* (in) not used on FreeBSD */
	uint8_t	bsd_isread;

	struct usb_iso_packet_descriptor iso_frame_desc[];	/* (in) ISO ONLY */
};

/* various prototypes */

int	usb_submit_urb(struct urb *urb, uint16_t mem_flags);
int	usb_unlink_urb(struct urb *urb);
int	usb_clear_halt(struct usb_device *dev, struct usb_host_endpoint *uhe);
int	usb_control_msg(struct usb_device *dev, struct usb_host_endpoint *pipe, uint8_t request, uint8_t requesttype, uint16_t value, uint16_t index, void *data, uint16_t size, uint32_t timeout);
int	usb_set_interface(struct usb_device *dev, uint8_t ifnum, uint8_t alternate);
int	usb_setup_endpoint(struct usb_device *dev, struct usb_host_endpoint *uhe, uint32_t bufsize);

struct usb_host_endpoint *usb_find_host_endpoint(struct usb_device *dev, uint8_t type, uint8_t ep);
struct urb *usb_alloc_urb(uint16_t iso_packets, uint16_t mem_flags);
struct usb_host_interface *usb_altnum_to_altsetting(const struct usb_interface *intf, uint8_t alt_index);
struct usb_interface *usb_ifnum_to_if(struct usb_device *dev, uint8_t iface_no);

void   *usb_buffer_alloc(struct usb_device *dev, uint32_t size, uint16_t mem_flags, uint8_t *dma_addr);
void   *usb_get_intfdata(struct usb_interface *intf);

void	usb_buffer_free(struct usb_device *dev, uint32_t size, void *addr, uint8_t dma_addr);
void	usb_free_urb(struct urb *urb);
void	usb_init_urb(struct urb *urb);
void	usb_kill_urb(struct urb *urb);
void	usb_set_intfdata(struct usb_interface *intf, void *data);
void	usb_linux_register(void *arg);
void	usb_linux_deregister(void *arg);

#define	interface_to_usbdev(intf) (intf)->linux_udev
#define	interface_to_bsddev(intf) (intf)->linux_udev->bsd_udev

#endif					/* _USB_COMPAT_LINUX_H */
