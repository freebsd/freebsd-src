/*-
 * Copyright (c) 2010 Yohanes Nugroho.  All rights reserved.
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

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/frame.h>
#include <machine/intr.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91_pitreg.h>
#include <arm/at91/at91_pmcvar.h>
#include <arm/at91/at91sam9g20reg.h>

__FBSDID("$FreeBSD$");

static struct at91pit_softc {
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	device_t		sc_dev;
} *pit_softc;

#define RD4(off) \
	bus_space_read_4(pit_softc->sc_st, pit_softc->sc_sh, (off))
#define WR4(off, val) \
	bus_space_write_4(pit_softc->sc_st, pit_softc->sc_sh, (off), (val))

static unsigned at91pit_get_timecount(struct timecounter *tc);
static int clock_intr(void *arg);

static struct timecounter at91pit_timecounter = {
	at91pit_get_timecount, /* get_timecount */
	NULL, /* no poll_pps */
	0xffffffffu, /* counter mask */
	0, /* frequency */
	"AT91SAM9261 timer", /* name */
	1000 /* quality */
};


uint32_t 
at91_pit_base(void);

uint32_t
at91_pit_size(void);

static int
at91pit_probe(device_t dev)
{
	device_set_desc(dev, "PIT");
	return (0);
}

uint32_t 
at91_pit_base(void)
{
	return (AT91SAM9G20_PIT_BASE);
}

uint32_t
at91_pit_size(void)
{
	return (AT91SAM9G20_PIT_SIZE);
}

static int pit_rate;
static int pit_cycle;
static int pit_counter;

static int
at91pit_attach(device_t dev)
{
	struct at91_softc *sc = device_get_softc(device_get_parent(dev));
	struct resource *irq;
	int rid = 0;
	void *ih;

	pit_softc = device_get_softc(dev);
	pit_softc->sc_st = sc->sc_st;
	pit_softc->sc_dev = dev;
	if (bus_space_subregion(sc->sc_st, sc->sc_sh, at91_pit_base(),
	    at91_pit_size(), &pit_softc->sc_sh) != 0)
	       panic("couldn't subregion pit registers");

	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 1, 1, 1,
	  RF_ACTIVE | RF_SHAREABLE);
	if (!irq)
		panic("Unable to allocate IRQ for the system timer");
	else
		bus_setup_intr(dev, irq, INTR_TYPE_CLK,
			clock_intr, NULL, NULL, &ih);
       

	device_printf(dev, "AT91SAM9x pit registered\n");
	return (0);
}

static device_method_t at91pit_methods[] = {
	DEVMETHOD(device_probe, at91pit_probe),
	DEVMETHOD(device_attach, at91pit_attach),
	{0,0},
};

static driver_t at91pit_driver = {
	"at91_pit",
	at91pit_methods,
	sizeof(struct at91pit_softc),
};

static devclass_t at91pit_devclass;

DRIVER_MODULE(at91_pit, atmelarm, at91pit_driver, at91pit_devclass, 0, 0);

static int
clock_intr(void *arg)
{

	struct trapframe *fp = arg;

	if (RD4(PIT_SR) & PIT_PITS_DONE) {
		uint32_t pivr = RD4(PIT_PIVR);
		if (PIT_CNT(pivr)>1) {
			printf("cnt = %d\n", PIT_CNT(pivr));
		}
		pit_counter += pit_cycle;
		hardclock(TRAPF_USERMODE(fp), TRAPF_PC(fp));
		return (FILTER_HANDLED);
	}
	return (FILTER_STRAY);
}

static unsigned 
at91pit_get_timecount(struct timecounter *tc)
{
	return pit_counter;
}

/*todo: review this*/
void
DELAY(int n)
{
	u_int32_t start, end, cur;

	start = RD4(PIT_PIIR);
	n = (n * 1000) / (at91_master_clock / 12);
	if (n <= 0)
		n = 1;
	end = (start + n);
	cur = start;
	if (start > end) {
		while (cur >= start || cur < end)
			cur = RD4(PIT_PIIR);
	} else {
		while (cur < end)
			cur = RD4(PIT_PIIR);
	}
}

/*
 * The 3 next functions must be implement with the futur PLL code.
 */
void
cpu_startprofclock(void)
{
}

void
cpu_stopprofclock(void)
{
}

#define HZ 100

void
cpu_initclocks(void)
{
	struct at91_pmc_clock *master;

	master = at91_pmc_clock_ref("mck");
	pit_rate =  master->hz / 16;
	pit_cycle = (pit_rate + HZ/2) / HZ;	
	at91pit_timecounter.tc_frequency = pit_rate;
	WR4(PIT_MR, 0);

	while (PIT_PIV(RD4(PIT_PIVR)) != 0);
		
	WR4(PIT_MR, (pit_cycle - 1) | PIT_IEN | PIT_EN);
	tc_init(&at91pit_timecounter);
}

void
cpu_reset(void)
{
	*(volatile int *)(AT91SAM9G20_BASE + AT91SAM9G20_RSTC_BASE +
	    RSTC_CR) = RSTC_PROCRST | RSTC_PERRST | RSTC_KEY;
	while (1)
		continue;
}
