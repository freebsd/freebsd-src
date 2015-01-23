/*-
 * Copyright (c) 2005 Olivier Houchard.  All rights reserved.
 * Copyright (c) 2010 Greg Ansley.  All rights reserved.
 * Copyright (c) 2012 Andrew Turner.  All rights reserved.
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
#include <arm/at91/at91sam9g45reg.h>
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
	1,	/* Parallel IO Controller D and E */
	0,
	5,	/* USART 0 */
	5,	/* USART 1 */
	5,	/* USART 2 */
	5,	/* USART 3 */
	0,	/* Multimedia Card Interface 0 */
	6,	/* Two-Wire Interface 0 */
	6,	/* Two-Wire Interface 1 */
	5,	/* Serial Peripheral Interface 0 */
	5,	/* Serial Peripheral Interface 1 */
	4,	/* Serial Synchronous Controller 0 */
	4,	/* Serial Synchronous Controller 1 */
	0,	/* Timer Counter 0, 1, 2, 3, 4 and 5 */
	0,	/* Pulse Width Modulation Controller */
	0,	/* Touch Screen Controller */
	0,	/* DMA Controller */
	2,	/* USB Host High Speed port */
	3,	/* LCD Controller */
	5,	/* AC97 Controller */
	3,	/* Ethernet */
	0,	/* Image Sensor Interface */
	2,	/* USB Device High Speed port */
	0,	/* (reserved) */
	0,	/* Multimedia Card Interface 1 */
	0,	/* (reserved) */
	0,	/* Advanced Interrupt Controller IRQ0 */
};

static const uint32_t at91_pio_base[] = {
	AT91SAM9G45_PIOA_BASE,
	AT91SAM9G45_PIOB_BASE,
	AT91SAM9G45_PIOC_BASE,
	AT91SAM9G45_PIOD_BASE,
	AT91SAM9G45_PIOE_BASE,
};

#define DEVICE(_name, _id, _unit)		\
	{					\
		_name, _unit,			\
		AT91SAM9G45_ ## _id ##_BASE,	\
		AT91SAM9G45_ ## _id ## _SIZE,	\
		AT91SAM9G45_IRQ_ ## _id		\
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
	DEVICE("at91_pio", PIOD, 3),
	DEVICE("at91_pio", PIOE, 4),
	DEVICE("at91_twi", TWI0, 0),
	DEVICE("at91_twi", TWI1, 1),
	DEVICE("at91_mci", HSMCI0, 0),
	DEVICE("at91_mci", HSMCI1, 1),
	DEVICE("uart", DBGU,   0),
	DEVICE("uart", USART0, 1),
	DEVICE("uart", USART1, 2),
	DEVICE("uart", USART2, 3),
	DEVICE("uart", USART3, 4),
	DEVICE("spi",  SPI0,   0),
	DEVICE("spi",  SPI1,   1),
	DEVICE("ate",  EMAC,   0),
	DEVICE("macb", EMAC,   0),
	DEVICE("nand", NAND,   0),
	DEVICE("ohci", OHCI,   0),
	{ 0, 0, 0, 0, 0 }
};

static void
at91_clock_init(void)
{
	struct at91_pmc_clock *clk;

	/* Update USB host port clock info */
	clk = at91_pmc_clock_ref("uhpck");
	clk->pmc_mask  = PMC_SCER_UHP_SAM9;
	at91_pmc_clock_deref(clk);

	/* Each SOC has different PLL contraints */
	clk = at91_pmc_clock_ref("plla");
	clk->pll_min_in    = SAM9G45_PLL_A_MIN_IN_FREQ;		/*   2 MHz */
	clk->pll_max_in    = SAM9G45_PLL_A_MAX_IN_FREQ;		/*  32 MHz */
	clk->pll_min_out   = SAM9G45_PLL_A_MIN_OUT_FREQ;	/* 400 MHz */
	clk->pll_max_out   = SAM9G45_PLL_A_MAX_OUT_FREQ;	/* 800 MHz */
	clk->pll_mul_shift = SAM9G45_PLL_A_MUL_SHIFT;
	clk->pll_mul_mask  = SAM9G45_PLL_A_MUL_MASK;
	clk->pll_div_shift = SAM9G45_PLL_A_DIV_SHIFT;
	clk->pll_div_mask  = SAM9G45_PLL_A_DIV_MASK;
	clk->set_outb      = at91_pmc_800mhz_plla_outb;
	at91_pmc_clock_deref(clk);
}

static struct at91_soc_data soc_data = {
	.soc_delay = at91_pit_delay,
	.soc_reset = at91_rst_cpu_reset,
	.soc_clock_init = at91_clock_init,
	.soc_irq_prio = at91_irq_prio,
	.soc_children = at91_devs,
	.soc_pio_base = at91_pio_base,
	.soc_pio_count = nitems(at91_pio_base),
};

AT91_SOC(AT91_T_SAM9G45, &soc_data);
