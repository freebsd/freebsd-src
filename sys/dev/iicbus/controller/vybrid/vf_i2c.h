/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2024 Pierre-Luc Drouin <pldrouin@pldrouin.net>
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

/*
 * Vybrid Family Inter-Integrated Circuit (I2C)
 * Originally based on Chapter 48, Vybrid Reference Manual, Rev. 5, 07/2013
 * Currently based on Chapter 21, LX2160A Reference Manual, Rev. 1, 10/2021
 *
 * The current implementation is based on the original driver by Ruslan Bukin,
 * later modified by Dawid Górecki, and split into FDT and ACPI drivers by Val
 * Packett.
 */

#ifndef __VF_I2C_H__
#define __VF_I2C_H__

#include "opt_acpi.h"
#include "opt_platform.h"

#ifdef FDT
#include <dev/extres/clk/clk.h>
#endif

#define VF_I2C_DEVSTR "Vybrid Family Inter-Integrated Circuit (I2C)"

#define HW_MVF600       0x01
#define HW_VF610        0x02

#define MVF600_DIV_REG	0x14

#define VF_I2C_DEFAULT_BUS_SPEED	100000

struct vf_i2c_softc {
	struct resource         *res[2];
	bus_space_tag_t         bst;
	bus_space_handle_t      bsh;
	uint32_t                freq;
	device_t                dev;
	device_t                iicbus;
	struct mtx              mutex;
	uintptr_t               hwtype;
#ifdef FDT
	clk_t                   clock;
#endif
};

extern driver_t vf_i2c_driver;

device_attach_t vf_i2c_attach_common;

#endif /* !__VF_I2C_H__ */
