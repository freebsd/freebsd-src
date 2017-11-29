/*-
 * Copyright (c) 2017 Emmanuel Vadot <manu@freebsd.org>
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#ifndef __CCU_NG_H__
#define __CCU_NG_H__

enum aw_ccung_type {
	H3_CCU = 1,
	H3_R_CCU,
	A31_CCU,
	A64_CCU,
	A64_R_CCU,
	A13_CCU,
	A83T_CCU,
	A83T_R_CCU,
};

struct aw_ccung_softc {
	device_t		dev;
	struct resource		*res;
	struct clkdom		*clkdom;
	struct mtx		mtx;
	int			type;
	struct aw_ccung_reset	*resets;
	int			nresets;
	struct aw_ccung_gate	*gates;
	int			ngates;
	struct aw_clk_init	*clk_init;
	int			n_clk_init;
};

struct aw_ccung_reset {
	uint32_t	offset;
	uint32_t	shift;
};

struct aw_ccung_gate {
	const char	*name;
	const char	*parent_name;
	uint32_t	id;
	uint32_t	offset;
	uint32_t	shift;
};

#endif /* __CCU_NG_H__ */
