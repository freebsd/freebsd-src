/*-
 * Copyright (c) 1997,1998 Doug Rabson
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
 *
 *	$Id: subr_bus.c,v 1.7 1998/10/25 17:44:51 phk Exp $
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus_private.h>
#include <sys/systm.h>

#include "opt_bus.h"

#ifdef BUS_DEBUG
#define PDEBUG(a)	(printf(__FUNCTION__ ":%d: ", __LINE__), printf a, printf("\n"))
#define DEVICENAME(d)	((d)? device_get_name(d): "no device")
#define DRIVERNAME(d)	((d)? d->name : "no driver")
#define DEVCLANAME(d)	((d)? d->name : "no devclass")

/* Produce the indenting, indent*2 spaces plus a '.' ahead of that to 
 * prevent syslog from deleting initial spaces
 */
#define indentprintf(p)	do { int iJ; printf("."); for (iJ=0; iJ<indent; iJ++) printf("  "); printf p ; } while(0)

static void print_method_list(device_method_t *m, int indent);
static void print_device_ops(device_ops_t ops, int indent);
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
#define DRIVERNAME(d)			/* nop */
#define DEVCLANAME(d)			/* nop */

#define print_method_list(m,i)		/* nop */
#define print_device_ops(o,i)		/* nop */
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
 * Method table handling
 */
static int next_method_offset = 1;
static int methods_count = 0;
static int methods_size = 0;

struct method {
    int offset;
    char* name;
};

static struct method *methods = 0;

static void
register_method(struct device_op_desc *desc)
{
    int i;
    struct method* m;

    for (i = 0; i < methods_count; i++)
	if (!strcmp(methods[i].name, desc->name)) {
	    desc->offset = methods[i].offset;
	    PDEBUG(("methods[%d] has the same name, %s, with offset %d",
	    		i, desc->name, desc->offset));
	    return;
	}

    if (methods_count == methods_size) {
	struct method* p;

	methods_size += 10;
	p = (struct method*) malloc(methods_size * sizeof(struct method),
				     M_DEVBUF, M_NOWAIT);
	if (!p)
	    panic("register_method: out of memory");
	if (methods) {
	    bcopy(methods, p, methods_count * sizeof(struct method));
	    free(methods, M_DEVBUF);
	}
	methods = p;
    }
    m = &methods[methods_count++];
    m->name = malloc(strlen(desc->name) + 1, M_DEVBUF, M_NOWAIT);
    if (!m->name)
	    panic("register_method: out of memory");
    strcpy(m->name, desc->name);
    desc->offset = m->offset = next_method_offset++;
}

static int error_method(void)
{
    return ENXIO;
}

static struct device_ops null_ops = {
    1, 
    { error_method }
};

static void
compile_methods(driver_t *driver)
{
    device_ops_t ops;
    struct device_method *m;
    int i;

    /*
     * First register any methods which need it.
     */
    for (i = 0, m = driver->methods; m->desc; i++, m++)
	if (!m->desc->offset)
	    register_method(m->desc);
	else
	    PDEBUG(("offset not equal to zero, method desc %d left as is", i));

    /*
     * Then allocate the compiled op table.
     */
    ops = malloc(sizeof(struct device_ops) + (next_method_offset-1) * sizeof(devop_t),
		 M_DEVBUF, M_NOWAIT);
    if (!ops)
	panic("compile_methods: out of memory");

    ops->maxoffset = next_method_offset;
    for (i = 0; i < next_method_offset; i++)
	ops->methods[i] = error_method;
    for (i = 0, m = driver->methods; m->desc; i++, m++)
	ops->methods[m->desc->offset] = m->func;
    PDEBUG(("%s has %d method%s, wasting %d bytes",
    		DRIVERNAME(driver), i, (i==1?"":"s"),
		(next_method_offset-i)*sizeof(devop_t)));

    driver->ops = ops;
}

/*
 * Devclass implementation
 */

static devclass_list_t devclasses = TAILQ_HEAD_INITIALIZER(devclasses);

static devclass_t
devclass_find_internal(const char *classname, int create)
{
    devclass_t dc;

    PDEBUG(("looking for %s", classname));
    if (!classname)
	return NULL;

    for (dc = TAILQ_FIRST(&devclasses); dc; dc = TAILQ_NEXT(dc, link))
	if (!strcmp(dc->name, classname))
	    return dc;

    PDEBUG(("%s not found%s", classname, (create? ", creating": "")));
    if (create) {
	dc = malloc(sizeof(struct devclass) + strlen(classname) + 1,
		    M_DEVBUF, M_NOWAIT);
	if (!dc)
	    return NULL;
	dc->name = (char*) (dc + 1);
	strcpy(dc->name, classname);
	dc->devices = NULL;
	dc->maxunit = 0;
	dc->nextunit = 0;
	TAILQ_INIT(&dc->drivers);
	TAILQ_INSERT_TAIL(&devclasses, dc, link);
    }

    return dc;
}

devclass_t
devclass_find(const char *classname)
{
    return devclass_find_internal(classname, FALSE);
}

int
devclass_add_driver(devclass_t dc, driver_t *driver)
{
    PDEBUG(("%s", DRIVERNAME(driver)));
    /*
     * Compile the drivers methods.
     */
    compile_methods(driver);

    /*
     * Make sure the devclass which the driver is implementing exists.
     */
    devclass_find_internal(driver->name, TRUE);

    TAILQ_INSERT_TAIL(&dc->drivers, driver, link);

    return 0;
}

int
devclass_delete_driver(devclass_t busclass, driver_t *driver)
{
    devclass_t dc = devclass_find(driver->name);
    device_t dev;
    int i;
    int error;

    PDEBUG(("%s from devclass %s", driver->name, DEVCLANAME(busclass)));

    if (!dc)
	return 0;

    /*
     * Disassociate from any devices.  We iterate through all the
     * devices in the devclass of the driver and detach any which are
     * using the driver.
     */
    for (i = 0; i < dc->maxunit; i++) {
	if (dc->devices[i]) {
	    dev = dc->devices[i];
	    if (dev->driver == driver) {
		if (error = device_detach(dev))
		    return error;
		device_set_driver(dev, NULL);
	    }
	}
    }

    TAILQ_REMOVE(&busclass->drivers, driver, link);
    return 0;
}

driver_t *
devclass_find_driver(devclass_t dc, const char *classname)
{
    driver_t *driver;

    PDEBUG(("%s in devclass %s", classname, DEVCLANAME(dc)));

    for (driver = TAILQ_FIRST(&dc->drivers); driver;
	 driver = TAILQ_NEXT(driver, link)) {
	if (!strcmp(driver->name, classname))
	    return driver;
    }

    PDEBUG(("not found"));
    return NULL;
}

const char *
devclass_get_name(devclass_t dc)
{
    return dc->name;
}

device_t
devclass_get_device(devclass_t dc, int unit)
{
    if (unit < 0 || unit >= dc->maxunit)
	return NULL;
    return dc->devices[unit];
}

void *
devclass_get_softc(devclass_t dc, int unit)
{
    device_t dev;

    if (unit < 0 || unit >= dc->maxunit)
	return NULL;
    dev = dc->devices[unit];
    if (!dev || dev->state < DS_ATTACHED)
	return NULL;
    return dev->softc;
}

int
devclass_get_devices(devclass_t dc, device_t **devlistp, int *devcountp)
{
    int i;
    int count;
    device_t *list;
    
    count = 0;
    for (i = 0; i < dc->maxunit; i++)
	if (dc->devices[i])
	    count++;

    list = malloc(count * sizeof(device_t), M_TEMP, M_NOWAIT);
    if (!list)
	return ENOMEM;

    count = 0;
    for (i = 0; i < dc->maxunit; i++)
	if (dc->devices[i]) {
	    list[count] = dc->devices[i];
	    count++;
	}

    *devlistp = list;
    *devcountp = count;

    return 0;
}

int
devclass_get_maxunit(devclass_t dc)
{
    return dc->maxunit;
}

static int
devclass_alloc_unit(devclass_t dc, int *unitp)
{
    int unit = *unitp;

    PDEBUG(("unit %d in devclass %s", unit, DEVCLANAME(dc)));

    /*
     * If we have been given a wired unit number, check for existing
     * device.
     */
    if (unit != -1) {
	device_t dev;
	dev = devclass_get_device(dc, unit);
	if (dev) {
	    printf("devclass_alloc_unit: %s%d already exists, using next available unit number\n", dc->name, unit);
	    unit = -1;
	}
    }

    if (unit == -1) {
	unit = dc->nextunit;
	dc->nextunit++;
    } else if (dc->nextunit <= unit)
	dc->nextunit = unit + 1;

    if (unit >= dc->maxunit) {
	device_t *newlist;
	int newsize;

	newsize = (dc->maxunit ? 2 * dc->maxunit
		   : MINALLOCSIZE / sizeof(device_t));
	newlist = malloc(sizeof(device_t) * newsize, M_DEVBUF, M_NOWAIT);
	if (!newlist)
	    return ENOMEM;
	bcopy(dc->devices, newlist, sizeof(device_t) * dc->maxunit);
	bzero(newlist + dc->maxunit,
	      sizeof(device_t) * (newsize - dc->maxunit));
	if (dc->devices)
	    free(dc->devices, M_DEVBUF);
	dc->devices = newlist;
	dc->maxunit = newsize;
    }
    PDEBUG(("now: unit %d in devclass %s", unit, DEVCLANAME(dc)));

    *unitp = unit;
    return 0;
}

static int
devclass_add_device(devclass_t dc, device_t dev)
{
    int error;

    PDEBUG(("%s in devclass %s", DEVICENAME(dev), DEVCLANAME(dc)));

    if (error = devclass_alloc_unit(dc, &dev->unit))
	return error;
    dc->devices[dev->unit] = dev;
    dev->devclass = dc;
    return 0;
}

static int
devclass_delete_device(devclass_t dc, device_t dev)
{
    PDEBUG(("%s in devclass %s", DEVICENAME(dev), DEVCLANAME(dc)));

    if (dev->devclass != dc
	|| dc->devices[dev->unit] != dev)
	panic("devclass_delete_device: inconsistent device class");
    dc->devices[dev->unit] = NULL;
    if (dev->flags & DF_WILDCARD)
	dev->unit = -1;
    dev->devclass = NULL;
    while (dc->nextunit > 0 && dc->devices[dc->nextunit - 1] == NULL)
	dc->nextunit--;
    return 0;
}

static device_t
make_device(device_t parent, const char *name,
	    int unit, void *ivars)
{
    device_t dev;
    devclass_t dc;
    int error;

    PDEBUG(("%s at %s as unit %d with%s ivars",
    	    name, DEVICENAME(parent), unit, (ivars? "":"out")));

    if (name) {
	dc = devclass_find_internal(name, TRUE);
	if (!dc) {
	    printf("make_device: can't find device class %s\n", name);
	    return NULL;
	}

	if (error = devclass_alloc_unit(dc, &unit))
	    return NULL;
    } else
	dc = NULL;

    dev = malloc(sizeof(struct device), M_DEVBUF, M_NOWAIT);
    if (!dev)
	return 0;

    dev->parent = parent;
    TAILQ_INIT(&dev->children);
    dev->ops = &null_ops;
    dev->driver = NULL;
    dev->devclass = dc;
    dev->unit = unit;
    dev->desc = NULL;
    dev->busy = 0;
    dev->flags = DF_ENABLED;
    if (unit == -1)
	dev->flags |= DF_WILDCARD;
    if (name)
	dev->flags |= DF_FIXEDCLASS;
    dev->ivars = ivars;
    dev->softc = NULL;

    if (dc)
	dc->devices[unit] = dev;

    dev->state = DS_NOTPRESENT;

    return dev;
}

static void
device_print_child(device_t dev, device_t child)
{
    printf("%s%d", device_get_name(child), device_get_unit(child));
    if (device_is_alive(child)) {
	if (device_get_desc(child))
	    printf(": <%s>", device_get_desc(child));
	BUS_PRINT_CHILD(dev, child);
    } else
	printf(" not found");
    printf("\n");
}

device_t
device_add_child(device_t dev, const char *name, int unit, void *ivars)
{
    device_t child;

    PDEBUG(("%s at %s as unit %d with%s ivars",
    	    name, DEVICENAME(dev), unit, (ivars? "":"out")));

    child = make_device(dev, name, unit, ivars);

    TAILQ_INSERT_TAIL(&dev->children, child, link);

    return child;
}

device_t
device_add_child_after(device_t dev, device_t place, const char *name,
		       int unit, void *ivars)
{
    device_t child;

    PDEBUG(("%s at %s after %s as unit %d with%s ivars",
    	    name, DEVICENAME(dev), DEVICENAME(place), unit, (ivars? "":"out")));

    child = make_device(dev, name, unit, ivars);

    if (place) {
	TAILQ_INSERT_AFTER(&dev->children, place, dev, link);
    } else {
	TAILQ_INSERT_HEAD(&dev->children, dev, link);
    }

    return child;
}

int
device_delete_child(device_t dev, device_t child)
{
    int error;

    PDEBUG(("%s from %s", DEVICENAME(child), DEVICENAME(dev)));

    if (error = device_detach(child))
	return error;
    if (child->devclass)
	devclass_delete_device(child->devclass, child);
    TAILQ_REMOVE(&dev->children, child, link);
    free(dev, M_DEVBUF);

    return 0;
}

/*
 * Find only devices attached to this bus.
 */
device_t
device_find_child(device_t dev, const char *classname, int unit)
{
    devclass_t dc;
    device_t child;

    dc = devclass_find(classname);
    if (!dc)
	return NULL;

    child = devclass_get_device(dc, unit);
    if (child && child->parent == dev)
	return child;
    return NULL;
}

static driver_t *
first_matching_driver(devclass_t dc, device_t dev)
{
    if (dev->devclass)
	return devclass_find_driver(dc, dev->devclass->name);
    else
	return TAILQ_FIRST(&dc->drivers);
}

static driver_t *
next_matching_driver(devclass_t dc, device_t dev, driver_t *last)
{
    if (dev->devclass) {
	driver_t *driver;
	for (driver = TAILQ_NEXT(last, link); driver;
	     driver = TAILQ_NEXT(driver, link))
	    if (!strcmp(dev->devclass->name, driver->name))
		return driver;
	return NULL;
    } else
	return TAILQ_NEXT(last, link);
}

static int
device_probe_child(device_t dev, device_t child)
{
    devclass_t dc;
    driver_t *driver;

    dc = dev->devclass;
    if (dc == NULL)
	panic("device_probe_child: parent device has no devclass");

    if (child->state == DS_ALIVE)
	return 0;

    for (driver = first_matching_driver(dc, child);
	 driver;
	 driver = next_matching_driver(dc, child, driver)) {
	PDEBUG(("Trying %s", DRIVERNAME(driver)));
	device_set_driver(child, driver);
	if (DEVICE_PROBE(child) == 0) {
	    if (!child->devclass)
		device_set_devclass(child, driver->name);
	    child->state = DS_ALIVE;
	    return 0;
	}
    }

    return ENXIO;
}

device_t
device_get_parent(device_t dev)
{
    return dev->parent;
}

driver_t *
device_get_driver(device_t dev)
{
    return dev->driver;
}

devclass_t
device_get_devclass(device_t dev)
{
    return dev->devclass;
}

const char *
device_get_name(device_t dev)
{
    if (dev->devclass)
	return devclass_get_name(dev->devclass);
    return NULL;
}

int
device_get_unit(device_t dev)
{
    return dev->unit;
}

const char *
device_get_desc(device_t dev)
{
    return dev->desc;
}

void
device_set_desc(device_t dev, const char* desc)
{
    dev->desc = desc;
}

void *
device_get_softc(device_t dev)
{
    return dev->softc;
}

void *
device_get_ivars(device_t dev)
{
    return dev->ivars;
}

device_state_t
device_get_state(device_t dev)
{
    return dev->state;
}

void
device_enable(device_t dev)
{
    dev->flags |= DF_ENABLED;
}

void
device_disable(device_t dev)
{
    dev->flags &= ~DF_ENABLED;
}

void
device_busy(device_t dev)
{
    if (dev->state < DS_ATTACHED)
	panic("device_busy: called for unattached device");
    if (dev->busy == 0 && dev->parent)
	device_busy(dev->parent);
    dev->busy++;
    dev->state = DS_BUSY;
}

void
device_unbusy(device_t dev)
{
    if (dev->state != DS_BUSY)
	panic("device_unbusy: called for non-busy device");
    dev->busy--;
    if (dev->busy == 0) {
	if (dev->parent)
	    device_unbusy(dev->parent);
	dev->state = DS_ATTACHED;
    }
}

int
device_is_enabled(device_t dev)
{
    return (dev->flags & DF_ENABLED) != 0;
}

int
device_is_alive(device_t dev)
{
    return dev->state >= DS_ALIVE;
}

int
device_set_devclass(device_t dev, const char *classname)
{
    devclass_t dc;

    if (dev->devclass) {
	printf("device_set_devclass: device class already set\n");
	return EINVAL;
    }

    dc = devclass_find_internal(classname, TRUE);
    if (!dc)
	return ENOMEM;

    return devclass_add_device(dc, dev);
}

int
device_set_driver(device_t dev, driver_t *driver)
{
    if (dev->state >= DS_ATTACHED)
	return EBUSY;

    if (dev->driver == driver)
	return 0;

    if (dev->softc) {
	free(dev->softc, M_DEVBUF);
	dev->softc = NULL;
    }
    dev->ops = &null_ops;
    dev->driver = driver;
    if (driver) {
	dev->ops = driver->ops;
	dev->softc = malloc(driver->softc, M_DEVBUF, M_NOWAIT);
	if (!dev->softc) {
	    dev->ops = &null_ops;
	    dev->driver = NULL;
	    return ENOMEM;
	}
	bzero(dev->softc, driver->softc);
    }
    return 0;
}

int
device_probe_and_attach(device_t dev)
{
    device_t bus = dev->parent;
    int error;

    if (dev->state >= DS_ALIVE)
	return 0;

    if (dev->flags & DF_ENABLED) {
	device_probe_child(bus, dev);
	device_print_child(bus, dev);
	if (dev->state == DS_ALIVE) {
	    error = DEVICE_ATTACH(dev);
	    if (!error)
		dev->state = DS_ATTACHED;
	    else {
		printf("device_probe_and_attach: %s%d attach returned %d\n",
		       dev->driver->name, dev->unit, error);
		device_set_driver(dev, NULL);
		dev->state = DS_NOTPRESENT;
	    }
	}
    } else
	printf("%s%d: disabled, not probed.\n",
	       dev->devclass->name, dev->unit);

    return 0;
}

int
device_detach(device_t dev)
{
    int error;

    PDEBUG(("%s", DEVICENAME(dev)));
    if (dev->state == DS_BUSY)
	return EBUSY;
    if (dev->state != DS_ATTACHED)
	return 0;

    if (error = DEVICE_DETACH(dev))
	    return error;

    if (!(dev->flags & DF_FIXEDCLASS))
	devclass_delete_device(dev->devclass, dev);

    dev->state = DS_NOTPRESENT;
    device_set_driver(dev, NULL);

    return 0;
}

int
device_shutdown(device_t dev)
{
    if (dev->state < DS_ATTACHED)
	return 0;
    return DEVICE_SHUTDOWN(dev);
}

/*
 * Access functions for device resources.
 */
extern struct config_device devtab[];
extern int devtab_count;

static int
resource_match_string(int i, char *resname, char *value)
{
	int j;
	struct resource *res;

	for (j = 0, res = devtab[i].resources;
	     j < devtab[i].resource_count; j++, res++)
		if (!strcmp(res->name, resname)
		    && res->type == RES_STRING
		    && !strcmp(res->u.stringval, value))
			return TRUE;
	return FALSE;
}

static int
resource_find(const char *name, int unit, char *resname, struct resource **result)
{
	int i, j;
	struct resource *res;

	/*
	 * First check specific instances, then generic.
	 */
	for (i = 0; i < devtab_count; i++) {
		if (devtab[i].unit < 0)
			continue;
		if (!strcmp(devtab[i].name, name) && devtab[i].unit == unit) {
			res = devtab[i].resources;
			for (j = 0; j < devtab[i].resource_count; j++, res++)
				if (!strcmp(res->name, resname)) {
					*result = res;
					return 0;
				}
		}
	}
	for (i = 0; i < devtab_count; i++) {
		if (devtab[i].unit >= 0)
			continue;
		if (!strcmp(devtab[i].name, name) && devtab[i].unit == unit) {
			res = devtab[i].resources;
			for (j = 0; j < devtab[i].resource_count; j++, res++)
				if (!strcmp(res->name, resname)) {
					*result = res;
					return 0;
				}
		}
	}
	return ENOENT;
}

int
resource_int_value(const char *name, int unit, char *resname, int *result)
{
	int error;
	struct resource *res;
	if ((error = resource_find(name, unit, resname, &res)) != 0)
		return error;
	if (res->type != RES_INT)
		return EFTYPE;
	*result = res->u.intval;
	return 0;
}

int
resource_long_value(const char *name, int unit, char *resname, long *result)
{
	int error;
	struct resource *res;
	if ((error = resource_find(name, unit, resname, &res)) != 0)
		return error;
	if (res->type != RES_LONG)
		return EFTYPE;
	*result = res->u.longval;
	return 0;
}

int
resource_string_value(const char *name, int unit, char *resname, char **result)
{
	int error;
	struct resource *res;
	if ((error = resource_find(name, unit, resname, &res)) != 0)
		return error;
	if (res->type != RES_STRING)
		return EFTYPE;
	*result = res->u.stringval;
	return 0;
}

int
resource_query_string(int i, char *resname, char *value)
{
	if (i < 0)
		i = 0;
	else
		i = i + 1;
	for (; i < devtab_count; i++)
		if (resource_match_string(i, resname, value))
			return i;
	return -1;
}

char *
resource_query_name(int i)
{
	return devtab[i].name;
}

int
resource_query_unit(int i)
{
	return devtab[i].unit;
}


/*
 * Some useful method implementations to make life easier for bus drivers.
 */
int
bus_generic_attach(device_t dev)
{
    device_t child;

    for (child = TAILQ_FIRST(&dev->children);
	 child; child = TAILQ_NEXT(child, link))
	device_probe_and_attach(child);

    return 0;
}

int
bus_generic_detach(device_t dev)
{
    device_t child;
    int error;

    if (dev->state != DS_ATTACHED)
	return EBUSY;

    for (child = TAILQ_FIRST(&dev->children);
	 child; child = TAILQ_NEXT(child, link))
	if (error = device_detach(child))
	    return error;

    return 0;
}

int
bus_generic_shutdown(device_t dev)
{
    device_t child;

    for (child = TAILQ_FIRST(&dev->children);
	 child; child = TAILQ_NEXT(child, link))
	DEVICE_SHUTDOWN(child);

    return 0;
}

void
bus_generic_print_child(device_t dev, device_t child)
{
}

int
bus_generic_read_ivar(device_t dev, device_t child, int index, u_long* result)
{
    return ENOENT;
}

int
bus_generic_write_ivar(device_t dev, device_t child, int index, u_long value)
{
    return ENOENT;
}

void *
bus_generic_create_intr(device_t dev, device_t child, int irq, driver_intr_t *intr, void *arg)
{
    /* Propagate up the bus hierarchy until someone handles it. */
    if (dev->parent)
	return BUS_CREATE_INTR(dev->parent, dev, irq, intr, arg);
    else
	return NULL;
}

int
bus_generic_connect_intr(device_t dev, void *ih)
{
    /* Propagate up the bus hierarchy until someone handles it. */
    if (dev->parent)
	return BUS_CONNECT_INTR(dev->parent, ih);
    else
	return EINVAL;
}

static int root_create_intr(device_t dev, device_t child,
			    driver_intr_t *intr, void *arg)
{
    /*
     * If an interrupt mapping gets to here something bad has happened.
     * Should probably panic.
     */
    return EINVAL;
}

static device_method_t root_methods[] = {
	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	bus_generic_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_create_intr,	root_create_intr),

	{ 0, 0 }
};

static driver_t root_driver = {
	"root",
	root_methods,
	DRIVER_TYPE_MISC,
	1,			/* no softc */
};

device_t root_bus;
devclass_t root_devclass;

static int
root_bus_module_handler(module_t mod, modeventtype_t what, void* arg)
{
    switch (what) {
    case MOD_LOAD:
	compile_methods(&root_driver);
	root_bus = make_device(NULL, "root", 0, NULL);
	root_bus->desc = "System root bus";
	root_bus->ops = root_driver.ops;
	root_bus->driver = &root_driver;
	root_bus->state = DS_ATTACHED;
	root_devclass = devclass_find_internal("root", FALSE);
	return 0;
    }

    return 0;
}

static moduledata_t root_bus_mod = {
	"rootbus",
	root_bus_module_handler,
	0
};
DECLARE_MODULE(rootbus, root_bus_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);

void
root_bus_configure()
{
    device_t dev;

    PDEBUG(("."));

    for (dev = TAILQ_FIRST(&root_bus->children); dev;
	 dev = TAILQ_NEXT(dev, link)) {
	device_probe_and_attach(dev);
    }
}

int
driver_module_handler(module_t mod, modeventtype_t what, void* arg)
{
    struct driver_module_data* data = (struct driver_module_data*) arg;
    devclass_t bus_devclass = devclass_find_internal(data->busname, TRUE);
    int error;

    switch (what) {
    case MOD_LOAD:
    	PDEBUG(("Loading module: driver %s on bus %s",
		DRIVERNAME(data->driver), data->busname));
	if (error = devclass_add_driver(bus_devclass,
					data->driver))
	    return error;
	*data->devclass =
	    devclass_find_internal(data->driver->name, TRUE);
	break;

    case MOD_UNLOAD:
    	PDEBUG(("Unloading module: driver %s from bus %s",
		DRIVERNAME(data->driver), data->busname));
	if (error = devclass_delete_driver(bus_devclass,
					   data->driver))
	    return error;
	break;
    }

    if (data->chainevh)
	return data->chainevh(mod, what, data->chainarg);
    else
	return 0;
}



#ifdef BUS_DEBUG

/* the _short versions avoid iteration by not calling anything that prints
 * more than oneliners. I love oneliners.
 */

static void
print_method_list(device_method_t *m, int indent)
{
	int i;

	if (!m)
		return;

	for (i = 0; m->desc; i++, m++)
		indentprintf(("method %d: %s, offset=%d\n",
			i, m->desc->name, m->desc->offset));
}

static void
print_device_ops(device_ops_t ops, int indent)
{
	int i;
	int count = 0;

	if (!ops)
		return;

	/* we present a list of the methods that are pointing to the
	 * error_method, but ignore the 0'th elements; it is always
	 * error_method.
	 */
	for (i = 1; i < ops->maxoffset; i++) {
		if (ops->methods[i] == error_method) {
			if (count == 0)
				indentprintf(("error_method:"));
			printf(" %d", i);
			count++;
		}
	}
	if (count)
		printf("\n");

	indentprintf(("(%d method%s, %d valid, %d error_method%s)\n",
		ops->maxoffset-1, (ops->maxoffset-1 == 1? "":"s"),
		ops->maxoffset-1-count,
		count, (count == 1? "":"'s")));
}

static void
print_device_short(device_t dev, int indent)
{
	if (!dev)
		return;

	indentprintf(("device %d: <%s> %sparent,%schildren,%s%s%s%sivars,%ssoftc,busy=%d\n",
		dev->unit, dev->desc,
		(dev->parent? "":"no "),
		(TAILQ_EMPTY(&dev->children)? "no ":""),
		(dev->flags&DF_ENABLED? "enabled,":"disabled,"),
		(dev->flags&DF_FIXEDCLASS? "fixed,":""),
		(dev->flags&DF_WILDCARD? "wildcard,":""),
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
	indentprintf(("Methods:\n"));
	print_device_ops(dev->ops, indent+1);
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

	for (child = TAILQ_FIRST(&dev->children); child;
		 child = TAILQ_NEXT(child, link))
		print_device_tree_short(child, indent+1);
}

void
print_device_tree(device_t dev, int indent)
/* print the device and all its children (indented) */
{
	device_t child;

	if (!dev)
		return;

	print_device(dev, indent);

	for (child = TAILQ_FIRST(&dev->children); child;
		 child = TAILQ_NEXT(child, link))
		print_device_tree(child, indent+1);
}

static void
print_driver_short(driver_t *driver, int indent)
{
	if (!driver)
		return;

	indentprintf(("driver %s: type = %s%s%s%s, softc size = %d\n",
		driver->name,
		/* yes, I know this looks silly, but going to bed at
		 * two o'clock and having to get up at 7:30 again is silly
		 * as well. As is sticking your head in a bucket of water.
		 */
		(driver->type == DRIVER_TYPE_TTY? "tty":""),
		(driver->type == DRIVER_TYPE_BIO? "bio":""),
		(driver->type == DRIVER_TYPE_NET? "net":""),
		(driver->type == DRIVER_TYPE_MISC? "misc":""),
		driver->softc));
}

static void
print_driver(driver_t *driver, int indent)
{
	if (!driver)
		return;

	print_driver_short(driver, indent);
	indentprintf(("Methods:\n"));
	print_method_list(driver->methods, indent+1);
	indentprintf(("Operations:\n"));
	print_device_ops(driver->ops, indent+1);
}


static void
print_driver_list(driver_list_t drivers, int indent)
{
	driver_t *driver;

	for (driver = TAILQ_FIRST(&drivers); driver;
	     driver = TAILQ_NEXT(driver, link))
		print_driver(driver, indent);
}

static void
print_devclass_short(devclass_t dc, int indent)
{
	device_t dev;

	if ( !dc )
		return;

	indentprintf(("devclass %s: max units = %d, next unit = %d\n",
		dc->name, dc->maxunit, dc->nextunit));
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
	for (dc = TAILQ_FIRST(&devclasses); dc; dc = TAILQ_NEXT(dc, link))
		print_devclass_short(dc, 0);
}

void
print_devclass_list(void)
{
	devclass_t dc;

	printf("Full listing of devclasses, drivers & devices:\n");
	for (dc = TAILQ_FIRST(&devclasses); dc; dc = TAILQ_NEXT(dc, link))
		print_devclass(dc, 0);
}

#endif
