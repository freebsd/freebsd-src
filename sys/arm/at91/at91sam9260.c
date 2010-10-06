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
#include <arm/at91/at91_aicreg.h>
#include <arm/at91/at91sam9260reg.h>
#include <arm/at91/at91_pmcreg.h>
#include <arm/at91/at91_pmcvar.h>

struct at91sam9_softc {
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
 * These values are the ones Atmel uses for its Linux port
 */
static const int at91_irq_prio[32] =
{
	7,	/* Advanced Interrupt Controller */
	7,	/* System Peripherals */
	1,	/* Parallel IO Controller A */
	1,	/* Parallel IO Controller B */
	1,	/* Parallel IO Controller C */
	0,	/* Analog-to-Digital Converter */
	5,	/* USART 0 */
	5,	/* USART 1 */
	5,	/* USART 2 */
	0,	/* Multimedia Card Interface */
	2,	/* USB Device Port */
	6,	/* Two-Wire Interface */
	5,	/* Serial Peripheral Interface 0 */
	5,	/* Serial Peripheral Interface 1 */
	5,	/* Serial Synchronous Controller */
	0,	/* (reserved) */
	0,	/* (reserved) */
	0,	/* Timer Counter 0 */
	0,	/* Timer Counter 1 */
	0,	/* Timer Counter 2 */
	2,	/* USB Host port */
	3,	/* Ethernet */
	0,	/* Image Sensor Interface */
	5,	/* USART 3 */
	5,	/* USART 4 */
	5,	/* USART 5 */
	0,	/* Timer Counter 3 */
	0,	/* Timer Counter 4 */
	0,	/* Timer Counter 5 */
	0,	/* Advanced Interrupt Controller IRQ0 */
	0,	/* Advanced Interrupt Controller IRQ1 */
	0,	/* Advanced Interrupt Controller IRQ2 */
};

#define DEVICE(_name, _id, _unit)		\
	{					\
		_name, _unit,			\
		AT91SAM9260_ ## _id ##_BASE,	\
		AT91SAM9260_ ## _id ## _SIZE,	\
		AT91SAM9260_IRQ_ ## _id		\
	}

static const struct cpu_devs at91_devs[] =
{
	DEVICE("at91_pmc", PMC,  0),
	DEVICE("at91_wdt", WDT,  0),
	DEVICE("at91_rst", RSTC, 0),
	DEVICE("at91_pit", PIT,  0),
	DEVICE("at91_pio", PIOA, 0),
	DEVICE("at91_pio", PIOB, 1),
	DEVICE("at91_pio", PIOC, 2),
	DEVICE("at91_twi", TWI, 0),
	DEVICE("at91_mci", MCI, 0),
	DEVICE("uart", DBGU,   0),
	DEVICE("uart", USART0, 1),
	DEVICE("uart", USART1, 2),
	DEVICE("uart", USART2, 3),
	DEVICE("uart", USART3, 4),
	DEVICE("uart", USART4, 5),
	DEVICE("uart", USART5, 6),
	DEVICE("spi",  SPI0,   0),
	DEVICE("spi",  SPI1,   1),
	DEVICE("ate",  EMAC,   0),
	DEVICE("macb", EMAC,   0),
	DEVICE("nand", NAND,   0),
	DEVICE("ohci", OHCI,   0),
	{ 0, 0, 0, 0, 0 }
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
		if (irq0 != AT91SAM9260_IRQ_SYSTEM)
			at91_pmc_clock_add(device_get_nameunit(kid), irq0, 0);
	}
	if (irq1 != 0)
		bus_set_resource(kid, SYS_RES_IRQ, 1, irq1, 1);
	if (irq2 != 0)
		bus_set_resource(kid, SYS_RES_IRQ, 2, irq2, 1);
	if (addr != 0 && addr < AT91SAM9260_BASE) 
		addr += AT91SAM9260_BASE;
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
at91_pll_outa(int freq)
{

	if (freq > 195000000)
		return (0x20000000);
	else 
		return (0x20008000);
}

static uint32_t
at91_pll_outb(int freq)
{
	return (0x4000);
}

static void
at91_identify(driver_t *drv, device_t parent)
{

	if (at91_cpu_is(AT91_CPU_SAM9260)) {
		at91_add_child(parent, 0, "at91sam9260", 0, 0, 0, -1, 0, 0);
		at91_cpu_add_builtin_children(parent);
	}
}

static int
at91_probe(device_t dev)
{

	if (at91_cpu_is(AT91_CPU_SAM9260)) {
		device_set_desc(dev, "AT91SAM9260");
		return (0);
	}
	return (ENXIO);
}

static int
at91_attach(device_t dev)
{
	struct at91_pmc_clock *clk;
	struct at91sam9_softc *sc = device_get_softc(dev);
	int i;

	struct at91_softc *at91sc = device_get_softc(device_get_parent(dev));

	sc->sc_st = at91sc->sc_st;
	sc->sc_sh = at91sc->sc_sh;
	sc->dev = dev;

	/* 
	 * XXX These values work for the RM9200, SAM926[01], and SAM9260
	 * will have to fix this when we want to support anything else. XXX
	 */
	if (bus_space_subregion(sc->sc_st, sc->sc_sh, AT91SAM9260_SYS_BASE,
	    AT91SAM9260_SYS_SIZE, &sc->sc_sys_sh) != 0)
		panic("Enable to map system registers");

	if (bus_space_subregion(sc->sc_st, sc->sc_sh, AT91SAM9260_DBGU_BASE,
	    AT91SAM9260_DBGU_SIZE, &sc->sc_dbg_sh) != 0)
		panic("Enable to map DBGU registers");

	if (bus_space_subregion(sc->sc_st, sc->sc_sh, AT91SAM9260_AIC_BASE,
	    AT91SAM9260_AIC_SIZE, &sc->sc_aic_sh) != 0)
		panic("Enable to map system registers");

	/* XXX Hack to tell atmelarm about the AIC */
	at91sc->sc_aic_sh = sc->sc_aic_sh;
	at91sc->sc_irq_system = AT91SAM9260_IRQ_SYSTEM;

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

	/* Disable all interrupts for DBGU */
	bus_space_write_4(sc->sc_st, sc->sc_dbg_sh, 0x0c, 0xffffffff);

	if (bus_space_subregion(sc->sc_st, sc->sc_sh,
	    AT91SAM9260_MATRIX_BASE, AT91SAM9260_MATRIX_SIZE,
	    &sc->sc_matrix_sh) != 0)
		panic("Enable to map matrix registers");

	/* activate NAND*/
	i = bus_space_read_4(sc->sc_st, sc->sc_matrix_sh,
	    AT91SAM9260_EBICSA);
	bus_space_write_4(sc->sc_st, sc->sc_matrix_sh,
	    AT91SAM9260_EBICSA, 
	    i | AT91_MATRIX_EBI_CS3A_SMC_SMARTMEDIA);


	/* Update USB device port clock info */
	clk = at91_pmc_clock_ref("udpck");
	clk->pmc_mask  = PMC_SCER_UDP_SAM9;
	at91_pmc_clock_deref(clk);

	/* Update USB host port clock info */
	clk = at91_pmc_clock_ref("uhpck");
	clk->pmc_mask  = PMC_SCER_UHP_SAM9;
	at91_pmc_clock_deref(clk);

	/* Each SOC has different PLL contraints */
	clk = at91_pmc_clock_ref("plla");
	clk->pll_min_in    = SAM9260_PLL_A_MIN_IN_FREQ;		/*   1 MHz */
	clk->pll_max_in    = SAM9260_PLL_A_MAX_IN_FREQ;		/*  32 MHz */
	clk->pll_min_out   = SAM9260_PLL_A_MIN_OUT_FREQ;	/*  80 MHz */
	clk->pll_max_out   = SAM9260_PLL_A_MAX_OUT_FREQ;	/* 240 MHz */
	clk->pll_mul_shift = SAM9260_PLL_A_MUL_SHIFT;
	clk->pll_mul_mask  = SAM9260_PLL_A_MUL_MASK;
	clk->pll_div_shift = SAM9260_PLL_A_DIV_SHIFT;
	clk->pll_div_mask  = SAM9260_PLL_A_DIV_MASK;
	clk->set_outb      = at91_pll_outa;
	at91_pmc_clock_deref(clk);

	/*
	 * Fudge MAX pll in frequence down below 3.0 Mhz to ensure 
	 * PMC alogrithm choose the divisor that causes the input clock 
	 * to be near the optimal 2 Mhz per datasheet. We know
	 * we are going to be using this for the USB clock at 96 Mhz.
	 * Causes no extra frequency deviation for all recomended crystal values.
	 */
	clk = at91_pmc_clock_ref("pllb");
	clk->pll_min_in    = SAM9260_PLL_B_MIN_IN_FREQ;		/*   1 MHz */
	clk->pll_max_in    = SAM9260_PLL_B_MAX_IN_FREQ;		/*   5 MHz */
	clk->pll_max_in    = 2999999;				/*  ~3 MHz */
	clk->pll_min_out   = SAM9260_PLL_B_MIN_OUT_FREQ;	/*  70 MHz */
	clk->pll_max_out   = SAM9260_PLL_B_MAX_OUT_FREQ;	/* 130 MHz */
	clk->pll_mul_shift = SAM9260_PLL_B_MUL_SHIFT;
	clk->pll_mul_mask  = SAM9260_PLL_B_MUL_MASK;
	clk->pll_div_shift = SAM9260_PLL_B_DIV_SHIFT;
	clk->pll_div_mask  = SAM9260_PLL_B_DIV_MASK;
	clk->set_outb      = at91_pll_outb;
	at91_pmc_clock_deref(clk);
	return (0);
}

static device_method_t at91sam9260_methods[] = {
	DEVMETHOD(device_probe, at91_probe),
	DEVMETHOD(device_attach, at91_attach),
	DEVMETHOD(device_identify, at91_identify),
	{0, 0},
};

static driver_t at91sam9260_driver = {
	"at91sam9260",
	at91sam9260_methods,
	sizeof(struct at91sam9_softc),
};

static devclass_t at91sam9260_devclass;

DRIVER_MODULE(at91sam9260, atmelarm, at91sam9260_driver, at91sam9260_devclass, 0, 0);
