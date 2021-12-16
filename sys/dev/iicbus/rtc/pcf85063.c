/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "clock_if.h"
#include "iicbus_if.h"

#define BIT(x)				(1 << (x))

#define PCF85063_CTRL1_REG		0x0
#define PCF85063_TIME_REG		0x4

#define PCF85063_CTRL1_TIME_FORMAT	BIT(1)
#define PCF85063_CTRL1_RTC_CLK_STOP	BIT(5)
#define PCF85063_TIME_REG_OSC_STOP	BIT(7)

#define PCF85063_HALF_OF_SEC_NS		500000000

struct pcf85063_time {
	uint8_t		sec;
	uint8_t		min;
	uint8_t		hour;
	uint8_t		day;
	uint8_t		dow;
	uint8_t		mon;
	uint8_t		year;
};

static int pcf85063_attach(device_t dev);
static int pcf85063_detach(device_t dev);
static int pcf85063_probe(device_t dev);

static int pcf85063_get_time(device_t dev, struct timespec *ts);
static int pcf85063_set_time(device_t dev, struct timespec *ts);

static int pcf85063_check_status(device_t dev);

static struct ofw_compat_data pcf85063_compat_data[] = {
	{ "nxp,pcf85063",		1},
	{ NULL,				0}
};

static int
pcf85063_check_status(device_t dev)
{
	uint8_t flags;
	int error;

	error = iicdev_readfrom(dev, PCF85063_TIME_REG, &flags, 1, IIC_WAIT);
	if (error != 0)
		return (error);

	if (flags & PCF85063_TIME_REG_OSC_STOP) {
		device_printf(dev,
		    "Low voltage flag set, date is incorrect.\n");
		return (ENXIO);
	}

	return (0);
}

static int
pcf85063_attach(device_t dev)
{

	clock_register_flags(dev, 1000000, 0);
	clock_schedule(dev, 1);

	return (0);
}

static int
pcf85063_detach(device_t dev)
{

	clock_unregister(dev);

	return (0);
}

static int
pcf85063_get_time(device_t dev, struct timespec *ts)
{
	struct pcf85063_time data;
	struct bcd_clocktime bcd;
	uint8_t control_reg;
	int error;

	error = pcf85063_check_status(dev);
	if (error != 0)
		return (error);

	/* read hour format (12/24 hour mode) */
	error = iicdev_readfrom(dev, PCF85063_CTRL1_REG, &control_reg,
	    sizeof(uint8_t), IIC_WAIT);
	if (error != 0)
		return (error);

	/* read current date and time */
	error = iicdev_readfrom(dev, PCF85063_TIME_REG, &data,
	    sizeof(struct pcf85063_time), IIC_WAIT);
	if (error != 0)
		return (error);

	bcd.nsec = 0;
	bcd.sec = data.sec & 0x7F;
	bcd.min = data.min & 0x7F;

	if (control_reg & PCF85063_CTRL1_TIME_FORMAT) {
		/* 12 hour mode */
		bcd.hour = data.hour & 0x1F;
		/* Check if hour is pm */
		bcd.ispm = data.hour & 0x20;
	} else {
		/* 24 hour mode */
		bcd.hour = data.hour & 0x3F;
		bcd.ispm = false;
	}

	bcd.dow = (data.dow & 0x7) + 1;
	bcd.day = data.day & 0x3F;
	bcd.mon = data.mon & 0x1F;
	bcd.year = data.year;

	clock_dbgprint_bcd(dev, CLOCK_DBG_READ, &bcd);
	error = clock_bcd_to_ts(&bcd, ts,
	    control_reg & PCF85063_CTRL1_TIME_FORMAT);

	return (error);
}

static int
pcf85063_set_time(device_t dev, struct timespec *ts)
{
	uint8_t time_reg, ctrl_reg;
	struct pcf85063_time data;
	struct bcd_clocktime bcd;
	int error;

	error = iicdev_readfrom(dev, PCF85063_TIME_REG, &ctrl_reg,
	    sizeof(uint8_t), IIC_WAIT);

	ts->tv_sec -= utc_offset();
	clock_ts_to_bcd(ts, &bcd, false);
	clock_dbgprint_bcd(dev, CLOCK_DBG_WRITE, &bcd);

	data.sec = bcd.sec;
	data.min = bcd.min;
	data.hour = bcd.hour;
	data.dow = bcd.dow - 1;
	data.day = bcd.day;
	data.mon = bcd.mon;
	data.year = bcd.year;

	if (ts->tv_nsec > PCF85063_HALF_OF_SEC_NS)
		data.sec++;

	/* disable clock */
	error = iicdev_readfrom(dev, PCF85063_CTRL1_REG, &ctrl_reg,
	    sizeof(uint8_t), IIC_WAIT);
	if (error != 0)
		return (error);

	ctrl_reg |= PCF85063_CTRL1_RTC_CLK_STOP;
	/* Explicitly set 24-hour mode. */
	ctrl_reg &= ~PCF85063_CTRL1_TIME_FORMAT;

	error = iicdev_writeto(dev, PCF85063_CTRL1_REG, &ctrl_reg,
	    sizeof(uint8_t), IIC_WAIT);
	if (error != 0)
		return (error);

	/* clock is disabled now, write time and date */
	error = iicdev_writeto(dev, PCF85063_TIME_REG, &data,
	    sizeof(struct pcf85063_time), IIC_WAIT);
	if (error != 0)
		return (error);

	/* restart clock */
	ctrl_reg &= ~PCF85063_CTRL1_RTC_CLK_STOP;
	error = iicdev_writeto(dev, PCF85063_CTRL1_REG, &ctrl_reg,
	    sizeof(uint8_t), IIC_WAIT);
	if (error != 0)
		return (error);

	/* reset low voltage flag */
	error = iicdev_readfrom(dev, PCF85063_TIME_REG, &time_reg,
	    sizeof(uint8_t), IIC_WAIT);
	if (error != 0)
		return (error);

	time_reg &= ~PCF85063_TIME_REG_OSC_STOP;

	error = iicdev_writeto(dev, PCF85063_TIME_REG, &time_reg,
	    sizeof(uint8_t), IIC_WAIT);

	return (error);
}

static int
pcf85063_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, pcf85063_compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "NXP pcf85063 Real Time Clock");

	return (BUS_PROBE_GENERIC);
}

static device_method_t pcf85063_methods [] = {
	DEVMETHOD(device_attach, pcf85063_attach),
	DEVMETHOD(device_detach, pcf85063_detach),
	DEVMETHOD(device_probe, pcf85063_probe),

	DEVMETHOD(clock_gettime, pcf85063_get_time),
	DEVMETHOD(clock_settime, pcf85063_set_time),

	DEVMETHOD_END
};

static driver_t pcf85063_driver = {
	"pcf85063",
	pcf85063_methods,
	0
};

static devclass_t pcf85063_devclass;

DRIVER_MODULE(pcf85063, iicbus, pcf85063_driver, pcf85063_devclass, NULL, NULL);
IICBUS_FDT_PNP_INFO(pcf85063_compat_data);
