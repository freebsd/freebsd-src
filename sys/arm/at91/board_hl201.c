/*-
 * Copyright (c) 2005-2008 Olivier Houchard.  All rights reserved.
 * Copyright (c) 2005-2008 Warner Losh.  All rights reserved.
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

#include <machine/board.h>
#include <arm/at91/at91board.h>
#include <arm/at91/at91sam9g20reg.h>
#include <arm/at91/at91_piovar.h>
#include <arm/at91/at91_pio_sam9g20.h>
#include <arm/at91/at91_smc.h>
#include <arm/at91/at91_gpio.h>
#include <dev/nand/nfc_at91.h>

static struct at91_smc_init nand_smc = {
	.ncs_rd_setup		= 0,
	.nrd_setup		= 2,
	.ncs_wr_setup		= 0,
	.nwe_setup		= 2,

	.ncs_rd_pulse		= 4,
	.nrd_pulse		= 4,
	.ncs_wr_pulse		= 4,
	.nwe_pulse		= 4,

	.nrd_cycle		= 7,
	.nwe_cycle		= 7,

	.mode			= SMC_MODE_READ | SMC_MODE_WRITE | SMC_MODE_EXNW_DISABLED,
	.tdf_cycles		= 3,
};

static struct at91_nand_params nand_param = {
	.ale			= 1u << 21,
	.cle			= 1u << 22,
	.width			= 8,
	.rnb_pin		= AT91_PIN_PC13,	/* Note: These pins not confirmed */
	.nce_pin		= AT91_PIN_PC14,
	.cs			= 3,
};

BOARD_INIT long
board_init(void)
{
	/* Setup Ethernet Pins */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, 1<<7, 0);

	at91_pio_gpio_input(AT91SAM9G20_PIOA_BASE, 1<<7);
	at91_pio_gpio_set_deglitch(AT91SAM9G20_PIOA_BASE, 1<<7, 1);

	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA19, 0);	/* ETXCK_EREFCK */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA17, 0);	/* ERXDV */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA14, 0);	/* ERX0 */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA15, 0);	/* ERX1 */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA18, 0);	/* ERXER */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA16, 0);	/* ETXEN */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA12, 0);	/* ETX0 */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA13, 0);	/* ETX1 */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA21, 0);	/* EMDIO */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA20, 0);	/* EMDC */

	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA28, 0);	/* ECRS */
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA29, 0);	/* ECOL */
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA25, 0);	/* ERX2 */
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA26, 0);	/* ERX3 */
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA27, 0);	/* ERXCK */
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA23, 0);	/* ETX2 */
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA24, 0);	/* ETX3 */
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA22, 0);	/* ETXER */

	/* Setup Static Memory Controller */
	at91_smc_setup(0, 3, &nand_smc);
	at91_enable_nand(&nand_param);

	/*
	 * This assumes
	 *  - RNB is on pin PC13
	 *  - CE is on pin PC14
	 *
	 * Nothing actually uses RNB right now.
	 *
	 * For CE, this currently asserts it during board setup and leaves it
	 * that way forever.
	 *
	 * All this can go away when the gpio pin-renumbering happens...
	 */
	at91_pio_use_gpio(AT91SAM9G20_PIOC_BASE, AT91C_PIO_PC13 | AT91C_PIO_PC14);
	at91_pio_gpio_input(AT91SAM9G20_PIOC_BASE, AT91C_PIO_PC13);	/* RNB */
	at91_pio_gpio_output(AT91SAM9G20_PIOC_BASE, AT91C_PIO_PC14, 0);	/* nCS */
	at91_pio_gpio_clear(AT91SAM9G20_PIOC_BASE, AT91C_PIO_PC14);	/* Assert nCS */

	return (at91_ramsize());
}

ARM_BOARD(NONE, "HOTe 201");
