/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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

#include "opt_at91.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/timetc.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <arm/at91/at91rm92reg.h>

#include <arm/at91/at91_pmcreg.h>
#include <arm/at91/at91_pmcvar.h>

static struct at91_pmc_softc {
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	struct resource	*mem_res;	/* Memory resource */
	device_t		dev;
	unsigned int		main_clock_hz;
	uint32_t		pllb_init;
} *pmc_softc;

static void at91_pmc_set_pllb_mode(struct at91_pmc_clock *, int);
static void at91_pmc_set_sys_mode(struct at91_pmc_clock *, int);
static void at91_pmc_set_periph_mode(struct at91_pmc_clock *, int);

static struct at91_pmc_clock slck = {
	.name = "slck",		// 32,768 Hz slow clock
	.hz = 32768,
	.refcnt = 1,
	.id = 0,
	.primary = 1,
};

static struct at91_pmc_clock main_ck = {
	.name = "main",		// Main clock
	.refcnt = 0,
	.id = 1,
	.primary = 1,
	.pmc_mask = PMC_IER_MOSCS,
};

static struct at91_pmc_clock plla = {
	.name = "plla",		// PLLA Clock, used for CPU clocking
	.parent = &main_ck,
	.refcnt = 1,
	.id = 0,
	.primary = 1,
	.pll = 1,
	.pmc_mask = PMC_IER_LOCKA,
};

static struct at91_pmc_clock pllb = {
	.name = "pllb",		// PLLB Clock, used for USB functions
	.parent = &main_ck,
	.refcnt = 0,
	.id = 0,
	.primary = 1,
	.pll = 1,
	.pmc_mask = PMC_IER_LOCKB,
	.set_mode = &at91_pmc_set_pllb_mode,
};

static struct at91_pmc_clock udpck = {
	.name = "udpck",
	.parent = &pllb,
	.pmc_mask = PMC_SCER_UDP,
	.set_mode = at91_pmc_set_sys_mode
};

static struct at91_pmc_clock uhpck = {
	.name = "uhpck",
	.parent = &pllb,
	.pmc_mask = PMC_SCER_UHP,
	.set_mode = at91_pmc_set_sys_mode
};

static struct at91_pmc_clock mck = {
	.name = "mck",
	.pmc_mask = PMC_IER_MCKRDY,
	.refcnt = 0,
};

static struct at91_pmc_clock udc_clk = {
	.name = "udc_clk",
	.parent = &mck,
	.pmc_mask = 1 << AT91RM92_IRQ_UDP,
	.set_mode = &at91_pmc_set_periph_mode
};

static struct at91_pmc_clock ohci_clk = {
	.name = "ohci_clk",
	.parent = &mck,
	.pmc_mask = 1 << AT91RM92_IRQ_UHP,
	.set_mode = &at91_pmc_set_periph_mode
};

static struct at91_pmc_clock *const clock_list[] = {
	&slck,
	&main_ck,
	&plla,
	&pllb,
	&udpck,
	&uhpck,
	&mck,
	&udc_clk,
	&ohci_clk
};

#if !defined(AT91C_MAIN_CLOCK)
static const unsigned int at91_mainf_tbl[] = {
	3000000, 3276800, 3686400, 3840000, 4000000,
	4433619, 4915200, 5000000, 5242880, 6000000,
	6144000, 6400000, 6553600, 7159090, 7372800,
	7864320, 8000000, 9830400, 10000000, 11059200,
	12000000, 12288000, 13560000, 14318180, 14745600,
	16000000, 17344700, 18432000, 20000000
};
#define	MAINF_TBL_LEN	(sizeof(at91_mainf_tbl) / sizeof(*at91_mainf_tbl))
#endif

static inline uint32_t
RD4(struct at91_pmc_softc *sc, bus_size_t off)
{
	return bus_read_4(sc->mem_res, off);
}

static inline void
WR4(struct at91_pmc_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->mem_res, off, val);
}

static void
at91_pmc_set_pllb_mode(struct at91_pmc_clock *clk, int on)
{
	struct at91_pmc_softc *sc = pmc_softc;
	uint32_t value;

	if (on) {
		on = PMC_IER_LOCKB;
		value = sc->pllb_init;
	} else {
		value = 0;
	}
	WR4(sc, CKGR_PLLBR, value);
	while ((RD4(sc, PMC_SR) & PMC_IER_LOCKB) != on)
		continue;
}

static void
at91_pmc_set_sys_mode(struct at91_pmc_clock *clk, int on)
{
	struct at91_pmc_softc *sc = pmc_softc;

	WR4(sc, on ? PMC_SCER : PMC_SCDR, clk->pmc_mask);
	if (on)
		while ((RD4(sc, PMC_SCSR) & clk->pmc_mask) != clk->pmc_mask)
			continue;
	else
		while ((RD4(sc, PMC_SCSR) & clk->pmc_mask) == clk->pmc_mask)
			continue;
}

static void
at91_pmc_set_periph_mode(struct at91_pmc_clock *clk, int on)
{
	struct at91_pmc_softc *sc = pmc_softc;

	WR4(sc, on ? PMC_PCER : PMC_PCDR, clk->pmc_mask);
	if (on)
		while ((RD4(sc, PMC_PCSR) & clk->pmc_mask) != clk->pmc_mask)
			continue;
	else
		while ((RD4(sc, PMC_PCSR) & clk->pmc_mask) == clk->pmc_mask)
			continue;
}

struct at91_pmc_clock *
at91_pmc_clock_ref(const char *name)
{
	int i;

	for (i = 0; i < sizeof(clock_list) / sizeof(clock_list[0]); i++)
		if (strcmp(name, clock_list[i]->name) == 0)
			return (clock_list[i]);

	return (NULL);
}

void
at91_pmc_clock_deref(struct at91_pmc_clock *clk)
{
}

void
at91_pmc_clock_enable(struct at91_pmc_clock *clk)
{
	/* XXX LOCKING? XXX */
	if (clk->parent)
		at91_pmc_clock_enable(clk->parent);
	if (clk->refcnt++ == 0 && clk->set_mode)
		clk->set_mode(clk, 1);
}

void
at91_pmc_clock_disable(struct at91_pmc_clock *clk)
{
	/* XXX LOCKING? XXX */
	if (--clk->refcnt == 0 && clk->set_mode)
		clk->set_mode(clk, 0);
	if (clk->parent)
		at91_pmc_clock_disable(clk->parent);
}

static int
at91_pmc_pll_rate(int freq, uint32_t reg, int is_pllb)
{
	uint32_t mul, div;

	div = reg & 0xff;
	mul = (reg >> 16) & 0x7ff;
	if (div != 0 && mul != 0) {
		freq /= div;
		freq *= mul + 1;
	} else {
		freq = 0;
	}
	if (is_pllb && (reg & (1 << 28)))
		freq >>= 1;
	return (freq);
}

static uint32_t
at91_pmc_pll_calc(uint32_t main_freq, uint32_t out_freq)
{
	uint32_t i, div = 0, mul = 0, diff = 1 << 30;
	unsigned ret = (out_freq > PMC_PLL_FAST_THRESH) ? 0xbe00 : 0x3e00; 

	if (out_freq > PMC_PLL_MAX_OUT_FREQ)
		goto fail;

	for (i = 1; i < 256; i++) {
		int32_t diff1;
		uint32_t input, mul1;

		input = main_freq / i;
		if (input < PMC_PLL_MIN_IN_FREQ)
			break;
		if (input > PMC_PLL_MAX_IN_FREQ)
			continue;

		mul1 = out_freq / input;
		if (mul1 > PMC_PLL_MULT_MAX)
			continue;
		if (mul1 < PMC_PLL_MULT_MIN)
			break;

		diff1 = out_freq - input * mul1;
		if (diff1 < 0)
			diff1 = -diff1;
		if (diff > diff1) {
			diff = diff1;
			div = i;
			mul = mul1;
			if (diff == 0)
				break;
		}
	}
	if (diff > (out_freq >> PMC_PLL_SHIFT_TOL))
		goto fail;
	return ret | ((mul - 1) << 16) | div;
fail:
	return 0;
}

static void
at91_pmc_init_clock(struct at91_pmc_softc *sc, unsigned int main_clock)
{
	uint32_t mckr;
	int freq;

	sc->main_clock_hz = main_clock;
	main_ck.hz = main_clock;
	plla.hz = at91_pmc_pll_rate(main_clock, RD4(sc, CKGR_PLLAR), 0);

	/*
	 * Initialize the usb clock.  This sets up pllb, but disables the
	 * actual clock.
	 */
	sc->pllb_init = at91_pmc_pll_calc(main_clock, 48000000 * 2) |0x10000000;
	pllb.hz = at91_pmc_pll_rate(main_clock, sc->pllb_init, 1);
	WR4(sc, PMC_PCDR, (1 << AT91RM92_IRQ_UHP) | (1 << AT91RM92_IRQ_UDP));
	WR4(sc, PMC_SCDR, PMC_SCER_UHP | PMC_SCER_UDP);
	WR4(sc, CKGR_PLLBR, 0);
	WR4(sc, PMC_SCER, PMC_SCER_MCKUDP);

	/*
	 * MCK and PCU derive from one of the primary clocks.  Initialize
	 * this relationship.
	 */
	mckr = RD4(sc, PMC_MCKR);
	mck.parent = clock_list[mckr & 0x3];
	mck.parent->refcnt++;
	freq = mck.parent->hz / (1 << ((mckr >> 2) & 3));
	mck.hz = freq / (1 + ((mckr >> 8) & 3));

	device_printf(sc->dev,
	    "Primary: %d Hz PLLA: %d MHz CPU: %d MHz MCK: %d MHz\n",
	    sc->main_clock_hz,
	    at91_pmc_pll_rate(main_clock, RD4(sc, CKGR_PLLAR), 0) / 1000000,
	    freq / 1000000, mck.hz / 1000000);
	WR4(sc, PMC_SCDR, PMC_SCER_PCK0 | PMC_SCER_PCK1 | PMC_SCER_PCK2 |
	    PMC_SCER_PCK3);
	/* XXX kludge, turn on all peripherals */
	WR4(sc, PMC_PCER, 0xffffffff);
	/* Disable all interrupts for PMC */
	WR4(sc, PMC_IDR, 0xffffffff);
}

static void
at91_pmc_deactivate(device_t dev)
{
	struct at91_pmc_softc *sc;

	sc = device_get_softc(dev);
	bus_generic_detach(sc->dev);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    rman_get_rid(sc->mem_res), sc->mem_res);
	sc->mem_res = 0;
	return;
}

static int
at91_pmc_activate(device_t dev)
{
	struct at91_pmc_softc *sc;
	int rid;

	sc = device_get_softc(dev);
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		goto errout;
	return (0);
errout:
	at91_pmc_deactivate(dev);
	return (ENOMEM);
}

static int
at91_pmc_probe(device_t dev)
{

	device_set_desc(dev, "PMC");
	return (0);
}

#if !defined(AT91C_MAIN_CLOCK)
static unsigned int
at91_pmc_sense_mainf(struct at91_pmc_softc *sc)
{
	unsigned int ckgr_val;
	unsigned int diff, matchdiff;
	int i, match;

	ckgr_val = (RD4(sc, CKGR_MCFR) & CKGR_MCFR_MAINF_MASK) << 11;

	/*
	 * Try to find the standard frequency that match best.
	 */
	match = 0;
	matchdiff = abs(ckgr_val - at91_mainf_tbl[0]);
	for (i = 1; i < MAINF_TBL_LEN; i++) {
		diff = abs(ckgr_val - at91_mainf_tbl[i]);
		if (diff < matchdiff) {
			match = i;
			matchdiff = diff;
		}
	}
	return (at91_mainf_tbl[match]);
}
#endif

static int
at91_pmc_attach(device_t dev)
{
	unsigned int mainf;
	int err;

	pmc_softc = device_get_softc(dev);
	pmc_softc->dev = dev;
	if ((err = at91_pmc_activate(dev)) != 0)
		return err;

	/*
	 * Configure main clock frequency.
	 */
#if !defined(AT91C_MAIN_CLOCK)
	mainf = at91_pmc_sense_mainf(pmc_softc);
#else
	mainf = AT91C_MAIN_CLOCK;
#endif
	at91_pmc_init_clock(pmc_softc, mainf);
	return (0);
}

static device_method_t at91_pmc_methods[] = {
	DEVMETHOD(device_probe, at91_pmc_probe),
	DEVMETHOD(device_attach, at91_pmc_attach),
	{0, 0},
};

static driver_t at91_pmc_driver = {
	"at91_pmc",
	at91_pmc_methods,
	sizeof(struct at91_pmc_softc),
};
static devclass_t at91_pmc_devclass;

DRIVER_MODULE(at91_pmc, atmelarm, at91_pmc_driver, at91_pmc_devclass, 0, 0);
