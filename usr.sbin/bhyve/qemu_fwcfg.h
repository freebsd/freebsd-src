/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#pragma once

#include <vmmapi.h>

#define QEMU_FWCFG_MAX_ARCHS 0x2
#define QEMU_FWCFG_MAX_ENTRIES 0x4000

struct qemu_fwcfg_item {
	uint32_t size;
	uint8_t *data;
};

int qemu_fwcfg_init(struct vmctx *const ctx);
