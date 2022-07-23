/*-
 * Copyright (c) 2015 Luiz Otavio O Souza <loos@FreeBSD.org>
 * Copyright (c) 2022 Mathew McBride <matt@traverse.com.au>
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

enum {
	TYPE_DS1307,
	TYPE_MAXIM1307,
	TYPE_MICROCHIP_MCP7491X,
	TYPE_EPSON_RX8035,
	TYPE_COUNT
};

struct ds1307_softc {
	device_t			sc_dev;
	struct intr_config_hook		enum_hook;
	uint32_t			chiptype;
	uint8_t				sc_ctrl;
	bool				sc_use_ampm;
};

static void ds1307_start(void *);

#ifdef FDT
static const struct ofw_compat_data ds1307_compat_data[] = {
	{"dallas,ds1307",		TYPE_DS1307},
	{"maxim,ds1307",		TYPE_MAXIM1307},
	{"microchip,mcp7941x",		TYPE_MICROCHIP_MCP7491X},
	{"epson,rx8035",		TYPE_EPSON_RX8035},
	{ NULL, 0 }
};
#endif

static int
ds1307_read1(device_t dev, uint8_t reg, uint8_t *data)
{

	return (iicdev_readfrom(dev, reg, data, 1, IIC_INTRWAIT));
}

static int
ds1307_write1(device_t dev, uint8_t reg, uint8_t data)
{

	return (iicdev_writeto(dev, reg, &data, 1, IIC_INTRWAIT));
}

static int
ds1307_ctrl_read(struct ds1307_softc *sc)
{
	int error;

	sc->sc_ctrl = 0;
	error = ds1307_read1(sc->sc_dev, DS1307_CONTROL, &sc->sc_ctrl);
	if (error) {
		device_printf(sc->sc_dev, "%s: cannot read from RTC: %d\n",
		    __func__, error);
		return (error);
	}

	return (0);
}

static int
ds1307_ctrl_write(struct ds1307_softc *sc)
{
	int error;
	uint8_t ctrl;

	ctrl = sc->sc_ctrl & DS1307_CTRL_MASK;
	error = ds1307_write1(sc->sc_dev, DS1307_CONTROL, ctrl);
	if (error != 0)
		device_printf(sc->sc_dev, "%s: cannot write to RTC: %d\n",
		    __func__, error);

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
	if (sc->chiptype == TYPE_MICROCHIP_MCP7491X)
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
	if (compat->ocd_str == NULL)
		return (ENXIO);

	switch(compat->ocd_data) {
		case TYPE_DS1307:
			device_set_desc(dev, "Dallas DS1307");
			break;
		case TYPE_MAXIM1307:
			device_set_desc(dev, "Maxim DS1307");
			break;
		case TYPE_MICROCHIP_MCP7491X:
			device_set_desc(dev, "Microchip MCP7491X");
			break;
		case TYPE_EPSON_RX8035:
			device_set_desc(dev, "Epson RX-8035");
			break;
		default:
			device_set_desc(dev, "Unknown DS1307-like device");
			break;
	}
	return (BUS_PROBE_DEFAULT);
#endif

	device_set_desc(dev, "Maxim DS1307 RTC");
	return (BUS_PROBE_NOWILDCARD);
}

static int
ds1307_attach(device_t dev)
{
#ifdef FDT
	const struct ofw_compat_data *compat;
#endif
	struct ds1307_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->enum_hook.ich_func = ds1307_start;
	sc->enum_hook.ich_arg = dev;
#ifdef FDT
	compat = ofw_bus_search_compatible(dev, ds1307_compat_data);
	sc->chiptype = compat->ocd_data;
	/* Unify the chiptypes to DS1307 where possible. */
	if (sc->chiptype == TYPE_MAXIM1307)
		sc->chiptype = TYPE_DS1307;
#else
	sc->chiptype = TYPE_DS1307;
#endif

	/*
	 * We have to wait until interrupts are enabled.  Usually I2C read
	 * and write only works when the interrupts are available.
	 */
	if (config_intrhook_establish(&sc->enum_hook) != 0)
		return (ENOMEM);

	return (0);
}

static int
ds1307_detach(device_t dev)
{

	clock_unregister(dev);
	return (0);
}

static bool
is_epson_time_valid(struct ds1307_softc *sc)
{
	device_t dev;
	int error;
	uint8_t ctrl2;

	dev = sc->sc_dev;

	/*
	 * The RX-8035 single register read is non-standard
	 * Refer to section 8.9.5 of the RX-8035 application manual:
	 * "I2C bus basic transfer format", under "Standard Read Method".
	 * Basically, register to read goes into the top 4 bits.
	 */
	error = ds1307_read1(dev, (RX8035_CTRL_2 << 4), &ctrl2);
	if (error) {
		device_printf(dev, "%s cannot read Control 2 register: %d\n",
		    __func__, error);
		return (false);
	}

	if (ctrl2 & RX8035_CTRL_2_XSTP) {
		device_printf(dev, "Oscillation stop detected (ctrl2=%#02x)\n",
		    ctrl2);
		return (false);
	}

	/*
	 * Power on reset (PON) generally implies oscillation stop,
	 * but catch it as well to be sure.
	 */
	if (ctrl2 & RX8035_CTRL_2_PON) {
		device_printf(dev, "Power-on reset detected (ctrl2=%#02x)\n",
		    ctrl2);
		return (false);
	}

	return (true);
}

static int
mark_epson_time_valid(struct ds1307_softc *sc)
{
	device_t dev;
	int error;
	uint8_t ctrl2;
	uint8_t control_mask;

	dev = sc->sc_dev;

	error = ds1307_read1(dev, (RX8035_CTRL_2 << 4), &ctrl2);
	if (error) {
		device_printf(dev, "%s cannot read Control 2 register: %d\n",
		    __func__, error);
		return (false);
	}

	control_mask = (RX8035_CTRL_2_PON | RX8035_CTRL_2_XSTP | RX8035_CTRL_2_VDET);
	ctrl2 = ctrl2 & ~(control_mask);

	error = ds1307_write1(dev, (RX8035_CTRL_2 << 4), ctrl2);
	if (error) {
		device_printf(dev, "%s cannot write to Control 2 register: %d\n",
		    __func__, error);
		return (false);
	}
	return (true);
}

static bool is_dev_time_valid(struct ds1307_softc *sc)
{
	device_t dev;
	int error;
	uint8_t osc_en;
	uint8_t secs;

	/* Epson RTCs have different control/status registers. */
	if (sc->chiptype == TYPE_EPSON_RX8035)
		return (is_epson_time_valid(sc));

	dev = sc->sc_dev;
	/* Check if the oscillator is disabled. */
	error = ds1307_read1(dev, DS1307_SECS, &secs);
	if (error) {
		device_printf(dev, "%s: cannot read from RTC: %d\n",
		    __func__, error);
		return (false);
	}

	switch (sc->chiptype) {
	case TYPE_MICROCHIP_MCP7491X:
		osc_en = 0x80;
		break;
	default:
		osc_en = 0x00;
		break;
	}
	if (((secs & DS1307_SECS_CH) ^ osc_en) != 0)
		return (false);

	return (true);
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

	config_intrhook_disestablish(&sc->enum_hook);

	if (!is_dev_time_valid(sc))
		device_printf(dev,
		    "WARNING: RTC clock stopped, check the battery.\n");

	/*
	 * Configuration parameters:
	 * square wave output cannot be changed or inhibited on the RX-8035,
	 * so don't present the sysctls there.
	 */
	if (sc->chiptype == TYPE_EPSON_RX8035)
		goto skip_sysctl;

	ctx = device_get_sysctl_ctx(dev);
	tree_node = device_get_sysctl_tree(dev);
	tree = SYSCTL_CHILDREN(tree_node);

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
skip_sysctl:

	/*
	 * Register as a clock with 1 second resolution.  Schedule the
	 * clock_settime() method to be called just after top-of-second;
	 * resetting the time resets top-of-second in the hardware.
	 */
	clock_register_flags(dev, 1000000, CLOCKF_SETTIME_NO_ADJ);
	clock_schedule(dev, 1);
}

static int
ds1307_gettime(device_t dev, struct timespec *ts)
{
	struct bcd_clocktime bct;
	struct ds1307_softc *sc;
	int error;
	uint8_t data[7], hourmask, ampm_mode;

	sc = device_get_softc(dev);
	error = iicdev_readfrom(sc->sc_dev, DS1307_SECS, data, sizeof(data),
	    IIC_INTRWAIT);
	if (error != 0) {
		device_printf(dev, "%s: cannot read from RTC: %d\n",
		    __func__, error);
		return (error);
	}

	if (!is_dev_time_valid(sc)) {
		device_printf(dev, "Device time not valid.\n");
		return (EINVAL);
	}

	/*
	 * If the chip is in AM/PM mode remember that.
	 * The EPSON uses a 1 to signify 24 hour mode, while the DS uses a 0,
	 * in slighly different positions.
	 */
	if (sc->chiptype == TYPE_EPSON_RX8035)
		ampm_mode = !(data[DS1307_HOUR] & RX8035_HOUR_USE_24);
	else
		ampm_mode = data[DS1307_HOUR] & DS1307_HOUR_USE_AMPM;

	if (ampm_mode) {
		sc->sc_use_ampm = true;
		hourmask = DS1307_HOUR_MASK_12HR;
	} else
		hourmask = DS1307_HOUR_MASK_24HR;

	bct.nsec = 0;
	bct.ispm = (data[DS1307_HOUR] & DS1307_HOUR_IS_PM) != 0;
	bct.sec  = data[DS1307_SECS]  & DS1307_SECS_MASK;
	bct.min  = data[DS1307_MINS]  & DS1307_MINS_MASK;
	bct.hour = data[DS1307_HOUR]  & hourmask;
	bct.day  = data[DS1307_DATE]  & DS1307_DATE_MASK;
	bct.mon  = data[DS1307_MONTH] & DS1307_MONTH_MASK;
	bct.year = data[DS1307_YEAR]  & DS1307_YEAR_MASK;

	clock_dbgprint_bcd(sc->sc_dev, CLOCK_DBG_READ, &bct); 
	return (clock_bcd_to_ts(&bct, ts, sc->sc_use_ampm));
}

static int
ds1307_settime(device_t dev, struct timespec *ts)
{
	struct bcd_clocktime bct;
	struct ds1307_softc *sc;
	int error, year;
	uint8_t data[7];
	uint8_t pmflags;

	sc = device_get_softc(dev);

	/*
	 * We request a timespec with no resolution-adjustment.  That also
	 * disables utc adjustment, so apply that ourselves.
	 */
	ts->tv_sec -= utc_offset();
	clock_ts_to_bcd(ts, &bct, sc->sc_use_ampm);
	clock_dbgprint_bcd(sc->sc_dev, CLOCK_DBG_WRITE, &bct);

	/*
	 * If the chip is in AM/PM mode, adjust hour and set flags as needed.
	 * The AM/PM bit polarity and position is different on the EPSON.
	 */
	if (sc->sc_use_ampm) {
		pmflags = (sc->chiptype != TYPE_EPSON_RX8035) ?
				DS1307_HOUR_USE_AMPM : 0;
		if (bct.ispm)
			pmflags |= DS1307_HOUR_IS_PM;

	} else if (sc->chiptype == TYPE_EPSON_RX8035)
		pmflags = RX8035_HOUR_USE_24;
	else
		pmflags = 0;

	data[DS1307_SECS]    = bct.sec;
	data[DS1307_MINS]    = bct.min;
	data[DS1307_HOUR]    = bct.hour | pmflags;
	data[DS1307_DATE]    = bct.day;
	data[DS1307_WEEKDAY] = bct.dow;
	data[DS1307_MONTH]   = bct.mon;
	data[DS1307_YEAR]    = bct.year & 0xff;
	if (sc->chiptype == TYPE_MICROCHIP_MCP7491X) {
		data[DS1307_SECS] |= MCP7941X_SECS_ST;
		data[DS1307_WEEKDAY] |= MCP7941X_WEEKDAY_VBATEN;
		year = bcd2bin(bct.year >> 8) * 100 + bcd2bin(bct.year & 0xff);
		if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
			data[DS1307_MONTH] |= MCP7941X_MONTH_LPYR;
	}

	/* Write the time back to RTC. */
	error = iicdev_writeto(sc->sc_dev, DS1307_SECS, data, sizeof(data),
	    IIC_INTRWAIT);
	if (error != 0)
		device_printf(dev, "%s: cannot write to RTC: %d\n",
		    __func__, error);

	if (sc->chiptype == TYPE_EPSON_RX8035)
		error = mark_epson_time_valid(sc);

	return (error);
}

static device_method_t ds1307_methods[] = {
	DEVMETHOD(device_probe,		ds1307_probe),
	DEVMETHOD(device_attach,	ds1307_attach),
	DEVMETHOD(device_detach,	ds1307_detach),

	DEVMETHOD(clock_gettime,	ds1307_gettime),
	DEVMETHOD(clock_settime,	ds1307_settime),

	DEVMETHOD_END
};

static driver_t ds1307_driver = {
	"ds1307",
	ds1307_methods,
	sizeof(struct ds1307_softc),
};

DRIVER_MODULE(ds1307, iicbus, ds1307_driver, NULL, NULL);
MODULE_VERSION(ds1307, 1);
MODULE_DEPEND(ds1307, iicbus, 1, 1, 1);
IICBUS_FDT_PNP_INFO(ds1307_compat_data);
