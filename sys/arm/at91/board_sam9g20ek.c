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

/* Atmel AT91SAM9G20EK Rev. B Development Card */

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

#if 1
	/*  TWI Two-wire Serial Data */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA23_TWD,  1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA24_TWCK, 1);
#endif
#if 1
	/* 
	 * Turn off Clock to DataFlash, conflicts with MCI clock.
	 * Remove resistor R42 if you need both DataFlash and SD Card
	 * access at the same time
	 */
	at91_pio_use_gpio(AT91SAM9G20_PIOA_BASE,AT91C_PIO_PA2);
	at91_pio_gpio_input(AT91SAM9G20_PIOA_BASE,AT91C_PIO_PA2);

	/* Turn off chip select to DataFlash */
	at91_pio_gpio_output(AT91SAM9G20_PIOC_BASE,AT91C_PIO_PC11, 0);
	at91_pio_gpio_set(AT91SAM9G20_PIOC_BASE,AT91C_PIO_PC11);
	at91_pio_use_gpio(AT91SAM9G20_PIOC_BASE,AT91C_PIO_PC11);

	/*  Multimedia Card  */
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA0_MCDB0, 1);
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA1_MCCDB, 1);
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA3_MCDB3, 1);
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA4_MCDB2, 1);
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA5_MCDB1, 1);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA8_MCCK,  1);
	at91_pio_use_gpio(AT91SAM9G20_PIOC_BASE, AT91C_PIO_PC9);
#else
	/* SPI0 to DataFlash */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA0, 0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA1, 0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE, AT91C_PIO_PA2, 0);
	at91_pio_use_periph_b(AT91SAM9G20_PIOC_BASE, AT91C_PIO_PC11,0);

	at91_pio_gpio_input(AT91SAM9G20_PIOA_BASE,AT91C_PIO_PA8);
	at91_pio_use_gpio(AT91SAM9G20_PIOA_BASE,AT91C_PIO_PA8);
#endif

#if 0
	static int at91_base = AT91SAM9G20_BASE; 
	volatile uint32_t *PIO = (uint32_t *)(at91_base + AT91SAM9G20_PIOA_BASE);
	volatile uint32_t *RST = (uint32_t *)(at91_base + AT91SAM9G20_RSTC_BASE);
	/*
	* Disable pull-up on:
	*      ERX0 (PA14) => PHY ADDR0
	*      ERX1 (PA15) => PHY ADDR1
	*      RXDV (PA17) => PHY normal mode (not Test mode)
	*      ERX2 (PA25) => PHY ADDR2
	*      ERX3 (PA26) => PHY ADDR3
	*      ECRS (PA28) => PHY ADDR4  => PHYADDR = 0x0
	*
	* PHY has internal pull-down
	*/
	PIO[PIO_PUDR/4] = 
		AT91C_PA14_ERX0 |
		AT91C_PA15_ERX1 |
		AT91C_PA17_ERXDV |
		AT91C_PA25_ERX2 |
		AT91C_PA26_ERX3 |
		AT91C_PA28_ECRS;


	/* Reset PHY - 500ms */
	RST[2] = 0xA5000D01;
	RST[0] = 0xA5000080;
	while  (!(RST[1] & (1 << 16))) ;

	PIO[PIO_PUER/4] = 
		AT91C_PA14_ERX0 |
		AT91C_PA15_ERX1 |
		AT91C_PA17_ERXDV |
		AT91C_PA25_ERX2 |
		AT91C_PA26_ERX3 |
		AT91C_PA28_ECRS;
#endif


	/* EMAC */
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA12_ETX0 ,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA13_ETX1,   0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA14_ERX0,   0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA15_ERX1,   0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA16_ETXEN,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA17_ERXDV,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA18_ERXER,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA19_ETXCK,  0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA20_EMDC,   0);
	at91_pio_use_periph_a(AT91SAM9G20_PIOA_BASE,AT91C_PA21_EMDIO,  0);

	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA10_ETX2_0, 0);
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA11_ETX3_0, 0);
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA22_ETXER,  0);
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA25_ERX2,   0);
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA26_ERX3,   0);
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA27_ERXCK,  0);
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA28_ECRS,   0);
	at91_pio_use_periph_b(AT91SAM9G20_PIOA_BASE,AT91C_PA29_ECOL,   0);

#if 0
	/* Handle Missing ETXER line */
	at91_pio_use_gpio(AT91SAM9G20_PIOA_BASE,AT91C_PA22_ETXER);
	at91_pio_gpio_output(AT91SAM9G20_PIOA_BASE,AT91C_PA22_ETXER, 0);
	at91_pio_gpio_clear(AT91SAM9G20_PIOA_BASE,AT91C_PA22_ETXER);
#endif
	return (at91_ramsize());
}
