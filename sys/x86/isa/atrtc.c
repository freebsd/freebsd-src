/*-
 * Copyright (c) 2008 Poul-Henning Kamp
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
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_isa.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <isa/rtc.h>
#ifdef DEV_ISA
#include <isa/isareg.h>
#include <isa/isavar.h>
#endif

#define	RTC_LOCK	mtx_lock_spin(&clock_lock)
#define	RTC_UNLOCK	mtx_unlock_spin(&clock_lock)

int	atrtcclock_disable = 0;

static	int	rtc_reg = -1;
static	u_char	rtc_statusa = RTCSA_DIVIDER | RTCSA_NOPROF;
static	u_char	rtc_statusb = RTCSB_24HR;

/*
 * RTC support routines
 */

int
rtcin(int reg)
{
	u_char val;

	RTC_LOCK;
	if (rtc_reg != reg) {
		inb(0x84);
		outb(IO_RTC, reg);
		rtc_reg = reg;
		inb(0x84);
	}
	val = inb(IO_RTC + 1);
	RTC_UNLOCK;
	return (val);
}

void
writertc(int reg, u_char val)
{

	RTC_LOCK;
	if (rtc_reg != reg) {
		inb(0x84);
		outb(IO_RTC, reg);
		rtc_reg = reg;
		inb(0x84);
	}
	outb(IO_RTC + 1, val);
	inb(0x84);
	RTC_UNLOCK;
}

static __inline int
readrtc(int port)
{
	return(bcd2bin(rtcin(port)));
}

void
atrtc_start(void)
{

	writertc(RTC_STATUSA, rtc_statusa);
	writertc(RTC_STATUSB, RTCSB_24HR);
}

void
atrtc_rate(unsigned rate)
{

	rtc_statusa = RTCSA_DIVIDER | rate;
	writertc(RTC_STATUSA, rtc_statusa);
}

void
atrtc_enable_intr(void)
{

	rtc_statusb |= RTCSB_PINTR;
	writertc(RTC_STATUSB, rtc_statusb);
	rtcin(RTC_INTR);
}

void
atrtc_restore(void)
{

	/* Restore all of the RTC's "status" (actually, control) registers. */
	rtcin(RTC_STATUSA);	/* dummy to get rtc_reg set */
	writertc(RTC_STATUSB, RTCSB_24HR);
	writertc(RTC_STATUSA, rtc_statusa);
	writertc(RTC_STATUSB, rtc_statusb);
	rtcin(RTC_INTR);
}

int
atrtc_setup_clock(void)
{
	int diag;

	if (atrtcclock_disable)
		return (0);

	diag = rtcin(RTC_DIAG);
	if (diag != 0) {
		printf("RTC BIOS diagnostic error %b\n",
		    diag, RTCDG_BITS);
		return (0);
	}

	stathz = RTC_NOPROFRATE;
	profhz = RTC_PROFRATE;

	return (1);
}

/**********************************************************************
 * RTC driver for subr_rtc
 */

#include "clock_if.h"

#include <sys/rman.h>

struct atrtc_softc {
	int port_rid, intr_rid;
	struct resource *port_res;
	struct resource *intr_res;
};

/*
 * Attach to the ISA PnP descriptors for the timer and realtime clock.
 */
static struct isa_pnp_id atrtc_ids[] = {
	{ 0x000bd041 /* PNP0B00 */, "AT realtime clock" },
	{ 0 }
};

static int
atrtc_probe(device_t dev)
{
	int result;
	
	device_set_desc(dev, "AT Real Time Clock");
	result = ISA_PNP_PROBE(device_get_parent(dev), dev, atrtc_ids);
	/* ENXIO if wrong PnP-ID, ENOENT ifno PnP-ID, zero if good PnP-iD */
	if (result != ENOENT)
		return(result);
	/* All PC's have an RTC, and we're hosed without it, so... */
	return (BUS_PROBE_LOW_PRIORITY);
}

static int
atrtc_attach(device_t dev)
{
	struct atrtc_softc *sc;
	int i;

	/*
	 * Not that we need them or anything, but grab our resources
	 * so they show up, correctly attributed, in the big picture.
	 */
	
	sc = device_get_softc(dev);
	if (!(sc->port_res = bus_alloc_resource(dev, SYS_RES_IOPORT,
	    &sc->port_rid, IO_RTC, IO_RTC + 1, 2, RF_ACTIVE)))
		device_printf(dev,"Warning: Couldn't map I/O.\n");
	if (!(sc->intr_res = bus_alloc_resource(dev, SYS_RES_IRQ,
	    &sc->intr_rid, 8, 8, 1, RF_ACTIVE)))
		device_printf(dev,"Warning: Couldn't map Interrupt.\n");
	clock_register(dev, 1000000);
	if (resource_int_value("atrtc", 0, "clock", &i) == 0 && i == 0)
		atrtcclock_disable = 1;
	return(0);
}

static int
atrtc_resume(device_t dev)
{

	atrtc_restore();
	return(0);
}

static int
atrtc_settime(device_t dev __unused, struct timespec *ts)
{
	struct clocktime ct;

	clock_ts_to_ct(ts, &ct);

	/* Disable RTC updates and interrupts. */
	writertc(RTC_STATUSB, RTCSB_HALT | RTCSB_24HR);

	writertc(RTC_SEC, bin2bcd(ct.sec)); 		/* Write back Seconds */
	writertc(RTC_MIN, bin2bcd(ct.min)); 		/* Write back Minutes */
	writertc(RTC_HRS, bin2bcd(ct.hour));		/* Write back Hours   */

	writertc(RTC_WDAY, ct.dow + 1);			/* Write back Weekday */
	writertc(RTC_DAY, bin2bcd(ct.day));		/* Write back Day */
	writertc(RTC_MONTH, bin2bcd(ct.mon));           /* Write back Month   */
	writertc(RTC_YEAR, bin2bcd(ct.year % 100));	/* Write back Year    */
#ifdef USE_RTC_CENTURY
	writertc(RTC_CENTURY, bin2bcd(ct.year / 100));	/* ... and Century    */
#endif

	/* Reenable RTC updates and interrupts. */
	writertc(RTC_STATUSB, rtc_statusb);
	rtcin(RTC_INTR);
	return (0);
}

static int
atrtc_gettime(device_t dev, struct timespec *ts)
{
	struct clocktime ct;
	int s;

	/* Look if we have a RTC present and the time is valid */
	if (!(rtcin(RTC_STATUSD) & RTCSD_PWR)) {
		device_printf(dev, "WARNING: Battery failure indication\n");
		return (EINVAL);
	}

	/* wait for time update to complete */
	/* If RTCSA_TUP is zero, we have at least 244us before next update */
	s = splhigh();
	while (rtcin(RTC_STATUSA) & RTCSA_TUP) {
		splx(s);
		s = splhigh();
	}
	ct.nsec = 0;
	ct.sec = readrtc(RTC_SEC);
	ct.min = readrtc(RTC_MIN);
	ct.hour = readrtc(RTC_HRS);
	ct.day = readrtc(RTC_DAY);
	ct.dow = readrtc(RTC_WDAY) - 1;
	ct.mon = readrtc(RTC_MONTH);
	ct.year = readrtc(RTC_YEAR);
#ifdef USE_RTC_CENTURY
	ct.year += readrtc(RTC_CENTURY) * 100;
#else
	ct.year += 2000;
#endif
	/* Set dow = -1 because some clocks don't set it correctly. */
	ct.dow = -1;
	return (clock_ct_to_ts(&ct, ts));
}

static device_method_t atrtc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		atrtc_probe),
	DEVMETHOD(device_attach,	atrtc_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
		/* XXX stop statclock? */
	DEVMETHOD(device_resume,	atrtc_resume),

	/* clock interface */
	DEVMETHOD(clock_gettime,	atrtc_gettime),
	DEVMETHOD(clock_settime,	atrtc_settime),

	{ 0, 0 }
};

static driver_t atrtc_driver = {
	"atrtc",
	atrtc_methods,
	sizeof(struct atrtc_softc),
};

static devclass_t atrtc_devclass;

DRIVER_MODULE(atrtc, isa, atrtc_driver, atrtc_devclass, 0, 0);
DRIVER_MODULE(atrtc, acpi, atrtc_driver, atrtc_devclass, 0, 0);

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(rtc, rtc)
{
	printf("%02x/%02x/%02x %02x:%02x:%02x, A = %02x, B = %02x, C = %02x\n",
		rtcin(RTC_YEAR), rtcin(RTC_MONTH), rtcin(RTC_DAY),
		rtcin(RTC_HRS), rtcin(RTC_MIN), rtcin(RTC_SEC),
		rtcin(RTC_STATUSA), rtcin(RTC_STATUSB), rtcin(RTC_INTR));
}
#endif /* DDB */
