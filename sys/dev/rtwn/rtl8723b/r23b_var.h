/*
 * Copyright (c) 2026 Ahmad Khalifa <vexeduxr@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef R23B_VAR_H
#define R23B_VAR_H

#include <dev/rtwn/rtl8723b/r23b_rom_defs.h>

struct r23b_softc {
	struct r92e_softc super;

	uint8_t bt_antnum;
};

#endif
