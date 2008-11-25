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

#include "opt_at91.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>

#include <arm/at91/at91board.h>
#include <arm/at91/at91rm92reg.h>
#include <arm/at91/at91_piovar.h>
#include <arm/at91/at91_pio_rm9200.h>

long
board_init(void)
{
	/*
	 * Since the USART supports RS-485 multidrop mode, it allows the
	 * TX pins to float.  However, for RS-232 operations, we don't want
	 * these pins to float.  Instead, they should be pulled up to avoid
	 * mismatches.  Linux does something similar when it configures the
	 * TX lines.  This implies that we also allow the RX lines to float
	 * rather than be in the state they are left in by the boot loader.
	 * Since they are input pins, I think that this is the right thing
	 * to do.
	 */

	/* PIOA's A periph: Turn USART 0 and 2's TX/RX pins */
	at91_pio_use_periph_a(AT91RM92_PIOA_BASE,
	    AT91C_PA18_RXD0 | AT91C_PA22_RXD2, 0);
	at91_pio_use_periph_a(AT91RM92_PIOA_BASE,
	    AT91C_PA17_TXD0 | AT91C_PA23_TXD2, 1);
	/* PIOA's B periph: Turn USART 3's TX/RX pins */
	at91_pio_use_periph_b(AT91RM92_PIOA_BASE, AT91C_PA6_RXD3, 0);
	at91_pio_use_periph_b(AT91RM92_PIOA_BASE, AT91C_PA5_TXD3, 1);
#ifdef AT91_TSC
	/* We're using TC0's A1 and A2 input */
	at91_pio_use_periph_b(AT91RM92_PIOA_BASE,
	    AT91C_PA19_TIOA1 | AT91C_PA21_TIOA2, 0);
#endif
	/* PIOB's A periph: Turn USART 1's TX/RX pins */
	at91_pio_use_periph_a(AT91RM92_PIOB_BASE, AT91C_PB21_RXD1, 0);
	at91_pio_use_periph_a(AT91RM92_PIOB_BASE, AT91C_PB20_TXD1, 1);

	/* Pin assignment */
#ifdef AT91_TSC
	/* Assert PA24 low -- talk to rubidium */
	at91_pio_use_gpio(AT91RM92_PIOA_BASE, AT91C_PIO_PA24);
	at91_pio_gpio_output(AT91RM92_PIOA_BASE, AT91C_PIO_PA24, 0);
	at91_pio_gpio_clear(AT91RM92_PIOA_BASE, AT91C_PIO_PA24);
	at91_pio_use_gpio(AT91RM92_PIOB_BASE,
	    AT91C_PIO_PB16 | AT91C_PIO_PB17 | AT91C_PIO_PB18 | AT91C_PIO_PB19);
#endif

	return (at91_ramsize());
}
