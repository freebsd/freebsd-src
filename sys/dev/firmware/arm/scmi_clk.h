/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2023 Arm Ltd
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#ifndef	_ARM64_SCMI_SCMI_CLK_H_
#define	_ARM64_SCMI_SCMI_CLK_H_

/*
 * SCMI Clock Protocol
 */

struct scmi_clk_protocol_attrs_out {
	uint32_t attributes;
#define	CLK_ATTRS_NCLOCKS_S		0
#define	CLK_ATTRS_NCLOCKS_M		(0xffff << CLK_ATTRS_NCLOCKS_S)
};

struct scmi_clk_attrs_in {
	uint32_t clock_id;
};

struct scmi_clk_attrs_out {
	uint32_t attributes;
#define	CLK_ATTRS_RATE_CHANGE_NOTIFY_SUPP	(1 << 31)
#define	CLK_ATTRS_RATE_REQ_CHANGE_NOTIFY_SUPP	(1 << 30)
#define	CLK_ATTRS_EXT_CLK_NAME			(1 << 29)
#define	CLK_ATTRS_ENABLED			(1 << 0)
	uint8_t clock_name[16];		/* only if attrs bit 29 unset */
	uint32_t clock_enable_delay;	/* worst case */
};

struct scmi_clk_name_get_in {
	uint32_t clock_id;
};

struct scmi_clk_name_get_out {
	uint32_t flags;
	uint8_t name[64];
};

enum scmi_clock_message_id {
	SCMI_CLOCK_ATTRIBUTES = 0x3,
	SCMI_CLOCK_RATE_SET = 0x5,
	SCMI_CLOCK_RATE_GET = 0x6,
	SCMI_CLOCK_CONFIG_SET = 0x7,
	SCMI_CLOCK_NAME_GET = 0x8,
};

#define SCMI_CLK_RATE_ASYNC_NOTIFY	(1 << 0)
#define SCMI_CLK_RATE_ASYNC_NORESP	(1 << 0 | 1 << 1)
#define SCMI_CLK_RATE_ROUND_DOWN	0
#define SCMI_CLK_RATE_ROUND_UP		(1 << 2)
#define SCMI_CLK_RATE_ROUND_CLOSEST	(1 << 3)

struct scmi_clk_state_in {
	uint32_t clock_id;
	uint32_t attributes;
};

struct scmi_clk_rate_get_in {
	uint32_t clock_id;
};

struct scmi_clk_rate_get_out {
	uint32_t rate_lsb;
	uint32_t rate_msb;
};

struct scmi_clk_rate_set_in {
	uint32_t flags;
	uint32_t clock_id;
	uint32_t rate_lsb;
	uint32_t rate_msb;
};

#endif /* !_ARM64_SCMI_SCMI_CLK_H_ */
