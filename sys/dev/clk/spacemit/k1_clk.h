/*
 * Copyright (c) 2026 Bojan Novković <bnovkov@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */


/*
 * Common helper macros for the Spacemit K1 SoC clocks.
 */

#ifndef _K1_CLK_H_
#define	_K1_CLK_H_

#include <dev/clk/clk.h>

/*
 * Helper macro to count the number of parent clocks passed
 * to the K1_CLK_COMMON macro. The maximum number of parents
 * for the K1 clocks is 8.
 */
#define _K1_CLK_PARENT_CNT_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define K1_CLK_PARENT_CNT(...) _K1_CLK_PARENT_CNT_IMPL(_, ## __VA_ARGS__, 8, \
    7, 6, 5, 4, 3, 2, 1, 0)

#define K1_CLK_COMMON(_clkname, _id, ...)			\
	.clkdef = {						\
	.id = _id,						\
	.name = #_clkname,					\
	.parent_names = (const char*[]) { __VA_ARGS__ },	\
	.parent_cnt = K1_CLK_PARENT_CNT(__VA_ARGS__),		\
}

#define K1_CLK_GATE(_clkname, _id, _reg, _gate_bit, ...)	\
	{							\
	K1_CLK_COMMON(_clkname, _id, __VA_ARGS__),		\
	.reg = _reg,						\
	.gate_mask = 1 << _gate_bit,				\
}

#define K1_CLK_DIV(_clkname, _id, _reg, _div_bits, _div_shift,	\
   _fc_bit, ...)						\
	{							\
	K1_CLK_COMMON(_clkname, _id, __VA_ARGS__),		\
	.reg = _reg,						\
	.div_nbits = _div_bits,					\
	.div_shift = _div_shift,				\
	.fc_mask = 1 << _fc_bit,				\
}

#define K1_CLK_DIV_MUX(_clkname, _id, _reg, _div_bits,	\
    _div_shift, _mux_nbits, _mux_shift, _fc_bit, ...)	\
	{						\
	K1_CLK_COMMON(_clkname, _id, __VA_ARGS__),	\
	.reg = _reg,					\
	.div_nbits = _div_bits,				\
	.div_shift = _div_shift,			\
	.mux_nbits = _mux_nbits,			\
	.mux_shift = _mux_shift,			\
	.fc_mask =  1 <<_fc_bit,			\
}

#define K1_CLK_DIV_GATE(_clkname, _id, _reg, _div_bits, _div_shift,	\
    _gate_bit, ...)							\
	{								\
	K1_CLK_COMMON(_clkname, _id, __VA_ARGS__),			\
	.reg = _reg,							\
	.gate_mask = (1 << _gate_bit),					\
	.div_nbits = _div_bits,						\
	.div_shift = _div_shift,					\
}

#define K1_CLK_MUX_GATE(_clkname, _id, _reg, _mux_nbits, _mux_shift,	\
    _gate_bit, ...)							\
	{								\
	K1_CLK_COMMON(_clkname, _id, __VA_ARGS__),			\
	.reg = _reg,							\
	.gate_mask = (1 << _gate_bit),					\
	.mux_nbits = _mux_nbits,					\
	.mux_shift = _mux_shift,					\
}

#define K1_CLK_DIV_MUX_GATE(_clkname, _id, _reg, _div_bits,	\
    _div_shift, _mux_nbits, _mux_shift,				\
    _gate_bit, _fc_bit, ...)					\
	{							\
	K1_CLK_COMMON(_clkname, _id, __VA_ARGS__),		\
	.reg = _reg,						\
	.gate_mask = (1 << _gate_bit),				\
	.div_nbits = _div_bits,					\
	.div_shift = _div_shift,				\
	.mux_nbits = _mux_nbits,				\
	.mux_shift = _mux_shift,				\
	.fc_mask =  1 <<_fc_bit,				\
}

#define K1_RESET(_id, _reg, _assert_bit, _deassert_bit)	\
	[_id] = {					\
	.reg = _reg,					\
	.assert_mask = (1 << _assert_bit),		\
	.deassert_mask = (1 << _deassert_bit),		\
}

struct k1_clk_def {
	struct clknode_init_def	clkdef;

	uint64_t rate; /* Fixed clock frequency. */
	uint32_t fixed_div; /* Fixed clock divisor. */
	uint32_t reg; /* Control register offset.*/
	uint32_t div_nbits; /* Frequency divisor. */
	uint32_t div_shift;
	uint32_t gate_mask; /* Gate bit. */
	uint32_t mux_nbits; /* Multiplexer offset. */
	uint32_t mux_shift;
	uint32_t fc_mask; /* Frequency change request bit. */
};

struct k1_reset_def {
	uint32_t reg;
	uint32_t assert_mask;
	uint32_t deassert_mask;
};

struct k1_clkdev_softc {
	struct mtx mtx;
	struct clkdom *clkdom;
	struct resource *res;

	const struct k1_clk_def *clks;
	const struct k1_reset_def *resets;
	int nclks;
	int nresets;

	/*
	 * Allow derived clknode implementations to
	 * override the generic K1 clock routines.
	 */
	clknode_class_t clknode_class;
};

DECLARE_CLASS(k1_clkdev_driver);
DECLARE_CLASS(k1_clknode_class);

int k1_clk_attach(device_t dev);

#endif
