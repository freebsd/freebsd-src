/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#pragma once

#include <vmmapi.h>

#include "qemu_fwcfg.h"

enum e820_memory_type {
	E820_TYPE_MEMORY = 1,
	E820_TYPE_RESERVED = 2,
	E820_TYPE_ACPI = 3,
	E820_TYPE_NVS = 4
};

struct e820_entry {
	uint64_t base;
	uint64_t length;
	uint32_t type;
} __packed;

struct qemu_fwcfg_item *e820_get_fwcfg_item(void);
int e820_init(struct vmctx *const ctx);
