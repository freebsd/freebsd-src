/*
 * USB ZyXEL omni.net LCD PLUS driver
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 * See Documentation/usb/usb-serial.txt for more information on using this driver
 *
 * Please report both successes and troubles to the author at omninet@kroah.com
 * 
 * (05/30/2001) gkh
 *	switched from using spinlock to a semaphore, which fixes lots of problems.
 *
 * (04/08/2001) gb
 *	Identify version on module load.
 *
 * (11/01/2000) Adam J. Richter
 *	usb_device_id table support
 * 
 * (10/05/2000) gkh
 *	Fixed bug with urb->dev not being set properly, now that the usb
 *	core needs it.
 * 
 * (08/28/2000) gkh
 *	Added locks for SMP safeness.
 *	Fixed MOD_INC and MOD_DEC logic and the ability to open a port more 
 *	than once.
 *	Fixed potential race in omninet_write_bulk_callback
 *
 * (07/19/2000) gkh
 *	Added module_init and module_exit functions to handle the fact that this
 *	driver is a loadable module now.
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


/*
 * Version Information
 */
#define DRIVER_VERSION "v1.1"
#define DRIVER_AUTHOR "Anonymous"
#define DRIVER_DESC "USB ZyXEL omni.net LCD PLUS Driver"

#define ZYXEL_VENDOR_ID		0x0586
#define ZYXEL_OMNINET_ID	0x1000

/* function prototypes */
static int  omninet_open		(struct usb_serial_port *port, struct file *filp);
static void omninet_close		(struct usb_serial_port *port, struct file *filp);
static void omninet_read_bulk_callback	(struct urb *urb);
static void omninet_write_bulk_callback	(struct urb *urb);
static int  omninet_write		(struct usb_serial_port *port, int from_user, const unsigned char *buf, int count);
static int  omninet_write_room		(struct usb_serial_port *port);
static void omninet_shutdown		(struct usb_serial *serial);

static struct usb_device_id id_table [] = {
	{ USB_DEVICE(ZYXEL_VENDOR_ID, ZYXEL_OMNINET_ID) },
	{ }						/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, id_table);


static struct usb_serial_device_type zyxel_omninet_device = {
	.owner =		THIS_MODULE,
	.name =			"ZyXEL - omni.net lcd plus usb",
	.id_table =		id_table,
	.num_interrupt_in =	1,
	.num_bulk_in =		1,
	.num_bulk_out =		2,
	.num_ports =		1,
	.open =			omninet_open,
	.close =		omninet_close,
	.write =		omninet_write,
	.write_room =		omninet_write_room,
	.read_bulk_callback =	omninet_read_bulk_callback,
	.write_bulk_callback =	omninet_write_bulk_callback,
	.shutdown =		omninet_shutdown,
};


/* The protocol.
 *
 * The omni.net always exchange 64 bytes of data with the host. The first
 * four bytes are the control header, you can see it in the above structure.
 *
 * oh_seq is a sequence number. Don't know if/how it's used.
 * oh_len is the length of the data bytes in the packet.
 * oh_xxx Bit-mapped, related to handshaking and status info.
 *	I normally set it to 0x03 in trasmitted frames.
 *	7: Active when the TA is in a CONNECTed state.
 *	6: unknown
 *	5: handshaking, unknown
 *	4: handshaking, unknown
 *	3: unknown, usually 0
 *	2: unknown, usually 0
 *	1: handshaking, unknown, usually set to 1 in trasmitted frames
 *	0: handshaking, unknown, usually set to 1 in trasmitted frames
 * oh_pad Probably a pad byte.
 *
 * After the header you will find data bytes if oh_len was greater than zero.
 *
 */

struct omninet_header
{
	__u8	oh_seq;
	__u8	oh_len;
	__u8	oh_xxx;
	__u8	oh_pad;
};

struct omninet_data
{
	__u8	od_outseq;	// Sequence number for bulk_out URBs
};

static int omninet_open (struct usb_serial_port *port, struct file *filp)
{
	struct usb_serial	*serial;
	struct usb_serial_port	*wport;
	struct omninet_data	*od;
	int			result = 0;

	if (port_paranoia_check (port, __FUNCTION__))
		return -ENODEV;

	dbg("%s - port %d", __FUNCTION__, port->number);

	serial = get_usb_serial (port, __FUNCTION__);
	if (!serial)
		return -ENODEV;

	od = kmalloc( sizeof(struct omninet_data), GFP_KERNEL );
	if( !od ) {
		err("%s- kmalloc(%Zd) failed.", __FUNCTION__, sizeof(struct omninet_data));
		return -ENOMEM;
	}

	port->private = od;
	wport = &serial->port[1];
	wport->tty = port->tty;

	/* Start reading from the device */
	FILL_BULK_URB(port->read_urb, serial->dev, 
		      usb_rcvbulkpipe(serial->dev, port->bulk_in_endpointAddress),
		      port->read_urb->transfer_buffer, port->read_urb->transfer_buffer_length,
		      omninet_read_bulk_callback, port);
	result = usb_submit_urb(port->read_urb);
	if (result)
		err("%s - failed submitting read urb, error %d", __FUNCTION__, result);

	return result;
}

static void omninet_close (struct usb_serial_port *port, struct file * filp)
{
	struct usb_serial 	*serial;
	struct usb_serial_port 	*wport;
	struct omninet_data 	*od;

	if (port_paranoia_check (port, __FUNCTION__))
		return;

	dbg("%s - port %d", __FUNCTION__, port->number);

	serial = get_usb_serial (port, __FUNCTION__);
	if (!serial)
		return;

	if (serial->dev) {
		wport = &serial->port[1];
		usb_unlink_urb (wport->write_urb);
		usb_unlink_urb (port->read_urb);
	}

	od = (struct omninet_data *)port->private;
	if (od)
		kfree(od);
}


#define OMNINET_DATAOFFSET	0x04
#define OMNINET_HEADERLEN	sizeof(struct omninet_header)
#define OMNINET_BULKOUTSIZE 	(64 - OMNINET_HEADERLEN)

static void omninet_read_bulk_callback (struct urb *urb)
{
	struct usb_serial_port 	*port 	= (struct usb_serial_port *)urb->context;
	struct usb_serial	*serial = get_usb_serial (port, __FUNCTION__);

	unsigned char 		*data 	= urb->transfer_buffer;
	struct omninet_header 	*header = (struct omninet_header *) &data[0];

	int i;
	int result;

//	dbg("omninet_read_bulk_callback");

	if (!serial) {
		dbg("%s - bad serial pointer, exiting", __FUNCTION__);
		return;
	}

	if (urb->status) {
		dbg("%s - nonzero read bulk status received: %d", __FUNCTION__, urb->status);
		return;
	}

	if ((debug) && (header->oh_xxx != 0x30)) {
		if (urb->actual_length) {
			printk (KERN_DEBUG __FILE__ ": omninet_read %d: ", header->oh_len);
			for (i = 0; i < (header->oh_len + OMNINET_HEADERLEN); i++) {
				printk ("%.2x ", data[i]);
			}
			printk ("\n");
		}
	}

	if (urb->actual_length && header->oh_len) {
		for (i = 0; i < header->oh_len; i++) {
			 tty_insert_flip_char(port->tty, data[OMNINET_DATAOFFSET + i], 0);
	  	}
	  	tty_flip_buffer_push(port->tty);
	}

	/* Continue trying to always read  */
	FILL_BULK_URB(urb, serial->dev, 
		      usb_rcvbulkpipe(serial->dev, port->bulk_in_endpointAddress),
		      urb->transfer_buffer, urb->transfer_buffer_length,
		      omninet_read_bulk_callback, port);
	result = usb_submit_urb(urb);
	if (result)
		err("%s - failed resubmitting read urb, error %d", __FUNCTION__, result);

	return;
}

static int omninet_write (struct usb_serial_port *port, int from_user, const unsigned char *buf, int count)
{
	struct usb_serial 	*serial	= port->serial;
	struct usb_serial_port 	*wport	= &serial->port[1];

	struct omninet_data 	*od 	= (struct omninet_data   *) port->private;
	struct omninet_header	*header = (struct omninet_header *) wport->write_urb->transfer_buffer;

	int			result;

//	dbg("omninet_write port %d", port->number);

	if (count == 0) {
		dbg("%s - write request of 0 bytes", __FUNCTION__);
		return (0);
	}
	if (wport->write_urb->status == -EINPROGRESS) {
		dbg("%s - already writing", __FUNCTION__);
		return (0);
	}

	count = (count > OMNINET_BULKOUTSIZE) ? OMNINET_BULKOUTSIZE : count;

	if (from_user) {
		if (copy_from_user(wport->write_urb->transfer_buffer + OMNINET_DATAOFFSET, buf, count) != 0) {
			result = -EFAULT;
			goto exit;
		}
	}
	else {
		memcpy (wport->write_urb->transfer_buffer + OMNINET_DATAOFFSET, buf, count);
	}

	usb_serial_debug_data (__FILE__, __FUNCTION__, count, wport->write_urb->transfer_buffer);

	header->oh_seq 	= od->od_outseq++;
	header->oh_len 	= count;
	header->oh_xxx  = 0x03;
	header->oh_pad 	= 0x00;

	/* send the data out the bulk port, always 64 bytes */
	wport->write_urb->transfer_buffer_length = 64;

	wport->write_urb->dev = serial->dev;
	result = usb_submit_urb(wport->write_urb);
	if (result)
		err("%s - failed submitting write urb, error %d", __FUNCTION__, result);
	else
		result = count;

exit:	
	return result;
}


static int omninet_write_room (struct usb_serial_port *port)
{
	struct usb_serial 	*serial = port->serial;
	struct usb_serial_port 	*wport 	= &serial->port[1];

	int room = 0; // Default: no room

	if (wport->write_urb->status != -EINPROGRESS)
		room = wport->bulk_out_size - OMNINET_HEADERLEN;

//	dbg("omninet_write_room returns %d", room);

	return (room);
}

static void omninet_write_bulk_callback (struct urb *urb)
{
/*	struct omninet_header	*header = (struct omninet_header  *) urb->transfer_buffer; */
	struct usb_serial_port 	*port   = (struct usb_serial_port *) urb->context;
	struct usb_serial 	*serial;

//	dbg("omninet_write_bulk_callback, port %0x\n", port);


	if (port_paranoia_check (port, __FUNCTION__)) {
		return;
	}

	serial = port->serial;
	if (serial_paranoia_check (serial, __FUNCTION__)) {
		return;
	}

	if (urb->status) {
		dbg("%s - nonzero write bulk status received: %d", __FUNCTION__, urb->status);
		return;
	}

	queue_task(&port->tqueue, &tq_immediate);
	mark_bh(IMMEDIATE_BH);

//	dbg("omninet_write_bulk_callback, tty %0x\n", tty);

	return;
}


static void omninet_shutdown (struct usb_serial *serial)
{
	dbg ("%s", __FUNCTION__);
}


static int __init omninet_init (void)
{
	usb_serial_register (&zyxel_omninet_device);
	info(DRIVER_VERSION ":" DRIVER_DESC);
	return 0;
}


static void __exit omninet_exit (void)
{
	usb_serial_deregister (&zyxel_omninet_device);
}


module_init(omninet_init);
module_exit(omninet_exit);

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug enabled or not");

