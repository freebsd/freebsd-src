/*-
 * Copyright (c) 2012 Marius Strobl <marius@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Ethernut 5 board support
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/board.h>
#include <arm/at91/at91_pioreg.h>
#include <arm/at91/at91_piovar.h>
#include <arm/at91/at91board.h>
#include <arm/at91/at91sam9260reg.h>

BOARD_INIT long
board_init(void)
{

	/*
	 * DBGU
	 */
	/* DRXD */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB14, 0);
	/* DTXD */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB15, 1);

	/*
	 * EMAC
	 */
	/* ETX0 */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA12, 0);
	/* ETX1 */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA13, 0);
	/* ERX0 */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA14, 0);
	/* ERX1 */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA15, 0);
	/* ETXEN */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA16, 0);
	/* ERXDV */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA17, 0);
	/* ERXER */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA18, 0);
	/* ETXCK */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA19, 0);
	/* EMDC */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA20, 0);
	/* EMDIO */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA21, 0);
	/* Not RMII */
	/* ETX2 */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA10, 0);
	/* ETX3 */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA11, 0);
	/* ETXER */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA22, 0);
	/* ERX2 */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA25, 0);
	/* ERX3 */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA26, 0);
	/* ERXCK */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA27, 0);
	/* ECRS */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA28, 0);
	/* ECOL */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA29, 0);


	/*
	 * MMC, wired to socket B.
	 */
	/* MCDB0 */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA0, 1);
	/* MCCDB */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA1, 1);
	/* MCDB3 */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA3, 1);
	/* MCDB2 */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA4, 1);
	/* MCDB1 */
	at91_pio_use_periph_b(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA5, 1);
	/* MCCK */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA8, 1);

	/*
	 * SPI0 and MMC are wired together, since we don't support sharing
	 * don't support the dataflash.  But if you did, you'd have to
	 * use CS0 and CS1.
	 */

	/*
	 * SPI1 is wired to a audio CODEC that we don't support, so
	 * give it a pass.
	 */

	/*
	 * TWI.  Only one child on the iic bus, which we take care of
	 * via hints.
	 */
	/* TWD */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA23, 1);
	/* TWCK */
	at91_pio_use_periph_a(AT91SAM9260_PIOA_BASE, AT91C_PIO_PA24, 1);

	/*
	 * USART0
	 */
	/* TXD0 */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB4, 1);
	/* RXD0 */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB5, 0);
	/* DSR0 */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB22, 0);
	/* DCD0 */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB23, 0);
	/* DTR0 */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB24, 1);
	/* RI0 */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB25, 0);
	/* RTS0 */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB26, 1);
	/* CTS0 */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB27, 0);

	/*
	 * USART1
	 */
	/* RTS1 */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB28, 1);
	/* CTS1 */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB29, 0);
	/* TXD1 */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB6, 1);
	/* RXD1 */
	at91_pio_use_periph_a(AT91SAM9260_PIOB_BASE, AT91C_PIO_PB7, 0);

	/* USART2 - USART5 aren't wired up, except via PIO pins, ignore them. */

	return (at91_ramsize());
}

ARM_BOARD(AT91SAM9260EK, "Atmel SMA9260-EK")
