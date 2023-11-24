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
 *
 */

#ifndef	_PERIPH_H_
#define	_PERIPH_H_

#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_mux.h>
#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_gate.h>

#define TBG_SEL                 0x0
#define DIV_SEL0                0x4
#define DIV_SEL1                0x8
#define DIV_SEL2                0xC
#define CLK_SEL                 0x10
#define CLK_DIS                 0x14
#define DIV_MASK		0x7

#define MUX_POS			1
#define DIV1_POS		2
#define DIV2_POS		3
#define GATE_POS		4
#define FIXED1_POS		5
#define FIXED2_POS		6
#define CLK_MUX_POS		7

#define RD4(_clk, offset, val)			\
    CLKDEV_READ_4(clknode_get_device(_clk), offset, val)

#define A37x0_INTERNAL_CLK_ID(_base, _pos)	\
    ((_base * 10) + (_pos))

#define CLK_FULL_DD(_name, _id, _gate_shift, _tbg_mux_shift,		\
    _clk_mux_shift, _div1_reg, _div2_reg, _div1_shift, _div2_shift,	\
    _tbg_mux_name, _div1_name, _div2_name, _clk_mux_name)		\
{									\
	.type = CLK_FULL_DD,						\
	.common_def.device_name = _name,				\
	.common_def.device_id = _id,					\
	.clk_def.full_dd.tbg_mux.clkdef.name = _tbg_mux_name,		\
	.clk_def.full_dd.tbg_mux.offset = TBG_SEL,			\
	.clk_def.full_dd.tbg_mux.shift = _tbg_mux_shift,		\
	.clk_def.full_dd.tbg_mux.width = 0x2,				\
	.clk_def.full_dd.tbg_mux.mux_flags = 0x0,			\
	.clk_def.full_dd.div1.clkdef.name = _div1_name,			\
	.clk_def.full_dd.div1.offset = _div1_reg,			\
	.clk_def.full_dd.div1.i_shift = _div1_shift,			\
	.clk_def.full_dd.div1.i_width = 0x3,				\
	.clk_def.full_dd.div1.f_shift = 0x0,				\
	.clk_def.full_dd.div1.f_width = 0x0,				\
	.clk_def.full_dd.div1.div_flags = 0x0,				\
	.clk_def.full_dd.div1.div_table = NULL,				\
	.clk_def.full_dd.div2.clkdef.name = _div2_name,			\
	.clk_def.full_dd.div2.offset = _div2_reg,			\
	.clk_def.full_dd.div2.i_shift = _div2_shift,			\
	.clk_def.full_dd.div2.i_width = 0x3,				\
	.clk_def.full_dd.div2.f_shift = 0x0,				\
	.clk_def.full_dd.div2.f_width = 0x0,				\
	.clk_def.full_dd.div2.div_flags = 0x0,				\
	.clk_def.full_dd.div2.div_table = NULL,				\
	.clk_def.full_dd.clk_mux.clkdef.name = _clk_mux_name,		\
	.clk_def.full_dd.clk_mux.offset = CLK_SEL,			\
	.clk_def.full_dd.clk_mux.shift = _clk_mux_shift,		\
	.clk_def.full_dd.clk_mux.width = 0x1,				\
	.clk_def.full_dd.clk_mux.mux_flags = 0x0,			\
	.clk_def.full_dd.gate.clkdef.name = _name,			\
	.clk_def.full_dd.gate.offset = CLK_DIS,				\
	.clk_def.full_dd.gate.shift = _gate_shift,			\
	.clk_def.full_dd.gate.on_value = 0,				\
	.clk_def.full_dd.gate.off_value = 1,				\
	.clk_def.full_dd.gate.mask = 0x1,				\
	.clk_def.full_dd.gate.gate_flags = 0x0				\
}

#define CLK_FULL(_name, _id, _gate_shift, _tbg_mux_shift,		\
    _clk_mux_shift, _div1_reg, _div1_shift, _div_table, _tbg_mux_name,	\
    _div1_name, _clk_mux_name)						\
{									\
	.type = CLK_FULL,						\
	.common_def.device_name = _name,				\
	.common_def.device_id = _id,					\
	.clk_def.full_d.tbg_mux.clkdef.name = _tbg_mux_name,		\
	.clk_def.full_d.tbg_mux.offset = TBG_SEL,			\
	.clk_def.full_d.tbg_mux.shift = _tbg_mux_shift,			\
	.clk_def.full_d.tbg_mux.width = 0x2,				\
	.clk_def.full_d.tbg_mux.mux_flags = 0x0,			\
	.clk_def.full_d.div.clkdef.name = _div1_name,			\
	.clk_def.full_d.div.offset = _div1_reg,				\
	.clk_def.full_d.div.i_shift = _div1_shift,			\
	.clk_def.full_d.div.i_width = 0x3,				\
	.clk_def.full_d.div.f_shift = 0x0,				\
	.clk_def.full_d.div.f_width = 0x0,				\
	.clk_def.full_d.div.div_flags = 0x0,				\
	.clk_def.full_d.div.div_table = _div_table,			\
	.clk_def.full_d.clk_mux.clkdef.name = _clk_mux_name,		\
	.clk_def.full_d.clk_mux.offset = CLK_SEL,			\
	.clk_def.full_d.clk_mux.shift = _clk_mux_shift,			\
	.clk_def.full_d.clk_mux.width = 0x1,				\
	.clk_def.full_d.clk_mux.mux_flags = 0x0,			\
	.clk_def.full_d.gate.clkdef.name = _name,			\
	.clk_def.full_d.gate.offset = CLK_DIS,				\
	.clk_def.full_d.gate.shift = _gate_shift,			\
	.clk_def.full_d.gate.on_value = 0,				\
	.clk_def.full_d.gate.off_value = 1,				\
	.clk_def.full_d.gate.mask = 0x1,				\
	.clk_def.full_d.gate.gate_flags = 0x0				\
}

#define CLK_CPU(_name, _id, _tbg_mux_shift, _clk_mux_shift, _div1_reg,	\
    _div1_shift, _div_table, _tbg_mux_name, _div1_name)			\
{									\
	.type = CLK_CPU,						\
	.common_def.device_name = _name,				\
	.common_def.device_id = _id,					\
	.clk_def.cpu.tbg_mux.clkdef.name = _tbg_mux_name,		\
	.clk_def.cpu.tbg_mux.offset = TBG_SEL,				\
	.clk_def.cpu.tbg_mux.shift = _tbg_mux_shift,			\
	.clk_def.cpu.tbg_mux.width = 0x2,				\
	.clk_def.cpu.tbg_mux.mux_flags = 0x0,				\
	.clk_def.cpu.div.clkdef.name = _div1_name,			\
	.clk_def.cpu.div.offset = _div1_reg,				\
	.clk_def.cpu.div.i_shift = _div1_shift,				\
	.clk_def.cpu.div.i_width = 0x3,					\
	.clk_def.cpu.div.f_shift = 0x0,					\
	.clk_def.cpu.div.f_width = 0x0,					\
	.clk_def.cpu.div.div_flags = 0x0,				\
	.clk_def.cpu.div.div_table = _div_table,			\
	.clk_def.cpu.clk_mux.clkdef.name = _name,			\
	.clk_def.cpu.clk_mux.offset = CLK_SEL,				\
	.clk_def.cpu.clk_mux.shift = _clk_mux_shift,			\
	.clk_def.cpu.clk_mux.width = 0x1,				\
	.clk_def.cpu.clk_mux.mux_flags = 0x0,				\
}

#define CLK_GATE(_name, _id, _gate_shift, _pname)			\
{									\
	.type = CLK_GATE,						\
	.common_def.device_name = _name,				\
	.common_def.device_id = _id,					\
	.common_def.pname = _pname,					\
	.clk_def.gate.gate.clkdef.name = _name,				\
	.clk_def.gate.gate.clkdef.parent_cnt = 1,			\
	.clk_def.gate.gate.offset = CLK_DIS,				\
	.clk_def.gate.gate.shift = _gate_shift,				\
	.clk_def.gate.gate.on_value = 0,				\
	.clk_def.gate.gate.off_value = 1,				\
	.clk_def.gate.gate.mask = 0x1,					\
	.clk_def.gate.gate.gate_flags = 0x0				\
}

#define CLK_MDD(_name, _id, _tbg_mux_shift, _clk_mux_shift, _div1_reg,	\
    _div2_reg, _div1_shift, _div2_shift, _tbg_mux_name, _div1_name,	\
    _div2_name)								\
{									\
	.type = CLK_MDD,						\
	.common_def.device_name = _name,				\
	.common_def.device_id = _id,					\
	.clk_def.mdd.tbg_mux.clkdef.name = _tbg_mux_name,		\
	.clk_def.mdd.tbg_mux.offset = TBG_SEL,				\
	.clk_def.mdd.tbg_mux.shift = _tbg_mux_shift,			\
	.clk_def.mdd.tbg_mux.width = 0x2,				\
	.clk_def.mdd.tbg_mux.mux_flags = 0x0,				\
	.clk_def.mdd.div1.clkdef.name = _div1_name,			\
	.clk_def.mdd.div1.offset = _div1_reg,				\
	.clk_def.mdd.div1.i_shift = _div1_shift,			\
	.clk_def.mdd.div1.i_width = 0x3,				\
	.clk_def.mdd.div1.f_shift = 0x0,				\
	.clk_def.mdd.div1.f_width = 0x0,				\
	.clk_def.mdd.div1.div_flags = 0x0,				\
	.clk_def.mdd.div1.div_table = NULL,				\
	.clk_def.mdd.div2.clkdef.name = _div2_name,			\
	.clk_def.mdd.div2.offset = _div2_reg,				\
	.clk_def.mdd.div2.i_shift = _div2_shift,			\
	.clk_def.mdd.div2.i_width = 0x3,				\
	.clk_def.mdd.div2.f_shift = 0x0,				\
	.clk_def.mdd.div2.f_width = 0x0,				\
	.clk_def.mdd.div2.div_flags = 0x0,				\
	.clk_def.mdd.div2.div_table = NULL,				\
	.clk_def.mdd.clk_mux.clkdef.name = _name,			\
	.clk_def.mdd.clk_mux.offset = CLK_SEL,				\
	.clk_def.mdd.clk_mux.shift = _clk_mux_shift,			\
	.clk_def.mdd.clk_mux.width = 0x1,				\
	.clk_def.mdd.clk_mux.mux_flags = 0x0				\
}

#define CLK_MUX_GATE(_name, _id, _gate_shift, _mux_shift, _pname,	\
     _mux_name, _fixed_name)						\
{									\
	.type = CLK_MUX_GATE,						\
	.common_def.device_name = _name,				\
	.common_def.device_id = _id,					\
	.common_def.pname = _pname,					\
	.clk_def.mux_gate.mux.clkdef.name = _mux_name,			\
	.clk_def.mux_gate.mux.offset = TBG_SEL,				\
	.clk_def.mux_gate.mux.shift = _mux_shift,			\
	.clk_def.mux_gate.mux.width = 0x1,				\
	.clk_def.mux_gate.mux.mux_flags = 0x0,				\
	.clk_def.mux_gate.gate.clkdef.name = _name,			\
	.clk_def.mux_gate.gate.offset = CLK_DIS,			\
	.clk_def.mux_gate.gate.shift = _gate_shift,			\
	.clk_def.mux_gate.gate.on_value = 0,				\
	.clk_def.mux_gate.gate.off_value = 1,				\
	.clk_def.mux_gate.gate.mask = 0x1,				\
	.clk_def.mux_gate.gate.gate_flags = 0x0,			\
	.clk_def.mux_gate.fixed.clkdef.name = _fixed_name		\
}

#define CLK_MUX_GATE_FIXED(_name, _id, _gate_shift, _mux_shift, 	\
     _mux_name, _gate_name, _fixed1_name)				\
{									\
	.type = CLK_MUX_GATE_FIXED,					\
	.common_def.device_name = _name,				\
	.common_def.device_id = _id,					\
	.clk_def.mux_gate_fixed.mux.clkdef.name = _mux_name,		\
	.clk_def.mux_gate_fixed.mux.offset = TBG_SEL,			\
	.clk_def.mux_gate_fixed.mux.shift = _mux_shift,			\
	.clk_def.mux_gate_fixed.mux.width = 0x1,			\
	.clk_def.mux_gate_fixed.mux.mux_flags = 0x0,			\
	.clk_def.mux_gate_fixed.gate.clkdef.name = _gate_name,		\
	.clk_def.mux_gate_fixed.gate.offset = CLK_DIS,			\
	.clk_def.mux_gate_fixed.gate.shift = _gate_shift,		\
	.clk_def.mux_gate_fixed.gate.on_value = 0,			\
	.clk_def.mux_gate_fixed.gate.off_value = 1,			\
	.clk_def.mux_gate_fixed.gate.mask = 0x1,			\
	.clk_def.mux_gate_fixed.gate.gate_flags = 0x0,			\
	.clk_def.mux_gate_fixed.fixed1.clkdef.name = _fixed1_name,	\
	.clk_def.mux_gate_fixed.fixed2.clkdef.name = _name		\
}

#define CLK_FIXED(_name, _id, _gate_shift, _mux_shift, _mux_name,	\
    _fixed_name)							\
{									\
	.type = CLK_FIXED,						\
	.common_def.device_name = _name,				\
	.common_def.device_id = _id,					\
	.clk_def.fixed.mux.clkdef.name = _mux_name,			\
	.clk_def.fixed.mux.offset = TBG_SEL,				\
	.clk_def.fixed.mux.shift = _mux_shift,				\
	.clk_def.fixed.mux.width = 0x1,					\
	.clk_def.fixed.mux.mux_flags = 0x0,				\
	.clk_def.fixed.gate.clkdef.name = _name,			\
	.clk_def.fixed.gate.offset = CLK_DIS,				\
	.clk_def.fixed.gate.shift = _gate_shift,			\
	.clk_def.fixed.gate.on_value = 0,				\
	.clk_def.fixed.gate.off_value = 1,				\
	.clk_def.fixed.gate.mask = 0x1,					\
	.clk_def.fixed.gate.gate_flags = 0x0,				\
	.clk_def.fixed.fixed.clkdef.name = _fixed_name			\
}

struct a37x0_periph_clk_softc {
	device_t			dev;
	struct resource			*res;
	struct clkdom			*clkdom;
	struct mtx			mtx;
	struct a37x0_periph_clknode_def *devices;
	int 				device_count;
};

struct a37x0_periph_clk_dd_def {
	struct clk_mux_def 	tbg_mux;
	struct clk_div_def	div1;
	struct clk_div_def	div2;
	struct clk_mux_def	clk_mux;
	struct clk_gate_def	gate;
};

struct a37x0_periph_clk_cpu_def {
	struct clk_mux_def 	tbg_mux;
	struct clk_div_def	div;
	struct clk_mux_def	clk_mux;
};

struct a37x0_periph_clk_d_def {
	struct clk_mux_def 	tbg_mux;
	struct clk_div_def	div;
	struct clk_mux_def	clk_mux;
	struct clk_gate_def	gate;
};

struct a37x0_periph_clk_fixed_def {
	struct clk_mux_def 	mux;
	struct clk_fixed_def	fixed;
	struct clk_gate_def	gate;
};

struct a37x0_periph_clk_gate_def {
	struct clk_gate_def	gate;
};

struct a37x0_periph_clk_mux_dd_def {
	struct clk_mux_def 	tbg_mux;
	struct clk_div_def	div1;
	struct clk_div_def	div2;
	struct clk_mux_def	clk_mux;
};

struct a37x0_periph_clk_mux_div_def {
	struct clk_mux_def 	mux;
	struct clk_div_def	div;
};

struct a37x0_periph_clk_mux_gate_def {
	struct clk_mux_def 	mux;
	struct clk_fixed_def	fixed;
	struct clk_gate_def	gate;
};

struct a37x0_periph_clk_mux_gate_fixed_def {
	struct clk_fixed_def	fixed1;
	struct clk_mux_def 	mux;
	struct clk_gate_def	gate;
	struct clk_fixed_def	fixed2;
};

enum a37x0_periph_clk_type {
	/* Double divider clock */
        CLK_FULL_DD,
	/* Single divider clock */
        CLK_FULL,
	/* Gate clock */
        CLK_GATE,
	/* Mux, gate clock */
        CLK_MUX_GATE,
	/* CPU clock */
        CLK_CPU,
	/* Clock with fixed frequency divider */
	CLK_FIXED,
	/* Clock with double divider, without gate */
	CLK_MDD,
	/* Clock with two fixed frequency dividers */
	CLK_MUX_GATE_FIXED
};

struct a37x0_periph_common_defs {
	char		*device_name;
	int		device_id;
	int		tbg_cnt;
	const char	*pname;
	const char 	**tbgs;
	const char	*xtal;
};

union a37x0_periph_clocks_defs {
	struct a37x0_periph_clk_dd_def full_dd;
	struct a37x0_periph_clk_d_def full_d;
	struct a37x0_periph_clk_gate_def gate;
	struct a37x0_periph_clk_mux_gate_def mux_gate;
	struct a37x0_periph_clk_cpu_def cpu;
	struct a37x0_periph_clk_fixed_def fixed;
	struct a37x0_periph_clk_mux_dd_def mdd;
	struct a37x0_periph_clk_mux_gate_fixed_def mux_gate_fixed;
};

struct a37x0_periph_clknode_def {
	enum a37x0_periph_clk_type		type;
	struct a37x0_periph_common_defs		common_def;
	union a37x0_periph_clocks_defs		clk_def;
};

int a37x0_periph_create_mux(struct clkdom *,
    struct clk_mux_def *, int);
int a37x0_periph_create_div(struct clkdom *,
    struct clk_div_def *, int);
int a37x0_periph_create_gate(struct clkdom *,
    struct clk_gate_def *, int);
void a37x0_periph_set_props(struct clknode_init_def *, const char **,
    unsigned int);
int a37x0_periph_d_register_full_clk_dd(struct clkdom *,
    struct a37x0_periph_clknode_def *);
int a37x0_periph_d_register_full_clk(struct clkdom *,
    struct a37x0_periph_clknode_def *);
int a37x0_periph_d_register_periph_cpu(struct clkdom *,
    struct a37x0_periph_clknode_def *);
int a37x0_periph_fixed_register_fixed(struct clkdom*,
    struct a37x0_periph_clknode_def *);
int a37x0_periph_gate_register_gate(struct clkdom *,
    struct a37x0_periph_clknode_def *);
int a37x0_periph_d_register_mdd(struct clkdom *,
    struct a37x0_periph_clknode_def *);
int a37x0_periph_d_register_mux_div_clk(struct clkdom *,
    struct a37x0_periph_clknode_def *);
int a37x0_periph_register_mux_gate(struct clkdom *,
    struct a37x0_periph_clknode_def *);
int a37x0_periph_register_mux_gate_fixed(struct clkdom *,
    struct a37x0_periph_clknode_def *);

int a37x0_periph_clk_read_4(device_t, bus_addr_t, uint32_t *);
void a37x0_periph_clk_device_unlock(device_t);
void a37x0_periph_clk_device_lock(device_t);
int a37x0_periph_clk_attach(device_t);
int a37x0_periph_clk_detach(device_t);

#endif
