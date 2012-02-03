/*-
 * Copyright (c) 2000 KIYOHARA Takashi <kiyohara@kk.iij4u.ne.jp>
 * Copyright (c) 2000 Takanori Watanabe <takawata@jp.FreeBSD.org>
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>


#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pc98/pc98/canbus.h>
#include <pc98/pc98/canbusvars.h>
#include "canbus_if.h"


#define CANBE_IO_DELAY_TIME 5000


static MALLOC_DEFINE(M_CANBUSDEV, "canbusdev", "CanBe device");
struct canbus_device {
	struct resource_list cbdev_resources;
};

/* canbus softc */
struct canbus_softc {
	int io_delay_time;			/* CanBe I/O delay time */

	struct sysctl_ctx_list canbus_sysctl_ctx;
						/* dynamic sysctl tree */

	/* index register */
	int index_id;				/* index ID */
	struct resource *index_res;		/* index resouce */
	bus_space_tag_t index_tag;		/* index tag */
	bus_space_handle_t index_handle;	/* index handle */

	/* data register */
	int data_id;				/* data ID */
	struct resource *data_res;		/* data resouce */
	bus_space_tag_t data_tag;		/* data tag */
	bus_space_handle_t data_handle;		/* data handle */
};


/* Device interface methods */
static void	canbus_identify(driver_t *, device_t);
static int	canbus_probe(device_t);
static int	canbus_attach(device_t);
static int	canbus_detach(device_t);

/* Bus interface methods */
static int	canbus_print_child(device_t, device_t);
static device_t	canbus_add_child(device_t, u_int, const char *, int);
static struct resource *	canbus_alloc_resource(
    device_t, device_t, int, int *, u_long, u_long, u_long, u_int);
static int	canbus_activate_resource(
    device_t, device_t, int, int, struct resource *);
static int	canbus_deactivate_resource(
    device_t, device_t, int, int, struct resource *);
static int	canbus_release_resource(
    device_t, device_t, int, int, struct resource *);
static int	canbus_set_resource (
    device_t, device_t, int, int, u_long, u_long);
static void	canbus_delete_resource(device_t, device_t, int, int);

/* canbus local function */
static void	set_ioresource(device_t dev);
static void	delete_ioresource(device_t dev);
static int	alloc_ioresource(device_t);
static void	release_ioresource(device_t);
static int	print_all_resources(device_t);

static device_method_t canbus_methods[] = { 
	/* Device interface */
	DEVMETHOD(device_identify,	canbus_identify),
	DEVMETHOD(device_probe,		canbus_probe),
	DEVMETHOD(device_attach,	canbus_attach),
	DEVMETHOD(device_detach,	canbus_detach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	canbus_print_child),
	DEVMETHOD(bus_add_child,	canbus_add_child),
	DEVMETHOD(bus_alloc_resource,	canbus_alloc_resource),
	DEVMETHOD(bus_activate_resource,	canbus_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	canbus_deactivate_resource),
	DEVMETHOD(bus_release_resource,	canbus_release_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_set_resource,	canbus_set_resource),
	DEVMETHOD(bus_delete_resource,	canbus_delete_resource),

	/* CanBe interface */
	DEVMETHOD(canbus_read,		canbus_read),
	DEVMETHOD(canbus_write,		canbus_write),
	DEVMETHOD(canbus_write_multi,	canbus_write_multi),

	{0, 0}
};

static driver_t canbus_driver = {
	"canbus",
	canbus_methods,
	sizeof(struct canbus_softc),
};

devclass_t canbus_devclass;
DRIVER_MODULE(canbus, nexus, canbus_driver, canbus_devclass, 0, 0);
MODULE_VERSION(canbus, 1);


static void
canbus_identify(driver_t *drv, device_t parent)
{
	if (device_find_child(parent, "canbus", 0) == NULL) {
		if (BUS_ADD_CHILD(parent, 33, "canbus", 0) == NULL)
			device_printf(parent, "canbus cannot attach\n");
	}
}


static int
canbus_probe(device_t dev)
{
	u_int8_t flag;

	set_ioresource(dev);
	if(alloc_ioresource(dev))
		return (ENXIO);
	flag = canbus_read(dev, NULL, CANBE_SOUND_INTR_ADDR);
	release_ioresource(dev);

	if (bootverbose)
		device_printf(dev, "probe flag = 0x%x\n", flag);

	if (flag != CANBE_SOUND_INTR_VAL0 && flag != CANBE_SOUND_INTR_VAL1 &&
	    flag != CANBE_SOUND_INTR_VAL2 && flag != CANBE_SOUND_INTR_VAL3) {
		device_printf(dev, "Device Not Found\n");
		return (ENXIO);
	}
	device_set_desc(dev, "CanBe I/O Bus");

	return (0);	
}

static int
canbus_attach(device_t dev)
{
	struct canbus_softc *sc = device_get_softc(dev);
	struct sysctl_oid *canbus_sysctl_tree;

	sc->io_delay_time = CANBE_IO_DELAY_TIME;

	/* I/O resource setup */
	if(alloc_ioresource(dev))
		return (ENXIO);

	/* Dynamic sysctl tree setup */
	sysctl_ctx_init(&sc->canbus_sysctl_ctx);
	canbus_sysctl_tree = SYSCTL_ADD_NODE(&sc->canbus_sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(/* tree top */), OID_AUTO,
	    "canbus", CTLFLAG_RD, 0, "CanBe I/O Bus");
	SYSCTL_ADD_INT(&sc->canbus_sysctl_ctx,
	    SYSCTL_CHILDREN(canbus_sysctl_tree), OID_AUTO, "io_delay_time",
	    CTLFLAG_RW, &sc->io_delay_time, 0, "CanBe Bus I/O delay time");

	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);
}


static int
canbus_detach(device_t dev)
{
	struct canbus_softc *sc = device_get_softc(dev);

	/* I/O resource free */
	release_ioresource(dev);
	delete_ioresource(dev);

	/* Dynamic sysctl tree destroy */
	if (sysctl_ctx_free(&sc->canbus_sysctl_ctx)) {
		device_printf(dev,
		    "can't free this context - other oids depend on it\n");
		return (ENOTEMPTY);
	}

	return (0);
}


static int
canbus_print_child(device_t dev, device_t child)
{
	int     retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += print_all_resources(child);
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static device_t
canbus_add_child(device_t bus, u_int order, const char *name, int unit)
{
	device_t child;
	struct canbus_device *cbdev;

	child = device_add_child_ordered(bus, order, name, unit);

	cbdev = malloc(
	    sizeof(struct canbus_device), M_CANBUSDEV, M_NOWAIT | M_ZERO);
	if (!cbdev)
		return (0);

	resource_list_init(&cbdev->cbdev_resources);
	device_set_ivars(child, cbdev);

	return (child);
}

static struct resource *
canbus_alloc_resource(device_t dev, device_t child, int type,
    int *rid, u_long start, u_long end, u_long count, u_int flags)
{
	return (BUS_ALLOC_RESOURCE(device_get_parent(dev),
	    child, type, rid, start, end, count, flags));
}

static int
canbus_activate_resource(
    device_t dev, device_t child, int type, int rid, struct resource *res)
{
	return (BUS_ACTIVATE_RESOURCE(
	    device_get_parent(dev), child, type, rid, res));
}

static int
canbus_deactivate_resource(
    device_t dev, device_t child, int type, int rid, struct resource *res)
{
	return (BUS_DEACTIVATE_RESOURCE(
	    device_get_parent(dev), child, type, rid, res));
}

static int
canbus_release_resource(
    device_t dev, device_t child, int type, int rid, struct resource *res)
{
	return (BUS_RELEASE_RESOURCE(
	    device_get_parent(dev), child, type, rid, res));
}

static int
canbus_set_resource (
    device_t dev, device_t child, int type, int rid, u_long start, u_long count)
{
	struct  canbus_device *cbdev =
	    (struct canbus_device *)device_get_ivars(child);
	struct resource_list *rl = &cbdev->cbdev_resources;

	resource_list_add(rl, type, rid, start, (start + count - 1), count);

	return (0);
}

static void
canbus_delete_resource(device_t dev, device_t child, int type, int rid)
{
        struct  canbus_device *cbdev =
	    (struct canbus_device *)device_get_ivars(child);
        struct resource_list *rl = &cbdev->cbdev_resources;

        resource_list_delete(rl, type, rid);
}


u_int8_t
canbus_read(device_t dev, device_t child, int reg)
{
	struct canbus_softc *sc = device_get_softc(dev);

	bus_space_write_1(sc->index_tag, sc->index_handle, 0, reg);
	return (bus_space_read_1(sc->data_tag, sc->data_handle, 0));
}

void
canbus_write(device_t dev, device_t child, int reg, u_int8_t val)
{
	struct canbus_softc *sc = device_get_softc(dev);

	bus_space_write_1(sc->index_tag, sc->index_handle, 0, reg);
	bus_space_write_1(sc->data_tag, sc->data_handle, 0, val);
}

void
canbus_write_multi(device_t dev,
    device_t child, int reg, const int count, const u_int8_t *vals)
{
	struct canbus_softc *sc = device_get_softc(dev);
	int i;

	bus_space_write_1(sc->index_tag, sc->index_handle, 0, reg);

	for (i = 0; i < count; i ++) {
		bus_space_write_1(sc->data_tag, sc->data_handle, 0, vals[i]);
		DELAY(sc->io_delay_time);
	}
}

void
canbus_delay(device_t dev, device_t child)
{
	struct canbus_softc *sc = device_get_softc(dev);

	DELAY(sc->io_delay_time);
}


/*
 * canbus local function.
 */

/*
 * CanBe I/O resource set function
 */
static void
set_ioresource(device_t dev)
{
	struct canbus_softc *sc = device_get_softc(dev);

	sc->index_id = 0;
	sc->data_id = 1;

	bus_set_resource(
	    dev, SYS_RES_IOPORT, sc->index_id, CANBE_IOPORT_INDEX, 1);
	bus_set_resource(
	    dev, SYS_RES_IOPORT, sc->data_id, CANBE_IOPORT_DATA, 1);
}

/*
 * CanBe I/O resource delete function
 */
static void
delete_ioresource(device_t dev)
{
	struct canbus_softc *sc = device_get_softc(dev);

	bus_delete_resource(dev, SYS_RES_IOPORT, sc->index_id);
	bus_delete_resource(dev, SYS_RES_IOPORT, sc->data_id);
}

/*
 * CanBe I/O resource alloc function
 */
static int
alloc_ioresource(device_t dev)
{
	struct canbus_softc *sc = device_get_softc(dev);

	sc->index_res = bus_alloc_resource_any(
	    dev, SYS_RES_IOPORT, &sc->index_id, RF_ACTIVE);
	sc->data_res = bus_alloc_resource_any(
	    dev, SYS_RES_IOPORT, &sc->data_id, RF_ACTIVE);
	if (sc->index_res == NULL || sc->data_res == NULL) {
		device_printf(dev, "could not map I/O\n");
		return (ENXIO);
	}

	sc->index_tag = rman_get_bustag(sc->index_res);
	sc->index_handle = rman_get_bushandle(sc->index_res);
	sc->data_tag = rman_get_bustag(sc->data_res);
	sc->data_handle = rman_get_bushandle(sc->data_res);

	return (0);
}

/*
 * CanBe I/O resource release function
 */
static void
release_ioresource(device_t dev)
{
	struct canbus_softc *sc = device_get_softc(dev);

	bus_release_resource(dev, SYS_RES_IOPORT, sc->index_id, sc->index_res);
	bus_release_resource(dev, SYS_RES_IOPORT, sc->data_id, sc->data_res);
}


static int
print_all_resources(device_t dev)
{
	struct  canbus_device *cbdev =
	    (struct canbus_device *)device_get_ivars(dev);
	struct resource_list *rl = &cbdev->cbdev_resources;
	int retval = 0;

	if (STAILQ_FIRST(rl))
		retval += printf(" at");

	retval += resource_list_print_type(rl, "port", SYS_RES_IOPORT, "%#lx");
	retval += resource_list_print_type(rl, "iomem", SYS_RES_MEMORY, "%#lx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");

	return retval;
}
