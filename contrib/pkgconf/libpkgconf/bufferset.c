/*
 * bufferset.c
 * dynamically-managed buffer sets
 *
 * Copyright (c) 2026 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

/*
 * !doc
 *
 * libpkgconf `bufferset` module
 * =============================
 *
 * The libpkgconf `bufferset` module contains the functions related to managing
 * dynamically-allocated sets of buffers.
 */

pkgconf_bufferset_t *
pkgconf_bufferset_extend(pkgconf_list_t *list, pkgconf_buffer_t *buffer)
{
	pkgconf_bufferset_t *set = calloc(1, sizeof(*set));

	if (set == NULL)
		return NULL;

	if (pkgconf_buffer_len(buffer))
		pkgconf_buffer_append(&set->buffer, pkgconf_buffer_str(buffer));

	pkgconf_node_insert_tail(&set->node, set, list);

	return set;
}

void
pkgconf_bufferset_free(pkgconf_list_t *list)
{
	pkgconf_node_t *iter, *iter_next;

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(list->head, iter_next, iter)
	{
		pkgconf_bufferset_t *set = iter->data;

		pkgconf_buffer_finalize(&set->buffer);
		pkgconf_node_delete(&set->node, list);

		free(set);
	}
}
