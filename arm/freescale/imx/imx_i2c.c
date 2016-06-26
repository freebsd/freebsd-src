/*-
 * Copyright (C) 2008-2009 Semihalf, Michal Hajduk
 * Copyright (c) 2012, 2013 The FreeBSD Foundation
 * Copyright (c) 2015 Ian Lepore <ian@FreeBSD.org>
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

/*
 * I2C driver for Freescale i.MX hardware.
 *
 * Note that the hardware is capable of running as both a master and a slave.
 * This driver currently implements only master-mode operations.
 *
 * This driver supports multi-master i2c busses, by detecting bus arbitration
 * loss and returning IIC_EBUSBSY status.  Notably, it does not do any kind of
 * retries if some other master jumps onto the bus and interrupts one of our
 * transfer cycles resulting in arbitration loss in mid-transfer.  The caller
 * must handle retries in a way that makes sense for the slave being addressed.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/module.h>
#include <sys/resource.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <arm/freescale/imx/imx_ccmvar.h>

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

/*
 * A table of available divisors and the associated coded values to put in the
 * FDR register to achieve that divisor.. There is no algorithmic relationship I
 * can see between divisors and the codes that go into the register.  The table
 * begins and ends with entries that handle insane configuration values.
 */
struct clkdiv {
	u_int divisor;
	u_int regcode;
};
static struct clkdiv clkdiv_table[] = {
        {    0, 0x20 }, {   22, 0x20 }, {   24, 0x21 }, {   26, 0x22 }, 
        {   28, 0x23 }, {   30, 0x00 }, {   32, 0x24 }, {   36, 0x25 }, 
        {   40, 0x26 }, {   42, 0x03 }, {   44, 0x27 }, {   48, 0x28 }, 
        {   52, 0x05 }, {   56, 0x29 }, {   60, 0x06 }, {   64, 0x2a }, 
        {   72, 0x2b }, {   80, 0x2c }, {   88, 0x09 }, {   96, 0x2d }, 
        {  104, 0x0a }, {  112, 0x2e }, {  128, 0x2f }, {  144, 0x0c }, 
        {  160, 0x30 }, {  192, 0x31 }, {  224, 0x32 }, {  240, 0x0f }, 
        {  256, 0x33 }, {  288, 0x10 }, {  320, 0x34 }, {  384, 0x35 }, 
        {  448, 0x36 }, {  480, 0x13 }, {  512, 0x37 }, {  576, 0x14 }, 
        {  640, 0x38 }, {  768, 0x39 }, {  896, 0x3a }, {  960, 0x17 }, 
        { 1024, 0x3b }, { 1152, 0x18 }, { 1280, 0x3c }, { 1536, 0x3d }, 
        { 1792, 0x3e }, { 1920, 0x1b }, { 2048, 0x3f }, { 2304, 0x1c }, 
        { 2560, 0x1d }, { 3072, 0x1e }, { 3840, 0x1f }, {UINT_MAX, 0x1f} 
};

static struct ofw_compat_data compat_data[] = {
	{"fsl,imx6q-i2c",  1},
	{"fsl,imx-i2c",	   1},
	{NULL,             0}
};

struct i2c_softc {
	device_t		dev;
	device_t		iicbus;
	struct resource		*res;
	int			rid;
	sbintime_t		byte_time_sbt;
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

	DEVMETHOD_END
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

	bus_write_1(sc->res, off, val);
}

static __inline uint8_t
i2c_read_reg(struct i2c_softc *sc, bus_size_t off)
{

	return (bus_read_1(sc->res, off));
}

static __inline void
i2c_flag_set(struct i2c_softc *sc, bus_size_t off, uint8_t mask)
{
	uint8_t status;

	status = i2c_read_reg(sc, off);
	status |= mask;
	i2c_write_reg(sc, off, status);
}

/* Wait for bus to become busy or not-busy. */
static int
wait_for_busbusy(struct i2c_softc *sc, int wantbusy)
{
	int retry, srb;

	retry = 1000;
	while (retry --) {
		srb = i2c_read_reg(sc, I2C_STATUS_REG) & I2CSR_MBB;
		if ((srb && wantbusy) || (!srb && !wantbusy))
			return (IIC_NOERR);
		DELAY(1);
	}
	return (IIC_ETIMEOUT);
}

/* Wait for transfer to complete, optionally check RXAK. */
static int
wait_for_xfer(struct i2c_softc *sc, int checkack)
{
	int retry, sr;

	/*
	 * Sleep for about the time it takes to transfer a byte (with precision
	 * set to tolerate 5% oversleep).  We calculate the approximate byte
	 * transfer time when we set the bus speed divisor.  Slaves are allowed
	 * to do clock-stretching so the actual transfer time can be larger, but
	 * this gets the bulk of the waiting out of the way without tying up the
	 * processor the whole time.
	 */
	pause_sbt("imxi2c", sc->byte_time_sbt, sc->byte_time_sbt / 20, 0);

	retry = 10000;
	while (retry --) {
		sr = i2c_read_reg(sc, I2C_STATUS_REG);
		if (sr & I2CSR_MIF) {
                        if (sr & I2CSR_MAL) 
				return (IIC_EBUSERR);
			else if (checkack && (sr & I2CSR_RXAK))
				return (IIC_ENOACK);
			else
				return (IIC_NOERR);
		}
		DELAY(1);
	}
	return (IIC_ETIMEOUT);
}

/*
 * Implement the error handling shown in the state diagram of the imx6 reference
 * manual.  If there was an error, then:
 *  - Clear master mode (MSTA and MTX).
 *  - Wait for the bus to become free or for a timeout to happen.
 *  - Disable the controller.
 */
static int
i2c_error_handler(struct i2c_softc *sc, int error)
{

	if (error != 0) {
		i2c_write_reg(sc, I2C_STATUS_REG, 0);
		i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN);
		wait_for_busbusy(sc, false);
		i2c_write_reg(sc, I2C_CONTROL_REG, 0);
	}
	return (error);
}

static int
i2c_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Freescale i.MX I2C");

	return (BUS_PROBE_DEFAULT);
}

static int
i2c_attach(device_t dev)
{
	struct i2c_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->rid = 0;

	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rid,
	    RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "could not allocate resources");
		return (ENXIO);
	}

	sc->iicbus = device_add_child(dev, "iicbus", -1);
	if (sc->iicbus == NULL) {
		device_printf(dev, "could not add iicbus child");
		return (ENXIO);
	}

	bus_generic_attach(dev);
	return (0);
}

static int
i2c_repeated_start(device_t dev, u_char slave, int timeout)
{
	struct i2c_softc *sc;
	int error;

	sc = device_get_softc(dev);

	if ((i2c_read_reg(sc, I2C_STATUS_REG) & I2CSR_MBB) == 0) {
		return (IIC_EBUSERR);
	}

	/*
	 * Set repeated start condition, delay (per reference manual, min 156nS)
	 * before writing slave address, wait for ack after write.
	 */
	i2c_flag_set(sc, I2C_CONTROL_REG, I2CCR_RSTA);
	DELAY(1);
	i2c_write_reg(sc, I2C_STATUS_REG, 0x0);
	i2c_write_reg(sc, I2C_DATA_REG, slave);
	error = wait_for_xfer(sc, true);
	return (i2c_error_handler(sc, error));
}

static int
i2c_start(device_t dev, u_char slave, int timeout)
{
	struct i2c_softc *sc;
	int error;

	sc = device_get_softc(dev);

	i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN);
	DELAY(10); /* Delay for controller to sample bus state. */
	if (i2c_read_reg(sc, I2C_STATUS_REG) & I2CSR_MBB) {
		return (i2c_error_handler(sc, IIC_EBUSERR));
	}
	i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN | I2CCR_MSTA | I2CCR_MTX);
	if ((error = wait_for_busbusy(sc, true)) != IIC_NOERR)
		return (i2c_error_handler(sc, error));
	i2c_write_reg(sc, I2C_STATUS_REG, 0);
	i2c_write_reg(sc, I2C_DATA_REG, slave);
	error = wait_for_xfer(sc, true);
	return (i2c_error_handler(sc, error));
}

static int
i2c_stop(device_t dev)
{
	struct i2c_softc *sc;

	sc = device_get_softc(dev);

	i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN);
	wait_for_busbusy(sc, false);
	i2c_write_reg(sc, I2C_CONTROL_REG, 0);
	return (IIC_NOERR);
}

static int
i2c_reset(device_t dev, u_char speed, u_char addr, u_char *oldadr)
{
	struct i2c_softc *sc;
	u_int busfreq, div, i, ipgfreq;

	sc = device_get_softc(dev);

	/*
	 * Look up the divisor that gives the nearest speed that doesn't exceed
	 * the configured value for the bus.
	 */
	ipgfreq = imx_ccm_ipg_hz();
	busfreq = IICBUS_GET_FREQUENCY(sc->iicbus, speed);
	div = howmany(ipgfreq, busfreq);
	for (i = 0; i < nitems(clkdiv_table); i++) {
		if (clkdiv_table[i].divisor >= div)
			break;
	}

	/*
	 * Calculate roughly how long it will take to transfer a byte (which
	 * requires 9 clock cycles) at the new bus speed.  This value is used to
	 * pause() while waiting for transfer-complete.  With a 66MHz IPG clock
	 * and the actual i2c bus speeds that leads to, for nominal 100KHz and
	 * 400KHz bus speeds the transfer times are roughly 104uS and 22uS.
	 */
	busfreq = ipgfreq / clkdiv_table[i].divisor;
	sc->byte_time_sbt = SBT_1US * (9000000 / busfreq);

	/*
	 * Disable the controller (do the reset), and set the new clock divisor.
	 */
	i2c_write_reg(sc, I2C_STATUS_REG, 0x0);
	i2c_write_reg(sc, I2C_CONTROL_REG, 0x0);
	i2c_write_reg(sc, I2C_FDR_REG, (uint8_t)clkdiv_table[i].regcode);
	return (IIC_NOERR);
}

static int
i2c_read(device_t dev, char *buf, int len, int *read, int last, int delay)
{
	struct i2c_softc *sc;
	int error, reg;

	sc = device_get_softc(dev);
	*read = 0;

	if (len) {
		if (len == 1)
			i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN |
			    I2CCR_MSTA | I2CCR_TXAK);
		else
			i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN |
			    I2CCR_MSTA);
                /* Dummy read to prime the receiver. */
		i2c_write_reg(sc, I2C_STATUS_REG, 0x0);
		i2c_read_reg(sc, I2C_DATA_REG);
	}

	error = 0;
	*read = 0;
	while (*read < len) {
		if ((error = wait_for_xfer(sc, false)) != IIC_NOERR)
			break;
		i2c_write_reg(sc, I2C_STATUS_REG, 0x0);
		if (last) {
			if (*read == len - 2) {
				/* NO ACK on last byte */
				i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN |
				    I2CCR_MSTA | I2CCR_TXAK);
			} else if (*read == len - 1) {
				/* Transfer done, signal stop. */
				i2c_write_reg(sc, I2C_CONTROL_REG, I2CCR_MEN |
				    I2CCR_TXAK);
				wait_for_busbusy(sc, false);
			}
		}
		reg = i2c_read_reg(sc, I2C_DATA_REG);
		*buf++ = reg;
		(*read)++;
	}

	return (i2c_error_handler(sc, error));
}

static int
i2c_write(device_t dev, const char *buf, int len, int *sent, int timeout)
{
	struct i2c_softc *sc;
	int error;

	sc = device_get_softc(dev);

	error = 0;
	*sent = 0;
	while (*sent < len) {
		i2c_write_reg(sc, I2C_STATUS_REG, 0x0);
		i2c_write_reg(sc, I2C_DATA_REG, *buf++);
		if ((error = wait_for_xfer(sc, true)) != IIC_NOERR)
			break;
		(*sent)++;
	}

	return (i2c_error_handler(sc, error));
}
