/*
 *  REINER SCT cyberJack pinpad/e-com USB Chipcard Reader Driver
 *
 *  Copyright (C) 2001  REINER SCT
 *  Author: Matthias Bruestle
 *
 *  Contact: linux-usb@sii.li (see MAINTAINERS)
 *
 *  This program is largely derived from work by the linux-usb group
 *  and associated source files.  Please see the usb/serial files for
 *  individual credits and copyrights.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Thanks to Greg Kroah-Hartman (greg@kroah.com) for his help and
 *  patience.
 *
 *  In case of problems, please write to the contact e-mail address
 *  mentioned above.
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

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.0"
#define DRIVER_AUTHOR "Matthias Bruestle"
#define DRIVER_DESC "REINER SCT cyberJack pinpad/e-com USB Chipcard Reader Driver"


#define CYBERJACK_VENDOR_ID	0x0C4B
#define CYBERJACK_PRODUCT_ID	0x0100

/* Function prototypes */
static int cyberjack_startup (struct usb_serial *serial);
static void cyberjack_shutdown (struct usb_serial *serial);
static int  cyberjack_open (struct usb_serial_port *port, struct file *filp);
static void cyberjack_close (struct usb_serial_port *port, struct file *filp);
static int cyberjack_write (struct usb_serial_port *port, int from_user,
	const unsigned char *buf, int count);
static void cyberjack_read_int_callback( struct urb *urb );
static void cyberjack_read_bulk_callback (struct urb *urb);
static void cyberjack_write_bulk_callback (struct urb *urb);

static struct usb_device_id id_table [] = {
	{ USB_DEVICE(CYBERJACK_VENDOR_ID, CYBERJACK_PRODUCT_ID) },
	{ }			/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, id_table);

static struct usb_serial_device_type cyberjack_device = {
	.owner =		THIS_MODULE,
	.name =			"Reiner SCT Cyberjack USB card reader",
	.id_table =		id_table,
	.num_interrupt_in =	1,
	.num_bulk_in =		1,
	.num_bulk_out =		1,
	.num_ports =		1,
	.startup =		cyberjack_startup,
	.shutdown =		cyberjack_shutdown,
	.open =			cyberjack_open,
	.close =		cyberjack_close,
	.write =		cyberjack_write,
	.read_int_callback =	cyberjack_read_int_callback,
	.read_bulk_callback =	cyberjack_read_bulk_callback,
	.write_bulk_callback =	cyberjack_write_bulk_callback,
};

struct cyberjack_private {
	short	rdtodo;		/* Bytes still to read */
	unsigned char	wrbuf[5*64];	/* Buffer for collecting data to write */
	short	wrfilled;	/* Overall data size we already got */
	short	wrsent;		/* Data akready sent */
};

/* do some startup allocations not currently performed by usb_serial_probe() */
static int cyberjack_startup (struct usb_serial *serial)
{
	struct cyberjack_private *priv;

	dbg("%s", __FUNCTION__);

	/* allocate the private data structure */
	serial->port->private = kmalloc(sizeof(struct cyberjack_private), GFP_KERNEL);
	if (!serial->port->private)
		return (-1); /* error */

	/* set initial values */
	priv = (struct cyberjack_private *)serial->port->private;
	priv->rdtodo = 0;
	priv->wrfilled = 0;
	priv->wrsent = 0;

	init_waitqueue_head(&serial->port->write_wait);

	return( 0 );
}

static void cyberjack_shutdown (struct usb_serial *serial)
{
	int i;
	
	dbg("%s", __FUNCTION__);

	for (i=0; i < serial->num_ports; ++i) {
		/* My special items, the standard routines free my urbs */
		if (serial->port[i].private)
			kfree(serial->port[i].private);
	}
}
	
static int  cyberjack_open (struct usb_serial_port *port, struct file *filp)
{
	struct cyberjack_private *priv;
	int result = 0;

	if (port_paranoia_check (port, __FUNCTION__))
		return -ENODEV;

	dbg("%s - port %d", __FUNCTION__, port->number);

	/* force low_latency on so that our tty_push actually forces
	 * the data through, otherwise it is scheduled, and with high
	 * data rates (like with OHCI) data can get lost.
	 */
	port->tty->low_latency = 1;

	priv = (struct cyberjack_private *)port->private;
	priv->rdtodo = 0;
	priv->wrfilled = 0;
	priv->wrsent = 0;

	/* shutdown any bulk reads that might be going on */
	usb_unlink_urb (port->write_urb);
	usb_unlink_urb (port->read_urb);
	usb_unlink_urb (port->interrupt_in_urb);

	port->interrupt_in_urb->dev = port->serial->dev;
	result = usb_submit_urb(port->interrupt_in_urb);
	if (result)
		err(" usb_submit_urb(read int) failed");
	dbg("%s - usb_submit_urb(int urb)", __FUNCTION__);

	return result;
}

static void cyberjack_close (struct usb_serial_port *port, struct file *filp)
{
	dbg("%s - port %d", __FUNCTION__, port->number);

	if (port->serial->dev) {
		/* shutdown any bulk reads that might be going on */
		usb_unlink_urb (port->write_urb);
		usb_unlink_urb (port->read_urb);
		usb_unlink_urb (port->interrupt_in_urb);
	}
}

static int cyberjack_write (struct usb_serial_port *port, int from_user, const unsigned char *buf, int count)
{
	struct usb_serial *serial = port->serial;
	struct cyberjack_private *priv = (struct cyberjack_private *)port->private;
	int result;
	int wrexpected;

	dbg("%s - port %d", __FUNCTION__, port->number);
	dbg("%s - from_user %d", __FUNCTION__, from_user);

	if (count == 0) {
		dbg("%s - write request of 0 bytes", __FUNCTION__);
		return (0);
	}

	if (port->write_urb->status == -EINPROGRESS) {
		dbg("%s - already writing", __FUNCTION__);
		return (0);
	}

	if( (count+priv->wrfilled)>sizeof(priv->wrbuf) ) {
		/* To much data  for buffer. Reset buffer. */
		priv->wrfilled=0;
		return (0);
	}

	/* Copy data */
	if (from_user) {
		if (copy_from_user(priv->wrbuf+priv->wrfilled, buf, count)) {
			return -EFAULT;
		}
	} else {
		memcpy (priv->wrbuf+priv->wrfilled, buf, count);
	}  
	usb_serial_debug_data (__FILE__, __FUNCTION__, count,
		priv->wrbuf+priv->wrfilled);
	priv->wrfilled += count;

	if( priv->wrfilled >= 3 ) {
		wrexpected = ((int)priv->wrbuf[2]<<8)+priv->wrbuf[1]+3;
		dbg("%s - expected data: %d", __FUNCTION__, wrexpected);
	} else {
		wrexpected = sizeof(priv->wrbuf);
	}

	if( priv->wrfilled >= wrexpected ) {
		/* We have enough data to begin transmission */
		int length;

		dbg("%s - transmitting data (frame 1)", __FUNCTION__);
		length = (wrexpected > port->bulk_out_size) ? port->bulk_out_size : wrexpected;

		memcpy (port->write_urb->transfer_buffer, priv->wrbuf, length );
		priv->wrsent=length;

		/* set up our urb */
		FILL_BULK_URB(port->write_urb, serial->dev, 
			      usb_sndbulkpipe(serial->dev, port->bulk_out_endpointAddress),
			      port->write_urb->transfer_buffer, length,
			      ((serial->type->write_bulk_callback) ? 
			       serial->type->write_bulk_callback : 
			       cyberjack_write_bulk_callback), 
			      port);

		/* send the data out the bulk port */
		result = usb_submit_urb(port->write_urb);
		if (result) {
			err("%s - failed submitting write urb, error %d", __FUNCTION__, result);
			/* Throw away data. No better idea what to do with it. */
			priv->wrfilled=0;
			priv->wrsent=0;
			return 0;
		}

		dbg("%s - priv->wrsent=%d", __FUNCTION__,priv->wrsent);
		dbg("%s - priv->wrfilled=%d", __FUNCTION__,priv->wrfilled);

		if( priv->wrsent>=priv->wrfilled ) {
			dbg("%s - buffer cleaned", __FUNCTION__);
			memset( priv->wrbuf, 0, sizeof(priv->wrbuf) );
			priv->wrfilled=0;
			priv->wrsent=0;
		}
	}

	return (count);
} 

static void cyberjack_read_int_callback( struct urb *urb )
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
	struct cyberjack_private *priv = (struct cyberjack_private *)port->private;
	struct usb_serial *serial;
	unsigned char *data = urb->transfer_buffer;

	if (port_paranoia_check (port, __FUNCTION__)) return;

	dbg("%s - port %d", __FUNCTION__, port->number);

	/* the urb might have been killed. */
	if (urb->status)
		return;

	serial = port->serial;
	if (serial_paranoia_check (serial, __FUNCTION__)) return;

	usb_serial_debug_data (__FILE__, __FUNCTION__, urb->actual_length, data);

	/* React only to interrupts signaling a bulk_in transfer */
	if( (urb->actual_length==4) && (data[0]==0x01) ) {
		short old_rdtodo = priv->rdtodo;
		int result;

		/* This is a announcement of comming bulk_ins. */
		unsigned short size = ((unsigned short)data[3]<<8)+data[2]+3;

		if( (size>259) || (size==0) ) {
			dbg( "Bad announced bulk_in data length: %d", size );
			/* Dunno what is most reliable to do here. */
			/* return; */
		}

		if( (old_rdtodo+size)<(old_rdtodo) ) {
			dbg( "To many bulk_in urbs to do." );
			return;
		}

		/* "+=" is probably more fault tollerant than "=" */
		priv->rdtodo += size;

		dbg("%s - rdtodo: %d", __FUNCTION__, priv->rdtodo);

		if( !old_rdtodo ) {
			port->read_urb->dev = port->serial->dev;
			result = usb_submit_urb(port->read_urb);
			if( result )
				err("%s - failed resubmitting read urb, error %d", __FUNCTION__, result);
			dbg("%s - usb_submit_urb(read urb)", __FUNCTION__);
		}
	}
}

static void cyberjack_read_bulk_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
	struct cyberjack_private *priv = (struct cyberjack_private *)port->private;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);
	struct tty_struct *tty;
	unsigned char *data = urb->transfer_buffer;
	int i;
	int result;

	dbg("%s - port %d", __FUNCTION__, port->number);
	
	if (!serial) {
		dbg("%s - bad serial pointer, exiting", __FUNCTION__);
		return;
	}

	if (urb->status) {
		usb_serial_debug_data (__FILE__, __FUNCTION__, urb->actual_length, urb->transfer_buffer);
		dbg("%s - nonzero read bulk status received: %d", __FUNCTION__, urb->status);
		return;
	}

	usb_serial_debug_data (__FILE__, __FUNCTION__, urb->actual_length, data);

	tty = port->tty;
	if (urb->actual_length) {
		for (i = 0; i < urb->actual_length ; ++i) {
			/* if we insert more than TTY_FLIPBUF_SIZE characters, we drop them. */
			if(tty->flip.count >= TTY_FLIPBUF_SIZE) {
				tty_flip_buffer_push(tty);
			}
			/* this doesn't actually push the data through unless tty->low_latency is set */
			tty_insert_flip_char(tty, data[i], 0);
		}
	  	tty_flip_buffer_push(tty);
	}

	/* Reduce urbs to do by one. */
	priv->rdtodo-=urb->actual_length;
	/* Just to be sure */
	if( priv->rdtodo<0 ) priv->rdtodo=0;

	dbg("%s - rdtodo: %d", __FUNCTION__, priv->rdtodo);

	/* Continue to read if we have still urbs to do. */
	if( priv->rdtodo /* || (urb->actual_length==port->bulk_in_endpointAddress)*/ ) {
		port->read_urb->dev = port->serial->dev;
		result = usb_submit_urb(port->read_urb);
		if (result)
			err("%s - failed resubmitting read urb, error %d", __FUNCTION__, result);
		dbg("%s - usb_submit_urb(read urb)", __FUNCTION__);
	}
}

static void cyberjack_write_bulk_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
	struct cyberjack_private *priv = (struct cyberjack_private *)port->private;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);

	dbg("%s - port %d", __FUNCTION__, port->number);
	
	if (!serial) {
		dbg("%s - bad serial pointer, exiting", __FUNCTION__);
		return;
	}

	if (urb->status) {
		dbg("%s - nonzero write bulk status received: %d", __FUNCTION__, urb->status);
		return;
	}

	/* only do something if we have more data to send */
	if( priv->wrfilled ) {
		int length, blksize, result;

		if (port->write_urb->status == -EINPROGRESS) {
			dbg("%s - already writing", __FUNCTION__);
			return;
		}

		dbg("%s - transmitting data (frame n)", __FUNCTION__);

		length = ((priv->wrfilled - priv->wrsent) > port->bulk_out_size) ?
			port->bulk_out_size : (priv->wrfilled - priv->wrsent);

		memcpy (port->write_urb->transfer_buffer, priv->wrbuf + priv->wrsent,
			length );
		priv->wrsent+=length;

		/* set up our urb */
		FILL_BULK_URB(port->write_urb, serial->dev, 
			      usb_sndbulkpipe(serial->dev, port->bulk_out_endpointAddress),
			      port->write_urb->transfer_buffer, length,
			      ((serial->type->write_bulk_callback) ? 
			       serial->type->write_bulk_callback : 
			       cyberjack_write_bulk_callback), 
			      port);

		/* send the data out the bulk port */
		result = usb_submit_urb(port->write_urb);
		if (result) {
			err("%s - failed submitting write urb, error %d", __FUNCTION__, result);
			/* Throw away data. No better idea what to do with it. */
			priv->wrfilled=0;
			priv->wrsent=0;
			queue_task(&port->tqueue, &tq_immediate);
			mark_bh(IMMEDIATE_BH);
			return;
		}

		dbg("%s - priv->wrsent=%d", __FUNCTION__,priv->wrsent);
		dbg("%s - priv->wrfilled=%d", __FUNCTION__,priv->wrfilled);

		blksize = ((int)priv->wrbuf[2]<<8)+priv->wrbuf[1]+3;

		if( (priv->wrsent>=priv->wrfilled) || (priv->wrsent>=blksize) ) {
			dbg("%s - buffer cleaned", __FUNCTION__);
			memset( priv->wrbuf, 0, sizeof(priv->wrbuf) );
			priv->wrfilled=0;
			priv->wrsent=0;
		}

		queue_task(&port->tqueue, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
		return;
	}

	queue_task(&port->tqueue, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
	
	return;
}

static int __init cyberjack_init (void)
{
	usb_serial_register (&cyberjack_device);

	info(DRIVER_VERSION " " DRIVER_AUTHOR);
	info(DRIVER_DESC);

	return 0;
}

static void __exit cyberjack_exit (void)
{
	usb_serial_deregister (&cyberjack_device);
}

module_init(cyberjack_init);
module_exit(cyberjack_exit);

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug enabled or not");

