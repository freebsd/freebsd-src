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
__FBSDID("$FreeBSD$");

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

#ifdef FDT
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
#define HYM8563_YEAR		0x08

struct hym8563_softc {
	device_t			dev;
	struct intr_config_hook		init_hook;
};

#ifdef FDT
static struct ofw_compat_data compat_data[] = {
	{"haoyu,hym8563", 1},
	{NULL,           0},
};
#endif


static inline int
hym8563_read_buf(struct hym8563_softc *sc, uint8_t reg, uint8_t *buf,
    uint16_t buflen) 
{

	return (iicdev_readfrom(sc->dev, reg, buf, buflen, IIC_WAIT));
}

static inline int
hym8563_write_buf(struct hym8563_softc *sc, uint8_t reg, uint8_t *buf,
    uint16_t buflen) 
{

	return (iicdev_writeto(sc->dev, reg, buf, buflen, IIC_WAIT));
}

static inline int
hym8563_read_1(struct hym8563_softc *sc, uint8_t reg, uint8_t *data) 
{

	return (iicdev_readfrom(sc->dev, reg, data, 1, IIC_WAIT));
}

static inline int
hym8563_write_1(struct hym8563_softc *sc, uint8_t reg, uint8_t val) 
{

	return (iicdev_writeto(sc->dev, reg, &val, 1, IIC_WAIT));
}

static int
hym8563_gettime(device_t dev, struct timespec *ts)
{
	struct hym8563_softc	*sc;
	struct bcd_clocktime	 bct;
	uint8_t			 buf[7];
	int 			 rv;

	sc = device_get_softc(dev);

	/* Read all RTC data */
	rv = hym8563_read_buf(sc, HYM8563_SEC, buf, sizeof(buf));
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
	rv = hym8563_write_1(sc, HYM8563_CTRL1, HYM8563_CTRL1_STOP);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot write CTRL1 register: %d\n", rv);
		return (rv);
	}

	/* Write all RTC data */
	rv = hym8563_write_buf(sc, HYM8563_SEC, buf, sizeof(buf));
	if (rv != 0) {
		device_printf(sc->dev, "Cannot write time registers: %d\n", rv);
		return (rv);
	}
	return (rv);

	/* Start RTC again */
	rv = hym8563_write_1(sc, HYM8563_CTRL1, 0);
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
	rv = hym8563_write_1(sc, HYM8563_CTRL1, 0);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot init CTRL1 register: %d\n", rv);
		return;
	}
	
	/* Disable interrupts and alarms */
	rv = hym8563_read_1(sc, HYM8563_CTRL2, &reg);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot read CTRL2 register: %d\n", rv);
		return;
	}
	rv &= ~HYM8563_CTRL2_TI_TP;
	rv &= ~HYM8563_CTRL2_AF;
	rv &= ~HYM8563_CTRL2_TF;
	rv = hym8563_write_1(sc, HYM8563_CTRL2, 0);
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

#ifdef FDT
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

static devclass_t hym8563_devclass;
static DEFINE_CLASS_0(hym8563_rtc, hym8563_driver, hym8563_methods,
    sizeof(struct hym8563_softc));
DRIVER_MODULE(hym8563, iicbus, hym8563_driver, hym8563_devclass, NULL, NULL);
MODULE_VERSION(hym8563, 1);
MODULE_DEPEND(hym8563, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
IICBUS_FDT_PNP_INFO(compat_data);