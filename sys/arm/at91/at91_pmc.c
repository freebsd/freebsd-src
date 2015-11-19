/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
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
#include <machine/intr.h>
#include <arm/at91/at91reg.h>
#include <arm/at91/at91var.h>

#include <arm/at91/at91_pmcreg.h>
#include <arm/at91/at91_pmcvar.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

static struct at91_pmc_softc {
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	struct resource	*mem_res;	/* Memory resource */
	device_t		dev;
} *pmc_softc;

static uint32_t pllb_init;

MALLOC_DECLARE(M_PMC);
MALLOC_DEFINE(M_PMC, "at91_pmc_clocks", "AT91 PMC Clock descriptors");

#define AT91_PMC_BASE 0xffffc00

static void at91_pmc_set_pllb_mode(struct at91_pmc_clock *, int);
static void at91_pmc_set_upll_mode(struct at91_pmc_clock *, int);
static void at91_pmc_set_sys_mode(struct at91_pmc_clock *, int);
static void at91_pmc_set_periph_mode(struct at91_pmc_clock *, int);
static void at91_pmc_clock_alias(const char *name, const char *alias);

static struct at91_pmc_clock slck = {
	.name = "slck",		/* 32,768 Hz slow clock */
	.hz = 32768,
	.refcnt = 1,
	.id = 0,
	.primary = 1,
};

/*
 * NOTE: Clocks for "ordinary peripheral" devices e.g. spi0, udp0, uhp0 etc.
 * are now created automatically. Only "system" clocks need be defined here.
 */
static struct at91_pmc_clock main_ck = {
	.name = "main",		/* Main clock */
	.refcnt = 0,
	.id = 1,
	.primary = 1,
	.pmc_mask = PMC_IER_MOSCS,
};

static struct at91_pmc_clock plla = {
	.name = "plla",		/* PLLA Clock, used for CPU clocking */
	.parent = &main_ck,
	.refcnt = 1,
	.id = 0,
	.primary = 1,
	.pll = 1,
	.pmc_mask = PMC_IER_LOCKA,
};

static struct at91_pmc_clock pllb = {
	.name = "pllb",		/* PLLB Clock, used for USB functions */
	.parent = &main_ck,
	.refcnt = 0,
	.id = 0,
	.primary = 1,
	.pll = 1,
	.pmc_mask = PMC_IER_LOCKB,
	.set_mode = &at91_pmc_set_pllb_mode,
};

/* Used by USB on at91sam9g45 */
static struct at91_pmc_clock upll = {
	.name = "upll",		/* UTMI PLL, used for USB functions on 9G45 family */
	.parent = &main_ck,
	.refcnt = 0,
	.id = 0,
	.primary = 1,
	.pll = 1,
	.pmc_mask = (1 << 6),
	.set_mode = &at91_pmc_set_upll_mode,
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
	.name = "mck",		/* Master (Peripheral) Clock */
	.pmc_mask = PMC_IER_MCKRDY,
	.refcnt = 0,
};

static struct at91_pmc_clock cpu = {
	.name = "cpu",		/* CPU Clock */
	.parent = &plla,
	.pmc_mask = PMC_SCER_PCK,
	.refcnt = 0,
};

/* "+32" or the automatic peripheral clocks */
static struct at91_pmc_clock *clock_list[16+32] = {
	&slck,
	&main_ck,
	&plla,
	&pllb,
	&upll,
	&udpck,
	&uhpck,
	&mck,
	&cpu
};

static inline uint32_t
RD4(struct at91_pmc_softc *sc, bus_size_t off)
{

	if (sc == NULL) {
		uint32_t *p = (uint32_t *)(AT91_BASE + AT91_PMC_BASE + off);

		return *p;
	}
	return (bus_read_4(sc->mem_res, off));
}

static inline void
WR4(struct at91_pmc_softc *sc, bus_size_t off, uint32_t val)
{

	if (sc == NULL) {
		uint32_t *p = (uint32_t *)(AT91_BASE + AT91_PMC_BASE + off);

		*p = val;
	} else
		bus_write_4(sc->mem_res, off, val);
}

/*
 * The following is unused currently since we don't ever set the PLLA
 * frequency of the device.  If we did, we'd have to also pay attention
 * to the ICPLLA bit in the PMC_PLLICPR register for frequencies lower
 * than ~600MHz, which the PMC code doesn't do right now.
 */
uint32_t
at91_pmc_800mhz_plla_outb(int freq)
{
	uint32_t outa;

	/*
	 * Set OUTA, per the data sheet.  See Table 46-16 titled
	 * PLLA Frequency Regarding ICPLLA and OUTA in the SAM9X25 doc,
	 * Table 46-17 in the SAM9G20 doc, or Table 46-16 in the SAM9G45 doc.
	 * Note: the frequencies overlap by 5MHz, so we add 3 here to
	 * center shoot the transition.
	 */

	freq /= 1000000;		/* MHz */
	if (freq >= 800)
		freq = 800;
	freq += 3;			/* Allow for overlap. */
	outa = 3 - ((freq / 50) & 3);	/* 750 / 50 = 7, see table */
	return (1 << 29)| (outa << 14);
}

uint32_t
at91_pmc_800mhz_pllb_outb(int freq)
{

	return (0);
}

void
at91_pmc_set_pllb_mode(struct at91_pmc_clock *clk, int on)
{
	struct at91_pmc_softc *sc = pmc_softc;
	uint32_t value;

	value = on ? pllb_init : 0;

	/*
	 * Only write to the register if the value is changing.  Besides being
	 * good common sense, this works around RM9200 Errata #26 (CKGR_PLL[AB]R
	 * must not be written with the same value currently in the register).
	 */
	if (RD4(sc, CKGR_PLLBR) != value) {
		WR4(sc, CKGR_PLLBR, value);
		while (on && (RD4(sc, PMC_SR) & PMC_IER_LOCKB) != PMC_IER_LOCKB)
			continue;
	}
}

static void
at91_pmc_set_upll_mode(struct at91_pmc_clock *clk, int on)
{
	struct at91_pmc_softc *sc = pmc_softc;
	uint32_t value;

	if (on) {
		on = PMC_IER_LOCKU;
		value = CKGR_UCKR_UPLLEN | CKGR_UCKR_BIASEN;
	} else
		value = 0;

	WR4(sc, CKGR_UCKR, RD4(sc, CKGR_UCKR) | value);
	while ((RD4(sc, PMC_SR) & PMC_IER_LOCKU) != on)
		continue;

	WR4(sc, PMC_USB, PMC_USB_USBDIV(9) | PMC_USB_USBS);
	WR4(sc, PMC_SCER, PMC_SCER_UHP_SAM9);
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
at91_pmc_clock_add(const char *name, uint32_t irq,
    struct at91_pmc_clock *parent)
{
	struct at91_pmc_clock *clk;
	int i, buflen;

	clk = malloc(sizeof(*clk), M_PMC, M_NOWAIT | M_ZERO);
	if (clk == NULL)
		goto err;

	buflen = strlen(name) + 1;
	clk->name = malloc(buflen, M_PMC, M_NOWAIT);
	if (clk->name == NULL)
		goto err;

	strlcpy(clk->name, name, buflen);
	clk->pmc_mask = 1 << irq;
	clk->set_mode = &at91_pmc_set_periph_mode;
	if (parent == NULL)
		clk->parent = &mck;
	else
		clk->parent = parent;

	for (i = 0; i < sizeof(clock_list) / sizeof(clock_list[0]); i++) {
		if (clock_list[i] == NULL) {
			clock_list[i] = clk;
			return (clk);
		}
	}
err:
	if (clk != NULL) {
		if (clk->name != NULL)
			free(clk->name, M_PMC);
		free(clk, M_PMC);
	}

	panic("could not allocate pmc clock '%s'", name);
	return (NULL);
}

static void
at91_pmc_clock_alias(const char *name, const char *alias)
{
	struct at91_pmc_clock *clk, *alias_clk;

	clk = at91_pmc_clock_ref(name);
	if (clk)
		alias_clk = at91_pmc_clock_add(alias, 0, clk->parent);

	if (clk && alias_clk) {
		alias_clk->hz = clk->hz;
		alias_clk->pmc_mask = clk->pmc_mask;
		alias_clk->set_mode = clk->set_mode;
	}
}

struct at91_pmc_clock *
at91_pmc_clock_ref(const char *name)
{
	int i;

	for (i = 0; i < sizeof(clock_list) / sizeof(clock_list[0]); i++) {
		if (clock_list[i] == NULL)
		    break;
		if (strcmp(name, clock_list[i]->name) == 0)
			return (clock_list[i]);
	}

	return (NULL);
}

void
at91_pmc_clock_deref(struct at91_pmc_clock *clk)
{
	if (clk == NULL)
		return;
}

void
at91_pmc_clock_enable(struct at91_pmc_clock *clk)
{
	if (clk == NULL)
		return;

	/* XXX LOCKING? XXX */
	if (clk->parent)
		at91_pmc_clock_enable(clk->parent);
	if (clk->refcnt++ == 0 && clk->set_mode)
		clk->set_mode(clk, 1);
}

void
at91_pmc_clock_disable(struct at91_pmc_clock *clk)
{
	if (clk == NULL)
		return;

	/* XXX LOCKING? XXX */
	if (--clk->refcnt == 0 && clk->set_mode)
		clk->set_mode(clk, 0);
	if (clk->parent)
		at91_pmc_clock_disable(clk->parent);
}

static int
at91_pmc_pll_rate(struct at91_pmc_clock *clk, uint32_t reg)
{
	uint32_t mul, div, freq;

	freq = clk->parent->hz;
	div = (reg >> clk->pll_div_shift) & clk->pll_div_mask;
	mul = (reg >> clk->pll_mul_shift) & clk->pll_mul_mask;

#if 0
	printf("pll = (%d /  %d) * %d = %d\n",
	    freq, div, mul + 1, (freq/div) * (mul+1));
#endif

	if (div != 0 && mul != 0) {
		freq /= div;
		freq *= mul + 1;
	} else
		freq = 0;
	clk->hz = freq;

	return (freq);
}

static uint32_t
at91_pmc_pll_calc(struct at91_pmc_clock *clk, uint32_t out_freq)
{
	uint32_t i, div = 0, mul = 0, diff = 1 << 30;

	unsigned ret = 0x3e00;

	if (out_freq > clk->pll_max_out)
		goto fail;

	for (i = 1; i < 256; i++) {
		int32_t diff1;
		uint32_t input, mul1;

		input = clk->parent->hz / i;
		if (input < clk->pll_min_in)
			break;
		if (input > clk->pll_max_in)
			continue;

		mul1 = out_freq / input;
		if (mul1 > (clk->pll_mul_mask + 1))
			continue;
		if (mul1 == 0)
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

	if (clk->set_outb != NULL)
		ret |= clk->set_outb(out_freq);

	return (ret |
		((mul - 1) << clk->pll_mul_shift) |
		(div << clk->pll_div_shift));
fail:
	return (0);
}

#if !defined(AT91C_MAIN_CLOCK)
static const unsigned int at91_main_clock_tbl[] = {
	3000000, 3276800, 3686400, 3840000, 4000000,
	4433619, 4915200, 5000000, 5242880, 6000000,
	6144000, 6400000, 6553600, 7159090, 7372800,
	7864320, 8000000, 9830400, 10000000, 11059200,
	12000000, 12288000, 13560000, 14318180, 14745600,
	16000000, 17344700, 18432000, 20000000
};
#define	MAIN_CLOCK_TBL_LEN	(sizeof(at91_main_clock_tbl) / sizeof(*at91_main_clock_tbl))
#endif

static unsigned int
at91_pmc_sense_main_clock(void)
{
#if !defined(AT91C_MAIN_CLOCK)
	unsigned int ckgr_val;
	unsigned int diff, matchdiff, freq;
	int i;

	ckgr_val = (RD4(NULL, CKGR_MCFR) & CKGR_MCFR_MAINF_MASK) << 11;

	/*
	 * Clocks up to 50MHz can be connected to some models.  If
	 * the frequency is >= 21MHz, assume that the slow clock can
	 * measure it correctly, and that any error can be adequately
	 * compensated for by roudning to the nearest 500Hz.  Users
	 * with fast, or odd-ball clocks will need to set
	 * AT91C_MAIN_CLOCK in the kernel config file.
	 */
	if (ckgr_val >= 21000000)
		return ((ckgr_val + 250) / 500 * 500);

	/*
	 * Try to find the standard frequency that match best.
	 */
	freq = at91_main_clock_tbl[0];
	matchdiff = abs(ckgr_val - at91_main_clock_tbl[0]);
	for (i = 1; i < MAIN_CLOCK_TBL_LEN; i++) {
		diff = abs(ckgr_val - at91_main_clock_tbl[i]);
		if (diff < matchdiff) {
			freq = at91_main_clock_tbl[i];
			matchdiff = diff;
		}
	}
	return (freq);
#else
	return (AT91C_MAIN_CLOCK);
#endif
}

void
at91_pmc_init_clock(void)
{
	struct at91_pmc_softc *sc = NULL;
	unsigned int main_clock;
	uint32_t mckr;
	uint32_t mdiv;

	soc_info.soc_data->soc_clock_init();

	main_clock = at91_pmc_sense_main_clock();

	if (at91_is_sam9() || at91_is_sam9xe()) {
		uhpck.pmc_mask = PMC_SCER_UHP_SAM9;
		udpck.pmc_mask = PMC_SCER_UDP_SAM9;
	}

	/* There is no pllb on AT91SAM9G45 */
	if (at91_cpu_is(AT91_T_SAM9G45)) {
		uhpck.parent = &upll;
		uhpck.pmc_mask = PMC_SCER_UHP_SAM9;
	}

	mckr = RD4(sc, PMC_MCKR);
	main_ck.hz = main_clock;

	/*
	 * Note: this means outa calc code for plla never used since
	 * we never change it.  If we did, we'd also have to mind
	 * ICPLLA to get the charge pump current right.
	 */
	at91_pmc_pll_rate(&plla, RD4(sc, CKGR_PLLAR));

	if (at91_cpu_is(AT91_T_SAM9G45) && (mckr & PMC_MCKR_PLLADIV2))
		plla.hz /= 2;

	/*
	 * Initialize the usb clock.  This sets up pllb, but disables the
	 * actual clock. XXX except for the if 0 :(
	 */
	if (!at91_cpu_is(AT91_T_SAM9G45)) {
		pllb_init = at91_pmc_pll_calc(&pllb, 48000000 * 2) | 0x10000000;
		at91_pmc_pll_rate(&pllb, pllb_init);
#if 0
		/* Turn off USB clocks */
		at91_pmc_set_periph_mode(&ohci_clk, 0);
		at91_pmc_set_periph_mode(&udc_clk, 0);
#endif
	}

	if (at91_is_rm92()) {
		WR4(sc, PMC_SCDR, PMC_SCER_UHP | PMC_SCER_UDP);
		WR4(sc, PMC_SCER, PMC_SCER_MCKUDP);
	} else
		WR4(sc, PMC_SCDR, PMC_SCER_UHP_SAM9 | PMC_SCER_UDP_SAM9);

	/*
	 * MCK and PCU derive from one of the primary clocks.  Initialize
	 * this relationship.
	 */
	mck.parent = clock_list[mckr & 0x3];
	mck.parent->refcnt++;

	cpu.hz = mck.hz = mck.parent->hz /
	    (1 << ((mckr & PMC_MCKR_PRES_MASK) >> 2));

	mdiv = (mckr & PMC_MCKR_MDIV_MASK) >> 8;
	if (at91_is_sam9() || at91_is_sam9xe()) {
		/*
		 * On AT91SAM9G45 when mdiv == 3 we need to divide
		 * MCK by 3 but not, for example, on 9g20.
		 */
		if (!at91_cpu_is(AT91_T_SAM9G45) || mdiv <= 2)
			mdiv *= 2;
		if (mdiv > 0)
			mck.hz /= mdiv;
	} else
		mck.hz /= (1 + mdiv);

	/* Only found on SAM9G20 */
	if (at91_cpu_is(AT91_T_SAM9G20))
		cpu.hz /= (mckr & PMC_MCKR_PDIV) ?  2 : 1;

	at91_master_clock = mck.hz;

	/* These clocks refrenced by "special" names */
	at91_pmc_clock_alias("ohci0", "ohci_clk");
	at91_pmc_clock_alias("udp0",  "udp_clk");

	/* Turn off "Progamable" clocks */
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
#ifdef FDT
	if (!ofw_bus_is_compatible(dev, "atmel,at91rm9200-pmc") &&
 		!ofw_bus_is_compatible(dev, "atmel,at91sam9260-pmc") &&
		!ofw_bus_is_compatible(dev, "atmel,at91sam9g45-pmc") &&
		!ofw_bus_is_compatible(dev, "atmel,at91sam9x5-pmc"))
		return (ENXIO);
#endif
	device_set_desc(dev, "PMC");
	return (0);
}

static int
at91_pmc_attach(device_t dev)
{
	int err;

	pmc_softc = device_get_softc(dev);
	pmc_softc->dev = dev;
	if ((err = at91_pmc_activate(dev)) != 0)
		return (err);

	/*
	 * Configure main clock frequency.
	 */
	at91_pmc_init_clock();

	/*
	 * Display info about clocks previously computed
	 */
	device_printf(dev,
	    "Primary: %d Hz PLLA: %d MHz CPU: %d MHz MCK: %d MHz\n",
	    main_ck.hz,
	    plla.hz / 1000000,
	    cpu.hz / 1000000, mck.hz / 1000000);

	return (0);
}

static device_method_t at91_pmc_methods[] = {
	DEVMETHOD(device_probe, at91_pmc_probe),
	DEVMETHOD(device_attach, at91_pmc_attach),
	DEVMETHOD_END
};

static driver_t at91_pmc_driver = {
	"at91_pmc",
	at91_pmc_methods,
	sizeof(struct at91_pmc_softc),
};
static devclass_t at91_pmc_devclass;

#ifdef FDT
EARLY_DRIVER_MODULE(at91_pmc, simplebus, at91_pmc_driver, at91_pmc_devclass,
    NULL, NULL, BUS_PASS_CPU);
#else
EARLY_DRIVER_MODULE(at91_pmc, atmelarm, at91_pmc_driver, at91_pmc_devclass,
    NULL, NULL, BUS_PASS_CPU);
#endif
