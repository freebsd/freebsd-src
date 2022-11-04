/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
 */

#pragma once

#include <contrib/dev/acpica/include/acpi.h>

#define BHYVE_ACPI_BASE 0xf2400

#define BASL_TABLE_ALIGNMENT 0x10
#define BASL_TABLE_ALIGNMENT_FACS 0x40

#define BASL_EXEC(x)                                                         \
	do {                                                                 \
		const int error = (x);                                       \
		if (error) {                                                 \
			warnc(error,                                         \
			    "BASL failed @ %s:%d\n    Failed to execute %s", \
			    __func__, __LINE__, #x);                         \
			return (error);                                      \
		}                                                            \
	} while (0)

#define QEMU_FWCFG_MAX_NAME 56

struct basl_table;

int basl_finish(void);
int basl_init(void);
int basl_table_append_bytes(struct basl_table *table, const void *bytes,
    uint32_t len);
int basl_table_create(struct basl_table **table, struct vmctx *ctx,
    const uint8_t *name, uint32_t alignment, uint32_t off);
