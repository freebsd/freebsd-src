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

enum e820_allocation_strategy {
	/* allocate any address */
	E820_ALLOCATE_ANY,
	/* allocate lowest address larger than address */
	E820_ALLOCATE_LOWEST,
	/* allocate highest address lower than address */
	E820_ALLOCATE_HIGHEST,
	/* allocate a specific address */
	E820_ALLOCATE_SPECIFIC
};

struct e820_entry {
	uint64_t base;
	uint64_t length;
	uint32_t type;
} __packed;

#define E820_ALIGNMENT_NONE 1

uint64_t e820_alloc(const uint64_t address, const uint64_t length,
    const uint64_t alignment, const enum e820_memory_type type,
    const enum e820_allocation_strategy strategy);
void e820_dump_table(void);
int e820_init(struct vmctx *const ctx);
int e820_finalize(void);
