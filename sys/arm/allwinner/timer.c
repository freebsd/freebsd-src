/*-
 * Copyright (c) 2012 Ganbold Tsagaankhuu <ganbold@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.	Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2.	Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <sys/kdb.h>

/**
 * Timer registers addr
 *
 */
#define SW_TIMER_IRQ_EN_REG 	0x00
#define SW_TIMER_IRQ_STA_REG 	0x04
#define SW_TIMER0_CTRL_REG 	0x10
#define SW_TIMER0_INT_VALUE_REG	0x14
#define SW_TIMER0_CUR_VALUE_REG	0x18

#define SYS_TIMER_SCAL		16 /* timer clock source pre-divsion */
#define SYS_TIMER_CLKSRC	24000000 /* timer clock source */
#define TMR_INTER_VAL		SYS_TIMER_CLKSRC/(SYS_TIMER_SCAL * 1000)

#define CLOCK_TICK_RATE		TMR_INTER_VAL 
#define INITIAL_TIMECOUNTER	(0xffffffff)

struct a10_timer_softc {
	device_t 	sc_dev;
	struct resource *res[2];
	bus_space_tag_t sc_bst;
	bus_space_handle_t sc_bsh;
	void 		*sc_ih;		/* interrupt handler */
	uint32_t 	sc_period;
	uint32_t 	clkfreq;
	struct eventtimer et;
};

int a10_timer_get_timerfreq(struct a10_timer_softc *);

#define timer_read_4(sc, reg)	\
	bus_space_read_4(sc->sc_bst, sc->sc_bsh, reg)
#define timer_write_4(sc, reg, val)	\
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, reg, val)

static u_int	a10_timer_get_timecount(struct timecounter *);
static int	a10_timer_timer_start(struct eventtimer *,
    struct bintime *, struct bintime *);
static int	a10_timer_timer_stop(struct eventtimer *);

static int a10_timer_initialized = 0;
static int a10_timer_intr(void *);
static int a10_timer_probe(device_t);
static int a10_timer_attach(device_t);

static struct timecounter a10_timer_timecounter = {
	.tc_name           = "a10_timer timer0",
	.tc_get_timecount  = a10_timer_get_timecount,
	.tc_counter_mask   = ~0u,
	.tc_frequency      = 0,
	.tc_quality        = 1000,
};

struct a10_timer_softc *a10_timer_sc = NULL;

static struct resource_spec a10_timer_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static int
a10_timer_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "a10,timers"))
		return (ENXIO);

	device_set_desc(dev, "Allwinner A10 timer");
	return (BUS_PROBE_DEFAULT);
}

static int
a10_timer_attach(device_t dev)
{
	struct a10_timer_softc *sc;
	int err;
	uint32_t val;
	uint32_t freq;

	sc = device_get_softc(dev);

	if (bus_alloc_resources(dev, a10_timer_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->sc_dev = dev;
	sc->sc_bst = rman_get_bustag(sc->res[0]);
	sc->sc_bsh = rman_get_bushandle(sc->res[0]);

	/* set interval */
	timer_write_4(sc, SW_TIMER0_INT_VALUE_REG, TMR_INTER_VAL);

	/* set clock source to HOSC, 16 pre-division */
	val = timer_read_4(sc, SW_TIMER0_CTRL_REG);
	val &= ~(0x07<<4);
	val &= ~(0x03<<2);
	val |= (4<<4) | (1<<2);
	timer_write_4(sc, SW_TIMER0_CTRL_REG, val);

	/* set mode to auto reload */
	val = timer_read_4(sc, SW_TIMER0_CTRL_REG);
	val |= (1<<1);
	timer_write_4(sc, SW_TIMER0_CTRL_REG, val);

	/* Enable timer0 */
	val = timer_read_4(sc, SW_TIMER_IRQ_EN_REG);
	val |= (1<<0);
	timer_write_4(sc, SW_TIMER_IRQ_EN_REG, val);

	/* Setup and enable the timer interrupt */
	err = bus_setup_intr(dev, sc->res[1], INTR_TYPE_CLK, a10_timer_intr,
	    NULL, sc, &sc->sc_ih);
	if (err != 0) {
		bus_release_resources(dev, a10_timer_spec, sc->res);
		device_printf(dev, "Unable to setup the clock irq handler, "
		    "err = %d\n", err);
		return (ENXIO);
	}
	freq = SYS_TIMER_CLKSRC;

        /* Set desired frequency in event timer and timecounter */
	sc->et.et_frequency = (uint64_t)freq;
	sc->clkfreq = (uint64_t)freq;
	sc->et.et_name = "a10_timer Eventtimer";
	sc->et.et_flags = ET_FLAGS_ONESHOT | ET_FLAGS_PERIODIC;
	sc->et.et_quality = 1000;
	sc->et.et_min_period.sec = 0;
	sc->et.et_min_period.frac =
	    ((0x00000002LLU << 32) / sc->et.et_frequency) << 32;
	sc->et.et_max_period.sec = 0xfffffff0U / sc->et.et_frequency;
	sc->et.et_max_period.frac =
	    ((0xfffffffeLLU << 32) / sc->et.et_frequency) << 32;
	sc->et.et_start = a10_timer_timer_start;
	sc->et.et_stop = a10_timer_timer_stop;
	sc->et.et_priv = sc;
	et_register(&sc->et);

	if (device_get_unit(dev) == 0)
		a10_timer_sc = sc;

	a10_timer_timecounter.tc_frequency = (uint64_t)freq;
	tc_init(&a10_timer_timecounter);

	printf("clock: hz=%d stathz = %d\n", hz, stathz);

	device_printf(sc->sc_dev, "timer clock frequency %d\n", sc->clkfreq);

	a10_timer_initialized = 1;
	
	return (0);
}

static int
a10_timer_timer_start(struct eventtimer *et, struct bintime *first,
    struct bintime *period)
{
	struct a10_timer_softc *sc;
	uint32_t clo, count;

	sc = (struct a10_timer_softc *)et->et_priv;

	if (first != NULL) {
		count = (sc->et.et_frequency * (first->frac >> 32)) >> 32;
		if (first->sec != 0)
			count += sc->et.et_frequency * first->sec;

		/* clear */
		timer_write_4(sc, SW_TIMER0_CUR_VALUE_REG, 0);
		clo = timer_read_4(sc, SW_TIMER0_CUR_VALUE_REG);
		clo += count;
		timer_write_4(sc, SW_TIMER0_CUR_VALUE_REG, clo);

		return (0);
	}

	return (EINVAL);
}

static int
a10_timer_timer_stop(struct eventtimer *et)
{
	struct a10_timer_softc *sc;
	uint32_t val;

	sc = (struct a10_timer_softc *)et->et_priv;

	/* clear */
	timer_write_4(sc, SW_TIMER0_CUR_VALUE_REG, 0);

	/* disable */
	val = timer_read_4(sc, SW_TIMER0_CTRL_REG);
	val &= ~(1<<0); /* Disable timer0 */
	timer_write_4(sc, SW_TIMER0_CTRL_REG, val);

	sc->sc_period = 0;

	return (0);
}

int
a10_timer_get_timerfreq(struct a10_timer_softc *sc)
{

	return (sc->clkfreq);
}

void
cpu_initclocks(void)
{
	cpu_initclocks_bsp();
}

static int
a10_timer_intr(void *arg)
{
	struct a10_timer_softc *sc;

	sc = (struct a10_timer_softc *)arg;

	if (sc->et.et_active)
		sc->et.et_event_cb(&sc->et, sc->et.et_arg);

	/* pending */
	timer_write_4(sc, SW_TIMER_IRQ_STA_REG, 0x1);

	return (FILTER_HANDLED);
}

u_int
a10_timer_get_timecount(struct timecounter *tc)
{

	if (a10_timer_sc == NULL)
		return (0);

	return (timer_read_4(a10_timer_sc, SW_TIMER0_CUR_VALUE_REG));
}

static device_method_t a10_timer_methods[] = {
	DEVMETHOD(device_probe,		a10_timer_probe),
	DEVMETHOD(device_attach,	a10_timer_attach),

	DEVMETHOD_END
};

static driver_t a10_timer_driver = {
	"a10_timer",
	a10_timer_methods,
	sizeof(struct a10_timer_softc),
};

static devclass_t a10_timer_devclass;

DRIVER_MODULE(a10_timer, simplebus, a10_timer_driver, a10_timer_devclass, 0, 0);

void
DELAY(int usec)
{
	uint32_t counter;
	uint32_t val, val_temp;
	int32_t nticks;

	/* Timer is not initialized yet */
	if (!a10_timer_initialized) {
		for (; usec > 0; usec--)
			for (counter = 200; counter > 0; counter--)
				/* Prevent optimizing out the loop */
				cpufunc_nullop();
		return;
	}

	val = timer_read_4(a10_timer_sc, SW_TIMER0_CUR_VALUE_REG);
        nticks = ((a10_timer_sc->clkfreq / 1000000 + 1) * usec);

        while (nticks > 0) {
                val_temp = timer_read_4(a10_timer_sc, SW_TIMER0_CUR_VALUE_REG);
                if (val > val_temp)
                        nticks -= (val - val_temp);
                else
                        nticks -= (val + (INITIAL_TIMECOUNTER - val_temp));

                val = val_temp;
        }
}

