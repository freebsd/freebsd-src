/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Scott Long
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
 * Thunderbolt 3 driver debug strings
 *
 * $FreeBSD$
 */

#ifndef _TB_DEBUG_H
#define _TB_DEBUG_H

typedef struct {
	uintmax_t	key;
	const char *	value;
} tb_string_t;

const char * tb_get_string(uintmax_t, tb_string_t *);
int tb_debug_sysctl(SYSCTL_HANDLER_ARGS);
void tb_parse_debug(u_int *, char *);

extern tb_string_t nhi_outmailcmd_opmode[];
extern tb_string_t nhi_frame_pdf[];
extern tb_string_t tb_security_level[];
extern tb_string_t tb_rdy_connmode[];
extern tb_string_t tb_mbox_connmode[];
extern tb_string_t tb_device_power[];
extern tb_string_t tb_notify_code[];
extern tb_string_t tb_adapter_type[];
extern tb_string_t tb_adapter_state[];
extern tb_string_t tb_notify_event[];

enum {
	/* Debug subsystems */
	DBG_NONE	= 0,
	DBG_INIT	= (1 << 0),
	DBG_INFO	= (1 << 1),
	DBG_RXQ		= (1 << 2),
	DBG_TXQ		= (1 << 3),
	DBG_INTR	= (1 << 4),
	DBG_TB		= (1 << 5),
	DBG_MBOX	= (1 << 6),
	DBG_BRIDGE	= (1 << 7),
	DBG_CFG		= (1 << 8),
	DBG_ROUTER	= (1 << 9),
	DBG_PORT	= (1 << 10),
	DBG_HCM		= (1 << 11),
	/* Debug levels */
	DBG_EXTRA	= (1 << 30),
	DBG_NOISY	= (1 << 31),
	DBG_FULL	= DBG_EXTRA | DBG_NOISY
};

/*
 * Macros to wrap printing.
 * Each softc type needs a `dev` and `debug` field.  Do tbdbg_printf as a
 * function to make format errors more clear during compile.
 */
void tbdbg_dprintf(device_t dev, u_int debug, u_int val, const char *fmt, ...) __printflike(4, 5);

#if defined(THUNDERBOLT_DEBUG) && (THUNDERBOLT_DEBUG > 0)
#define tb_debug(sc, level, fmt...)	\
	tbdbg_dprintf((sc)->dev, (sc)->debug, level, ##fmt)
#else
#define tb_debug(sc, level, fmt...)
#endif
#define tb_printf(sc, fmt...)		\
	device_printf((sc)->dev, ##fmt)

#endif /* _TB_DEBUG_H */
