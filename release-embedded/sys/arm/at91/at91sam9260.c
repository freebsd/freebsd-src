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
#include <arm/at91/at91reg.h>
#include <arm/at91/at91soc.h>
#include <arm/at91/at91_aicreg.h>
#include <arm/at91/at91sam9260reg.h>
#include <arm/at91/at91_pitreg.h>
#include <arm/at91/at91_pmcreg.h>
#include <arm/at91/at91_pmcvar.h>
#include <arm/at91/at91_rstreg.h>

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

#define	DEVICE(_name, _id, _unit)		\
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

/*
 * The following is unused currently since we don't ever set the PLLA
 * frequency of the device.
 */
static uint32_t
at91_pll_outa(int freq)
{
	uint32_t outa = 0;

	/*
	 * Set OUTA, per the data sheet.  See Table 40-15 titled
	 * PLLA Characteristics in the SAM9260 doc.
	 */

	if (freq > 155000000)
		outa = 2 << 14;
	return ((1 << 29) | outa);
}

static uint32_t
at91_pll_outb(int freq)
{

	return (1 << 14);
}

static void
at91_clock_init(void)
{
	struct at91_pmc_clock *clk;

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
	 * Fudge MAX pll in frequence down below 3.0 MHz to ensure
	 * PMC alogrithm choose the divisor that causes the input clock
	 * to be near the optimal 2 MHz per datasheet.  We know
	 * we are going to be using this for the USB clock at 96 MHz.
	 * Causes no extra frequency deviation for all recomended crystal
	 * values.  See Note 1, table 40-16 SAM9260 doc.
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
}

static struct at91_soc_data soc_data = {
	.soc_delay = at91_pit_delay,
	.soc_reset = at91_rst_cpu_reset,
	.soc_clock_init = at91_clock_init,
	.soc_irq_prio = at91_irq_prio,
	.soc_children = at91_devs,
};

AT91_SOC(AT91_T_SAM9260, &soc_data);
