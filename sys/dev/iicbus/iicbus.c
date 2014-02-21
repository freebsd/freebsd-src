/*-
 * Copyright (c) 1998, 2001 Nicolas Souchu
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
__FBSDID("$FreeBSD$");

/*
 * Autoconfiguration and support routines for the Philips serial I2C bus
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h> 

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbus_if.h"

/* See comments below for why auto-scanning is a bad idea. */
#define SCAN_IICBUS 0

static int
iicbus_probe(device_t dev)
{

	device_set_desc(dev, "Philips I2C bus");

	/* Allow other subclasses to override this driver. */
	return (BUS_PROBE_GENERIC);
}

#if SCAN_IICBUS
static int 
iic_probe_device(device_t dev, u_char addr)
{
	int count;
	char byte;

	if ((addr & 1) == 0) {
		/* is device writable? */
		if (!iicbus_start(dev, (u_char)addr, 0)) {
			iicbus_stop(dev);
			return (1);
		}
	} else {
		/* is device readable? */
		if (!iicbus_block_read(dev, (u_char)addr, &byte, 1, &count))
			return (1);
	}

	return (0);
}
#endif

/*
 * We add all the devices which we know about.
 * The generic attach routine will attach them if they are alive.
 */
static int
iicbus_attach(device_t dev)
{
#if SCAN_IICBUS
	unsigned char addr;
#endif
	struct iicbus_softc *sc = IICBUS_SOFTC(dev);
	int strict;

	sc->dev = dev;
	mtx_init(&sc->lock, "iicbus", NULL, MTX_DEF);
	iicbus_reset(dev, IIC_FASTEST, 0, NULL);
	if (resource_int_value(device_get_name(dev),
		device_get_unit(dev), "strict", &strict) == 0)
		sc->strict = strict;
	else
		sc->strict = 1;

	/* device probing is meaningless since the bus is supposed to be
	 * hot-plug. Moreover, some I2C chips do not appreciate random
	 * accesses like stop after start to fast, reads for less than
	 * x bytes...
	 */
#if SCAN_IICBUS
	printf("Probing for devices on iicbus%d:", device_get_unit(dev));

	/* probe any devices */
	for (addr = 16; addr < 240; addr++) {
		if (iic_probe_device(dev, (u_char)addr)) {
			printf(" <%x>", addr);
		}
	}
	printf("\n");
#endif
	bus_generic_probe(dev);
	bus_enumerate_hinted_children(dev);
	bus_generic_attach(dev);
        return (0);
}
  
static int
iicbus_detach(device_t dev)
{
	struct iicbus_softc *sc = IICBUS_SOFTC(dev);

	iicbus_reset(dev, IIC_FASTEST, 0, NULL);
	bus_generic_detach(dev);
	mtx_destroy(&sc->lock);
	return (0);
}
  
static int
iicbus_print_child(device_t dev, device_t child)
{
	struct iicbus_ivar *devi = IICBUS_IVAR(child);
	int retval = 0;

	retval += bus_print_child_header(dev, child);
	if (devi->addr != 0)
		retval += printf(" at addr %#x", devi->addr);
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static void
iicbus_probe_nomatch(device_t bus, device_t child)
{
	struct iicbus_ivar *devi = IICBUS_IVAR(child);

	device_printf(bus, "<unknown card>");
	printf(" at addr %#x\n", devi->addr);
	return;
}

static int
iicbus_child_location_str(device_t bus, device_t child, char *buf,
    size_t buflen)
{
	struct iicbus_ivar *devi = IICBUS_IVAR(child);

	snprintf(buf, buflen, "addr=%#x", devi->addr);
	return (0);
}

static int
iicbus_child_pnpinfo_str(device_t bus, device_t child, char *buf,
    size_t buflen)
{
	*buf = '\0';
	return (0);
}

static int
iicbus_read_ivar(device_t bus, device_t child, int which, uintptr_t *result)
{
	struct iicbus_ivar *devi = IICBUS_IVAR(child);

	switch (which) {
	default:
		return (EINVAL);
	case IICBUS_IVAR_ADDR:
		*result = devi->addr;
		break;
	}
	return (0);
}

static device_t
iicbus_add_child(device_t dev, u_int order, const char *name, int unit)
{
	device_t child;
	struct iicbus_ivar *devi;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL)
		return (child);
	devi = malloc(sizeof(struct iicbus_ivar), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (devi == NULL) {
		device_delete_child(dev, child);
		return (0);
	}
	device_set_ivars(child, devi);
	return (child);
}

static void
iicbus_hinted_child(device_t bus, const char *dname, int dunit)
{
	device_t child;
	struct iicbus_ivar *devi;

	child = BUS_ADD_CHILD(bus, 0, dname, dunit);
	devi = IICBUS_IVAR(child);
	resource_int_value(dname, dunit, "addr", &devi->addr);
}

int
iicbus_generic_intr(device_t dev, int event, char *buf)
{

	return (0);
}

int
iicbus_null_callback(device_t dev, int index, caddr_t data)
{

	return (0);
}

int
iicbus_null_repeated_start(device_t dev, u_char addr)
{

	return (IIC_ENOTSUPP);
}

static device_method_t iicbus_methods[] = {
        /* device interface */
        DEVMETHOD(device_probe,         iicbus_probe),
        DEVMETHOD(device_attach,        iicbus_attach),
        DEVMETHOD(device_detach,        iicbus_detach),

        /* bus interface */
        DEVMETHOD(bus_add_child,	iicbus_add_child),
        DEVMETHOD(bus_print_child,      iicbus_print_child),
	DEVMETHOD(bus_probe_nomatch,	iicbus_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	iicbus_read_ivar),
	DEVMETHOD(bus_child_pnpinfo_str, iicbus_child_pnpinfo_str),
	DEVMETHOD(bus_child_location_str, iicbus_child_location_str),
	DEVMETHOD(bus_hinted_child,	iicbus_hinted_child),

	/* iicbus interface */
	DEVMETHOD(iicbus_transfer,	iicbus_transfer),

	DEVMETHOD_END
};

driver_t iicbus_driver = {
        "iicbus",
        iicbus_methods,
        sizeof(struct iicbus_softc),
};

devclass_t iicbus_devclass;

MODULE_VERSION(iicbus, IICBUS_MODVER);
DRIVER_MODULE(iicbus, iichb, iicbus_driver, iicbus_devclass, 0, 0);

