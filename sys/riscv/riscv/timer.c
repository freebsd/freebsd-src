/*-
 * Copyright (c) 2015-2025 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

/*
 * RISC-V Timer
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/intr.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/vdso.h>
#include <sys/watchdog.h>

#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/sbi.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>

struct riscv_timer_softc {
	struct resource		*irq_res;
	void			*ih;
	uint32_t		clkfreq;
	struct eventtimer	et;
};
static struct riscv_timer_softc *riscv_timer_sc = NULL;

static timecounter_get_t riscv_timer_tc_get_timecount;
static timecounter_fill_vdso_timehands_t riscv_timer_tc_fill_vdso_timehands;

static struct timecounter riscv_timer_timecount = {
	.tc_name           = "RISC-V Timecounter",
	.tc_get_timecount  = riscv_timer_tc_get_timecount,
	.tc_poll_pps       = NULL,
	.tc_counter_mask   = ~0u,
	.tc_frequency      = 0,
	.tc_quality        = 1000,
	.tc_fill_vdso_timehands = riscv_timer_tc_fill_vdso_timehands,
};

static inline uint64_t
get_timecount(void)
{

	return (rdtime());
}

static inline void
set_timecmp(uint64_t timecmp)
{

	if (has_sstc)
		csr_write(stimecmp, timecmp);
	else
		sbi_set_timer(timecmp);
}

static u_int
riscv_timer_tc_get_timecount(struct timecounter *tc __unused)
{

	return (get_timecount());
}

static uint32_t
riscv_timer_tc_fill_vdso_timehands(struct vdso_timehands *vdso_th,
    struct timecounter *tc)
{
	vdso_th->th_algo = VDSO_TH_ALGO_RISCV_RDTIME;
	bzero(vdso_th->th_res, sizeof(vdso_th->th_res));
	return (1);
}

static int
riscv_timer_et_start(struct eventtimer *et, sbintime_t first, sbintime_t period)
{
	uint64_t counts;

	if (first != 0) {
		counts = ((uint32_t)et->et_frequency * first) >> 32;
		set_timecmp(get_timecount() + counts);

		return (0);
	}

	return (EINVAL);
}

static int
riscv_timer_et_stop(struct eventtimer *et)
{

	/* Disable timer interrupts. */
	csr_clear(sie, SIE_STIE);

	return (0);
}

static int
riscv_timer_intr(void *arg)
{
	struct riscv_timer_softc *sc;

	sc = (struct riscv_timer_softc *)arg;

	if (has_sstc)
		csr_write(stimecmp, -1UL);
	else
		sbi_set_timer(-1UL);

	if (sc->et.et_active)
		sc->et.et_event_cb(&sc->et, sc->et.et_arg);

	return (FILTER_HANDLED);
}

static int
riscv_timer_get_timebase(device_t dev, uint32_t *freq)
{
	phandle_t node;
	int len;

	node = OF_finddevice("/cpus");
	if (node == -1) {
		if (bootverbose)
			device_printf(dev, "Can't find cpus node.\n");
		return (ENXIO);
	}

	len = OF_getproplen(node, "timebase-frequency");
	if (len != 4) {
		if (bootverbose)
			device_printf(dev,
			    "Can't find timebase-frequency property.\n");
		return (ENXIO);
	}

	OF_getencprop(node, "timebase-frequency", freq, len);

	return (0);
}

static int
riscv_timer_probe(device_t dev)
{

	device_set_desc(dev, "RISC-V Timer");

	return (BUS_PROBE_DEFAULT);
}

static int
riscv_timer_attach(device_t dev)
{
	struct riscv_timer_softc *sc;
	int irq, rid, error;
	phandle_t iparent;
	pcell_t cell;
	device_t rootdev;

	sc = device_get_softc(dev);
	if (riscv_timer_sc != NULL)
		return (ENXIO);

	if (device_get_unit(dev) != 0)
		return (ENXIO);

	if (riscv_timer_get_timebase(dev, &sc->clkfreq) != 0) {
		device_printf(dev, "No clock frequency specified\n");
		return (ENXIO);
	}

	riscv_timer_sc = sc;

	rootdev = intr_irq_root_device(INTR_ROOT_IRQ);
	iparent = OF_xref_from_node(ofw_bus_get_node(rootdev));
	cell = IRQ_TIMER_SUPERVISOR;
	irq = ofw_bus_map_intr(dev, iparent, 1, &cell);
	error = bus_set_resource(dev, SYS_RES_IRQ, 0, irq, 1);
	if (error != 0) {
		device_printf(dev, "Unable to register IRQ resource\n");
		return (ENXIO);
	}

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Unable to alloc IRQ resource\n");
		return (ENXIO);
	}

	/* Setup IRQs handler */
	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_CLK,
	    riscv_timer_intr, NULL, sc, &sc->ih);
	if (error != 0) {
		device_printf(dev, "Unable to setup IRQ resource\n");
		return (ENXIO);
	}

	riscv_timer_timecount.tc_frequency = sc->clkfreq;
	riscv_timer_timecount.tc_priv = sc;
	tc_init(&riscv_timer_timecount);

	sc->et.et_name = "RISC-V Eventtimer";
	sc->et.et_flags = ET_FLAGS_ONESHOT | ET_FLAGS_PERCPU;
	sc->et.et_quality = 1000;

	sc->et.et_frequency = sc->clkfreq;
	sc->et.et_min_period = (0x00000002LLU << 32) / sc->et.et_frequency;
	sc->et.et_max_period = (0xfffffffeLLU << 32) / sc->et.et_frequency;
	sc->et.et_start = riscv_timer_et_start;
	sc->et.et_stop = riscv_timer_et_stop;
	sc->et.et_priv = sc;
	et_register(&sc->et);

	set_cputicker(get_timecount, sc->clkfreq, false);

	return (0);
}

static device_method_t riscv_timer_methods[] = {
	DEVMETHOD(device_probe,		riscv_timer_probe),
	DEVMETHOD(device_attach,	riscv_timer_attach),
	{ 0, 0 }
};

static driver_t riscv_timer_driver = {
	"timer",
	riscv_timer_methods,
	sizeof(struct riscv_timer_softc),
};

EARLY_DRIVER_MODULE(timer, nexus, riscv_timer_driver, 0, 0,
    BUS_PASS_TIMER + BUS_PASS_ORDER_MIDDLE);

void
DELAY(int usec)
{
	int64_t counts, counts_per_usec;
	uint64_t first, last;

	/*
	 * Check the timers are setup, if not just
	 * use a for loop for the meantime
	 */
	if (riscv_timer_sc == NULL) {
		for (; usec > 0; usec--)
			for (counts = 200; counts > 0; counts--)
				/*
				 * Prevent the compiler from optimizing
				 * out the loop
				 */
				cpufunc_nullop();
		return;
	}
	TSENTER();

	/* Get the number of times to count */
	counts_per_usec = ((riscv_timer_timecount.tc_frequency / 1000000) + 1);

	/*
	 * Clamp the timeout at a maximum value (about 32 seconds with
	 * a 66MHz clock). *Nobody* should be delay()ing for anywhere
	 * near that length of time and if they are, they should be hung
	 * out to dry.
	 */
	if (usec >= (0x80000000U / counts_per_usec))
		counts = (0x80000000U / counts_per_usec) - 1;
	else
		counts = usec * counts_per_usec;

	first = get_timecount();

	while (counts > 0) {
		last = get_timecount();
		counts -= (int64_t)(last - first);
		first = last;
	}
	TSEXIT();
}
