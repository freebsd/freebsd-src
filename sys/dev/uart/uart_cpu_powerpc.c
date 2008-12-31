/*-
 * Copyright (c) 2006 Marcel Moolenaar
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
__FBSDID("$FreeBSD: src/sys/dev/uart/uart_cpu_powerpc.c,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>

bus_space_tag_t uart_bus_space_io = PPC_BUS_SPACE_IO;
bus_space_tag_t uart_bus_space_mem = PPC_BUS_SPACE_MEM;

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{

	return ((b1->bsh == b2->bsh) ? 1 : 0);
}

int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	char buf[64];
	struct uart_class *class;
	phandle_t input, opts;
	int error;

	class = &uart_z8530_class;
	if (class == NULL)
		return (ENXIO);

	if ((opts = OF_finddevice("/options")) == -1)
		return (ENXIO);
	switch (devtype) {
	case UART_DEV_CONSOLE:
		if (OF_getprop(opts, "input-device", buf, sizeof(buf)) == -1)
			return (ENXIO);
		input = OF_finddevice(buf);
		if (input == -1)
			return (ENXIO);
		if (OF_getprop(opts, "output-device", buf, sizeof(buf)) == -1)
			return (ENXIO);
		if (OF_finddevice(buf) != input)
			return (ENXIO);
		break;
	case UART_DEV_DBGPORT:
		if (!getenv_string("hw.uart.dbgport", buf, sizeof(buf)))
			return (ENXIO);
		input = OF_finddevice(buf);
		if (input == -1)
			return (ENXIO);
		break;
	default:
		return (EINVAL);
	}

	if (OF_getprop(input, "device_type", buf, sizeof(buf)) == -1)
		return (ENXIO);
	if (strcmp(buf, "serial") != 0)
		return (ENXIO);
	if (OF_getprop(input, "name", buf, sizeof(buf)) == -1)
		return (ENXIO);
	if (strcmp(buf, "ch-a"))
		return (ENXIO);

	error = OF_decode_addr(input, 0, &di->bas.bst, &di->bas.bsh);
	if (error)
		return (error);

	di->ops = uart_getops(class);

	di->bas.rclk = 230400;
	di->bas.chan = 1;
	di->bas.regshft = 4;

	di->baudrate = 0;
	di->databits = 8;
	di->stopbits = 1;
	di->parity = UART_PARITY_NONE;
	return (0);
}
