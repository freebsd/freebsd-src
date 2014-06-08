/*
 * Copyright © 2013 David Herrmann <dh.herrmann@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <config.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "libevdev.h"
#include "libevdev-int.h"
#include "libevdev-util.h"
#include "event-names.h"

struct name_lookup {
	const char *name;
	size_t len;
};

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

LIBEVDEV_EXPORT int
libevdev_event_type_from_name(const char *name)
{
	return libevdev_event_type_from_name_n(name, strlen(name));
}

LIBEVDEV_EXPORT int
libevdev_event_type_from_name_n(const char *name, size_t len)
{
	struct name_lookup lookup;
	const struct name_entry *entry;

	lookup.name = name;
	lookup.len = len;

	entry = lookup_name(ev_names, ARRAY_LENGTH(ev_names), &lookup);

	return entry ? (int)entry->value : -1;
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

LIBEVDEV_EXPORT int
libevdev_event_code_from_name(unsigned int type, const char *name)
{
	return libevdev_event_code_from_name_n(type, name, strlen(name));
}

LIBEVDEV_EXPORT int
libevdev_event_code_from_name_n(unsigned int type, const char *name, size_t len)
{
	struct name_lookup lookup;
	const struct name_entry *entry;
	int real_type;

	/* verify that @name is really of type @type */
	real_type = type_from_prefix(name, len);
	if (real_type < 0 || (unsigned int)real_type != type)
		return -1;

	/* now look up the name @name and return the constant */
	lookup.name = name;
	lookup.len = len;

	entry = lookup_name(code_names, ARRAY_LENGTH(code_names), &lookup);

	return entry ? (int)entry->value : -1;
}
