/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <machine/vmm.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "e820.h"
#include "qemu_fwcfg.h"

#define E820_FWCFG_FILE_NAME "etc/e820"

#define KB (1024UL)
#define MB (1024 * KB)
#define GB (1024 * MB)

struct e820_element {
	TAILQ_ENTRY(e820_element) chain;
	uint64_t base;
	uint64_t end;
	enum e820_memory_type type;
};
static TAILQ_HEAD(e820_table, e820_element) e820_table = TAILQ_HEAD_INITIALIZER(
    e820_table);

static struct e820_element *
e820_element_alloc(uint64_t base, uint64_t end, enum e820_memory_type type)
{
	struct e820_element *element;

	element = calloc(1, sizeof(*element));
	if (element == NULL) {
		return (NULL);
	}

	element->base = base;
	element->end = end;
	element->type = type;

	return (element);
}

struct qemu_fwcfg_item *
e820_get_fwcfg_item(void)
{
	struct qemu_fwcfg_item *fwcfg_item;
	struct e820_element *element;
	struct e820_entry *entries;
	int count, i;

	count = 0;
	TAILQ_FOREACH(element, &e820_table, chain) {
		++count;
	}
	if (count == 0) {
		warnx("%s: E820 table empty", __func__);
		return (NULL);
	}

	fwcfg_item = calloc(1, sizeof(struct qemu_fwcfg_item));
	if (fwcfg_item == NULL) {
		return (NULL);
	}

	fwcfg_item->size = count * sizeof(struct e820_entry);
	fwcfg_item->data = calloc(count, sizeof(struct e820_entry));
	if (fwcfg_item->data == NULL) {
		free(fwcfg_item);
		return (NULL);
	}

	i = 0;
	entries = (struct e820_entry *)fwcfg_item->data;
	TAILQ_FOREACH(element, &e820_table, chain) {
		struct e820_entry *entry = &entries[i];

		entry->base = element->base;
		entry->length = element->end - element->base;
		entry->type = element->type;

		++i;
	}

	return (fwcfg_item);
}

static int
e820_add_entry(const uint64_t base, const uint64_t end,
    const enum e820_memory_type type)
{
	struct e820_element *new_element;
	struct e820_element *element;
	struct e820_element *ram_element;

	assert(end >= base);

	new_element = e820_element_alloc(base, end, type);
	if (new_element == NULL) {
		return (ENOMEM);
	}

	/*
	 * E820 table should always be sorted in ascending order. Therefore,
	 * search for a range whose end is larger than the base parameter.
	 */
	TAILQ_FOREACH(element, &e820_table, chain) {
		if (element->end > base) {
			break;
		}
	}

	/*
	 * System memory requires special handling.
	 */
	if (type == E820_TYPE_MEMORY) {
		/*
		 * base is larger than of any existing element. Add new system
		 * memory at the end of the table.
		 */
		if (element == NULL) {
			TAILQ_INSERT_TAIL(&e820_table, new_element, chain);
			return (0);
		}

		/*
		 * System memory shouldn't overlap with any existing element.
		 */
		assert(end >= element->base);

		TAILQ_INSERT_BEFORE(element, new_element, chain);

		return (0);
	}

	assert(element != NULL);
	/* Non system memory should be allocated inside system memory. */
	assert(element->type == E820_TYPE_MEMORY);
	/* New element should fit into existing system memory element. */
	assert(base >= element->base && end <= element->end);

	if (base == element->base) {
		/*
		 * New element at system memory base boundary. Add new
		 * element before current and adjust the base of the old
		 * element.
		 *
		 * Old table:
		 * 	[ 0x1000, 0x4000] RAM		<-- element
		 * New table:
		 * 	[ 0x1000, 0x2000] Reserved
		 * 	[ 0x2000, 0x4000] RAM		<-- element
		 */
		TAILQ_INSERT_BEFORE(element, new_element, chain);
		element->base = end;
	} else if (end == element->end) {
		/*
		 * New element at system memory end boundary. Add new
		 * element after current and adjust the end of the
		 * current element.
		 *
		 * Old table:
		 * 	[ 0x1000, 0x4000] RAM		<-- element
		 * New table:
		 * 	[ 0x1000, 0x3000] RAM		<-- element
		 * 	[ 0x3000, 0x4000] Reserved
		 */
		TAILQ_INSERT_AFTER(&e820_table, element, new_element, chain);
		element->end = base;
	} else {
		/*
		 * New element inside system memory entry. Split it by
		 * adding a system memory element and the new element
		 * before current.
		 *
		 * Old table:
		 * 	[ 0x1000, 0x4000] RAM		<-- element
		 * New table:
		 * 	[ 0x1000, 0x2000] RAM
		 * 	[ 0x2000, 0x3000] Reserved
		 * 	[ 0x3000, 0x4000] RAM		<-- element
		 */
		ram_element = e820_element_alloc(element->base, base,
		    E820_TYPE_MEMORY);
		if (ram_element == NULL) {
			return (ENOMEM);
		}
		TAILQ_INSERT_BEFORE(element, ram_element, chain);
		TAILQ_INSERT_BEFORE(element, new_element, chain);
		element->base = end;
	}

	return (0);
}

int
e820_init(struct vmctx *const ctx)
{
	uint64_t lowmem_size, highmem_size;
	int error;

	TAILQ_INIT(&e820_table);

	lowmem_size = vm_get_lowmem_size(ctx);
	error = e820_add_entry(0, lowmem_size, E820_TYPE_MEMORY);
	if (error) {
		warnx("%s: Could not add lowmem", __func__);
		return (error);
	}

	highmem_size = vm_get_highmem_size(ctx);
	if (highmem_size != 0) {
		error = e820_add_entry(4 * GB, 4 * GB + highmem_size,
		    E820_TYPE_MEMORY);
		if (error) {
			warnx("%s: Could not add highmem", __func__);
			return (error);
		}
	}

	return (0);
}
