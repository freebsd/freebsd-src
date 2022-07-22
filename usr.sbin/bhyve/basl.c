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
#include "qemu_loader.h"

struct basl_table_checksum {
	STAILQ_ENTRY(basl_table_checksum) chain;
	uint32_t off;
	uint32_t start;
	uint32_t len;
};

struct basl_table_length {
	STAILQ_ENTRY(basl_table_length) chain;
	uint32_t off;
	uint8_t size;
};

struct basl_table_pointer {
	STAILQ_ENTRY(basl_table_pointer) chain;
	uint8_t src_signature[ACPI_NAMESEG_SIZE];
	uint32_t off;
	uint8_t size;
};

struct basl_table {
	STAILQ_ENTRY(basl_table) chain;
	struct vmctx *ctx;
	uint8_t fwcfg_name[QEMU_FWCFG_MAX_NAME];
	void *data;
	uint32_t len;
	uint32_t off;
	uint32_t alignment;
	STAILQ_HEAD(basl_table_checksum_list, basl_table_checksum) checksums;
	STAILQ_HEAD(basl_table_length_list, basl_table_length) lengths;
	STAILQ_HEAD(basl_table_pointer_list, basl_table_pointer) pointers;
};
static STAILQ_HEAD(basl_table_list, basl_table) basl_tables = STAILQ_HEAD_INITIALIZER(
    basl_tables);

static struct qemu_loader *basl_loader;

static __inline uint64_t
basl_le_dec(void *pp, size_t len)
{
	assert(len <= 8);

	switch (len) {
	case 1:
		return ((uint8_t *)pp)[0];
	case 2:
		return le16dec(pp);
	case 4:
		return le32dec(pp);
	case 8:
		return le64dec(pp);
	}

	return 0;
}

static __inline void
basl_le_enc(void *pp, uint64_t val, size_t len)
{
	char buf[8];

	assert(len <= 8);

	le64enc(buf, val);
	memcpy(pp, buf, len);
}

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

static int __unused
basl_dump(const bool mem)
{
	struct basl_table *table;

	STAILQ_FOREACH(table, &basl_tables, chain) {
		BASL_EXEC(basl_dump_table(table, mem));
	}

	return (0);
}

void
basl_fill_gas(ACPI_GENERIC_ADDRESS *const gas, const uint8_t space_id,
    const uint8_t bit_width, const uint8_t bit_offset,
    const uint8_t access_width, const uint64_t address)
{
	assert(gas != NULL);

	gas->SpaceId = space_id;
	gas->BitWidth = bit_width;
	gas->BitOffset = bit_offset;
	gas->AccessWidth = access_width;
	gas->Address = htole64(address);
}

static int
basl_finish_install_guest_tables(struct basl_table *const table, uint32_t *const off)
{
	void *gva;

	table->off = roundup2(*off, table->alignment);
	*off = table->off + table->len;
	if (*off <= table->off) {
		warnx("%s: invalid table length 0x%8x @ offset 0x%8x", __func__,
		    table->len, table->off);
		return (EFAULT);
	}

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

	/* Cause guest bios to copy the ACPI table into guest memory. */
	BASL_EXEC(
	    qemu_fwcfg_add_file(table->fwcfg_name, table->len, table->data));
	BASL_EXEC(qemu_loader_alloc(basl_loader, table->fwcfg_name,
	    table->alignment, QEMU_LOADER_ALLOC_HIGH));

	return (0);
}

static int
basl_finish_patch_checksums(struct basl_table *const table)
{
	struct basl_table_checksum *checksum;

	STAILQ_FOREACH(checksum, &table->checksums, chain) {
		uint8_t *gva, *checksum_gva;
		uint64_t gpa;
		uint32_t len;
		uint8_t sum;
		
		len = checksum->len;
		if (len == BASL_TABLE_CHECKSUM_LEN_FULL_TABLE) {
			len = table->len;
		}

		assert(checksum->off < table->len);
		assert(checksum->start < table->len);
		assert(checksum->start + len <= table->len);

		/*
		 * Install ACPI tables directly in guest memory for use by
		 * guests which do not boot via EFI. EFI ROMs provide a pointer
		 * to the firmware generated ACPI tables instead, but it doesn't
		 * hurt to install the tables always.
		 */
		gpa = BHYVE_ACPI_BASE + table->off + checksum->start;
		if ((gpa < BHYVE_ACPI_BASE) ||
		    (gpa < BHYVE_ACPI_BASE + table->off)) {
			warnx("%s: invalid gpa (off 0x%8x start 0x%8x)",
			    __func__, table->off, checksum->start);
			return (EFAULT);
		}

		gva = vm_map_gpa(table->ctx, gpa, len);
		if (gva == NULL) {
			warnx("%s: could not map gpa [ 0x%16lx, 0x%16lx ]",
			    __func__, gpa, gpa + len);
			return (ENOMEM);
		}
	
		checksum_gva = gva + checksum->off;
		if (checksum_gva < gva) {
			warnx("%s: invalid checksum offset 0x%8x", __func__,
			    checksum->off);
			return (EFAULT);
		}

		sum = 0;
		for (uint32_t i = 0; i < len; ++i) {
			sum += *(gva + i);
		}
		*checksum_gva = -sum;

		/* Cause guest bios to patch the checksum. */
		BASL_EXEC(qemu_loader_add_checksum(basl_loader,
		    table->fwcfg_name, checksum->off, checksum->start, len));
	}

	return (0);
}

static struct basl_table *
basl_get_table_by_signature(const uint8_t signature[ACPI_NAMESEG_SIZE])
{
	struct basl_table *table;

	STAILQ_FOREACH(table, &basl_tables, chain) {
		const ACPI_TABLE_HEADER *const header =
		    (const ACPI_TABLE_HEADER *)table->data;

		if (strncmp(header->Signature, signature,
			sizeof(header->Signature)) == 0) {
			return (table);
		}
	}

	warnx("%s: %.4s not found", __func__, signature);
	return (NULL);
}

static int
basl_finish_patch_pointers(struct basl_table *const table)
{
	struct basl_table_pointer *pointer;

	STAILQ_FOREACH(pointer, &table->pointers, chain) {
		const struct basl_table *src_table;
		uint8_t *gva;
		uint64_t gpa, val;

		assert(pointer->off < table->len);
		assert(pointer->off + pointer->size <= table->len);

		src_table = basl_get_table_by_signature(pointer->src_signature);
		if (src_table == NULL) {
			warnx("%s: could not find ACPI table %.4s", __func__,
			    pointer->src_signature);
			return (EFAULT);
		}

		/*
		 * Install ACPI tables directly in guest memory for use by
		 * guests which do not boot via EFI. EFI ROMs provide a pointer
		 * to the firmware generated ACPI tables instead, but it doesn't
		 * hurt to install the tables always.
		 */
		gpa = BHYVE_ACPI_BASE + table->off;
		if (gpa < BHYVE_ACPI_BASE) {
			warnx("%s: table offset of 0x%8x is too large",
			    __func__, table->off);
			return (EFAULT);
		}

		gva = vm_map_gpa(table->ctx, gpa, table->len);
		if (gva == NULL) {
			warnx("%s: could not map gpa [ 0x%16lx, 0x%16lx ]",
			    __func__, gpa, gpa + table->len);
			return (ENOMEM);
		}

		val = basl_le_dec(gva + pointer->off, pointer->size);
		val += BHYVE_ACPI_BASE + src_table->off;
		basl_le_enc(gva + pointer->off, val, pointer->size);

		/* Cause guest bios to patch the pointer. */
		BASL_EXEC(
		    qemu_loader_add_pointer(basl_loader, table->fwcfg_name,
			src_table->fwcfg_name, pointer->off, pointer->size));
	}

	return (0);
}

static int
basl_finish_set_length(struct basl_table *const table)
{
	struct basl_table_length *length;

	STAILQ_FOREACH(length, &table->lengths, chain) {
		assert(length->off < table->len);
		assert(length->off + length->size <= table->len);

		basl_le_enc((uint8_t *)table->data + length->off, table->len,
		    length->size);
	}

	return (0);
}

int
basl_finish(void)
{
	struct basl_table *table;
	uint32_t off = 0;

	if (STAILQ_EMPTY(&basl_tables)) {
		warnx("%s: no ACPI tables found", __func__);
		return (EINVAL);
	}

	/*
	 * We have to install all tables before we can patch them. Therefore,
	 * use two loops. The first one installs all tables and the second one
	 * patches them.
	 */
	STAILQ_FOREACH(table, &basl_tables, chain) {
		BASL_EXEC(basl_finish_set_length(table));
		BASL_EXEC(basl_finish_install_guest_tables(table, &off));
	}
	STAILQ_FOREACH(table, &basl_tables, chain) {
		BASL_EXEC(basl_finish_patch_pointers(table));

		/*
		 * Calculate the checksum as last step!
		 */
		BASL_EXEC(basl_finish_patch_checksums(table));
	}
	BASL_EXEC(qemu_loader_finish(basl_loader));

	return (0);
}

int
basl_init(void)
{
	return (qemu_loader_create(&basl_loader, QEMU_FWCFG_FILE_TABLE_LOADER));
}

int
basl_table_add_checksum(struct basl_table *const table, const uint32_t off,
    const uint32_t start, const uint32_t len)
{
	struct basl_table_checksum *checksum;

	assert(table != NULL);

	checksum = calloc(1, sizeof(struct basl_table_checksum));
	if (checksum == NULL) {
		warnx("%s: failed to allocate checksum", __func__);
		return (ENOMEM);
	}

	checksum->off = off;
	checksum->start = start;
	checksum->len = len;

	STAILQ_INSERT_TAIL(&table->checksums, checksum, chain);

	return (0);
}

int
basl_table_add_length(struct basl_table *const table, const uint32_t off,
    const uint8_t size)
{
	struct basl_table_length *length;

	assert(table != NULL);
	assert(size == 4 || size == 8);

	length = calloc(1, sizeof(struct basl_table_length));
	if (length == NULL) {
		warnx("%s: failed to allocate length", __func__);
		return (ENOMEM);
	}

	length->off = off;
	length->size = size;

	STAILQ_INSERT_TAIL(&table->lengths, length, chain);

	return (0);
}

int
basl_table_add_pointer(struct basl_table *const table,
    const uint8_t src_signature[ACPI_NAMESEG_SIZE], const uint32_t off,
    const uint8_t size)
{
	struct basl_table_pointer *pointer;

	assert(table != NULL);
	assert(size == 4 || size == 8);

	pointer = calloc(1, sizeof(struct basl_table_pointer));
	if (pointer == NULL) {
		warnx("%s: failed to allocate pointer", __func__);
		return (ENOMEM);
	}

	memcpy(pointer->src_signature, src_signature,
	    sizeof(pointer->src_signature));
	pointer->off = off;
	pointer->size = size;

	STAILQ_INSERT_TAIL(&table->pointers, pointer, chain);

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
basl_table_append_checksum(struct basl_table *const table, const uint32_t start,
    const uint32_t len)
{
	assert(table != NULL);

	BASL_EXEC(basl_table_add_checksum(table, table->len, start, len));
	BASL_EXEC(basl_table_append_int(table, 0, 1));

	return (0);
}

int
basl_table_append_content(struct basl_table *table, void *data, uint32_t len)
{
	assert(data != NULL);
	assert(len >= sizeof(ACPI_TABLE_HEADER));

	return (basl_table_append_bytes(table,
	    (void *)((uintptr_t)(data) + sizeof(ACPI_TABLE_HEADER)),
	    len - sizeof(ACPI_TABLE_HEADER)));
}

int
basl_table_append_fwcfg(struct basl_table *const table,
    const uint8_t *fwcfg_name, const uint32_t alignment, const uint8_t size)
{
	assert(table != NULL);
	assert(fwcfg_name != NULL);
	assert(size <= sizeof(uint64_t));

	BASL_EXEC(qemu_loader_alloc(basl_loader, fwcfg_name, alignment,
	    QEMU_LOADER_ALLOC_HIGH));
	BASL_EXEC(qemu_loader_add_pointer(basl_loader, table->fwcfg_name,
	    fwcfg_name, table->len, size));
	BASL_EXEC(basl_table_append_int(table, 0, size));

	return (0);
}

int
basl_table_append_gas(struct basl_table *const table, const uint8_t space_id,
    const uint8_t bit_width, const uint8_t bit_offset,
    const uint8_t access_width, const uint64_t address)
{
	ACPI_GENERIC_ADDRESS gas_le = {
		.SpaceId = space_id,
		.BitWidth = bit_width,
		.BitOffset = bit_offset,
		.AccessWidth = access_width,
		.Address = htole64(address),
	};

	return (basl_table_append_bytes(table, &gas_le, sizeof(gas_le)));
}

int
basl_table_append_header(struct basl_table *const table,
    const uint8_t signature[ACPI_NAMESEG_SIZE], const uint8_t revision,
    const uint32_t oem_revision)
{
	ACPI_TABLE_HEADER header_le;
	/* + 1 is required for the null terminator */
	char oem_table_id[ACPI_OEM_TABLE_ID_SIZE + 1];

	assert(table != NULL);
	assert(table->len == 0);

	memcpy(header_le.Signature, signature, ACPI_NAMESEG_SIZE);
	header_le.Length = 0; /* patched by basl_finish */
	header_le.Revision = revision;
	header_le.Checksum = 0; /* patched by basl_finish */
	memcpy(header_le.OemId, "BHYVE ", ACPI_OEM_ID_SIZE);
	snprintf(oem_table_id, ACPI_OEM_TABLE_ID_SIZE, "BV%.4s  ", signature);
	memcpy(header_le.OemTableId, oem_table_id,
	    sizeof(header_le.OemTableId));
	header_le.OemRevision = htole32(oem_revision);
	memcpy(header_le.AslCompilerId, "BASL", ACPI_NAMESEG_SIZE);
	header_le.AslCompilerRevision = htole32(0x20220504);

	BASL_EXEC(
	    basl_table_append_bytes(table, &header_le, sizeof(header_le)));

	BASL_EXEC(basl_table_add_length(table,
	    offsetof(ACPI_TABLE_HEADER, Length), sizeof(header_le.Length)));
	BASL_EXEC(basl_table_add_checksum(table,
	    offsetof(ACPI_TABLE_HEADER, Checksum), 0,
	    BASL_TABLE_CHECKSUM_LEN_FULL_TABLE));

	return (0);
}

int
basl_table_append_int(struct basl_table *const table, const uint64_t val,
    const uint8_t size)
{
	char buf[8];

	assert(size <= sizeof(val));

	basl_le_enc(buf, val, size);
	return (basl_table_append_bytes(table, buf, size));
}

int
basl_table_append_length(struct basl_table *const table, const uint8_t size)
{
	assert(table != NULL);
	assert(size <= sizeof(table->len));

	BASL_EXEC(basl_table_add_length(table, table->len, size));
	BASL_EXEC(basl_table_append_int(table, 0, size));

	return (0);
}

int
basl_table_append_pointer(struct basl_table *const table,
    const uint8_t src_signature[ACPI_NAMESEG_SIZE], const uint8_t size)
{
	assert(table != NULL);
	assert(size == 4 || size == 8);

	BASL_EXEC(basl_table_add_pointer(table, src_signature, table->len, size));
	BASL_EXEC(basl_table_append_int(table, 0, size));

	return (0);
}

int
basl_table_create(struct basl_table **const table, struct vmctx *ctx,
    const uint8_t *const name, const uint32_t alignment)
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

	STAILQ_INIT(&new_table->checksums);
	STAILQ_INIT(&new_table->lengths);
	STAILQ_INIT(&new_table->pointers);

	STAILQ_INSERT_TAIL(&basl_tables, new_table, chain);

	*table = new_table;

	return (0);
}
