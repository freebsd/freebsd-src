#ifndef __MDTK_CLK_H__
#define __MDTK_CLK_H__

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Martin Filla, Michal Meloun <mmel@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Parent list */
#define PLIST(_name) static const char *_name[]

#define PLL(_id, cname, pname, _base_reg, _pwr_reg, _en_mask, _flags, _pcwbits, \
                   _pd_reg, _pd_shift, _tuner_reg, _pcw_reg, \
                   _pcw_shift)                          \
{                                                                   \
    .clkdef.id = _id,                                                \
    .clkdef.name = cname,                                            \
    .clkdef.parent_names = (const char *[]){pname},                    \
    .clkdef.parent_cnt = 1,                                            \
    .clkdef.flags = CLK_NODE_STATIC_STRINGS,                        \
    .pll_base_reg = (_base_reg),                                        \
    .pll_pwr_reg = (_pwr_reg),                                          \
    .pll_en_mask = (_en_mask),                                          \
    .pll_pd_reg = (_pd_reg),                                            \
    .pll_tuner_reg = (_tuner_reg),                                      \
    .pll_pd_shift = (_pd_shift),                                        \
    .pll_flags = (_flags),                                              \
    .pll_pcwbits = (_pcwbits),                                          \
    .pll_pcw_reg = (_pcw_reg),                                          \
    .pll_pcw_shift = (_pcw_shift),                                      \
}

/* Standard gate. */
#define    GATE(_id, cname, plist, o, s)                    \
{                                    \
    .clkdef.id = _id,                        \
    .clkdef.name = cname,                        \
    .clkdef.parent_names = (const char *[]){plist},            \
    .clkdef.parent_cnt = 1,                        \
    .clkdef.flags = CLK_NODE_STATIC_STRINGS,            \
    .offset = o,                            \
    .shift = s,                            \
    .mask = 1,                            \
    .on_value = 1,                            \
    .off_value = 0,                            \
}

/* Fixed rate clock. */
#define    FRATE(_id, _name, _freq)                    \
{                                    \
    .clkdef.id = _id,                        \
    .clkdef.name = _name,                        \
    .clkdef.parent_cnt = 0,                        \
    .clkdef.flags = CLK_NODE_STATIC_STRINGS,            \
    .freq = _freq,                            \
}

/* Link */
#define    LINK(_idx, _clkname, _pname)                \
{                                    \
    .clkdef.id = _idx,                            \
    .clkdef.name = _clkname,                        \
    .clkdef.parent_name = _pname,                        \
    .clkdef.flags = CLK_NODE_STATIC_STRINGS \
}

/* Fixed factor clock */
#define    FFACT(_id, _name, _pname, _mult, _div)                \
{                                    \
    .clkdef.id = _id,                    \
    .clkdef.name = _name,                    \
    .clkdef.parent_names = (const char *[]){_pname},    \
    .clkdef.parent_cnt = 1,                    \
    .clkdef.flags = CLK_NODE_STATIC_STRINGS,        \
    .mult = _mult,                        \
    .div = _div,                        \
}

/* Divided clock */
#define DIV(_id, _name, _pname, _reg, _shift, _width) {    \
    .clkdef.id = _id,                    \
    .clkdef.name = _name,                    \
    .clkdef.parent_names = (const char *[]){_pname},                \
    .clkdef.parent_cnt = 1, \
    .offset = _reg,                \
    .i_shift = _shift, \
    .i_width = _width, \
}

/* Pure multiplexer. */
#define    MUX0(_id, cname, plists, _reg, _shift, _width)                \
{                                    \
    .clkdef.id = _id,                        \
    .clkdef.name = cname,                        \
    .clkdef.parent_names = plists,                    \
    .clkdef.parent_cnt = nitems(plists),                \
    .clkdef.flags = CLK_NODE_STATIC_STRINGS,            \
    .offset = _reg,                            \
    .shift  = _shift,                            \
    .width = _width,                            \
}

struct mdtk_clk_def {
    struct clk_pll_def *pll_def;
    struct clk_link_def *linked_def;
    struct clk_fixed_def *fixed_def;
    struct clk_mux_def *muxes_def;
    struct clk_gate_def *gates_def;
    struct clk_div_def *dived_def;
    int num_pll;
    int num_linked;
    int num_fixed;
    int num_muxes;
    int num_gates;
    int num_dived;
};

struct mdtk_clk_softc {
    device_t dev;
    struct resource *mem_res;
    struct mtx mtx;
    struct clkdom *clkdom;
    struct syscon *syscon;
};

int mdtk_clkdev_read_4(device_t dev, bus_addr_t addr, uint32_t *val);
int mdtk_clkdev_write_4(device_t dev, bus_addr_t addr, uint32_t val);
int mdtk_clkdev_modify_4(device_t dev, bus_addr_t addr, uint32_t clear_mask,
                         uint32_t set_mask);

void mdtk_clkdev_device_lock(device_t dev);
void mdtk_clkdev_device_unlock(device_t dev);
void mdtk_register_clocks(device_t dev, struct mdtk_clk_def *cldef);
int mdtk_hwreset_by_idx(struct mdtk_clk_softc *sc, intptr_t idx, bool reset);
#endif
