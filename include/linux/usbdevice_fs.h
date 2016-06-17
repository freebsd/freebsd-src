/*****************************************************************************/

/*
 *	usbdevice_fs.h  --  USB device file system.
 *
 *	Copyright (C) 2000
 *          Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  History:
 *   0.1  04.01.2000  Created
 *
 *  $Id: usbdevice_fs.h,v 1.1 2000/01/06 18:40:41 tom Exp $
 */

/*****************************************************************************/

#ifndef _LINUX_USBDEVICE_FS_H
#define _LINUX_USBDEVICE_FS_H

#include <linux/types.h>

/* --------------------------------------------------------------------- */

#define USBDEVICE_SUPER_MAGIC 0x9fa2

/* usbdevfs ioctl codes */

struct usbdevfs_ctrltransfer {
	__u8 requesttype;
	__u8 request;
	__u16 value;
	__u16 index;
	__u16 length;
	__u32 timeout;  /* in milliseconds */
 	void *data;
};

struct usbdevfs_bulktransfer {
	unsigned int ep;
	unsigned int len;
	unsigned int timeout; /* in milliseconds */
	void *data;
};

struct usbdevfs_setinterface {
	unsigned int interface;
	unsigned int altsetting;
};

struct usbdevfs_disconnectsignal {
	unsigned int signr;
	void *context;
};

#define USBDEVFS_MAXDRIVERNAME 255

struct usbdevfs_getdriver {
	unsigned int interface;
	char driver[USBDEVFS_MAXDRIVERNAME + 1];
};

struct usbdevfs_connectinfo {
	unsigned int devnum;
	unsigned char slow;
};

#define USBDEVFS_URB_DISABLE_SPD           1
#define USBDEVFS_URB_ISO_ASAP              2
#define USBDEVFS_URB_QUEUE_BULK            0x10

#define USBDEVFS_URB_TYPE_ISO		   0
#define USBDEVFS_URB_TYPE_INTERRUPT	   1
#define USBDEVFS_URB_TYPE_CONTROL	   2
#define USBDEVFS_URB_TYPE_BULK		   3

struct usbdevfs_iso_packet_desc {
	unsigned int length;
	unsigned int actual_length;
	unsigned int status;
};

struct usbdevfs_urb {
	unsigned char type;
	unsigned char endpoint;
	int status;
	unsigned int flags;
	void *buffer;
	int buffer_length;
	int actual_length;
	int start_frame;
	int number_of_packets;
	int error_count;
	unsigned int signr;  /* signal to be sent on error, -1 if none should be sent */
	void *usercontext;
	struct usbdevfs_iso_packet_desc iso_frame_desc[0];
};

/* ioctls for talking to drivers in the usbcore module: */
struct usbdevfs_ioctl {
	int	ifno;		/* interface 0..N ; negative numbers reserved */
	int	ioctl_code;	/* MUST encode size + direction of data so the
				 * macros in <asm/ioctl.h> give correct values */
	void	*data;		/* param buffer (in, or out) */
};

/* You can do most things with hubs just through control messages,
 * except find out what device connects to what port. */
struct usbdevfs_hub_portinfo {
	char nports;		/* number of downstream ports in this hub */
	char port [127];	/* e.g. port 3 connects to device 27 */
};

#define USBDEVFS_CONTROL           _IOWR('U', 0, struct usbdevfs_ctrltransfer)
#define USBDEVFS_BULK              _IOWR('U', 2, struct usbdevfs_bulktransfer)
#define USBDEVFS_RESETEP           _IOR('U', 3, unsigned int)
#define USBDEVFS_SETINTERFACE      _IOR('U', 4, struct usbdevfs_setinterface)
#define USBDEVFS_SETCONFIGURATION  _IOR('U', 5, unsigned int)
#define USBDEVFS_GETDRIVER         _IOW('U', 8, struct usbdevfs_getdriver)
#define USBDEVFS_SUBMITURB         _IOR('U', 10, struct usbdevfs_urb)
#define USBDEVFS_DISCARDURB        _IO('U', 11)
#define USBDEVFS_REAPURB           _IOW('U', 12, void *)
#define USBDEVFS_REAPURBNDELAY     _IOW('U', 13, void *)
#define USBDEVFS_DISCSIGNAL        _IOR('U', 14, struct usbdevfs_disconnectsignal)
#define USBDEVFS_CLAIMINTERFACE    _IOR('U', 15, unsigned int)
#define USBDEVFS_RELEASEINTERFACE  _IOR('U', 16, unsigned int)
#define USBDEVFS_CONNECTINFO       _IOW('U', 17, struct usbdevfs_connectinfo)
#define USBDEVFS_IOCTL             _IOWR('U', 18, struct usbdevfs_ioctl)
#define USBDEVFS_HUB_PORTINFO      _IOR('U', 19, struct usbdevfs_hub_portinfo)
#define USBDEVFS_RESET             _IO('U', 20)
#define USBDEVFS_CLEAR_HALT        _IOR('U', 21, unsigned int)
#define USBDEVFS_DISCONNECT        _IO('U', 22)
#define USBDEVFS_CONNECT           _IO('U', 23)

/* --------------------------------------------------------------------- */

#ifdef __KERNEL__

#include <linux/list.h>
#include <asm/semaphore.h>

/*
 * inode number macros
 */
#define ITYPE(x)   ((x)&(0xf<<28))
#define ISPECIAL   (0<<28)
#define IBUS       (1<<28)
#define IDEVICE    (2<<28)
#define IBUSNR(x)  (((x)>>8)&0xff)
#define IDEVNR(x)  ((x)&0xff)

#define IROOT      1

struct dev_state {
	struct list_head list;      /* state list */
	struct rw_semaphore devsem; /* protects modifications to dev (dev == NULL indicating disconnect) */ 
	struct usb_device *dev;
	struct file *file;
	spinlock_t lock;            /* protects the async urb lists */
	struct list_head async_pending;
	struct list_head async_completed;
	wait_queue_head_t wait;     /* wake up if a request completed */
	unsigned int discsignr;
	struct task_struct *disctask;
	void *disccontext;
	unsigned long ifclaimed;
};

/* internal methods & data */
extern struct usb_driver usbdevfs_driver;
extern struct file_operations usbdevfs_drivers_fops;
extern struct file_operations usbdevfs_devices_fops;
extern struct file_operations usbdevfs_device_file_operations;
extern struct inode_operations usbdevfs_device_inode_operations;
extern struct inode_operations usbdevfs_bus_inode_operations;
extern struct file_operations usbdevfs_bus_file_operations;
extern void usbdevfs_conn_disc_event(void);

#endif /* __KERNEL__ */

/* --------------------------------------------------------------------- */
#endif /* _LINUX_USBDEVICE_FS_H */
