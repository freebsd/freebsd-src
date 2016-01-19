/*-
 * Copyright (c) 2014 SRI International
 * Copyright (c) 2012 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
#include <sys/conf.h>
#include <sys/clock.h>
#include <sys/endian.h>
#include <sys/rman.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/beri/berirom/berirom.h>

#include <sys/stdint.h>

devclass_t	berirom_devclass;

static d_read_t berirom_read;

static struct cdevsw berirom_cdevsw = {
	.d_version =	D_VERSION,
	.d_read =	berirom_read,
	.d_name =	"berirom",
};

static int
berirom_read(struct cdev *dev, struct uio *uio, int flag)
{
	struct berirom_softc *sc;
	u_long offset;
	uint32_t word;
	int error;

	sc = dev->si_drv1;
	/* XXX: should accomidate read requests longer than the ROM */
	if (uio->uio_offset < 0 || uio->uio_offset % 4 != 0 ||
	    uio->uio_resid % 4 != 0)
		return (ENODEV);
	if ((uio->uio_offset + uio->uio_resid < 0) ||
	    (uio->uio_offset + uio->uio_resid > sc->br_size))
		return (ENODEV);
	while (uio->uio_resid > 0) {
		offset = uio->uio_offset;
		if (offset + 4 > sc->br_size)
			return (ENODEV);
		word = bus_read_4(sc->br_res, offset);
		if ((error = uiomove(&word, sizeof(word), uio)) != 0)
			return (error);
	}
	return (0);
}

void
berirom_attach(struct berirom_softc *sc)
{
	u_int32_t bdate, btime, svnrev_bcd, svnrev, word;
	struct clocktime ct;
	struct timespec ts;
	struct make_dev_args args;

	if (rman_get_size(sc->br_res) < 4) {
		device_printf(sc->br_dev, "BRAM too small to process (%d)\n",
		    sc->br_size);
		return;
	}

	make_dev_args_init(&args);
	args.mda_devsw = &berirom_cdevsw;
	args.mda_mode = S_IRUSR | S_IRGRP | S_IROTH;
	args.mda_unit = sc->br_unit;
	args.mda_si_drv1 = sc;
	make_dev_s(&args, &sc->br_cdev, "berirom%d", sc->br_unit);
	if (sc->br_unit == 0) {
		sc->br_cdev_alias = make_dev_alias(sc->br_cdev, "berirom");
		sc->br_cdev_alias->si_drv1 = sc;
	}

	word = le32toh(bus_read_4(sc->br_res, 0));
	if ((word & 0x0000FFF0) == 0x2010) {
		if (rman_get_size(sc->br_res) < 12) {
			device_printf(sc->br_dev, "BRAM seems to be v1 "
			    "format, but is too small (%d)\n", sc->br_size);
			return;
		}
		sc->br_size = MIN(20, rman_get_size(sc->br_res));
		/*
		 * bdate is the build date in BCD MMDDYYYY
		 *   i.e. 0x08302012 is August 30th, 2012
		 */
		bdate = word;
		/* XXX: use bcd2bin() */
		ct.day = ((bdate >> 28) & 0xF) * 10 + ((bdate >> 24) & 0xF);
		ct.mon = ((bdate >> 20) & 0xF) * 10 + ((bdate >> 16) & 0xF);
		ct.year = ((bdate >> 4) & 0xF) * 10 + (bdate & 0xF) +
		    ((bdate >> 12) & 0xF) * 1000 + ((bdate >> 8) & 0xF) * 100;

		/*
		 * btime is the build time in BCD 00HHMMSS
		 *   i.e. 0x00173700 is 5:37pm
		 */
		btime = le32toh(bus_read_4(sc->br_res, 4));
		ct.hour = ((btime >> 20) & 0xF) * 10 + ((btime >> 16) & 0xF);
		ct.min = ((btime >> 12) & 0xF) * 10 + ((btime >> 8) & 0xF);
		ct.sec = ((btime >> 4) & 0xF) * 10 + (btime & 0xF);
		ct.nsec = 0;
		if (clock_ct_to_ts(&ct, &ts) != 0) {
	device_printf(sc->br_dev, "Built on %d-%02d-%02d at %02d:%02d:%02d\n",
	    ct.year, ct.mon, ct.day, ct.hour, ct.min, ct.sec);
			device_printf(sc->br_dev, "Impossible date & time "
			    "in BRAM %08x%08x\n", bdate, btime);
			return;
		}

		svnrev_bcd = le32toh(bus_read_4(sc->br_res, 8));
		svnrev =
		    10000000 * ((svnrev_bcd >> 28) & 0xF) +
		    1000000 * ((svnrev_bcd >> 24) & 0xF) +
		    100000 * ((svnrev_bcd >> 20) & 0xF) +
		    10000 * ((svnrev_bcd >> 16) & 0xF) +
		    1000 * ((svnrev_bcd >> 12) & 0xF) +
		    100 * ((svnrev_bcd >> 8) & 0xF) +
		    10 * ((svnrev_bcd >> 4) & 0xF) +
		    (svnrev_bcd & 0xF);
	} else {
		device_printf(sc->br_dev, "Unknown BRAM format\n");
		return;
	}

	/*
	 * If we're here then ct, ts, and svnrev should be valid.  
	 */
	device_printf(sc->br_dev, "Built on %d-%02d-%02d at %02d:%02d:%02d "
	    "(%ld)\n",
	    ct.year, ct.mon, ct.day, ct.hour, ct.min, ct.sec, ts.tv_sec);
	device_printf(sc->br_dev, "SVN revision: r%d\n", svnrev);
}

void
berirom_detach(struct berirom_softc *sc)
{

	return;
}
