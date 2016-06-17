/*
 * KLSI KL5KUSB105 chip RS232 converter driver
 *
 *   Copyright (C) 2001 Utz-Uwe Haus <haus@uuhaus.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 * All information about the device was acquired using SniffUSB ans snoopUSB
 * on Windows98.
 * It was written out of frustration with the PalmConnect USB Serial adapter
 * sold by Palm Inc.
 * Neither Palm, nor their contractor (MCCI) or their supplier (KLSI) provided
 * information that was not already available.
 *
 * It seems that KLSI bought some silicon-design information from ScanLogic, 
 * whose SL11R processor is at the core of the KL5KUSB chipset from KLSI.
 * KLSI has firmware available for their devices; it is probable that the
 * firmware differs from that used by KLSI in their products. If you have an
 * original KLSI device and can provide some information on it, I would be 
 * most interested in adding support for it here. If you have any information 
 * on the protocol used (or find errors in my reverse-engineered stuff), please
 * let me know.
 *
 * The code was only tested with a PalmConnect USB adapter; if you
 * are adventurous, try it with any KLSI-based device and let me know how it
 * breaks so that I can fix it!
 */

/* TODO:
 *	check modem line signals
 *	implement handshaking or decide that we do not support it
 */

/* History:
 *   0.3a - implemented pools of write URBs
 *   0.3  - alpha version for public testing
 *   0.2  - TIOCMGET works, so autopilot(1) can be used!
 *   0.1  - can be used to to pilot-xfer -p /dev/ttyUSB0 -l
 *
 *   The driver skeleton is mainly based on mct_u232.c and various other 
 *   pieces of code shamelessly copied from the drivers/usb/serial/ directory.
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
#include <asm/uaccess.h>
#include <linux/usb.h>

#ifdef CONFIG_USB_SERIAL_DEBUG
 	static int debug = 1;
#else
 	static int debug;
#endif

#include "usb-serial.h"
#include "kl5kusb105.h"


/*
 * Version Information
 */
#define DRIVER_VERSION "v0.3a"
#define DRIVER_AUTHOR "Utz-Uwe Haus <haus@uuhaus.de>"
#define DRIVER_DESC "KLSI KL5KUSB105 chipset USB->Serial Converter driver"


/*
 * Function prototypes
 */
static int  klsi_105_startup	         (struct usb_serial *serial);
static void klsi_105_shutdown	         (struct usb_serial *serial);
static int  klsi_105_open	         (struct usb_serial_port *port,
					  struct file *filp);
static void klsi_105_close	         (struct usb_serial_port *port,
					  struct file *filp);
static int  klsi_105_write	         (struct usb_serial_port *port,
					  int from_user,
					  const unsigned char *buf,
					  int count);
static void klsi_105_write_bulk_callback (struct urb *urb);
static int  klsi_105_chars_in_buffer     (struct usb_serial_port *port);
static int  klsi_105_write_room          (struct usb_serial_port *port);

static void klsi_105_read_bulk_callback  (struct urb *urb);
static void klsi_105_set_termios         (struct usb_serial_port *port,
					  struct termios * old);
static int  klsi_105_ioctl	         (struct usb_serial_port *port,
					  struct file * file,
					  unsigned int cmd,
					  unsigned long arg);
static void klsi_105_throttle		 (struct usb_serial_port *port);
static void klsi_105_unthrottle		 (struct usb_serial_port *port);
/*
static void klsi_105_break_ctl	         (struct usb_serial_port *port,
					  int break_state );
 */

/*
 * All of the device info needed for the KLSI converters.
 */
static struct usb_device_id id_table [] = {
	{ USB_DEVICE(PALMCONNECT_VID, PALMCONNECT_PID) },
	{ USB_DEVICE(KLSI_VID, KLSI_KL5KUSB105D_PID) },
	{ }		/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, id_table);


static struct usb_serial_device_type kl5kusb105d_device = {
	.owner =             THIS_MODULE,
	.name =		     "KL5KUSB105D / PalmConnect",
	.id_table =	     id_table,
	.num_interrupt_in =  1,
	.num_bulk_in =	     1,
	.num_bulk_out =	     1,
	.num_ports =	     1,
	.open =		     klsi_105_open,
	.close =	     klsi_105_close,
	.write =	     klsi_105_write,
	.write_bulk_callback = klsi_105_write_bulk_callback,
	.chars_in_buffer =   klsi_105_chars_in_buffer,
	.write_room =        klsi_105_write_room,
	.read_bulk_callback =klsi_105_read_bulk_callback,
	.ioctl =	     klsi_105_ioctl,
	.set_termios =	     klsi_105_set_termios,
	/*.break_ctl =	     klsi_105_break_ctl,*/
	.startup =	     klsi_105_startup,
	.shutdown =	     klsi_105_shutdown,
	.throttle =	     klsi_105_throttle,
	.unthrottle =	     klsi_105_unthrottle,
};

struct klsi_105_port_settings {
	__u8	pktlen;		/* always 5, it seems */
	__u8	baudrate;
	__u8	databits;
	__u8	unknown1;
	__u8	unknown2;
} __attribute__ ((packed));

/* we implement a pool of NUM_URBS urbs per usb_serial */
#define NUM_URBS			1
#define URB_TRANSFER_BUFFER_SIZE	64
struct klsi_105_private {
	struct klsi_105_port_settings	cfg;
	struct termios			termios;
	unsigned long			line_state; /* modem line settings */
	/* write pool */
	struct urb *			write_urb_pool[NUM_URBS];
	spinlock_t			write_urb_pool_lock;
	unsigned long			bytes_in;
	unsigned long			bytes_out;
};


/*
 * Handle vendor specific USB requests
 */


#define KLSI_TIMEOUT	 (HZ * 5 ) /* default urb timeout */

static int klsi_105_chg_port_settings(struct usb_serial *serial,
				      struct klsi_105_port_settings *settings)
{
	int rc;

        rc = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			     KL5KUSB105A_SIO_SET_DATA,
                             USB_TYPE_VENDOR | USB_DIR_OUT | USB_RECIP_INTERFACE,
			     0, /* value */
			     0, /* index */
			     settings,
			     sizeof(struct klsi_105_port_settings),
			     KLSI_TIMEOUT);
	if (rc < 0)
		err("Change port settings failed (error = %d)", rc);
	info("%s - %d byte block, baudrate %x, databits %d, u1 %d, u2 %d",
	    __FUNCTION__,
	    settings->pktlen,
	    settings->baudrate, settings->databits,
	    settings->unknown1, settings->unknown2);
        return rc;
} /* klsi_105_chg_port_settings */

/* translate a 16-bit status value from the device to linux's TIO bits */
static unsigned long klsi_105_status2linestate(const __u16 status)
{
	unsigned long res = 0;

	res =   ((status & KL5KUSB105A_DSR) ? TIOCM_DSR : 0)
	      | ((status & KL5KUSB105A_CTS) ? TIOCM_CTS : 0)
	      ;

	return res;
}
/* 
 * Read line control via vendor command and return result through
 * *line_state_p 
 */
/* It seems that the status buffer has always only 2 bytes length */
#define KLSI_STATUSBUF_LEN	2
static int klsi_105_get_line_state(struct usb_serial *serial,
				   unsigned long *line_state_p)
{
	int rc;
	__u8 status_buf[KLSI_STATUSBUF_LEN] = { -1,-1};
	__u16 status;

	info("%s - sending SIO Poll request", __FUNCTION__);
        rc = usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev, 0),
			     KL5KUSB105A_SIO_POLL,
                             USB_TYPE_VENDOR | USB_DIR_IN,
			     0, /* value */
			     0, /* index */
			     status_buf, KLSI_STATUSBUF_LEN,
			     10*HZ
			     );
	if (rc < 0)
		err("Reading line status failed (error = %d)", rc);
	else {
		status = status_buf[0] + (status_buf[1]<<8);

		info("%s - read status %x %x", __FUNCTION__,
		     status_buf[0], status_buf[1]);

		*line_state_p = klsi_105_status2linestate(status);
	}

        return rc;
}


/*
 * Driver's tty interface functions
 */

static int klsi_105_startup (struct usb_serial *serial)
{
	struct klsi_105_private *priv;
	int i;

	/* check if we support the product id (see keyspan.c)
	 * FIXME
	 */

	/* allocate the private data structure */
	for (i=0; i<serial->num_ports; i++) {
		serial->port[i].private = kmalloc(sizeof(struct klsi_105_private),
						   GFP_KERNEL);
		if (!serial->port[i].private) {
			dbg("%skmalloc for klsi_105_private failed.", __FUNCTION__);
			return (-1); /* error */
		}
		priv = (struct klsi_105_private *)serial->port[i].private;
		/* set initial values for control structures */
		priv->cfg.pktlen    = 5;
		priv->cfg.baudrate  = kl5kusb105a_sio_b9600;
		priv->cfg.databits  = kl5kusb105a_dtb_8;
		priv->cfg.unknown1  = 0;
		priv->cfg.unknown2  = 1;

		priv->line_state    = 0;

		priv->bytes_in	    = 0;
		priv->bytes_out	    = 0;

		spin_lock_init (&priv->write_urb_pool_lock);
		for (i=0; i<NUM_URBS; i++) {
			struct urb* urb = usb_alloc_urb(0);

			priv->write_urb_pool[i] = urb;
			if (urb == NULL) {
				err("No more urbs???");
				continue;
			}

			urb->transfer_buffer = NULL;
			urb->transfer_buffer = kmalloc (URB_TRANSFER_BUFFER_SIZE,
							GFP_KERNEL);
			if (!urb->transfer_buffer) {
				err("%s - out of memory for urb buffers.", __FUNCTION__);
				continue;
			}
		}

		/* priv->termios is left uninitalized until port opening */
		init_waitqueue_head(&serial->port[i].write_wait);
	}
	
	return (0);
} /* klsi_105_startup */


static void klsi_105_shutdown (struct usb_serial *serial)
{
	int i;
	
	dbg("%s", __FUNCTION__);

	/* stop reads and writes on all ports */
	for (i=0; i < serial->num_ports; ++i) {
		struct klsi_105_private *priv = 
			(struct klsi_105_private*) serial->port[i].private;
		unsigned long flags;

		if (priv) {
			/* kill our write urb pool */
			int j;
			struct urb **write_urbs = priv->write_urb_pool;
			spin_lock_irqsave(&priv->write_urb_pool_lock,flags);

			for (j = 0; j < NUM_URBS; j++) {
				if (write_urbs[j]) {
					/* FIXME - uncomment the following
					 * usb_unlink_urb call when the host
					 * controllers get fixed to set
					 * urb->dev = NULL after the urb is
					 * finished.  Otherwise this call
					 * oopses. */
					/* usb_unlink_urb(write_urbs[j]); */
					if (write_urbs[j]->transfer_buffer)
						    kfree(write_urbs[j]->transfer_buffer);
					usb_free_urb (write_urbs[j]);
				}
			}

			spin_unlock_irqrestore (&priv->write_urb_pool_lock,
					       	flags);

			kfree(serial->port[i].private);
		}
	}
} /* klsi_105_shutdown */

static int  klsi_105_open (struct usb_serial_port *port, struct file *filp)
{
	struct usb_serial *serial = port->serial;
	struct klsi_105_private *priv = (struct klsi_105_private *)port->private;
	int retval = 0;
	int rc;
	int i;
	unsigned long line_state;

	dbg("%s port %d", __FUNCTION__, port->number);

	/* force low_latency on so that our tty_push actually forces
	 * the data through
	 * port->tty->low_latency = 1; */

	/* Do a defined restart:
	 * Set up sane default baud rate and send the 'READ_ON'
	 * vendor command. 
	 * FIXME: set modem line control (how?)
	 * Then read the modem line control and store values in
	 * priv->line_state.
	 */
	priv->cfg.pktlen   = 5;
	priv->cfg.baudrate = kl5kusb105a_sio_b9600;
	priv->cfg.databits = kl5kusb105a_dtb_8;
	priv->cfg.unknown1 = 0;
	priv->cfg.unknown2 = 1;
	klsi_105_chg_port_settings(serial, &(priv->cfg));
	
	/* set up termios structure */
	priv->termios.c_iflag = port->tty->termios->c_iflag;
	priv->termios.c_oflag = port->tty->termios->c_oflag;
	priv->termios.c_cflag = port->tty->termios->c_cflag;
	priv->termios.c_lflag = port->tty->termios->c_lflag;
	for (i=0; i<NCCS; i++)
		priv->termios.c_cc[i] = port->tty->termios->c_cc[i];


	/* READ_ON and urb submission */
	FILL_BULK_URB(port->read_urb, serial->dev, 
		      usb_rcvbulkpipe(serial->dev,
				      port->bulk_in_endpointAddress),
		      port->read_urb->transfer_buffer,
		      port->read_urb->transfer_buffer_length,
		      klsi_105_read_bulk_callback,
		      port);
	port->read_urb->transfer_flags |= USB_QUEUE_BULK;

	rc = usb_submit_urb(port->read_urb);
	if (rc) {
		err("%s - failed submitting read urb, error %d", __FUNCTION__, rc);
		retval = rc;
		goto exit;
	}

	rc = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev,0),
			     KL5KUSB105A_SIO_CONFIGURE,
			     USB_TYPE_VENDOR|USB_DIR_OUT|USB_RECIP_INTERFACE,
			     KL5KUSB105A_SIO_CONFIGURE_READ_ON,
			     0, /* index */
			     NULL,
			     0,
			     KLSI_TIMEOUT);
	if (rc < 0) {
		err("Enabling read failed (error = %d)", rc);
		retval = rc;
	} else 
		dbg("%s - enabled reading", __FUNCTION__);

	rc = klsi_105_get_line_state(serial, &line_state);
	if (rc >= 0) {
		priv->line_state = line_state;
		dbg("%s - read line state 0x%lx", __FUNCTION__, line_state);
		retval = 0;
	} else
		retval = rc;

exit:
	return retval;
} /* klsi_105_open */


static void klsi_105_close (struct usb_serial_port *port, struct file *filp)
{
	struct usb_serial *serial;
	struct klsi_105_private *priv 
		= (struct klsi_105_private *)port->private;
	int rc;

	dbg("%s port %d", __FUNCTION__, port->number);

	serial = get_usb_serial (port, __FUNCTION__);

	if(!serial)
		return;

	/* send READ_OFF */
	rc = usb_control_msg (serial->dev,
			      usb_sndctrlpipe(serial->dev, 0),
			      KL5KUSB105A_SIO_CONFIGURE,
			      USB_TYPE_VENDOR | USB_DIR_OUT,
			      KL5KUSB105A_SIO_CONFIGURE_READ_OFF,
			      0, /* index */
			      NULL, 0,
			      KLSI_TIMEOUT);
	if (rc < 0)
		    err("Disabling read failed (error = %d)", rc);

	/* shutdown our bulk reads and writes */
	usb_unlink_urb (port->write_urb);
	usb_unlink_urb (port->read_urb);
	/* unlink our write pool */
	/* FIXME */
	/* wgg - do I need this? I think so. */
	usb_unlink_urb (port->interrupt_in_urb);
	info("kl5kusb105 port stats: %ld bytes in, %ld bytes out", priv->bytes_in, priv->bytes_out);
} /* klsi_105_close */


/* We need to write a complete 64-byte data block and encode the
 * number actually sent in the first double-byte, LSB-order. That 
 * leaves at most 62 bytes of payload.
 */
#define KLSI_105_DATA_OFFSET	2   /* in the bulk urb data block */


static int klsi_105_write (struct usb_serial_port *port, int from_user,
			   const unsigned char *buf, int count)
{
	struct usb_serial *serial = port->serial;
	struct klsi_105_private *priv = 
		(struct klsi_105_private*) port->private;
	int result, size;
	int bytes_sent=0;

	dbg("%s - port %d", __FUNCTION__, port->number);

	while (count > 0) {
		/* try to find a free urb (write 0 bytes if none) */
		struct urb *urb = NULL;
		unsigned long flags;
		int i;
		/* since the pool is per-port we might not need the spin lock !? */
		spin_lock_irqsave (&priv->write_urb_pool_lock, flags);
		for (i=0; i<NUM_URBS; i++) {
			if (priv->write_urb_pool[i]->status != -EINPROGRESS) {
				urb = priv->write_urb_pool[i];
				dbg("%s - using pool URB %d", __FUNCTION__, i);
				break;
			}
		}
		spin_unlock_irqrestore (&priv->write_urb_pool_lock, flags);

		if (urb==NULL) {
			dbg("%s - no more free urbs", __FUNCTION__);
			goto exit;
		}

		if (urb->transfer_buffer == NULL) {
			urb->transfer_buffer = kmalloc (URB_TRANSFER_BUFFER_SIZE, GFP_KERNEL);
			if (urb->transfer_buffer == NULL) {
				err("%s - no more kernel memory...", __FUNCTION__);
				goto exit;
			}
		}

		size = min (count, port->bulk_out_size - KLSI_105_DATA_OFFSET);
		size = min (size, URB_TRANSFER_BUFFER_SIZE - KLSI_105_DATA_OFFSET);

		if (from_user) {
			if (copy_from_user(urb->transfer_buffer
					   + KLSI_105_DATA_OFFSET, buf, size)) {
				return -EFAULT;
			}
		} else {
			memcpy (urb->transfer_buffer + KLSI_105_DATA_OFFSET,
			       	buf, size);
		}

		/* write payload size into transfer buffer */
		((__u8 *)urb->transfer_buffer)[0] = (__u8) (size & 0xFF);
		((__u8 *)urb->transfer_buffer)[1] = (__u8) ((size & 0xFF00)>>8);

		/* set up our urb */
		FILL_BULK_URB(urb, serial->dev,
			      usb_sndbulkpipe(serial->dev,
					      port->bulk_out_endpointAddress),
			      urb->transfer_buffer,
			      URB_TRANSFER_BUFFER_SIZE,
			      klsi_105_write_bulk_callback,
			      port);
		urb->transfer_flags |= USB_QUEUE_BULK;


		/* send the data out the bulk port */
		result = usb_submit_urb(urb);
		if (result) {
			err("%s - failed submitting write urb, error %d", __FUNCTION__, result);
			goto exit;
		}
		buf += size;
		bytes_sent += size;
		count -= size;
	}
exit:
	priv->bytes_out+=bytes_sent;

	return bytes_sent;	/* that's how much we wrote */
} /* klsi_105_write */

static void klsi_105_write_bulk_callback ( struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
	struct usb_serial *serial = port->serial;

	dbg("%s - port %d", __FUNCTION__, port->number);
	
	if (!serial) {
		dbg("%s - bad serial pointer, exiting", __FUNCTION__);
		return;
	}

	if (urb->status) {
		dbg("%s - nonzero write bulk status received: %d", __FUNCTION__,
		    urb->status);
		return;
	}

	/* from generic_write_bulk_callback */
	queue_task(&port->tqueue, &tq_immediate);
	mark_bh(IMMEDIATE_BH);

	return;
} /* klsi_105_write_bulk_completion_callback */


/* return number of characters currently in the writing process */
static int klsi_105_chars_in_buffer (struct usb_serial_port *port)
{
	int chars = 0;
	int i;
	unsigned long flags;
	struct klsi_105_private *priv = 
		(struct klsi_105_private*) port->private;

	spin_lock_irqsave (&priv->write_urb_pool_lock, flags);

	for (i = 0; i < NUM_URBS; ++i) {
		if (priv->write_urb_pool[i]->status == -EINPROGRESS) {
			chars += URB_TRANSFER_BUFFER_SIZE;
		}
	}

	spin_unlock_irqrestore (&priv->write_urb_pool_lock, flags);

	dbg("%s - returns %d", __FUNCTION__, chars);
	return (chars);
}

static int klsi_105_write_room (struct usb_serial_port *port)
{
	unsigned long flags;
	int i;
	int room = 0;
	struct klsi_105_private *priv = 
		(struct klsi_105_private*) port->private;

	spin_lock_irqsave (&priv->write_urb_pool_lock, flags);
	for (i = 0; i < NUM_URBS; ++i) {
		if (priv->write_urb_pool[i]->status != -EINPROGRESS) {
			room += URB_TRANSFER_BUFFER_SIZE;
		}
	}

	spin_unlock_irqrestore (&priv->write_urb_pool_lock, flags);

	dbg("%s - returns %d", __FUNCTION__, room);
	return (room);
}



static void klsi_105_read_bulk_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
	struct usb_serial *serial = port->serial;
	struct klsi_105_private *priv = 
		(struct klsi_105_private*) port->private;
	struct tty_struct *tty;
	unsigned char *data = urb->transfer_buffer;
	int rc;

        dbg("%s - port %d", __FUNCTION__, port->number);

	/* The urb might have been killed. */
        if (urb->status) {
                dbg("%s - nonzero read bulk status received: %d", __FUNCTION__,
		    urb->status);
                return;
        }
	if (!serial) {
		dbg("%s - bad serial pointer, exiting", __FUNCTION__);
		return;
	}
	
	/* The data received is again preceded by a length double-byte in LSB-
	 * first order (see klsi_105_write() )
	 */
	if (urb->actual_length == 0) {
		/* empty urbs seem to happen, we ignore them */
		/* dbg("%s - emtpy URB", __FUNCTION__); */
	       ;
	} else if (urb->actual_length <= 2) {
		dbg("%s - size %d URB not understood", __FUNCTION__,
		    urb->actual_length);
		usb_serial_debug_data (__FILE__, __FUNCTION__, urb->actual_length, data);
	} else {
		int i;
		int bytes_sent = ((__u8 *) data)[0] +
				 ((unsigned int) ((__u8 *) data)[1] << 8);
		tty = port->tty;
		/* we should immediately resubmit the URB, before attempting
		 * to pass the data on to the tty layer. But that needs locking
		 * against re-entry an then mixed-up data because of
		 * intermixed tty_flip_buffer_push()s
		 * FIXME
		 */ 
		usb_serial_debug_data (__FILE__, __FUNCTION__,
				       urb->actual_length, data);

		if (bytes_sent + 2 > urb->actual_length) {
			dbg("%s - trying to read more data than available"
			    " (%d vs. %d)", __FUNCTION__,
			    bytes_sent+2, urb->actual_length);
			/* cap at implied limit */
			bytes_sent = urb->actual_length - 2;
		}

		for (i = 2; i < 2+bytes_sent; i++) {
			/* if we insert more than TTY_FLIPBUF_SIZE characters,
			 * we drop them. */
			if(tty->flip.count >= TTY_FLIPBUF_SIZE) {
				tty_flip_buffer_push(tty);
			}
			/* this doesn't actually push the data through unless 
			 * tty->low_latency is set */
			tty_insert_flip_char(tty, ((__u8*) data)[i], 0);
		}
		tty_flip_buffer_push(tty);
		priv->bytes_in += bytes_sent;
	}
	/* Continue trying to always read  */
	FILL_BULK_URB(port->read_urb, serial->dev, 
		      usb_rcvbulkpipe(serial->dev,
				      port->bulk_in_endpointAddress),
		      port->read_urb->transfer_buffer,
		      port->read_urb->transfer_buffer_length,
		      klsi_105_read_bulk_callback,
		      port);
	rc = usb_submit_urb(port->read_urb);
	if (rc)
		err("%s - failed resubmitting read urb, error %d", __FUNCTION__, rc);
} /* klsi_105_read_bulk_callback */


static void klsi_105_set_termios (struct usb_serial_port *port,
				  struct termios *old_termios)
{
	struct usb_serial *serial = port->serial;
	struct klsi_105_private *priv = (struct klsi_105_private *)port->private;
	unsigned int iflag = port->tty->termios->c_iflag;
	unsigned int old_iflag = old_termios->c_iflag;
	unsigned int cflag = port->tty->termios->c_cflag;
	unsigned int old_cflag = old_termios->c_cflag;
	
	/*
	 * Update baud rate
	 */
	if( (cflag & CBAUD) != (old_cflag & CBAUD) ) {
	        /* reassert DTR and (maybe) RTS on transition from B0 */
		if( (old_cflag & CBAUD) == B0 ) {
			dbg("%s: baud was B0", __FUNCTION__);
#if 0
			priv->control_state |= TIOCM_DTR;
			/* don't set RTS if using hardware flow control */
			if (!(old_cflag & CRTSCTS)) {
				priv->control_state |= TIOCM_RTS;
			}
			mct_u232_set_modem_ctrl(serial, priv->control_state);
#endif
		}
		
		switch(cflag & CBAUD) {
		case B0: /* handled below */
			break;
		case B1200: priv->cfg.baudrate = kl5kusb105a_sio_b1200;
			break;
		case B2400: priv->cfg.baudrate = kl5kusb105a_sio_b2400;
			break;
		case B4800: priv->cfg.baudrate = kl5kusb105a_sio_b4800;
			break;
		case B9600: priv->cfg.baudrate = kl5kusb105a_sio_b9600;
			break;
		case B19200: priv->cfg.baudrate = kl5kusb105a_sio_b19200;
			break;
		case B38400: priv->cfg.baudrate = kl5kusb105a_sio_b38400;
			break;
		case B57600: priv->cfg.baudrate = kl5kusb105a_sio_b57600;
			break;
		case B115200: priv->cfg.baudrate = kl5kusb105a_sio_b115200;
			break;
		default:
			err("KLSI USB->Serial converter:"
			    " unsupported baudrate request, using default"
			    " of 9600");
			priv->cfg.baudrate = kl5kusb105a_sio_b9600;
			break;
		}
		if ((cflag & CBAUD) == B0 ) {
			dbg("%s: baud is B0", __FUNCTION__);
			/* Drop RTS and DTR */
			/* maybe this should be simulated by sending read
			 * disable and read enable messages?
			 */
			;
#if 0
			priv->control_state &= ~(TIOCM_DTR | TIOCM_RTS);
        		mct_u232_set_modem_ctrl(serial, priv->control_state);
#endif
		}
	}

	if ((cflag & CSIZE) != (old_cflag & CSIZE)) {
		/* set the number of data bits */
		switch (cflag & CSIZE) {
		case CS5:
			dbg("%s - 5 bits/byte not supported", __FUNCTION__);
			return ;
		case CS6:
			dbg("%s - 6 bits/byte not supported", __FUNCTION__);
			return ;
		case CS7:
			priv->cfg.databits = kl5kusb105a_dtb_7;
			break;
		case CS8:
			priv->cfg.databits = kl5kusb105a_dtb_8;
			break;
		default:
			err("CSIZE was not CS5-CS8, using default of 8");
			priv->cfg.databits = kl5kusb105a_dtb_8;
			break;
		}
	}

	/*
	 * Update line control register (LCR)
	 */
	if ((cflag & (PARENB|PARODD)) != (old_cflag & (PARENB|PARODD))
	    || (cflag & CSTOPB) != (old_cflag & CSTOPB) ) {
		
#if 0
		priv->last_lcr = 0;

		/* set the parity */
		if (cflag & PARENB)
			priv->last_lcr |= (cflag & PARODD) ?
				MCT_U232_PARITY_ODD : MCT_U232_PARITY_EVEN;
		else
			priv->last_lcr |= MCT_U232_PARITY_NONE;

		/* set the number of stop bits */
		priv->last_lcr |= (cflag & CSTOPB) ?
			MCT_U232_STOP_BITS_2 : MCT_U232_STOP_BITS_1;

		mct_u232_set_line_ctrl(serial, priv->last_lcr);
#endif
		;
	}
	
	/*
	 * Set flow control: well, I do not really now how to handle DTR/RTS.
	 * Just do what we have seen with SniffUSB on Win98.
	 */
	if( (iflag & IXOFF) != (old_iflag & IXOFF)
	    || (iflag & IXON) != (old_iflag & IXON)
	    ||  (cflag & CRTSCTS) != (old_cflag & CRTSCTS) ) {
		
		/* Drop DTR/RTS if no flow control otherwise assert */
#if 0
		if ((iflag & IXOFF) || (iflag & IXON) || (cflag & CRTSCTS) )
			priv->control_state |= TIOCM_DTR | TIOCM_RTS;
		else
			priv->control_state &= ~(TIOCM_DTR | TIOCM_RTS);
		mct_u232_set_modem_ctrl(serial, priv->control_state);
#endif
		;
	}

	/* now commit changes to device */
	klsi_105_chg_port_settings(serial, &(priv->cfg));
} /* klsi_105_set_termios */


#if 0
static void mct_u232_break_ctl( struct usb_serial_port *port, int break_state )
{
	struct usb_serial *serial = port->serial;
	struct mct_u232_private *priv = (struct mct_u232_private *)port->private;
	unsigned char lcr = priv->last_lcr;

	dbg("%sstate=%d", __FUNCTION__, break_state);

	if (break_state)
		lcr |= MCT_U232_SET_BREAK;

	mct_u232_set_line_ctrl(serial, lcr);
} /* mct_u232_break_ctl */
#endif

static int klsi_105_ioctl (struct usb_serial_port *port, struct file * file,
			   unsigned int cmd, unsigned long arg)
{
	struct usb_serial *serial = port->serial;
	struct klsi_105_private *priv = (struct klsi_105_private *)port->private;
	int mask;
	
	dbg("%scmd=0x%x", __FUNCTION__, cmd);

	/* Based on code from acm.c and others */
	switch (cmd) {
	case TIOCMGET: {
		int rc;
		unsigned long line_state;
		dbg("%s - TIOCMGET request, just guessing", __FUNCTION__);

		rc = klsi_105_get_line_state(serial, &line_state);
		if (rc < 0) {
			err("Reading line control failed (error = %d)", rc);
			/* better return value? EAGAIN? */
			return -ENOIOCTLCMD;
		} else {
			priv->line_state = line_state;
			dbg("%s - read line state 0x%lx", __FUNCTION__, line_state);
		}
		return put_user(priv->line_state, (unsigned long *) arg); 
	       };

	case TIOCMSET: /* Turns on and off the lines as specified by the mask */
	case TIOCMBIS: /* turns on (Sets) the lines as specified by the mask */
	case TIOCMBIC: /* turns off (Clears) the lines as specified by the mask */
		if (get_user(mask, (unsigned long *) arg))
			return -EFAULT;

		if ((cmd == TIOCMSET) || (mask & TIOCM_RTS)) {
			/* RTS needs set */
			if( ((cmd == TIOCMSET) && (mask & TIOCM_RTS)) ||
			    (cmd == TIOCMBIS) )
				dbg("%s - set RTS not handled", __FUNCTION__);
				/* priv->control_state |=  TIOCM_RTS; */
			else
				dbg("%s - clear RTS not handled", __FUNCTION__);
				/* priv->control_state &= ~TIOCM_RTS; */
		}

		if ((cmd == TIOCMSET) || (mask & TIOCM_DTR)) {
			/* DTR needs set */
			if( ((cmd == TIOCMSET) && (mask & TIOCM_DTR)) ||
			    (cmd == TIOCMBIS) )
				dbg("%s - set DTR not handled", __FUNCTION__);
			/*	priv->control_state |=  TIOCM_DTR; */
			else
				dbg("%s - clear DTR not handled", __FUNCTION__);
				/* priv->control_state &= ~TIOCM_DTR; */
		}
		/*
		mct_u232_set_modem_ctrl(serial, priv->control_state);
		*/
		break;
					
	case TIOCMIWAIT:
		/* wait for any of the 4 modem inputs (DCD,RI,DSR,CTS)*/
		/* TODO */
		dbg("%s - TIOCMIWAIT not handled", __FUNCTION__);
		return -ENOIOCTLCMD;

	case TIOCGICOUNT:
		/* return count of modemline transitions */
		/* TODO */
		dbg("%s - TIOCGICOUNT not handled", __FUNCTION__);
		return -ENOIOCTLCMD;
	case TCGETS: {
	     /* return current info to caller */
	     int retval;

	     dbg("%s - TCGETS data faked/incomplete", __FUNCTION__);

	     retval = verify_area(VERIFY_WRITE, (void *)arg,
				  sizeof(struct termios));

	     if (retval)
			 return(retval);

	     kernel_termios_to_user_termios((struct termios *)arg,  
					    &priv->termios);
	     return(0);
	     }
	case TCSETS: {
		/* set port termios to the one given by the user */
		int retval;

		dbg("%s - TCSETS not handled", __FUNCTION__);

		retval = verify_area(VERIFY_READ, (void *)arg,
				     sizeof(struct termios));

		if (retval)
			    return(retval);

		user_termios_to_kernel_termios(&priv->termios,
					       (struct termios *)arg);
		klsi_105_set_termios(port, &priv->termios);
		return(0);
	     }
	case TCSETSW: {
		/* set port termios and try to wait for completion of last
		 * write operation */
		/* We guess here. If there are not too many write urbs
		 * outstanding, we lie. */
		/* what is the right way to wait here? schedule() ? */
	        /*
		while (klsi_105_chars_in_buffer(port) > (NUM_URBS / 4 ) * URB_TRANSFER_BUFFER_SIZE)
			    schedule();
		 */
		return -ENOIOCTLCMD;
		      }
	default:
		dbg("%s: arg not supported - 0x%04x", __FUNCTION__,cmd);
		return(-ENOIOCTLCMD);
		break;
	}
	return 0;
} /* klsi_105_ioctl */

static void klsi_105_throttle (struct usb_serial_port *port)
{
	dbg("%s - port %d", __FUNCTION__, port->number);
	usb_unlink_urb (port->read_urb);
}

static void klsi_105_unthrottle (struct usb_serial_port *port)
{
	int result;

	dbg("%s - port %d", __FUNCTION__, port->number);

	port->read_urb->dev = port->serial->dev;
	result = usb_submit_urb(port->read_urb);
	if (result)
		err("%s - failed submitting read urb, error %d", __FUNCTION__,
		    result);
}



static int __init klsi_105_init (void)
{
	usb_serial_register (&kl5kusb105d_device);

	info(DRIVER_DESC " " DRIVER_VERSION);
	return 0;
}


static void __exit klsi_105_exit (void)
{
	usb_serial_deregister (&kl5kusb105d_device);
}


module_init (klsi_105_init);
module_exit (klsi_105_exit);

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL"); 


MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "enable extensive debugging messages");
/* FIXME: implement
MODULE_PARM(num_urbs, "i");
MODULE_PARM_DESC(num_urbs, "number of URBs to use in write pool");
*/

/* vim: set sts=8 ts=8 sw=8: */
