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
 *	$Id: subr_bus.c,v 1.2 1998/06/14 13:45:58 dfr Exp $
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus_private.h>
#include <sys/systm.h>

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
    driver->ops = ops;
}

/*
 * Devclass implementation
 */

static devclass_list_t devclasses;

static void
devclass_init(void)
{
    TAILQ_INIT(&devclasses);
}

static devclass_t
devclass_find_internal(const char *classname, int create)
{
    devclass_t dc;

    for (dc = TAILQ_FIRST(&devclasses); dc; dc = TAILQ_NEXT(dc, link))
	if (!strcmp(dc->name, classname))
	    return dc;

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
devclass_delete_driver(devclass_t dc, driver_t *driver)
{
    device_t bus;
    device_t dev;
    int i;
    int error;

    /*
     * Disassociate from any devices.  We iterate through all the
     * devices attached to any bus in this class.
     */
    for (i = 0; i < dc->maxunit; i++) {
	if (dc->devices[i]) {
	    bus = dc->devices[i]->parent;
	    for (dev = TAILQ_FIRST(&bus->children); dev;
		 dev = TAILQ_NEXT(dev, link))
		if (dev->driver == driver) {
		    if (error = DEVICE_DETACH(dev))
			return error;
		    device_set_driver(dev, NULL);
		}
	}
    }

    TAILQ_REMOVE(&dc->drivers, driver, link);
    return 0;
}

driver_t *
devclass_find_driver(devclass_t dc, const char *classname)
{
    driver_t *driver;

    for (driver = TAILQ_FIRST(&dc->drivers); driver;
	 driver = TAILQ_NEXT(driver, link))
	if (!strcmp(driver->name, classname))
	    return driver;

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
    device_t dev;
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

    *unitp = unit;
    return 0;
}

static int
devclass_add_device(devclass_t dc, device_t dev)
{
    int error;

    if (error = devclass_alloc_unit(dc, &dev->unit))
	return error;
    dc->devices[dev->unit] = dev;
    dev->devclass = dc;
    return 0;
}

static int
devclass_delete_device(devclass_t dc, device_t dev)
{
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
    driver_t *driver;
    device_t dev;
    devclass_t dc;
    int error;

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

    child = make_device(dev, name, unit, ivars);

    TAILQ_INSERT_TAIL(&dev->children, child, link);

    return child;
}

device_t
device_add_child_after(device_t dev, device_t place, const char *name,
		       int unit, void *ivars)
{
    device_t child;

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

    if (error = DEVICE_DETACH(child))
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
    void *softc;

    dc = dev->devclass;
    if (dc == NULL)
	panic("device_probe_child: parent device has no devclass");

    if (child->state == DS_ALIVE)
	return 0;

    for (driver = first_matching_driver(dc, child);
	 driver;
	 driver = next_matching_driver(dc, child, driver)) {
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
		printf("device_probe_and_attach: %s%n attach returned %d\n",
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
 * Some useful method implementations to make life easier for bus drivers.
 */
int
bus_generic_attach(device_t dev)
{
    device_t child;
    int error;

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
	DEVICE_DETACH(child);

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
	devclass_init();
	compile_methods(&root_driver);
	root_bus = make_device(NULL, "root", 0, NULL);
	root_bus->ops = root_driver.ops;
	root_bus->driver = &root_driver;
	root_bus->state = DS_ATTACHED;
	root_devclass = devclass_find("root");
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
    int error;

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
	if (error = devclass_add_driver(bus_devclass,
					data->driver))
	    return error;
	*data->devclass =
	    devclass_find_internal(data->driver->name, TRUE);
	break;

    case MOD_UNLOAD:
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
