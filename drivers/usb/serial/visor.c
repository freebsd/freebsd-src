/*
 * USB HandSpring Visor, Palm m50x, and Sony Clie driver
 * (supports all of the Palm OS USB devices)
 *
 *	Copyright (C) 1999 - 2003
 *	    Greg Kroah-Hartman (greg@kroah.com)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 * See Documentation/usb/usb-serial.txt for more information on using this driver
 *
 * (06/03/2003) Judd Montgomery <judd at jpilot.org>
 *	Added support for module parameter options for untested/unknown
 *	devices.
 *
 * (03/09/2003) gkh
 *	Added support for the Sony Clie NZ90V device.  Thanks to Martin Brachtl
 *	<brachtl@redgrep.cz> for the information.
 *
 * (3/07/2003) Adam Pennington <adamp@coed.org>
 *      Backported version 2.1 of the driver from the 2.5 bitkeeper tree
 *      making Treo actually work.
 *
 * (2/18/2003) Adam Powell <hazelsct at debian.org>
 *	Backported 2.5 driver mods to support Handspring Treo.
 *
 * (2/11/2003) Adam Powell <hazelsct at debian.org>
 *	Added device and vendor ids for the Samsung I330 phone.
 *
 * (04/03/2002) gkh
 *	Added support for the Sony OS 4.1 devices.  Thanks to Hiroyuki ARAKI
 *	<hiro@zob.ne.jp> for the information.
 *
 * (03/23/2002) gkh
 *	Added support for the Palm i705 device, thanks to Thomas Riemer
 *	<tom@netmech.com> for the information.
 *
 * (03/21/2002) gkh
 *	Added support for the Palm m130 device, thanks to Udo Eisenbarth
 *	<udo.eisenbarth@web.de> for the information.
 *
 * (02/21/2002) SilaS
 *	Added support for the Palm m515 devices.
 *
 * (02/15/2002) gkh
 *	Added support for the Clie S-360 device.
 *
 * (12/18/2001) gkh
 *	Added better Clie support for 3.5 devices.  Thanks to Geoffrey Levand
 *	for the patch.
 *
 * (11/11/2001) gkh
 *	Added support for the m125 devices, and added check to prevent oopses
 *	for Clié devices that lie about the number of ports they have.
 *
 * (08/30/2001) gkh
 *	Added support for the Clie devices, both the 3.5 and 4.0 os versions.
 *	Many thanks to Daniel Burke, and Bryan Payne for helping with this.
 *
 * (08/23/2001) gkh
 *	fixed a few potential bugs pointed out by Oliver Neukum.
 *
 * (05/30/2001) gkh
 *	switched from using spinlock to a semaphore, which fixes lots of problems.
 *
 * (05/28/2000) gkh
 *	Added initial support for the Palm m500 and Palm m505 devices.
 *
 * (04/08/2001) gb
 *	Identify version on module load.
 *
 * (01/21/2000) gkh
 *	Added write_room and chars_in_buffer, as they were previously using the
 *	generic driver versions which is all wrong now that we are using an urb
 *	pool.  Thanks to Wolfgang Grandegger for pointing this out to me.
 *	Removed count assignment in the write function, which was not needed anymore
 *	either.  Thanks to Al Borchers for pointing this out.
 *
 * (12/12/2000) gkh
 *	Moved MOD_DEC to end of visor_close to be nicer, as the final write 
 *	message can sleep.
 * 
 * (11/12/2000) gkh
 *	Fixed bug with data being dropped on the floor by forcing tty->low_latency
 *	to be on.  Hopefully this fixes the OHCI issue!
 *
 * (11/01/2000) Adam J. Richter
 *	usb_device_id table support
 * 
 * (10/05/2000) gkh
 *	Fixed bug with urb->dev not being set properly, now that the usb
 *	core needs it.
 * 
 * (09/11/2000) gkh
 *	Got rid of always calling kmalloc for every urb we wrote out to the
 *	device.
 *	Added visor_read_callback so we can keep track of bytes in and out for
 *	those people who like to know the speed of their device.
 *	Removed DEBUG #ifdefs with call to usb_serial_debug_data
 *
 * (09/06/2000) gkh
 *	Fixed oops in visor_exit.  Need to uncomment usb_unlink_urb call _after_
 *	the host controller drivers set urb->dev = NULL when the urb is finished.
 *
 * (08/28/2000) gkh
 *	Added locks for SMP safeness.
 *
 * (08/08/2000) gkh
 *	Fixed endian problem in visor_startup.
 *	Fixed MOD_INC and MOD_DEC logic and the ability to open a port more 
 *	than once.
 * 
 * (07/23/2000) gkh
 *	Added pool of write urbs to speed up transfers to the visor.
 * 
 * (07/19/2000) gkh
 *	Added module_init and module_exit functions to handle the fact that this
 *	driver is a loadable module now.
 *
 * (07/03/2000) gkh
 *	Added visor_set_ioctl and visor_set_termios functions (they don't do much
 *	of anything, but are good for debugging.)
 * 
 * (06/25/2000) gkh
 *	Fixed bug in visor_unthrottle that should help with the disconnect in PPP
 *	bug that people have been reporting.
 *
 * (06/23/2000) gkh
 *	Cleaned up debugging statements in a quest to find UHCI timeout bug.
 *
 * (04/27/2000) Ryan VanderBijl
 * 	Fixed memory leak in visor_close
 *
 * (03/26/2000) gkh
 *	Split driver up into device specific pieces.
 * 
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <linux/usb.h>

#ifdef CONFIG_USB_SERIAL_DEBUG
	static int debug = 1;
#else
	static int debug;
#endif

#include "usb-serial.h"
#include "visor.h"

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.7"
#define DRIVER_AUTHOR "Greg Kroah-Hartman <greg@kroah.com>"
#define DRIVER_DESC "USB HandSpring Visor, Palm m50x, Treo, Sony Clié driver"

/* function prototypes for a handspring visor */
static int  visor_open		(struct usb_serial_port *port, struct file *filp);
static void visor_close		(struct usb_serial_port *port, struct file *filp);
static int  visor_write		(struct usb_serial_port *port, int from_user, const unsigned char *buf, int count);
static int  visor_write_room		(struct usb_serial_port *port);
static int  visor_chars_in_buffer	(struct usb_serial_port *port);
static void visor_throttle	(struct usb_serial_port *port);
static void visor_unthrottle	(struct usb_serial_port *port);
static int  visor_startup	(struct usb_serial *serial);
static void visor_shutdown	(struct usb_serial *serial);
static int  visor_ioctl		(struct usb_serial_port *port, struct file * file, unsigned int cmd, unsigned long arg);
static void visor_set_termios	(struct usb_serial_port *port, struct termios *old_termios);
static void visor_write_bulk_callback	(struct urb *urb);
static void visor_read_bulk_callback	(struct urb *urb);
static void visor_read_int_callback	(struct urb *urb);
static int  clie_3_5_startup	(struct usb_serial *serial);
static void treo_attach		(struct usb_serial *serial);

/* Parameters that may be passed into the module. */
static int vendor = -1;
static int product = -1;
static int param_register;


static struct usb_device_id id_table [] = {
	{ USB_DEVICE(HANDSPRING_VENDOR_ID, HANDSPRING_VISOR_ID) },
	{ USB_DEVICE(HANDSPRING_VENDOR_ID, HANDSPRING_TREO_ID) },
	{ USB_DEVICE(HANDSPRING_VENDOR_ID, HANDSPRING_TREO600_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_M500_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_M505_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_M515_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_I705_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_M100_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_M125_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_M130_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_TUNGSTEN_T_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_TUNGSTEN_Z_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_ZIRE_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_4_0_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_S360_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_4_1_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_NX60_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_NZ90V_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_UX50_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_TJ25_ID) },
	{ USB_DEVICE(SAMSUNG_VENDOR_ID, SAMSUNG_SCH_I330_ID) },
	{ USB_DEVICE(GARMIN_VENDOR_ID, GARMIN_IQUE_3600_ID) },
	{ USB_DEVICE(ACEECA_VENDOR_ID, ACEECA_MEZ1000_ID) },
	{ }					/* Terminating entry */
};

static struct usb_device_id clie_id_3_5_table [] = {
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_3_5_ID) },
	{ }					/* Terminating entry */
};

static __devinitdata struct usb_device_id id_table_combined [] = {
	{ USB_DEVICE(HANDSPRING_VENDOR_ID, HANDSPRING_VISOR_ID) },
	{ USB_DEVICE(HANDSPRING_VENDOR_ID, HANDSPRING_TREO_ID) },
	{ USB_DEVICE(HANDSPRING_VENDOR_ID, HANDSPRING_TREO600_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_M500_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_M505_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_M515_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_I705_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_M100_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_M125_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_M130_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_TUNGSTEN_T_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_TUNGSTEN_Z_ID) },
	{ USB_DEVICE(PALM_VENDOR_ID, PALM_ZIRE_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_3_5_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_4_0_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_S360_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_4_1_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_NX60_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_NZ90V_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_UX50_ID) },
	{ USB_DEVICE(SONY_VENDOR_ID, SONY_CLIE_TJ25_ID) },
	{ USB_DEVICE(SAMSUNG_VENDOR_ID, SAMSUNG_SCH_I330_ID) },
	{ USB_DEVICE(GARMIN_VENDOR_ID, GARMIN_IQUE_3600_ID) },
	{ USB_DEVICE(ACEECA_VENDOR_ID, ACEECA_MEZ1000_ID) },
	{ }					/* Terminating entry */
};

/* For passed in parameters */
static struct usb_device_id id_param_table [] = {
	{ },
	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, id_table_combined);



/* All of the device info needed for the Handspring Visor, and Palm 4.0 devices */
static struct usb_serial_device_type handspring_device = {
	.owner =		THIS_MODULE,
	.name =			"Handspring Visor / Treo / Palm 4.0 / Clié 4.x",
	.id_table =		id_table,
	.num_interrupt_in =	NUM_DONT_CARE,
	.num_bulk_in =		2,
	.num_bulk_out =		2,
	.num_ports =		2,
	.open =			visor_open,
	.close =		visor_close,
	.throttle =		visor_throttle,
	.unthrottle =		visor_unthrottle,
	.startup =		visor_startup,
	.shutdown =		visor_shutdown,
	.ioctl =		visor_ioctl,
	.set_termios =		visor_set_termios,
	.write =		visor_write,
	.write_room =		visor_write_room,
	.chars_in_buffer =	visor_chars_in_buffer,
	.write_bulk_callback =	visor_write_bulk_callback,
	.read_bulk_callback =	visor_read_bulk_callback,
	.read_int_callback =	visor_read_int_callback,
};

/* device info for the Sony Clie OS version 3.5 */
static struct usb_serial_device_type clie_3_5_device = {
	.owner =		THIS_MODULE,
	.name =			"Sony Clié 3.5",
	.id_table =		clie_id_3_5_table,
	.num_interrupt_in =	0,
	.num_bulk_in =		1,
	.num_bulk_out =		1,
	.num_ports =		1,
	.open =			visor_open,
	.close =		visor_close,
	.throttle =		visor_throttle,
	.unthrottle =		visor_unthrottle,
	.startup =		clie_3_5_startup,
	.ioctl =		visor_ioctl,
	.set_termios =		visor_set_termios,
	.write =		visor_write,
	.write_room =		visor_write_room,
	.chars_in_buffer =	visor_chars_in_buffer,
	.write_bulk_callback =	visor_write_bulk_callback,
	.read_bulk_callback =	visor_read_bulk_callback,
};

/* This structure is for Handspring Visor, and Palm 4.0 devices that are not
 * compiled into the kernel, but can be passed in when the module is loaded.
 * This will allow the visor driver to work with new Vendor and Device IDs
 * without recompiling the driver.
 */
static struct usb_serial_device_type param_device = {
	.owner =		THIS_MODULE,
	.name =			"user specified device with Palm 4.x protocols",
	.id_table =		id_param_table,
	.num_interrupt_in =	NUM_DONT_CARE,
	.num_bulk_in =		2,
	.num_bulk_out =		2,
	.num_ports =		2,
	.open =			visor_open,
	.close =		visor_close,
	.throttle =		visor_throttle,
	.unthrottle =		visor_unthrottle,
	.startup =		visor_startup,
	.shutdown =		visor_shutdown,
	.ioctl =		visor_ioctl,
	.set_termios =		visor_set_termios,
	.write =		visor_write,
	.write_room =		visor_write_room,
	.chars_in_buffer =	visor_chars_in_buffer,
	.write_bulk_callback =	visor_write_bulk_callback,
	.read_bulk_callback =	visor_read_bulk_callback,
	.read_int_callback =	visor_read_int_callback,
};

#define NUM_URBS			24
#define URB_TRANSFER_BUFFER_SIZE	768
static struct urb	*write_urb_pool[NUM_URBS];
static spinlock_t	write_urb_pool_lock;
static int		bytes_in;
static int		bytes_out;


/******************************************************************************
 * Handspring Visor specific driver functions
 ******************************************************************************/
static int visor_open (struct usb_serial_port *port, struct file *filp)
{
	struct usb_serial *serial = port->serial;
	int result = 0;

	if (port_paranoia_check (port, __FUNCTION__))
		return -ENODEV;
	
	dbg("%s - port %d", __FUNCTION__, port->number);

	if (!port->read_urb) {
		/* this is needed for some brain dead Sony devices */
		err ("Device lied about number of ports, please use a lower one.");
		return -ENODEV;
	}

	bytes_in = 0;
	bytes_out = 0;

	/*
	 * Force low_latency on so that our tty_push actually forces the data
	 * through, otherwise it is scheduled, and with high data rates (like
	 * with OHCI) data can get lost.
	 */
	if (port->tty)
		port->tty->low_latency = 1;

	/* Start reading from the device */
	usb_fill_bulk_urb (port->read_urb, serial->dev,
			   usb_rcvbulkpipe (serial->dev, 
					    port->bulk_in_endpointAddress),
			   port->read_urb->transfer_buffer,
			   port->read_urb->transfer_buffer_length,
			   visor_read_bulk_callback, port);
	result = usb_submit_urb(port->read_urb);
	if (result) {
		err("%s - failed submitting read urb, error %d", __FUNCTION__, result);
		goto exit;
	}
	
	if (port->interrupt_in_urb) {
		dbg("%s - adding interrupt input for treo", __FUNCTION__);
		result = usb_submit_urb(port->interrupt_in_urb);
		if (result)
			err("%s - failed submitting interrupt urb, error %d\n",
			    __FUNCTION__, result);
	}
exit:
	return result;
}


static void visor_close (struct usb_serial_port *port, struct file * filp)
{
	struct usb_serial *serial;
	unsigned char *transfer_buffer;

	if (port_paranoia_check (port, __FUNCTION__))
		return;
	
	dbg("%s - port %d", __FUNCTION__, port->number);
			 
	serial = get_usb_serial (port, __FUNCTION__);
	if (!serial)
		return;
	



	if (serial->dev) {
		/* only send a shutdown message if the 
		 * device is still here */
		transfer_buffer =  kmalloc (0x12, GFP_KERNEL);
		if (!transfer_buffer) {
			err("%s - kmalloc(%d) failed.", __FUNCTION__, 0x12);
		} else {
			/* send a shutdown message to the device */
			usb_control_msg (serial->dev,
					 usb_rcvctrlpipe(serial->dev, 0),
					 VISOR_CLOSE_NOTIFICATION, 0xc2,
					 0x0000, 0x0000, 
					 transfer_buffer, 0x12, 300);
			kfree (transfer_buffer);
		}

		/* shutdown our urbs */
		usb_unlink_urb (port->read_urb);
		if (port->interrupt_in_urb)
		  usb_unlink_urb (port->interrupt_in_urb);
		/* Try to send shutdown message, if the device is gone, this will just fail. */
		transfer_buffer =  kmalloc (0x12, GFP_KERNEL);
		if (transfer_buffer) {
		  usb_control_msg (serial->dev,
				 usb_rcvctrlpipe(serial->dev, 0),
				 VISOR_CLOSE_NOTIFICATION, 0xc2,
				 0x0000, 0x0000, 
				 transfer_buffer, 0x12, 300);
		kfree (transfer_buffer);			
		
		}
	}
	/* Uncomment the following line if you want to see some statistics in your syslog */
	info ("Bytes In = %d  Bytes Out = %d", bytes_in, bytes_out);
}


static int visor_write (struct usb_serial_port *port, int from_user, const unsigned char *buf, int count)
{
	struct usb_serial *serial = port->serial;
	struct urb *urb;
	const unsigned char *current_position = buf;
	unsigned long flags;
	int status;
	int i;
	int bytes_sent = 0;
	int transfer_size;

	dbg("%s - port %d", __FUNCTION__, port->number);

	while (count > 0) {
		/* try to find a free urb in our list of them */
		urb = NULL;
		spin_lock_irqsave (&write_urb_pool_lock, flags);
		for (i = 0; i < NUM_URBS; ++i) {
			if (write_urb_pool[i]->status != -EINPROGRESS) {
				urb = write_urb_pool[i];
				break;
			}
		}
		spin_unlock_irqrestore (&write_urb_pool_lock, flags);
		if (urb == NULL) {
			dbg("%s - no more free urbs", __FUNCTION__);
			goto exit;
		}
		if (urb->transfer_buffer == NULL) {
			urb->transfer_buffer = kmalloc (URB_TRANSFER_BUFFER_SIZE, GFP_KERNEL);
			if (urb->transfer_buffer == NULL) {
				err("%s no more kernel memory...", __FUNCTION__);
				goto exit;
			}
		}
		
		transfer_size = min (count, URB_TRANSFER_BUFFER_SIZE);
		if (from_user) {
			if (copy_from_user (urb->transfer_buffer, current_position, transfer_size)) {
				bytes_sent = -EFAULT;
				break;
			}
		} else {
			memcpy (urb->transfer_buffer, current_position, transfer_size);
		}

		usb_serial_debug_data (__FILE__, __FUNCTION__, transfer_size, urb->transfer_buffer);

		/* build up our urb */
		FILL_BULK_URB (urb, serial->dev, usb_sndbulkpipe(serial->dev, port->bulk_out_endpointAddress), 
				urb->transfer_buffer, transfer_size, visor_write_bulk_callback, port);
		urb->transfer_flags |= USB_QUEUE_BULK;

		/* send it down the pipe */
		status = usb_submit_urb(urb);
		if (status) {
			err("%s - usb_submit_urb(write bulk) failed with status = %d", __FUNCTION__, status);
			bytes_sent = status;
			break;
		}

		current_position += transfer_size;
		bytes_sent += transfer_size;
		count -= transfer_size;
		bytes_out += transfer_size;
	}

exit:
	return bytes_sent;
} 


static int visor_write_room (struct usb_serial_port *port)
{
	unsigned long flags;
	int i;
	int room = 0;

	dbg("%s - port %d", __FUNCTION__, port->number);
	
	spin_lock_irqsave (&write_urb_pool_lock, flags);

	for (i = 0; i < NUM_URBS; ++i) {
		if (write_urb_pool[i]->status != -EINPROGRESS) {
			room += URB_TRANSFER_BUFFER_SIZE;
		}
	}
	
	spin_unlock_irqrestore (&write_urb_pool_lock, flags);
	
	dbg("%s - returns %d", __FUNCTION__, room);
	return (room);
}


static int visor_chars_in_buffer (struct usb_serial_port *port)
{
	unsigned long flags;
	int i;
	int chars = 0;

	dbg("%s - port %d", __FUNCTION__, port->number);
	
	spin_lock_irqsave (&write_urb_pool_lock, flags);

	for (i = 0; i < NUM_URBS; ++i) {
		if (write_urb_pool[i]->status == -EINPROGRESS) {
			chars += URB_TRANSFER_BUFFER_SIZE;
		}
	}
	
	spin_unlock_irqrestore (&write_urb_pool_lock, flags);

	dbg("%s - returns %d", __FUNCTION__, chars);
	return (chars);
}


static void visor_write_bulk_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;

	if (port_paranoia_check (port, __FUNCTION__))
		return;
	
	dbg("%s - port %d", __FUNCTION__, port->number);
	
	if (urb->status) {
		dbg("%s - nonzero write bulk status received: %d", __FUNCTION__, urb->status);
		return;
	}

	queue_task(&port->tqueue, &tq_immediate);
	mark_bh(IMMEDIATE_BH);

	return;
}


static void visor_read_bulk_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);
	struct tty_struct *tty;
	unsigned char *data = urb->transfer_buffer;
	int i;
	int result;

	if (port_paranoia_check (port, __FUNCTION__))
		return;

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (!serial) {
		dbg("%s - bad serial pointer, exiting", __FUNCTION__);
		return;
	}

	if (urb->status) {
		dbg("%s - nonzero read bulk status received: %d", __FUNCTION__, urb->status);
		return;
	}

	usb_serial_debug_data (__FILE__, __FUNCTION__, urb->actual_length, data);

	tty = port->tty;
	if (tty && urb->actual_length) {
		for (i = 0; i < urb->actual_length ; ++i) {
			/* if we insert more than TTY_FLIPBUF_SIZE characters, we drop them. */
			if(tty->flip.count >= TTY_FLIPBUF_SIZE) {
				tty_flip_buffer_push(tty);
			}
			/* this doesn't actually push the data through unless tty->low_latency is set */
			tty_insert_flip_char(tty, data[i], 0);
		}
		tty_flip_buffer_push(tty);
		bytes_in += urb->actual_length;
	}

	/* Continue trying to always read  */
	usb_fill_bulk_urb (port->read_urb, serial->dev,
			   usb_rcvbulkpipe (serial->dev,
					    port->bulk_in_endpointAddress),
			   port->read_urb->transfer_buffer,
			   port->read_urb->transfer_buffer_length,
			   visor_read_bulk_callback, port);
	result = usb_submit_urb(port->read_urb);
	if (result)
		err("%s - failed resubmitting read urb, error %d", __FUNCTION__, result);
	return;
}


static void visor_read_int_callback (struct urb *urb)
{
	switch (urb->status) {
	case 0:
		/* success */
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* this urb is terminated, clean up */
		dbg("%s - urb shutting down with status: %d",
		    __FUNCTION__, urb->status);
		return;
	default:
		dbg("%s - nonzero urb status received: %d",
		    __FUNCTION__, urb->status);
		goto exit;
	}

	/*
	 * This information is still unknown what it can be used for.
	 * If anyone has an idea, please let the author know...
	 *
	 * Rumor has it this endpoint is used to notify when data
	 * is ready to be read from the bulk ones.
	 */
	usb_serial_debug_data (__FILE__, __FUNCTION__, urb->actual_length,
			       urb->transfer_buffer);

exit:
	return;
}


static void visor_throttle (struct usb_serial_port *port)
{
	dbg("%s - port %d", __FUNCTION__, port->number);
	usb_unlink_urb (port->read_urb);
}


static void visor_unthrottle (struct usb_serial_port *port)
{
	int result;

	dbg("%s - port %d", __FUNCTION__, port->number);

	port->read_urb->dev = port->serial->dev;
	result = usb_submit_urb(port->read_urb);
	if (result)
		err("%s - failed submitting read urb, error %d", __FUNCTION__, result);
}

static int visor_startup (struct usb_serial *serial)
{
	int response;
	int i;
	unsigned char *transfer_buffer;

	dbg("%s", __FUNCTION__);

	dbg("%s - Set config to 1", __FUNCTION__);
	usb_set_configuration (serial->dev, 1);

	if ((serial->dev->descriptor.idVendor == HANDSPRING_VENDOR_ID) &&
	    (serial->dev->descriptor.idProduct == HANDSPRING_VISOR_ID)) {
		struct visor_connection_info *connection_info;
		char *string;
		int num_ports;

		transfer_buffer = kmalloc (sizeof (*connection_info),
					   GFP_KERNEL);
		if (!transfer_buffer) {
			err("%s - kmalloc(%d) failed.", __FUNCTION__,
			    sizeof (*connection_info));
			return -ENOMEM;
		}

		/* send a get connection info request */
		response = usb_control_msg (serial->dev,
					    usb_rcvctrlpipe(serial->dev, 0),
					    VISOR_GET_CONNECTION_INFORMATION,
					    0xc2, 0x0000, 0x0000,
					    transfer_buffer,
					    sizeof (*connection_info), 300);
		if (response < 0) {
			err("%s - error getting connection information",
			    __FUNCTION__);
			goto exit;
		}

		connection_info = (struct visor_connection_info *)transfer_buffer;
		le16_to_cpus(&connection_info->num_ports);
		num_ports = connection_info->num_ports;

		/* handle devices that report invalid stuff here */
		if (num_ports > 2)
			num_ports = 2;
		info("%s: Number of ports: %d", serial->type->name, connection_info->num_ports);
		for (i = 0; i < num_ports; ++i) {
			switch (connection_info->connections[i].port_function_id) {
				case VISOR_FUNCTION_GENERIC:
					string = "Generic";
					break;
				case VISOR_FUNCTION_DEBUGGER:
					string = "Debugger";
					break;
				case VISOR_FUNCTION_HOTSYNC:
					string = "HotSync";
					break;
				case VISOR_FUNCTION_CONSOLE:
					string = "Console";
					break;
				case VISOR_FUNCTION_REMOTE_FILE_SYS:
					string = "Remote File System";
					break;
				default:
					string = "unknown";
					break;	
			}
			info("%s: port %d, is for %s use and is bound to ttyUSB%d", serial->type->name,
			     connection_info->connections[i].port, string, serial->minor + i);
		}
	} else {
		struct palm_ext_connection_info *connection_info;

		transfer_buffer = kmalloc (sizeof (*connection_info),
					   GFP_KERNEL);
		if (!transfer_buffer) {
			err("%s - kmalloc(%d) failed.", __FUNCTION__,
			    sizeof (*connection_info));
			return -ENOMEM;
		}

		response = usb_control_msg (serial->dev, usb_rcvctrlpipe(serial->dev, 0), 
					    PALM_GET_EXT_CONNECTION_INFORMATION,
					    0xc2, 0x0000, 0x0000, transfer_buffer, 
					    sizeof (*connection_info), 300);
		if (response < 0) {
			err("%s - error %d getting connection info",
			    __FUNCTION__, response);
		} else {
			usb_serial_debug_data (__FILE__, __FUNCTION__, 0x14, transfer_buffer);
		}
	}

	/* Do our horrible Treo hack, if we should */
	treo_attach(serial);

	/* ask for the number of bytes available, but ignore the response as it is broken */
	response = usb_control_msg (serial->dev, usb_rcvctrlpipe(serial->dev, 0), VISOR_REQUEST_BYTES_AVAILABLE,
					0xc2, 0x0000, 0x0005, transfer_buffer, 0x02, 300);
	if (response < 0) {
		err("%s - error getting bytes available request", __FUNCTION__);
	}

exit:
	kfree (transfer_buffer);

	/* continue on with initialization */
	return 0;
}

static int clie_3_5_startup (struct usb_serial *serial)
{
	int result;
	u8 data;

	dbg("%s", __FUNCTION__);

	/*
	 * Note that PEG-300 series devices expect the following two calls.
	 */

	/* get the config number */
	result = usb_control_msg (serial->dev, usb_rcvctrlpipe(serial->dev, 0),
				  USB_REQ_GET_CONFIGURATION, USB_DIR_IN,
				  0, 0, &data, 1, HZ * 3);
	if (result < 0) {
		err("%s: get config number failed: %d", __FUNCTION__, result);
		return result;
	}
	if (result != 1) {
		err("%s: get config number bad return length: %d", __FUNCTION__, result);
		return -EIO;
	}

	/* get the interface number */
	result = usb_control_msg (serial->dev, usb_rcvctrlpipe(serial->dev, 0),
				  USB_REQ_GET_INTERFACE, 
				  USB_DIR_IN | USB_DT_DEVICE,
				  0, 0, &data, 1, HZ * 3);
	if (result < 0) {
		err("%s: get interface number failed: %d", __FUNCTION__, result);
		return result;
	}
	if (result != 1) {
		err("%s: get interface number bad return length: %d", __FUNCTION__, result);
		return -EIO;
	}

	return 0;
}

static void treo_attach (struct usb_serial *serial)
{
	struct usb_serial_port *port;
	int i;

	/* Only do this endpoint hack for the Handspring devices with
	 * interrupt in endpoints, which for now are the Treo devices. */
	if ((serial->dev->descriptor.idVendor != HANDSPRING_VENDOR_ID) ||
	    (serial->num_interrupt_in == 0))
		return;

	dbg("%s", __FUNCTION__);

	/* Ok, this is pretty ugly, but these devices want to use the
	 * interrupt endpoint as paired up with a bulk endpoint for a
	 * "virtual serial port".  So let's force the endpoints to be
	 * where we want them to be. */
	for (i = serial->num_bulk_in; i < serial->num_ports; ++i) {
		port = &serial->port[i];
		port->read_urb = serial->port[0].read_urb;
		port->bulk_in_endpointAddress = serial->port[0].bulk_in_endpointAddress;
		port->bulk_in_buffer = serial->port[0].bulk_in_buffer;
	}

	for (i = serial->num_bulk_out; i < serial->num_ports; ++i) {
		port = &serial->port[i];
		port->write_urb = serial->port[0].write_urb;
		port->bulk_out_size = serial->port[0].bulk_out_size;
		port->bulk_out_endpointAddress = serial->port[0].bulk_out_endpointAddress;
		port->bulk_out_buffer = serial->port[0].bulk_out_buffer;
	}

	for (i = serial->num_interrupt_in; i < serial->num_ports; ++i) {
		port = &serial->port[i];
		port->interrupt_in_urb = serial->port[0].interrupt_in_urb;
		port->interrupt_in_endpointAddress = serial->port[0].interrupt_in_endpointAddress;
		port->interrupt_in_buffer = serial->port[0].interrupt_in_buffer;
 	}
}

static void visor_shutdown (struct usb_serial *serial)
{
	dbg("%s", __FUNCTION__);
}

static int visor_ioctl (struct usb_serial_port *port, struct file * file, unsigned int cmd, unsigned long arg)
{
	dbg("%s - port %d, cmd 0x%.4x", __FUNCTION__, port->number, cmd);

	return -ENOIOCTLCMD;
}


/* This function is all nice and good, but we don't change anything based on it :) */
static void visor_set_termios (struct usb_serial_port *port, struct termios *old_termios)
{
	unsigned int cflag;

	dbg("%s - port %d", __FUNCTION__, port->number);

	if ((!port->tty) || (!port->tty->termios)) {
		dbg("%s - no tty structures", __FUNCTION__);
		return;
	}

	cflag = port->tty->termios->c_cflag;
	/* check that they really want us to change something */
	if (old_termios) {
		if ((cflag == old_termios->c_cflag) &&
		    (RELEVANT_IFLAG(port->tty->termios->c_iflag) == RELEVANT_IFLAG(old_termios->c_iflag))) {
			dbg("%s - nothing to change...", __FUNCTION__);
			return;
		}
	}

	/* get the byte size */
	switch (cflag & CSIZE) {
		case CS5:	dbg("%s - data bits = 5", __FUNCTION__);   break;
		case CS6:	dbg("%s - data bits = 6", __FUNCTION__);   break;
		case CS7:	dbg("%s - data bits = 7", __FUNCTION__);   break;
		default:
		case CS8:	dbg("%s - data bits = 8", __FUNCTION__);   break;
	}
	
	/* determine the parity */
	if (cflag & PARENB)
		if (cflag & PARODD)
			dbg("%s - parity = odd", __FUNCTION__);
		else
			dbg("%s - parity = even", __FUNCTION__);
	else
		dbg("%s - parity = none", __FUNCTION__);

	/* figure out the stop bits requested */
	if (cflag & CSTOPB)
		dbg("%s - stop bits = 2", __FUNCTION__);
	else
		dbg("%s - stop bits = 1", __FUNCTION__);

	
	/* figure out the flow control settings */
	if (cflag & CRTSCTS)
		dbg("%s - RTS/CTS is enabled", __FUNCTION__);
	else
		dbg("%s - RTS/CTS is disabled", __FUNCTION__);
	
	/* determine software flow control */
	if (I_IXOFF(port->tty))
		dbg("%s - XON/XOFF is enabled, XON = %2x, XOFF = %2x",
		    __FUNCTION__, START_CHAR(port->tty), STOP_CHAR(port->tty));
	else
		dbg("%s - XON/XOFF is disabled", __FUNCTION__);

	/* get the baud rate wanted */
	dbg("%s - baud rate = %d", __FUNCTION__, tty_get_baud_rate(port->tty));

	return;
}


static int __init visor_init (void)
{
	struct urb *urb;
	int i;

	/* Only if parameters were passed to us */
	if ((vendor > 0) && (product > 0)) {
       		struct usb_device_id usb_dev_temp[]=
	       		{{USB_DEVICE(vendor, product)}};
		id_param_table[0] = usb_dev_temp[0];
		info("Untested USB device specified at time of module insertion");
		info("Warning: This is not guaranteed to work");
		info("Using a newer kernel is preferred to this method");
		info("Adding Palm OS protocol 4.x support for unknown device: 0x%x/0x%x",
			param_device.id_table[0].idVendor, param_device.id_table[0].idProduct);
		param_register = 1;
		usb_serial_register (&param_device);
	}
	usb_serial_register (&handspring_device);
	usb_serial_register (&clie_3_5_device);
	
	/* create our write urb pool and transfer buffers */ 
	spin_lock_init (&write_urb_pool_lock);
	for (i = 0; i < NUM_URBS; ++i) {
		urb = usb_alloc_urb(0);
		write_urb_pool[i] = urb;
		if (urb == NULL) {
			err("No more urbs???");
			continue;
		}

		urb->transfer_buffer = NULL;
		urb->transfer_buffer = kmalloc (URB_TRANSFER_BUFFER_SIZE, GFP_KERNEL);
		if (!urb->transfer_buffer) {
			err("%s - out of memory for urb buffers.", __FUNCTION__);
			continue;
		}
	}

	info(DRIVER_DESC " " DRIVER_VERSION);

	return 0;
}


static void __exit visor_exit (void)
{
	int i;
	unsigned long flags;

	if (param_register) {
		param_register = 0;
		usb_serial_deregister (&param_device);
	}
	usb_serial_deregister (&handspring_device);
	usb_serial_deregister (&clie_3_5_device);

	spin_lock_irqsave (&write_urb_pool_lock, flags);

	for (i = 0; i < NUM_URBS; ++i) {
		if (write_urb_pool[i]) {
			/* FIXME - uncomment the following usb_unlink_urb call when
			 * the host controllers get fixed to set urb->dev = NULL after
			 * the urb is finished.  Otherwise this call oopses. */
			/* usb_unlink_urb(write_urb_pool[i]); */
			if (write_urb_pool[i]->transfer_buffer)
				kfree(write_urb_pool[i]->transfer_buffer);
			usb_free_urb (write_urb_pool[i]);
		}
	}

	spin_unlock_irqrestore (&write_urb_pool_lock, flags);
}


module_init(visor_init);
module_exit(visor_exit);

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug enabled or not");
MODULE_PARM(vendor, "i");
MODULE_PARM_DESC(vendor, "User specified vendor ID");
MODULE_PARM(product, "i");
MODULE_PARM_DESC(product, "User specified product ID");
