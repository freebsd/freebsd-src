/*
 * IEEE 1284.3 Parallel port daisy chain and multiplexor code
 * 
 * Copyright (C) 1999, 2000  Tim Waugh <tim@cyberelk.demon.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * ??-12-1998: Initial implementation.
 * 31-01-1999: Make port-cloning transparent.
 * 13-02-1999: Move DeviceID technique from parport_probe.
 * 13-03-1999: Get DeviceID from non-IEEE 1284.3 devices too.
 * 22-02-2000: Count devices that are actually detected.
 *
 * Any part of this program may be used in documents licensed under
 * the GNU Free Documentation License, Version 1.1 or any later version
 * published by the Free Software Foundation.
 */

#include <linux/parport.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#undef DEBUG /* undef me for production */

#ifdef DEBUG
#define DPRINTK(stuff...) printk (stuff)
#else
#define DPRINTK(stuff...)
#endif

static struct daisydev {
	struct daisydev *next;
	struct parport *port;
	int daisy;
	int devnum;
} *topology = NULL;

static int numdevs = 0;

/* Forward-declaration of lower-level functions. */
static int mux_present (struct parport *port);
static int num_mux_ports (struct parport *port);
static int select_port (struct parport *port);
static int assign_addrs (struct parport *port);

/* Add a device to the discovered topology. */
static void add_dev (int devnum, struct parport *port, int daisy)
{
	struct daisydev *newdev;
	newdev = kmalloc (sizeof (struct daisydev), GFP_KERNEL);
	if (newdev) {
		newdev->port = port;
		newdev->daisy = daisy;
		newdev->devnum = devnum;
		newdev->next = topology;
		if (!topology || topology->devnum >= devnum)
			topology = newdev;
		else {
			struct daisydev *prev = topology;
			while (prev->next && prev->next->devnum < devnum)
				prev = prev->next;
			newdev->next = prev->next;
			prev->next = newdev;
		}
	}
}

/* Clone a parport (actually, make an alias). */
static struct parport *clone_parport (struct parport *real, int muxport)
{
	struct parport *extra = parport_register_port (real->base,
						       real->irq,
						       real->dma,
						       real->ops);
	if (extra) {
		extra->portnum = real->portnum;
		extra->physport = real;
		extra->muxport = muxport;
	}

	return extra;
}

/* Discover the IEEE1284.3 topology on a port -- muxes and daisy chains.
 * Return value is number of devices actually detected. */
int parport_daisy_init (struct parport *port)
{
	int detected = 0;
	char *deviceid;
	static const char *th[] = { /*0*/"th", "st", "nd", "rd", "th" };
	int num_ports;
	int i;

	/* Because this is called before any other devices exist,
	 * we don't have to claim exclusive access.  */

	/* If mux present on normal port, need to create new
	 * parports for each extra port. */
	if (port->muxport < 0 && mux_present (port) &&
	    /* don't be fooled: a mux must have 2 or 4 ports. */
	    ((num_ports = num_mux_ports (port)) == 2 || num_ports == 4)) {
		/* Leave original as port zero. */
		port->muxport = 0;
		printk (KERN_INFO
			"%s: 1st (default) port of %d-way multiplexor\n",
			port->name, num_ports);
		for (i = 1; i < num_ports; i++) {
			/* Clone the port. */
			struct parport *extra = clone_parport (port, i);
			if (!extra) {
				if (signal_pending (current))
					break;

				schedule ();
				continue;
			}

			printk (KERN_INFO
				"%s: %d%s port of %d-way multiplexor on %s\n",
				extra->name, i + 1, th[i + 1], num_ports,
				port->name);

			/* Analyse that port too.  We won't recurse
			   forever because of the 'port->muxport < 0'
			   test above. */
			parport_announce_port (extra);
		}
	}

	if (port->muxport >= 0)
		select_port (port);

	parport_daisy_deselect_all (port);
	detected += assign_addrs (port);

	/* Count the potential legacy device at the end. */
	add_dev (numdevs++, port, -1);

	/* Find out the legacy device's IEEE 1284 device ID. */
	deviceid = kmalloc (1000, GFP_KERNEL);
	if (deviceid) {
		if (parport_device_id (numdevs - 1, deviceid, 1000) > 2)
			detected++;

		kfree (deviceid);
	}

	return detected;
}

/* Forget about devices on a physical port. */
void parport_daisy_fini (struct parport *port)
{
	struct daisydev *dev, *prev = topology;
	while (prev && prev->port == port) {
		topology = topology->next;
		kfree (prev);
		prev = topology;
	}

	while (prev) {
		dev = prev->next;
		if (dev && dev->port == port) {
			prev->next = dev->next;
			kfree (dev);
		}
		prev = prev->next;
	}

	/* Gaps in the numbering could be handled better.  How should
           someone enumerate through all IEEE1284.3 devices in the
           topology?. */
	if (!topology) numdevs = 0;
	return;
}

/**
 *	parport_open - find a device by canonical device number
 *	@devnum: canonical device number
 *	@name: name to associate with the device
 *	@pf: preemption callback
 *	@kf: kick callback
 *	@irqf: interrupt handler
 *	@flags: registration flags
 *	@handle: driver data
 *
 *	This function is similar to parport_register_device(), except
 *	that it locates a device by its number rather than by the port
 *	it is attached to.  See parport_find_device() and
 *	parport_find_class().
 *
 *	All parameters except for @devnum are the same as for
 *	parport_register_device().  The return value is the same as
 *	for parport_register_device().
 **/

struct pardevice *parport_open (int devnum, const char *name,
				int (*pf) (void *), void (*kf) (void *),
				void (*irqf) (int, void *, struct pt_regs *),
				int flags, void *handle)
{
	struct parport *port = parport_enumerate ();
	struct pardevice *dev;
	int portnum;
	int muxnum;
	int daisynum;

	if (parport_device_coords (devnum,  &portnum, &muxnum, &daisynum))
		return NULL;

	while (port && ((port->portnum != portnum) ||
			(port->muxport != muxnum)))
		port = port->next;

	if (!port)
		/* No corresponding parport. */
		return NULL;

	dev = parport_register_device (port, name, pf, kf,
				       irqf, flags, handle);
	if (dev)
		dev->daisy = daisynum;

	/* Check that there really is a device to select. */
	if (daisynum >= 0) {
		int selected;
		parport_claim_or_block (dev);
		selected = port->daisy;
		parport_release (dev);

		if (selected != port->daisy) {
			/* No corresponding device. */
			parport_unregister_device (dev);
			return NULL;
		}
	}

	return dev;
}

/**
 *	parport_close - close a device opened with parport_open()
 *	@dev: device to close
 *
 *	This is to parport_open() as parport_unregister_device() is to
 *	parport_register_device().
 **/

void parport_close (struct pardevice *dev)
{
	parport_unregister_device (dev);
}

/**
 *	parport_device_num - convert device coordinates
 *	@parport: parallel port number
 *	@mux: multiplexor port number (-1 for no multiplexor)
 *	@daisy: daisy chain address (-1 for no daisy chain address)
 *
 *	This tries to locate a device on the given parallel port,
 *	multiplexor port and daisy chain address, and returns its
 *	device number or -NXIO if no device with those coordinates
 *	exists.
 **/

int parport_device_num (int parport, int mux, int daisy)
{
	struct daisydev *dev = topology;

	while (dev && dev->port->portnum != parport &&
	       dev->port->muxport != mux && dev->daisy != daisy)
		dev = dev->next;

	if (!dev)
		return -ENXIO;

	return dev->devnum;
}

/**
 *	parport_device_coords - convert canonical device number
 *	@devnum: device number
 *	@parport: pointer to storage for parallel port number
 *	@mux: pointer to storage for multiplexor port number
 *	@daisy: pointer to storage for daisy chain address
 *
 *	This function converts a device number into its coordinates in
 *	terms of which parallel port in the system it is attached to,
 *	which multiplexor port it is attached to if there is a
 *	multiplexor on that port, and which daisy chain address it has
 *	if it is in a daisy chain.
 *
 *	The caller must allocate storage for @parport, @mux, and
 *	@daisy.
 *
 *	If there is no device with the specified device number, -ENXIO
 *	is returned.  Otherwise, the values pointed to by @parport,
 *	@mux, and @daisy are set to the coordinates of the device,
 *	with -1 for coordinates with no value.
 *
 *	This function is not actually very useful, but this interface
 *	was suggested by IEEE 1284.3.
 **/

int parport_device_coords (int devnum, int *parport, int *mux, int *daisy)
{
	struct daisydev *dev = topology;

	while (dev && dev->devnum != devnum)
		dev = dev->next;

	if (!dev)
		return -ENXIO;

	if (parport) *parport = dev->port->portnum;
	if (mux) *mux = dev->port->muxport;
	if (daisy) *daisy = dev->daisy;
	return 0;
}

/* Send a daisy-chain-style CPP command packet. */
static int cpp_daisy (struct parport *port, int cmd)
{
	unsigned char s;

	parport_data_forward (port);
	parport_write_data (port, 0xaa); udelay (2);
	parport_write_data (port, 0x55); udelay (2);
	parport_write_data (port, 0x00); udelay (2);
	parport_write_data (port, 0xff); udelay (2);
	s = parport_read_status (port) & (PARPORT_STATUS_BUSY
					  | PARPORT_STATUS_PAPEROUT
					  | PARPORT_STATUS_SELECT
					  | PARPORT_STATUS_ERROR);
	if (s != (PARPORT_STATUS_BUSY
		  | PARPORT_STATUS_PAPEROUT
		  | PARPORT_STATUS_SELECT
		  | PARPORT_STATUS_ERROR)) {
		DPRINTK (KERN_DEBUG "%s: cpp_daisy: aa5500ff(%02x)\n",
			 port->name, s);
		return -ENXIO;
	}

	parport_write_data (port, 0x87); udelay (2);
	s = parport_read_status (port) & (PARPORT_STATUS_BUSY
					  | PARPORT_STATUS_PAPEROUT
					  | PARPORT_STATUS_SELECT
					  | PARPORT_STATUS_ERROR);
	if (s != (PARPORT_STATUS_SELECT | PARPORT_STATUS_ERROR)) {
		DPRINTK (KERN_DEBUG "%s: cpp_daisy: aa5500ff87(%02x)\n",
			 port->name, s);
		return -ENXIO;
	}

	parport_write_data (port, 0x78); udelay (2);
	parport_write_data (port, cmd); udelay (2);
	parport_frob_control (port,
			      PARPORT_CONTROL_STROBE,
			      PARPORT_CONTROL_STROBE);
	udelay (1);
	parport_frob_control (port, PARPORT_CONTROL_STROBE, 0);
	udelay (1);
	s = parport_read_status (port);
	parport_write_data (port, 0xff); udelay (2);

	return s;
}

/* Send a mux-style CPP command packet. */
static int cpp_mux (struct parport *port, int cmd)
{
	unsigned char s;
	int rc;

	parport_data_forward (port);
	parport_write_data (port, 0xaa); udelay (2);
	parport_write_data (port, 0x55); udelay (2);
	parport_write_data (port, 0xf0); udelay (2);
	parport_write_data (port, 0x0f); udelay (2);
	parport_write_data (port, 0x52); udelay (2);
	parport_write_data (port, 0xad); udelay (2);
	parport_write_data (port, cmd); udelay (2);

	s = parport_read_status (port);
	if (!(s & PARPORT_STATUS_ACK)) {
		DPRINTK (KERN_DEBUG "%s: cpp_mux: aa55f00f52ad%02x(%02x)\n",
			 port->name, cmd, s);
		return -EIO;
	}

	rc = (((s & PARPORT_STATUS_SELECT   ? 1 : 0) << 0) |
	      ((s & PARPORT_STATUS_PAPEROUT ? 1 : 0) << 1) |
	      ((s & PARPORT_STATUS_BUSY     ? 0 : 1) << 2) |
	      ((s & PARPORT_STATUS_ERROR    ? 0 : 1) << 3));

	return rc;
}

void parport_daisy_deselect_all (struct parport *port)
{
	cpp_daisy (port, 0x30);
}

int parport_daisy_select (struct parport *port, int daisy, int mode)
{
	switch (mode)
	{
		// For these modes we should switch to EPP mode:
		case IEEE1284_MODE_EPP:
		case IEEE1284_MODE_EPPSL:
		case IEEE1284_MODE_EPPSWE:
			return (cpp_daisy (port, 0x20 + daisy) &
				PARPORT_STATUS_ERROR);

		// For these modes we should switch to ECP mode:
		case IEEE1284_MODE_ECP:
		case IEEE1284_MODE_ECPRLE:
		case IEEE1284_MODE_ECPSWE: 
			return (cpp_daisy (port, 0xd0 + daisy) &
				PARPORT_STATUS_ERROR);

		// Nothing was told for BECP in Daisy chain specification.
		// May be it's wise to use ECP?
		case IEEE1284_MODE_BECP:
		// Others use compat mode
		case IEEE1284_MODE_NIBBLE:
		case IEEE1284_MODE_BYTE:
		case IEEE1284_MODE_COMPAT:
		default:
			return (cpp_daisy (port, 0xe0 + daisy) &
				PARPORT_STATUS_ERROR);
	}
}

static int mux_present (struct parport *port)
{
	return cpp_mux (port, 0x51) == 3;
}

static int num_mux_ports (struct parport *port)
{
	return cpp_mux (port, 0x58);
}

static int select_port (struct parport *port)
{
	int muxport = port->muxport;
	return cpp_mux (port, 0x60 + muxport) == muxport;
}

static int assign_addrs (struct parport *port)
{
	unsigned char s, last_dev;
	unsigned char daisy;
	int thisdev = numdevs;
	int detected;
	char *deviceid;

	parport_data_forward (port);
	parport_write_data (port, 0xaa); udelay (2);
	parport_write_data (port, 0x55); udelay (2);
	parport_write_data (port, 0x00); udelay (2);
	parport_write_data (port, 0xff); udelay (2);
	s = parport_read_status (port) & (PARPORT_STATUS_BUSY
					  | PARPORT_STATUS_PAPEROUT
					  | PARPORT_STATUS_SELECT
					  | PARPORT_STATUS_ERROR);
	if (s != (PARPORT_STATUS_BUSY
		  | PARPORT_STATUS_PAPEROUT
		  | PARPORT_STATUS_SELECT
		  | PARPORT_STATUS_ERROR)) {
		DPRINTK (KERN_DEBUG "%s: assign_addrs: aa5500ff(%02x)\n",
			 port->name, s);
		return 0;
	}

	parport_write_data (port, 0x87); udelay (2);
	s = parport_read_status (port) & (PARPORT_STATUS_BUSY
					  | PARPORT_STATUS_PAPEROUT
					  | PARPORT_STATUS_SELECT
					  | PARPORT_STATUS_ERROR);
	if (s != (PARPORT_STATUS_SELECT | PARPORT_STATUS_ERROR)) {
		DPRINTK (KERN_DEBUG "%s: assign_addrs: aa5500ff87(%02x)\n",
			 port->name, s);
		return 0;
	}

	parport_write_data (port, 0x78); udelay (2);
	last_dev = 0; /* We've just been speaking to a device, so we
			 know there must be at least _one_ out there. */

	for (daisy = 0; daisy < 4; daisy++) {
		parport_write_data (port, daisy);
		udelay (2);
		parport_frob_control (port,
				      PARPORT_CONTROL_STROBE,
				      PARPORT_CONTROL_STROBE);
		udelay (1);
		parport_frob_control (port, PARPORT_CONTROL_STROBE, 0);
		udelay (1);

		if (last_dev)
			/* No more devices. */
			break;

		last_dev = !(parport_read_status (port)
			     & PARPORT_STATUS_BUSY);

		add_dev (numdevs++, port, daisy);
	}

	parport_write_data (port, 0xff); udelay (2);
	detected = numdevs - thisdev;
	DPRINTK (KERN_DEBUG "%s: Found %d daisy-chained devices\n", port->name,
		 detected);

	/* Ask the new devices to introduce themselves. */
	deviceid = kmalloc (1000, GFP_KERNEL);
	if (!deviceid) return 0;

	for (daisy = 0; thisdev < numdevs; thisdev++, daisy++)
		parport_device_id (thisdev, deviceid, 1000);

	kfree (deviceid);
	return detected;
}

/* Find a device with a particular manufacturer and model string,
   starting from a given device number.  Like the PCI equivalent,
   'from' itself is skipped. */

/**
 *	parport_find_device - find a specific device
 *	@mfg: required manufacturer string
 *	@mdl: required model string
 *	@from: previous device number found in search, or %NULL for
 *	       new search
 *
 *	This walks through the list of parallel port devices looking
 *	for a device whose 'MFG' string matches @mfg and whose 'MDL'
 *	string matches @mdl in their IEEE 1284 Device ID.
 *
 *	When a device is found matching those requirements, its device
 *	number is returned; if there is no matching device, a negative
 *	value is returned.
 *
 *	A new search it initiated by passing %NULL as the @from
 *	argument.  If @from is not %NULL, the search continues from
 *	that device.
 **/

int parport_find_device (const char *mfg, const char *mdl, int from)
{
	struct daisydev *d = topology; /* sorted by devnum */

	/* Find where to start. */
	while (d && d->devnum <= from)
		d = d->next;

	/* Search. */
	while (d) {
		struct parport_device_info *info;
		info = &d->port->probe_info[1 + d->daisy];
		if ((!mfg || !strcmp (mfg, info->mfr)) &&
		    (!mdl || !strcmp (mdl, info->model)))
			break;

		d = d->next;
	}

	if (d)
		return d->devnum;

	return -1;
}

/**
 *	parport_find_class - find a device in a specified class
 *	@cls: required class
 *	@from: previous device number found in search, or %NULL for
 *	       new search
 *
 *	This walks through the list of parallel port devices looking
 *	for a device whose 'CLS' string matches @cls in their IEEE
 *	1284 Device ID.
 *
 *	When a device is found matching those requirements, its device
 *	number is returned; if there is no matching device, a negative
 *	value is returned.
 *
 *	A new search it initiated by passing %NULL as the @from
 *	argument.  If @from is not %NULL, the search continues from
 *	that device.
 **/

int parport_find_class (parport_device_class cls, int from)
{
	struct daisydev *d = topology; /* sorted by devnum */

	/* Find where to start. */
	while (d && d->devnum <= from)
		d = d->next;

	/* Search. */
	while (d && d->port->probe_info[1 + d->daisy].class != cls)
		d = d->next;

	if (d)
		return d->devnum;

	return -1;
}
