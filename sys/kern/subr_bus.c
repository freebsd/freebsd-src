/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1997,1998,2003 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_bus.h"
#include "opt_ddb.h"
#include "opt_iommu.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/domainset.h>
#include <sys/eventhandler.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <machine/bus.h>
#include <sys/random.h>
#include <sys/refcount.h>
#include <sys/rman.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/bus.h>
#include <sys/cpuset.h>
#ifdef INTRNG
#include <sys/intr.h>
#endif

#include <net/vnet.h>

#include <machine/cpu.h>
#include <machine/stdarg.h>

#include <vm/uma.h>
#include <vm/vm.h>

#include <dev/iommu/iommu.h>

#include <ddb/ddb.h>

SYSCTL_NODE(_hw, OID_AUTO, bus, CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    NULL);
SYSCTL_ROOT_NODE(OID_AUTO, dev, CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    NULL);

static bool disable_failed_devs = false;
SYSCTL_BOOL(_hw_bus, OID_AUTO, disable_failed_devices, CTLFLAG_RWTUN, &disable_failed_devs,
    0, "Do not retry attaching devices that return an error from DEVICE_ATTACH the first time");

/*
 * Used to attach drivers to devclasses.
 */
typedef struct driverlink *driverlink_t;
struct driverlink {
	kobj_class_t	driver;
	TAILQ_ENTRY(driverlink) link;	/* list of drivers in devclass */
	int		pass;
	int		flags;
#define DL_DEFERRED_PROBE	1	/* Probe deferred on this */
	TAILQ_ENTRY(driverlink) passlink;
};

/*
 * Forward declarations
 */
typedef TAILQ_HEAD(devclass_list, devclass) devclass_list_t;
typedef TAILQ_HEAD(driver_list, driverlink) driver_list_t;
typedef TAILQ_HEAD(device_list, _device) device_list_t;

struct devclass {
	TAILQ_ENTRY(devclass) link;
	devclass_t	parent;		/* parent in devclass hierarchy */
	driver_list_t	drivers;	/* bus devclasses store drivers for bus */
	char		*name;
	device_t	*devices;	/* array of devices indexed by unit */
	int		maxunit;	/* size of devices array */
	int		flags;
#define DC_HAS_CHILDREN		1

	struct sysctl_ctx_list sysctl_ctx;
	struct sysctl_oid *sysctl_tree;
};

struct device_prop_elm {
	const char *name;
	void *val;
	void *dtr_ctx;
	device_prop_dtr_t dtr;
	LIST_ENTRY(device_prop_elm) link;
};

TASKQUEUE_DEFINE_THREAD(bus);

static void device_destroy_props(device_t dev);

/**
 * @brief Implementation of _device.
 *
 * The structure is named "_device" instead of "device" to avoid type confusion
 * caused by other subsystems defining a (struct device).
 */
struct _device {
	/*
	 * A device is a kernel object. The first field must be the
	 * current ops table for the object.
	 */
	KOBJ_FIELDS;

	/*
	 * Device hierarchy.
	 */
	TAILQ_ENTRY(_device)	link;	/**< list of devices in parent */
	TAILQ_ENTRY(_device)	devlink; /**< global device list membership */
	device_t	parent;		/**< parent of this device  */
	device_list_t	children;	/**< list of child devices */

	/*
	 * Details of this device.
	 */
	driver_t	*driver;	/**< current driver */
	devclass_t	devclass;	/**< current device class */
	int		unit;		/**< current unit number */
	char*		nameunit;	/**< name+unit e.g. foodev0 */
	char*		desc;		/**< driver specific description */
	u_int		busy;		/**< count of calls to device_busy() */
	device_state_t	state;		/**< current device state  */
	uint32_t	devflags;	/**< api level flags for device_get_flags() */
	u_int		flags;		/**< internal device flags  */
	u_int	order;			/**< order from device_add_child_ordered() */
	void	*ivars;			/**< instance variables  */
	void	*softc;			/**< current driver's variables  */
	LIST_HEAD(, device_prop_elm) props;

	struct sysctl_ctx_list sysctl_ctx; /**< state for sysctl variables  */
	struct sysctl_oid *sysctl_tree;	/**< state for sysctl variables */
};

static MALLOC_DEFINE(M_BUS, "bus", "Bus data structures");
static MALLOC_DEFINE(M_BUS_SC, "bus-sc", "Bus data structures, softc");

EVENTHANDLER_LIST_DEFINE(device_attach);
EVENTHANDLER_LIST_DEFINE(device_detach);
EVENTHANDLER_LIST_DEFINE(device_nomatch);
EVENTHANDLER_LIST_DEFINE(dev_lookup);

static void devctl2_init(void);
static bool device_frozen;

#define DRIVERNAME(d)	((d)? d->name : "no driver")
#define DEVCLANAME(d)	((d)? d->name : "no devclass")

#ifdef BUS_DEBUG

static int bus_debug = 1;
SYSCTL_INT(_debug, OID_AUTO, bus_debug, CTLFLAG_RWTUN, &bus_debug, 0,
    "Bus debug level");
#define PDEBUG(a)	if (bus_debug) {printf("%s:%d: ", __func__, __LINE__), printf a; printf("\n");}
#define DEVICENAME(d)	((d)? device_get_name(d): "no device")

/**
 * Produce the indenting, indent*2 spaces plus a '.' ahead of that to
 * prevent syslog from deleting initial spaces
 */
#define indentprintf(p)	do { int iJ; printf("."); for (iJ=0; iJ<indent; iJ++) printf("  "); printf p ; } while (0)

static void print_device_short(device_t dev, int indent);
static void print_device(device_t dev, int indent);
void print_device_tree_short(device_t dev, int indent);
void print_device_tree(device_t dev, int indent);
static void print_driver_short(driver_t *driver, int indent);
static void print_driver(driver_t *driver, int indent);
static void print_driver_list(driver_list_t drivers, int indent);
static void print_devclass_short(devclass_t dc, int indent);
static void print_devclass(devclass_t dc, int indent);
void print_devclass_list_short(void);
void print_devclass_list(void);

#else
/* Make the compiler ignore the function calls */
#define PDEBUG(a)			/* nop */
#define DEVICENAME(d)			/* nop */

#define print_device_short(d,i)		/* nop */
#define print_device(d,i)		/* nop */
#define print_device_tree_short(d,i)	/* nop */
#define print_device_tree(d,i)		/* nop */
#define print_driver_short(d,i)		/* nop */
#define print_driver(d,i)		/* nop */
#define print_driver_list(d,i)		/* nop */
#define print_devclass_short(d,i)	/* nop */
#define print_devclass(d,i)		/* nop */
#define print_devclass_list_short()	/* nop */
#define print_devclass_list()		/* nop */
#endif

/*
 * dev sysctl tree
 */

enum {
	DEVCLASS_SYSCTL_PARENT,
};

static int
devclass_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	devclass_t dc = (devclass_t)arg1;
	const char *value;

	switch (arg2) {
	case DEVCLASS_SYSCTL_PARENT:
		value = dc->parent ? dc->parent->name : "";
		break;
	default:
		return (EINVAL);
	}
	return (SYSCTL_OUT_STR(req, value));
}

static void
devclass_sysctl_init(devclass_t dc)
{
	if (dc->sysctl_tree != NULL)
		return;
	sysctl_ctx_init(&dc->sysctl_ctx);
	dc->sysctl_tree = SYSCTL_ADD_NODE(&dc->sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_dev), OID_AUTO, dc->name,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
	SYSCTL_ADD_PROC(&dc->sysctl_ctx, SYSCTL_CHILDREN(dc->sysctl_tree),
	    OID_AUTO, "%parent",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dc, DEVCLASS_SYSCTL_PARENT, devclass_sysctl_handler, "A",
	    "parent class");
}

enum {
	DEVICE_SYSCTL_DESC,
	DEVICE_SYSCTL_DRIVER,
	DEVICE_SYSCTL_LOCATION,
	DEVICE_SYSCTL_PNPINFO,
	DEVICE_SYSCTL_PARENT,
	DEVICE_SYSCTL_IOMMU,
};

static int
device_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sb;
	device_t dev = (device_t)arg1;
	device_t iommu;
	int error;
	uint16_t rid;
	const char *c;

	sbuf_new_for_sysctl(&sb, NULL, 1024, req);
	sbuf_clear_flags(&sb, SBUF_INCLUDENUL);
	bus_topo_lock();
	switch (arg2) {
	case DEVICE_SYSCTL_DESC:
		sbuf_cat(&sb, dev->desc ? dev->desc : "");
		break;
	case DEVICE_SYSCTL_DRIVER:
		sbuf_cat(&sb, dev->driver ? dev->driver->name : "");
		break;
	case DEVICE_SYSCTL_LOCATION:
		bus_child_location(dev, &sb);
		break;
	case DEVICE_SYSCTL_PNPINFO:
		bus_child_pnpinfo(dev, &sb);
		break;
	case DEVICE_SYSCTL_PARENT:
		sbuf_cat(&sb, dev->parent ? dev->parent->nameunit : "");
		break;
	case DEVICE_SYSCTL_IOMMU:
		iommu = NULL;
		error = device_get_prop(dev, DEV_PROP_NAME_IOMMU,
		    (void **)&iommu);
		c = "";
		if (error == 0 && iommu != NULL) {
			sbuf_printf(&sb, "unit=%s", device_get_nameunit(iommu));
			c = " ";
		}
		rid = 0;
#ifdef IOMMU
		iommu_get_requester(dev, &rid);
#endif
		if (rid != 0)
			sbuf_printf(&sb, "%srid=%#x", c, rid);
		break;
	default:
		error = EINVAL;
		goto out;
	}
	error = sbuf_finish(&sb);
out:
	bus_topo_unlock();
	sbuf_delete(&sb);
	return (error);
}

static void
device_sysctl_init(device_t dev)
{
	devclass_t dc = dev->devclass;
	int domain;

	if (dev->sysctl_tree != NULL)
		return;
	devclass_sysctl_init(dc);
	sysctl_ctx_init(&dev->sysctl_ctx);
	dev->sysctl_tree = SYSCTL_ADD_NODE_WITH_LABEL(&dev->sysctl_ctx,
	    SYSCTL_CHILDREN(dc->sysctl_tree), OID_AUTO,
	    dev->nameunit + strlen(dc->name),
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "", "device_index");
	SYSCTL_ADD_PROC(&dev->sysctl_ctx, SYSCTL_CHILDREN(dev->sysctl_tree),
	    OID_AUTO, "%desc", CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, DEVICE_SYSCTL_DESC, device_sysctl_handler, "A",
	    "device description");
	SYSCTL_ADD_PROC(&dev->sysctl_ctx, SYSCTL_CHILDREN(dev->sysctl_tree),
	    OID_AUTO, "%driver",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, DEVICE_SYSCTL_DRIVER, device_sysctl_handler, "A",
	    "device driver name");
	SYSCTL_ADD_PROC(&dev->sysctl_ctx, SYSCTL_CHILDREN(dev->sysctl_tree),
	    OID_AUTO, "%location",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, DEVICE_SYSCTL_LOCATION, device_sysctl_handler, "A",
	    "device location relative to parent");
	SYSCTL_ADD_PROC(&dev->sysctl_ctx, SYSCTL_CHILDREN(dev->sysctl_tree),
	    OID_AUTO, "%pnpinfo",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, DEVICE_SYSCTL_PNPINFO, device_sysctl_handler, "A",
	    "device identification");
	SYSCTL_ADD_PROC(&dev->sysctl_ctx, SYSCTL_CHILDREN(dev->sysctl_tree),
	    OID_AUTO, "%parent",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, DEVICE_SYSCTL_PARENT, device_sysctl_handler, "A",
	    "parent device");
	SYSCTL_ADD_PROC(&dev->sysctl_ctx, SYSCTL_CHILDREN(dev->sysctl_tree),
	    OID_AUTO, "%iommu",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, DEVICE_SYSCTL_IOMMU, device_sysctl_handler, "A",
	    "iommu unit handling the device requests");
	if (bus_get_domain(dev, &domain) == 0)
		SYSCTL_ADD_INT(&dev->sysctl_ctx,
		    SYSCTL_CHILDREN(dev->sysctl_tree), OID_AUTO, "%domain",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, domain, "NUMA domain");
}

static void
device_sysctl_update(device_t dev)
{
	devclass_t dc = dev->devclass;

	if (dev->sysctl_tree == NULL)
		return;
	sysctl_rename_oid(dev->sysctl_tree, dev->nameunit + strlen(dc->name));
}

static void
device_sysctl_fini(device_t dev)
{
	if (dev->sysctl_tree == NULL)
		return;
	sysctl_ctx_free(&dev->sysctl_ctx);
	dev->sysctl_tree = NULL;
}

static struct device_list bus_data_devices;
static int bus_data_generation = 1;

static kobj_method_t null_methods[] = {
	KOBJMETHOD_END
};

DEFINE_CLASS(null, null_methods, 0);

void
bus_topo_assert(void)
{

	GIANT_REQUIRED;	
}

struct mtx *
bus_topo_mtx(void)
{

	return (&Giant);
}

void
bus_topo_lock(void)
{

	mtx_lock(bus_topo_mtx());
}

void
bus_topo_unlock(void)
{

	mtx_unlock(bus_topo_mtx());
}

/*
 * Bus pass implementation
 */

static driver_list_t passes = TAILQ_HEAD_INITIALIZER(passes);
static int bus_current_pass = BUS_PASS_ROOT;

/**
 * @internal
 * @brief Register the pass level of a new driver attachment
 *
 * Register a new driver attachment's pass level.  If no driver
 * attachment with the same pass level has been added, then @p new
 * will be added to the global passes list.
 *
 * @param new		the new driver attachment
 */
static void
driver_register_pass(struct driverlink *new)
{
	struct driverlink *dl;

	/* We only consider pass numbers during boot. */
	if (bus_current_pass == BUS_PASS_DEFAULT)
		return;

	/*
	 * Walk the passes list.  If we already know about this pass
	 * then there is nothing to do.  If we don't, then insert this
	 * driver link into the list.
	 */
	TAILQ_FOREACH(dl, &passes, passlink) {
		if (dl->pass < new->pass)
			continue;
		if (dl->pass == new->pass)
			return;
		TAILQ_INSERT_BEFORE(dl, new, passlink);
		return;
	}
	TAILQ_INSERT_TAIL(&passes, new, passlink);
}

/**
 * @brief Retrieve the current bus pass
 *
 * Retrieves the current bus pass level.  Call the BUS_NEW_PASS()
 * method on the root bus to kick off a new device tree scan for each
 * new pass level that has at least one driver.
 */
int
bus_get_pass(void)
{

	return (bus_current_pass);
}

/**
 * @brief Raise the current bus pass
 *
 * Raise the current bus pass level to @p pass.  Call the BUS_NEW_PASS()
 * method on the root bus to kick off a new device tree scan for each
 * new pass level that has at least one driver.
 */
static void
bus_set_pass(int pass)
{
	struct driverlink *dl;

	if (bus_current_pass > pass)
		panic("Attempt to lower bus pass level");

	TAILQ_FOREACH(dl, &passes, passlink) {
		/* Skip pass values below the current pass level. */
		if (dl->pass <= bus_current_pass)
			continue;

		/*
		 * Bail once we hit a driver with a pass level that is
		 * too high.
		 */
		if (dl->pass > pass)
			break;

		/*
		 * Raise the pass level to the next level and rescan
		 * the tree.
		 */
		bus_current_pass = dl->pass;
		BUS_NEW_PASS(root_bus);
	}

	/*
	 * If there isn't a driver registered for the requested pass,
	 * then bus_current_pass might still be less than 'pass'.  Set
	 * it to 'pass' in that case.
	 */
	if (bus_current_pass < pass)
		bus_current_pass = pass;
	KASSERT(bus_current_pass == pass, ("Failed to update bus pass level"));
}

/*
 * Devclass implementation
 */

static devclass_list_t devclasses = TAILQ_HEAD_INITIALIZER(devclasses);

/**
 * @internal
 * @brief Find or create a device class
 *
 * If a device class with the name @p classname exists, return it,
 * otherwise if @p create is non-zero create and return a new device
 * class.
 *
 * If @p parentname is non-NULL, the parent of the devclass is set to
 * the devclass of that name.
 *
 * @param classname	the devclass name to find or create
 * @param parentname	the parent devclass name or @c NULL
 * @param create	non-zero to create a devclass
 */
static devclass_t
devclass_find_internal(const char *classname, const char *parentname,
		       int create)
{
	devclass_t dc;

	PDEBUG(("looking for %s", classname));
	if (!classname)
		return (NULL);

	TAILQ_FOREACH(dc, &devclasses, link) {
		if (!strcmp(dc->name, classname))
			break;
	}

	if (create && !dc) {
		PDEBUG(("creating %s", classname));
		dc = malloc(sizeof(struct devclass) + strlen(classname) + 1,
		    M_BUS, M_WAITOK | M_ZERO);
		dc->parent = NULL;
		dc->name = (char*) (dc + 1);
		strcpy(dc->name, classname);
		TAILQ_INIT(&dc->drivers);
		TAILQ_INSERT_TAIL(&devclasses, dc, link);

		bus_data_generation_update();
	}

	/*
	 * If a parent class is specified, then set that as our parent so
	 * that this devclass will support drivers for the parent class as
	 * well.  If the parent class has the same name don't do this though
	 * as it creates a cycle that can trigger an infinite loop in
	 * device_probe_child() if a device exists for which there is no
	 * suitable driver.
	 */
	if (parentname && dc && !dc->parent &&
	    strcmp(classname, parentname) != 0) {
		dc->parent = devclass_find_internal(parentname, NULL, TRUE);
		dc->parent->flags |= DC_HAS_CHILDREN;
	}

	return (dc);
}

/**
 * @brief Create a device class
 *
 * If a device class with the name @p classname exists, return it,
 * otherwise create and return a new device class.
 *
 * @param classname	the devclass name to find or create
 */
devclass_t
devclass_create(const char *classname)
{
	return (devclass_find_internal(classname, NULL, TRUE));
}

/**
 * @brief Find a device class
 *
 * If a device class with the name @p classname exists, return it,
 * otherwise return @c NULL.
 *
 * @param classname	the devclass name to find
 */
devclass_t
devclass_find(const char *classname)
{
	return (devclass_find_internal(classname, NULL, FALSE));
}

/**
 * @brief Register that a device driver has been added to a devclass
 *
 * Register that a device driver has been added to a devclass.  This
 * is called by devclass_add_driver to accomplish the recursive
 * notification of all the children classes of dc, as well as dc.
 * Each layer will have BUS_DRIVER_ADDED() called for all instances of
 * the devclass.
 *
 * We do a full search here of the devclass list at each iteration
 * level to save storing children-lists in the devclass structure.  If
 * we ever move beyond a few dozen devices doing this, we may need to
 * reevaluate...
 *
 * @param dc		the devclass to edit
 * @param driver	the driver that was just added
 */
static void
devclass_driver_added(devclass_t dc, driver_t *driver)
{
	devclass_t parent;
	int i;

	/*
	 * Call BUS_DRIVER_ADDED for any existing buses in this class.
	 */
	for (i = 0; i < dc->maxunit; i++)
		if (dc->devices[i] && device_is_attached(dc->devices[i]))
			BUS_DRIVER_ADDED(dc->devices[i], driver);

	/*
	 * Walk through the children classes.  Since we only keep a
	 * single parent pointer around, we walk the entire list of
	 * devclasses looking for children.  We set the
	 * DC_HAS_CHILDREN flag when a child devclass is created on
	 * the parent, so we only walk the list for those devclasses
	 * that have children.
	 */
	if (!(dc->flags & DC_HAS_CHILDREN))
		return;
	parent = dc;
	TAILQ_FOREACH(dc, &devclasses, link) {
		if (dc->parent == parent)
			devclass_driver_added(dc, driver);
	}
}

static void
device_handle_nomatch(device_t dev)
{
	BUS_PROBE_NOMATCH(dev->parent, dev);
	EVENTHANDLER_DIRECT_INVOKE(device_nomatch, dev);
	dev->flags |= DF_DONENOMATCH;
}

/**
 * @brief Add a device driver to a device class
 *
 * Add a device driver to a devclass. This is normally called
 * automatically by DRIVER_MODULE(). The BUS_DRIVER_ADDED() method of
 * all devices in the devclass will be called to allow them to attempt
 * to re-probe any unmatched children.
 *
 * @param dc		the devclass to edit
 * @param driver	the driver to register
 */
int
devclass_add_driver(devclass_t dc, driver_t *driver, int pass, devclass_t *dcp)
{
	driverlink_t dl;
	devclass_t child_dc;
	const char *parentname;

	PDEBUG(("%s", DRIVERNAME(driver)));

	/* Don't allow invalid pass values. */
	if (pass <= BUS_PASS_ROOT)
		return (EINVAL);

	dl = malloc(sizeof *dl, M_BUS, M_WAITOK|M_ZERO);

	/*
	 * Compile the driver's methods. Also increase the reference count
	 * so that the class doesn't get freed when the last instance
	 * goes. This means we can safely use static methods and avoids a
	 * double-free in devclass_delete_driver.
	 */
	kobj_class_compile((kobj_class_t) driver);

	/*
	 * If the driver has any base classes, make the
	 * devclass inherit from the devclass of the driver's
	 * first base class. This will allow the system to
	 * search for drivers in both devclasses for children
	 * of a device using this driver.
	 */
	if (driver->baseclasses)
		parentname = driver->baseclasses[0]->name;
	else
		parentname = NULL;
	child_dc = devclass_find_internal(driver->name, parentname, TRUE);
	if (dcp != NULL)
		*dcp = child_dc;

	dl->driver = driver;
	TAILQ_INSERT_TAIL(&dc->drivers, dl, link);
	driver->refs++;		/* XXX: kobj_mtx */
	dl->pass = pass;
	driver_register_pass(dl);

	if (device_frozen) {
		dl->flags |= DL_DEFERRED_PROBE;
	} else {
		devclass_driver_added(dc, driver);
	}
	bus_data_generation_update();
	return (0);
}

/**
 * @brief Register that a device driver has been deleted from a devclass
 *
 * Register that a device driver has been removed from a devclass.
 * This is called by devclass_delete_driver to accomplish the
 * recursive notification of all the children classes of busclass, as
 * well as busclass.  Each layer will attempt to detach the driver
 * from any devices that are children of the bus's devclass.  The function
 * will return an error if a device fails to detach.
 *
 * We do a full search here of the devclass list at each iteration
 * level to save storing children-lists in the devclass structure.  If
 * we ever move beyond a few dozen devices doing this, we may need to
 * reevaluate...
 *
 * @param busclass	the devclass of the parent bus
 * @param dc		the devclass of the driver being deleted
 * @param driver	the driver being deleted
 */
static int
devclass_driver_deleted(devclass_t busclass, devclass_t dc, driver_t *driver)
{
	devclass_t parent;
	device_t dev;
	int error, i;

	/*
	 * Disassociate from any devices.  We iterate through all the
	 * devices in the devclass of the driver and detach any which are
	 * using the driver and which have a parent in the devclass which
	 * we are deleting from.
	 *
	 * Note that since a driver can be in multiple devclasses, we
	 * should not detach devices which are not children of devices in
	 * the affected devclass.
	 *
	 * If we're frozen, we don't generate NOMATCH events. Mark to
	 * generate later.
	 */
	for (i = 0; i < dc->maxunit; i++) {
		if (dc->devices[i]) {
			dev = dc->devices[i];
			if (dev->driver == driver && dev->parent &&
			    dev->parent->devclass == busclass) {
				if ((error = device_detach(dev)) != 0)
					return (error);
				if (device_frozen) {
					dev->flags &= ~DF_DONENOMATCH;
					dev->flags |= DF_NEEDNOMATCH;
				} else {
					device_handle_nomatch(dev);
				}
			}
		}
	}

	/*
	 * Walk through the children classes.  Since we only keep a
	 * single parent pointer around, we walk the entire list of
	 * devclasses looking for children.  We set the
	 * DC_HAS_CHILDREN flag when a child devclass is created on
	 * the parent, so we only walk the list for those devclasses
	 * that have children.
	 */
	if (!(busclass->flags & DC_HAS_CHILDREN))
		return (0);
	parent = busclass;
	TAILQ_FOREACH(busclass, &devclasses, link) {
		if (busclass->parent == parent) {
			error = devclass_driver_deleted(busclass, dc, driver);
			if (error)
				return (error);
		}
	}
	return (0);
}

/**
 * @brief Delete a device driver from a device class
 *
 * Delete a device driver from a devclass. This is normally called
 * automatically by DRIVER_MODULE().
 *
 * If the driver is currently attached to any devices,
 * devclass_delete_driver() will first attempt to detach from each
 * device. If one of the detach calls fails, the driver will not be
 * deleted.
 *
 * @param dc		the devclass to edit
 * @param driver	the driver to unregister
 */
int
devclass_delete_driver(devclass_t busclass, driver_t *driver)
{
	devclass_t dc = devclass_find(driver->name);
	driverlink_t dl;
	int error;

	PDEBUG(("%s from devclass %s", driver->name, DEVCLANAME(busclass)));

	if (!dc)
		return (0);

	/*
	 * Find the link structure in the bus' list of drivers.
	 */
	TAILQ_FOREACH(dl, &busclass->drivers, link) {
		if (dl->driver == driver)
			break;
	}

	if (!dl) {
		PDEBUG(("%s not found in %s list", driver->name,
		    busclass->name));
		return (ENOENT);
	}

	error = devclass_driver_deleted(busclass, dc, driver);
	if (error != 0)
		return (error);

	TAILQ_REMOVE(&busclass->drivers, dl, link);
	free(dl, M_BUS);

	/* XXX: kobj_mtx */
	driver->refs--;
	if (driver->refs == 0)
		kobj_class_free((kobj_class_t) driver);

	bus_data_generation_update();
	return (0);
}

/**
 * @brief Quiesces a set of device drivers from a device class
 *
 * Quiesce a device driver from a devclass. This is normally called
 * automatically by DRIVER_MODULE().
 *
 * If the driver is currently attached to any devices,
 * devclass_quiesece_driver() will first attempt to quiesce each
 * device.
 *
 * @param dc		the devclass to edit
 * @param driver	the driver to unregister
 */
static int
devclass_quiesce_driver(devclass_t busclass, driver_t *driver)
{
	devclass_t dc = devclass_find(driver->name);
	driverlink_t dl;
	device_t dev;
	int i;
	int error;

	PDEBUG(("%s from devclass %s", driver->name, DEVCLANAME(busclass)));

	if (!dc)
		return (0);

	/*
	 * Find the link structure in the bus' list of drivers.
	 */
	TAILQ_FOREACH(dl, &busclass->drivers, link) {
		if (dl->driver == driver)
			break;
	}

	if (!dl) {
		PDEBUG(("%s not found in %s list", driver->name,
		    busclass->name));
		return (ENOENT);
	}

	/*
	 * Quiesce all devices.  We iterate through all the devices in
	 * the devclass of the driver and quiesce any which are using
	 * the driver and which have a parent in the devclass which we
	 * are quiescing.
	 *
	 * Note that since a driver can be in multiple devclasses, we
	 * should not quiesce devices which are not children of
	 * devices in the affected devclass.
	 */
	for (i = 0; i < dc->maxunit; i++) {
		if (dc->devices[i]) {
			dev = dc->devices[i];
			if (dev->driver == driver && dev->parent &&
			    dev->parent->devclass == busclass) {
				if ((error = device_quiesce(dev)) != 0)
					return (error);
			}
		}
	}

	return (0);
}

/**
 * @internal
 */
static driverlink_t
devclass_find_driver_internal(devclass_t dc, const char *classname)
{
	driverlink_t dl;

	PDEBUG(("%s in devclass %s", classname, DEVCLANAME(dc)));

	TAILQ_FOREACH(dl, &dc->drivers, link) {
		if (!strcmp(dl->driver->name, classname))
			return (dl);
	}

	PDEBUG(("not found"));
	return (NULL);
}

/**
 * @brief Return the name of the devclass
 */
const char *
devclass_get_name(devclass_t dc)
{
	return (dc->name);
}

/**
 * @brief Find a device given a unit number
 *
 * @param dc		the devclass to search
 * @param unit		the unit number to search for
 *
 * @returns		the device with the given unit number or @c
 *			NULL if there is no such device
 */
device_t
devclass_get_device(devclass_t dc, int unit)
{
	if (dc == NULL || unit < 0 || unit >= dc->maxunit)
		return (NULL);
	return (dc->devices[unit]);
}

/**
 * @brief Find the softc field of a device given a unit number
 *
 * @param dc		the devclass to search
 * @param unit		the unit number to search for
 *
 * @returns		the softc field of the device with the given
 *			unit number or @c NULL if there is no such
 *			device
 */
void *
devclass_get_softc(devclass_t dc, int unit)
{
	device_t dev;

	dev = devclass_get_device(dc, unit);
	if (!dev)
		return (NULL);

	return (device_get_softc(dev));
}

/**
 * @brief Get a list of devices in the devclass
 *
 * An array containing a list of all the devices in the given devclass
 * is allocated and returned in @p *devlistp. The number of devices
 * in the array is returned in @p *devcountp. The caller should free
 * the array using @c free(p, M_TEMP), even if @p *devcountp is 0.
 *
 * @param dc		the devclass to examine
 * @param devlistp	points at location for array pointer return
 *			value
 * @param devcountp	points at location for array size return value
 *
 * @retval 0		success
 * @retval ENOMEM	the array allocation failed
 */
int
devclass_get_devices(devclass_t dc, device_t **devlistp, int *devcountp)
{
	int count, i;
	device_t *list;

	count = devclass_get_count(dc);
	list = malloc(count * sizeof(device_t), M_TEMP, M_NOWAIT|M_ZERO);
	if (!list)
		return (ENOMEM);

	count = 0;
	for (i = 0; i < dc->maxunit; i++) {
		if (dc->devices[i]) {
			list[count] = dc->devices[i];
			count++;
		}
	}

	*devlistp = list;
	*devcountp = count;

	return (0);
}

/**
 * @brief Get a list of drivers in the devclass
 *
 * An array containing a list of pointers to all the drivers in the
 * given devclass is allocated and returned in @p *listp.  The number
 * of drivers in the array is returned in @p *countp. The caller should
 * free the array using @c free(p, M_TEMP).
 *
 * @param dc		the devclass to examine
 * @param listp		gives location for array pointer return value
 * @param countp	gives location for number of array elements
 *			return value
 *
 * @retval 0		success
 * @retval ENOMEM	the array allocation failed
 */
int
devclass_get_drivers(devclass_t dc, driver_t ***listp, int *countp)
{
	driverlink_t dl;
	driver_t **list;
	int count;

	count = 0;
	TAILQ_FOREACH(dl, &dc->drivers, link)
		count++;
	list = malloc(count * sizeof(driver_t *), M_TEMP, M_NOWAIT);
	if (list == NULL)
		return (ENOMEM);

	count = 0;
	TAILQ_FOREACH(dl, &dc->drivers, link) {
		list[count] = dl->driver;
		count++;
	}
	*listp = list;
	*countp = count;

	return (0);
}

/**
 * @brief Get the number of devices in a devclass
 *
 * @param dc		the devclass to examine
 */
int
devclass_get_count(devclass_t dc)
{
	int count, i;

	count = 0;
	for (i = 0; i < dc->maxunit; i++)
		if (dc->devices[i])
			count++;
	return (count);
}

/**
 * @brief Get the maximum unit number used in a devclass
 *
 * Note that this is one greater than the highest currently-allocated unit.  If
 * @p dc is NULL, @c -1 is returned to indicate that not even the devclass has
 * been allocated yet.
 *
 * @param dc		the devclass to examine
 */
int
devclass_get_maxunit(devclass_t dc)
{
	if (dc == NULL)
		return (-1);
	return (dc->maxunit);
}

/**
 * @brief Find a free unit number in a devclass
 *
 * This function searches for the first unused unit number greater
 * that or equal to @p unit. Note: This can return INT_MAX which
 * may be rejected elsewhere.
 *
 * @param dc		the devclass to examine
 * @param unit		the first unit number to check
 */
int
devclass_find_free_unit(devclass_t dc, int unit)
{
	if (dc == NULL)
		return (unit);
	while (unit < dc->maxunit && dc->devices[unit] != NULL)
		unit++;
	return (unit);
}

/**
 * @brief Set the parent of a devclass
 *
 * The parent class is normally initialised automatically by
 * DRIVER_MODULE().
 *
 * @param dc		the devclass to edit
 * @param pdc		the new parent devclass
 */
void
devclass_set_parent(devclass_t dc, devclass_t pdc)
{
	dc->parent = pdc;
}

/**
 * @brief Get the parent of a devclass
 *
 * @param dc		the devclass to examine
 */
devclass_t
devclass_get_parent(devclass_t dc)
{
	return (dc->parent);
}

struct sysctl_ctx_list *
devclass_get_sysctl_ctx(devclass_t dc)
{
	return (&dc->sysctl_ctx);
}

struct sysctl_oid *
devclass_get_sysctl_tree(devclass_t dc)
{
	return (dc->sysctl_tree);
}

/**
 * @internal
 * @brief Allocate a unit number
 *
 * On entry, @p *unitp is the desired unit number (or @c DEVICE_UNIT_ANY if any
 * will do). The allocated unit number is returned in @p *unitp.
 *
 * @param dc		the devclass to allocate from
 * @param unitp		points at the location for the allocated unit
 *			number
 *
 * @retval 0		success
 * @retval EEXIST	the requested unit number is already allocated
 * @retval ENOMEM	memory allocation failure
 * @retval EINVAL	unit is negative or we've run out of units
 */
static int
devclass_alloc_unit(devclass_t dc, device_t dev, int *unitp)
{
	const char *s;
	int unit = *unitp;

	PDEBUG(("unit %d in devclass %s", unit, DEVCLANAME(dc)));

	/* Ask the parent bus if it wants to wire this device. */
	if (unit == DEVICE_UNIT_ANY)
		BUS_HINT_DEVICE_UNIT(device_get_parent(dev), dev, dc->name,
		    &unit);

	/* Unit numbers are either DEVICE_UNIT_ANY or in [0,INT_MAX) */
	if ((unit < 0 && unit != DEVICE_UNIT_ANY) || unit == INT_MAX)
		return (EINVAL);

	/* If we were given a wired unit number, check for existing device */
	if (unit != DEVICE_UNIT_ANY) {
		if (unit < dc->maxunit && dc->devices[unit] != NULL) {
			if (bootverbose)
				printf("%s: %s%d already exists; skipping it\n",
				    dc->name, dc->name, *unitp);
			return (EEXIST);
		}
	} else {
		/* Unwired device, find the next available slot for it */
		unit = 0;
		for (unit = 0; unit < INT_MAX; unit++) {
			/* If this device slot is already in use, skip it. */
			if (unit < dc->maxunit && dc->devices[unit] != NULL)
				continue;

			/* If there is an "at" hint for a unit then skip it. */
			if (resource_string_value(dc->name, unit, "at", &s) ==
			    0)
				continue;

			break;
		}
	}

	/*
	 * Unit numbers must be in the range [0, INT_MAX), so exclude INT_MAX as
	 * too large. We constrain maxunit below to be <= INT_MAX. This means we
	 * can treat unit and maxunit as normal integers with normal math
	 * everywhere and we only have to flag INT_MAX as invalid.
	 */
	if (unit == INT_MAX)
		return (EINVAL);

	/*
	 * We've selected a unit beyond the length of the table, so let's extend
	 * the table to make room for all units up to and including this one.
	 */
	if (unit >= dc->maxunit) {
		int newsize;

		newsize = unit + 1;
		dc->devices = reallocf(dc->devices,
		    newsize * sizeof(*dc->devices), M_BUS, M_WAITOK);
		memset(dc->devices + dc->maxunit, 0,
		    sizeof(device_t) * (newsize - dc->maxunit));
		dc->maxunit = newsize;
	}
	PDEBUG(("now: unit %d in devclass %s", unit, DEVCLANAME(dc)));

	*unitp = unit;
	return (0);
}

/**
 * @internal
 * @brief Add a device to a devclass
 *
 * A unit number is allocated for the device (using the device's
 * preferred unit number if any) and the device is registered in the
 * devclass. This allows the device to be looked up by its unit
 * number, e.g. by decoding a dev_t minor number.
 *
 * @param dc		the devclass to add to
 * @param dev		the device to add
 *
 * @retval 0		success
 * @retval EEXIST	the requested unit number is already allocated
 * @retval ENOMEM	memory allocation failure
 * @retval EINVAL	Unit number invalid or too many units
 */
static int
devclass_add_device(devclass_t dc, device_t dev)
{
	int buflen, error;

	PDEBUG(("%s in devclass %s", DEVICENAME(dev), DEVCLANAME(dc)));

	buflen = snprintf(NULL, 0, "%s%d$", dc->name, INT_MAX);
	if (buflen < 0)
		return (ENOMEM);
	dev->nameunit = malloc(buflen, M_BUS, M_WAITOK|M_ZERO);

	if ((error = devclass_alloc_unit(dc, dev, &dev->unit)) != 0) {
		free(dev->nameunit, M_BUS);
		dev->nameunit = NULL;
		return (error);
	}
	dc->devices[dev->unit] = dev;
	dev->devclass = dc;
	snprintf(dev->nameunit, buflen, "%s%d", dc->name, dev->unit);

	return (0);
}

/**
 * @internal
 * @brief Delete a device from a devclass
 *
 * The device is removed from the devclass's device list and its unit
 * number is freed.

 * @param dc		the devclass to delete from
 * @param dev		the device to delete
 *
 * @retval 0		success
 */
static int
devclass_delete_device(devclass_t dc, device_t dev)
{
	if (!dc || !dev)
		return (0);

	PDEBUG(("%s in devclass %s", DEVICENAME(dev), DEVCLANAME(dc)));

	if (dev->devclass != dc || dc->devices[dev->unit] != dev)
		panic("devclass_delete_device: inconsistent device class");
	dc->devices[dev->unit] = NULL;
	if (dev->flags & DF_WILDCARD)
		dev->unit = DEVICE_UNIT_ANY;
	dev->devclass = NULL;
	free(dev->nameunit, M_BUS);
	dev->nameunit = NULL;

	return (0);
}

/**
 * @internal
 * @brief Make a new device and add it as a child of @p parent
 *
 * @param parent	the parent of the new device
 * @param name		the devclass name of the new device or @c NULL
 *			to leave the devclass unspecified
 * @parem unit		the unit number of the new device of @c DEVICE_UNIT_ANY
 *			to leave the unit number unspecified
 *
 * @returns the new device
 */
static device_t
make_device(device_t parent, const char *name, int unit)
{
	device_t dev;
	devclass_t dc;

	PDEBUG(("%s at %s as unit %d", name, DEVICENAME(parent), unit));

	if (name) {
		dc = devclass_find_internal(name, NULL, TRUE);
		if (!dc) {
			printf("make_device: can't find device class %s\n",
			    name);
			return (NULL);
		}
	} else {
		dc = NULL;
	}

	dev = malloc(sizeof(*dev), M_BUS, M_WAITOK|M_ZERO);
	dev->parent = parent;
	TAILQ_INIT(&dev->children);
	kobj_init((kobj_t) dev, &null_class);
	dev->driver = NULL;
	dev->devclass = NULL;
	dev->unit = unit;
	dev->nameunit = NULL;
	dev->desc = NULL;
	dev->busy = 0;
	dev->devflags = 0;
	dev->flags = DF_ENABLED;
	dev->order = 0;
	if (unit == DEVICE_UNIT_ANY)
		dev->flags |= DF_WILDCARD;
	if (name) {
		dev->flags |= DF_FIXEDCLASS;
		if (devclass_add_device(dc, dev)) {
			kobj_delete((kobj_t) dev, M_BUS);
			return (NULL);
		}
	}
	if (parent != NULL && device_has_quiet_children(parent))
		dev->flags |= DF_QUIET | DF_QUIET_CHILDREN;
	dev->ivars = NULL;
	dev->softc = NULL;
	LIST_INIT(&dev->props);

	dev->state = DS_NOTPRESENT;

	TAILQ_INSERT_TAIL(&bus_data_devices, dev, devlink);
	bus_data_generation_update();

	return (dev);
}

/**
 * @internal
 * @brief Print a description of a device.
 */
static int
device_print_child(device_t dev, device_t child)
{
	int retval = 0;

	if (device_is_alive(child))
		retval += BUS_PRINT_CHILD(dev, child);
	else
		retval += device_printf(child, " not found\n");

	return (retval);
}

/**
 * @brief Create a new device
 *
 * This creates a new device and adds it as a child of an existing
 * parent device. The new device will be added after the last existing
 * child with order zero.
 *
 * @param dev		the device which will be the parent of the
 *			new child device
 * @param name		devclass name for new device or @c NULL if not
 *			specified
 * @param unit		unit number for new device or @c DEVICE_UNIT_ANY if not
 *			specified
 *
 * @returns		the new device
 */
device_t
device_add_child(device_t dev, const char *name, int unit)
{
	return (device_add_child_ordered(dev, 0, name, unit));
}

/**
 * @brief Create a new device
 *
 * This creates a new device and adds it as a child of an existing
 * parent device. The new device will be added after the last existing
 * child with the same order.
 *
 * @param dev		the device which will be the parent of the
 *			new child device
 * @param order		a value which is used to partially sort the
 *			children of @p dev - devices created using
 *			lower values of @p order appear first in @p
 *			dev's list of children
 * @param name		devclass name for new device or @c NULL if not
 *			specified
 * @param unit		unit number for new device or @c DEVICE_UNIT_ANY if not
 *			specified
 *
 * @returns		the new device
 */
device_t
device_add_child_ordered(device_t dev, u_int order, const char *name, int unit)
{
	device_t child;
	device_t place;

	PDEBUG(("%s at %s with order %u as unit %d",
	    name, DEVICENAME(dev), order, unit));
	KASSERT(name != NULL || unit == DEVICE_UNIT_ANY,
	    ("child device with wildcard name and specific unit number"));

	child = make_device(dev, name, unit);
	if (child == NULL)
		return (child);
	child->order = order;

	TAILQ_FOREACH(place, &dev->children, link) {
		if (place->order > order)
			break;
	}

	if (place) {
		/*
		 * The device 'place' is the first device whose order is
		 * greater than the new child.
		 */
		TAILQ_INSERT_BEFORE(place, child, link);
	} else {
		/*
		 * The new child's order is greater or equal to the order of
		 * any existing device. Add the child to the tail of the list.
		 */
		TAILQ_INSERT_TAIL(&dev->children, child, link);
	}

	bus_data_generation_update();
	return (child);
}

/**
 * @brief Delete a device
 *
 * This function deletes a device along with all of its children. If
 * the device currently has a driver attached to it, the device is
 * detached first using device_detach().
 *
 * @param dev		the parent device
 * @param child		the device to delete
 *
 * @retval 0		success
 * @retval non-zero	a unit error code describing the error
 */
int
device_delete_child(device_t dev, device_t child)
{
	int error;
	device_t grandchild;

	PDEBUG(("%s from %s", DEVICENAME(child), DEVICENAME(dev)));

	/*
	 * Detach child.  Ideally this cleans up any grandchild
	 * devices.
	 */
	if ((error = device_detach(child)) != 0)
		return (error);

	/* Delete any grandchildren left after detach. */
	while ((grandchild = TAILQ_FIRST(&child->children)) != NULL) {
		error = device_delete_child(child, grandchild);
		if (error)
			return (error);
	}

	device_destroy_props(dev);
	if (child->devclass)
		devclass_delete_device(child->devclass, child);
	if (child->parent)
		BUS_CHILD_DELETED(dev, child);
	TAILQ_REMOVE(&dev->children, child, link);
	TAILQ_REMOVE(&bus_data_devices, child, devlink);
	kobj_delete((kobj_t) child, M_BUS);

	bus_data_generation_update();
	return (0);
}

/**
 * @brief Delete all children devices of the given device, if any.
 *
 * This function deletes all children devices of the given device, if
 * any, using the device_delete_child() function for each device it
 * finds. If a child device cannot be deleted, this function will
 * return an error code.
 *
 * @param dev		the parent device
 *
 * @retval 0		success
 * @retval non-zero	a device would not detach
 */
int
device_delete_children(device_t dev)
{
	device_t child;
	int error;

	PDEBUG(("Deleting all children of %s", DEVICENAME(dev)));

	error = 0;

	while ((child = TAILQ_FIRST(&dev->children)) != NULL) {
		error = device_delete_child(dev, child);
		if (error) {
			PDEBUG(("Failed deleting %s", DEVICENAME(child)));
			break;
		}
	}
	return (error);
}

/**
 * @brief Find a device given a unit number
 *
 * This is similar to devclass_get_devices() but only searches for
 * devices which have @p dev as a parent.
 *
 * @param dev		the parent device to search
 * @param unit		the unit number to search for.  If the unit is
 *			@c DEVICE_UNIT_ANY, return the first child of @p dev
 *			which has name @p classname (that is, the one with the
 *			lowest unit.)
 *
 * @returns		the device with the given unit number or @c
 *			NULL if there is no such device
 */
device_t
device_find_child(device_t dev, const char *classname, int unit)
{
	devclass_t dc;
	device_t child;

	dc = devclass_find(classname);
	if (!dc)
		return (NULL);

	if (unit != DEVICE_UNIT_ANY) {
		child = devclass_get_device(dc, unit);
		if (child && child->parent == dev)
			return (child);
	} else {
		for (unit = 0; unit < devclass_get_maxunit(dc); unit++) {
			child = devclass_get_device(dc, unit);
			if (child && child->parent == dev)
				return (child);
		}
	}
	return (NULL);
}

/**
 * @internal
 */
static driverlink_t
first_matching_driver(devclass_t dc, device_t dev)
{
	if (dev->devclass)
		return (devclass_find_driver_internal(dc, dev->devclass->name));
	return (TAILQ_FIRST(&dc->drivers));
}

/**
 * @internal
 */
static driverlink_t
next_matching_driver(devclass_t dc, device_t dev, driverlink_t last)
{
	if (dev->devclass) {
		driverlink_t dl;
		for (dl = TAILQ_NEXT(last, link); dl; dl = TAILQ_NEXT(dl, link))
			if (!strcmp(dev->devclass->name, dl->driver->name))
				return (dl);
		return (NULL);
	}
	return (TAILQ_NEXT(last, link));
}

/**
 * @internal
 */
int
device_probe_child(device_t dev, device_t child)
{
	devclass_t dc;
	driverlink_t best = NULL;
	driverlink_t dl;
	int result, pri = 0;
	/* We should preserve the devclass (or lack of) set by the bus. */
	int hasclass = (child->devclass != NULL);

	bus_topo_assert();

	dc = dev->devclass;
	if (!dc)
		panic("device_probe_child: parent device has no devclass");

	/*
	 * If the state is already probed, then return.
	 */
	if (child->state == DS_ALIVE)
		return (0);

	for (; dc; dc = dc->parent) {
		for (dl = first_matching_driver(dc, child);
		     dl;
		     dl = next_matching_driver(dc, child, dl)) {
			/* If this driver's pass is too high, then ignore it. */
			if (dl->pass > bus_current_pass)
				continue;

			PDEBUG(("Trying %s", DRIVERNAME(dl->driver)));
			result = device_set_driver(child, dl->driver);
			if (result == ENOMEM)
				return (result);
			else if (result != 0)
				continue;
			if (!hasclass) {
				if (device_set_devclass(child,
				    dl->driver->name) != 0) {
					char const * devname =
					    device_get_name(child);
					if (devname == NULL)
						devname = "(unknown)";
					printf("driver bug: Unable to set "
					    "devclass (class: %s "
					    "devname: %s)\n",
					    dl->driver->name,
					    devname);
					(void)device_set_driver(child, NULL);
					continue;
				}
			}

			/* Fetch any flags for the device before probing. */
			resource_int_value(dl->driver->name, child->unit,
			    "flags", &child->devflags);

			result = DEVICE_PROBE(child);

			/*
			 * If probe returns 0, this is the driver that wins this
			 * device.
			 */
			if (result == 0) {
				best = dl;
				pri = 0;
				goto exact_match;	/* C doesn't have break 2 */
			}

			/* Reset flags and devclass before the next probe. */
			child->devflags = 0;
			if (!hasclass)
				(void)device_set_devclass(child, NULL);

			/*
			 * Reset DF_QUIET in case this driver doesn't
			 * end up as the best driver.
			 */
			device_verbose(child);

			/*
			 * Probes that return BUS_PROBE_NOWILDCARD or lower
			 * only match on devices whose driver was explicitly
			 * specified.
			 */
			if (result <= BUS_PROBE_NOWILDCARD &&
			    !(child->flags & DF_FIXEDCLASS)) {
				result = ENXIO;
			}

			/*
			 * The driver returned an error so it
			 * certainly doesn't match.
			 */
			if (result > 0) {
				(void)device_set_driver(child, NULL);
				continue;
			}

			/*
			 * A priority lower than SUCCESS, remember the
			 * best matching driver. Initialise the value
			 * of pri for the first match.
			 */
			if (best == NULL || result > pri) {
				best = dl;
				pri = result;
				continue;
			}
		}
	}

	if (best == NULL)
		return (ENXIO);

	/*
	 * If we found a driver, change state and initialise the devclass.
	 * Set the winning driver, devclass, and flags.
	 */
	result = device_set_driver(child, best->driver);
	if (result != 0)
		return (result);
	if (!child->devclass) {
		result = device_set_devclass(child, best->driver->name);
		if (result != 0) {
			(void)device_set_driver(child, NULL);
			return (result);
		}
	}
	resource_int_value(best->driver->name, child->unit,
	    "flags", &child->devflags);

	/*
	 * A bit bogus. Call the probe method again to make sure that we have
	 * the right description for the device.
	 */
	result = DEVICE_PROBE(child);
	if (result > 0) {
		if (!hasclass)
			(void)device_set_devclass(child, NULL);
		(void)device_set_driver(child, NULL);
		return (result);
	}

exact_match:
	child->state = DS_ALIVE;
	bus_data_generation_update();
	return (0);
}

/**
 * @brief Return the parent of a device
 */
device_t
device_get_parent(device_t dev)
{
	return (dev->parent);
}

/**
 * @brief Get a list of children of a device
 *
 * An array containing a list of all the children of the given device
 * is allocated and returned in @p *devlistp. The number of devices
 * in the array is returned in @p *devcountp. The caller should free
 * the array using @c free(p, M_TEMP).
 *
 * @param dev		the device to examine
 * @param devlistp	points at location for array pointer return
 *			value
 * @param devcountp	points at location for array size return value
 *
 * @retval 0		success
 * @retval ENOMEM	the array allocation failed
 */
int
device_get_children(device_t dev, device_t **devlistp, int *devcountp)
{
	int count;
	device_t child;
	device_t *list;

	count = 0;
	TAILQ_FOREACH(child, &dev->children, link) {
		count++;
	}
	if (count == 0) {
		*devlistp = NULL;
		*devcountp = 0;
		return (0);
	}

	list = malloc(count * sizeof(device_t), M_TEMP, M_NOWAIT|M_ZERO);
	if (!list)
		return (ENOMEM);

	count = 0;
	TAILQ_FOREACH(child, &dev->children, link) {
		list[count] = child;
		count++;
	}

	*devlistp = list;
	*devcountp = count;

	return (0);
}

/**
 * @brief Return the current driver for the device or @c NULL if there
 * is no driver currently attached
 */
driver_t *
device_get_driver(device_t dev)
{
	return (dev->driver);
}

/**
 * @brief Return the current devclass for the device or @c NULL if
 * there is none.
 */
devclass_t
device_get_devclass(device_t dev)
{
	return (dev->devclass);
}

/**
 * @brief Return the name of the device's devclass or @c NULL if there
 * is none.
 */
const char *
device_get_name(device_t dev)
{
	if (dev != NULL && dev->devclass)
		return (devclass_get_name(dev->devclass));
	return (NULL);
}

/**
 * @brief Return a string containing the device's devclass name
 * followed by an ascii representation of the device's unit number
 * (e.g. @c "foo2").
 */
const char *
device_get_nameunit(device_t dev)
{
	return (dev->nameunit);
}

/**
 * @brief Return the device's unit number.
 */
int
device_get_unit(device_t dev)
{
	return (dev->unit);
}

/**
 * @brief Return the device's description string
 */
const char *
device_get_desc(device_t dev)
{
	return (dev->desc);
}

/**
 * @brief Return the device's flags
 */
uint32_t
device_get_flags(device_t dev)
{
	return (dev->devflags);
}

struct sysctl_ctx_list *
device_get_sysctl_ctx(device_t dev)
{
	return (&dev->sysctl_ctx);
}

struct sysctl_oid *
device_get_sysctl_tree(device_t dev)
{
	return (dev->sysctl_tree);
}

/**
 * @brief Print the name of the device followed by a colon and a space
 *
 * @returns the number of characters printed
 */
int
device_print_prettyname(device_t dev)
{
	const char *name = device_get_name(dev);

	if (name == NULL)
		return (printf("unknown: "));
	return (printf("%s%d: ", name, device_get_unit(dev)));
}

/**
 * @brief Print the name of the device followed by a colon, a space
 * and the result of calling vprintf() with the value of @p fmt and
 * the following arguments.
 *
 * @returns the number of characters printed
 */
int
device_printf(device_t dev, const char * fmt, ...)
{
	char buf[128];
	struct sbuf sb;
	const char *name;
	va_list ap;
	size_t retval;

	retval = 0;

	sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);
	sbuf_set_drain(&sb, sbuf_printf_drain, &retval);

	name = device_get_name(dev);

	if (name == NULL)
		sbuf_cat(&sb, "unknown: ");
	else
		sbuf_printf(&sb, "%s%d: ", name, device_get_unit(dev));

	va_start(ap, fmt);
	sbuf_vprintf(&sb, fmt, ap);
	va_end(ap);

	sbuf_finish(&sb);
	sbuf_delete(&sb);

	return (retval);
}

/**
 * @brief Print the name of the device followed by a colon, a space
 * and the result of calling log() with the value of @p fmt and
 * the following arguments.
 *
 * @returns the number of characters printed
 */
int
device_log(device_t dev, int pri, const char * fmt, ...)
{
	char buf[128];
	struct sbuf sb;
	const char *name;
	va_list ap;
	size_t retval;

	retval = 0;

	sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);

	name = device_get_name(dev);

	if (name == NULL)
		sbuf_cat(&sb, "unknown: ");
	else
		sbuf_printf(&sb, "%s%d: ", name, device_get_unit(dev));

	va_start(ap, fmt);
	sbuf_vprintf(&sb, fmt, ap);
	va_end(ap);

	sbuf_finish(&sb);

	log(pri, "%.*s", (int) sbuf_len(&sb), sbuf_data(&sb));
	retval = sbuf_len(&sb);

	sbuf_delete(&sb);

	return (retval);
}

/**
 * @internal
 */
static void
device_set_desc_internal(device_t dev, const char *desc, bool allocated)
{
	if (dev->desc && (dev->flags & DF_DESCMALLOCED)) {
		free(dev->desc, M_BUS);
		dev->flags &= ~DF_DESCMALLOCED;
		dev->desc = NULL;
	}

	if (allocated && desc)
		dev->flags |= DF_DESCMALLOCED;
	dev->desc = __DECONST(char *, desc);

	bus_data_generation_update();
}

/**
 * @brief Set the device's description
 *
 * The value of @c desc should be a string constant that will not
 * change (at least until the description is changed in a subsequent
 * call to device_set_desc() or device_set_desc_copy()).
 */
void
device_set_desc(device_t dev, const char *desc)
{
	device_set_desc_internal(dev, desc, false);
}

/**
 * @brief Set the device's description
 *
 * A printf-like version of device_set_desc().
 */
void
device_set_descf(device_t dev, const char *fmt, ...)
{
	va_list ap;
	char *buf = NULL;

	va_start(ap, fmt);
	vasprintf(&buf, M_BUS, fmt, ap);
	va_end(ap);
	device_set_desc_internal(dev, buf, true);
}

/**
 * @brief Set the device's description
 *
 * The string pointed to by @c desc is copied. Use this function if
 * the device description is generated, (e.g. with sprintf()).
 */
void
device_set_desc_copy(device_t dev, const char *desc)
{
	char *buf;

	buf = strdup_flags(desc, M_BUS, M_WAITOK);
	device_set_desc_internal(dev, buf, true);
}

/**
 * @brief Set the device's flags
 */
void
device_set_flags(device_t dev, uint32_t flags)
{
	dev->devflags = flags;
}

/**
 * @brief Return the device's softc field
 *
 * The softc is allocated and zeroed when a driver is attached, based
 * on the size field of the driver.
 */
void *
device_get_softc(device_t dev)
{
	return (dev->softc);
}

/**
 * @brief Set the device's softc field
 *
 * Most drivers do not need to use this since the softc is allocated
 * automatically when the driver is attached.
 */
void
device_set_softc(device_t dev, void *softc)
{
	if (dev->softc && !(dev->flags & DF_EXTERNALSOFTC))
		free(dev->softc, M_BUS_SC);
	dev->softc = softc;
	if (dev->softc)
		dev->flags |= DF_EXTERNALSOFTC;
	else
		dev->flags &= ~DF_EXTERNALSOFTC;
}

/**
 * @brief Free claimed softc
 *
 * Most drivers do not need to use this since the softc is freed
 * automatically when the driver is detached.
 */
void
device_free_softc(void *softc)
{
	free(softc, M_BUS_SC);
}

/**
 * @brief Claim softc
 *
 * This function can be used to let the driver free the automatically
 * allocated softc using "device_free_softc()". This function is
 * useful when the driver is refcounting the softc and the softc
 * cannot be freed when the "device_detach" method is called.
 */
void
device_claim_softc(device_t dev)
{
	if (dev->softc)
		dev->flags |= DF_EXTERNALSOFTC;
	else
		dev->flags &= ~DF_EXTERNALSOFTC;
}

/**
 * @brief Get the device's ivars field
 *
 * The ivars field is used by the parent device to store per-device
 * state (e.g. the physical location of the device or a list of
 * resources).
 */
void *
device_get_ivars(device_t dev)
{
	KASSERT(dev != NULL, ("device_get_ivars(NULL, ...)"));
	return (dev->ivars);
}

/**
 * @brief Set the device's ivars field
 */
void
device_set_ivars(device_t dev, void * ivars)
{
	KASSERT(dev != NULL, ("device_set_ivars(NULL, ...)"));
	dev->ivars = ivars;
}

/**
 * @brief Return the device's state
 */
device_state_t
device_get_state(device_t dev)
{
	return (dev->state);
}

/**
 * @brief Set the DF_ENABLED flag for the device
 */
void
device_enable(device_t dev)
{
	dev->flags |= DF_ENABLED;
}

/**
 * @brief Clear the DF_ENABLED flag for the device
 */
void
device_disable(device_t dev)
{
	dev->flags &= ~DF_ENABLED;
}

/**
 * @brief Increment the busy counter for the device
 */
void
device_busy(device_t dev)
{

	/*
	 * Mark the device as busy, recursively up the tree if this busy count
	 * goes 0->1.
	 */
	if (refcount_acquire(&dev->busy) == 0 && dev->parent != NULL)
		device_busy(dev->parent);
}

/**
 * @brief Decrement the busy counter for the device
 */
void
device_unbusy(device_t dev)
{

	/*
	 * Mark the device as unbsy, recursively if this is the last busy count.
	 */
	if (refcount_release(&dev->busy) && dev->parent != NULL)
		device_unbusy(dev->parent);
}

/**
 * @brief Set the DF_QUIET flag for the device
 */
void
device_quiet(device_t dev)
{
	dev->flags |= DF_QUIET;
}

/**
 * @brief Set the DF_QUIET_CHILDREN flag for the device
 */
void
device_quiet_children(device_t dev)
{
	dev->flags |= DF_QUIET_CHILDREN;
}

/**
 * @brief Clear the DF_QUIET flag for the device
 */
void
device_verbose(device_t dev)
{
	dev->flags &= ~DF_QUIET;
}

ssize_t
device_get_property(device_t dev, const char *prop, void *val, size_t sz,
    device_property_type_t type)
{
	device_t bus = device_get_parent(dev);

	switch (type) {
	case DEVICE_PROP_ANY:
	case DEVICE_PROP_BUFFER:
	case DEVICE_PROP_HANDLE:	/* Size checks done in implementation. */
		break;
	case DEVICE_PROP_UINT32:
		if (sz % 4 != 0)
			return (-1);
		break;
	case DEVICE_PROP_UINT64:
		if (sz % 8 != 0)
			return (-1);
		break;
	default:
		return (-1);
	}

	return (BUS_GET_PROPERTY(bus, dev, prop, val, sz, type));
}

bool
device_has_property(device_t dev, const char *prop)
{
	return (device_get_property(dev, prop, NULL, 0, DEVICE_PROP_ANY) >= 0);
}

/**
 * @brief Return non-zero if the DF_QUIET_CHIDLREN flag is set on the device
 */
int
device_has_quiet_children(device_t dev)
{
	return ((dev->flags & DF_QUIET_CHILDREN) != 0);
}

/**
 * @brief Return non-zero if the DF_QUIET flag is set on the device
 */
int
device_is_quiet(device_t dev)
{
	return ((dev->flags & DF_QUIET) != 0);
}

/**
 * @brief Return non-zero if the DF_ENABLED flag is set on the device
 */
int
device_is_enabled(device_t dev)
{
	return ((dev->flags & DF_ENABLED) != 0);
}

/**
 * @brief Return non-zero if the device was successfully probed
 */
int
device_is_alive(device_t dev)
{
	return (dev->state >= DS_ALIVE);
}

/**
 * @brief Return non-zero if the device currently has a driver
 * attached to it
 */
int
device_is_attached(device_t dev)
{
	return (dev->state >= DS_ATTACHED);
}

/**
 * @brief Return non-zero if the device is currently suspended.
 */
int
device_is_suspended(device_t dev)
{
	return ((dev->flags & DF_SUSPENDED) != 0);
}

/**
 * @brief Set the devclass of a device
 * @see devclass_add_device().
 */
int
device_set_devclass(device_t dev, const char *classname)
{
	devclass_t dc;
	int error;

	if (!classname) {
		if (dev->devclass)
			devclass_delete_device(dev->devclass, dev);
		return (0);
	}

	if (dev->devclass) {
		printf("device_set_devclass: device class already set\n");
		return (EINVAL);
	}

	dc = devclass_find_internal(classname, NULL, TRUE);
	if (!dc)
		return (ENOMEM);

	error = devclass_add_device(dc, dev);

	bus_data_generation_update();
	return (error);
}

/**
 * @brief Set the devclass of a device and mark the devclass fixed.
 * @see device_set_devclass()
 */
int
device_set_devclass_fixed(device_t dev, const char *classname)
{
	int error;

	if (classname == NULL)
		return (EINVAL);

	error = device_set_devclass(dev, classname);
	if (error)
		return (error);
	dev->flags |= DF_FIXEDCLASS;
	return (0);
}

/**
 * @brief Query the device to determine if it's of a fixed devclass
 * @see device_set_devclass_fixed()
 */
bool
device_is_devclass_fixed(device_t dev)
{
	return ((dev->flags & DF_FIXEDCLASS) != 0);
}

/**
 * @brief Set the driver of a device
 *
 * @retval 0		success
 * @retval EBUSY	the device already has a driver attached
 * @retval ENOMEM	a memory allocation failure occurred
 */
int
device_set_driver(device_t dev, driver_t *driver)
{
	int domain;
	struct domainset *policy;

	if (dev->state >= DS_ATTACHED)
		return (EBUSY);

	if (dev->driver == driver)
		return (0);

	if (dev->softc && !(dev->flags & DF_EXTERNALSOFTC)) {
		free(dev->softc, M_BUS_SC);
		dev->softc = NULL;
	}
	device_set_desc(dev, NULL);
	kobj_delete((kobj_t) dev, NULL);
	dev->driver = driver;
	if (driver) {
		kobj_init((kobj_t) dev, (kobj_class_t) driver);
		if (!(dev->flags & DF_EXTERNALSOFTC) && driver->size > 0) {
			if (bus_get_domain(dev, &domain) == 0)
				policy = DOMAINSET_PREF(domain);
			else
				policy = DOMAINSET_RR();
			dev->softc = malloc_domainset(driver->size, M_BUS_SC,
			    policy, M_WAITOK | M_ZERO);
		}
	} else {
		kobj_init((kobj_t) dev, &null_class);
	}

	bus_data_generation_update();
	return (0);
}

/**
 * @brief Probe a device, and return this status.
 *
 * This function is the core of the device autoconfiguration
 * system. Its purpose is to select a suitable driver for a device and
 * then call that driver to initialise the hardware appropriately. The
 * driver is selected by calling the DEVICE_PROBE() method of a set of
 * candidate drivers and then choosing the driver which returned the
 * best value. This driver is then attached to the device using
 * device_attach().
 *
 * The set of suitable drivers is taken from the list of drivers in
 * the parent device's devclass. If the device was originally created
 * with a specific class name (see device_add_child()), only drivers
 * with that name are probed, otherwise all drivers in the devclass
 * are probed. If no drivers return successful probe values in the
 * parent devclass, the search continues in the parent of that
 * devclass (see devclass_get_parent()) if any.
 *
 * @param dev		the device to initialise
 *
 * @retval 0		success
 * @retval ENXIO	no driver was found
 * @retval ENOMEM	memory allocation failure
 * @retval non-zero	some other unix error code
 * @retval -1		Device already attached
 */
int
device_probe(device_t dev)
{
	int error;

	bus_topo_assert();

	if (dev->state >= DS_ALIVE)
		return (-1);

	if (!(dev->flags & DF_ENABLED)) {
		if (bootverbose && device_get_name(dev) != NULL) {
			device_print_prettyname(dev);
			printf("not probed (disabled)\n");
		}
		return (-1);
	}
	if ((error = device_probe_child(dev->parent, dev)) != 0) {
		if (bus_current_pass == BUS_PASS_DEFAULT &&
		    !(dev->flags & DF_DONENOMATCH)) {
			device_handle_nomatch(dev);
		}
		return (error);
	}
	return (0);
}

/**
 * @brief Probe a device and attach a driver if possible
 *
 * calls device_probe() and attaches if that was successful.
 */
int
device_probe_and_attach(device_t dev)
{
	int error;

	bus_topo_assert();

	error = device_probe(dev);
	if (error == -1)
		return (0);
	else if (error != 0)
		return (error);

	return (device_attach(dev));
}

/**
 * @brief Attach a device driver to a device
 *
 * This function is a wrapper around the DEVICE_ATTACH() driver
 * method. In addition to calling DEVICE_ATTACH(), it initialises the
 * device's sysctl tree, optionally prints a description of the device
 * and queues a notification event for user-based device management
 * services.
 *
 * Normally this function is only called internally from
 * device_probe_and_attach().
 *
 * @param dev		the device to initialise
 *
 * @retval 0		success
 * @retval ENXIO	no driver was found
 * @retval ENOMEM	memory allocation failure
 * @retval non-zero	some other unix error code
 */
int
device_attach(device_t dev)
{
	uint64_t attachtime;
	uint16_t attachentropy;
	int error;

	if (resource_disabled(dev->driver->name, dev->unit)) {
		/*
		 * Mostly detach the device, but leave it attached to
		 * the devclass to reserve the name and unit.
		 */
		device_disable(dev);
		(void)device_set_driver(dev, NULL);
		dev->state = DS_NOTPRESENT;
		if (bootverbose)
			 device_printf(dev, "disabled via hints entry\n");
		return (ENXIO);
	}

	KASSERT(IS_DEFAULT_VNET(TD_TO_VNET(curthread)),
	    ("device_attach: curthread is not in default vnet"));
	CURVNET_SET_QUIET(TD_TO_VNET(curthread));

	device_sysctl_init(dev);
	if (!device_is_quiet(dev))
		device_print_child(dev->parent, dev);
	attachtime = get_cyclecount();
	dev->state = DS_ATTACHING;
	if ((error = DEVICE_ATTACH(dev)) != 0) {
		printf("device_attach: %s%d attach returned %d\n",
		    dev->driver->name, dev->unit, error);
		BUS_CHILD_DETACHED(dev->parent, dev);
		if (disable_failed_devs) {
			/*
			 * When the user has asked to disable failed devices, we
			 * directly disable the device, but leave it in the
			 * attaching state. It will not try to probe/attach the
			 * device further. This leaves the device numbering
			 * intact for other similar devices in the system. It
			 * can be removed from this state with devctl.
			 */
			device_disable(dev);
		} else {
			/*
			 * Otherwise, when attach fails, tear down the state
			 * around that so we can retry when, for example, new
			 * drivers are loaded.
			 */
			if (!(dev->flags & DF_FIXEDCLASS))
				devclass_delete_device(dev->devclass, dev);
			(void)device_set_driver(dev, NULL);
			device_sysctl_fini(dev);
			KASSERT(dev->busy == 0, ("attach failed but busy"));
			dev->state = DS_NOTPRESENT;
		}
		CURVNET_RESTORE();
		return (error);
	}
	CURVNET_RESTORE();
	dev->flags |= DF_ATTACHED_ONCE;
	/*
	 * We only need the low bits of this time, but ranges from tens to thousands
	 * have been seen, so keep 2 bytes' worth.
	 */
	attachentropy = (uint16_t)(get_cyclecount() - attachtime);
	random_harvest_direct(&attachentropy, sizeof(attachentropy), RANDOM_ATTACH);
	device_sysctl_update(dev);
	dev->state = DS_ATTACHED;
	dev->flags &= ~DF_DONENOMATCH;
	EVENTHANDLER_DIRECT_INVOKE(device_attach, dev);
	return (0);
}

/**
 * @brief Detach a driver from a device
 *
 * This function is a wrapper around the DEVICE_DETACH() driver
 * method. If the call to DEVICE_DETACH() succeeds, it calls
 * BUS_CHILD_DETACHED() for the parent of @p dev, queues a
 * notification event for user-based device management services and
 * cleans up the device's sysctl tree.
 *
 * @param dev		the device to un-initialise
 *
 * @retval 0		success
 * @retval ENXIO	no driver was found
 * @retval ENOMEM	memory allocation failure
 * @retval non-zero	some other unix error code
 */
int
device_detach(device_t dev)
{
	int error;

	bus_topo_assert();

	PDEBUG(("%s", DEVICENAME(dev)));
	if (dev->busy > 0)
		return (EBUSY);
	if (dev->state == DS_ATTACHING) {
		device_printf(dev, "device in attaching state! Deferring detach.\n");
		return (EBUSY);
	}
	if (dev->state != DS_ATTACHED)
		return (0);

	EVENTHANDLER_DIRECT_INVOKE(device_detach, dev, EVHDEV_DETACH_BEGIN);
	if ((error = DEVICE_DETACH(dev)) != 0) {
		EVENTHANDLER_DIRECT_INVOKE(device_detach, dev,
		    EVHDEV_DETACH_FAILED);
		return (error);
	} else {
		EVENTHANDLER_DIRECT_INVOKE(device_detach, dev,
		    EVHDEV_DETACH_COMPLETE);
	}
	if (!device_is_quiet(dev))
		device_printf(dev, "detached\n");
	if (dev->parent)
		BUS_CHILD_DETACHED(dev->parent, dev);

	if (!(dev->flags & DF_FIXEDCLASS))
		devclass_delete_device(dev->devclass, dev);

	device_verbose(dev);
	dev->state = DS_NOTPRESENT;
	(void)device_set_driver(dev, NULL);
	device_sysctl_fini(dev);

	return (0);
}

/**
 * @brief Tells a driver to quiesce itself.
 *
 * This function is a wrapper around the DEVICE_QUIESCE() driver
 * method. If the call to DEVICE_QUIESCE() succeeds.
 *
 * @param dev		the device to quiesce
 *
 * @retval 0		success
 * @retval ENXIO	no driver was found
 * @retval ENOMEM	memory allocation failure
 * @retval non-zero	some other unix error code
 */
int
device_quiesce(device_t dev)
{
	PDEBUG(("%s", DEVICENAME(dev)));
	if (dev->busy > 0)
		return (EBUSY);
	if (dev->state != DS_ATTACHED)
		return (0);

	return (DEVICE_QUIESCE(dev));
}

/**
 * @brief Notify a device of system shutdown
 *
 * This function calls the DEVICE_SHUTDOWN() driver method if the
 * device currently has an attached driver.
 *
 * @returns the value returned by DEVICE_SHUTDOWN()
 */
int
device_shutdown(device_t dev)
{
	if (dev->state < DS_ATTACHED)
		return (0);
	return (DEVICE_SHUTDOWN(dev));
}

/**
 * @brief Set the unit number of a device
 *
 * This function can be used to override the unit number used for a
 * device (e.g. to wire a device to a pre-configured unit number).
 */
int
device_set_unit(device_t dev, int unit)
{
	devclass_t dc;
	int err;

	if (unit == dev->unit)
		return (0);
	dc = device_get_devclass(dev);
	if (unit < dc->maxunit && dc->devices[unit])
		return (EBUSY);
	err = devclass_delete_device(dc, dev);
	if (err)
		return (err);
	dev->unit = unit;
	err = devclass_add_device(dc, dev);
	if (err)
		return (err);

	bus_data_generation_update();
	return (0);
}

/*======================================*/
/*
 * Some useful method implementations to make life easier for bus drivers.
 */

/**
 * @brief Initialize a resource mapping request
 *
 * This is the internal implementation of the public API
 * resource_init_map_request.  Callers may be using a different layout
 * of struct resource_map_request than the kernel, so callers pass in
 * the size of the structure they are using to identify the structure
 * layout.
 */
void
resource_init_map_request_impl(struct resource_map_request *args, size_t sz)
{
	bzero(args, sz);
	args->size = sz;
	args->memattr = VM_MEMATTR_DEVICE;
}

/**
 * @brief Validate a resource mapping request
 *
 * Translate a device driver's mapping request (@p in) to a struct
 * resource_map_request using the current structure layout (@p out).
 * In addition, validate the offset and length from the mapping
 * request against the bounds of the resource @p r.  If the offset or
 * length are invalid, fail with EINVAL.  If the offset and length are
 * valid, the absolute starting address of the requested mapping is
 * returned in @p startp and the length of the requested mapping is
 * returned in @p lengthp.
 */
int
resource_validate_map_request(struct resource *r,
    struct resource_map_request *in, struct resource_map_request *out,
    rman_res_t *startp, rman_res_t *lengthp)
{
	rman_res_t end, length, start;

	/*
	 * This assumes that any callers of this function are compiled
	 * into the kernel and use the same version of the structure
	 * as this file.
	 */
	MPASS(out->size == sizeof(struct resource_map_request));

	if (in != NULL)
		bcopy(in, out, imin(in->size, out->size));
	start = rman_get_start(r) + out->offset;
	if (out->length == 0)
		length = rman_get_size(r);
	else
		length = out->length;
	end = start + length - 1;
	if (start > rman_get_end(r) || start < rman_get_start(r))
		return (EINVAL);
	if (end > rman_get_end(r) || end < start)
		return (EINVAL);
	*lengthp = length;
	*startp = start;
	return (0);
}

/**
 * @brief Initialise a resource list.
 *
 * @param rl		the resource list to initialise
 */
void
resource_list_init(struct resource_list *rl)
{
	STAILQ_INIT(rl);
}

/**
 * @brief Reclaim memory used by a resource list.
 *
 * This function frees the memory for all resource entries on the list
 * (if any).
 *
 * @param rl		the resource list to free
 */
void
resource_list_free(struct resource_list *rl)
{
	struct resource_list_entry *rle;

	while ((rle = STAILQ_FIRST(rl)) != NULL) {
		if (rle->res)
			panic("resource_list_free: resource entry is busy");
		STAILQ_REMOVE_HEAD(rl, link);
		free(rle, M_BUS);
	}
}

/**
 * @brief Add a resource entry.
 *
 * This function adds a resource entry using the given @p type, @p
 * start, @p end and @p count values. A rid value is chosen by
 * searching sequentially for the first unused rid starting at zero.
 *
 * @param rl		the resource list to edit
 * @param type		the resource entry type (e.g. SYS_RES_MEMORY)
 * @param start		the start address of the resource
 * @param end		the end address of the resource
 * @param count		XXX end-start+1
 */
int
resource_list_add_next(struct resource_list *rl, int type, rman_res_t start,
    rman_res_t end, rman_res_t count)
{
	int rid;

	rid = 0;
	while (resource_list_find(rl, type, rid) != NULL)
		rid++;
	resource_list_add(rl, type, rid, start, end, count);
	return (rid);
}

/**
 * @brief Add or modify a resource entry.
 *
 * If an existing entry exists with the same type and rid, it will be
 * modified using the given values of @p start, @p end and @p
 * count. If no entry exists, a new one will be created using the
 * given values.  The resource list entry that matches is then returned.
 *
 * @param rl		the resource list to edit
 * @param type		the resource entry type (e.g. SYS_RES_MEMORY)
 * @param rid		the resource identifier
 * @param start		the start address of the resource
 * @param end		the end address of the resource
 * @param count		XXX end-start+1
 */
struct resource_list_entry *
resource_list_add(struct resource_list *rl, int type, int rid,
    rman_res_t start, rman_res_t end, rman_res_t count)
{
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (!rle) {
		rle = malloc(sizeof(struct resource_list_entry), M_BUS,
		    M_WAITOK);
		STAILQ_INSERT_TAIL(rl, rle, link);
		rle->type = type;
		rle->rid = rid;
		rle->res = NULL;
		rle->flags = 0;
	}

	if (rle->res)
		panic("resource_list_add: resource entry is busy");

	rle->start = start;
	rle->end = end;
	rle->count = count;
	return (rle);
}

/**
 * @brief Determine if a resource entry is busy.
 *
 * Returns true if a resource entry is busy meaning that it has an
 * associated resource that is not an unallocated "reserved" resource.
 *
 * @param rl		the resource list to search
 * @param type		the resource entry type (e.g. SYS_RES_MEMORY)
 * @param rid		the resource identifier
 *
 * @returns Non-zero if the entry is busy, zero otherwise.
 */
int
resource_list_busy(struct resource_list *rl, int type, int rid)
{
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (rle == NULL || rle->res == NULL)
		return (0);
	if ((rle->flags & (RLE_RESERVED | RLE_ALLOCATED)) == RLE_RESERVED) {
		KASSERT(!(rman_get_flags(rle->res) & RF_ACTIVE),
		    ("reserved resource is active"));
		return (0);
	}
	return (1);
}

/**
 * @brief Determine if a resource entry is reserved.
 *
 * Returns true if a resource entry is reserved meaning that it has an
 * associated "reserved" resource.  The resource can either be
 * allocated or unallocated.
 *
 * @param rl		the resource list to search
 * @param type		the resource entry type (e.g. SYS_RES_MEMORY)
 * @param rid		the resource identifier
 *
 * @returns Non-zero if the entry is reserved, zero otherwise.
 */
int
resource_list_reserved(struct resource_list *rl, int type, int rid)
{
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (rle != NULL && rle->flags & RLE_RESERVED)
		return (1);
	return (0);
}

/**
 * @brief Find a resource entry by type and rid.
 *
 * @param rl		the resource list to search
 * @param type		the resource entry type (e.g. SYS_RES_MEMORY)
 * @param rid		the resource identifier
 *
 * @returns the resource entry pointer or NULL if there is no such
 * entry.
 */
struct resource_list_entry *
resource_list_find(struct resource_list *rl, int type, int rid)
{
	struct resource_list_entry *rle;

	STAILQ_FOREACH(rle, rl, link) {
		if (rle->type == type && rle->rid == rid)
			return (rle);
	}
	return (NULL);
}

/**
 * @brief Delete a resource entry.
 *
 * @param rl		the resource list to edit
 * @param type		the resource entry type (e.g. SYS_RES_MEMORY)
 * @param rid		the resource identifier
 */
void
resource_list_delete(struct resource_list *rl, int type, int rid)
{
	struct resource_list_entry *rle = resource_list_find(rl, type, rid);

	if (rle) {
		if (rle->res != NULL)
			panic("resource_list_delete: resource has not been released");
		STAILQ_REMOVE(rl, rle, resource_list_entry, link);
		free(rle, M_BUS);
	}
}

/**
 * @brief Allocate a reserved resource
 *
 * This can be used by buses to force the allocation of resources
 * that are always active in the system even if they are not allocated
 * by a driver (e.g. PCI BARs).  This function is usually called when
 * adding a new child to the bus.  The resource is allocated from the
 * parent bus when it is reserved.  The resource list entry is marked
 * with RLE_RESERVED to note that it is a reserved resource.
 *
 * Subsequent attempts to allocate the resource with
 * resource_list_alloc() will succeed the first time and will set
 * RLE_ALLOCATED to note that it has been allocated.  When a reserved
 * resource that has been allocated is released with
 * resource_list_release() the resource RLE_ALLOCATED is cleared, but
 * the actual resource remains allocated.  The resource can be released to
 * the parent bus by calling resource_list_unreserve().
 *
 * @param rl		the resource list to allocate from
 * @param bus		the parent device of @p child
 * @param child		the device for which the resource is being reserved
 * @param type		the type of resource to allocate
 * @param rid		a pointer to the resource identifier
 * @param start		hint at the start of the resource range - pass
 *			@c 0 for any start address
 * @param end		hint at the end of the resource range - pass
 *			@c ~0 for any end address
 * @param count		hint at the size of range required - pass @c 1
 *			for any size
 * @param flags		any extra flags to control the resource
 *			allocation - see @c RF_XXX flags in
 *			<sys/rman.h> for details
 *
 * @returns		the resource which was allocated or @c NULL if no
 *			resource could be allocated
 */
struct resource *
resource_list_reserve(struct resource_list *rl, device_t bus, device_t child,
    int type, int *rid, rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource_list_entry *rle = NULL;
	int passthrough = (device_get_parent(child) != bus);
	struct resource *r;

	if (passthrough)
		panic(
    "resource_list_reserve() should only be called for direct children");
	if (flags & RF_ACTIVE)
		panic(
    "resource_list_reserve() should only reserve inactive resources");

	r = resource_list_alloc(rl, bus, child, type, rid, start, end, count,
	    flags);
	if (r != NULL) {
		rle = resource_list_find(rl, type, *rid);
		rle->flags |= RLE_RESERVED;
	}
	return (r);
}

/**
 * @brief Helper function for implementing BUS_ALLOC_RESOURCE()
 *
 * Implement BUS_ALLOC_RESOURCE() by looking up a resource from the list
 * and passing the allocation up to the parent of @p bus. This assumes
 * that the first entry of @c device_get_ivars(child) is a struct
 * resource_list. This also handles 'passthrough' allocations where a
 * child is a remote descendant of bus by passing the allocation up to
 * the parent of bus.
 *
 * Typically, a bus driver would store a list of child resources
 * somewhere in the child device's ivars (see device_get_ivars()) and
 * its implementation of BUS_ALLOC_RESOURCE() would find that list and
 * then call resource_list_alloc() to perform the allocation.
 *
 * @param rl		the resource list to allocate from
 * @param bus		the parent device of @p child
 * @param child		the device which is requesting an allocation
 * @param type		the type of resource to allocate
 * @param rid		a pointer to the resource identifier
 * @param start		hint at the start of the resource range - pass
 *			@c 0 for any start address
 * @param end		hint at the end of the resource range - pass
 *			@c ~0 for any end address
 * @param count		hint at the size of range required - pass @c 1
 *			for any size
 * @param flags		any extra flags to control the resource
 *			allocation - see @c RF_XXX flags in
 *			<sys/rman.h> for details
 *
 * @returns		the resource which was allocated or @c NULL if no
 *			resource could be allocated
 */
struct resource *
resource_list_alloc(struct resource_list *rl, device_t bus, device_t child,
    int type, int *rid, rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource_list_entry *rle = NULL;
	int passthrough = (device_get_parent(child) != bus);
	int isdefault = RMAN_IS_DEFAULT_RANGE(start, end);

	if (passthrough) {
		return (BUS_ALLOC_RESOURCE(device_get_parent(bus), child,
		    type, rid, start, end, count, flags));
	}

	rle = resource_list_find(rl, type, *rid);

	if (!rle)
		return (NULL);		/* no resource of that type/rid */

	if (rle->res) {
		if (rle->flags & RLE_RESERVED) {
			if (rle->flags & RLE_ALLOCATED)
				return (NULL);
			if ((flags & RF_ACTIVE) &&
			    bus_activate_resource(child, type, *rid,
			    rle->res) != 0)
				return (NULL);
			rle->flags |= RLE_ALLOCATED;
			return (rle->res);
		}
		device_printf(bus,
		    "resource entry %#x type %d for child %s is busy\n", *rid,
		    type, device_get_nameunit(child));
		return (NULL);
	}

	if (isdefault) {
		start = rle->start;
		count = ulmax(count, rle->count);
		end = ulmax(rle->end, start + count - 1);
	}

	rle->res = BUS_ALLOC_RESOURCE(device_get_parent(bus), child,
	    type, rid, start, end, count, flags);

	/*
	 * Record the new range.
	 */
	if (rle->res) {
		rle->start = rman_get_start(rle->res);
		rle->end = rman_get_end(rle->res);
		rle->count = count;
	}

	return (rle->res);
}

/**
 * @brief Helper function for implementing BUS_RELEASE_RESOURCE()
 *
 * Implement BUS_RELEASE_RESOURCE() using a resource list. Normally
 * used with resource_list_alloc().
 *
 * @param rl		the resource list which was allocated from
 * @param bus		the parent device of @p child
 * @param child		the device which is requesting a release
 * @param res		the resource to release
 *
 * @retval 0		success
 * @retval non-zero	a standard unix error code indicating what
 *			error condition prevented the operation
 */
int
resource_list_release(struct resource_list *rl, device_t bus, device_t child,
    struct resource *res)
{
	struct resource_list_entry *rle = NULL;
	int passthrough = (device_get_parent(child) != bus);
	int error;

	if (passthrough) {
		return (BUS_RELEASE_RESOURCE(device_get_parent(bus), child,
		    res));
	}

	rle = resource_list_find(rl, rman_get_type(res), rman_get_rid(res));

	if (!rle)
		panic("resource_list_release: can't find resource");
	if (!rle->res)
		panic("resource_list_release: resource entry is not busy");
	if (rle->flags & RLE_RESERVED) {
		if (rle->flags & RLE_ALLOCATED) {
			if (rman_get_flags(res) & RF_ACTIVE) {
				error = bus_deactivate_resource(child, res);
				if (error)
					return (error);
			}
			rle->flags &= ~RLE_ALLOCATED;
			return (0);
		}
		return (EINVAL);
	}

	error = BUS_RELEASE_RESOURCE(device_get_parent(bus), child, res);
	if (error)
		return (error);

	rle->res = NULL;
	return (0);
}

/**
 * @brief Release all active resources of a given type
 *
 * Release all active resources of a specified type.  This is intended
 * to be used to cleanup resources leaked by a driver after detach or
 * a failed attach.
 *
 * @param rl		the resource list which was allocated from
 * @param bus		the parent device of @p child
 * @param child		the device whose active resources are being released
 * @param type		the type of resources to release
 *
 * @retval 0		success
 * @retval EBUSY	at least one resource was active
 */
int
resource_list_release_active(struct resource_list *rl, device_t bus,
    device_t child, int type)
{
	struct resource_list_entry *rle;
	int error, retval;

	retval = 0;
	STAILQ_FOREACH(rle, rl, link) {
		if (rle->type != type)
			continue;
		if (rle->res == NULL)
			continue;
		if ((rle->flags & (RLE_RESERVED | RLE_ALLOCATED)) ==
		    RLE_RESERVED)
			continue;
		retval = EBUSY;
		error = resource_list_release(rl, bus, child, rle->res);
		if (error != 0)
			device_printf(bus,
			    "Failed to release active resource: %d\n", error);
	}
	return (retval);
}

/**
 * @brief Fully release a reserved resource
 *
 * Fully releases a resource reserved via resource_list_reserve().
 *
 * @param rl		the resource list which was allocated from
 * @param bus		the parent device of @p child
 * @param child		the device whose reserved resource is being released
 * @param type		the type of resource to release
 * @param rid		the resource identifier
 * @param res		the resource to release
 *
 * @retval 0		success
 * @retval non-zero	a standard unix error code indicating what
 *			error condition prevented the operation
 */
int
resource_list_unreserve(struct resource_list *rl, device_t bus, device_t child,
    int type, int rid)
{
	struct resource_list_entry *rle = NULL;
	int passthrough = (device_get_parent(child) != bus);

	if (passthrough)
		panic(
    "resource_list_unreserve() should only be called for direct children");

	rle = resource_list_find(rl, type, rid);

	if (!rle)
		panic("resource_list_unreserve: can't find resource");
	if (!(rle->flags & RLE_RESERVED))
		return (EINVAL);
	if (rle->flags & RLE_ALLOCATED)
		return (EBUSY);
	rle->flags &= ~RLE_RESERVED;
	return (resource_list_release(rl, bus, child, rle->res));
}

/**
 * @brief Print a description of resources in a resource list
 *
 * Print all resources of a specified type, for use in BUS_PRINT_CHILD().
 * The name is printed if at least one resource of the given type is available.
 * The format is used to print resource start and end.
 *
 * @param rl		the resource list to print
 * @param name		the name of @p type, e.g. @c "memory"
 * @param type		type type of resource entry to print
 * @param format	printf(9) format string to print resource
 *			start and end values
 *
 * @returns		the number of characters printed
 */
int
resource_list_print_type(struct resource_list *rl, const char *name, int type,
    const char *format)
{
	struct resource_list_entry *rle;
	int printed, retval;

	printed = 0;
	retval = 0;
	/* Yes, this is kinda cheating */
	STAILQ_FOREACH(rle, rl, link) {
		if (rle->type == type) {
			if (printed == 0)
				retval += printf(" %s ", name);
			else
				retval += printf(",");
			printed++;
			retval += printf(format, rle->start);
			if (rle->count > 1) {
				retval += printf("-");
				retval += printf(format, rle->start +
						 rle->count - 1);
			}
		}
	}
	return (retval);
}

/**
 * @brief Releases all the resources in a list.
 *
 * @param rl		The resource list to purge.
 *
 * @returns		nothing
 */
void
resource_list_purge(struct resource_list *rl)
{
	struct resource_list_entry *rle;

	while ((rle = STAILQ_FIRST(rl)) != NULL) {
		if (rle->res)
			bus_release_resource(rman_get_device(rle->res),
			    rle->type, rle->rid, rle->res);
		STAILQ_REMOVE_HEAD(rl, link);
		free(rle, M_BUS);
	}
}

device_t
bus_generic_add_child(device_t dev, u_int order, const char *name, int unit)
{
	return (device_add_child_ordered(dev, order, name, unit));
}

/**
 * @brief Helper function for implementing DEVICE_PROBE()
 *
 * This function can be used to help implement the DEVICE_PROBE() for
 * a bus (i.e. a device which has other devices attached to it). It
 * calls the DEVICE_IDENTIFY() method of each driver in the device's
 * devclass.
 */
int
bus_generic_probe(device_t dev)
{
	bus_identify_children(dev);
	return (0);
}

/**
 * @brief Ask drivers to add child devices of the given device.
 *
 * This function allows drivers for child devices of a bus to identify
 * child devices and add them as children of the given device.  NB:
 * The driver for @param dev must implement the BUS_ADD_CHILD method.
 *
 * @param dev		the parent device
 */
void
bus_identify_children(device_t dev)
{
	devclass_t dc = dev->devclass;
	driverlink_t dl;

	TAILQ_FOREACH(dl, &dc->drivers, link) {
		/*
		 * If this driver's pass is too high, then ignore it.
		 * For most drivers in the default pass, this will
		 * never be true.  For early-pass drivers they will
		 * only call the identify routines of eligible drivers
		 * when this routine is called.  Drivers for later
		 * passes should have their identify routines called
		 * on early-pass buses during BUS_NEW_PASS().
		 */
		if (dl->pass > bus_current_pass)
			continue;
		DEVICE_IDENTIFY(dl->driver, dev);
	}
}

/**
 * @brief Helper function for implementing DEVICE_ATTACH()
 *
 * This function can be used to help implement the DEVICE_ATTACH() for
 * a bus. It calls device_probe_and_attach() for each of the device's
 * children.
 */
int
bus_generic_attach(device_t dev)
{
	bus_attach_children(dev);
	return (0);
}

/**
 * @brief Probe and attach all children of the given device
 *
 * This function attempts to attach a device driver to each unattached
 * child of the given device using device_probe_and_attach().  If an
 * individual child fails to attach this function continues attaching
 * other children.
 *
 * @param dev		the parent device
 */
void
bus_attach_children(device_t dev)
{
	device_t child;

	TAILQ_FOREACH(child, &dev->children, link) {
		device_probe_and_attach(child);
	}
}

/**
 * @brief Helper function for delaying attaching children
 *
 * Many buses can't run transactions on the bus which children need to probe and
 * attach until after interrupts and/or timers are running.  This function
 * delays their attach until interrupts and timers are enabled.
 */
void
bus_delayed_attach_children(device_t dev)
{
	/* Probe and attach the bus children when interrupts are available */
	config_intrhook_oneshot((ich_func_t)bus_attach_children, dev);
}

/**
 * @brief Helper function for implementing DEVICE_DETACH()
 *
 * This function can be used to help implement the DEVICE_DETACH() for
 * a bus.  It detaches and deletes all children.  If an individual
 * child fails to detach, this function stops and returns an error.
 *
 * @param dev		the parent device
 *
 * @retval 0		success
 * @retval non-zero	a device would not detach
 */
int
bus_generic_detach(device_t dev)
{
	int error;

	error = bus_detach_children(dev);
	if (error != 0)
		return (error);

	return (device_delete_children(dev));
}

/**
 * @brief Detach drivers from all children of a device
 *
 * This function attempts to detach a device driver from each attached
 * child of the given device using device_detach().  If an individual
 * child fails to detach this function stops and returns an error.
 * NB: Children that were successfully detached are not re-attached if
 * an error occurs.
 *
 * @param dev		the parent device
 *
 * @retval 0		success
 * @retval non-zero	a device would not detach
 */
int
bus_detach_children(device_t dev)
{
	device_t child;
	int error;

	/*
	 * Detach children in the reverse order.
	 * See bus_generic_suspend for details.
	 */
	TAILQ_FOREACH_REVERSE(child, &dev->children, device_list, link) {
		if ((error = device_detach(child)) != 0)
			return (error);
	}

	return (0);
}

/**
 * @brief Helper function for implementing DEVICE_SHUTDOWN()
 *
 * This function can be used to help implement the DEVICE_SHUTDOWN()
 * for a bus. It calls device_shutdown() for each of the device's
 * children.
 */
int
bus_generic_shutdown(device_t dev)
{
	device_t child;

	/*
	 * Shut down children in the reverse order.
	 * See bus_generic_suspend for details.
	 */
	TAILQ_FOREACH_REVERSE(child, &dev->children, device_list, link) {
		device_shutdown(child);
	}

	return (0);
}

/**
 * @brief Default function for suspending a child device.
 *
 * This function is to be used by a bus's DEVICE_SUSPEND_CHILD().
 */
int
bus_generic_suspend_child(device_t dev, device_t child)
{
	int	error;

	error = DEVICE_SUSPEND(child);

	if (error == 0) {
		child->flags |= DF_SUSPENDED;
	} else {
		printf("DEVICE_SUSPEND(%s) failed: %d\n",
		    device_get_nameunit(child), error);
	}

	return (error);
}

/**
 * @brief Default function for resuming a child device.
 *
 * This function is to be used by a bus's DEVICE_RESUME_CHILD().
 */
int
bus_generic_resume_child(device_t dev, device_t child)
{
	DEVICE_RESUME(child);
	child->flags &= ~DF_SUSPENDED;

	return (0);
}

/**
 * @brief Helper function for implementing DEVICE_SUSPEND()
 *
 * This function can be used to help implement the DEVICE_SUSPEND()
 * for a bus. It calls DEVICE_SUSPEND() for each of the device's
 * children. If any call to DEVICE_SUSPEND() fails, the suspend
 * operation is aborted and any devices which were suspended are
 * resumed immediately by calling their DEVICE_RESUME() methods.
 */
int
bus_generic_suspend(device_t dev)
{
	int		error;
	device_t	child;

	/*
	 * Suspend children in the reverse order.
	 * For most buses all children are equal, so the order does not matter.
	 * Other buses, such as acpi, carefully order their child devices to
	 * express implicit dependencies between them.  For such buses it is
	 * safer to bring down devices in the reverse order.
	 */
	TAILQ_FOREACH_REVERSE(child, &dev->children, device_list, link) {
		error = BUS_SUSPEND_CHILD(dev, child);
		if (error != 0) {
			child = TAILQ_NEXT(child, link);
			if (child != NULL) {
				TAILQ_FOREACH_FROM(child, &dev->children, link)
					BUS_RESUME_CHILD(dev, child);
			}
			return (error);
		}
	}
	return (0);
}

/**
 * @brief Helper function for implementing DEVICE_RESUME()
 *
 * This function can be used to help implement the DEVICE_RESUME() for
 * a bus. It calls DEVICE_RESUME() on each of the device's children.
 */
int
bus_generic_resume(device_t dev)
{
	device_t	child;

	TAILQ_FOREACH(child, &dev->children, link) {
		BUS_RESUME_CHILD(dev, child);
		/* if resume fails, there's nothing we can usefully do... */
	}
	return (0);
}

/**
 * @brief Helper function for implementing BUS_RESET_POST
 *
 * Bus can use this function to implement common operations of
 * re-attaching or resuming the children after the bus itself was
 * reset, and after restoring bus-unique state of children.
 *
 * @param dev	The bus
 * #param flags	DEVF_RESET_*
 */
int
bus_helper_reset_post(device_t dev, int flags)
{
	device_t child;
	int error, error1;

	error = 0;
	TAILQ_FOREACH(child, &dev->children,link) {
		BUS_RESET_POST(dev, child);
		error1 = (flags & DEVF_RESET_DETACH) != 0 ?
		    device_probe_and_attach(child) :
		    BUS_RESUME_CHILD(dev, child);
		if (error == 0 && error1 != 0)
			error = error1;
	}
	return (error);
}

static void
bus_helper_reset_prepare_rollback(device_t dev, device_t child, int flags)
{
	child = TAILQ_NEXT(child, link);
	if (child == NULL)
		return;
	TAILQ_FOREACH_FROM(child, &dev->children,link) {
		BUS_RESET_POST(dev, child);
		if ((flags & DEVF_RESET_DETACH) != 0)
			device_probe_and_attach(child);
		else
			BUS_RESUME_CHILD(dev, child);
	}
}

/**
 * @brief Helper function for implementing BUS_RESET_PREPARE
 *
 * Bus can use this function to implement common operations of
 * detaching or suspending the children before the bus itself is
 * reset, and then save bus-unique state of children that must
 * persists around reset.
 *
 * @param dev	The bus
 * #param flags	DEVF_RESET_*
 */
int
bus_helper_reset_prepare(device_t dev, int flags)
{
	device_t child;
	int error;

	if (dev->state != DS_ATTACHED)
		return (EBUSY);

	TAILQ_FOREACH_REVERSE(child, &dev->children, device_list, link) {
		if ((flags & DEVF_RESET_DETACH) != 0) {
			error = device_get_state(child) == DS_ATTACHED ?
			    device_detach(child) : 0;
		} else {
			error = BUS_SUSPEND_CHILD(dev, child);
		}
		if (error == 0) {
			error = BUS_RESET_PREPARE(dev, child);
			if (error != 0) {
				if ((flags & DEVF_RESET_DETACH) != 0)
					device_probe_and_attach(child);
				else
					BUS_RESUME_CHILD(dev, child);
			}
		}
		if (error != 0) {
			bus_helper_reset_prepare_rollback(dev, child, flags);
			return (error);
		}
	}
	return (0);
}

/**
 * @brief Helper function for implementing BUS_PRINT_CHILD().
 *
 * This function prints the first part of the ascii representation of
 * @p child, including its name, unit and description (if any - see
 * device_set_desc()).
 *
 * @returns the number of characters printed
 */
int
bus_print_child_header(device_t dev, device_t child)
{
	int	retval = 0;

	if (device_get_desc(child)) {
		retval += device_printf(child, "<%s>", device_get_desc(child));
	} else {
		retval += printf("%s", device_get_nameunit(child));
	}

	return (retval);
}

/**
 * @brief Helper function for implementing BUS_PRINT_CHILD().
 *
 * This function prints the last part of the ascii representation of
 * @p child, which consists of the string @c " on " followed by the
 * name and unit of the @p dev.
 *
 * @returns the number of characters printed
 */
int
bus_print_child_footer(device_t dev, device_t child)
{
	return (printf(" on %s\n", device_get_nameunit(dev)));
}

/**
 * @brief Helper function for implementing BUS_PRINT_CHILD().
 *
 * This function prints out the VM domain for the given device.
 *
 * @returns the number of characters printed
 */
int
bus_print_child_domain(device_t dev, device_t child)
{
	int domain;

	/* No domain? Don't print anything */
	if (BUS_GET_DOMAIN(dev, child, &domain) != 0)
		return (0);

	return (printf(" numa-domain %d", domain));
}

/**
 * @brief Helper function for implementing BUS_PRINT_CHILD().
 *
 * This function simply calls bus_print_child_header() followed by
 * bus_print_child_footer().
 *
 * @returns the number of characters printed
 */
int
bus_generic_print_child(device_t dev, device_t child)
{
	int	retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += bus_print_child_domain(dev, child);
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

/**
 * @brief Stub function for implementing BUS_READ_IVAR().
 *
 * @returns ENOENT
 */
int
bus_generic_read_ivar(device_t dev, device_t child, int index,
    uintptr_t * result)
{
	return (ENOENT);
}

/**
 * @brief Stub function for implementing BUS_WRITE_IVAR().
 *
 * @returns ENOENT
 */
int
bus_generic_write_ivar(device_t dev, device_t child, int index,
    uintptr_t value)
{
	return (ENOENT);
}

/**
 * @brief Helper function for implementing BUS_GET_PROPERTY().
 *
 * This simply calls the BUS_GET_PROPERTY of the parent of dev,
 * until a non-default implementation is found.
 */
ssize_t
bus_generic_get_property(device_t dev, device_t child, const char *propname,
    void *propvalue, size_t size, device_property_type_t type)
{
	if (device_get_parent(dev) != NULL)
		return (BUS_GET_PROPERTY(device_get_parent(dev), child,
		    propname, propvalue, size, type));

	return (-1);
}

/**
 * @brief Helper function for implementing BUS_DRIVER_ADDED().
 *
 * This implementation of BUS_DRIVER_ADDED() simply calls the driver's
 * DEVICE_IDENTIFY() method to allow it to add new children to the bus
 * and then calls device_probe_and_attach() for each unattached child.
 */
void
bus_generic_driver_added(device_t dev, driver_t *driver)
{
	device_t child;

	DEVICE_IDENTIFY(driver, dev);
	TAILQ_FOREACH(child, &dev->children, link) {
		if (child->state == DS_NOTPRESENT)
			device_probe_and_attach(child);
	}
}

/**
 * @brief Helper function for implementing BUS_NEW_PASS().
 *
 * This implementing of BUS_NEW_PASS() first calls the identify
 * routines for any drivers that probe at the current pass.  Then it
 * walks the list of devices for this bus.  If a device is already
 * attached, then it calls BUS_NEW_PASS() on that device.  If the
 * device is not already attached, it attempts to attach a driver to
 * it.
 */
void
bus_generic_new_pass(device_t dev)
{
	driverlink_t dl;
	devclass_t dc;
	device_t child;

	dc = dev->devclass;
	TAILQ_FOREACH(dl, &dc->drivers, link) {
		if (dl->pass == bus_current_pass)
			DEVICE_IDENTIFY(dl->driver, dev);
	}
	TAILQ_FOREACH(child, &dev->children, link) {
		if (child->state >= DS_ATTACHED)
			BUS_NEW_PASS(child);
		else if (child->state == DS_NOTPRESENT)
			device_probe_and_attach(child);
	}
}

/**
 * @brief Helper function for implementing BUS_SETUP_INTR().
 *
 * This simple implementation of BUS_SETUP_INTR() simply calls the
 * BUS_SETUP_INTR() method of the parent of @p dev.
 */
int
bus_generic_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_filter_t *filter, driver_intr_t *intr, void *arg,
    void **cookiep)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_SETUP_INTR(dev->parent, child, irq, flags,
		    filter, intr, arg, cookiep));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_TEARDOWN_INTR().
 *
 * This simple implementation of BUS_TEARDOWN_INTR() simply calls the
 * BUS_TEARDOWN_INTR() method of the parent of @p dev.
 */
int
bus_generic_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_TEARDOWN_INTR(dev->parent, child, irq, cookie));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_SUSPEND_INTR().
 *
 * This simple implementation of BUS_SUSPEND_INTR() simply calls the
 * BUS_SUSPEND_INTR() method of the parent of @p dev.
 */
int
bus_generic_suspend_intr(device_t dev, device_t child, struct resource *irq)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_SUSPEND_INTR(dev->parent, child, irq));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_RESUME_INTR().
 *
 * This simple implementation of BUS_RESUME_INTR() simply calls the
 * BUS_RESUME_INTR() method of the parent of @p dev.
 */
int
bus_generic_resume_intr(device_t dev, device_t child, struct resource *irq)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_RESUME_INTR(dev->parent, child, irq));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_ADJUST_RESOURCE().
 *
 * This simple implementation of BUS_ADJUST_RESOURCE() simply calls the
 * BUS_ADJUST_RESOURCE() method of the parent of @p dev.
 */
int
bus_generic_adjust_resource(device_t dev, device_t child, struct resource *r,
    rman_res_t start, rman_res_t end)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_ADJUST_RESOURCE(dev->parent, child, r, start, end));
	return (EINVAL);
}

/*
 * @brief Helper function for implementing BUS_TRANSLATE_RESOURCE().
 *
 * This simple implementation of BUS_TRANSLATE_RESOURCE() simply calls the
 * BUS_TRANSLATE_RESOURCE() method of the parent of @p dev.  If there is no
 * parent, no translation happens.
 */
int
bus_generic_translate_resource(device_t dev, int type, rman_res_t start,
    rman_res_t *newstart)
{
	if (dev->parent)
		return (BUS_TRANSLATE_RESOURCE(dev->parent, type, start,
		    newstart));
	*newstart = start;
	return (0);
}

/**
 * @brief Helper function for implementing BUS_ALLOC_RESOURCE().
 *
 * This simple implementation of BUS_ALLOC_RESOURCE() simply calls the
 * BUS_ALLOC_RESOURCE() method of the parent of @p dev.
 */
struct resource *
bus_generic_alloc_resource(device_t dev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_ALLOC_RESOURCE(dev->parent, child, type, rid,
		    start, end, count, flags));
	return (NULL);
}

/**
 * @brief Helper function for implementing BUS_RELEASE_RESOURCE().
 *
 * This simple implementation of BUS_RELEASE_RESOURCE() simply calls the
 * BUS_RELEASE_RESOURCE() method of the parent of @p dev.
 */
int
bus_generic_release_resource(device_t dev, device_t child, struct resource *r)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_RELEASE_RESOURCE(dev->parent, child, r));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_ACTIVATE_RESOURCE().
 *
 * This simple implementation of BUS_ACTIVATE_RESOURCE() simply calls the
 * BUS_ACTIVATE_RESOURCE() method of the parent of @p dev.
 */
int
bus_generic_activate_resource(device_t dev, device_t child, struct resource *r)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_ACTIVATE_RESOURCE(dev->parent, child, r));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_DEACTIVATE_RESOURCE().
 *
 * This simple implementation of BUS_DEACTIVATE_RESOURCE() simply calls the
 * BUS_DEACTIVATE_RESOURCE() method of the parent of @p dev.
 */
int
bus_generic_deactivate_resource(device_t dev, device_t child,
    struct resource *r)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_DEACTIVATE_RESOURCE(dev->parent, child, r));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_MAP_RESOURCE().
 *
 * This simple implementation of BUS_MAP_RESOURCE() simply calls the
 * BUS_MAP_RESOURCE() method of the parent of @p dev.
 */
int
bus_generic_map_resource(device_t dev, device_t child, struct resource *r,
    struct resource_map_request *args, struct resource_map *map)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_MAP_RESOURCE(dev->parent, child, r, args, map));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_UNMAP_RESOURCE().
 *
 * This simple implementation of BUS_UNMAP_RESOURCE() simply calls the
 * BUS_UNMAP_RESOURCE() method of the parent of @p dev.
 */
int
bus_generic_unmap_resource(device_t dev, device_t child, struct resource *r,
    struct resource_map *map)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_UNMAP_RESOURCE(dev->parent, child, r, map));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_BIND_INTR().
 *
 * This simple implementation of BUS_BIND_INTR() simply calls the
 * BUS_BIND_INTR() method of the parent of @p dev.
 */
int
bus_generic_bind_intr(device_t dev, device_t child, struct resource *irq,
    int cpu)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_BIND_INTR(dev->parent, child, irq, cpu));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_CONFIG_INTR().
 *
 * This simple implementation of BUS_CONFIG_INTR() simply calls the
 * BUS_CONFIG_INTR() method of the parent of @p dev.
 */
int
bus_generic_config_intr(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_CONFIG_INTR(dev->parent, irq, trig, pol));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_DESCRIBE_INTR().
 *
 * This simple implementation of BUS_DESCRIBE_INTR() simply calls the
 * BUS_DESCRIBE_INTR() method of the parent of @p dev.
 */
int
bus_generic_describe_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie, const char *descr)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_DESCRIBE_INTR(dev->parent, child, irq, cookie,
		    descr));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_GET_CPUS().
 *
 * This simple implementation of BUS_GET_CPUS() simply calls the
 * BUS_GET_CPUS() method of the parent of @p dev.
 */
int
bus_generic_get_cpus(device_t dev, device_t child, enum cpu_sets op,
    size_t setsize, cpuset_t *cpuset)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent != NULL)
		return (BUS_GET_CPUS(dev->parent, child, op, setsize, cpuset));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_GET_DMA_TAG().
 *
 * This simple implementation of BUS_GET_DMA_TAG() simply calls the
 * BUS_GET_DMA_TAG() method of the parent of @p dev.
 */
bus_dma_tag_t
bus_generic_get_dma_tag(device_t dev, device_t child)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent != NULL)
		return (BUS_GET_DMA_TAG(dev->parent, child));
	return (NULL);
}

/**
 * @brief Helper function for implementing BUS_GET_BUS_TAG().
 *
 * This simple implementation of BUS_GET_BUS_TAG() simply calls the
 * BUS_GET_BUS_TAG() method of the parent of @p dev.
 */
bus_space_tag_t
bus_generic_get_bus_tag(device_t dev, device_t child)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent != NULL)
		return (BUS_GET_BUS_TAG(dev->parent, child));
	return ((bus_space_tag_t)0);
}

/**
 * @brief Helper function for implementing BUS_GET_RESOURCE().
 *
 * This implementation of BUS_GET_RESOURCE() uses the
 * resource_list_find() function to do most of the work. It calls
 * BUS_GET_RESOURCE_LIST() to find a suitable resource list to
 * search.
 */
int
bus_generic_rl_get_resource(device_t dev, device_t child, int type, int rid,
    rman_res_t *startp, rman_res_t *countp)
{
	struct resource_list *		rl = NULL;
	struct resource_list_entry *	rle = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return (EINVAL);

	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return (ENOENT);

	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;

	return (0);
}

/**
 * @brief Helper function for implementing BUS_SET_RESOURCE().
 *
 * This implementation of BUS_SET_RESOURCE() uses the
 * resource_list_add() function to do most of the work. It calls
 * BUS_GET_RESOURCE_LIST() to find a suitable resource list to
 * edit.
 */
int
bus_generic_rl_set_resource(device_t dev, device_t child, int type, int rid,
    rman_res_t start, rman_res_t count)
{
	struct resource_list *		rl = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return (EINVAL);

	resource_list_add(rl, type, rid, start, (start + count - 1), count);

	return (0);
}

/**
 * @brief Helper function for implementing BUS_DELETE_RESOURCE().
 *
 * This implementation of BUS_DELETE_RESOURCE() uses the
 * resource_list_delete() function to do most of the work. It calls
 * BUS_GET_RESOURCE_LIST() to find a suitable resource list to
 * edit.
 */
void
bus_generic_rl_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct resource_list *		rl = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return;

	resource_list_delete(rl, type, rid);

	return;
}

/**
 * @brief Helper function for implementing BUS_RELEASE_RESOURCE().
 *
 * This implementation of BUS_RELEASE_RESOURCE() uses the
 * resource_list_release() function to do most of the work. It calls
 * BUS_GET_RESOURCE_LIST() to find a suitable resource list.
 */
int
bus_generic_rl_release_resource(device_t dev, device_t child,
    struct resource *r)
{
	struct resource_list *		rl = NULL;

	if (device_get_parent(child) != dev)
		return (BUS_RELEASE_RESOURCE(device_get_parent(dev), child, r));

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return (EINVAL);

	return (resource_list_release(rl, dev, child, r));
}

/**
 * @brief Helper function for implementing BUS_ALLOC_RESOURCE().
 *
 * This implementation of BUS_ALLOC_RESOURCE() uses the
 * resource_list_alloc() function to do most of the work. It calls
 * BUS_GET_RESOURCE_LIST() to find a suitable resource list.
 */
struct resource *
bus_generic_rl_alloc_resource(device_t dev, device_t child, int type,
    int *rid, rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource_list *		rl = NULL;

	if (device_get_parent(child) != dev)
		return (BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
		    type, rid, start, end, count, flags));

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return (NULL);

	return (resource_list_alloc(rl, dev, child, type, rid,
	    start, end, count, flags));
}

/**
 * @brief Helper function for implementing BUS_ALLOC_RESOURCE().
 *
 * This implementation of BUS_ALLOC_RESOURCE() allocates a
 * resource from a resource manager.  It uses BUS_GET_RMAN()
 * to obtain the resource manager.
 */
struct resource *
bus_generic_rman_alloc_resource(device_t dev, device_t child, int type,
    int *rid, rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource *r;
	struct rman *rm;

	rm = BUS_GET_RMAN(dev, type, flags);
	if (rm == NULL)
		return (NULL);

	r = rman_reserve_resource(rm, start, end, count, flags & ~RF_ACTIVE,
	    child);
	if (r == NULL)
		return (NULL);
	rman_set_rid(r, *rid);
	rman_set_type(r, type);

	if (flags & RF_ACTIVE) {
		if (bus_activate_resource(child, type, *rid, r) != 0) {
			rman_release_resource(r);
			return (NULL);
		}
	}

	return (r);
}

/**
 * @brief Helper function for implementing BUS_ADJUST_RESOURCE().
 *
 * This implementation of BUS_ADJUST_RESOURCE() adjusts resources only
 * if they were allocated from the resource manager returned by
 * BUS_GET_RMAN().
 */
int
bus_generic_rman_adjust_resource(device_t dev, device_t child,
    struct resource *r, rman_res_t start, rman_res_t end)
{
	struct rman *rm;

	rm = BUS_GET_RMAN(dev, rman_get_type(r), rman_get_flags(r));
	if (rm == NULL)
		return (ENXIO);
	if (!rman_is_region_manager(r, rm))
		return (EINVAL);
	return (rman_adjust_resource(r, start, end));
}

/**
 * @brief Helper function for implementing BUS_RELEASE_RESOURCE().
 *
 * This implementation of BUS_RELEASE_RESOURCE() releases resources
 * allocated by bus_generic_rman_alloc_resource.
 */
int
bus_generic_rman_release_resource(device_t dev, device_t child,
    struct resource *r)
{
#ifdef INVARIANTS
	struct rman *rm;
#endif
	int error;

#ifdef INVARIANTS
	rm = BUS_GET_RMAN(dev, rman_get_type(r), rman_get_flags(r));
	KASSERT(rman_is_region_manager(r, rm),
	    ("%s: rman %p doesn't match for resource %p", __func__, rm, r));
#endif

	if (rman_get_flags(r) & RF_ACTIVE) {
		error = bus_deactivate_resource(child, r);
		if (error != 0)
			return (error);
	}
	return (rman_release_resource(r));
}

/**
 * @brief Helper function for implementing BUS_ACTIVATE_RESOURCE().
 *
 * This implementation of BUS_ACTIVATE_RESOURCE() activates resources
 * allocated by bus_generic_rman_alloc_resource.
 */
int
bus_generic_rman_activate_resource(device_t dev, device_t child,
    struct resource *r)
{
	struct resource_map map;
#ifdef INVARIANTS
	struct rman *rm;
#endif
	int error, type;

	type = rman_get_type(r);
#ifdef INVARIANTS
	rm = BUS_GET_RMAN(dev, type, rman_get_flags(r));
	KASSERT(rman_is_region_manager(r, rm),
	    ("%s: rman %p doesn't match for resource %p", __func__, rm, r));
#endif

	error = rman_activate_resource(r);
	if (error != 0)
		return (error);

	switch (type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		if ((rman_get_flags(r) & RF_UNMAPPED) == 0) {
			error = BUS_MAP_RESOURCE(dev, child, r, NULL, &map);
			if (error != 0)
				break;

			rman_set_mapping(r, &map);
		}
		break;
#ifdef INTRNG
	case SYS_RES_IRQ:
		error = intr_activate_irq(child, r);
		break;
#endif
	}
	if (error != 0)
		rman_deactivate_resource(r);
	return (error);
}

/**
 * @brief Helper function for implementing BUS_DEACTIVATE_RESOURCE().
 *
 * This implementation of BUS_DEACTIVATE_RESOURCE() deactivates
 * resources allocated by bus_generic_rman_alloc_resource.
 */
int
bus_generic_rman_deactivate_resource(device_t dev, device_t child,
    struct resource *r)
{
	struct resource_map map;
#ifdef INVARIANTS
	struct rman *rm;
#endif
	int error, type;

	type = rman_get_type(r);
#ifdef INVARIANTS
	rm = BUS_GET_RMAN(dev, type, rman_get_flags(r));
	KASSERT(rman_is_region_manager(r, rm),
	    ("%s: rman %p doesn't match for resource %p", __func__, rm, r));
#endif

	error = rman_deactivate_resource(r);
	if (error != 0)
		return (error);

	switch (type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		if ((rman_get_flags(r) & RF_UNMAPPED) == 0) {
			rman_get_mapping(r, &map);
			BUS_UNMAP_RESOURCE(dev, child, r, &map);
		}
		break;
#ifdef INTRNG
	case SYS_RES_IRQ:
		intr_deactivate_irq(child, r);
		break;
#endif
	}
	return (0);
}

/**
 * @brief Helper function for implementing BUS_CHILD_PRESENT().
 *
 * This simple implementation of BUS_CHILD_PRESENT() simply calls the
 * BUS_CHILD_PRESENT() method of the parent of @p dev.
 */
int
bus_generic_child_present(device_t dev, device_t child)
{
	return (BUS_CHILD_PRESENT(device_get_parent(dev), dev));
}

/**
 * @brief Helper function for implementing BUS_GET_DOMAIN().
 *
 * This simple implementation of BUS_GET_DOMAIN() calls the
 * BUS_GET_DOMAIN() method of the parent of @p dev.  If @p dev
 * does not have a parent, the function fails with ENOENT.
 */
int
bus_generic_get_domain(device_t dev, device_t child, int *domain)
{
	if (dev->parent)
		return (BUS_GET_DOMAIN(dev->parent, dev, domain));

	return (ENOENT);
}

/**
 * @brief Helper function to implement normal BUS_GET_DEVICE_PATH()
 *
 * This function knows how to (a) pass the request up the tree if there's
 * a parent and (b) Knows how to supply a FreeBSD locator.
 *
 * @param bus		bus in the walk up the tree
 * @param child		leaf node to print information about
 * @param locator	BUS_LOCATOR_xxx string for locator
 * @param sb		Buffer to print information into
 */
int
bus_generic_get_device_path(device_t bus, device_t child, const char *locator,
    struct sbuf *sb)
{
	int rv = 0;
	device_t parent;

	/*
	 * We don't recurse on ACPI since either we know the handle for the
	 * device or we don't. And if we're in the generic routine, we don't
	 * have a ACPI override. All other locators build up a path by having
	 * their parents create a path and then adding the path element for this
	 * node. That's why we recurse with parent, bus rather than the typical
	 * parent, child: each spot in the tree is independent of what our child
	 * will do with this path.
	 */
	parent = device_get_parent(bus);
	if (parent != NULL && strcmp(locator, BUS_LOCATOR_ACPI) != 0) {
		rv = BUS_GET_DEVICE_PATH(parent, bus, locator, sb);
	}
	if (strcmp(locator, BUS_LOCATOR_FREEBSD) == 0) {
		if (rv == 0) {
			sbuf_printf(sb, "/%s", device_get_nameunit(child));
		}
		return (rv);
	}
	/*
	 * Don't know what to do. So assume we do nothing. Not sure that's
	 * the right thing, but keeps us from having a big list here.
	 */
	return (0);
}


/**
 * @brief Helper function for implementing BUS_RESCAN().
 *
 * This null implementation of BUS_RESCAN() always fails to indicate
 * the bus does not support rescanning.
 */
int
bus_null_rescan(device_t dev)
{
	return (ENODEV);
}

/*
 * Some convenience functions to make it easier for drivers to use the
 * resource-management functions.  All these really do is hide the
 * indirection through the parent's method table, making for slightly
 * less-wordy code.  In the future, it might make sense for this code
 * to maintain some sort of a list of resources allocated by each device.
 */

int
bus_alloc_resources(device_t dev, struct resource_spec *rs,
    struct resource **res)
{
	int i;

	for (i = 0; rs[i].type != -1; i++)
		res[i] = NULL;
	for (i = 0; rs[i].type != -1; i++) {
		res[i] = bus_alloc_resource_any(dev,
		    rs[i].type, &rs[i].rid, rs[i].flags);
		if (res[i] == NULL && !(rs[i].flags & RF_OPTIONAL)) {
			bus_release_resources(dev, rs, res);
			return (ENXIO);
		}
	}
	return (0);
}

void
bus_release_resources(device_t dev, const struct resource_spec *rs,
    struct resource **res)
{
	int i;

	for (i = 0; rs[i].type != -1; i++)
		if (res[i] != NULL) {
			bus_release_resource(
			    dev, rs[i].type, rs[i].rid, res[i]);
			res[i] = NULL;
		}
}

/**
 * @brief Wrapper function for BUS_ALLOC_RESOURCE().
 *
 * This function simply calls the BUS_ALLOC_RESOURCE() method of the
 * parent of @p dev.
 */
struct resource *
bus_alloc_resource(device_t dev, int type, int *rid, rman_res_t start,
    rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource *res;

	if (dev->parent == NULL)
		return (NULL);
	res = BUS_ALLOC_RESOURCE(dev->parent, dev, type, rid, start, end,
	    count, flags);
	return (res);
}

/**
 * @brief Wrapper function for BUS_ADJUST_RESOURCE().
 *
 * This function simply calls the BUS_ADJUST_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_adjust_resource(device_t dev, struct resource *r, rman_res_t start,
    rman_res_t end)
{
	if (dev->parent == NULL)
		return (EINVAL);
	return (BUS_ADJUST_RESOURCE(dev->parent, dev, r, start, end));
}

int
bus_adjust_resource_old(device_t dev, int type __unused, struct resource *r,
    rman_res_t start, rman_res_t end)
{
	return (bus_adjust_resource(dev, r, start, end));
}

/**
 * @brief Wrapper function for BUS_TRANSLATE_RESOURCE().
 *
 * This function simply calls the BUS_TRANSLATE_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_translate_resource(device_t dev, int type, rman_res_t start,
    rman_res_t *newstart)
{
	if (dev->parent == NULL)
		return (EINVAL);
	return (BUS_TRANSLATE_RESOURCE(dev->parent, type, start, newstart));
}

/**
 * @brief Wrapper function for BUS_ACTIVATE_RESOURCE().
 *
 * This function simply calls the BUS_ACTIVATE_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_activate_resource(device_t dev, struct resource *r)
{
	if (dev->parent == NULL)
		return (EINVAL);
	return (BUS_ACTIVATE_RESOURCE(dev->parent, dev, r));
}

int
bus_activate_resource_old(device_t dev, int type, int rid, struct resource *r)
{
	return (bus_activate_resource(dev, r));
}

/**
 * @brief Wrapper function for BUS_DEACTIVATE_RESOURCE().
 *
 * This function simply calls the BUS_DEACTIVATE_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_deactivate_resource(device_t dev, struct resource *r)
{
	if (dev->parent == NULL)
		return (EINVAL);
	return (BUS_DEACTIVATE_RESOURCE(dev->parent, dev, r));
}

int
bus_deactivate_resource_old(device_t dev, int type, int rid, struct resource *r)
{
	return (bus_deactivate_resource(dev, r));
}

/**
 * @brief Wrapper function for BUS_MAP_RESOURCE().
 *
 * This function simply calls the BUS_MAP_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_map_resource(device_t dev, struct resource *r,
    struct resource_map_request *args, struct resource_map *map)
{
	if (dev->parent == NULL)
		return (EINVAL);
	return (BUS_MAP_RESOURCE(dev->parent, dev, r, args, map));
}

int
bus_map_resource_old(device_t dev, int type, struct resource *r,
    struct resource_map_request *args, struct resource_map *map)
{
	return (bus_map_resource(dev, r, args, map));
}

/**
 * @brief Wrapper function for BUS_UNMAP_RESOURCE().
 *
 * This function simply calls the BUS_UNMAP_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_unmap_resource(device_t dev, struct resource *r, struct resource_map *map)
{
	if (dev->parent == NULL)
		return (EINVAL);
	return (BUS_UNMAP_RESOURCE(dev->parent, dev, r, map));
}

int
bus_unmap_resource_old(device_t dev, int type, struct resource *r,
    struct resource_map *map)
{
	return (bus_unmap_resource(dev, r, map));
}

/**
 * @brief Wrapper function for BUS_RELEASE_RESOURCE().
 *
 * This function simply calls the BUS_RELEASE_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_release_resource(device_t dev, struct resource *r)
{
	int rv;

	if (dev->parent == NULL)
		return (EINVAL);
	rv = BUS_RELEASE_RESOURCE(dev->parent, dev, r);
	return (rv);
}

int
bus_release_resource_old(device_t dev, int type, int rid, struct resource *r)
{
	return (bus_release_resource(dev, r));
}

/**
 * @brief Wrapper function for BUS_SETUP_INTR().
 *
 * This function simply calls the BUS_SETUP_INTR() method of the
 * parent of @p dev.
 */
int
bus_setup_intr(device_t dev, struct resource *r, int flags,
    driver_filter_t filter, driver_intr_t handler, void *arg, void **cookiep)
{
	int error;

	if (dev->parent == NULL)
		return (EINVAL);
	error = BUS_SETUP_INTR(dev->parent, dev, r, flags, filter, handler,
	    arg, cookiep);
	if (error != 0)
		return (error);
	if (handler != NULL && !(flags & INTR_MPSAFE))
		device_printf(dev, "[GIANT-LOCKED]\n");
	return (0);
}

/**
 * @brief Wrapper function for BUS_TEARDOWN_INTR().
 *
 * This function simply calls the BUS_TEARDOWN_INTR() method of the
 * parent of @p dev.
 */
int
bus_teardown_intr(device_t dev, struct resource *r, void *cookie)
{
	if (dev->parent == NULL)
		return (EINVAL);
	return (BUS_TEARDOWN_INTR(dev->parent, dev, r, cookie));
}

/**
 * @brief Wrapper function for BUS_SUSPEND_INTR().
 *
 * This function simply calls the BUS_SUSPEND_INTR() method of the
 * parent of @p dev.
 */
int
bus_suspend_intr(device_t dev, struct resource *r)
{
	if (dev->parent == NULL)
		return (EINVAL);
	return (BUS_SUSPEND_INTR(dev->parent, dev, r));
}

/**
 * @brief Wrapper function for BUS_RESUME_INTR().
 *
 * This function simply calls the BUS_RESUME_INTR() method of the
 * parent of @p dev.
 */
int
bus_resume_intr(device_t dev, struct resource *r)
{
	if (dev->parent == NULL)
		return (EINVAL);
	return (BUS_RESUME_INTR(dev->parent, dev, r));
}

/**
 * @brief Wrapper function for BUS_BIND_INTR().
 *
 * This function simply calls the BUS_BIND_INTR() method of the
 * parent of @p dev.
 */
int
bus_bind_intr(device_t dev, struct resource *r, int cpu)
{
	if (dev->parent == NULL)
		return (EINVAL);
	return (BUS_BIND_INTR(dev->parent, dev, r, cpu));
}

/**
 * @brief Wrapper function for BUS_DESCRIBE_INTR().
 *
 * This function first formats the requested description into a
 * temporary buffer and then calls the BUS_DESCRIBE_INTR() method of
 * the parent of @p dev.
 */
int
bus_describe_intr(device_t dev, struct resource *irq, void *cookie,
    const char *fmt, ...)
{
	va_list ap;
	char descr[MAXCOMLEN + 1];

	if (dev->parent == NULL)
		return (EINVAL);
	va_start(ap, fmt);
	vsnprintf(descr, sizeof(descr), fmt, ap);
	va_end(ap);
	return (BUS_DESCRIBE_INTR(dev->parent, dev, irq, cookie, descr));
}

/**
 * @brief Wrapper function for BUS_SET_RESOURCE().
 *
 * This function simply calls the BUS_SET_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_set_resource(device_t dev, int type, int rid,
    rman_res_t start, rman_res_t count)
{
	return (BUS_SET_RESOURCE(device_get_parent(dev), dev, type, rid,
	    start, count));
}

/**
 * @brief Wrapper function for BUS_GET_RESOURCE().
 *
 * This function simply calls the BUS_GET_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_get_resource(device_t dev, int type, int rid,
    rman_res_t *startp, rman_res_t *countp)
{
	return (BUS_GET_RESOURCE(device_get_parent(dev), dev, type, rid,
	    startp, countp));
}

/**
 * @brief Wrapper function for BUS_GET_RESOURCE().
 *
 * This function simply calls the BUS_GET_RESOURCE() method of the
 * parent of @p dev and returns the start value.
 */
rman_res_t
bus_get_resource_start(device_t dev, int type, int rid)
{
	rman_res_t start;
	rman_res_t count;
	int error;

	error = BUS_GET_RESOURCE(device_get_parent(dev), dev, type, rid,
	    &start, &count);
	if (error)
		return (0);
	return (start);
}

/**
 * @brief Wrapper function for BUS_GET_RESOURCE().
 *
 * This function simply calls the BUS_GET_RESOURCE() method of the
 * parent of @p dev and returns the count value.
 */
rman_res_t
bus_get_resource_count(device_t dev, int type, int rid)
{
	rman_res_t start;
	rman_res_t count;
	int error;

	error = BUS_GET_RESOURCE(device_get_parent(dev), dev, type, rid,
	    &start, &count);
	if (error)
		return (0);
	return (count);
}

/**
 * @brief Wrapper function for BUS_DELETE_RESOURCE().
 *
 * This function simply calls the BUS_DELETE_RESOURCE() method of the
 * parent of @p dev.
 */
void
bus_delete_resource(device_t dev, int type, int rid)
{
	BUS_DELETE_RESOURCE(device_get_parent(dev), dev, type, rid);
}

/**
 * @brief Wrapper function for BUS_CHILD_PRESENT().
 *
 * This function simply calls the BUS_CHILD_PRESENT() method of the
 * parent of @p dev.
 */
int
bus_child_present(device_t child)
{
	return (BUS_CHILD_PRESENT(device_get_parent(child), child));
}

/**
 * @brief Wrapper function for BUS_CHILD_PNPINFO().
 *
 * This function simply calls the BUS_CHILD_PNPINFO() method of the parent of @p
 * dev.
 */
int
bus_child_pnpinfo(device_t child, struct sbuf *sb)
{
	device_t parent;

	parent = device_get_parent(child);
	if (parent == NULL)
		return (0);
	return (BUS_CHILD_PNPINFO(parent, child, sb));
}

/**
 * @brief Generic implementation that does nothing for bus_child_pnpinfo
 *
 * This function has the right signature and returns 0 since the sbuf is passed
 * to us to append to.
 */
int
bus_generic_child_pnpinfo(device_t dev, device_t child, struct sbuf *sb)
{
	return (0);
}

/**
 * @brief Wrapper function for BUS_CHILD_LOCATION().
 *
 * This function simply calls the BUS_CHILD_LOCATION() method of the parent of
 * @p dev.
 */
int
bus_child_location(device_t child, struct sbuf *sb)
{
	device_t parent;

	parent = device_get_parent(child);
	if (parent == NULL)
		return (0);
	return (BUS_CHILD_LOCATION(parent, child, sb));
}

/**
 * @brief Generic implementation that does nothing for bus_child_location
 *
 * This function has the right signature and returns 0 since the sbuf is passed
 * to us to append to.
 */
int
bus_generic_child_location(device_t dev, device_t child, struct sbuf *sb)
{
	return (0);
}

/**
 * @brief Wrapper function for BUS_GET_CPUS().
 *
 * This function simply calls the BUS_GET_CPUS() method of the
 * parent of @p dev.
 */
int
bus_get_cpus(device_t dev, enum cpu_sets op, size_t setsize, cpuset_t *cpuset)
{
	device_t parent;

	parent = device_get_parent(dev);
	if (parent == NULL)
		return (EINVAL);
	return (BUS_GET_CPUS(parent, dev, op, setsize, cpuset));
}

/**
 * @brief Wrapper function for BUS_GET_DMA_TAG().
 *
 * This function simply calls the BUS_GET_DMA_TAG() method of the
 * parent of @p dev.
 */
bus_dma_tag_t
bus_get_dma_tag(device_t dev)
{
	device_t parent;

	parent = device_get_parent(dev);
	if (parent == NULL)
		return (NULL);
	return (BUS_GET_DMA_TAG(parent, dev));
}

/**
 * @brief Wrapper function for BUS_GET_BUS_TAG().
 *
 * This function simply calls the BUS_GET_BUS_TAG() method of the
 * parent of @p dev.
 */
bus_space_tag_t
bus_get_bus_tag(device_t dev)
{
	device_t parent;

	parent = device_get_parent(dev);
	if (parent == NULL)
		return ((bus_space_tag_t)0);
	return (BUS_GET_BUS_TAG(parent, dev));
}

/**
 * @brief Wrapper function for BUS_GET_DOMAIN().
 *
 * This function simply calls the BUS_GET_DOMAIN() method of the
 * parent of @p dev.
 */
int
bus_get_domain(device_t dev, int *domain)
{
	return (BUS_GET_DOMAIN(device_get_parent(dev), dev, domain));
}

/* Resume all devices and then notify userland that we're up again. */
static int
root_resume(device_t dev)
{
	int error;

	error = bus_generic_resume(dev);
	if (error == 0) {
		devctl_notify("kernel", "power", "resume", NULL);
	}
	return (error);
}

static int
root_print_child(device_t dev, device_t child)
{
	int	retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += printf("\n");

	return (retval);
}

static int
root_setup_intr(device_t dev, device_t child, struct resource *irq, int flags,
    driver_filter_t *filter, driver_intr_t *intr, void *arg, void **cookiep)
{
	/*
	 * If an interrupt mapping gets to here something bad has happened.
	 */
	panic("root_setup_intr");
}

/*
 * If we get here, assume that the device is permanent and really is
 * present in the system.  Removable bus drivers are expected to intercept
 * this call long before it gets here.  We return -1 so that drivers that
 * really care can check vs -1 or some ERRNO returned higher in the food
 * chain.
 */
static int
root_child_present(device_t dev, device_t child)
{
	return (-1);
}

static int
root_get_cpus(device_t dev, device_t child, enum cpu_sets op, size_t setsize,
    cpuset_t *cpuset)
{
	switch (op) {
	case INTR_CPUS:
		/* Default to returning the set of all CPUs. */
		if (setsize != sizeof(cpuset_t))
			return (EINVAL);
		*cpuset = all_cpus;
		return (0);
	default:
		return (EINVAL);
	}
}

static kobj_method_t root_methods[] = {
	/* Device interface */
	KOBJMETHOD(device_shutdown,	bus_generic_shutdown),
	KOBJMETHOD(device_suspend,	bus_generic_suspend),
	KOBJMETHOD(device_resume,	root_resume),

	/* Bus interface */
	KOBJMETHOD(bus_print_child,	root_print_child),
	KOBJMETHOD(bus_read_ivar,	bus_generic_read_ivar),
	KOBJMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	KOBJMETHOD(bus_setup_intr,	root_setup_intr),
	KOBJMETHOD(bus_child_present,	root_child_present),
	KOBJMETHOD(bus_get_cpus,	root_get_cpus),

	KOBJMETHOD_END
};

static driver_t root_driver = {
	"root",
	root_methods,
	1,			/* no softc */
};

device_t	root_bus;
devclass_t	root_devclass;

static int
root_bus_module_handler(module_t mod, int what, void* arg)
{
	switch (what) {
	case MOD_LOAD:
		TAILQ_INIT(&bus_data_devices);
		kobj_class_compile((kobj_class_t) &root_driver);
		root_bus = make_device(NULL, "root", 0);
		root_bus->desc = "System root bus";
		kobj_init((kobj_t) root_bus, (kobj_class_t) &root_driver);
		root_bus->driver = &root_driver;
		root_bus->state = DS_ATTACHED;
		root_devclass = devclass_find_internal("root", NULL, FALSE);
		devctl2_init();
		return (0);

	case MOD_SHUTDOWN:
		device_shutdown(root_bus);
		return (0);
	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

static moduledata_t root_bus_mod = {
	"rootbus",
	root_bus_module_handler,
	NULL
};
DECLARE_MODULE(rootbus, root_bus_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);

/**
 * @brief Automatically configure devices
 *
 * This function begins the autoconfiguration process by calling
 * device_probe_and_attach() for each child of the @c root0 device.
 */
void
root_bus_configure(void)
{
	PDEBUG(("."));

	/* Eventually this will be split up, but this is sufficient for now. */
	bus_set_pass(BUS_PASS_DEFAULT);
}

/**
 * @brief Module handler for registering device drivers
 *
 * This module handler is used to automatically register device
 * drivers when modules are loaded. If @p what is MOD_LOAD, it calls
 * devclass_add_driver() for the driver described by the
 * driver_module_data structure pointed to by @p arg
 */
int
driver_module_handler(module_t mod, int what, void *arg)
{
	struct driver_module_data *dmd;
	devclass_t bus_devclass;
	kobj_class_t driver;
	int error, pass;

	dmd = (struct driver_module_data *)arg;
	bus_devclass = devclass_find_internal(dmd->dmd_busname, NULL, TRUE);
	error = 0;

	switch (what) {
	case MOD_LOAD:
		if (dmd->dmd_chainevh)
			error = dmd->dmd_chainevh(mod,what,dmd->dmd_chainarg);

		pass = dmd->dmd_pass;
		driver = dmd->dmd_driver;
		PDEBUG(("Loading module: driver %s on bus %s (pass %d)",
		    DRIVERNAME(driver), dmd->dmd_busname, pass));
		error = devclass_add_driver(bus_devclass, driver, pass,
		    dmd->dmd_devclass);
		break;

	case MOD_UNLOAD:
		PDEBUG(("Unloading module: driver %s from bus %s",
		    DRIVERNAME(dmd->dmd_driver),
		    dmd->dmd_busname));
		error = devclass_delete_driver(bus_devclass,
		    dmd->dmd_driver);

		if (!error && dmd->dmd_chainevh)
			error = dmd->dmd_chainevh(mod,what,dmd->dmd_chainarg);
		break;
	case MOD_QUIESCE:
		PDEBUG(("Quiesce module: driver %s from bus %s",
		    DRIVERNAME(dmd->dmd_driver),
		    dmd->dmd_busname));
		error = devclass_quiesce_driver(bus_devclass,
		    dmd->dmd_driver);

		if (!error && dmd->dmd_chainevh)
			error = dmd->dmd_chainevh(mod,what,dmd->dmd_chainarg);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

/**
 * @brief Enumerate all hinted devices for this bus.
 *
 * Walks through the hints for this bus and calls the bus_hinted_child
 * routine for each one it fines.  It searches first for the specific
 * bus that's being probed for hinted children (eg isa0), and then for
 * generic children (eg isa).
 *
 * @param	dev	bus device to enumerate
 */
void
bus_enumerate_hinted_children(device_t bus)
{
	int i;
	const char *dname, *busname;
	int dunit;

	/*
	 * enumerate all devices on the specific bus
	 */
	busname = device_get_nameunit(bus);
	i = 0;
	while (resource_find_match(&i, &dname, &dunit, "at", busname) == 0)
		BUS_HINTED_CHILD(bus, dname, dunit);

	/*
	 * and all the generic ones.
	 */
	busname = device_get_name(bus);
	i = 0;
	while (resource_find_match(&i, &dname, &dunit, "at", busname) == 0)
		BUS_HINTED_CHILD(bus, dname, dunit);
}

#ifdef BUS_DEBUG

/* the _short versions avoid iteration by not calling anything that prints
 * more than oneliners. I love oneliners.
 */

static void
print_device_short(device_t dev, int indent)
{
	if (!dev)
		return;

	indentprintf(("device %d: <%s> %sparent,%schildren,%s%s%s%s%s,%sivars,%ssoftc,busy=%d\n",
	    dev->unit, dev->desc,
	    (dev->parent? "":"no "),
	    (TAILQ_EMPTY(&dev->children)? "no ":""),
	    (dev->flags&DF_ENABLED? "enabled,":"disabled,"),
	    (dev->flags&DF_FIXEDCLASS? "fixed,":""),
	    (dev->flags&DF_WILDCARD? "wildcard,":""),
	    (dev->flags&DF_DESCMALLOCED? "descmalloced,":""),
	    (dev->flags&DF_SUSPENDED? "suspended,":""),
	    (dev->ivars? "":"no "),
	    (dev->softc? "":"no "),
	    dev->busy));
}

static void
print_device(device_t dev, int indent)
{
	if (!dev)
		return;

	print_device_short(dev, indent);

	indentprintf(("Parent:\n"));
	print_device_short(dev->parent, indent+1);
	indentprintf(("Driver:\n"));
	print_driver_short(dev->driver, indent+1);
	indentprintf(("Devclass:\n"));
	print_devclass_short(dev->devclass, indent+1);
}

void
print_device_tree_short(device_t dev, int indent)
/* print the device and all its children (indented) */
{
	device_t child;

	if (!dev)
		return;

	print_device_short(dev, indent);

	TAILQ_FOREACH(child, &dev->children, link) {
		print_device_tree_short(child, indent+1);
	}
}

void
print_device_tree(device_t dev, int indent)
/* print the device and all its children (indented) */
{
	device_t child;

	if (!dev)
		return;

	print_device(dev, indent);

	TAILQ_FOREACH(child, &dev->children, link) {
		print_device_tree(child, indent+1);
	}
}

static void
print_driver_short(driver_t *driver, int indent)
{
	if (!driver)
		return;

	indentprintf(("driver %s: softc size = %zd\n",
	    driver->name, driver->size));
}

static void
print_driver(driver_t *driver, int indent)
{
	if (!driver)
		return;

	print_driver_short(driver, indent);
}

static void
print_driver_list(driver_list_t drivers, int indent)
{
	driverlink_t driver;

	TAILQ_FOREACH(driver, &drivers, link) {
		print_driver(driver->driver, indent);
	}
}

static void
print_devclass_short(devclass_t dc, int indent)
{
	if ( !dc )
		return;

	indentprintf(("devclass %s: max units = %d\n", dc->name, dc->maxunit));
}

static void
print_devclass(devclass_t dc, int indent)
{
	int i;

	if ( !dc )
		return;

	print_devclass_short(dc, indent);
	indentprintf(("Drivers:\n"));
	print_driver_list(dc->drivers, indent+1);

	indentprintf(("Devices:\n"));
	for (i = 0; i < dc->maxunit; i++)
		if (dc->devices[i])
			print_device(dc->devices[i], indent+1);
}

void
print_devclass_list_short(void)
{
	devclass_t dc;

	printf("Short listing of devclasses, drivers & devices:\n");
	TAILQ_FOREACH(dc, &devclasses, link) {
		print_devclass_short(dc, 0);
	}
}

void
print_devclass_list(void)
{
	devclass_t dc;

	printf("Full listing of devclasses, drivers & devices:\n");
	TAILQ_FOREACH(dc, &devclasses, link) {
		print_devclass(dc, 0);
	}
}

#endif

/*
 * User-space access to the device tree.
 *
 * We implement a small set of nodes:
 *
 * hw.bus			Single integer read method to obtain the
 *				current generation count.
 * hw.bus.devices		Reads the entire device tree in flat space.
 * hw.bus.rman			Resource manager interface
 *
 * We might like to add the ability to scan devclasses and/or drivers to
 * determine what else is currently loaded/available.
 */

static int
sysctl_bus_info(SYSCTL_HANDLER_ARGS)
{
	struct u_businfo	ubus;

	ubus.ub_version = BUS_USER_VERSION;
	ubus.ub_generation = bus_data_generation;

	return (SYSCTL_OUT(req, &ubus, sizeof(ubus)));
}
SYSCTL_PROC(_hw_bus, OID_AUTO, info, CTLTYPE_STRUCT | CTLFLAG_RD |
    CTLFLAG_MPSAFE, NULL, 0, sysctl_bus_info, "S,u_businfo",
    "bus-related data");

static int
sysctl_devices(SYSCTL_HANDLER_ARGS)
{
	struct sbuf		sb;
	int			*name = (int *)arg1;
	u_int			namelen = arg2;
	int			index;
	device_t		dev;
	struct u_device		*udev;
	int			error;

	if (namelen != 2)
		return (EINVAL);

	if (bus_data_generation_check(name[0]))
		return (EINVAL);

	index = name[1];

	/*
	 * Scan the list of devices, looking for the requested index.
	 */
	TAILQ_FOREACH(dev, &bus_data_devices, devlink) {
		if (index-- == 0)
			break;
	}
	if (dev == NULL)
		return (ENOENT);

	/*
	 * Populate the return item, careful not to overflow the buffer.
	 */
	udev = malloc(sizeof(*udev), M_BUS, M_WAITOK | M_ZERO);
	udev->dv_handle = (uintptr_t)dev;
	udev->dv_parent = (uintptr_t)dev->parent;
	udev->dv_devflags = dev->devflags;
	udev->dv_flags = dev->flags;
	udev->dv_state = dev->state;
	sbuf_new(&sb, udev->dv_fields, sizeof(udev->dv_fields), SBUF_FIXEDLEN);
	if (dev->nameunit != NULL)
		sbuf_cat(&sb, dev->nameunit);
	sbuf_putc(&sb, '\0');
	if (dev->desc != NULL)
		sbuf_cat(&sb, dev->desc);
	sbuf_putc(&sb, '\0');
	if (dev->driver != NULL)
		sbuf_cat(&sb, dev->driver->name);
	sbuf_putc(&sb, '\0');
	bus_child_pnpinfo(dev, &sb);
	sbuf_putc(&sb, '\0');
	bus_child_location(dev, &sb);
	sbuf_putc(&sb, '\0');
	error = sbuf_finish(&sb);
	if (error == 0)
		error = SYSCTL_OUT(req, udev, sizeof(*udev));
	sbuf_delete(&sb);
	free(udev, M_BUS);
	return (error);
}

SYSCTL_NODE(_hw_bus, OID_AUTO, devices,
    CTLFLAG_RD | CTLFLAG_NEEDGIANT, sysctl_devices,
    "system device tree");

int
bus_data_generation_check(int generation)
{
	if (generation != bus_data_generation)
		return (1);

	/* XXX generate optimised lists here? */
	return (0);
}

void
bus_data_generation_update(void)
{
	atomic_add_int(&bus_data_generation, 1);
}

int
bus_free_resource(device_t dev, int type, struct resource *r)
{
	if (r == NULL)
		return (0);
	return (bus_release_resource(dev, type, rman_get_rid(r), r));
}

device_t
device_lookup_by_name(const char *name)
{
	device_t dev;

	TAILQ_FOREACH(dev, &bus_data_devices, devlink) {
		if (dev->nameunit != NULL && strcmp(dev->nameunit, name) == 0)
			return (dev);
	}
	return (NULL);
}

/*
 * /dev/devctl2 implementation.  The existing /dev/devctl device has
 * implicit semantics on open, so it could not be reused for this.
 * Another option would be to call this /dev/bus?
 */
static int
find_device(struct devreq *req, device_t *devp)
{
	device_t dev;

	/*
	 * First, ensure that the name is nul terminated.
	 */
	if (memchr(req->dr_name, '\0', sizeof(req->dr_name)) == NULL)
		return (EINVAL);

	/*
	 * Second, try to find an attached device whose name matches
	 * 'name'.
	 */
	dev = device_lookup_by_name(req->dr_name);
	if (dev != NULL) {
		*devp = dev;
		return (0);
	}

	/* Finally, give device enumerators a chance. */
	dev = NULL;
	EVENTHANDLER_DIRECT_INVOKE(dev_lookup, req->dr_name, &dev);
	if (dev == NULL)
		return (ENOENT);
	*devp = dev;
	return (0);
}

static bool
driver_exists(device_t bus, const char *driver)
{
	devclass_t dc;

	for (dc = bus->devclass; dc != NULL; dc = dc->parent) {
		if (devclass_find_driver_internal(dc, driver) != NULL)
			return (true);
	}
	return (false);
}

static void
device_gen_nomatch(device_t dev)
{
	device_t child;

	if (dev->flags & DF_NEEDNOMATCH &&
	    dev->state == DS_NOTPRESENT) {
		device_handle_nomatch(dev);
	}
	dev->flags &= ~DF_NEEDNOMATCH;
	TAILQ_FOREACH(child, &dev->children, link) {
		device_gen_nomatch(child);
	}
}

static void
device_do_deferred_actions(void)
{
	devclass_t dc;
	driverlink_t dl;

	/*
	 * Walk through the devclasses to find all the drivers we've tagged as
	 * deferred during the freeze and call the driver added routines. They
	 * have already been added to the lists in the background, so the driver
	 * added routines that trigger a probe will have all the right bidders
	 * for the probe auction.
	 */
	TAILQ_FOREACH(dc, &devclasses, link) {
		TAILQ_FOREACH(dl, &dc->drivers, link) {
			if (dl->flags & DL_DEFERRED_PROBE) {
				devclass_driver_added(dc, dl->driver);
				dl->flags &= ~DL_DEFERRED_PROBE;
			}
		}
	}

	/*
	 * We also defer no-match events during a freeze. Walk the tree and
	 * generate all the pent-up events that are still relevant.
	 */
	device_gen_nomatch(root_bus);
	bus_data_generation_update();
}

static int
device_get_path(device_t dev, const char *locator, struct sbuf *sb)
{
	device_t parent;
	int error;

	KASSERT(sb != NULL, ("sb is NULL"));
	parent = device_get_parent(dev);
	if (parent == NULL) {
		error = sbuf_putc(sb, '/');
	} else {
		error = BUS_GET_DEVICE_PATH(parent, dev, locator, sb);
		if (error == 0) {
			error = sbuf_error(sb);
			if (error == 0 && sbuf_len(sb) <= 1)
				error = EIO;
		}
	}
	sbuf_finish(sb);
	return (error);
}

static int
devctl2_ioctl(struct cdev *cdev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct devreq *req;
	device_t dev;
	int error, old;

	/* Locate the device to control. */
	bus_topo_lock();
	req = (struct devreq *)data;
	switch (cmd) {
	case DEV_ATTACH:
	case DEV_DETACH:
	case DEV_ENABLE:
	case DEV_DISABLE:
	case DEV_SUSPEND:
	case DEV_RESUME:
	case DEV_SET_DRIVER:
	case DEV_CLEAR_DRIVER:
	case DEV_RESCAN:
	case DEV_DELETE:
	case DEV_RESET:
		error = priv_check(td, PRIV_DRIVER);
		if (error == 0)
			error = find_device(req, &dev);
		break;
	case DEV_FREEZE:
	case DEV_THAW:
		error = priv_check(td, PRIV_DRIVER);
		break;
	case DEV_GET_PATH:
		error = find_device(req, &dev);
		break;
	default:
		error = ENOTTY;
		break;
	}
	if (error) {
		bus_topo_unlock();
		return (error);
	}

	/* Perform the requested operation. */
	switch (cmd) {
	case DEV_ATTACH:
		if (device_is_attached(dev))
			error = EBUSY;
		else if (!device_is_enabled(dev))
			error = ENXIO;
		else
			error = device_probe_and_attach(dev);
		break;
	case DEV_DETACH:
		if (!device_is_attached(dev)) {
			error = ENXIO;
			break;
		}
		if (!(req->dr_flags & DEVF_FORCE_DETACH)) {
			error = device_quiesce(dev);
			if (error)
				break;
		}
		error = device_detach(dev);
		break;
	case DEV_ENABLE:
		if (device_is_enabled(dev)) {
			error = EBUSY;
			break;
		}

		/*
		 * If the device has been probed but not attached (e.g.
		 * when it has been disabled by a loader hint), just
		 * attach the device rather than doing a full probe.
		 */
		device_enable(dev);
		if (dev->devclass != NULL) {
			/*
			 * If the device was disabled via a hint, clear
			 * the hint.
			 */
			if (resource_disabled(dev->devclass->name, dev->unit))
				resource_unset_value(dev->devclass->name,
				    dev->unit, "disabled");

			/* Allow any drivers to rebid. */
			if (!(dev->flags & DF_FIXEDCLASS))
				devclass_delete_device(dev->devclass, dev);
		}
		error = device_probe_and_attach(dev);
		break;
	case DEV_DISABLE:
		if (!device_is_enabled(dev)) {
			error = ENXIO;
			break;
		}

		if (!(req->dr_flags & DEVF_FORCE_DETACH)) {
			error = device_quiesce(dev);
			if (error)
				break;
		}

		/*
		 * Force DF_FIXEDCLASS on around detach to preserve
		 * the existing name.
		 */
		old = dev->flags;
		dev->flags |= DF_FIXEDCLASS;
		error = device_detach(dev);
		if (!(old & DF_FIXEDCLASS))
			dev->flags &= ~DF_FIXEDCLASS;
		if (error == 0)
			device_disable(dev);
		break;
	case DEV_SUSPEND:
		if (device_is_suspended(dev)) {
			error = EBUSY;
			break;
		}
		if (device_get_parent(dev) == NULL) {
			error = EINVAL;
			break;
		}
		error = BUS_SUSPEND_CHILD(device_get_parent(dev), dev);
		break;
	case DEV_RESUME:
		if (!device_is_suspended(dev)) {
			error = EINVAL;
			break;
		}
		if (device_get_parent(dev) == NULL) {
			error = EINVAL;
			break;
		}
		error = BUS_RESUME_CHILD(device_get_parent(dev), dev);
		break;
	case DEV_SET_DRIVER: {
		devclass_t dc;
		char driver[128];

		error = copyinstr(req->dr_data, driver, sizeof(driver), NULL);
		if (error)
			break;
		if (driver[0] == '\0') {
			error = EINVAL;
			break;
		}
		if (dev->devclass != NULL &&
		    strcmp(driver, dev->devclass->name) == 0)
			/* XXX: Could possibly force DF_FIXEDCLASS on? */
			break;

		/*
		 * Scan drivers for this device's bus looking for at
		 * least one matching driver.
		 */
		if (dev->parent == NULL) {
			error = EINVAL;
			break;
		}
		if (!driver_exists(dev->parent, driver)) {
			error = ENOENT;
			break;
		}
		dc = devclass_create(driver);
		if (dc == NULL) {
			error = ENOMEM;
			break;
		}

		/* Detach device if necessary. */
		if (device_is_attached(dev)) {
			if (req->dr_flags & DEVF_SET_DRIVER_DETACH)
				error = device_detach(dev);
			else
				error = EBUSY;
			if (error)
				break;
		}

		/* Clear any previously-fixed device class and unit. */
		if (dev->flags & DF_FIXEDCLASS)
			devclass_delete_device(dev->devclass, dev);
		dev->flags |= DF_WILDCARD;
		dev->unit = DEVICE_UNIT_ANY;

		/* Force the new device class. */
		error = devclass_add_device(dc, dev);
		if (error)
			break;
		dev->flags |= DF_FIXEDCLASS;
		error = device_probe_and_attach(dev);
		break;
	}
	case DEV_CLEAR_DRIVER:
		if (!(dev->flags & DF_FIXEDCLASS)) {
			error = 0;
			break;
		}
		if (device_is_attached(dev)) {
			if (req->dr_flags & DEVF_CLEAR_DRIVER_DETACH)
				error = device_detach(dev);
			else
				error = EBUSY;
			if (error)
				break;
		}

		dev->flags &= ~DF_FIXEDCLASS;
		dev->flags |= DF_WILDCARD;
		devclass_delete_device(dev->devclass, dev);
		error = device_probe_and_attach(dev);
		break;
	case DEV_RESCAN:
		if (!device_is_attached(dev)) {
			error = ENXIO;
			break;
		}
		error = BUS_RESCAN(dev);
		break;
	case DEV_DELETE: {
		device_t parent;

		parent = device_get_parent(dev);
		if (parent == NULL) {
			error = EINVAL;
			break;
		}
		if (!(req->dr_flags & DEVF_FORCE_DELETE)) {
			if (bus_child_present(dev) != 0) {
				error = EBUSY;
				break;
			}
		}
		
		error = device_delete_child(parent, dev);
		break;
	}
	case DEV_FREEZE:
		if (device_frozen)
			error = EBUSY;
		else
			device_frozen = true;
		break;
	case DEV_THAW:
		if (!device_frozen)
			error = EBUSY;
		else {
			device_do_deferred_actions();
			device_frozen = false;
		}
		break;
	case DEV_RESET:
		if ((req->dr_flags & ~(DEVF_RESET_DETACH)) != 0) {
			error = EINVAL;
			break;
		}
		if (device_get_parent(dev) == NULL) {
			error = EINVAL;
			break;
		}
		error = BUS_RESET_CHILD(device_get_parent(dev), dev,
		    req->dr_flags);
		break;
	case DEV_GET_PATH: {
		struct sbuf *sb;
		char locator[64];
		ssize_t len;

		error = copyinstr(req->dr_buffer.buffer, locator,
		    sizeof(locator), NULL);
		if (error != 0)
			break;
		sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND |
		    SBUF_INCLUDENUL /* | SBUF_WAITOK */);
		error = device_get_path(dev, locator, sb);
		if (error == 0) {
			len = sbuf_len(sb);
			if (req->dr_buffer.length < len) {
				error = ENAMETOOLONG;
			} else {
				error = copyout(sbuf_data(sb),
				    req->dr_buffer.buffer, len);
			}
			req->dr_buffer.length = len;
		}
		sbuf_delete(sb);
		break;
	}
	}
	bus_topo_unlock();
	return (error);
}

static struct cdevsw devctl2_cdevsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	devctl2_ioctl,
	.d_name =	"devctl2",
};

static void
devctl2_init(void)
{
	make_dev_credf(MAKEDEV_ETERNAL, &devctl2_cdevsw, 0, NULL,
	    UID_ROOT, GID_WHEEL, 0644, "devctl2");
}

/*
 * For maintaining device 'at' location info to avoid recomputing it
 */
struct device_location_node {
	const char *dln_locator;
	const char *dln_path;
	TAILQ_ENTRY(device_location_node) dln_link;
};
typedef TAILQ_HEAD(device_location_list, device_location_node) device_location_list_t;

struct device_location_cache {
	device_location_list_t dlc_list;
};


/*
 * Location cache for wired devices.
 */
device_location_cache_t *
dev_wired_cache_init(void)
{
	device_location_cache_t *dcp;

	dcp = malloc(sizeof(*dcp), M_BUS, M_WAITOK | M_ZERO);
	TAILQ_INIT(&dcp->dlc_list);

	return (dcp);
}

void
dev_wired_cache_fini(device_location_cache_t *dcp)
{
	struct device_location_node *dln, *tdln;

	TAILQ_FOREACH_SAFE(dln, &dcp->dlc_list, dln_link, tdln) {
		free(dln, M_BUS);
	}
	free(dcp, M_BUS);
}

static struct device_location_node *
dev_wired_cache_lookup(device_location_cache_t *dcp, const char *locator)
{
	struct device_location_node *dln;

	TAILQ_FOREACH(dln, &dcp->dlc_list, dln_link) {
		if (strcmp(locator, dln->dln_locator) == 0)
			return (dln);
	}

	return (NULL);
}

static struct device_location_node *
dev_wired_cache_add(device_location_cache_t *dcp, const char *locator, const char *path)
{
	struct device_location_node *dln;
	size_t loclen, pathlen;

	loclen = strlen(locator) + 1;
	pathlen = strlen(path) + 1;
	dln = malloc(sizeof(*dln) + loclen + pathlen, M_BUS, M_WAITOK | M_ZERO);
	dln->dln_locator = (char *)(dln + 1);
	memcpy(__DECONST(char *, dln->dln_locator), locator, loclen);
	dln->dln_path = dln->dln_locator + loclen;
	memcpy(__DECONST(char *, dln->dln_path), path, pathlen);
	TAILQ_INSERT_HEAD(&dcp->dlc_list, dln, dln_link);

	return (dln);
}

bool
dev_wired_cache_match(device_location_cache_t *dcp, device_t dev,
    const char *at)
{
	struct sbuf *sb;
	const char *cp;
	char locator[32];
	int error, len;
	struct device_location_node *res;

	cp = strchr(at, ':');
	if (cp == NULL)
		return (false);
	len = cp - at;
	if (len > sizeof(locator) - 1)	/* Skip too long locator */
		return (false);
	memcpy(locator, at, len);
	locator[len] = '\0';
	cp++;

	error = 0;
	/* maybe cache this inside device_t and look that up, but not yet */
	res = dev_wired_cache_lookup(dcp, locator);
	if (res == NULL) {
		sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND |
		    SBUF_INCLUDENUL | SBUF_NOWAIT);
		if (sb != NULL) {
			error = device_get_path(dev, locator, sb);
			if (error == 0) {
				res = dev_wired_cache_add(dcp, locator,
				    sbuf_data(sb));
			}
			sbuf_delete(sb);
		}
	}
	if (error != 0 || res == NULL || res->dln_path == NULL)
		return (false);

	return (strcmp(res->dln_path, cp) == 0);
}

static struct device_prop_elm *
device_prop_find(device_t dev, const char *name)
{
	struct device_prop_elm *e;

	bus_topo_assert();

	LIST_FOREACH(e, &dev->props, link) {
		if (strcmp(name, e->name) == 0)
			return (e);
	}
	return (NULL);
}

int
device_set_prop(device_t dev, const char *name, void *val,
    device_prop_dtr_t dtr, void *dtr_ctx)
{
	struct device_prop_elm *e, *e1;

	bus_topo_assert();

	e = device_prop_find(dev, name);
	if (e != NULL)
		goto found;

	e1 = malloc(sizeof(*e), M_BUS, M_WAITOK);
	e = device_prop_find(dev, name);
	if (e != NULL) {
		free(e1, M_BUS);
		goto found;
	}

	e1->name = name;
	e1->val = val;
	e1->dtr = dtr;
	e1->dtr_ctx = dtr_ctx;
	LIST_INSERT_HEAD(&dev->props, e1, link);
	return (0);

found:
	LIST_REMOVE(e, link);
	if (e->dtr != NULL)
		e->dtr(dev, name, e->val, e->dtr_ctx);
	e->val = val;
	e->dtr = dtr;
	e->dtr_ctx = dtr_ctx;
	LIST_INSERT_HEAD(&dev->props, e, link);
	return (EEXIST);
}

int
device_get_prop(device_t dev, const char *name, void **valp)
{
	struct device_prop_elm *e;

	bus_topo_assert();

	e = device_prop_find(dev, name);
	if (e == NULL)
		return (ENOENT);
	*valp = e->val;
	return (0);
}

int
device_clear_prop(device_t dev, const char *name)
{
	struct device_prop_elm *e;

	bus_topo_assert();

	e = device_prop_find(dev, name);
	if (e == NULL)
		return (ENOENT);
	LIST_REMOVE(e, link);
	if (e->dtr != NULL)
		e->dtr(dev, e->name, e->val, e->dtr_ctx);
	free(e, M_BUS);
	return (0);
}

static void
device_destroy_props(device_t dev)
{
	struct device_prop_elm *e;

	bus_topo_assert();

	while ((e = LIST_FIRST(&dev->props)) != NULL) {
		LIST_REMOVE_HEAD(&dev->props, link);
		if (e->dtr != NULL)
			e->dtr(dev, e->name, e->val, e->dtr_ctx);
		free(e, M_BUS);
	}
}

void
device_clear_prop_alldev(const char *name)
{
	device_t dev;

	TAILQ_FOREACH(dev, &bus_data_devices, devlink) {
		device_clear_prop(dev, name);
	}
}

/*
 * APIs to manage deprecation and obsolescence.
 */
static int obsolete_panic = 0;
SYSCTL_INT(_debug, OID_AUTO, obsolete_panic, CTLFLAG_RWTUN, &obsolete_panic, 0,
    "Panic when obsolete features are used (0 = never, 1 = if obsolete, "
    "2 = if deprecated)");

static void
gone_panic(int major, int running, const char *msg)
{
	switch (obsolete_panic)
	{
	case 0:
		return;
	case 1:
		if (running < major)
			return;
		/* FALLTHROUGH */
	default:
		panic("%s", msg);
	}
}

void
_gone_in(int major, const char *msg)
{
	gone_panic(major, P_OSREL_MAJOR(__FreeBSD_version), msg);
	if (P_OSREL_MAJOR(__FreeBSD_version) >= major)
		printf("Obsolete code will be removed soon: %s\n", msg);
	else
		printf("Deprecated code (to be removed in FreeBSD %d): %s\n",
		    major, msg);
}

void
_gone_in_dev(device_t dev, int major, const char *msg)
{
	gone_panic(major, P_OSREL_MAJOR(__FreeBSD_version), msg);
	if (P_OSREL_MAJOR(__FreeBSD_version) >= major)
		device_printf(dev,
		    "Obsolete code will be removed soon: %s\n", msg);
	else
		device_printf(dev,
		    "Deprecated code (to be removed in FreeBSD %d): %s\n",
		    major, msg);
}

#ifdef DDB
DB_SHOW_COMMAND(device, db_show_device)
{
	device_t dev;

	if (!have_addr)
		return;

	dev = (device_t)addr;

	db_printf("name:    %s\n", device_get_nameunit(dev));
	db_printf("  driver:  %s\n", DRIVERNAME(dev->driver));
	db_printf("  class:   %s\n", DEVCLANAME(dev->devclass));
	db_printf("  addr:    %p\n", dev);
	db_printf("  parent:  %p\n", dev->parent);
	db_printf("  softc:   %p\n", dev->softc);
	db_printf("  ivars:   %p\n", dev->ivars);
}

DB_SHOW_ALL_COMMAND(devices, db_show_all_devices)
{
	device_t dev;

	TAILQ_FOREACH(dev, &bus_data_devices, devlink) {
		db_show_device((db_expr_t)dev, true, count, modif);
	}
}
#endif
