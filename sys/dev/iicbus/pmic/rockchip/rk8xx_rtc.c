/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Emmanuel Vadot <manu@FreeBSD.org>
 * Copyright (c) 2021 Peter Jeremy <peterj@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/clock.h>

#include <dev/iicbus/pmic/rockchip/rk8xx.h>

int
rk8xx_gettime(device_t dev, struct timespec *ts)
{
	struct rk8xx_softc *sc;
	struct bcd_clocktime bct;
	uint8_t data[7];
	uint8_t ctrl;
	int error;

	sc = device_get_softc(dev);

	/* Latch the RTC value into the shadow registers and set 24hr mode */
	error = rk8xx_read(dev, sc->rtc_regs.ctrl, &ctrl, 1);
	if (error != 0)
		return (error);

	ctrl |= sc->rtc_regs.ctrl_readsel_mask;
	ctrl &= ~(sc->rtc_regs.ctrl_ampm_mask | sc->rtc_regs.ctrl_gettime_mask);
	error = rk8xx_write(dev, sc->rtc_regs.ctrl, &ctrl, 1);
	if (error != 0)
		return (error);
	ctrl |= sc->rtc_regs.ctrl_gettime_mask;
	error = rk8xx_write(dev, sc->rtc_regs.ctrl, &ctrl, 1);
	if (error != 0)
		return (error);
	if (sc->type == RK809 || sc->type == RK817) {
		/* wait one 32khz cycle for clock shadow registers to latch */
		DELAY(1000000 / 32000);
	}
	ctrl &= ~sc->rtc_regs.ctrl_gettime_mask;
	error = rk8xx_write(dev, sc->rtc_regs.ctrl, &ctrl, 1);
	if (error != 0)
		return (error);

	/* This works as long as sc->rtc_regs.secs = 0 */
	error = rk8xx_read(dev, sc->rtc_regs.secs, data, 7);
	if (error != 0)
		return (error);

	/*
	 * If the reported year is earlier than 2019, assume the clock is unset.
	 * This is both later than the reset value for the RK805 and RK808 as
	 * well as being prior to the current time.
	 */
	if (data[sc->rtc_regs.years] < 0x19)
		return (EINVAL);

	memset(&bct, 0, sizeof(bct));
	bct.year = data[sc->rtc_regs.years];
	bct.mon = data[sc->rtc_regs.months] & sc->rtc_regs.months_mask;
	bct.day = data[sc->rtc_regs.days] & sc->rtc_regs.days_mask;
	bct.hour = data[sc->rtc_regs.hours] & sc->rtc_regs.hours_mask;
	bct.min = data[sc->rtc_regs.minutes] & sc->rtc_regs.minutes_mask;
	bct.sec = data[sc->rtc_regs.secs] & sc->rtc_regs.secs_mask;
	bct.dow = data[sc->rtc_regs.weeks] & sc->rtc_regs.weeks_mask;
	/* The day of week is reported as 1-7 with 1 = Monday */
	if (bct.dow == 7)
		bct.dow = 0;
	bct.ispm = 0;
	if (sc->type == RK809 || sc->type == RK817)
		bct.year += 0x2000;	/* valid for 2000-2099 only */

	if (bootverbose)
		device_printf(dev, "Read RTC: %02x-%02x-%02x %02x:%02x:%02x\n",
		    bct.year, bct.mon, bct.day, bct.hour, bct.min, bct.sec);

	return (clock_bcd_to_ts(&bct, ts, false));
}

int
rk8xx_settime(device_t dev, struct timespec *ts)
{
	struct rk8xx_softc *sc;
	struct bcd_clocktime bct;
	uint8_t data[7];
	int error;
	uint8_t ctrl;

	sc = device_get_softc(dev);

	clock_ts_to_bcd(ts, &bct, false);

	/* This works as long as RK805_RTC_SECS = 0 */
	if (sc->type == RK809 || sc->type == RK817) {
		/* valid for 2000-2099 only */
		if ((bct.year & 0xff00) != 0x2000) {
			device_printf(dev, "year out of range\n");
			return (EINVAL);
		}
		bct.year &= 0x00ff;
	}
	data[sc->rtc_regs.years] = bct.year;
	data[sc->rtc_regs.months] = bct.mon;
	data[sc->rtc_regs.days] = bct.day;
	data[sc->rtc_regs.hours] = bct.hour;
	data[sc->rtc_regs.minutes] = bct.min;
	data[sc->rtc_regs.secs] = bct.sec;
	data[sc->rtc_regs.weeks] = bct.dow;
	/* The day of week is reported as 1-7 with 1 = Monday */
	if (data[sc->rtc_regs.weeks] == 0)
		data[sc->rtc_regs.weeks] = 7;

	error = rk8xx_read(dev, sc->rtc_regs.ctrl, &ctrl, 1);
	if (error != 0)
		return (error);

	ctrl |= sc->rtc_regs.ctrl_stop_mask;
	ctrl &= ~sc->rtc_regs.ctrl_ampm_mask;
	error = rk8xx_write(dev, sc->rtc_regs.ctrl, &ctrl, 1);
	if (error != 0)
		return (error);

	error = rk8xx_write(dev, sc->rtc_regs.secs, data, 7);
	ctrl &= ~sc->rtc_regs.ctrl_stop_mask;
	rk8xx_write(dev, sc->rtc_regs.ctrl, &ctrl, 1);

	return (error);
}
