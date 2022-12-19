/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
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
 *
 * $FreeBSD$
 */

#ifndef	_ARM64_SCMI_SCMI_PROTOCOLS_H_
#define	_ARM64_SCMI_SCMI_PROTOCOLS_H_

enum scmi_std_protocol {
	SCMI_PROTOCOL_ID_BASE = 0x10,
	SCMI_PROTOCOL_ID_POWER_DOMAIN = 0x11,
	SCMI_PROTOCOL_ID_SYSTEM = 0x12,
	SCMI_PROTOCOL_ID_PERF = 0x13,
	SCMI_PROTOCOL_ID_CLOCK = 0x14,
	SCMI_PROTOCOL_ID_SENSOR = 0x15,
	SCMI_PROTOCOL_ID_RESET_DOMAIN = 0x16,
	SCMI_PROTOCOL_ID_VOLTAGE_DOMAIN = 0x17,
};

enum scmi_status_code {
	SCMI_SUCCESS =  0,
	SCMI_NOT_SUPPORTED = -1,
	SCMI_INVALID_PARAMETERS = -2,
	SCMI_DENIED = -3,
	SCMI_NOT_FOUND = -4,
	SCMI_OUT_OF_RANGE = -5,
	SCMI_BUSY = -6,
	SCMI_COMMS_ERROR = -7,
	SCMI_GENERIC_ERROR = -8,
	SCMI_HARDWARE_ERROR = -9,
	SCMI_PROTOCOL_ERROR = -10,
};

#define	SCMI_PROTOCOL_ATTRIBUTES	0x1

#endif /* !_ARM64_SCMI_SCMI_PROTOCOLS_H_ */
