/*
 * drivers/usb/usb.c
 *
 * (C) Copyright Linus Torvalds 1999
 * (C) Copyright Johannes Erdfelt 1999-2001
 * (C) Copyright Andreas Gal 1999
 * (C) Copyright Gregory P. Smith 1999
 * (C) Copyright Deti Fliegl 1999 (new USB architecture)
 * (C) Copyright Randy Dunlap 2000
 * (C) Copyright David Brownell 2000 (kernel hotplug, usb_device_id)
 * (C) Copyright Yggdrasil Computing, Inc. 2000
 *     (usb_device_id matching changes by Adam J. Richter)
 *
 * NOTE! This is not actually a driver at all, rather this is
 * just a collection of helper routines that implement the
 * generic USB things that the real drivers can use..
 *
 * Think of this as a "USB library" rather than anything else.
 * It should be considered a slave, with no callbacks. Callbacks
 * are evil.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/interrupt.h>  /* for in_interrupt() */
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/spinlock.h>

#ifdef CONFIG_USB_DEBUG
	#define DEBUG
#else
	#undef DEBUG
#endif
#include <linux/usb.h>

#include "hcd.h"

static const int usb_bandwidth_option =
#ifdef CONFIG_USB_BANDWIDTH
				1;
#else
				0;
#endif

extern int  usb_hub_init(void);
extern void usb_hub_cleanup(void);

/*
 * Prototypes for the device driver probing/loading functions
 */
static void usb_find_drivers(struct usb_device *);
static int  usb_find_interface_driver(struct usb_device *, unsigned int);
static void usb_check_support(struct usb_device *);

/*
 * We have a per-interface "registered driver" list.
 */
LIST_HEAD(usb_driver_list);
LIST_HEAD(usb_bus_list);
struct semaphore usb_bus_list_lock;

devfs_handle_t usb_devfs_handle;	/* /dev/usb dir. */

static struct usb_busmap busmap;

static struct usb_driver *usb_minors[16];

/**
 *	usb_register - register a USB driver
 *	@new_driver: USB operations for the driver
 *
 *	Registers a USB driver with the USB core.  The list of unattached
 *	interfaces will be rescanned whenever a new driver is added, allowing
 *	the new driver to attach to any recognized devices.
 *	Returns a negative error code on failure and 0 on success.
 */
int usb_register(struct usb_driver *new_driver)
{
	if (new_driver->fops != NULL) {
		if (usb_minors[new_driver->minor/16]) {
			 err("error registering %s driver", new_driver->name);
			return -EINVAL;
		}
		usb_minors[new_driver->minor/16] = new_driver;
	}

	info("registered new driver %s", new_driver->name);

	init_MUTEX(&new_driver->serialize);

	/* Add it to the list of known drivers */
	list_add_tail(&new_driver->driver_list, &usb_driver_list);

	usb_scan_devices();

	return 0;
}

/**
 *	usb_scan_devices - scans all unclaimed USB interfaces
 *
 *	Goes through all unclaimed USB interfaces, and offers them to all
 *	registered USB drivers through the 'probe' function.
 *	This will automatically be called after usb_register is called.
 *	It is called by some of the USB subsystems after one of their subdrivers
 *	are registered.
 */
void usb_scan_devices(void)
{
	struct list_head *tmp;

	down (&usb_bus_list_lock);
	tmp = usb_bus_list.next;
	while (tmp != &usb_bus_list) {
		struct usb_bus *bus = list_entry(tmp,struct usb_bus, bus_list);

		tmp = tmp->next;
		usb_check_support(bus->root_hub);
	}
	up (&usb_bus_list_lock);
}

/*
 * This function is part of a depth-first search down the device tree,
 * removing any instances of a device driver.
 */
static void usb_drivers_purge(struct usb_driver *driver,struct usb_device *dev)
{
	int i;

	if (!dev) {
		err("null device being purged!!!");
		return;
	}

	for (i=0; i<USB_MAXCHILDREN; i++)
		if (dev->children[i])
			usb_drivers_purge(driver, dev->children[i]);

	if (!dev->actconfig)
		return;
			
	for (i = 0; i < dev->actconfig->bNumInterfaces; i++) {
		struct usb_interface *interface = &dev->actconfig->interface[i];
		
		if (interface->driver == driver) {
			down(&driver->serialize);
			driver->disconnect(dev, interface->private_data);
			up(&driver->serialize);
			/* if driver->disconnect didn't release the interface */
			if (interface->driver)
				usb_driver_release_interface(driver, interface);
			/*
			 * This will go through the list looking for another
			 * driver that can handle the device
			 */
			usb_find_interface_driver(dev, i);
		}
	}
}

/**
 *	usb_deregister - unregister a USB driver
 *	@driver: USB operations of the driver to unregister
 *
 *	Unlinks the specified driver from the internal USB driver list.
 */
void usb_deregister(struct usb_driver *driver)
{
	struct list_head *tmp;

	info("deregistering driver %s", driver->name);
	if (driver->fops != NULL)
		usb_minors[driver->minor/16] = NULL;

	/*
	 * first we remove the driver, to be sure it doesn't get used by
	 * another thread while we are stepping through removing entries
	 */
	list_del(&driver->driver_list);

	down (&usb_bus_list_lock);
	tmp = usb_bus_list.next;
	while (tmp != &usb_bus_list) {
		struct usb_bus *bus = list_entry(tmp,struct usb_bus,bus_list);

		tmp = tmp->next;
		usb_drivers_purge(driver, bus->root_hub);
	}
	up (&usb_bus_list_lock);
}

int usb_ifnum_to_ifpos(struct usb_device *dev, unsigned ifnum)
{
	int i;

	for (i = 0; i < dev->actconfig->bNumInterfaces; i++)
		if (dev->actconfig->interface[i].altsetting[0].bInterfaceNumber == ifnum)
			return i;

	return -EINVAL;
}

struct usb_interface *usb_ifnum_to_if(struct usb_device *dev, unsigned ifnum)
{
	int i;

	for (i = 0; i < dev->actconfig->bNumInterfaces; i++)
		if (dev->actconfig->interface[i].altsetting[0].bInterfaceNumber == ifnum)
			return &dev->actconfig->interface[i];

	return NULL;
}

struct usb_endpoint_descriptor *usb_epnum_to_ep_desc(struct usb_device *dev, unsigned epnum)
{
	int i, j, k;

	for (i = 0; i < dev->actconfig->bNumInterfaces; i++)
		for (j = 0; j < dev->actconfig->interface[i].num_altsetting; j++)
			for (k = 0; k < dev->actconfig->interface[i].altsetting[j].bNumEndpoints; k++)
				if (epnum == dev->actconfig->interface[i].altsetting[j].endpoint[k].bEndpointAddress)
					return &dev->actconfig->interface[i].altsetting[j].endpoint[k];

	return NULL;
}

/*
 * usb_calc_bus_time - approximate periodic transaction time in nanoseconds
 * @speed: from dev->speed; USB_SPEED_{LOW,FULL,HIGH}
 * @is_input: true iff the transaction sends data to the host
 * @isoc: true for isochronous transactions, false for interrupt ones
 * @bytecount: how many bytes in the transaction.
 *
 * Returns approximate bus time in nanoseconds for a periodic transaction.
 * See USB 2.0 spec section 5.11.3; only periodic transfers need to be
 * scheduled in software, this function is only used for such scheduling.
 */
long usb_calc_bus_time (int speed, int is_input, int isoc, int bytecount)
{
	unsigned long	tmp;

	switch (speed) {
	case USB_SPEED_LOW: 	/* INTR only */
		if (is_input) {
			tmp = (67667L * (31L + 10L * BitTime (bytecount))) / 1000L;
			return (64060L + (2 * BW_HUB_LS_SETUP) + BW_HOST_DELAY + tmp);
		} else {
			tmp = (66700L * (31L + 10L * BitTime (bytecount))) / 1000L;
			return (64107L + (2 * BW_HUB_LS_SETUP) + BW_HOST_DELAY + tmp);
		}
	case USB_SPEED_FULL:	/* ISOC or INTR */
		if (isoc) {
			tmp = (8354L * (31L + 10L * BitTime (bytecount))) / 1000L;
			return (((is_input) ? 7268L : 6265L) + BW_HOST_DELAY + tmp);
		} else {
			tmp = (8354L * (31L + 10L * BitTime (bytecount))) / 1000L;
			return (9107L + BW_HOST_DELAY + tmp);
		}
	case USB_SPEED_HIGH:	/* ISOC or INTR */
		// FIXME adjust for input vs output
		if (isoc)
			tmp = HS_USECS (bytecount);
		else
			tmp = HS_USECS_ISO (bytecount);
		return tmp;
	default:
		dbg ("bogus device speed!");
		return -1;
	}
}


/*
 * usb_check_bandwidth():
 *
 * old_alloc is from host_controller->bandwidth_allocated in microseconds;
 * bustime is from calc_bus_time(), but converted to microseconds.
 *
 * returns <bustime in us> if successful,
 * or USB_ST_BANDWIDTH_ERROR if bandwidth request fails.
 *
 * FIXME:
 * This initial implementation does not use Endpoint.bInterval
 * in managing bandwidth allocation.
 * It probably needs to be expanded to use Endpoint.bInterval.
 * This can be done as a later enhancement (correction).
 * This will also probably require some kind of
 * frame allocation tracking...meaning, for example,
 * that if multiple drivers request interrupts every 10 USB frames,
 * they don't all have to be allocated at
 * frame numbers N, N+10, N+20, etc.  Some of them could be at
 * N+11, N+21, N+31, etc., and others at
 * N+12, N+22, N+32, etc.
 * However, this first cut at USB bandwidth allocation does not
 * contain any frame allocation tracking.
 */
int usb_check_bandwidth (struct usb_device *dev, struct urb *urb)
{
	int		new_alloc;
	int		old_alloc = dev->bus->bandwidth_allocated;
	unsigned int	pipe = urb->pipe;
	long		bustime;

	bustime = usb_calc_bus_time (dev->speed, usb_pipein(pipe),
			usb_pipeisoc(pipe), usb_maxpacket(dev, pipe, usb_pipeout(pipe)));
	if (usb_pipeisoc(pipe))
		bustime = NS_TO_US(bustime) / urb->number_of_packets;
	else
		bustime = NS_TO_US(bustime);

	new_alloc = old_alloc + (int)bustime;
		/* what new total allocated bus time would be */

	if (new_alloc > FRAME_TIME_MAX_USECS_ALLOC)
		dbg("usb-check-bandwidth %sFAILED: was %u, would be %u, bustime = %ld us",
			usb_bandwidth_option ? "" : "would have ",
			old_alloc, new_alloc, bustime);

	if (!usb_bandwidth_option)	/* don't enforce it */
		return (bustime);
	return (new_alloc <= FRAME_TIME_MAX_USECS_ALLOC) ? bustime : USB_ST_BANDWIDTH_ERROR;
}

void usb_claim_bandwidth (struct usb_device *dev, struct urb *urb, int bustime, int isoc)
{
	dev->bus->bandwidth_allocated += bustime;
	if (isoc)
		dev->bus->bandwidth_isoc_reqs++;
	else
		dev->bus->bandwidth_int_reqs++;
	urb->bandwidth = bustime;

#ifdef USB_BANDWIDTH_MESSAGES
	dbg("bandwidth alloc increased by %d to %d for %d requesters",
		bustime,
		dev->bus->bandwidth_allocated,
		dev->bus->bandwidth_int_reqs + dev->bus->bandwidth_isoc_reqs);
#endif
}

/*
 * usb_release_bandwidth():
 *
 * called to release a pipe's bandwidth (in microseconds)
 */
void usb_release_bandwidth(struct usb_device *dev, struct urb *urb, int isoc)
{
	dev->bus->bandwidth_allocated -= urb->bandwidth;
	if (isoc)
		dev->bus->bandwidth_isoc_reqs--;
	else
		dev->bus->bandwidth_int_reqs--;

#ifdef USB_BANDWIDTH_MESSAGES
	dbg("bandwidth alloc reduced by %d to %d for %d requesters",
		urb->bandwidth,
		dev->bus->bandwidth_allocated,
		dev->bus->bandwidth_int_reqs + dev->bus->bandwidth_isoc_reqs);
#endif
	urb->bandwidth = 0;
}

static void usb_bus_get(struct usb_bus *bus)
{
	atomic_inc(&bus->refcnt);
}

static void usb_bus_put(struct usb_bus *bus)
{
	if (atomic_dec_and_test(&bus->refcnt))
		kfree(bus);
}

/**
 *	usb_alloc_bus - creates a new USB host controller structure
 *	@op: pointer to a struct usb_operations that this bus structure should use
 *
 *	Creates a USB host controller bus structure with the specified 
 *	usb_operations and initializes all the necessary internal objects.
 *	(For use only by USB Host Controller Drivers.)
 *
 *	If no memory is available, NULL is returned.
 *
 *	The caller should call usb_free_bus() when it is finished with the structure.
 */
struct usb_bus *usb_alloc_bus(struct usb_operations *op)
{
	struct usb_bus *bus;

	bus = kmalloc(sizeof(*bus), GFP_KERNEL);
	if (!bus)
		return NULL;

	memset(&bus->devmap, 0, sizeof(struct usb_devmap));

#ifdef DEVNUM_ROUND_ROBIN
	bus->devnum_next = 1;
#endif /* DEVNUM_ROUND_ROBIN */

	bus->op = op;
	bus->root_hub = NULL;
	bus->hcpriv = NULL;
	bus->busnum = -1;
	bus->bandwidth_allocated = 0;
	bus->bandwidth_int_reqs  = 0;
	bus->bandwidth_isoc_reqs = 0;

	INIT_LIST_HEAD(&bus->bus_list);
	INIT_LIST_HEAD(&bus->inodes);

	atomic_set(&bus->refcnt, 1);

	return bus;
}

/**
 *	usb_free_bus - frees the memory used by a bus structure
 *	@bus: pointer to the bus to free
 *
 *	(For use only by USB Host Controller Drivers.)
 */
void usb_free_bus(struct usb_bus *bus)
{
	if (!bus)
		return;

	usb_bus_put(bus);
}

/**
 *	usb_register_bus - registers the USB host controller with the usb core
 *	@bus: pointer to the bus to register
 *
 *	(For use only by USB Host Controller Drivers.)
 */
void usb_register_bus(struct usb_bus *bus)
{
	int busnum;

	down (&usb_bus_list_lock);
	busnum = find_next_zero_bit(busmap.busmap, USB_MAXBUS, 1);
	if (busnum < USB_MAXBUS) {
		set_bit(busnum, busmap.busmap);
		bus->busnum = busnum;
	} else
		warn("too many buses");

	usb_bus_get(bus);

	/* Add it to the list of buses */
	list_add(&bus->bus_list, &usb_bus_list);
	up (&usb_bus_list_lock);

	usbdevfs_add_bus(bus);

	info("new USB bus registered, assigned bus number %d", bus->busnum);
}

/**
 *	usb_deregister_bus - deregisters the USB host controller
 *	@bus: pointer to the bus to deregister
 *
 *	(For use only by USB Host Controller Drivers.)
 */
void usb_deregister_bus(struct usb_bus *bus)
{
	info("USB bus %d deregistered", bus->busnum);

	/*
	 * NOTE: make sure that all the devices are removed by the
	 * controller code, as well as having it call this when cleaning
	 * itself up
	 */
	down (&usb_bus_list_lock);
	list_del(&bus->bus_list);
	clear_bit(bus->busnum, busmap.busmap);
	up (&usb_bus_list_lock);

	usbdevfs_remove_bus(bus);

	usb_bus_put(bus);
}

/*
 * This function is for doing a depth-first search for devices which
 * have support, for dynamic loading of driver modules.
 */
static void usb_check_support(struct usb_device *dev)
{
	int i;

	if (!dev) {
		err("null device being checked!!!");
		return;
	}

	for (i=0; i<USB_MAXCHILDREN; i++)
		if (dev->children[i])
			usb_check_support(dev->children[i]);

	if (!dev->actconfig)
		return;

	/* now we check this device */
	if (dev->devnum > 0)
		for (i = 0; i < dev->actconfig->bNumInterfaces; i++)
			usb_find_interface_driver(dev, i);
}


/*
 * This is intended to be used by usb device drivers that need to
 * claim more than one interface on a device at once when probing
 * (audio and acm are good examples).  No device driver should have
 * to mess with the internal usb_interface or usb_device structure
 * members.
 */
void usb_driver_claim_interface(struct usb_driver *driver, struct usb_interface *iface, void* priv)
{
	if (!iface || !driver)
		return;

	dbg("%s driver claimed interface %p", driver->name, iface);

	iface->driver = driver;
	iface->private_data = priv;
} /* usb_driver_claim_interface() */

/*
 * This should be used by drivers to check other interfaces to see if
 * they are available or not.
 */
int usb_interface_claimed(struct usb_interface *iface)
{
	if (!iface)
		return 0;

	return (iface->driver != NULL);
} /* usb_interface_claimed() */

/*
 * This should be used by drivers to release their claimed interfaces
 */
void usb_driver_release_interface(struct usb_driver *driver, struct usb_interface *iface)
{
	/* this should never happen, don't release something that's not ours */
	if (!iface || iface->driver != driver)
		return;

	iface->driver = NULL;
	iface->private_data = NULL;
}


/**
 * usb_match_id - find first usb_device_id matching device or interface
 * @dev: the device whose descriptors are considered when matching
 * @interface: the interface of interest
 * @id: array of usb_device_id structures, terminated by zero entry
 *
 * usb_match_id searches an array of usb_device_id's and returns
 * the first one matching the device or interface, or null.
 * This is used when binding (or rebinding) a driver to an interface.
 * Most USB device drivers will use this indirectly, through the usb core,
 * but some layered driver frameworks use it directly.
 * These device tables are exported with MODULE_DEVICE_TABLE, through
 * modutils and "modules.usbmap", to support the driver loading
 * functionality of USB hotplugging.
 *
 * What Matches:
 *
 * The "match_flags" element in a usb_device_id controls which
 * members are used.  If the corresponding bit is set, the
 * value in the device_id must match its corresponding member
 * in the device or interface descriptor, or else the device_id
 * does not match.
 *
 * "driver_info" is normally used only by device drivers,
 * but you can create a wildcard "matches anything" usb_device_id
 * as a driver's "modules.usbmap" entry if you provide an id with
 * only a nonzero "driver_info" field.  If you do this, the USB device
 * driver's probe() routine should use additional intelligence to
 * decide whether to bind to the specified interface.
 * 
 * What Makes Good usb_device_id Tables:
 *
 * The match algorithm is very simple, so that intelligence in
 * driver selection must come from smart driver id records.
 * Unless you have good reasons to use another selection policy,
 * provide match elements only in related groups, and order match
 * specifiers from specific to general.  Use the macros provided
 * for that purpose if you can.
 *
 * The most specific match specifiers use device descriptor
 * data.  These are commonly used with product-specific matches;
 * the USB_DEVICE macro lets you provide vendor and product IDs,
 * and you can also match against ranges of product revisions.
 * These are widely used for devices with application or vendor
 * specific bDeviceClass values.
 *
 * Matches based on device class/subclass/protocol specifications
 * are slightly more general; use the USB_DEVICE_INFO macro, or
 * its siblings.  These are used with single-function devices
 * where bDeviceClass doesn't specify that each interface has
 * its own class. 
 *
 * Matches based on interface class/subclass/protocol are the
 * most general; they let drivers bind to any interface on a
 * multiple-function device.  Use the USB_INTERFACE_INFO
 * macro, or its siblings, to match class-per-interface style 
 * devices (as recorded in bDeviceClass).
 *  
 * Within those groups, remember that not all combinations are
 * meaningful.  For example, don't give a product version range
 * without vendor and product IDs; or specify a protocol without
 * its associated class and subclass.
 */   
const struct usb_device_id *
usb_match_id(struct usb_device *dev, struct usb_interface *interface,
	     const struct usb_device_id *id)
{
	struct usb_interface_descriptor	*intf = 0;

	/* proc_connectinfo in devio.c may call us with id == NULL. */
	if (id == NULL)
		return NULL;

	/* It is important to check that id->driver_info is nonzero,
	   since an entry that is all zeroes except for a nonzero
	   id->driver_info is the way to create an entry that
	   indicates that the driver want to examine every
	   device and interface. */
	for (; id->idVendor || id->bDeviceClass || id->bInterfaceClass ||
	       id->driver_info; id++) {

		if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
		    id->idVendor != dev->descriptor.idVendor)
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
		    id->idProduct != dev->descriptor.idProduct)
			continue;

		/* No need to test id->bcdDevice_lo != 0, since 0 is never
		   greater than any unsigned number. */
		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO) &&
		    (id->bcdDevice_lo > dev->descriptor.bcdDevice))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI) &&
		    (id->bcdDevice_hi < dev->descriptor.bcdDevice))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS) &&
		    (id->bDeviceClass != dev->descriptor.bDeviceClass))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS) &&
		    (id->bDeviceSubClass!= dev->descriptor.bDeviceSubClass))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL) &&
		    (id->bDeviceProtocol != dev->descriptor.bDeviceProtocol))
			continue;

		intf = &interface->altsetting [interface->act_altsetting];

		if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_CLASS) &&
		    (id->bInterfaceClass != intf->bInterfaceClass))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_SUBCLASS) &&
		    (id->bInterfaceSubClass != intf->bInterfaceSubClass))
		    continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_PROTOCOL) &&
		    (id->bInterfaceProtocol != intf->bInterfaceProtocol))
		    continue;

		return id;
	}

	return NULL;
}

/*
 * This entrypoint gets called for each new device.
 *
 * We now walk the list of registered USB drivers,
 * looking for one that will accept this interface.
 *
 * "New Style" drivers use a table describing the devices and interfaces
 * they handle.  Those tables are available to user mode tools deciding
 * whether to load driver modules for a new device.
 *
 * The probe return value is changed to be a private pointer.  This way
 * the drivers don't have to dig around in our structures to set the
 * private pointer if they only need one interface. 
 *
 * Returns: 0 if a driver accepted the interface, -1 otherwise
 */
static int usb_find_interface_driver(struct usb_device *dev, unsigned ifnum)
{
	struct list_head *tmp;
	struct usb_interface *interface;
	void *private;
	const struct usb_device_id *id;
	struct usb_driver *driver;
	int i;
	
	if ((!dev) || (ifnum >= dev->actconfig->bNumInterfaces)) {
		err("bad find_interface_driver params");
		return -1;
	}

	down(&dev->serialize);

	interface = dev->actconfig->interface + ifnum;

	if (usb_interface_claimed(interface))
		goto out_err;

	private = NULL;
	for (tmp = usb_driver_list.next; tmp != &usb_driver_list;) {
		driver = list_entry(tmp, struct usb_driver, driver_list);
		tmp = tmp->next;

		id = driver->id_table;
		/* new style driver? */
		if (id) {
			for (i = 0; i < interface->num_altsetting; i++) {
			  	interface->act_altsetting = i;
				id = usb_match_id(dev, interface, id);
				if (id) {
					down(&driver->serialize);
					private = driver->probe(dev,ifnum,id);
					up(&driver->serialize);
					if (private != NULL)
						break;
				}
			}

			/* if driver not bound, leave defaults unchanged */
			if (private == NULL)
				interface->act_altsetting = 0;
		} else { /* "old style" driver */
			down(&driver->serialize);
			private = driver->probe(dev, ifnum, NULL);
			up(&driver->serialize);
		}

		/* probe() may have changed the config on us */
		interface = dev->actconfig->interface + ifnum;

		if (private) {
			usb_driver_claim_interface(driver, interface, private);
			up(&dev->serialize);
			return 0;
		}
	}

out_err:
	up(&dev->serialize);
	return -1;
}

/*
 * This simply converts the interface _number_ (as in interface.bInterfaceNumber) and
 * converts it to the interface _position_ (as in dev->actconfig->interface + position)
 * and calls usb_find_interface_driver().
 *
 * Note that the number is the same as the position for all interfaces _except_
 * devices with interfaces not sequentially numbered (e.g., 0, 2, 3, etc).
 */
int usb_find_interface_driver_for_ifnum(struct usb_device *dev, unsigned ifnum)
{
	int ifpos = usb_ifnum_to_ifpos(dev, ifnum);

	if (0 > ifpos)
		return -EINVAL;

	return usb_find_interface_driver(dev, ifpos);
}

#ifdef	CONFIG_HOTPLUG

/*
 * USB hotplugging invokes what /proc/sys/kernel/hotplug says
 * (normally /sbin/hotplug) when USB devices get added or removed.
 *
 * This invokes a user mode policy agent, typically helping to load driver
 * or other modules, configure the device, and more.  Drivers can provide
 * a MODULE_DEVICE_TABLE to help with module loading subtasks.
 *
 * Some synchronization is important: removes can't start processing
 * before the add-device processing completes, and vice versa.  That keeps
 * a stack of USB-related identifiers stable while they're in use.  If we
 * know that agents won't complete after they return (such as by forking
 * a process that completes later), it's enough to just waitpid() for the
 * agent -- as is currently done.
 *
 * The reason: we know we're called either from khubd (the typical case)
 * or from root hub initialization (init, kapmd, modprobe, etc).  In both
 * cases, we know no other thread can recycle our address, since we must
 * already have been serialized enough to prevent that.
 */
static void call_policy_interface (char *verb, struct usb_device *dev, int interface)
{
	char *argv [3], **envp, *buf, *scratch;
	int i = 0, value;

	if (!hotplug_path [0])
		return;
	if (in_interrupt ()) {
		dbg ("In_interrupt");
		return;
	}
	if (!current->fs->root) {
		/* statically linked USB is initted rather early */
		dbg ("call_policy %s, num %d -- no FS yet", verb, dev->devnum);
		return;
	}
	if (dev->devnum < 0) {
		dbg ("device already deleted ??");
		return;
	}
	if (!(envp = (char **) kmalloc (20 * sizeof (char *), GFP_KERNEL))) {
		dbg ("enomem");
		return;
	}
	if (!(buf = kmalloc (256, GFP_KERNEL))) {
		kfree (envp);
		dbg ("enomem2");
		return;
	}

	/* only one standardized param to hotplug command: type */
	argv [0] = hotplug_path;
	argv [1] = "usb";
	argv [2] = 0;

	/* minimal command environment */
	envp [i++] = "HOME=/";
	envp [i++] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";

#ifdef	DEBUG
	/* hint that policy agent should enter no-stdout debug mode */
	envp [i++] = "DEBUG=kernel";
#endif
	/* extensible set of named bus-specific parameters,
	 * supporting multiple driver selection algorithms.
	 */
	scratch = buf;

	/* action:  add, remove */
	envp [i++] = scratch;
	scratch += sprintf (scratch, "ACTION=%s", verb) + 1;

#ifdef	CONFIG_USB_DEVICEFS
	/* If this is available, userspace programs can directly read
	 * all the device descriptors we don't tell them about.  Or
	 * even act as usermode drivers.
	 *
	 * FIXME reduce hardwired intelligence here
	 */
	envp [i++] = "DEVFS=/proc/bus/usb";
	envp [i++] = scratch;
	scratch += sprintf (scratch, "DEVICE=/proc/bus/usb/%03d/%03d",
		dev->bus->busnum, dev->devnum) + 1;
#endif

	/* per-device configuration hacks are common */
	envp [i++] = scratch;
	scratch += sprintf (scratch, "PRODUCT=%x/%x/%x",
		dev->descriptor.idVendor,
		dev->descriptor.idProduct,
		dev->descriptor.bcdDevice) + 1;

	/* class-based driver binding models */
	envp [i++] = scratch;
	scratch += sprintf (scratch, "TYPE=%d/%d/%d",
			    dev->descriptor.bDeviceClass,
			    dev->descriptor.bDeviceSubClass,
			    dev->descriptor.bDeviceProtocol) + 1;
	if (dev->descriptor.bDeviceClass == 0) {
		int alt = dev->actconfig->interface [interface].act_altsetting;

		envp [i++] = scratch;
		scratch += sprintf (scratch, "INTERFACE=%d/%d/%d",
			dev->actconfig->interface [interface].altsetting [alt].bInterfaceClass,
			dev->actconfig->interface [interface].altsetting [alt].bInterfaceSubClass,
			dev->actconfig->interface [interface].altsetting [alt].bInterfaceProtocol)
			+ 1;
	}
	envp [i++] = 0;
	/* assert: (scratch - buf) < sizeof buf */

	/* NOTE: user mode daemons can call the agents too */

	dbg ("kusbd: %s %s %d", argv [0], verb, dev->devnum);
	value = call_usermodehelper (argv [0], argv, envp);
	kfree (buf);
	kfree (envp);
	if (value != 0)
		dbg ("kusbd policy returned 0x%x", value);
}

static void call_policy (char *verb, struct usb_device *dev)
{
	int i;
	for (i = 0; i < dev->actconfig->bNumInterfaces; i++) {
		call_policy_interface (verb, dev, i);
	}
}

#else

static inline void
call_policy (char *verb, struct usb_device *dev)
{ } 

#endif	/* CONFIG_HOTPLUG */


/*
 * This entrypoint gets called for each new device.
 *
 * All interfaces are scanned for matching drivers.
 */
static void usb_find_drivers(struct usb_device *dev)
{
	unsigned ifnum;
	unsigned rejected = 0;
	unsigned claimed = 0;

	for (ifnum = 0; ifnum < dev->actconfig->bNumInterfaces; ifnum++) {
		/* if this interface hasn't already been claimed */
		if (!usb_interface_claimed(dev->actconfig->interface + ifnum)) {
			if (usb_find_interface_driver(dev, ifnum))
				rejected++;
			else
				claimed++;
		}
	}
 
	if (rejected)
		dbg("unhandled interfaces on device");

	if (!claimed) {
		warn("USB device %d (vend/prod 0x%x/0x%x) is not claimed by any active driver.",
			dev->devnum,
			dev->descriptor.idVendor,
			dev->descriptor.idProduct);
#ifdef DEBUG
		usb_show_device(dev);
#endif
	}
}

/*
 * Only HC's should call usb_alloc_dev and usb_free_dev directly
 * Anybody may use usb_inc_dev_use or usb_dec_dev_use
 */
struct usb_device *usb_alloc_dev(struct usb_device *parent, struct usb_bus *bus)
{
	struct usb_device *dev;

	dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	memset(dev, 0, sizeof(*dev));

	usb_bus_get(bus);

	if (!parent)
		dev->devpath [0] = '0';

	dev->bus = bus;
	dev->parent = parent;
	atomic_set(&dev->refcnt, 1);
	INIT_LIST_HEAD(&dev->inodes);
	INIT_LIST_HEAD(&dev->filelist);

	init_MUTEX(&dev->serialize);

	dev->bus->op->allocate(dev);

	return dev;
}

void usb_free_dev(struct usb_device *dev)
{
	if (atomic_dec_and_test(&dev->refcnt)) {
		dev->bus->op->deallocate(dev);
		usb_destroy_configuration(dev);

		usb_bus_put(dev->bus);

		kfree(dev);
	}
}

void usb_inc_dev_use(struct usb_device *dev)
{
	atomic_inc(&dev->refcnt);
}

/* ------------------------------------------------------------------------------------- 
 * New USB Core Functions
 * -------------------------------------------------------------------------------------*/

/**
 *	usb_alloc_urb - creates a new urb for a USB driver to use
 *	@iso_packets: number of iso packets for this urb
 *
 *	Creates an urb for the USB driver to use and returns a pointer to it.
 *	If no memory is available, NULL is returned.
 *
 *	If the driver want to use this urb for interrupt, control, or bulk
 *	endpoints, pass '0' as the number of iso packets.
 *
 *	The driver should call usb_free_urb() when it is finished with the urb.
 */
struct urb *usb_alloc_urb(int iso_packets)
{
	struct urb *urb;

	urb = (struct urb *)kmalloc(sizeof(struct urb) + iso_packets * sizeof(struct iso_packet_descriptor),
			/* pessimize to prevent deadlocks */ GFP_ATOMIC);
	if (!urb) {
		err("alloc_urb: kmalloc failed");
		return NULL;
	}

	memset(urb, 0, sizeof(*urb));

	spin_lock_init(&urb->lock);

	return urb;
}

/**
 *	usb_free_urb - frees the memory used by a urb
 *	@urb: pointer to the urb to free
 *
 *	If an urb is created with a call to usb_create_urb() it should be
 *	cleaned up with a call to usb_free_urb() when the driver is finished
 *	with it.
 */
void usb_free_urb(struct urb* urb)
{
	if (urb)
		kfree(urb);
}
/*-------------------------------------------------------------------*/
int usb_submit_urb(struct urb *urb)
{
	if (urb && urb->dev && urb->dev->bus && urb->dev->bus->op)
		return urb->dev->bus->op->submit_urb(urb);
	else
		return -ENODEV;
}

/*-------------------------------------------------------------------*/
int usb_unlink_urb(struct urb *urb)
{
	if (urb && urb->dev && urb->dev->bus && urb->dev->bus->op)
		return urb->dev->bus->op->unlink_urb(urb);
	else
		return -ENODEV;
}
/*-------------------------------------------------------------------*
 *                     COMPLETION HANDLERS                           *
 *-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*
 * completion handler for compatibility wrappers (sync control/bulk) *
 *-------------------------------------------------------------------*/
static void usb_api_blocking_completion(struct urb *urb)
{
	struct usb_api_data *awd = (struct usb_api_data *)urb->context;

	awd->done = 1;
	wmb();
	wake_up(&awd->wqh);
}

/*-------------------------------------------------------------------*
 *                         COMPATIBILITY STUFF                       *
 *-------------------------------------------------------------------*/

// Starts urb and waits for completion or timeout
static int usb_start_wait_urb(struct urb *urb, int timeout, int* actual_length)
{ 
	DECLARE_WAITQUEUE(wait, current);
	struct usb_api_data awd;
	int status;

	init_waitqueue_head(&awd.wqh); 	
	awd.done = 0;

	set_current_state(TASK_UNINTERRUPTIBLE);
	add_wait_queue(&awd.wqh, &wait);

	urb->context = &awd;
	status = usb_submit_urb(urb);
	if (status) {
		// something went wrong
		usb_free_urb(urb);
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&awd.wqh, &wait);
		return status;
	}

	while (timeout && !awd.done)
	{
		timeout = schedule_timeout(timeout);
		set_current_state(TASK_UNINTERRUPTIBLE);
		rmb();
	}

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&awd.wqh, &wait);

	if (!timeout && !awd.done) {
		if (urb->status != -EINPROGRESS) {	/* No callback?!! */
			printk(KERN_ERR "usb: raced timeout, "
			    "pipe 0x%x status %d time left %d\n",
			    urb->pipe, urb->status, timeout);
			status = urb->status;
		} else {
			printk("usb_control/bulk_msg: timeout\n");
			usb_unlink_urb(urb);  // remove urb safely
			status = -ETIMEDOUT;
		}
	} else
		status = urb->status;

	if (actual_length)
		*actual_length = urb->actual_length;

	usb_free_urb(urb);
  	return status;
}

/*-------------------------------------------------------------------*/
// returns status (negative) or length (positive)
int usb_internal_control_msg(struct usb_device *usb_dev, unsigned int pipe, 
			    struct usb_ctrlrequest *cmd,  void *data, int len, int timeout)
{
	struct urb *urb;
	int retv;
	int length;

	urb = usb_alloc_urb(0);
	if (!urb)
		return -ENOMEM;
  
	FILL_CONTROL_URB(urb, usb_dev, pipe, (unsigned char*)cmd, data, len,
		   usb_api_blocking_completion, 0);

	retv = usb_start_wait_urb(urb, timeout, &length);
	if (retv < 0)
		return retv;
	else
		return length;
}

/**
 *	usb_control_msg - Builds a control urb, sends it off and waits for completion
 *	@dev: pointer to the usb device to send the message to
 *	@pipe: endpoint "pipe" to send the message to
 *	@request: USB message request value
 *	@requesttype: USB message request type value
 *	@value: USB message value
 *	@index: USB message index value
 *	@data: pointer to the data to send
 *	@size: length in bytes of the data to send
 *	@timeout: time to wait for the message to complete before timing out (if 0 the wait is forever)
 *
 *	This function sends a simple control message to a specified endpoint
 *	and waits for the message to complete, or timeout.
 *	
 *	If successful, it returns the number of bytes transferred; 
 *	otherwise, it returns a negative error number.
 *
 *	Don't use this function from within an interrupt context, like a
 *	bottom half handler.  If you need a asyncronous message, or need to send
 *	a message from within interrupt context, use usb_submit_urb()
 */
int usb_control_msg(struct usb_device *dev, unsigned int pipe, __u8 request, __u8 requesttype,
			 __u16 value, __u16 index, void *data, __u16 size, int timeout)
{
	struct usb_ctrlrequest *dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	int ret;
	
	if (!dr)
		return -ENOMEM;

	dr->bRequestType = requesttype;
	dr->bRequest = request;
	dr->wValue = cpu_to_le16p(&value);
	dr->wIndex = cpu_to_le16p(&index);
	dr->wLength = cpu_to_le16p(&size);

	//dbg("usb_control_msg");	

	ret = usb_internal_control_msg(dev, pipe, dr, data, size, timeout);

	kfree(dr);

	return ret;
}


/**
 *	usb_bulk_msg - Builds a bulk urb, sends it off and waits for completion
 *	@usb_dev: pointer to the usb device to send the message to
 *	@pipe: endpoint "pipe" to send the message to
 *	@data: pointer to the data to send
 *	@len: length in bytes of the data to send
 *	@actual_length: pointer to a location to put the actual length transferred in bytes
 *	@timeout: time to wait for the message to complete before timing out (if 0 the wait is forever)
 *
 *	This function sends a simple bulk message to a specified endpoint
 *	and waits for the message to complete, or timeout.
 *	
 *	If successful, it returns 0, otherwise a negative error number.
 *	The number of actual bytes transferred will be stored in the 
 *	actual_length paramater.
 *
 *	Don't use this function from within an interrupt context, like a
 *	bottom half handler.  If you need a asyncronous message, or need to
 *	send a message from within interrupt context, use usb_submit_urb()
 */
int usb_bulk_msg(struct usb_device *usb_dev, unsigned int pipe, 
			void *data, int len, int *actual_length, int timeout)
{
	struct urb *urb;

	if (len < 0)
		return -EINVAL;

	urb=usb_alloc_urb(0);
	if (!urb)
		return -ENOMEM;

	FILL_BULK_URB(urb, usb_dev, pipe, data, len,
		    usb_api_blocking_completion, 0);

	return usb_start_wait_urb(urb,timeout,actual_length);
}

/*
 * usb_get_current_frame_number()
 *
 * returns the current frame number for the parent USB bus/controller
 * of the given USB device.
 */
int usb_get_current_frame_number(struct usb_device *usb_dev)
{
	return usb_dev->bus->op->get_frame_number (usb_dev);
}
/*-------------------------------------------------------------------*/

static int usb_parse_endpoint(struct usb_endpoint_descriptor *endpoint, unsigned char *buffer, int size)
{
	struct usb_descriptor_header *header;
	unsigned char *begin;
	int parsed = 0, len, numskipped;

	header = (struct usb_descriptor_header *)buffer;

	/* Everything should be fine being passed into here, but we sanity */
	/*  check JIC */
	if (header->bLength > size) {
		err("ran out of descriptors parsing");
		return -1;
	}
		
	if (header->bDescriptorType != USB_DT_ENDPOINT) {
		warn("unexpected descriptor 0x%X, expecting endpoint descriptor, type 0x%X",
			endpoint->bDescriptorType, USB_DT_ENDPOINT);
		return parsed;
	}

	if (header->bLength == USB_DT_ENDPOINT_AUDIO_SIZE)
		memcpy(endpoint, buffer, USB_DT_ENDPOINT_AUDIO_SIZE);
	else
		memcpy(endpoint, buffer, USB_DT_ENDPOINT_SIZE);
	
	le16_to_cpus(&endpoint->wMaxPacketSize);

	buffer += header->bLength;
	size -= header->bLength;
	parsed += header->bLength;

	/* Skip over the rest of the Class Specific or Vendor Specific */
	/*  descriptors */
	begin = buffer;
	numskipped = 0;
	while (size >= sizeof(struct usb_descriptor_header)) {
		header = (struct usb_descriptor_header *)buffer;

		if (header->bLength < 2) {
			err("invalid descriptor length of %d", header->bLength);
			return -1;
		}

		/* If we find another "proper" descriptor then we're done  */
		if ((header->bDescriptorType == USB_DT_ENDPOINT) ||
		    (header->bDescriptorType == USB_DT_INTERFACE) ||
		    (header->bDescriptorType == USB_DT_CONFIG) ||
		    (header->bDescriptorType == USB_DT_DEVICE))
			break;

		dbg("skipping descriptor 0x%X",
			header->bDescriptorType);
		numskipped++;

		buffer += header->bLength;
		size -= header->bLength;
		parsed += header->bLength;
	}
	if (numskipped)
		dbg("skipped %d class/vendor specific endpoint descriptors", numskipped);

	/* Copy any unknown descriptors into a storage area for drivers */
	/*  to later parse */
	len = (int)(buffer - begin);
	if (!len) {
		endpoint->extra = NULL;
		endpoint->extralen = 0;
		return parsed;
	}

	endpoint->extra = kmalloc(len, GFP_KERNEL);

	if (!endpoint->extra) {
		err("couldn't allocate memory for endpoint extra descriptors");
		endpoint->extralen = 0;
		return parsed;
	}

	memcpy(endpoint->extra, begin, len);
	endpoint->extralen = len;

	return parsed;
}

static int usb_parse_interface(struct usb_interface *interface, unsigned char *buffer, int size)
{
	int i, len, numskipped, retval, parsed = 0;
	struct usb_descriptor_header *header;
	struct usb_interface_descriptor *ifp;
	unsigned char *begin;

	interface->act_altsetting = 0;
	interface->num_altsetting = 0;
	interface->max_altsetting = USB_ALTSETTINGALLOC;

	interface->altsetting = kmalloc(sizeof(struct usb_interface_descriptor) * interface->max_altsetting, GFP_KERNEL);
	
	if (!interface->altsetting) {
		err("couldn't kmalloc interface->altsetting");
		return -1;
	}

	while (size > 0) {
		if (interface->num_altsetting >= interface->max_altsetting) {
			void *ptr;
			int oldmas;

			oldmas = interface->max_altsetting;
			interface->max_altsetting += USB_ALTSETTINGALLOC;
			if (interface->max_altsetting > USB_MAXALTSETTING) {
				warn("too many alternate settings (max %d)",
					USB_MAXALTSETTING);
				return -1;
			}

			ptr = interface->altsetting;
			interface->altsetting = kmalloc(sizeof(struct usb_interface_descriptor) * interface->max_altsetting, GFP_KERNEL);
			if (!interface->altsetting) {
				err("couldn't kmalloc interface->altsetting");
				interface->altsetting = ptr;
				return -1;
			}
			memcpy(interface->altsetting, ptr, sizeof(struct usb_interface_descriptor) * oldmas);

			kfree(ptr);
		}

		ifp = interface->altsetting + interface->num_altsetting;
		interface->num_altsetting++;

		memcpy(ifp, buffer, USB_DT_INTERFACE_SIZE);

		/* Skip over the interface */
		buffer += ifp->bLength;
		parsed += ifp->bLength;
		size -= ifp->bLength;

		begin = buffer;
		numskipped = 0;

		/* Skip over any interface, class or vendor descriptors */
		while (size >= sizeof(struct usb_descriptor_header)) {
			header = (struct usb_descriptor_header *)buffer;

			if (header->bLength < 2) {
				err("invalid descriptor length of %d", header->bLength);
				return -1;
			}

			/* If we find another "proper" descriptor then we're done  */
			if ((header->bDescriptorType == USB_DT_INTERFACE) ||
			    (header->bDescriptorType == USB_DT_ENDPOINT) ||
			    (header->bDescriptorType == USB_DT_CONFIG) ||
			    (header->bDescriptorType == USB_DT_DEVICE))
				break;

			numskipped++;

			buffer += header->bLength;
			parsed += header->bLength;
			size -= header->bLength;
		}

		if (numskipped)
			dbg("skipped %d class/vendor specific interface descriptors", numskipped);

		/* Copy any unknown descriptors into a storage area for */
		/*  drivers to later parse */
		len = (int)(buffer - begin);
		if (!len) {
			ifp->extra = NULL;
			ifp->extralen = 0;
		} else {
			ifp->extra = kmalloc(len, GFP_KERNEL);

			if (!ifp->extra) {
				err("couldn't allocate memory for interface extra descriptors");
				ifp->extralen = 0;
				return -1;
			}
			memcpy(ifp->extra, begin, len);
			ifp->extralen = len;
		}

		/* Did we hit an unexpected descriptor? */
		header = (struct usb_descriptor_header *)buffer;
		if ((size >= sizeof(struct usb_descriptor_header)) &&
		    ((header->bDescriptorType == USB_DT_CONFIG) ||
		     (header->bDescriptorType == USB_DT_DEVICE)))
			return parsed;

		if (ifp->bNumEndpoints > USB_MAXENDPOINTS) {
			warn("too many endpoints");
			return -1;
		}

		ifp->endpoint = (struct usb_endpoint_descriptor *)
			kmalloc(ifp->bNumEndpoints *
			sizeof(struct usb_endpoint_descriptor), GFP_KERNEL);
		if (!ifp->endpoint) {
			err("out of memory");
			return -1;	
		}

		memset(ifp->endpoint, 0, ifp->bNumEndpoints *
			sizeof(struct usb_endpoint_descriptor));
	
		for (i = 0; i < ifp->bNumEndpoints; i++) {
			header = (struct usb_descriptor_header *)buffer;

			if (header->bLength > size) {
				err("ran out of descriptors parsing");
				return -1;
			}
		
			retval = usb_parse_endpoint(ifp->endpoint + i, buffer, size);
			if (retval < 0)
				return retval;

			buffer += retval;
			parsed += retval;
			size -= retval;
		}

		/* We check to see if it's an alternate to this one */
		ifp = (struct usb_interface_descriptor *)buffer;
		if (size < USB_DT_INTERFACE_SIZE ||
		    ifp->bDescriptorType != USB_DT_INTERFACE ||
		    !ifp->bAlternateSetting)
			return parsed;
	}

	return parsed;
}

int usb_parse_configuration(struct usb_config_descriptor *config, char *buffer)
{
	int i, retval, size;
	struct usb_descriptor_header *header;

	memcpy(config, buffer, USB_DT_CONFIG_SIZE);
	le16_to_cpus(&config->wTotalLength);
	size = config->wTotalLength;

	if (config->bNumInterfaces > USB_MAXINTERFACES) {
		warn("too many interfaces");
		return -1;
	}

	config->interface = (struct usb_interface *)
		kmalloc(config->bNumInterfaces *
		sizeof(struct usb_interface), GFP_KERNEL);
	dbg("kmalloc IF %p, numif %i", config->interface, config->bNumInterfaces);
	if (!config->interface) {
		err("out of memory");
		return -1;	
	}

	memset(config->interface, 0,
	       config->bNumInterfaces * sizeof(struct usb_interface));

	buffer += config->bLength;
	size -= config->bLength;
	
	config->extra = NULL;
	config->extralen = 0;

	for (i = 0; i < config->bNumInterfaces; i++) {
		int numskipped, len;
		char *begin;

		/* Skip over the rest of the Class Specific or Vendor */
		/*  Specific descriptors */
		begin = buffer;
		numskipped = 0;
		while (size >= sizeof(struct usb_descriptor_header)) {
			header = (struct usb_descriptor_header *)buffer;

			if ((header->bLength > size) || (header->bLength < 2)) {
				err("invalid descriptor length of %d", header->bLength);
				return -1;
			}

			/* If we find another "proper" descriptor then we're done  */
			if ((header->bDescriptorType == USB_DT_ENDPOINT) ||
			    (header->bDescriptorType == USB_DT_INTERFACE) ||
			    (header->bDescriptorType == USB_DT_CONFIG) ||
			    (header->bDescriptorType == USB_DT_DEVICE))
				break;

			dbg("skipping descriptor 0x%X", header->bDescriptorType);
			numskipped++;

			buffer += header->bLength;
			size -= header->bLength;
		}
		if (numskipped)
			dbg("skipped %d class/vendor specific endpoint descriptors", numskipped);

		/* Copy any unknown descriptors into a storage area for */
		/*  drivers to later parse */
		len = (int)(buffer - begin);
		if (len) {
			if (config->extralen) {
				warn("extra config descriptor");
			} else {
				config->extra = kmalloc(len, GFP_KERNEL);
				if (!config->extra) {
					err("couldn't allocate memory for config extra descriptors");
					config->extralen = 0;
					return -1;
				}

				memcpy(config->extra, begin, len);
				config->extralen = len;
			}
		}

		retval = usb_parse_interface(config->interface + i, buffer, size);
		if (retval < 0)
			return retval;

		buffer += retval;
		size -= retval;
	}

	return size;
}

void usb_destroy_configuration(struct usb_device *dev)
{
	int c, i, j, k;
	
	if (!dev->config)
		return;

	if (dev->rawdescriptors) {
		for (i = 0; i < dev->descriptor.bNumConfigurations; i++)
			kfree(dev->rawdescriptors[i]);

		kfree(dev->rawdescriptors);
	}

	for (c = 0; c < dev->descriptor.bNumConfigurations; c++) {
		struct usb_config_descriptor *cf = &dev->config[c];

		if (!cf->interface)
			break;

		for (i = 0; i < cf->bNumInterfaces; i++) {
			struct usb_interface *ifp =
				&cf->interface[i];
				
			if (!ifp->altsetting)
				break;

			for (j = 0; j < ifp->num_altsetting; j++) {
				struct usb_interface_descriptor *as =
					&ifp->altsetting[j];
					
				if(as->extra) {
					kfree(as->extra);
				}

				if (!as->endpoint)
					break;
					
				for(k = 0; k < as->bNumEndpoints; k++) {
					if(as->endpoint[k].extra) {
						kfree(as->endpoint[k].extra);
					}
				}	
				kfree(as->endpoint);
			}

			kfree(ifp->altsetting);
		}
		kfree(cf->interface);
	}
	kfree(dev->config);
}

/* for returning string descriptors in UTF-16LE */
static int ascii2utf (char *ascii, __u8 *utf, int utfmax)
{
	int retval;

	for (retval = 0; *ascii && utfmax > 1; utfmax -= 2, retval += 2) {
		*utf++ = *ascii++ & 0x7f;
		*utf++ = 0;
	}
	return retval;
}

/*
 * root_hub_string is used by each host controller's root hub code,
 * so that they're identified consistently throughout the system.
 */
int usb_root_hub_string (int id, int serial, char *type, __u8 *data, int len)
{
	char buf [30];

	// assert (len > (2 * (sizeof (buf) + 1)));
	// assert (strlen (type) <= 8);

	// language ids
	if (id == 0) {
		*data++ = 4; *data++ = 3;	/* 4 bytes data */
		*data++ = 0; *data++ = 0;	/* some language id */
		return 4;

	// serial number
	} else if (id == 1) {
		sprintf (buf, "%x", serial);

	// product description
	} else if (id == 2) {
		sprintf (buf, "USB %s Root Hub", type);

	// id 3 == vendor description

	// unsupported IDs --> "stall"
	} else
	    return 0;

	data [0] = 2 + ascii2utf (buf, data + 2, len - 2);
	data [1] = 3;
	return data [0];
}

/*
 * __usb_get_extra_descriptor() finds a descriptor of specific type in the
 * extra field of the interface and endpoint descriptor structs.
 */

int __usb_get_extra_descriptor(char *buffer, unsigned size, unsigned char type, void **ptr)
{
	struct usb_descriptor_header *header;

	while (size >= sizeof(struct usb_descriptor_header)) {
		header = (struct usb_descriptor_header *)buffer;

		if (header->bLength < 2) {
			err("invalid descriptor length of %d", header->bLength);
			return -1;
		}

		if (header->bDescriptorType == type) {
			*ptr = header;
			return 0;
		}

		buffer += header->bLength;
		size -= header->bLength;
	}
	return -1;
}

/*
 * Something got disconnected. Get rid of it, and all of its children.
 */
void usb_disconnect(struct usb_device **pdev)
{
	struct usb_device * dev = *pdev;
	int i;

	if (!dev)
		return;

	*pdev = NULL;

	info("USB disconnect on device %s-%s address %d",
			dev->bus->bus_name, dev->devpath, dev->devnum);

	if (dev->actconfig) {
		for (i = 0; i < dev->actconfig->bNumInterfaces; i++) {
			struct usb_interface *interface = &dev->actconfig->interface[i];
			struct usb_driver *driver = interface->driver;
			if (driver) {
				down(&driver->serialize);
				driver->disconnect(dev, interface->private_data);
				up(&driver->serialize);
				/* if driver->disconnect didn't release the interface */
				if (interface->driver)
					usb_driver_release_interface(driver, interface);
			}
		}
	}

	/* Free up all the children.. */
	for (i = 0; i < USB_MAXCHILDREN; i++) {
		struct usb_device **child = dev->children + i;
		if (*child)
			usb_disconnect(child);
	}

	/* Let policy agent unload modules etc */
	call_policy ("remove", dev);

	/* Free the device number and remove the /proc/bus/usb entry */
	if (dev->devnum > 0) {
		clear_bit(dev->devnum, &dev->bus->devmap.devicemap);
		usbdevfs_remove_device(dev);
	}

	/* Free up the device itself */
	usb_free_dev(dev);
}

/*
 * Connect a new USB device. This basically just initializes
 * the USB device information and sets up the topology - it's
 * up to the low-level driver to reset the port and actually
 * do the setup (the upper levels don't know how to do that).
 */
void usb_connect(struct usb_device *dev)
{
	int devnum;
	// FIXME needs locking for SMP!!
	/* why? this is called only from the hub thread, 
	 * which hopefully doesn't run on multiple CPU's simultaneously 8-)
	 */
	dev->descriptor.bMaxPacketSize0 = 8;  /* Start off at 8 bytes  */
#ifndef DEVNUM_ROUND_ROBIN
	devnum = find_next_zero_bit(dev->bus->devmap.devicemap, 128, 1);
#else	/* round_robin alloc of devnums */
	/* Try to allocate the next devnum beginning at bus->devnum_next. */
	devnum = find_next_zero_bit(dev->bus->devmap.devicemap, 128, dev->bus->devnum_next);
	if (devnum >= 128)
		devnum = find_next_zero_bit(dev->bus->devmap.devicemap, 128, 1);

	dev->bus->devnum_next = ( devnum >= 127 ? 1 : devnum + 1);
#endif	/* round_robin alloc of devnums */

	if (devnum < 128) {
		set_bit(devnum, dev->bus->devmap.devicemap);
		dev->devnum = devnum;
	}
}

/*
 * These are the actual routines to send
 * and receive control messages.
 */

/* USB spec identifies 5 second timeouts.
 * Some devices (MGE Ellipse UPSes, etc) need it, too.
 */
#define GET_TIMEOUT 5
#define SET_TIMEOUT 5

int usb_set_address(struct usb_device *dev)
{
	return usb_control_msg(dev, usb_snddefctrl(dev), USB_REQ_SET_ADDRESS,
		0, dev->devnum, 0, NULL, 0, HZ * SET_TIMEOUT);
}

int usb_get_descriptor(struct usb_device *dev, unsigned char type, unsigned char index, void *buf, int size)
{
	int i = 5;
	int result;
	
	memset(buf,0,size);	// Make sure we parse really received data

	while (i--) {
		if ((result = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			(type << 8) + index, 0, buf, size, HZ * GET_TIMEOUT)) > 0 ||
		     result == -EPIPE)
			break;	/* retry if the returned length was 0; flaky device */
	}
	return result;
}

int usb_get_class_descriptor(struct usb_device *dev, int ifnum,
		unsigned char type, unsigned char id, void *buf, int size)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
		USB_REQ_GET_DESCRIPTOR, USB_RECIP_INTERFACE | USB_DIR_IN,
		(type << 8) + id, ifnum, buf, size, HZ * GET_TIMEOUT);
}

int usb_get_string(struct usb_device *dev, unsigned short langid, unsigned char index, void *buf, int size)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
		USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
		(USB_DT_STRING << 8) + index, langid, buf, size, HZ * GET_TIMEOUT);
}

int usb_get_device_descriptor(struct usb_device *dev)
{
	int ret = usb_get_descriptor(dev, USB_DT_DEVICE, 0, &dev->descriptor,
				     sizeof(dev->descriptor));
	if (ret >= 0) {
		le16_to_cpus(&dev->descriptor.bcdUSB);
		le16_to_cpus(&dev->descriptor.idVendor);
		le16_to_cpus(&dev->descriptor.idProduct);
		le16_to_cpus(&dev->descriptor.bcdDevice);
	}
	return ret;
}

int usb_get_status(struct usb_device *dev, int type, int target, void *data)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
		USB_REQ_GET_STATUS, USB_DIR_IN | type, 0, target, data, 2, HZ * GET_TIMEOUT);
}

int usb_get_protocol(struct usb_device *dev, int ifnum)
{
	unsigned char type;
	int ret;

	if ((ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
	    USB_REQ_GET_PROTOCOL, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
	    0, ifnum, &type, 1, HZ * GET_TIMEOUT)) < 0)
		return ret;

	return type;
}

int usb_set_protocol(struct usb_device *dev, int ifnum, int protocol)
{
	return usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		USB_REQ_SET_PROTOCOL, USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		protocol, ifnum, NULL, 0, HZ * SET_TIMEOUT);
}

int usb_set_idle(struct usb_device *dev, int ifnum, int duration, int report_id)
{
	return usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		USB_REQ_SET_IDLE, USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		(duration << 8) | report_id, ifnum, NULL, 0, HZ * SET_TIMEOUT);
}

void usb_set_maxpacket(struct usb_device *dev)
{
	int i, b;

	for (i=0; i<dev->actconfig->bNumInterfaces; i++) {
		struct usb_interface *ifp = dev->actconfig->interface + i;
		struct usb_interface_descriptor *as = ifp->altsetting + ifp->act_altsetting;
		struct usb_endpoint_descriptor *ep = as->endpoint;
		int e;

		for (e=0; e<as->bNumEndpoints; e++) {
			b = ep[e].bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
			if ((ep[e].bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
				USB_ENDPOINT_XFER_CONTROL) {	/* Control => bidirectional */
				dev->epmaxpacketout[b] = ep[e].wMaxPacketSize;
				dev->epmaxpacketin [b] = ep[e].wMaxPacketSize;
				}
			else if (usb_endpoint_out(ep[e].bEndpointAddress)) {
				if (ep[e].wMaxPacketSize > dev->epmaxpacketout[b])
					dev->epmaxpacketout[b] = ep[e].wMaxPacketSize;
			}
			else {
				if (ep[e].wMaxPacketSize > dev->epmaxpacketin [b])
					dev->epmaxpacketin [b] = ep[e].wMaxPacketSize;
			}
		}
	}
}

/*
 * endp: endpoint number in bits 0-3;
 *	direction flag in bit 7 (1 = IN, 0 = OUT)
 */
int usb_clear_halt(struct usb_device *dev, int pipe)
{
	int result;
	__u16 status;
	unsigned char *buffer;
	int endp=usb_pipeendpoint(pipe)|(usb_pipein(pipe)<<7);

/*
	if (!usb_endpoint_halted(dev, endp & 0x0f, usb_endpoint_out(endp)))
		return 0;
*/

	result = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT, 0, endp, NULL, 0, HZ * SET_TIMEOUT);

	/* don't clear if failed */
	if (result < 0)
		return result;

	buffer = kmalloc(sizeof(status), GFP_KERNEL);
	if (!buffer) {
		err("unable to allocate memory for configuration descriptors");
		return -ENOMEM;
	}

	result = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
		USB_REQ_GET_STATUS, USB_DIR_IN | USB_RECIP_ENDPOINT, 0, endp,
		buffer, sizeof(status), HZ * SET_TIMEOUT);

	memcpy(&status, buffer, sizeof(status));
	kfree(buffer);

	if (result < 0)
		return result;

	if (le16_to_cpu(status) & 1)
		return -EPIPE;		/* still halted */

	usb_endpoint_running(dev, usb_pipeendpoint(pipe), usb_pipeout(pipe));

	/* toggle is reset on clear */

	usb_settoggle(dev, usb_pipeendpoint(pipe), usb_pipeout(pipe), 0);

	return 0;
}

int usb_set_interface(struct usb_device *dev, int interface, int alternate)
{
	struct usb_interface *iface;
	int ret;

	iface = usb_ifnum_to_if(dev, interface);
	if (!iface) {
		warn("selecting invalid interface %d", interface);
		return -EINVAL;
	}

	/* 9.4.10 says devices don't need this, if the interface
	   only has one alternate setting */
	if (iface->num_altsetting == 1) {
		dbg("ignoring set_interface for dev %d, iface %d, alt %d",
			dev->devnum, interface, alternate);
		return 0;
	}

	if ((ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
	    USB_REQ_SET_INTERFACE, USB_RECIP_INTERFACE, alternate,
	    interface, NULL, 0, HZ * 5)) < 0)
		return ret;

	iface->act_altsetting = alternate;
	dev->toggle[0] = 0;	/* 9.1.1.5 says to do this */
	dev->toggle[1] = 0;
	usb_set_maxpacket(dev);
	return 0;
}

int usb_set_configuration(struct usb_device *dev, int configuration)
{
	int i, ret;
	struct usb_config_descriptor *cp = NULL;
	
	for (i=0; i<dev->descriptor.bNumConfigurations; i++) {
		if (dev->config[i].bConfigurationValue == configuration) {
			cp = &dev->config[i];
			break;
		}
	}
	if (!cp) {
		warn("selecting invalid configuration %d", configuration);
		return -EINVAL;
	}

	if ((ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
	    USB_REQ_SET_CONFIGURATION, 0, configuration, 0, NULL, 0, HZ * SET_TIMEOUT)) < 0)
		return ret;

	dev->actconfig = cp;
	dev->toggle[0] = 0;
	dev->toggle[1] = 0;
	usb_set_maxpacket(dev);

	return 0;
}

int usb_get_report(struct usb_device *dev, int ifnum, unsigned char type, unsigned char id, void *buf, int size)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
		USB_REQ_GET_REPORT, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		(type << 8) + id, ifnum, buf, size, HZ * GET_TIMEOUT);
}

int usb_set_report(struct usb_device *dev, int ifnum, unsigned char type, unsigned char id, void *buf, int size)
{
	return usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		USB_REQ_SET_REPORT, USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		(type << 8) + id, ifnum, buf, size, HZ);
}

int usb_get_configuration(struct usb_device *dev)
{
	int result;
	unsigned int cfgno, length;
	unsigned char *buffer;
	unsigned char *bigbuffer;
 	struct usb_config_descriptor *desc;

	if (dev->descriptor.bNumConfigurations > USB_MAXCONFIG) {
		warn("too many configurations");
		return -EINVAL;
	}

	if (dev->descriptor.bNumConfigurations < 1) {
		warn("not enough configurations");
		return -EINVAL;
	}

	dev->config = (struct usb_config_descriptor *)
		kmalloc(dev->descriptor.bNumConfigurations *
		sizeof(struct usb_config_descriptor), GFP_KERNEL);
	if (!dev->config) {
		err("out of memory");
		return -ENOMEM;	
	}
	memset(dev->config, 0, dev->descriptor.bNumConfigurations *
		sizeof(struct usb_config_descriptor));

	dev->rawdescriptors = (char **)kmalloc(sizeof(char *) *
		dev->descriptor.bNumConfigurations, GFP_KERNEL);
	if (!dev->rawdescriptors) {
		err("out of memory");
		return -ENOMEM;
	}

	buffer = kmalloc(8, GFP_KERNEL);
	if (!buffer) {
		err("unable to allocate memory for configuration descriptors");
		return -ENOMEM;
	}
	desc = (struct usb_config_descriptor *)buffer;

	for (cfgno = 0; cfgno < dev->descriptor.bNumConfigurations; cfgno++) {
		/* We grab the first 8 bytes so we know how long the whole */
		/*  configuration is */
		result = usb_get_descriptor(dev, USB_DT_CONFIG, cfgno, buffer, 8);
		if (result < 8) {
			if (result < 0)
				err("unable to get descriptor");
			else {
				err("config descriptor too short (expected %i, got %i)", 8, result);
				result = -EINVAL;
			}
			goto err;
		}

  	  	/* Get the full buffer */
		length = le16_to_cpu(desc->wTotalLength);

		bigbuffer = kmalloc(length, GFP_KERNEL);
		if (!bigbuffer) {
			err("unable to allocate memory for configuration descriptors");
			result = -ENOMEM;
			goto err;
		}

		/* Now that we know the length, get the whole thing */
		result = usb_get_descriptor(dev, USB_DT_CONFIG, cfgno, bigbuffer, length);
		if (result < 0) {
			err("couldn't get all of config descriptors");
			kfree(bigbuffer);
			goto err;
		}	
	
		if (result < length) {
			err("config descriptor too short (expected %i, got %i)", length, result);
			result = -EINVAL;
			kfree(bigbuffer);
			goto err;
		}

		dev->rawdescriptors[cfgno] = bigbuffer;

		result = usb_parse_configuration(&dev->config[cfgno], bigbuffer);
		if (result > 0)
			dbg("descriptor data left");
		else if (result < 0) {
			result = -EINVAL;
			goto err;
		}
	}

	kfree(buffer);
	return 0;
err:
	kfree(buffer);
	dev->descriptor.bNumConfigurations = cfgno;
	return result;
}

/*
 * usb_string:
 *	returns string length (> 0) or error (< 0)
 */
int usb_string(struct usb_device *dev, int index, char *buf, size_t size)
{
	unsigned char *tbuf;
	int err;
	unsigned int u, idx;

	if (size <= 0 || !buf || !index)
		return -EINVAL;
	buf[0] = 0;
	tbuf = kmalloc(256, GFP_KERNEL);
	if (!tbuf)
		return -ENOMEM;

	/* get langid for strings if it's not yet known */
	if (!dev->have_langid) {
		err = usb_get_string(dev, 0, 0, tbuf, 4);
		if (err < 0) {
			err("error getting string descriptor 0 (error=%d)", err);
			goto errout;
		} else if (err < 4 || tbuf[0] < 4) {
			err("string descriptor 0 too short");
			err = -EINVAL;
			goto errout;
		} else {
			dev->have_langid = -1;
			dev->string_langid = tbuf[2] | (tbuf[3]<< 8);
				/* always use the first langid listed */
			dbg("USB device number %d default language ID 0x%x",
				dev->devnum, dev->string_langid);
		}
	}

	/*
	 * Just ask for a maximum length string and then take the length
	 * that was returned.
	 */
	err = usb_get_string(dev, dev->string_langid, index, tbuf, 255);
	if (err < 0)
		goto errout;

	size--;		/* leave room for trailing NULL char in output buffer */
	for (idx = 0, u = 2; u < err; u += 2) {
		if (idx >= size)
			break;
		if (tbuf[u+1])			/* high byte */
			buf[idx++] = '?';  /* non-ASCII character */
		else
			buf[idx++] = tbuf[u];
	}
	buf[idx] = 0;
	err = idx;

 errout:
	kfree(tbuf);
	return err;
}

/*
 * By the time we get here, the device has gotten a new device ID
 * and is in the default state. We need to identify the thing and
 * get the ball rolling..
 *
 * Returns 0 for success, != 0 for error.
 */
int usb_new_device(struct usb_device *dev)
{
	int err;

	/* USB v1.1 5.5.3 */
	/* We read the first 8 bytes from the device descriptor to get to */
	/*  the bMaxPacketSize0 field. Then we set the maximum packet size */
	/*  for the control pipe, and retrieve the rest */
	dev->epmaxpacketin [0] = 8;
	dev->epmaxpacketout[0] = 8;

	err = usb_set_address(dev);
	if (err < 0) {
		err("USB device not accepting new address=%d (error=%d)",
			dev->devnum, err);
		clear_bit(dev->devnum, &dev->bus->devmap.devicemap);
		dev->devnum = -1;
		return 1;
	}

	wait_ms(10);	/* Let the SET_ADDRESS settle */

	err = usb_get_descriptor(dev, USB_DT_DEVICE, 0, &dev->descriptor, 8);
	if (err < 8) {
		if (err < 0)
			err("USB device not responding, giving up (error=%d)", err);
		else
			err("USB device descriptor short read (expected %i, got %i)", 8, err);
		clear_bit(dev->devnum, &dev->bus->devmap.devicemap);
		dev->devnum = -1;
		return 1;
	}
	dev->epmaxpacketin [0] = dev->descriptor.bMaxPacketSize0;
	dev->epmaxpacketout[0] = dev->descriptor.bMaxPacketSize0;

	err = usb_get_device_descriptor(dev);
	if (err < (signed)sizeof(dev->descriptor)) {
		if (err < 0)
			err("unable to get device descriptor (error=%d)", err);
		else
			err("USB device descriptor short read (expected %Zi, got %i)",
				sizeof(dev->descriptor), err);
	
		clear_bit(dev->devnum, &dev->bus->devmap.devicemap);
		dev->devnum = -1;
		return 1;
	}

	err = usb_get_configuration(dev);
	if (err < 0) {
		err("unable to get device %d configuration (error=%d)",
			dev->devnum, err);
		clear_bit(dev->devnum, &dev->bus->devmap.devicemap);
		dev->devnum = -1;
		return 1;
	}

	/* we set the default configuration here */
	err = usb_set_configuration(dev, dev->config[0].bConfigurationValue);
	if (err) {
		err("failed to set device %d default configuration (error=%d)",
			dev->devnum, err);
		clear_bit(dev->devnum, &dev->bus->devmap.devicemap);
		dev->devnum = -1;
		return 1;
	}

	dbg("new device strings: Mfr=%d, Product=%d, SerialNumber=%d",
		dev->descriptor.iManufacturer, dev->descriptor.iProduct, dev->descriptor.iSerialNumber);
#ifdef DEBUG
	if (dev->descriptor.iManufacturer)
		usb_show_string(dev, "Manufacturer", dev->descriptor.iManufacturer);
	if (dev->descriptor.iProduct)
		usb_show_string(dev, "Product", dev->descriptor.iProduct);
	if (dev->descriptor.iSerialNumber)
		usb_show_string(dev, "SerialNumber", dev->descriptor.iSerialNumber);
#endif

	/* now that the basic setup is over, add a /proc/bus/usb entry */
	usbdevfs_add_device(dev);

	/* find drivers willing to handle this device */
	usb_find_drivers(dev);

	/* userspace may load modules and/or configure further */
	call_policy ("add", dev);

	return 0;
}

static int usb_open(struct inode * inode, struct file * file)
{
	int minor = MINOR(inode->i_rdev);
	struct usb_driver *c = usb_minors[minor/16];
	int err = -ENODEV;
	struct file_operations *old_fops, *new_fops = NULL;

	/*
	 * No load-on-demand? Randy, could you ACK that it's really not
	 * supposed to be done?					-- AV
	 */
	if (!c || !(new_fops = fops_get(c->fops)))
		return err;
	old_fops = file->f_op;
	file->f_op = new_fops;
	/* Curiouser and curiouser... NULL ->open() as "no device" ? */
	if (file->f_op->open)
		err = file->f_op->open(inode,file);
	if (err) {
		fops_put(file->f_op);
		file->f_op = fops_get(old_fops);
	}
	fops_put(old_fops);
	return err;
}

static struct file_operations usb_fops = {
	owner:		THIS_MODULE,
	open:		usb_open,
};

int usb_major_init(void)
{
	if (devfs_register_chrdev(USB_MAJOR, "usb", &usb_fops)) {
		err("unable to get major %d for usb devices", USB_MAJOR);
		return -EBUSY;
	}

	usb_devfs_handle = devfs_mk_dir(NULL, "usb", NULL);

	return 0;
}

void usb_major_cleanup(void)
{
	devfs_unregister(usb_devfs_handle);
	devfs_unregister_chrdev(USB_MAJOR, "usb");
}


#ifdef CONFIG_PROC_FS
struct list_head *usb_driver_get_list(void)
{
	return &usb_driver_list;
}

struct list_head *usb_bus_get_list(void)
{
	return &usb_bus_list;
}
#endif


/*
 * Init
 */
static int __init usb_init(void)
{
	init_MUTEX(&usb_bus_list_lock);
	usb_major_init();
	usbdevfs_init();
	usb_hub_init();

	return 0;
}

/*
 * Cleanup
 */
static void __exit usb_exit(void)
{
	usb_major_cleanup();
	usbdevfs_cleanup();
	usb_hub_cleanup();
}

module_init(usb_init);
module_exit(usb_exit);

/*
 * USB may be built into the kernel or be built as modules.
 * If the USB core [and maybe a host controller driver] is built
 * into the kernel, and other device drivers are built as modules,
 * then these symbols need to be exported for the modules to use.
 */
EXPORT_SYMBOL(usb_ifnum_to_ifpos);
EXPORT_SYMBOL(usb_ifnum_to_if);
EXPORT_SYMBOL(usb_epnum_to_ep_desc);

EXPORT_SYMBOL(usb_register);
EXPORT_SYMBOL(usb_deregister);
EXPORT_SYMBOL(usb_scan_devices);
EXPORT_SYMBOL(usb_alloc_bus);
EXPORT_SYMBOL(usb_free_bus);
EXPORT_SYMBOL(usb_register_bus);
EXPORT_SYMBOL(usb_deregister_bus);
EXPORT_SYMBOL(usb_alloc_dev);
EXPORT_SYMBOL(usb_free_dev);
EXPORT_SYMBOL(usb_inc_dev_use);

EXPORT_SYMBOL(usb_find_interface_driver_for_ifnum);
EXPORT_SYMBOL(usb_driver_claim_interface);
EXPORT_SYMBOL(usb_interface_claimed);
EXPORT_SYMBOL(usb_driver_release_interface);
EXPORT_SYMBOL(usb_match_id);

EXPORT_SYMBOL(usb_root_hub_string);
EXPORT_SYMBOL(usb_new_device);
EXPORT_SYMBOL(usb_reset_device);
EXPORT_SYMBOL(usb_connect);
EXPORT_SYMBOL(usb_disconnect);

EXPORT_SYMBOL(usb_calc_bus_time);
EXPORT_SYMBOL(usb_check_bandwidth);
EXPORT_SYMBOL(usb_claim_bandwidth);
EXPORT_SYMBOL(usb_release_bandwidth);

EXPORT_SYMBOL(usb_set_address);
EXPORT_SYMBOL(usb_get_descriptor);
EXPORT_SYMBOL(usb_get_class_descriptor);
EXPORT_SYMBOL(__usb_get_extra_descriptor);
EXPORT_SYMBOL(usb_get_device_descriptor);
EXPORT_SYMBOL(usb_get_string);
EXPORT_SYMBOL(usb_string);
EXPORT_SYMBOL(usb_get_protocol);
EXPORT_SYMBOL(usb_set_protocol);
EXPORT_SYMBOL(usb_get_report);
EXPORT_SYMBOL(usb_set_report);
EXPORT_SYMBOL(usb_set_idle);
EXPORT_SYMBOL(usb_clear_halt);
EXPORT_SYMBOL(usb_set_interface);
EXPORT_SYMBOL(usb_get_configuration);
EXPORT_SYMBOL(usb_set_configuration);
EXPORT_SYMBOL(usb_get_status);

EXPORT_SYMBOL(usb_get_current_frame_number);

EXPORT_SYMBOL(usb_alloc_urb);
EXPORT_SYMBOL(usb_free_urb);
EXPORT_SYMBOL(usb_submit_urb);
EXPORT_SYMBOL(usb_unlink_urb);

EXPORT_SYMBOL(usb_control_msg);
EXPORT_SYMBOL(usb_bulk_msg);

EXPORT_SYMBOL(usb_devfs_handle);
MODULE_LICENSE("GPL");
