/*
 * USB ConnectTech WhiteHEAT driver
 *
 *	Copyright (C) 1999 - 2001
 *	    Greg Kroah-Hartman (greg@kroah.com)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 * See Documentation/usb/usb-serial.txt for more information on using this driver
 *
 * (05/30/2001) gkh
 *	switched from using spinlock to a semaphore, which fixes lots of problems.
 *
 * (04/08/2001) gb
 *	Identify version on module load.
 * 
 * 2001_Mar_19 gkh
 *	Fixed MOD_INC and MOD_DEC logic, the ability to open a port more 
 *	than once, and the got the proper usb_device_id table entries so
 *	the driver works again.
 *
 * (11/01/2000) Adam J. Richter
 *	usb_device_id table support
 * 
 * (10/05/2000) gkh
 *	Fixed bug with urb->dev not being set properly, now that the usb
 *	core needs it.
 * 
 * (10/03/2000) smd
 *	firmware is improved to guard against crap sent to device
 *	firmware now replies CMD_FAILURE on bad things
 *	read_callback fix you provided for private info struct
 *	command_finished now indicates success or fail
 *	setup_port struct now packed to avoid gcc padding
 *	firmware uses 1 based port numbering, driver now handles that
 *
 * (09/11/2000) gkh
 *	Removed DEBUG #ifdefs with call to usb_serial_debug_data
 *
 * (07/19/2000) gkh
 *	Added module_init and module_exit functions to handle the fact that this
 *	driver is a loadable module now.
 *	Fixed bug with port->minor that was found by Al Borchers
 *
 * (07/04/2000) gkh
 *	Added support for port settings. Baud rate can now be changed. Line signals
 *	are not transferred to and from the tty layer yet, but things seem to be 
 *	working well now.
 *
 * (05/04/2000) gkh
 *	First cut at open and close commands. Data can flow through the ports at
 *	default speeds now.
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
#include "whiteheat_fw.h"		/* firmware for the ConnectTech WhiteHEAT device */
#include "whiteheat.h"			/* WhiteHEAT specific commands */

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.2"
#define DRIVER_AUTHOR "Greg Kroah-Hartman <greg@kroah.com>"
#define DRIVER_DESC "USB ConnectTech WhiteHEAT driver"

#define CONNECT_TECH_VENDOR_ID		0x0710
#define CONNECT_TECH_FAKE_WHITE_HEAT_ID	0x0001
#define CONNECT_TECH_WHITE_HEAT_ID	0x8001

/*
   ID tables for whiteheat are unusual, because we want to different
   things for different versions of the device.  Eventually, this
   will be doable from a single table.  But, for now, we define two
   separate ID tables, and then a third table that combines them
   just for the purpose of exporting the autoloading information.
*/
static struct usb_device_id id_table_std [] = {
	{ USB_DEVICE(CONNECT_TECH_VENDOR_ID, CONNECT_TECH_WHITE_HEAT_ID) },
	{ }						/* Terminating entry */
};

static struct usb_device_id id_table_prerenumeration [] = {
	{ USB_DEVICE(CONNECT_TECH_VENDOR_ID, CONNECT_TECH_FAKE_WHITE_HEAT_ID) },
	{ }						/* Terminating entry */
};

static __devinitdata struct usb_device_id id_table_combined [] = {
	{ USB_DEVICE(CONNECT_TECH_VENDOR_ID, CONNECT_TECH_WHITE_HEAT_ID) },
	{ USB_DEVICE(CONNECT_TECH_VENDOR_ID, CONNECT_TECH_FAKE_WHITE_HEAT_ID) },
	{ }						/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, id_table_combined);

/* function prototypes for the Connect Tech WhiteHEAT serial converter */
static int  whiteheat_open		(struct usb_serial_port *port, struct file *filp);
static void whiteheat_close		(struct usb_serial_port *port, struct file *filp);
static int  whiteheat_ioctl		(struct usb_serial_port *port, struct file * file, unsigned int cmd, unsigned long arg);
static void whiteheat_set_termios	(struct usb_serial_port *port, struct termios * old);
static void whiteheat_throttle		(struct usb_serial_port *port);
static void whiteheat_unthrottle	(struct usb_serial_port *port);
static int  whiteheat_fake_startup	(struct usb_serial *serial);
static int  whiteheat_real_startup	(struct usb_serial *serial);
static void whiteheat_real_shutdown	(struct usb_serial *serial);

static struct usb_serial_device_type whiteheat_fake_device = {
	.owner =		THIS_MODULE,
	.name =			"Connect Tech - WhiteHEAT - (prerenumeration)",
	.id_table =		id_table_prerenumeration,
	.num_interrupt_in =	NUM_DONT_CARE,
	.num_bulk_in =		NUM_DONT_CARE,
	.num_bulk_out =		NUM_DONT_CARE,
	.num_ports =		1,
	.startup =		whiteheat_fake_startup,
};

static struct usb_serial_device_type whiteheat_device = {
	.owner =		THIS_MODULE,
	.name =			"Connect Tech - WhiteHEAT",
	.id_table =		id_table_std,
	.num_interrupt_in =	NUM_DONT_CARE,
	.num_bulk_in =		NUM_DONT_CARE,
	.num_bulk_out =		NUM_DONT_CARE,
	.num_ports =		4,
	.open =			whiteheat_open,
	.close =		whiteheat_close,
	.throttle =		whiteheat_throttle,
	.unthrottle =		whiteheat_unthrottle,
	.ioctl =		whiteheat_ioctl,
	.set_termios =		whiteheat_set_termios,
	.startup =		whiteheat_real_startup,
	.shutdown =		whiteheat_real_shutdown,
};

struct whiteheat_private {
	__u8			command_finished;
	wait_queue_head_t	wait_command;	/* for handling sleeping while waiting for a command to finish */
};


/* local function prototypes */
static inline void set_rts	(struct usb_serial_port *port, unsigned char rts);
static inline void set_dtr	(struct usb_serial_port *port, unsigned char dtr);
static inline void set_break	(struct usb_serial_port *port, unsigned char brk);



#define COMMAND_PORT		4
#define COMMAND_TIMEOUT		(2*HZ)	/* 2 second timeout for a command */

/*****************************************************************************
 * Connect Tech's White Heat specific driver functions
 *****************************************************************************/
static void command_port_write_callback (struct urb *urb)
{
	dbg("%s", __FUNCTION__);

	if (urb->status) {
		dbg ("nonzero urb status: %d", urb->status);
		return;
	}

	usb_serial_debug_data (__FILE__, __FUNCTION__, urb->actual_length, urb->transfer_buffer);

	return;
}


static void command_port_read_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);
	struct whiteheat_private *info;
	unsigned char *data = urb->transfer_buffer;
	int result;

	dbg("%s", __FUNCTION__);

	if (urb->status) {
		dbg("%s - nonzero urb status: %d", __FUNCTION__, urb->status);
		return;
	}

	if (!serial) {
		dbg("%s - bad serial pointer, exiting", __FUNCTION__);
		return;
	}
	
	usb_serial_debug_data (__FILE__, __FUNCTION__, urb->actual_length, data);

	info = (struct whiteheat_private *)port->private;
	if (!info) {
		dbg("%s - info is NULL, exiting.", __FUNCTION__);
		return;
	}

	/* right now, if the command is COMMAND_COMPLETE, just flip the bit saying the command finished */
	/* in the future we're going to have to pay attention to the actual command that completed */
	if (data[0] == WHITEHEAT_CMD_COMPLETE) {
		info->command_finished = WHITEHEAT_CMD_COMPLETE;
		wake_up_interruptible(&info->wait_command);
	}
	
	if (data[0] == WHITEHEAT_CMD_FAILURE) {
		info->command_finished = WHITEHEAT_CMD_FAILURE;
		wake_up_interruptible(&info->wait_command);
	}
	
	/* Continue trying to always read */
	FILL_BULK_URB(port->read_urb, serial->dev, 
		      usb_rcvbulkpipe(serial->dev, port->bulk_in_endpointAddress),
		      port->read_urb->transfer_buffer, port->read_urb->transfer_buffer_length,
		      command_port_read_callback, port);
	result = usb_submit_urb(port->read_urb);
	if (result)
		dbg("%s - failed resubmitting read urb, error %d", __FUNCTION__, result);
}


static int whiteheat_send_cmd (struct usb_serial *serial, __u8 command, __u8 *data, __u8 datasize)
{
	struct whiteheat_private *info;
	struct usb_serial_port *port;
	int timeout;
	__u8 *transfer_buffer;
	int retval = 0;

	dbg("%s - command %d", __FUNCTION__, command);

	port = &serial->port[COMMAND_PORT];
	info = (struct whiteheat_private *)port->private;
	info->command_finished = FALSE;
	
	transfer_buffer = (__u8 *)port->write_urb->transfer_buffer;
	transfer_buffer[0] = command;
	memcpy (&transfer_buffer[1], data, datasize);
	port->write_urb->transfer_buffer_length = datasize + 1;
	port->write_urb->dev = serial->dev;
	retval = usb_submit_urb (port->write_urb);
	if (retval) {
		dbg("%s - submit urb failed", __FUNCTION__);
		goto exit;
	}

	/* wait for the command to complete */
	timeout = COMMAND_TIMEOUT;
	while (timeout && (info->command_finished == FALSE)) {
		timeout = interruptible_sleep_on_timeout (&info->wait_command, timeout);
	}

	if (info->command_finished == FALSE) {
		dbg("%s - command timed out.", __FUNCTION__);
		retval = -ETIMEDOUT;
		goto exit;
	}

	if (info->command_finished == WHITEHEAT_CMD_FAILURE) {
		dbg("%s - command failed.", __FUNCTION__);
		retval = -EIO;
		goto exit;
	}

	if (info->command_finished == WHITEHEAT_CMD_COMPLETE)
		dbg("%s - command completed.", __FUNCTION__);

exit:
	return retval;
}


static int whiteheat_open (struct usb_serial_port *port, struct file *filp)
{
	struct whiteheat_min_set	open_command;
	struct usb_serial_port 		*command_port;
	struct whiteheat_private	*info;
	int				retval = 0;

	dbg("%s - port %d", __FUNCTION__, port->number);

	/* set up some stuff for our command port */
	command_port = &port->serial->port[COMMAND_PORT];
	if (command_port->private == NULL) {
		info = (struct whiteheat_private *)kmalloc (sizeof(struct whiteheat_private), GFP_KERNEL);
		if (info == NULL) {
			err("%s - out of memory", __FUNCTION__);
			retval = -ENOMEM;
			goto exit;
		}
		
		init_waitqueue_head(&info->wait_command);
		command_port->private = info;
		command_port->write_urb->complete = command_port_write_callback;
		command_port->read_urb->complete = command_port_read_callback;
		command_port->read_urb->dev = port->serial->dev;
		command_port->tty = port->tty;		/* need this to "fake" our our sanity check macros */
		retval = usb_submit_urb (command_port->read_urb);
		if (retval) {
			err("%s - failed submitting read urb, error %d", __FUNCTION__, retval);
			goto exit;
		}
	}
	
	/* Start reading from the device */
	port->read_urb->dev = port->serial->dev;
	retval = usb_submit_urb(port->read_urb);
	if (retval) {
		err("%s - failed submitting read urb, error %d", __FUNCTION__, retval);
		goto exit;
	}

	/* send an open port command */
	/* firmware uses 1 based port numbering */
	open_command.port = port->number - port->serial->minor + 1;
	retval = whiteheat_send_cmd (port->serial, WHITEHEAT_OPEN, (__u8 *)&open_command, sizeof(open_command));
	if (retval)
		goto exit;

	/* Need to do device specific setup here (control lines, baud rate, etc.) */
	/* FIXME!!! */

exit:
	dbg("%s - exit, retval = %d", __FUNCTION__, retval);
	return retval;
}


static void whiteheat_close(struct usb_serial_port *port, struct file * filp)
{
	struct whiteheat_min_set	close_command;
	
	dbg("%s - port %d", __FUNCTION__, port->number);
	
	/* send a close command to the port */
	/* firmware uses 1 based port numbering */
	close_command.port = port->number - port->serial->minor + 1;
	whiteheat_send_cmd (port->serial, WHITEHEAT_CLOSE, (__u8 *)&close_command, sizeof(close_command));

	/* Need to change the control lines here */
	/* FIXME */
	
	/* shutdown our bulk reads and writes */
	usb_unlink_urb (port->write_urb);
	usb_unlink_urb (port->read_urb);
}


static int whiteheat_ioctl (struct usb_serial_port *port, struct file * file, unsigned int cmd, unsigned long arg)
{
	dbg("%s - port %d, cmd 0x%.4x", __FUNCTION__, port->number, cmd);

	return -ENOIOCTLCMD;
}


static void whiteheat_set_termios (struct usb_serial_port *port, struct termios *old_termios)
{
	unsigned int cflag;
	struct whiteheat_port_settings port_settings;

	dbg("%s -port %d", __FUNCTION__, port->number);

	if ((!port->tty) || (!port->tty->termios)) {
		dbg("%s - no tty structures", __FUNCTION__);
		goto exit;
	}
	
	cflag = port->tty->termios->c_cflag;
	/* check that they really want us to change something */
	if (old_termios) {
		if ((cflag == old_termios->c_cflag) &&
		    (RELEVANT_IFLAG(port->tty->termios->c_iflag) == RELEVANT_IFLAG(old_termios->c_iflag))) {
			dbg("%s - nothing to change...", __FUNCTION__);
			goto exit;
		}
	}

	/* set the port number */
	/* firmware uses 1 based port numbering */
	port_settings.port = port->number + 1;

	/* get the byte size */
	switch (cflag & CSIZE) {
		case CS5:	port_settings.bits = 5;   break;
		case CS6:	port_settings.bits = 6;   break;
		case CS7:	port_settings.bits = 7;   break;
		default:
		case CS8:	port_settings.bits = 8;   break;
	}
	dbg("%s - data bits = %d", __FUNCTION__, port_settings.bits);
	
	/* determine the parity */
	if (cflag & PARENB)
		if (cflag & PARODD)
			port_settings.parity = 'o';
		else
			port_settings.parity = 'e';
	else
		port_settings.parity = 'n';
	dbg("%s - parity = %c", __FUNCTION__, port_settings.parity);

	/* figure out the stop bits requested */
	if (cflag & CSTOPB)
		port_settings.stop = 2;
	else
		port_settings.stop = 1;
	dbg("%s - stop bits = %d", __FUNCTION__, port_settings.stop);

	
	/* figure out the flow control settings */
	if (cflag & CRTSCTS)
		port_settings.hflow = (WHITEHEAT_CTS_FLOW | WHITEHEAT_RTS_FLOW);
	else
		port_settings.hflow = 0;
	dbg("%s - hardware flow control = %s %s %s %s", __FUNCTION__,
	    (port_settings.hflow & WHITEHEAT_CTS_FLOW) ? "CTS" : "",
	    (port_settings.hflow & WHITEHEAT_RTS_FLOW) ? "RTS" : "",
	    (port_settings.hflow & WHITEHEAT_DSR_FLOW) ? "DSR" : "",
	    (port_settings.hflow & WHITEHEAT_DTR_FLOW) ? "DTR" : "");
	
	/* determine software flow control */
	if (I_IXOFF(port->tty))
		port_settings.sflow = 'b';
	else
		port_settings.sflow = 'n';
	dbg("%s - software flow control = %c", __FUNCTION__, port_settings.sflow);
	
	port_settings.xon = START_CHAR(port->tty);
	port_settings.xoff = STOP_CHAR(port->tty);
	dbg("%s - XON = %2x, XOFF = %2x", __FUNCTION__, port_settings.xon, port_settings.xoff);

	/* get the baud rate wanted */
	port_settings.baud = tty_get_baud_rate(port->tty);
	dbg("%s - baud rate = %d", __FUNCTION__, port_settings.baud);

	/* handle any settings that aren't specified in the tty structure */
	port_settings.lloop = 0;
	
	/* now send the message to the device */
	whiteheat_send_cmd (port->serial, WHITEHEAT_SETUP_PORT, (__u8 *)&port_settings, sizeof(port_settings));
	
exit:
	return;
}


static void whiteheat_throttle (struct usb_serial_port *port)
{
	dbg("%s - port %d", __FUNCTION__, port->number);

	/* Change the control signals */
	/* FIXME!!! */

	return;
}


static void whiteheat_unthrottle (struct usb_serial_port *port)
{
	dbg("%s - port %d", __FUNCTION__, port->number);

	/* Change the control signals */
	/* FIXME!!! */

	return;
}


/* steps to download the firmware to the WhiteHEAT device:
 - hold the reset (by writing to the reset bit of the CPUCS register)
 - download the VEND_AX.HEX file to the chip using VENDOR_REQUEST-ANCHOR_LOAD
 - release the reset (by writing to the CPUCS register)
 - download the WH.HEX file for all addresses greater than 0x1b3f using
   VENDOR_REQUEST-ANCHOR_EXTERNAL_RAM_LOAD
 - hold the reset
 - download the WH.HEX file for all addresses less than 0x1b40 using
   VENDOR_REQUEST_ANCHOR_LOAD
 - release the reset
 - device renumerated itself and comes up as new device id with all
   firmware download completed.
*/
static int whiteheat_fake_startup (struct usb_serial *serial)
{
	int response;
	const struct whiteheat_hex_record *record;
	
	dbg("%s", __FUNCTION__);
	
	response = ezusb_set_reset (serial, 1);

	record = &whiteheat_loader[0];
	while (record->address != 0xffff) {
		response = ezusb_writememory (serial, record->address, 
				(unsigned char *)record->data, record->data_size, 0xa0);
		if (response < 0) {
			err("%s - ezusb_writememory failed for loader (%d %04X %p %d)",
				__FUNCTION__, response, record->address, record->data, record->data_size);
			break;
		}
		++record;
	}

	response = ezusb_set_reset (serial, 0);

	record = &whiteheat_firmware[0];
	while (record->address < 0x1b40) {
		++record;
	}
	while (record->address != 0xffff) {
		response = ezusb_writememory (serial, record->address, 
				(unsigned char *)record->data, record->data_size, 0xa3);
		if (response < 0) {
			err("%s - ezusb_writememory failed for first firmware step (%d %04X %p %d)", 
				__FUNCTION__, response, record->address, record->data, record->data_size);
			break;
		}
		++record;
	}
	
	response = ezusb_set_reset (serial, 1);

	record = &whiteheat_firmware[0];
	while (record->address < 0x1b40) {
		response = ezusb_writememory (serial, record->address, 
				(unsigned char *)record->data, record->data_size, 0xa0);
		if (response < 0) {
			err("%s - ezusb_writememory failed for second firmware step (%d %04X %p %d)", 
				__FUNCTION__, response, record->address, record->data, record->data_size);
			break;
		}
		++record;
	}

	response = ezusb_set_reset (serial, 0);

	/* we want this device to fail to have a driver assigned to it. */
	return 1;
}


static int  whiteheat_real_startup (struct usb_serial *serial)
{
	struct whiteheat_hw_info *hw_info;
	int pipe;
	int ret;
	int alen;
	__u8 command[2] = { WHITEHEAT_GET_HW_INFO, 0 };
	__u8 result[sizeof(*hw_info) + 1];

	pipe = usb_rcvbulkpipe (serial->dev, 7);
	usb_bulk_msg (serial->dev, pipe, result, sizeof(result), &alen, 2 * HZ);
	/*
	 * We ignore the return code. In the case where rmmod/insmod is
	 * performed with a WhiteHEAT connected, the above times out
	 * because the endpoint is already prepped, meaning the below succeeds
	 * regardless. All other cases the above succeeds.
	 */

	pipe = usb_sndbulkpipe (serial->dev, 7);
	ret = usb_bulk_msg (serial->dev, pipe, command, sizeof(command), &alen, 2 * HZ);
	if (ret) {
		err("%s: Couldn't send command [%d]", serial->type->name, ret);
		goto error_out;
	} else if (alen != sizeof(command)) {
		err("%s: Send command incomplete [%d]", serial->type->name, alen);
		goto error_out;
	}

	pipe = usb_rcvbulkpipe (serial->dev, 7);
	ret = usb_bulk_msg (serial->dev, pipe, result, sizeof(result), &alen, 2 * HZ);
	if (ret) {
		err("%s: Couldn't get results [%d]", serial->type->name, ret);
		goto error_out;
	} else if (alen != sizeof(result)) {
		err("%s: Get results incomplete [%d]", serial->type->name, alen);
		goto error_out;
	} else if (result[0] != command[0]) {
		err("%s: Command failed [%d]", serial->type->name, result[0]);
		goto error_out;
	}

	hw_info = (struct whiteheat_hw_info *)&result[1];

	info("%s: Driver %s: Firmware v%d.%02d", serial->type->name,
	     DRIVER_VERSION, hw_info->sw_major_rev, hw_info->sw_minor_rev);

	return 0;

error_out:
	err("%s: Unable to retrieve firmware version, try replugging\n", serial->type->name);
	/*
	 * Return that we've claimed the interface. A failure here may be
	 * due to interception by the command_callback routine or other
	 * causes that don't mean that the firmware isn't running. This may
	 * change in the future. Probably should actually.
	 */
	return 0;
}

static void whiteheat_real_shutdown (struct usb_serial *serial)
{
	struct usb_serial_port *command_port;

	dbg("%s", __FUNCTION__);

	/* free up our private data for our command port */
	command_port = &serial->port[COMMAND_PORT];
	if (command_port->private != NULL) {
		kfree (command_port->private);
		command_port->private = NULL;
	}

	return;
}


static void set_command (struct usb_serial_port *port, unsigned char state, unsigned char command)
{
	struct whiteheat_rdb_set rdb_command;
	
	/* send a set rts command to the port */
	/* firmware uses 1 based port numbering */
	rdb_command.port = port->number - port->serial->minor + 1;
	rdb_command.state = state;

	whiteheat_send_cmd (port->serial, command, (__u8 *)&rdb_command, sizeof(rdb_command));
}


static inline void set_rts (struct usb_serial_port *port, unsigned char rts)
{
	set_command (port, rts, WHITEHEAT_SET_RTS);
}


static inline void set_dtr (struct usb_serial_port *port, unsigned char dtr)
{
	set_command (port, dtr, WHITEHEAT_SET_DTR);
}


static inline void set_break (struct usb_serial_port *port, unsigned char brk)
{
	set_command (port, brk, WHITEHEAT_SET_BREAK);
}


static int __init whiteheat_init (void)
{
	usb_serial_register (&whiteheat_fake_device);
	usb_serial_register (&whiteheat_device);
	info(DRIVER_DESC " " DRIVER_VERSION);
	return 0;
}


static void __exit whiteheat_exit (void)
{
	usb_serial_deregister (&whiteheat_fake_device);
	usb_serial_deregister (&whiteheat_device);
}


module_init(whiteheat_init);
module_exit(whiteheat_exit);

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug enabled or not");

