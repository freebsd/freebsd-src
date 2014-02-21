/*-
 * Copyright (c) 2008 TAKAHASHI Yoshihiro
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <pc98/cbus/cbus.h>
#include <isa/isavar.h>

/*
 * modified for PC98 by Kakefuda
 */

/*
 * RTC support routines
 */

static void rtc_serialcombit(int);
static void rtc_serialcom(int);
static int rtc_inb(void);
static void rtc_outb(int);

static void
rtc_serialcombit(int i)
{
	outb(IO_RTC, ((i&0x01)<<5)|0x07);
	DELAY(1);
	outb(IO_RTC, ((i&0x01)<<5)|0x17);
	DELAY(1);
	outb(IO_RTC, ((i&0x01)<<5)|0x07);
	DELAY(1);
}

static void
rtc_serialcom(int i)
{
	rtc_serialcombit(i&0x01);
	rtc_serialcombit((i&0x02)>>1);
	rtc_serialcombit((i&0x04)>>2);
	rtc_serialcombit((i&0x08)>>3);
	outb(IO_RTC, 0x07);
	DELAY(1);
	outb(IO_RTC, 0x0f);
	DELAY(1);
	outb(IO_RTC, 0x07);
 	DELAY(1);
}

static void
rtc_outb(int val)
{
	int s;
	int sa = 0;

	for (s=0;s<8;s++) {
	    sa = ((val >> s) & 0x01) ? 0x27 : 0x07;
	    outb(IO_RTC, sa);		/* set DI & CLK 0 */
	    DELAY(1);
	    outb(IO_RTC, sa | 0x10);	/* CLK 1 */
	    DELAY(1);
	}
	outb(IO_RTC, sa & 0xef);	/* CLK 0 */
}

static int
rtc_inb(void)
{
	int s;
	int sa = 0;

	for (s=0;s<8;s++) {
	    sa |= ((inb(0x33) & 0x01) << s);
	    outb(IO_RTC, 0x17);	/* CLK 1 */
	    DELAY(1);
	    outb(IO_RTC, 0x07);	/* CLK 0 */
	    DELAY(2);
	}
	return sa;
}

/**********************************************************************
 * RTC driver for subr_rtc
 */

#include "clock_if.h"

#include <sys/rman.h>

struct pcrtc_softc {
	int port_rid1, port_rid2;
	struct resource *port_res1, *port_res2;
};

/*
 * Attach to the ISA PnP descriptors for the timer and realtime clock.
 */
static struct isa_pnp_id pcrtc_ids[] = {
	{ 0x000bd041 /* PNP0B00 */, "AT realtime clock" },
	{ 0 }
};

static int
pcrtc_probe(device_t dev)
{
	int result;

	device_set_desc(dev, "PC Real Time Clock");
	result = ISA_PNP_PROBE(device_get_parent(dev), dev, pcrtc_ids);
	/* ENXIO if wrong PnP-ID, ENOENT ifno PnP-ID, zero if good PnP-iD */
	if (result != ENOENT)
		return(result);
	/* All PC's have an RTC, and we're hosed without it, so... */
	return (BUS_PROBE_LOW_PRIORITY);
}

static int
pcrtc_attach(device_t dev)
{
	struct pcrtc_softc *sc;

	/*
	 * Not that we need them or anything, but grab our resources
	 * so they show up, correctly attributed, in the big picture.
	 */
	sc = device_get_softc(dev);
	sc->port_rid1 = 0;
	bus_set_resource(dev, SYS_RES_IOPORT, sc->port_rid1, IO_RTC, 1);
	if (!(sc->port_res1 = bus_alloc_resource(dev, SYS_RES_IOPORT,
	    &sc->port_rid1, IO_RTC, IO_RTC, 1, RF_ACTIVE)))
		device_printf(dev, "Warning: Couldn't map I/O.\n");
	sc->port_rid2 = 1;
	bus_set_resource(dev, SYS_RES_IOPORT, sc->port_rid2, 0x33, 1);
	if (!(sc->port_res2 = bus_alloc_resource(dev, SYS_RES_IOPORT,
	    &sc->port_rid2, 0x33, 0x33, 1, RF_ACTIVE)))
		device_printf(dev, "Warning: Couldn't map I/O.\n");

	clock_register(dev, 1000000);
	return(0);
}

static int
pcrtc_settime(device_t dev __unused, struct timespec *ts)
{
	struct clocktime ct;

	clock_ts_to_ct(ts, &ct);

	rtc_serialcom(0x01);	/* Register shift command. */

	rtc_outb(bin2bcd(ct.sec)); 		/* Write back Seconds */
	rtc_outb(bin2bcd(ct.min)); 		/* Write back Minutes */
	rtc_outb(bin2bcd(ct.hour)); 		/* Write back Hours   */

	rtc_outb(bin2bcd(ct.day));		/* Write back Day     */
	rtc_outb((ct.mon << 4) | ct.dow);	/* Write back Month and DOW */
	rtc_outb(bin2bcd(ct.year % 100));	/* Write back Year    */

	rtc_serialcom(0x02);	/* Time set & Counter hold command. */
	rtc_serialcom(0x00);	/* Register hold command. */

	return (0);
}

static int
pcrtc_gettime(device_t dev, struct timespec *ts)
{
	struct clocktime ct;
	int i;

	rtc_serialcom(0x03);	/* Time Read */
	rtc_serialcom(0x01);	/* Register shift command. */
	DELAY(20);

	ct.nsec = 0;
	ct.sec = bcd2bin(rtc_inb() & 0xff);		/* sec */
	ct.min = bcd2bin(rtc_inb() & 0xff);		/* min */
	ct.hour = bcd2bin(rtc_inb() & 0xff);		/* hour */
	ct.day = bcd2bin(rtc_inb() & 0xff);		/* date */
	i = rtc_inb();
	ct.dow = i & 0x0f;				/* dow */
	ct.mon = (i >> 4) & 0x0f;			/* month */
	ct.year = bcd2bin(rtc_inb() & 0xff) + 1900;	/* year */
	if (ct.year < 1995)
		ct.year += 100;

	/* Set dow = -1 because some clocks don't set it correctly. */
	ct.dow = -1;

	return (clock_ct_to_ts(&ct, ts));
}

static device_method_t pcrtc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcrtc_probe),
	DEVMETHOD(device_attach,	pcrtc_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
		/* XXX stop statclock? */
	DEVMETHOD(device_resume,	bus_generic_resume),
		/* XXX restart statclock? */

	/* clock interface */
	DEVMETHOD(clock_gettime,	pcrtc_gettime),
	DEVMETHOD(clock_settime,	pcrtc_settime),

	{ 0, 0 }
};

static driver_t pcrtc_driver = {
	"pcrtc",
	pcrtc_methods,
	sizeof(struct pcrtc_softc),
};

static devclass_t pcrtc_devclass;

DRIVER_MODULE(pcrtc, isa, pcrtc_driver, pcrtc_devclass, 0, 0);
