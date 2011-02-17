/*-
 * Copyright (c) 2005 Olivier Houchard.  All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#define	_ARM32_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91rm92reg.h>
#include <arm/at91/at91_aicreg.h>
#include <arm/at91/at91_pmcreg.h>
#include <arm/at91/at91_pmcvar.h>

struct at91rm92_softc {
	device_t dev;
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_sh;
	bus_space_handle_t sc_sys_sh;
	bus_space_handle_t sc_aic_sh;
	bus_space_handle_t sc_dbg_sh;
	bus_space_handle_t sc_matrix_sh;
};
/*
 * Standard priority levels for the system.  0 is lowest and 7 is highest.
 * These values are the ones Atmel uses for its Linux port, which differ
 * a little form the ones that are in the standard distribution.  Also,
 * the ones marked with 'TWEEK' are different based on experience.
 */
static const int at91_irq_prio[32] =
{
	7,	/* Advanced Interrupt Controller (FIQ) */
	7,	/* System Peripherals */
	1,	/* Parallel IO Controller A */
	1,	/* Parallel IO Controller B */
	1,	/* Parallel IO Controller C */
	1,	/* Parallel IO Controller D */
	5,	/* USART 0 */
	5,	/* USART 1 */
	5,	/* USART 2 */
	5,	/* USART 3 */
	0,	/* Multimedia Card Interface */
	2,	/* USB Device Port */
	4,	/* Two-Wire Interface */		/* TWEEK */
	5,	/* Serial Peripheral Interface */
	4,	/* Serial Synchronous Controller 0 */
	6,	/* Serial Synchronous Controller 1 */	/* TWEEK */
	4,	/* Serial Synchronous Controller 2 */
	0,	/* Timer Counter 0 */
	6,	/* Timer Counter 1 */			/* TWEEK */
	0,	/* Timer Counter 2 */
	0,	/* Timer Counter 3 */
	0,	/* Timer Counter 4 */
	0,	/* Timer Counter 5 */
	2,	/* USB Host port */
	3,	/* Ethernet MAC */
	0,	/* Advanced Interrupt Controller (IRQ0) */
	0,	/* Advanced Interrupt Controller (IRQ1) */
	0,	/* Advanced Interrupt Controller (IRQ2) */
	0,	/* Advanced Interrupt Controller (IRQ3) */
	0,	/* Advanced Interrupt Controller (IRQ4) */
	0,	/* Advanced Interrupt Controller (IRQ5) */
 	0	/* Advanced Interrupt Controller (IRQ6) */
};

#define DEVICE(_name, _id, _unit)		\
	{					\
		_name, _unit,			\
		AT91RM92_ ## _id ##_BASE,	\
		AT91RM92_ ## _id ## _SIZE,	\
		AT91RM92_IRQ_ ## _id		\
	}

static const struct cpu_devs at91_devs[] =
{
	DEVICE("at91_pmc",   PMC,    0),
	DEVICE("at91_st",    ST,     0),
	DEVICE("at91_pio",   PIOA,   0),
	DEVICE("at91_pio",   PIOB,   1),
	DEVICE("at91_pio",   PIOC,   2),
	DEVICE("at91_pio",   PIOD,   3),
	DEVICE("at91_rtc",   RTC,    0),

	DEVICE("at91_mci",   MCI,    0),
	DEVICE("at91_twi",   TWI,    0),
	DEVICE("at91_udp",   UDP,    0),
	DEVICE("ate",        EMAC,   0),
	DEVICE("at91_ssc",   SSC0,   0),
	DEVICE("at91_ssc",   SSC1,   1),
	DEVICE("at91_ssc",   SSC2,   2),
	DEVICE("spi",        SPI,    0),

	DEVICE("uart",       DBGU,   0),
	DEVICE("uart",       USART0, 1),
	DEVICE("uart",       USART1, 2),
	DEVICE("uart",       USART2, 3),
	DEVICE("uart",       USART3, 4),
	DEVICE("at91_aic",   AIC,    0),
	DEVICE("at91_mc",    MC,     0),
	DEVICE("at91_tc",    TC0,    0),
	DEVICE("at91_tc",    TC1,    1),
	DEVICE("ohci",       OHCI,   0),
	DEVICE("af91_cfata", CF,     0),
	{	0, 0, 0, 0, 0 }
};

static void
at91_add_child(device_t dev, int prio, const char *name, int unit,
    bus_addr_t addr, bus_size_t size, int irq0, int irq1, int irq2)
{
	device_t kid;
	struct at91_ivar *ivar;

	kid = device_add_child_ordered(dev, prio, name, unit);
	if (kid == NULL) {
	    printf("Can't add child %s%d ordered\n", name, unit);
	    return;
	}
	ivar = malloc(sizeof(*ivar), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ivar == NULL) {
		device_delete_child(dev, kid);
		printf("Can't add alloc ivar\n");
		return;
	}
	device_set_ivars(kid, ivar);
	resource_list_init(&ivar->resources);
	if (irq0 != -1) {
		bus_set_resource(kid, SYS_RES_IRQ, 0, irq0, 1);
		if (irq0 != AT91RM92_IRQ_SYSTEM)
			at91_pmc_clock_add(device_get_nameunit(kid), irq0, 0);
	}
	if (irq1 != 0)
		bus_set_resource(kid, SYS_RES_IRQ, 1, irq1, 1);
	if (irq2 != 0)
		bus_set_resource(kid, SYS_RES_IRQ, 2, irq2, 1);
	if (addr != 0 && addr < AT91RM92_BASE) 
		addr += AT91RM92_BASE;
	if (addr != 0)
		bus_set_resource(kid, SYS_RES_MEMORY, 0, addr, size);
}

static void
at91_cpu_add_builtin_children(device_t dev)
{
	int i;
	const struct cpu_devs *walker;
	
	for (i = 1, walker = at91_devs; walker->name; i++, walker++) {
		at91_add_child(dev, i, walker->name, walker->unit,
		    walker->mem_base, walker->mem_len, walker->irq0,
		    walker->irq1, walker->irq2);
	}
}

static uint32_t
at91_pll_outb(int freq)
{

	if (freq > 155000000)
		return (0x0000);
	else 
		return (0x8000);
}

static void
at91_identify(driver_t *drv, device_t parent)
{

	if (at91_cpu_is(AT91_CPU_RM9200)) {
		at91_add_child(parent, 0, "at91rm920", 0, 0, 0, -1, 0, 0);
		at91_cpu_add_builtin_children(parent);
	}
}

static int
at91_probe(device_t dev)
{

	if (at91_cpu_is(AT91_CPU_RM9200)) {
		device_set_desc(dev, "AT91RM9200");
		return (0);
	}
	return (ENXIO);
}

static int
at91_attach(device_t dev)
{
	struct at91_pmc_clock *clk;
	struct at91rm92_softc *sc = device_get_softc(dev);
	int i;

	struct at91_softc *at91sc = device_get_softc(device_get_parent(dev));

	sc->sc_st = at91sc->sc_st;
	sc->sc_sh = at91sc->sc_sh;
	sc->dev = dev;

	if (bus_space_subregion(sc->sc_st, sc->sc_sh, AT91RM92_SYS_BASE,
	    AT91RM92_SYS_SIZE, &sc->sc_sys_sh) != 0)
		panic("Enable to map system registers");

	if (bus_space_subregion(sc->sc_st, sc->sc_sh, AT91RM92_DBGU_BASE,
	    AT91RM92_DBGU_SIZE, &sc->sc_dbg_sh) != 0)
		panic("Enable to map DBGU registers");

	if (bus_space_subregion(sc->sc_st, sc->sc_sh, AT91RM92_AIC_BASE,
	    AT91RM92_AIC_SIZE, &sc->sc_aic_sh) != 0)
		panic("Enable to map system registers");

	/* XXX Hack to tell atmelarm about the AIC */
	at91sc->sc_aic_sh = sc->sc_aic_sh;
	at91sc->sc_irq_system = AT91RM92_IRQ_SYSTEM;

	for (i = 0; i < 32; i++) {
		bus_space_write_4(sc->sc_st, sc->sc_aic_sh, IC_SVR + 
		    i * 4, i);
		/* Priority. */
		bus_space_write_4(sc->sc_st, sc->sc_aic_sh, IC_SMR + i * 4,
		    at91_irq_prio[i]);
		if (i < 8)
			bus_space_write_4(sc->sc_st, sc->sc_aic_sh, IC_EOICR,
			    1);
	}

	bus_space_write_4(sc->sc_st, sc->sc_aic_sh, IC_SPU, 32);
	/* No debug. */
	bus_space_write_4(sc->sc_st, sc->sc_aic_sh, IC_DCR, 0);
	/* Disable and clear all interrupts. */
	bus_space_write_4(sc->sc_st, sc->sc_aic_sh, IC_IDCR, 0xffffffff);
	bus_space_write_4(sc->sc_st, sc->sc_aic_sh, IC_ICCR, 0xffffffff);

	/* Disable all interrupts for RTC (0xe24 == RTC_IDR) */
	bus_space_write_4(sc->sc_st, sc->sc_sys_sh, 0xe24, 0xffffffff);

	/* Disable all interrupts for the SDRAM controller */
	bus_space_write_4(sc->sc_st, sc->sc_sys_sh, 0xfa8, 0xffffffff);

	/* Disable all interrupts for DBGU */
	bus_space_write_4(sc->sc_st, sc->sc_dbg_sh, 0x0c, 0xffffffff);

	/* Update USB device port clock info */
	clk = at91_pmc_clock_ref("udpck");
	clk->pmc_mask  = PMC_SCER_UDP;
	at91_pmc_clock_deref(clk);

	/* Update USB host port clock info */
	clk = at91_pmc_clock_ref("uhpck");
	clk->pmc_mask  = PMC_SCER_UHP;
	at91_pmc_clock_deref(clk);

	/* Each SOC has different PLL contraints */
	clk = at91_pmc_clock_ref("plla");
	clk->pll_min_in    = RM9200_PLL_A_MIN_IN_FREQ;		/*   1 MHz */
	clk->pll_max_in    = RM9200_PLL_A_MAX_IN_FREQ;		/*  32 MHz */
	clk->pll_min_out   = RM9200_PLL_A_MIN_OUT_FREQ;		/*  80 MHz */
	clk->pll_max_out   = RM9200_PLL_A_MAX_OUT_FREQ;		/* 180 MHz */
	clk->pll_mul_shift = RM9200_PLL_A_MUL_SHIFT;
	clk->pll_mul_mask  = RM9200_PLL_A_MUL_MASK;
	clk->pll_div_shift = RM9200_PLL_A_DIV_SHIFT;
	clk->pll_div_mask  = RM9200_PLL_A_DIV_MASK;
	clk->set_outb      = at91_pll_outb;
	at91_pmc_clock_deref(clk);

	clk = at91_pmc_clock_ref("pllb");
	clk->pll_min_in    = RM9200_PLL_B_MIN_IN_FREQ;		/* 100 KHz */
	clk->pll_max_in    = RM9200_PLL_B_MAX_IN_FREQ;		/*  32 MHz */
	clk->pll_min_out   = RM9200_PLL_B_MIN_OUT_FREQ;		/*  30 MHz */
	clk->pll_max_out   = RM9200_PLL_B_MAX_OUT_FREQ;		/* 240 MHz */
	clk->pll_mul_shift = RM9200_PLL_B_MUL_SHIFT;
	clk->pll_mul_mask  = RM9200_PLL_B_MUL_MASK;
	clk->pll_div_shift = RM9200_PLL_B_DIV_SHIFT;
	clk->pll_div_mask  = RM9200_PLL_B_DIV_MASK;
	clk->set_outb      = at91_pll_outb;
	at91_pmc_clock_deref(clk);

	return (0);
}

static device_method_t at91_methods[] = {
	DEVMETHOD(device_probe, at91_probe),
	DEVMETHOD(device_attach, at91_attach),
	DEVMETHOD(device_identify, at91_identify),
	{0, 0},
};

static driver_t at91rm92_driver = {
	"at91rm920",
	at91_methods,
	sizeof(struct at91rm92_softc),
};

static devclass_t at91rm92_devclass;

DRIVER_MODULE(at91rm920, atmelarm, at91rm92_driver, at91rm92_devclass, 0, 0);
