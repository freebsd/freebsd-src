/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

/*
 * UART console routines.
 */
bus_space_tag_t uart_bus_space_io;
bus_space_tag_t uart_bus_space_mem;

static int
uart_fdt_get_clock(phandle_t node, pcell_t *cell)
{
	pcell_t clock;

	if ((OF_getprop(node, "clock-frequency", &clock,
	    sizeof(clock))) <= 0)
		return (ENXIO);

	if (clock == 0)
		/* Try to retrieve parent 'bus-frequency' */
		/* XXX this should go to simple-bus fixup or so */
		if ((OF_getprop(OF_parent(node), "bus-frequency", &clock,
		    sizeof(clock))) <= 0)
			clock = 0;

	*cell = fdt32_to_cpu(clock);
	return (0);
}

static int
uart_fdt_get_shift(phandle_t node, pcell_t *cell)
{
	pcell_t shift;

	if ((OF_getprop(node, "reg-shift", &shift, sizeof(shift))) <= 0)
		shift = 0;
	*cell = fdt32_to_cpu(shift);
	return (0);
}

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{

	return ((b1->bsh == b2->bsh && b1->bst == b2->bst) ? 1 : 0);
}

int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	char buf[64];
	struct uart_class *class;
	phandle_t node, chosen;
	pcell_t shift, br, rclk;
	u_long start, size, pbase, psize;
	int err;

	uart_bus_space_mem = fdtbus_bs_tag;
	uart_bus_space_io = NULL;

	/* Allow overriding the FDT uning the environment. */
	class = &uart_ns8250_class;
	err = uart_getenv(devtype, di, class);
	if (!err)
		return (0);

	if (devtype != UART_DEV_CONSOLE)
		return (ENXIO);

	/*
	 * Retrieve /chosen/std{in,out}.
	 */
	if ((chosen = OF_finddevice("/chosen")) == -1)
		return (ENXIO);
	if (OF_getprop(chosen, "stdin", buf, sizeof(buf)) <= 0)
		return (ENXIO);
	if ((node = OF_finddevice(buf)) == -1)
		return (ENXIO);
	if (OF_getprop(chosen, "stdout", buf, sizeof(buf)) <= 0)
		return (ENXIO);
	if (OF_finddevice(buf) != node)
		/* Only stdin == stdout is supported. */
		return (ENXIO);
	/*
	 * Retrieve serial attributes.
	 */
	uart_fdt_get_shift(node, &shift);

	if (OF_getprop(node, "current-speed", &br, sizeof(br)) <= 0)
		br = 0;
	br = fdt32_to_cpu(br);

	if ((err = uart_fdt_get_clock(node, &rclk)) != 0)
		return (err);
	/*
	 * Finalize configuration.
	 */
	if (fdt_is_compatible(node, "fsl,imx-uart"))
		class = &uart_imx_class;
	if (fdt_is_compatible(node, "quicc"))
		class = &uart_quicc_class;
	if (fdt_is_compatible(node, "lpc"))
		class = &uart_lpc_class;
	if (fdt_is_compatible(node, "ns16550"))
		class = &uart_ns8250_class;
	if (fdt_is_compatible(node, "arm,pl011"))
		class = &uart_pl011_class;
	if (fdt_is_compatible(node, "exynos"))
		class = &uart_s3c2410_class;
	if (fdt_is_compatible(node, "cadence,uart"))
		class = &uart_cdnc_class;

	di->bas.chan = 0;
	di->bas.regshft = (u_int)shift;
	di->baudrate = br;
	di->bas.rclk = (u_int)rclk;
	di->ops = uart_getops(class);
	di->databits = 8;
	di->stopbits = 1;
	di->parity = UART_PARITY_NONE;
	di->bas.bst = uart_bus_space_mem;

	err = fdt_regsize(node, &start, &size);
	if (err)
		return (ENXIO);
	err = fdt_get_range(OF_parent(node), 0, &pbase, &psize);
	if (err)
		pbase = 0;

	start += pbase;

	return (bus_space_map(di->bas.bst, start, size, 0, &di->bas.bsh));
}
