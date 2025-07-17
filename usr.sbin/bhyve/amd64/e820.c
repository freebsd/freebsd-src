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

#include "debug.h"
#include "e820.h"
#include "qemu_fwcfg.h"

/*
 * E820 always uses 64 bit entries. Emulation code will use vm_paddr_t since it
 * works on physical addresses. If vm_paddr_t is larger than uint64_t E820 can't
 * hold all possible physical addresses and we can get into trouble.
 */
static_assert(sizeof(vm_paddr_t) <= sizeof(uint64_t),
    "Unable to represent physical memory by E820 table");

#define E820_FWCFG_FILE_NAME "etc/e820"

#define KB (1024UL)
#define MB (1024 * KB)
#define GB (1024 * MB)

/*
 * Fix E820 memory holes:
 * [    A0000,    C0000) VGA
 * [    C0000,   100000) ROM
 */
#define E820_VGA_MEM_BASE 0xA0000
#define E820_VGA_MEM_END 0xC0000
#define E820_ROM_MEM_BASE 0xC0000
#define E820_ROM_MEM_END 0x100000

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

static const char *
e820_get_type_name(const enum e820_memory_type type)
{
	switch (type) {
	case E820_TYPE_MEMORY:
		return ("RAM");
	case E820_TYPE_RESERVED:
		return ("Reserved");
	case E820_TYPE_ACPI:
		return ("ACPI");
	case E820_TYPE_NVS:
		return ("NVS");
	default:
		return ("Unknown");
	}
}

void
e820_dump_table(void)
{
	struct e820_element *element;
	uint64_t i;

	EPRINTLN("E820 map:");
	
	i = 0;
	TAILQ_FOREACH(element, &e820_table, chain) {
		EPRINTLN("  (%4lu) [%16lx, %16lx] %s", i,
		    element->base, element->end,
		    e820_get_type_name(element->type));

		++i;
	}
}

static struct qemu_fwcfg_item *
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
	struct e820_element *sib_element;
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

	/*
	 * If some one tries to allocate a specific address, it could happen, that
	 * this address is not allocatable. Therefore, do some checks. If the
	 * address is not allocatable, don't panic. The user may have a fallback and
	 * tries to allocate another address. This is true for the GVT-d emulation
	 * which tries to reuse the host address of the graphics stolen memory and
	 * falls back to allocating the highest address below 4 GB.
	 */
	if (element == NULL || element->type != E820_TYPE_MEMORY ||
	    (base < element->base || end > element->end))
		return (ENOMEM);

	if (base == element->base && end == element->end) {
		/*
		 * The new entry replaces an existing one.
		 *
		 * Old table:
		 * 	[ 0x1000, 0x4000] RAM		<-- element
		 * New table:
		 *	[ 0x1000, 0x4000] Reserved
		 */
		TAILQ_INSERT_BEFORE(element, new_element, chain);
		TAILQ_REMOVE(&e820_table, element, chain);
		free(element);
	} else if (base == element->base) {
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

	/*
	 * If the previous element has the same type and ends at our base
	 * boundary, we can merge both entries.
	 */
	sib_element = TAILQ_PREV(new_element, e820_table, chain);
	if (sib_element != NULL &&
	    sib_element->type == new_element->type &&
	    sib_element->end == new_element->base) {
		new_element->base = sib_element->base;
		TAILQ_REMOVE(&e820_table, sib_element, chain);
		free(sib_element);
	}

	/*
	 * If the next element has the same type and starts at our end
	 * boundary, we can merge both entries.
	 */
	sib_element = TAILQ_NEXT(new_element, chain);
	if (sib_element != NULL &&
	    sib_element->type == new_element->type &&
	    sib_element->base == new_element->end) {
		/* Merge new element into subsequent one. */
		new_element->end = sib_element->end;
		TAILQ_REMOVE(&e820_table, sib_element, chain);
		free(sib_element);
	}

	return (0);
}

static int
e820_add_memory_hole(const uint64_t base, const uint64_t end)
{
	struct e820_element *element;
	struct e820_element *ram_element;

	assert(end >= base);

	/*
	 * E820 table should be always sorted in ascending order. Therefore,
	 * search for an element which end is larger than the base parameter.
	 */
	TAILQ_FOREACH(element, &e820_table, chain) {
		if (element->end > base) {
			break;
		}
	}

	if (element == NULL || end <= element->base) {
		/* Nothing to do. Hole already exists */
		return (0);
	}

	/* Memory holes are only allowed in system memory */
	assert(element->type == E820_TYPE_MEMORY);

	if (base == element->base) {
		/*
		 * New hole at system memory base boundary.
		 *
		 * Old table:
		 * 	[ 0x1000, 0x4000] RAM
		 * New table:
		 * 	[ 0x2000, 0x4000] RAM
		 */
		element->base = end;
	} else if (end == element->end) {
		/*
		 * New hole at system memory end boundary.
		 *
		 * Old table:
		 * 	[ 0x1000, 0x4000] RAM
		 * New table:
		 * 	[ 0x1000, 0x3000] RAM
		 */
		element->end = base;
	} else {
		/*
		 * New hole inside system memory entry. Split the system memory.
		 *
		 * Old table:
		 * 	[ 0x1000, 0x4000] RAM		<-- element
		 * New table:
		 * 	[ 0x1000, 0x2000] RAM
		 * 	[ 0x3000, 0x4000] RAM		<-- element
		 */
		ram_element = e820_element_alloc(element->base, base,
		    E820_TYPE_MEMORY);
		if (ram_element == NULL) {
			return (ENOMEM);
		}
		TAILQ_INSERT_BEFORE(element, ram_element, chain);
		element->base = end;
	}

	return (0);
}

static uint64_t
e820_alloc_highest(const uint64_t max_address, const uint64_t length,
    const uint64_t alignment, const enum e820_memory_type type)
{
	struct e820_element *element;

	TAILQ_FOREACH_REVERSE(element, &e820_table, e820_table, chain) {
		uint64_t address, base, end;

		end = MIN(max_address, element->end);
		base = roundup2(element->base, alignment);

		/*
		 * If end - length == 0, we would allocate memory at address 0. This
		 * address is mostly unusable and we should avoid allocating it.
		 * Therefore, search for another block in that case.
		 */
		if (element->type != E820_TYPE_MEMORY || end < base ||
		    end - base < length || end - length == 0) {
			continue;
		}

		address = rounddown2(end - length, alignment);

		if (e820_add_entry(address, address + length, type) != 0) {
			return (0);
		}

		return (address);
	}

	return (0);
}

static uint64_t
e820_alloc_lowest(const uint64_t min_address, const uint64_t length,
    const uint64_t alignment, const enum e820_memory_type type)
{
	struct e820_element *element;

	TAILQ_FOREACH(element, &e820_table, chain) {
		uint64_t base, end;

		end = element->end;
		base = MAX(min_address, roundup2(element->base, alignment));

		/*
		 * If base == 0, we would allocate memory at address 0. This
		 * address is mostly unusable and we should avoid allocating it.
		 * Therefore, search for another block in that case.
		 */
		if (element->type != E820_TYPE_MEMORY || end < base ||
		    end - base < length || base == 0) {
			continue;
		}

		if (e820_add_entry(base, base + length, type) != 0) {
			return (0);
		}

		return (base);
	}

	return (0);
}

uint64_t
e820_alloc(const uint64_t address, const uint64_t length,
    const uint64_t alignment, const enum e820_memory_type type,
    const enum e820_allocation_strategy strategy)
{
	assert(powerof2(alignment));
	assert((address & (alignment - 1)) == 0);

	switch (strategy) {
	case E820_ALLOCATE_ANY:
		/*
		 * Allocate any address. Therefore, ignore the address parameter
		 * and reuse the code path for allocating the lowest address.
		 */
		return (e820_alloc_lowest(0, length, alignment, type));
	case E820_ALLOCATE_LOWEST:
		return (e820_alloc_lowest(address, length, alignment, type));
	case E820_ALLOCATE_HIGHEST:
		return (e820_alloc_highest(address, length, alignment, type));
	case E820_ALLOCATE_SPECIFIC:
		if (e820_add_entry(address, address + length, type) != 0) {
			return (0);
		}

		return (address);
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

	error = e820_add_memory_hole(E820_VGA_MEM_BASE, E820_VGA_MEM_END);
	if (error) {
		warnx("%s: Could not add VGA memory", __func__);
		return (error);
	}

	error = e820_add_memory_hole(E820_ROM_MEM_BASE, E820_ROM_MEM_END);
	if (error) {
		warnx("%s: Could not add ROM area", __func__);
		return (error);
	}

	return (0);
}

int
e820_finalize(void)
{
	struct qemu_fwcfg_item *e820_fwcfg_item;
	int error;

	e820_fwcfg_item = e820_get_fwcfg_item();
	if (e820_fwcfg_item == NULL) {
		warnx("invalid e820 table");
		return (ENOMEM);
	}
	error = qemu_fwcfg_add_file("etc/e820",
	    e820_fwcfg_item->size, e820_fwcfg_item->data);
	if (error != 0) {
		warnx("could not add qemu fwcfg etc/e820");
		free(e820_fwcfg_item->data);
		free(e820_fwcfg_item);
		return (error);
	}
	free(e820_fwcfg_item);

	return (0);
}
