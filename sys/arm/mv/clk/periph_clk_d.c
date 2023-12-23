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
 * Register chain: mux (select proper TBG) -> div1 (first frequency divider) ->
 * div2 (second frequency divider) -> mux (select divided freq.
 * or xtal output) -> gate (enable or disable clock), which is also final node
 */

int
a37x0_periph_d_register_full_clk_dd(struct clkdom *clkdom,
    struct a37x0_periph_clknode_def *device_def)
{
	const char *parent_names[PARENT_CNT];
	struct clk_mux_def *clk_mux;
	struct clk_mux_def *tbg_mux;
	struct clk_gate_def *gate;
	struct clk_div_def *div1;
	struct clk_div_def *div2;
	int error, dev_id;

	dev_id = device_def->common_def.device_id;
	tbg_mux = &device_def->clk_def.full_dd.tbg_mux;
	div1 = &device_def->clk_def.full_dd.div1;
	div2 = &device_def->clk_def.full_dd.div2;
	gate = &device_def->clk_def.full_dd.gate;
	clk_mux = &device_def->clk_def.full_dd.clk_mux;

	a37x0_periph_set_props(&tbg_mux->clkdef, device_def->common_def.tbgs,
	    device_def->common_def.tbg_cnt);

	error = a37x0_periph_create_mux(clkdom,
	    tbg_mux, A37x0_INTERNAL_CLK_ID(dev_id, MUX_POS));
	if (error)
		goto fail;

	a37x0_periph_set_props(&div1->clkdef, &tbg_mux->clkdef.name, 1);
	error = a37x0_periph_create_div(clkdom, div1,
	    A37x0_INTERNAL_CLK_ID(dev_id, DIV1_POS));
	if (error)
		goto fail;

	a37x0_periph_set_props(&div2->clkdef, &div1->clkdef.name, 1);
	error = a37x0_periph_create_div(clkdom, div2,
	    A37x0_INTERNAL_CLK_ID(dev_id, DIV2_POS));
	if (error)
		goto fail;

	parent_names[0] = device_def->common_def.xtal;
	parent_names[1] = div2->clkdef.name;

	a37x0_periph_set_props(&clk_mux->clkdef, parent_names, PARENT_CNT);
	error = a37x0_periph_create_mux(clkdom, clk_mux,
	    A37x0_INTERNAL_CLK_ID(dev_id, CLK_MUX_POS));
	if (error)
		goto fail;

	a37x0_periph_set_props(&gate->clkdef, &clk_mux->clkdef.name, 1);
	error = a37x0_periph_create_gate(clkdom, gate,
	    dev_id);
	if (error)
		goto fail;

fail:

	return (error);
}

/*
 * Register chain: mux (select proper TBG) -> div1 (first frequency divider) ->
 * mux (select divided freq. or xtal output) -> gate (enable or disable clock),
 * which is also final node
 */

int
a37x0_periph_d_register_full_clk(struct clkdom *clkdom,
    struct a37x0_periph_clknode_def *device_def)
{
	const char *parent_names[PARENT_CNT];
	struct clk_mux_def *tbg_mux;
	struct clk_mux_def *clk_mux;
	struct clk_gate_def *gate;
	struct clk_div_def *div;
	int error, dev_id;

	dev_id = device_def->common_def.device_id;
	tbg_mux = &device_def->clk_def.full_d.tbg_mux;
	div = &device_def->clk_def.full_d.div;
	gate = &device_def->clk_def.full_d.gate;
	clk_mux = &device_def->clk_def.full_d. clk_mux;

	a37x0_periph_set_props(&tbg_mux->clkdef, device_def->common_def.tbgs,
	    device_def->common_def.tbg_cnt);
	error = a37x0_periph_create_mux(clkdom, tbg_mux,
	    A37x0_INTERNAL_CLK_ID(device_def->common_def.device_id, MUX_POS));
	if (error)
		goto fail;

	a37x0_periph_set_props(&div->clkdef, &tbg_mux->clkdef.name, 1);
	error = a37x0_periph_create_div(clkdom, div,
	    A37x0_INTERNAL_CLK_ID(device_def->common_def.device_id, DIV1_POS));
	if (error)
		goto fail;

	parent_names[0] = device_def->common_def.xtal;
	parent_names[1] = div->clkdef.name;

	a37x0_periph_set_props(&clk_mux->clkdef, parent_names, PARENT_CNT);
	error = a37x0_periph_create_mux(clkdom, clk_mux,
	    A37x0_INTERNAL_CLK_ID(dev_id, CLK_MUX_POS));
	if (error)
		goto fail;

	a37x0_periph_set_props(&gate->clkdef, &clk_mux->clkdef.name, 1);
	error = a37x0_periph_create_gate(clkdom, gate,
	    dev_id);
	if (error)
		goto fail;

fail:

	return (error);
}

/*
 * Register CPU clock. It consists of mux (select proper TBG) -> div (frequency
 * divider) -> mux (choose divided or xtal output).
 */

int
a37x0_periph_d_register_periph_cpu(struct clkdom *clkdom,
    struct a37x0_periph_clknode_def *device_def)
{
	const char *parent_names[PARENT_CNT];
	struct clk_mux_def *clk_mux;
	struct clk_mux_def *tbg_mux;
	struct clk_div_def *div;
	int error, dev_id;

	dev_id = device_def->common_def.device_id;
	tbg_mux = &device_def->clk_def.cpu.tbg_mux;
	div = &device_def->clk_def.cpu.div;
	clk_mux = &device_def->clk_def.cpu.clk_mux;

	a37x0_periph_set_props(&tbg_mux->clkdef, device_def->common_def.tbgs,
	    device_def->common_def.tbg_cnt);
	error = a37x0_periph_create_mux(clkdom, tbg_mux,
	    A37x0_INTERNAL_CLK_ID(dev_id, MUX_POS));
	if (error)
		goto fail;

	a37x0_periph_set_props(&div->clkdef, &tbg_mux->clkdef.name, 1);
	error = a37x0_periph_create_div(clkdom, div,
	    A37x0_INTERNAL_CLK_ID(dev_id, DIV1_POS));
	if (error)
		goto fail;

	parent_names[0] = device_def->common_def.xtal;
	parent_names[1] = div->clkdef.name;

	a37x0_periph_set_props(&clk_mux->clkdef, parent_names, PARENT_CNT);
	error = a37x0_periph_create_mux(clkdom, clk_mux,
	    dev_id);

fail:

	return (error);
}

/*
 * Register chain: mux (choose proper TBG) -> div1 (first frequency divider) ->
 * div2 (second frequency divider) -> mux (choose divided or xtal output).
 */
int
a37x0_periph_d_register_mdd(struct clkdom *clkdom,
    struct a37x0_periph_clknode_def *device_def)
{
	const char *parent_names[PARENT_CNT];
	struct clk_mux_def *tbg_mux;
	struct clk_mux_def *clk_mux;
	struct clk_div_def *div1;
	struct clk_div_def *div2;
	int error, dev_id;

	dev_id = device_def->common_def.device_id;
	tbg_mux = &device_def->clk_def.mdd.tbg_mux;
	div1 = &device_def->clk_def.mdd.div1;
	div2 = &device_def->clk_def.mdd.div2;
	clk_mux = &device_def->clk_def.mdd.clk_mux;

	a37x0_periph_set_props(&tbg_mux->clkdef, device_def->common_def.tbgs,
	    device_def->common_def.tbg_cnt);
	error = a37x0_periph_create_mux(clkdom, tbg_mux,
	    A37x0_INTERNAL_CLK_ID(dev_id, MUX_POS));
	if (error)
		goto fail;

	a37x0_periph_set_props(&div1->clkdef, &tbg_mux->clkdef.name, 1);
	error = a37x0_periph_create_div(clkdom, div1,
	    A37x0_INTERNAL_CLK_ID(dev_id, DIV1_POS));
	if (error)
		goto fail;

	a37x0_periph_set_props(&div2->clkdef, &div1->clkdef.name, 1);
	error = a37x0_periph_create_div(clkdom, div2,
	    A37x0_INTERNAL_CLK_ID(dev_id, DIV2_POS));

	if (error)
		goto fail;

	parent_names[0] = device_def->common_def.xtal;
	parent_names[1] = div2->clkdef.name;

	a37x0_periph_set_props(&clk_mux->clkdef, parent_names, PARENT_CNT);
	error = a37x0_periph_create_mux(clkdom, clk_mux,
	    dev_id);
	if (error)
		goto fail;

fail:

	return (error);
}
