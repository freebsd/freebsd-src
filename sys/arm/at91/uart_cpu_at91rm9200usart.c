/*-
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_uart.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/at91/uart_cpu_at91rm9200usart.c,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <machine/bus.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>

#include <arm/at91/at91rm92reg.h>

bus_space_tag_t uart_bus_space_io;
bus_space_tag_t uart_bus_space_mem;

extern struct uart_class at91_usart_class;
extern struct bus_space at91_bs_tag;

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{
	return ((b1->bsh == b2->bsh && b1->bst == b2->bst) ? 1 : 0);
}

int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	struct uart_class *class;

	class = &at91_usart_class;
	di->ops = uart_getops(class);
	di->bas.chan = 0;
	di->bas.bst = &at91_bs_tag;
	/* 
	 * XXX: Not pretty, but will work because we map VA == PA early
	 * for the last 1MB of memory.
	 */
#ifdef SKYEYE_WORKAROUNDS
	di->bas.bsh = AT91RM92_BASE + AT91RM92_USART0_BASE;
	di->baudrate = 38400;
#else
	di->bas.bsh = AT91RM92_BASE + AT91RM92_SYS_BASE + DBGU;
	di->baudrate = 115200;
#endif
	di->bas.regshft = 0;
	di->bas.rclk = 0;
	di->databits = 8;
	di->stopbits = 1;
	di->parity = UART_PARITY_NONE;
	uart_bus_space_io = &at91_bs_tag;
	uart_bus_space_mem = NULL;
	/* Check the environment for overrides */
	uart_getenv(devtype, di, class);
	return (0);
}
