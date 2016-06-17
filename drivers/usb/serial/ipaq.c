/*
 * USB Compaq iPAQ driver
 *
 *	Copyright (C) 2001 - 2002
 *	    Ganesh Varadarajan <ganesh@veritas.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 * (12/12/2002) ganesh
 * 	Added support for practically all devices supported by ActiveSync
 * 	on Windows. Thanks to Wes Cilldhaire <billybobjoehenrybob@hotmail.com>.
 *
 * (26/11/2002) ganesh
 * 	Added insmod options to specify product and vendor id.
 * 	Use modprobe ipaq vendor=0xfoo product=0xbar
 *
 * (26/7/2002) ganesh
 * 	Fixed up broken error handling in ipaq_open. Retry the "kickstart"
 * 	packet much harder - this drastically reduces connection failures.
 *
 * (30/4/2002) ganesh
 * 	Added support for the Casio EM500. Completely untested. Thanks
 * 	to info from Nathan <wfilardo@fuse.net>
 *
 * (19/3/2002) ganesh
 * 	Don't submit urbs while holding spinlocks. Thanks to Greg for pointing
 * 	this out.
 *
 * (8/3/2002) ganesh
 * 	The ipaq sometimes emits a '\0' before the CLIENT string. At this
 * 	point of time, the ppp ldisc is not yet attached to the tty, so
 * 	n_tty echoes "^ " to the ipaq, which messes up the chat. In 2.5.6-pre2
 * 	this causes a panic because echo_char() tries to sleep in interrupt
 * 	context.
 * 	The fix is to tell the upper layers that this is a raw device so that
 * 	echoing is suppressed. Thanks to Lyle Lindholm for a detailed bug
 * 	report.
 *
 * (25/2/2002) ganesh
 * 	Added support for the HP Jornada 548 and 568. Completely untested.
 * 	Thanks to info from Heath Robinson and Arieh Davidoff.
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
	static int debug = 0;
#endif

#include "usb-serial.h"
#include "ipaq.h"

#define KP_RETRIES	100

/*
 * Version Information
 */

#define DRIVER_VERSION "v0.5"
#define DRIVER_AUTHOR "Ganesh Varadarajan <ganesh@veritas.com>"
#define DRIVER_DESC "USB PocketPC PDA driver"

static int	product, vendor;

/* Function prototypes for an ipaq */
static int  ipaq_open (struct usb_serial_port *port, struct file *filp);
static void ipaq_close (struct usb_serial_port *port, struct file *filp);
static int  ipaq_startup (struct usb_serial *serial);
static void ipaq_shutdown (struct usb_serial *serial);
static int ipaq_write(struct usb_serial_port *port, int from_user, const unsigned char *buf,
		       int count);
static int ipaq_write_bulk(struct usb_serial_port *port, int from_user, const unsigned char *buf,
			   int count);
static void ipaq_write_gather(struct usb_serial_port *port);
static void ipaq_read_bulk_callback (struct urb *urb);
static void ipaq_write_bulk_callback(struct urb *urb);
static int ipaq_write_room(struct usb_serial_port *port);
static int ipaq_chars_in_buffer(struct usb_serial_port *port);
static void ipaq_destroy_lists(struct usb_serial_port *port);


static struct usb_device_id ipaq_id_table [] = {
	/* The first entry is a placeholder for the insmod-specified device */
	{ USB_DEVICE(COMPAQ_VENDOR_ID, COMPAQ_IPAQ_ID) },
	{ USB_DEVICE(ASKEY_VENDOR_ID, ASKEY_PRODUCT_ID) },
	{ USB_DEVICE(BCOM_VENDOR_ID, BCOM_0065_ID) },
	{ USB_DEVICE(BCOM_VENDOR_ID, BCOM_0066_ID) },
	{ USB_DEVICE(BCOM_VENDOR_ID, BCOM_0067_ID) },
	{ USB_DEVICE(CASIO_VENDOR_ID, CASIO_2001_ID) },
	{ USB_DEVICE(CASIO_VENDOR_ID, CASIO_EM500_ID) },
	{ USB_DEVICE(COMPAQ_VENDOR_ID, COMPAQ_IPAQ_ID) },
	{ USB_DEVICE(COMPAQ_VENDOR_ID, COMPAQ_0032_ID) },
	{ USB_DEVICE(DELL_VENDOR_ID, DELL_AXIM_ID) },
	{ USB_DEVICE(FSC_VENDOR_ID, FSC_LOOX_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_JORNADA_548_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_JORNADA_568_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_2016_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_2116_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_2216_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_3016_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_3116_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_3216_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_4016_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_4116_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_4216_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_5016_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_5116_ID) },
	{ USB_DEVICE(HP_VENDOR_ID, HP_5216_ID) },
	{ USB_DEVICE(LINKUP_VENDOR_ID, LINKUP_PRODUCT_ID) },
	{ USB_DEVICE(MICROSOFT_VENDOR_ID, MICROSOFT_00CE_ID) },
	{ USB_DEVICE(PORTATEC_VENDOR_ID, PORTATEC_PRODUCT_ID) },
	{ USB_DEVICE(ROVER_VENDOR_ID, ROVER_P5_ID) },
	{ USB_DEVICE(SAGEM_VENDOR_ID, SAGEM_WIRELESS_ID) },
	{ USB_DEVICE(SOCKET_VENDOR_ID, SOCKET_PRODUCT_ID) },
	{ USB_DEVICE(TOSHIBA_VENDOR_ID, TOSHIBA_PRODUCT_ID) },
	{ USB_DEVICE(TOSHIBA_VENDOR_ID, TOSHIBA_E310_ID) },
	{ USB_DEVICE(TOSHIBA_VENDOR_ID, TOSHIBA_E740_ID) },
	{ USB_DEVICE(TOSHIBA_VENDOR_ID, TOSHIBA_E335_ID) },
	{ USB_DEVICE(HTC_VENDOR_ID, HTC_PRODUCT_ID) },
	{ USB_DEVICE(NEC_VENDOR_ID, NEC_PRODUCT_ID) },
	{ USB_DEVICE(ASUS_VENDOR_ID, ASUS_A600_PRODUCT_ID) },
	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, ipaq_id_table);

/* All of the device info needed for the Compaq iPAQ */
struct usb_serial_device_type ipaq_device = {
	.owner =		THIS_MODULE,
	.name =			"PocketPC PDA",
	.id_table =		ipaq_id_table,
	.num_interrupt_in =	NUM_DONT_CARE,
	.num_bulk_in =		1,
	.num_bulk_out =		1,
	.num_ports =		1,
	.open =			ipaq_open,
	.close =		ipaq_close,
	.startup =		ipaq_startup,
	.shutdown =		ipaq_shutdown,
	.write =		ipaq_write,
	.write_room =		ipaq_write_room,
	.chars_in_buffer =	ipaq_chars_in_buffer,
	.read_bulk_callback =	ipaq_read_bulk_callback,
	.write_bulk_callback =	ipaq_write_bulk_callback,
};

static spinlock_t	write_list_lock;
static int		bytes_in;
static int		bytes_out;

static int ipaq_open(struct usb_serial_port *port, struct file *filp)
{
	struct usb_serial	*serial = port->serial;
	struct ipaq_private	*priv;
	struct ipaq_packet	*pkt;
	int			i, result = 0;
	int			retries = KP_RETRIES;

	if (port_paranoia_check(port, __FUNCTION__)) {
		return -ENODEV;
	}
	
	dbg("%s - port %d", __FUNCTION__, port->number);

	bytes_in = 0;
	bytes_out = 0;
	priv = (struct ipaq_private *)kmalloc(sizeof(struct ipaq_private), GFP_KERNEL);
	if (priv == NULL) {
		err("%s - Out of memory", __FUNCTION__);
		return -ENOMEM;
	}
	port->private = (void *)priv;
	priv->active = 0;
	priv->queue_len = 0;
	INIT_LIST_HEAD(&priv->queue);
	INIT_LIST_HEAD(&priv->freelist);

	for (i = 0; i < URBDATA_QUEUE_MAX / PACKET_SIZE; i++) {
		pkt = kmalloc(sizeof(struct ipaq_packet), GFP_KERNEL);
		if (pkt == NULL) {
			goto enomem;
		}
		pkt->data = kmalloc(PACKET_SIZE, GFP_KERNEL);
		if (pkt->data == NULL) {
			kfree(pkt);
			goto enomem;
		}
		pkt->len = 0;
		pkt->written = 0;
		INIT_LIST_HEAD(&pkt->list);
		list_add(&pkt->list, &priv->freelist);
		priv->free_len += PACKET_SIZE;
	}

	/*
	 * Force low latency on. This will immediately push data to the line
	 * discipline instead of queueing.
	 */

	port->tty->low_latency = 1;
	port->tty->raw = 1;
	port->tty->real_raw = 1;

	/*
	 * Lose the small buffers usbserial provides. Make larger ones.
	 */

	kfree(port->bulk_in_buffer);
	kfree(port->bulk_out_buffer);
	port->bulk_in_buffer = kmalloc(URBDATA_SIZE, GFP_KERNEL);
	if (port->bulk_in_buffer == NULL) {
		goto enomem;
	}
	port->bulk_out_buffer = kmalloc(URBDATA_SIZE, GFP_KERNEL);
	if (port->bulk_out_buffer == NULL) {
		kfree(port->bulk_in_buffer);
		goto enomem;
	}
	port->read_urb->transfer_buffer = port->bulk_in_buffer;
	port->write_urb->transfer_buffer = port->bulk_out_buffer;
	port->read_urb->transfer_buffer_length = URBDATA_SIZE;
	port->bulk_out_size = port->write_urb->transfer_buffer_length = URBDATA_SIZE;
	
	/* Start reading from the device */
	FILL_BULK_URB(port->read_urb, serial->dev, 
		      usb_rcvbulkpipe(serial->dev, port->bulk_in_endpointAddress),
		      port->read_urb->transfer_buffer, port->read_urb->transfer_buffer_length,
		      ipaq_read_bulk_callback, port);
	result = usb_submit_urb(port->read_urb);
	if (result) {
		err("%s - failed submitting read urb, error %d", __FUNCTION__, result);
		goto error;
	}

	/*
	 * Send out control message observed in win98 sniffs. Not sure what
	 * it does, but from empirical observations, it seems that the device
	 * will start the chat sequence once one of these messages gets
	 * through. Since this has a reasonably high failure rate, we retry
	 * several times.
	 */

	while (retries--) {
		result = usb_control_msg(serial->dev,
				usb_sndctrlpipe(serial->dev, 0), 0x22, 0x21,
				0x1, 0, NULL, 0, HZ / 10 + 1);
		if (result == 0) {
			return 0;
		}
	}
	err("%s - failed doing control urb, error %d", __FUNCTION__, result);
	goto error;

enomem:
	result = -ENOMEM;
	err("%s - Out of memory", __FUNCTION__);
error:
	ipaq_destroy_lists(port);
	kfree(priv);
	return result;
}


static void ipaq_close(struct usb_serial_port *port, struct file *filp)
{
	struct usb_serial	*serial;
	struct ipaq_private	*priv = port->private;

	if (port_paranoia_check(port, __FUNCTION__)) {
		return; 
	}
	
	dbg("%s - port %d", __FUNCTION__, port->number);
			 
	serial = get_usb_serial(port, __FUNCTION__);
	if (!serial)
		return;

	/*
	 * shut down bulk read and write
	 */

	usb_unlink_urb(port->write_urb);
	usb_unlink_urb(port->read_urb);
	ipaq_destroy_lists(port);
	kfree(priv);
	port->private = NULL;

	/* Uncomment the following line if you want to see some statistics in your syslog */
	/* info ("Bytes In = %d  Bytes Out = %d", bytes_in, bytes_out); */
}

static void ipaq_read_bulk_callback(struct urb *urb)
{
	struct usb_serial_port	*port = (struct usb_serial_port *)urb->context;
	struct usb_serial	*serial = get_usb_serial (port, __FUNCTION__);
	struct tty_struct	*tty;
	unsigned char		*data = urb->transfer_buffer;
	int			i, result;

	if (port_paranoia_check(port, __FUNCTION__))
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
	FILL_BULK_URB(port->read_urb, serial->dev, 
		      usb_rcvbulkpipe(serial->dev, port->bulk_in_endpointAddress),
		      port->read_urb->transfer_buffer, port->read_urb->transfer_buffer_length,
		      ipaq_read_bulk_callback, port);
	result = usb_submit_urb(port->read_urb);
	if (result)
		err("%s - failed resubmitting read urb, error %d", __FUNCTION__, result);
	return;
}

static int ipaq_write(struct usb_serial_port *port, int from_user, const unsigned char *buf,
		       int count)
{
	const unsigned char	*current_position = buf;
	int			bytes_sent = 0;
	int			transfer_size;

	dbg("%s - port %d", __FUNCTION__, port->number);

	usb_serial_debug_data(__FILE__, __FUNCTION__, count, buf);
	
	while (count > 0) {
		transfer_size = min(count, PACKET_SIZE);
		if (ipaq_write_bulk(port, from_user, current_position, transfer_size)) {
			break;
		}
		current_position += transfer_size;
		bytes_sent += transfer_size;
		count -= transfer_size;
		bytes_out += transfer_size;
	}

	return bytes_sent;
} 

static int ipaq_write_bulk(struct usb_serial_port *port, int from_user, const unsigned char *buf,
			   int count)
{
	struct ipaq_private	*priv = port->private;
	struct ipaq_packet	*pkt = NULL;
	int			result = 0;
	unsigned long		flags;

	if (priv->free_len <= 0) {
		dbg("%s - we're stuffed", __FUNCTION__);
		return -EAGAIN;
	}

	spin_lock_irqsave(&write_list_lock, flags);
	if (!list_empty(&priv->freelist)) {
		pkt = list_entry(priv->freelist.next, struct ipaq_packet, list);
		list_del(&pkt->list);
		priv->free_len -= PACKET_SIZE;
	}
	spin_unlock_irqrestore(&write_list_lock, flags);
	if (pkt == NULL) {
		dbg("%s - we're stuffed", __FUNCTION__);
		return -EAGAIN;
	}

	if (from_user) {
		if (copy_from_user(pkt->data, buf, count))
			return -EFAULT;
	} else {
		memcpy(pkt->data, buf, count);
	}
	usb_serial_debug_data(__FILE__, __FUNCTION__, count, pkt->data);

	pkt->len = count;
	pkt->written = 0;
	spin_lock_irqsave(&write_list_lock, flags);
	list_add_tail(&pkt->list, &priv->queue);
	priv->queue_len += count;
	if (priv->active == 0) {
		priv->active = 1;
		ipaq_write_gather(port);
		spin_unlock_irqrestore(&write_list_lock, flags);
		result = usb_submit_urb(port->write_urb);
		if (result) {
			err("%s - failed submitting write urb, error %d", __FUNCTION__, result);
		}
	} else {
		spin_unlock_irqrestore(&write_list_lock, flags);
	}
	return result;
}

static void ipaq_write_gather(struct usb_serial_port *port)
{
	struct ipaq_private	*priv = (struct ipaq_private *)port->private;
	struct usb_serial	*serial = port->serial;
	int			count, room;
	struct ipaq_packet	*pkt;
	struct urb		*urb = port->write_urb;
	struct list_head	*tmp;

	if (urb->status == -EINPROGRESS) {
		/* Should never happen */
		err("%s - flushing while urb is active !", __FUNCTION__);
		return;
	}
	room = URBDATA_SIZE;
	for (tmp = priv->queue.next; tmp != &priv->queue;) {
		pkt = list_entry(tmp, struct ipaq_packet, list);
		tmp = tmp->next;
		count = min(room, (int)(pkt->len - pkt->written));
		memcpy(urb->transfer_buffer + (URBDATA_SIZE - room),
		       pkt->data + pkt->written, count);
		room -= count;
		pkt->written += count;
		priv->queue_len -= count;
		if (pkt->written == pkt->len) {
			list_del(&pkt->list);
			list_add(&pkt->list, &priv->freelist);
			priv->free_len += PACKET_SIZE;
		}
		if (room == 0) {
			break;
		}
	}

	count = URBDATA_SIZE - room;
	FILL_BULK_URB(port->write_urb, serial->dev, 
		      usb_sndbulkpipe(serial->dev, port->bulk_out_endpointAddress),
		      port->write_urb->transfer_buffer, count, ipaq_write_bulk_callback,
		      port);
	return;
}

static void ipaq_write_bulk_callback(struct urb *urb)
{
	struct usb_serial_port	*port = (struct usb_serial_port *)urb->context;
	struct ipaq_private	*priv = (struct ipaq_private *)port->private;
	unsigned long		flags;
	int			result;

	if (port_paranoia_check (port, __FUNCTION__)) {
		return;
	}
	
	dbg("%s - port %d", __FUNCTION__, port->number);
	
	if (urb->status) {
		dbg("%s - nonzero write bulk status received: %d", __FUNCTION__, urb->status);
	}

	spin_lock_irqsave(&write_list_lock, flags);
	if (!list_empty(&priv->queue)) {
		ipaq_write_gather(port);
		spin_unlock_irqrestore(&write_list_lock, flags);
		result = usb_submit_urb(port->write_urb);
		if (result) {
			err("%s - failed submitting write urb, error %d", __FUNCTION__, result);
		}
	} else {
		priv->active = 0;
		spin_unlock_irqrestore(&write_list_lock, flags);
	}
	queue_task(&port->tqueue, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
	
	return;
}

static int ipaq_write_room(struct usb_serial_port *port)
{
	struct ipaq_private	*priv = (struct ipaq_private *)port->private;

	dbg("%s - freelen %d", __FUNCTION__, priv->free_len);
	return priv->free_len;
}

static int ipaq_chars_in_buffer(struct usb_serial_port *port)
{
	struct ipaq_private	*priv = (struct ipaq_private *)port->private;

	dbg("%s - queuelen %d", __FUNCTION__, priv->queue_len);
	return priv->queue_len;
}

static void ipaq_destroy_lists(struct usb_serial_port *port)
{
	struct ipaq_private	*priv = (struct ipaq_private *)port->private;
	struct list_head	*tmp;
	struct ipaq_packet	*pkt;

	for (tmp = priv->queue.next; tmp != &priv->queue;) {
		pkt = list_entry(tmp, struct ipaq_packet, list);
		tmp = tmp->next;
		kfree(pkt->data);
		kfree(pkt);
	}
	for (tmp = priv->freelist.next; tmp != &priv->freelist;) {
		pkt = list_entry(tmp, struct ipaq_packet, list);
		tmp = tmp->next;
		kfree(pkt->data);
		kfree(pkt);
	}
	return;
}


static int ipaq_startup(struct usb_serial *serial)
{
	dbg("%s", __FUNCTION__);
	usb_set_configuration(serial->dev, 1);
	return 0;
}

static void ipaq_shutdown(struct usb_serial *serial)
{
	dbg("%s", __FUNCTION__);
}

static int __init ipaq_init(void)
{
	spin_lock_init(&write_list_lock);
	info(DRIVER_DESC " " DRIVER_VERSION);
	if (vendor) {
		ipaq_id_table[0].idVendor = vendor;
		ipaq_id_table[0].idProduct = product;
	}
	usb_serial_register(&ipaq_device);

	return 0;
}


static void __exit ipaq_exit(void)
{
	usb_serial_deregister(&ipaq_device);
}


module_init(ipaq_init);
module_exit(ipaq_exit);

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug enabled or not");

MODULE_PARM(vendor, "h");
MODULE_PARM_DESC(vendor, "User specified USB idVendor");

MODULE_PARM(product, "h");
MODULE_PARM_DESC(product, "User specified USB idProduct");
