/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Semihalf.
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_gate.h>
#include <dev/extres/clk/clk_mux.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "clkdev_if.h"
#include "periph.h"

int
a37x0_periph_create_mux(struct clkdom *clkdom,
    struct clk_mux_def *mux, int id)
{
	int error;

	mux->clkdef.id = id;

	error = clknode_mux_register(clkdom, mux);
	if (error != 0) {
		printf("Failed to create %s: %d\n", mux->clkdef.name, error);
		return (error);
	}

	return (0);
}

int
a37x0_periph_create_div(struct clkdom *clkdom,
    struct clk_div_def *div, int id)
{
	int error;

	div->clkdef.id = id;

	error = clknode_div_register(clkdom, div);
	if (error != 0) {
		printf("Failed to register %s: %d\n", div->clkdef.name, error);
		return (error);
	}

	return (0);
}

int
a37x0_periph_create_gate(struct clkdom *clkdom,
    struct clk_gate_def *gate, int id)
{
	int error;

	gate->clkdef.id = id;

	error = clknode_gate_register(clkdom, gate);
	if (error != 0) {
		printf("Failed to create %s:%d\n", gate->clkdef.name, error);
		return (error);
	}

	return (0);
}

void
a37x0_periph_set_props(struct clknode_init_def *clkdef,
    const char **parent_names, unsigned int parent_cnt)
{

	clkdef->parent_names = parent_names;
	clkdef->parent_cnt = parent_cnt;
}

