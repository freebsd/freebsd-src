#ifndef __LINUX_USB_H
#define __LINUX_USB_H

/* USB constants */

/*
 * Device and/or Interface Class codes
 */
#define USB_CLASS_PER_INTERFACE		0	/* for DeviceClass */
#define USB_CLASS_AUDIO			1
#define USB_CLASS_COMM			2
#define USB_CLASS_HID			3
#define USB_CLASS_PHYSICAL		5
#define USB_CLASS_STILL_IMAGE		6
#define USB_CLASS_PRINTER		7
#define USB_CLASS_MASS_STORAGE		8
#define USB_CLASS_HUB			9
#define USB_CLASS_CDC_DATA		0x0a
#define USB_CLASS_CSCID			0x0b	/* chip+ smart card */
#define USB_CLASS_CONTENT_SEC		0x0d	/* content security */
#define USB_CLASS_APP_SPEC		0xfe
#define USB_CLASS_VENDOR_SPEC		0xff

/*
 * USB types
 */
#define USB_TYPE_MASK			(0x03 << 5)
#define USB_TYPE_STANDARD		(0x00 << 5)
#define USB_TYPE_CLASS			(0x01 << 5)
#define USB_TYPE_VENDOR			(0x02 << 5)
#define USB_TYPE_RESERVED		(0x03 << 5)

/*
 * USB recipients
 */
#define USB_RECIP_MASK			0x1f
#define USB_RECIP_DEVICE		0x00
#define USB_RECIP_INTERFACE		0x01
#define USB_RECIP_ENDPOINT		0x02
#define USB_RECIP_OTHER			0x03

/*
 * USB directions
 */
#define USB_DIR_OUT			0		/* to device */
#define USB_DIR_IN			0x80		/* to host */

/*
 * Descriptor types
 */
#define USB_DT_DEVICE			0x01
#define USB_DT_CONFIG			0x02
#define USB_DT_STRING			0x03
#define USB_DT_INTERFACE		0x04
#define USB_DT_ENDPOINT			0x05

#define USB_DT_HID			(USB_TYPE_CLASS | 0x01)
#define USB_DT_REPORT			(USB_TYPE_CLASS | 0x02)
#define USB_DT_PHYSICAL			(USB_TYPE_CLASS | 0x03)
#define USB_DT_HUB			(USB_TYPE_CLASS | 0x09)

/*
 * Descriptor sizes per descriptor type
 */
#define USB_DT_DEVICE_SIZE		18
#define USB_DT_CONFIG_SIZE		9
#define USB_DT_INTERFACE_SIZE		9
#define USB_DT_ENDPOINT_SIZE		7
#define USB_DT_ENDPOINT_AUDIO_SIZE	9	/* Audio extension */
#define USB_DT_HUB_NONVAR_SIZE		7
#define USB_DT_HID_SIZE			9

/*
 * Endpoints
 */
#define USB_ENDPOINT_NUMBER_MASK	0x0f	/* in bEndpointAddress */
#define USB_ENDPOINT_DIR_MASK		0x80

#define USB_ENDPOINT_XFERTYPE_MASK	0x03	/* in bmAttributes */
#define USB_ENDPOINT_XFER_CONTROL	0
#define USB_ENDPOINT_XFER_ISOC		1
#define USB_ENDPOINT_XFER_BULK		2
#define USB_ENDPOINT_XFER_INT		3

/*
 * USB Packet IDs (PIDs)
 */
#define USB_PID_UNDEF_0			0xf0
#define USB_PID_OUT			0xe1
#define USB_PID_ACK			0xd2
#define USB_PID_DATA0			0xc3
#define USB_PID_PING			0xb4	/* USB 2.0 */
#define USB_PID_SOF			0xa5
#define USB_PID_NYET			0x96	/* USB 2.0 */
#define USB_PID_DATA2			0x87	/* USB 2.0 */
#define USB_PID_SPLIT			0x78	/* USB 2.0 */
#define USB_PID_IN			0x69
#define USB_PID_NAK			0x5a
#define USB_PID_DATA1			0x4b
#define USB_PID_PREAMBLE		0x3c	/* Token mode */
#define USB_PID_ERR			0x3c	/* USB 2.0: handshake mode */
#define USB_PID_SETUP			0x2d
#define USB_PID_STALL			0x1e
#define USB_PID_MDATA			0x0f	/* USB 2.0 */

/*
 * Standard requests
 */
#define USB_REQ_GET_STATUS		0x00
#define USB_REQ_CLEAR_FEATURE		0x01
#define USB_REQ_SET_FEATURE		0x03
#define USB_REQ_SET_ADDRESS		0x05
#define USB_REQ_GET_DESCRIPTOR		0x06
#define USB_REQ_SET_DESCRIPTOR		0x07
#define USB_REQ_GET_CONFIGURATION	0x08
#define USB_REQ_SET_CONFIGURATION	0x09
#define USB_REQ_GET_INTERFACE		0x0A
#define USB_REQ_SET_INTERFACE		0x0B
#define USB_REQ_SYNCH_FRAME		0x0C

/*
 * HID requests
 */
#define USB_REQ_GET_REPORT		0x01
#define USB_REQ_GET_IDLE		0x02
#define USB_REQ_GET_PROTOCOL		0x03
#define USB_REQ_SET_REPORT		0x09
#define USB_REQ_SET_IDLE		0x0A
#define USB_REQ_SET_PROTOCOL		0x0B


#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/interrupt.h>	/* for in_interrupt() */
#include <linux/config.h>
#include <linux/list.h>

#define USB_MAJOR 180

static __inline__ void wait_ms(unsigned int ms)
{
	if(!in_interrupt()) {
		current->state = TASK_UNINTERRUPTIBLE;
		schedule_timeout(1 + ms * HZ / 1000);
	}
	else
		mdelay(ms);
}

/**
 * struct usb_ctrlrequest - structure used to make USB device control requests easier to create and decode
 * @bRequestType: matches the USB bmRequestType field
 * @bRequest: matches the USB bRequest field
 * @wValue: matches the USB wValue field
 * @wIndex: matches the USB wIndex field
 * @wLength: matches the USB wLength field
 *
 * This structure is used to send control requests to a USB device.  It matches
 * the different fields of the USB 2.0 Spec section 9.3, table 9-2.  See the
 * USB spec for a fuller description of the different fields, and what they are
 * used for.
 */
struct usb_ctrlrequest {
	__u8 bRequestType;
	__u8 bRequest;
	__u16 wValue;
	__u16 wIndex;
	__u16 wLength;
} __attribute__ ((packed));

/*
 * USB-status codes:
 * USB_ST* maps to -E* and should go away in the future
 */

#define USB_ST_NOERROR		0
#define USB_ST_CRC		(-EILSEQ)
#define USB_ST_BITSTUFF		(-EPROTO)
#define USB_ST_NORESPONSE	(-ETIMEDOUT)			/* device not responding/handshaking */
#define USB_ST_DATAOVERRUN	(-EOVERFLOW)
#define USB_ST_DATAUNDERRUN	(-EREMOTEIO)
#define USB_ST_BUFFEROVERRUN	(-ECOMM)
#define USB_ST_BUFFERUNDERRUN	(-ENOSR)
#define USB_ST_INTERNALERROR	(-EPROTO) 			/* unknown error */
#define USB_ST_SHORT_PACKET	(-EREMOTEIO)
#define USB_ST_PARTIAL_ERROR	(-EXDEV)			/* ISO transfer only partially completed */
#define USB_ST_URB_KILLED	(-ENOENT)			/* URB canceled by user */
#define USB_ST_URB_PENDING	(-EINPROGRESS)
#define USB_ST_REMOVED		(-ENODEV) 			/* device not existing or removed */
#define USB_ST_TIMEOUT		(-ETIMEDOUT)			/* communication timed out, also in urb->status**/
#define USB_ST_NOTSUPPORTED	(-ENOSYS)			
#define USB_ST_BANDWIDTH_ERROR	(-ENOSPC)			/* too much bandwidth used */
#define USB_ST_URB_INVALID_ERROR  (-EINVAL)			/* invalid value/transfer type */
#define USB_ST_URB_REQUEST_ERROR  (-ENXIO)			/* invalid endpoint */
#define USB_ST_STALL		(-EPIPE) 			/* pipe stalled, also in urb->status*/

/*
 * USB device number allocation bitmap. There's one bitmap
 * per USB tree.
 */
struct usb_devmap {
	unsigned long devicemap[128 / (8*sizeof(unsigned long))];
};

#define USB_MAXBUS		64

struct usb_busmap {
	unsigned long busmap[USB_MAXBUS / (8*sizeof(unsigned long))];
};

/*
 * This is a USB device descriptor.
 *
 * USB device information
 */

/* Everything but the endpoint maximums are aribtrary */
#define USB_MAXCONFIG		8
#define USB_ALTSETTINGALLOC     4
#define USB_MAXALTSETTING	128  /* Hard limit */
#define USB_MAXINTERFACES	32
#define USB_MAXENDPOINTS	32

/* All standard descriptors have these 2 fields in common */
struct usb_descriptor_header {
	__u8  bLength;
	__u8  bDescriptorType;
} __attribute__ ((packed));

/* Device descriptor */
struct usb_device_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u16 bcdUSB;
	__u8  bDeviceClass;
	__u8  bDeviceSubClass;
	__u8  bDeviceProtocol;
	__u8  bMaxPacketSize0;
	__u16 idVendor;
	__u16 idProduct;
	__u16 bcdDevice;
	__u8  iManufacturer;
	__u8  iProduct;
	__u8  iSerialNumber;
	__u8  bNumConfigurations;
} __attribute__ ((packed));

/* Endpoint descriptor */
struct usb_endpoint_descriptor {
	__u8  bLength		__attribute__ ((packed));
	__u8  bDescriptorType	__attribute__ ((packed));
	__u8  bEndpointAddress	__attribute__ ((packed));
	__u8  bmAttributes	__attribute__ ((packed));
	__u16 wMaxPacketSize	__attribute__ ((packed));
	__u8  bInterval		__attribute__ ((packed));
	__u8  bRefresh		__attribute__ ((packed));
	__u8  bSynchAddress	__attribute__ ((packed));

   	unsigned char *extra;   /* Extra descriptors */
	int extralen;
};

/* Interface descriptor */
struct usb_interface_descriptor {
	__u8  bLength		__attribute__ ((packed));
	__u8  bDescriptorType	__attribute__ ((packed));
	__u8  bInterfaceNumber	__attribute__ ((packed));
	__u8  bAlternateSetting	__attribute__ ((packed));
	__u8  bNumEndpoints	__attribute__ ((packed));
	__u8  bInterfaceClass	__attribute__ ((packed));
	__u8  bInterfaceSubClass __attribute__ ((packed));
	__u8  bInterfaceProtocol __attribute__ ((packed));
	__u8  iInterface	__attribute__ ((packed));

  	struct usb_endpoint_descriptor *endpoint;

   	unsigned char *extra;   /* Extra descriptors */
	int extralen;
};

struct usb_interface {
	struct usb_interface_descriptor *altsetting;

	int act_altsetting;		/* active alternate setting */
	int num_altsetting;		/* number of alternate settings */
	int max_altsetting;             /* total memory allocated */
 
	struct usb_driver *driver;	/* driver */
	void *private_data;
};

/* Configuration descriptor information.. */
struct usb_config_descriptor {
	__u8  bLength		__attribute__ ((packed));
	__u8  bDescriptorType	__attribute__ ((packed));
	__u16 wTotalLength	__attribute__ ((packed));
	__u8  bNumInterfaces	__attribute__ ((packed));
	__u8  bConfigurationValue __attribute__ ((packed));
	__u8  iConfiguration	__attribute__ ((packed));
	__u8  bmAttributes	__attribute__ ((packed));
	__u8  MaxPower		__attribute__ ((packed));

	struct usb_interface *interface;

   	unsigned char *extra;   /* Extra descriptors */
	int extralen;
};

/* String descriptor */
struct usb_string_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u16 wData[1];
} __attribute__ ((packed));

struct usb_device;

/*
 * Device table entry for "new style" table-driven USB drivers.
 * User mode code can read these tables to choose which modules to load.
 * Declare the table as __devinitdata, and as a MODULE_DEVICE_TABLE.
 *
 * With a device table provide bind() instead of probe().  Then the
 * third bind() parameter will point to a matching entry from this
 * table.  (Null value reserved.)
 * 
 * Terminate the driver's table with an all-zeroes entry.
 * Init the fields you care about; zeroes are not used in comparisons.
 */
#define USB_DEVICE_ID_MATCH_VENDOR		0x0001
#define USB_DEVICE_ID_MATCH_PRODUCT		0x0002
#define USB_DEVICE_ID_MATCH_DEV_LO		0x0004
#define USB_DEVICE_ID_MATCH_DEV_HI		0x0008
#define USB_DEVICE_ID_MATCH_DEV_CLASS		0x0010
#define USB_DEVICE_ID_MATCH_DEV_SUBCLASS	0x0020
#define USB_DEVICE_ID_MATCH_DEV_PROTOCOL	0x0040
#define USB_DEVICE_ID_MATCH_INT_CLASS		0x0080
#define USB_DEVICE_ID_MATCH_INT_SUBCLASS	0x0100
#define USB_DEVICE_ID_MATCH_INT_PROTOCOL	0x0200

#define USB_DEVICE_ID_MATCH_DEVICE		(USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT)
#define USB_DEVICE_ID_MATCH_DEV_RANGE		(USB_DEVICE_ID_MATCH_DEV_LO | USB_DEVICE_ID_MATCH_DEV_HI)
#define USB_DEVICE_ID_MATCH_DEVICE_AND_VERSION	(USB_DEVICE_ID_MATCH_DEVICE | USB_DEVICE_ID_MATCH_DEV_RANGE)
#define USB_DEVICE_ID_MATCH_DEV_INFO \
	(USB_DEVICE_ID_MATCH_DEV_CLASS | USB_DEVICE_ID_MATCH_DEV_SUBCLASS | USB_DEVICE_ID_MATCH_DEV_PROTOCOL)
#define USB_DEVICE_ID_MATCH_INT_INFO \
	(USB_DEVICE_ID_MATCH_INT_CLASS | USB_DEVICE_ID_MATCH_INT_SUBCLASS | USB_DEVICE_ID_MATCH_INT_PROTOCOL)

/* Some useful macros */
#define USB_DEVICE(vend,prod) \
	match_flags: USB_DEVICE_ID_MATCH_DEVICE, idVendor: (vend), idProduct: (prod)
#define USB_DEVICE_VER(vend,prod,lo,hi) \
	match_flags: USB_DEVICE_ID_MATCH_DEVICE_AND_VERSION, idVendor: (vend), idProduct: (prod), bcdDevice_lo: (lo), bcdDevice_hi: (hi)
#define USB_DEVICE_INFO(cl,sc,pr) \
	match_flags: USB_DEVICE_ID_MATCH_DEV_INFO, bDeviceClass: (cl), bDeviceSubClass: (sc), bDeviceProtocol: (pr)
#define USB_INTERFACE_INFO(cl,sc,pr) \
	match_flags: USB_DEVICE_ID_MATCH_INT_INFO, bInterfaceClass: (cl), bInterfaceSubClass: (sc), bInterfaceProtocol: (pr)

struct usb_device_id {
	/* This bitmask is used to determine which of the following fields
	 * are to be used for matching.
	 */
	__u16		match_flags;

	/*
	 * vendor/product codes are checked, if vendor is nonzero
	 * Range is for device revision (bcdDevice), inclusive;
	 * zero values here mean range isn't considered
	 */
	__u16		idVendor;
	__u16		idProduct;
	__u16		bcdDevice_lo, bcdDevice_hi;

	/*
	 * if device class != 0, these can be match criteria;
	 * but only if this bDeviceClass value is nonzero
	 */
	__u8		bDeviceClass;
	__u8		bDeviceSubClass;
	__u8		bDeviceProtocol;

	/*
	 * if interface class != 0, these can be match criteria;
	 * but only if this bInterfaceClass value is nonzero
	 */
	__u8		bInterfaceClass;
	__u8		bInterfaceSubClass;
	__u8		bInterfaceProtocol;

	/*
	 * for driver's use; not involved in driver matching.
	 */
	unsigned long	driver_info;
};

/**
 * struct usb_driver - identifies USB driver to usbcore
 * @owner: Pointer to the module owner of this driver; initialize
 *      it using THIS_MODULE.
 * @name: The driver name should be unique among USB drivers,
 *      and should normally be the same as the module name.
 * @probe: Called to see if the driver is willing to manage a particular
 *      interface on a device.  The probe routine returns a handle that 
 *      will later be provided to disconnect(), or a null pointer to
 *      indicate that the driver will not handle the interface.
 *      The handle is normally a pointer to driver-specific data.
 *      If the probe() routine needs to access the interface
 *      structure itself, use usb_ifnum_to_if() to make sure it's using
 *      the right one.
 * @disconnect: Called when the interface is no longer accessible, usually
 *      because its device has been (or is being) disconnected.  The
 *      handle passed is what was returned by probe(), or was provided
 *      to usb_driver_claim_interface().
 * @ioctl: Used for drivers that want to talk to userspace through
 *      the "usbfs" filesystem.  This lets devices provide ways to
 *      expose information to user space regardless of where they
 *      do (or don't) show up otherwise in the filesystem.
 * @fops: pointer to a fops structure if the driver wants to use the USB
 *	major number.
 * @minor: the starting minor number for this driver, if the fops
 *	pointer is set.
 * @id_table: USB drivers use ID table to support hotplugging.
 *      Export this with MODULE_DEVICE_TABLE(usb,...), or use NULL to
 *      say that probe() should be called for any unclaimed interface.
 *
 * USB drivers must provide a name, probe() and disconnect() methods,
 * and an id_table.  Other driver fields are optional.
 *
 * The id_table is used in hotplugging.  It holds a set of descriptors,
 * and specialized data may be associated with each entry.  That table
 * is used by both user and kernel mode hotplugging support.
 * The probe() and disconnect() methods are called in a context where
 * they can sleep, but they should avoid abusing the privilege.  Most
 * work to connect to a device should be done when the device is opened,
 * and undone at the last close.  The disconnect code needs to address
 * concurrency issues with respect to open() and close() methods, as
 * well as forcing all pending I/O requests to complete (by unlinking
 * them as necessary, and blocking until the unlinks complete).
 */
struct usb_driver {
	struct module *owner;

	const char *name;

	void *(*probe)(
	    struct usb_device *dev,		/* the device */
	    unsigned intf,			/* what interface */
	    const struct usb_device_id *id	/* from id_table */
	    );
	void (*disconnect)(struct usb_device *, void *);

	struct list_head driver_list;

	struct file_operations *fops;
	int minor;

	struct semaphore serialize;

	int (*ioctl)(struct usb_device *dev, unsigned int code, void *buf);

	const struct usb_device_id *id_table;
};
	
/*----------------------------------------------------------------------------* 
 * New USB Structures                                                         *
 *----------------------------------------------------------------------------*/

/*
 * urb->transfer_flags:
 */
#define USB_DISABLE_SPD		0x0001
#define URB_SHORT_NOT_OK	USB_DISABLE_SPD
#define USB_ISO_ASAP		0x0002
#define USB_ASYNC_UNLINK	0x0008
#define USB_QUEUE_BULK		0x0010
#define USB_NO_FSBR		0x0020
#define USB_ZERO_PACKET		0x0040  // Finish bulk OUTs always with zero length packet
#define URB_NO_INTERRUPT	0x0080	/* HINT: no non-error interrupt needed */
					/* ... less overhead for QUEUE_BULK */
#define USB_TIMEOUT_KILLED	0x1000	// only set by HCD!

struct iso_packet_descriptor
{
	unsigned int offset;
	unsigned int length;		// expected length
	unsigned int actual_length;
	unsigned int status;
};

#define usb_iso_packet_descriptor	iso_packet_descriptor

struct urb;
typedef void (*usb_complete_t)(struct urb *);

struct urb
{
	spinlock_t lock;		// lock for the URB
	void *hcpriv;			// private data for host controller
	struct list_head urb_list;	// list pointer to all active urbs 
	struct urb *next;		// pointer to next URB	
	struct usb_device *dev;		// pointer to associated USB device
	unsigned int pipe;		// pipe information
	int status;			// returned status
	unsigned int transfer_flags;	// USB_DISABLE_SPD | USB_ISO_ASAP | etc.
	void *transfer_buffer;		// associated data buffer
	dma_addr_t transfer_dma;	// dma addr for transfer_buffer
	int transfer_buffer_length;	// data buffer length
	int actual_length;              // actual data buffer length	
	int bandwidth;			// bandwidth for this transfer request (INT or ISO)
	unsigned char *setup_packet;	// setup packet (control only)
	dma_addr_t setup_dma;		// dma addr for setup_packet
	//
	int start_frame;		// start frame (iso/irq only)
	int number_of_packets;		// number of packets in this request (iso)
	int interval;                   // polling interval (irq only)
	int error_count;		// number of errors in this transfer (iso only)
	int timeout;			// timeout (in jiffies)
	//
	void *context;			// context for completion routine
	usb_complete_t complete;	// pointer to completion routine
	//
	struct iso_packet_descriptor iso_frame_desc[0];
};

/**
 * FILL_CONTROL_URB - macro to help initialize a control urb
 * @URB: pointer to the urb to initialize.
 * @DEV: pointer to the struct usb_device for this urb.
 * @PIPE: the endpoint pipe
 * @SETUP_PACKET: pointer to the setup_packet buffer
 * @TRANSFER_BUFFER: pointer to the transfer buffer
 * @BUFFER_LENGTH: length of the transfer buffer
 * @COMPLETE: pointer to the usb_complete_t function
 * @CONTEXT: what to set the urb context to.
 *
 * Initializes a control urb with the proper information needed to submit
 * it to a device.  This macro is depreciated, the usb_fill_control_urb()
 * function should be used instead.
 */
#define FILL_CONTROL_URB(URB,DEV,PIPE,SETUP_PACKET,TRANSFER_BUFFER,BUFFER_LENGTH,COMPLETE,CONTEXT) \
    do {\
	spin_lock_init(&(URB)->lock);\
	(URB)->dev=DEV;\
	(URB)->pipe=PIPE;\
	(URB)->setup_packet=SETUP_PACKET;\
	(URB)->transfer_buffer=TRANSFER_BUFFER;\
	(URB)->transfer_buffer_length=BUFFER_LENGTH;\
	(URB)->complete=COMPLETE;\
	(URB)->context=CONTEXT;\
    } while (0)

/**
 * FILL_BULK_URB - macro to help initialize a bulk urb
 * @URB: pointer to the urb to initialize.
 * @DEV: pointer to the struct usb_device for this urb.
 * @PIPE: the endpoint pipe
 * @TRANSFER_BUFFER: pointer to the transfer buffer
 * @BUFFER_LENGTH: length of the transfer buffer
 * @COMPLETE: pointer to the usb_complete_t function
 * @CONTEXT: what to set the urb context to.
 *
 * Initializes a bulk urb with the proper information needed to submit it
 * to a device.  This macro is depreciated, the usb_fill_bulk_urb()
 * function should be used instead.
 */
#define FILL_BULK_URB(URB,DEV,PIPE,TRANSFER_BUFFER,BUFFER_LENGTH,COMPLETE,CONTEXT) \
    do {\
	spin_lock_init(&(URB)->lock);\
	(URB)->dev=DEV;\
	(URB)->pipe=PIPE;\
	(URB)->transfer_buffer=TRANSFER_BUFFER;\
	(URB)->transfer_buffer_length=BUFFER_LENGTH;\
	(URB)->complete=COMPLETE;\
	(URB)->context=CONTEXT;\
    } while (0)
    
/**
 * FILL_INT_URB - macro to help initialize a interrupt urb
 * @URB: pointer to the urb to initialize.
 * @DEV: pointer to the struct usb_device for this urb.
 * @PIPE: the endpoint pipe
 * @TRANSFER_BUFFER: pointer to the transfer buffer
 * @BUFFER_LENGTH: length of the transfer buffer
 * @COMPLETE: pointer to the usb_complete_t function
 * @CONTEXT: what to set the urb context to.
 * @INTERVAL: what to set the urb interval to.
 *
 * Initializes a interrupt urb with the proper information needed to submit
 * it to a device.  This macro is depreciated, the usb_fill_int_urb()
 * function should be used instead.
 */
#define FILL_INT_URB(URB,DEV,PIPE,TRANSFER_BUFFER,BUFFER_LENGTH,COMPLETE,CONTEXT,INTERVAL) \
    do {\
	spin_lock_init(&(URB)->lock);\
	(URB)->dev=DEV;\
	(URB)->pipe=PIPE;\
	(URB)->transfer_buffer=TRANSFER_BUFFER;\
	(URB)->transfer_buffer_length=BUFFER_LENGTH;\
	(URB)->complete=COMPLETE;\
	(URB)->context=CONTEXT;\
	(URB)->interval=INTERVAL;\
	(URB)->start_frame=-1;\
    } while (0)

#define FILL_CONTROL_URB_TO(a,aa,b,c,d,e,f,g,h) \
    do {\
	spin_lock_init(&(a)->lock);\
	(a)->dev=aa;\
	(a)->pipe=b;\
	(a)->setup_packet=c;\
	(a)->transfer_buffer=d;\
	(a)->transfer_buffer_length=e;\
	(a)->complete=f;\
	(a)->context=g;\
	(a)->timeout=h;\
    } while (0)

#define FILL_BULK_URB_TO(a,aa,b,c,d,e,f,g) \
    do {\
	spin_lock_init(&(a)->lock);\
	(a)->dev=aa;\
	(a)->pipe=b;\
	(a)->transfer_buffer=c;\
	(a)->transfer_buffer_length=d;\
	(a)->complete=e;\
	(a)->context=f;\
	(a)->timeout=g;\
    } while (0)
 
/**
 * usb_fill_control_urb - initializes a control urb
 * @urb: pointer to the urb to initialize.
 * @dev: pointer to the struct usb_device for this urb.
 * @pipe: the endpoint pipe
 * @setup_packet: pointer to the setup_packet buffer
 * @transfer_buffer: pointer to the transfer buffer
 * @buffer_length: length of the transfer buffer
 * @complete: pointer to the usb_complete_t function
 * @context: what to set the urb context to.
 *
 * Initializes a control urb with the proper information needed to submit
 * it to a device.
 */
static inline void usb_fill_control_urb (struct urb *urb,
					 struct usb_device *dev,
					 unsigned int pipe,
					 unsigned char *setup_packet,
					 void *transfer_buffer,
					 int buffer_length,
					 usb_complete_t complete,
					 void *context)
{
	spin_lock_init(&urb->lock);
	urb->dev = dev;
	urb->pipe = pipe;
	urb->setup_packet = setup_packet;
	urb->transfer_buffer = transfer_buffer;
	urb->transfer_buffer_length = buffer_length;
	urb->complete = complete;
	urb->context = context;
}

/**
 * usb_fill_bulk_urb - macro to help initialize a bulk urb
 * @urb: pointer to the urb to initialize.
 * @dev: pointer to the struct usb_device for this urb.
 * @pipe: the endpoint pipe
 * @transfer_buffer: pointer to the transfer buffer
 * @buffer_length: length of the transfer buffer
 * @complete: pointer to the usb_complete_t function
 * @context: what to set the urb context to.
 *
 * Initializes a bulk urb with the proper information needed to submit it
 * to a device.
 */
static inline void usb_fill_bulk_urb (struct urb *urb,
				      struct usb_device *dev,
				      unsigned int pipe,
				      void *transfer_buffer,
				      int buffer_length,
				      usb_complete_t complete,
				      void *context)
				      
{
	spin_lock_init(&urb->lock);
	urb->dev = dev;
	urb->pipe = pipe;
	urb->transfer_buffer = transfer_buffer;
	urb->transfer_buffer_length = buffer_length;
	urb->complete = complete;
	urb->context = context;
}
    
/**
 * usb_fill_int_urb - macro to help initialize a interrupt urb
 * @urb: pointer to the urb to initialize.
 * @dev: pointer to the struct usb_device for this urb.
 * @pipe: the endpoint pipe
 * @transfer_buffer: pointer to the transfer buffer
 * @buffer_length: length of the transfer buffer
 * @complete: pointer to the usb_complete_t function
 * @context: what to set the urb context to.
 * @interval: what to set the urb interval to.
 *
 * Initializes a interrupt urb with the proper information needed to submit
 * it to a device.
 */
static inline void usb_fill_int_urb (struct urb *urb,
				     struct usb_device *dev,
				     unsigned int pipe,
				     void *transfer_buffer,
				     int buffer_length,
				     usb_complete_t complete,
				     void *context,
				     int interval)
{
	spin_lock_init(&urb->lock);
	urb->dev = dev;
	urb->pipe = pipe;
	urb->transfer_buffer = transfer_buffer;
	urb->transfer_buffer_length = buffer_length;
	urb->complete = complete;
	urb->context = context;
	urb->interval = interval;
	urb->start_frame = -1;
}

struct urb *usb_alloc_urb(int iso_packets);
void usb_free_urb (struct urb *urb);
int usb_submit_urb(struct urb *urb);
int usb_unlink_urb(struct urb *urb);
int usb_internal_control_msg(struct usb_device *usb_dev, unsigned int pipe, struct usb_ctrlrequest *cmd,  void *data, int len, int timeout);
int usb_bulk_msg(struct usb_device *usb_dev, unsigned int pipe, void *data, int len, int *actual_length, int timeout);

/*-------------------------------------------------------------------*
 *                         SYNCHRONOUS CALL SUPPORT                  *
 *-------------------------------------------------------------------*/

struct usb_api_data
{
	wait_queue_head_t wqh;
	int done;
	/* void* stuff;	*/	/* Possible extension later. */
};

/* -------------------------------------------------------------------------- */

struct usb_operations {
	int (*allocate)(struct usb_device *);
	int (*deallocate)(struct usb_device *);
	int (*get_frame_number) (struct usb_device *usb_dev);
	int (*submit_urb) (struct urb* purb);
	int (*unlink_urb) (struct urb* purb);
};

#define DEVNUM_ROUND_ROBIN	/***** OPTION *****/

/*
 * Allocated per bus we have
 */
struct usb_bus {
	int busnum;			/* Bus number (in order of reg) */
	char *bus_name;			/* stable id (PCI slot_name etc) */

#ifdef DEVNUM_ROUND_ROBIN
	int devnum_next;                /* Next open device number in round-robin allocation */
#endif /* DEVNUM_ROUND_ROBIN */

	struct usb_devmap devmap;       /* Device map */
	struct usb_operations *op;      /* Operations (specific to the HC) */
	struct usb_device *root_hub;    /* Root hub */
	struct list_head bus_list;
	void *hcpriv;                   /* Host Controller private data */

	int bandwidth_allocated;	/* on this Host Controller; */
					  /* applies to Int. and Isoc. pipes; */
					  /* measured in microseconds/frame; */
					  /* range is 0..900, where 900 = */
					  /* 90% of a 1-millisecond frame */
	int bandwidth_int_reqs;		/* number of Interrupt requesters */
	int bandwidth_isoc_reqs;	/* number of Isoc. requesters */

	/* usbdevfs inode list */
	struct list_head inodes;

	atomic_t refcnt;
};

/*
 * As of USB 2.0, full/low speed devices are segregated into trees.
 * One type grows from USB 1.1 host controllers (OHCI, UHCI etc).
 * The other type grows from high speed hubs when they connect to
 * full/low speed devices using "Transaction Translators" (TTs).
 *
 * TTs should only be known to the hub driver, and high speed bus
 * drivers (only EHCI for now).  They affect periodic scheduling and
 * sometimes control/bulk error recovery.
 */
struct usb_tt {
	struct usb_device	*hub;	/* upstream highspeed hub */
	int			multi;	/* true means one TT per port */
};


/* This is arbitrary.
 * From USB 2.0 spec Table 11-13, offset 7, a hub can
 * have up to 255 ports. The most yet reported is 10.
 */
#define USB_MAXCHILDREN		(16)

struct usb_device {
	int		devnum;		/* Address on USB bus */
	char		devpath [16];	/* Use in messages: /port/port/... */

	enum {
		USB_SPEED_UNKNOWN = 0,			/* enumerating */
		USB_SPEED_LOW, USB_SPEED_FULL,		/* usb 1.1 */
		USB_SPEED_HIGH				/* usb 2.0 */
	} speed;

	struct usb_tt	*tt; 		/* low/full speed dev, highspeed hub */
	int		ttport;		/* device port on that tt hub */

	atomic_t refcnt;		/* Reference count */
	struct semaphore serialize;

	unsigned int toggle[2];		/* one bit for each endpoint ([0] = IN, [1] = OUT) */
	unsigned int halted[2];		/* endpoint halts; one bit per endpoint # & direction; */
					/* [0] = IN, [1] = OUT */
	int epmaxpacketin[16];		/* INput endpoint specific maximums */
	int epmaxpacketout[16];		/* OUTput endpoint specific maximums */

	struct usb_device *parent;
	struct usb_bus *bus;		/* Bus we're part of */

	struct usb_device_descriptor descriptor;/* Descriptor */
	struct usb_config_descriptor *config;	/* All of the configs */
	struct usb_config_descriptor *actconfig;/* the active configuration */

	char **rawdescriptors;		/* Raw descriptors for each config */

	int have_langid;		/* whether string_langid is valid yet */
	int string_langid;		/* language ID for strings */
  
	void *hcpriv;			/* Host Controller private data */
	
        /* usbdevfs inode list */
	struct list_head inodes;
	struct list_head filelist;

	/*
	 * Child devices - these can be either new devices
	 * (if this is a hub device), or different instances
	 * of this same device.
	 *
	 * Each instance needs its own set of data structures.
	 */

	int maxchild;			/* Number of ports if hub */
	struct usb_device *children[USB_MAXCHILDREN];
};

extern int usb_ifnum_to_ifpos(struct usb_device *dev, unsigned ifnum);
extern struct usb_interface *usb_ifnum_to_if(struct usb_device *dev, unsigned ifnum);
extern struct usb_endpoint_descriptor *usb_epnum_to_ep_desc(struct usb_device *dev, unsigned epnum);

extern int usb_register(struct usb_driver *);
extern void usb_deregister(struct usb_driver *);
extern void usb_scan_devices(void);

/* used these for multi-interface device registration */
extern int usb_find_interface_driver_for_ifnum(struct usb_device *dev, unsigned int ifnum);
extern void usb_driver_claim_interface(struct usb_driver *driver, struct usb_interface *iface, void* priv);
extern int usb_interface_claimed(struct usb_interface *iface);
extern void usb_driver_release_interface(struct usb_driver *driver, struct usb_interface *iface);
const struct usb_device_id *usb_match_id(struct usb_device *dev,
					 struct usb_interface *interface,
					 const struct usb_device_id *id);

extern struct usb_bus *usb_alloc_bus(struct usb_operations *);
extern void usb_free_bus(struct usb_bus *);
extern void usb_register_bus(struct usb_bus *);
extern void usb_deregister_bus(struct usb_bus *);

extern struct usb_device *usb_alloc_dev(struct usb_device *parent, struct usb_bus *);
extern void usb_free_dev(struct usb_device *);
extern void usb_inc_dev_use(struct usb_device *);
#define usb_dec_dev_use usb_free_dev

extern int usb_control_msg(struct usb_device *dev, unsigned int pipe, __u8 request, __u8 requesttype, __u16 value, __u16 index, void *data, __u16 size, int timeout);

extern int usb_root_hub_string(int id, int serial, char *type, __u8 *data, int len);
extern void usb_connect(struct usb_device *dev);
extern void usb_disconnect(struct usb_device **);

extern void usb_destroy_configuration(struct usb_device *dev);

int usb_get_current_frame_number (struct usb_device *usb_dev);


/**
 * usb_make_path - returns stable device path in the usb tree
 * @dev: the device whose path is being constructed
 * @buf: where to put the string
 * @size: how big is "buf"?
 *
 * Returns length of the string (> 0) or negative if size was too small.
 *
 * This identifier is intended to be "stable", reflecting physical paths in
 * hardware such as physical bus addresses for host controllers or ports on
 * USB hubs.  That makes it stay the same until systems are physically
 * reconfigured, by re-cabling a tree of USB devices or by moving USB host
 * controllers.  Adding and removing devices, including virtual root hubs
 * in host controller driver modules, does not change these path identifers;
 * neither does rebooting or re-enumerating.  These are more useful identifiers
 * than changeable ("unstable") ones like bus numbers or device addresses.
 * (The stability of the id depends on stability of the bus_name associated
 * with the bus the device uses; that is normally stable.)
 *
 * With a partial exception for devices connected to USB 2.0 root hubs, these
 * identifiers are also predictable.  So long as the device tree isn't changed,
 * plugging any USB device into a given hub port always gives it the same path.
 * Because of the use of "companion" controllers, devices connected to ports on
 * USB 2.0 root hubs (EHCI host controllers) will get one path ID if they are
 * high speed, and a different one if they are full or low speed.
 */
static inline int usb_make_path (struct usb_device *dev, char *buf, size_t size)
{
	int actual;
	actual = snprintf (buf, size, "usb-%s-%s",
		dev->bus->bus_name, dev->devpath);
	return (actual >= size) ? -1 : actual;
}


/*
 * Calling this entity a "pipe" is glorifying it. A USB pipe
 * is something embarrassingly simple: it basically consists
 * of the following information:
 *  - device number (7 bits)
 *  - endpoint number (4 bits)
 *  - current Data0/1 state (1 bit)
 *  - direction (1 bit)
 *  - speed (1 bit)
 *  - max packet size (2 bits: 8, 16, 32 or 64) [Historical; now gone.]
 *  - pipe type (2 bits: control, interrupt, bulk, isochronous)
 *
 * That's 18 bits. Really. Nothing more. And the USB people have
 * documented these eighteen bits as some kind of glorious
 * virtual data structure.
 *
 * Let's not fall in that trap. We'll just encode it as a simple
 * unsigned int. The encoding is:
 *
 *  - max size:		bits 0-1	(00 = 8, 01 = 16, 10 = 32, 11 = 64) [Historical; now gone.]
 *  - direction:	bit 7		(0 = Host-to-Device [Out], 1 = Device-to-Host [In])
 *  - device:		bits 8-14
 *  - endpoint:		bits 15-18
 *  - Data0/1:		bit 19
 *  - speed:		bit 26		(0 = Full, 1 = Low Speed)
 *  - pipe type:	bits 30-31	(00 = isochronous, 01 = interrupt, 10 = control, 11 = bulk)
 *
 * Why? Because it's arbitrary, and whatever encoding we select is really
 * up to us. This one happens to share a lot of bit positions with the UHCI
 * specification, so that much of the uhci driver can just mask the bits
 * appropriately.
 *
 * NOTE:  there's no encoding (yet?) for a "high speed" endpoint; treat them
 * like full speed devices.
 */

#define PIPE_ISOCHRONOUS		0
#define PIPE_INTERRUPT			1
#define PIPE_CONTROL			2
#define PIPE_BULK			3

#define usb_maxpacket(dev, pipe, out)	(out \
				? (dev)->epmaxpacketout[usb_pipeendpoint(pipe)] \
				: (dev)->epmaxpacketin [usb_pipeendpoint(pipe)] )
#define usb_packetid(pipe)	(((pipe) & USB_DIR_IN) ? USB_PID_IN : USB_PID_OUT)

#define usb_pipeout(pipe)	((((pipe) >> 7) & 1) ^ 1)
#define usb_pipein(pipe)	(((pipe) >> 7) & 1)
#define usb_pipedevice(pipe)	(((pipe) >> 8) & 0x7f)
#define usb_pipe_endpdev(pipe)	(((pipe) >> 8) & 0x7ff)
#define usb_pipeendpoint(pipe)	(((pipe) >> 15) & 0xf)
#define usb_pipedata(pipe)	(((pipe) >> 19) & 1)
#define usb_pipeslow(pipe)	(((pipe) >> 26) & 1)
#define usb_pipetype(pipe)	(((pipe) >> 30) & 3)
#define usb_pipeisoc(pipe)	(usb_pipetype((pipe)) == PIPE_ISOCHRONOUS)
#define usb_pipeint(pipe)	(usb_pipetype((pipe)) == PIPE_INTERRUPT)
#define usb_pipecontrol(pipe)	(usb_pipetype((pipe)) == PIPE_CONTROL)
#define usb_pipebulk(pipe)	(usb_pipetype((pipe)) == PIPE_BULK)

#define PIPE_DEVEP_MASK		0x0007ff00

/* The D0/D1 toggle bits */
#define usb_gettoggle(dev, ep, out) (((dev)->toggle[out] >> (ep)) & 1)
#define	usb_dotoggle(dev, ep, out)  ((dev)->toggle[out] ^= (1 << (ep)))
static inline void usb_settoggle(struct usb_device *dev,
				 unsigned int ep, 
				 unsigned int out,
				 int bit)
{
	dev->toggle[out] &= ~(1 << ep);
	dev->toggle[out] |= bit << ep;
}

/* Endpoint halt control/status */
#define usb_endpoint_out(ep_dir)	(((ep_dir >> 7) & 1) ^ 1)
#define usb_endpoint_halt(dev, ep, out) ((dev)->halted[out] |= (1 << (ep)))
#define usb_endpoint_running(dev, ep, out) ((dev)->halted[out] &= ~(1 << (ep)))
#define usb_endpoint_halted(dev, ep, out) ((dev)->halted[out] & (1 << (ep)))

static inline unsigned int __create_pipe(struct usb_device *dev, unsigned int endpoint)
{
	return (dev->devnum << 8) | (endpoint << 15) |
		((dev->speed == USB_SPEED_LOW) << 26);
}

static inline unsigned int __default_pipe(struct usb_device *dev)
{
	return ((dev->speed == USB_SPEED_LOW) << 26);
}

/* Create various pipes... */
#define usb_sndctrlpipe(dev,endpoint)	((PIPE_CONTROL << 30) | __create_pipe(dev,endpoint))
#define usb_rcvctrlpipe(dev,endpoint)	((PIPE_CONTROL << 30) | __create_pipe(dev,endpoint) | USB_DIR_IN)
#define usb_sndisocpipe(dev,endpoint)	((PIPE_ISOCHRONOUS << 30) | __create_pipe(dev,endpoint))
#define usb_rcvisocpipe(dev,endpoint)	((PIPE_ISOCHRONOUS << 30) | __create_pipe(dev,endpoint) | USB_DIR_IN)
#define usb_sndbulkpipe(dev,endpoint)	((PIPE_BULK << 30) | __create_pipe(dev,endpoint))
#define usb_rcvbulkpipe(dev,endpoint)	((PIPE_BULK << 30) | __create_pipe(dev,endpoint) | USB_DIR_IN)
#define usb_sndintpipe(dev,endpoint)	((PIPE_INTERRUPT << 30) | __create_pipe(dev,endpoint))
#define usb_rcvintpipe(dev,endpoint)	((PIPE_INTERRUPT << 30) | __create_pipe(dev,endpoint) | USB_DIR_IN)
#define usb_snddefctrl(dev)		((PIPE_CONTROL << 30) | __default_pipe(dev))
#define usb_rcvdefctrl(dev)		((PIPE_CONTROL << 30) | __default_pipe(dev) | USB_DIR_IN)

/*
 * Send and receive control messages..
 */
int usb_new_device(struct usb_device *dev);
int usb_reset_device(struct usb_device *dev);
int usb_set_address(struct usb_device *dev);
int usb_get_descriptor(struct usb_device *dev, unsigned char desctype,
	unsigned char descindex, void *buf, int size);
int usb_get_class_descriptor(struct usb_device *dev, int ifnum, unsigned char desctype,
	unsigned char descindex, void *buf, int size);
int usb_get_device_descriptor(struct usb_device *dev);
int __usb_get_extra_descriptor(char *buffer, unsigned size, unsigned char type, void **ptr);
int usb_get_status(struct usb_device *dev, int type, int target, void *data);
int usb_get_configuration(struct usb_device *dev);
int usb_get_protocol(struct usb_device *dev, int ifnum);
int usb_set_protocol(struct usb_device *dev, int ifnum, int protocol);
int usb_set_interface(struct usb_device *dev, int ifnum, int alternate);
int usb_set_idle(struct usb_device *dev, int ifnum, int duration, int report_id);
int usb_set_configuration(struct usb_device *dev, int configuration);
int usb_get_report(struct usb_device *dev, int ifnum, unsigned char type,
	unsigned char id, void *buf, int size);
int usb_set_report(struct usb_device *dev, int ifnum, unsigned char type,
	unsigned char id, void *buf, int size);
int usb_string(struct usb_device *dev, int index, char *buf, size_t size);
int usb_clear_halt(struct usb_device *dev, int pipe);
void usb_set_maxpacket(struct usb_device *dev);

#define usb_get_extra_descriptor(ifpoint,type,ptr)\
	__usb_get_extra_descriptor((ifpoint)->extra,(ifpoint)->extralen,type,(void**)ptr)

/*
 * Debugging helpers..
 */
void usb_show_device_descriptor(struct usb_device_descriptor *);
void usb_show_config_descriptor(struct usb_config_descriptor *);
void usb_show_interface_descriptor(struct usb_interface_descriptor *);
void usb_show_endpoint_descriptor(struct usb_endpoint_descriptor *);
void usb_show_device(struct usb_device *);
void usb_show_string(struct usb_device *dev, char *id, int index);

#ifdef DEBUG
#define dbg(format, arg...) printk(KERN_DEBUG __FILE__ ": " format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif
#define err(format, arg...) printk(KERN_ERR __FILE__ ": " format "\n" , ## arg)
#define info(format, arg...) printk(KERN_INFO __FILE__ ": " format "\n" , ## arg)
#define warn(format, arg...) printk(KERN_WARNING __FILE__ ": " format "\n" , ## arg)


/*
 * bus and driver list
 */

extern struct list_head usb_driver_list;
extern struct list_head usb_bus_list;
extern struct semaphore usb_bus_list_lock;

/*
 * USB device fs stuff
 */

#ifdef CONFIG_USB_DEVICEFS

/*
 * these are expected to be called from the USB core/hub thread
 * with the kernel lock held
 */
extern void usbdevfs_add_bus(struct usb_bus *bus);
extern void usbdevfs_remove_bus(struct usb_bus *bus);
extern void usbdevfs_add_device(struct usb_device *dev);
extern void usbdevfs_remove_device(struct usb_device *dev);

extern int usbdevfs_init(void);
extern void usbdevfs_cleanup(void);

#else /* CONFIG_USB_DEVICEFS */

static inline void usbdevfs_add_bus(struct usb_bus *bus) {}
static inline void usbdevfs_remove_bus(struct usb_bus *bus) {}
static inline void usbdevfs_add_device(struct usb_device *dev) {}
static inline void usbdevfs_remove_device(struct usb_device *dev) {}

static inline int usbdevfs_init(void) { return 0; }
static inline void usbdevfs_cleanup(void) { }

#endif /* CONFIG_USB_DEVICEFS */

#endif  /* __KERNEL__ */

#endif
