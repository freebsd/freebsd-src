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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_cpu_fdt.h>

/*
 * UART console routines.
 */
bus_space_tag_t uart_bus_space_io;
bus_space_tag_t uart_bus_space_mem;

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{

	if (b1->bst != b2->bst)
		return (0);
	if (pmap_kextract(b1->bsh) == 0)
		return (0);
	if (pmap_kextract(b2->bsh) == 0)
		return (0);
	return ((pmap_kextract(b1->bsh) == pmap_kextract(b2->bsh)) ? 1 : 0);
}

static int
phandle_chosen_propdev(phandle_t chosen, const char *name, phandle_t *node)
{
	char buf[64];

	if (OF_getprop(chosen, name, buf, sizeof(buf)) <= 0)
		return (ENXIO);
	if ((*node = OF_finddevice(buf)) == -1)
		return (ENXIO);
	
	return (0);
}

static const struct ofw_compat_data *
uart_fdt_find_compatible(phandle_t node, const struct ofw_compat_data *cd)
{
	const struct ofw_compat_data *ocd;

	for (ocd = cd; ocd->ocd_str != NULL; ocd++) {
		if (fdt_is_compatible(node, ocd->ocd_str))
			return (ocd);
	}
	return (NULL);
}

static uintptr_t
uart_fdt_find_by_node(phandle_t node, int class_list)
{
	struct ofw_compat_data **cd;
	const struct ofw_compat_data *ocd;

	if (class_list) {
		SET_FOREACH(cd, uart_fdt_class_set) {
			ocd = uart_fdt_find_compatible(node, *cd);
			if ((ocd != NULL) && (ocd->ocd_data != 0))
				return (ocd->ocd_data);
		}
	} else {
		SET_FOREACH(cd, uart_fdt_class_and_device_set) {
			ocd = uart_fdt_find_compatible(node, *cd);
			if ((ocd != NULL) && (ocd->ocd_data != 0))
				return (ocd->ocd_data);
		}
	}
	return (0);
}

int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	struct uart_class *class;
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int shift, rclk;
	int br, err;

	/* Allow overriding the FDT using the environment. */
	class = &uart_ns8250_class;
	err = uart_getenv(devtype, di, class);
	if (!err)
		return (0);

	if (devtype != UART_DEV_CONSOLE)
		return (ENXIO);

	err = uart_cpu_fdt_probe(&class, &bst, &bsh, &br, &rclk, &shift);
	if (err != 0)
		return (err);

	/*
	 * Finalize configuration.
	 */
	di->bas.chan = 0;
	di->bas.regshft = shift;
	di->baudrate = br;
	di->bas.rclk = rclk;
	di->ops = uart_getops(class);
	di->databits = 8;
	di->stopbits = 1;
	di->parity = UART_PARITY_NONE;
	di->bas.bst = bst;
	di->bas.bsh = bsh;

	uart_bus_space_mem = di->bas.bst;
	uart_bus_space_io = NULL;

	return (err);
}
