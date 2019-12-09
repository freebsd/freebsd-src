/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Michael Zhilin <mizhka@freebsd.org> All rights reserved.
 * Copyright (c) 2019 Ian Lepore <ian@freebsd.org>
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
 */

/*
 * GPIOTHS - Temp/Humidity sensor over GPIO.
 *
 * This is driver for Temperature & Humidity sensor which provides digital
 * output over single-wire protocol from embedded 8-bit microcontroller.
 * Note that it uses a custom single-wire protocol, it is not 1-wire(tm).
 * 
 * This driver supports the following chips:
 *   DHT11:  Temp   0c to 50c +-2.0c, Humidity 20% to  90% +-5%
 *   DHT12:  Temp -20c to 60c +-0.5c, Humidity 20% to  95% +-5%
 *   DHT21:  Temp -40c to 80c +-0.3c, Humidity  0% to 100% +-3%
 *   DHT22:  Temp -40c to 80c +-0.3c, Humidity  0% to 100% +-2%
 *   AM2301: Same as DHT21, but also supports i2c interface.
 *   AM2302: Same as DHT22, but also supports i2c interface.
 *
 * Temp/Humidity sensor can't be discovered automatically, please specify hints
 * as part of loader or kernel configuration:
 *	hint.gpioths.0.at="gpiobus0"
 *	hint.gpioths.0.pins=<PIN>
 *
 * Or configure via FDT data.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <dev/gpio/gpiobusvar.h>

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

static struct ofw_compat_data compat_data[] = {
	{"dht11",  true},
	{NULL,     false}
};
OFWBUS_PNP_INFO(compat_data);
SIMPLEBUS_PNP_INFO(compat_data);
#endif /* FDT */

#define	PIN_IDX 0			/* Use the first/only configured pin. */

#define	GPIOTHS_POLLTIME	5	/* in seconds */

#define	GPIOTHS_DHT_STARTCYCLE	20000	/* 20ms = 20000us */
#define	GPIOTHS_DHT_TIMEOUT	1000	/* 1ms = 1000us */
#define	GPIOTHS_DHT_CYCLES	41
#define	GPIOTHS_DHT_ONEBYTEMASK	0xFF

struct gpioths_softc {
	device_t		 dev;
	gpio_pin_t		 pin;
	int			 temp;
	int			 hum;
	int			 fails;
	struct timeout_task	 task;
	bool			 detaching;
};

static int
gpioths_probe(device_t dev)
{
	int rv;

	/*
	 * By default we only bid to attach if specifically added by our parent
	 * (usually via hint.gpioths.#.at=busname).  On FDT systems we bid as
	 * the default driver based on being configured in the FDT data.
	 */
	rv = BUS_PROBE_NOWILDCARD;

#ifdef FDT
	if (ofw_bus_status_okay(dev) &&
	    ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		rv = BUS_PROBE_DEFAULT;
#endif

	device_set_desc(dev, "DHT11/DHT22 Temperature and Humidity Sensor");

	return (rv);
}

static int
gpioths_dht_timeuntil(struct gpioths_softc *sc, bool lev, uint32_t *time)
{
	bool		cur_level;
	int		i;

	for (i = 0; i < GPIOTHS_DHT_TIMEOUT; i++) {
		gpio_pin_is_active(sc->pin, &cur_level);
		if (cur_level == lev) {
			if (time != NULL)
				*time = i;
			return (0);
		}
		DELAY(1);
	}

	/* Timeout */
	return (ETIMEDOUT);
}

static void
gpioths_dht_initread(struct gpioths_softc *sc)
{

	/*
	 * According to specifications we need to drive the data line low for at
	 * least 20ms then drive it high, to wake up the chip and signal it to
	 * send a measurement. After sending this start signal, we switch the
	 * pin back to input so the device can begin talking to us.
	 */
	gpio_pin_setflags(sc->pin, GPIO_PIN_OUTPUT);
	gpio_pin_set_active(sc->pin, false);
	pause_sbt("gpioths", ustosbt(GPIOTHS_DHT_STARTCYCLE), C_PREL(2), 0);
	gpio_pin_set_active(sc->pin, true);
	gpio_pin_setflags(sc->pin, GPIO_PIN_INPUT);
}

static int
gpioths_dht_readbytes(struct gpioths_softc *sc)
{
	uint32_t		 calibrations[GPIOTHS_DHT_CYCLES];
	uint32_t		 intervals[GPIOTHS_DHT_CYCLES];
	uint32_t		 err, avglen, value;
	uint8_t			 crc, calc;
	int			 i, negmul, offset, size, tmphi, tmplo;

	gpioths_dht_initread(sc);
	
	err = gpioths_dht_timeuntil(sc, false, NULL);
	if (err) {
		device_printf(sc->dev, "err(START) = %d\n", err);
		goto error;
	}

	/* reading - 41 cycles */
	for (i = 0; i < GPIOTHS_DHT_CYCLES; i++) {
		err = gpioths_dht_timeuntil(sc, true, &calibrations[i]);
		if (err) {
			device_printf(sc->dev, "err(CAL, %d) = %d\n", i, err);
			goto error;
		}
		err = gpioths_dht_timeuntil(sc, false, &intervals[i]);
		if (err) {
			device_printf(sc->dev, "err(INTERVAL, %d) = %d\n", i, err);
			goto error;
		}
	}

	/* Calculate average data calibration cycle length */
	avglen = 0;
	for (i = 1; i < GPIOTHS_DHT_CYCLES; i++)
		avglen += calibrations[i];

	avglen = avglen / (GPIOTHS_DHT_CYCLES - 1);

	/* Calculate data */
	value = 0;
	offset = 1;
	size = sizeof(value) * 8;
	for (i = offset; i < size + offset; i++) {
		value <<= 1;
		if (intervals[i] > avglen)
			value += 1;
	}

	/* Calculate CRC */
	crc = 0;
	offset = sizeof(value) * 8 + 1;
	size = sizeof(crc) * 8;
	for (i = offset;  i < size + offset; i++) {
		crc <<= 1;
		if (intervals[i] > avglen)
			crc += 1;
	}

	calc = 0;
	for (i = 0; i < sizeof(value); i++)
		calc += (value >> (8*i)) & GPIOTHS_DHT_ONEBYTEMASK;

#ifdef GPIOTHS_DEBUG
	/* Debug bits */
	for (i = 0; i < GPIOTHS_DHT_CYCLES; i++)
		device_printf(dev, "%d: %d %d\n", i, calibrations[i],
		    intervals[i]);

	device_printf(dev, "len=%d, data=%x, crc=%x/%x\n", avglen, value, crc,
	    calc);
#endif /* GPIOTHS_DEBUG */

	/* CRC check */
	if (calc != crc) {
		err = -1;
		goto error;
	}

	/*
	 * For DHT11/12, the values are split into 8 bits of integer and 8 bits
	 * of fractional tenths.  On DHT11 the fraction bytes are always zero.
	 * On DHT12 the sign bit is in the high bit of the fraction byte.
	 *  - DHT11: 0HHHHHHH 00000000 00TTTTTT 00000000
	 *  - DHT12: 0HHHHHHH 0000hhhh 00TTTTTT s000tttt
	 *
	 * For DHT21/21, the values are are encoded in 16 bits each, with the
	 * temperature sign bit in the high bit.  The values are tenths of a
	 * degree C and tenths of a percent RH.
	 *  - DHT21: 000000HH HHHHHHHH s00000TT TTTTTTTT
	 *  - DHT22: 000000HH HHHHHHHH s00000TT TTTTTTTT
	 *
	 * For all devices, some bits are always zero because of the range of
	 * values supported by the device.
	 *
	 * We figure out how to decode things based on the high byte of the
	 * humidity.  A DHT21/22 cannot report a value greater than 3 in
	 * the upper bits of its 16-bit humidity.  A DHT11/12 should not report
	 * a value lower than 20.  To allow for the possibility that a device
	 * could report a value slightly out of its sensitivity range, we split
	 * the difference and say if the value is greater than 10 it must be a
	 * DHT11/12 (that would be a humidity over 256% on a DHT21/22).
	 */
#define	DK_OFFSET 2731 /* Offset between K and C, in decikelvins. */
	if ((value >> 24) > 10) {
		/* DHT11 or DHT12 */
		tmphi = (value >> 8) & 0x3f;
		tmplo = value & 0x0f;
		negmul = (value & 0x80) ? -1 : 1;
		sc->temp = DK_OFFSET + (negmul * (tmphi * 10 + tmplo));
		sc->hum = (value >> 24) & 0x7f;
	} else {
                /* DHT21 or DHT22 */
		negmul = (value & 0x8000) ? -1 : 1;
		sc->temp = DK_OFFSET + (negmul * (value & 0x03ff));
		sc->hum = ((value >> 16) & 0x03ff) / 10;
	}

	sc->fails = 0;

#ifdef GPIOTHS_DEBUG
	/* Debug bits */
	device_printf(dev, "fails=%d, temp=%d, hum=%d\n", sc->fails,
	    sc->temp, sc->hum);
#endif /* GPIOTHS_DEBUG */

	return (0);
error:
	sc->fails++;
	return (err);
}

static void
gpioths_poll(void *arg, int pending __unused)
{
	struct gpioths_softc	*sc;

	sc = (struct gpioths_softc *)arg;

	gpioths_dht_readbytes(sc);
	if (!sc->detaching)
		taskqueue_enqueue_timeout_sbt(taskqueue_thread, &sc->task,
		    GPIOTHS_POLLTIME * SBT_1S, 0, C_PREL(3));
}

static int
gpioths_attach(device_t dev)
{
	struct gpioths_softc	*sc;
	struct sysctl_ctx_list	*ctx;
	struct sysctl_oid	*tree;
	int err;

	sc = device_get_softc(dev);
	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);

	sc->dev = dev;

	TIMEOUT_TASK_INIT(taskqueue_thread, &sc->task, 0, gpioths_poll, sc);

#ifdef FDT
	/* Try to configure our pin from fdt data on fdt-based systems. */
	err = gpio_pin_get_by_ofw_idx(dev, ofw_bus_get_node(dev), PIN_IDX,
	    &sc->pin);
#else
	err = ENOENT;
#endif
	/*
	 * If we didn't get configured by fdt data and our parent is gpiobus,
	 * see if we can be configured by the bus (allows hinted attachment even
	 * on fdt-based systems).
	 */
	if (err != 0 &&
	    strcmp("gpiobus", device_get_name(device_get_parent(dev))) == 0)
		err = gpio_pin_get_by_child_index(dev, PIN_IDX, &sc->pin);

	/* If we didn't get configured by either method, whine and punt. */
	if (err != 0) {
		device_printf(sc->dev,
		    "cannot acquire gpio pin (config error)\n");
		return (err);
	}

	/*
	 * Ensure we have control of our pin, and preset the data line to its
	 * idle condition (high).  Leave the line in input mode, relying on the
	 * external pullup to keep the line high while idle.
	 */
	err = gpio_pin_setflags(sc->pin, GPIO_PIN_OUTPUT);
	if (err != 0) {
		device_printf(dev, "gpio_pin_setflags(OUT) = %d\n", err);
		return (err);
	}
	err = gpio_pin_set_active(sc->pin, true);
	if (err != 0) {
		device_printf(dev, "gpio_pin_set_active(false) = %d\n", err);
		return (err);
	}
	err = gpio_pin_setflags(sc->pin, GPIO_PIN_INPUT);
	if (err != 0) {
		device_printf(dev, "gpio_pin_setflags(IN) = %d\n", err);
		return (err);
	}

	/* 
	 * Do an initial read so we have correct values for reporting before
	 * registering the sysctls that can access those values.  This also
	 * schedules the periodic polling the driver does every few seconds to
	 * update the sysctl variables.
	 */
	gpioths_poll(sc, 0);

	sysctl_add_oid(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "temperature",				\
	    CTLFLAG_RD | CTLTYPE_INT | CTLFLAG_MPSAFE,
	    &sc->temp, 0, sysctl_handle_int, "IK", "temperature", NULL);

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "humidity",
	    CTLFLAG_RD, &sc->hum, 0, "relative humidity(%)");

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "fails",
	    CTLFLAG_RD, &sc->fails, 0,
	    "failures since last successful read");

	return (0);
}

static int
gpioths_detach(device_t dev)
{
	struct gpioths_softc	*sc;

	sc = device_get_softc(dev);
	gpio_pin_release(sc->pin);
	sc->detaching = true;
	while (taskqueue_cancel_timeout(taskqueue_thread, &sc->task, NULL) != 0)
		taskqueue_drain_timeout(taskqueue_thread, &sc->task);

	return (0);
}

/* Driver bits */
static device_method_t gpioths_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			gpioths_probe),
	DEVMETHOD(device_attach,		gpioths_attach),
	DEVMETHOD(device_detach,		gpioths_detach),

	DEVMETHOD_END
};

static devclass_t gpioths_devclass;

DEFINE_CLASS_0(gpioths, gpioths_driver, gpioths_methods, sizeof(struct gpioths_softc));

#ifdef FDT
DRIVER_MODULE(gpioths, simplebus, gpioths_driver, gpioths_devclass, 0, 0);
#endif

DRIVER_MODULE(gpioths, gpiobus, gpioths_driver, gpioths_devclass, 0, 0);
MODULE_DEPEND(gpioths, gpiobus, 1, 1, 1);
