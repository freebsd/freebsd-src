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
 *	$Id: smbus.c,v 1.1.1.2 1998/08/13 15:16:58 son Exp $
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h> 

#include <dev/smbus/smbconf.h>
#include <dev/smbus/smbus.h>

/*
 * Autoconfiguration and support routines for the Philips serial I2C bus
 */

#define DEVTOSMBUS(dev) ((struct smbus_device*)device_get_ivars(dev))

/*
 * structure used to attach devices to the I2C bus
 */
struct smbus_device {
	const char *smbd_name;		/* device name */
	const u_char smbd_addr;		/* address of the device */
	const char *smbd_desc;		/* device descriptor */
};

/*
 * list of known devices
 */
struct smbus_device smbus_children[] = {
	{ "smb", 0, "General Call" },
	{ NULL, 0 }
};

static devclass_t smbus_devclass;

/*
 * Device methods
 */
static int smbus_probe(device_t);
static int smbus_attach(device_t);
static void smbus_print_child(device_t, device_t);
static int smbus_read_ivar(device_t , device_t, int, u_long *);

static device_method_t smbus_methods[] = {
        /* device interface */
        DEVMETHOD(device_probe,         smbus_probe),
        DEVMETHOD(device_attach,        smbus_attach),
        DEVMETHOD(device_detach,        bus_generic_detach),
        DEVMETHOD(device_shutdown,      bus_generic_shutdown),

        /* bus interface */
        DEVMETHOD(bus_print_child,      smbus_print_child),
        DEVMETHOD(bus_read_ivar,        smbus_read_ivar),
        DEVMETHOD(bus_write_ivar,       bus_generic_write_ivar),
        DEVMETHOD(bus_create_intr,      bus_generic_create_intr),
        DEVMETHOD(bus_connect_intr,     bus_generic_connect_intr),

        { 0, 0 }
};

static driver_t smbus_driver = {
        "smbus",
        smbus_methods,
        DRIVER_TYPE_MISC,
        sizeof(struct smbus_softc),
};

/*
 * At 'probe' time, we add all the devices which we know about to the
 * bus.  The generic attach routine will probe and attach them if they
 * are alive.
 */
static int
smbus_probe(device_t dev)
{
	struct smbus_device *smbdev;
	device_t child;

	for (smbdev = smbus_children; smbdev->smbd_name; smbdev++) {

		child = device_add_child(dev, smbdev->smbd_name, -1, smbdev);
		device_set_desc(child, smbdev->smbd_desc);
	}

	return (0);
}

static int
smbus_attach(device_t dev)
{
	struct smbus_softc *sc = device_get_softc(dev);
	device_t parent = device_get_parent(dev);

	printf("Probing for devices on the SMB bus:\n");
	bus_generic_attach(dev);
         
        return (0);
}

void
smbus_generic_intr(device_t dev, u_char devaddr, char low, char high)
{
	return;
}

static void
smbus_print_child(device_t bus, device_t dev)
{
	struct smbus_device* smbdev = DEVTOSMBUS(dev);

	printf(" on %s%d addr 0x%x", device_get_name(bus),
		device_get_unit(bus), smbdev->smbd_addr);

	return;
}

static int
smbus_read_ivar(device_t bus, device_t dev, int index, u_long* result)
{
	struct smbus_device* smbdev = DEVTOSMBUS(dev);

	switch (index) {
	case SMBUS_IVAR_ADDR:
		*result = smbdev->smbd_addr;
		break;
	}
	return (ENOENT);
}

DRIVER_MODULE(smbus, iicsmb, smbus_driver, smbus_devclass, 0, 0);
