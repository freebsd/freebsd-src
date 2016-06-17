/*
 *	$Id: pci.c,v 1.91 1999/01/21 13:34:01 davem Exp $
 *
 *	PCI Bus Services, see include/linux/pci.h for further explanation.
 *
 *	Copyright 1993 -- 1997 Drew Eckhardt, Frederic Potter,
 *	David Mosberger-Tang
 *
 *	Copyright 1997 -- 2000 Martin Mares <mj@ucw.cz>
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
#include <linux/pm.h>
#include <linux/kmod.h>		/* for hotplug_path */
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/cache.h>

#include <asm/page.h>
#include <asm/dma.h>	/* isa_dma_bridge_buggy */

#undef DEBUG

#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

LIST_HEAD(pci_root_buses);
LIST_HEAD(pci_devices);

/**
 * pci_find_slot - locate PCI device from a given PCI slot
 * @bus: number of PCI bus on which desired PCI device resides
 * @devfn: encodes number of PCI slot in which the desired PCI 
 * device resides and the logical device number within that slot 
 * in case of multi-function devices.
 *
 * Given a PCI bus and slot/function number, the desired PCI device 
 * is located in system global list of PCI devices.  If the device
 * is found, a pointer to its data structure is returned.  If no 
 * device is found, %NULL is returned.
 */
struct pci_dev *
pci_find_slot(unsigned int bus, unsigned int devfn)
{
	struct pci_dev *dev;

	pci_for_each_dev(dev) {
		if (dev->bus->number == bus && dev->devfn == devfn)
			return dev;
	}
	return NULL;
}

/**
 * pci_find_subsys - begin or continue searching for a PCI device by vendor/subvendor/device/subdevice id
 * @vendor: PCI vendor id to match, or %PCI_ANY_ID to match all vendor ids
 * @device: PCI device id to match, or %PCI_ANY_ID to match all device ids
 * @ss_vendor: PCI subsystem vendor id to match, or %PCI_ANY_ID to match all vendor ids
 * @ss_device: PCI subsystem device id to match, or %PCI_ANY_ID to match all device ids
 * @from: Previous PCI device found in search, or %NULL for new search.
 *
 * Iterates through the list of known PCI devices.  If a PCI device is
 * found with a matching @vendor, @device, @ss_vendor and @ss_device, a pointer to its
 * device structure is returned.  Otherwise, %NULL is returned.
 * A new search is initiated by passing %NULL to the @from argument.
 * Otherwise if @from is not %NULL, searches continue from next device on the global list.
 */
struct pci_dev *
pci_find_subsys(unsigned int vendor, unsigned int device,
		unsigned int ss_vendor, unsigned int ss_device,
		const struct pci_dev *from)
{
	struct list_head *n = from ? from->global_list.next : pci_devices.next;

	while (n != &pci_devices) {
		struct pci_dev *dev = pci_dev_g(n);
		if ((vendor == PCI_ANY_ID || dev->vendor == vendor) &&
		    (device == PCI_ANY_ID || dev->device == device) &&
		    (ss_vendor == PCI_ANY_ID || dev->subsystem_vendor == ss_vendor) &&
		    (ss_device == PCI_ANY_ID || dev->subsystem_device == ss_device))
			return dev;
		n = n->next;
	}
	return NULL;
}


/**
 * pci_find_device - begin or continue searching for a PCI device by vendor/device id
 * @vendor: PCI vendor id to match, or %PCI_ANY_ID to match all vendor ids
 * @device: PCI device id to match, or %PCI_ANY_ID to match all device ids
 * @from: Previous PCI device found in search, or %NULL for new search.
 *
 * Iterates through the list of known PCI devices.  If a PCI device is
 * found with a matching @vendor and @device, a pointer to its device structure is
 * returned.  Otherwise, %NULL is returned.
 * A new search is initiated by passing %NULL to the @from argument.
 * Otherwise if @from is not %NULL, searches continue from next device on the global list.
 */
struct pci_dev *
pci_find_device(unsigned int vendor, unsigned int device, const struct pci_dev *from)
{
	return pci_find_subsys(vendor, device, PCI_ANY_ID, PCI_ANY_ID, from);
}


/**
 * pci_find_class - begin or continue searching for a PCI device by class
 * @class: search for a PCI device with this class designation
 * @from: Previous PCI device found in search, or %NULL for new search.
 *
 * Iterates through the list of known PCI devices.  If a PCI device is
 * found with a matching @class, a pointer to its device structure is
 * returned.  Otherwise, %NULL is returned.
 * A new search is initiated by passing %NULL to the @from argument.
 * Otherwise if @from is not %NULL, searches continue from next device
 * on the global list.
 */
struct pci_dev *
pci_find_class(unsigned int class, const struct pci_dev *from)
{
	struct list_head *n = from ? from->global_list.next : pci_devices.next;

	while (n != &pci_devices) {
		struct pci_dev *dev = pci_dev_g(n);
		if (dev->class == class)
			return dev;
		n = n->next;
	}
	return NULL;
}

/**
 * pci_find_capability - query for devices' capabilities 
 * @dev: PCI device to query
 * @cap: capability code
 *
 * Tell if a device supports a given PCI capability.
 * Returns the address of the requested capability structure within the
 * device's PCI configuration space or 0 in case the device does not
 * support it.  Possible values for @cap:
 *
 *  %PCI_CAP_ID_PM           Power Management 
 *
 *  %PCI_CAP_ID_AGP          Accelerated Graphics Port 
 *
 *  %PCI_CAP_ID_VPD          Vital Product Data 
 *
 *  %PCI_CAP_ID_SLOTID       Slot Identification 
 *
 *  %PCI_CAP_ID_MSI          Message Signalled Interrupts
 *
 *  %PCI_CAP_ID_CHSWP        CompactPCI HotSwap 
 *
 *  %PCI_CAP_ID_PCIX         PCI-X
 */
int
pci_find_capability(struct pci_dev *dev, int cap)
{
	u16 status;
	u8 pos, id;
	int ttl = 48;

	pci_read_config_word(dev, PCI_STATUS, &status);
	if (!(status & PCI_STATUS_CAP_LIST))
		return 0;
	switch (dev->hdr_type) {
	case PCI_HEADER_TYPE_NORMAL:
	case PCI_HEADER_TYPE_BRIDGE:
		pci_read_config_byte(dev, PCI_CAPABILITY_LIST, &pos);
		break;
	case PCI_HEADER_TYPE_CARDBUS:
		pci_read_config_byte(dev, PCI_CB_CAPABILITY_LIST, &pos);
		break;
	default:
		return 0;
	}
	while (ttl-- && pos >= 0x40) {
		pos &= ~3;
		pci_read_config_byte(dev, pos + PCI_CAP_LIST_ID, &id);
		if (id == 0xff)
			break;
		if (id == cap)
			return pos;
		pci_read_config_byte(dev, pos + PCI_CAP_LIST_NEXT, &pos);
	}
	return 0;
}


/**
 * pci_find_parent_resource - return resource region of parent bus of given region
 * @dev: PCI device structure contains resources to be searched
 * @res: child resource record for which parent is sought
 *
 *  For given resource region of given device, return the resource
 *  region of parent bus the given region is contained in or where
 *  it should be allocated from.
 */
struct resource *
pci_find_parent_resource(const struct pci_dev *dev, struct resource *res)
{
	const struct pci_bus *bus = dev->bus;
	int i;
	struct resource *best = NULL;

	for(i=0; i<4; i++) {
		struct resource *r = bus->resource[i];
		if (!r)
			continue;
		if (res->start && !(res->start >= r->start && res->end <= r->end))
			continue;	/* Not contained */
		if ((res->flags ^ r->flags) & (IORESOURCE_IO | IORESOURCE_MEM))
			continue;	/* Wrong type */
		if (!((res->flags ^ r->flags) & IORESOURCE_PREFETCH))
			return r;	/* Exact match */
		if ((res->flags & IORESOURCE_PREFETCH) && !(r->flags & IORESOURCE_PREFETCH))
			best = r;	/* Approximating prefetchable by non-prefetchable */
	}
	return best;
}

/**
 * pci_set_power_state - Set the power state of a PCI device
 * @dev: PCI device to be suspended
 * @state: Power state we're entering
 *
 * Transition a device to a new power state, using the Power Management 
 * Capabilities in the device's config space.
 *
 * RETURN VALUE: 
 * -EINVAL if trying to enter a lower state than we're already in.
 * 0 if we're already in the requested state.
 * -EIO if device does not support PCI PM.
 * 0 if we can successfully change the power state.
 */

int
pci_set_power_state(struct pci_dev *dev, int state)
{
	int pm;
	u16 pmcsr;

	/* bound the state we're entering */
	if (state > 3) state = 3;

	/* Validate current state:
	 * Can enter D0 from any state, but if we can only go deeper 
	 * to sleep if we're already in a low power state
	 */
	if (state > 0 && dev->current_state > state)
		return -EINVAL;
	else if (dev->current_state == state) 
		return 0;        /* we're already there */

	/* find PCI PM capability in list */
	pm = pci_find_capability(dev, PCI_CAP_ID_PM);
	
	/* abort if the device doesn't support PM capabilities */
	if (!pm) return -EIO; 

	/* check if this device supports the desired state */
	if (state == 1 || state == 2) {
		u16 pmc;
		pci_read_config_word(dev,pm + PCI_PM_PMC,&pmc);
		if (state == 1 && !(pmc & PCI_PM_CAP_D1)) return -EIO;
		else if (state == 2 && !(pmc & PCI_PM_CAP_D2)) return -EIO;
	}

	/* If we're in D3, force entire word to 0.
	 * This doesn't affect PME_Status, disables PME_En, and
	 * sets PowerState to 0.
	 */
	if (dev->current_state >= 3)
		pmcsr = 0;
	else {
		pci_read_config_word(dev, pm + PCI_PM_CTRL, &pmcsr);
		pmcsr &= ~PCI_PM_CTRL_STATE_MASK;
		pmcsr |= state;
	}

	/* enter specified state */
	pci_write_config_word(dev, pm + PCI_PM_CTRL, pmcsr);

	/* Mandatory power management transition delays */
	/* see PCI PM 1.1 5.6.1 table 18 */
	if(state == 3 || dev->current_state == 3)
	{
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ/100);
	}
	else if(state == 2 || dev->current_state == 2)
		udelay(200);
	dev->current_state = state;

	return 0;
}

/**
 * pci_save_state - save the PCI configuration space of a device before suspending
 * @dev: - PCI device that we're dealing with
 * @buffer: - buffer to hold config space context
 *
 * @buffer must be large enough to hold the entire PCI 2.2 config space 
 * (>= 64 bytes).
 */
int
pci_save_state(struct pci_dev *dev, u32 *buffer)
{
	int i;
	if (buffer) {
		/* XXX: 100% dword access ok here? */
		for (i = 0; i < 16; i++)
			pci_read_config_dword(dev, i * 4,&buffer[i]);
	}
	return 0;
}

/** 
 * pci_restore_state - Restore the saved state of a PCI device
 * @dev: - PCI device that we're dealing with
 * @buffer: - saved PCI config space
 *
 */
int 
pci_restore_state(struct pci_dev *dev, u32 *buffer)
{
	int i;

	if (buffer) {
		for (i = 0; i < 16; i++)
			pci_write_config_dword(dev,i * 4, buffer[i]);
	}
	/*
	 * otherwise, write the context information we know from bootup.
	 * This works around a problem where warm-booting from Windows
	 * combined with a D3(hot)->D0 transition causes PCI config
	 * header data to be forgotten.
	 */	
	else {
		for (i = 0; i < 6; i ++)
			pci_write_config_dword(dev,
					       PCI_BASE_ADDRESS_0 + (i * 4),
					       dev->resource[i].start);
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
	}
	return 0;
}

/**
 * pci_enable_device_bars - Initialize some of a device for use
 * @dev: PCI device to be initialized
 * @bars: bitmask of BAR's that must be configured
 *
 *  Initialize device before it's used by a driver. Ask low-level code
 *  to enable selected I/O and memory resources. Wake up the device if it 
 *  was suspended. Beware, this function can fail.
 */
 
int
pci_enable_device_bars(struct pci_dev *dev, int bars)
{
	int err;

	pci_set_power_state(dev, 0);
	if ((err = pcibios_enable_device(dev, bars)) < 0)
		return err;
	return 0;
}

/**
 * pci_enable_device - Initialize device before it's used by a driver.
 * @dev: PCI device to be initialized
 *
 *  Initialize device before it's used by a driver. Ask low-level code
 *  to enable I/O and memory. Wake up the device if it was suspended.
 *  Beware, this function can fail.
 */
int
pci_enable_device(struct pci_dev *dev)
{
	return pci_enable_device_bars(dev, 0x3F);
}

/**
 * pci_disable_device - Disable PCI device after use
 * @dev: PCI device to be disabled
 *
 * Signal to the system that the PCI device is not in use by the system
 * anymore.  This only involves disabling PCI bus-mastering, if active.
 */
void
pci_disable_device(struct pci_dev *dev)
{
	u16 pci_command;

	pci_read_config_word(dev, PCI_COMMAND, &pci_command);
	if (pci_command & PCI_COMMAND_MASTER) {
		pci_command &= ~PCI_COMMAND_MASTER;
		pci_write_config_word(dev, PCI_COMMAND, pci_command);
	}
}

/**
 * pci_enable_wake - enable device to generate PME# when suspended
 * @dev: - PCI device to operate on
 * @state: - Current state of device.
 * @enable: - Flag to enable or disable generation
 * 
 * Set the bits in the device's PM Capabilities to generate PME# when
 * the system is suspended. 
 *
 * -EIO is returned if device doesn't have PM Capabilities. 
 * -EINVAL is returned if device supports it, but can't generate wake events.
 * 0 if operation is successful.
 * 
 */
int pci_enable_wake(struct pci_dev *dev, u32 state, int enable)
{
	int pm;
	u16 value;

	/* find PCI PM capability in list */
	pm = pci_find_capability(dev, PCI_CAP_ID_PM);

	/* If device doesn't support PM Capabilities, but request is to disable
	 * wake events, it's a nop; otherwise fail */
	if (!pm) 
		return enable ? -EIO : 0; 

	/* Check device's ability to generate PME# */
	pci_read_config_word(dev,pm+PCI_PM_PMC,&value);

	value &= PCI_PM_CAP_PME_MASK;
	value >>= ffs(value);   /* First bit of mask */

	/* Check if it can generate PME# from requested state. */
	if (!value || !(value & (1 << state))) 
		return enable ? -EINVAL : 0;

	pci_read_config_word(dev, pm + PCI_PM_CTRL, &value);

	/* Clear PME_Status by writing 1 to it and enable PME# */
	value |= PCI_PM_CTRL_PME_STATUS | PCI_PM_CTRL_PME_ENABLE;

	if (!enable)
		value &= ~PCI_PM_CTRL_PME_ENABLE;

	pci_write_config_word(dev, pm + PCI_PM_CTRL, value);
	
	return 0;
}

int
pci_get_interrupt_pin(struct pci_dev *dev, struct pci_dev **bridge)
{
	u8 pin;

	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
	if (!pin)
		return -1;
	pin--;
	while (dev->bus->self) {
		pin = (pin + PCI_SLOT(dev->devfn)) % 4;
		dev = dev->bus->self;
	}
	*bridge = dev;
	return pin;
}

/**
 *	pci_release_region - Release a PCI bar
 *	@pdev: PCI device whose resources were previously reserved by pci_request_region
 *	@bar: BAR to release
 *
 *	Releases the PCI I/O and memory resources previously reserved by a
 *	successful call to pci_request_region.  Call this function only
 *	after all use of the PCI regions has ceased.
 */
void pci_release_region(struct pci_dev *pdev, int bar)
{
	if (pci_resource_len(pdev, bar) == 0)
		return;
	if (pci_resource_flags(pdev, bar) & IORESOURCE_IO)
		release_region(pci_resource_start(pdev, bar),
				pci_resource_len(pdev, bar));
	else if (pci_resource_flags(pdev, bar) & IORESOURCE_MEM)
		release_mem_region(pci_resource_start(pdev, bar),
				pci_resource_len(pdev, bar));
}

/**
 *	pci_request_region - Reserved PCI I/O and memory resource
 *	@pdev: PCI device whose resources are to be reserved
 *	@bar: BAR to be reserved
 *	@res_name: Name to be associated with resource.
 *
 *	Mark the PCI region associated with PCI device @pdev BR @bar as
 *	being reserved by owner @res_name.  Do not access any
 *	address inside the PCI regions unless this call returns
 *	successfully.
 *
 *	Returns 0 on success, or %EBUSY on error.  A warning
 *	message is also printed on failure.
 */
int pci_request_region(struct pci_dev *pdev, int bar, char *res_name)
{
	if (pci_resource_len(pdev, bar) == 0)
		return 0;
		
	if (pci_resource_flags(pdev, bar) & IORESOURCE_IO) {
		if (!request_region(pci_resource_start(pdev, bar),
			    pci_resource_len(pdev, bar), res_name))
			goto err_out;
	}
	else if (pci_resource_flags(pdev, bar) & IORESOURCE_MEM) {
		if (!request_mem_region(pci_resource_start(pdev, bar),
				        pci_resource_len(pdev, bar), res_name))
			goto err_out;
	}
	
	return 0;

err_out:
	printk (KERN_WARNING "PCI: Unable to reserve %s region #%d:%lx@%lx for device %s\n",
		pci_resource_flags(pdev, bar) & IORESOURCE_IO ? "I/O" : "mem",
		bar + 1, /* PCI BAR # */
		pci_resource_len(pdev, bar), pci_resource_start(pdev, bar),
		pdev->slot_name);
	return -EBUSY;
}


/**
 *	pci_release_regions - Release reserved PCI I/O and memory resources
 *	@pdev: PCI device whose resources were previously reserved by pci_request_regions
 *
 *	Releases all PCI I/O and memory resources previously reserved by a
 *	successful call to pci_request_regions.  Call this function only
 *	after all use of the PCI regions has ceased.
 */

void pci_release_regions(struct pci_dev *pdev)
{
	int i;
	
	for (i = 0; i < 6; i++)
		pci_release_region(pdev, i);
}

/**
 *	pci_request_regions - Reserved PCI I/O and memory resources
 *	@pdev: PCI device whose resources are to be reserved
 *	@res_name: Name to be associated with resource.
 *
 *	Mark all PCI regions associated with PCI device @pdev as
 *	being reserved by owner @res_name.  Do not access any
 *	address inside the PCI regions unless this call returns
 *	successfully.
 *
 *	Returns 0 on success, or %EBUSY on error.  A warning
 *	message is also printed on failure.
 */
int pci_request_regions(struct pci_dev *pdev, char *res_name)
{
	int i;
	
	for (i = 0; i < 6; i++)
		if(pci_request_region(pdev, i, res_name))
			goto err_out;
	return 0;

err_out:
	printk (KERN_WARNING "PCI: Unable to reserve %s region #%d:%lx@%lx for device %s\n",
		pci_resource_flags(pdev, i) & IORESOURCE_IO ? "I/O" : "mem",
		i + 1, /* PCI BAR # */
		pci_resource_len(pdev, i), pci_resource_start(pdev, i),
		pdev->slot_name);
	while(--i >= 0)
		pci_release_region(pdev, i);
		
	return -EBUSY;
}


/*
 *  Registration of PCI drivers and handling of hot-pluggable devices.
 */

static LIST_HEAD(pci_drivers);

/**
 * pci_match_device - Tell if a PCI device structure has a matching PCI device id structure
 * @ids: array of PCI device id structures to search in
 * @dev: the PCI device structure to match against
 * 
 * Used by a driver to check whether a PCI device present in the
 * system is in its list of supported devices.Returns the matching
 * pci_device_id structure or %NULL if there is no match.
 */
const struct pci_device_id *
pci_match_device(const struct pci_device_id *ids, const struct pci_dev *dev)
{
	while (ids->vendor || ids->subvendor || ids->class_mask) {
		if ((ids->vendor == PCI_ANY_ID || ids->vendor == dev->vendor) &&
		    (ids->device == PCI_ANY_ID || ids->device == dev->device) &&
		    (ids->subvendor == PCI_ANY_ID || ids->subvendor == dev->subsystem_vendor) &&
		    (ids->subdevice == PCI_ANY_ID || ids->subdevice == dev->subsystem_device) &&
		    !((ids->class ^ dev->class) & ids->class_mask))
			return ids;
		ids++;
	}
	return NULL;
}

static int
pci_announce_device(struct pci_driver *drv, struct pci_dev *dev)
{
	const struct pci_device_id *id;
	int ret = 0;

	if (drv->id_table) {
		id = pci_match_device(drv->id_table, dev);
		if (!id) {
			ret = 0;
			goto out;
		}
	} else
		id = NULL;

	dev_probe_lock();
	if (drv->probe(dev, id) >= 0) {
		dev->driver = drv;
		ret = 1;
	}
	dev_probe_unlock();
out:
	return ret;
}

/**
 * pci_register_driver - register a new pci driver
 * @drv: the driver structure to register
 * 
 * Adds the driver structure to the list of registered drivers
 * Returns the number of pci devices which were claimed by the driver
 * during registration.  The driver remains registered even if the
 * return value is zero.
 */
int
pci_register_driver(struct pci_driver *drv)
{
	struct pci_dev *dev;
	int count = 0;

	list_add_tail(&drv->node, &pci_drivers);
	pci_for_each_dev(dev) {
		if (!pci_dev_driver(dev))
			count += pci_announce_device(drv, dev);
	}
	return count;
}

/**
 * pci_unregister_driver - unregister a pci driver
 * @drv: the driver structure to unregister
 * 
 * Deletes the driver structure from the list of registered PCI drivers,
 * gives it a chance to clean up by calling its remove() function for
 * each device it was responsible for, and marks those devices as
 * driverless.
 */

void
pci_unregister_driver(struct pci_driver *drv)
{
	struct pci_dev *dev;

	list_del(&drv->node);
	pci_for_each_dev(dev) {
		if (dev->driver == drv) {
			if (drv->remove)
				drv->remove(dev);
			dev->driver = NULL;
		}
	}
}

#ifdef CONFIG_HOTPLUG

#ifndef FALSE
#define FALSE	(0)
#define TRUE	(!FALSE)
#endif

static void
run_sbin_hotplug(struct pci_dev *pdev, int insert)
{
	int i;
	char *argv[3], *envp[8];
	char id[20], sub_id[24], bus_id[24], class_id[20];

	if (!hotplug_path[0])
		return;

	sprintf(class_id, "PCI_CLASS=%04X", pdev->class);
	sprintf(id, "PCI_ID=%04X:%04X", pdev->vendor, pdev->device);
	sprintf(sub_id, "PCI_SUBSYS_ID=%04X:%04X", pdev->subsystem_vendor, pdev->subsystem_device);
	sprintf(bus_id, "PCI_SLOT_NAME=%s", pdev->slot_name);

	i = 0;
	argv[i++] = hotplug_path;
	argv[i++] = "pci";
	argv[i] = 0;

	i = 0;
	/* minimal command environment */
	envp[i++] = "HOME=/";
	envp[i++] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
	
	/* other stuff we want to pass to /sbin/hotplug */
	envp[i++] = class_id;
	envp[i++] = id;
	envp[i++] = sub_id;
	envp[i++] = bus_id;
	if (insert)
		envp[i++] = "ACTION=add";
	else
		envp[i++] = "ACTION=remove";
	envp[i] = 0;

	call_usermodehelper (argv [0], argv, envp);
}

/**
 * pci_announce_device_to_drivers - tell the drivers a new device has appeared
 * @dev: the device that has shown up
 *
 * Notifys the drivers that a new device has appeared, and also notifys
 * userspace through /sbin/hotplug.
 */
void
pci_announce_device_to_drivers(struct pci_dev *dev)
{
	struct list_head *ln;

	for(ln=pci_drivers.next; ln != &pci_drivers; ln=ln->next) {
		struct pci_driver *drv = list_entry(ln, struct pci_driver, node);
		if (drv->remove && pci_announce_device(drv, dev))
			break;
	}

	/* notify userspace of new hotplug device */
	run_sbin_hotplug(dev, TRUE);
}

/**
 * pci_insert_device - insert a hotplug device
 * @dev: the device to insert
 * @bus: where to insert it
 *
 * Add a new device to the device lists and notify userspace (/sbin/hotplug).
 */
void
pci_insert_device(struct pci_dev *dev, struct pci_bus *bus)
{
	list_add_tail(&dev->bus_list, &bus->devices);
	list_add_tail(&dev->global_list, &pci_devices);
#ifdef CONFIG_PROC_FS
	pci_proc_attach_device(dev);
#endif
	pci_announce_device_to_drivers(dev);
}

static void
pci_free_resources(struct pci_dev *dev)
{
	int i;

	for (i = 0; i < PCI_NUM_RESOURCES; i++) {
		struct resource *res = dev->resource + i;
		if (res->parent)
			release_resource(res);
	}
}

/**
 * pci_remove_device - remove a hotplug device
 * @dev: the device to remove
 *
 * Delete the device structure from the device lists and 
 * notify userspace (/sbin/hotplug).
 */
void
pci_remove_device(struct pci_dev *dev)
{
	if (dev->driver) {
		if (dev->driver->remove)
			dev->driver->remove(dev);
		dev->driver = NULL;
	}
	list_del(&dev->bus_list);
	list_del(&dev->global_list);
	pci_free_resources(dev);
#ifdef CONFIG_PROC_FS
	pci_proc_detach_device(dev);
#endif

	/* notify userspace of hotplug device removal */
	run_sbin_hotplug(dev, FALSE);
}

#endif

static struct pci_driver pci_compat_driver = {
	name: "compat"
};

/**
 * pci_dev_driver - get the pci_driver of a device
 * @dev: the device to query
 *
 * Returns the appropriate pci_driver structure or %NULL if there is no 
 * registered driver for the device.
 */
struct pci_driver *
pci_dev_driver(const struct pci_dev *dev)
{
	if (dev->driver)
		return dev->driver;
	else {
		int i;
		for(i=0; i<=PCI_ROM_RESOURCE; i++)
			if (dev->resource[i].flags & IORESOURCE_BUSY)
				return &pci_compat_driver;
	}
	return NULL;
}


/*
 * This interrupt-safe spinlock protects all accesses to PCI
 * configuration space.
 */

static spinlock_t pci_lock = SPIN_LOCK_UNLOCKED;

/*
 *  Wrappers for all PCI configuration access functions.  They just check
 *  alignment, do locking and call the low-level functions pointed to
 *  by pci_dev->ops.
 */

#define PCI_byte_BAD 0
#define PCI_word_BAD (pos & 1)
#define PCI_dword_BAD (pos & 3)

#define PCI_OP(rw,size,type) \
int pci_##rw##_config_##size (struct pci_dev *dev, int pos, type value) \
{									\
	int res;							\
	unsigned long flags;						\
	if (PCI_##size##_BAD) return PCIBIOS_BAD_REGISTER_NUMBER;	\
	spin_lock_irqsave(&pci_lock, flags);				\
	res = dev->bus->ops->rw##_##size(dev, pos, value);		\
	spin_unlock_irqrestore(&pci_lock, flags);			\
	return res;							\
}

PCI_OP(read, byte, u8 *)
PCI_OP(read, word, u16 *)
PCI_OP(read, dword, u32 *)
PCI_OP(write, byte, u8)
PCI_OP(write, word, u16)
PCI_OP(write, dword, u32)

/**
 * pci_set_master - enables bus-mastering for device dev
 * @dev: the PCI device to enable
 *
 * Enables bus-mastering on the device and calls pcibios_set_master()
 * to do the needed arch specific settings.
 */
void
pci_set_master(struct pci_dev *dev)
{
	u16 cmd;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (! (cmd & PCI_COMMAND_MASTER)) {
		DBG("PCI: Enabling bus mastering for device %s\n", dev->slot_name);
		cmd |= PCI_COMMAND_MASTER;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	pcibios_set_master(dev);
}

#ifndef HAVE_ARCH_PCI_MWI
/* This can be overridden by arch code. */
u8 pci_cache_line_size = L1_CACHE_BYTES >> 2;

/**
 * pci_generic_prep_mwi - helper function for pci_set_mwi
 * @dev: the PCI device for which MWI is enabled
 *
 * Helper function for implementation the arch-specific pcibios_set_mwi
 * function.  Originally copied from drivers/net/acenic.c.
 * Copyright 1998-2001 by Jes Sorensen, <jes@trained-monkey.org>.
 *
 * RETURNS: An appriopriate -ERRNO error value on eror, or zero for success.
 */
static int
pci_generic_prep_mwi(struct pci_dev *dev)
{
	u8 cacheline_size;

	if (!pci_cache_line_size)
		return -EINVAL;		/* The system doesn't support MWI. */

	/* Validate current setting: the PCI_CACHE_LINE_SIZE must be
	   equal to or multiple of the right value. */
	pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &cacheline_size);
	if (cacheline_size >= pci_cache_line_size &&
	    (cacheline_size % pci_cache_line_size) == 0)
		return 0;

	/* Write the correct value. */
	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, pci_cache_line_size);
	/* Read it back. */
	pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &cacheline_size);
	if (cacheline_size == pci_cache_line_size)
		return 0;

	printk(KERN_WARNING "PCI: cache line size of %d is not supported "
	       "by device %s\n", pci_cache_line_size << 2, dev->slot_name);

	return -EINVAL;
}
#endif /* !HAVE_ARCH_PCI_MWI */

/**
 * pci_set_mwi - enables memory-write-invalidate PCI transaction
 * @dev: the PCI device for which MWI is enabled
 *
 * Enables the Memory-Write-Invalidate transaction in %PCI_COMMAND,
 * and then calls @pcibios_set_mwi to do the needed arch specific
 * operations or a generic mwi-prep function.
 *
 * RETURNS: An appriopriate -ERRNO error value on eror, or zero for success.
 */
int
pci_set_mwi(struct pci_dev *dev)
{
	int rc;
	u16 cmd;

#ifdef HAVE_ARCH_PCI_MWI
	rc = pcibios_set_mwi(dev);
#else
	rc = pci_generic_prep_mwi(dev);
#endif

	if (rc)
		return rc;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (! (cmd & PCI_COMMAND_INVALIDATE)) {
		DBG("PCI: Enabling Mem-Wr-Inval for device %s\n", dev->slot_name);
		cmd |= PCI_COMMAND_INVALIDATE;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	
	return 0;
}

/**
 * pci_clear_mwi - disables Memory-Write-Invalidate for device dev
 * @dev: the PCI device to disable
 *
 * Disables PCI Memory-Write-Invalidate transaction on the device
 */
void
pci_clear_mwi(struct pci_dev *dev)
{
	u16 cmd;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (cmd & PCI_COMMAND_INVALIDATE) {
		cmd &= ~PCI_COMMAND_INVALIDATE;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
}

int
pci_set_dma_mask(struct pci_dev *dev, u64 mask)
{
	if (!pci_dma_supported(dev, mask))
		return -EIO;

	dev->dma_mask = mask;

	return 0;
}
    
int
pci_dac_set_dma_mask(struct pci_dev *dev, u64 mask)
{
	if (!pci_dac_dma_supported(dev, mask))
		return -EIO;

	dev->dma_mask = mask;

	return 0;
}
    
/*
 * Translate the low bits of the PCI base
 * to the resource type
 */
static inline unsigned int pci_calc_resource_flags(unsigned int flags)
{
	if (flags & PCI_BASE_ADDRESS_SPACE_IO)
		return IORESOURCE_IO;

	if (flags & PCI_BASE_ADDRESS_MEM_PREFETCH)
		return IORESOURCE_MEM | IORESOURCE_PREFETCH;

	return IORESOURCE_MEM;
}

/*
 * Find the extent of a PCI decode, do sanity checks.
 */
static u32 pci_size(u32 base, u32 maxbase, unsigned long mask)
{
	u32 size = mask & maxbase;	/* Find the significant bits */
	if (!size)
		return 0;
	size = size & ~(size-1);	/* Get the lowest of them to find the decode size */
	size -= 1;			/* extent = size - 1 */
	if (base == maxbase && ((base | size) & mask) != mask)
		return 0;		/* base == maxbase can be valid only
					   if the BAR has been already
					   programmed with all 1s */
	return size;
}

static void pci_read_bases(struct pci_dev *dev, unsigned int howmany, int rom)
{
	unsigned int pos, reg, next;
	u32 l, sz;
	struct resource *res;

	for(pos=0; pos<howmany; pos = next) {
		next = pos+1;
		res = &dev->resource[pos];
		res->name = dev->name;
		reg = PCI_BASE_ADDRESS_0 + (pos << 2);
		pci_read_config_dword(dev, reg, &l);
		pci_write_config_dword(dev, reg, ~0);
		pci_read_config_dword(dev, reg, &sz);
		pci_write_config_dword(dev, reg, l);
		if (!sz || sz == 0xffffffff)
			continue;
		if (l == 0xffffffff)
			l = 0;
		if ((l & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY) {
			sz = pci_size(l, sz, PCI_BASE_ADDRESS_MEM_MASK);
			if (!sz)
				continue;
			res->start = l & PCI_BASE_ADDRESS_MEM_MASK;
			res->flags |= l & ~PCI_BASE_ADDRESS_MEM_MASK;
		} else {
			sz = pci_size(l, sz, PCI_BASE_ADDRESS_IO_MASK & 0xffff);
			if (!sz)
				continue;
			res->start = l & PCI_BASE_ADDRESS_IO_MASK;
			res->flags |= l & ~PCI_BASE_ADDRESS_IO_MASK;
		}
		res->end = res->start + (unsigned long) sz;
		res->flags |= pci_calc_resource_flags(l);
		if ((l & (PCI_BASE_ADDRESS_SPACE | PCI_BASE_ADDRESS_MEM_TYPE_MASK))
		    == (PCI_BASE_ADDRESS_SPACE_MEMORY | PCI_BASE_ADDRESS_MEM_TYPE_64)) {
			pci_read_config_dword(dev, reg+4, &l);
			next++;
#if BITS_PER_LONG == 64
			res->start |= ((unsigned long) l) << 32;
			res->end = res->start + sz;
			pci_write_config_dword(dev, reg+4, ~0);
			pci_read_config_dword(dev, reg+4, &sz);
			pci_write_config_dword(dev, reg+4, l);
			if (~sz)
				res->end = res->start + 0xffffffff +
						(((unsigned long) ~sz) << 32);
#else
			if (l) {
				printk(KERN_ERR "PCI: Unable to handle 64-bit address for device %s\n", dev->slot_name);
				res->start = 0;
				res->flags = 0;
				continue;
			}
#endif
		}
	}
	if (rom) {
		dev->rom_base_reg = rom;
		res = &dev->resource[PCI_ROM_RESOURCE];
		res->name = dev->name;
		pci_read_config_dword(dev, rom, &l);
		pci_write_config_dword(dev, rom, ~PCI_ROM_ADDRESS_ENABLE);
		pci_read_config_dword(dev, rom, &sz);
		pci_write_config_dword(dev, rom, l);
		if (l == 0xffffffff)
			l = 0;
		if (sz && sz != 0xffffffff) {
			sz = pci_size(l, sz, PCI_ROM_ADDRESS_MASK);
			if (!sz)
				return;
			res->flags = (l & PCI_ROM_ADDRESS_ENABLE) |
			  IORESOURCE_MEM | IORESOURCE_PREFETCH | IORESOURCE_READONLY | IORESOURCE_CACHEABLE;
			res->start = l & PCI_ROM_ADDRESS_MASK;
			res->end = res->start + (unsigned long) sz;
		}
	}
}

void __devinit pci_read_bridge_bases(struct pci_bus *child)
{
	struct pci_dev *dev = child->self;
	u8 io_base_lo, io_limit_lo;
	u16 mem_base_lo, mem_limit_lo;
	unsigned long base, limit;
	struct resource *res;
	int i;

	if (!dev)		/* It's a host bus, nothing to read */
		return;

	if (dev->transparent) {
		printk("Transparent bridge - %s\n", dev->name);
		for(i = 0; i < 4; i++)
			child->resource[i] = child->parent->resource[i];
		return;
	}

	for(i=0; i<3; i++)
		child->resource[i] = &dev->resource[PCI_BRIDGE_RESOURCES+i];

	res = child->resource[0];
	pci_read_config_byte(dev, PCI_IO_BASE, &io_base_lo);
	pci_read_config_byte(dev, PCI_IO_LIMIT, &io_limit_lo);
	base = (io_base_lo & PCI_IO_RANGE_MASK) << 8;
	limit = (io_limit_lo & PCI_IO_RANGE_MASK) << 8;

	if ((io_base_lo & PCI_IO_RANGE_TYPE_MASK) == PCI_IO_RANGE_TYPE_32) {
		u16 io_base_hi, io_limit_hi;
		pci_read_config_word(dev, PCI_IO_BASE_UPPER16, &io_base_hi);
		pci_read_config_word(dev, PCI_IO_LIMIT_UPPER16, &io_limit_hi);
		base |= (io_base_hi << 16);
		limit |= (io_limit_hi << 16);
	}

	if (base && base <= limit) {
		res->flags = (io_base_lo & PCI_IO_RANGE_TYPE_MASK) | IORESOURCE_IO;
		res->start = base;
		res->end = limit + 0xfff;
	}

	res = child->resource[1];
	pci_read_config_word(dev, PCI_MEMORY_BASE, &mem_base_lo);
	pci_read_config_word(dev, PCI_MEMORY_LIMIT, &mem_limit_lo);
	base = (mem_base_lo & PCI_MEMORY_RANGE_MASK) << 16;
	limit = (mem_limit_lo & PCI_MEMORY_RANGE_MASK) << 16;
	if (base && base <= limit) {
		res->flags = (mem_base_lo & PCI_MEMORY_RANGE_TYPE_MASK) | IORESOURCE_MEM;
		res->start = base;
		res->end = limit + 0xfffff;
	}

	res = child->resource[2];
	pci_read_config_word(dev, PCI_PREF_MEMORY_BASE, &mem_base_lo);
	pci_read_config_word(dev, PCI_PREF_MEMORY_LIMIT, &mem_limit_lo);
	base = (mem_base_lo & PCI_PREF_RANGE_MASK) << 16;
	limit = (mem_limit_lo & PCI_PREF_RANGE_MASK) << 16;

	if ((mem_base_lo & PCI_PREF_RANGE_TYPE_MASK) == PCI_PREF_RANGE_TYPE_64) {
		u32 mem_base_hi, mem_limit_hi;
		pci_read_config_dword(dev, PCI_PREF_BASE_UPPER32, &mem_base_hi);
		pci_read_config_dword(dev, PCI_PREF_LIMIT_UPPER32, &mem_limit_hi);
#if BITS_PER_LONG == 64
		base |= ((long) mem_base_hi) << 32;
		limit |= ((long) mem_limit_hi) << 32;
#else
		if (mem_base_hi || mem_limit_hi) {
			printk(KERN_ERR "PCI: Unable to handle 64-bit address space for %s\n", child->name);
			return;
		}
#endif
	}
	if (base && base <= limit) {
		res->flags = (mem_base_lo & PCI_MEMORY_RANGE_TYPE_MASK) | IORESOURCE_MEM | IORESOURCE_PREFETCH;
		res->start = base;
		res->end = limit + 0xfffff;
	}
}

static struct pci_bus * __devinit pci_alloc_bus(void)
{
	struct pci_bus *b;

	b = kmalloc(sizeof(*b), GFP_KERNEL);
	if (b) {
		memset(b, 0, sizeof(*b));
		INIT_LIST_HEAD(&b->children);
		INIT_LIST_HEAD(&b->devices);
	}
	return b;
}

struct pci_bus * __devinit pci_add_new_bus(struct pci_bus *parent, struct pci_dev *dev, int busnr)
{
	struct pci_bus *child;
	int i;

	/*
	 * Allocate a new bus, and inherit stuff from the parent..
	 */
	child = pci_alloc_bus();

	list_add_tail(&child->node, &parent->children);
	child->self = dev;
	dev->subordinate = child;
	child->parent = parent;
	child->ops = parent->ops;
	child->sysdata = parent->sysdata;

	/*
	 * Set up the primary, secondary and subordinate
	 * bus numbers.
	 */
	child->number = child->secondary = busnr;
	child->primary = parent->secondary;
	child->subordinate = 0xff;

	/* Set up default resource pointers and names.. */
	for (i = 0; i < 4; i++) {
		child->resource[i] = &dev->resource[PCI_BRIDGE_RESOURCES+i];
		child->resource[i]->name = child->name;
	}

	return child;
}

unsigned int __devinit pci_do_scan_bus(struct pci_bus *bus);

/*
 * If it's a bridge, configure it and scan the bus behind it.
 * For CardBus bridges, we don't scan behind as the devices will
 * be handled by the bridge driver itself.
 *
 * We need to process bridges in two passes -- first we scan those
 * already configured by the BIOS and after we are done with all of
 * them, we proceed to assigning numbers to the remaining buses in
 * order to avoid overlaps between old and new bus numbers.
 */
static int __devinit pci_scan_bridge(struct pci_bus *bus, struct pci_dev * dev, int max, int pass)
{
	unsigned int buses;
	unsigned short cr;
	struct pci_bus *child;
	int is_cardbus = (dev->hdr_type == PCI_HEADER_TYPE_CARDBUS);

	pci_read_config_dword(dev, PCI_PRIMARY_BUS, &buses);
	DBG("Scanning behind PCI bridge %s, config %06x, pass %d\n", dev->slot_name, buses & 0xffffff, pass);
	if ((buses & 0xffff00) && !pcibios_assign_all_busses()) {
		/*
		 * Bus already configured by firmware, process it in the first
		 * pass and just note the configuration.
		 */
		if (pass)
			return max;
		child = pci_add_new_bus(bus, dev, 0);
		child->primary = buses & 0xFF;
		child->secondary = (buses >> 8) & 0xFF;
		child->subordinate = (buses >> 16) & 0xFF;
		child->number = child->secondary;
		if (!is_cardbus) {
			unsigned int cmax = pci_do_scan_bus(child);
			if (cmax > max) max = cmax;
		} else {
			unsigned int cmax = child->subordinate;
			if (cmax > max) max = cmax;
		}
	} else {
		/*
		 * We need to assign a number to this bus which we always
		 * do in the second pass. We also keep all address decoders
		 * on the bridge disabled during scanning.  FIXME: Why?
		 */
		if (!pass)
			return max;
		pci_read_config_word(dev, PCI_COMMAND, &cr);
		pci_write_config_word(dev, PCI_COMMAND, 0x0000);
		pci_write_config_word(dev, PCI_STATUS, 0xffff);

		child = pci_add_new_bus(bus, dev, ++max);
		buses = (buses & 0xff000000)
		      | ((unsigned int)(child->primary)     <<  0)
		      | ((unsigned int)(child->secondary)   <<  8)
		      | ((unsigned int)(child->subordinate) << 16);
		/*
		 * We need to blast all three values with a single write.
		 */
		pci_write_config_dword(dev, PCI_PRIMARY_BUS, buses);
		if (!is_cardbus) {
			/* Now we can scan all subordinate buses... */
			max = pci_do_scan_bus(child);
		} else {
			/*
			 * For CardBus bridges, we leave 4 bus numbers
			 * as cards with a PCI-to-PCI bridge can be
			 * inserted later.
			 */
			max += 3;
		}
		/*
		 * Set the subordinate bus number to its real value.
		 */
		child->subordinate = max;
		pci_write_config_byte(dev, PCI_SUBORDINATE_BUS, max);
		pci_write_config_word(dev, PCI_COMMAND, cr);
	}
	sprintf(child->name, (is_cardbus ? "PCI CardBus #%02x" : "PCI Bus #%02x"), child->number);
	return max;
}

/*
 * Read interrupt line and base address registers.
 * The architecture-dependent code can tweak these, of course.
 */
static void pci_read_irq(struct pci_dev *dev)
{
	unsigned char irq;

	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &irq);
	if (irq)
		pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
	dev->irq = irq;
}

/**
 * pci_setup_device - fill in class and map information of a device
 * @dev: the device structure to fill
 *
 * Initialize the device structure with information about the device's 
 * vendor,class,memory and IO-space addresses,IRQ lines etc.
 * Called at initialisation of the PCI subsystem and by CardBus services.
 * Returns 0 on success and -1 if unknown type of device (not normal, bridge
 * or CardBus).
 */
int pci_setup_device(struct pci_dev * dev)
{
	u32 class;

	sprintf(dev->slot_name, "%02x:%02x.%d", dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
	sprintf(dev->name, "PCI device %04x:%04x", dev->vendor, dev->device);
	
	pci_read_config_dword(dev, PCI_CLASS_REVISION, &class);
	class >>= 8;				    /* upper 3 bytes */
	dev->class = class;
	class >>= 8;

	DBG("Found %02x:%02x [%04x/%04x] %06x %02x\n", dev->bus->number, dev->devfn, dev->vendor, dev->device, class, dev->hdr_type);

	/* "Unknown power state" */
	dev->current_state = 4;

	switch (dev->hdr_type) {		    /* header type */
	case PCI_HEADER_TYPE_NORMAL:		    /* standard header */
		if (class == PCI_CLASS_BRIDGE_PCI)
			goto bad;
		pci_read_irq(dev);
		pci_read_bases(dev, 6, PCI_ROM_ADDRESS);
		pci_read_config_word(dev, PCI_SUBSYSTEM_VENDOR_ID, &dev->subsystem_vendor);
		pci_read_config_word(dev, PCI_SUBSYSTEM_ID, &dev->subsystem_device);
		break;

	case PCI_HEADER_TYPE_BRIDGE:		    /* bridge header */
		if (class != PCI_CLASS_BRIDGE_PCI)
			goto bad;
		/* The PCI-to-PCI bridge spec requires that subtractive
		   decoding (i.e. transparent) bridge must have programming
		   interface code of 0x01. */ 
		dev->transparent = ((dev->class & 0xff) == 1);
		pci_read_bases(dev, 2, PCI_ROM_ADDRESS1);
		break;

	case PCI_HEADER_TYPE_CARDBUS:		    /* CardBus bridge header */
		if (class != PCI_CLASS_BRIDGE_CARDBUS)
			goto bad;
		pci_read_irq(dev);
		pci_read_bases(dev, 1, 0);
		pci_read_config_word(dev, PCI_CB_SUBSYSTEM_VENDOR_ID, &dev->subsystem_vendor);
		pci_read_config_word(dev, PCI_CB_SUBSYSTEM_ID, &dev->subsystem_device);
		break;

	default:				    /* unknown header */
		printk(KERN_ERR "PCI: device %s has unknown header type %02x, ignoring.\n",
			dev->slot_name, dev->hdr_type);
		return -1;

	bad:
		printk(KERN_ERR "PCI: %s: class %x doesn't match header type %02x. Ignoring class.\n",
		       dev->slot_name, class, dev->hdr_type);
		dev->class = PCI_CLASS_NOT_DEFINED;
	}

	/* We found a fine healthy device, go go go... */
	return 0;
}

/*
 * Read the config data for a PCI device, sanity-check it
 * and fill in the dev structure...
 */
struct pci_dev * __devinit pci_scan_device(struct pci_dev *temp)
{
	struct pci_dev *dev;
	u32 l;

	if (pci_read_config_dword(temp, PCI_VENDOR_ID, &l))
		return NULL;

	/* some broken boards return 0 or ~0 if a slot is empty: */
	if (l == 0xffffffff || l == 0x00000000 || l == 0x0000ffff || l == 0xffff0000)
		return NULL;

	dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	memcpy(dev, temp, sizeof(*dev));
	dev->vendor = l & 0xffff;
	dev->device = (l >> 16) & 0xffff;

	/* Assume 32-bit PCI; let 64-bit PCI cards (which are far rarer)
	   set this higher, assuming the system even supports it.  */
	dev->dma_mask = 0xffffffff;
	if (pci_setup_device(dev) < 0) {
		kfree(dev);
		dev = NULL;
	}
	return dev;
}

struct pci_dev * __devinit pci_scan_slot(struct pci_dev *temp)
{
	struct pci_bus *bus = temp->bus;
	struct pci_dev *dev;
	struct pci_dev *first_dev = NULL;
	int func = 0;
	int is_multi = 0;
	u8 hdr_type;

	for (func = 0; func < 8; func++, temp->devfn++) {
		if (pci_read_config_byte(temp, PCI_HEADER_TYPE, &hdr_type))
			continue;
		temp->hdr_type = hdr_type & 0x7f;

		dev = pci_scan_device(temp);
		if (!pcibios_scan_all_fns() && func == 0) {
			if (!dev)
				break;
		} else {
			if (!dev)
				continue;
			is_multi = 1;
		}

		pci_name_device(dev);
		if (!first_dev) {
			is_multi = hdr_type & 0x80;
			first_dev = dev;
		}

		/*
		 * Link the device to both the global PCI device chain and
		 * the per-bus list of devices.
		 */
		list_add_tail(&dev->global_list, &pci_devices);
		list_add_tail(&dev->bus_list, &bus->devices);

		/* Fix up broken headers */
		pci_fixup_device(PCI_FIXUP_HEADER, dev);

		/*
		 * If this is a single function device
		 * don't scan past the first function.
		 */
		if (!is_multi)
			break;

	}
	return first_dev;
}

unsigned int __devinit pci_do_scan_bus(struct pci_bus *bus)
{
	unsigned int devfn, max, pass;
	struct list_head *ln;
	struct pci_dev *dev, dev0;

	DBG("Scanning bus %02x\n", bus->number);
	max = bus->secondary;

	/* Create a device template */
	memset(&dev0, 0, sizeof(dev0));
	dev0.bus = bus;
	dev0.sysdata = bus->sysdata;

	/* Go find them, Rover! */
	for (devfn = 0; devfn < 0x100; devfn += 8) {
		dev0.devfn = devfn;
		pci_scan_slot(&dev0);
	}

	/*
	 * After performing arch-dependent fixup of the bus, look behind
	 * all PCI-to-PCI bridges on this bus.
	 */
	DBG("Fixups for bus %02x\n", bus->number);
	pcibios_fixup_bus(bus);
	for (pass=0; pass < 2; pass++)
		for (ln=bus->devices.next; ln != &bus->devices; ln=ln->next) {
			dev = pci_dev_b(ln);
			if (dev->hdr_type == PCI_HEADER_TYPE_BRIDGE || dev->hdr_type == PCI_HEADER_TYPE_CARDBUS)
				max = pci_scan_bridge(bus, dev, max, pass);
		}

	/*
	 * We've scanned the bus and so we know all about what's on
	 * the other side of any bridges that may be on this bus plus
	 * any devices.
	 *
	 * Return how far we've got finding sub-buses.
	 */
	DBG("Bus scan for %02x returning with max=%02x\n", bus->number, max);
	return max;
}

int __devinit pci_bus_exists(const struct list_head *list, int nr)
{
	const struct list_head *l;

	for(l=list->next; l != list; l = l->next) {
		const struct pci_bus *b = pci_bus_b(l);
		if (b->number == nr || pci_bus_exists(&b->children, nr))
			return 1;
	}
	return 0;
}

struct pci_bus * __devinit pci_alloc_primary_bus(int bus)
{
	struct pci_bus *b;

	if (pci_bus_exists(&pci_root_buses, bus)) {
		/* If we already got to this bus through a different bridge, ignore it */
		DBG("PCI: Bus %02x already known\n", bus);
		return NULL;
	}

	b = pci_alloc_bus();
	list_add_tail(&b->node, &pci_root_buses);

	b->number = b->secondary = bus;
	b->resource[0] = &ioport_resource;
	b->resource[1] = &iomem_resource;
	return b;
}

struct pci_bus * __devinit pci_scan_bus(int bus, struct pci_ops *ops, void *sysdata)
{
	struct pci_bus *b = pci_alloc_primary_bus(bus);
	if (b) {
		b->sysdata = sysdata;
		b->ops = ops;
		b->subordinate = pci_do_scan_bus(b);
	}
	return b;
}

#ifdef CONFIG_PM

/*
 * PCI Power management..
 *
 * This needs to be done centralized, so that we power manage PCI
 * devices in the right order: we should not shut down PCI bridges
 * before we've shut down the devices behind them, and we should
 * not wake up devices before we've woken up the bridge to the
 * device.. Eh?
 *
 * We do not touch devices that don't have a driver that exports
 * a suspend/resume function. That is just too dangerous. If the default
 * PCI suspend/resume functions work for a device, the driver can
 * easily implement them (ie just have a suspend function that calls
 * the pci_set_power_state() function).
 */

static int pci_pm_save_state_device(struct pci_dev *dev, u32 state)
{
	int error = 0;
	if (dev) {
		struct pci_driver *driver = dev->driver;
		if (driver && driver->save_state) 
			error = driver->save_state(dev,state);
	}
	return error;
}

static int pci_pm_suspend_device(struct pci_dev *dev, u32 state)
{
	int error = 0;
	if (dev) {
		struct pci_driver *driver = dev->driver;
		if (driver && driver->suspend)
			error = driver->suspend(dev,state);
	}
	return error;
}

static int pci_pm_resume_device(struct pci_dev *dev)
{
	int error = 0;
	if (dev) {
		struct pci_driver *driver = dev->driver;
		if (driver && driver->resume)
			error = driver->resume(dev);
	}
	return error;
}

static int pci_pm_save_state_bus(struct pci_bus *bus, u32 state)
{
	struct list_head *list;
	int error = 0;

	list_for_each(list, &bus->children) {
		error = pci_pm_save_state_bus(pci_bus_b(list),state);
		if (error) return error;
	}
	list_for_each(list, &bus->devices) {
		error = pci_pm_save_state_device(pci_dev_b(list),state);
		if (error) return error;
	}
	return 0;
}

static int pci_pm_suspend_bus(struct pci_bus *bus, u32 state)
{
	struct list_head *list;

	/* Walk the bus children list */
	list_for_each(list, &bus->children) 
		pci_pm_suspend_bus(pci_bus_b(list),state);

	/* Walk the device children list */
	list_for_each(list, &bus->devices)
		pci_pm_suspend_device(pci_dev_b(list),state);
	return 0;
}

static int pci_pm_resume_bus(struct pci_bus *bus)
{
	struct list_head *list;

	/* Walk the device children list */
	list_for_each(list, &bus->devices)
		pci_pm_resume_device(pci_dev_b(list));

	/* And then walk the bus children */
	list_for_each(list, &bus->children)
		pci_pm_resume_bus(pci_bus_b(list));
	return 0;
}

static int pci_pm_save_state(u32 state)
{
	struct list_head *list;
	struct pci_bus *bus;
	int error = 0;

	list_for_each(list, &pci_root_buses) {
		bus = pci_bus_b(list);
		error = pci_pm_save_state_bus(bus,state);
		if (!error)
			error = pci_pm_save_state_device(bus->self,state);
	}
	return error;
}

static int pci_pm_suspend(u32 state)
{
	struct list_head *list;
	struct pci_bus *bus;

	list_for_each(list, &pci_root_buses) {
		bus = pci_bus_b(list);
		pci_pm_suspend_bus(bus,state);
		pci_pm_suspend_device(bus->self,state);
	}
	return 0;
}

int pci_pm_resume(void)
{
	struct list_head *list;
	struct pci_bus *bus;

	list_for_each(list, &pci_root_buses) {
		bus = pci_bus_b(list);
		pci_pm_resume_device(bus->self);
		pci_pm_resume_bus(bus);
	}
	return 0;
}

static int 
pci_pm_callback(struct pm_dev *pm_device, pm_request_t rqst, void *data)
{
	int error = 0;

	switch (rqst) {
	case PM_SAVE_STATE:
		error = pci_pm_save_state((unsigned long)data);
		break;
	case PM_SUSPEND:
		error = pci_pm_suspend((unsigned long)data);
		break;
	case PM_RESUME:
		error = pci_pm_resume();
		break;
	default: break;
	}
	return error;
}

#endif

/*
 * Pool allocator ... wraps the pci_alloc_consistent page allocator, so
 * small blocks are easily used by drivers for bus mastering controllers.
 * This should probably be sharing the guts of the slab allocator.
 */

struct pci_pool {	/* the pool */
	struct list_head	page_list;
	spinlock_t		lock;
	size_t			blocks_per_page;
	size_t			size;
	int			flags;
	struct pci_dev		*dev;
	size_t			allocation;
	char			name [32];
	wait_queue_head_t	waitq;
};

struct pci_page {	/* cacheable header for 'allocation' bytes */
	struct list_head	page_list;
	void			*vaddr;
	dma_addr_t		dma;
	unsigned long		bitmap [0];
};

#define	POOL_TIMEOUT_JIFFIES	((100 /* msec */ * HZ) / 1000)
#define	POOL_POISON_BYTE	0xa7

// #define CONFIG_PCIPOOL_DEBUG


/**
 * pci_pool_create - Creates a pool of pci consistent memory blocks, for dma.
 * @name: name of pool, for diagnostics
 * @pdev: pci device that will be doing the DMA
 * @size: size of the blocks in this pool.
 * @align: alignment requirement for blocks; must be a power of two
 * @allocation: returned blocks won't cross this boundary (or zero)
 * @flags: SLAB_* flags (not all are supported).
 *
 * Returns a pci allocation pool with the requested characteristics, or
 * null if one can't be created.  Given one of these pools, pci_pool_alloc()
 * may be used to allocate memory.  Such memory will all have "consistent"
 * DMA mappings, accessible by the device and its driver without using
 * cache flushing primitives.  The actual size of blocks allocated may be
 * larger than requested because of alignment.
 *
 * If allocation is nonzero, objects returned from pci_pool_alloc() won't
 * cross that size boundary.  This is useful for devices which have
 * addressing restrictions on individual DMA transfers, such as not crossing
 * boundaries of 4KBytes.
 */
struct pci_pool *
pci_pool_create (const char *name, struct pci_dev *pdev,
	size_t size, size_t align, size_t allocation, int flags)
{
	struct pci_pool		*retval;

	if (align == 0)
		align = 1;
	if (size == 0)
		return 0;
	else if (size < align)
		size = align;
	else if ((size % align) != 0) {
		size += align + 1;
		size &= ~(align - 1);
	}

	if (allocation == 0) {
		if (PAGE_SIZE < size)
			allocation = size;
		else
			allocation = PAGE_SIZE;
		// FIXME: round up for less fragmentation
	} else if (allocation < size)
		return 0;

	if (!(retval = kmalloc (sizeof *retval, flags)))
		return retval;

#ifdef	CONFIG_PCIPOOL_DEBUG
	flags |= SLAB_POISON;
#endif

	strncpy (retval->name, name, sizeof retval->name);
	retval->name [sizeof retval->name - 1] = 0;

	retval->dev = pdev;
	INIT_LIST_HEAD (&retval->page_list);
	spin_lock_init (&retval->lock);
	retval->size = size;
	retval->flags = flags;
	retval->allocation = allocation;
	retval->blocks_per_page = allocation / size;
	init_waitqueue_head (&retval->waitq);

#ifdef CONFIG_PCIPOOL_DEBUG
	printk (KERN_DEBUG "pcipool create %s/%s size %d, %d/page (%d alloc)\n",
		pdev ? pdev->slot_name : NULL, retval->name, size,
		retval->blocks_per_page, allocation);
#endif

	return retval;
}


static struct pci_page *
pool_alloc_page (struct pci_pool *pool, int mem_flags)
{
	struct pci_page	*page;
	int		mapsize;

	mapsize = pool->blocks_per_page;
	mapsize = (mapsize + BITS_PER_LONG - 1) / BITS_PER_LONG;
	mapsize *= sizeof (long);

	page = (struct pci_page *) kmalloc (mapsize + sizeof *page, mem_flags);
	if (!page)
		return 0;
	page->vaddr = pci_alloc_consistent (pool->dev,
					    pool->allocation,
					    &page->dma);
	if (page->vaddr) {
		memset (page->bitmap, 0xff, mapsize);	// bit set == free
		if (pool->flags & SLAB_POISON)
			memset (page->vaddr, POOL_POISON_BYTE, pool->allocation);
		list_add (&page->page_list, &pool->page_list);
	} else {
		kfree (page);
		page = 0;
	}
	return page;
}


static inline int
is_page_busy (int blocks, unsigned long *bitmap)
{
	while (blocks > 0) {
		if (*bitmap++ != ~0UL)
			return 1;
		blocks -= BITS_PER_LONG;
	}
	return 0;
}

static void
pool_free_page (struct pci_pool *pool, struct pci_page *page)
{
	dma_addr_t	dma = page->dma;

	if (pool->flags & SLAB_POISON)
		memset (page->vaddr, POOL_POISON_BYTE, pool->allocation);
	pci_free_consistent (pool->dev, pool->allocation, page->vaddr, dma);
	list_del (&page->page_list);
	kfree (page);
}


/**
 * pci_pool_destroy - destroys a pool of pci memory blocks.
 * @pool: pci pool that will be destroyed
 *
 * Caller guarantees that no more memory from the pool is in use,
 * and that nothing will try to use the pool after this call.
 */
void
pci_pool_destroy (struct pci_pool *pool)
{
	unsigned long		flags;

#ifdef CONFIG_PCIPOOL_DEBUG
	printk (KERN_DEBUG "pcipool destroy %s/%s\n",
		pool->dev ? pool->dev->slot_name : NULL,
		pool->name);
#endif

	spin_lock_irqsave (&pool->lock, flags);
	while (!list_empty (&pool->page_list)) {
		struct pci_page		*page;
		page = list_entry (pool->page_list.next,
				struct pci_page, page_list);
		if (is_page_busy (pool->blocks_per_page, page->bitmap)) {
			printk (KERN_ERR "pci_pool_destroy %s/%s, %p busy\n",
				pool->dev ? pool->dev->slot_name : NULL,
				pool->name, page->vaddr);
			/* leak the still-in-use consistent memory */
			list_del (&page->page_list);
			kfree (page);
		} else
			pool_free_page (pool, page);
	}
	spin_unlock_irqrestore (&pool->lock, flags);
	kfree (pool);
}


/**
 * pci_pool_alloc - get a block of consistent memory
 * @pool: pci pool that will produce the block
 * @mem_flags: SLAB_KERNEL or SLAB_ATOMIC
 * @handle: pointer to dma address of block
 *
 * This returns the kernel virtual address of a currently unused block,
 * and reports its dma address through the handle.
 * If such a memory block can't be allocated, null is returned.
 */
void *
pci_pool_alloc (struct pci_pool *pool, int mem_flags, dma_addr_t *handle)
{
	unsigned long		flags;
	struct list_head	*entry;
	struct pci_page		*page;
	int			map, block;
	size_t			offset;
	void			*retval;

restart:
	spin_lock_irqsave (&pool->lock, flags);
	list_for_each (entry, &pool->page_list) {
		int		i;
		page = list_entry (entry, struct pci_page, page_list);
		/* only cachable accesses here ... */
		for (map = 0, i = 0;
				i < pool->blocks_per_page;
				i += BITS_PER_LONG, map++) {
			if (page->bitmap [map] == 0)
				continue;
			block = ffz (~ page->bitmap [map]);
			if ((i + block) < pool->blocks_per_page) {
				clear_bit (block, &page->bitmap [map]);
				offset = (BITS_PER_LONG * map) + block;
				offset *= pool->size;
				goto ready;
			}
		}
	}
	if (!(page = pool_alloc_page (pool, mem_flags))) {
		if (mem_flags == SLAB_KERNEL) {
			DECLARE_WAITQUEUE (wait, current);

			current->state = TASK_INTERRUPTIBLE;
			add_wait_queue (&pool->waitq, &wait);
			spin_unlock_irqrestore (&pool->lock, flags);

			schedule_timeout (POOL_TIMEOUT_JIFFIES);

			current->state = TASK_RUNNING;
			remove_wait_queue (&pool->waitq, &wait);
			goto restart;
		}
		retval = 0;
		goto done;
	}

	clear_bit (0, &page->bitmap [0]);
	offset = 0;
ready:
	retval = offset + page->vaddr;
	*handle = offset + page->dma;
done:
	spin_unlock_irqrestore (&pool->lock, flags);
	return retval;
}


static struct pci_page *
pool_find_page (struct pci_pool *pool, dma_addr_t dma)
{
	unsigned long		flags;
	struct list_head	*entry;
	struct pci_page		*page;

	spin_lock_irqsave (&pool->lock, flags);
	list_for_each (entry, &pool->page_list) {
		page = list_entry (entry, struct pci_page, page_list);
		if (dma < page->dma)
			continue;
		if (dma < (page->dma + pool->allocation))
			goto done;
	}
	page = 0;
done:
	spin_unlock_irqrestore (&pool->lock, flags);
	return page;
}


/**
 * pci_pool_free - put block back into pci pool
 * @pool: the pci pool holding the block
 * @vaddr: virtual address of block
 * @dma: dma address of block
 *
 * Caller promises neither device nor driver will again touch this block
 * unless it is first re-allocated.
 */
void
pci_pool_free (struct pci_pool *pool, void *vaddr, dma_addr_t dma)
{
	struct pci_page		*page;
	unsigned long		flags;
	int			map, block;

	if ((page = pool_find_page (pool, dma)) == 0) {
		printk (KERN_ERR "pci_pool_free %s/%s, %p/%x (bad dma)\n",
			pool->dev ? pool->dev->slot_name : NULL,
			pool->name, vaddr, (int) (dma & 0xffffffff));
		return;
	}
#ifdef	CONFIG_PCIPOOL_DEBUG
	if (((dma - page->dma) + (void *)page->vaddr) != vaddr) {
		printk (KERN_ERR "pci_pool_free %s/%s, %p (bad vaddr)/%x\n",
			pool->dev ? pool->dev->slot_name : NULL,
			pool->name, vaddr, (int) (dma & 0xffffffff));
		return;
	}
#endif

	block = dma - page->dma;
	block /= pool->size;
	map = block / BITS_PER_LONG;
	block %= BITS_PER_LONG;

#ifdef	CONFIG_PCIPOOL_DEBUG
	if (page->bitmap [map] & (1UL << block)) {
		printk (KERN_ERR "pci_pool_free %s/%s, dma %x already free\n",
			pool->dev ? pool->dev->slot_name : NULL,
			pool->name, dma);
		return;
	}
#endif
	if (pool->flags & SLAB_POISON)
		memset (vaddr, POOL_POISON_BYTE, pool->size);

	spin_lock_irqsave (&pool->lock, flags);
	set_bit (block, &page->bitmap [map]);
	if (waitqueue_active (&pool->waitq))
		wake_up (&pool->waitq);
	/*
	 * Resist a temptation to do
	 *    if (!is_page_busy(bpp, page->bitmap)) pool_free_page(pool, page);
	 * it is not interrupt safe. Better have empty pages hang around.
	 */
	spin_unlock_irqrestore (&pool->lock, flags);
}


void __devinit  pci_init(void)
{
	struct pci_dev *dev;

	pcibios_init();

	pci_for_each_dev(dev) {
		pci_fixup_device(PCI_FIXUP_FINAL, dev);
	}

#ifdef CONFIG_PM
	pm_register(PM_PCI_DEV, 0, pci_pm_callback);
#endif
}

static int __devinit pci_setup(char *str)
{
	while (str) {
		char *k = strchr(str, ',');
		if (k)
			*k++ = 0;
		if (*str && (str = pcibios_setup(str)) && *str) {
			/* PCI layer options should be handled here */
			printk(KERN_ERR "PCI: Unknown option `%s'\n", str);
		}
		str = k;
	}
	return 1;
}

__setup("pci=", pci_setup);

EXPORT_SYMBOL(pci_read_config_byte);
EXPORT_SYMBOL(pci_read_config_word);
EXPORT_SYMBOL(pci_read_config_dword);
EXPORT_SYMBOL(pci_write_config_byte);
EXPORT_SYMBOL(pci_write_config_word);
EXPORT_SYMBOL(pci_write_config_dword);
EXPORT_SYMBOL(pci_devices);
EXPORT_SYMBOL(pci_root_buses);
EXPORT_SYMBOL(pci_enable_device_bars);
EXPORT_SYMBOL(pci_enable_device);
EXPORT_SYMBOL(pci_disable_device);
EXPORT_SYMBOL(pci_find_capability);
EXPORT_SYMBOL(pci_release_regions);
EXPORT_SYMBOL(pci_request_regions);
EXPORT_SYMBOL(pci_release_region);
EXPORT_SYMBOL(pci_request_region);
EXPORT_SYMBOL(pci_find_class);
EXPORT_SYMBOL(pci_find_device);
EXPORT_SYMBOL(pci_find_slot);
EXPORT_SYMBOL(pci_find_subsys);
EXPORT_SYMBOL(pci_set_master);
EXPORT_SYMBOL(pci_set_mwi);
EXPORT_SYMBOL(pci_clear_mwi);
EXPORT_SYMBOL(pci_set_dma_mask);
EXPORT_SYMBOL(pci_dac_set_dma_mask);
EXPORT_SYMBOL(pci_assign_resource);
EXPORT_SYMBOL(pci_register_driver);
EXPORT_SYMBOL(pci_unregister_driver);
EXPORT_SYMBOL(pci_dev_driver);
EXPORT_SYMBOL(pci_match_device);
EXPORT_SYMBOL(pci_find_parent_resource);

#ifdef CONFIG_HOTPLUG
EXPORT_SYMBOL(pci_setup_device);
EXPORT_SYMBOL(pci_insert_device);
EXPORT_SYMBOL(pci_remove_device);
EXPORT_SYMBOL(pci_announce_device_to_drivers);
EXPORT_SYMBOL(pci_add_new_bus);
EXPORT_SYMBOL(pci_do_scan_bus);
EXPORT_SYMBOL(pci_scan_slot);
EXPORT_SYMBOL(pci_scan_bus);
EXPORT_SYMBOL(pci_scan_device);
EXPORT_SYMBOL(pci_read_bridge_bases);
#ifdef CONFIG_PROC_FS
EXPORT_SYMBOL(pci_proc_attach_device);
EXPORT_SYMBOL(pci_proc_detach_device);
EXPORT_SYMBOL(pci_proc_attach_bus);
EXPORT_SYMBOL(pci_proc_detach_bus);
EXPORT_SYMBOL(proc_bus_pci_dir);
#endif
#endif

EXPORT_SYMBOL(pci_set_power_state);
EXPORT_SYMBOL(pci_save_state);
EXPORT_SYMBOL(pci_restore_state);
EXPORT_SYMBOL(pci_enable_wake);

/* Obsolete functions */

EXPORT_SYMBOL(pcibios_present);
EXPORT_SYMBOL(pcibios_read_config_byte);
EXPORT_SYMBOL(pcibios_read_config_word);
EXPORT_SYMBOL(pcibios_read_config_dword);
EXPORT_SYMBOL(pcibios_write_config_byte);
EXPORT_SYMBOL(pcibios_write_config_word);
EXPORT_SYMBOL(pcibios_write_config_dword);
EXPORT_SYMBOL(pcibios_find_class);
EXPORT_SYMBOL(pcibios_find_device);

/* Quirk info */

EXPORT_SYMBOL(isa_dma_bridge_buggy);
EXPORT_SYMBOL(pci_pci_problems);

/* Pool allocator */

EXPORT_SYMBOL (pci_pool_create);
EXPORT_SYMBOL (pci_pool_destroy);
EXPORT_SYMBOL (pci_pool_alloc);
EXPORT_SYMBOL (pci_pool_free);

