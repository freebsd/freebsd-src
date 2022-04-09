/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
__FBSDID("$FreeBSD$");

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

#define PARENT_CNT	2

/*
 * Register clock with fixed frequency divider clock. Chain consists of:
 * fixed clock (output from xtal/2) -> mux (choose fixed or xtal frequency)
 */

int
a37x0_periph_fixed_register_fixed(struct clkdom *clkdom,
    struct a37x0_periph_clknode_def *device_def)
{
	const char *parent_names[PARENT_CNT];
	struct clk_fixed_def fixed_def;
	struct clk_gate_def *gate;
	struct clk_mux_def *mux;
	int error, dev_id;

	dev_id = device_def->common_def.device_id;
	mux = &device_def->clk_def.fixed.mux;
	gate = &device_def->clk_def.fixed.gate;
	fixed_def = device_def->clk_def.fixed.fixed;

	fixed_def.clkdef.parent_names = &device_def->common_def.xtal;
	fixed_def.clkdef.parent_cnt = 1;
	fixed_def.clkdef.id = A37x0_INTERNAL_CLK_ID(dev_id, FIXED1_POS);
	fixed_def.clkdef.flags = 0;
	fixed_def.mult = 1;
	fixed_def.div = 2;
	fixed_def.freq = 0;

	parent_names[0] = device_def->common_def.xtal;
	parent_names[1] = fixed_def.clkdef.name;

	error = clknode_fixed_register(clkdom, &fixed_def);
	if (error)
		goto fail;

	a37x0_periph_set_props(&mux->clkdef, parent_names ,PARENT_CNT);
	error = a37x0_periph_create_mux(clkdom, mux,
	    A37x0_INTERNAL_CLK_ID(dev_id, MUX_POS));
	if (error)
		goto fail;

	a37x0_periph_set_props(&gate->clkdef, &mux->clkdef.name, 1);
	error = a37x0_periph_create_gate(clkdom, gate,
	    dev_id);
	if (error)
		goto fail;

fail:

	return (error);
}
