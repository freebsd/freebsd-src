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
 *	$Id: bus.h,v 1.2 1998/06/14 13:46:09 dfr Exp $
 */

#ifndef _SYS_BUS_H_
#define _SYS_BUS_H_

#ifdef KERNEL

/*
 * Forward declarations
 */
typedef struct device		*device_t;
typedef struct driver		driver_t;
typedef struct device_method	device_method_t;
typedef struct devclass		*devclass_t;
typedef struct device_ops	*device_ops_t;
typedef struct device_op_desc	*device_op_desc_t;

typedef void driver_intr_t(void*);

typedef enum driver_type {
    DRIVER_TYPE_TTY,
    DRIVER_TYPE_BIO,
    DRIVER_TYPE_NET,
    DRIVER_TYPE_MISC,
    MAX_DRIVER_TYPE
} driver_type_t;

typedef int (*devop_t)(void);

struct device_method {
    device_op_desc_t	desc;
    devop_t		func;
};

struct driver {
    const char		*name;		/* driver name */
    device_method_t	*methods;	/* method table */
    driver_type_t	type;
    size_t		softc;		/* size of device softc struct */
    TAILQ_ENTRY(driver) link;		/* list of devices on bus */
    device_ops_t	ops;		/* compiled method table */
};

typedef enum device_state device_state_t;
enum device_state {
    DS_NOTPRESENT,			/* not probed or probe failed */
    DS_ALIVE,				/* probe succeeded */
    DS_ATTACHED,			/* attach method called */
    DS_BUSY				/* device is open */
};

/*
 * The root bus, to which all top-level busses are attached.
 */
extern device_t root_bus;
extern devclass_t root_devclass;
void root_bus_configure(void);

/*
 * Useful functions for implementing busses.
 */
int bus_generic_attach(device_t dev);
int bus_generic_detach(device_t dev);
int bus_generic_shutdown(device_t dev);
void bus_generic_print_child(device_t dev, device_t child);
int bus_generic_read_ivar(device_t dev, device_t child, int which, u_long *result);
int bus_generic_write_ivar(device_t dev, device_t child, int which, u_long value);
int bus_generic_map_intr(device_t dev, device_t child, driver_intr_t *intr, void *arg);

/*
 * Access functions for device.
 */
device_t device_get_parent(device_t dev);
device_t device_add_child(device_t dev, const char *name,
			  int unit, void *ivars);
device_t device_add_child_after(device_t dev, device_t place, const char *name,
				int unit, void *ivars);
device_t device_find_child(device_t dev, const char *classname, int unit);
int device_delete_child(device_t dev, device_t child);
driver_t *device_get_driver(device_t dev);
devclass_t device_get_devclass(device_t dev);
int device_set_devclass(device_t dev, const char *classname);
int device_set_driver(device_t dev, driver_t *driver);
void device_set_desc(device_t dev, const char* desc);
const char* device_get_desc(device_t dev);
const char* device_get_name(device_t dev);
int device_get_unit(device_t dev);
void *device_get_softc(device_t dev);
void *device_get_ivars(device_t dev);
device_state_t device_get_state(device_t dev);
void device_enable(device_t dev);
void device_disable(device_t dev);
void device_busy(device_t dev);
void device_unbusy(device_t dev);
int device_is_enabled(device_t dev);
int device_is_alive(device_t dev); /* did probe succeed? */
int device_probe_and_attach(device_t dev);
int device_detach(device_t dev);
int device_shutdown(device_t dev);

/*
 * Access functions for devclass.
 */
devclass_t devclass_find(const char *classname);
int devclass_add_driver(devclass_t dc, driver_t *driver);
int devclass_delete_driver(devclass_t dc, driver_t *driver);
driver_t *devclass_find_driver(devclass_t dc, const char *classname);
const char *devclass_get_name(devclass_t dc);
device_t devclass_get_device(devclass_t dc, int unit);
void *devclass_get_softc(devclass_t dc, int unit);
int devclass_get_devices(devclass_t dc, device_t **devlistp, int *devcountp);
int devclass_get_maxunit(devclass_t dc);

/*
 * Shorthand for constructing method tables.
 */
#define DEVMETHOD(NAME, FUNC) { &NAME##_desc, (devop_t) FUNC }

/*
 * Some common device interfaces.
 */
#include "device_if.h"
#include "bus_if.h"

#ifdef _SYS_MODULE_H_

/*
 * Module support for automatically adding drivers to busses.
 */
struct driver_module_data {
    modeventhand_t	chainevh;
    void*		chainarg;
    const char*		busname;
    driver_t*		driver;
    devclass_t*		devclass;
};

int driver_module_handler(module_t, modeventtype_t, void*);

#define DRIVER_MODULE(name, busname, driver, devclass, evh, arg)	\
									\
static struct driver_module_data name##_driver_mod = {			\
	evh, arg,							\
	#busname,							\
	&driver,							\
	&devclass,							\
};									\
									\
static moduledata_t name##_mod = {					\
	#name,								\
	driver_module_handler,						\
	&name##_driver_mod						\
};									\
DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE)

#define CDEV_DRIVER_MODULE(name, busname, driver, devclass,	\
			   major, devsw, evh, arg)		\
								\
static struct cdevsw_module_data name##_cdevsw_mod = {		\
    evh, arg, makedev(major, 0), &devsw				\
};								\
								\
DRIVER_MODULE(name, busname, driver, devclass,			\
	      cdevsw_module_handler, &name##_cdevsw_mod)

#endif

#endif /* KERNEL */

#endif /* !_SYS_BUS_H_ */
