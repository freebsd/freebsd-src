/*-
 * Copyright (c) 2010 Andreas Tobler.
 * Copyright (c) 2013-2014 Luiz Otavio O Souza <loos@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

/* LM75 registers. */
#define	LM75_TEMP	0x0
#define	LM75_CONF	0x1
#define	LM75_CONF_FSHIFT	3
#define	LM75_CONF_FAULT		0x18
#define	LM75_CONF_POL		0x04
#define	LM75_CONF_MODE		0x02
#define	LM75_CONF_SHUTD		0x01
#define	LM75_CONF_MASK		0x1f
#define	LM75_THYST	0x2
#define	LM75_TOS	0x3

/* LM75 constants. */
#define	LM75_TEST_PATTERN	0xa
#define	LM75_MIN_TEMP		-55
#define	LM75_MAX_TEMP		125
#define	TZ_ZEROC		27315
#define	TZ_ZEROC_DIVIDER	100

enum max_resolution{
	BITS_9 = 1,
	BITS_11
};

/* Regular bus attachment functions */
static int  lm75_probe(device_t);
static int  lm75_attach(device_t);

struct lm75_softc {
	device_t		sc_dev;
	struct intr_config_hook enum_hook;
	uint32_t		sc_addr;
	uint32_t		sc_conf;
	uint8_t			sc_resolution;
	uint8_t			sc_max_resolution;
	uint16_t		sc_multiplier;
};

/* Utility functions */
static int  lm75_conf_read(struct lm75_softc *);
static int  lm75_conf_write(struct lm75_softc *);
static int  lm75_temp_read(struct lm75_softc *, uint8_t, int *);
static int  lm75_temp_write(struct lm75_softc *, uint8_t, int);
static void lm75_start(void *);
static int  lm75_read(device_t, uint32_t, uint8_t, uint8_t *, size_t);
static int  lm75_write(device_t, uint32_t, uint8_t *, size_t);
static int  lm75_str_mode(char *);
static int  lm75_str_pol(char *);
static int  lm75_temp_sysctl(SYSCTL_HANDLER_ARGS);
static int  lm75_faults_sysctl(SYSCTL_HANDLER_ARGS);
static int  lm75_mode_sysctl(SYSCTL_HANDLER_ARGS);
static int  lm75_pol_sysctl(SYSCTL_HANDLER_ARGS);
static int  lm75_shutdown_sysctl(SYSCTL_HANDLER_ARGS);
static int  lm75_resolution_sysctl(SYSCTL_HANDLER_ARGS);

static device_method_t  lm75_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lm75_probe),
	DEVMETHOD(device_attach,	lm75_attach),

	DEVMETHOD_END
};

static driver_t lm75_driver = {
	"lm75",
	lm75_methods,
	sizeof(struct lm75_softc)
};

#ifdef FDT
static struct ofw_compat_data compat_data[] = {
	{"national,lm75",	BITS_9},
	{"ti,lm75",		BITS_9},
	{0,0}
};
#endif

DRIVER_MODULE(lm75, iicbus, lm75_driver, 0, 0);

static int
lm75_read(device_t dev, uint32_t addr, uint8_t reg, uint8_t *data, size_t len)
{
	struct iic_msg msg[2] = {
	    { addr, IIC_M_WR | IIC_M_NOSTOP, 1, &reg },
	    { addr, IIC_M_RD, len, data },
	};

	if (iicbus_transfer(dev, msg, nitems(msg)) != 0)
		return (-1);

	return (0);
}

static int
lm75_write(device_t dev, uint32_t addr, uint8_t *data, size_t len)
{
	struct iic_msg msg[1] = {
	    { addr, IIC_M_WR, len, data },
	};

	if (iicbus_transfer(dev, msg, nitems(msg)) != 0)
		return (-1);

	return (0);
}

static int
lm75_probe(device_t dev)
{
#ifdef FDT
	const struct ofw_compat_data *compat_ptr;
#endif
	struct lm75_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_max_resolution = 9;

#ifdef FDT
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	compat_ptr = ofw_bus_search_compatible(dev, compat_data);

	switch (compat_ptr->ocd_data){
	case BITS_9:
		sc->sc_max_resolution = 9;
		break;
	case BITS_11:
		sc->sc_max_resolution = 11;
		break;
	default:
		return (ENXIO);
	}
#endif
	device_set_desc(dev, "LM75 temperature sensor");

	return (BUS_PROBE_GENERIC);
}

static int
lm75_attach(device_t dev)
{
	struct lm75_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_addr = iicbus_get_addr(dev);

	sc->enum_hook.ich_func = lm75_start;
	sc->enum_hook.ich_arg = dev;

	switch (sc->sc_max_resolution) {
	case 9:
		sc->sc_resolution = 9;
		sc->sc_max_resolution = 9;
		sc->sc_multiplier = 10;
		break;
	case 11:
		sc->sc_resolution = 11;
		sc->sc_max_resolution = 11;
		sc->sc_multiplier = 1000;
		break;
	default:
		return (ENXIO);
	}

	/*
	 * We have to wait until interrupts are enabled.  Usually I2C read
	 * and write only works when the interrupts are available.
	 */
	if (config_intrhook_establish(&sc->enum_hook) != 0)
		return (ENOMEM);

	return (0);
}

static int
lm75_type_detect(struct lm75_softc *sc)
{
	int i, lm75a;
	uint8_t buf8;
	uint32_t conf;

	/* Save the contents of the configuration register. */
	if (lm75_conf_read(sc) != 0)
		return (-1);
	conf = sc->sc_conf;

	/*
	 * Just write some pattern at configuration register so we can later
	 * verify.  The test pattern should be pretty harmless.
	 */
	sc->sc_conf = LM75_TEST_PATTERN;
	if (lm75_conf_write(sc) != 0)
		return (-1);

	/*
	 * Read the configuration register again and check for our test
	 * pattern.
	 */
	if (lm75_conf_read(sc) != 0)
		return (-1);
	if (sc->sc_conf != LM75_TEST_PATTERN)
		return (-1);

	/*
	 * Read from nonexistent registers (0x4 ~ 0x6).
	 * LM75A always return 0xff for nonexistent registers.
	 * LM75 will return the last read value - our test pattern written to
	 * configuration register.
	 */
	lm75a = 0;
	for (i = 4; i <= 6; i++) {
		if (lm75_read(sc->sc_dev, sc->sc_addr, i,
		    &buf8, sizeof(buf8)) < 0)
			return (-1);
		if (buf8 != LM75_TEST_PATTERN && buf8 != 0xff)
			return (-1);
		if (buf8 == 0xff)
			lm75a++;
	}
	if (lm75a == 3){
		sc->sc_multiplier = 1000;
		sc->sc_resolution = 11;
		sc->sc_max_resolution = 11;
	}

	/* Restore the configuration register. */
	sc->sc_conf = conf;
	if (lm75_conf_write(sc) != 0)
		return (-1);

	return (0);
}

static void
lm75_start(void *xdev)
{
	device_t dev;
	struct lm75_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;
	char *mult_format;

	dev = (device_t)xdev;
	sc = device_get_softc(dev);
	ctx = device_get_sysctl_ctx(dev);
	tree_node = device_get_sysctl_tree(dev);
	tree = SYSCTL_CHILDREN(tree_node);

	config_intrhook_disestablish(&sc->enum_hook);

	/*
	 * Detect the kind of chip we are attaching to.
	 * This may not work for LM75 clones.
	 */
	if (lm75_type_detect(sc) != 0) {
		device_printf(dev, "cannot detect sensor.\n");
#ifndef FDT
		return;
#endif
	}

	device_printf(dev,"%d bit resolution sensor attached.\n",
			sc->sc_resolution);

	if (sc->sc_multiplier == 1000)
		mult_format = "IK3";
	else
		mult_format = "IK";

	/* Temperature. */
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "temperature",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, LM75_TEMP,
	    lm75_temp_sysctl, mult_format, "Current temperature");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "thyst",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, dev, LM75_THYST,
	    lm75_temp_sysctl, mult_format, "Hysteresis temperature");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "tos",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, dev, LM75_TOS,
	    lm75_temp_sysctl, mult_format, "Overtemperature");

	/* Configuration parameters. */
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "faults",
	    CTLFLAG_RW | CTLTYPE_UINT | CTLFLAG_MPSAFE, dev, 0,
	    lm75_faults_sysctl, "IU", "LM75 fault queue");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "mode",
	    CTLFLAG_RW | CTLTYPE_STRING | CTLFLAG_MPSAFE, dev, 0,
	    lm75_mode_sysctl, "A", "LM75 mode");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "polarity",
	    CTLFLAG_RW | CTLTYPE_STRING | CTLFLAG_MPSAFE, dev, 0,
	    lm75_pol_sysctl, "A", "LM75 OS polarity");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "shutdown",
	    CTLFLAG_RW | CTLTYPE_UINT | CTLFLAG_MPSAFE, dev, 0,
	    lm75_shutdown_sysctl, "IU", "LM75 shutdown");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "resolution",
	    CTLFLAG_RW | CTLTYPE_INT | CTLFLAG_MPSAFE, dev, 0,
	    lm75_resolution_sysctl, "IU", "LM75 resolution");
}

static int
lm75_conf_read(struct lm75_softc *sc)
{
	uint8_t buf8;

	if (lm75_read(sc->sc_dev, sc->sc_addr, LM75_CONF,
	    &buf8, sizeof(buf8)) < 0)
		return (-1);
	sc->sc_conf = (uint32_t)buf8;

	return (0);
}

static int
lm75_conf_write(struct lm75_softc *sc)
{
	uint8_t buf8[2];

	buf8[0] = LM75_CONF;
	buf8[1] = (uint8_t)sc->sc_conf & LM75_CONF_MASK;
	if (lm75_write(sc->sc_dev, sc->sc_addr, buf8, sizeof(buf8)) < 0)
		return (-1);

	return (0);
}

static int
lm75_temp_read(struct lm75_softc *sc, uint8_t reg, int32_t *temp)
{
	int32_t buf;
	uint8_t buf8[2];
	uint8_t resolution = sc->sc_resolution;
	uint16_t multiplier = sc->sc_multiplier;

	if (lm75_read(sc->sc_dev, sc->sc_addr, reg, buf8, sizeof(buf8)) < 0)
		return (-1);

	buf = (int16_t)((buf8[0] << 8) | buf8[1]);
	*temp = ((buf >> (16 - resolution)) * multiplier) >> (resolution - 8);

	*temp += TZ_ZEROC * sc->sc_multiplier / TZ_ZEROC_DIVIDER;

	return (0);
}

static int
lm75_temp_write(struct lm75_softc *sc, uint8_t reg, int32_t temp)
{
	int32_t buf;
	uint8_t buf8[3], resolution = sc->sc_resolution;
	uint16_t multiplier = sc->sc_multiplier;

	temp -= TZ_ZEROC * multiplier / TZ_ZEROC_DIVIDER;
	if (temp > LM75_MAX_TEMP * multiplier)
		temp = LM75_MAX_TEMP * multiplier;
	if (temp < LM75_MIN_TEMP * multiplier)
		temp = LM75_MIN_TEMP * multiplier;

	buf = ((temp << (resolution - 8)) / multiplier) << (16 - resolution);

	buf8[0] = reg;
	buf8[1] = (buf >> 8) & 0xff;
	buf8[2] = buf & 0xff;

	if (lm75_write(sc->sc_dev, sc->sc_addr, buf8, sizeof(buf8)) < 0)
		return (-1);

	return (0);
}

static int
lm75_str_mode(char *buf)
{
	int len, rtrn;

	rtrn = -1;
	len = strlen(buf);
	if (len > 2 && strncasecmp("interrupt", buf, len) == 0)
		rtrn = 1;
	else if (len > 2 && strncasecmp("comparator", buf, len) == 0)
		rtrn = 0;

	return (rtrn);
}

static int
lm75_str_pol(char *buf)
{
	int len, rtrn;

	rtrn = -1;
	len = strlen(buf);
	if (len > 1 && strncasecmp("high", buf, len) == 0)
		rtrn = 1;
	else if (len > 1 && strncasecmp("low", buf, len) == 0)
		rtrn = 0;
	else if (len > 8 && strncasecmp("active-high", buf, len) == 0)
		rtrn = 1;
	else if (len > 8 && strncasecmp("active-low", buf, len) == 0)
		rtrn = 0;

	return (rtrn);
}

static int
lm75_temp_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	int error;
	int32_t temp;
	struct lm75_softc *sc;
	uint8_t reg;

	dev = (device_t)arg1;
	reg = (uint8_t)arg2;
	sc = device_get_softc(dev);

	if (lm75_temp_read(sc, reg, &temp) != 0)
		return (EIO);

	error = sysctl_handle_int(oidp, &temp, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (lm75_temp_write(sc, reg, temp) != 0)
		return (EIO);

	return (error);
}

static int
lm75_faults_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	int lm75_faults[] = { 1, 2, 4, 6 };
	int error, faults, i, newf, tmp;
	struct lm75_softc *sc;

	dev = (device_t)arg1;
	sc = device_get_softc(dev);
	tmp = (sc->sc_conf & LM75_CONF_FAULT) >> LM75_CONF_FSHIFT;
	if (tmp >= nitems(lm75_faults))
		tmp = nitems(lm75_faults) - 1;
	faults = lm75_faults[tmp];

	error = sysctl_handle_int(oidp, &faults, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (faults != lm75_faults[tmp]) {
		newf = 0;
		for (i = 0; i < nitems(lm75_faults); i++)
			if (faults >= lm75_faults[i])
				newf = i;
		sc->sc_conf &= ~LM75_CONF_FAULT;
		sc->sc_conf |= newf << LM75_CONF_FSHIFT;
		if (lm75_conf_write(sc) != 0)
			return (EIO);
	}

	return (error);
}

static int
lm75_mode_sysctl(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	device_t dev;
	int error, mode, newm;
	struct lm75_softc *sc;

	dev = (device_t)arg1;
	sc = device_get_softc(dev);
	if (sc->sc_conf & LM75_CONF_MODE) {
		mode = 1;
		strlcpy(buf, "interrupt", sizeof(buf));
	} else {
		mode = 0;
		strlcpy(buf, "comparator", sizeof(buf));
	}

	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	newm = lm75_str_mode(buf);
	if (newm != -1 && mode != newm) {
		sc->sc_conf &= ~LM75_CONF_MODE;
		if (newm == 1)
			sc->sc_conf |= LM75_CONF_MODE;
		if (lm75_conf_write(sc) != 0)
			return (EIO);
	}

	return (error);
}

static int
lm75_pol_sysctl(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	device_t dev;
	int error, newp, pol;
	struct lm75_softc *sc;

	dev = (device_t)arg1;
	sc = device_get_softc(dev);
	if (sc->sc_conf & LM75_CONF_POL) {
		pol = 1;
		strlcpy(buf, "active-high", sizeof(buf));
	} else {
		pol = 0;
		strlcpy(buf, "active-low", sizeof(buf));
	}

	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	newp = lm75_str_pol(buf);
	if (newp != -1 && pol != newp) {
		sc->sc_conf &= ~LM75_CONF_POL;
		if (newp == 1)
			sc->sc_conf |= LM75_CONF_POL;
		if (lm75_conf_write(sc) != 0)
			return (EIO);
	}

	return (error);
}

static int
lm75_shutdown_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	int error, shutdown, tmp;
	struct lm75_softc *sc;

	dev = (device_t)arg1;
	sc = device_get_softc(dev);
	tmp = shutdown = (sc->sc_conf & LM75_CONF_SHUTD) ? 1 : 0;

	error = sysctl_handle_int(oidp, &shutdown, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (shutdown != tmp) {
		sc->sc_conf &= ~LM75_CONF_SHUTD;
		if (shutdown)
			sc->sc_conf |= LM75_CONF_SHUTD;
		if (lm75_conf_write(sc) != 0)
			return (EIO);
	}

	return (error);
}

static int
lm75_resolution_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	int error;
	struct lm75_softc *sc;
	int resolution;

	dev = (device_t)arg1;
	sc = device_get_softc(dev);
	resolution = sc->sc_resolution;

	error = sysctl_handle_int(oidp, &resolution, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (resolution > sc->sc_max_resolution || resolution < 9)
		return (EINVAL);

	sc->sc_resolution = (uint8_t) resolution;

	return (0);
}
