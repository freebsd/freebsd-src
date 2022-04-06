/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
 */

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include <contrib/dev/acpica/include/acpi.h>
#pragma clang diagnostic pop

#define ACPI_GAS_ACCESS_WIDTH_LEGACY 0
#define ACPI_GAS_ACCESS_WIDTH_UNDEFINED 0
#define ACPI_GAS_ACCESS_WIDTH_BYTE 1
#define ACPI_GAS_ACCESS_WIDTH_WORD 2
#define ACPI_GAS_ACCESS_WIDTH_DWORD 3
#define ACPI_GAS_ACCESS_WIDTH_QWORD 4

#define BHYVE_ACPI_BASE 0xf2400

#define BASL_TABLE_ALIGNMENT 0x10
#define BASL_TABLE_ALIGNMENT_FACS 0x40

#define BASL_TABLE_CHECKSUM_LEN_FULL_TABLE (-1U)

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

void basl_fill_gas(ACPI_GENERIC_ADDRESS *gas, uint8_t space_id,
    uint8_t bit_width, uint8_t bit_offset, uint8_t access_width,
    uint64_t address);
int basl_finish(void);
int basl_init(void);
int basl_table_add_checksum(struct basl_table *const table, const uint32_t off,
    const uint32_t start, const uint32_t len);
int basl_table_add_length(struct basl_table *const table, const uint32_t off,
    const uint8_t size);
int basl_table_add_pointer(struct basl_table *const table,
    const uint8_t src_signature[ACPI_NAMESEG_SIZE], const uint32_t off,
    const uint8_t size);
int basl_table_append_bytes(struct basl_table *table, const void *bytes,
    uint32_t len);
int basl_table_append_checksum(struct basl_table *table, uint32_t start,
    uint32_t len);
/* Add an ACPI_TABLE_* to basl without its header. */
int basl_table_append_content(struct basl_table *table, void *data,
    uint32_t len);
int basl_table_append_gas(struct basl_table *table, uint8_t space_id,
    uint8_t bit_width, uint8_t bit_offset, uint8_t access_width,
    uint64_t address);
int basl_table_append_header(struct basl_table *table,
    const uint8_t signature[ACPI_NAMESEG_SIZE], uint8_t revision,
    uint32_t oem_revision);
int basl_table_append_int(struct basl_table *table, uint64_t val, uint8_t size);
int basl_table_append_length(struct basl_table *table, uint8_t size);
int basl_table_append_pointer(struct basl_table *table,
    const uint8_t src_signature[ACPI_NAMESEG_SIZE], uint8_t size);
int basl_table_create(struct basl_table **table, struct vmctx *ctx,
    const uint8_t *name, uint32_t alignment);
