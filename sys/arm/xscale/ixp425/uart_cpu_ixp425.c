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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/xscale/ixp425/uart_cpu_ixp425.c,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <machine/bus.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

bus_space_tag_t uart_bus_space_io;
bus_space_tag_t uart_bus_space_mem;

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{
	return ((b1->bsh == b2->bsh && b1->bst == b2->bst) ? 1 : 0);
}

int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	uint32_t i, ivar, vaddr;

	/*
	 * Scan the hints. The IXP425 only have 2 serial ports, so only
	 * scan them.
	 */
	for (i = 0; i < 2; i++) {
		if (resource_int_value("uart", i, "flags", &ivar))
			continue;
		if (devtype == UART_DEV_CONSOLE && !UART_FLAGS_CONSOLE(ivar))
			continue;
		if (devtype == UART_DEV_DBGPORT && !UART_FLAGS_DBGPORT(ivar))
			continue;
		/*
		 * We have a possible device. Make sure it's enabled and
		 * that we have an I/O port.
		 */
		if (resource_int_value("uart", i, "disabled", &ivar) == 0 &&
		    ivar != 0)
			continue;
		if (resource_int_value("uart", i, "addr", &ivar) != 0 ||
		    ivar == 0)
			continue;
		/* Got it. Fill in the instance and return it. */
		di->ops = uart_getops(&uart_ns8250_class);
		di->bas.chan = 0;
		di->bas.bst = &ixp425_a4x_bs_tag;
		di->bas.regshft = 0;
		di->bas.rclk = IXP425_UART_FREQ;
		di->baudrate = 115200;
		di->databits = 8;
		di->stopbits = 1;
		di->parity = UART_PARITY_NONE;
		uart_bus_space_io = NULL;
		uart_bus_space_mem = &ixp425_a4x_bs_tag;

		getvbase(ivar, IXP425_REG_SIZE, &vaddr);
		di->bas.bsh = vaddr;
		return (0);
	}

	return (ENXIO);
}
