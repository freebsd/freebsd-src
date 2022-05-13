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

#include "iicbb_if.h"
#include "lkpi_iic_if.h"

static void lkpi_iicbb_setsda(device_t dev, int val);
static void lkpi_iicbb_setscl(device_t dev, int val);
static int lkpi_iicbb_getscl(device_t dev);
static int lkpi_iicbb_getsda(device_t dev);
static int lkpi_iicbb_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr);

struct lkpi_iicbb_softc {
	device_t		iicbb;
	struct i2c_adapter	*adapter;
};

static int
lkpi_iicbb_probe(device_t dev)
{

	device_set_desc(dev, "LinuxKPI I2CBB");
	return (BUS_PROBE_NOWILDCARD);
}

static int
lkpi_iicbb_attach(device_t dev)
{
	struct lkpi_iicbb_softc *sc;

	sc = device_get_softc(dev);
	sc->iicbb = device_add_child(dev, "iicbb", -1);
	if (sc->iicbb == NULL) {
		device_printf(dev, "Couldn't add iicbb child, aborting\n");
		return (ENXIO);
	}
	bus_generic_attach(dev);
	return (0);
}

static int
lkpi_iicbb_detach(device_t dev)
{
	struct lkpi_iicbb_softc *sc;

	sc = device_get_softc(dev);
	if (sc->iicbb)
		device_delete_child(dev, sc->iicbb);
	return (0);
}

static int
lkpi_iicbb_add_adapter(device_t dev, struct i2c_adapter *adapter)
{
	struct lkpi_iicbb_softc *sc;

	sc = device_get_softc(dev);
	sc->adapter = adapter;

	return (0);
}

static struct i2c_adapter *
lkpi_iicbb_get_adapter(device_t dev)
{
	struct lkpi_iicbb_softc *sc;

	sc = device_get_softc(dev);
	return (sc->adapter);
}

static device_method_t lkpi_iicbb_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		lkpi_iicbb_probe),
	DEVMETHOD(device_attach,	lkpi_iicbb_attach),
	DEVMETHOD(device_detach,	lkpi_iicbb_detach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* iicbb interface */
	DEVMETHOD(iicbb_setsda,		lkpi_iicbb_setsda),
	DEVMETHOD(iicbb_setscl,		lkpi_iicbb_setscl),
	DEVMETHOD(iicbb_getsda,		lkpi_iicbb_getsda),
	DEVMETHOD(iicbb_getscl,		lkpi_iicbb_getscl),
	DEVMETHOD(iicbb_reset,		lkpi_iicbb_reset),

	/* lkpi_iicbb interface */
	DEVMETHOD(lkpi_iic_add_adapter,	lkpi_iicbb_add_adapter),
	DEVMETHOD(lkpi_iic_get_adapter,	lkpi_iicbb_get_adapter),

	DEVMETHOD_END
};

driver_t lkpi_iicbb_driver = {
	"lkpi_iicbb",
	lkpi_iicbb_methods,
	sizeof(struct lkpi_iicbb_softc),
};

DRIVER_MODULE(lkpi_iicbb, lkpi_iic, lkpi_iicbb_driver, 0, 0);
DRIVER_MODULE(iicbb, lkpi_iicbb, iicbb_driver, 0, 0);
MODULE_DEPEND(lkpi_iicbb, iicbb, IICBB_MINVER, IICBB_PREFVER, IICBB_MAXVER);

static void
lkpi_iicbb_setsda(device_t dev, int val)
{
	struct lkpi_iicbb_softc *sc;
	struct i2c_algo_bit_data *algo_data;

	sc = device_get_softc(dev);
	algo_data = (struct i2c_algo_bit_data *)sc->adapter->algo_data;
	algo_data->setsda(algo_data->data, val);
	cpu_spinwait();
	DELAY(algo_data->udelay);
}

static void
lkpi_iicbb_setscl(device_t dev, int val)
{
	struct lkpi_iicbb_softc *sc;
	struct i2c_algo_bit_data *algo_data;

	sc = device_get_softc(dev);

	algo_data = (struct i2c_algo_bit_data *)sc->adapter->algo_data;
	algo_data->setscl(algo_data->data, val);
	cpu_spinwait();
	DELAY(algo_data->udelay);
}

static int
lkpi_iicbb_getscl(device_t dev)
{
	struct lkpi_iicbb_softc *sc;
	struct i2c_algo_bit_data *algo_data;
	unsigned long orig_ticks;
	int ret = 0;

	sc = device_get_softc(dev);

	algo_data = (struct i2c_algo_bit_data *)sc->adapter->algo_data;

	orig_ticks = ticks;
	while (!ret) {
		ret = algo_data->getscl(algo_data->data);

		if (ret)
			break;

		if (ticks > orig_ticks + algo_data->timeout)
			return (ETIMEDOUT);

		cpu_spinwait();
		DELAY(algo_data->udelay);
	}
	DELAY(algo_data->udelay);
	return (ret);
}

static int
lkpi_iicbb_getsda(device_t dev)
{
	struct lkpi_iicbb_softc *sc;
	struct i2c_algo_bit_data *algo_data;
	int ret = 0;

	sc = device_get_softc(dev);
	algo_data = (struct i2c_algo_bit_data *)sc->adapter->algo_data;

	cpu_spinwait();
	DELAY(algo_data->udelay);
	ret = algo_data->getsda(algo_data->data);
	cpu_spinwait();
	DELAY(algo_data->udelay);
	return (ret);
}

static int
lkpi_iicbb_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{

	return (0);
}

int
lkpi_i2cbb_transfer(struct i2c_adapter *adapter, struct i2c_msg *msgs, int nmsgs)
{

	/* TODO: convert from i2c_msg to iic_msg and call IICBUS_TRANFER */
	return (0);
}

int
lkpi_i2c_bit_add_bus(struct i2c_adapter *adapter)
{
	device_t lkpi_iicbb;
	int error;

	if (bootverbose)
		device_printf(adapter->dev.parent->bsddev,
		    "Adding i2c adapter %s\n", adapter->name);
	lkpi_iicbb = device_add_child(adapter->dev.parent->bsddev, "lkpi_iicbb", -1);
	if (lkpi_iicbb == NULL) {
		device_printf(adapter->dev.parent->bsddev, "Couldn't add lkpi_iicbb\n");
		return (ENXIO);
	}

	error = bus_generic_attach(adapter->dev.parent->bsddev);
	if (error) {
		device_printf(adapter->dev.parent->bsddev,
		  "failed to attach child: error %d\n", error);
		return (ENXIO);
	}
	LKPI_IIC_ADD_ADAPTER(lkpi_iicbb, adapter);
	return (0);
}

