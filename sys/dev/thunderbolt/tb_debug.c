/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 */

#include "opt_thunderbolt.h"

/* PCIe bridge for Thunderbolt */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/gsb_crc32.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/stdarg.h>

#include <dev/thunderbolt/nhi_reg.h>
#include <dev/thunderbolt/nhi_var.h>
#include <dev/thunderbolt/tb_reg.h>
#include <dev/thunderbolt/tb_var.h>
#include <dev/thunderbolt/tbcfg_reg.h>
#include <dev/thunderbolt/tb_debug.h>

tb_string_t nhi_outmailcmd_opmode[] = {
	{ 0x000,	"Safe Mode" },
	{ 0x100,	"Authentication Mode" },
	{ 0x200,	"Endpoint Mode" },
	{ 0x300,	"Connection Manager Fully Functional" },
	{ 0, NULL }
};

tb_string_t nhi_frame_pdf[] = {
	{ 0x01, "PDF_READ" },
	{ 0x02, "PDF_WRITE" },
	{ 0x03, "PDF_NOTIFY" },
	{ 0x04, "PDF_NOTIFY_ACK" },
	{ 0x05, "PDF_HOTPLUG" },
	{ 0x06, "PDF_XDOMAIN_REQ" },
	{ 0x07, "PDF_XDOMAIN_RESP" },
	{ 0x0a, "PDF_CM_EVENT" },
	{ 0x0b, "PDF_CM_REQ" },
	{ 0x0c, "PDF_CM_RESP" },
	{ 0, NULL }
};

tb_string_t tb_security_level[] = {
	{ TBSEC_NONE,	"None" },
	{ TBSEC_USER,	"User" },
	{ TBSEC_SECURE,	"Secure Authorization" },
	{ TBSEC_DP,	"Display Port" },
	{ TBSEC_UNKNOWN,"Unknown" },
	{ 0, NULL }
};

tb_string_t tb_mbox_connmode[] = {
	{ INMAILCMD_SETMODE_CERT_TB_1ST_DEPTH, "Certified/1st" },
	{ INMAILCMD_SETMODE_ANY_TB_1ST_DEPTH,  "Any/1st"	},
	{ INMAILCMD_SETMODE_CERT_TB_ANY_DEPTH, "Certified/Any"	},
	{ INMAILCMD_SETMODE_ANY_TB_ANY_DEPTH,  "Any/Any"	},
	{ 0, NULL }
};

tb_string_t tb_device_power[] = {
	{ 0x0, "Self-powered" },
	{ 0x1, "Normal power" },
	{ 0x2, "High power" },
	{ 0x3, "Unknown power draw" },
	{ 0, NULL }
};

tb_string_t tb_notify_code[] = {
	{ 0x03, "DEVCONN" },
	{ 0x04, "DISCONN" },
	{ 0x05, "DPCONN" },
	{ 0x06, "DOMCONN" },
	{ 0x07, "DOMDISCONN" },
	{ 0x08, "DPCHANGE" },
	{ 0x09, "I2C" },
	{ 0x0a, "RTD3" },
	{ 0, NULL }
};

tb_string_t tb_adapter_type[] = {
	{ ADP_CS2_UNSUPPORTED, "Unsupported Adapter" },
	{ ADP_CS2_LANE, "Lane Adapter" },
	{ ADP_CS2_HOSTIF, "Host Interface Adapter" },
	{ ADP_CS2_PCIE_DFP, "Downstream PCIe Adapter" },
	{ ADP_CS2_PCIE_UFP, "Upstream PCIe Adapter" },
	{ ADP_CS2_DP_OUT, "DP OUT Adapter" },
	{ ADP_CS2_DP_IN, "DP IN Adapter" },
	{ ADP_CS2_USB3_DFP, "Downstream USB3 Adapter" },
	{ ADP_CS2_USB3_UFP, "Upstream USB3 Adapter" },
	{ 0, NULL }
};

tb_string_t tb_adapter_state[] = {
	{ CAP_LANE_STATE_DISABLE, "Disabled" },
	{ CAP_LANE_STATE_TRAINING, "Training" },
	{ CAP_LANE_STATE_CL0, "CL0" },
	{ CAP_LANE_STATE_TXCL0, "TX CL0s" },
	{ CAP_LANE_STATE_RXCL0, "RX CL0s" },
	{ CAP_LANE_STATE_CL1, "CL1" },
	{ CAP_LANE_STATE_CL2, "CL2" },
	{ CAP_LANE_STATE_CLD, "CLd" },
	{ 0, NULL }
};

tb_string_t tb_notify_event[] = {
	{ TB_CFG_ERR_CONN,	"Connection error" },
	{ TB_CFG_ERR_LINK,	"Link error" },
	{ TB_CFG_ERR_ADDR,	"Addressing error" },
	{ TB_CFG_ERR_ADP,	"Invalid adapter" },
	{ TB_CFG_ERR_ENUM,	"Enumeration error" },
	{ TB_CFG_ERR_NUA,	"Adapter not enumerated" },
	{ TB_CFG_ERR_LEN,	"Invalid request length" },
	{ TB_CFG_ERR_HEC,	"Invalid packet header" },
	{ TB_CFG_ERR_FC,	"Flow control error" },
	{ TB_CFG_ERR_PLUG,	"Hot plug error" },
	{ TB_CFG_ERR_LOCK,	"Adapter locked" },
	{ TB_CFG_HP_ACK,	"Hotplug acknowledgement" },
	{ TB_CFG_DP_BW,		"Display port bandwidth change" },
	{ 0, NULL }
};

const char *
tb_get_string(uintmax_t key, tb_string_t *table)
{

	if (table == NULL)
		return ("<null>");

	while (table->value != NULL) {
		if (table->key == key)
			return (table->value);
		table++;
	}

	return ("<unknown>");
}

static struct tb_debug_string {
	char *name;
	int flag;
} tb_debug_strings[] = {
	{"info", DBG_INFO},
	{"init", DBG_INIT},
	{"info", DBG_INFO},
	{"rxq", DBG_RXQ},
	{"txq", DBG_TXQ},
	{"intr", DBG_INTR},
	{"tb",	DBG_TB},
	{"mbox", DBG_MBOX},
	{"bridge", DBG_BRIDGE},
	{"cfg", DBG_CFG},
	{"router", DBG_ROUTER},
	{"port", DBG_PORT},
	{"hcm", DBG_HCM},
	{"extra", DBG_EXTRA},
	{"noisy", DBG_NOISY},
	{"full", DBG_FULL}
};

enum tb_debug_level_combiner {
	COMB_NONE,
	COMB_ADD,
	COMB_SUB
};

int
tb_debug_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct sbuf *sbuf;
#if defined (THUNDERBOLT_DEBUG) && (THUNDERBOLT_DEBUG > 0)
	struct tb_debug_string *string;
	char *buffer;
	size_t sz;
	u_int *debug;
	int i, len;
#endif
	int error;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

#if defined (THUNDERBOLT_DEBUG) && (THUNDERBOLT_DEBUG > 0)
	debug = (u_int *)arg1;

	sbuf_printf(sbuf, "%#x", *debug);

	sz = sizeof(tb_debug_strings) / sizeof(tb_debug_strings[0]);
	for (i = 0; i < sz; i++) {
		string = &tb_debug_strings[i];
		if (*debug & string->flag)
			sbuf_printf(sbuf, ",%s", string->name);
	}

	error = sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	if (error || req->newptr == NULL)
		return (error);

	len = req->newlen - req->newidx;
	if (len == 0)
		return (0);

	buffer = malloc(len, M_THUNDERBOLT, M_ZERO|M_WAITOK);
	error = SYSCTL_IN(req, buffer, len);

	tb_parse_debug(debug, buffer);

	free(buffer, M_THUNDERBOLT);
#else
	sbuf_printf(sbuf, "debugging unavailable");
	error = sbuf_finish(sbuf);
	sbuf_delete(sbuf);
#endif

	return (error);
}

void
tb_parse_debug(u_int *debug, char *list)
{
	struct tb_debug_string *string;
	enum tb_debug_level_combiner op;
	char *token, *endtoken;
	size_t sz;
	int flags, i;

	if (list == NULL || *list == '\0')
		return;

	if (*list == '+') {
		op = COMB_ADD;
		list++;
	} else if (*list == '-') {
		op = COMB_SUB;
		list++;
	} else
		op = COMB_NONE;
	if (*list == '\0')
		return;

	flags = 0;
	sz = sizeof(tb_debug_strings) / sizeof(tb_debug_strings[0]);
	while ((token = strsep(&list, ":,")) != NULL) {

		/* Handle integer flags */
		flags |= strtol(token, &endtoken, 0);
		if (token != endtoken)
			continue;

		/* Handle text flags */
		for (i = 0; i < sz; i++) {
			string = &tb_debug_strings[i];
			if (strcasecmp(token, string->name) == 0) {
				flags |= string->flag;
				break;
			}
		}
	}

	switch (op) {
	case COMB_NONE:
		*debug = flags;
		break;
	case COMB_ADD:
		*debug |= flags;
		break;
	case COMB_SUB:
		*debug &= (~flags);
		break;
	}
	return;
}

void
tbdbg_dprintf(device_t dev, u_int debug, u_int val, const char *fmt, ...)
{
#if defined(THUNDERBOLT_DEBUG) && (THUNDERBOLT_DEBUG > 0)
	va_list ap;
	u_int lvl, dbg;

	lvl = debug & 0xc0000000;
	dbg = debug & 0x3fffffff;
	va_start(ap, fmt);
	if ((lvl >= (val & 0xc0000000)) &&
	    ((dbg & (val & 0x3fffffff)) != 0)) {
		device_printf(dev, "");
		vprintf(fmt, ap);
	}
	va_end(ap);
#endif
}
