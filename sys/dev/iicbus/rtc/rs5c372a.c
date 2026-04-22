/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Justin Hibbits
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

/*
 * Driver for the Richo rs5c372a RTC.  The chip itself includes 2 alarm clocks
 * in addition to the clock component, but this driver offers only the RTC
 * component.
 *
 * Like many other RTCs, this reports the date and time in BCD.
 *
 * The `Hour' register uses bit 5 in a dual role:  In 24-hour time, it's a part
 * of the first digit (0, 1, 2).  In 12-hour time it denotes PM, so 12PM is
 * reported as 0x32, 1PM is 0x21, etc.
 */
#define	RS5C372_REG_SEC		0x0
#define	RS5C372_REG_MIN		0x1
#define	RS5C372_REG_HOUR	0x2
#define	  HOUR_HR_M		  0x1f
#define	  HOUR_PM		  0x20
#define	RS5C372_REG_DOW		0x3
#define	RS5C372_REG_DAY		0x4
#define	RS5C372_REG_MON		0x5
#define	RS5C372_REG_YEAR	0x6
#define	RS5C372_REG_CTRL1	0xe
#define	RS5C372_REG_CTRL2	0xf
#define	  CTRL_PM		  0x20

static struct ofw_compat_data compat_data[] = {
	{ "ricoh,rs5c372a", 1 },
	{ NULL, 0 }
};

static int
rs5c372a_gettime(device_t dev, struct timespec *ts)
{
	struct bcd_clocktime ct = {};
	uint8_t clock_regs[7];
	int err;
	uint8_t ctrl2;
	bool is_12hr = true;

	err = iicdev_readfrom(dev, RS5C372_REG_CTRL2, &ctrl2,
	    sizeof(ctrl2), IIC_WAIT);
	if (err != 0)
		return (err);
	err = iicdev_readfrom(dev, RS5C372_REG_SEC, clock_regs,
	    sizeof(clock_regs), IIC_WAIT);
	if (err != 0)
		return (err);

	if (ctrl2 & CTRL_PM)
		is_12hr = false;
	ct.sec = clock_regs[RS5C372_REG_SEC];
	ct.min = clock_regs[RS5C372_REG_MIN];
	ct.hour = clock_regs[RS5C372_REG_HOUR];
	ct.dow = clock_regs[RS5C372_REG_DOW];
	ct.day = clock_regs[RS5C372_REG_DAY];
	ct.mon = clock_regs[RS5C372_REG_MON];
	ct.year = clock_regs[RS5C372_REG_YEAR];

	if (is_12hr) {
		ct.ispm = ct.hour & HOUR_PM;
		ct.hour &= HOUR_HR_M;
	}
	clock_bcd_to_ts(&ct, ts, ct.ispm);

	return (0);
}

static int
rs5c372a_settime(device_t dev, struct timespec *ts)
{
	struct bcd_clocktime ct;
	uint8_t clock_regs[7];
	uint8_t ctrl2;
	int err;
	bool is_12hr = true;

	err = iicdev_readfrom(dev, RS5C372_REG_CTRL2, &ctrl2,
	    sizeof(ctrl2), IIC_WAIT);
	if (err != 0)
		return (err);
	if (ctrl2 & CTRL_PM)
		is_12hr = false;
	clock_ts_to_bcd(ts, &ct, is_12hr);
	clock_regs[RS5C372_REG_SEC] = ct.sec;
	clock_regs[RS5C372_REG_MIN] = ct.min;
	clock_regs[RS5C372_REG_HOUR] = ct.hour;
	clock_regs[RS5C372_REG_DAY] = ct.day;
	clock_regs[RS5C372_REG_DOW] = ct.dow;
	clock_regs[RS5C372_REG_MON] = ct.mon;
	clock_regs[RS5C372_REG_YEAR] = ct.year & 0xff;

	if (is_12hr) {
		if (ct.ispm)
			clock_regs[RS5C372_REG_HOUR] |= HOUR_PM;
	}

	err = iicdev_writeto(dev, RS5C372_REG_SEC, clock_regs,
	    sizeof(clock_regs), IIC_WAIT);

	return (err);
}

static int
rs5c372a_probe(device_t dev)
{
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Richo RS5C372A RTC");

	return (BUS_PROBE_DEFAULT);
}

static int
rs5c372a_attach(device_t dev)
{

	/* Register with 1s resolution */
	clock_register(dev, 1000000);
	clock_schedule(dev, 1);
	return (0);
}

static device_method_t rs5c372a_methods[] = {
	/* Device methods */
	DEVMETHOD(device_probe,		rs5c372a_probe),
	DEVMETHOD(device_attach,	rs5c372a_attach),

	/* Clock methods */
	DEVMETHOD(clock_gettime,	rs5c372a_gettime),
	DEVMETHOD(clock_settime,	rs5c372a_settime),
	DEVMETHOD_END
};


DEFINE_CLASS_0(rs5c372a, rs5c372a_driver, rs5c372a_methods, 0);
DRIVER_MODULE(rs5c372a, iicbus, rs5c372a_driver, NULL, NULL);
MODULE_VERSION(rs5c372a, 1);
MODULE_DEPEND(rs5c372a, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
IICBUS_FDT_PNP_INFO(compat_data);
