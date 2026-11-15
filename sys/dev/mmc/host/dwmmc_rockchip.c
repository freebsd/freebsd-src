/*
 * Copyright 2017 Emmanuel Vadot <manu@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmc_fdt_helpers.h>

#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>

#include <dev/mmc/host/dwmmc_var.h>

#include "opt_mmccam.h"

enum RKTYPE {
	RK2928 = 1,
	RK3288,
	RK3328,
};

static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk2928-dw-mshc",	RK2928},
	{"rockchip,rk3288-dw-mshc",	RK3288},
	{"rockchip,rk3328-dw-mshc",	RK3328},
	{NULL,				0},
};

/* Number of sample phases tried while tuning. */
#define	RK_TUNING_NUM_PHASES		360
#define	RK_TUNING_ITER_TO_PHASE(i)	howmany((i) * 360, RK_TUNING_NUM_PHASES)

struct rockchip_dwmmc_softc {
	struct dwmmc_softc	sc;		/* Must be first */
	clk_t			drv_clk;	/* Output (drive) phase clock */
	clk_t			sample_clk;	/* Input (sample) phase clock */
	int			drv_phase;	/* Current phases, -1: unknown */
	int			sample_phase;
	int			def_drv_phase;	/* Phases at attach time */
	int			def_sample_phase;
};

static int dwmmc_rockchip_update_ios(struct dwmmc_softc *sc, struct mmc_ios *ios);
#ifdef MMCCAM
static int dwmmc_rockchip_execute_tuning(struct dwmmc_softc *sc,
    uint32_t opcode);
#endif

static int
rockchip_dwmmc_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Synopsys DesignWare Mobile "
	    "Storage Host Controller (RockChip)");

	return (BUS_PROBE_VENDOR);
}

static int
rockchip_dwmmc_attach(device_t dev)
{
	struct rockchip_dwmmc_softc *rsc;
	struct dwmmc_softc *sc;
	int type;
	int rc;

	rsc = device_get_softc(dev);
	sc = &rsc->sc;
	sc->hwtype = HWTYPE_ROCKCHIP;
	type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	switch (type) {
	case RK2928:
		sc->use_pio = 1;
		break;
	}

	rsc->drv_phase = rsc->sample_phase = -1;
	rsc->def_drv_phase = rsc->def_sample_phase = 0;
	sc->update_ios = &dwmmc_rockchip_update_ios;
	rc = dwmmc_attach(dev);

	/*
	 * HS200 needs the sample phase to be tuned, which requires both the
	 * drive and the sample phase clocks.  Only the MMCCAM build can
	 * tune.
	 */
#ifdef MMCCAM
	if (rc == 0 &&
	    clk_get_by_ofw_name(dev, 0, "ciu-drive", &rsc->drv_clk) == 0 &&
	    clk_get_by_ofw_name(dev, 0, "ciu-sample", &rsc->sample_clk) == 0 &&
	    clk_get_phase(rsc->drv_clk, &rsc->def_drv_phase) == 0 &&
	    clk_get_phase(rsc->sample_clk, &rsc->def_sample_phase) == 0) {
		/*
		 * Remember the phases the controller starts with: legacy
		 * speeds keep using them, and they are restored if tuning
		 * fails.
		 */
		if (bootverbose)
			device_printf(dev, "default phases: drive %d, "
			    "sample %d\n", rsc->def_drv_phase,
			    rsc->def_sample_phase);
		sc->execute_tuning = &dwmmc_rockchip_execute_tuning;
		return (rc);
	}
#endif
	device_printf(dev, "Disabling HS200+ (no tuning support)\n");
	sc->host.caps &= ~(MMC_CAP_MMC_HS200 | MMC_CAP_MMC_HS400);

	return (rc);
}

static int
dwmmc_rockchip_update_ios(struct dwmmc_softc *sc, struct mmc_ios *ios)
{
	struct rockchip_dwmmc_softc *rsc;
	unsigned int clock;
	int error;

	rsc = (struct rockchip_dwmmc_softc *)sc;

	if (ios->clock && ios->clock != sc->bus_hz) {
		sc->bus_hz = clock = ios->clock;
		/* Set the MMC clock. */
		if (sc->ciu) {
			/*
			 * Apparently you need to set the ciu clock to
			 * the double of bus_hz
			 */
			error = clk_set_freq(sc->ciu, clock * 2,
			    CLK_SET_ROUND_DOWN);
			if (error != 0) {
				device_printf(sc->dev,
				    "failed to set frequency to %u Hz: %d\n",
				    clock, error);
				return (error);
			}
			if (bootverbose) {
				uint64_t actual;

				if (clk_get_freq(sc->ciu, &actual) == 0)
					device_printf(sc->dev,
					    "ciu clock %ju Hz for bus clock "
					    "%u Hz\n", (uintmax_t)actual,
					    clock);
			}
		}
	}

	/*
	 * Only HS200 needs the phases to be set: the legacy speeds keep the
	 * phases the controller starts with.  The drive phase is not tuned,
	 * a fixed offset provides enough hold time.  At up to 150MHz, 180
	 * degrees gives more margin than the usual 90 degrees.
	 */
	if (rsc->drv_clk != NULL && ios->clock != 0 &&
	    (ios->timing == bus_timing_mmc_hs200 ||
	    ios->timing == bus_timing_uhs_sdr104) &&
	    rsc->drv_phase != 180) {
		error = clk_set_phase(rsc->drv_clk, 180);
		if (error != 0)
			device_printf(sc->dev,
			    "failed to set drive phase 180: %d\n", error);
		else
			rsc->drv_phase = 180;
	}
	return (0);
}

#ifdef MMCCAM
struct rk_phase_range {
	int	start;
	int	end;
};

/* Put back the phases the controller started with, after a failed tuning. */
static void
dwmmc_rockchip_restore_phases(struct rockchip_dwmmc_softc *rsc)
{

	rsc->sample_phase = -1;
	if (clk_set_phase(rsc->sample_clk, rsc->def_sample_phase) == 0)
		rsc->sample_phase = rsc->def_sample_phase;
	rsc->drv_phase = -1;
	if (clk_set_phase(rsc->drv_clk, rsc->def_drv_phase) == 0)
		rsc->drv_phase = rsc->def_drv_phase;
}

/*
 * Find the sample phase to use for HS200: try each phase, collect the ranges
 * of phases where the tuning block is received intact and pick the middle of
 * the longest one.
 */
static int
dwmmc_rockchip_execute_tuning(struct dwmmc_softc *sc, uint32_t opcode)
{
	struct rockchip_dwmmc_softc *rsc;
	struct rk_phase_range ranges[RK_TUNING_NUM_PHASES / 2 + 1];
	int longest, longest_len, middle, phase, i, nranges, error;
	bool good, prev_good;
	int ntimeout = 0, nio = 0, nbad = 0;

	rsc = (struct rockchip_dwmmc_softc *)sc;

	nranges = 0;
	prev_good = false;

	for (i = 0; i < RK_TUNING_NUM_PHASES; ) {
		rsc->sample_phase = -1;
		error = clk_set_phase(rsc->sample_clk,
		    RK_TUNING_ITER_TO_PHASE(i));
		if (error != 0) {
			device_printf(sc->dev,
			    "cannot set sample phase %d: %d\n",
			    RK_TUNING_ITER_TO_PHASE(i), error);
			dwmmc_rockchip_restore_phases(rsc);
			return (error);
		}
		error = dwmmc_send_tuning(sc, opcode);
		good = error == 0;
		if (error == ETIMEDOUT)
			ntimeout++;
		else if (error == EIO)
			nio++;
		else if (error == EBADMSG)
			nbad++;

		if (good && !prev_good) {
			ranges[nranges].start = i;
			ranges[nranges].end = i;
			nranges++;
		} else if (good) {
			ranges[nranges - 1].end = i;
		}
		prev_good = good;
		if (good || i == RK_TUNING_NUM_PHASES - 1) {
			i++;
		} else {
			/*
			 * Testing a bad phase is slow, so skip 20 degrees
			 * after one.  The edges will be off by a few degrees,
			 * which doesn't matter because we pick the middle.
			 */
			i += howmany(20 * RK_TUNING_NUM_PHASES, 360);
			if (i >= RK_TUNING_NUM_PHASES)
				i = RK_TUNING_NUM_PHASES - 1;
		}
	}

	if (bootverbose) {
		device_printf(sc->dev, "tuning failures: %d cmd timeout, "
		    "%d data error, %d wrong pattern\n", ntimeout, nio, nbad);
		for (i = 0; i < nranges; i++)
			device_printf(sc->dev, "good phase range %d-%d\n",
			    RK_TUNING_ITER_TO_PHASE(ranges[i].start),
			    RK_TUNING_ITER_TO_PHASE(ranges[i].end));
	}

	if (nranges == 0) {
		device_printf(sc->dev, "All phases bad!\n");
		/* Don't leave a bad phase behind for the fallback speed. */
		dwmmc_rockchip_restore_phases(rsc);
		return (EIO);
	}

	/* All phases work: use the default one. */
	if (ranges[0].start == 0 &&
	    ranges[0].end == RK_TUNING_NUM_PHASES - 1) {
		if (clk_set_phase(rsc->sample_clk,
		    rsc->def_sample_phase) == 0)
			rsc->sample_phase = rsc->def_sample_phase;
		if (bootverbose)
			device_printf(sc->dev,
			    "All phases work, using default phase %d\n",
			    rsc->def_sample_phase);
		return (0);
	}

	/* The range that ends at the last phase wraps around to the first. */
	if (nranges > 1 && ranges[0].start == 0 &&
	    ranges[nranges - 1].end == RK_TUNING_NUM_PHASES - 1) {
		ranges[0].start = ranges[nranges - 1].start -
		    RK_TUNING_NUM_PHASES;
		nranges--;
	}

	longest = 0;
	longest_len = 0;
	for (i = 0; i < nranges; i++) {
		int len = ranges[i].end - ranges[i].start + 1;

		if (len > longest_len) {
			longest = i;
			longest_len = len;
		}
	}

	middle = ranges[longest].start + longest_len / 2;
	middle %= RK_TUNING_NUM_PHASES;
	if (middle < 0)
		middle += RK_TUNING_NUM_PHASES;
	phase = RK_TUNING_ITER_TO_PHASE(middle);

	error = clk_set_phase(rsc->sample_clk, phase);
	if (error != 0) {
		dwmmc_rockchip_restore_phases(rsc);
		return (error);
	}
	rsc->sample_phase = phase;
	if (bootverbose)
		device_printf(sc->dev,
		    "Successfully tuned phase to %d\n", phase);
	return (0);
}
#endif /* MMCCAM */

static device_method_t rockchip_dwmmc_methods[] = {
	/* bus interface */
	DEVMETHOD(device_probe, rockchip_dwmmc_probe),
	DEVMETHOD(device_attach, rockchip_dwmmc_attach),
	DEVMETHOD(device_detach, dwmmc_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rockchip_dwmmc, rockchip_dwmmc_driver, rockchip_dwmmc_methods,
    sizeof(struct rockchip_dwmmc_softc), dwmmc_driver);

DRIVER_MODULE(rockchip_dwmmc, simplebus, rockchip_dwmmc_driver, 0, 0);
DRIVER_MODULE(rockchip_dwmmc, ofwbus, rockchip_dwmmc_driver, NULL, NULL);
#ifndef MMCCAM
MMC_DECLARE_BRIDGE(rockchip_dwmmc);
#endif
