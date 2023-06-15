/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
 */

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <contrib/dev/acpica/include/acpi.h>
#pragma GCC diagnostic pop

#include "qemu_fwcfg.h"

#define ACPI_GAS_ACCESS_WIDTH_LEGACY 0
#define ACPI_GAS_ACCESS_WIDTH_UNDEFINED 0
#define ACPI_GAS_ACCESS_WIDTH_BYTE 1
#define ACPI_GAS_ACCESS_WIDTH_WORD 2
#define ACPI_GAS_ACCESS_WIDTH_DWORD 3
#define ACPI_GAS_ACCESS_WIDTH_QWORD 4

#define ACPI_SPCR_INTERRUPT_TYPE_8259 0x1
#define ACPI_SPCR_INTERRUPT_TYPE_APIC 0x2
#define ACPI_SPCR_INTERRUPT_TYPE_SAPIC 0x4
#define ACPI_SPCR_INTERRUPT_TYPE_GIC 0x8

#define ACPI_SPCR_BAUD_RATE_9600 3
#define ACPI_SPCR_BAUD_RATE_19200 4
#define ACPI_SPCR_BAUD_RATE_57600 6
#define ACPI_SPCR_BAUD_RATE_115200 7

#define ACPI_SPCR_PARITY_NO_PARITY 0

#define ACPI_SPCR_STOP_BITS_1 1

#define ACPI_SPCR_FLOW_CONTROL_DCD 0x1
#define ACPI_SPCR_FLOW_CONTROL_RTS_CTS 0x2
#define ACPI_SPCR_FLOW_CONTROL_XON_XOFF 0x4

#define ACPI_SPCR_TERMINAL_TYPE_VT100 0
#define ACPI_SPCR_TERMINAL_TYPE_VT100_PLUS 1
#define ACPI_SPCR_TERMINAL_TYPE_VT_UTF8 2
#define ACPI_SPCR_TERMINAL_TYPE_ANSI 3

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

struct basl_table;

void basl_fill_gas(ACPI_GENERIC_ADDRESS *gas, uint8_t space_id,
    uint8_t bit_width, uint8_t bit_offset, uint8_t access_width,
    uint64_t address);
int basl_finish(void);
int basl_init(struct vmctx *ctx);
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
int basl_table_append_fwcfg(struct basl_table *table,
    const uint8_t *fwcfg_name, uint32_t alignment,
    uint8_t size);
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
/* Adds the table to RSDT and XSDT */
int basl_table_register_to_rsdt(struct basl_table *table);
