/*
 * USB Serial Converter driver
 *
 * Copyright (C) 1999 - 2002 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2000 Peter Berger (pberger@brimson.com)
 * Copyright (C) 2000 Al Borchers (borchers@steinerpoint.com)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License version
 *	2 as published by the Free Software Foundation.
 *
 * This driver was originally based on the ACM driver by Armin Fuerst (which was 
 * based on a driver by Brad Keryan)
 *
 * See Documentation/usb/usb-serial.txt for more information on using this driver
 *
 * (10/10/2001) gkh
 *	usb_serial_disconnect() now sets the serial->dev pointer is to NULL to
 *	help prevent child drivers from accessing the device since it is now
 *	gone.
 *
 * (09/13/2001) gkh
 *	Moved generic driver initialize after we have registered with the USB
 *	core.  Thanks to Randy Dunlap for pointing this problem out.
 *
 * (07/03/2001) gkh
 *	Fixed module paramater size.  Thanks to John Brockmeyer for the pointer.
 *	Fixed vendor and product getting defined through the MODULE_PARM macro
 *	if the Generic driver wasn't compiled in.
 *	Fixed problem with generic_shutdown() not being called for drivers that
 *	don't have a shutdown() function.
 *
 * (06/06/2001) gkh
 *	added evil hack that is needed for the prolific pl2303 device due to the
 *	crazy way its endpoints are set up.
 *
 * (05/30/2001) gkh
 *	switched from using spinlock to a semaphore, which fixes lots of problems.
 *
 * (04/08/2001) gb
 *	Identify version on module load.
 *
 * 2001_02_05 gkh
 *	Fixed buffer overflows bug with the generic serial driver.  Thanks to
 *	Todd Squires <squirest@ct0.com> for fixing this.
 *
 * (01/10/2001) gkh
 *	Fixed bug where the generic serial adaptor grabbed _any_ device that was
 *	offered to it.
 *
 * (12/12/2000) gkh
 *	Removed MOD_INC and MOD_DEC from poll and disconnect functions, and
 *	moved them to the serial_open and serial_close functions.
 *	Also fixed bug with there not being a MOD_DEC for the generic driver
 *	(thanks to Gary Brubaker for finding this.)
 *
 * (11/29/2000) gkh
 *	Small NULL pointer initialization cleanup which saves a bit of disk image
 *
 * (11/01/2000) Adam J. Richter
 *	instead of using idVendor/idProduct pairs, usb serial drivers
 *	now identify their hardware interest with usb_device_id tables,
 *	which they usually have anyhow for use with MODULE_DEVICE_TABLE.
 *
 * (10/05/2000) gkh
 *	Fixed bug with urb->dev not being set properly, now that the usb
 *	core needs it.
 * 
 * (09/11/2000) gkh
 *	Removed DEBUG #ifdefs with call to usb_serial_debug_data
 *
 * (08/28/2000) gkh
 *	Added port_lock to port structure.
 *	Added locks for SMP safeness to generic driver
 *	Fixed the ability to open a generic device's port more than once.
 *
 * (07/23/2000) gkh
 *	Added bulk_out_endpointAddress to port structure.
 *
 * (07/19/2000) gkh, pberger, and borchers
 *	Modifications to allow usb-serial drivers to be modules.
 *
 * (07/03/2000) gkh
 *	Added more debugging to serial_ioctl call
 * 
 * (06/25/2000) gkh
 *	Changed generic_write_bulk_callback to not call wake_up_interruptible
 *	directly, but to have port_softint do it at a safer time.
 *
 * (06/23/2000) gkh
 *	Cleaned up debugging statements in a quest to find UHCI timeout bug.
 *
 * (05/22/2000) gkh
 *	Changed the makefile, enabling the big CONFIG_USB_SERIAL_SOMTHING to be 
 *	removed from the individual device source files.
 *
 * (05/03/2000) gkh
 *	Added the Digi Acceleport driver from Al Borchers and Peter Berger.
 * 
 * (05/02/2000) gkh
 *	Changed devfs and tty register code to work properly now. This was based on
 *	the ACM driver changes by Vojtech Pavlik.
 *
 * (04/27/2000) Ryan VanderBijl
 * 	Put calls to *_paranoia_checks into one function.
 * 
 * (04/23/2000) gkh
 *	Fixed bug that Randy Dunlap found for Generic devices with no bulk out ports.
 *	Moved when the startup code printed out the devices that are supported.
 *
 * (04/19/2000) gkh
 *	Added driver for ZyXEL omni.net lcd plus ISDN TA
 *	Made startup info message specify which drivers were compiled in.
 *
 * (04/03/2000) gkh
 *	Changed the probe process to remove the module unload races.
 *	Changed where the tty layer gets initialized to have devfs work nicer.
 *	Added initial devfs support.
 *
 * (03/26/2000) gkh
 *	Split driver up into device specific pieces.
 * 
 * (03/19/2000) gkh
 *	Fixed oops that could happen when device was removed while a program
 *	was talking to the device.
 *	Removed the static urbs and now all urbs are created and destroyed
 *	dynamically.
 *	Reworked the internal interface. Now everything is based on the 
 *	usb_serial_port structure instead of the larger usb_serial structure.
 *	This fixes the bug that a multiport device could not have more than
 *	one port open at one time.
 *
 * (03/17/2000) gkh
 *	Added config option for debugging messages.
 *	Added patch for keyspan pda from Brian Warner.
 *
 * (03/06/2000) gkh
 *	Added the keyspan pda code from Brian Warner <warner@lothar.com>
 *	Moved a bunch of the port specific stuff into its own structure. This
 *	is in anticipation of the true multiport devices (there's a bug if you
 *	try to access more than one port of any multiport device right now)
 *
 * (02/21/2000) gkh
 *	Made it so that any serial devices only have to specify which functions
 *	they want to overload from the generic function calls (great, 
 *	inheritance in C, in a driver, just what I wanted...)
 *	Added support for set_termios and ioctl function calls. No drivers take
 *	advantage of this yet.
 *	Removed the #ifdef MODULE, now there is no module specific code.
 *	Cleaned up a few comments in usb-serial.h that were wrong (thanks again
 *	to Miles Lott).
 *	Small fix to get_free_serial.
 *
 * (02/14/2000) gkh
 *	Removed the Belkin and Peracom functionality from the driver due to
 *	the lack of support from the vendor, and me not wanting people to 
 *	accidenatly buy the device, expecting it to work with Linux.
 *	Added read_bulk_callback and write_bulk_callback to the type structure
 *	for the needs of the FTDI and WhiteHEAT driver.
 *	Changed all reverences to FTDI to FTDI_SIO at the request of Bill
 *	Ryder.
 *	Changed the output urb size back to the max endpoint size to make
 *	the ftdi_sio driver have it easier, and due to the fact that it didn't
 *	really increase the speed any.
 *
 * (02/11/2000) gkh
 *	Added VISOR_FUNCTION_CONSOLE to the visor startup function. This was a
 *	patch from Miles Lott (milos@insync.net).
 *	Fixed bug with not restoring the minor range that a device grabs, if
 *	the startup function fails (thanks Miles for finding this).
 *
 * (02/05/2000) gkh
 *	Added initial framework for the Keyspan PDA serial converter so that
 *	Brian Warner has a place to put his code.
 *	Made the ezusb specific functions generic enough that different
 *	devices can use them (whiteheat and keyspan_pda both need them).
 *	Split out a whole bunch of structure and other stuff to a seperate
 *	usb-serial.h file.
 *	Made the Visor connection messages a little more understandable, now
 *	that Miles Lott (milos@insync.net) has gotten the Generic channel to
 *	work. Also made them always show up in the log file.
 * 
 * (01/25/2000) gkh
 *	Added initial framework for FTDI serial converter so that Bill Ryder
 *	has a place to put his code.
 *	Added the vendor specific info from Handspring. Now we can print out
 *	informational debug messages as well as understand what is happening.
 *
 * (01/23/2000) gkh
 *	Fixed problem of crash when trying to open a port that didn't have a
 *	device assigned to it. Made the minor node finding a little smarter,
 *	now it looks to find a continous space for the new device.
 *
 * (01/21/2000) gkh
 *	Fixed bug in visor_startup with patch from Miles Lott (milos@insync.net)
 *	Fixed get_serial_by_minor which was all messed up for multi port 
 *	devices. Fixed multi port problem for generic devices. Now the number
 *	of ports is determined by the number of bulk out endpoints for the
 *	generic device.
 *
 * (01/19/2000) gkh
 *	Removed lots of cruft that was around from the old (pre urb) driver 
 *	interface.
 *	Made the serial_table dynamic. This should save lots of memory when
 *	the number of minor nodes goes up to 256.
 *	Added initial support for devices that have more than one port. 
 *	Added more debugging comments for the Visor, and added a needed 
 *	set_configuration call.
 *
 * (01/17/2000) gkh
 *	Fixed the WhiteHEAT firmware (my processing tool had a bug)
 *	and added new debug loader firmware for it.
 *	Removed the put_char function as it isn't really needed.
 *	Added visor startup commands as found by the Win98 dump.
 * 
 * (01/13/2000) gkh
 *	Fixed the vendor id for the generic driver to the one I meant it to be.
 *
 * (01/12/2000) gkh
 *	Forget the version numbering...that's pretty useless...
 *	Made the driver able to be compiled so that the user can select which
 *	converter they want to use. This allows people who only want the Visor
 *	support to not pay the memory size price of the WhiteHEAT.
 *	Fixed bug where the generic driver (idVendor=0000 and idProduct=0000)
 *	grabbed the root hub. Not good.
 * 
 * version 0.4.0 (01/10/2000) gkh
 *	Added whiteheat.h containing the firmware for the ConnectTech WhiteHEAT
 *	device. Added startup function to allow firmware to be downloaded to
 *	a device if it needs to be.
 *	Added firmware download logic to the WhiteHEAT device.
 *	Started to add #defines to split up the different drivers for potential
 *	configuration option.
 *	
 * version 0.3.1 (12/30/99) gkh
 *      Fixed problems with urb for bulk out.
 *      Added initial support for multiple sets of endpoints. This enables
 *      the Handspring Visor to be attached successfully. Only the first
 *      bulk in / bulk out endpoint pair is being used right now.
 *
 * version 0.3.0 (12/27/99) gkh
 *	Added initial support for the Handspring Visor based on a patch from
 *	Miles Lott (milos@sneety.insync.net)
 *	Cleaned up the code a bunch and converted over to using urbs only.
 *
 * version 0.2.3 (12/21/99) gkh
 *	Added initial support for the Connect Tech WhiteHEAT converter.
 *	Incremented the number of ports in expectation of getting the
 *	WhiteHEAT to work properly (4 ports per connection).
 *	Added notification on insertion and removal of what port the
 *	device is/was connected to (and what kind of device it was).
 *
 * version 0.2.2 (12/16/99) gkh
 *	Changed major number to the new allocated number. We're legal now!
 *
 * version 0.2.1 (12/14/99) gkh
 *	Fixed bug that happens when device node is opened when there isn't a
 *	device attached to it. Thanks to marek@webdesign.no for noticing this.
 *
 * version 0.2.0 (11/10/99) gkh
 *	Split up internals to make it easier to add different types of serial 
 *	converters to the code.
 *	Added a "generic" driver that gets it's vendor and product id
 *	from when the module is loaded. Thanks to David E. Nelson (dnelson@jump.net)
 *	for the idea and sample code (from the usb scanner driver.)
 *	Cleared up any licensing questions by releasing it under the GNU GPL.
 *
 * version 0.1.2 (10/25/99) gkh
 * 	Fixed bug in detecting device.
 *
 * version 0.1.1 (10/05/99) gkh
 * 	Changed the major number to not conflict with anything else.
 *
 * version 0.1 (09/28/99) gkh
 * 	Can recognize the two different devices and start up a read from
 *	device when asked to. Writes also work. No control signals yet, this
 *	all is vendor specific data (i.e. no spec), also no control for
 *	different baud rates or other bit settings.
 *	Currently we are using the same devid as the acm driver. This needs
 *	to change.
 * 
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/smp_lock.h>
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
#define DRIVER_VERSION "v1.4"
#define DRIVER_AUTHOR "Greg Kroah-Hartman, greg@kroah.com, http://www.kroah.com/linux-usb/"
#define DRIVER_DESC "USB Serial Driver core"

/* function prototypes for a "generic" type serial converter (no flow control, not all endpoints needed) */
/* need to always compile these in, as some of the other devices use these functions as their own. */
/* if a driver does not provide a function pointer, the generic function will be called. */
static int  generic_open		(struct usb_serial_port *port, struct file *filp);
static void generic_close		(struct usb_serial_port *port, struct file *filp);
static int  generic_write		(struct usb_serial_port *port, int from_user, const unsigned char *buf, int count);
static int  generic_write_room		(struct usb_serial_port *port);
static int  generic_chars_in_buffer	(struct usb_serial_port *port);
static void generic_read_bulk_callback	(struct urb *urb);
static void generic_write_bulk_callback	(struct urb *urb);
static void generic_shutdown		(struct usb_serial *serial);


#ifdef CONFIG_USB_SERIAL_GENERIC
static __u16	vendor	= 0x05f9;
static __u16	product	= 0xffff;

static struct usb_device_id generic_device_ids[2]; /* Initially all zeroes. */

/* All of the device info needed for the Generic Serial Converter */
static struct usb_serial_device_type generic_device = {
	.owner =		THIS_MODULE,
	.name =			"Generic",
	.id_table =		generic_device_ids,
	.num_interrupt_in =	NUM_DONT_CARE,
	.num_bulk_in =		NUM_DONT_CARE,
	.num_bulk_out =		NUM_DONT_CARE,
	.num_ports =		1,
	.shutdown =		generic_shutdown,
};
#endif


/* local function prototypes */
static int  serial_open (struct tty_struct *tty, struct file * filp);
static void serial_close (struct tty_struct *tty, struct file * filp);
static int  serial_write (struct tty_struct * tty, int from_user, const unsigned char *buf, int count);
static int  serial_write_room (struct tty_struct *tty);
static int  serial_chars_in_buffer (struct tty_struct *tty);
static void serial_throttle (struct tty_struct * tty);
static void serial_unthrottle (struct tty_struct * tty);
static int  serial_ioctl (struct tty_struct *tty, struct file * file, unsigned int cmd, unsigned long arg);
static void serial_set_termios (struct tty_struct *tty, struct termios * old);
static void serial_shutdown (struct usb_serial *serial);

static void * usb_serial_probe(struct usb_device *dev, unsigned int ifnum,
			       const struct usb_device_id *id);
static void usb_serial_disconnect(struct usb_device *dev, void *ptr);

static struct usb_driver usb_serial_driver = {
	.name =		"serial",
	.probe =	usb_serial_probe,
	.disconnect =	usb_serial_disconnect,
	.id_table =	NULL, 			/* check all devices */
};

/* There is no MODULE_DEVICE_TABLE for usbserial.c.  Instead
   the MODULE_DEVICE_TABLE declarations in each serial driver
   cause the "hotplug" program to pull in whatever module is necessary
   via modprobe, and modprobe will load usbserial because the serial
   drivers depend on it.
*/


static int			serial_refcount;
static struct tty_driver	serial_tty_driver;
static struct tty_struct *	serial_tty[SERIAL_TTY_MINORS];
static struct termios *		serial_termios[SERIAL_TTY_MINORS];
static struct termios *		serial_termios_locked[SERIAL_TTY_MINORS];
static struct usb_serial	*serial_table[SERIAL_TTY_MINORS];	/* initially all NULL */


static LIST_HEAD(usb_serial_driver_list);


static struct usb_serial *get_serial_by_minor (unsigned int minor)
{
	return serial_table[minor];
}


static struct usb_serial *get_free_serial (int num_ports, int *minor)
{
	struct usb_serial *serial = NULL;
	int i, j;
	int good_spot;

	dbg("%s %d", __FUNCTION__, num_ports);

	*minor = 0;
	for (i = 0; i < SERIAL_TTY_MINORS; ++i) {
		if (serial_table[i])
			continue;

		good_spot = 1;
		for (j = 1; j <= num_ports-1; ++j)
			if (serial_table[i+j])
				good_spot = 0;
		if (good_spot == 0)
			continue;
			
		if (!(serial = kmalloc(sizeof(struct usb_serial), GFP_KERNEL))) {
			err("%s - Out of memory", __FUNCTION__);
			return NULL;
		}
		memset(serial, 0, sizeof(struct usb_serial));
		serial->magic = USB_SERIAL_MAGIC;
		serial_table[i] = serial;
		*minor = i;
		dbg("%s - minor base = %d", __FUNCTION__, *minor);
		for (i = *minor+1; (i < (*minor + num_ports)) && (i < SERIAL_TTY_MINORS); ++i)
			serial_table[i] = serial;
		return serial;
	}
	return NULL;
}

static void return_serial (struct usb_serial *serial)
{
	int i;

	dbg("%s", __FUNCTION__);

	if (serial == NULL)
		return;

	for (i = 0; i < serial->num_ports; ++i) {
		serial_table[serial->minor + i] = NULL;
	}

	return;
}

#ifdef USES_EZUSB_FUNCTIONS
/* EZ-USB Control and Status Register.  Bit 0 controls 8051 reset */
#define CPUCS_REG    0x7F92

int ezusb_writememory (struct usb_serial *serial, int address, unsigned char *data, int length, __u8 bRequest)
{
	int result;
	unsigned char *transfer_buffer;

	/* dbg("ezusb_writememory %x, %d", address, length); */
	if (!serial->dev) {
		dbg("%s - no physical device present, failing.", __FUNCTION__);
		return -ENODEV;
	}

	transfer_buffer =  kmalloc (length, GFP_KERNEL);
	if (!transfer_buffer) {
		err("%s - kmalloc(%d) failed.", __FUNCTION__, length);
		return -ENOMEM;
	}
	memcpy (transfer_buffer, data, length);
	result = usb_control_msg (serial->dev, usb_sndctrlpipe(serial->dev, 0), bRequest, 0x40, address, 0, transfer_buffer, length, 3*HZ);
	kfree (transfer_buffer);
	return result;
}

int ezusb_set_reset (struct usb_serial *serial, unsigned char reset_bit)
{
	int	response;
	dbg("%s - %d", __FUNCTION__, reset_bit);
	response = ezusb_writememory (serial, CPUCS_REG, &reset_bit, 1, 0xa0);
	if (response < 0) {
		err("%s- %d failed", __FUNCTION__, reset_bit);
	}
	return response;
}

#endif	/* USES_EZUSB_FUNCTIONS */

/*****************************************************************************
 * Driver tty interface functions
 *****************************************************************************/
static int serial_open (struct tty_struct *tty, struct file * filp)
{
	struct usb_serial *serial;
	struct usb_serial_port *port;
	unsigned int portNumber;
	int retval = 0;
	
	dbg("%s", __FUNCTION__);

	/* initialize the pointer incase something fails */
	tty->driver_data = NULL;

	/* get the serial object associated with this tty pointer */
	serial = get_serial_by_minor (MINOR(tty->device));

	if (serial_paranoia_check (serial, __FUNCTION__))
		return -ENODEV;

	/* set up our port structure making the tty driver remember our port object, and us it */
	portNumber = MINOR(tty->device) - serial->minor;
	port = &serial->port[portNumber];
	tty->driver_data = port;

	down (&port->sem);
	port->tty = tty;
	 
	/* lock this module before we call it */
	if (serial->type->owner)
		__MOD_INC_USE_COUNT(serial->type->owner);

	++port->open_count;
	if (port->open_count == 1) {
		/* only call the device specific open if this 
		 * is the first time the port is opened */
		if (serial->type->open)
			retval = serial->type->open(port, filp);
		else
			retval = generic_open(port, filp);
	}

	if (retval) {
		port->open_count = 0;
		if (serial->type->owner)
			__MOD_DEC_USE_COUNT(serial->type->owner);
	}

	up (&port->sem);
	return retval;
}

static void __serial_close(struct usb_serial_port *port, struct file *filp)
{
	if (!port->open_count) {
		dbg ("%s - port not opened", __FUNCTION__);
		return;
	}

	--port->open_count;
	if (port->open_count <= 0) {
		/* only call the device specific close if this 
		 * port is being closed by the last owner */
		if (port->serial->type->close)
			port->serial->type->close(port, filp);
		else
			generic_close(port, filp);
		port->open_count = 0;
		if (port->tty) {
			port->tty->driver_data = NULL;
			port->tty = NULL;
		}
	}

	if (port->serial->type->owner)
		__MOD_DEC_USE_COUNT(port->serial->type->owner);
}

static void serial_close(struct tty_struct *tty, struct file * filp)
{
	struct usb_serial_port *port = (struct usb_serial_port *) tty->driver_data;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);

	if (!serial)
		return;

	down (&port->sem);

	dbg("%s - port %d", __FUNCTION__, port->number);

	/* if disconnect beat us to the punch here, there's nothing to do */
	if (tty->driver_data) {
		__serial_close(port, filp);
	}

	up (&port->sem);
}

static int serial_write (struct tty_struct * tty, int from_user, const unsigned char *buf, int count)
{
	struct usb_serial_port *port = (struct usb_serial_port *) tty->driver_data;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);
	int retval = -EINVAL;

	if (!serial)
		return -ENODEV;

	down (&port->sem);

	dbg("%s - port %d, %d byte(s)", __FUNCTION__, port->number, count);

	if (!port->open_count) {
		dbg("%s - port not opened", __FUNCTION__);
		goto exit;
	}

	/* pass on to the driver specific version of this function if it is available */
	if (serial->type->write)
		retval = serial->type->write(port, from_user, buf, count);
	else
		retval = generic_write(port, from_user, buf, count);

exit:
	up (&port->sem);
	return retval;
}

static int serial_write_room (struct tty_struct *tty) 
{
	struct usb_serial_port *port = (struct usb_serial_port *) tty->driver_data;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);
	int retval = -EINVAL;

	if (!serial)
		return -ENODEV;

	down (&port->sem);

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (!port->open_count) {
		dbg("%s - port not open", __FUNCTION__);
		goto exit;
	}

	/* pass on to the driver specific version of this function if it is available */
	if (serial->type->write_room)
		retval = serial->type->write_room(port);
	else
		retval = generic_write_room(port);

exit:
	up (&port->sem);
	return retval;
}

static int serial_chars_in_buffer (struct tty_struct *tty) 
{
	struct usb_serial_port *port = (struct usb_serial_port *) tty->driver_data;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);
	int retval = -EINVAL;

	if (!serial)
		return -ENODEV;

	down (&port->sem);

	dbg("%s = port %d", __FUNCTION__, port->number);

	if (!port->open_count) {
		dbg("%s - port not open", __FUNCTION__);
		goto exit;
	}

	/* pass on to the driver specific version of this function if it is available */
	if (serial->type->chars_in_buffer)
		retval = serial->type->chars_in_buffer(port);
	else
		retval = generic_chars_in_buffer(port);

exit:
	up (&port->sem);
	return retval;
}

static void serial_throttle (struct tty_struct * tty)
{
	struct usb_serial_port *port = (struct usb_serial_port *) tty->driver_data;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);

	if (!serial)
		return;

	down (&port->sem);

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (!port->open_count) {
		dbg ("%s - port not open", __FUNCTION__);
		goto exit;
	}

	/* pass on to the driver specific version of this function */
	if (serial->type->throttle)
		serial->type->throttle(port);

exit:
	up (&port->sem);
}

static void serial_unthrottle (struct tty_struct * tty)
{
	struct usb_serial_port *port = (struct usb_serial_port *) tty->driver_data;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);

	if (!serial)
		return;

	down (&port->sem);

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (!port->open_count) {
		dbg("%s - port not open", __FUNCTION__);
		goto exit;
	}

	/* pass on to the driver specific version of this function */
	if (serial->type->unthrottle)
		serial->type->unthrottle(port);

exit:
	up (&port->sem);
}

static int serial_ioctl (struct tty_struct *tty, struct file * file, unsigned int cmd, unsigned long arg)
{
	struct usb_serial_port *port = (struct usb_serial_port *) tty->driver_data;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);
	int retval = -ENODEV;

	if (!serial)
		return -ENODEV;

	down (&port->sem);

	dbg("%s - port %d, cmd 0x%.4x", __FUNCTION__, port->number, cmd);

	if (!port->open_count) {
		dbg ("%s - port not open", __FUNCTION__);
		goto exit;
	}

	/* pass on to the driver specific version of this function if it is available */
	if (serial->type->ioctl)
		retval = serial->type->ioctl(port, file, cmd, arg);
	else
		retval = -ENOIOCTLCMD;

exit:
	up (&port->sem);
	return retval;
}

static void serial_set_termios (struct tty_struct *tty, struct termios * old)
{
	struct usb_serial_port *port = (struct usb_serial_port *) tty->driver_data;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);

	if (!serial)
		return;

	down (&port->sem);

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (!port->open_count) {
		dbg("%s - port not open", __FUNCTION__);
		goto exit;
	}

	/* pass on to the driver specific version of this function if it is available */
	if (serial->type->set_termios)
		serial->type->set_termios(port, old);

exit:
	up (&port->sem);
}

static void serial_break (struct tty_struct *tty, int break_state)
{
	struct usb_serial_port *port = (struct usb_serial_port *) tty->driver_data;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);

	if (!serial)
		return;

	down (&port->sem);

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (!port->open_count) {
		dbg("%s - port not open", __FUNCTION__);
		goto exit;
	}

	/* pass on to the driver specific version of this function if it is available */
	if (serial->type->break_ctl)
		serial->type->break_ctl(port, break_state);

exit:
	up (&port->sem);
}

static void serial_shutdown (struct usb_serial *serial)
{
	dbg ("%s", __FUNCTION__);

	if (serial->type->shutdown)
		serial->type->shutdown(serial);
	else
		generic_shutdown(serial);
}

static int serial_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct usb_serial *serial;
	int length = 0;
	int i;
	off_t begin = 0;
	char tmp[40];

	dbg("%s", __FUNCTION__);
	length += sprintf (page, "usbserinfo:1.0 driver:%s\n", DRIVER_VERSION);
	for (i = 0; i < SERIAL_TTY_MINORS && length < PAGE_SIZE; ++i) {
		serial = get_serial_by_minor(i);
		if (serial == NULL)
			continue;

		length += sprintf (page+length, "%d:", i);
		if (serial->type->owner)
			length += sprintf (page+length, " module:%s", serial->type->owner->name);
		length += sprintf (page+length, " name:\"%s\"", serial->type->name);
		length += sprintf (page+length, " vendor:%04x product:%04x", serial->vendor, serial->product);
		length += sprintf (page+length, " num_ports:%d", serial->num_ports);
		length += sprintf (page+length, " port:%d", i - serial->minor + 1);

		usb_make_path(serial->dev, tmp, sizeof(tmp));
		length += sprintf (page+length, " path:%s", tmp);
			
		length += sprintf (page+length, "\n");
		if ((length + begin) > (off + count))
			goto done;
		if ((length + begin) < off) {
			begin += length;
			length = 0;
		}
	}
	*eof = 1;
done:
	if (off >= (length + begin))
		return 0;
	*start = page + (off-begin);
	return ((count < begin+length-off) ? count : begin+length-off);
}

/*****************************************************************************
 * generic devices specific driver functions
 *****************************************************************************/
static int generic_open (struct usb_serial_port *port, struct file *filp)
{
	struct usb_serial *serial = port->serial;
	int result = 0;

	if (port_paranoia_check (port, __FUNCTION__))
		return -ENODEV;

	dbg("%s - port %d", __FUNCTION__, port->number);

	/* force low_latency on so that our tty_push actually forces the data through, 
	   otherwise it is scheduled, and with high data rates (like with OHCI) data
	   can get lost. */
	if (port->tty)
		port->tty->low_latency = 1;

	/* if we have a bulk interrupt, start reading from it */
	if (serial->num_bulk_in) {
		/* Start reading from the device */
		usb_fill_bulk_urb (port->read_urb, serial->dev,
				   usb_rcvbulkpipe(serial->dev, port->bulk_in_endpointAddress),
				   port->read_urb->transfer_buffer,
				   port->read_urb->transfer_buffer_length,
				   ((serial->type->read_bulk_callback) ?
				     serial->type->read_bulk_callback :
				     generic_read_bulk_callback),
				   port);
		result = usb_submit_urb(port->read_urb);
		if (result)
			err("%s - failed resubmitting read urb, error %d", __FUNCTION__, result);
	}

	return result;
}

static void generic_cleanup (struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (serial->dev) {
		/* shutdown any bulk reads that might be going on */
		if (serial->num_bulk_out)
			usb_unlink_urb (port->write_urb);
		if (serial->num_bulk_in)
			usb_unlink_urb (port->read_urb);
	}
}

static void generic_close (struct usb_serial_port *port, struct file * filp)
{
	dbg("%s - port %d", __FUNCTION__, port->number);
	generic_cleanup (port);
}

static int generic_write (struct usb_serial_port *port, int from_user, const unsigned char *buf, int count)
{
	struct usb_serial *serial = port->serial;
	int result;

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (count == 0) {
		dbg("%s - write request of 0 bytes", __FUNCTION__);
		return (0);
	}

	/* only do something if we have a bulk out endpoint */
	if (serial->num_bulk_out) {
		if (port->write_urb->status == -EINPROGRESS) {
			dbg("%s - already writing", __FUNCTION__);
			return (0);
		}

		count = (count > port->bulk_out_size) ? port->bulk_out_size : count;

		if (from_user) {
			if (copy_from_user(port->write_urb->transfer_buffer, buf, count))
				return -EFAULT;
		}
		else {
			memcpy (port->write_urb->transfer_buffer, buf, count);
		}

		usb_serial_debug_data (__FILE__, __FUNCTION__, count, port->write_urb->transfer_buffer);

		/* set up our urb */
		usb_fill_bulk_urb (port->write_urb, serial->dev,
				   usb_sndbulkpipe (serial->dev,
						    port->bulk_out_endpointAddress),
				   port->write_urb->transfer_buffer, count,
				   ((serial->type->write_bulk_callback) ? 
				     serial->type->write_bulk_callback :
				     generic_write_bulk_callback), port);

		/* send the data out the bulk port */
		result = usb_submit_urb(port->write_urb);
		if (result)
			err("%s - failed submitting write urb, error %d", __FUNCTION__, result);
		else
			result = count;

		return result;
	}

	/* no bulk out, so return 0 bytes written */
	return (0);
}

static int generic_write_room (struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;
	int room = 0;

	dbg("%s - port %d", __FUNCTION__, port->number);
	
	if (serial->num_bulk_out) {
		if (port->write_urb->status != -EINPROGRESS)
			room = port->bulk_out_size;
	}

	dbg("%s - returns %d", __FUNCTION__, room);
	return (room);
}

static int generic_chars_in_buffer (struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;
	int chars = 0;

	dbg("%s - port %d", __FUNCTION__, port->number);

	if (serial->num_bulk_out) {
		if (port->write_urb->status == -EINPROGRESS)
			chars = port->write_urb->transfer_buffer_length;
	}

	dbg("%s - returns %d", __FUNCTION__, chars);
	return (chars);
}

static void generic_read_bulk_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
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
	}

	/* Continue trying to always read  */
	usb_fill_bulk_urb (port->read_urb, serial->dev,
			   usb_rcvbulkpipe (serial->dev,
				   	    port->bulk_in_endpointAddress),
			   port->read_urb->transfer_buffer,
			   port->read_urb->transfer_buffer_length,
			   ((serial->type->read_bulk_callback) ? 
			     serial->type->read_bulk_callback : 
			     generic_read_bulk_callback), port);
	result = usb_submit_urb(port->read_urb);
	if (result)
		err("%s - failed resubmitting read urb, error %d", __FUNCTION__, result);
}

static void generic_write_bulk_callback (struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
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

	queue_task(&port->tqueue, &tq_immediate);
	mark_bh(IMMEDIATE_BH);

	return;
}

static void generic_shutdown (struct usb_serial *serial)
{
	int i;

	dbg("%s", __FUNCTION__);

	/* stop reads and writes on all ports */
	for (i=0; i < serial->num_ports; ++i) {
		generic_cleanup (&serial->port[i]);
	}
}

static void port_softint(void *private)
{
	struct usb_serial_port *port = (struct usb_serial_port *)private;
	struct usb_serial *serial = get_usb_serial (port, __FUNCTION__);
	struct tty_struct *tty;

	dbg("%s - port %d", __FUNCTION__, port->number);
	
	if (!serial)
		return;

	tty = port->tty;
	if (!tty)
		return;

	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup) {
		dbg("%s - write wakeup call.", __FUNCTION__);
		(tty->ldisc.write_wakeup)(tty);
	}

	wake_up_interruptible(&tty->write_wait);
}



static void * usb_serial_probe(struct usb_device *dev, unsigned int ifnum,
			       const struct usb_device_id *id)
{
	struct usb_serial *serial = NULL;
	struct usb_serial_port *port;
	struct usb_interface *interface;
	struct usb_interface_descriptor *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_endpoint_descriptor *interrupt_in_endpoint[MAX_NUM_PORTS];
	struct usb_endpoint_descriptor *bulk_in_endpoint[MAX_NUM_PORTS];
	struct usb_endpoint_descriptor *bulk_out_endpoint[MAX_NUM_PORTS];
	struct usb_serial_device_type *type = NULL;
	struct list_head *tmp;
	int found;
	int minor;
	int buffer_size;
	int i;
	int num_interrupt_in = 0;
	int num_bulk_in = 0;
	int num_bulk_out = 0;
	int num_ports;
	int max_endpoints;
	const struct usb_device_id *id_pattern = NULL;

	/* loop through our list of known serial converters, and see if this
	   device matches. */
	found = 0;
	interface = &dev->actconfig->interface[ifnum];
	list_for_each (tmp, &usb_serial_driver_list) {
		type = list_entry(tmp, struct usb_serial_device_type, driver_list);
		id_pattern = usb_match_id(dev, interface, type->id_table);
		if (id_pattern != NULL) {
			dbg("descriptor matches");
			found = 1;
			break;
		}
	}
	if (!found) {
		/* no match */
		dbg("none matched");
		return(NULL);
	}
	
	/* descriptor matches, let's find the endpoints needed */
	/* check out the endpoints */
	iface_desc = &interface->altsetting[0];
	for (i = 0; i < iface_desc->bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i];
		
		if ((endpoint->bEndpointAddress & 0x80) &&
		    ((endpoint->bmAttributes & 3) == 0x02)) {
			/* we found a bulk in endpoint */
			dbg("found bulk in");
			bulk_in_endpoint[num_bulk_in] = endpoint;
			++num_bulk_in;
		}

		if (((endpoint->bEndpointAddress & 0x80) == 0x00) &&
		    ((endpoint->bmAttributes & 3) == 0x02)) {
			/* we found a bulk out endpoint */
			dbg("found bulk out");
			bulk_out_endpoint[num_bulk_out] = endpoint;
			++num_bulk_out;
		}
		
		if ((endpoint->bEndpointAddress & 0x80) &&
		    ((endpoint->bmAttributes & 3) == 0x03)) {
			/* we found a interrupt in endpoint */
			dbg("found interrupt in");
			interrupt_in_endpoint[num_interrupt_in] = endpoint;
			++num_interrupt_in;
		}
	}

#if defined(CONFIG_USB_SERIAL_PL2303) || defined(CONFIG_USB_SERIAL_PL2303_MODULE)
	/* BEGIN HORRIBLE HACK FOR PL2303 */ 
	/* this is needed due to the looney way its endpoints are set up */
	if (((dev->descriptor.idVendor == PL2303_VENDOR_ID) &&
	     (dev->descriptor.idProduct == PL2303_PRODUCT_ID)) ||
	    ((dev->descriptor.idVendor == ATEN_VENDOR_ID) &&
	     (dev->descriptor.idProduct == ATEN_PRODUCT_ID))) {
		if (ifnum == 1) {
			/* check out the endpoints of the other interface*/
			struct usb_interface *other_iface;

			other_iface = &dev->actconfig->interface[ifnum ^ 1];
			iface_desc = &other_iface->altsetting[0];
			for (i = 0; i < iface_desc->bNumEndpoints; ++i) {
				endpoint = &iface_desc->endpoint[i];
				if ((endpoint->bEndpointAddress & 0x80) &&
				    ((endpoint->bmAttributes & 3) == 0x03)) {
					/* we found a interrupt in endpoint */
					dbg("found interrupt in for Prolific device on separate interface");
					interrupt_in_endpoint[num_interrupt_in] = endpoint;
					++num_interrupt_in;
				}
			}
		}

		/* Now make sure the PL-2303 is configured correctly.
		 * If not, give up now and hope this hack will work
		 * properly during a later invocation of usb_serial_probe
		 */
		if (num_bulk_in == 0 || num_bulk_out == 0) {
			info("PL-2303 hack: descriptors matched but endpoints did not");
			return NULL;
		}
	}
	/* END HORRIBLE HACK FOR PL2303 */
#endif

	/* found all that we need */
	info("%s converter detected", type->name);

#ifdef CONFIG_USB_SERIAL_GENERIC
	if (type == &generic_device) {
		num_ports = num_bulk_out;
		if (num_ports == 0) {
			err("Generic device with no bulk out, not allowed.");
			return NULL;
		}
	} else
#endif
		num_ports = type->num_ports;

	serial = get_free_serial (num_ports, &minor);
	if (serial == NULL) {
		err("No more free serial devices");
		return NULL;
	}

	serial->dev = dev;
	serial->type = type;
	serial->interface = interface;
	serial->minor = minor;
	serial->num_ports = num_ports;
	serial->num_bulk_in = num_bulk_in;
	serial->num_bulk_out = num_bulk_out;
	serial->num_interrupt_in = num_interrupt_in;
	serial->vendor = dev->descriptor.idVendor;
	serial->product = dev->descriptor.idProduct;

	/* set up the endpoint information */
	for (i = 0; i < num_bulk_in; ++i) {
		endpoint = bulk_in_endpoint[i];
		port = &serial->port[i];
		port->read_urb = usb_alloc_urb (0);
		if (!port->read_urb) {
			err("No free urbs available");
			goto probe_error;
		}
		buffer_size = endpoint->wMaxPacketSize;
		port->bulk_in_endpointAddress = endpoint->bEndpointAddress;
		port->bulk_in_buffer = kmalloc (buffer_size, GFP_KERNEL);
		if (!port->bulk_in_buffer) {
			err("Couldn't allocate bulk_in_buffer");
			goto probe_error;
		}
		usb_fill_bulk_urb (port->read_urb, dev,
				   usb_rcvbulkpipe (dev,
					   	    endpoint->bEndpointAddress),
				   port->bulk_in_buffer, buffer_size,
				   ((serial->type->read_bulk_callback) ? 
				     serial->type->read_bulk_callback : 
				     generic_read_bulk_callback),
				   port);
	}

	for (i = 0; i < num_bulk_out; ++i) {
		endpoint = bulk_out_endpoint[i];
		port = &serial->port[i];
		port->write_urb = usb_alloc_urb(0);
		if (!port->write_urb) {
			err("No free urbs available");
			goto probe_error;
		}
		buffer_size = endpoint->wMaxPacketSize;
		port->bulk_out_size = buffer_size;
		port->bulk_out_endpointAddress = endpoint->bEndpointAddress;
		port->bulk_out_buffer = kmalloc (buffer_size, GFP_KERNEL);
		if (!port->bulk_out_buffer) {
			err("Couldn't allocate bulk_out_buffer");
			goto probe_error;
		}
		usb_fill_bulk_urb (port->write_urb, dev,
				   usb_sndbulkpipe (dev,
						    endpoint->bEndpointAddress),
				   port->bulk_out_buffer, buffer_size, 
				   ((serial->type->write_bulk_callback) ? 
				     serial->type->write_bulk_callback : 
				     generic_write_bulk_callback),
				   port);
	}

	for (i = 0; i < num_interrupt_in; ++i) {
		endpoint = interrupt_in_endpoint[i];
		port = &serial->port[i];
		port->interrupt_in_urb = usb_alloc_urb(0);
		if (!port->interrupt_in_urb) {
			err("No free urbs available");
			goto probe_error;
		}
		buffer_size = endpoint->wMaxPacketSize;
		port->interrupt_in_endpointAddress = endpoint->bEndpointAddress;
		port->interrupt_in_buffer = kmalloc (buffer_size, GFP_KERNEL);
		if (!port->interrupt_in_buffer) {
			err("Couldn't allocate interrupt_in_buffer");
			goto probe_error;
		}
		usb_fill_int_urb (port->interrupt_in_urb, dev, 
				  usb_rcvintpipe (dev,
						  endpoint->bEndpointAddress),
				  port->interrupt_in_buffer, buffer_size, 
				  serial->type->read_int_callback, port, 
				  endpoint->bInterval);
	}

	/* initialize some parts of the port structures */
	/* we don't use num_ports here cauz some devices have more endpoint pairs than ports */
	max_endpoints = max(num_bulk_in, num_bulk_out);
	max_endpoints = max(max_endpoints, num_interrupt_in);
	max_endpoints = max(max_endpoints, (int)serial->num_ports);
	dbg("%s - setting up %d port structures for this device", __FUNCTION__, max_endpoints);
	for (i = 0; i < max_endpoints; ++i) {
		port = &serial->port[i];
		port->number = i + serial->minor;
		port->serial = serial;
		port->magic = USB_SERIAL_PORT_MAGIC;
		port->tqueue.routine = port_softint;
		port->tqueue.data = port;
		init_MUTEX (&port->sem);
	}

	/* if this device type has a startup function, call it */
	if (type->startup) {
		i = type->startup (serial);
		if (i < 0)
			goto probe_error;
		if (i > 0)
			return serial;
	}

	/* initialize the devfs nodes for this device and let the user know what ports we are bound to */
	for (i = 0; i < serial->num_ports; ++i) {
		tty_register_devfs (&serial_tty_driver, 0, serial->port[i].number);
		info("%s converter now attached to ttyUSB%d (or usb/tts/%d for devfs)", 
		     type->name, serial->port[i].number, serial->port[i].number);
	}

	return serial; /* success */


probe_error:
	for (i = 0; i < num_bulk_in; ++i) {
		port = &serial->port[i];
		if (port->read_urb)
			usb_free_urb (port->read_urb);
		if (port->bulk_in_buffer)
			kfree (port->bulk_in_buffer);
	}
	for (i = 0; i < num_bulk_out; ++i) {
		port = &serial->port[i];
		if (port->write_urb)
			usb_free_urb (port->write_urb);
		if (port->bulk_out_buffer)
			kfree (port->bulk_out_buffer);
	}
	for (i = 0; i < num_interrupt_in; ++i) {
		port = &serial->port[i];
		if (port->interrupt_in_urb)
			usb_free_urb (port->interrupt_in_urb);
		if (port->interrupt_in_buffer)
			kfree (port->interrupt_in_buffer);
	}

	/* return the minor range that this device had */
	return_serial (serial);

	/* free up any memory that we allocated */
	kfree (serial);
	return NULL;
}

static void usb_serial_disconnect(struct usb_device *dev, void *ptr)
{
	struct usb_serial *serial = (struct usb_serial *) ptr;
	struct usb_serial_port *port;
	int i;

	dbg ("%s", __FUNCTION__);
	if (serial) {
		/* fail all future close/read/write/ioctl/etc calls */
		for (i = 0; i < serial->num_ports; ++i) {
			port = &serial->port[i];
			down (&port->sem);
			if (port->tty != NULL)
				while (port->open_count > 0)
					__serial_close(port, NULL);
			up (&port->sem);
		}

		serial->dev = NULL;
		serial_shutdown (serial);

		for (i = 0; i < serial->num_ports; ++i)
			serial->port[i].open_count = 0;

		for (i = 0; i < serial->num_bulk_in; ++i) {
			port = &serial->port[i];
			if (port->read_urb) {
				usb_unlink_urb (port->read_urb);
				usb_free_urb (port->read_urb);
			}
			if (port->bulk_in_buffer)
				kfree (port->bulk_in_buffer);
		}
		for (i = 0; i < serial->num_bulk_out; ++i) {
			port = &serial->port[i];
			if (port->write_urb) {
				usb_unlink_urb (port->write_urb);
				usb_free_urb (port->write_urb);
			}
			if (port->bulk_out_buffer)
				kfree (port->bulk_out_buffer);
		}
		for (i = 0; i < serial->num_interrupt_in; ++i) {
			port = &serial->port[i];
			if (port->interrupt_in_urb) {
				usb_unlink_urb (port->interrupt_in_urb);
				usb_free_urb (port->interrupt_in_urb);
			}
			if (port->interrupt_in_buffer)
				kfree (port->interrupt_in_buffer);
		}

		for (i = 0; i < serial->num_ports; ++i) {
			tty_unregister_devfs (&serial_tty_driver, serial->port[i].number);
			info("%s converter now disconnected from ttyUSB%d", serial->type->name, serial->port[i].number);
		}

		/* return the minor range that this device had */
		return_serial (serial);

		/* free up any memory that we allocated */
		kfree (serial);

	} else {
		info("device disconnected");
	}

}


static struct tty_driver serial_tty_driver = {
	.magic =		TTY_DRIVER_MAGIC,
	.driver_name =		"usb-serial",
#ifndef CONFIG_DEVFS_FS
	.name =			"ttyUSB",
#else
	.name =			"usb/tts/%d",
#endif
	.major =		SERIAL_TTY_MAJOR,
	.minor_start =		0,
	.num =			SERIAL_TTY_MINORS,
	.type =			TTY_DRIVER_TYPE_SERIAL,
	.subtype =		SERIAL_TYPE_NORMAL,
	.flags =		TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS,

	.refcount =		&serial_refcount,
	.table =		serial_tty,
	.termios =		serial_termios,
	.termios_locked =	serial_termios_locked,

	.open =			serial_open,
	.close =		serial_close,
	.write =		serial_write,
	.write_room =		serial_write_room,
	.ioctl =		serial_ioctl,
	.set_termios =		serial_set_termios,
	.throttle =		serial_throttle,
	.unthrottle =		serial_unthrottle,
	.break_ctl =		serial_break,
	.chars_in_buffer =	serial_chars_in_buffer,
	.read_proc =		serial_read_proc,
};


static int __init usb_serial_init(void)
{
	int i;
	int result;

	/* Initalize our global data */
	for (i = 0; i < SERIAL_TTY_MINORS; ++i) {
		serial_table[i] = NULL;
	}

	/* register the tty driver */
	serial_tty_driver.init_termios          = tty_std_termios;
	serial_tty_driver.init_termios.c_cflag  = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	if (tty_register_driver (&serial_tty_driver)) {
		err("%s - failed to register tty driver", __FUNCTION__);
		return -1;
	}

	/* register the USB driver */
	result = usb_register(&usb_serial_driver);
	if (result < 0) {
		tty_unregister_driver(&serial_tty_driver);
		err("usb_register failed for the usb-serial driver. Error number %d", result);
		return -1;
	}

#ifdef CONFIG_USB_SERIAL_GENERIC
	generic_device_ids[0].idVendor = vendor;
	generic_device_ids[0].idProduct = product;
	generic_device_ids[0].match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT;
	/* register our generic driver with ourselves */
	usb_serial_register (&generic_device);
#endif

	info(DRIVER_DESC " " DRIVER_VERSION);

	return 0;
}


static void __exit usb_serial_exit(void)
{

#ifdef CONFIG_USB_SERIAL_GENERIC
	/* remove our generic driver */
	usb_serial_deregister (&generic_device);
#endif
	
	usb_deregister(&usb_serial_driver);
	tty_unregister_driver(&serial_tty_driver);
}


module_init(usb_serial_init);
module_exit(usb_serial_exit);


int usb_serial_register(struct usb_serial_device_type *new_device)
{
	/* Add this device to our list of devices */
	list_add(&new_device->driver_list, &usb_serial_driver_list);

	info ("USB Serial support registered for %s", new_device->name);

	usb_scan_devices();

	return 0;
}


void usb_serial_deregister(struct usb_serial_device_type *device)
{
	struct usb_serial *serial;
	int i;

	info("USB Serial deregistering driver %s", device->name);

	/* clear out the serial_table if the device is attached to a port */
	for(i = 0; i < SERIAL_TTY_MINORS; ++i) {
		serial = serial_table[i];
		if ((serial != NULL) && (serial->type == device)) {
			usb_driver_release_interface (&usb_serial_driver, serial->interface);
			usb_serial_disconnect (NULL, serial);
		}
	}

	list_del(&device->driver_list);
}



/* If the usb-serial core is built into the core, the usb-serial drivers
   need these symbols to load properly as modules. */
EXPORT_SYMBOL(usb_serial_register);
EXPORT_SYMBOL(usb_serial_deregister);
#ifdef USES_EZUSB_FUNCTIONS
	EXPORT_SYMBOL(ezusb_writememory);
	EXPORT_SYMBOL(ezusb_set_reset);
#endif


/* Module information */
MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug enabled or not");

#ifdef CONFIG_USB_SERIAL_GENERIC
MODULE_PARM(vendor, "h");
MODULE_PARM_DESC(vendor, "User specified USB idVendor");

MODULE_PARM(product, "h");
MODULE_PARM_DESC(product, "User specified USB idProduct");
#endif
