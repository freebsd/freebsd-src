/*-
 * Copyright (c) 2006 Wojciech A. Koszek <wkoszek@FreeBSD.org>
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
 *
 * $Id: uart_cpu_mips_xlr.c,v 1.5 2008-07-16 20:22:39 jayachandranc Exp $
 */
/*
 * Skeleton of this file was based on respective code for ARM
 * code written by Olivier Houchard.
 */
/*
 * XLRMIPS: This file is hacked from arm/... 
 */
#include "opt_uart.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cons.h>

#include <machine/bus.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

static int xlr_uart_probe(struct uart_bas *bas);
static void xlr_uart_init(struct uart_bas *bas, int, int, int, int);
static void xlr_uart_term(struct uart_bas *bas);
static void xlr_uart_putc(struct uart_bas *bas, int);
static int xlr_uart_poll(struct uart_bas *bas);
static int xlr_uart_getc(struct uart_bas *bas);
struct mtx xlr_uart_mtx;    /*UartLock*/

struct uart_ops xlr_uart_ns8250_ops = {
	.probe = xlr_uart_probe,
	.init = xlr_uart_init,
	.term = xlr_uart_term,
	.putc = xlr_uart_putc,
	.poll = xlr_uart_poll,
	.getc = xlr_uart_getc,
};

bus_space_tag_t uart_bus_space_io;
bus_space_tag_t uart_bus_space_mem;

static __inline void xlr_uart_lock(struct mtx *hwmtx)
{
	if(!mtx_initialized(hwmtx))
		return;
	if(!kdb_active && hwmtx != NULL)
		mtx_lock_spin(hwmtx);		
}

static __inline void xlr_uart_unlock(struct mtx *hwmtx)
{
	if(!mtx_initialized(hwmtx))
		return;
	if(!kdb_active && hwmtx != NULL)
		mtx_unlock_spin(hwmtx);		
}


static int xlr_uart_probe(struct uart_bas *bas)
{
	int res;
	xlr_uart_lock(&xlr_uart_mtx);
	res = uart_ns8250_ops.probe(bas);
	xlr_uart_unlock(&xlr_uart_mtx);
	return res;
}

static void xlr_uart_init(struct uart_bas *bas, int baudrate, int databits, 
				int stopbits, int parity)

{
	xlr_uart_lock(&xlr_uart_mtx);
	uart_ns8250_ops.init(bas,baudrate,databits,stopbits,parity);
	xlr_uart_unlock(&xlr_uart_mtx);
}

static void xlr_uart_term(struct uart_bas *bas)
{
	xlr_uart_lock(&xlr_uart_mtx);
	uart_ns8250_ops.term(bas);
	xlr_uart_unlock(&xlr_uart_mtx);
}

static void xlr_uart_putc(struct uart_bas *bas, int c)
{
	xlr_uart_lock(&xlr_uart_mtx);
	uart_ns8250_ops.putc(bas,c);
	xlr_uart_unlock(&xlr_uart_mtx);
}

static int xlr_uart_poll(struct uart_bas *bas)
{
	int res;
	xlr_uart_lock(&xlr_uart_mtx);
	res = uart_ns8250_ops.poll(bas);
	xlr_uart_unlock(&xlr_uart_mtx);
	return res;
}

static int xlr_uart_getc(struct uart_bas *bas)
{
	return uart_ns8250_ops.getc(bas);
}

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{
	return ((b1->bsh == b2->bsh && b1->bst == b2->bst) ? 1 : 0);
}


int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	di->ops = xlr_uart_ns8250_ops;
	di->bas.chan = 0;
	di->bas.bst = uart_bus_space_mem;
	/* TODO Need to call bus_space_map() here */
	di->bas.bsh = 0xbef14000; /* Try with UART0 */
	di->bas.regshft = 2;
	/* divisor = rclk / (baudrate * 16); */
	di->bas.rclk = 66000000;

	di->baudrate = 38400;
	di->databits = 8;
	di->stopbits = 1;
	di->parity = UART_PARITY_NONE;

	/* TODO: Read env variables for all console parameters */

	return (0);
}

static void xlr_uart_mtx_init(void *dummy __unused)
{
	mtx_init(&xlr_uart_mtx, "uart lock",NULL,MTX_SPIN);
}
SYSINIT(xlr_init_uart_mtx, SI_SUB_LOCK, SI_ORDER_ANY, xlr_uart_mtx_init, NULL);

