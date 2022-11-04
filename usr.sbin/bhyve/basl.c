/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <machine/vmm.h>

#include <assert.h>
#include <err.h>
#include <libutil.h>
#include <stddef.h>
#include <stdio.h>
#include <vmmapi.h>

#include "basl.h"

struct basl_table {
	STAILQ_ENTRY(basl_table) chain;
	struct vmctx *ctx;
	uint8_t fwcfg_name[QEMU_FWCFG_MAX_NAME];
	void *data;
	uint32_t len;
	uint32_t off;
	uint32_t alignment;
};
static STAILQ_HEAD(basl_table_list, basl_table) basl_tables = STAILQ_HEAD_INITIALIZER(
    basl_tables);

static int
basl_dump_table(const struct basl_table *const table, const bool mem)
{
	const ACPI_TABLE_HEADER *const header = table->data;
	const uint8_t *data;

	if (!mem) {
		data = table->data;
	} else {
		data = vm_map_gpa(table->ctx, BHYVE_ACPI_BASE + table->off,
		    table->len);
		if (data == NULL) {
			return (ENOMEM);
		}
	}

	printf("%.4s @ %8x (%s)\n", header->Signature,
	    BHYVE_ACPI_BASE + table->off, mem ? "Memory" : "FwCfg");
	hexdump(data, table->len, NULL, 0);

	return (0);
}

static int
basl_dump(const bool mem)
{
	struct basl_table *table;

	STAILQ_FOREACH(table, &basl_tables, chain) {
		BASL_EXEC(basl_dump_table(table, mem));
	}

	return (0);
}

static int
basl_finish_install_guest_tables(struct basl_table *const table)
{
	void *gva;

	/*
	 * Install ACPI tables directly in guest memory for use by guests which
	 * do not boot via EFI. EFI ROMs provide a pointer to the firmware
	 * generated ACPI tables instead, but it doesn't hurt to install the
	 * tables always.
	 */
	gva = vm_map_gpa(table->ctx, BHYVE_ACPI_BASE + table->off, table->len);
	if (gva == NULL) {
		warnx("%s: could not map gpa [ 0x%16lx, 0x%16lx ]", __func__,
		    (uint64_t)BHYVE_ACPI_BASE + table->off,
		    (uint64_t)BHYVE_ACPI_BASE + table->off + table->len);
		return (ENOMEM);
	}
	memcpy(gva, table->data, table->len);

	return (0);
}

int
basl_finish(void)
{
	struct basl_table *table;

	if (STAILQ_EMPTY(&basl_tables)) {
		warnx("%s: no ACPI tables found", __func__);
		return (EINVAL);
	}

	STAILQ_FOREACH(table, &basl_tables, chain) {
		BASL_EXEC(basl_finish_install_guest_tables(table));
	}

	return (0);
}

int
basl_init(void)
{
	return (0);
}

int
basl_table_append_bytes(struct basl_table *const table, const void *const bytes,
    const uint32_t len)
{
	void *end;

	assert(table != NULL);
	assert(bytes != NULL);

	if (table->len + len <= table->len) {
		warnx("%s: table too large (table->len 0x%8x len 0x%8x)",
		    __func__, table->len, len);
		return (EFAULT);
	}

	table->data = reallocf(table->data, table->len + len);
	if (table->data == NULL) {
		warnx("%s: failed to realloc table to length 0x%8x", __func__,
		    table->len + len);
		table->len = 0;
		return (ENOMEM);
	}

	end = (uint8_t *)table->data + table->len;
	table->len += len;

	memcpy(end, bytes, len);

	return (0);
}

int
basl_table_create(struct basl_table **const table, struct vmctx *ctx,
    const uint8_t *const name, const uint32_t alignment,
    const uint32_t off)
{
	struct basl_table *new_table;

	assert(table != NULL);

	new_table = calloc(1, sizeof(struct basl_table));
	if (new_table == NULL) {
		warnx("%s: failed to allocate table", __func__);
		return (ENOMEM);
	}

	new_table->ctx = ctx;

	snprintf(new_table->fwcfg_name, sizeof(new_table->fwcfg_name),
	    "etc/acpi/%s", name);

	new_table->alignment = alignment;
	new_table->off = off;

	STAILQ_INSERT_TAIL(&basl_tables, new_table, chain);

	*table = new_table;

	return (0);
}
