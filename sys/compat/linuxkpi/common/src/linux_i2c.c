/*-
 * Copyright (c) 2021 Beckhoff Automation GmbH & Co. KG
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/list.h>
#include <linux/pci.h>

#include "iicbus_if.h"
#include "iicbb_if.h"
#include "lkpi_iic_if.h"

static int lkpi_i2c_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs);
static int lkpi_i2c_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr);

struct lkpi_iic_softc {
	device_t		iicbus;
	struct i2c_adapter	*adapter;
};

static int
lkpi_iic_probe(device_t dev)
{

	device_set_desc(dev, "LinuxKPI I2C");
	return (BUS_PROBE_NOWILDCARD);
}

static int
lkpi_iic_attach(device_t dev)
{
	struct lkpi_iic_softc *sc;

	sc = device_get_softc(dev);
	sc->iicbus = device_add_child(dev, "iicbus", -1);
	if (sc->iicbus == NULL) {
		device_printf(dev, "Couldn't add iicbus child, aborting\n");
		return (ENXIO);
	}
	bus_generic_attach(dev);
	return (0);
}

static int
lkpi_iic_detach(device_t dev)
{
	struct lkpi_iic_softc *sc;

	sc = device_get_softc(dev);
	if (sc->iicbus)
		device_delete_child(dev, sc->iicbus);
	return (0);
}

static int
lkpi_iic_add_adapter(device_t dev, struct i2c_adapter *adapter)
{
	struct lkpi_iic_softc *sc;

	sc = device_get_softc(dev);
	sc->adapter = adapter;

	return (0);
}

static device_method_t lkpi_iic_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		lkpi_iic_probe),
	DEVMETHOD(device_attach,	lkpi_iic_attach),
	DEVMETHOD(device_detach,	lkpi_iic_detach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* iicbus interface */
	DEVMETHOD(iicbus_transfer,	lkpi_i2c_transfer),
	DEVMETHOD(iicbus_reset,		lkpi_i2c_reset),
	DEVMETHOD(iicbus_callback,	iicbus_null_callback),

	/* lkpi_iic interface */
	DEVMETHOD(lkpi_iic_add_adapter,	lkpi_iic_add_adapter),

	DEVMETHOD_END
};

devclass_t lkpi_iic_devclass;

driver_t lkpi_iic_driver = {
	"lkpi_iic",
	lkpi_iic_methods,
	sizeof(struct lkpi_iic_softc),
};

DRIVER_MODULE(lkpi_iic, drmn, lkpi_iic_driver, lkpi_iic_devclass, 0, 0);
DRIVER_MODULE(iicbus, lkpi_iic, iicbus_driver, iicbus_devclass, 0, 0);

static int
lkpi_i2c_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{

	/* That doesn't seems to be supported in linux */
	return (0);
}

static int
lkpi_i2c_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	struct lkpi_iic_softc *sc;
	struct i2c_msg *linux_msgs;
	int i, ret = 0;

	sc = device_get_softc(dev);
	if (sc->adapter == NULL)
		return (ENXIO);
	linux_set_current(curthread);

	linux_msgs = malloc(sizeof(struct i2c_msg) * nmsgs,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	for (i = 0; i < nmsgs; i++) {
		linux_msgs[i].addr = msgs[i].slave;
		linux_msgs[i].len = msgs[i].len;
		linux_msgs[i].buf = msgs[i].buf;
		if (msgs[i].flags & IIC_M_RD) {
			linux_msgs[i].flags |= I2C_M_RD;
			for (int j = 0; j < msgs[i].len; j++)
				msgs[i].buf[j] = 0;
		}
		if (msgs[i].flags & IIC_M_NOSTART)
			linux_msgs[i].flags |= I2C_M_NOSTART;
	}
	ret = i2c_transfer(sc->adapter, linux_msgs, nmsgs);
	free(linux_msgs, M_DEVBUF);

	if (ret < 0)
		return (-ret);
	return (0);
}

int
lkpi_i2c_add_adapter(struct i2c_adapter *adapter)
{
	device_t lkpi_iic;
	int error;

	if (bootverbose)
		device_printf(adapter->dev.parent->bsddev,
		    "Adding i2c adapter %s\n", adapter->name);
	lkpi_iic = device_add_child(adapter->dev.parent->bsddev, "lkpi_iic", -1);
	if (lkpi_iic == NULL) {
		device_printf(adapter->dev.parent->bsddev, "Couldn't add lkpi_iic\n");
		return (ENXIO);
	}

	error = bus_generic_attach(adapter->dev.parent->bsddev);
	if (error) {
		device_printf(adapter->dev.parent->bsddev,
		  "failed to attach child: error %d\n", error);
		return (ENXIO);
	}
	LKPI_IIC_ADD_ADAPTER(lkpi_iic, adapter);
	return (0);
}

int
lkpi_i2c_del_adapter(struct i2c_adapter *adapter)
{
	device_t child;

	if (bootverbose)
		device_printf(adapter->dev.parent->bsddev,
		    "Removing i2c adapter %s\n", adapter->name);

	child = device_find_child(adapter->dev.parent->bsddev, "lkpi_iic", -1);
	if (child != NULL)
		device_delete_child(adapter->dev.parent->bsddev, child);

	child = device_find_child(adapter->dev.parent->bsddev, "lkpi_iicbb", -1);
	if (child != NULL)
		device_delete_child(adapter->dev.parent->bsddev, child);

	return (0);
}
