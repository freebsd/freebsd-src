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
#ifndef __aarch64__
#include <machine/fdt.h>
#endif

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_cpu_fdt.h>

#ifdef __aarch64__
extern bus_space_tag_t fdtbus_bs_tag;
#endif

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
	const char *propnames[] = {"stdout-path", "linux,stdout-path", "stdout",
	    "stdin-path", "stdin", NULL};
	const char **name;
	struct uart_class *class;
	phandle_t node, chosen;
	pcell_t shift, br, rclk;
	u_long start, size, pbase, psize;
	char *cp;
	int err;

	uart_bus_space_mem = fdtbus_bs_tag;
	uart_bus_space_io = NULL;

	/* Allow overriding the FDT using the environment. */
	class = &uart_ns8250_class;
	err = uart_getenv(devtype, di, class);
	if (!err)
		return (0);

	if (devtype != UART_DEV_CONSOLE)
		return (ENXIO);

	/* Has the user forced a specific device node? */
	cp = kern_getenv("hw.fdt.console");
	if (cp == NULL) {
		/*
		 * Retrieve /chosen/std{in,out}.
		 */
		node = -1;
		if ((chosen = OF_finddevice("/chosen")) != -1) {
			for (name = propnames; *name != NULL; name++) {
				if (phandle_chosen_propdev(chosen, *name,
				    &node) == 0)
					break;
			}
		}
		if (chosen == -1 || *name == NULL)
			node = OF_finddevice("serial0"); /* Last ditch */
	} else {
		node = OF_finddevice(cp);
	}

	if (node == -1) /* Can't find anything */
		return (ENXIO);

	/*
	 * Check old style of UART definition first. Unfortunately, the common
	 * FDT processing is not possible if we have clock, power domains and
	 * pinmux stuff.
	 */
	class = (struct uart_class *)uart_fdt_find_by_node(node, 0);
	if (class != NULL) {
		if ((err = uart_fdt_get_clock(node, &rclk)) != 0)
			return (err);
	} else {
		/* Check class only linker set */
		class =
		    (struct uart_class *)uart_fdt_find_by_node(node, 1);
		if (class == NULL)
			return (ENXIO);
		rclk = 0;
	}

	/*
	 * Retrieve serial attributes.
	 */
	if (uart_fdt_get_shift(node, &shift) != 0)
		shift = uart_getregshift(class);

	if (OF_getprop(node, "current-speed", &br, sizeof(br)) <= 0)
		br = 0;
	else
		br = fdt32_to_cpu(br);

	/*
	 * Finalize configuration.
	 */
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
