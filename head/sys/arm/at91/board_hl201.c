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

#include <arm/at91/at91board.h>
#include <arm/at91/at91sam9g20reg.h>
#include <arm/at91/at91_piovar.h>
#include <arm/at91/at91_pio_sam9g20.h>

long
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

	return (at91_ramsize());
}
