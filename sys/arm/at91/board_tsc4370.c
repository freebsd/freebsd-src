/*-
 * Copyright (c) 2005-2008 Olivier Houchard.  All rights reserved.
 * Copyright (c) 2005-2012 Warner Losh.  All rights reserved.
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
#include <arm/at91/at91var.h>
#include <arm/at91/at91rm92reg.h>
#include <arm/at91/at91rm9200var.h>
#include <arm/at91/at91_piovar.h>
#include <arm/at91/at91_pioreg.h>

BOARD_INIT long
board_init(void)
{

	at91rm9200_set_subtype(AT91_ST_RM9200_PQFP);

	at91rm9200_config_uart(AT91_ID_DBGU, 0, 0);   /* DBGU just Tx and Rx */
	at91rm9200_config_uart(AT91RM9200_ID_USART0, 1, 0);   /* Tx and Rx */
	at91rm9200_config_uart(AT91RM9200_ID_USART1, 2, 0);   /* Tx and Rx */
	at91rm9200_config_uart(AT91RM9200_ID_USART2, 3, 0);   /* Tx and Rx */
	at91rm9200_config_uart(AT91RM9200_ID_USART3, 4, 0);   /* Tx and Rx */

	at91rm9200_config_mci(0);			/* tsc4370 board has only 1 wire */
							/* Newer boards may have 4 wires */

	/* Configure TWI */
	/* Configure SPI + dataflash */
	/* Configure SSC */
	/* Configure USB Host */
	/* Configure FPGA attached to chip selects */

	/* Pin assignment */
	/* Assert PA24 low -- talk to rubidium */
	at91_pio_use_gpio(AT91RM92_PIOA_BASE, AT91C_PIO_PA24);
	at91_pio_gpio_output(AT91RM92_PIOA_BASE, AT91C_PIO_PA24, 0);
	at91_pio_gpio_clear(AT91RM92_PIOA_BASE, AT91C_PIO_PA24);
	at91_pio_use_gpio(AT91RM92_PIOB_BASE,
	    AT91C_PIO_PB16 | AT91C_PIO_PB17 | AT91C_PIO_PB18 | AT91C_PIO_PB19);

	return (at91_ramsize());
}

ARM_BOARD(NONE, "TSC4370 Controller Board");
