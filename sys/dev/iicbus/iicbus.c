/*-
 * Copyright (c) 1998 Nicolas Souchu
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
 *	$Id: iicbus.c,v 1.1.2.7 1998/08/29 16:54:16 son Exp $
 *
 */

/*
 * Autoconfiguration and support routines for the Philips serial I2C bus
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h> 

#include <machine/clock.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbus_if.h"

#define DEVTOIICBUS(dev) ((struct iicbus_device*)device_get_ivars(dev))

/*
 * structure used to attach devices to the I2C bus
 */
struct iicbus_device {
	const char *iicd_name;		/* device name */
	int iicd_class;			/* driver or slave device class */
	const char *iicd_desc;		/* device descriptor */
	u_char iicd_addr;		/* address of the device */
	int iicd_alive;			/* 1 if device found */
};

/*
 * Common I2C addresses
 */
#define I2C_GENERAL_CALL	0x0
#define I2C_MASTER_ADDRESS	0xaa
#define I2C_INET_ADDRESS	0xaa

#define MAXSLAVE 256

#define IICBUS_UNKNOWN_CLASS	0
#define IICBUS_DEVICE_CLASS	1
#define IICBUS_DRIVER_CLASS	2

/*
 * list of known devices
 */
struct iicbus_device iicbus_children[] = {
	{ "iic", IICBUS_DRIVER_CLASS, "General Call", I2C_GENERAL_CALL },
	{ "iicsmb", IICBUS_DRIVER_CLASS, "I2C to SMB bridge" },
	{ "iic", IICBUS_DEVICE_CLASS, "PCF8574 I2C to 8 bits parallel i/o", 64},
	{ "iic", IICBUS_DEVICE_CLASS, "PCF8584 as slave", I2C_MASTER_ADDRESS },
	{ "ic", IICBUS_DEVICE_CLASS, "network interface", I2C_INET_ADDRESS },
	{ NULL, 0 }
};

static devclass_t iicbus_devclass;

/*
 * Device methods
 */
static int iicbus_probe(device_t);
static int iicbus_attach(device_t);
static void iicbus_print_child(device_t, device_t);
static int iicbus_read_ivar(device_t , device_t, int, u_long *);

static device_method_t iicbus_methods[] = {
        /* device interface */
        DEVMETHOD(device_probe,         iicbus_probe),
        DEVMETHOD(device_attach,        iicbus_attach),
        DEVMETHOD(device_detach,        bus_generic_detach),
        DEVMETHOD(device_shutdown,      bus_generic_shutdown),

        /* bus interface */
        DEVMETHOD(bus_print_child,      iicbus_print_child),
        DEVMETHOD(bus_read_ivar,        iicbus_read_ivar),
        DEVMETHOD(bus_write_ivar,       bus_generic_write_ivar),
        DEVMETHOD(bus_create_intr,      bus_generic_create_intr),
        DEVMETHOD(bus_connect_intr,     bus_generic_connect_intr),

        { 0, 0 }
};

static driver_t iicbus_driver = {
        "iicbus",
        iicbus_methods,
        DRIVER_TYPE_MISC,
        sizeof(struct iicbus_softc),
};

/*
 * At 'probe' time, we add all the devices which we know about to the
 * bus.  The generic attach routine will probe and attach them if they
 * are alive.
 */
static int
iicbus_probe(device_t dev)
{
	struct iicbus_softc *sc = device_get_softc(dev);
	struct iicbus_device *iicdev;
	device_t child;

	/* XXX should query parent */
	sc->ownaddr = I2C_MASTER_ADDRESS;

	iicbus_reset(dev, IIC_FASTEST);

	for (iicdev = iicbus_children; iicdev->iicd_name; iicdev++) {

		/* probe devices, not drivers */
		switch (iicdev->iicd_class) {
		case IICBUS_DEVICE_CLASS:
			if (!iicbus_start(dev, iicdev->iicd_addr)) {
				iicbus_stop(dev);
				iicdev->iicd_alive = 1;
			}
			break;
		case IICBUS_DRIVER_CLASS:
			iicdev->iicd_addr = sc->ownaddr;
			break;
		default:
			panic("%s: unknown class!", __FUNCTION__);
		}

		child = device_add_child(dev, iicdev->iicd_name, -1, iicdev);
		device_set_desc(child, iicdev->iicd_desc);
	}

	return (0);
}

static int
iicbus_attach(device_t dev)
{
	bus_generic_attach(dev);
         
        return (0);
}

int
iicbus_generic_intr(device_t dev, int event, char *buf)
{
	return (0);
}

static void
iicbus_print_child(device_t bus, device_t dev)
{
	struct iicbus_device* iicdev = DEVTOIICBUS(dev);

	switch (iicdev->iicd_class) {	
	case IICBUS_DEVICE_CLASS:
		printf(" on %s%d addr %d %s", device_get_name(bus),
			device_get_unit(bus), iicdev->iicd_addr,
			(iicdev->iicd_alive) ? "found" : "not found");
		break;

	case IICBUS_DRIVER_CLASS:
		printf(" on %s%d", device_get_name(bus),
			device_get_unit(bus));
		break;

	default:
		panic("%s: unknown class!", __FUNCTION__);
	}

	return;
}

static int
iicbus_read_ivar(device_t bus, device_t dev, int index, u_long* result)
{
	struct iicbus_device* iicdev = DEVTOIICBUS(dev);

	switch (index) {
	case IICBUS_IVAR_ADDR:
		*result = (u_long)iicdev->iicd_addr;
		break;

	default:
		return (ENOENT);
	}

	return (0);
}

DRIVER_MODULE(iicbus, pcf, iicbus_driver, iicbus_devclass, 0, 0);
