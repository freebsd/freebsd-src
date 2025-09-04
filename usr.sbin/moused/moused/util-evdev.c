/*
 * Copyright © 2013 David Herrmann <dh.herrmann@gmail.com>
 * Copyright © 2013 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <dev/evdev/input.h>

#include "event-names.h"
#include "util-evdev.h"

#define ARRAY_LENGTH(a) (sizeof(a) / (sizeof((a)[0])))

struct name_lookup {
	const char *name;
	size_t len;
};

static inline bool
startswith(const char *str, size_t len, const char *prefix, size_t plen)
{
	return len >= plen && !strncmp(str, prefix, plen);
}

static int type_from_prefix(const char *name, ssize_t len)
{
	const char *e;
	size_t i;
	ssize_t l;

	/* MAX_ is not allowed, even though EV_MAX exists */
	if (startswith(name, len, "MAX_", 4))
		return -1;
	/* BTN_ is special as there is no EV_BTN type */
	if (startswith(name, len, "BTN_", 4))
		return EV_KEY;
	/* FF_STATUS_ is special as FF_ is a prefix of it, so test it first */
	if (startswith(name, len, "FF_STATUS_", 10))
		return EV_FF_STATUS;

	for (i = 0; i < ARRAY_LENGTH(ev_names); ++i) {
		/* skip EV_ prefix so @e is suffix of [EV_]XYZ */
		e = &ev_names[i].name[3];
		l = strlen(e);

		/* compare prefix and test for trailing _ */
		if (len > l && startswith(name, len, e, l) && name[l] == '_')
			return ev_names[i].value;
	}

	return -1;
}

static int cmp_entry(const void *vlookup, const void *ventry)
{
	const struct name_lookup *lookup = vlookup;
	const struct name_entry *entry = ventry;
	int r;

	r = strncmp(lookup->name, entry->name, lookup->len);
	if (!r) {
		if (entry->name[lookup->len])
			r = -1;
		else
			r = 0;
	}

	return r;
}

static const struct name_entry*
lookup_name(const struct name_entry *array, size_t asize,
	    struct name_lookup *lookup)
{
	const struct name_entry *entry;

	entry = bsearch(lookup, array, asize, sizeof(*array), cmp_entry);
	if (!entry)
		return NULL;

	return entry;
}

int
libevdev_event_type_get_max(unsigned int type)
{
	if (type > EV_MAX)
		return -1;

	return ev_max[type];
}

int
libevdev_event_code_from_name(unsigned int type, const char *name)
{
	struct name_lookup lookup;
	const struct name_entry *entry;
	int real_type;
	size_t len = strlen(name);

	real_type = type_from_prefix(name, len);
	if (real_type < 0 || (unsigned int)real_type != type)
		return -1;

	lookup.name = name;
	lookup.len = len;

	entry = lookup_name(code_names, ARRAY_LENGTH(code_names), &lookup);

	return entry ? (int)entry->value : -1;
}

static int
libevdev_event_type_from_name_n(const char *name, size_t len)
{
	struct name_lookup lookup;
	const struct name_entry *entry;

	lookup.name = name;
	lookup.len = len;

	entry = lookup_name(ev_names, ARRAY_LENGTH(ev_names), &lookup);

	return entry ? (int)entry->value : -1;
}

int
libevdev_event_type_from_name(const char *name)
{
	return libevdev_event_type_from_name_n(name, strlen(name));
}

static int
libevdev_property_from_name_n(const char *name, size_t len)
{
	struct name_lookup lookup;
	const struct name_entry *entry;

	lookup.name = name;
	lookup.len = len;

	entry = lookup_name(prop_names, ARRAY_LENGTH(prop_names), &lookup);

	return entry ? (int)entry->value : -1;
}

int
libevdev_property_from_name(const char *name)
{
	return libevdev_property_from_name_n(name, strlen(name));
}
