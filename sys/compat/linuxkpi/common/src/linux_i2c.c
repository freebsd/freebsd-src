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

static struct sx lkpi_sx_i2c;

static void
lkpi_sysinit_i2c(void *arg __unused)
{

	sx_init(&lkpi_sx_i2c, "lkpi-i2c");
}

static void
lkpi_sysuninit_i2c(void *arg __unused)
{

	sx_destroy(&lkpi_sx_i2c);
}

SYSINIT(lkpi_i2c, SI_SUB_DRIVERS, SI_ORDER_ANY,
    lkpi_sysinit_i2c, NULL);
SYSUNINIT(lkpi_i2c, SI_SUB_DRIVERS, SI_ORDER_ANY,
    lkpi_sysuninit_i2c, NULL);

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

static struct i2c_adapter *
lkpi_iic_get_adapter(device_t dev)
{
	struct lkpi_iic_softc *sc;

	sc = device_get_softc(dev);
	return (sc->adapter);
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
	DEVMETHOD(lkpi_iic_get_adapter,	lkpi_iic_get_adapter),

	DEVMETHOD_END
};

driver_t lkpi_iic_driver = {
	"lkpi_iic",
	lkpi_iic_methods,
	sizeof(struct lkpi_iic_softc),
};

DRIVER_MODULE(lkpi_iic, drmn, lkpi_iic_driver, 0, 0);
DRIVER_MODULE(lkpi_iic, drm, lkpi_iic_driver, 0, 0);
DRIVER_MODULE(iicbus, lkpi_iic, iicbus_driver, 0, 0);
MODULE_DEPEND(linuxkpi, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);

static int
lkpi_i2c_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{

	/* That doesn't seems to be supported in linux */
	return (0);
}

static int i2c_check_for_quirks(struct i2c_adapter *adapter,
    struct iic_msg *msgs, uint32_t nmsgs)
{
	const struct i2c_adapter_quirks *quirks;
	device_t dev;
	int i, max_nmsgs;
	bool check_len;

	dev = adapter->dev.parent->bsddev;
	quirks = adapter->quirks;
	if (quirks == NULL)
		return (0);

	check_len = true;
	max_nmsgs = quirks->max_num_msgs;

	if (quirks->flags & I2C_AQ_COMB) {
		max_nmsgs = 2;

		if (nmsgs == 2) {
			if (quirks->flags & I2C_AQ_COMB_WRITE_FIRST &&
			    msgs[0].flags & IIC_M_RD) {
				device_printf(dev,
				    "Error: "
				    "first combined message must be write\n");
				return (EOPNOTSUPP);
			}
			if (quirks->flags & I2C_AQ_COMB_READ_SECOND &&
			    !(msgs[1].flags & IIC_M_RD)) {
				device_printf(dev,
				    "Error: "
				    "second combined message must be read\n");
				return (EOPNOTSUPP);
			}

			if (quirks->flags & I2C_AQ_COMB_SAME_ADDR &&
			    msgs[0].slave != msgs[1].slave) {
				device_printf(dev,
				    "Error: "
				    "combined message must be use the same "
				    "address\n");
				return (EOPNOTSUPP);
			}

			if (quirks->max_comb_1st_msg_len &&
			    msgs[0].len > quirks->max_comb_1st_msg_len) {
				device_printf(dev,
				    "Error: "
				    "message too long: %hu > %hu max\n",
				    msgs[0].len,
				    quirks->max_comb_1st_msg_len);
				return (EOPNOTSUPP);
			}
			if (quirks->max_comb_2nd_msg_len &&
			    msgs[1].len > quirks->max_comb_2nd_msg_len) {
				device_printf(dev,
				    "Error: "
				    "message too long: %hu > %hu max\n",
				    msgs[1].len,
				    quirks->max_comb_2nd_msg_len);
				return (EOPNOTSUPP);
			}

			check_len = false;
		}
	}

	if (max_nmsgs && nmsgs > max_nmsgs) {
		device_printf(dev,
		    "Error: too many messages: %d > %d max\n",
		    nmsgs, max_nmsgs);
		return (EOPNOTSUPP);
	}

	for (i = 0; i < nmsgs; i++) {
		if (msgs[i].flags & IIC_M_RD) {
			if (check_len && quirks->max_read_len &&
			    msgs[i].len > quirks->max_read_len) {
				device_printf(dev,
				    "Error: "
				    "message %d too long: %hu > %hu max\n",
				    i, msgs[i].len, quirks->max_read_len);
				return (EOPNOTSUPP);
			}
			if (quirks->flags & I2C_AQ_NO_ZERO_LEN_READ &&
			    msgs[i].len == 0) {
				device_printf(dev,
				    "Error: message %d of length 0\n", i);
				return (EOPNOTSUPP);
			}
		} else {
			if (check_len && quirks->max_write_len &&
			    msgs[i].len > quirks->max_write_len) {
				device_printf(dev,
				    "Message %d too long: %hu > %hu max\n",
				    i, msgs[i].len, quirks->max_write_len);
				return (EOPNOTSUPP);
			}
			if (quirks->flags & I2C_AQ_NO_ZERO_LEN_WRITE &&
			    msgs[i].len == 0) {
				device_printf(dev,
				    "Error: message %d of length 0\n", i);
				return (EOPNOTSUPP);
			}
		}
	}

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
	ret = i2c_check_for_quirks(sc->adapter, msgs, nmsgs);
	if (ret != 0)
		return (ret);
	linux_set_current(curthread);

	linux_msgs = malloc(sizeof(struct i2c_msg) * nmsgs,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	for (i = 0; i < nmsgs; i++) {
		linux_msgs[i].addr = msgs[i].slave >> 1;
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

	if (adapter->name[0] == '\0')
		return (-EINVAL);
	if (bootverbose)
		device_printf(adapter->dev.parent->bsddev,
		    "Adding i2c adapter %s\n", adapter->name);
	sx_xlock(&lkpi_sx_i2c);
	lkpi_iic = device_add_child(adapter->dev.parent->bsddev, "lkpi_iic", -1);
	if (lkpi_iic == NULL) {
		device_printf(adapter->dev.parent->bsddev, "Couldn't add lkpi_iic\n");
		sx_xunlock(&lkpi_sx_i2c);
		return (ENXIO);
	}

	bus_topo_lock();
	error = bus_generic_attach(adapter->dev.parent->bsddev);
	bus_topo_unlock();
	if (error) {
		device_printf(adapter->dev.parent->bsddev,
		  "failed to attach child: error %d\n", error);
		sx_xunlock(&lkpi_sx_i2c);
		return (ENXIO);
	}
	LKPI_IIC_ADD_ADAPTER(lkpi_iic, adapter);
	sx_xunlock(&lkpi_sx_i2c);
	return (0);
}

int
lkpi_i2c_del_adapter(struct i2c_adapter *adapter)
{
	device_t child;
	int unit, rv;

	if (adapter == NULL)
		return (-EINVAL);
	if (bootverbose)
		device_printf(adapter->dev.parent->bsddev,
		    "Removing i2c adapter %s\n", adapter->name);
	sx_xlock(&lkpi_sx_i2c);
	unit = 0;
	while ((child = device_find_child(adapter->dev.parent->bsddev, "lkpi_iic", unit++)) != NULL) {

		if (adapter == LKPI_IIC_GET_ADAPTER(child)) {
			bus_topo_lock();
			device_delete_child(adapter->dev.parent->bsddev, child);
			bus_topo_unlock();
			rv = 0;
			goto out;
		}
	}

	unit = 0;
	while ((child = device_find_child(adapter->dev.parent->bsddev, "lkpi_iicbb", unit++)) != NULL) {

		if (adapter == LKPI_IIC_GET_ADAPTER(child)) {
			bus_topo_lock();
			device_delete_child(adapter->dev.parent->bsddev, child);
			bus_topo_unlock();
			rv = 0;
			goto out;
		}
	}
	rv = -EINVAL;
out:
	sx_xunlock(&lkpi_sx_i2c);
	return (rv);
}
