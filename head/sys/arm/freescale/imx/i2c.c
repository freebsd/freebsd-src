/*-
 * Copyright (C) 2008-2009 Semihalf, Michal Hajduk
 * Copyright (c) 2012, 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Oleksandr Rybalko
 * under sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <sys/lock.h>
#include <sys/mutex.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include "iicbus_if.h"

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#define I2C_ADDR_REG		0x00 /* I2C slave address register */
#define I2C_FDR_REG		0x04 /* I2C frequency divider register */
#define I2C_CONTROL_REG		0x08 /* I2C control register */
#define I2C_STATUS_REG		0x0C /* I2C status register */
#define I2C_DATA_REG		0x10 /* I2C data register */
#define I2C_DFSRR_REG		0x14 /* I2C Digital Filter Sampling rate */

#define I2CCR_MEN		(1 << 7) /* Module enable */
#define I2CCR_MSTA		(1 << 5) /* Master/slave mode */
#define I2CCR_MTX		(1 << 4) /* Transmit/receive mode */
#define I2CCR_TXAK		(1 << 3) /* Transfer acknowledge */
#define I2CCR_RSTA		(1 << 2) /* Repeated START */

#define I2CSR_MCF		(1 << 7) /* Data transfer */
#define I2CSR_MASS		(1 << 6) /* Addressed as a slave */
#define I2CSR_MBB		(1 << 5) /* Bus busy */
#define I2CSR_MAL		(1 << 4) /* Arbitration lost */
#define I2CSR_SRW		(1 << 2) /* Slave read/write */
#define I2CSR_MIF		(1 << 1) /* Module interrupt */
#define I2CSR_RXAK		(1 << 0) /* Received acknowledge */

#define I2C_BAUD_RATE_FAST	0x31
#define I2C_BAUD_RATE_DEF	0x3F
#define I2C_DFSSR_DIV		0x10

#ifdef  DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);		\
		printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

struct i2c_softc {
	device_t		dev;
	device_t		iicbus;
	struct resource		*res;
	struct mtx		mutex;
	int			rid;
	bus_space_handle_t	bsh;
	bus_space_tag_t		bst;
};

static phandle_t i2c_get_node(device_t, device_t);
static int i2c_probe(device_t);
static int i2c_attach(device_t);

static int i2c_repeated_start(device_t, u_char, int);
static int i2c_start(device_t, u_char, int);
static int i2c_stop(device_t);
static int i2c_reset(device_t, u_char, u_char, u_char *);
static int i2c_read(device_t, char *, int, int *, int, int);
static int i2c_write(device_t, const char *, int, int *, int);

static device_method_t i2c_methods[] = {
	DEVMETHOD(device_probe,			i2c_probe),
	DEVMETHOD(device_attach,		i2c_attach),

	/* OFW methods */
	DEVMETHOD(ofw_bus_get_node,		i2c_get_node),

	DEVMETHOD(iicbus_callback,		iicbus_null_callback),
	DEVMETHOD(iicbus_repeated_start,	i2c_repeated_start),
	DEVMETHOD(iicbus_start,			i2c_start),
	DEVMETHOD(iicbus_stop,			i2c_stop),
	DEVMETHOD(iicbus_reset,			i2c_reset),
	DEVMETHOD(iicbus_read,			i2c_read),
	DEVMETHOD(iicbus_write,			i2c_write),
	DEVMETHOD(iicbus_transfer,		iicbus_transfer_gen),

	{ 0, 0 }
};

static driver_t i2c_driver = {
	"iichb",
	i2c_methods,
	sizeof(struct i2c_softc),
};
static devclass_t  i2c_devclass;

DRIVER_MODULE(i2c, simplebus, i2c_driver, i2c_devclass, 0, 0);
DRIVER_MODULE(iicbus, i2c, iicbus_driver, iicbus_devclass, 0, 0);

static phandle_t
i2c_get_node(device_t bus, device_t dev)
{
	/*
	 * Share controller node with iicbus device
	 */
	return ofw_bus_get_node(bus);
}

static __inline void
i2c_write_reg(struct i2c_softc *sc, bus_size_t off, uint8_t val)
{

	bus_space_write_1(sc->bst, sc->bsh, off, val);
}

static __inline uint8_t
i2c_read_reg(struct i2c_softc *sc, bus_size_t off)
{

	return (bus_space_read_1(sc->bst, sc->bsh, off));
}

static __inline void
i2c_flag_set(struct i2c_softc *sc, bus_size_t off, uint8_t mask)
{
	uint8_t status;

	status = i2c_read_reg(sc, off);
	status |= mask;
	i2c_write_reg(sc, off, status);
}

/* Wait for transfer interrupt flag */
static int
wait_for_iif(struct i2c_softc *sc)
{
	int retry;

	retry = 1000;
	while (retry --) {
		if (i2c_read_reg(sc, I2C_STATUS_REG) & I2CSR_MIF)
			return (IIC_NOERR);
		DELAY(10);
	}

	return (IIC_ETIMEOUT);
}

/* Wait for free bus */
static int
wait_for_nibb(struct i2c_softc *sc)
{
	int retry;

	retry = 1000;
	while (retry --) {
		if ((i2c_read_reg(sc, I2C_STATUS_REG) & I2CSR_MBB) == 0)
			return (IIC_NOERR);
		DELAY(10);
	}

	return (IIC_ETIMEOUT);
}

/* Wait for transfer complete+interrupt flag */
static int
wait_for_icf(struct i2c_softc *sc)
{
	int retry;

	retry = 1000;
	while (retry --) {

		if ((i2c_read_reg(sc, I2C_STATUS_REG) &
		    (I2CSR_MCF|I2CSR_MIF)) == (I2CSR_MCF|I2CSR_MIF))
			return (IIC_NOERR);
		DELAY(10);
	}

	return (IIC_ETIMEOUT);
}

static int
i2c_probe(device_t dev)
{
	struct i2c_softc *sc;

	if (!ofw_bus_is_compatible(dev, "fsl,imx-i2c"))
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->rid = 0;

	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rid,
	    RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->bst = rman_get_bustag(sc->res);
	sc->bsh = rman_get_bushandle(sc->res);

	/* Enable I2C */
	i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);
	device_set_desc(dev, "I2C bus controller");

	return (BUS_PROBE_DEFAULT);
}

static int
i2c_attach(device_t dev)
{
	struct i2c_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->rid = 0;

	mtx_init(&sc->mutex, device_get_nameunit(dev), "I2C", MTX_DEF);

	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rid,
	    RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "could not allocate resources");
		mtx_destroy(&sc->mutex);
		return (ENXIO);
	}

	sc->bst = rman_get_bustag(sc->res);
	sc->bsh = rman_get_bushandle(sc->res);

	sc->iicbus = device_add_child(dev, "iicbus", -1);
	if (sc->iicbus == NULL) {
		device_printf(dev, "could not add iicbus child");
		mtx_destroy(&sc->mutex);
		return (ENXIO);
	}

	bus_generic_attach(dev);
	return (IIC_NOERR);
}

static int
i2c_repeated_start(device_t dev, u_char slave, int timeout)
{
	struct i2c_softc *sc;
	int error;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mutex);

	i2c_write_reg(sc, I2C_ADDR_REG, slave);
	if ((i2c_read_reg(sc, I2C_STATUS_REG) & I2CSR_MBB) == 0) {
		mtx_unlock(&sc->mutex);
		return (IIC_EBUSBSY);
	}

	/* Set repeated start condition */
	DELAY(10);
	i2c_flag_set(sc, I2C_CONTROL_REG, I2CCR_RSTA);
	DELAY(10);
	/* Clear status */
	i2c_write_reg(sc, I2C_STATUS_REG, 0x0);
	/* Write target address - LSB is R/W bit */
	i2c_write_reg(sc, I2C_DATA_REG, slave);

	error = wait_for_iif(sc);

	/* Clear status */
	i2c_write_reg(sc, I2C_STATUS_REG, 0x0);

	mtx_unlock(&sc->mutex);

	if (error)
		return (error);

	return (IIC_NOERR);
}

static int
i2c_start(device_t dev, u_char slave, int timeout)
{
	struct i2c_softc *sc;
	int error;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mutex);
	i2c_write_reg(sc, I2C_ADDR_REG, slave);
	if (i2c_read_reg(sc, I2C_STATUS_REG) & I2CSR_MBB) {
		mtx_unlock(&sc->mutex);
		return (IIC_EBUSBSY);
	}

	/* Set start condition */
	i2c_write_reg(sc, I2C_CONTROL_REG,
	    I2CCR_MEN | I2CCR_MSTA | I2CCR_TXAK);
	DELAY(100);
	i2c_write_reg(sc, I2C_CONTROL_REG,
	    I2CCR_MEN | I2CCR_MSTA | I2CCR_MTX | I2CCR_TXAK);
	/* Clear status */
	i2c_write_reg(sc, I2C_STATUS_REG, 0x0);
	/* Write target address - LSB is R/W bit */
	i2c_write_reg(sc, I2C_DATA_REG, slave);

	error = wait_for_iif(sc);

	mtx_unlock(&sc->mutex);
	if (error)
		return (error);

	return (IIC_NOERR);
}


static int
i2c_stop(device_t dev)
{
	struct i2c_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mutex);
	i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN | I2CCR_TXAK);
	DELAY(100);
	/* Reset controller if bus still busy after STOP */
	if (wait_for_nibb(sc) == IIC_ETIMEOUT) {
		i2c_write_reg(sc, I2C_CONTROL_REG, 0);
		DELAY(1000);
		i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN | I2CCR_TXAK);

		i2c_write_reg(sc, I2C_STATUS_REG, 0x0);
	}
	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}

static int
i2c_reset(device_t dev, u_char speed, u_char addr, u_char *oldadr)
{
	struct i2c_softc *sc;
	uint8_t baud_rate;

	sc = device_get_softc(dev);

	switch (speed) {
	case IIC_FAST:
		baud_rate = I2C_BAUD_RATE_FAST;
		break;
	case IIC_SLOW:
	case IIC_UNKNOWN:
	case IIC_FASTEST:
	default:
		baud_rate = I2C_BAUD_RATE_DEF;
		break;
	}

	mtx_lock(&sc->mutex);
	i2c_write_reg(sc, I2C_CONTROL_REG, 0x0);
	i2c_write_reg(sc, I2C_STATUS_REG, 0x0);
	DELAY(1000);

	i2c_write_reg(sc, I2C_FDR_REG, 20);
	i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN);
	DELAY(1000);
	i2c_write_reg(sc, I2C_STATUS_REG, 0x0);
	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}

static int
i2c_read(device_t dev, char *buf, int len, int *read, int last, int delay)
{
	struct i2c_softc *sc;
	int error, reg;

	sc = device_get_softc(dev);
	*read = 0;

	mtx_lock(&sc->mutex);

	if (len) {
		if (len == 1)
			i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN |
			    I2CCR_MSTA | I2CCR_TXAK);

		else
			i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN |
			    I2CCR_MSTA);

		/* dummy read */
		i2c_read_reg(sc, I2C_DATA_REG);
		DELAY(1000);
	}

	while (*read < len) {
		error = wait_for_icf(sc);
		if (error) {
			mtx_unlock(&sc->mutex);
			return (error);
		}
		i2c_write_reg(sc, I2C_STATUS_REG, 0x0);
		if ((*read == len - 2) && last) {
			/* NO ACK on last byte */
			i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN |
			    I2CCR_MSTA | I2CCR_TXAK);
		}

		if ((*read == len - 1) && last) {
			/* Transfer done, remove master bit */
			i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN |
			    I2CCR_TXAK);
		}

		reg = i2c_read_reg(sc, I2C_DATA_REG);
		*buf++ = reg;
		(*read)++;
	}
	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}

static int
i2c_write(device_t dev, const char *buf, int len, int *sent, int timeout)
{
	struct i2c_softc *sc;
	int error;

	sc = device_get_softc(dev);
	*sent = 0;

	mtx_lock(&sc->mutex);
	while (*sent < len) {
		i2c_write_reg(sc, I2C_STATUS_REG, 0x0);
		i2c_write_reg(sc, I2C_DATA_REG, *buf++);

		error = wait_for_iif(sc);
		if (error) {
			mtx_unlock(&sc->mutex);
			return (error);
		}

		(*sent)++;
	}
	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}
