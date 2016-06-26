/*-
 * Copyright (c) 2009 Gallon Sylvestre.  All rights reserved.
 * Copyright (c) 2010 Greg Ansley.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <sys/systm.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91_pitreg.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#ifndef PIT_PRESCALE
#define PIT_PRESCALE (16)
#endif

static struct pit_softc {
	struct resource	*mem_res;	/* Memory resource */
	void		*intrhand;	/* Interrupt handle */
	device_t	sc_dev;
} *sc;

static uint32_t timecount = 0;
static unsigned at91_pit_get_timecount(struct timecounter *tc);
static int pit_intr(void *arg);

static inline uint32_t
RD4(struct pit_softc *sc, bus_size_t off)
{

	return (bus_read_4(sc->mem_res, off));
}

static inline void
WR4(struct pit_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, val);
}

void
at91_pit_delay(int us)
{
	int32_t cnt, last, piv;
	uint64_t pit_freq;
	const uint64_t mhz  = 1E6;

	if (sc == NULL)
		return;

	last = PIT_PIV(RD4(sc, PIT_PIIR));

	/* Max delay ~= 260s. @ 133Mhz */
	pit_freq = at91_master_clock / PIT_PRESCALE;
	cnt  = howmany(pit_freq * us, mhz);
	cnt  = (cnt <= 0) ? 1 : cnt;

	while (cnt > 0) {
		piv = PIT_PIV(RD4(sc, PIT_PIIR));
			cnt  -= piv - last ;
		if (piv < last)
			cnt -= PIT_PIV(~0u) - last;
		last = piv;
	}
}

static struct timecounter at91_pit_timecounter = {
	at91_pit_get_timecount, /* get_timecount */
	NULL, /* no poll_pps */
	0xffffffff, /* counter mask */
	0 / PIT_PRESCALE, /* frequency */
	"AT91SAM9 timer", /* name */
	1000 /* quality */
};

static int
at91_pit_probe(device_t dev)
{
#ifdef FDT
	if (!ofw_bus_is_compatible(dev, "atmel,at91sam9260-pit"))
		return (ENXIO);
#endif
	device_set_desc(dev, "AT91SAM9 PIT");
        return (0);
}

static int
at91_pit_attach(device_t dev)
{
	void *ih;
	int rid, err = 0;
	struct resource *irq;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);

	if (sc->mem_res == NULL)
		panic("couldn't allocate register resources");

	rid = 0;
	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 1, 1, 1,
	    RF_ACTIVE | RF_SHAREABLE);
	if (!irq) {
		device_printf(dev, "could not allocate interrupt resources.\n");
		err = ENOMEM;
		goto out;
	}

	/* Activate the interrupt. */
	err = bus_setup_intr(dev, irq, INTR_TYPE_CLK, pit_intr, NULL, NULL,
	    &ih);

	at91_pit_timecounter.tc_frequency =  at91_master_clock / PIT_PRESCALE;
	tc_init(&at91_pit_timecounter);

	/* Enable the PIT here. */
	WR4(sc, PIT_MR, PIT_PIV(at91_master_clock / PIT_PRESCALE / hz) |
	    PIT_EN | PIT_IEN);
out:
	return (err);
}


static int
pit_intr(void *arg)
{
	struct trapframe *fp = arg;
	uint32_t icnt;

	if (RD4(sc, PIT_SR) & PIT_PITS_DONE) {
		icnt = RD4(sc, PIT_PIVR) >> 20;

		/* Just add in the overflows we just read */
		timecount +=  PIT_PIV(RD4(sc, PIT_MR)) * icnt;

		hardclock(TRAPF_USERMODE(fp), TRAPF_PC(fp));
		return (FILTER_HANDLED);
	}
	return (FILTER_STRAY);
}

static unsigned
at91_pit_get_timecount(struct timecounter *tc)
{
	uint32_t piir, icnt;

	piir = RD4(sc, PIT_PIIR); /* Current  count | over flows */
	icnt = piir >> 20;	/* Overflows */
	return (timecount + PIT_PIV(piir) + PIT_PIV(RD4(sc, PIT_MR)) * icnt);
}

static device_method_t at91_pit_methods[] = {
	DEVMETHOD(device_probe, at91_pit_probe),
	DEVMETHOD(device_attach, at91_pit_attach),
	DEVMETHOD_END
};

static driver_t at91_pit_driver = {
	"at91_pit",
	at91_pit_methods,
	sizeof(struct pit_softc),
};

static devclass_t at91_pit_devclass;

#ifdef FDT
EARLY_DRIVER_MODULE(at91_pit, simplebus, at91_pit_driver, at91_pit_devclass,
    NULL, NULL, BUS_PASS_TIMER);
#else
EARLY_DRIVER_MODULE(at91_pit, atmelarm, at91_pit_driver, at91_pit_devclass,
    NULL, NULL, BUS_PASS_TIMER);
#endif
