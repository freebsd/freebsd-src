/*-
 * Copyright (c) 2009 Greg Ansley.  All rights reserved.
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

/* Calao Systems QIL-9G20-Cxx
 * http://www.calao-systems.com 
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>

#include <arm/at91/at91board.h>
#include <arm/at91/at91reg.h>
#include <arm/at91/at91var.h>
#include <arm/at91/at91sam9g20reg.h>
#include <arm/at91/at91_piovar.h>
#include <arm/at91/at91_pio_sam9g20.h>
//#include <arm/at91/at91_led.h>

#define AT91SAM9G20_LED_BASE AT91SAM9G20_PIOA_BASE
#define AT91SAM9G20_LED_SIZE AT91SAM9G20_PIO_SIZE
#define AT91SAM9G20_IRQ_LED AT91SAM9G20_IRQ_PIOA

long
board_init(void)
{

	//at91_led_create("power", 0, 9, 0);

	/* PIOB's A periph: Turn USART 0's TX/RX pins */
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB14_DRXD, 0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB15_DTXD, 1);

	/* PIOB's A periph: Turn USART 0's TX/RX pins */
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB4_TXD0, 1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB5_RXD0, 0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB22_DSR0, 0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB23_DCD0, 0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB24_DTR0, 1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB25_RI0, 0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB26_RTS0, 1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB27_CTS0, 0);

	/* PIOB's A periph: Turn USART 1's TX/RX pins */
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB6_TXD1, 1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB7_RXD1, 0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB28_RTS1, 1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOB_BASE, AT91C_PB29_CTS1, 0);

	/*  TWI Two-wire Serial Data */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA23_TWD,  1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA24_TWCK, 1);

	/*  Multimedia Card  */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA6_MCDA0,  1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA7_MCCDA,  1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA8_MCCK,   1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA9_MCDA1,  1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA10_MCDA2, 1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA11_MCDA3, 1);

	/* SPI0 to DataFlash */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PA0_SPI0_MISO,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PA1_SPI0_MOSI,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PA2_SPI0_SPCK,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PA3_SPI0_NPCS0, 0);

	/* EMAC */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA19_ETXCK,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA21_EMDIO,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA20_EMDC,   0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA17_ERXDV,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA16_ETXEN,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA12_ETX0 ,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA13_ETX1,   0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA14_ERX0,   0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA15_ERX1,   0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA18_ERXER,  0);


	return (at91_ramsize());
}
