/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ian Lepore.
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

/*
 * Driver for Texas Instruments ADS101x and ADS111x family i2c ADC chips.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/sx.h>
#include <sys/sysctl.h>

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbus_if.h"

/*
 * Chip registers, bit definitions, shifting and masking values.
 */
#define	ADS111x_CONV			0	/* Reg 0: Latest sample (ro) */

#define	ADS111x_CONF			1	/* Reg 1: Config (rw) */
#define	  ADS111x_CONF_OS_SHIFT		15	/* Operational state */
#define	  ADS111x_CONF_MUX_SHIFT	12	/* Input mux setting */
#define	  ADS111x_CONF_GAIN_SHIFT	 9	/* Programmable gain amp */
#define	  ADS111x_CONF_MODE_SHIFT	 8	/* Operational mode */
#define	  ADS111x_CONF_RATE_SHIFT	 5	/* Sample rate */
#define	  ADS111x_CONF_COMP_DISABLE	 3	/* Comparator disable */

#define	ADS111x_LOTHRESH		2	/* Compare lo threshold (rw) */

#define	ADS111x_HITHRESH		3	/* Compare hi threshold (rw) */

/*
 * On config write, the operational-state bit starts a measurement, on read it
 * indicates when the measurement process is complete/idle.
 */
#define	  ADS111x_CONF_MEASURE		(1u << ADS111x_CONF_OS_SHIFT)
#define	  ADS111x_CONF_IDLE		(1u << ADS111x_CONF_OS_SHIFT)

/*
 * The default values for config items that are not per-channel.  Mostly, this
 * turns off the comparator on chips that have that feature, because this driver
 * doesn't support it directly.  However, the user is allowed to enable the
 * comparator and we'll leave it alone if they do.  That allows them connect the
 * alert pin to something and use the feature without any help from this driver.
 */
#define	ADS111x_CONF_DEFAULT    \
    ((1 << ADS111x_CONF_MODE_SHIFT) | ADS111x_CONF_COMP_DISABLE)
#define	ADS111x_CONF_USERMASK   0x001f

/*
 * Per-channel defaults.  The chip only has one control register, and we load
 * per-channel values into it every time we make a measurement on that channel.
 * These are the default values for the control register from the datasheet, for
 * values we maintain on a per-channel basis.
 */
#define	DEFAULT_GAINIDX		2
#define	DEFAULT_RATEIDX		4

/*
 * Full-scale ranges for each available amplifier setting, in microvolts.  The
 * ADS1x13 chips are fixed-range, the other chips contain a programmable gain
 * amplifier, and the full scale range is based on the amplifier setting.
 */
static const u_int fixedranges[8] = {
	2048000, 2048000, 2048000, 2048000, 2048000, 2048000, 2048000, 2048000,
};
static const u_int gainranges[8] = {
	6144000, 4096000, 2048000, 1024000,  512000,  256000,  256000,  256000,
};

/* The highest value for the ADS101x chip is 0x7ff0, for ADS111x it's 0x7fff. */
#define	ADS101x_RANGEDIV	((1 << 15) - 15)
#define	ADS111x_RANGEDIV	((1 << 15) - 1)

/* Samples per second; varies based on chip type. */
static const u_int rates101x[8] = {128, 250, 490, 920, 1600, 2400, 3300, 3300};
static const u_int rates111x[8] = {  8,  16,  32,  64,  128,  250,  475,  860};

struct ads111x_channel {
	u_int gainidx;		/* Amplifier (full-scale range) config index */
	u_int rateidx;		/* Samples per second config index */
	bool  configured;	/* Channel has been configured */
};

#define	ADS111x_MAX_CHANNELS	8

struct ads111x_chipinfo {
	const char	*name;
	const u_int	*rangetab;
	const u_int	*ratetab;
	u_int		numchan;
	int		rangediv;
};

static struct ads111x_chipinfo ads111x_chip_infos[] = {
	{ "ADS1013", fixedranges, rates101x, 1, ADS101x_RANGEDIV },
	{ "ADS1014", gainranges,  rates101x, 1, ADS101x_RANGEDIV },
	{ "ADS1015", gainranges,  rates101x, 8, ADS101x_RANGEDIV },
	{ "ADS1113", fixedranges, rates111x, 1, ADS111x_RANGEDIV },
	{ "ADS1114", gainranges,  rates111x, 1, ADS111x_RANGEDIV },
	{ "ADS1115", gainranges,  rates111x, 8, ADS111x_RANGEDIV },
};

#ifdef FDT
static struct ofw_compat_data compat_data[] = {
	{"ti,ads1013",   (uintptr_t)&ads111x_chip_infos[0]},
	{"ti,ads1014",   (uintptr_t)&ads111x_chip_infos[1]},
	{"ti,ads1015",   (uintptr_t)&ads111x_chip_infos[2]},
	{"ti,ads1113",   (uintptr_t)&ads111x_chip_infos[3]},
	{"ti,ads1114",   (uintptr_t)&ads111x_chip_infos[4]},
	{"ti,ads1115",   (uintptr_t)&ads111x_chip_infos[5]},
	{NULL,           (uintptr_t)NULL},
};
IICBUS_FDT_PNP_INFO(compat_data);
#endif

struct ads111x_softc {
	device_t	dev;
	struct sx	lock;
	int		addr;
	int		cfgword;
	const struct ads111x_chipinfo
			*chipinfo;
	struct ads111x_channel
			channels[ADS111x_MAX_CHANNELS];
};

static int
ads111x_write_2(struct ads111x_softc *sc, int reg, int val) 
{
	uint8_t data[3];
	struct iic_msg msgs[1];
	uint8_t slaveaddr;

	slaveaddr = iicbus_get_addr(sc->dev);

	data[0] = reg;
	be16enc(&data[1], val);

	msgs[0].slave = slaveaddr;
	msgs[0].flags = IIC_M_WR;
	msgs[0].len   = sizeof(data);
	msgs[0].buf   = data;

	return (iicbus_transfer_excl(sc->dev, msgs, nitems(msgs), IIC_WAIT));
}

static int
ads111x_read_2(struct ads111x_softc *sc, int reg, int *val) 
{
	int err;
	uint8_t data[2];

	err = iic2errno(iicdev_readfrom(sc->dev, reg, data, 2, IIC_WAIT));
	if (err == 0)
		*val = (int16_t)be16dec(data);

	return (err);
}

static int
ads111x_sample_voltage(struct ads111x_softc *sc, int channum, int *voltage) 
{
	struct ads111x_channel *chan;
	int err, cfgword, convword, rate, retries, waitns;
	int64_t fsrange;

	chan = &sc->channels[channum];

	/* Ask the chip to do a one-shot measurement of the given channel. */
	cfgword = sc->cfgword |
	    (1 << ADS111x_CONF_OS_SHIFT) |
	    (channum << ADS111x_CONF_MUX_SHIFT) |
	    (chan->gainidx << ADS111x_CONF_GAIN_SHIFT) |
	    (chan->rateidx << ADS111x_CONF_RATE_SHIFT);
	if ((err = ads111x_write_2(sc, ADS111x_CONF, cfgword)) != 0)
		return (err);

	/*
	 * Calculate how long it will take to make the measurement at the
	 * current sampling rate (round up).  The measurement averaging time
	 * ranges from 300us to 125ms, so we yield the cpu while waiting.
	 */
	rate = sc->chipinfo->ratetab[chan->rateidx];
	waitns = (1000000000 + rate - 1) / rate;
	err = pause_sbt("ads111x", nstosbt(waitns), 0, C_PREL(2));
	if (err != 0 && err != EWOULDBLOCK)
		return (err);

	/*
	 * In theory the measurement should be available now; we waited long
	 * enough.  However, the chip times its averaging intervals using an
	 * internal 1 MHz oscillator which likely isn't running at the same rate
	 * as the system clock, so we have to double-check that the measurement
	 * is complete before reading the result.  If it's not ready yet, yield
	 * the cpu again for 5% of the time we originally calculated.
	 *
	 * Unlike most i2c slaves, this device does not auto-increment the
	 * register number on reads, so we can't read both status and
	 * measurement in one operation.
	 */
	for (retries = 5; ; --retries) {
		if (retries == 0)
			return (EWOULDBLOCK);
		if ((err = ads111x_read_2(sc, ADS111x_CONF, &cfgword)) != 0)
			return (err);
		if (cfgword & ADS111x_CONF_IDLE)
			break;
		pause_sbt("ads111x", nstosbt(waitns / 20), 0, C_PREL(2));
	}

	/* Retrieve the sample and convert it to microvolts. */
	if ((err = ads111x_read_2(sc, ADS111x_CONV, &convword)) != 0)
		return (err);
	fsrange = sc->chipinfo->rangetab[chan->gainidx];
	*voltage = (int)((convword * fsrange ) / sc->chipinfo->rangediv);

	return (err);
}

static int
ads111x_sysctl_gainidx(SYSCTL_HANDLER_ARGS)
{
	struct ads111x_softc *sc;
	int chan, err, gainidx;

	sc = arg1;
	chan = arg2;

	gainidx = sc->channels[chan].gainidx;
	err = sysctl_handle_int(oidp, &gainidx, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	if (gainidx < 0 || gainidx > 7)
		return (EINVAL);
	sx_xlock(&sc->lock);
	sc->channels[chan].gainidx = gainidx;
	sx_xunlock(&sc->lock);

	return (err);
}

static int
ads111x_sysctl_rateidx(SYSCTL_HANDLER_ARGS)
{
	struct ads111x_softc *sc;
	int chan, err, rateidx;

	sc = arg1;
	chan = arg2;

	rateidx = sc->channels[chan].rateidx;
	err = sysctl_handle_int(oidp, &rateidx, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	if (rateidx < 0 || rateidx > 7)
		return (EINVAL);
	sx_xlock(&sc->lock);
	sc->channels[chan].rateidx = rateidx;
	sx_xunlock(&sc->lock);

	return (err);
}

static int
ads111x_sysctl_voltage(SYSCTL_HANDLER_ARGS)
{
	struct ads111x_softc *sc;
	int chan, err, voltage;

	sc = arg1;
	chan = arg2;

	if (req->oldptr != NULL) {
		sx_xlock(&sc->lock);
		err = ads111x_sample_voltage(sc, chan, &voltage);
		sx_xunlock(&sc->lock);
		if (err != 0) {
			device_printf(sc->dev,
			    "conversion read failed, error %d\n", err);
			return (err);
		}
	}
	err = sysctl_handle_int(oidp, &voltage, 0, req);
	return (err);
}

static int
ads111x_sysctl_config(SYSCTL_HANDLER_ARGS)
{
	struct ads111x_softc *sc;
	int config, err;

	sc = arg1;
	config = sc->cfgword & ADS111x_CONF_USERMASK;
	err = sysctl_handle_int(oidp, &config, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	sx_xlock(&sc->lock);
	sc->cfgword = config & ADS111x_CONF_USERMASK;
	err = ads111x_write_2(sc, ADS111x_CONF, sc->cfgword);
	sx_xunlock(&sc->lock);

	return (err);
}
static int
ads111x_sysctl_lothresh(SYSCTL_HANDLER_ARGS)
{
	struct ads111x_softc *sc;
	int thresh, err;

	sc = arg1;
	if ((err = ads111x_read_2(sc, ADS111x_LOTHRESH, &thresh)) != 0)
		return (err);
	err = sysctl_handle_int(oidp, &thresh, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	sx_xlock(&sc->lock);
	err = ads111x_write_2(sc, ADS111x_CONF, thresh);
	sx_xunlock(&sc->lock);

	return (err);
}

static int
ads111x_sysctl_hithresh(SYSCTL_HANDLER_ARGS)
{
	struct ads111x_softc *sc;
	int thresh, err;

	sc = arg1;
	if ((err = ads111x_read_2(sc, ADS111x_HITHRESH, &thresh)) != 0)
		return (err);
	err = sysctl_handle_int(oidp, &thresh, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	sx_xlock(&sc->lock);
	err = ads111x_write_2(sc, ADS111x_CONF, thresh);
	sx_xunlock(&sc->lock);

	return (err);
}

static void
ads111x_setup_channel(struct ads111x_softc *sc, int chan, int gainidx, int rateidx)
{
	struct ads111x_channel *c;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *chantree, *devtree;
	char chanstr[4];

	c = &sc->channels[chan];
	c->gainidx = gainidx;
	c->rateidx = rateidx;

	/*
	 *  If setting up the channel for the first time, create channel's
	 *  sysctl entries.  We might have already configured the channel if
	 *  config data for it exists in both FDT and hints.
	 */

	if (c->configured)
		return;

	ctx = device_get_sysctl_ctx(sc->dev);
	devtree = device_get_sysctl_tree(sc->dev);
	snprintf(chanstr, sizeof(chanstr), "%d", chan);
	chantree = SYSCTL_ADD_NODE(ctx, SYSCTL_CHILDREN(devtree), OID_AUTO,
	    chanstr, CTLFLAG_RD, NULL, "channel data");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(chantree), OID_AUTO,
	    "gain_index", CTLTYPE_INT | CTLFLAG_RWTUN, sc, chan,
	    ads111x_sysctl_gainidx, "I", "programmable gain amp setting, 0-7");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(chantree), OID_AUTO,
	    "rate_index", CTLTYPE_INT | CTLFLAG_RWTUN, sc, chan,
	    ads111x_sysctl_rateidx, "I", "sample rate setting, 0-7");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(chantree), OID_AUTO,
	    "voltage", CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_SKIP, sc, chan,
	    ads111x_sysctl_voltage, "I", "sampled voltage in microvolts");

	c->configured = true;
}

static void
ads111x_add_channels(struct ads111x_softc *sc)
{
	const char *name;
	uint32_t chan, gainidx, num_added, rateidx, unit;
	bool found;

#ifdef FDT
	phandle_t child, node;

	/* Configure any channels that have FDT data. */
	num_added = 0;
	node = ofw_bus_get_node(sc->dev);
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if (OF_getencprop(child, "reg", &chan, sizeof(chan)) == -1)
			continue;
		if (chan >= ADS111x_MAX_CHANNELS)
			continue;
		gainidx = DEFAULT_GAINIDX;
		rateidx = DEFAULT_RATEIDX;
		OF_getencprop(child, "ti,gain", &gainidx, sizeof(gainidx));
		OF_getencprop(child, "ti,datarate", &rateidx, sizeof(rateidx));
		ads111x_setup_channel(sc, chan, gainidx, rateidx);
		++num_added;
	}
#else
	num_added = 0;
#endif

	/* Configure any channels that have hint data. */
	name = device_get_name(sc->dev);
	unit = device_get_unit(sc->dev);
	for (chan = 0; chan < sc->chipinfo->numchan; ++chan) {
		found = false;
		gainidx = DEFAULT_GAINIDX;
		rateidx = DEFAULT_RATEIDX;
		if (resource_int_value(name, unit, "gain_index", &gainidx) == 0)
			found = true;
		if (resource_int_value(name, unit, "rate_index", &gainidx) == 0)
			found = true;
		if (found) {
			ads111x_setup_channel(sc, chan, gainidx, rateidx);
			++num_added;
		}
	}

	/* If any channels were configured via FDT or hints, we're done. */
	if (num_added > 0)
		return;

	/*
	 * No channel config; add all possible channels using default values,
	 * and let the user configure the ones they want on the fly via sysctl.
	 */
	for (chan = 0; chan < sc->chipinfo->numchan; ++chan) {
		gainidx = DEFAULT_GAINIDX;
		rateidx = DEFAULT_RATEIDX;
		ads111x_setup_channel(sc, chan, gainidx, rateidx);
	}
}

static const struct ads111x_chipinfo *
ads111x_find_chipinfo(device_t dev)
{
	const struct ads111x_chipinfo *info;
	const char *chiptype;
	int i;

#ifdef FDT
	if (ofw_bus_status_okay(dev)) {
		info = (struct ads111x_chipinfo*)
		    ofw_bus_search_compatible(dev, compat_data)->ocd_data;
		if (info != NULL)
			return (info);
	}
#endif

	/* For hinted devices, we must be told the chip type. */
	chiptype = NULL;
	resource_string_value(device_get_name(dev), device_get_unit(dev),
	    "type", &chiptype);
	if (chiptype != NULL) {
		for (i = 0; i < nitems(ads111x_chip_infos); ++i) {
			info = &ads111x_chip_infos[i];
			if (strcasecmp(chiptype, info->name) == 0)
				return (info);
		}
	}
	return (NULL);
}

static int
ads111x_probe(device_t dev)
{
	const struct ads111x_chipinfo *info;

	info = ads111x_find_chipinfo(dev);
	if (info != NULL) {
		device_set_desc(dev, info->name);
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
ads111x_attach(device_t dev)
{
	struct ads111x_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	int err;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->addr = iicbus_get_addr(dev);
	sc->cfgword = ADS111x_CONF_DEFAULT;

	sc->chipinfo = ads111x_find_chipinfo(sc->dev);
	if (sc->chipinfo == NULL) {
		device_printf(dev,
		    "cannot get chipinfo (but it worked during probe)");
		return (ENXIO);
	}

	/* Set the default chip config. */
	if ((err = ads111x_write_2(sc, ADS111x_CONF, sc->cfgword)) != 0) {
		device_printf(dev, "cannot write chip config register\n");
		return (err);
	}

	/* Add the sysctl handler to set the chip configuration register.  */
	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "config", CTLTYPE_INT | CTLFLAG_RWTUN, sc, 0,
	    ads111x_sysctl_config, "I", "configuration register word");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "lo_thresh", CTLTYPE_INT | CTLFLAG_RWTUN, sc, 0,
	    ads111x_sysctl_lothresh, "I", "comparator low threshold");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "hi_thresh", CTLTYPE_INT | CTLFLAG_RWTUN, sc, 0,
	    ads111x_sysctl_hithresh, "I", "comparator high threshold");

	/* Set up channels based on metadata or default config. */
	ads111x_add_channels(sc);

	sx_init(&sc->lock, "ads111x");

	return (0);
}

static int
ads111x_detach(device_t dev)
{
	struct ads111x_softc *sc;

	sc = device_get_softc(dev);

	sx_destroy(&sc->lock);
	return (0);
}

static device_method_t ads111x_methods[] = {
	DEVMETHOD(device_probe,		ads111x_probe),
	DEVMETHOD(device_attach,	ads111x_attach),
	DEVMETHOD(device_detach,	ads111x_detach),

	DEVMETHOD_END,
};

static driver_t ads111x_driver = {
	"ads111x",
	ads111x_methods,
	sizeof(struct ads111x_softc),
};
static devclass_t ads111x_devclass;

DRIVER_MODULE(ads111x, iicbus, ads111x_driver, ads111x_devclass, NULL, NULL);
MODULE_VERSION(ads111x, 1);
MODULE_DEPEND(ads111x, iicbus, 1, 1, 1);
