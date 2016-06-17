/* Hey EMACS -*- linux-c -*-
 *
 * tiglusb -- Texas Instruments' USB GraphLink (aka SilverLink) driver.
 * Target: Texas Instruments graphing calculators (http://lpg.ticalc.org).
 *
 * Copyright (C) 2001-2002:
 *   Romain Lievin <roms@lpg.ticalc.org>
 *   Julien BLACHE <jb@technologeek.org>
 * under the terms of the GNU General Public License.
 *
 * Based on dabusb.c, printer.c & scanner.c
 *
 * Please see the file: linux/Documentation/usb/SilverLink.txt
 * and the website at:  http://lpg.ticalc.org/prj_usb/
 * for more info.
 *
 *   1.0x, Romain & Julien: initial submit.
 *   1.03, Greg Kroah: modifications.
 *   1.04, Julien: clean-up & fixes; Romain: 2.4 backport.
 *   1.05, Randy Dunlap: bug fix with the timeout parameter (divide-by-zero).
 *   1.06, Romain: synched with 2.5, version/firmware changed (confusing).
 */

#include <linux/module.h>
#include <linux/socket.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/usb.h>
#include <linux/smp_lock.h>
#include <linux/devfs_fs_kernel.h>

#include <linux/ticable.h>
#include "tiglusb.h"

/*
 * Version Information
 */
#define DRIVER_VERSION "1.06"
#define DRIVER_AUTHOR  "Romain Lievin <roms@lpg.ticalc.org> & Julien Blache <jb@jblache.org>"
#define DRIVER_DESC    "TI-GRAPH LINK USB (aka SilverLink) driver"
#define DRIVER_LICENSE "GPL"

/* ----- global variables --------------------------------------------- */

static tiglusb_t tiglusb[MAXTIGL];
static int timeout = TIMAXTIME;	/* timeout in tenth of seconds     */

static devfs_handle_t devfs_handle;

/*---------- misc functions ------------------------------------------- */

/*
 * Re-initialize device
 */
static inline int
clear_device (struct usb_device *dev)
{
	if (usb_set_configuration (dev, dev->config[0].bConfigurationValue) < 0) {
		err ("clear_device failed");
		return -1;
	}

	return 0;
}

/* 
 * Clear input & output pipes (endpoints)
 */
static inline int
clear_pipes (struct usb_device *dev)
{
	unsigned int pipe;

	pipe = usb_sndbulkpipe (dev, 1);
	if (usb_clear_halt (dev, usb_pipeendpoint (pipe))) {
		err ("clear_pipe (r), request failed");
		return -1;
	}

	pipe = usb_sndbulkpipe (dev, 2);
	if (usb_clear_halt (dev, usb_pipeendpoint (pipe))) {
		err ("clear_pipe (w), request failed");
		return -1;
	}

	return 0;
}

/* ----- file operations functions--------------------------------------- */

static int
tiglusb_open (struct inode *inode, struct file *filp)
{
	int devnum = minor (inode->i_rdev);
	ptiglusb_t s;

	if (devnum < TIUSB_MINOR || devnum >= (TIUSB_MINOR + MAXTIGL))
		return -EIO;

	s = &tiglusb[devnum - TIUSB_MINOR];

	if (down_interruptible (&s->mutex)) {
		return -ERESTARTSYS;
	}

	while (!s->dev || s->opened) {
		up (&s->mutex);

		if (filp->f_flags & O_NONBLOCK) {
			return -EBUSY;
		}

		schedule_timeout (HZ / 2);

		if (signal_pending (current)) {
			return -EAGAIN;
		}

		if (down_interruptible (&s->mutex)) {
			return -ERESTARTSYS;
		}
	}

	s->opened = 1;
	up (&s->mutex);

	filp->f_pos = 0;
	filp->private_data = s;

	return 0;
}

static int
tiglusb_release (struct inode *inode, struct file *filp)
{
	ptiglusb_t s = (ptiglusb_t) filp->private_data;

	if (down_interruptible (&s->mutex)) {
		return -ERESTARTSYS;
	}

	s->state = _stopped;
	up (&s->mutex);

	if (!s->remove_pending)
		clear_device (s->dev);
	else
		wake_up (&s->remove_ok);

	s->opened = 0;

	return 0;
}

static ssize_t
tiglusb_read (struct file *filp, char *buf, size_t count, loff_t * f_pos)
{
	ptiglusb_t s = (ptiglusb_t) filp->private_data;
	ssize_t ret = 0;
	int bytes_to_read = 0;
	int bytes_read = 0;
	int result = 0;
	char buffer[BULK_RCV_MAX];
	unsigned int pipe;

	if (*f_pos)
		return -ESPIPE;

	if (s->remove_pending)
		return -EIO;

	if (!s->dev)
		return -EIO;

	bytes_to_read = (count >= BULK_RCV_MAX) ? BULK_RCV_MAX : count;

	pipe = usb_rcvbulkpipe (s->dev, 1);
	result = usb_bulk_msg (s->dev, pipe, buffer, bytes_to_read,
			       &bytes_read, HZ * 10 / timeout);
	if (result == -ETIMEDOUT) {	/* NAK */
		ret = result;
		if (!bytes_read) {
			dbg ("quirk !");
		}
		warn ("tiglusb_read, NAK received.");
		goto out;
	} else if (result == -EPIPE) {	/* STALL -- shouldn't happen */
		warn ("clear_halt request to remove STALL condition.");
		if (usb_clear_halt (s->dev, usb_pipeendpoint (pipe)))
			err ("clear_halt, request failed");
		clear_device (s->dev);
		ret = result;
		goto out;
	} else if (result < 0) {	/* We should not get any I/O errors */
		err ("funky result: %d. Please notify maintainer.", result);
		ret = -EIO;
		goto out;
	}

	if (copy_to_user (buf, buffer, bytes_read)) {
		ret = -EFAULT;
	}

      out:
	return ret ? ret : bytes_read;
}

static ssize_t
tiglusb_write (struct file *filp, const char *buf, size_t count, loff_t * f_pos)
{
	ptiglusb_t s = (ptiglusb_t) filp->private_data;
	ssize_t ret = 0;
	int bytes_to_write = 0;
	int bytes_written = 0;
	int result = 0;
	char buffer[BULK_SND_MAX];
	unsigned int pipe;

	if (*f_pos)
		return -ESPIPE;

	if (s->remove_pending)
		return -EIO;

	if (!s->dev)
		return -EIO;

	bytes_to_write = (count >= BULK_SND_MAX) ? BULK_SND_MAX : count;
	if (copy_from_user (buffer, buf, bytes_to_write)) {
		ret = -EFAULT;
		goto out;
	}

	pipe = usb_sndbulkpipe (s->dev, 2);
	result = usb_bulk_msg (s->dev, pipe, buffer, bytes_to_write,
			       &bytes_written, HZ * 10 / timeout);

	if (result == -ETIMEDOUT) {	/* NAK */
		warn ("tiglusb_write, NAK received.");
		ret = result;
		goto out;
	} else if (result == -EPIPE) {	/* STALL -- shouldn't happen */
		warn ("clear_halt request to remove STALL condition.");
		if (usb_clear_halt (s->dev, usb_pipeendpoint (pipe)))
			err ("clear_halt, request failed");
		clear_device (s->dev);
		ret = result;
		goto out;
	} else if (result < 0) {	/* We should not get any I/O errors */
		warn ("funky result: %d. Please notify maintainer.", result);
		ret = -EIO;
		goto out;
	}

	if (bytes_written != bytes_to_write) {
		ret = -EIO;
	}

      out:
	return ret ? ret : bytes_written;
}

static int
tiglusb_ioctl (struct inode *inode, struct file *filp,
	       unsigned int cmd, unsigned long arg)
{
	ptiglusb_t s = (ptiglusb_t) filp->private_data;
	int ret = 0;

	if (s->remove_pending)
		return -EIO;

	if (down_interruptible (&s->mutex)) {
		return -ERESTARTSYS;
	}

	if (!s->dev) {
		up (&s->mutex);
		return -EIO;
	}

	switch (cmd) {
	case IOCTL_TIUSB_TIMEOUT:
		timeout = arg;	// timeout value in tenth of seconds
		break;
	case IOCTL_TIUSB_RESET_DEVICE:
		dbg ("IOCTL_TIGLUSB_RESET_DEVICE");
		if (clear_device (s->dev))
			ret = -EIO;
		break;
	case IOCTL_TIUSB_RESET_PIPES:
		dbg ("IOCTL_TIGLUSB_RESET_PIPES");
		if (clear_pipes (s->dev))
			ret = -EIO;
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	up (&s->mutex);

	return ret;
}

/* ----- kernel module registering ------------------------------------ */

static struct file_operations tiglusb_fops = {
	.owner =        THIS_MODULE,
	.llseek =	no_llseek,
	.read =		tiglusb_read,
	.write =	tiglusb_write,
	.ioctl =	tiglusb_ioctl,
	.open =		tiglusb_open,
	.release =	tiglusb_release,
};

/* --- initialisation code ------------------------------------- */

static void *
tiglusb_probe (struct usb_device *dev, unsigned int ifnum,
	       const struct usb_device_id *id)
{
	int minor = -1;
	int i;
	ptiglusb_t s;
	char name[8];

	dbg ("probing vendor id 0x%x, device id 0x%x ifnum:%d",
	     dev->descriptor.idVendor, dev->descriptor.idProduct, ifnum);

	/*
	 * We don't handle multiple configurations. As of version 0x0103 of
	 * the TIGL hardware, there's only 1 configuration.
	 */

	if (dev->descriptor.bNumConfigurations != 1)
		return NULL;

	if ((dev->descriptor.idProduct != 0xe001)
	    && (dev->descriptor.idVendor != 0x451))
		return NULL;

	if (usb_set_configuration (dev, dev->config[0].bConfigurationValue) < 0) {
		err ("tiglusb_probe: set_configuration failed");
		return NULL;
	}

	/*
	 * Find a tiglusb struct
	 */
	for (i = 0; i < MAXTIGL; i++) {
		ptiglusb_t s = &tiglusb[i];
		if (!s->dev) {
			minor = i;
			break;
		}
	}

	if (minor == -1)
		return NULL;

	s = &tiglusb[minor];

	down (&s->mutex);
	s->remove_pending = 0;
	s->dev = dev;
	up (&s->mutex);
	dbg ("bound to interface: %d", ifnum);

	sprintf (name, "%d", s->minor);
	dbg ("registering to devfs : major = %d, minor = %d, node = %s",
	     TIUSB_MAJOR, (TIUSB_MINOR + s->minor), name);
	s->devfs =
	    devfs_register (devfs_handle, name, DEVFS_FL_DEFAULT, TIUSB_MAJOR,
			    TIUSB_MINOR + s->minor, S_IFCHR | S_IRUGO | S_IWUGO,
			    &tiglusb_fops, NULL);

	/* Display firmware version */
	info ("firmware revision %i.%02x",
		dev->descriptor.bcdDevice >> 8,
		dev->descriptor.bcdDevice & 0xff);

	return s;
}

static void
tiglusb_disconnect (struct usb_device *dev, void *drv_context)
{
	ptiglusb_t s = (ptiglusb_t) drv_context;

	if (!s || !s->dev)
		info ("bogus disconnect");

	s->remove_pending = 1;
	wake_up (&s->wait);
	if (s->state == _started)
		sleep_on (&s->remove_ok);
	down (&s->mutex);
	s->dev = NULL;
	s->opened = 0;

	devfs_unregister (s->devfs);
	s->devfs = NULL;

	info ("device %d removed", s->minor);

	up (&s->mutex);
}

static struct usb_device_id tiglusb_ids[] = {
	{USB_DEVICE (0x0451, 0xe001)},
	{}
};

MODULE_DEVICE_TABLE (usb, tiglusb_ids);

static struct usb_driver tiglusb_driver = {
	.owner =        THIS_MODULE,
	.name =		"tiglusb",
	.probe =	tiglusb_probe,
	.disconnect =	tiglusb_disconnect,
	.id_table =	tiglusb_ids,
};

/* --- initialisation code ------------------------------------- */

#ifndef MODULE
/*
 * You can use 'tiusb=timeout'
 */
static int __init
tiglusb_setup (char *str)
{
	int ints[2];

	str = get_options (str, ARRAY_SIZE (ints), ints);

	if (ints[0] > 0) {
		timeout = ints[1];
	}
	if (!timeout)
		timeout = TIMAXTIME;

	return 1;
}
#endif

static int __init
tiglusb_init (void)
{
	unsigned u;
	int result;

	/* initialize struct */
	for (u = 0; u < MAXTIGL; u++) {
		ptiglusb_t s = &tiglusb[u];
		memset (s, 0, sizeof (tiglusb_t));
		init_MUTEX (&s->mutex);
		s->dev = NULL;
		s->minor = u;
		s->opened = 0;
		init_waitqueue_head (&s->wait);
		init_waitqueue_head (&s->remove_ok);
	}
	if (timeout <= 0)
               timeout = TIMAXTIME;

	/* register device */
	if (register_chrdev (TIUSB_MAJOR, "tiglusb", &tiglusb_fops)) {
		err ("unable to get major %d", TIUSB_MAJOR);
		return -EIO;
	}

	/* Use devfs, tree: /dev/ticables/usb/[0..3] */
	devfs_handle = devfs_mk_dir (NULL, "ticables/usb", NULL);

	/* register USB module */
	result = usb_register (&tiglusb_driver);
	if (result < 0) {
		unregister_chrdev (TIUSB_MAJOR, "tiglusb");
		return -1;
	}

	info (DRIVER_DESC ", version " DRIVER_VERSION);

       if (timeout <= 0)
               timeout = TIMAXTIME;

	if (!timeout)
		timeout = TIMAXTIME;

	return 0;
}

static void __exit
tiglusb_cleanup (void)
{
	usb_deregister (&tiglusb_driver);
	devfs_unregister (devfs_handle);
	unregister_chrdev (TIUSB_MAJOR, "tiglusb");
}

/* --------------------------------------------------------------------- */

__setup ("tiusb=", tiglusb_setup);
module_init (tiglusb_init);
module_exit (tiglusb_cleanup);

MODULE_AUTHOR (DRIVER_AUTHOR);
MODULE_DESCRIPTION (DRIVER_DESC);
MODULE_LICENSE (DRIVER_LICENSE);

MODULE_PARM (timeout, "i");
MODULE_PARM_DESC (timeout, "Timeout in tenths of seconds (default=1.5 seconds)");

/* --------------------------------------------------------------------- */
