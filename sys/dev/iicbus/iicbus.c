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
 *
 * $FreeBSD$
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


#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbus_if.h"

#define DEVTOIICBUS(dev) ((struct iicbus_device*)device_get_ivars(dev))

static devclass_t iicbus_devclass;

/*
 * Device methods
 */
static int iicbus_probe(device_t);
static int iicbus_attach(device_t);
static int iicbus_detach(device_t);
static int iicbus_add_child(device_t dev, int order, const char *name, int unit);

static device_method_t iicbus_methods[] = {
        /* device interface */
        DEVMETHOD(device_probe,         iicbus_probe),
        DEVMETHOD(device_attach,        iicbus_attach),
        DEVMETHOD(device_detach,        iicbus_detach),

        /* bus interface */
        DEVMETHOD(bus_add_child,	iicbus_add_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
        DEVMETHOD(bus_print_child,      bus_generic_print_child),

        { 0, 0 }
};

static driver_t iicbus_driver = {
        "iicbus",
        iicbus_methods,
        sizeof(struct iicbus_softc),
};

static int
iicbus_probe(device_t dev)
{
	device_set_desc(dev, "Philips I2C bus");

	return (0);
}

#if 0
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
	iicbus_reset(dev, IIC_FASTEST, 0, NULL);

	/* device probing is meaningless since the bus is supposed to be
	 * hot-plug. Moreover, some I2C chips do not appreciate random
	 * accesses like stop after start to fast, reads for less than
	 * x bytes...
	 */
#if 0
	printf("Probing for devices on iicbus%d:", device_get_unit(dev));

	/* probe any devices */
	for (addr = FIRST_SLAVE_ADDR; addr <= LAST_SLAVE_ADDR; addr++) {
		if (iic_probe_device(dev, (u_char)addr)) {
			printf(" <%x>", addr);
		}
	}
	printf("\n");
#endif
  
	/* attach any known device */
	device_add_child(dev, NULL, -1);

	bus_generic_attach(dev);
         
        return (0);
}
  
static int
iicbus_detach(device_t dev)
{
	iicbus_reset(dev, IIC_FASTEST, 0, NULL);
  
	bus_generic_detach(dev);
  
	return (0);
}
  
static int
iicbus_add_child(device_t dev, int order, const char *name, int unit)
{
	device_add_child_ordered(dev, order, name, unit);

	bus_generic_attach(dev);

	return (0);
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

DRIVER_MODULE(iicbus, pcf, iicbus_driver, iicbus_devclass, 0, 0);
DRIVER_MODULE(iicbus, iicbb, iicbus_driver, iicbus_devclass, 0, 0);
DRIVER_MODULE(iicbus, bti2c, iicbus_driver, iicbus_devclass, 0, 0);
MODULE_VERSION(iicbus, IICBUS_MODVER);
