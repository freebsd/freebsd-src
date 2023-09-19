/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2012 Broadcom Corporation
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY BROADCOM ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL BROADCOM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include "iicbus_if.h"
#include "iicoc.h"

DRIVER_MODULE(iicbus, iicoc, iicbus_driver, 0, 0);

static void
iicoc_dev_write(device_t dev, int reg, int value)
{
	struct iicoc_softc *sc;

	sc = device_get_softc(dev);
	bus_write_1(sc->mem_res, reg<<sc->reg_shift, value);
}

static int
iicoc_dev_read(device_t dev, int reg)
{
	uint8_t val;
	struct iicoc_softc *sc;

	sc = device_get_softc(dev);
	val = bus_read_1(sc->mem_res, reg<<sc->reg_shift);
	return (val);
}

static int
iicoc_wait_on_status(device_t dev, uint8_t bit)
{
	int tries = I2C_TIMEOUT;
	uint8_t status;

	do {
		status = iicoc_dev_read(dev, OC_I2C_STATUS_REG);
	} while ((status & bit) != 0 && --tries > 0);

	return (tries == 0 ? -1: 0);
}

static int
iicoc_rd_cmd(device_t dev, uint8_t cmd)
{
	uint8_t data;

	iicoc_dev_write(dev, OC_I2C_CMD_REG, cmd);
	if (iicoc_wait_on_status(dev, OC_STATUS_TIP) < 0) {
		device_printf(dev, "read: Timeout waiting for TIP clear.\n");
		return (-1);
	}
	data = iicoc_dev_read(dev, OC_I2C_DATA_REG);
	return (data);
}

static int
iicoc_wr_cmd(device_t dev, uint8_t data, uint8_t cmd)
{

	iicoc_dev_write(dev, OC_I2C_DATA_REG, data);
	iicoc_dev_write(dev, OC_I2C_CMD_REG, cmd);
	if (iicoc_wait_on_status(dev, OC_STATUS_TIP) < 0) {
		device_printf(dev, "write: Timeout waiting for TIP clear.\n");
		return (-1);
	}
	return (0);
}

static int
iicoc_wr_ack_cmd(device_t dev, uint8_t data, uint8_t cmd)
{

	if (iicoc_wr_cmd(dev, data, cmd) < 0)
		return (-1);

	if (iicoc_dev_read(dev, OC_I2C_STATUS_REG) & OC_STATUS_NACK) {
		device_printf(dev, "write: I2C command ACK Error.\n");
		return (IIC_ENOACK);
	}
	return (0);
}

int
iicoc_init(device_t dev)
{
	struct iicoc_softc *sc;
	int value;

	sc = device_get_softc(dev);
	value = iicoc_dev_read(dev, OC_I2C_CTRL_REG);
	iicoc_dev_write(dev, OC_I2C_CTRL_REG,
	    value & ~(OC_CONTROL_EN | OC_CONTROL_IEN));
	value = (sc->clockfreq/(5 * sc->i2cfreq)) - 1;
	iicoc_dev_write(dev, OC_I2C_PRESCALE_LO_REG, value & 0xff);
	iicoc_dev_write(dev, OC_I2C_PRESCALE_HI_REG, value >> 8);
	value = iicoc_dev_read(dev, OC_I2C_CTRL_REG);
	iicoc_dev_write(dev, OC_I2C_CTRL_REG, value | OC_CONTROL_EN);

	value = iicoc_dev_read(dev, OC_I2C_CTRL_REG);
	/* return 0 on success, 1 on error */
	return ((value & OC_CONTROL_EN) == 0);
}

static int
iicoc_iicbus_start_common(device_t dev, u_char slave, int timeout, bool repeat)
{
	int error = IIC_EBUSERR;
	struct iicoc_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->sc_mtx);
	sc->i2cdev_addr = (slave >> 1);

	/* Verify the bus is idle */
	if (!repeat && iicoc_wait_on_status(dev, OC_STATUS_BUSY) < 0)
		goto i2c_stx_error;

	/* Write Slave Address */
	if (iicoc_wr_ack_cmd(dev, slave, OC_COMMAND_START)) {
		device_printf(dev,
		    "I2C write slave address [0x%x] failed.\n", slave);
		error = IIC_ENOACK;
		goto i2c_stx_error;
	}

	/* Verify Arbitration is not Lost */
	if (iicoc_dev_read(dev, OC_I2C_STATUS_REG) & OC_STATUS_AL) {
		device_printf(dev, "I2C Bus Arbitration Lost, Aborting.\n");
		error = IIC_EBUSERR;
		goto i2c_stx_error;
	}
	error = IIC_NOERR;
	mtx_unlock(&sc->sc_mtx);
	return (error);

i2c_stx_error:
	iicoc_dev_write(dev, OC_I2C_CMD_REG, OC_COMMAND_STOP);
	iicoc_wait_on_status(dev, OC_STATUS_BUSY);  /* wait for idle */
	mtx_unlock(&sc->sc_mtx);
	return (error);
}

int
iicoc_iicbus_start(device_t dev, u_char slave, int timeout)
{

	return (iicoc_iicbus_start_common(dev, slave, timeout, false));
}

int
iicoc_iicbus_repeated_start(device_t dev, u_char slave, int timeout)
{

	return (iicoc_iicbus_start_common(dev, slave, timeout, true));
}

int
iicoc_iicbus_stop(device_t dev)
{
	int error = 0;
	struct iicoc_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->sc_mtx);
	iicoc_dev_write(dev, OC_I2C_CMD_REG, OC_COMMAND_STOP);
	iicoc_wait_on_status(dev, OC_STATUS_BUSY);  /* wait for idle */
	mtx_unlock(&sc->sc_mtx);
	return (error);
}

int
iicoc_iicbus_write(device_t dev, const char *buf, int len, int *sent,
    int timeout)
{
	uint8_t value;
	int i;

	value = buf[0];
	/* Write Slave Offset */
	if (iicoc_wr_ack_cmd(dev, value, OC_COMMAND_WRITE)) {
		device_printf(dev, "I2C write slave offset failed.\n");
		goto i2c_tx_error;
	}

	for (i = 1; i < len; i++) {
		/* Write data byte */
		value = buf[i];
		if (iicoc_wr_cmd(dev, value, OC_COMMAND_WRITE)) {
			device_printf(dev, "I2C write data byte %d failed.\n",
			    i);
			goto i2c_tx_error;
		}
	}
	*sent = len;
	return (IIC_NOERR);

i2c_tx_error:
	return (IIC_EBUSERR);
}

int
iicoc_iicbus_read(device_t dev, char *buf, int len, int *read, int last,
    int delay)
{
	int data, i;
	uint8_t cmd;

	for (i = 0; i < len; i++) {
		/* Read data byte */
		cmd = (i == len - 1) ? OC_COMMAND_RDNACK : OC_COMMAND_READ;
		data = iicoc_rd_cmd(dev, cmd);
		if (data < 0) {
			device_printf(dev,
			    "I2C read data byte %d failed.\n", i);
			goto i2c_rx_error;
		}
		buf[i] = (uint8_t)data;
	}

	*read = len;
	return (IIC_NOERR);

i2c_rx_error:
	return (IIC_EBUSERR);
}

int
iicoc_iicbus_reset(device_t dev, u_char speed, u_char addr, u_char *oldadr)
{
	int error;
	struct iicoc_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->sc_mtx);
	error = iicoc_init(dev);
	mtx_unlock(&sc->sc_mtx);
	return (error);
}
