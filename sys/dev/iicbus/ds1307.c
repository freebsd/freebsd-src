/*-
 * Copyright (c) 2015 Luiz Otavio O Souza <loos@FreeBSD.org>
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for Maxim DS1307 I2C real-time clock/calendar.
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>
#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include <dev/iicbus/ds1307reg.h>

#include "clock_if.h"
#include "iicbus_if.h"

struct ds1307_softc {
	device_t	sc_dev;
	int		sc_year0;
	struct intr_config_hook	enum_hook;
	uint16_t	sc_addr;	/* DS1307 slave address. */
	uint8_t		sc_ctrl;
	int		sc_mcp7941x;
};

static void ds1307_start(void *);

#ifdef FDT
static const struct ofw_compat_data ds1307_compat_data[] = {
    {"dallas,ds1307", (uintptr_t)"Maxim DS1307 RTC"},
    {"maxim,ds1307", (uintptr_t)"Maxim DS1307 RTC"},
    {"microchip,mcp7941x", (uintptr_t)"Microchip MCP7941x RTC"},
    { NULL, 0 }
};
#endif

static int
ds1307_read(device_t dev, uint16_t addr, uint8_t reg, uint8_t *data, size_t len)
{
	struct iic_msg msg[2] = {
	    { addr, IIC_M_WR | IIC_M_NOSTOP, 1, &reg },
	    { addr, IIC_M_RD, len, data },
	};

	return (iicbus_transfer(dev, msg, nitems(msg)));
}

static int
ds1307_write(device_t dev, uint16_t addr, uint8_t *data, size_t len)
{
	struct iic_msg msg[1] = {
	    { addr, IIC_M_WR, len, data },
	};

	return (iicbus_transfer(dev, msg, nitems(msg)));
}

static int
ds1307_ctrl_read(struct ds1307_softc *sc)
{
	int error;

	sc->sc_ctrl = 0;
	error = ds1307_read(sc->sc_dev, sc->sc_addr, DS1307_CONTROL,
	    &sc->sc_ctrl, sizeof(sc->sc_ctrl));
	if (error) {
		device_printf(sc->sc_dev, "cannot read from RTC.\n");
		return (error);
	}

	return (0);
}

static int
ds1307_ctrl_write(struct ds1307_softc *sc)
{
	int error;
	uint8_t data[2];

	data[0] = DS1307_CONTROL;
	data[1] = sc->sc_ctrl & DS1307_CTRL_MASK;
	error = ds1307_write(sc->sc_dev, sc->sc_addr, data, sizeof(data));
	if (error != 0)
		device_printf(sc->sc_dev, "cannot write to RTC.\n");

	return (error);
}

static int
ds1307_osc_enable(struct ds1307_softc *sc)
{
	int error;
	uint8_t data[2], secs;

	secs = 0;
	error = ds1307_read(sc->sc_dev, sc->sc_addr, DS1307_SECS,
	    &secs, sizeof(secs));
	if (error) {
		device_printf(sc->sc_dev, "cannot read from RTC.\n");
		return (error);
	}
	/* Check if the oscillator is disabled. */
	if ((secs & DS1307_SECS_CH) == 0)
		return (0);
	device_printf(sc->sc_dev, "clock was halted, check the battery.\n");
	data[0] = DS1307_SECS;
	data[1] = secs & DS1307_SECS_MASK;
	error = ds1307_write(sc->sc_dev, sc->sc_addr, data, sizeof(data));
	if (error != 0)
		device_printf(sc->sc_dev, "cannot write to RTC.\n");

	return (error);
}

static int
ds1307_set_24hrs_mode(struct ds1307_softc *sc)
{
	int error;
	uint8_t data[2], hour;

	hour = 0;
	error = ds1307_read(sc->sc_dev, sc->sc_addr, DS1307_HOUR,
	    &hour, sizeof(hour));
	if (error) {
		device_printf(sc->sc_dev, "cannot read from RTC.\n");
		return (error);
	}
	data[0] = DS1307_HOUR;
	data[1] = hour & DS1307_HOUR_MASK;
	error = ds1307_write(sc->sc_dev, sc->sc_addr, data, sizeof(data));
	if (error != 0)
		device_printf(sc->sc_dev, "cannot write to RTC.\n");

	return (error);
}

static int
ds1307_sqwe_sysctl(SYSCTL_HANDLER_ARGS)
{
	int sqwe, error, newv, sqwe_bit;
	struct ds1307_softc *sc;

	sc = (struct ds1307_softc *)arg1;
	error = ds1307_ctrl_read(sc);
	if (error != 0)
		return (error);
	if (sc->sc_mcp7941x)
		sqwe_bit = MCP7941X_CTRL_SQWE;
	else
		sqwe_bit = DS1307_CTRL_SQWE;
	sqwe = newv = (sc->sc_ctrl & sqwe_bit) ? 1 : 0;
	error = sysctl_handle_int(oidp, &newv, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (sqwe != newv) {
		sc->sc_ctrl &= ~sqwe_bit;
		if (newv)
			sc->sc_ctrl |= sqwe_bit;
		error = ds1307_ctrl_write(sc);
		if (error != 0)
			return (error);
	}

	return (error);
}

static int
ds1307_sqw_freq_sysctl(SYSCTL_HANDLER_ARGS)
{
	int ds1307_sqw_freq[] = { 1, 4096, 8192, 32768 };
	int error, freq, i, newf, tmp;
	struct ds1307_softc *sc;

	sc = (struct ds1307_softc *)arg1;
	error = ds1307_ctrl_read(sc);
	if (error != 0)
		return (error);
	tmp = (sc->sc_ctrl & DS1307_CTRL_RS_MASK);
	if (tmp >= nitems(ds1307_sqw_freq))
		tmp = nitems(ds1307_sqw_freq) - 1;
	freq = ds1307_sqw_freq[tmp];
	error = sysctl_handle_int(oidp, &freq, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (freq != ds1307_sqw_freq[tmp]) {
		newf = 0;
		for (i = 0; i < nitems(ds1307_sqw_freq); i++)
			if (freq >= ds1307_sqw_freq[i])
				newf = i;
		sc->sc_ctrl &= ~DS1307_CTRL_RS_MASK;
		sc->sc_ctrl |= newf;
		error = ds1307_ctrl_write(sc);
		if (error != 0)
			return (error);
	}

	return (error);
}

static int
ds1307_sqw_out_sysctl(SYSCTL_HANDLER_ARGS)
{
	int sqwe, error, newv;
	struct ds1307_softc *sc;

	sc = (struct ds1307_softc *)arg1;
	error = ds1307_ctrl_read(sc);
	if (error != 0)
		return (error);
	sqwe = newv = (sc->sc_ctrl & DS1307_CTRL_OUT) ? 1 : 0;
	error = sysctl_handle_int(oidp, &newv, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (sqwe != newv) {
		sc->sc_ctrl &= ~DS1307_CTRL_OUT;
		if (newv)
			sc->sc_ctrl |= DS1307_CTRL_OUT;
		error = ds1307_ctrl_write(sc);
		if (error != 0)
			return (error);
	}

	return (error);
}

static int
ds1307_probe(device_t dev)
{
#ifdef FDT
	const struct ofw_compat_data *compat;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	compat = ofw_bus_search_compatible(dev, ds1307_compat_data);

	if (compat == NULL)
		return (ENXIO);

	device_set_desc(dev, (const char *)compat->ocd_data);

	return (BUS_PROBE_DEFAULT);
#else
	device_set_desc(dev, "Maxim DS1307 RTC");

	return (BUS_PROBE_DEFAULT);
#endif
}

static int
ds1307_attach(device_t dev)
{
	struct ds1307_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_addr = iicbus_get_addr(dev);
	sc->sc_year0 = 1900;
	sc->enum_hook.ich_func = ds1307_start;
	sc->enum_hook.ich_arg = dev;

	if (ofw_bus_is_compatible(dev, "microchip,mcp7941x"))
		sc->sc_mcp7941x = 1;

	/*
	 * We have to wait until interrupts are enabled.  Usually I2C read
	 * and write only works when the interrupts are available.
	 */
	if (config_intrhook_establish(&sc->enum_hook) != 0)
		return (ENOMEM);

	return (0);
}

static void
ds1307_start(void *xdev)
{
	device_t dev;
	struct ds1307_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;

	dev = (device_t)xdev;
	sc = device_get_softc(dev);
	ctx = device_get_sysctl_ctx(dev);
	tree_node = device_get_sysctl_tree(dev);
	tree = SYSCTL_CHILDREN(tree_node);

	config_intrhook_disestablish(&sc->enum_hook);
	/* Set the 24 hours mode. */
	if (ds1307_set_24hrs_mode(sc) != 0)
		return;
	/* Enable the oscillator if halted. */
	if (ds1307_osc_enable(sc) != 0)
		return;

	/* Configuration parameters. */
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "sqwe",
	    CTLFLAG_RW | CTLTYPE_UINT | CTLFLAG_MPSAFE, sc, 0,
	    ds1307_sqwe_sysctl, "IU", "DS1307 square-wave enable");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "sqw_freq",
	    CTLFLAG_RW | CTLTYPE_UINT | CTLFLAG_MPSAFE, sc, 0,
	    ds1307_sqw_freq_sysctl, "IU",
	    "DS1307 square-wave output frequency");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "sqw_out",
	    CTLFLAG_RW | CTLTYPE_UINT | CTLFLAG_MPSAFE, sc, 0,
	    ds1307_sqw_out_sysctl, "IU", "DS1307 square-wave output state");

	/* 1 second resolution. */
	clock_register(dev, 1000000);
}

static int
ds1307_gettime(device_t dev, struct timespec *ts)
{
	int error;
	struct clocktime ct;
	struct ds1307_softc *sc;
	uint8_t data[7];

	sc = device_get_softc(dev);
	memset(data, 0, sizeof(data));
	error = ds1307_read(sc->sc_dev, sc->sc_addr, DS1307_SECS,
	    data, sizeof(data)); 
	if (error != 0) {
		device_printf(dev, "cannot read from RTC.\n");
		return (error);
	}
	ct.nsec = 0;
	ct.sec = FROMBCD(data[DS1307_SECS] & DS1307_SECS_MASK);
	ct.min = FROMBCD(data[DS1307_MINS] & DS1307_MINS_MASK);
	ct.hour = FROMBCD(data[DS1307_HOUR] & DS1307_HOUR_MASK);
	ct.day = FROMBCD(data[DS1307_DATE] & DS1307_DATE_MASK);
	ct.dow = data[DS1307_WEEKDAY] & DS1307_WEEKDAY_MASK;
	ct.mon = FROMBCD(data[DS1307_MONTH] & DS1307_MONTH_MASK);
	ct.year = FROMBCD(data[DS1307_YEAR] & DS1307_YEAR_MASK);
	ct.year += sc->sc_year0;
	if (ct.year < POSIX_BASE_YEAR)
		ct.year += 100;	/* assume [1970, 2069] */

	return (clock_ct_to_ts(&ct, ts));
}

static int
ds1307_settime(device_t dev, struct timespec *ts)
{
	int error;
	struct clocktime ct;
	struct ds1307_softc *sc;
	uint8_t data[8];

	sc = device_get_softc(dev);
	/* Accuracy is only one second. */
	if (ts->tv_nsec >= 500000000)
		ts->tv_sec++;
	ts->tv_nsec = 0;
	clock_ts_to_ct(ts, &ct);
	memset(data, 0, sizeof(data));
	data[0] = DS1307_SECS;
	data[DS1307_SECS + 1] = TOBCD(ct.sec);
	data[DS1307_MINS + 1] = TOBCD(ct.min);
	data[DS1307_HOUR + 1] = TOBCD(ct.hour);
	data[DS1307_DATE + 1] = TOBCD(ct.day);
	data[DS1307_WEEKDAY + 1] = ct.dow;
	data[DS1307_MONTH + 1] = TOBCD(ct.mon);
	data[DS1307_YEAR + 1] = TOBCD(ct.year % 100);
	/* Write the time back to RTC. */
	error = ds1307_write(dev, sc->sc_addr, data, sizeof(data));
	if (error != 0)
		device_printf(dev, "cannot write to RTC.\n");

	return (error);
}

static device_method_t ds1307_methods[] = {
	DEVMETHOD(device_probe,		ds1307_probe),
	DEVMETHOD(device_attach,	ds1307_attach),

	DEVMETHOD(clock_gettime,	ds1307_gettime),
	DEVMETHOD(clock_settime,	ds1307_settime),

	DEVMETHOD_END
};

static driver_t ds1307_driver = {
	"ds1307",
	ds1307_methods,
	sizeof(struct ds1307_softc),
};

static devclass_t ds1307_devclass;

DRIVER_MODULE(ds1307, iicbus, ds1307_driver, ds1307_devclass, NULL, NULL);
MODULE_VERSION(ds1307, 1);
MODULE_DEPEND(ds1307, iicbus, 1, 1, 1);
