/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Andrew Turner
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#include <sys/param.h>

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "uart_backend.h"
#include "uart_emul.h"

#define	UART_FIFO_SIZE		16

#define	UARTDR			0x00
#define	UARTDR_RSR_SHIFT	8

#define	UARTRSR			0x01
#define	UARTRSR_OE		(1 << 3)

#define	UARTFR			0x06
#define	UARTFR_TXFE		(1 << 7)
#define	UARTFR_RXFF		(1 << 6)
#define	UARTFR_TXFF		(1 << 5)
#define	UARTFR_RXFE		(1 << 4)

#define	UARTRTINTR		(1 << 6)
#define	UARTTXINTR		(1 << 5)
#define	UARTRXINTR		(1 << 4)

#define	UARTIBRD		0x09

#define	UARTFBRD		0x0a
#define	UARTFBRD_MASK		0x003f

#define	UARTLCR_H		0x0b
#define	UARTLCR_H_MASK		0x00ff
#define	UARTLCR_H_FEN		(1 << 4)

#define	UARTCR			0x0c
/* TODO: Check the flags in the UARTCR register */
#define	UARTCR_MASK		0xffc7
#define	UARTCR_LBE		(1 << 7)

#define	UARTIFLS		0x0d
#define	UARTIFLS_MASK		0x003f
#define	UARTIFLS_RXIFLSEL(x)	(((x) >> 3) & 0x7)
#define	UARTIFLS_TXIFLSEL(x)	(((x) >> 0) & 0x7)

#define	UARTIMSC		0x0e
#define	UARTIMSC_MASK		0x07ff

#define	UARTRIS			0x0f
#define	UARTMIS			0x10

#define	UARTICR			0x11

#define	UARTPeriphID		0x00241011
#define	UARTPeriphID0		0x3f8
#define	UARTPeriphID0_VAL	(((UARTPeriphID) >>  0) & 0xff)
#define	UARTPeriphID1		0x3f9
#define	UARTPeriphID1_VAL	(((UARTPeriphID) >>  8) & 0xff)
#define	UARTPeriphID2		0x3fa
#define	UARTPeriphID2_VAL	(((UARTPeriphID) >> 16) & 0xff)
#define	UARTPeriphID3		0x3fb
#define	UARTPeriphID3_VAL	(((UARTPeriphID) >> 24) & 0xff)

#define	UARTPCellID		0xb105f00d
#define	UARTPCellID0		0x3fc
#define	UARTPCellID0_VAL	(((UARTPCellID) >>  0) & 0xff)
#define	UARTPCellID1		0x3fd
#define	UARTPCellID1_VAL	(((UARTPCellID) >>  8) & 0xff)
#define	UARTPCellID2		0x3fe
#define	UARTPCellID2_VAL	(((UARTPCellID) >> 16) & 0xff)
#define	UARTPCellID3		0x3ff
#define	UARTPCellID3_VAL	(((UARTPCellID) >> 24) & 0xff)

struct uart_pl011_softc {
	struct uart_softc *backend;
	pthread_mutex_t mtx;	/* protects all softc elements */

	uint16_t	irq_state;

	uint16_t	rsr;

	uint16_t	cr;
	uint16_t	ifls;
	uint16_t	imsc;
	uint16_t	lcr_h;

	uint16_t	ibrd;
	uint16_t	fbrd;

	void	*arg;
	uart_intr_func_t intr_assert;
	uart_intr_func_t intr_deassert;
};

static void
uart_reset(struct uart_pl011_softc *sc)
{
	sc->ifls = 0x12;

	/* no fifo until enabled by software */
	uart_rxfifo_reset(sc->backend, 1);
}

static int
uart_rx_trigger_level(struct uart_pl011_softc *sc)
{
	/* If the FIFO is disabled trigger when we have any data */
	if ((sc->lcr_h & UARTLCR_H_FEN) != 0)
		return (1);

	/* Trigger base on how full the fifo is */
	switch (UARTIFLS_RXIFLSEL(sc->ifls)) {
	case 0:
		return (UART_FIFO_SIZE / 8);
	case 1:
		return (UART_FIFO_SIZE / 4);
	case 2:
		return (UART_FIFO_SIZE / 2);
	case 3:
		return (UART_FIFO_SIZE * 3 / 4);
	case 4:
		return (UART_FIFO_SIZE * 7 / 8);
	default:
		/* TODO: Find out what happens in this case */
		return (UART_FIFO_SIZE);
	}
}

static void
uart_toggle_intr(struct uart_pl011_softc *sc)
{
	if ((sc->irq_state & sc->imsc) == 0)
		(*sc->intr_deassert)(sc->arg);
	else
		(*sc->intr_assert)(sc->arg);
}

static void
uart_drain(int fd __unused, enum ev_type ev, void *arg)
{
	struct uart_pl011_softc *sc;
	int old_size, trig_lvl;
	bool loopback;

	sc = arg;

	assert(ev == EVF_READ);

	/*
	 * This routine is called in the context of the mevent thread
	 * to take out the softc lock to protect against concurrent
	 * access from a vCPU i/o exit
	 */
	pthread_mutex_lock(&sc->mtx);

	old_size = uart_rxfifo_numchars(sc->backend);

	loopback = (sc->cr & UARTCR_LBE) != 0;
	uart_rxfifo_drain(sc->backend, loopback);

	/* If we cross the trigger level raise UARTRXINTR */
	trig_lvl = uart_rx_trigger_level(sc);
	if (old_size < trig_lvl &&
	    uart_rxfifo_numchars(sc->backend) >= trig_lvl)
		sc->irq_state |= UARTRXINTR;

	if (uart_rxfifo_numchars(sc->backend) > 0)
		sc->irq_state |= UARTRTINTR;
	if (!loopback)
		uart_toggle_intr(sc);

	pthread_mutex_unlock(&sc->mtx);
}

void
uart_pl011_write(struct uart_pl011_softc *sc, int offset, uint32_t value)
{
	bool loopback;

	pthread_mutex_lock(&sc->mtx);
	switch (offset) {
	case UARTDR:
		loopback = (sc->cr & UARTCR_LBE) != 0;
		if (uart_rxfifo_putchar(sc->backend, value & 0xff, loopback))
			sc->rsr |= UARTRSR_OE;

		/* We don't have a TX fifo, so trigger when we have data */
		sc->irq_state |= UARTTXINTR;
		break;
	case UARTRSR:
		/* Any write clears this register */
		sc->rsr = 0;
		break;
	case UARTFR:
		/* UARTFR is a read-only register */
		break;
	/* TODO: UARTILPR */
	case UARTIBRD:
		sc->ibrd = value;
		break;
	case UARTFBRD:
		sc->fbrd = value & UARTFBRD_MASK;
		break;
	case UARTLCR_H:
		/* Check if the FIFO enable bit changed */
		if (((sc->lcr_h ^ value) & UARTLCR_H_FEN) != 0) {
			if ((value & UARTLCR_H_FEN) != 0) {
				uart_rxfifo_reset(sc->backend, UART_FIFO_SIZE);
			} else {
				uart_rxfifo_reset(sc->backend, 1);
			}
		}
		sc->lcr_h = value & UARTLCR_H_MASK;
		break;
	case UARTCR:
		sc->cr = value & UARTCR_MASK;
		break;
	case UARTIFLS:
		sc->ifls = value & UARTCR_MASK;
		break;
	case UARTIMSC:
		sc->imsc = value & UARTIMSC_MASK;
		break;
	case UARTRIS:
	case UARTMIS:
		/* UARTRIS and UARTMIS are read-only registers */
		break;
	case UARTICR:
		sc->irq_state &= ~value;
		break;
	default:
		/* Ignore writes to unassigned/ID registers */
		break;
	}
	uart_toggle_intr(sc);
	pthread_mutex_unlock(&sc->mtx);
}

uint32_t
uart_pl011_read(struct uart_pl011_softc *sc, int offset)
{
	uint32_t reg;
	int fifo_sz;

	reg = 0;
	pthread_mutex_lock(&sc->mtx);
	switch (offset) {
	case UARTDR:
		reg = uart_rxfifo_getchar(sc->backend);
		/* Deassert the irq if below the trigger level */
		fifo_sz = uart_rxfifo_numchars(sc->backend);
		if (fifo_sz < uart_rx_trigger_level(sc))
			sc->irq_state &= ~UARTRXINTR;
		if (fifo_sz == 0)
			sc->irq_state &= ~UARTRTINTR;

		reg |= sc->rsr << UARTDR_RSR_SHIFT;

		/* After reading from the fifo there is now space in it */
		sc->rsr &= UARTRSR_OE;
		break;
	case UARTRSR:
		/* Any write clears this register */
		reg = sc->rsr;
		break;
	case UARTFR:
		/* Transmit is intstant, so the fifo is always empty */
		reg = UARTFR_TXFE;

		/* Set the receive fifo full/empty flags */
		fifo_sz = uart_rxfifo_numchars(sc->backend);
		if (fifo_sz == UART_FIFO_SIZE)
			reg |= UARTFR_RXFF;
		else if (fifo_sz == 0)
			reg |= UARTFR_RXFE;
		break;
	/* TODO: UARTILPR */
	case UARTIBRD:
		reg = sc->ibrd;
		break;
	case UARTFBRD:
		reg = sc->fbrd;
		break;
	case UARTLCR_H:
		reg = sc->lcr_h;
		break;
	case UARTCR:
		reg = sc->cr;
		break;
	case UARTIMSC:
		reg = sc->imsc;
		break;
	case UARTRIS:
		reg = sc->irq_state;
		break;
	case UARTMIS:
		reg = sc->irq_state & sc->imsc;
		break;
	case UARTICR:
		reg = 0;
		break;
	case UARTPeriphID0:
		reg = UARTPeriphID0_VAL;
		break;
	case UARTPeriphID1:
		reg =UARTPeriphID1_VAL;
		break;
	case UARTPeriphID2:
		reg = UARTPeriphID2_VAL;
		break;
	case UARTPeriphID3:
		reg = UARTPeriphID3_VAL;
		break;
	case UARTPCellID0:
		reg = UARTPCellID0_VAL;
		break;
	case UARTPCellID1:
		reg = UARTPCellID1_VAL;
		break;
	case UARTPCellID2:
		reg = UARTPCellID2_VAL;
		break;
	case UARTPCellID3:
		reg = UARTPCellID3_VAL;
		break;
	default:
		/* Return 0 in reads from unasigned registers */
		reg = 0;
		break;
	}
	uart_toggle_intr(sc);
	pthread_mutex_unlock(&sc->mtx);

	return (reg);
}

struct uart_pl011_softc *
uart_pl011_init(uart_intr_func_t intr_assert, uart_intr_func_t intr_deassert,
    void *arg)
{
	struct uart_pl011_softc *sc;

	sc = calloc(1, sizeof(struct uart_pl011_softc));

	sc->arg = arg;
	sc->intr_assert = intr_assert;
	sc->intr_deassert = intr_deassert;
	sc->backend = uart_init();

	pthread_mutex_init(&sc->mtx, NULL);

	uart_reset(sc);

	return (sc);
}

int
uart_pl011_tty_open(struct uart_pl011_softc *sc, const char *device)
{
	return (uart_tty_open(sc->backend, device, uart_drain, sc));
}
