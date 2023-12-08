/*-
 * Copyright (c) 2023 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#include <sys/param.h>

#include "stand.h"

static uint32_t loader_features;

#define	FEATURE_ENTRY(name, desc)	{ FEATURE_##name, #name, desc }
static const struct feature_entry {
	uint32_t	value;
	const char	*name;
	const char	*desc;
} feature_map[] = {
	FEATURE_ENTRY(EARLY_ACPI,  "Loader probes ACPI in early startup"),
};

void
feature_enable(uint32_t mask)
{

	loader_features |= mask;
}

bool
feature_name_is_enabled(const char *name)
{
	const struct feature_entry *entry;

	for (size_t i = 0; i < nitems(feature_map); i++) {
		entry = &feature_map[i];

		if (strcmp(entry->name, name) == 0)
			return ((loader_features & entry->value) != 0);
	}

	return (false);
}

void
feature_iter(feature_iter_fn *iter_fn, void *cookie)
{
	const struct feature_entry *entry;

	for (size_t i = 0; i < nitems(feature_map); i++) {
		entry = &feature_map[i];

		(*iter_fn)(cookie, entry->name, entry->desc,
		    (loader_features & entry->value) != 0);
	}
}
