/*-
 * Copyright (c) 2017 Hiroki Mori.  All rights reserved.
 * Copyright (c) 2017 Ian Lepore.  All rights reserved.
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
 *
 * This code base on isl12xx.c
 */

#include <sys/cdefs.h>
/*
 * Driver for realtime clock HAOYU HYM8563
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>

#if defined(FDT) && !defined(__powerpc64__) 
#include <dev/clk/clk.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "clock_if.h"
#include "iicbus_if.h"

/* Registers */
#define	HYM8563_CTRL1		0x00
#define	 HYM8563_CTRL1_TEST		(1 << 7)
#define	 HYM8563_CTRL1_STOP		(1 << 5)
#define	 HYM8563_CTRL1_TESTC		(1 << 3)

#define	HYM8563_CTRL2		0x01
#define	 HYM8563_CTRL2_TI_TP		(1 << 4)
#define	 HYM8563_CTRL2_AF		(1 << 3)
#define	 HYM8563_CTRL2_TF		(1 << 2)
#define	 HYM8563_CTRL2_AIE		(1 << 1)
#define	 HYM8563_CTRL2_TIE		(1 << 0)

#define	HYM8563_SEC		0x02	/* plus battery low bit */
#define	 HYM8563_SEC_VL			(1 << 7)

#define	HYM8563_MIN		0x03
#define	HYM8563_HOUR		0x04
#define	HYM8563_DAY		0x05
#define	HYM8563_WEEKDAY		0x06
#define	HYM8563_MONTH		0x07	/* plus 1 bit for century */
#define	 HYM8563_MONTH_CENTURY		(1 << 7)
#define	HYM8563_YEAR		0x08

#define	HYM8563_CLKOUT		0x0D
#define	 HYM8563_CLKOUT_ENABLE	(1 << 7)
#define	 HYM8563_CLKOUT_32768	0
#define	 HYM8563_CLKOUT_1024	1
#define	 HYM8563_CLKOUT_32	2
#define	 HYM8563_CLKOUT_1	3
#define	 HYM8563_CLKOUT_MASK	3

struct hym8563_softc {
	device_t			dev;
	struct intr_config_hook		init_hook;
};

#if defined(FDT) && !defined(__powerpc64__) 
/* Clock class and method */
struct hym8563_clk_sc {
	device_t		base_dev;
};


static struct ofw_compat_data compat_data[] = {
	{"haoyu,hym8563", 1},
	{NULL,           0},
};
#endif


static inline int
hym8563_read_buf(device_t dev, uint8_t reg, uint8_t *buf, uint16_t buflen)
{

	return (iicdev_readfrom(dev, reg, buf, buflen, IIC_WAIT));
}

static inline int
hym8563_write_buf(device_t dev, uint8_t reg, uint8_t *buf,  uint16_t buflen)
{

	return (iicdev_writeto(dev, reg, buf, buflen, IIC_WAIT));
}

static inline int
hym8563_read_1(device_t dev, uint8_t reg, uint8_t *data)
{

	return (iicdev_readfrom(dev, reg, data, 1, IIC_WAIT));
}

static inline int
hym8563_write_1(device_t dev, uint8_t reg, uint8_t val)
{

	return (iicdev_writeto(dev, reg, &val, 1, IIC_WAIT));
}

#if defined(FDT) && !defined(__powerpc64__) 
static int
hym8563_clk_set_gate(struct clknode *clk, bool enable)
{
	struct hym8563_clk_sc *sc;
	uint8_t val;
	int rv;

	sc = clknode_get_softc(clk);

	rv = hym8563_read_1(sc->base_dev, HYM8563_CLKOUT, &val);
	if (rv != 0) {
		device_printf(sc->base_dev,
		    "Cannot read CLKOUT registers: %d\n", rv);
		return (rv);
	}
	if (enable)
		val |= HYM8563_CLKOUT_ENABLE;
	else
		val &= ~HYM8563_CLKOUT_ENABLE;
	hym8563_write_1(sc->base_dev, HYM8563_CLKOUT, val);
	if (rv != 0) {
		device_printf(sc->base_dev,
		    "Cannot write CLKOUT registers: %d\n", rv);
		return (rv);
	}
	return (0);
}

static int
hym8563_clk_recalc(struct clknode *clk, uint64_t *freq)
{
	struct hym8563_clk_sc *sc;
	uint8_t val;
	int rv;

	sc = clknode_get_softc(clk);

	rv = hym8563_read_1(sc->base_dev, HYM8563_CLKOUT, &val);
	if (rv != 0) {
		device_printf(sc->base_dev,
		    "Cannot read CLKOUT registers: %d\n", rv);
		return (rv);
	}

	switch (val & HYM8563_CLKOUT_MASK) {
	case HYM8563_CLKOUT_32768:
		*freq = 32768;
		break;
	case HYM8563_CLKOUT_1024:
		*freq = 1024;
		break;
	case HYM8563_CLKOUT_32:
		*freq = 32;
		break;
	case HYM8563_CLKOUT_1:
		*freq = 1;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}
static int
hym8563_clk_set(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct hym8563_clk_sc *sc;
	uint8_t val, tmp;
	int rv;

	sc = clknode_get_softc(clk);

	switch (*fout) {
	case 32768:
		tmp = HYM8563_CLKOUT_32768;
		break;
	case 1024:
		tmp = HYM8563_CLKOUT_1024;
		break;
	case 32:
		tmp = HYM8563_CLKOUT_32;
		break;
	case 1:
		tmp = HYM8563_CLKOUT_1;
		break;
	default:
		*stop = 1;
		return (EINVAL);
	}

	rv = hym8563_read_1(sc->base_dev, HYM8563_CLKOUT, &val);
	if (rv != 0) {
		device_printf(sc->base_dev,
		    "Cannot read CLKOUT registers: %d\n", rv);
		return (rv);
	}

	val &= ~HYM8563_CLKOUT_MASK;
	val |= tmp;
	rv = hym8563_write_1(sc->base_dev, HYM8563_CLKOUT, val);
	if (rv != 0) {
		device_printf(sc->base_dev,
		    "Cannot write CLKOUT registers: %d\n", rv);
		return (rv);
	}

	return (0);
}

static clknode_method_t hym8563_clk_clknode_methods[] = {
	CLKNODEMETHOD(clknode_recalc_freq,	hym8563_clk_recalc),
	CLKNODEMETHOD(clknode_set_freq,		hym8563_clk_set),
	CLKNODEMETHOD(clknode_set_gate,		hym8563_clk_set_gate),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(hym8563_clk_clknode, hym8563_clk_clknode_class,
    hym8563_clk_clknode_methods, sizeof(struct hym8563_clk_sc),
    clknode_class);


static int
hym8563_attach_clocks(struct hym8563_softc *sc)
{
	struct clkdom *clkdom;
	struct clknode_init_def clkidef;
	struct clknode *clk;
	struct hym8563_clk_sc *clksc;
	const char **clknames;
	phandle_t node;
	int nclks, rv;

	node = ofw_bus_get_node(sc->dev);

	/* clock-output-names are optional. Could use them for clkidef.name. */
	nclks = ofw_bus_string_list_to_array(node, "clock-output-names",
	    &clknames);

	clkdom = clkdom_create(sc->dev);

	memset(&clkidef, 0, sizeof(clkidef));
	clkidef.id = 1;
	clkidef.name = (nclks == 1) ? clknames[0] : "hym8563-clkout";
	clk = clknode_create(clkdom, &hym8563_clk_clknode_class, &clkidef);
	if (clk == NULL) {
		device_printf(sc->dev, "Cannot create '%s'.\n", clkidef.name);
		return (ENXIO);
	}
	clksc = clknode_get_softc(clk);
	clksc->base_dev = sc->dev;
	clknode_register(clkdom, clk);

	rv = clkdom_finit(clkdom);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot finalize clkdom initialization: "
		    "%d\n", rv);
		return (ENXIO);
	}

	if (bootverbose)
		clkdom_dump(clkdom);

	return (0);
}
#endif

static int
hym8563_gettime(device_t dev, struct timespec *ts)
{
	struct hym8563_softc	*sc;
	struct bcd_clocktime	 bct;
	uint8_t			 buf[7];
	int 			 rv;

	sc = device_get_softc(dev);

	/* Read all RTC data */
	rv = hym8563_read_buf(sc->dev, HYM8563_SEC, buf, sizeof(buf));
	if (rv != 0) {
		device_printf(sc->dev, "Cannot read time registers: %d\n", rv);
		return (rv);
	}

	/* Check for low voltage flag */
	if (buf[0] & HYM8563_SEC_VL)
	{
		device_printf(sc->dev,
		    "WARNING: RTC battery failed; time is invalid\n");
		return (EINVAL);
	}

	bzero(&bct, sizeof(bct));
	bct.sec  = buf[0] & 0x7F;
	bct.min  = buf[1] & 0x7F;
	bct.hour = buf[2] & 0x3f;
	bct.day  = buf[3] & 0x3f;
	/* buf[4] is  weekday */
	bct.mon  = buf[5] & 0x1f;
	bct.year = buf[6] & 0xff;
	if (buf[5] & HYM8563_MONTH_CENTURY)
		bct.year += 0x100;

	clock_dbgprint_bcd(sc->dev, CLOCK_DBG_READ, &bct);
	return (clock_bcd_to_ts(&bct, ts, false));
}

static int
hym8563_settime(device_t dev, struct timespec *ts)
{
	struct hym8563_softc	*sc;
	struct bcd_clocktime 	 bct;
	uint8_t			 buf[7];
	int 			 rv;

	sc = device_get_softc(dev);
	ts->tv_sec -= utc_offset();
	clock_ts_to_bcd(ts, &bct, false);
	clock_dbgprint_bcd(sc->dev, CLOCK_DBG_WRITE, &bct);

	buf[0] = bct.sec;	/* Also clear VL flag */
	buf[1] = bct.min;
	buf[2] = bct.hour;
	buf[3] = bct.day;
	buf[4] = bct.dow;
	buf[5] = bct.mon;
	buf[6] = bct.year & 0xFF;
	if (bct.year > 0x99)
		buf[5] |= HYM8563_MONTH_CENTURY;

	/* Stop RTC */
	rv = hym8563_write_1(sc->dev, HYM8563_CTRL1, HYM8563_CTRL1_STOP);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot write CTRL1 register: %d\n", rv);
		return (rv);
	}

	/* Write all RTC data */
	rv = hym8563_write_buf(sc->dev, HYM8563_SEC, buf, sizeof(buf));
	if (rv != 0) {
		device_printf(sc->dev, "Cannot write time registers: %d\n", rv);
		return (rv);
	}
	return (rv);

	/* Start RTC again */
	rv = hym8563_write_1(sc->dev, HYM8563_CTRL1, 0);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot write CTRL1 register: %d\n", rv);
		return (rv);
	}

	return (0);
}

static void
hym8563_init(void *arg)
{
	struct hym8563_softc *sc;
	uint8_t reg;
	int rv;

	sc = (struct hym8563_softc*)arg;
	config_intrhook_disestablish(&sc->init_hook);

	/* Clear CTL1 register (stop and test bits) */
	rv = hym8563_write_1(sc->dev, HYM8563_CTRL1, 0);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot init CTRL1 register: %d\n", rv);
		return;
	}

	/* Disable interrupts and alarms */
	rv = hym8563_read_1(sc->dev, HYM8563_CTRL2, &reg);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot read CTRL2 register: %d\n", rv);
		return;
	}
	rv &= ~HYM8563_CTRL2_TI_TP;
	rv &= ~HYM8563_CTRL2_AF;
	rv &= ~HYM8563_CTRL2_TF;
	rv = hym8563_write_1(sc->dev, HYM8563_CTRL2, 0);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot write CTRL2 register: %d\n", rv);
		return;
	}

	/*
	 * Register as a system realtime clock.
	 */
	clock_register_flags(sc->dev, 1000000, 0);
	clock_schedule(sc->dev, 1);
	return;
}

static int
hym8563_probe(device_t dev)
{

#if defined(FDT) && !defined(__powerpc64__) 
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
		device_set_desc(dev, "HYM8694 RTC");
		return (BUS_PROBE_DEFAULT);
	}
#endif
	return (ENXIO);
}

static int
hym8563_attach(device_t dev)
{
	struct hym8563_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

#if defined(FDT) && !defined(__powerpc64__) 
	if (hym8563_attach_clocks(sc) != 0)
		return(ENXIO);
#endif

	/*
	 * Chip init must wait until interrupts are enabled.  Often i2c access
	 * works only when the interrupts are available.
	 */
	sc->init_hook.ich_func = hym8563_init;
	sc->init_hook.ich_arg = sc;
	if (config_intrhook_establish(&sc->init_hook) != 0)
		return (ENOMEM);

	return (0);
}

static int
hym8563_detach(device_t dev)
{

	clock_unregister(dev);
	return (0);
}

static device_method_t hym8563_methods[] = {
        /* device_if methods */
	DEVMETHOD(device_probe,		hym8563_probe),
	DEVMETHOD(device_attach,	hym8563_attach),
	DEVMETHOD(device_detach,	hym8563_detach),

        /* clock_if methods */
	DEVMETHOD(clock_gettime,	hym8563_gettime),
	DEVMETHOD(clock_settime,	hym8563_settime),

	DEVMETHOD_END,
};

static DEFINE_CLASS_0(hym8563_rtc, hym8563_driver, hym8563_methods,
    sizeof(struct hym8563_softc));
EARLY_DRIVER_MODULE(hym8563, iicbus, hym8563_driver, NULL, NULL,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_FIRST);
MODULE_VERSION(hym8563, 1);
MODULE_DEPEND(hym8563, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
#if defined(FDT) && !defined(__powerpc64__) 
IICBUS_FDT_PNP_INFO(compat_data);
#endif