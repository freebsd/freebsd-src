/*-
 * Copyright (c) 2006 Benno Rice.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <sys/timetc.h>
#include <machine/armreg.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/intr.h>

#include <arm/xscale/pxa/pxavar.h>
#include <arm/xscale/pxa/pxareg.h>

#define	PXA_TIMER_FREQUENCY	3686400
#define	PXA_TIMER_TICK		(PXA_TIMER_FREQUENCY / hz)

struct pxa_timer_softc {
	struct resource	*	pt_res[5];
	bus_space_tag_t		pt_bst;
	bus_space_handle_t	pt_bsh;
};

static struct resource_spec pxa_timer_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ SYS_RES_IRQ,		2,	RF_ACTIVE },
	{ SYS_RES_IRQ,		3,	RF_ACTIVE },
	{ -1, 0 }
};

static struct pxa_timer_softc *timer_softc = NULL;

static int	pxa_timer_probe(device_t);
static int	pxa_timer_attach(device_t);

static driver_filter_t	pxa_hardclock;

static unsigned	pxa_timer_get_timecount(struct timecounter *);

uint32_t	pxa_timer_get_osmr(int);
void		pxa_timer_set_osmr(int, uint32_t);
uint32_t	pxa_timer_get_oscr(void);
void		pxa_timer_set_oscr(uint32_t);
uint32_t	pxa_timer_get_ossr(void);
void		pxa_timer_clear_ossr(uint32_t);
void		pxa_timer_watchdog_enable(void);
void		pxa_timer_watchdog_disable(void);
void		pxa_timer_interrupt_enable(int);
void		pxa_timer_interrupt_disable(int);

static struct timecounter pxa_timer_timecounter = {
	.tc_get_timecount = pxa_timer_get_timecount,
	.tc_name = "OS Timer",
	.tc_frequency = PXA_TIMER_FREQUENCY,
	.tc_counter_mask = ~0u,
	.tc_quality = 1000,
};

static int
pxa_timer_probe(device_t dev)
{

	device_set_desc(dev, "OS Timer");
	return (0);
}

static int
pxa_timer_attach(device_t dev)
{
	int	error;
	void	*ihl;
	struct	pxa_timer_softc *sc;

	sc = (struct pxa_timer_softc *)device_get_softc(dev);

	if (timer_softc != NULL)
		return (ENXIO);

	error = bus_alloc_resources(dev, pxa_timer_spec, sc->pt_res);
	if (error) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->pt_bst = rman_get_bustag(sc->pt_res[0]);
	sc->pt_bsh = rman_get_bushandle(sc->pt_res[0]);

	timer_softc = sc;

	pxa_timer_interrupt_disable(-1);
	pxa_timer_watchdog_disable();

	if (bus_setup_intr(dev, sc->pt_res[1], INTR_TYPE_CLK,
	    pxa_hardclock, NULL, NULL, &ihl) != 0) {
		bus_release_resources(dev, pxa_timer_spec, sc->pt_res);
		device_printf(dev, "could not setup hardclock interrupt\n");
		return (ENXIO);
	}

	return (0);
}

static int
pxa_hardclock(void *arg)
{
	struct		trapframe *frame;

	frame = (struct trapframe *)arg;

	/* Clear the interrupt */
	pxa_timer_clear_ossr(OST_SR_CH0);

	/* Schedule next tick */
	pxa_timer_set_osmr(0, pxa_timer_get_oscr() + PXA_TIMER_TICK);

	/* Do what we came here for */
	hardclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
	
	return (FILTER_HANDLED);
}

static device_method_t pxa_timer_methods[] = {
	DEVMETHOD(device_probe, pxa_timer_probe),
	DEVMETHOD(device_attach, pxa_timer_attach),

	{0, 0}
};

static driver_t pxa_timer_driver = {
	"timer",
	pxa_timer_methods,
	sizeof(struct pxa_timer_softc),
};

static devclass_t pxa_timer_devclass;

DRIVER_MODULE(pxatimer, pxa, pxa_timer_driver, pxa_timer_devclass, 0, 0);

static unsigned
pxa_timer_get_timecount(struct timecounter *tc)
{

	return (pxa_timer_get_oscr());
}

void
cpu_initclocks(void)
{

	pxa_timer_set_oscr(0);
	pxa_timer_set_osmr(0, PXA_TIMER_TICK);
	pxa_timer_interrupt_enable(0);

	tc_init(&pxa_timer_timecounter);
}

void
cpu_reset(void)
{
	uint32_t	val;

	(void)disable_interrupts(PSR_I|PSR_F);

	val = pxa_timer_get_oscr();
	val += PXA_TIMER_FREQUENCY;
	pxa_timer_set_osmr(3, val);
	pxa_timer_watchdog_enable();

	for(;;);
}

void
DELAY(int usec)
{
	uint32_t	val;

	if (timer_softc == NULL) {
		for (; usec > 0; usec--)
			for (val = 100; val > 0; val--)
				;
		return;
	}

	val = pxa_timer_get_oscr();
	val += (PXA_TIMER_FREQUENCY * usec) / 1000000;
	while (pxa_timer_get_oscr() <= val);
}

uint32_t
pxa_timer_get_osmr(int which)
{

	return (bus_space_read_4(timer_softc->pt_bst,
	    timer_softc->pt_bsh, which * 0x4));
}

void
pxa_timer_set_osmr(int which, uint32_t val)
{

	bus_space_write_4(timer_softc->pt_bst,
	    timer_softc->pt_bsh, which * 0x4, val);
}

uint32_t
pxa_timer_get_oscr()
{

	return (bus_space_read_4(timer_softc->pt_bst,
	    timer_softc->pt_bsh, OST_CR));
}

void
pxa_timer_set_oscr(uint32_t val)
{

	bus_space_write_4(timer_softc->pt_bst,
	    timer_softc->pt_bsh, OST_CR, val);
}

uint32_t
pxa_timer_get_ossr()
{

	return (bus_space_read_4(timer_softc->pt_bst,
	    timer_softc->pt_bsh, OST_SR));
}

void
pxa_timer_clear_ossr(uint32_t val)
{

	bus_space_write_4(timer_softc->pt_bst,
	    timer_softc->pt_bsh, OST_SR, val);
}

void
pxa_timer_watchdog_enable()
{

	bus_space_write_4(timer_softc->pt_bst,
	    timer_softc->pt_bsh, OST_WR, 0x1);
}

void
pxa_timer_watchdog_disable()	
{

	bus_space_write_4(timer_softc->pt_bst,
	    timer_softc->pt_bsh, OST_WR, 0x0);
}

void
pxa_timer_interrupt_enable(int which)
{
	uint32_t	oier;

	if (which == -1) {
		bus_space_write_4(timer_softc->pt_bst,
		    timer_softc->pt_bsh, OST_IR, 0xf);
		return;
	}

	oier = bus_space_read_4(timer_softc->pt_bst,
	    timer_softc->pt_bsh, OST_IR);
	oier |= 1 << which;
	bus_space_write_4(timer_softc->pt_bst,
	    timer_softc->pt_bsh, OST_IR, oier);
}

void
pxa_timer_interrupt_disable(int which)
{
	uint32_t	oier;

	if (which == -1) {
		bus_space_write_4(timer_softc->pt_bst,
		    timer_softc->pt_bsh, OST_IR, 0);
	}

	oier = bus_space_read_4(timer_softc->pt_bst,
	    timer_softc->pt_bsh, OST_IR);
	oier &= ~(1 << which);
	bus_space_write_4(timer_softc->pt_bst,
	    timer_softc->pt_bsh, OST_IR, oier);
}
