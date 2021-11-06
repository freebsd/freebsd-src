/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Andriy Gapon
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

/*
 * Driver for HTU21D and compatible temperature and humidity sensors.
 * Reference documents:
 * - Measurement Specialties HTU21D datasheet,
 * - Sensirion SHT21 datasheet,
 * - Silicon Labs Si7021 datasheet,
 * - HTU2X Serial Number Reading application note,
 * - Sensirion Electronic Identification Code (How to read-out the serial number
 *   of SHT2x) application note.
 */
#define	HTU21_ADDR		0x40

#define	HTU21_GET_TEMP		0xe3
#define	HTU21_GET_HUM		0xe5
#define	HTU21_GET_TEMP_NH	0xf3
#define	HTU21_GET_HUM_NH	0xf5
#define	HTU21_WRITE_CFG		0xe6
#define	HTU21_READ_CFG		0xe7
#define	HTU21_RESET		0xfe

#define	HTU2x_SERIAL0_0		0xfa
#define	HTU2x_SERIAL0_1		0x0f
#define	HTU2x_SERIAL1_0		0xfc
#define	HTU2x_SERIAL1_1		0xc9

struct htu21_softc {
	device_t		sc_dev;
	uint32_t		sc_addr;
	uint8_t			sc_serial[8];
	int			sc_errcount;
	bool			sc_hold;
};

#ifdef FDT
static struct ofw_compat_data compat_data[] = {
	{ "meas,htu21",		true },
	{ NULL,			false }
};
#endif

static uint8_t
calc_crc(uint16_t data)
{
	static const uint16_t polynomial = 0x3100;
	int i;

	for (i = 0; i < 16; i++) {
		int msb_neq = data & 0x8000;

		data <<= 1;
		if (msb_neq)
			data ^= polynomial;
	}
	return (data >> 8);
}

static int
check_crc_16(const uint8_t *data, uint8_t expected)
{
	uint8_t crc;

	crc = calc_crc(((uint16_t)data[0] << 8) | data[1]);
	return (crc == expected);
}

static int
check_crc_8(const uint8_t data, uint8_t expected)
{
	uint8_t crc;

	crc = calc_crc(data);
	return (crc == expected);
}

static int
htu21_get_measurement(device_t dev, uint8_t cmd, uint8_t *data, int count)
{

	struct iic_msg msgs[2];
	struct htu21_softc *sc;
	int error;

	sc = device_get_softc(dev);
	msgs[0].slave = sc->sc_addr;
	msgs[0].flags = IIC_M_WR | IIC_M_NOSTOP;
	msgs[0].len = 1;
	msgs[0].buf = &cmd;

	msgs[1].slave = sc->sc_addr;
	msgs[1].flags = IIC_M_RD;
	msgs[1].len = count;
	msgs[1].buf = data;

	error = iicbus_transfer_excl(dev, msgs, nitems(msgs), IIC_INTRWAIT);
	return (error);
}

static int
htu21_get_measurement_nohold(device_t dev, uint8_t cmd,
    uint8_t *data, int count)
{
	struct iic_msg msgs[2];
	struct htu21_softc *sc;
	int error;
	int i;

	sc = device_get_softc(dev);

	msgs[0].slave = sc->sc_addr;
	msgs[0].flags = IIC_M_WR;
	msgs[0].len = 1;
	msgs[0].buf = &cmd;

	msgs[1].slave = sc->sc_addr;
	msgs[1].flags = IIC_M_RD;
	msgs[1].len = count;
	msgs[1].buf = data;

	error = iicbus_transfer_excl(dev, &msgs[0], 1, IIC_INTRWAIT);
	if (error != 0)
		return (error);

	for (i = 0; i < hz; i++) {
		error = iicbus_transfer_excl(dev, &msgs[1], 1, IIC_INTRWAIT);
		if (error == 0)
			return (0);
		if (error != IIC_ENOACK)
			break;
		pause("htu21", 1);
	}
	return (error);
}

static int
htu21_temp_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct htu21_softc *sc;
	device_t dev;
	uint8_t raw_data[3];
	int error, temp;

	dev = arg1;
	sc = device_get_softc(dev);

	if (req->oldptr != NULL) {
		if (sc->sc_hold)
			error = htu21_get_measurement(dev, HTU21_GET_TEMP,
			    raw_data, nitems(raw_data));
		else
			error = htu21_get_measurement_nohold(dev,
			    HTU21_GET_TEMP_NH, raw_data, nitems(raw_data));

		if (error != 0) {
			return (EIO);
		} else if (!check_crc_16(raw_data, raw_data[2])) {
			temp = -1;
			sc->sc_errcount++;
		} else {
			temp = (((uint16_t)raw_data[0]) << 8) |
			    (raw_data[1] & 0xfc);
			temp = ((temp * 17572) >> 16 ) + 27315 - 4685;
		}
	}

	error = sysctl_handle_int(oidp, &temp, 0, req);
	return (error);
}

static int
htu21_rh_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct htu21_softc *sc;
	device_t dev;
	uint8_t raw_data[3];
	int error, rh;

	dev = arg1;
	sc = device_get_softc(dev);

	if (req->oldptr != NULL) {
		if (sc->sc_hold)
			error = htu21_get_measurement(dev, HTU21_GET_HUM,
			    raw_data, nitems(raw_data));
		else
			error = htu21_get_measurement_nohold(dev,
			    HTU21_GET_HUM_NH, raw_data, nitems(raw_data));

		if (error != 0) {
			return (EIO);
		} else if (!check_crc_16(raw_data, raw_data[2])) {
			rh = -1;
			sc->sc_errcount++;
		} else {
			rh = (((uint16_t)raw_data[0]) << 8) |
			    (raw_data[1] & 0xfc);
			rh = ((rh * 12500) >> 16 ) - 600;
		}
	}

	error = sysctl_handle_int(oidp, &rh, 0, req);
	return (error);
}

static int
htu21_get_cfg(device_t dev, uint8_t *cfg)
{

	struct iic_msg msgs[2];
	struct htu21_softc *sc;
	uint8_t cmd;
	int error;

	sc = device_get_softc(dev);
	cmd = HTU21_READ_CFG;
	msgs[0].slave = sc->sc_addr;
	msgs[0].flags = IIC_M_WR | IIC_M_NOSTOP;
	msgs[0].len = 1;
	msgs[0].buf = &cmd;

	msgs[1].slave = sc->sc_addr;
	msgs[1].flags = IIC_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = cfg;

	error = iicbus_transfer_excl(dev, msgs, nitems(msgs), IIC_INTRWAIT);
	return (error);
}

static int
htu21_set_cfg(device_t dev, uint8_t cfg)
{

	struct iic_msg msg;
	struct htu21_softc *sc;
	uint8_t buf[2];
	int error;

	sc = device_get_softc(dev);
	buf[0] = HTU21_WRITE_CFG;
	buf[1] = cfg;
	msg.slave = sc->sc_addr;
	msg.flags = IIC_M_WR;
	msg.len = 2;
	msg.buf = buf;

	error = iicbus_transfer_excl(dev, &msg, 1, IIC_INTRWAIT);
	return (error);
}

static int
htu21_heater_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct htu21_softc *sc;
	device_t dev;
	uint8_t cfg;
	int error, heater;

	dev = arg1;
	sc = device_get_softc(dev);

	if (req->oldptr != NULL) {
		error = htu21_get_cfg(dev, &cfg);
		if (error != 0)
			return (EIO);
		heater = (cfg & 0x04) != 0;
	}
	error = sysctl_handle_int(oidp, &heater, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	cfg &= ~0x04;
	cfg |= (heater > 0) << 2;
	error = htu21_set_cfg(dev, cfg);
	return (error != 0 ? EIO : 0);
}

static int
htu21_power_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct htu21_softc *sc;
	device_t dev;
	uint8_t cfg;
	int error, power;

	dev = arg1;
	sc = device_get_softc(dev);

	if (req->oldptr != NULL) {
		error = htu21_get_cfg(dev, &cfg);
		if (error != 0)
			return (EIO);
		power = (cfg & 0x40) == 0;
	}
	error = sysctl_handle_int(oidp, &power, 0, req);
	return (error);
}

/*
 * May be incompatible with some chips like SHT21 and Si7021.
 */
static int
htu21_get_serial(device_t dev)
{

	struct iic_msg msgs[2];
	struct htu21_softc *sc;
	uint8_t data[8];
	uint8_t cmd[2];
	int error, cksum_err;
	int i;

	sc = device_get_softc(dev);
	cmd[0] = HTU2x_SERIAL0_0;
	cmd[1] = HTU2x_SERIAL0_1;
	msgs[0].slave = sc->sc_addr;
	msgs[0].flags = IIC_M_WR | IIC_M_NOSTOP;
	msgs[0].len = nitems(cmd);
	msgs[0].buf = cmd;

	msgs[1].slave = sc->sc_addr;
	msgs[1].flags = IIC_M_RD;
	msgs[1].len = nitems(data);
	msgs[1].buf = data;

	error = iicbus_transfer_excl(dev, msgs, nitems(msgs), IIC_INTRWAIT);
	if (error != 0)
		return (EIO);

	cksum_err = 0;
	for (i = 0; i < nitems(data); i += 2) {
		if (!check_crc_8(data[i], data[i + 1]))
			cksum_err = EINVAL;
		sc->sc_serial[2 + i / 2] = data[i];
	}

	cmd[0] = HTU2x_SERIAL1_0;
	cmd[1] = HTU2x_SERIAL1_1;
	msgs[0].slave = sc->sc_addr;
	msgs[0].flags = IIC_M_WR | IIC_M_NOSTOP;
	msgs[0].len = nitems(cmd);
	msgs[0].buf = cmd;

	msgs[1].slave = sc->sc_addr;
	msgs[1].flags = IIC_M_RD;
	msgs[1].len = 6;
	msgs[1].buf = data;

	error = iicbus_transfer_excl(dev, msgs, nitems(msgs), IIC_INTRWAIT);
	if (error != 0)
		return (EIO);

	if (!check_crc_16(&data[0], data[2]))
		cksum_err = EINVAL;
	sc->sc_serial[6] = data[0];
	sc->sc_serial[7] = data[1];

	if (!check_crc_16(&data[3], data[5]))
		cksum_err = EINVAL;
	sc->sc_serial[0] = data[3];
	sc->sc_serial[1] = data[4];

	return (cksum_err);
}

static void
htu21_start(void *arg)
{
	device_t dev;
	struct htu21_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;
	int error;

	sc = arg;
	dev = sc->sc_dev;

	for (int i = 0; i < 5; i++) {
		error = htu21_get_serial(dev);
		if (error == 0)
			break;
	}
	if (error != EIO) {
		device_printf(dev, "serial number: %8D (checksum %scorrect)\n",
		    sc->sc_serial, ":", error == 0 ? "" : "in");
	} else {
		device_printf(dev, "failed to get serial number, err = %d\n",
		    error);
	}

	ctx = device_get_sysctl_ctx(dev);
	tree_node = device_get_sysctl_tree(dev);
	tree = SYSCTL_CHILDREN(tree_node);

	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "temperature",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, 0,
	    htu21_temp_sysctl, "IK2", "Current temperature");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "humidity",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, 0,
	    htu21_rh_sysctl, "I", "Relative humidity in 0.01%% units");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "heater",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, dev, 0,
	    htu21_heater_sysctl, "IU", "Enable built-in heater");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "power",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, 0,
	    htu21_power_sysctl, "IU", "If sensor's power is good");
	SYSCTL_ADD_BOOL(ctx, tree, OID_AUTO, "hold_bus",
	    CTLFLAG_RW, &sc->sc_hold, 0,
	    "Whether device should hold I2C bus while measuring");
	SYSCTL_ADD_INT(ctx, tree, OID_AUTO, "crc_errors",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, &sc->sc_errcount, 0,
	    "Number of checksum errors");
}

static int
htu21_probe(device_t dev)
{
	uint8_t addr;

#ifdef FDT
	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);
#endif
	addr = iicbus_get_addr(dev);
	if (addr != (HTU21_ADDR << 1)) {
		device_printf(dev, "non-standard slave address 0x%02x\n",
		    addr >> 1);
	}

	device_set_desc(dev, "HTU21 temperature and humidity sensor");
	return (BUS_PROBE_GENERIC);
}

static int
htu21_attach(device_t dev)
{
	struct htu21_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_addr = iicbus_get_addr(dev);

	/*
	 * We have to wait until interrupts are enabled.  Usually I2C read
	 * and write only works when the interrupts are available.
	 */
	config_intrhook_oneshot(htu21_start, sc);
	return (0);
}

static int
htu21_detach(device_t dev)
{
	return (0);
}

static device_method_t  htu21_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		htu21_probe),
	DEVMETHOD(device_attach,	htu21_attach),
	DEVMETHOD(device_detach,	htu21_detach),

	DEVMETHOD_END
};

static driver_t htu21_driver = {
	"htu21",
	htu21_methods,
	sizeof(struct htu21_softc)
};

static devclass_t htu21_devclass;

DRIVER_MODULE(htu21, iicbus, htu21_driver, htu21_devclass, 0, 0);
MODULE_DEPEND(htu21, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(htu21, 1);
#ifdef FDT
IICBUS_FDT_PNP_INFO(compat_data);
#endif
