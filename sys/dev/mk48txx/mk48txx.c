/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: mk48txx.c,v 1.7 2001/04/08 17:05:10 tsutsui Exp $
 *
 * $FreeBSD$
 */

/*
 * Mostek MK48T02, MK48T08, MK48T59 time-of-day chip subroutines.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <mk48txx/mk48txxreg.h>

#include "clock_if.h"

struct mk48txx_softc {
	bus_space_tag_t	mk_bt;		/* bus tag & handle */
	bus_space_handle_t mk_bh;	/* */
	bus_size_t	mk_nvramsz;	/* Size of NVRAM on the chip */
	bus_size_t	mk_clkoffset;	/* Offset in NVRAM to clock bits */
	u_int		mk_year0;	/* What year is represented on the system
					   by the chip's year counter at 0 */
};

int mk48txx_auto_century_adjust = 1;

struct {
	const char *name;
	bus_size_t nvramsz;
	bus_size_t clkoff;
	int flags;
#define MK48TXX_EXT_REGISTERS	1	/* Has extended register set */
} mk48txx_models[] = {
	{ "mk48t02", MK48T02_CLKSZ, MK48T02_CLKOFF, 0 },
	{ "mk48t08", MK48T08_CLKSZ, MK48T08_CLKOFF, 0 },
	{ "mk48t59", MK48T59_CLKSZ, MK48T59_CLKOFF, MK48TXX_EXT_REGISTERS },
};

int
mk48txx_attach(device_t dev, bus_space_tag_t bt, bus_space_handle_t bh,
    const char *model, int year0)
{
	struct mk48txx_softc *sc;
	bus_size_t clkoff = 0, nvramsz = 0;
	int i;

	device_printf(dev, "model %s", model);
	i = sizeof(mk48txx_models) / sizeof(mk48txx_models[0]);
	while (--i >= 0) {
		if (strcmp(model, mk48txx_models[i].name) == 0) {
			nvramsz = mk48txx_models[i].nvramsz;
			clkoff = mk48txx_models[i].clkoff;
			break;
		}
	}
	if (i < 0) {
		device_printf(dev, " (unsupported)\n");
		return (ENXIO);
	}
	printf("\n");

	sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT);
	sc->mk_bt = bt;
	sc->mk_bh = bh;
	sc->mk_nvramsz = nvramsz;
	sc->mk_clkoffset = clkoff;
	sc->mk_year0 = year0;
	device_set_softc(dev, sc);
	clock_register(dev, 1000000);	/* 1 second resolution. */

	return (0);
}

/*
 * Get time-of-day and convert to a `struct timespec'
 * Return 0 on success; an error number otherwise.
 */
int
mk48txx_gettime(device_t dev, struct timespec *ts)
{
	struct mk48txx_softc *mk = device_get_softc(dev);
	bus_space_tag_t bt = mk->mk_bt;
	bus_space_handle_t bh = mk->mk_bh;
	bus_size_t clkoff = mk->mk_clkoffset;
	struct clocktime ct;
	int year;
	u_int8_t csr;

	/* enable read (stop time) */
	csr = bus_space_read_1(bt, bh, clkoff + MK48TXX_ICSR);
	csr |= MK48TXX_CSR_READ;
	bus_space_write_1(bt, bh, clkoff + MK48TXX_ICSR, csr);

	ct.nsec = 0;
	ct.sec = FROMBCD(bus_space_read_1(bt, bh, clkoff + MK48TXX_ISEC));
	ct.min = FROMBCD(bus_space_read_1(bt, bh, clkoff + MK48TXX_IMIN));
	ct.hour = FROMBCD(bus_space_read_1(bt, bh, clkoff + MK48TXX_IHOUR));
	ct.day = FROMBCD(bus_space_read_1(bt, bh, clkoff + MK48TXX_IDAY));
	ct.dow = FROMBCD(bus_space_read_1(bt, bh, clkoff + MK48TXX_IWDAY)) % 7;
	ct.mon = FROMBCD(bus_space_read_1(bt, bh, clkoff + MK48TXX_IMON));
	year = FROMBCD(bus_space_read_1(bt, bh, clkoff + MK48TXX_IYEAR));

	year += mk->mk_year0;
	if (year < POSIX_BASE_YEAR && mk48txx_auto_century_adjust != 0)
		year += 100;

	ct.year = year;

	/* time wears on */
	csr = bus_space_read_1(bt, bh, clkoff + MK48TXX_ICSR);
	csr &= ~MK48TXX_CSR_READ;
	bus_space_write_1(bt, bh, clkoff + MK48TXX_ICSR, csr);

	return (clock_ct_to_ts(&ct, ts));
}

/*
 * Set the time-of-day clock based on the value of the `struct timespec' arg.
 * Return 0 on success; an error number otherwise.
 */
int
mk48txx_settime(device_t dev, struct timespec *ts)
{
	struct mk48txx_softc *mk = device_get_softc(dev);
	bus_space_tag_t bt = mk->mk_bt;
	bus_space_handle_t bh = mk->mk_bh;
	bus_size_t clkoff = mk->mk_clkoffset;
	struct clocktime ct;
	u_int8_t csr;
	int year;

	/* Accuracy is only one second. */
	if (ts->tv_nsec >= 500000000)
		ts->tv_sec++;
	ts->tv_nsec = 0;
	clock_ts_to_ct(ts, &ct);

	year = ct.year - mk->mk_year0;
	if (year > 99 && mk48txx_auto_century_adjust != 0)
		year -= 100;

	/* enable write */
	csr = bus_space_read_1(bt, bh, clkoff + MK48TXX_ICSR);
	csr |= MK48TXX_CSR_WRITE;
	bus_space_write_1(bt, bh, clkoff + MK48TXX_ICSR, csr);

	bus_space_write_1(bt, bh, clkoff + MK48TXX_ISEC, TOBCD(ct.sec));
	bus_space_write_1(bt, bh, clkoff + MK48TXX_IMIN, TOBCD(ct.min));
	bus_space_write_1(bt, bh, clkoff + MK48TXX_IHOUR, TOBCD(ct.hour));
	bus_space_write_1(bt, bh, clkoff + MK48TXX_IWDAY, TOBCD(ct.dow));
	bus_space_write_1(bt, bh, clkoff + MK48TXX_IDAY, TOBCD(ct.day));
	bus_space_write_1(bt, bh, clkoff + MK48TXX_IMON, TOBCD(ct.mon));
	bus_space_write_1(bt, bh, clkoff + MK48TXX_IYEAR, TOBCD(year));

	/* load them up */
	csr = bus_space_read_1(bt, bh, clkoff + MK48TXX_ICSR);
	csr &= ~MK48TXX_CSR_WRITE;
	bus_space_write_1(bt, bh, clkoff + MK48TXX_ICSR, csr);
	return (0);
}

int
mk48txx_get_nvram_size(device_t dev, bus_size_t *vp)
{
	struct mk48txx_softc *mk;

	mk = device_get_softc(dev);
	*vp = mk->mk_nvramsz;
	return (0);
}
