/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/endian.h>
#include <sys/queue.h>

#include <machine/vmm.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vmmapi.h>

#include "qemu_fwcfg.h"
#include "qemu_loader.h"

struct qemu_loader_entry {
	uint32_t cmd_le;
	union {
		struct {
			uint8_t name[QEMU_FWCFG_MAX_NAME];
			uint32_t alignment_le;
			uint8_t zone;
		} alloc;
		struct {
			uint8_t dest_name[QEMU_FWCFG_MAX_NAME];
			uint8_t src_name[QEMU_FWCFG_MAX_NAME];
			uint32_t off_le;
			uint8_t size;
		} add_pointer;
		struct {
			uint8_t name[QEMU_FWCFG_MAX_NAME];
			uint32_t off_le;
			uint32_t start_le;
			uint32_t len_le;
		} add_checksum;
		struct {
			uint8_t dest_name[QEMU_FWCFG_MAX_NAME];
			uint8_t src_name[QEMU_FWCFG_MAX_NAME];
			uint32_t dest_off_le;
			uint32_t src_off_le;
			uint8_t size;
		} write_pointer;

		/* padding */
		uint8_t pad[124];
	};
} __packed;

enum qemu_loader_command {
	QEMU_LOADER_CMD_ALLOC = 0x1,
	QEMU_LOADER_CMD_ADD_POINTER = 0x2,
	QEMU_LOADER_CMD_ADD_CHECKSUM = 0x3,
	QEMU_LOADER_CMD_WRITE_POINTER = 0x4,
};

struct qemu_loader_element {
	STAILQ_ENTRY(qemu_loader_element) chain;
	struct qemu_loader_entry entry;
};

struct qemu_loader {
	uint8_t fwcfg_name[QEMU_FWCFG_MAX_NAME];
	STAILQ_HEAD(qemu_loader_list, qemu_loader_element) list;
};

int
qemu_loader_alloc(struct qemu_loader *const loader, const uint8_t *name,
    const uint32_t alignment, const enum qemu_loader_zone zone)
{
	struct qemu_loader_element *element;

	if (strlen(name) >= QEMU_FWCFG_MAX_NAME)
		return (EINVAL);

	element = calloc(1, sizeof(struct qemu_loader_element));
	if (element == NULL) {
		warnx("%s: failed to allocate command", __func__);
		return (ENOMEM);
	}

	element->entry.cmd_le = htole32(QEMU_LOADER_CMD_ALLOC);
	strncpy(element->entry.alloc.name, name, QEMU_FWCFG_MAX_NAME);
	element->entry.alloc.alignment_le = htole32(alignment);
	element->entry.alloc.zone = zone;

	/*
	 * The guest always works on copies of the fwcfg item, which where
	 * loaded into guest memory. Loading a fwcfg item is caused by ALLOC.
	 * For that reason, ALLOC should be scheduled in front of any other
	 * commands.
	 */
	STAILQ_INSERT_HEAD(&loader->list, element, chain);

	return (0);
}

int
qemu_loader_add_checksum(struct qemu_loader *const loader, const uint8_t *name,
    const uint32_t off, const uint32_t start, const uint32_t len)
{
	struct qemu_loader_element *element;

	if (strlen(name) >= QEMU_FWCFG_MAX_NAME)
		return (EINVAL);

	element = calloc(1, sizeof(struct qemu_loader_element));
	if (element == NULL) {
		warnx("%s: failed to allocate command", __func__);
		return (ENOMEM);
	}

	element->entry.cmd_le = htole32(QEMU_LOADER_CMD_ADD_CHECKSUM);
	strncpy(element->entry.add_checksum.name, name, QEMU_FWCFG_MAX_NAME);
	element->entry.add_checksum.off_le = htole32(off);
	element->entry.add_checksum.start_le = htole32(start);
	element->entry.add_checksum.len_le = htole32(len);

	STAILQ_INSERT_TAIL(&loader->list, element, chain);

	return (0);
}

int
qemu_loader_add_pointer(struct qemu_loader *const loader,
    const uint8_t *dest_name, const uint8_t *src_name, const uint32_t off,
    const uint8_t size)
{
	struct qemu_loader_element *element;

	if (strlen(dest_name) >= QEMU_FWCFG_MAX_NAME ||
	    strlen(src_name) >= QEMU_FWCFG_MAX_NAME)
		return (EINVAL);

	element = calloc(1, sizeof(struct qemu_loader_element));
	if (element == NULL) {
		warnx("%s: failed to allocate command", __func__);
		return (ENOMEM);
	}

	element->entry.cmd_le = htole32(QEMU_LOADER_CMD_ADD_POINTER);
	strncpy(element->entry.add_pointer.dest_name, dest_name,
	    QEMU_FWCFG_MAX_NAME);
	strncpy(element->entry.add_pointer.src_name, src_name,
	    QEMU_FWCFG_MAX_NAME);
	element->entry.add_pointer.off_le = htole32(off);
	element->entry.add_pointer.size = size;

	STAILQ_INSERT_TAIL(&loader->list, element, chain);

	return (0);
}

int
qemu_loader_create(struct qemu_loader **const new_loader,
    const uint8_t *fwcfg_name)
{
	struct qemu_loader *loader;

	if (new_loader == NULL || strlen(fwcfg_name) >= QEMU_FWCFG_MAX_NAME) {
		return (EINVAL);
	}

	loader = calloc(1, sizeof(struct qemu_loader));
	if (loader == NULL) {
		warnx("%s: failed to allocate loader", __func__);
		return (ENOMEM);
	}

	strncpy(loader->fwcfg_name, fwcfg_name, QEMU_FWCFG_MAX_NAME);
	STAILQ_INIT(&loader->list);

	*new_loader = loader;

	return (0);
}

static const uint8_t *
qemu_loader_get_zone_name(const enum qemu_loader_zone zone)
{
	switch (zone) {
	case QEMU_LOADER_ALLOC_HIGH:
		return ("HIGH");
	case QEMU_LOADER_ALLOC_FSEG:
		return ("FSEG");
	default:
		return ("Unknown");
	}
}

static void __unused
qemu_loader_dump_entry(const struct qemu_loader_entry *const entry)
{
	switch (le32toh(entry->cmd_le)) {
	case QEMU_LOADER_CMD_ALLOC:
		printf("CMD_ALLOC\n\r");
		printf("  name     : %s\n\r", entry->alloc.name);
		printf("  alignment: %8x\n\r",
		    le32toh(entry->alloc.alignment_le));
		printf("  zone     : %s\n\r",
		    qemu_loader_get_zone_name(entry->alloc.zone));
		break;
	case QEMU_LOADER_CMD_ADD_POINTER:
		printf("CMD_ADD_POINTER\n\r");
		printf("  dest_name: %s\n\r", entry->add_pointer.dest_name);
		printf("  src_name : %s\n\r", entry->add_pointer.src_name);
		printf("  off      : %8x\n\r",
		    le32toh(entry->add_pointer.off_le));
		printf("  size     : %8x\n\r", entry->add_pointer.size);
		break;
	case QEMU_LOADER_CMD_ADD_CHECKSUM:
		printf("CMD_ADD_CHECKSUM\n\r");
		printf("  name     : %s\n\r", entry->add_checksum.name);
		printf("  off      : %8x\n\r",
		    le32toh(entry->add_checksum.off_le));
		printf("  start    : %8x\n\r",
		    le32toh(entry->add_checksum.start_le));
		printf("  length   : %8x\n\r",
		    le32toh(entry->add_checksum.len_le));
		break;
	case QEMU_LOADER_CMD_WRITE_POINTER:
		printf("CMD_WRITE_POINTER\n\r");
		printf("  dest_name: %s\n\r", entry->write_pointer.dest_name);
		printf("  src_name : %s\n\r", entry->write_pointer.src_name);
		printf("  dest_off : %8x\n\r",
		    le32toh(entry->write_pointer.dest_off_le));
		printf("  src_off  : %8x\n\r",
		    le32toh(entry->write_pointer.src_off_le));
		printf("  size     : %8x\n\r", entry->write_pointer.size);
		break;
	default:
		printf("UNKNOWN\n\r");
		break;
	}
}

int
qemu_loader_finish(struct qemu_loader *const loader)
{
	struct qemu_loader_element *element;
	struct qemu_loader_entry *data;
	size_t len = 0;

	STAILQ_FOREACH(element, &loader->list, chain) {
		len += sizeof(struct qemu_loader_entry);
	}
	if (len == 0) {
		warnx("%s: bios loader empty", __func__);
		return (EFAULT);
	}

	data = calloc(1, len);
	if (data == NULL) {
		warnx("%s: failed to allocate fwcfg data", __func__);
		return (ENOMEM);
	}

	int i = 0;
	STAILQ_FOREACH(element, &loader->list, chain) {
		memcpy(&data[i], &element->entry,
		    sizeof(struct qemu_loader_entry));
		++i;
	}

	return (qemu_fwcfg_add_file(loader->fwcfg_name, len, data));
}
