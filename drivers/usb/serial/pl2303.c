/*
 * Prolific PL2303 USB to serial adaptor driver
 *
 * Copyright (C) 2001-2003 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2003 IBM Corp.
 *
 * Original driver for 2.2.x by anonymous
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 * See Documentation/usb/usb-serial.txt for more information on using this driver
 * 2003_Apr_24 gkh
 *	Added line error reporting support.  Hopefully it is correct...
 *
 * 2001_Oct_06 gkh
 *	Added RTS and DTR line control.  Thanks to joe@bndlg.de for parts of it.
 *
 * 2001_Sep_19 gkh
 *	Added break support.
 *
 * 2001_Aug_30 gkh
 *	fixed oops in write_bulk_callback.
 *
 * 2001_Aug_28 gkh
 *	reworked buffer logic to be like other usb-serial drivers.  Hopefully
 *	removing some reported problems.
 *
 * 2001_Jun_06 gkh
 *	finished porting to 2.4 format.
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
#include <linux/serial.h>
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
#include "pl2303.h"

/*
 * Version Information
 */
#define DRIVER_VERSION "v0.10"
#define DRIVER_DESC "Prolific PL2303 USB to serial adaptor driver"



static struct usb_device_id id_table [] = {
	{ USB_DEVICE(PL2303_VENDOR_ID, PL2303_PRODUCT_ID) },
	{ USB_DEVICE(PL2303_VENDOR_ID, PL2303_PRODUCT_ID_RSAQ2) },
	{ USB_DEVICE(IODATA_VENDOR_ID, IODATA_PRODUCT_ID) },
	{ USB_DEVICE(ATEN_VENDOR_ID, ATEN_PRODUCT_ID) },
	{ USB_DEVICE(ATEN_VENDOR_ID2, ATEN_PRODUCT_ID) },
	{ USB_DEVICE(ELCOM_VENDOR_ID, ELCOM_PRODUCT_ID) },
	{ USB_DEVICE(ITEGNO_VENDOR_ID, ITEGNO_PRODUCT_ID) },
	{ USB_DEVICE(MA620_VENDOR_ID, MA620_PRODUCT_ID) },
	{ USB_DEVICE(RATOC_VENDOR_ID, RATOC_PRODUCT_ID) },
	{ USB_DEVICE(TRIPP_VENDOR_ID, TRIPP_PRODUCT_ID) },
	{ USB_DEVICE(RADIOSHACK_VENDOR_ID, RADIOSHACK_PRODUCT_ID) },
	{ USB_DEVICE(DCU10_VENDOR_ID, DCU10_PRODUCT_ID) },
	{ USB_DEVICE(SITECOM_VENDOR_ID, SITECOM_PRODUCT_ID) },
	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, id_table);


#define SET_LINE_REQUEST_TYPE		0x21
#define SET_LINE_REQUEST		0x20

#define SET_CONTROL_REQUEST_TYPE	0x21
#define SET_CONTROL_REQUEST		0x22
#define CONTROL_DTR			0x01
#define CONTROL_RTS			0x02

#define BREAK_REQUEST_TYPE		0x21
#define BREAK_REQUEST			0x23	
#define BREAK_ON			0xffff
#define BREAK_OFF			0x0000

#define GET_LINE_REQUEST_TYPE		0xa1
#define GET_LINE_REQUEST		0x21

#define VENDOR_WRITE_REQUEST_TYPE	0x40
#define VENDOR_WRITE_REQUEST		0x01

#define VENDOR_READ_REQUEST_TYPE	0xc0
#define VENDOR_READ_REQUEST		0x01

#define UART_STATE			0x08
#define UART_DCD			0x01
#define UART_DSR			0x02
#define UART_BREAK_ERROR		0x04
#define UART_RING			0x08
#define UART_FRAME_ERROR		0x10
#define UART_PARITY_ERROR		0x20
#define UART_OVERRUN_ERROR		0x40
#define UART_CTS			0x80

/* function prototypes for a PL2303 serial converter */
static int pl2303_open (struct usb_serial_port *port, struct file *filp);
static void pl2303_close (struct usb_serial_port *port, struct file *filp);
static void pl2303_set_termios (struct usb_serial_port *port,
				struct termios *old);
static int pl2303_ioctl (struct usb_serial_port *port, struct file *file,
			 unsigned int cmd, unsigned long arg);
static void pl2303_read_int_callback (struct urb *urb);
static void pl2303_read_bulk_callback (struct urb *urb);
static void pl2303_write_bulk_callback (struct urb *urb);
static int pl2303_write (struct usb_serial_port *port, int from_user,
			 const unsigned char *buf, int count);
static void pl2303_break_ctl(struct usb_serial_port *port,int break_state);
static int pl2303_startup (struct usb_serial *serial);
static void pl2303_shutdown (struct usb_serial *serial);


/* All of the device info needed for the PL2303 SIO serial converter */
static struct usb_serial_device_type pl2303_device = {
	.owner =		THIS_MODULE,
	.name =			"PL-2303",
	.id_table =		id_table,
	.num_interrupt_in =	NUM_DONT_CARE,
	.num_bulk_in =		1,
	.num_bulk_out =		1,
	.num_ports =		1,
	.open =			pl2303_open,
	.close =		pl2303_close,
	.write =		pl2303_write,
	.ioctl =		pl2303_ioctl,
	.break_ctl =		pl2303_break_ctl,
	.set_termios =		pl2303_set_termios,
	.read_bulk_callback =	pl2303_read_bulk_callback,
	.read_int_callback =	pl2303_read_int_callback,
	.write_bulk_callback =	pl2303_write_bulk_callback,
	.startup =		pl2303_startup,
	.shutdown =		pl2303_shutdown,
};

struct pl2303_private {
	spinlock_t lock;
	wait_queue_head_t delta_msr_wait;
	u8 line_control;
	u8 line_status;
	u8 termios_initialized;
};


static int pl2303_startup (struct usb_serial *serial)
{
	struct pl2303_private *priv;
	int i;

	for (i = 0; i < serial->num_ports; ++i) {
		priv = kmalloc (sizeof (struct pl2303_private), GFP_KERNEL);
		if (!priv)
			return -ENOMEM;
		memset (priv, 0x00, sizeof (struct pl2303_private));
		spin_lock_init(&priv->lock);
		init_waitqueue_head(&priv->delta_msr_wait);
		usb_set_serial_port_data(&serial->port[i], priv);
	}
	return 0;
}

static int set_control_lines (struct usb_device *dev, u8 value)
{
	int retval;
	
	retval = usb_control_msg (dev, usb_sndctrlpipe (dev, 0),
				  SET_CONTROL_REQUEST, SET_CONTROL_REQUEST_TYPE,
				  value, 0, NULL, 0, 100);
	dbg("%s - value = %d, retval = %d", __FUNCTION__, value, retval);
	return retval;
}

static int pl2303_write (struct usb_serial_port *port, int from_user,  const unsigned char *buf, int count)
{
	int result;

	dbg("%s - port %d, %d bytes", __FUNCTION__, port->number, count);

	if (port->write_urb->status == -EINPROGRESS) {
		dbg("%s - already writing", __FUNCTION__);
		return 0;
	}

	count = (count > port->bulk_out_size) ? port->bulk_out_size : count;
	if (from_user) {
		if (copy_from_user (port->write_urb->transfer_buffer, buf, count))
			return -EFAULT;
	} else {
		memcpy (port->write_urb->transfer_buffer, buf, count);
	}
	
	usb_serial_debug_data (__FILE__, __FUNCTION__, count, port->write_urb->transfer_buffer);

	port->write_urb->transfer_buffer_length = count;
	port->write_urb->dev = port->serial->dev;
	result = usb_submit_urb (port->write_urb);
	if (result)
		err("%s - failed submitting write urb, error %d", __FUNCTION__, result);
	else
		result = count;

	return result;
}



static void pl2303_set_termios (struct usb_serial_port *port, struct termios *old_termios)
{
	struct usb_serial *serial = port->serial;
	struct pl2303_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;
	unsigned int cflag;
	unsigned char *buf;
	int baud;
	int i;
	u8 control;

	dbg("%s -  port %d, initialized = %d", __FUNCTION__, port->number, 
	     priv->termios_initialized);

	if ((!port->tty) || (!port->tty->termios)) {
		dbg("%s - no tty structures", __FUNCTION__);
		return;
	}

	spin_lock_irqsave(&priv->lock, flags);
	if (!priv->termios_initialized) {
		*(port->tty->termios) = tty_std_termios;
		port->tty->termios->c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
		priv->termios_initialized = 1;
	}
	spin_unlock_irqrestore(&priv->lock, flags);

	cflag = port->tty->termios->c_cflag;
	/* check that they really want us to change something */
	if (old_termios) {
		if ((cflag == old_termios->c_cflag) &&
		    (RELEVANT_IFLAG(port->tty->termios->c_iflag) == RELEVANT_IFLAG(old_termios->c_iflag))) {
		    dbg("%s - nothing to change...", __FUNCTION__);
		    return;
		}
	}

	buf = kmalloc (7, GFP_KERNEL);
	if (!buf) {
		err("%s - out of memory.", __FUNCTION__);
		return;
	}
	memset (buf, 0x00, 0x07);
	
	i = usb_control_msg (serial->dev, usb_rcvctrlpipe (serial->dev, 0),
			     GET_LINE_REQUEST, GET_LINE_REQUEST_TYPE,
			     0, 0, buf, 7, 100);
	dbg ("0xa1:0x21:0:0  %d - %x %x %x %x %x %x %x", i,
	     buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);


	if (cflag & CSIZE) {
		switch (cflag & CSIZE) {
			case CS5:	buf[6] = 5;	break;
			case CS6:	buf[6] = 6;	break;
			case CS7:	buf[6] = 7;	break;
			default:
			case CS8:	buf[6] = 8;	break;
		}
		dbg("%s - data bits = %d", __FUNCTION__, buf[6]);
	}

	baud = 0;
	switch (cflag & CBAUD) {
		case B0:	baud = 0;	break;
		case B75:	baud = 75;	break;
		case B150:	baud = 150;	break;
		case B300:	baud = 300;	break;
		case B600:	baud = 600;	break;
		case B1200:	baud = 1200;	break;
		case B1800:	baud = 1800;	break;
		case B2400:	baud = 2400;	break;
		case B4800:	baud = 4800;	break;
		case B9600:	baud = 9600;	break;
		case B19200:	baud = 19200;	break;
		case B38400:	baud = 38400;	break;
		case B57600:	baud = 57600;	break;
		case B115200:	baud = 115200;	break;
		case B230400:	baud = 230400;	break;
		case B460800:	baud = 460800;	break;
		default:
			err ("pl2303 driver does not support the baudrate requested (fix it)");
			break;
	}
	dbg("%s - baud = %d", __FUNCTION__, baud);
	if (baud) {
		buf[0] = baud & 0xff;
		buf[1] = (baud >> 8) & 0xff;
		buf[2] = (baud >> 16) & 0xff;
		buf[3] = (baud >> 24) & 0xff;
	}

	/* For reference buf[4]=0 is 1 stop bits */
	/* For reference buf[4]=1 is 1.5 stop bits */
	/* For reference buf[4]=2 is 2 stop bits */
	if (cflag & CSTOPB) {
		buf[4] = 2;
		dbg("%s - stop bits = 2", __FUNCTION__);
	} else {
		buf[4] = 0;
		dbg("%s - stop bits = 1", __FUNCTION__);
	}

	if (cflag & PARENB) {
		/* For reference buf[5]=0 is none parity */
		/* For reference buf[5]=1 is odd parity */
		/* For reference buf[5]=2 is even parity */
		/* For reference buf[5]=3 is mark parity */
		/* For reference buf[5]=4 is space parity */
		if (cflag & PARODD) {
			buf[5] = 1;
			dbg("%s - parity = odd", __FUNCTION__);
		} else {
			buf[5] = 2;
			dbg("%s - parity = even", __FUNCTION__);
		}
	} else {
		buf[5] = 0;
		dbg("%s - parity = none", __FUNCTION__);
	}

	i = usb_control_msg (serial->dev, usb_sndctrlpipe (serial->dev, 0),
			     SET_LINE_REQUEST, SET_LINE_REQUEST_TYPE, 
			     0, 0, buf, 7, 100);
	dbg ("0x21:0x20:0:0  %d", i);

	/* change control lines if we are switching to or from B0 */
	spin_lock_irqsave(&priv->lock, flags);
	control = priv->line_control;
	if ((cflag & CBAUD) == B0)
		priv->line_control &= ~(CONTROL_DTR | CONTROL_RTS);
	else
		priv->line_control |= (CONTROL_DTR | CONTROL_RTS);
	if (control != priv->line_control) {
		control = priv->line_control;
		spin_unlock_irqrestore(&priv->lock, flags);
		set_control_lines(serial->dev, control);
	} else {
		spin_unlock_irqrestore(&priv->lock, flags);
	}
	
	buf[0] = buf[1] = buf[2] = buf[3] = buf[4] = buf[5] = buf[6] = 0;

	i = usb_control_msg (serial->dev, usb_rcvctrlpipe (serial->dev, 0),
			     GET_LINE_REQUEST, GET_LINE_REQUEST_TYPE,
			     0, 0, buf, 7, 100);
	dbg ("0xa1:0x21:0:0  %d - %x %x %x %x %x %x %x", i,
	     buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

	if (cflag & CRTSCTS) {
		i = usb_control_msg (serial->dev, usb_sndctrlpipe (serial->dev, 0),
				     VENDOR_WRITE_REQUEST, VENDOR_WRITE_REQUEST_TYPE,
				     0x0, 0x41, NULL, 0, 100);
		dbg ("0x40:0x1:0x0:0x41  %d", i);
	}

	kfree (buf);
}       


static int pl2303_open (struct usb_serial_port *port, struct file *filp)
{
	struct termios tmp_termios;
	struct usb_serial *serial = port->serial;
	unsigned char *buf;
	int result;

	if (port_paranoia_check (port, __FUNCTION__))
		return -ENODEV;
		
	dbg("%s -  port %d", __FUNCTION__, port->number);

	usb_clear_halt(serial->dev, port->write_urb->pipe);
	usb_clear_halt(serial->dev, port->read_urb->pipe);

	buf = kmalloc(10, GFP_KERNEL);
	if (buf==NULL)
		return -ENOMEM;

#define FISH(a,b,c,d)								\
	result=usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev,0),	\
			       b, a, c, d, buf, 1, 100);			\
	dbg("0x%x:0x%x:0x%x:0x%x  %d - %x",a,b,c,d,result,buf[0]);

#define SOUP(a,b,c,d)								\
	result=usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev,0),	\
			       b, a, c, d, NULL, 0, 100);			\
	dbg("0x%x:0x%x:0x%x:0x%x  %d",a,b,c,d,result);

	FISH (VENDOR_READ_REQUEST_TYPE, VENDOR_READ_REQUEST, 0x8484, 0);
	SOUP (VENDOR_WRITE_REQUEST_TYPE, VENDOR_WRITE_REQUEST, 0x0404, 0);
	FISH (VENDOR_READ_REQUEST_TYPE, VENDOR_READ_REQUEST, 0x8484, 0);
	FISH (VENDOR_READ_REQUEST_TYPE, VENDOR_READ_REQUEST, 0x8383, 0);
	FISH (VENDOR_READ_REQUEST_TYPE, VENDOR_READ_REQUEST, 0x8484, 0);
	SOUP (VENDOR_WRITE_REQUEST_TYPE, VENDOR_WRITE_REQUEST, 0x0404, 1);
	FISH (VENDOR_READ_REQUEST_TYPE, VENDOR_READ_REQUEST, 0x8484, 0);
	FISH (VENDOR_READ_REQUEST_TYPE, VENDOR_READ_REQUEST, 0x8383, 0);

	kfree(buf);

	/* Setup termios */
	if (port->tty) {
		pl2303_set_termios (port, &tmp_termios);
	}

	//FIXME: need to assert RTS and DTR if CRTSCTS off

	dbg("%s - submitting read urb", __FUNCTION__);
	port->read_urb->dev = serial->dev;
	result = usb_submit_urb (port->read_urb);
	if (result) {
		err("%s - failed submitting read urb, error %d", __FUNCTION__, result);
		pl2303_close (port, NULL);
		return -EPROTO;
	}

	dbg("%s - submitting interrupt urb", __FUNCTION__);
	port->interrupt_in_urb->dev = serial->dev;
	result = usb_submit_urb (port->interrupt_in_urb);
	if (result) {
		err("%s - failed submitting interrupt urb, error %d", __FUNCTION__, result);
		pl2303_close (port, NULL);
		return -EPROTO;
	}
	return 0;
}


static void pl2303_close (struct usb_serial_port *port, struct file *filp)
{
	struct usb_serial *serial;
	struct pl2303_private *priv;
	unsigned long flags;
	unsigned int c_cflag;
	int result;

	if (port_paranoia_check (port, __FUNCTION__))
		return;
	serial = get_usb_serial (port, __FUNCTION__);
	if (!serial)
		return;
	
	dbg("%s - port %d", __FUNCTION__, port->number);

	if (serial->dev) {
		if (port->tty) {
			c_cflag = port->tty->termios->c_cflag;
			if (c_cflag & HUPCL) {
				/* drop DTR and RTS */
				priv = usb_get_serial_port_data(port);
				spin_lock_irqsave(&priv->lock, flags);
				priv->line_control = 0;
				spin_unlock_irqrestore (&priv->lock, flags);
				set_control_lines (port->serial->dev, 0);
			}
		}

		/* shutdown our urbs */
		dbg("%s - shutting down urbs", __FUNCTION__);
		result = usb_unlink_urb (port->write_urb);
		if (result)
			dbg("%s - usb_unlink_urb (write_urb)"
			    " failed with reason: %d", __FUNCTION__,
			     result);

		result = usb_unlink_urb (port->read_urb);
		if (result)
			dbg("%s - usb_unlink_urb (read_urb) "
			    "failed with reason: %d", __FUNCTION__,
			     result);

		result = usb_unlink_urb (port->interrupt_in_urb);
		if (result)
			dbg("%s - usb_unlink_urb (interrupt_in_urb)"
			    " failed with reason: %d", __FUNCTION__,
			     result);
	}
}

static int set_modem_info (struct usb_serial_port *port, unsigned int cmd, unsigned int *value)
{
	struct pl2303_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;
	unsigned int arg;
	u8 control;

	if (copy_from_user(&arg, value, sizeof(int)))
		return -EFAULT;

	spin_lock_irqsave (&priv->lock, flags);
	switch (cmd) {
		case TIOCMBIS:
			if (arg & TIOCM_RTS)
				priv->line_control |= CONTROL_RTS;
			if (arg & TIOCM_DTR)
				priv->line_control |= CONTROL_DTR;
			break;

		case TIOCMBIC:
			if (arg & TIOCM_RTS)
				priv->line_control &= ~CONTROL_RTS;
			if (arg & TIOCM_DTR)
				priv->line_control &= ~CONTROL_DTR;
			break;

		case TIOCMSET:
			/* turn off RTS and DTR and then only turn
			   on what was asked to */
			priv->line_control &= ~(CONTROL_RTS | CONTROL_DTR);
			priv->line_control |= ((arg & TIOCM_RTS) ? CONTROL_RTS : 0);
			priv->line_control |= ((arg & TIOCM_DTR) ? CONTROL_DTR : 0);
			break;
	}
	control = priv->line_control;
	spin_unlock_irqrestore (&priv->lock, flags);

	return set_control_lines (port->serial->dev, control);
}

static int get_modem_info (struct usb_serial_port *port, unsigned int *value)
{
	struct pl2303_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;
	unsigned int mcr;
	unsigned int status;
	unsigned int result;

	spin_lock_irqsave (&priv->lock, flags);
	mcr = priv->line_control;
	status = priv->line_status;
	spin_unlock_irqrestore (&priv->lock, flags);

	result = ((mcr & CONTROL_DTR)		? TIOCM_DTR : 0)
		  | ((mcr & CONTROL_RTS)	? TIOCM_RTS : 0)
		  | ((status & UART_CTS)	? TIOCM_CTS : 0)
		  | ((status & UART_DSR)	? TIOCM_DSR : 0)
		  | ((status & UART_RING)	? TIOCM_RI  : 0)
		  | ((status & UART_DCD)	? TIOCM_CD  : 0);

	dbg("%s - result = %x", __FUNCTION__, result);

	if (copy_to_user(value, &result, sizeof(int)))
		return -EFAULT;
	return 0;
}

static int wait_modem_info(struct usb_serial_port *port, unsigned int arg)
{
	struct pl2303_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;
	unsigned int prevstatus;
	unsigned int status;
	unsigned int changed;

	spin_lock_irqsave (&priv->lock, flags);
	prevstatus = priv->line_status;
	spin_unlock_irqrestore (&priv->lock, flags);

	while (1) {
		interruptible_sleep_on(&priv->delta_msr_wait);
		/* see if a signal did it */
		if (signal_pending(current))
			return -ERESTARTSYS;
		
		spin_lock_irqsave (&priv->lock, flags);
		status = priv->line_status;
		spin_unlock_irqrestore (&priv->lock, flags);
		
		changed=prevstatus^status;
		
		if (((arg & TIOCM_RNG) && (changed & UART_RING)) ||
		    ((arg & TIOCM_DSR) && (changed & UART_DSR)) ||
		    ((arg & TIOCM_CD)  && (changed & UART_DCD)) ||
		    ((arg & TIOCM_CTS) && (changed & UART_CTS)) ) {
			return 0;
		}
		prevstatus = status;
	}
	/* NOTREACHED */
	return 0;
}

static int pl2303_ioctl (struct usb_serial_port *port, struct file *file, unsigned int cmd, unsigned long arg)
{
	dbg("%s (%d) cmd = 0x%04x", __FUNCTION__, port->number, cmd);

	switch (cmd) {
		
		case TIOCMGET:
			dbg("%s (%d) TIOCMGET", __FUNCTION__, port->number);
			return get_modem_info (port, (unsigned int *)arg);

		case TIOCMBIS:
		case TIOCMBIC:
		case TIOCMSET:
			dbg("%s (%d) TIOCMSET/TIOCMBIC/TIOCMSET", __FUNCTION__,  port->number);
			return set_modem_info(port, cmd, (unsigned int *) arg);

		case TIOCMIWAIT:
			dbg("%s (%d) TIOCMIWAIT", __FUNCTION__,  port->number);
			return wait_modem_info(port, arg);
		
		default:
			dbg("%s not supported = 0x%04x", __FUNCTION__, cmd);
			break;
	}

	return -ENOIOCTLCMD;
}

static void pl2303_break_ctl (struct usb_serial_port *port, int break_state)
{
	struct usb_serial *serial = port->serial;
	u16 state;
	int result;

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (break_state == 0)
		state = BREAK_OFF;
	else
		state = BREAK_ON;
	dbg("%s - turning break %s", state==BREAK_OFF ? "off" : "on", __FUNCTION__);

	result = usb_control_msg (serial->dev, usb_sndctrlpipe (serial->dev, 0),
				  BREAK_REQUEST, BREAK_REQUEST_TYPE, state, 
				  0, NULL, 0, 100);
	if (result)
		dbg("%s - error sending break = %d", __FUNCTION__, result);
}


static void pl2303_shutdown (struct usb_serial *serial)
{
	int i;

	dbg("%s", __FUNCTION__);

	for (i = 0; i < serial->num_ports; ++i) {
		kfree (usb_get_serial_port_data(&serial->port[i]));
		usb_set_serial_port_data(&serial->port[i], NULL);
	}		
}


static void pl2303_read_int_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *) urb->context;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);
	struct pl2303_private *priv = usb_get_serial_port_data(port);
	unsigned char *data = urb->transfer_buffer;
	unsigned long flags;

	dbg("%s (%d)", __FUNCTION__, port->number);

	/* ints auto restart... */

	switch (urb->status) {
	case 0:
		/* success */
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* this urb is terminated, clean up */
		dbg("%s - urb shutting down with status: %d", __FUNCTION__, urb->status);
		return;
	default:
		dbg("%s - nonzero urb status received: %d", __FUNCTION__, urb->status);
		return;
	}

	if (!serial) {
		return;
	}

	usb_serial_debug_data (__FILE__, __FUNCTION__, urb->actual_length, urb->transfer_buffer);

	if (urb->actual_length < UART_STATE)
		return;

	/* Save off the uart status for others to look at */
	spin_lock_irqsave(&priv->lock, flags);
	priv->line_status = data[UART_STATE];
	spin_unlock_irqrestore(&priv->lock, flags);
	wake_up_interruptible (&priv->delta_msr_wait);

	return;
}


static void pl2303_read_bulk_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *) urb->context;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);
	struct pl2303_private *priv = usb_get_serial_port_data(port);
	struct tty_struct *tty;
	unsigned char *data = urb->transfer_buffer;
	unsigned long flags;
	int i;
	int result;
	u8 status;
	char tty_flag;

	if (port_paranoia_check (port, __FUNCTION__))
		return;

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (!serial) {
		dbg("%s - bad serial pointer, exiting", __FUNCTION__);
		return;
	}

	if (urb->status) {
		dbg("%s - urb->status = %d", __FUNCTION__, urb->status);
		if (!port->open_count) {
			dbg("%s - port is closed, exiting.", __FUNCTION__);
			return;
		}
		if (urb->status == -EPROTO) {
			/* PL2303 mysteriously fails with -EPROTO reschedule the read */
			dbg("%s - caught -EPROTO, resubmitting the urb", __FUNCTION__);
			urb->status = 0;
			urb->dev = serial->dev;
			result = usb_submit_urb(urb);
			if (result)
				err("%s - failed resubmitting read urb, error %d", __FUNCTION__, result);
			return;
		}
		dbg("%s - unable to handle the error, exiting.", __FUNCTION__);
		return;
	}

	usb_serial_debug_data (__FILE__, __FUNCTION__, urb->actual_length, data);

	/* get tty_flag from status */
	tty_flag = TTY_NORMAL;

	spin_lock_irqsave(&priv->lock, flags);
	status = priv->line_status;
	spin_unlock_irqrestore(&priv->lock, flags);

	/* break takes precedence over parity, */
	/* which takes precedence over framing errors */
	if (status & UART_BREAK_ERROR )
		tty_flag = TTY_BREAK;
	else if (status & UART_PARITY_ERROR)
		tty_flag = TTY_PARITY;
	else if (status & UART_FRAME_ERROR)
		tty_flag = TTY_FRAME;
	dbg("%s - tty_flag = %d", __FUNCTION__, tty_flag);

	tty = port->tty;
	if (tty && urb->actual_length) {
		/* overrun is special, not associated with a char */
		if (status & UART_OVERRUN_ERROR)
			tty_insert_flip_char(tty, 0, TTY_OVERRUN);

		for (i = 0; i < urb->actual_length; ++i) {
			if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
				tty_flip_buffer_push(tty);
			}
			tty_insert_flip_char (tty, data[i], tty_flag);
		}
		tty_flip_buffer_push (tty);
	}

	/* Schedule the next read _if_ we are still open */
	if (port->open_count) {
		urb->dev = serial->dev;
		result = usb_submit_urb(urb);
		if (result)
			err("%s - failed resubmitting read urb, error %d", __FUNCTION__, result);
	}

	return;
}



static void pl2303_write_bulk_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *) urb->context;
	int result;

	if (port_paranoia_check (port, __FUNCTION__))
		return;
	
	dbg("%s - port %d", __FUNCTION__, port->number);
	
	if (urb->status) {
		/* error in the urb, so we have to resubmit it */
		if (serial_paranoia_check (port->serial, __FUNCTION__)) {
			return;
		}
		dbg("%s - Overflow in write", __FUNCTION__);
		dbg("%s - nonzero write bulk status received: %d", __FUNCTION__, urb->status);
		port->write_urb->transfer_buffer_length = 1;
		port->write_urb->dev = port->serial->dev;
		result = usb_submit_urb (port->write_urb);
		if (result)
			err("%s - failed resubmitting write urb, error %d", __FUNCTION__, result);

		return;
	}

	queue_task(&port->tqueue, &tq_immediate);
	mark_bh(IMMEDIATE_BH);

	return;
}


static int __init pl2303_init (void)
{
	int retval;
	retval = usb_serial_register(&pl2303_device);
	if (retval)
		goto failed_usb_serial_register;
	info(DRIVER_DESC " " DRIVER_VERSION);
	return 0;
failed_usb_serial_register:
	return retval;
}


static void __exit pl2303_exit (void)
{
	usb_serial_deregister (&pl2303_device);
}


module_init(pl2303_init);
module_exit(pl2303_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug enabled or not");

