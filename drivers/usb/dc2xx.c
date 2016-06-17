/*
 * Copyright (C) 1999-2000 by David Brownell <dbrownell@users.sourceforge.net>
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
 
 
/*
 * USB driver for Kodak DC-2XX series digital still cameras
 *
 * The protocol here is the same as the one going over a serial line, but
 * it uses USB for speed.  Set up /dev/kodak, get gphoto (www.gphoto.org),
 * and have fun!
 *
 * This should also work for a number of other digital (non-Kodak) cameras,
 * by adding the vendor and product IDs to the table below.  They'll need
 * to be the sort using USB just as a fast bulk data channel.
 */

/*
 * HISTORY
 *
 * 26 August, 1999 -- first release (0.1), works with my DC-240.
 * 	The DC-280 (2Mpixel) should also work, but isn't tested.
 *	If you use gphoto, make sure you have the USB updates.
 *	Lives in a 2.3.14 or so Linux kernel, in drivers/usb.
 * 31 August, 1999 -- minor update to recognize DC-260 and handle
 *	its endpoints being in a different order.  Note that as
 *	of gPhoto 0.36pre, the USB updates are integrated.
 * 12 Oct, 1999 -- handle DC-280 interface class (0xff not 0x0);
 *	added timeouts to bulk_msg calls.  Minor updates, docs.
 * 03 Nov, 1999 -- update for 2.3.25 kernel API changes.
 * 08 Jan, 2000 .. multiple camera support
 * 12 Aug, 2000 .. add some real locking, remove an Oops
 * 10 Oct, 2000 .. usb_device_id table created. 
 * 01 Nov, 2000 .. usb_device_id support added by Adam J. Richter
 * 08 Apr, 2001 .. Identify version on module load. gb
 *
 * Thanks to:  the folk who've provided USB product IDs, sent in
 * patches, and shared their successes!
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/devfs_fs_kernel.h>

#ifdef CONFIG_USB_DEBUG
	#define DEBUG
#else
	#undef DEBUG
#endif
#include <linux/usb.h>


/* /dev/usb dir. */
extern devfs_handle_t usb_devfs_handle;			

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.0.0"
#define DRIVER_AUTHOR "David Brownell, <dbrownell@users.sourceforge.net>"
#define DRIVER_DESC "USB Camera Driver for Kodak DC-2xx series cameras"


/* current USB framework handles max of 16 USB devices per driver */
#define	MAX_CAMERAS		16

/* USB char devs use USB_MAJOR and from USB_CAMERA_MINOR_BASE up */
#define	USB_CAMERA_MINOR_BASE	80


// XXX remove packet size limit, now that bulk transfers seem fixed

/* Application protocol limit is 0x8002; USB has disliked that limit! */
#define	MAX_PACKET_SIZE		0x2000		/* e.g. image downloading */

#define	MAX_READ_RETRY		5		/* times to retry reads */
#define	MAX_WRITE_RETRY		5		/* times to retry writes */
#define	RETRY_TIMEOUT		(HZ)		/* sleep between retries */


/* table of cameras that work through this driver */
static struct usb_device_id camera_table [] = {
	/* These have the same application level protocol */  
	{ USB_DEVICE(0x040a, 0x0120) },		// Kodak DC-240
	{ USB_DEVICE(0x040a, 0x0130) },		// Kodak DC-280
	{ USB_DEVICE(0x040a, 0x0131) },		// Kodak DC-5000
	{ USB_DEVICE(0x040a, 0x0132) },		// Kodak DC-3400

	/* These have a different application level protocol which
	 * is part of the Flashpoint "DigitaOS".  That supports some
	 * non-camera devices, and some non-Kodak cameras.
	 * Use this driver to get USB and "OpenDis" to talk.
	 */  
	{ USB_DEVICE(0x040a, 0x0100) },		// Kodak DC-220
	{ USB_DEVICE(0x040a, 0x0110) },		// Kodak DC-260
	{ USB_DEVICE(0x040a, 0x0111) },		// Kodak DC-265
	{ USB_DEVICE(0x040a, 0x0112) },		// Kodak DC-290
	{ USB_DEVICE(0xf003, 0x6002) },		// HP PhotoSmart C500
	{ USB_DEVICE(0x03f0, 0x4102) },		// HP PhotoSmart C618
	{ USB_DEVICE(0x0a17, 0x1001) },		// Pentax EI-200

	/* Other USB devices may well work here too, so long as they
	 * just stick to half duplex bulk packet exchanges.  That
	 * means, among other things, no iso or interrupt endpoints.
	 */

	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, camera_table);


struct camera_state {
	struct usb_device	*dev;		/* USB device handle */
	int			inEP;		/* read endpoint */
	int			outEP;		/* write endpoint */
	const struct usb_device_id	*info;	/* DC-240, etc */
	int			subminor;	/* which minor dev #? */
	struct semaphore	sem;		/* locks this struct */

	/* this is non-null iff the device is open */
	char			*buf;		/* buffer for I/O */

	devfs_handle_t		devfs;		/* devfs device */

	/* always valid */
	wait_queue_head_t	wait;		/* for timed waits */
};

/* Support multiple cameras, possibly of different types.  */
static struct camera_state *minor_data [MAX_CAMERAS];

/* make this an rwlock if contention becomes an issue */
static DECLARE_MUTEX (state_table_mutex);

static ssize_t camera_read (struct file *file,
	char *buf, size_t len, loff_t *ppos)
{
	struct camera_state	*camera;
	int			retries;
	int			retval = 0;

	if (len > MAX_PACKET_SIZE)
		return -EINVAL;

	camera = (struct camera_state *) file->private_data;
	down (&camera->sem);
	if (!camera->dev) {
		up (&camera->sem);
		return -ENODEV;
	}

	/* Big reads are common, for image downloading.  Smaller ones
	 * are also common (even "directory listing" commands don't
	 * send very much data).  We preserve packet boundaries here,
	 * they matter in the application protocol.
	 */
	for (retries = 0; retries < MAX_READ_RETRY; retries++) {
		int			count;

		if (signal_pending (current)) {
			retval = -EINTR;
			break;
		}

		retval = usb_bulk_msg (camera->dev,
			  usb_rcvbulkpipe (camera->dev, camera->inEP),
			  camera->buf, len, &count, HZ*10);

		dbg ("read (%Zd) - 0x%x %d", len, retval, count);

		if (!retval) {
			if (copy_to_user (buf, camera->buf, count))
				retval = -EFAULT;
			else
				retval = count;
			break;
		}
		if (retval != USB_ST_TIMEOUT)
			break;
		interruptible_sleep_on_timeout (&camera->wait, RETRY_TIMEOUT);

		dbg ("read (%Zd) - retry", len);
	}
	up (&camera->sem);
	return retval;
}

static ssize_t camera_write (struct file *file,
	const char *buf, size_t len, loff_t *ppos)
{
	struct camera_state	*camera;
	ssize_t			bytes_written = 0;

	if (len > MAX_PACKET_SIZE)
		return -EINVAL;

	camera = (struct camera_state *) file->private_data;
	down (&camera->sem);
	if (!camera->dev) {
		up (&camera->sem);
		return -ENODEV;
	}
	
	/* most writes will be small: simple commands, sometimes with
	 * parameters.  putting images (like borders) into the camera
	 * would be the main use of big writes.
	 */
	while (len > 0) {
		char		*obuf = camera->buf;
		int		maxretry = MAX_WRITE_RETRY;
		unsigned long	copy_size, thistime;

		/* it's not clear that retrying can do any good ... or that
		 * fragmenting application packets into N writes is correct.
		 */
		thistime = copy_size = len;
		if (copy_from_user (obuf, buf, copy_size)) {
			bytes_written = -EFAULT;
			break;
		}
		while (thistime) {
			int		result;
			int		count;

			if (signal_pending (current)) {
				if (!bytes_written)
					bytes_written = -EINTR;
				goto done;
			}

			result = usb_bulk_msg (camera->dev,
				 usb_sndbulkpipe (camera->dev, camera->outEP),
				 obuf, thistime, &count, HZ*10);

			if (result)
				dbg ("write USB err - %d", result);

			if (count) {
				obuf += count;
				thistime -= count;
				maxretry = MAX_WRITE_RETRY;
				continue;
			} else if (!result)
				break;
				
			if (result == USB_ST_TIMEOUT) {	/* NAK - delay a bit */
				if (!maxretry--) {
					if (!bytes_written)
						bytes_written = -ETIME;
					goto done;
				}
                                interruptible_sleep_on_timeout (&camera->wait,
					RETRY_TIMEOUT);
				continue;
			} 
			if (!bytes_written)
				bytes_written = -EIO;
			goto done;
		}
		bytes_written += copy_size;
		len -= copy_size;
		buf += copy_size;
	}
done:
	up (&camera->sem);
	dbg ("wrote %Zd", bytes_written); 
	return bytes_written;
}

static int camera_open (struct inode *inode, struct file *file)
{
	struct camera_state	*camera = NULL;
	int			subminor;
	int			value = 0;

	down (&state_table_mutex);
	subminor = MINOR (inode->i_rdev) - USB_CAMERA_MINOR_BASE;
	if (subminor < 0 || subminor >= MAX_CAMERAS
			|| !(camera = minor_data [subminor])) {
		up (&state_table_mutex);
		return -ENODEV;
	}
	down (&camera->sem);
	up (&state_table_mutex);

	if (camera->buf) {
		value = -EBUSY;
		goto done;
	}

	if (!(camera->buf = (char *) kmalloc (MAX_PACKET_SIZE, GFP_KERNEL))) {
		value = -ENOMEM;
		goto done;
	}

	dbg ("open #%d", subminor); 

	file->private_data = camera;
done:
	up (&camera->sem);
	return value;
}

static int camera_release (struct inode *inode, struct file *file)
{
	struct camera_state	*camera;
	int			subminor;

	camera = (struct camera_state *) file->private_data;
	down (&state_table_mutex);
	down (&camera->sem);

	if (camera->buf) {
		kfree (camera->buf);
		camera->buf = 0;
	}
	subminor = camera->subminor;

	/* If camera was unplugged with open file ... */
	if (!camera->dev) {
		minor_data [subminor] = NULL;
		kfree (camera);
	} else
		up (&camera->sem);
	
	up (&state_table_mutex);

	dbg ("close #%d", subminor); 

	return 0;
}

	/* XXX should define some ioctls to expose camera type
	 * to applications ... what USB exposes should suffice.
	 * apps should be able to see the camera type.
	 */
static /* const */ struct file_operations usb_camera_fops = {
	    /* Uses GCC initializer extension; simpler to maintain */
	owner:		THIS_MODULE,
	read:		camera_read,
	write:		camera_write,
	open:		camera_open,
	release:	camera_release,
};



static void *
camera_probe (struct usb_device *dev, unsigned int ifnum, const struct usb_device_id *camera_info)
{
	int				i;
	struct usb_interface_descriptor	*interface;
	struct usb_endpoint_descriptor	*endpoint;
	int				direction, ep;
	char name[8];
	struct camera_state		*camera = NULL;


	/* these have one config, one interface */
	if (dev->descriptor.bNumConfigurations != 1
			|| dev->config[0].bNumInterfaces != 1) {
		dbg ("Bogus camera config info");
		return NULL;
	}

	/* models differ in how they report themselves */
	interface = &dev->actconfig->interface[ifnum].altsetting[0];
	if ((interface->bInterfaceClass != USB_CLASS_PER_INTERFACE
		&& interface->bInterfaceClass != USB_CLASS_VENDOR_SPEC)
			|| interface->bInterfaceSubClass != 0
			|| interface->bInterfaceProtocol != 0
			|| interface->bNumEndpoints != 2
			) {
		dbg ("Bogus camera interface info");
		return NULL;
	}


	/* select "subminor" number (part of a minor number) */
	down (&state_table_mutex);
	for (i = 0; i < MAX_CAMERAS; i++) {
		if (!minor_data [i])
			break;
	}
	if (i >= MAX_CAMERAS) {
		info ("Ignoring additional USB Camera");
		goto bye;
	}

	/* allocate & init camera state */
	camera = minor_data [i] = kmalloc (sizeof *camera, GFP_KERNEL);
	if (!camera) {
		err ("no memory!");
		goto bye;
	}

	init_MUTEX (&camera->sem);
	camera->info = camera_info;
	camera->subminor = i;
	camera->buf = NULL;
	init_waitqueue_head (&camera->wait);


	/* get input and output endpoints (either order) */
	endpoint = interface->endpoint;
	camera->outEP = camera->inEP =  -1;

	ep = endpoint [0].bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
	direction = endpoint [0].bEndpointAddress & USB_ENDPOINT_DIR_MASK;
	if (direction == USB_DIR_IN)
		camera->inEP = ep;
	else
		camera->outEP = ep;

	ep = endpoint [1].bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
	direction = endpoint [1].bEndpointAddress & USB_ENDPOINT_DIR_MASK;
	if (direction == USB_DIR_IN)
		camera->inEP = ep;
	else
		camera->outEP = ep;

	if (camera->outEP == -1 || camera->inEP == -1
			|| endpoint [0].bmAttributes != USB_ENDPOINT_XFER_BULK
			|| endpoint [1].bmAttributes != USB_ENDPOINT_XFER_BULK
			) {
		dbg ("Bogus endpoints");
		goto error;
	}

	info ("USB Camera #%d connected, major/minor %d/%d", camera->subminor,
		USB_MAJOR, USB_CAMERA_MINOR_BASE + camera->subminor);

	camera->dev = dev;
	usb_inc_dev_use (dev);

	/* If we have devfs, register the device */
	sprintf(name, "dc2xx%d", camera->subminor);
	camera->devfs = devfs_register(usb_devfs_handle, name,
				       DEVFS_FL_DEFAULT, USB_MAJOR,
				       USB_CAMERA_MINOR_BASE + camera->subminor,
				       S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP |
				       S_IWGRP, &usb_camera_fops, NULL);

	goto bye;

error:
	minor_data [camera->subminor] = NULL;
	kfree (camera);
	camera = NULL;
bye:
	up (&state_table_mutex);
	return camera;
}

static void camera_disconnect(struct usb_device *dev, void *ptr)
{
	struct camera_state	*camera = (struct camera_state *) ptr;
	int			subminor = camera->subminor;

	down (&state_table_mutex);
	down (&camera->sem);

	devfs_unregister(camera->devfs); 

	/* If camera's not opened, we can clean up right away.
	 * Else apps see a disconnect on next I/O; the release cleans.
	 */
	if (!camera->buf) {
		minor_data [subminor] = NULL;
		kfree (camera);
		camera = NULL;
	} else
		camera->dev = NULL;

	info ("USB Camera #%d disconnected", subminor);
	usb_dec_dev_use (dev);

	if (camera != NULL)
		up (&camera->sem);
	up (&state_table_mutex);
}

static /* const */ struct usb_driver camera_driver = {
	name:		"dc2xx",

	id_table:	camera_table,
	probe:		camera_probe,
	disconnect:	camera_disconnect,

	fops:		&usb_camera_fops,
	minor:		USB_CAMERA_MINOR_BASE
};


int __init usb_dc2xx_init(void)
{
 	if (usb_register (&camera_driver) < 0)
 		return -1;
	info(DRIVER_VERSION ":" DRIVER_DESC);
	return 0;
}

void __exit usb_dc2xx_cleanup(void)
{
	usb_deregister (&camera_driver);
}

module_init (usb_dc2xx_init);
module_exit (usb_dc2xx_cleanup);

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

