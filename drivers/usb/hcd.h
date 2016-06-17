/*
 * Copyright (c) 2001 by David Brownell
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


/*-------------------------------------------------------------------------*/

/*
 * USB Host Controller Driver (usb_hcd) framework
 *
 * Since "struct usb_bus" is so thin, you can't share much code in it.
 * This framework is a layer over that, and should be more sharable.
 */

/*-------------------------------------------------------------------------*/

struct usb_hcd {	/* usb_bus.hcpriv points to this */

	/*
	 * housekeeping
	 */
	struct usb_bus		*bus;		/* hcd is-a bus */
	struct list_head	hcd_list;

	const char		*product_desc;
	const char		*description;	/* "ehci-hcd" etc */

	struct timer_list	rh_timer;	/* drives root hub */
	struct list_head	dev_list;	/* devices on this bus */
	struct tq_struct	work;

	/*
	 * hardware info/state
	 */
	struct hc_driver	*driver;	/* hw-specific hooks */
	int			irq;		/* irq allocated */
	void			*regs;		/* device memory/io */

#ifdef	CONFIG_PCI
	/* a few non-PCI controllers exist, mostly for OHCI */
	struct pci_dev		*pdev;		/* pci is typical */
	int			region;		/* pci region for regs */
	u32			pci_state [16];	/* for PM state save */
	atomic_t		resume_count;	/* multiple resumes issue */
#endif

	int			state;
#	define	__ACTIVE		0x01
#	define	__SLEEPY		0x02
#	define	__SUSPEND		0x04
#	define	__TRANSIENT		0x80

#	define	USB_STATE_HALT		0
#	define	USB_STATE_RUNNING	(__ACTIVE)
#	define	USB_STATE_READY		(__ACTIVE|__SLEEPY)
#	define	USB_STATE_QUIESCING	(__SUSPEND|__TRANSIENT|__ACTIVE)
#	define	USB_STATE_RESUMING	(__SUSPEND|__TRANSIENT)
#	define	USB_STATE_SUSPENDED	(__SUSPEND)

#define	HCD_IS_RUNNING(state) ((state) & __ACTIVE)
#define	HCD_IS_SUSPENDED(state) ((state) & __SUSPEND)

	/* more shared queuing code would be good; it should support
	 * smarter scheduling, handle transaction translators, etc;
	 * input size of periodic table to an interrupt scheduler. 
	 * (ohci 32, uhci 1024, ehci 256/512/1024).
	 */
};

struct hcd_dev {	/* usb_device.hcpriv points to this */
	struct list_head	dev_list;	/* on this hcd */
	struct list_head	urb_list;	/* pending on this dev */

	/* per-configuration HC/HCD state, such as QH or ED */
	void			*ep[32];
};

// urb.hcpriv is really hardware-specific

struct hcd_timeout {	/* timeouts we allocate */
	struct list_head	timeout_list;
	struct timer_list	timer;
};

/*-------------------------------------------------------------------------*/

/* each driver provides one of these, and hardware init support */

struct hc_driver {
	const char	*description;	/* "ehci-hcd" etc */

	/* irq handler */
	void	(*irq) (struct usb_hcd *hcd, struct pt_regs *regs);

	int	flags;
#define	HCD_MEMORY	0x0001		/* HC regs use memory (else I/O) */
#define	HCD_USB11	0x0010		/* USB 1.1 */
#define	HCD_USB2	0x0020		/* USB 2.0 */

	/* called to init HCD and root hub */
	int	(*start) (struct usb_hcd *hcd);

	/* called after all devices were suspended */
	int	(*suspend) (struct usb_hcd *hcd, u32 state);

	/* called before any devices get resumed */
	int	(*resume) (struct usb_hcd *hcd);

	/* cleanly make HCD stop writing memory and doing I/O */
	void	(*stop) (struct usb_hcd *hcd);

	/* return current frame number */
	int	(*get_frame_number) (struct usb_hcd *hcd);

// FIXME: rework generic-to-specific HCD linkage (specific contains generic)

	/* memory lifecycle */
	struct usb_hcd	*(*hcd_alloc) (void);
	void		(*hcd_free) (struct usb_hcd *hcd);

	/* manage i/o requests, device state */
	int	(*urb_enqueue) (struct usb_hcd *hcd, struct urb *urb,
					int mem_flags);
	int	(*urb_dequeue) (struct usb_hcd *hcd, struct urb *urb);

	// frees configuration resources -- allocated as needed during
	// urb_enqueue, and not freed by urb_dequeue
	void		(*free_config) (struct usb_hcd *hcd,
				struct usb_device *dev);

	/* root hub support */
	int		(*hub_status_data) (struct usb_hcd *hcd, char *buf);
	int		(*hub_control) (struct usb_hcd *hcd,
				u16 typeReq, u16 wValue, u16 wIndex,
				char *buf, u16 wLength);
};

extern void usb_hcd_giveback_urb (struct usb_hcd *hcd, struct urb *urb,
		struct pt_regs *regs);

#ifdef CONFIG_PCI

struct pci_device_id;
extern int usb_hcd_pci_probe (struct pci_dev *dev,
				const struct pci_device_id *id);
extern void usb_hcd_pci_remove (struct pci_dev *dev);

#ifdef CONFIG_PM
// FIXME:  see Documentation/power/pci.txt (2.4.6 and later?)
// extern int usb_hcd_pci_save_state (struct pci_dev *dev, u32 state);
extern int usb_hcd_pci_suspend (struct pci_dev *dev, u32 state);
extern int usb_hcd_pci_resume (struct pci_dev *dev);
// extern int usb_hcd_pci_enable_wake (struct pci_dev *dev, u32 state, int flg);
#endif /* CONFIG_PM */

#endif /* CONFIG_PCI */

/*-------------------------------------------------------------------------*/

/*
 * HCD Root Hub support
 */

#include "hub.h"

/* (shifted) direction/type/recipient from the USB 2.0 spec, table 9.2 */
#define DeviceRequest \
	((USB_DIR_IN|USB_TYPE_STANDARD|USB_RECIP_DEVICE)<<8)
#define DeviceOutRequest \
	((USB_DIR_OUT|USB_TYPE_STANDARD|USB_RECIP_DEVICE)<<8)

#define InterfaceRequest \
	((USB_DIR_IN|USB_TYPE_STANDARD|USB_RECIP_INTERFACE)<<8)

#define EndpointRequest \
	((USB_DIR_IN|USB_TYPE_STANDARD|USB_RECIP_INTERFACE)<<8)
#define EndpointOutRequest \
	((USB_DIR_OUT|USB_TYPE_STANDARD|USB_RECIP_INTERFACE)<<8)

/* table 9.6 standard features */
#define DEVICE_REMOTE_WAKEUP	1
#define ENDPOINT_HALT		0

/* class requests from the USB 2.0 hub spec, table 11-15 */
/* GetBusState and SetHubDescriptor are optional, omitted */
#define ClearHubFeature		(0x2000 | USB_REQ_CLEAR_FEATURE)
#define ClearPortFeature	(0x2300 | USB_REQ_CLEAR_FEATURE)
#define GetHubDescriptor	(0xa000 | USB_REQ_GET_DESCRIPTOR)
#define GetHubStatus		(0xa000 | USB_REQ_GET_STATUS)
#define GetPortStatus		(0xa300 | USB_REQ_GET_STATUS)
#define SetHubFeature		(0x2000 | USB_REQ_SET_FEATURE)
#define SetPortFeature		(0x2300 | USB_REQ_SET_FEATURE)


/*-------------------------------------------------------------------------*/

/*
 * Generic bandwidth allocation constants/support
 */
#define FRAME_TIME_USECS	1000L
#define BitTime(bytecount)  (7 * 8 * bytecount / 6)  /* with integer truncation */
		/* Trying not to use worst-case bit-stuffing
                   of (7/6 * 8 * bytecount) = 9.33 * bytecount */
		/* bytecount = data payload byte count */

#define NS_TO_US(ns)	((ns + 500L) / 1000L)
			/* convert & round nanoseconds to microseconds */

extern void usb_claim_bandwidth (struct usb_device *dev, struct urb *urb,
		int bustime, int isoc);
extern void usb_release_bandwidth (struct usb_device *dev, struct urb *urb,
		int isoc);

/*
 * Full/low speed bandwidth allocation constants/support.
 */
#define BW_HOST_DELAY	1000L		/* nanoseconds */
#define BW_HUB_LS_SETUP	333L		/* nanoseconds */
                        /* 4 full-speed bit times (est.) */

#define FRAME_TIME_BITS         12000L		/* frame = 1 millisecond */
#define FRAME_TIME_MAX_BITS_ALLOC	(90L * FRAME_TIME_BITS / 100L)
#define FRAME_TIME_MAX_USECS_ALLOC	(90L * FRAME_TIME_USECS / 100L)

extern int usb_check_bandwidth (struct usb_device *dev, struct urb *urb);

/*
 * Ceiling microseconds (typical) for that many bytes at high speed
 * ISO is a bit less, no ACK ... from USB 2.0 spec, 5.11.3 (and needed
 * to preallocate bandwidth)
 */
#define USB2_HOST_DELAY	5	/* nsec, guess */
#define HS_USECS(bytes) NS_TO_US ( ((55 * 8 * 2083)/1000) \
	+ ((2083UL * (3167 + BitTime (bytes)))/1000) \
	+ USB2_HOST_DELAY)
#define HS_USECS_ISO(bytes) NS_TO_US ( ((long)(38 * 8 * 2.083)) \
	+ ((2083UL * (3167 + BitTime (bytes)))/1000) \
	+ USB2_HOST_DELAY)

extern long usb_calc_bus_time (int speed, int is_input,
			int isoc, int bytecount);

/*-------------------------------------------------------------------------*/

/* hub.h ... DeviceRemovable in 2.4.2-ac11, gone in 2.4.10 */
// bleech -- resurfaced in 2.4.11 or 2.4.12
#define bitmap 	DeviceRemovable


/*-------------------------------------------------------------------------*/

/* random stuff */

#define	RUN_CONTEXT (in_irq () ? "in_irq" \
		: (in_interrupt () ? "in_interrupt" : "can sleep"))

/* 2.5 changes ... */

#ifndef container_of
#define	container_of	list_entry
#endif

#define usb_get_urb(x) (x)
#define usb_put_urb(x)

static inline struct usb_bus *hcd_to_bus (struct usb_hcd *hcd)
	{ return hcd->bus; }

static inline void
usb_hub_tt_clear_buffer (struct usb_device *dev, int pipe)
	{ }

#define URB_ZERO_PACKET	USB_ZERO_PACKET
#define URB_ISO_ASAP	USB_ISO_ASAP
