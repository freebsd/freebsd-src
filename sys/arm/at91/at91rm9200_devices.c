/*-
 * Copyright (c) 2012 M. Warner Losh.  All Rights Reserved.
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
#include <arm/at91/at91board.h>
#include <arm/at91/at91rm92reg.h>
#include <arm/at91/at91rm9200var.h>
#include <arm/at91/at91_pioreg.h>
#include <arm/at91/at91_piovar.h>

/*
 * The AT91RM9200 uses the same silicon for both the BGA and PQFP
 * packages.  There's no documented way to detect this at runtime,
 * so we require the board code to register what type of SoC is on the
 * board in question.  The pinouts are not quite compatible, and we
 * use this information to cope with the slight differences.
 */
void
at91rm9200_set_subtype(enum at91_soc_subtype st)
{

	switch (st) {
	case AT91_ST_RM9200_BGA:
	case AT91_ST_RM9200_PQFP:
		soc_info.subtype = st;
		break;
	default:
		panic("Bad SoC subtype %d for at91rm9200_set_subtype.", st);
		break;
	}
}

void
at91rm9200_config_uart(unsigned devid, unsigned unit, unsigned pinmask)
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

	/*
	 * Current boards supported don't need the extras, but they should be
	 * implemented.  But that should wait until the new pin api goes in.
	 */
	switch (devid) {
	case AT91_ID_DBGU:
		at91_pio_use_periph_a(AT91RM92_PIOA_BASE, AT91C_PIO_PA30, 0); /* DRXD */
		at91_pio_use_periph_a(AT91RM92_PIOA_BASE, AT91C_PIO_PA31, 1); /* DTXD */
		break;

	case AT91RM9200_ID_USART0:
		at91_pio_use_periph_a(AT91RM92_PIOA_BASE, AT91C_PIO_PA17, 1); /* TXD0 */
		at91_pio_use_periph_a(AT91RM92_PIOA_BASE, AT91C_PIO_PA18, 0); /* RXD0 */
		/* CTS PA20 */
		/* RTS -- errata #39 PA21 */
		break;

	case AT91RM9200_ID_USART1:
		at91_pio_use_periph_a(AT91RM92_PIOB_BASE, AT91C_PIO_PB20, 1); /* TXD1 */
		at91_pio_use_periph_a(AT91RM92_PIOB_BASE, AT91C_PIO_PB21, 0); /* RXD1 */
		/* RI - PB18 */
		/* DTR - PB19 */
		/* DCD - PB23 */
		/* CTS - PB24 */
		/* DSR - PB25 */
		/* RTS - PB26 */
		break;

	case AT91RM9200_ID_USART2:
		at91_pio_use_periph_a(AT91RM92_PIOA_BASE, AT91C_PIO_PA22, 0); /* RXD2 */
		at91_pio_use_periph_a(AT91RM92_PIOA_BASE, AT91C_PIO_PA23, 1); /* TXD2 */
		/* CTS - PA30 B periph */
		/* RTS - PA31 B periph */
		break;

	case AT91RM9200_ID_USART3:
		at91_pio_use_periph_b(AT91RM92_PIOA_BASE, AT91C_PIO_PA5, 1); /* TXD3 */
		at91_pio_use_periph_b(AT91RM92_PIOA_BASE, AT91C_PIO_PA6, 0); /* RXD3 */
		/* CTS - PB0 B periph */
		/* RTS - PB1 B periph */
		break;

	default:
		break;
	}
}

void
at91rm9200_config_mci(int has_4wire)
{
	/* XXX TODO chip changed GPIO, other slots, etc */
	at91_pio_use_periph_a(AT91RM92_PIOA_BASE, AT91C_PIO_PA27,  0); /* MCCK */
	at91_pio_use_periph_a(AT91RM92_PIOA_BASE, AT91C_PIO_PA28, 1);  /* MCCDA */
	at91_pio_use_periph_a(AT91RM92_PIOA_BASE, AT91C_PIO_PA29, 1);  /* MCDA0 */
	if (has_4wire) {
		at91_pio_use_periph_b(AT91RM92_PIOB_BASE, AT91C_PIO_PB3, 1); /* MCDA1 */
		at91_pio_use_periph_b(AT91RM92_PIOB_BASE, AT91C_PIO_PB4, 1); /* MCDA2 */
		at91_pio_use_periph_b(AT91RM92_PIOB_BASE, AT91C_PIO_PB5, 1); /* MCDA3 */
	}
}
