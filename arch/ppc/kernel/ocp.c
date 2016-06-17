/*
 * ocp.c
 *
 *      (c) Benjamin Herrenschmidt (benh@kernel.crashing.org)
 *          Mipsys - France
 *
 "          Derived from work (c) Armin Kuster akuster@pacbell.net
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR   IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/bootmem.h>
#include <asm/io.h>
#include <asm/ocp.h>
#include <asm/errno.h>
#include <asm/rwsem.h>
#include <asm/semaphore.h>

//#define DBG(x)	printk x
#define DBG(x)

extern int mem_init_done;

extern struct ocp_def core_ocp[];	/* Static list of devices, provided by
					   CPU core */

LIST_HEAD(ocp_devices);			/* List of all OCP devices */
LIST_HEAD(ocp_drivers);			/* List of all OCP drivers */
DECLARE_RWSEM(ocp_devices_sem);		/* Global semaphores for those lists */
DECLARE_MUTEX(ocp_drivers_sem);		/* Global semaphores for those lists */

static int ocp_inited;

/**
 *	ocp_driver_match	-	Match one driver to one device
 *	@drv: driver to match
 *	@dev: device to match
 *
 *	This function returns 0 if the driver and device don't match
 */
static int
ocp_driver_match(struct ocp_driver *drv, struct ocp_device *dev)
{
	const struct ocp_device_id *ids = drv->id_table;

	if (!ids)
		return 0;

	while (ids->vendor || ids->function) {
		if ((ids->vendor == OCP_ANY_ID
		     || ids->vendor == dev->def->vendor)
		    && (ids->function == OCP_ANY_ID
			|| ids->function == dev->def->function))
		        return 1;
		ids++;
	}
	return 0;
}


/**
 *	ocp_bind_drivers	-	Match all drivers with all devices
 *	@candidate: driver beeing registered
 *
 *	This function is called on driver registration and device discovery,
 *	it redo the matching of all "driverless" devices with all possible
 *	driver candidates.
 *	The driver beeing registered can be optionally passed in, in which
 *	case, the function will return -ENODEV is no match have been found
 *	or if all matches failed with a different code than -EAGAIN
 */
static int
ocp_bind_drivers(struct ocp_driver *candidate)
{
	struct list_head	*deventry, *drventry;
	struct ocp_device	*dev;
	struct ocp_driver	*drv;
	int			one_again, one_match;
	int			count = 0;

	DBG(("ocp: binding drivers...\n"));
	do {
		/* We re-do the match loop if we had a sucess match and got one -EAGAIN
		 */
		one_match = one_again = 0;
		down_read(&ocp_devices_sem);
		list_for_each(deventry, &ocp_devices) {
			dev = list_entry(deventry, struct ocp_device, link);
			if (dev->driver != NULL)
				continue;
			DBG(("ocp: device %s unmatched, trying to match...\n", dev->name));
			list_for_each(drventry, &ocp_drivers) {
				drv = list_entry(drventry, struct ocp_driver, link);
				if (ocp_driver_match(drv, dev)) {
					int rc;

					/* Hrm... shall we set dev->driver after or before ? */
					DBG(("ocp: match with driver %s, calling probe...\n", drv->name));
					rc = drv->probe(dev);
					DBG(("ocp: probe result: %d\n", rc));
					if (rc == 0) {
						/* Driver matched, next device */
						dev->driver = drv;
						one_match = 1;
						if (drv == candidate)
							count++;
						break;
					} else if (rc == -EAGAIN) {
						/* Driver matched but asked for later call, next device */
						one_again = 1;
						if (drv == candidate)
							count++;
						break;
					}
				}
			}
		}
		up_read(&ocp_devices_sem);
	} while(one_match && one_again);
	DBG(("ocp: binding drivers... done.\n"));

	return count;
}

/**
 *	ocp_register_driver	-	Register an OCP driver
 *	@drv: pointer to statically defined ocp_driver structure
 *
 *	The driver's probe() callback is called either recursively
 *	by this function or upon later call of ocp_driver_init
 *
 *      NOTE: Probe is called with ocp_drivers_sem held, it shouldn't
 *      call ocp_register/unregister_driver on his own code path.
 *      however, it _can_ call ocp_find_device().
 *
 *	NOTE2: Detection of devices is a 2 pass step on this implementation,
 *	hotswap isn't supported. First, all OCP devices are put in the device
 *	list, _then_ all drivers are probed on each match.
 *
 *      NOTE3: Drivers are allowed to return -EAGAIN from the probe() routine.
 *      this will cause them to be called again for this specific device as
 *	soon as another device have been probed or another driver registered.
 *	this, gives a simple way for a driver like EMAC to wait for another driver,
 *	like MAL to be up. There is potentially a small race if MAL happens to
 *	unregister, but this is hopefully never happening.
 *
 *	This function returns a count of how many devices actually matched
 *	(wether the probe routine returned 0 or -EAGAIN, a different error
 *	code isn't considered as a match).
 */

int
ocp_register_driver(struct ocp_driver *drv)
{
	int	rc = 0;

	DBG(("ocp: ocp_register_driver(%s)...\n", drv->name));

	/* Add to driver list */
	down(&ocp_drivers_sem);
	list_add_tail(&drv->link, &ocp_drivers);

	/* Check matching devices */
	rc = ocp_bind_drivers(drv);

	up(&ocp_drivers_sem);

	DBG(("ocp: ocp_register_driver(%s)... done, count: %d.\n", drv->name, rc));

	return rc;
}

/**
 *	ocp_unregister_driver	-	Unregister an OCP driver
 *	@drv: pointer to statically defined ocp_driver structure
 *
 *	The driver's remove() callback is called recursively
 *	by this function for any device already registered
 */

void
ocp_unregister_driver(struct ocp_driver *drv)
{
	struct ocp_device	*dev;
	struct list_head	*entry;

	DBG(("ocp: ocp_unregister_driver(%s)...\n", drv->name));

	/* Call remove() routine for all devices using it */
	down(&ocp_drivers_sem);
	down_read(&ocp_devices_sem);
	list_for_each(entry, &ocp_devices) {
		dev = list_entry(entry, struct ocp_device, link);
		if (dev->driver == drv) {
			drv->remove(dev);
			dev->driver = NULL;
			dev->drvdata = NULL;
		}
	}
	up_read(&ocp_devices_sem);

	/* Unlink driver structure */
	list_del_init(&drv->link);
	up(&ocp_drivers_sem);

	DBG(("ocp: ocp_unregister_driver(%s)... done.\n", drv->name));
}

/* Core of ocp_find_device(). Caller must hold ocp_devices_sem */
static struct ocp_device *
__ocp_find_device(unsigned int vendor, unsigned int function, int index)
{
	struct list_head	*entry;
	struct ocp_device	*dev, *found = NULL;

	DBG(("ocp: __ocp_find_device(vendor: %x, function: %x, index: %d)...\n", vendor, function, index));

	list_for_each(entry, &ocp_devices) {
		dev = list_entry(entry, struct ocp_device, link);
		if (vendor != OCP_ANY_ID && vendor != dev->def->vendor)
			continue;
		if (function != OCP_ANY_ID && function != dev->def->function)
			continue;
		if (index != OCP_ANY_INDEX && index != dev->def->index)
			continue;
		found = dev;
		break;
	}

	DBG(("ocp: __ocp_find_device(vendor: %x, function: %x, index: %d)... done\n", vendor, function, index));

	return found;
}

/**
 *	ocp_find_device	-	Find a device by function & index
 *      @vendor: vendor ID of the device (or OCP_ANY_ID)
 *	@function: function code of the device (or OCP_ANY_ID)
 *	@idx: index of the device (or OCP_ANY_INDEX)
 *
 *	This function allows a lookup of a given function by it's
 *	index, it's typically used to find the MAL or ZMII associated
 *	with an EMAC or similar horrors.
 *      You can pass vendor, though you usually want OCP_ANY_ID there...
 */
struct ocp_device *
ocp_find_device(unsigned int vendor, unsigned int function, int index)
{
	struct ocp_device	*dev;

	down_read(&ocp_devices_sem);
	dev = __ocp_find_device(vendor, function, index);
	up_read(&ocp_devices_sem);

	return dev;
}

/**
 *	ocp_get_one_device -	Find a def by function & index
 *      @vendor: vendor ID of the device (or OCP_ANY_ID)
 *	@function: function code of the device (or OCP_ANY_ID)
 *	@idx: index of the device (or OCP_ANY_INDEX)
 *
 *	This function allows a lookup of a given ocp_def by it's
 *	vendor, function, and index.  The main purpose for is to
 *	allow modification of the def before binding to the driver
 */
struct ocp_def *
ocp_get_one_device(unsigned int vendor, unsigned int function, int index)
{
	struct ocp_device	*dev;
	struct ocp_def		*found = NULL;

	DBG(("ocp: ocp_get_one_device(vendor: %x, function: %x, index: %d)...\n",
		vendor, function, index));

	dev = ocp_find_device(vendor, function, index);

	if (dev) 
		found = dev->def;

	DBG(("ocp: ocp_get_one_device(vendor: %x, function: %x, index: %d)... done.\n",
		vendor, function, index));

	return found;
}

/**
 *	ocp_add_one_device	-	Add a device
 *	@def: static device definition structure
 *
 *	This function adds a device definition to the
 *	device list. It may only be called before
 *	ocp_driver_init() and will return an error
 *	otherwise.
 */
int
ocp_add_one_device(struct ocp_def *def)
{
	struct	ocp_device	*dev;

	DBG(("ocp: ocp_add_one_device(vendor: %x, function: %x, index: %d)...\n", vendor, function, index));

	/* Can't be called after ocp driver init */
	if (ocp_inited)
		return 1;

	if (mem_init_done)
		dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	else
		dev = alloc_bootmem(sizeof(*dev));

	if (dev == NULL)
		return 1;
	memset(dev, 0, sizeof(*dev));
	dev->def = def;
	dev->current_state = 4;
	sprintf(dev->name, "OCP device %04x:%04x:%04x",
		dev->def->vendor, dev->def->function, dev->def->index);
	down_write(&ocp_devices_sem);
	list_add_tail(&dev->link, &ocp_devices);
	up_write(&ocp_devices_sem);

	DBG(("ocp: ocp_add_one_device(vendor: %x, function: %x, index: %d)...done.\n", vendor, function, index));

	return 0;
}

/**
 *	ocp_remove_one_device -	Remove a device by function & index
 *      @vendor: vendor ID of the device (or OCP_ANY_ID)
 *	@function: function code of the device (or OCP_ANY_ID)
 *	@idx: index of the device (or OCP_ANY_INDEX)
 *
 *	This function allows removal of a given function by its
 *	index. It may only be called before ocp_driver_init()
 *	and will return an error otherwise.
 */
int
ocp_remove_one_device(unsigned int vendor, unsigned int function, int index)
{
	struct ocp_device *dev;
	int	rc = 0;

	DBG(("ocp: ocp_remove_one_device(vendor: %x, function: %x, index: %d)...\n", vendor, function, index));

	/* Can't be called after ocp driver init */
	if (ocp_inited)
		return 1;

	down_write(&ocp_devices_sem);
	dev = __ocp_find_device(vendor, function, index);
	if (dev != NULL)
		list_del((struct list_head *)dev);
	else
		rc = 1;
	up_write(&ocp_devices_sem);

	DBG(("ocp: ocp_remove_one_device(vendor: %x, function: %x, index: %d)... done.\n", vendor, function, index));

	return rc;
}

#ifdef CONFIG_PM
/**
 * OCP Power management..
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
 *
 * BenH: Implementation here couldn't work properly. This version
 *       slightly modified and _might_ be more useable, but real
 *       PM support will probably have to wait for 2.5
 */

static int ocp_pm_save_state_device(struct ocp_device *dev, u32 state)
{
	int error = 0;
	if (dev) {
		struct ocp_driver *driver = dev->driver;
		if (driver && driver->save_state)
			error = driver->save_state(dev,state);
	}
	return error;
}

static int ocp_pm_suspend_device(struct ocp_device *dev, u32 state)
{
	int error = 0;
	if (dev) {
		struct ocp_driver *driver = dev->driver;
		if (driver && driver->suspend)
			error = driver->suspend(dev,state);
	}
	return error;
}

static int ocp_pm_resume_device(struct ocp_device *dev)
{
	int error = 0;
	if (dev) {
		struct ocp_driver *driver = dev->driver;
		if (driver && driver->resume)
			error = driver->resume(dev);
	}
	return error;
}

static int
ocp_pm_callback(struct pm_dev *pm_device, pm_request_t rqst, void *data)
{
	int error = 0;
	struct list_head	*entry;
	struct ocp_device	*dev;

	down(&ocp_drivers_sem);
	down_read(&ocp_devices_sem);

	list_for_each(entry, &ocp_devices) {
		dev = list_entry(entry, struct ocp_device, link);
		switch (rqst) {
		case PM_SAVE_STATE:
			error = ocp_pm_save_state_device(dev, 3);
			break;
		case PM_SUSPEND:
			error = ocp_pm_suspend_device(dev, 3);
			break;
		case PM_RESUME:
			error = ocp_pm_resume_device(dev);
			break;
		default: break;
		}
		if (error)
			break;
	}
	return error;
}

/*
 * Is this ever used ?
 */
void
ppc4xx_cpm_fr(u32 bits, int val)
{
	unsigned long flags;

	save_flags(flags);
	cli();

	if (val)
		mtdcr(DCRN_CPMFR, mfdcr(DCRN_CPMFR) | bits);
	else
		mtdcr(DCRN_CPMFR, mfdcr(DCRN_CPMFR) & ~bits);

	restore_flags(flags);
}
#endif /* CONFIG_PM */

/**
 *	ocp_early_init	-	Init OCP device management
 *
 *	This function builds the list of devices before setup_arch. 
 *	This allows platform code to modify the device lists before
 *	they are bound to drivers (changes to paddr, removing devices
 *	etc)
 */
int __init
ocp_early_init(void)
{
	struct ocp_def	*def;

	DBG(("ocp: ocp_early_init()...\n"));

	/* Fill the devices list */
	for (def = core_ocp; def->vendor != OCP_VENDOR_INVALID; def++)
		ocp_add_one_device(def);

	DBG(("ocp: ocp_early_init()... done.\n"));

	return 0;
}

/**
 *	ocp_driver_init	-	Init OCP device management
 *
 *	This function is meant to be called once, and only once to initialize
 *	the OCP device management. Note that it can actually be called at any 
 *	time, it's perfectly legal to register drivers before 
 *	ocp_driver_init() is called
 */
int
ocp_driver_init(void)
{
	/* ocp_driver_init is by default an initcall. If your arch requires 
	 * this to be called earlier, then go on, ocp_driver_init is 
	 * non-static for that purpose, and can safely be called twice
	 */
	if (ocp_inited)
		return 0;
	ocp_inited = 1;

	DBG(("ocp: ocp_driver_init()...\n"));

	/* Call drivers probes */
	down(&ocp_drivers_sem);
	ocp_bind_drivers(NULL);
	up(&ocp_drivers_sem);

#ifdef CONFIG_PM
	pm_register(PM_SYS_DEV, 0, ocp_pm_callback);
#endif

	DBG(("ocp: ocp_driver_init()... done.\n"));

	return 0;
}

__initcall(ocp_driver_init);

EXPORT_SYMBOL(ocp_find_device);
EXPORT_SYMBOL(ocp_register_driver);
EXPORT_SYMBOL(ocp_unregister_driver);
