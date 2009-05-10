/*-
 * Copyright (c) 2008 TAKAHASHI Yoshihiro
 * Copyright (c) 2003 M. Warner Losh, Marcel Moolenaar
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>

bus_space_tag_t uart_bus_space_io = I386_BUS_SPACE_IO;
bus_space_tag_t uart_bus_space_mem = I386_BUS_SPACE_MEM;

static struct {
	u_long iobase;
	struct uart_class *class;
} uart_pc98_devs[] = {
	{ 0x238, &uart_ns8250_class },
	{ 0, NULL }
};

struct uart_class *
uart_pc98_getdev(u_long port)
{
	int i;

	for (i = 0; uart_pc98_devs[i].iobase; i++) {
		if (port == uart_pc98_devs[i].iobase)
			return (uart_pc98_devs[i].class);
	}
	return (NULL);
}

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{

	if (bus_space_compare(b1->bst, b1->bsh, b2->bst, b2->bsh) == 0)
		return (1);

	return (0);
}

int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	struct uart_class *class;
	unsigned int i, ivar;

	class = &uart_ns8250_class;
	if (class == NULL)
		return (ENXIO);

	/* Check the environment. */
	if (uart_getenv(devtype, di, class) == 0)
		return (0);

	/*
	 * There is a serial port on all pc98 hardware.  It is 8251 or
	 * an enhance version of that.  Some pc98 have the second serial
	 * port which is 16550A compatible.
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
		if (resource_int_value("uart", i, "port", &ivar) != 0 ||
		    ivar == 0)
			continue;

		class = uart_pc98_getdev(ivar);
		if (class == NULL)
			continue;

		di->ops = uart_getops(class);
		di->bas.chan = 0;
		di->bas.bst = uart_bus_space_io;
		if (bus_space_map(di->bas.bst, ivar, uart_getrange(class), 0,
		    &di->bas.bsh) != 0)
			continue;
		di->bas.regshft = 0;
		di->bas.rclk = 0;
		if (resource_int_value("uart", i, "baud", &ivar) != 0)
			ivar = 0;
		di->baudrate = ivar;
		di->databits = 8;
		di->stopbits = 1;
		di->parity = UART_PARITY_NONE;
		return (0);
	}

	return (ENXIO);
}
